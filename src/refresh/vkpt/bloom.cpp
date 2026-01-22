/********************************************************************
*
*
*	VKPT Renderer: Bloom Post-Processing Effect
*
*	Implements a high-quality bloom effect using Gaussian blur to create
*	realistic light bleeding from bright areas. Simulates the scattering
*	of light through camera optics and the human eye's response to
*	intense light sources.
*
*	Algorithm:
*	1. Downscale: Reduce resolution for efficient blur computation
*	2. Blur: Two-pass separable Gaussian blur (horizontal + vertical)
*	3. Composite: Blend bloom with scene radiance
*
*	Features:
*	- Adaptive sigma based on output resolution (scales with screen size)
*	- Separate underwater bloom settings for enhanced effect
*	- Smooth transitions between normal and underwater bloom
*	- Debug mode for visualization and tuning
*
*	Configuration (cvars):
*	- flt_bloom_enable:            Enable/disable bloom (0/1)
*	- flt_bloom_intensity:         Bloom strength for normal scenes
*	- flt_bloom_sigma:             Blur radius (fraction of screen height)
*	- flt_bloom_intensity_water:   Bloom strength underwater
*	- flt_bloom_sigma_water:       Blur radius underwater
*	- flt_bloom_debug:             Debug visualization mode
*
*	Performance:
*	- Uses quarter-resolution buffers for blur passes
*	- Separable Gaussian kernel reduces complexity from O(n²) to O(n)
*	- Sample count automatically adjusted based on effective sigma
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
#include "system/system.h"



/**
*
*
*
*	Pipeline Identifiers:
*
*
*
**/
//! Pipeline types for bloom effect stages.
enum {
	BLUR,             //! Gaussian blur pass (horizontal + vertical).
	COMPOSITE,        //! Final compositing with scene.
	DOWNSCALE,        //! Downscale high-res input for efficient blur.

	BLOOM_NUM_PIPELINES
};



/**
*
*
*
*	Module State:
*
*
*
**/
//! Compute pipelines for bloom stages.
static VkPipeline pipelines[BLOOM_NUM_PIPELINES];
//! Pipeline layout for blur passes.
static VkPipelineLayout pipeline_layout_blur;
//! Pipeline layout for composite pass.
static VkPipelineLayout pipeline_layout_composite;

//! Push constants for Gaussian blur shader.
struct bloom_blur_push_constants {
	//! Horizontal pixel step (1.0 for horizontal blur, 0.0 for vertical).
	float pixstep_x;
	//! Vertical pixel step (0.0 for horizontal blur, 1.0 for vertical).
	float pixstep_y;
	//! Scale factor for Gaussian exponent: -1/(2*sigma²).
	float argument_scale;
	//! Normalization scale: 1/(sqrt(2π)*sigma).
	float normalization_scale;
	//! Number of samples per direction (computed from sigma).
	int num_samples;
	//! Pass identifier (0 = horizontal, 1 = vertical).
	int pass;
};

//! Precomputed push constants for horizontal blur.
static struct bloom_blur_push_constants push_constants_hblur;
//! Precomputed push constants for vertical blur.
static struct bloom_blur_push_constants push_constants_vblur;

//! Bloom enable/disable cvar.
cvar_t *cvar_bloom_enable = NULL;
//! Debug visualization cvar.
cvar_t *cvar_bloom_debug = NULL;
//! Blur sigma (radius) cvar for normal scenes.
cvar_t *cvar_bloom_sigma = NULL;
//! Intensity multiplier cvar for normal scenes.
cvar_t *cvar_bloom_intensity = NULL;
//! Blur sigma cvar for underwater scenes.
cvar_t *cvar_bloom_sigma_water = NULL;
//! Intensity multiplier cvar for underwater scenes.
cvar_t *cvar_bloom_intensity_water = NULL;

//! Current bloom intensity (interpolated for smooth transitions).
static float bloom_intensity;
//! Current bloom sigma (blur radius as fraction of screen height).
static float bloom_sigma;
//! Underwater transition animation state [0.0, 1.0].
static float under_water_animation;



