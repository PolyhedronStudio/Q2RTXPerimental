/********************************************************************
*
*
*	VKPT Renderer: Shadow Mapping
*
*	Implements hardware-accelerated shadow mapping for real-time shadows
*	from the primary sun light source. Uses traditional depth buffer
*	rendering to generate shadow maps that can be sampled during path
*	tracing to determine light visibility and create realistic shadows.
*
*	Architecture:
*	- Dual shadow maps: One for opaque geometry, one for transparent
*	- Two-layer depth array for efficient sampling and filtering
*	- Orthographic projection from sun's perspective
*	- Instance-based rendering for models and dynamic objects
*
*	Algorithm:
*	1. Setup: Compute orthographic projection from sun direction
*	2. Render Pass 1: Rasterize opaque/dynamic geometry to depth buffer
*	3. Render Pass 2: Rasterize transparent geometry to second layer
*	4. Sample: Path tracer samples shadow maps for visibility tests
*
*	Features:
*	- Adaptive shadow map framing (fits to scene AABB)
*	- Random disk sampling for soft shadow support
*	- Front-face culling to reduce shadow acne
*	- Separate handling of transparent surfaces
*	- Instance batching for model rendering
*
*	Configuration:
*	- SHADOWMAP_SIZE: Resolution of shadow depth buffer (defined in vkpt.h)
*	- MAX_MODEL_INSTANCES: Maximum number of model instances per frame
*
*	Performance:
*	- Uses VK_FORMAT_D32_SFLOAT for high precision depth
*	- Single-pass geometry rendering with push constants
*	- Efficient instance batching reduces draw call overhead
*
*
********************************************************************/
/*
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

#include "vkpt.h"

#include <float.h>



/**
*
*
*
*	Module State:
*
*
*
**/
//! Pipeline layout for shadow map rendering.
VkPipelineLayout        pipeline_layout_smap;
//! Render pass for depth-only shadow map generation.
VkRenderPass            render_pass_smap;
//! Graphics pipeline for shadow map rasterization.
VkPipeline              pipeline_smap;
//! Framebuffer for first shadow map layer (opaque geometry).
static VkFramebuffer           framebuffer_smap;
//! Framebuffer for second shadow map layer (transparent geometry).
static VkFramebuffer           framebuffer_smap2;
//! Shadow map depth image (2-layer array).
static VkImage                 img_smap;
//! Image view for first shadow map layer.
static VkImageView             imv_smap_depth;
//! Image view for second shadow map layer.
static VkImageView             imv_smap_depth2;
//! Image view for both shadow map layers as 2D array.
static VkImageView             imv_smap_depth_array;
//! Device memory for shadow map image.
static VkDeviceMemory          mem_smap;


/**
*
*
*
*	Instance Data Structures:
*
*
*
**/
//! Instance data for shadow map rendering of models.
typedef struct
{
	mat4_t model_matrix;    //! Model-to-world transformation matrix.
	VkBuffer buffer;        //! Vertex buffer containing model geometry.
	size_t vertex_offset;   //! Offset into vertex buffer (in bytes).
	uint32_t prim_count;    //! Number of triangles to render.
} shadowmap_instance_t;

//! Current number of shadow map instances to render this frame.
static uint32_t num_shadowmap_instances;
//! Array of shadow map instances for batched rendering.
static shadowmap_instance_t shadowmap_instances[MAX_MODEL_INSTANCES];



/**
*
*
*
*	Public API - Instance Management:
*
*
*
**/
/**
*	@brief	Resets the shadow map instance list for a new frame.
*	@note	Call this at the start of each frame before adding instances.
**/
void vkpt_shadow_map_reset_instances()
{
	num_shadowmap_instances = 0;
}

