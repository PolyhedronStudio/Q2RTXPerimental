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
#include "refresh/fonts_mtsdf.h"

#include <assert.h>

#include "vkpt.h"
#include "debug/debug_draw.h"
#include "shader/global_textures.h"

enum {
	STRETCH_PIC_SDR,
	STRETCH_PIC_HDR,
	STRETCH_PIC_NUM_PIPELINES
};

#define TEXNUM_WHITE (~0)
#define MAX_STRETCH_PICS (1<<14)

static drawStatic_t draw = {
	.scale = 1.0f,
	.alpha_scale = 1.0f,
	.style_flags = STYLE_FLAG_NONE
};

//! Total number of stretch pics.
static int num_stretch_pics = 0;
typedef struct {
	float x, y, w, h;		// 4 floats = 16 bytes.
	float s, t, w_s, h_t;	// 4 floats = 16 bytes.
	// 32 bytes up till here.
	uint32_t color, tex_handle;	// 2 uint32_t	= 8 bytes.
	float	pivot_x, pivot_y;	// 2 float		= 8 bytes.
	// 48 bytes up till here.
	// These are pads to keep memory properly aligned with GLSL.
	float	angle, view_depth;	// 2 float = 8 bytes. (view_depth used for 3D depth test)
	// 56 bytes up till here.
	float	pad02, pad03;	// 2 float = 8 bytes.
	// 64 bytes up till here.
	//
	// Now adding the matrix:
	float	matTransform[16]; // 16 float = 64 bytes.
	// Total: 128 bytes base.

	// Extended Style Payload (128 bytes, total 256 bytes):
	uint32_t stroke_colors[4];      //!< 16 bytes: Top, Right, Bottom, Left stroke outline colors
	float    stroke_thickness[4];   //!< 16 bytes: Top, Right, Bottom, Left stroke outline thicknesses
	uint32_t outer_glow_colors[4];   //!< 16 bytes: Top, Right, Bottom, Left outer glow colors
	float    outer_glow_radius[4];   //!< 16 bytes: Top, Right, Bottom, Left outer glow radii
	uint32_t inner_glow_colors[4];   //!< 16 bytes: Top, Right, Bottom, Left inner glow colors
	float    inner_glow_radius[4];   //!< 16 bytes: Top, Right, Bottom, Left inner glow radii
	float    corner_radii[4];       //!< 16 bytes: TL, TR, BR, BL corner radii
	uint32_t style_flags;           //!< 4 bytes: STYLE_FLAG_* bitmask
	uint32_t sdf_tex_handle;        //!< 4 bytes: optional SDF silhouette/MTSDF texture
	float    sdf_pixel_range;       //!< 4 bytes: distance range in pixels
	float    pad_style[1];          //!< 4 bytes: align to 16 bytes
} StretchPic_t;
static_assert( sizeof( StretchPic_t ) == 256, "StretchPic_t must be exactly 256 bytes for std430 alignment" );

//! Not using global UBO b/c it's only filled when a world is drawn, but here we need it all the time
typedef struct {
	float ui_hdr_nits;
	float tm_hdr_saturation_scale;
	float screen_width;
	float screen_height;
} StretchPic_UBO_t;

/**
*	The following struct is a bit of an odd one, however, it will hold state of each
*	time clipRect has changed. Storing the num_stretch_pic offset and num_stretch_pic_count
*	in order for each subsequent draw call to apply their own scissor rectangle.
* 
*	The num_scissor_groups is rebuild each frame, and reset at the end of that frame.
**/
typedef struct {
	clipRect_t clip_rect;

	//! Offset into the SBO pic buffer.
	uint32_t num_stretch_pic_offset;
	//! The count of stretch pics that were added during this scissor's clip rect.
	uint32_t num_stretch_pic_count;
} StretchPic_Scissor_Group;
//! Actual stretch pic scissor groups.
static StretchPic_Scissor_Group stretch_pic_scissor_groups[ MAX_STRETCH_PICS ]; // WID: TODO: Find a sane number instead to not waste ram? Or just a dynamic buffer/queue of sorts.
//! Number of scissor groups active in the current frame.
static uint32_t num_stretch_pic_scissor_groups;

//! This stores the actual clip_rect set by R_ client calls.
static clipRect_t clip_rect;
//! Each individual time that clip_enable is enabled, a new stretchpic scissor group is added
//! for the current frame. 
static bool clip_enable = false;
//! Stretch pic memory queue.
static StretchPic_t stretch_pic_queue[MAX_STRETCH_PICS];

static VkPipelineLayout        pipeline_layout_stretch_pic;
static VkPipelineLayout        pipeline_layout_final_blit;
static VkRenderPass            render_pass_stretch_pic;
static VkPipeline              pipeline_stretch_pic[STRETCH_PIC_NUM_PIPELINES];
static VkPipeline              pipeline_final_blit;
static VkFramebuffer*          framebuffer_stretch_pic = NULL;
static BufferResource_t        buf_stretch_pic_queue[MAX_FRAMES_IN_FLIGHT];
static BufferResource_t        buf_ubo[MAX_FRAMES_IN_FLIGHT];
static VkDescriptorSetLayout   desc_set_layout_sbo;
static VkDescriptorSetLayout   desc_set_layout_ubo;
static VkDescriptorPool        desc_pool_sbo;
static VkDescriptorPool        desc_pool_ubo;
static VkDescriptorSet         desc_set_sbo[MAX_FRAMES_IN_FLIGHT];
static VkDescriptorSet         desc_set_ubo[MAX_FRAMES_IN_FLIGHT];

extern cvar_t* cvar_ui_hdr_nits;
extern cvar_t* cvar_tm_hdr_saturation_scale;



/**
*
*
*	Stretch Pic Examples:
* 
* 
* 
**/
/*
// 1. Draw a card panel with 12px rounded corners, 2px white stroke, and selective cyan outer glow
R_SetCornerRadius( 12.0f );
R_SetStroke( MakeColor( 255, 255, 255, 255 ), 2.0f );
R_SetOuterGlowRadius4( 0.0f, 16.0f, 16.0f, 16.0f ); // Omit top glow
R_SetOuterGlow( MakeColor( 0, 200, 255, 180 ), 16.0f );
R_DrawPic( 100, 100, card_image_handle );
R_ClearStyle(); // Reset styling back to default zero

// 2. Load TrueType font at 32px raster height and render 3D occluded text in world space
qhandle_t ttf_font = R_RegisterFontTTF( "fonts/inter.ttf", 32.0f );
vec3_t text_origin = { 512.0f, -256.0f, 72.0f };

// Add a 1.5px black outline around the font glyphs
R_SetStroke( MakeColor( 0, 0, 0, 255 ), 1.5f );

// Occluded behind world walls/geometry:
R_DrawString3DOccluded( text_origin, nullptr /* billboard * /, 16.0f /* height in units * /,
"Protected Area", ttf_font, MakeColor( 255, 220, 0, 255 ) );

// Always-on-top overhead tag:
R_DrawString3DNonOccluded( text_origin, nullptr, 12.0f,
						   "^2Ally [100 HP]", ttf_font, MakeColor( 255, 255, 255, 255 ) );
R_ClearStyle(); 
*/


/**
*
* 
*	Stretch Pic Enqueue-ing:
* 
* 
**/
/**
*	@brief
**/
VkExtent2D vkpt_draw_get_extent(void) {
	return qvk.extent_unscaled;
}

