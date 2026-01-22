/********************************************************************
*
*
*	VKPT Renderer: 2D UI Rendering and Stretch Pic System
*
*	Implements high-performance 2D rendering for user interface elements,
*	menus, HUD, text, and images. Uses a queued batch rendering approach
*	with instanced drawing, scissor rectangle support for clipping, and
*	HDR/SDR pipeline variants for proper tone mapping on HDR displays.
*
*	Architecture:
*	- Storage Buffer Object (SBO) for stretch pic instance data
*	- Descriptor sets for texture sampling and uniform parameters
*	- Dynamic pipeline creation with HDR/SDR specialization constants
*	- Instanced rendering: 4-vertex triangle strip per pic instance
*	- Render pass attached to swapchain framebuffers
*	- Final blit pipeline for post-processing effects
*
*	Algorithm:
*	1. Queue Phase: Client calls R_DrawStretchPic_RTX, R_DrawChar_RTX, etc.
*	   - Each call enqueues a StretchPic_t into stretch_pic_queue[]
*	   - Scissor groups track clip rectangles for batching
*	   - Alpha, color, scale, and rotation transforms are applied
*	2. Submit Phase: vkpt_draw_submit_stretch_pics() called per frame
*	   - Copy queue to GPU storage buffer
*	   - Begin render pass on swapchain image
*	   - For each scissor group:
*	     * Set viewport and scissor rectangle
*	     * Bind descriptor sets (SBO, textures, UBO)
*	     * Draw instanced: 4 verts × num_pics in group
*	   - End render pass
*	3. Shader Execution:
*	   - Vertex shader reads per-instance data from SBO
*	   - Generates quad with position/UV from instance transform
*	   - Fragment shader samples texture and applies color/alpha
*
*	Features:
*	- Instanced rendering for efficient batch submission
*	- Scissor rectangle grouping for UI clipping (dialogs, scrollable regions)
*	- HDR display support with configurable nits and saturation
*	- Rotation and pivot point transforms for animated UI elements
*	- Alpha blending with pre-multiplied alpha and sRGB correction
*	- Character and string rendering with monospace font atlases
*	- Texture tiling for backgrounds and repeating patterns
*	- Raw image updates for dynamic content (video playback, cinematics)
*	- Aspect-ratio-preserving scaling for icons and sprites
*
*	Scissor Group System:
*	- Each R_SetClipRect_RTX() call creates a new scissor group
*	- Groups batch consecutive pics with the same clip rectangle
*	- Reduces state changes: viewport/scissor set once per group
*	- Default group spans entire screen when clipping is disabled
*
*	Configuration:
*	- MAX_STRETCH_PICS: Maximum queued pics per frame (16384)
*	- STRETCH_PIC_SDR/HDR: Pipeline variants for display type
*	- TEXNUM_WHITE: Special handle for solid color fills (~0)
*	- MAX_FRAMES_IN_FLIGHT: Per-frame resource duplication (2)
*
*	Performance:
*	- Single storage buffer upload per frame (batched memcpy)
*	- Instanced rendering: 1 draw call per scissor group
*	- Dynamic viewport/scissor state reduces pipeline switching
*	- Orthographic projection precomputed in vertex shader
*	- sRGB texture sampling handled by image view format
*
*	Memory Layout (StretchPic_t, 128 bytes):
*	- Position/size: x, y, w, h (16 bytes)
*	- UV coords: s, t, w_s, h_t (16 bytes)
*	- Color/texture: RGBA8, tex_handle (8 bytes)
*	- Transform: pivot_x, pivot_y, angle (12 bytes, +4 pad)
*	- Matrix: 4×4 orthographic projection (64 bytes)
*
*
********************************************************************/
/*
Copyright (C) 2018 Christoph Schied
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "shared/shared.h"
#include "refresh/refresh.h"
#include "client/client.h"
#include "refresh/images.h"

#include <assert.h>

#include "vkpt.h"
#include "shader/global_textures.h"



/**
*
*
*
*	Configuration and Constants:
*
*
*
**/
//! Pipeline index for SDR (Standard Dynamic Range) displays.
enum {
	STRETCH_PIC_SDR,
	STRETCH_PIC_HDR,
	STRETCH_PIC_NUM_PIPELINES
};

//! Special texture handle for solid color fills (white texture).
#define TEXNUM_WHITE (~0)
//! Maximum number of stretch pics that can be queued per frame.
#define MAX_STRETCH_PICS (1<<14)



/**
*
*
*
*	Module State:
*
*
*
**/
//! Global draw state (scale, alpha, colors).
static drawStatic_t draw = {
	.scale = 1.0f,
	.alpha_scale = 1.0f
};

//! Total number of stretch pics currently queued.
static int num_stretch_pics = 0;

//! Per-instance stretch pic data (uploaded to GPU storage buffer).
typedef struct {
	float x, y, w, h;		// Position and size in screen space (16 bytes).
	float s, t, w_s, h_t;	// UV coordinates and dimensions (16 bytes).
	// 32 bytes up till here.
	uint32_t color, tex_handle;	// RGBA8 color and texture handle (8 bytes).
	float	pivot_x, pivot_y;	// Rotation pivot point (8 bytes).
	// 48 bytes up till here.
	// These are pads to keep memory properly aligned with GLSL.
	float	angle, pad01;	// Rotation angle + padding (8 bytes).
	// 56 bytes up till here.
	float	pad02, pad03;	// Additional padding (8 bytes).
	// 64 bytes up till here.
	//
	// Now adding the matrix:
	float	matTransform[16]; // 4×4 orthographic projection matrix (64 bytes).
	// Total: 128 bytes.
} StretchPic_t;

//! Uniform buffer object for HDR and tone mapping parameters.
//! Not using global UBO b/c it's only filled when a world is drawn, but here we need it all the time
typedef struct {
	float ui_hdr_nits;				//! Nits value for UI elements on HDR displays.
	float tm_hdr_saturation_scale;	//! Saturation scaling for tone mapping.
} StretchPic_UBO_t;

/**
*	Scissor group structure for batching stretch pics with the same clip rectangle.
*	
*	The following struct is a bit of an odd one, however, it will hold state of each
*	time clipRect has changed. Storing the num_stretch_pic offset and num_stretch_pic_count
*	in order for each subsequent draw call to apply their own scissor rectangle.
* 
*	The num_scissor_groups is rebuilt each frame, and reset at the end of that frame.
**/
typedef struct {
	clipRect_t clip_rect;			//! Clip rectangle for this group.

	//! Offset into the SBO pic buffer.
	uint32_t num_stretch_pic_offset;
	//! The count of stretch pics that were added during this scissor's clip rect.
	uint32_t num_stretch_pic_count;
} StretchPic_Scissor_Group;

//! Array of scissor groups (one per unique clip rectangle per frame).
static StretchPic_Scissor_Group stretch_pic_scissor_groups[ MAX_STRETCH_PICS ]; // WID: TODO: Find a sane number instead to not waste ram? Or just a dynamic buffer/queue of sorts.
//! Number of scissor groups active in the current frame.
static uint32_t num_stretch_pic_scissor_groups;

//! Current clip rectangle set by R_SetClipRect_RTX() calls.
static clipRect_t clip_rect;
//! Whether clipping is enabled (true) or full-screen rendering (false).
//! Each individual time that clip_enable is enabled, a new stretchpic scissor group is added
//! for the current frame. 
static bool clip_enable = false;

//! Stretch pic memory queue (filled by client calls, uploaded to GPU each frame).
static StretchPic_t stretch_pic_queue[MAX_STRETCH_PICS];