/**
*	@brief	Adds a model instance to be rendered into the shadow map.
*	@param	model_matrix	Model-to-world transformation matrix (4x4).
*	@param	buffer			Vertex buffer containing the model geometry.
*	@param	vertex_offset	Byte offset into the vertex buffer.
*	@param	prim_count		Number of triangles to render.
*	@note	Silently ignores instances beyond MAX_MODEL_INSTANCES limit.
**/
void vkpt_shadow_map_add_instance( const float* model_matrix, VkBuffer buffer, size_t vertex_offset, uint32_t prim_count )
{
	if ( num_shadowmap_instances < MAX_MODEL_INSTANCES ) {
		shadowmap_instance_t* instance = shadowmap_instances + num_shadowmap_instances;

		memcpy( instance->model_matrix, model_matrix, sizeof( mat4_t ) );
		instance->buffer = buffer;
		instance->vertex_offset = vertex_offset;
		instance->prim_count = prim_count;

		++num_shadowmap_instances;
	}
}

/**
*
*
*
*	Render Pass Creation (Internal):
*
*
*
**/
/**
*	@brief	Creates the Vulkan render pass for shadow map rendering.
*	@note	Uses depth-only attachment with VK_FORMAT_D32_SFLOAT for high precision.
*			Configured for clear-on-load and store-on-complete operations.
**/
static void
create_render_pass( void )
{
	LOG_FUNC();
	
	// Create depth attachment descriptor for 32-bit float depth buffer.
	VkAttachmentDescription depth_attachment = {
		.format         = VK_FORMAT_D32_SFLOAT,
		.samples        = VK_SAMPLE_COUNT_1_BIT,
		.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	// Reference to depth attachment for subpass.
	VkAttachmentReference depth_attachment_ref = {
		.attachment = 0, // Index in render pass attachment array.
		.layout     = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR,
	};

	// Single subpass for depth rendering.
	VkSubpassDescription subpass = {
		.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.pDepthStencilAttachment = &depth_attachment_ref,
	};

	// Subpass dependency for synchronization with external operations.
	VkSubpassDependency dependencies[] = {
		{
			.srcSubpass    = VK_SUBPASS_EXTERNAL,
			.dstSubpass    = 0, // Index for own subpass.
			.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
			               | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		},
	};

	VkRenderPassCreateInfo render_pass_info = {
		.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments    = &depth_attachment,
		.subpassCount    = 1,
		.pSubpasses      = &subpass,
		.dependencyCount = LENGTH(dependencies),
		.pDependencies   = dependencies,
	};

	_VK(vkCreateRenderPass(qvk.device, &render_pass_info, NULL, &render_pass_smap));
	ATTACH_LABEL_VARIABLE( render_pass_smap, RENDER_PASS );
}



/**
*
*
*
*	Public API - Initialization & Cleanup:
*
*
*
**/
/**
*	@brief	Initializes shadow mapping resources including images, views, and render pass.
*	@return	VK_SUCCESS on success, Vulkan error code on failure.
*	@note	Creates a 2-layer depth array image at SHADOWMAP_SIZE resolution.
*			Transitions image to shader-read-only layout after creation.
**/
VkResult
vkpt_shadow_map_initialize()
{
	create_render_pass();

	// Create pipeline layout with push constants for view-projection matrix.
	VkPushConstantRange push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof( float ) * 16,
	};

	CREATE_PIPELINE_LAYOUT( qvk.device, &pipeline_layout_smap,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constant_range,
	);

	// Create 2-layer depth array image for shadow maps.
	VkImageCreateInfo img_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.extent = {
			.width = SHADOWMAP_SIZE,
			.height = SHADOWMAP_SIZE,
			.depth = 1,
		},
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_D32_SFLOAT,
		.mipLevels = 1,
		.arrayLayers = 2,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		       | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = qvk.queue_idx_graphics,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	_VK( vkCreateImage( qvk.device, &img_info, NULL, &img_smap ) );
	ATTACH_LABEL_VARIABLE( img_smap, IMAGE );

	// Allocate and bind device memory for shadow map image.
	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements( qvk.device, img_smap, &mem_req );

	_VK( allocate_gpu_memory( mem_req, &mem_smap ) );

	_VK( vkBindImageMemory( qvk.device, img_smap, mem_smap, 0 ) );

	// Create image view for first shadow map layer (opaque geometry).
	VkImageViewCreateInfo img_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_D32_SFLOAT,
		.image = img_smap,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A,
		},
	};
	_VK( vkCreateImageView( qvk.device, &img_view_info, NULL, &imv_smap_depth ) );
	ATTACH_LABEL_VARIABLE( imv_smap_depth, IMAGE_VIEW );

	// Create image view for second shadow map layer (transparent geometry).
	img_view_info.subresourceRange.baseArrayLayer = 1;
	_VK( vkCreateImageView( qvk.device, &img_view_info, NULL, &imv_smap_depth2 ) );
	ATTACH_LABEL_VARIABLE( imv_smap_depth2, IMAGE_VIEW );

	// Create 2D array image view for both layers (for shader sampling).
	img_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	img_view_info.subresourceRange.baseArrayLayer = 0;
	img_view_info.subresourceRange.layerCount = 2;
	_VK( vkCreateImageView( qvk.device, &img_view_info, NULL, &imv_smap_depth_array ) );
	ATTACH_LABEL_VARIABLE( imv_smap_depth_array, IMAGE_VIEW );

	// Transition shadow map image to shader-read-only layout.
	VkCommandBuffer cmd_buf = vkpt_begin_command_buffer( &qvk.cmd_buffers_graphics );

	IMAGE_BARRIER( cmd_buf,
		img_mem_barrier.image = img_smap;
		img_mem_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		img_mem_barrier.subresourceRange.levelCount = 1;
		img_mem_barrier.subresourceRange.layerCount = 2;
		img_mem_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		img_mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		img_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		img_mem_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	);

	vkpt_submit_command_buffer_simple( cmd_buf, qvk.queue_graphics, true );

	return VK_SUCCESS;
}