/**
*	@brief	
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
*	@brief	Enqueue a 'stretch pic' draw command.
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

	sp->view_depth = 0.0f;
	sp->pad02 = 0.0f;
	sp->pad03 = 0.0f;

	// Populate extended style payload
	sp->style_flags = draw.style_flags;
	if ( draw.style_flags != 0 ) {
		sp->stroke_colors[ 0 ] = draw.stroke_colors[ 0 ];
		sp->stroke_colors[ 1 ] = draw.stroke_colors[ 1 ];
		sp->stroke_colors[ 2 ] = draw.stroke_colors[ 2 ];
		sp->stroke_colors[ 3 ] = draw.stroke_colors[ 3 ];
		sp->stroke_thickness[ 0 ] = draw.stroke_thickness[ 0 ];
		sp->stroke_thickness[ 1 ] = draw.stroke_thickness[ 1 ];
		sp->stroke_thickness[ 2 ] = draw.stroke_thickness[ 2 ];
		sp->stroke_thickness[ 3 ] = draw.stroke_thickness[ 3 ];
		sp->outer_glow_colors[ 0 ] = draw.outer_glow_colors[ 0 ];
		sp->outer_glow_colors[ 1 ] = draw.outer_glow_colors[ 1 ];
		sp->outer_glow_colors[ 2 ] = draw.outer_glow_colors[ 2 ];
		sp->outer_glow_colors[ 3 ] = draw.outer_glow_colors[ 3 ];
		sp->outer_glow_radius[ 0 ] = draw.outer_glow_radius[ 0 ];
		sp->outer_glow_radius[ 1 ] = draw.outer_glow_radius[ 1 ];
		sp->outer_glow_radius[ 2 ] = draw.outer_glow_radius[ 2 ];
		sp->outer_glow_radius[ 3 ] = draw.outer_glow_radius[ 3 ];
		sp->inner_glow_colors[ 0 ] = draw.inner_glow_colors[ 0 ];
		sp->inner_glow_colors[ 1 ] = draw.inner_glow_colors[ 1 ];
		sp->inner_glow_colors[ 2 ] = draw.inner_glow_colors[ 2 ];
		sp->inner_glow_colors[ 3 ] = draw.inner_glow_colors[ 3 ];
		sp->inner_glow_radius[ 0 ] = draw.inner_glow_radius[ 0 ];
		sp->inner_glow_radius[ 1 ] = draw.inner_glow_radius[ 1 ];
		sp->inner_glow_radius[ 2 ] = draw.inner_glow_radius[ 2 ];
		sp->inner_glow_radius[ 3 ] = draw.inner_glow_radius[ 3 ];
		sp->corner_radii[ 0 ] = draw.corner_radii[ 0 ];
		sp->corner_radii[ 1 ] = draw.corner_radii[ 1 ];
		sp->corner_radii[ 2 ] = draw.corner_radii[ 2 ];
		sp->corner_radii[ 3 ] = draw.corner_radii[ 3 ];
		sp->sdf_tex_handle = 0;
		sp->sdf_pixel_range = 0.0f;
		sp->pad_style[ 0 ] = 0.0f;

		// MTSDF Font Atlas rendering
		if ( ( draw.style_flags & STYLE_FLAG_SDF_MTSDF ) != 0 ) {
			sp->sdf_pixel_range = 8.0f;
			sp->sdf_tex_handle = (uint32_t)tex_handle;
		}

		// Check if source image has an associated silhouette SDF texture
		if ( ( draw.style_flags & ( STYLE_FLAG_OUTLINE | STYLE_FLAG_OUTER_GLOW | STYLE_FLAG_INNER_GLOW ) ) &&
		     tex_handle >= 0 && tex_handle < MAX_RIMAGES &&
		     r_images[ tex_handle ].sdf_image_handle > 0 ) {
			sp->style_flags |= STYLE_FLAG_SILHOUETTE_SDF;
			sp->sdf_tex_handle = r_images[ tex_handle ].sdf_image_handle;
			sp->sdf_pixel_range = r_images[ tex_handle ].sdf_pixel_range > 0.0f ? r_images[ tex_handle ].sdf_pixel_range : 8.0f;
		}
	} else {
		sp->sdf_tex_handle = 0;
		sp->sdf_pixel_range = 0.0f;
		sp->pad_style[ 0 ] = 0.0f;
	}
}

/**
*	@brief	WID: Supports rotating around a specified pivot point.
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
		color = ( color & 0xffffff ) | ( ( int )( alpha ) << 24 );
	}

	// Store in the color and texture handle. If not available in our current
	// image registration sequence, apply the 'White' texture instead.
	sp->color = color;
	sp->tex_handle = tex_handle;
	if ( tex_handle >= 0 && tex_handle < MAX_RIMAGES
		&& !r_images[ tex_handle ].registration_sequence ) {
		sp->tex_handle = TEXNUM_WHITE;
	}

	sp->view_depth = 0.0f;
	sp->pad02 = 0.0f;
	sp->pad03 = 0.0f;

	// Populate extended style payload
	sp->style_flags = draw.style_flags;
	if ( draw.style_flags != 0 ) {
		sp->stroke_colors[ 0 ] = draw.stroke_colors[ 0 ];
		sp->stroke_colors[ 1 ] = draw.stroke_colors[ 1 ];
		sp->stroke_colors[ 2 ] = draw.stroke_colors[ 2 ];
		sp->stroke_colors[ 3 ] = draw.stroke_colors[ 3 ];
		sp->stroke_thickness[ 0 ] = draw.stroke_thickness[ 0 ];
		sp->stroke_thickness[ 1 ] = draw.stroke_thickness[ 1 ];
		sp->stroke_thickness[ 2 ] = draw.stroke_thickness[ 2 ];
		sp->stroke_thickness[ 3 ] = draw.stroke_thickness[ 3 ];
		sp->outer_glow_colors[ 0 ] = draw.outer_glow_colors[ 0 ];
		sp->outer_glow_colors[ 1 ] = draw.outer_glow_colors[ 1 ];
		sp->outer_glow_colors[ 2 ] = draw.outer_glow_colors[ 2 ];
		sp->outer_glow_colors[ 3 ] = draw.outer_glow_colors[ 3 ];
		sp->outer_glow_radius[ 0 ] = draw.outer_glow_radius[ 0 ];
		sp->outer_glow_radius[ 1 ] = draw.outer_glow_radius[ 1 ];
		sp->outer_glow_radius[ 2 ] = draw.outer_glow_radius[ 2 ];
		sp->outer_glow_radius[ 3 ] = draw.outer_glow_radius[ 3 ];
		sp->inner_glow_colors[ 0 ] = draw.inner_glow_colors[ 0 ];
		sp->inner_glow_colors[ 1 ] = draw.inner_glow_colors[ 1 ];
		sp->inner_glow_colors[ 2 ] = draw.inner_glow_colors[ 2 ];
		sp->inner_glow_colors[ 3 ] = draw.inner_glow_colors[ 3 ];
		sp->inner_glow_radius[ 0 ] = draw.inner_glow_radius[ 0 ];
		sp->inner_glow_radius[ 1 ] = draw.inner_glow_radius[ 1 ];
		sp->inner_glow_radius[ 2 ] = draw.inner_glow_radius[ 2 ];
		sp->inner_glow_radius[ 3 ] = draw.inner_glow_radius[ 3 ];
		sp->corner_radii[ 0 ] = draw.corner_radii[ 0 ];
		sp->corner_radii[ 1 ] = draw.corner_radii[ 1 ];
		sp->corner_radii[ 2 ] = draw.corner_radii[ 2 ];
		sp->corner_radii[ 3 ] = draw.corner_radii[ 3 ];
		sp->sdf_tex_handle = 0;
		sp->sdf_pixel_range = 0.0f;
		sp->pad_style[ 0 ] = 0.0f;

		// MTSDF Font Atlas rendering
		if ( ( draw.style_flags & STYLE_FLAG_SDF_MTSDF ) != 0 ) {
			sp->sdf_pixel_range = 8.0f;
			sp->sdf_tex_handle = (uint32_t)tex_handle;
		}

		// Check if source image has an associated silhouette SDF texture
		if ( ( draw.style_flags & ( STYLE_FLAG_OUTLINE | STYLE_FLAG_OUTER_GLOW | STYLE_FLAG_INNER_GLOW ) ) &&
			 tex_handle >= 0 && tex_handle < MAX_RIMAGES &&
			 r_images[ tex_handle ].sdf_image_handle > 0 ) {
			sp->style_flags |= STYLE_FLAG_SILHOUETTE_SDF;
			sp->sdf_tex_handle = r_images[ tex_handle ].sdf_image_handle;
			sp->sdf_pixel_range = r_images[ tex_handle ].sdf_pixel_range > 0.0f ? r_images[ tex_handle ].sdf_pixel_range : 8.0f;
		}
	} else {
		sp->sdf_tex_handle = 0;
		sp->sdf_pixel_range = 0.0f;
		sp->pad_style[ 0 ] = 0.0f;
	}
}

/**
*
*
*	(RenderPass-) Initialize/Destroy:
*
*
**/
/**
*	@brief	
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
*	@brief
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
			.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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
*	@brief
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
*	@brief
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
*	@brief
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

	VkSpecializationInfo specInfo_SDR = {.mapEntryCount = 1, .pMapEntries = specEntries, .dataSize = sizeof(uint32_t), .pData = &spec_data[0]};
	VkSpecializationInfo specInfo_HDR = {.mapEntryCount = 1, .pMapEntries = specEntries, .dataSize = sizeof(uint32_t), .pData = &spec_data[1]};

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
		.minDepth = 0.0f,
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

	framebuffer_stretch_pic = malloc(qvk.num_swap_chain_images * sizeof(*framebuffer_stretch_pic));
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
*	 Stretch Pic Clearing/Drawing:
* 
* 
**/
/**
*	@brief
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
*	@brief
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
	VkExtent2D extent = vkpt_draw_get_extent();
	ubo->screen_width = (float)extent.width;
	ubo->screen_height = (float)extent.height;
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
			.minDepth = 0.0f,
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
*	@brief
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
*	@brief
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
*	'Draw' Refresh 'R_***' Function Pointer Implementations:
*
*
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

void
R_ClearColor_RTX(void)
{
	draw.colors[0].u32 = U32_WHITE;
	draw.colors[1].u32 = U32_ORANGE;
}

void
R_SetAlpha_RTX(float alpha)
{
    alpha = powf(fabsf(alpha), 0.4545f); // un-sRGB the alpha
	draw.colors[0].u8[3] = draw.colors[1].u8[3] = alpha * 255;
}

void
R_SetAlphaScale_RTX(float alpha)
{
	draw.alpha_scale = alpha;
}

void
R_SetColor_RTX(uint32_t color)
{
	draw.colors[0].u32   = color;
	draw.colors[1].u8[3] = draw.colors[0].u8[3];
}

void
R_LightPoint_RTX(const vec3_t origin, vec3_t light)
{
	VectorSet(light, 1, 1, 1);
}

void
R_SetScale_RTX(float scale)
{
	draw.scale = scale;
}

/**
*	@brief	Set stroke color and uniform thickness across all four edges.
*	@param	color		Packed RGBA border color.
*	@param	thickness	Stroke thickness in pixels.
**/
void R_SetStroke_RTX( const uint32_t color, const float thickness ) {
	if ( thickness <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_OUTLINE;
		draw.stroke_thickness[ 0 ] = draw.stroke_thickness[ 1 ] = draw.stroke_thickness[ 2 ] = draw.stroke_thickness[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= STYLE_FLAG_OUTLINE;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.stroke_colors[ i ] = color;
		draw.stroke_thickness[ i ] = thickness;
	}
}

/**
*	@brief	Set stroke thickness uniformly across all four edges.
*	@param	thickness	Stroke thickness in pixels.
**/
void R_SetStrokeThickness_RTX( const float thickness ) {
	if ( thickness <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_OUTLINE;
	} else {
		draw.style_flags |= STYLE_FLAG_OUTLINE;
	}
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.stroke_thickness[ i ] = max( 0.0f, thickness );
	}
}