//! Pipeline layout for stretch pic rendering (SBO + textures + UBO).
static VkPipelineLayout        pipeline_layout_stretch_pic;
//! Pipeline layout for final blit operations.
static VkPipelineLayout        pipeline_layout_final_blit;
//! Render pass for drawing stretch pics to swapchain images.
static VkRenderPass            render_pass_stretch_pic;
//! Graphics pipelines for stretch pic rendering (SDR and HDR variants).
static VkPipeline              pipeline_stretch_pic[STRETCH_PIC_NUM_PIPELINES];
//! Graphics pipeline for final blit operations.
static VkPipeline              pipeline_final_blit;
//! Framebuffers for each swapchain image (dynamically allocated).
static VkFramebuffer*          framebuffer_stretch_pic = NULL;
//! Storage buffers for stretch pic instance data (per frame in flight).
static BufferResource_t        buf_stretch_pic_queue[MAX_FRAMES_IN_FLIGHT];
//! Uniform buffers for HDR parameters (per frame in flight).
static BufferResource_t        buf_ubo[MAX_FRAMES_IN_FLIGHT];
//! Descriptor set layout for storage buffer object.
static VkDescriptorSetLayout   desc_set_layout_sbo;
//! Descriptor set layout for uniform buffer object.
static VkDescriptorSetLayout   desc_set_layout_ubo;
//! Descriptor pool for SBO descriptor sets.
static VkDescriptorPool        desc_pool_sbo;
//! Descriptor pool for UBO descriptor sets.
static VkDescriptorPool        desc_pool_ubo;
//! Descriptor sets for SBO (per frame in flight).
static VkDescriptorSet         desc_set_sbo[MAX_FRAMES_IN_FLIGHT];
//! Descriptor sets for UBO (per frame in flight).
static VkDescriptorSet         desc_set_ubo[MAX_FRAMES_IN_FLIGHT];

//! Console variable for UI brightness on HDR displays (nits).
extern cvar_t* cvar_ui_hdr_nits;
//! Console variable for HDR saturation scaling during tone mapping.
extern cvar_t* cvar_tm_hdr_saturation_scale;



/**
*
*
*
*	Utility Functions:
*
*
*
**/
/**
*	@brief	Get the current rendering extent (unscaled resolution).
*	@return	VkExtent2D structure containing width and height.
*	@note	Returns the unscaled swapchain extent for proper UI coordinate mapping.
**/
VkExtent2D vkpt_draw_get_extent(void) {
	return qvk.extent_unscaled;
}



/**
*
*
*
*	Scissor Group Management:
*
*
*
**/
/**
*	@brief	Resets scissor groups to default state for a new frame.
*	@return	VK_SUCCESS on success.
*	@note	Called at the start of each frame to initialize the default full-screen scissor group.
*			All scissor groups from the previous frame are discarded.
**/
static VkResult vkpt_draw_clear_scissor_groups( void ) {
	// We always resort to the default scissor group.
	num_stretch_pic_scissor_groups = 1;
	// Clear the first default scissor group back to defaults.
	clipRect_t defaultClipRect = {
			.left = 0,
			.top = 0,
			.right = (float)vkpt_draw_get_extent().width,
			.bottom = (float)vkpt_draw_get_extent().height,
	};
	stretch_pic_scissor_groups[ 0 ].clip_rect = defaultClipRect;
	stretch_pic_scissor_groups[ 0 ].num_stretch_pic_count = 0;
	stretch_pic_scissor_groups[ 0 ].num_stretch_pic_offset = 0;
	return VK_SUCCESS;
}



/**
*
*
*
*	Stretch Pic Enqueueing:
*
*
*
**/
/**
*	@brief	Enqueue a 'stretch pic' draw command without rotation.
*	@param	x			Screen-space X position (left edge).
*	@param	y			Screen-space Y position (top edge).
*	@param	w			Width in screen pixels.
*	@param	h			Height in screen pixels.
*	@param	s1			Left UV coordinate (0.0 to 1.0).
*	@param	t1			Top UV coordinate (0.0 to 1.0).
*	@param	s2			Right UV coordinate (0.0 to 1.0).
*	@param	t2			Bottom UV coordinate (0.0 to 1.0).
*	@param	color		RGBA8 color value (format: 0xAABBGGRR).
*	@param	tex_handle	Texture handle from r_images array, or TEXNUM_WHITE for solid fills.
*	@note	Silently returns if alpha_scale is 0 (fully transparent).
*			Aborts with error if queue is full (MAX_STRETCH_PICS exceeded).
*			Invalid textures are automatically replaced with TEXNUM_WHITE.
*			Alpha channel is scaled by draw.alpha_scale for global transparency control.
**/
static inline void enqueue_stretch_pic(
	float x, float y, float w, float h,
	float s1, float t1, float s2, float t2,
	uint32_t color, int tex_handle ) {
	if ( draw.alpha_scale == 0.f )
		return;

	if ( num_stretch_pics == MAX_STRETCH_PICS ) {
		Com_EPrintf( "Error: stretch pic queue full!\n" );
		assert( 0 );
		return;
	}
	assert( tex_handle );

	// Increment the general num_stretch_pics count and fetch a pointer to it in the queue buffer.
	StretchPic_t *sp = stretch_pic_queue + num_stretch_pics++;

	// Increment the current amount of stretch pics in the active scissor group.
	StretchPic_Scissor_Group *scissor_group = &stretch_pic_scissor_groups[ num_stretch_pic_scissor_groups - 1 ];
	scissor_group->num_stretch_pic_count = num_stretch_pics - scissor_group->num_stretch_pic_offset;

	// Can we prevent another scissor group?
	//if ( clip_enable ) {
	//	if ( x >= clip_rect.right || x + w <= clip_rect.left || y >= clip_rect.bottom || y + h <= clip_rect.top )
	//		return;
	//}
	 
	// Screen width/height.
	float width = r_config.width * draw.scale;
	float height = r_config.height * draw.scale;

	// Projection matrix.
	create_orthographic_matrix( sp->matTransform, 0, width, 0, height, 1, -1 );

	// No rotating here.
	sp->pivot_x = 0;
	sp->pivot_y = 0;
	sp->angle = 0;

	// Get Rect.
	sp->x = x;
	sp->y = y;
	sp->w = w;
	sp->h = h;

	// Setup the UV coordinates to use.
	sp->s   = s1;
	sp->t   = t1;
	sp->w_s = s2 - s1;
	sp->h_t = t2 - t1;

	// Determine alpha value and apply it to color.
	if (draw.alpha_scale < 1.f)
	{
		float alpha = (color >> 24) & 0xff;
		alpha *= draw.alpha_scale;
		alpha = max(0.f, min(255.f, alpha));
		color = (color & 0xffffff) | ((int)(alpha) << 24);
	}

	// Store in the color and texture handle. If not available in our current
	// image registration sequence, apply the 'White' texture instead.
	sp->color = color;
	sp->tex_handle = tex_handle;
	if(tex_handle >= 0 && tex_handle < MAX_RIMAGES
	&& !r_images[tex_handle].registration_sequence) {
		sp->tex_handle = TEXNUM_WHITE;
	}
}