/**
*
*
*
*	Blur Parameter Computation:
*
*
*
**/
/**
*	@brief	Computes push constants for Gaussian blur based on current sigma.
*	@note	Calculates pixel steps, Gaussian parameters, and sample count for both
*			horizontal and vertical blur passes. Effective sigma is computed from
*			bloom_sigma and screen height, then clamped to reasonable range [1, 100].
*			Sample count is 4*sigma for proper kernel coverage.
**/
static void compute_push_constants( void ) {
	// Convert sigma from screen-height fraction to pixels (at quarter resolution).
	float sigma_pixels = bloom_sigma * (float)( qvk.extent_taa_output.height );

	// Effective sigma at quarter resolution, with safety clamping.
	float effective_sigma = sigma_pixels * 0.25f;
	effective_sigma = min( effective_sigma, 100.f );
	effective_sigma = max( effective_sigma, 1.f );

	// Set up horizontal blur parameters.
	push_constants_hblur.pixstep_x = 1.f;
	push_constants_hblur.pixstep_y = 0.f;
	push_constants_hblur.argument_scale = -1.f / ( 2.0 * effective_sigma * effective_sigma );
	push_constants_hblur.normalization_scale = 1.f / ( sqrtf( 2 * M_PI ) * effective_sigma );
	push_constants_hblur.num_samples = roundf( effective_sigma * 4.f );
	push_constants_hblur.pass = 0;

	// Copy to vertical blur and adjust pixel steps.
	push_constants_vblur = push_constants_hblur;
	push_constants_vblur.pixstep_x = 0.f;
	push_constants_vblur.pixstep_y = 1.f;
	push_constants_vblur.pass = 1;
}



/**
*
*
*
*	Public Interface:
*
*
*
**/
/**
*	@brief	Resets bloom state to initial values from cvars.
*	@note	Called when entering a new level or resetting renderer state.
**/
void vkpt_bloom_reset() {
	bloom_intensity = cvar_bloom_intensity->value;
	bloom_sigma = cvar_bloom_sigma->value;
	under_water_animation = 0.f;
}

//! Linear interpolation helper.
static float mix( float a, float b, float s ) {
	return a * ( 1.f - s ) + b * s;
}

/**
*	@brief	Updates bloom parameters for the current frame.
*	@param	ubo			Uniform buffer to write bloom parameters.
*	@param	frame_time	Delta time since last frame (for smooth transitions).
*	@param	under_water	True if camera is underwater.
*	@param	menu_mode	True if in menu (may affect bloom behavior).
*	@note	Smoothly transitions between normal and underwater bloom settings over
*			~0.33 seconds. Writes final bloom parameters to the uniform buffer.
**/
void vkpt_bloom_update( QVKUniformBuffer_t* ubo, float frame_time, bool under_water, bool menu_mode ) {
	if ( under_water ) {
		// Fade in underwater bloom.
		under_water_animation = min( 1.f, under_water_animation + frame_time * 3.f );
		bloom_intensity = cvar_bloom_intensity_water->value;
		bloom_sigma = cvar_bloom_sigma->value;
	} else {
		// Fade out underwater bloom.
		under_water_animation = max( 0.f, under_water_animation - frame_time * 3.f );
		bloom_intensity = mix( cvar_bloom_intensity->value, cvar_bloom_intensity_water->value, under_water_animation );
		bloom_sigma = mix(cvar_bloom_sigma->value, cvar_bloom_sigma_water->value, under_water_animation);
	}

	static uint32_t menu_start_ms = 0;

	if (menu_mode)
	{
		if (menu_start_ms == 0)
			menu_start_ms = Sys_Milliseconds();
		uint32_t current_ms = Sys_Milliseconds();

		float phase = max(0.f, min(1.f, (float)(current_ms - menu_start_ms) / 150.f));
		ubo->tonemap_hdr_clamp_strength = phase; // Clamp color in HDR mode, to ensure menu is legible
		phase = powf(phase, 0.25f);

		bloom_sigma = phase * 0.03f;

		ubo->bloom_intensity = 1.f;
	}
	else
	{
		menu_start_ms = 0;

		ubo->bloom_intensity = bloom_intensity;
		ubo->tonemap_hdr_clamp_strength = 0.f;
	}
}