/**
*	@brief	Set stroke thickness individually per edge (Top, Right, Bottom, Left).
*	@param	top		Top stroke thickness in pixels.
*	@param	right	Right stroke thickness in pixels.
*	@param	bottom	Bottom stroke thickness in pixels.
*	@param	left	Left stroke thickness in pixels.
**/
void R_SetStrokeThickness4_RTX( const float top, const float right, const float bottom, const float left ) {
	draw.stroke_thickness[ 0 ] = max( 0.0f, top );
	draw.stroke_thickness[ 1 ] = max( 0.0f, right );
	draw.stroke_thickness[ 2 ] = max( 0.0f, bottom );
	draw.stroke_thickness[ 3 ] = max( 0.0f, left );
	if ( top > 0.0f || right > 0.0f || bottom > 0.0f || left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_OUTLINE;
	} else {
		draw.style_flags &= ~STYLE_FLAG_OUTLINE;
	}
}

/**
*	@brief	Set stroke outline colors individually per edge (Top, Right, Bottom, Left).
*	@param	top		Top stroke color.
*	@param	right	Right stroke color.
*	@param	bottom	Bottom stroke color.
*	@param	left	Left stroke color.
**/
void R_SetStrokeColors4_RTX( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left ) {
	draw.stroke_colors[ 0 ] = top;
	draw.stroke_colors[ 1 ] = right;
	draw.stroke_colors[ 2 ] = bottom;
	draw.stroke_colors[ 3 ] = left;
}

/**
*	@brief	Set stroke outline with custom alignment and modifier flags.
*	@param	color		Stroke outline color.
*	@param	thickness	Stroke thickness in pixels.
*	@param	flags		Alignment bitflags (STYLE_FLAG_STROKE_ALIGN_*).
**/
void R_SetStrokeEx_RTX( const uint32_t color, const float thickness, const uint32_t flags ) {
	R_SetStroke_RTX( color, thickness );
	draw.style_flags = ( draw.style_flags & ~STYLE_FLAG_STROKE_ALIGN_MASK ) | ( flags & STYLE_FLAG_STROKE_ALIGN_MASK );
}

/**
*	@brief	Set stroke outline colors and thicknesses per edge with custom alignment flags.
*	@param	colors		Array of 4 colors [Top, Right, Bottom, Left].
*	@param	thickness	Array of 4 thicknesses [Top, Right, Bottom, Left].
*	@param	flags		Alignment bitflags (STYLE_FLAG_STROKE_ALIGN_*).
**/
void R_SetStroke4Ex_RTX( const uint32_t colors[ 4 ], const float thickness[ 4 ], const uint32_t flags ) {
	if ( colors != NULL && thickness != NULL ) {
		R_SetStrokeColors4_RTX( colors[ 0 ], colors[ 1 ], colors[ 2 ], colors[ 3 ] );
		R_SetStrokeThickness4_RTX( thickness[ 0 ], thickness[ 1 ], thickness[ 2 ], thickness[ 3 ] );
	}
	draw.style_flags = ( draw.style_flags & ~STYLE_FLAG_STROKE_ALIGN_MASK ) | ( flags & STYLE_FLAG_STROKE_ALIGN_MASK );
}