/**
*	@brief	Enqueue a 'stretch pic' draw command with rotation support.
*	@param	x			Screen-space X position (left edge).
*	@param	y			Screen-space Y position (top edge).
*	@param	w			Width in screen pixels.
*	@param	h			Height in screen pixels.
*	@param	s1			Left UV coordinate (0.0 to 1.0).
*	@param	t1			Top UV coordinate (0.0 to 1.0).
*	@param	s2			Right UV coordinate (0.0 to 1.0).
*	@param	t2			Bottom UV coordinate (0.0 to 1.0).
*	@param	angle		Rotation angle in degrees (clockwise).
*	@param	pivot_x		X coordinate of rotation pivot point (relative to x).
*	@param	pivot_y		Y coordinate of rotation pivot point (relative to y).
*	@param	color		RGBA8 color value (format: 0xAABBGGRR).
*	@param	tex_handle	Texture handle from r_images array, or TEXNUM_WHITE for solid fills.
*	@param	flags		Additional flags (currently unused).
*	@note	Supports rotating around a specified pivot point for animated UI elements.
*			Rotation is performed in the vertex shader by transforming vertices around pivot.
*			All other behavior matches enqueue_stretch_pic().
**/
static inline void enqueue_stretch_rotate_pic(
	float x, float y, float w, float h,
	float s1, float t1, float s2, float t2,
	float angle, float pivot_x, float pivot_y,
	uint32_t color, const int32_t tex_handle, const int32_t flags ) {

	if ( draw.alpha_scale == 0.f )
		return;

	if ( num_stretch_pics == MAX_STRETCH_PICS ) {
		Com_EPrintf( "Error: stretch pic queue full!\n" );
		assert( 0 );
		return;
	}
	assert( tex_handle );

	// Increment the general num_stretch_pics count and fetch a pointer to it in the queue buffer.
	StretchPic_t *sp = stretch_pic_queue + num_stretch_pics++;

	// Increment the current amount of stretch pics in the active scissor group.
	StretchPic_Scissor_Group *scissor_group = &stretch_pic_scissor_groups[ num_stretch_pic_scissor_groups - 1 ];
	scissor_group->num_stretch_pic_count = num_stretch_pics - scissor_group->num_stretch_pic_offset;

	// Screen width/height.
	float width = r_config.width * draw.scale;
	float height = r_config.height * draw.scale;

	// Projection matrix.
	create_orthographic_matrix( sp->matTransform, 0, width, 0, height, 1, -1 );

	// Setup rotating.
	sp->pivot_x = pivot_x;
	sp->pivot_y = pivot_y;
	sp->angle = angle;

	// Get Rect.
	sp->x = x;
	sp->y = y;
	sp->w = w;
	sp->h = h;

	// Setup the UV coordinates to use.
	sp->s = s1;
	sp->t = t1;
	sp->w_s = s2 - s1;
	sp->h_t = t2 - t1;

	// Determine alpha value and apply it to color.
	if ( draw.alpha_scale < 1.f ) {
		float alpha = ( color >> 24 ) & 0xff;
		alpha *= draw.alpha_scale;
		alpha = max( 0.f, min( 255.f, alpha ) );
		color = ( color & 0xffffff ) | ( (int)( alpha ) << 24 );
	}

	// Store in the color and texture handle. If not available in our current
	// image registration sequence, apply the 'White' texture instead.
	sp->color = color;
	sp->tex_handle = tex_handle;
	if ( tex_handle >= 0 && tex_handle < MAX_RIMAGES
		&& !r_images[ tex_handle ].registration_sequence ) {
		sp->tex_handle = TEXNUM_WHITE;
	}
}



/**
*
*
*
*	Render Pass Creation:
*
*
*
**/
/**
*	@brief	Creates the render pass for stretch pic rendering.
*	@note	Sets up a single-subpass render pass that renders to swapchain images.
*			Uses VK_ATTACHMENT_LOAD_OP_LOAD to preserve existing framebuffer content
*			(allows rendering UI on top of previously rendered 3D scene).
*			Final layout is VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for direct presentation.
**/
static void create_render_pass(void) {
	LOG_FUNC();
	VkAttachmentDescription color_attachment = {
		.format         = qvk.surf_format.format,
		.samples        = VK_SAMPLE_COUNT_1_BIT,
		.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
		//.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		//.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
		.initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference color_attachment_ref = {
		.attachment = 0, /* index in fragment shader */
		.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments    = &color_attachment_ref,
	};

	VkSubpassDependency dependencies[] = {
		{
			.srcSubpass    = VK_SUBPASS_EXTERNAL,
			.dstSubpass    = 0, /* index for own subpass */
			.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0, /* XXX verify */
			.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
			               | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		},
	};

	VkRenderPassCreateInfo render_pass_info = {
		.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments    = &color_attachment,
		.subpassCount    = 1,
		.pSubpasses      = &subpass,
		.dependencyCount = LENGTH(dependencies),
		.pDependencies   = dependencies,
	};

	_VK(vkCreateRenderPass(qvk.device, &render_pass_info, NULL, &render_pass_stretch_pic));
	ATTACH_LABEL_VARIABLE(render_pass_stretch_pic, RENDER_PASS);
}



/**
*
*
*
*	Initialization and Cleanup:
*
*
*
**/
/**
*	@brief	Initialize the draw system and allocate Vulkan resources.
*	@return	VK_SUCCESS on success, or Vulkan error code on failure.
*	@note	Creates render pass, descriptor set layouts, descriptor pools,
*			and per-frame storage/uniform buffers for stretch pic rendering.
*			Must be called once during renderer initialization before any drawing.
*			Pairs with vkpt_draw_destroy() for cleanup.
**/
VkResult vkpt_draw_initialize() {
	num_stretch_pics = 0;
	vkpt_draw_clear_scissor_groups();

	LOG_FUNC();
	create_render_pass();
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_VK(buffer_create(buf_stretch_pic_queue + i, sizeof(StretchPic_t) * MAX_STRETCH_PICS, 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

		_VK(buffer_create(buf_ubo + i, sizeof(StretchPic_UBO_t),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	}

	VkDescriptorSetLayoutBinding layout_bindings[] = {
		{
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.binding         = 0,
			.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
		},
	};

	VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = LENGTH(layout_bindings),
		.pBindings    = layout_bindings,
	};

	_VK(vkCreateDescriptorSetLayout(qvk.device, &layout_info, NULL, &desc_set_layout_sbo));
	ATTACH_LABEL_VARIABLE(desc_set_layout_sbo, DESCRIPTOR_SET_LAYOUT);

	VkDescriptorPoolSize pool_size = {
		.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 1,
		.pPoolSizes    = &pool_size,
		.maxSets       = MAX_FRAMES_IN_FLIGHT,
	};

	_VK(vkCreateDescriptorPool(qvk.device, &pool_info, NULL, &desc_pool_sbo));
	ATTACH_LABEL_VARIABLE(desc_pool_sbo, DESCRIPTOR_POOL);

	VkDescriptorSetLayoutBinding layout_bindings_ubo[] = {
		{
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.binding         = 2,
			.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	};

	VkDescriptorSetLayoutCreateInfo layout_info_ubo = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = LENGTH(layout_bindings_ubo),
		.pBindings    = layout_bindings_ubo,
	};

	_VK(vkCreateDescriptorSetLayout(qvk.device, &layout_info_ubo, NULL, &desc_set_layout_ubo));
	ATTACH_LABEL_VARIABLE(desc_set_layout_ubo, DESCRIPTOR_SET_LAYOUT);

	VkDescriptorPoolSize pool_size_ubo = {
		.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	};

	VkDescriptorPoolCreateInfo pool_info_ubo = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 1,
		.pPoolSizes    = &pool_size_ubo,
		.maxSets       = MAX_FRAMES_IN_FLIGHT,
	};

	_VK(vkCreateDescriptorPool(qvk.device, &pool_info_ubo, NULL, &desc_pool_ubo));
	ATTACH_LABEL_VARIABLE(desc_pool_ubo, DESCRIPTOR_POOL);


	VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool     = desc_pool_sbo,
		.descriptorSetCount = 1,
		.pSetLayouts        = &desc_set_layout_sbo,
	};

	VkDescriptorSetAllocateInfo descriptor_set_alloc_info_ubo = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool     = desc_pool_ubo,
		.descriptorSetCount = 1,
		.pSetLayouts        = &desc_set_layout_ubo,
	};

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_VK(vkAllocateDescriptorSets(qvk.device, &descriptor_set_alloc_info, desc_set_sbo + i));
		BufferResource_t *sbo = buf_stretch_pic_queue + i;

		VkDescriptorBufferInfo buf_info = {
			.buffer = sbo->buffer,
			.offset = 0,
			.range  = sizeof(StretchPic_t) * MAX_STRETCH_PICS,
		};

		VkWriteDescriptorSet output_buf_write = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = desc_set_sbo[i],
			.dstBinding      = 0,
			.dstArrayElement = 0,
			.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.pBufferInfo     = &buf_info,
		};

		vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

		_VK(vkAllocateDescriptorSets(qvk.device, &descriptor_set_alloc_info_ubo, desc_set_ubo + i));
		BufferResource_t *ubo = buf_ubo + i;

		VkDescriptorBufferInfo buf_info_ubo = {
			.buffer = ubo->buffer,
			.offset = 0,
			.range  = sizeof(StretchPic_UBO_t),
		};

		VkWriteDescriptorSet output_buf_write_ubo = {
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = desc_set_ubo[i],
			.dstBinding      = 2,
			.dstArrayElement = 0,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.pBufferInfo     = &buf_info_ubo,
		};

		vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write_ubo, 0, NULL);
	}
	return VK_SUCCESS;
}