VkResult
vkpt_bloom_initialize()
{
	cvar_bloom_enable = Cvar_Get("bloom_enable", "1", 0);
	cvar_bloom_debug = Cvar_Get("bloom_debug", "0", 0);
	cvar_bloom_sigma = Cvar_Get("bloom_sigma", "0.037", 0); // relative to screen height
	cvar_bloom_intensity = Cvar_Get("bloom_intensity", "0.002", 0);
	cvar_bloom_sigma_water = Cvar_Get("bloom_sigma_water", "0.037", 0);
	cvar_bloom_intensity_water = Cvar_Get("bloom_intensity_water", "0.2", 0);

	VkDescriptorSetLayout desc_set_layouts[] = {
		qvk.desc_set_layout_ubo, qvk.desc_set_layout_textures,
		qvk.desc_set_layout_vertex_buffer
	};

	VkPushConstantRange push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset     = 0,
		.size       = sizeof(push_constants_hblur)
	};

	CREATE_PIPELINE_LAYOUT(qvk.device, &pipeline_layout_blur,
		.setLayoutCount         = LENGTH(desc_set_layouts),
		.pSetLayouts            = desc_set_layouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &push_constant_range
	);
	ATTACH_LABEL_VARIABLE(pipeline_layout_blur, PIPELINE_LAYOUT);

	CREATE_PIPELINE_LAYOUT(qvk.device, &pipeline_layout_composite,
		.setLayoutCount         = LENGTH(desc_set_layouts),
		.pSetLayouts            = desc_set_layouts,
	);
	ATTACH_LABEL_VARIABLE(pipeline_layout_composite, PIPELINE_LAYOUT);

	return VK_SUCCESS;
}

VkResult
vkpt_bloom_destroy()
{
	vkDestroyPipelineLayout(qvk.device, pipeline_layout_blur, NULL);
	pipeline_layout_blur = NULL;
	vkDestroyPipelineLayout(qvk.device, pipeline_layout_composite, NULL);
	pipeline_layout_composite = NULL;

	return VK_SUCCESS;
}

VkResult
vkpt_bloom_create_pipelines()
{
	VkComputePipelineCreateInfo pipeline_info[BLOOM_NUM_PIPELINES] = {
		[BLUR] = {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = SHADER_STAGE(QVK_MOD_BLOOM_BLUR_COMP, VK_SHADER_STAGE_COMPUTE_BIT),
			.layout = pipeline_layout_blur,
		},
		[COMPOSITE] = {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = SHADER_STAGE(QVK_MOD_BLOOM_COMPOSITE_COMP, VK_SHADER_STAGE_COMPUTE_BIT),
			.layout = pipeline_layout_composite,
		},
		[DOWNSCALE] = {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = SHADER_STAGE(QVK_MOD_BLOOM_DOWNSCALE_COMP, VK_SHADER_STAGE_COMPUTE_BIT),
			.layout = pipeline_layout_composite,
		}
	};

	_VK(vkCreateComputePipelines(qvk.device, 0, LENGTH(pipeline_info), pipeline_info, 0, pipelines));
	return VK_SUCCESS;
}

VkResult
vkpt_bloom_destroy_pipelines()
{
	for(int i = 0; i < BLOOM_NUM_PIPELINES; i++)
		vkDestroyPipeline(qvk.device, pipelines[i], NULL);

	return VK_SUCCESS;
}


#define BARRIER_COMPUTE(cmd_buf, img) \
	do { \
		VkImageSubresourceRange subresource_range = { \
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, \
			.baseMipLevel   = 0, \
			.levelCount     = 1, \
			.baseArrayLayer = 0, \
			.layerCount     = 1 \
		}; \
		IMAGE_BARRIER(cmd_buf, \
				.image            = img, \
				.subresourceRange = subresource_range, \
				.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT, \
				.dstAccessMask    = VK_ACCESS_SHADER_READ_BIT, \
				.oldLayout        = VK_IMAGE_LAYOUT_GENERAL, \
				.newLayout        = VK_IMAGE_LAYOUT_GENERAL, \
		); \
	} while(0)

#define BARRIER_TO_COPY_DEST(cmd_buf, img) \
	do { \
		VkImageSubresourceRange subresource_range = { \
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, \
			.baseMipLevel   = 0, \
			.levelCount     = 1, \
			.baseArrayLayer = 0, \
			.layerCount     = 1 \
		}; \
		IMAGE_BARRIER(cmd_buf, \
				.image            = img, \
				.subresourceRange = subresource_range, \
				.srcAccessMask    = 0, \
				.dstAccessMask    = 0, \
				.oldLayout        = VK_IMAGE_LAYOUT_GENERAL, \
				.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, \
		); \
	} while(0)

#define BARRIER_FROM_COPY_DEST(cmd_buf, img) \
	do { \
		VkImageSubresourceRange subresource_range = { \
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, \
			.baseMipLevel   = 0, \
			.levelCount     = 1, \
			.baseArrayLayer = 0, \
			.layerCount     = 1 \
		}; \
		IMAGE_BARRIER(cmd_buf, \
				.image            = img, \
				.subresourceRange = subresource_range, \
				.srcAccessMask    = 0, \
				.dstAccessMask    = VK_ACCESS_SHADER_READ_BIT, \
				.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, \
				.newLayout        = VK_IMAGE_LAYOUT_GENERAL, \
		); \
	} while(0)