/**
*	@brief	Set outer glow color and uniform radius across all four edges.
*	@param	color	Outer glow color.
*	@param	radius	Outer glow radius in pixels.
**/
void R_SetOuterGlow_RTX( const uint32_t color, const float radius ) {
	if ( radius <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_OUTER_GLOW;
		draw.outer_glow_radius[ 0 ] = draw.outer_glow_radius[ 1 ] = draw.outer_glow_radius[ 2 ] = draw.outer_glow_radius[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= ( STYLE_FLAG_OUTER_GLOW | STYLE_FLAG_EDGE_ALL );
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.outer_glow_colors[ i ] = color;
		draw.outer_glow_radius[ i ] = radius;
	}
}

/**
*	@brief	Set outer glow radius individually per edge (Top, Right, Bottom, Left).
*	@param	top		Top outer glow radius in pixels.
*	@param	right	Right outer glow radius in pixels.
*	@param	bottom	Bottom outer glow radius in pixels.
*	@param	left	Left outer glow radius in pixels.
**/
void R_SetOuterGlowRadius4_RTX( const float top, const float right, const float bottom, const float left ) {
	draw.outer_glow_radius[ 0 ] = max( 0.0f, top );
	draw.outer_glow_radius[ 1 ] = max( 0.0f, right );
	draw.outer_glow_radius[ 2 ] = max( 0.0f, bottom );
	draw.outer_glow_radius[ 3 ] = max( 0.0f, left );
	if ( top > 0.0f || right > 0.0f || bottom > 0.0f || left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_OUTER_GLOW;
	} else {
		draw.style_flags &= ~STYLE_FLAG_OUTER_GLOW;
	}
}

/**
*	@brief	Set outer glow colors individually per edge (Top, Right, Bottom, Left).
*	@param	top		Top outer glow color.
*	@param	right	Right outer glow color.
*	@param	bottom	Bottom outer glow color.
*	@param	left	Left outer glow color.
**/
void R_SetOuterGlowColors4_RTX( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left ) {
	draw.outer_glow_colors[ 0 ] = top;
	draw.outer_glow_colors[ 1 ] = right;
	draw.outer_glow_colors[ 2 ] = bottom;
	draw.outer_glow_colors[ 3 ] = left;
}

/**
*	@brief	Set which edges emit outer glow.
*	@param	edge_mask	Bitmask of enabled edges (STYLE_FLAG_EDGE_TOP, etc.).
**/
void R_SetOuterGlowEdges_RTX( const uint32_t edge_mask ) {
	draw.style_flags = ( draw.style_flags & ~STYLE_FLAG_EDGE_ALL ) | ( edge_mask & STYLE_FLAG_EDGE_ALL );
}

/**
*	@brief	Set outer glow with custom falloff and blending flags.
*	@param	color	Outer glow color.
*	@param	radius	Outer glow radius in pixels.
*	@param	flags	Style modifier flags (STYLE_FLAG_GLOW_FALLOFF_*, STYLE_FLAG_GLOW_BLEND_*).
**/
void R_SetOuterGlowEx_RTX( const uint32_t color, const float radius, const uint32_t flags ) {
	R_SetOuterGlow_RTX( color, radius );
	const uint32_t glow_mask = ( STYLE_FLAG_GLOW_FALLOFF_EXP | STYLE_FLAG_GLOW_BLEND_ADDITIVE | STYLE_FLAG_EDGE_ALL );
	draw.style_flags = ( draw.style_flags & ~glow_mask ) | ( flags & glow_mask );
}

/**
*	@brief	Set outer glow colors and radii per edge with custom falloff/blend flags.
*	@param	colors	Array of 4 colors [Top, Right, Bottom, Left].
*	@param	radii	Array of 4 radii [Top, Right, Bottom, Left].
*	@param	flags	Style modifier flags.
**/
void R_SetOuterGlow4Ex_RTX( const uint32_t colors[ 4 ], const float radii[ 4 ], const uint32_t flags ) {
	if ( colors != NULL && radii != NULL ) {
		R_SetOuterGlowColors4_RTX( colors[ 0 ], colors[ 1 ], colors[ 2 ], colors[ 3 ] );
		R_SetOuterGlowRadius4_RTX( radii[ 0 ], radii[ 1 ], radii[ 2 ], radii[ 3 ] );
	}
	const uint32_t glow_mask = ( STYLE_FLAG_GLOW_FALLOFF_EXP | STYLE_FLAG_GLOW_BLEND_ADDITIVE | STYLE_FLAG_EDGE_ALL );
	draw.style_flags = ( draw.style_flags & ~glow_mask ) | ( flags & glow_mask );
}

/**
*	@brief	Set inner glow color and uniform radius across all four edges.
*	@param	color	Inner glow color.
*	@param	radius	Inner glow radius in pixels.
**/
void R_SetInnerGlow_RTX( const uint32_t color, const float radius ) {
	if ( radius <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_INNER_GLOW;
		draw.inner_glow_radius[ 0 ] = draw.inner_glow_radius[ 1 ] = draw.inner_glow_radius[ 2 ] = draw.inner_glow_radius[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= STYLE_FLAG_INNER_GLOW;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.inner_glow_colors[ i ] = color;
		draw.inner_glow_radius[ i ] = radius;
	}
}

/**
*	@brief	Set inner glow colors individually per edge (Top, Right, Bottom, Left).
*	@param	top		Top inner glow color.
*	@param	right	Right inner glow color.
*	@param	bottom	Bottom inner glow color.
*	@param	left	Left inner glow color.
**/
void R_SetInnerGlowColors4_RTX( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left ) {
	draw.inner_glow_colors[ 0 ] = top;
	draw.inner_glow_colors[ 1 ] = right;
	draw.inner_glow_colors[ 2 ] = bottom;
	draw.inner_glow_colors[ 3 ] = left;
}

/**
*	@brief	Set inner glow radius individually per edge (Top, Right, Bottom, Left).
*	@param	top		Top inner glow radius in pixels.
*	@param	right	Right inner glow radius in pixels.
*	@param	bottom	Bottom inner glow radius in pixels.
*	@param	left	Left inner glow radius in pixels.
**/
void R_SetInnerGlowRadius4_RTX( const float top, const float right, const float bottom, const float left ) {
	draw.inner_glow_radius[ 0 ] = max( 0.0f, top );
	draw.inner_glow_radius[ 1 ] = max( 0.0f, right );
	draw.inner_glow_radius[ 2 ] = max( 0.0f, bottom );
	draw.inner_glow_radius[ 3 ] = max( 0.0f, left );
	if ( top > 0.0f || right > 0.0f || bottom > 0.0f || left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_INNER_GLOW;
	} else {
		draw.style_flags &= ~STYLE_FLAG_INNER_GLOW;
	}
}

/**
*	@brief	Set inner glow with custom falloff and blending flags.
*	@param	color	Inner glow color.
*	@param	radius	Inner glow radius in pixels.
*	@param	flags	Style modifier flags.
**/
void R_SetInnerGlowEx_RTX( const uint32_t color, const float radius, const uint32_t flags ) {
	R_SetInnerGlow_RTX( color, radius );
	const uint32_t glow_mask = ( STYLE_FLAG_GLOW_FALLOFF_EXP | STYLE_FLAG_GLOW_BLEND_ADDITIVE );
	draw.style_flags = ( draw.style_flags & ~glow_mask ) | ( flags & glow_mask );
}

/**
*	@brief	Set uniform corner radius across all four corners.
*	@param	radius	Corner radius in pixels.
**/
void R_SetCornerRadius_RTX( const float radius ) {
	if ( radius <= 0.0f ) {
		draw.style_flags &= ~STYLE_FLAG_CORNER_RADIUS;
		draw.corner_radii[ 0 ] = draw.corner_radii[ 1 ] = draw.corner_radii[ 2 ] = draw.corner_radii[ 3 ] = 0.0f;
		return;
	}
	draw.style_flags |= STYLE_FLAG_CORNER_RADIUS;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.corner_radii[ i ] = radius;
	}
}

/**
*	@brief	Set corner radii individually (Top-Left, Top-Right, Bottom-Right, Bottom-Left).
*	@param	top_left		Top-left corner radius in pixels.
*	@param	top_right		Top-right corner radius in pixels.
*	@param	bottom_right	Bottom-right corner radius in pixels.
*	@param	bottom_left		Bottom-left corner radius in pixels.
**/
void R_SetCornerRadius4_RTX( const float top_left, const float top_right, const float bottom_right, const float bottom_left ) {
	draw.corner_radii[ 0 ] = max( 0.0f, top_left );
	draw.corner_radii[ 1 ] = max( 0.0f, top_right );
	draw.corner_radii[ 2 ] = max( 0.0f, bottom_right );
	draw.corner_radii[ 3 ] = max( 0.0f, bottom_left );
	if ( top_left > 0.0f || top_right > 0.0f || bottom_right > 0.0f || bottom_left > 0.0f ) {
		draw.style_flags |= STYLE_FLAG_CORNER_RADIUS;
	} else {
		draw.style_flags &= ~STYLE_FLAG_CORNER_RADIUS;
	}
}

/**
*	@brief	Reset all 2D styling to default (no stroke, no glow, no corner radius).
**/
void R_ClearStyle_RTX( void ) {
	draw.style_flags = STYLE_FLAG_NONE;
	for ( int32_t i = 0; i < 4; i++ ) {
		draw.stroke_colors[ i ] = 0;
		draw.stroke_thickness[ i ] = 0.0f;
		draw.outer_glow_colors[ i ] = 0;
		draw.outer_glow_radius[ i ] = 0.0f;
		draw.inner_glow_colors[ i ] = 0;
		draw.inner_glow_radius[ i ] = 0.0f;
		draw.corner_radii[ i ] = 0.0f;
	}
}

/**
*	@brief	Draw a 2D line segment with specified thickness and color.
*	@param	x1			Start X coordinate.
*	@param	y1			Start Y coordinate.
*	@param	x2			End X coordinate.
*	@param	y2			End Y coordinate.
*	@param	thickness	Line thickness in pixels.
*	@param	color		Packed RGBA color.
**/
void R_DrawLine2D_RTX( const float x1, const float y1, const float x2, const float y2, const float thickness, const uint32_t color ) {
	const float dx = x2 - x1;
	const float dy = y2 - y1;
	const float len = sqrt( ( dx * dx ) + ( dy * dy ) );
	if ( len < 0.001f ) {
		R_DrawStretchPic_RTX( (int)( x1 - thickness * 0.5f ), (int)( y1 - thickness * 0.5f ), (int)thickness, (int)thickness, TEXNUM_WHITE );
		return;
	}
	const float angle = atan2( dy, dx ) * ( 180.0f / (float)M_PI );
	const float cx = ( x1 + x2 ) * 0.5f;
	const float cy = ( y1 + y2 ) * 0.5f;
	const float line_w = len;
	const float line_h = max( 1.0f, thickness );

	enqueue_stretch_rotate_pic(
		cx - line_w * 0.5f, cy - line_h * 0.5f,
		line_w, line_h,
		0.0f, 0.0f, 1.0f, 1.0f,
		angle, line_w * 0.5f, line_h * 0.5f,
		color, TEXNUM_WHITE, 0 );
}

void
R_DrawStretchPic_RTX(int x, int y, int w, int h, qhandle_t pic ) {
	float eps = +1e-5f; /* fixes some ugly artifacts */
	enqueue_stretch_pic(
		x,    y,    w,    h,
		0.0f + eps, 0.0f + eps, 1.0f - eps, 1.0f - eps,
		draw.colors[0].u32, pic);
}
void
R_DrawRotateStretchPic_RTX( int x, int y, int w, int h, float angle, int pivot_x, int pivot_y, qhandle_t pic ) {
	float eps = +1e-5f; /* fixes some ugly artifacts */
	enqueue_stretch_rotate_pic(
		x, y, w, h,
		0.0f + eps, 0.0f + eps, 1.0f - eps, 1.0f - eps,
		angle, pivot_x, pivot_y,
		draw.colors[ 0 ].u32, pic, 0 );
}

void
R_DrawPic_RTX(int x, int y, qhandle_t pic)
{
	image_t *image = IMG_ForHandle(pic);
	R_DrawStretchPic(x, y, image->width, image->height, pic);
}
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

void
R_DrawStretchRaw_RTX(int x, int y, int w, int h)
{
	if(!qvk.raw_image)
		return;
	R_DrawStretchPic(x, y, w, h, qvk.raw_image - r_images);
}

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

void
R_DiscardRawPic_RTX(void)
{
	if(qvk.raw_image) {
		R_UnregisterImage(qvk.raw_image - r_images);
		qvk.raw_image = NULL;
	}
}

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

#define DIV64 (1.0f / 64.0f)

void
R_TileClear_RTX(int x, int y, int w, int h, qhandle_t pic)
{
	enqueue_stretch_pic(x, y, w, h,
		x * DIV64, y * DIV64, (x + w) * DIV64, (y + h) * DIV64,
		U32_WHITE, pic);
}

void
R_DrawFill8_RTX(int x, int y, int w, int h, int c)
{
	if(!w || !h)
		return;
	enqueue_stretch_pic(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		d_8to24table[c & 0xff], TEXNUM_WHITE);
}

void
R_DrawFill32_RTX(int x, int y, int w, int h, uint32_t color)
{
	if(!w || !h)
		return;
	enqueue_stretch_pic(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		color, TEXNUM_WHITE);
}

void
R_DrawFill8f_RTX( float x, float y, float w, float h, int32_t c ) {
	if ( !w || !h )
		return;
	enqueue_stretch_pic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		d_8to24table[ c & 0xff ], TEXNUM_WHITE );
}