/**
*	@brief	Destroy the draw system and free all Vulkan resources.
*	@return	VK_SUCCESS on success.
*	@note	Destroys render pass, descriptor pools, descriptor set layouts,
*			and per-frame storage/uniform buffers allocated during initialization.
*			Must be called during renderer shutdown to avoid resource leaks.
*			Pipelines and framebuffers are destroyed separately via vkpt_draw_destroy_pipelines().
**/
VkResult vkpt_draw_destroy() {
	LOG_FUNC();
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		buffer_destroy(buf_stretch_pic_queue + i);
		buffer_destroy(buf_ubo + i);
	}
	vkDestroyRenderPass(qvk.device, render_pass_stretch_pic, NULL);
	vkDestroyDescriptorPool(qvk.device, desc_pool_sbo, NULL);
	vkDestroyDescriptorSetLayout(qvk.device, desc_set_layout_sbo, NULL);
	vkDestroyDescriptorPool(qvk.device, desc_pool_ubo, NULL);
	vkDestroyDescriptorSetLayout(qvk.device, desc_set_layout_ubo, NULL);

	return VK_SUCCESS;
}



/**
*
*
*
*	Pipeline Creation and Destruction:
*
*
*
**/
/**
*	@brief	Destroy graphics pipelines and framebuffers.
*	@return	VK_SUCCESS on success.
*	@note	Destroys stretch pic pipelines (SDR/HDR variants), final blit pipeline,
*			pipeline layouts, and all swapchain framebuffers.
*			Called during resolution changes and renderer shutdown.
*			Pipelines can be recreated via vkpt_draw_create_pipelines().
**/
VkResult vkpt_draw_destroy_pipelines() {
	LOG_FUNC();
	for(int i = 0; i < STRETCH_PIC_NUM_PIPELINES; i++) {
		vkDestroyPipeline(qvk.device, pipeline_stretch_pic[i], NULL);
	}
	vkDestroyPipeline(qvk.device, pipeline_final_blit, NULL);
	vkDestroyPipelineLayout(qvk.device, pipeline_layout_stretch_pic, NULL);
	vkDestroyPipelineLayout(qvk.device, pipeline_layout_final_blit, NULL);
	for(int i = 0; i < qvk.num_swap_chain_images; i++) {
		vkDestroyFramebuffer(qvk.device, framebuffer_stretch_pic[i], NULL);
	}
	free(framebuffer_stretch_pic);
	framebuffer_stretch_pic = NULL;
	
	return VK_SUCCESS;
}