/**
*	@brief	Destroys all shadow mapping resources.
*	@return	VK_SUCCESS on success.
*	@note	Frees image views, images, memory, render pass, and pipeline layout.
**/
VkResult
vkpt_shadow_map_destroy()
{
	vkDestroyImageView( qvk.device, imv_smap_depth, NULL );
	imv_smap_depth = VK_NULL_HANDLE;
	vkDestroyImageView( qvk.device, imv_smap_depth2, NULL );
	imv_smap_depth2 = VK_NULL_HANDLE;
	vkDestroyImageView( qvk.device, imv_smap_depth_array, NULL );
	imv_smap_depth_array = VK_NULL_HANDLE;
	vkDestroyImage( qvk.device, img_smap, NULL );
	img_smap = VK_NULL_HANDLE;
	vkFreeMemory( qvk.device, mem_smap, NULL );
	mem_smap = VK_NULL_HANDLE;

	vkDestroyRenderPass( qvk.device, render_pass_smap, NULL );
	render_pass_smap = VK_NULL_HANDLE;
	vkDestroyPipelineLayout( qvk.device, pipeline_layout_smap, NULL );
	pipeline_layout_smap = VK_NULL_HANDLE;

	return VK_SUCCESS;
}

/**
*	@brief	Returns the shadow map array image view for shader access.
*	@return	Vulkan image view handle for 2D array depth texture.
**/
VkImageView vkpt_shadow_map_get_view()
{
	return imv_smap_depth_array;
}