void
R_DrawFill32f_RTX( float x, float y, float w, float h, uint32_t color ) {
	if ( !w || !h )
		return;
	enqueue_stretch_pic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
		color, TEXNUM_WHITE );
}

static void vkpt_debug_make_style_from_packed( uint32_t color, float thickness, float outline_thickness, uint16_t style_flags, vkpt_debug_draw_style_t *style ) {
	vkpt_debug_draw_make_style( style,
		( ( color >> 0 ) & 0xFF ) / 255.0f,
		( ( color >> 8 ) & 0xFF ) / 255.0f,
		( ( color >> 16 ) & 0xFF ) / 255.0f,
		( ( color >> 24 ) & 0xFF ) / 255.0f,
		thickness, outline_thickness, style_flags );
}

/**
*	@brief	Queue a world-space debug box for vkpt when debug geometry rendering is enabled.
*	@param	mins	World-space AABB minimum corner.
*	@param	maxs	World-space AABB maximum corner.
*	@param	color	Packed RGBA color used by the debug primitive.
*	@note	This remains isolated in the vkpt debug module and no-ops when the debug cvar gate is disabled.
**/
void R_DrawDebugBox_RTX( const vec3_t mins, const vec3_t maxs, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	/**
	*	Early out when the global vkpt debug draw cvar gate is disabled.
	**/
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}

	vkpt_debug_draw_style_t style;
	vkpt_debug_make_style_from_packed( color, thickness, outline_thickness, style_flags, &style );

	/**
	*	Queue the world-space AABB primitive in the isolated debug draw module.
	**/
	vkpt_debug_draw_add_box( mins, maxs, &style );
}