/**
*	@brief	Create graphics pipelines and framebuffers for stretch pic rendering.
*	@return	VK_SUCCESS on success, or Vulkan error code on failure.
*	@note	Creates:
*			- Pipeline layouts (stretch pic and final blit)
*			- Two stretch pic pipelines (SDR and HDR variants using specialization constants)
*			- Final blit pipeline for post-processing
*			- One framebuffer per swapchain image
*			
*			Pipeline features:
*			- Triangle strip topology (4 vertices per instance)
*			- No vertex input (vertices generated in shader)
*			- Alpha blending enabled (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
*			- Dynamic viewport and scissor state
*			- No depth testing (pure 2D rendering)
*			
*			Called during initialization and after resolution changes.
*			Pairs with vkpt_draw_destroy_pipelines() for cleanup.
**/
VkResult vkpt_draw_create_pipelines() {
	LOG_FUNC();

	assert(desc_set_layout_sbo);
	VkDescriptorSetLayout desc_set_layouts[] = {
		desc_set_layout_sbo, qvk.desc_set_layout_textures, desc_set_layout_ubo
	};
	CREATE_PIPELINE_LAYOUT(qvk.device, &pipeline_layout_stretch_pic, 
		.setLayoutCount = LENGTH(desc_set_layouts),
		.pSetLayouts    = desc_set_layouts
	);

	desc_set_layouts[0] = qvk.desc_set_layout_ubo;

	CREATE_PIPELINE_LAYOUT(qvk.device, &pipeline_layout_final_blit,
		.setLayoutCount = LENGTH(desc_set_layouts),
		.pSetLayouts = desc_set_layouts
	);

	VkSpecializationMapEntry specEntries[] = {
		{ .constantID = 0, .offset = 0, .size = sizeof(uint32_t) }
	};

	// "HDR display" flag
	uint32_t spec_data[] = {
		0,
		1,
	};

	VkSpecializationInfo specInfo_SDR = {};
	specInfo_SDR.mapEntryCount = 1;
	specInfo_SDR.pMapEntries = specEntries;
	specInfo_SDR.dataSize = sizeof(uint32_t);
	specInfo_SDR.pData = &spec_data[0];

	VkSpecializationInfo specInfo_HDR = {};
	specInfo_HDR.mapEntryCount = 1;
	specInfo_HDR.pMapEntries = specEntries;
	specInfo_HDR.dataSize = sizeof(uint32_t);
	specInfo_HDR.pData = &spec_data[1];

	VkPipelineShaderStageCreateInfo shader_info_SDR[] = {
		SHADER_STAGE(QVK_MOD_STRETCH_PIC_VERT, VK_SHADER_STAGE_VERTEX_BIT),
		SHADER_STAGE_SPEC(QVK_MOD_STRETCH_PIC_FRAG, VK_SHADER_STAGE_FRAGMENT_BIT, &specInfo_SDR)
	};

	VkPipelineShaderStageCreateInfo shader_info_HDR[] = {
		SHADER_STAGE(QVK_MOD_STRETCH_PIC_VERT, VK_SHADER_STAGE_VERTEX_BIT),
		SHADER_STAGE_SPEC(QVK_MOD_STRETCH_PIC_FRAG, VK_SHADER_STAGE_FRAGMENT_BIT, &specInfo_HDR)
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount   = 0,
		.pVertexBindingDescriptions      = NULL,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions    = NULL,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		.primitiveRestartEnable = VK_FALSE, /* VK_TRUE ?? */
	};

	VkViewport viewport = {
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = (float) vkpt_draw_get_extent().width,
		.height   = (float) vkpt_draw_get_extent().height,
		.minDepth = -1.0f,
		.maxDepth = 1.0f,
	};

	VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = vkpt_draw_get_extent(),
	};

	//VkPipelineViewportStateCreateInfo viewport_state = {
	//	.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	//	//.pNext = NULL,
	//	.viewportCount = 1,
	//	//.pViewports = &viewport,
	//	.scissorCount = 1,
	//	//.pScissors = &scissor,
	//};
	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = NULL,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor,
	};

	VkPipelineRasterizationStateCreateInfo rasterizer_state = {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable        = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE, /* skip rasterizer */
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.lineWidth               = 1.0f,
		.cullMode                = VK_CULL_MODE_NONE/*VK_CULL_MODE_BACK_BIT*/,
		.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable         = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp          = 0.0f,
		.depthBiasSlopeFactor    = 0.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisample_state = {
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable   = VK_FALSE,
		.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading      = 1.0f,
		.pSampleMask           = NULL,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable      = VK_FALSE,
	};

	VkPipelineColorBlendAttachmentState color_blend_attachment = {
		.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT
			                 | VK_COLOR_COMPONENT_G_BIT
			                 | VK_COLOR_COMPONENT_B_BIT
			                 | VK_COLOR_COMPONENT_A_BIT,
		.blendEnable         = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp        = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaBlendOp        = VK_BLEND_OP_ADD,
	};

	VkPipelineColorBlendStateCreateInfo color_blend_state = {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable   = VK_FALSE,
		.logicOp         = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments    = &color_blend_attachment,
		.blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f },
	};

	// WID: Scissor and Viewport dynamic state.
	const VkDynamicState dynamic_states_enabled[ 2 ] = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	const VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = NULL,
		.pDynamicStates = &dynamic_states_enabled[0],
		.dynamicStateCount = 2,
		.flags = 0
	};

	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount          = LENGTH(shader_info_SDR),

		.pVertexInputState   = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pViewportState      = &viewport_state,
		.pRasterizationState = &rasterizer_state,
		.pMultisampleState   = &multisample_state,
		.pDepthStencilState  = NULL,
		.pColorBlendState    = &color_blend_state,
		// WID: Add in support for dynamic scissor clip areas.
		//.pDynamicState       = NULL,
		.pDynamicState		 = &dynamic_state,
		
		.layout              = pipeline_layout_stretch_pic,
		.renderPass          = render_pass_stretch_pic,
		.subpass             = 0,

		.basePipelineHandle  = VK_NULL_HANDLE,
		.basePipelineIndex   = -1,
	};

	pipeline_info.pStages = shader_info_SDR;
	_VK(vkCreateGraphicsPipelines(qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline_stretch_pic[STRETCH_PIC_SDR]));
	ATTACH_LABEL_VARIABLE(pipeline_stretch_pic[STRETCH_PIC_SDR], PIPELINE);

	pipeline_info.pStages = shader_info_HDR;
	_VK(vkCreateGraphicsPipelines(qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline_stretch_pic[STRETCH_PIC_HDR]));
	ATTACH_LABEL_VARIABLE(pipeline_stretch_pic[STRETCH_PIC_HDR], PIPELINE);


	VkPipelineShaderStageCreateInfo shader_info_final_blit[] = {
		SHADER_STAGE(QVK_MOD_FINAL_BLIT_VERT, VK_SHADER_STAGE_VERTEX_BIT),
		SHADER_STAGE(QVK_MOD_FINAL_BLIT_LANCZOS_FRAG, VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	pipeline_info.pStages = shader_info_final_blit;
	pipeline_info.layout = pipeline_layout_final_blit;

	_VK(vkCreateGraphicsPipelines(qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline_final_blit));
	ATTACH_LABEL_VARIABLE(pipeline_final_blit, PIPELINE);

	framebuffer_stretch_pic = (VkFramebuffer*)malloc(qvk.num_swap_chain_images * sizeof(*framebuffer_stretch_pic));
	for(int i = 0; i < qvk.num_swap_chain_images; i++) {
		VkImageView attachments[] = {
			qvk.swap_chain_image_views[i]
		};

		VkFramebufferCreateInfo fb_create_info = {
			.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass      = render_pass_stretch_pic,
			.attachmentCount = 1,
			.pAttachments    = attachments,
			.width           = vkpt_draw_get_extent().width,
			.height          = vkpt_draw_get_extent().height,
			.layers          = 1,
		};

		_VK(vkCreateFramebuffer(qvk.device, &fb_create_info, NULL, framebuffer_stretch_pic + i));
		ATTACH_LABEL_VARIABLE(framebuffer_stretch_pic[i], FRAMEBUFFER);
	}

	return VK_SUCCESS;
}



/**
*
*
*
*	Stretch Pic Rendering:
*
*
*
**/
/**
*	@brief	Clear the stretch pic queue for a new frame.
*	@return	VK_SUCCESS on success.
*	@note	Resets stretch pic count and scissor groups to default state.
*			Called at the beginning of each frame before client rendering code.
*			After this call, the queue is empty and ready for new pics.
**/
VkResult vkpt_draw_clear_stretch_pics() {
	// We always resort to the default scissor group.
	vkpt_draw_clear_scissor_groups();

	// Clear out number of stretch pics.
	num_stretch_pics = 0;

	// Success.
	return VK_SUCCESS;
}

/**
*	@brief	Submit all queued stretch pics for rendering to the swapchain.
*	@param	cmd_buf		Vulkan command buffer to record rendering commands into.
*	@return	VK_SUCCESS on success, or VK_ERROR if rendering fails.
*	@note	Rendering algorithm:
*			1. Upload stretch pic queue to GPU storage buffer
*			2. Upload HDR parameters to uniform buffer
*			3. Begin render pass on current swapchain framebuffer
*			4. For each scissor group:
*			   - Set dynamic viewport to full screen
*			   - Set dynamic scissor rectangle to group's clip rect
*			   - Bind descriptor sets (SBO, textures, UBO)
*			   - Bind appropriate pipeline (SDR or HDR based on display type)
*			   - Draw instanced: 4 vertices × num_pics_in_group
*			5. End render pass
*			6. Reset queue and scissor groups for next frame
*			
*			Uses instanced rendering for efficiency: vertex shader reads per-instance
*			data from SBO and generates quad geometry procedurally. Each instance
*			draws a single stretch pic with its own position, UV, color, and transform.
*			
*			Early-out if num_stretch_pics == 0 (nothing to draw).
**/
VkResult vkpt_draw_submit_stretch_pics(VkCommandBuffer cmd_buf) {
	if (num_stretch_pics == 0)
		return VK_SUCCESS;

	BufferResource_t *buf_spq = buf_stretch_pic_queue + qvk.current_frame_index;
	StretchPic_t *spq_dev = (StretchPic_t *) buffer_map(buf_spq);
	memcpy(spq_dev, stretch_pic_queue, sizeof(StretchPic_t) * num_stretch_pics);
	buffer_unmap(buf_spq);
	spq_dev = NULL;

	BufferResource_t *ubo_res = buf_ubo + qvk.current_frame_index;
	StretchPic_UBO_t *ubo = (StretchPic_UBO_t *) buffer_map(ubo_res);
	ubo->ui_hdr_nits = cvar_ui_hdr_nits->value;
	ubo->tm_hdr_saturation_scale = cvar_tm_hdr_saturation_scale->value;
	buffer_unmap(ubo_res);
	ubo = NULL;

	VkRenderPassBeginInfo render_pass_info = {
		.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass        = render_pass_stretch_pic,
		.framebuffer       = framebuffer_stretch_pic[qvk.current_swap_chain_image_index],
		.renderArea.offset = { 0, 0 },
		.renderArea.extent = vkpt_draw_get_extent(),
	};

	VkDescriptorSet desc_sets[] = {
		desc_set_sbo[qvk.current_frame_index],
		qvk_get_current_desc_set_textures(),
		desc_set_ubo[qvk.current_frame_index],
	};

	// WID: This does 1 single draw call, for all instances. Effectively not allowing us to clip
	// by passing in a different scissor area.
	#if 0
	//vkCmdBeginRenderPass(cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	//vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//		pipeline_layout_stretch_pic, 0, LENGTH(desc_sets), desc_sets, 0, 0);
	//vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_stretch_pic[qvk.surf_is_hdr ? STRETCH_PIC_HDR : STRETCH_PIC_SDR]);
	//vkCmdDraw(cmd_buf, 4, num_stretch_pics, 0, 0);
	//vkCmdEndRenderPass(cmd_buf);
	#endif
	// WID: This will just render all pics in 1 render pass, but with multiple vkCmdDraw calls.
	#if 0
	//vkCmdBeginRenderPass( cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE );
	//for ( int32_t i = 0; i < num_stretch_pics; i++ ) {
	//	vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//		pipeline_layout_stretch_pic, 0, LENGTH( desc_sets ), desc_sets, 0, 0 );
	//	vkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_stretch_pic[ qvk.surf_is_hdr ? STRETCH_PIC_HDR : STRETCH_PIC_SDR ] );
	//	vkCmdDraw( cmd_buf, 4, 1, 0, i );
	//}
	//vkCmdEndRenderPass( cmd_buf );
	#endif
	// WID: This does 1 single draw call, for all instances. Effectively not allowing us to clip
	// by passing in a different scissor area.
	#if 1
	// Begin Render Pass.
	vkCmdBeginRenderPass( cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE );
	for ( int32_t group_index = 0; group_index < num_stretch_pic_scissor_groups; group_index++ ) {
		// Apply scissor.
		StretchPic_Scissor_Group *scissor_group = &stretch_pic_scissor_groups[ group_index ];

		// Apply viewport.
		VkViewport viewport = {
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)vkpt_draw_get_extent().width,
			.height = (float)vkpt_draw_get_extent().height,
			.minDepth = -1.0f,
			.maxDepth = 1.0f,
		};
		vkCmdSetViewport( cmd_buf, 0, 1, &viewport );

		// Bind Descriptor Sets.
		vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline_layout_stretch_pic, 0, LENGTH( desc_sets ), desc_sets, 0, 0 );
		// Bind Pipeline.
		vkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_stretch_pic[ qvk.surf_is_hdr ? STRETCH_PIC_HDR : STRETCH_PIC_SDR ] );
		// Apply scissor rectangle area.
		VkRect2D scissor_rect = {
			.offset = {
				.x = scissor_group->clip_rect.left,
				.y = scissor_group->clip_rect.top,
			},
			.extent = {
				.width = scissor_group->clip_rect.right - scissor_group->clip_rect.left,
				.height = scissor_group->clip_rect.bottom - scissor_group->clip_rect.top
			}
		};
		vkCmdSetScissor( cmd_buf, 0, 1, &scissor_rect );
		// Draw the group of instances that reside in this area group.
		vkCmdDraw( cmd_buf, 4, scissor_group->num_stretch_pic_count, 0, scissor_group->num_stretch_pic_offset );
	}
	// Finish the render pass.
	vkCmdEndRenderPass( cmd_buf );
	#endif

	// We always resort to the default scissor group.
	vkpt_draw_clear_scissor_groups();

	num_stretch_pics = 0;

	return VK_SUCCESS;
}