/**
*
*
*
*	Public API - Pipeline Management:
*
*
*
**/
/**
*	@brief	Creates the shadow map graphics pipeline and framebuffers.
*	@return	VK_SUCCESS on success, Vulkan error code on failure.
*	@note	Creates a depth-only graphics pipeline with front-face culling.
*			Sets up two framebuffers for the two shadow map layers.
**/
VkResult
vkpt_shadow_map_create_pipelines()
{
	LOG_FUNC();

	// Create vertex shader stage (no fragment shader needed for depth-only).
	VkPipelineShaderStageCreateInfo shader_info[] = {
		SHADER_STAGE( QVK_MOD_SHADOW_MAP_VERT, VK_SHADER_STAGE_VERTEX_BIT )
	};

	// Vertex input: Single binding with 3D position data.
	VkVertexInputBindingDescription vertex_binding_desc = {
		.binding = 0,
		.stride = sizeof( float ) * 3,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	VkVertexInputAttributeDescription vertex_attribute_desc = {
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = 0
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount   = 1,
		.pVertexBindingDescriptions      = &vertex_binding_desc,
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions    = &vertex_attribute_desc,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	VkViewport viewport = {
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = (float)SHADOWMAP_SIZE,
		.height   = (float)SHADOWMAP_SIZE,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = { SHADOWMAP_SIZE, SHADOWMAP_SIZE }
	};

	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports    = &viewport,
		.scissorCount  = 1,
		.pScissors     = &scissor,
	};

	// Rasterization state: Front-face culling to reduce shadow acne.
	VkPipelineRasterizationStateCreateInfo rasterizer_state = {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.lineWidth               = 1.0f,
		.cullMode                = VK_CULL_MODE_FRONT_BIT,
		.frontFace               = VK_FRONT_FACE_CLOCKWISE,
	};

	// No multisampling for shadow maps.
	VkPipelineMultisampleStateCreateInfo multisample_state = {
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable   = VK_FALSE,
		.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading      = 1.0f,
		.pSampleMask           = NULL,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable      = VK_FALSE,
	};

	// Depth test and write enabled, using less-than comparison.
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
	};

	// Dynamic viewport and scissor for flexibility.
	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = LENGTH(dynamic_states),
		.pDynamicStates = dynamic_states
	};

	// Create graphics pipeline for shadow map rendering.
	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = LENGTH(shader_info),
		.pStages    = shader_info,

		.pVertexInputState   = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pViewportState      = &viewport_state,
		.pRasterizationState = &rasterizer_state,
		.pMultisampleState   = &multisample_state,
		.pDepthStencilState  = &depth_stencil_state,
		.pColorBlendState    = NULL,
		.pDynamicState       = &dynamic_state_info,
		
		.layout              = pipeline_layout_smap,
		.renderPass          = render_pass_smap,
		.subpass             = 0,

		.basePipelineHandle  = VK_NULL_HANDLE,
		.basePipelineIndex   = -1,
	};

	_VK( vkCreateGraphicsPipelines( qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline_smap ) );
	ATTACH_LABEL_VARIABLE( pipeline_smap, PIPELINE );

	// Create framebuffer for first shadow map layer.
	VkImageView attachments[] = {
		imv_smap_depth
	};

	VkFramebufferCreateInfo fb_create_info = {
		.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass      = render_pass_smap,
		.attachmentCount = 1,
		.pAttachments    = attachments,
		.width           = SHADOWMAP_SIZE,
		.height          = SHADOWMAP_SIZE,
		.layers          = 1,
	};

	_VK( vkCreateFramebuffer( qvk.device, &fb_create_info, NULL, &framebuffer_smap ) );
	ATTACH_LABEL_VARIABLE( framebuffer_smap, FRAMEBUFFER );

	// Create framebuffer for second shadow map layer.
	attachments[0] = imv_smap_depth2;
	_VK( vkCreateFramebuffer( qvk.device, &fb_create_info, NULL, &framebuffer_smap2 ) );
	ATTACH_LABEL_VARIABLE( framebuffer_smap2, FRAMEBUFFER );

	return VK_SUCCESS;
}