/**
*	@brief	Queue a world-space debug line segment.
*	@param	start	World-space start point.
*	@param	end		World-space end point.
*	@param	color	Packed RGBA color.
**/
void R_DrawDebugLine_RTX( const vec3_t start, const vec3_t end, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}
	vkpt_debug_draw_style_t style;
	vkpt_debug_make_style_from_packed( color, thickness, outline_thickness, style_flags, &style );
	vkpt_debug_draw_add_line( start, end, &style );
}

/**
*	@brief	Queue a world-space debug arrow (shaft + head edges).
*	@param	start		World-space tail point.
*	@param	end			World-space tip point.
*	@param	head_length	Length of the arrow head in world units.
*	@param	color		Packed RGBA color.
**/
void R_DrawDebugArrow_RTX( const vec3_t start, const vec3_t end, float head_length, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}
	vkpt_debug_draw_style_t style;
	vkpt_debug_make_style_from_packed( color, thickness, outline_thickness, style_flags, &style );
	vkpt_debug_draw_add_arrow( start, end, head_length, &style );
}

/**
*	@brief	Queue a world-space debug sphere billboard.
*	@param	center	World-space center.
*	@param	radius	Sphere radius in world units.
*	@param	color	Packed RGBA color.
**/
void R_DrawDebugSphere_RTX( const vec3_t center, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}
	vkpt_debug_draw_style_t style;
	vkpt_debug_make_style_from_packed( color, thickness, outline_thickness, style_flags, &style );
	vkpt_debug_draw_add_sphere( center, radius, &style );
}

/**
*	@brief	Queue a world-space debug capsule (thick segment with rounded ends).
*	@param	start	World-space start point (bottom hemisphere center).
*	@param	end		World-space end point (top hemisphere center).
*	@param	radius	Capsule radius in world units.
*	@param	color	Packed RGBA color.
**/
void R_DrawDebugCapsule_RTX( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}
	vkpt_debug_draw_style_t style;
	vkpt_debug_make_style_from_packed( color, thickness, outline_thickness, style_flags, &style );
	vkpt_debug_draw_add_capsule( start, end, radius, &style );
}

/**
*	@brief	Queue a world-space debug cylinder wireframe.
*	@param	start	World-space bottom center.
*	@param	end		World-space top center.
*	@param	radius	Cylinder radius in world units.
*	@param	color	Packed RGBA color.
**/
void R_DrawDebugCylinder_RTX( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}
	vkpt_debug_draw_style_t style;
	vkpt_debug_make_style_from_packed( color, thickness, outline_thickness, style_flags, &style );
	vkpt_debug_draw_add_cylinder( start, end, radius, &style );
}

/**
*	@brief	Draw a single character from either a TrueType MTSDF font or a legacy bitmap font.
*	@param	x		Top-left X coordinate in virtual screen pixels.
*	@param	y		Top-left Y coordinate in virtual screen pixels.
*	@param	flags	UI color modifier flags (UI_ALTCOLOR, UI_XORCOLOR).
*	@param	c		Character code.
*	@param	font	Font handle (legacy font or MTSDF font).
**/
static inline void draw_char( int x, int y, int flags, int c, qhandle_t font ) {
	const font_mtsdf_t *desc = Font_GetDescriptorTTF( font );
	if ( desc != NULL ) {
		uint8_t ch = (uint8_t)( c & 255 );
		uint32_t char_color = ( ( flags & UI_ALTCOLOR ) != 0 ) ? draw.colors[ 1 ].u32 : draw.colors[ 0 ].u32;
		if ( ch >= 128 && !desc->glyph_valid[ ch ] ) {
			ch &= 0x7F;
			char_color = draw.colors[ 1 ].u32;
		}
		if ( ch == 17 ) {
			ch = ']';
		} else if ( ch == 16 ) {
			ch = '[';
		} else if ( ch == 11 ) {
			ch = '_';
		}
		if ( ch == 32 || !desc->glyph_valid[ ch ] ) {
			return;
		}
		const font_glyph_mtsdf_t *g = &desc->glyphs[ ch ];
		const float gx = (float)Q_rint( (float)x + g->bearing_x );
		const float gy = (float)Q_rint( (float)y + desc->ascent - g->bearing_y );

		// Save current style flags and inject MTSDF flag
		const uint32_t saved_flags = draw.style_flags;
		draw.style_flags |= STYLE_FLAG_SDF_MTSDF;

		enqueue_stretch_pic( gx, gy, g->width, g->height,
			g->s0, g->t0, g->s1, g->t1,
			char_color, desc->atlas_image );

		draw.style_flags = saved_flags;
		return;
	}

	// Legacy bitmap font path
	if ( ( c & 127 ) == 32 ) {
		return;
	}

	if ( flags & UI_ALTCOLOR ) {
		c |= 0x80;
	}
	if ( flags & UI_XORCOLOR ) {
		c ^= 0x80;
	}

	const float s = ( c & 15 ) * 0.0625f;
	const float t = ( c >> 4 ) * 0.0625f;
	const float eps = 1e-5f;

	enqueue_stretch_pic( (float)x, (float)y, CHAR_WIDTH, CHAR_HEIGHT,
		s + eps, t + eps, s + 0.0625f - eps, t + 0.0625f - eps,
		draw.colors[ c >> 7 ].u32, font );
}

/**
*	@brief	Draw a single character glyph at the specified 2D screen location.
**/
void R_DrawChar_RTX( int x, int y, int flags, int c, qhandle_t font ) {
	draw_char( x, y, flags, c & 255, font );
}