/**
*
*
*
*	Final Blit Operations:
*
*
*
**/
/**
*	@brief	Perform a simple image blit from source image to swapchain (no filtering).
*	@param	cmd_buf		Vulkan command buffer to record blit commands into.
*	@param	image		Source image to blit from (typically rendered scene).
*	@param	extent		Extent of the source image.
*	@return	VK_SUCCESS on success.
*	@note	Performs a nearest-neighbor blit from source image to swapchain image.
*			Transitions source image from GENERAL to TRANSFER_SRC_OPTIMAL.
*			Transitions swapchain image from PRESENT_SRC_KHR to TRANSFER_DST_OPTIMAL.
*			After blit, transitions swapchain image to PRESENT_SRC_KHR for presentation.
*			Used for simple resolution scaling without filtering or post-processing.
**/
VkResult vkpt_final_blit_simple(VkCommandBuffer cmd_buf, VkImage image, VkExtent2D extent) {
	VkImageSubresourceRange subresource_range = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	IMAGE_BARRIER(cmd_buf,
		.image = qvk.swap_chain_images[qvk.current_swap_chain_image_index],
		.subresourceRange = subresource_range,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	IMAGE_BARRIER(cmd_buf,
		.image = image,
		.subresourceRange = subresource_range,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	);

	VkOffset3D blit_size = {
		.x = extent.width,
		.y = extent.height,
		.z = 1
	};
	VkOffset3D blit_size_unscaled = {
		.x = qvk.extent_unscaled.width,.y = qvk.extent_unscaled.height,.z = 1
	};
	VkImageBlit img_blit = {
		.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.srcOffsets = { [1] = blit_size },
		.dstOffsets = { [1] = blit_size_unscaled },
	};
	vkCmdBlitImage(cmd_buf,
		image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		qvk.swap_chain_images[qvk.current_swap_chain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &img_blit, VK_FILTER_NEAREST);

	IMAGE_BARRIER(cmd_buf,
		.image = qvk.swap_chain_images[qvk.current_swap_chain_image_index],
		.subresourceRange = subresource_range,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = 0,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	);

	IMAGE_BARRIER(cmd_buf,
		.image = image,
		.subresourceRange = subresource_range,
		.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_GENERAL
	);

	return VK_SUCCESS;
}

/**
*	@brief	Perform a filtered blit using the final blit pipeline (for post-processing).
*	@param	cmd_buf		Vulkan command buffer to record rendering commands into.
*	@return	VK_SUCCESS on success.
*	@note	Uses the final_blit graphics pipeline to render a full-screen quad.
*			Allows shader-based filtering, scaling, and post-processing effects.
*			Binds global UBO and texture descriptors for shader access.
*			Renders a single triangle strip (4 vertices) covering the screen.
*			Used when custom filtering or effects are needed beyond simple blitting.
**/
VkResult vkpt_final_blit_filtered(VkCommandBuffer cmd_buf) {
	VkRenderPassBeginInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = render_pass_stretch_pic,
		.framebuffer = framebuffer_stretch_pic[qvk.current_swap_chain_image_index],
		.renderArea.offset = { 0, 0 },
		.renderArea.extent = vkpt_draw_get_extent()
	};

	VkDescriptorSet desc_sets[] = {
		qvk.desc_set_ubo,
		qvk_get_current_desc_set_textures()
	};

	vkCmdBeginRenderPass(cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout_final_blit, 0, LENGTH(desc_sets), desc_sets, 0, 0);
	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_final_blit);
	//// Apply viewport.
	//VkViewport viewport = {
	//	.x = 0.0f,
	//	.y = 0.0f,
	//	.width = (float)vkpt_draw_get_extent().width,
	//	.height = (float)vkpt_draw_get_extent().height,
	//	.minDepth = -1.0f,
	//	.maxDepth = 1.0f,
	//};
	//vkCmdSetViewport( cmd_buf, 0, 1, &viewport );
	//VkRect2D scissor_rect = {
	//	.offset = {
	//		.x = 0,
	//		.y = 0,
	//	},
	//	.extent = {
	//		.width = (float)vkpt_draw_get_extent().width,
	//		.height = (float)vkpt_draw_get_extent().height,
	//	}
	//};
	//vkCmdSetScissor( cmd_buf, 0, 1, &scissor_rect );
	vkCmdDraw(cmd_buf, 4, 1, 0, 0);
	vkCmdEndRenderPass(cmd_buf);

	return VK_SUCCESS;
}



/**
*
*
*
*	Refresh API - Client Rendering Functions:
*
*
*
**/
/**
*	@brief	Set the clip rectangle for subsequent draw calls.
*	@param	clip	Pointer to clipRect_t structure defining the clipping region, or NULL to disable clipping.
*	@note	Creates a new scissor group for batching draw calls with the same clip rectangle.
*			When clip is NULL, clipping is disabled and the full screen is used.
*			Each call to this function starts a new scissor group, allowing different
*			UI regions (dialogs, panels, scroll areas) to have independent clipping.
*			The scissor rectangle is applied during vkpt_draw_submit_stretch_pics().
**/
void R_SetClipRect_RTX(const clipRect_t *clip) 
{ 
	// We're in for another scissor group.
	const uint32_t scissor_group_index = num_stretch_pic_scissor_groups++;

	// Specify clip rectangle area:
	if (clip) {
		clip_enable = true;
		clip_rect = *clip;
	// Resort to defaults:
	} else {
		clip_enable = false;
		clip_rect.left = 0;
		clip_rect.top = 0;
		clip_rect.right = vkpt_draw_get_extent().width;
		clip_rect.bottom = vkpt_draw_get_extent().height;
	}

	// Get scissor group pointer.
	StretchPic_Scissor_Group *scissor_group = &stretch_pic_scissor_groups[ scissor_group_index ];
	// Update its scissor rect.
	scissor_group->clip_rect = clip_rect;
	// It has no pics yet.
	scissor_group->num_stretch_pic_count = 0;
	// It does have an offset.
	scissor_group->num_stretch_pic_offset = num_stretch_pics;
}

/**
*	@brief	Reset draw color to white (no tint).
*	@note	Sets both primary and secondary color slots to U32_WHITE (0xFFFFFFFF).
*			Subsequent draw calls will render with no color tint applied.
**/
void
R_ClearColor_RTX(void)
{
	draw.colors[0].u32 = U32_WHITE;
	draw.colors[1].u32 = U32_WHITE;
}

/**
*	@brief	Set the global alpha transparency value for subsequent draws.
*	@param	alpha	Alpha value in range [0.0, 1.0] (0 = transparent, 1 = opaque).
*	@note	Applies inverse sRGB gamma correction (power 0.4545) to alpha value.
*			This corrects for sRGB rendering and ensures linear alpha blending.
*			Alpha is converted to 0-255 range and applied to both color slots.
**/
void
R_SetAlpha_RTX(float alpha)
{
    alpha = powf(fabsf(alpha), 0.4545f); // un-sRGB the alpha
	draw.colors[0].u8[3] = draw.colors[1].u8[3] = alpha * 255;
}