/**
*	@brief	Destroys shadow map pipelines and framebuffers.
*	@return	VK_SUCCESS on success.
**/
VkResult
vkpt_shadow_map_destroy_pipelines()
{
	LOG_FUNC();
	vkDestroyFramebuffer( qvk.device, framebuffer_smap, NULL );
	framebuffer_smap = VK_NULL_HANDLE;
	vkDestroyFramebuffer( qvk.device, framebuffer_smap2, NULL );
	framebuffer_smap2 = VK_NULL_HANDLE;
	vkDestroyPipeline( qvk.device, pipeline_smap, NULL );
	pipeline_smap = VK_NULL_HANDLE;
	return VK_SUCCESS;
}

/**
*
*
*
*	Public API - Rendering:
*
*
*
**/
/**
*	@brief	Renders geometry into the shadow map from the sun's perspective.
*	@param	cmd_buf					Command buffer to record rendering commands.
*	@param	view_projection_matrix	View-projection matrix from sun's orthographic projection.
*	@param	static_offset			Vertex offset for static BSP geometry.
*	@param	num_static_verts		Number of vertices in static geometry.
*	@param	dynamic_offset			Vertex offset for dynamic geometry.
*	@param	num_dynamic_verts		Number of vertices in dynamic geometry.
*	@param	transparent_offset		Vertex offset for transparent geometry.
*	@param	num_transparent_verts	Number of vertices in transparent geometry.
*	@return	VK_SUCCESS on success.
*	@note	Renders in two passes: opaque/dynamic to layer 0, transparent to layer 1.
*			Transitions shadow map image layouts before and after rendering.
**/
VkResult
vkpt_shadow_map_render( VkCommandBuffer cmd_buf, float* view_projection_matrix,
	uint32_t static_offset, uint32_t num_static_verts,
	uint32_t dynamic_offset, uint32_t num_dynamic_verts,
	uint32_t transparent_offset, uint32_t num_transparent_verts )
{
	// Transition shadow map from shader-read to depth-attachment layout.
	IMAGE_BARRIER( cmd_buf,
		img_mem_barrier.image = img_smap;
		img_mem_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		img_mem_barrier.subresourceRange.levelCount = 1;
		img_mem_barrier.subresourceRange.layerCount = 2;
		img_mem_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		img_mem_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		img_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_mem_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	);

	// Clear depth to 1.0 (far plane).
	VkClearValue clear_depth = {};
	clear_depth.depthStencil.depth = 1.f;
	
	// Begin render pass for first shadow map layer (opaque geometry).
	VkRenderPassBeginInfo render_pass_info = {
		.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass        = render_pass_smap,
		.framebuffer       = framebuffer_smap,
		.renderArea.offset = { 0, 0 },
		.renderArea.extent = { SHADOWMAP_SIZE, SHADOWMAP_SIZE },
		.clearValueCount   = 1,
		.pClearValues      = &clear_depth
	};

	vkCmdBeginRenderPass( cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE );
	vkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_smap );

	// Set viewport and scissor.
	VkViewport viewport =
	{
		.width = ( float )SHADOWMAP_SIZE,
		.height = ( float )SHADOWMAP_SIZE,
		.minDepth = 0,
		.maxDepth = 1.0f,
	};

	vkCmdSetViewport( cmd_buf, 0, 1, &viewport );

	VkRect2D scissor =
	{
		.extent.width = SHADOWMAP_SIZE,
		.extent.height = SHADOWMAP_SIZE,
		.offset.x = 0,
		.offset.y = 0,
	};

	vkCmdSetScissor( cmd_buf, 0, 1, &scissor );

	// Push view-projection matrix for static/dynamic geometry.
	vkCmdPushConstants( cmd_buf, pipeline_layout_smap, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( mat4_t ), view_projection_matrix );

	// Render static BSP geometry.
	VkDeviceSize vertex_offset = vkpt_refdef.bsp_mesh_world.vertex_data_offset;
	vkCmdBindVertexBuffers( cmd_buf, 0, 1, &qvk.buf_world.buffer, &vertex_offset );

	vkCmdDraw( cmd_buf, num_static_verts, 1, static_offset, 0 );

	// Render dynamic geometry (entities, particles, etc).
	vertex_offset = 0;
	vkCmdBindVertexBuffers( cmd_buf, 0, 1, &qvk.buf_positions_instanced.buffer, &vertex_offset );

	vkCmdDraw( cmd_buf, num_dynamic_verts, 1, dynamic_offset, 0 );

	// Render model instances with their own transformations.
	mat4_t mvp_matrix;

	for ( uint32_t instance_idx = 0; instance_idx < num_shadowmap_instances; instance_idx++ ) {
		const shadowmap_instance_t* mi = shadowmap_instances + instance_idx;

		// Compute model-view-projection matrix.
		mult_matrix_matrix( mvp_matrix, view_projection_matrix, mi->model_matrix );

		vkCmdPushConstants( cmd_buf, pipeline_layout_smap, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( mat4_t ), mvp_matrix );

		vkCmdBindVertexBuffers( cmd_buf, 0, 1, &mi->buffer, &mi->vertex_offset );

		vkCmdDraw( cmd_buf, mi->prim_count * 3, 1, 0, 0 );
	}

	vkCmdEndRenderPass( cmd_buf );

	// Begin render pass for second shadow map layer (transparent geometry).
	render_pass_info.framebuffer = framebuffer_smap2;
	vkCmdBeginRenderPass( cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE );

	vkCmdPushConstants( cmd_buf, pipeline_layout_smap, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( mat4_t ), view_projection_matrix );

	// Render transparent BSP geometry.
	vertex_offset = vkpt_refdef.bsp_mesh_world.vertex_data_offset;
	vkCmdBindVertexBuffers( cmd_buf, 0, 1, &qvk.buf_world.buffer, &vertex_offset );

	vkCmdDraw( cmd_buf, num_transparent_verts, 1, transparent_offset, 0 );

	vkCmdEndRenderPass( cmd_buf );

	// Transition shadow map back to shader-read layout for sampling.
	IMAGE_BARRIER( cmd_buf,
		img_mem_barrier.image = img_smap;
		img_mem_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		img_mem_barrier.subresourceRange.levelCount = 1;
		img_mem_barrier.subresourceRange.layerCount = 2;
		img_mem_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		img_mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		img_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		img_mem_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		);

	return VK_SUCCESS;
}