/**
*	@brief	Draw a 2D text string supporting proportional TrueType MTSDF fonts or fixed-width bitmap fonts.
*	@param	x		X start coordinate in virtual screen pixels.
*	@param	y		Y start coordinate in virtual screen pixels.
*	@param	flags	UI color modifier flags.
*	@param	maxlen	Maximum characters to render.
*	@param	s		Null-terminated string.
*	@param	font	Font handle.
*	@return	Ending X pixel coordinate after rendering the string.
**/
int R_DrawString_RTX( int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font ) {
	const font_mtsdf_t *desc = Font_GetDescriptorTTF( font );
	if ( desc != NULL ) {
		const uint32_t color_u32 = ( ( flags & UI_ALTCOLOR ) != 0 ) ? draw.colors[ 1 ].u32 : draw.colors[ 0 ].u32;
		float cur_x = (float)x;
		while ( maxlen-- && *s ) {
			uint8_t c = (uint8_t)( *s++ );
			uint32_t char_color = color_u32;
			if ( c >= 128 && !desc->glyph_valid[ c ] ) {
				c &= 0x7F;
				char_color = draw.colors[ 1 ].u32;
			}
			if ( c == 17 ) {
				c = ']';
			} else if ( c == 16 ) {
				c = '[';
			} else if ( c == 11 ) {
				c = '_';
			}
			if ( desc->glyph_valid[ c ] ) {
				const font_glyph_mtsdf_t *g = &desc->glyphs[ c ];
				if ( c != 32 && g->width > 0.0f && g->height > 0.0f ) {
					const float gx = (float)Q_rint( cur_x + g->bearing_x );
					const float gy = (float)Q_rint( (float)y + desc->ascent - g->bearing_y );

					const uint32_t saved_flags = draw.style_flags;
					draw.style_flags |= STYLE_FLAG_SDF_MTSDF;

					enqueue_stretch_pic( gx, gy, g->width, g->height,
						g->s0, g->t0, g->s1, g->t1,
						char_color, desc->atlas_image );

					draw.style_flags = saved_flags;
				}
				cur_x += g->advance;
			} else {
				cur_x += desc->glyphs[ ' ' ].advance;
			}
		}
		return Q_rint( cur_x );
	}

	// Legacy bitmap font path
	while ( maxlen-- && *s ) {
		const byte c = *s++;
		draw_char( x, y, flags, c, font );
		x += CHAR_WIDTH;
	}
	return x;
}

/**
*	@brief	Register a TrueType / OpenType font and generate its runtime MTSDF atlas.
*	@param	path			Virtual file path to the .ttf or .otf file.
*	@param	pixel_height	Reference pixel height for rasterization.
*	@return	Image handle to the registered MTSDF font atlas, or 0 on failure.
**/
qhandle_t R_RegisterFontTTF_RTX( const char *path, const float pixel_height ) {
	return R_RegisterFontTTF_Impl( path, pixel_height );
}