/**
*	@brief	Set the global alpha scaling factor.
*	@param	alpha	Alpha scale in range [0.0, 1.0] (multiplied with per-draw alpha).
*	@note	Provides a secondary alpha control that scales all alpha values.
*			When alpha_scale is 0, all draws are skipped (early-out optimization).
*			Useful for fading entire UI elements or screens in/out.
**/
void
R_SetAlphaScale_RTX(float alpha)
{
	draw.alpha_scale = alpha;
}

/**
*	@brief	Set the draw color for subsequent rendering.
*	@param	color	RGBA8 color value (format: 0xAABBGGRR).
*	@note	Sets primary color and copies alpha to secondary color slot.
*			Color is applied as a multiplicative tint to texture samples.
**/
void
R_SetColor_RTX(uint32_t color)
{
	draw.colors[0].u32   = color;
	draw.colors[1].u8[3] = draw.colors[0].u8[3];
}

/**
*	@brief	Sample world lighting at a point in 3D space.
*	@param	origin	3D world position to sample lighting at.
*	@param	light	Output: RGB light color/intensity vector.
*	@note	Stub implementation: Always returns white light (1, 1, 1).
*			In a full implementation, would sample lightmap or light grid.
*			Currently unused for 2D UI rendering.
**/
void
R_LightPoint_RTX(const vec3_t origin, vec3_t light)
{
	VectorSet(light, 1, 1, 1);
}

/**
*	@brief	Set the global UI scaling factor.
*	@param	scale	Scale multiplier (1.0 = native resolution, 2.0 = double size).
*	@note	Affects coordinate space for subsequent draw calls.
*			Useful for high-DPI displays or user-configurable UI scaling.
**/
void
R_SetScale_RTX(float scale)
{
	draw.scale = scale;
}

/**
*	@brief	Draw a stretched image (texture quad) to the screen.
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	w		Width in screen pixels.
*	@param	h		Height in screen pixels.
*	@param	pic		Image handle from r_images array.
*	@note	Renders texture with full UV coverage (0,0 to 1,1).
*			Applies current draw color, alpha, and scale settings.
*			Uses a small epsilon offset to prevent texture sampling artifacts at edges.
**/
void
R_DrawStretchPic_RTX(int x, int y, int w, int h, qhandle_t pic ) {
	float eps = +1e-5f; /* fixes some ugly artifacts */
	enqueue_stretch_pic(
		x,    y,    w,    h,
		0.0f + eps, 0.0f + eps, 1.0f - eps, 1.0f - eps,
		draw.colors[0].u32, pic);
}

/**
*	@brief	Draw a stretched image with rotation support.
*	@param	x			Screen-space X position (left edge).
*	@param	y			Screen-space Y position (top edge).
*	@param	w			Width in screen pixels.
*	@param	h			Height in screen pixels.
*	@param	angle		Rotation angle in degrees (clockwise).
*	@param	pivot_x		X coordinate of rotation pivot (relative to x).
*	@param	pivot_y		Y coordinate of rotation pivot (relative to y).
*	@param	pic			Image handle from r_images array.
*	@note	Renders texture with full UV coverage (0,0 to 1,1).
*			Rotation is performed in vertex shader around specified pivot point.
*			Useful for animated UI elements like spinning icons or compass needles.
**/
void
R_DrawRotateStretchPic_RTX( int x, int y, int w, int h, float angle, int pivot_x, int pivot_y, qhandle_t pic ) {
	float eps = +1e-5f; /* fixes some ugly artifacts */
	enqueue_stretch_rotate_pic(
		x, y, w, h,
		0.0f + eps, 0.0f + eps, 1.0f - eps, 1.0f - eps,
		angle, pivot_x, pivot_y,
		draw.colors[ 0 ].u32, pic, 0 );
}

/**
*	@brief	Draw an image at native size (no stretching).
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	pic		Image handle from r_images array.
*	@note	Automatically queries image dimensions and calls R_DrawStretchPic.
*			Image is rendered at its original texture resolution.
**/
void
R_DrawPic_RTX(int x, int y, qhandle_t pic)
{
	image_t *image = IMG_ForHandle(pic);
	R_DrawStretchPic(x, y, image->width, image->height, pic);
}

/**
*	@brief	Draw a portion of an image with custom source and destination rectangles.
*	@param	destX		Destination X position on screen.
*	@param	destY		Destination Y position on screen.
*	@param	destW		Destination width on screen.
*	@param	destH		Destination height on screen.
*	@param	pic			Image handle from r_images array.
*	@param	srcX		Source X position in texture (pixels).
*	@param	srcY		Source Y position in texture (pixels).
*	@param	srcW		Source width in texture (pixels).
*	@param	srcH		Source height in texture (pixels).
*	@note	Allows rendering a sub-region of a texture (sprite sheet, atlas).
*			Coordinates are converted to normalized UV space (0.0 to 1.0).
*			Useful for icon atlases, sprite sheets, and texture packing.
**/
void
R_DrawPicEx_RTX( double destX, double destY, double destW, double destH, qhandle_t pic,
	double srcX, double srcY, double srcW, double srcH ) {

	image_t *image = IMG_ForHandle( pic );

	float eps = +1e-5f; /* fixes some ugly artifacts */

	// Calculate the source coordinates in normalized texture space.
	double s0 = srcX / image->width;
	double t0 = srcY / image->height;
	double s1 = ( srcX + srcW ) / image->width;
	double t1 = ( srcY + srcH ) / image->height;
	enqueue_stretch_pic(
		destX, destY, destW, destH,
		s0 + eps, t0 + eps, s1 - eps, t1 - eps,
		draw.colors[ 0 ].u32, pic );
}

/**
*	@brief	Draw a raw image (dynamic/cinematics) to the screen.
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	w		Width in screen pixels.
*	@param	h		Height in screen pixels.
*	@note	Uses the currently registered raw image (qvk.raw_image).
*			Raw images are dynamic textures updated via R_UpdateRawPic_RTX.
*			Returns early if no raw image is registered.
*			Typically used for video playback or animated cinematics.
**/
void
R_DrawStretchRaw_RTX(int x, int y, int w, int h)
{
	if(!qvk.raw_image)
		return;
	R_DrawStretchPic(x, y, w, h, qvk.raw_image - r_images);
}

/**
*	@brief	Update the raw image texture with new pixel data.
*	@param	pic_w		Width of the raw image in pixels.
*	@param	pic_h		Height of the raw image in pixels.
*	@param	pic			Pointer to RGBA8 pixel data.
*	@note	Unregisters the previous raw image (if any) and registers a new one.
*			Raw image is given a unique name using a sequential ID counter.
*			Memory is allocated and data is copied before registration.
*			Used for video playback where texture content changes each frame.
**/
void
R_UpdateRawPic_RTX(int pic_w, int pic_h, const uint32_t *pic)
{
	if(qvk.raw_image)
		R_UnregisterImage(qvk.raw_image - r_images);

	size_t raw_size = pic_w * pic_h * 4;
	byte *raw_data = Z_Malloc(raw_size);
	memcpy(raw_data, pic, raw_size);
	static int raw_id;
	qvk.raw_image = r_images + R_RegisterRawImage(va("**raw[%d]**", raw_id++), pic_w, pic_h, raw_data, IT_SPRITE, IF_SRGB);
}

/**
*	@brief	Discard the current raw image texture.
*	@note	Unregisters the raw image (if any) and sets qvk.raw_image to NULL.
*			Called when video playback stops or cinematic ends.
*			Frees GPU resources associated with the raw texture.
**/
void
R_DiscardRawPic_RTX(void)
{
	if(qvk.raw_image) {
		R_UnregisterImage(qvk.raw_image - r_images);
		qvk.raw_image = NULL;
	}
}