/**
*
*
*
*	Shadow Map Setup (Internal):
*
*
*
**/
/**
*	@brief	Generates a random point on a unit disk using polar coordinates.
*	@param	u	Output X coordinate on disk [-1, 1].
*	@param	v	Output Y coordinate on disk [-1, 1].
*	@note	Uses uniform sampling in polar coordinates for soft shadow support.
**/
static void sample_disk( float* u, float* v )
{
	float a = frand();
	float b = frand();

	// Convert uniform random variables to polar coordinates.
	float theta = 2.0 * M_PI * a;
	float r = sqrtf( b );

	// Convert to Cartesian coordinates.
	*u = r * cos( theta );
	*v = r * sin( theta );
}

/**
*	@brief	Computes the shadow map view-projection matrix from sun light parameters.
*	@param	light				Sun light source parameters (direction, angular size).
*	@param	bbox_min			Minimum corner of scene bounding box.
*	@param	bbox_max			Maximum corner of scene bounding box.
*	@param	VP					Output view-projection matrix (4x4).
*	@param	depth_scale			Output depth range scale factor.
*	@param	random_sampling		If true, randomly perturb light direction for soft shadows.
*	@note	Creates an orthographic projection that tightly fits the scene AABB.
*			The projection is square (equal X/Y extents) to avoid stretching.
*			Random sampling perturbs the light direction within its angular size.
**/
void vkpt_shadow_map_setup( const sun_light_t* light, const float* bbox_min, const float* bbox_max, float* VP, float* depth_scale, bool random_sampling )
{
	// Compute orthonormal basis from sun direction.
	vec3_t up_dir = { 0.0f, 0.0f, 1.0f };
	if ( light->direction[2] >= 0.99f )
		VectorSet( up_dir, 1.f, 0.f, 0.f );

	vec3_t look_dir;
	VectorScale( light->direction, -1.f, look_dir );
	VectorNormalize( look_dir );
	vec3_t left_dir;
	CrossProduct( up_dir, look_dir, left_dir );
	VectorNormalize( left_dir );
	CrossProduct( look_dir, left_dir, up_dir );
	VectorNormalize( up_dir );

	// Apply random sampling for soft shadow support.
	if ( random_sampling ) {
		float u, v;
		sample_disk( &u, &v );
		
		// Perturb look direction within angular size cone.
		VectorMA( look_dir, u * light->angular_size_rad * 0.5f, left_dir, look_dir );
		VectorMA( look_dir, v * light->angular_size_rad * 0.5f, up_dir, look_dir );

		VectorNormalize( look_dir );

		// Re-orthonormalize basis after perturbation.
		CrossProduct( up_dir, look_dir, left_dir );
		VectorNormalize( left_dir );
		CrossProduct( look_dir, left_dir, up_dir );
		VectorNormalize( up_dir );
	}

	// Construct view matrix from orthonormal basis.
	float view_matrix[16] = {
		left_dir[0], up_dir[0], look_dir[0], 0.f,
		left_dir[1], up_dir[1], look_dir[1], 0.f,
		left_dir[2], up_dir[2], look_dir[2], 0.f,
		0.f, 0.f, 0.f, 1.f
	};

	// Transform scene AABB corners to view space to find tight bounds.
	vec3_t view_aabb_min = { FLT_MAX, FLT_MAX, FLT_MAX };
	vec3_t view_aabb_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for ( int i = 0; i < 8; i++ ) {
		float corner[4];
		corner[0] = ( i & 1 ) ? bbox_max[0] : bbox_min[0];
		corner[1] = ( i & 2 ) ? bbox_max[1] : bbox_min[1];
		corner[2] = ( i & 4 ) ? bbox_max[2] : bbox_min[2];
		corner[3] = 1.f;

		float view_corner[4];
		mult_matrix_vector( view_corner, view_matrix, corner );

		view_aabb_min[0] = min( view_aabb_min[0], view_corner[0] );
		view_aabb_min[1] = min( view_aabb_min[1], view_corner[1] );
		view_aabb_min[2] = min( view_aabb_min[2], view_corner[2] );
		view_aabb_max[0] = max( view_aabb_max[0], view_corner[0] );
		view_aabb_max[1] = max( view_aabb_max[1], view_corner[1] );
		view_aabb_max[2] = max( view_aabb_max[2], view_corner[2] );
	}

	// Make projection square to avoid stretching artifacts.
	vec3_t diagonal;
	VectorSubtract( view_aabb_max, view_aabb_min, diagonal );

	float maxXY = max( diagonal[0], diagonal[1] );
	vec3_t diff;
	diff[0] = ( maxXY - diagonal[0] ) * 0.5f;
	diff[1] = ( maxXY - diagonal[1] ) * 0.5f;
	diff[2] = 0.f;
	VectorSubtract( view_aabb_min, diff, view_aabb_min );
	VectorAdd( view_aabb_max, diff, view_aabb_max );

	// Create orthographic projection matrix and combine with view.
	float projection_matrix[16];
	create_orthographic_matrix( projection_matrix, view_aabb_min[0], view_aabb_max[0], view_aabb_min[1], view_aabb_max[1], view_aabb_min[2], view_aabb_max[2] );
	mult_matrix_matrix( VP, projection_matrix, view_matrix );

	*depth_scale = view_aabb_max[2] - view_aabb_min[2];
}