/**
*	@brief	Internal helper to draw 3D world-space text with optional camera occlusion.
*	@param	origin		World-space 3D origin (X, Y, Z).
*	@param	angles		Pitch, Yaw, Roll orientation in degrees, or NULL for camera-facing billboard.
*	@param	scale		Character height in Quake world units.
*	@param	text		String to render.
*	@param	font		Font handle (TrueType MTSDF font or legacy bitmap font).
*	@param	color		Packed RGBA color tint.
*	@param	occluded	If true, tests against scene depth (TEX_PT_VIEW_DEPTH_A); if false, renders always on top.
**/
static void DrawString3D_Internal( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color, const bool occluded ) {
	/**
	*	Sanity checks and early returns: validate inputs and camera view definition.
	**/
	if ( text == NULL || text[ 0 ] == '\0' || scale <= 0.0f ) {
		return;
	}
	if ( vkpt_refdef.fd == NULL ) {
		return;
	}

	/**
	*	Calculate distance from camera eye to text origin for depth testing and culling.
	**/
	vec3_t to_origin;
	VectorSubtract( origin, vkpt_refdef.fd->vieworg, to_origin );
	const float view_dist = VectorLength( to_origin );
	if ( view_dist < 0.1f ) {
		return;
	}

	/**
	*	Determine orientation basis vectors (forward, right, up).
	**/
	vec3_t fwd, right, up;
	if ( angles != NULL ) {
		AngleVectors( angles, fwd, right, up );
	} else {
		// Construct camera-facing billboard vectors
		VectorNormalize2( to_origin, fwd );
		const vec3_t world_up = { 0.0f, 0.0f, 1.0f };
		CrossProduct( world_up, fwd, right );
		if ( VectorLength( right ) < 0.001f ) {
			const vec3_t alt_up = { 0.0f, 1.0f, 0.0f };
			CrossProduct( alt_up, fwd, right );
		}
		VectorNormalize( right );
		CrossProduct( fwd, right, up );
		VectorNormalize( up );
	}

	/**
	*	Compute camera View-Projection matrix.
	**/
	float proj[ 16 ];
	create_projection_matrix( proj,
		vkpt_refdef.z_near > 0.0f ? vkpt_refdef.z_near : 4.0f,
		vkpt_refdef.z_far > 0.0f ? vkpt_refdef.z_far : 4096.0f,
		vkpt_refdef.fd->fov_x,
		vkpt_refdef.fd->fov_y );

	float vp[ 16 ];
	mult_matrix_matrix( vp, proj, vkpt_refdef.view_matrix );

	const font_mtsdf_t *desc = Font_GetDescriptorTTF( font );
	const float font_ref_h = ( desc != NULL && desc->pixel_height > 0.0f ) ? desc->pixel_height : 8.0f;
	const float unit_scale = scale / font_ref_h;

	float cursor_x = 0.0f;
	const char *s = text;

	/**
	*	Tessellate string into 3D character quads.
	**/
	while ( *s != '\0' ) {
		if ( num_stretch_pics == MAX_STRETCH_PICS ) {
			Com_EPrintf( "%s: Stretch pic queue full in 3D text!\n", __func__ );
			break;
		}

		const uint8_t ch = (uint8_t)( *s++ );
		float glyph_w = 0.0f;
		float glyph_h = 0.0f;
		float advance = 0.0f;
		float bx = 0.0f;
		float by = 0.0f;
		float s0 = 0.0f, t0 = 0.0f, s1 = 1.0f, t1 = 1.0f;
		qhandle_t glyph_tex = font;
		bool is_mtsdf = false;

		if ( desc != NULL ) {
			if ( !desc->glyph_valid[ ch ] ) {
				cursor_x += desc->glyphs[ ' ' ].advance * unit_scale;
				continue;
			}
			const font_glyph_mtsdf_t *g = &desc->glyphs[ ch ];
			if ( ch == 32 || g->width <= 0.0f || g->height <= 0.0f ) {
				cursor_x += g->advance * unit_scale;
				continue;
			}
			glyph_w = g->width * unit_scale;
			glyph_h = g->height * unit_scale;
			advance = g->advance * unit_scale;
			bx = g->bearing_x * unit_scale;
			by = ( desc->ascent - g->bearing_y ) * unit_scale;
			s0 = g->s0; t0 = g->t0; s1 = g->s1; t1 = g->t1;
			glyph_tex = desc->atlas_image;
			is_mtsdf = true;
		} else {
			if ( ( ch & 127 ) == 32 ) {
				cursor_x += scale;
				continue;
			}
			glyph_w = scale;
			glyph_h = scale;
			advance = scale;
			bx = 0.0f;
			by = 0.0f;
			const float cs = ( ch & 15 ) * 0.0625f;
			const float ct = ( ch >> 4 ) * 0.0625f;
			const float eps = 1e-5f;
			s0 = cs + eps; t0 = ct + eps;
			s1 = cs + 0.0625f - eps; t1 = ct + 0.0625f - eps;
			glyph_tex = font;
			is_mtsdf = false;
		}

		/**
		*	Construct 4x4 character transform matrix in world space.
		**/
		vec3_t char_top_left;
		char_top_left[ 0 ] = origin[ 0 ] + ( cursor_x + bx ) * right[ 0 ] - by * up[ 0 ];
		char_top_left[ 1 ] = origin[ 1 ] + ( cursor_x + bx ) * right[ 1 ] - by * up[ 1 ];
		char_top_left[ 2 ] = origin[ 2 ] + ( cursor_x + bx ) * right[ 2 ] - by * up[ 2 ];

		float char_mat[ 16 ];
		char_mat[ 0 ] = glyph_w * right[ 0 ];
		char_mat[ 1 ] = glyph_w * right[ 1 ];
		char_mat[ 2 ] = glyph_w * right[ 2 ];
		char_mat[ 3 ] = 0.0f;

		char_mat[ 4 ] = -glyph_h * up[ 0 ];
		char_mat[ 5 ] = -glyph_h * up[ 1 ];
		char_mat[ 6 ] = -glyph_h * up[ 2 ];
		char_mat[ 7 ] = 0.0f;

		char_mat[ 8 ] = fwd[ 0 ];
		char_mat[ 9 ] = fwd[ 1 ];
		char_mat[ 10 ] = fwd[ 2 ];
		char_mat[ 11 ] = 0.0f;

		char_mat[ 12 ] = char_top_left[ 0 ];
		char_mat[ 13 ] = char_top_left[ 1 ];
		char_mat[ 14 ] = char_top_left[ 2 ];
		char_mat[ 15 ] = 1.0f;

		// Fetch stretch pic queue entry
		StretchPic_t *sp = stretch_pic_queue + num_stretch_pics++;
		StretchPic_Scissor_Group *scissor_group = &stretch_pic_scissor_groups[ num_stretch_pic_scissor_groups - 1 ];
		scissor_group->num_stretch_pic_count = num_stretch_pics - scissor_group->num_stretch_pic_offset;

		// Concatenate MVP matrix: sp->matTransform = VP * char_mat
		mult_matrix_matrix( sp->matTransform, vp, char_mat );

		// Set unit quad parameters for 3D world projection
		sp->x = 0.0f;
		sp->y = 0.0f;
		sp->w = 1.0f;
		sp->h = 1.0f;
		sp->pivot_x = 0.0f;
		sp->pivot_y = 0.0f;
		sp->angle = 0.0f;
		sp->view_depth = view_dist;
		sp->pad02 = 0.0f;
		sp->pad03 = 0.0f;

		sp->s = s0;
		sp->t = t0;
		sp->w_s = s1 - s0;
		sp->h_t = t1 - t0;
		sp->color = color;
		sp->tex_handle = glyph_tex;

		// Set up 3D style and depth test flags
		sp->style_flags = draw.style_flags;
		if ( occluded ) {
			sp->style_flags |= STYLE_FLAG_DEPTH_TEST;
		} else {
			sp->style_flags &= ~STYLE_FLAG_DEPTH_TEST;
		}
		if ( is_mtsdf ) {
			sp->style_flags |= STYLE_FLAG_SDF_MTSDF;
			sp->sdf_tex_handle = glyph_tex;
			sp->sdf_pixel_range = desc->sdf_pixel_range;
		} else {
			sp->sdf_tex_handle = 0;
			sp->sdf_pixel_range = 0.0f;
		}

		sp->stroke_colors[ 0 ] = draw.stroke_colors[ 0 ];
		sp->stroke_colors[ 1 ] = draw.stroke_colors[ 1 ];
		sp->stroke_colors[ 2 ] = draw.stroke_colors[ 2 ];
		sp->stroke_colors[ 3 ] = draw.stroke_colors[ 3 ];
		sp->stroke_thickness[ 0 ] = draw.stroke_thickness[ 0 ];
		sp->stroke_thickness[ 1 ] = draw.stroke_thickness[ 1 ];
		sp->stroke_thickness[ 2 ] = draw.stroke_thickness[ 2 ];
		sp->stroke_thickness[ 3 ] = draw.stroke_thickness[ 3 ];
		sp->outer_glow_colors[ 0 ] = draw.outer_glow_colors[ 0 ];
		sp->outer_glow_colors[ 1 ] = draw.outer_glow_colors[ 1 ];
		sp->outer_glow_colors[ 2 ] = draw.outer_glow_colors[ 2 ];
		sp->outer_glow_colors[ 3 ] = draw.outer_glow_colors[ 3 ];
		sp->outer_glow_radius[ 0 ] = draw.outer_glow_radius[ 0 ];
		sp->outer_glow_radius[ 1 ] = draw.outer_glow_radius[ 1 ];
		sp->outer_glow_radius[ 2 ] = draw.outer_glow_radius[ 2 ];
		sp->outer_glow_radius[ 3 ] = draw.outer_glow_radius[ 3 ];
		sp->inner_glow_colors[ 0 ] = draw.inner_glow_colors[ 0 ];
		sp->inner_glow_colors[ 1 ] = draw.inner_glow_colors[ 1 ];
		sp->inner_glow_colors[ 2 ] = draw.inner_glow_colors[ 2 ];
		sp->inner_glow_colors[ 3 ] = draw.inner_glow_colors[ 3 ];
		sp->inner_glow_radius[ 0 ] = draw.inner_glow_radius[ 0 ];
		sp->inner_glow_radius[ 1 ] = draw.inner_glow_radius[ 1 ];
		sp->inner_glow_radius[ 2 ] = draw.inner_glow_radius[ 2 ];
		sp->inner_glow_radius[ 3 ] = draw.inner_glow_radius[ 3 ];
		sp->corner_radii[ 0 ] = draw.corner_radii[ 0 ];
		sp->corner_radii[ 1 ] = draw.corner_radii[ 1 ];
		sp->corner_radii[ 2 ] = draw.corner_radii[ 2 ];
		sp->corner_radii[ 3 ] = draw.corner_radii[ 3 ];
		sp->pad_style[ 0 ] = 0.0f;

		cursor_x += advance;
	}
}

/**
*	@brief	Draw 3D world-space text occluded by world geometry (walls, pillars, entities).
*	@param	origin	World-space 3D origin (X, Y, Z).
*	@param	angles	Pitch, Yaw, Roll orientation in degrees, or NULL for camera-facing billboard.
*	@param	scale	Character height in Quake world units.
*	@param	text	String to render.
*	@param	font	Font handle (TrueType MTSDF font or legacy bitmap font).
*	@param	color	Packed RGBA color tint.
**/
void R_DrawString3DOccluded_RTX( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color ) {
	DrawString3D_Internal( origin, angles, scale, text, font, color, true );
}

/**
*	@brief	Draw 3D world-space text as an always-on-top overlay.
*	@param	origin	World-space 3D origin (X, Y, Z).
*	@param	angles	Pitch, Yaw, Roll orientation in degrees, or NULL for camera-facing billboard.
*	@param	scale	Character height in Quake world units.
*	@param	text	String to render.
*	@param	font	Font handle (TrueType MTSDF font or legacy bitmap font).
*	@param	color	Packed RGBA color tint.
**/
void R_DrawString3DNonOccluded_RTX( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color ) {
	DrawString3D_Internal( origin, angles, scale, text, font, color, false );
}

/**
*	@brief	Draw 3D world-space text (defaults to occluded).
*	@param	origin	World-space 3D origin (X, Y, Z).
*	@param	angles	Pitch, Yaw, Roll orientation in degrees, or NULL for camera-facing billboard.
*	@param	scale	Character height in Quake world units.
*	@param	text	String to render.
*	@param	font	Font handle (TrueType MTSDF font or legacy bitmap font).
*	@param	color	Packed RGBA color tint.
**/
void R_DrawString3D_RTX( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color ) {
	DrawString3D_Internal( origin, angles, scale, text, font, color, true );
}

// vim: shiftwidth=4 noexpandtab tabstop=4 cindent