static void bloom_debug_show_image(VkCommandBuffer cmd_buf, int vis_img)
{
	VkImageSubresourceLayers subresource = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkOffset3D offset_UL = {
		.x = 0,
		.y = 0,
		.z = 0
	};

	VkOffset3D offset_UL_input = {
		.x = IMG_WIDTH / 4,
		.y = IMG_HEIGHT / 4,
		.z = 1
	};

	VkOffset3D offset_LR_input = {
		.x = IMG_WIDTH,
		.y = IMG_HEIGHT,
		.z = 1
	};

	VkImageBlit blit_region = {
		.srcSubresource = subresource,
		.srcOffsets[0] = offset_UL,
		.srcOffsets[1] = offset_UL_input,
		.dstSubresource = subresource,
		.dstOffsets[0] = offset_UL,
		.dstOffsets[1] = offset_LR_input,
	};

	BARRIER_TO_COPY_DEST(cmd_buf, qvk.images[VKPT_IMG_TAA_OUTPUT]);

	vkCmdBlitImage(cmd_buf,
		qvk.images[vis_img], VK_IMAGE_LAYOUT_GENERAL,
		qvk.images[VKPT_IMG_TAA_OUTPUT], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit_region, VK_FILTER_LINEAR);

	BARRIER_FROM_COPY_DEST(cmd_buf, qvk.images[VKPT_IMG_TAA_OUTPUT]);
}

VkResult
vkpt_bloom_record_cmd_buffer(VkCommandBuffer cmd_buf)
{
	VkExtent2D extent = qvk.extent_taa_output;

	compute_push_constants();

	VkDescriptorSet desc_sets[] = {
		qvk.desc_set_ubo,
		qvk_get_current_desc_set_textures(),
		qvk.desc_set_vertex_buffer
	};

	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_TAA_OUTPUT]);

	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[DOWNSCALE]);
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
		pipeline_layout_composite, 0, LENGTH(desc_sets), desc_sets, 0, 0);

	vkCmdDispatch(cmd_buf,
		(extent.width / 4 + 15) / 16,
		(extent.height / 4 + 15) / 16,
		1);

	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_BLOOM_VBLUR]);

	if(cvar_bloom_debug->integer == 1)
	{
		bloom_debug_show_image(cmd_buf, VKPT_IMG_BLOOM_VBLUR);
		return VK_SUCCESS;
	}

	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_BLOOM_HBLUR]);

	// apply horizontal blur from BLOOM_VBLUR -> BLOOM_HBLUR
	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[BLUR]);
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
		pipeline_layout_blur, 0, LENGTH(desc_sets), desc_sets, 0, 0);

	vkCmdPushConstants(cmd_buf, pipeline_layout_blur, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof(push_constants_hblur), &push_constants_hblur);
	vkCmdDispatch(cmd_buf,
		(extent.width / 4 + 15) / 16,
		(extent.height / 4 + 15) / 16,
		1);

	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_BLOOM_HBLUR]);

	if(cvar_bloom_debug->integer == 2)
	{
		bloom_debug_show_image(cmd_buf, VKPT_IMG_BLOOM_HBLUR);
		return VK_SUCCESS;
	}

	// vertical blur from BLOOM_HBLUR -> BLOOM_VBLUR
	vkCmdPushConstants(cmd_buf, pipeline_layout_blur, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof(push_constants_vblur), &push_constants_vblur);
	vkCmdDispatch(cmd_buf,
		(extent.width / 4 + 15) / 16,
		(extent.height / 4 + 15) / 16,
		1);

	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_BLOOM_VBLUR]);

	if(cvar_bloom_debug->integer == 3)
	{
		bloom_debug_show_image(cmd_buf, VKPT_IMG_BLOOM_VBLUR);
		return VK_SUCCESS;
	}

	// composite bloom into TAA_OUTPUT
	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[COMPOSITE]);
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
		pipeline_layout_composite, 0, LENGTH(desc_sets), desc_sets, 0, 0);
	vkCmdDispatch(cmd_buf,
		(extent.width + 15) / 16,
		(extent.height + 15) / 16,
		1);

	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_BLOOM_VBLUR]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_TAA_OUTPUT]);


	return VK_SUCCESS;
}