/**
*	@brief	Draw an image with aspect ratio preservation (letterboxing/pillarboxing).
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	w		Width in screen pixels.
*	@param	h		Height in screen pixels.
*	@param	pic		Image handle from r_images array.
*	@note	Adjusts UV coordinates to maintain image aspect ratio within destination rect.
*			If image aspect doesn't match dest aspect, adds black bars (letterbox/pillarbox).
*			Scrap images (font atlases) bypass this and use normal stretch.
*			Useful for displaying widescreen images on 4:3 displays and vice versa.
**/
void R_DrawKeepAspectPic_RTX(int x, int y, int w, int h, qhandle_t pic)
{
    image_t *image = IMG_ForHandle(pic);

    if (image->flags & IF_SCRAP) {
        R_DrawStretchPic_RTX(x, y, w, h, pic);
        return;
    }

    float scale_w = w;
    float scale_h = h * image->aspect;
    float scale = max(scale_w, scale_h);

    float s = (1.0f - scale_w / scale) * 0.5f;
    float t = (1.0f - scale_h / scale) * 0.5f;

    enqueue_stretch_pic(x, y, w, h, s, t, 1.0f - s, 1.0f - t, draw.colors[0].u32, pic);
}

//! Division constant for texture tiling (1/64).
#define DIV64 (1.0f / 64.0f)

/**
*	@brief	Draw a tiled texture (repeating pattern) to fill a rectangle.
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	w		Width in screen pixels.
*	@param	h		Height in screen pixels.
*	@param	pic		Image handle from r_images array (typically 64×64 tile).
*	@note	UV coordinates are scaled to repeat the texture across the destination.
*			Assumes texture is 64×64 pixels (DIV64 = 1/64).
*			Each 64 pixels on screen corresponds to 1.0 in UV space (one tile repeat).
*			Used for tiled backgrounds, console backgrounds, and repeating patterns.
**/
void
R_TileClear_RTX(int x, int y, int w, int h, qhandle_t pic)
{
	enqueue_stretch_pic(x, y, w, h,
		x * DIV64, y * DIV64, (x + w) * DIV64, (y + h) * DIV64,
		U32_WHITE, pic);
}

/**
*	@brief	Draw a solid color-filled rectangle (8-bit palette index).
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	w		Width in screen pixels.
*	@param	h		Height in screen pixels.
*	@param	c		8-bit palette color index.
*	@note	Uses TEXNUM_WHITE (solid white texture) with palette color applied.
*			Palette index is looked up in d_8to24table to get RGBA8 color.
*			Early-out if width or height is zero.
*			Used for simple colored rectangles in menus and HUD.
**/
void
R_DrawFill8_RTX(int x, int y, int w, int h, int c)
{
	if(!w || !h)
		return;
	enqueue_stretch_pic(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		d_8to24table[c & 0xff], TEXNUM_WHITE);
}

/**
*	@brief	Draw a solid color-filled rectangle (32-bit RGBA).
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	w		Width in screen pixels.
*	@param	h		Height in screen pixels.
*	@param	color	32-bit RGBA8 color value (format: 0xAABBGGRR).
*	@note	Uses TEXNUM_WHITE (solid white texture) with specified color applied.
*			Early-out if width or height is zero.
*			Allows arbitrary RGB colors with alpha transparency.
**/
void
R_DrawFill32_RTX(int x, int y, int w, int h, uint32_t color)
{
	if(!w || !h)
		return;
	enqueue_stretch_pic(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		color, TEXNUM_WHITE);
}

/**
*	@brief	Draw a solid color-filled rectangle with floating-point coordinates (8-bit palette).
*	@param	x		Screen-space X position (left edge, float).
*	@param	y		Screen-space Y position (top edge, float).
*	@param	w		Width in screen pixels (float).
*	@param	h		Height in screen pixels (float).
*	@param	c		8-bit palette color index.
*	@note	Floating-point variant of R_DrawFill8_RTX for sub-pixel positioning.
*			Useful for smooth animations and high-DPI rendering.
**/
void
R_DrawFill8f_RTX( float x, float y, float w, float h, int32_t c ) {
	if ( !w || !h )
		return;
	enqueue_stretch_pic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		d_8to24table[ c & 0xff ], TEXNUM_WHITE );
}

/**
*	@brief	Draw a solid color-filled rectangle with floating-point coordinates (32-bit RGBA).
*	@param	x		Screen-space X position (left edge, float).
*	@param	y		Screen-space Y position (top edge, float).
*	@param	w		Width in screen pixels (float).
*	@param	h		Height in screen pixels (float).
*	@param	color	32-bit RGBA8 color value (format: 0xAABBGGRR).
*	@note	Floating-point variant of R_DrawFill32_RTX for sub-pixel positioning.
*			Useful for smooth animations and high-DPI rendering.
**/
void
R_DrawFill32f_RTX( float x, float y, float w, float h, uint32_t color ) {
	if ( !w || !h )
		return;
	enqueue_stretch_pic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		color, TEXNUM_WHITE );
}



/**
*
*
*
*	Text Rendering:
*
*
*
**/
/**
*	@brief	Draw a single character from a monospace font atlas.
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	flags	Rendering flags (UI_ALTCOLOR, UI_XORCOLOR).
*	@param	c		Character code (ASCII, may have color bits in high byte).
*	@param	font	Font texture handle from r_images array.
*	@note	Font atlas layout: 16×16 grid of characters (256 total).
*			Each character occupies 1/16 of texture space (0.0625 UVs).
*			
*			Character code handling:
*			- Lower 7 bits: ASCII character (0-127)
*			- Bit 7: Color selection (0 = colors[0], 1 = colors[1])
*			- UI_ALTCOLOR flag: Force alternate color (sets bit 7)
*			- UI_XORCOLOR flag: Toggle color bit (XOR with bit 7)
*			
*			Space characters (ASCII 32) are skipped (no rendering).
*			
*			Character dimensions: CHAR_WIDTH × CHAR_HEIGHT pixels.
*			UV epsilon (1e-5) prevents sampling artifacts at glyph edges.
**/
static inline void
draw_char(int x, int y, int flags, int c, qhandle_t font)
{
	if ((c & 127) == 32) {
		return;
	}

	if (flags & UI_ALTCOLOR) {
		c |= 0x80;
	}
	if (flags & UI_XORCOLOR) {
		c ^= 0x80;
	}

	float s = (c & 15) * 0.0625f;
	float t = (c >> 4) * 0.0625f;

	float eps = 1e-5f; /* fixes some ugly artifacts */

	enqueue_stretch_pic(x, y, CHAR_WIDTH, CHAR_HEIGHT,
		s + eps, t + eps, s + 0.0625f - eps, t + 0.0625f - eps,
		draw.colors[c >> 7].u32, font);
}

/**
*	@brief	Render a single character to the screen (public API).
*	@param	x		Screen-space X position (left edge).
*	@param	y		Screen-space Y position (top edge).
*	@param	flags	Rendering flags (UI_ALTCOLOR, UI_XORCOLOR).
*	@param	c		Character code (ASCII with optional color bits).
*	@param	font	Font texture handle from r_images array.
*	@note	Masks character code to 8 bits (0-255) before rendering.
*			See draw_char() for detailed character rendering behavior.
**/
void
R_DrawChar_RTX(int x, int y, int flags, int c, qhandle_t font)
{
	draw_char(x, y, flags, c & 255, font);
}

/**
*	@brief	Render a string of text to the screen.
*	@param	x		Screen-space X position (left edge of first character).
*	@param	y		Screen-space Y position (top edge).
*	@param	flags	Rendering flags (UI_ALTCOLOR, UI_XORCOLOR) applied to all characters.
*	@param	maxlen	Maximum number of characters to render (0 = unlimited).
*	@param	s		Null-terminated string to render.
*	@param	font	Font texture handle from r_images array.
*	@return	Screen-space X position after the last rendered character.
*	@note	Characters are rendered left-to-right with fixed spacing (CHAR_WIDTH).
*			Stops rendering when maxlen is reached or null terminator is found.
*			Each character advances X position by CHAR_WIDTH pixels.
*			Returned X value can be used for cursor positioning or string measurement.
**/
int
R_DrawString_RTX(int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font)
{
	while(maxlen-- && *s) {
		byte c = *s++;
		draw_char(x, y, flags, c, font);
		x += CHAR_WIDTH;
	}

	return x;
}

// vim: shiftwidth=4 noexpandtab tabstop=4 cindent
