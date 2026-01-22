/********************************************************************
*
*
*	VKPT Renderer: Hardware Ray Tracing Path Tracer
*
*	Implements the core hardware-accelerated ray tracing system using
*	Vulkan RT extensions (VK_KHR_acceleration_structure and
*	VK_KHR_ray_tracing_pipeline). Manages acceleration structures,
*	ray tracing pipelines, and executes multiple ray tracing passes
*	to compute primary visibility, reflections/refractions, and
*	direct/indirect lighting with multiple bounces.
*
*	Architecture:
*	- BLAS (Bottom-Level Acceleration Structures): Geometry containers
*	  - Dynamic: World geometry (BSP brushes, dynamic models)
*	  - Transparent: Alpha-blended models (glass, water)
*	  - Masked: Alpha-tested models (fences, foliage)
*	  - Viewer: First-person player model
*	  - Weapon: First-person weapon model
*	  - Explosions: Explosion sprite geometry
*	  - Particles: Particle system quads
*	  - Beams: Laser/lightning beams (AABB-based)
*	  - Sprites: Billboard sprites
*	- TLAS (Top-Level Acceleration Structures): Instance containers
*	  - Geometry TLAS: All opaque/masked world and model instances
*	  - Effects TLAS: Particles, beams, sprites, explosions
*
*	Ray Tracing Pipeline Stages:
*	1. Ray Generation (RGEN): Generates rays from camera or surface points
*	2. Intersection (RINT): Custom intersection for procedural geometry (beams)
*	3. Any Hit (RAHIT): Alpha testing and transparency evaluation
*	4. Closest Hit (RCHIT): Surface shading and material evaluation
*	5. Miss (RMISS): Sky/environment evaluation for rays that hit nothing
*
*	Algorithm Flow (Per Frame):
*	1. Build BLAS: Update bottom-level acceleration structures for dynamic geometry
*	2. Build TLAS: Update top-level acceleration structures with current instances
*	3. Primary Rays: Trace rays from camera to find first surface hit
*	4. Reflections/Refractions: Trace secondary rays for specular surfaces (2 bounces)
*	5. Direct Lighting: Sample light sources for direct illumination
*	6. Indirect Lighting: Trace diffuse bounce rays for global illumination (1-2 bounces)
*
*	Features:
*	- Multi-pass ray tracing with different shader configurations
*	- Separate handling of opaque, transparent, and masked geometry
*	- Procedural intersection for cylindrical beam effects
*	- Alpha-tested sprites and particles with custom any-hit shaders
*	- Caustics support via specialized lighting pipeline
*	- Multi-GPU rendering with instance-based load distribution
*
*	Configuration:
*	- Pipeline Variants: 7 specialized RT pipelines (primary, reflect_1/2, direct, indirect_1/2, caustics)
*	- Shader Binding Table: Per-pipeline shader groups for geometry types
*	- Descriptor Sets: TLAS bindings, particle/beam/sprite buffers
*	- Scratch Buffer: Shared temporary storage for AS builds (32 MB)
*
*	Performance Optimizations:
*	- Acceleration structure caching with match info to avoid rebuilds
*	- Dynamic geometry bloat factor (2x) to reduce allocations
*	- Scratch buffer reuse across multiple AS builds per frame
*	- Instance batching for efficient TLAS updates
*	- Fast build flags for dynamic geometry (prefer build speed)
*	- Fast trace flags for static geometry (prefer trace speed)
*	- Multi-GPU work distribution via push constants
*
*	Shader Binding Table Layout:
*	- SBT_RGEN: Ray generation shader entry point
*	- SBT_RMISS_EMPTY: Miss shader for sky/environment
*	- SBT_RCHIT_GEOMETRY: Closest hit for opaque geometry
*	- SBT_RAHIT_MASKED: Any hit for alpha-tested surfaces
*	- SBT_RCHIT_EFFECTS: Closest hit for effects (unused placeholder)
*	- SBT_RAHIT_PARTICLE: Any hit for particle quads
*	- SBT_RAHIT_EXPLOSION: Any hit for explosion sprites
*	- SBT_RAHIT_SPRITE: Any hit for billboard sprites
*	- SBT_RINT_BEAM: Intersection + any hit for cylindrical beams
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
#include "vkpt.h"
#include "vk_util.h"
#include "shader/vertex_buffer.h"
#include "../../client/cl_client.h"

#include <assert.h>



/**
*
*
*
*	Constants and Descriptor Binding Indices:
*
*
*
**/
//! Binding index for acceleration structure (TLAS) in ray tracing descriptor set.
#define RAY_GEN_ACCEL_STRUCTURE_BINDING_IDX 0
//! Binding index for particle color buffer (texel buffer for particle colors).
#define RAY_GEN_PARTICLE_COLOR_BUFFER_BINDING_IDX 1
//! Binding index for beam color buffer (texel buffer for beam colors).
#define RAY_GEN_BEAM_COLOR_BUFFER_BINDING_IDX 2
//! Binding index for sprite info buffer (texel buffer for sprite parameters).
#define RAY_GEN_SPRITE_INFO_BUFFER_BINDING_IDX 3
//! Binding index for beam intersection buffer (texel buffer for beam cylinder parameters).
#define RAY_GEN_BEAM_INTERSECT_BUFFER_BINDING_IDX 4

//! Size of shared scratch buffer for acceleration structure builds (32 MB).
//! This buffer is reused for all BLAS and TLAS builds within a single frame.
#define SIZE_SCRATCH_BUFFER (1 << 25)

/**
*
*
*
*	Module State and Data Structures:
*
*
*
**/
//! Shader group handle size from ray tracing pipeline properties (typically 32 bytes).
static uint32_t shaderGroupHandleSize = 0;
//! Shader group base alignment for shader binding table (typically 64 bytes on NVIDIA).
static uint32_t shaderGroupBaseAlignment = 0;
//! Minimum alignment for scratch buffer offsets during AS builds.
static uint32_t minAccelerationStructureScratchOffsetAlignment = 0;

/**
*	@brief	Acceleration structure match info for caching and rebuild detection.
*	@note	Used to determine if an existing AS can be reused or needs rebuilding.
*/
typedef struct {
	int fast_build;           //! Whether AS was built with PREFER_FAST_BUILD flag.
	uint32_t vertex_count;    //! Allocated vertex capacity (may exceed actual count for dynamic geometry).
	uint32_t index_count;     //! Allocated index capacity (may exceed actual count for dynamic geometry).
	uint32_t aabb_count;      //! Allocated AABB capacity for procedural geometry.
	uint32_t instance_count;  //! Allocated instance capacity for TLAS.
} accel_match_info_t;

/**
*	@brief	Acceleration structure container with metadata.
*	@note	Holds both BLAS and TLAS with their associated memory and match info.
*/
typedef struct {
	VkAccelerationStructureKHR accel;  //! Vulkan acceleration structure handle.
	accel_match_info_t match;          //! Match info for rebuild detection.
	BufferResource_t mem;              //! Buffer backing the AS storage.
	bool present;                      //! Whether the AS contains valid geometry this frame.
} accel_struct_t;

/**
*	@brief	Ray tracing pipeline indices.
*	@note	Each pipeline uses different shaders and configurations for specific ray tracing passes.
*/
typedef enum {
	PIPELINE_PRIMARY_RAYS,              //! Primary visibility rays from camera.
	PIPELINE_REFLECT_REFRACT_1,         //! First reflection/refraction bounce.
	PIPELINE_REFLECT_REFRACT_2,         //! Second reflection/refraction bounce.
	PIPELINE_DIRECT_LIGHTING,           //! Direct lighting without caustics.
	PIPELINE_DIRECT_LIGHTING_CAUSTICS,  //! Direct lighting with caustics enabled.
	PIPELINE_INDIRECT_LIGHTING_FIRST,   //! First indirect lighting bounce (includes particles).
	PIPELINE_INDIRECT_LIGHTING_SECOND,  //! Second indirect lighting bounce.

	PIPELINE_COUNT
} pipeline_index_t;

//! Shared scratch buffer for all acceleration structure builds.
BufferResource_t           buf_accel_scratch;
//! Current write pointer into scratch buffer (reset each frame).
static size_t                     scratch_buf_ptr = 0;

//! Instance buffers for TLAS builds (one per frame in flight).
static BufferResource_t           buf_instances[MAX_FRAMES_IN_FLIGHT];

//! Bottom-level acceleration structures for dynamic world geometry (BSP, dynamic models).
static accel_struct_t             blas_dynamic[MAX_FRAMES_IN_FLIGHT];
//! BLAS for transparent models (glass, water, alpha-blended surfaces).
static accel_struct_t             blas_transparent_models[MAX_FRAMES_IN_FLIGHT];
//! BLAS for masked models (alpha-tested surfaces like fences, foliage).
static accel_struct_t             blas_masked_models[MAX_FRAMES_IN_FLIGHT];
//! BLAS for first-person viewer body model.
static accel_struct_t             blas_viewer_models[MAX_FRAMES_IN_FLIGHT];
//! BLAS for first-person weapon model.
static accel_struct_t             blas_viewer_weapon[MAX_FRAMES_IN_FLIGHT];
//! BLAS for explosion sprites.
static accel_struct_t             blas_explosions[MAX_FRAMES_IN_FLIGHT];
//! BLAS for particle system quads.
static accel_struct_t             blas_particles[MAX_FRAMES_IN_FLIGHT];
//! BLAS for cylindrical beam effects (uses AABBs with custom intersection).
static accel_struct_t             blas_beams[MAX_FRAMES_IN_FLIGHT];
//! BLAS for billboard sprites.
static accel_struct_t             blas_sprites[MAX_FRAMES_IN_FLIGHT];

//! Top-level acceleration structure for all geometry (world, models).
static accel_struct_t             tlas_geometry[MAX_FRAMES_IN_FLIGHT];
//! Top-level acceleration structure for effects (particles, beams, sprites, explosions).
static accel_struct_t             tlas_effects[MAX_FRAMES_IN_FLIGHT];

//! Shader binding table buffer containing shader group handles.
static BufferResource_t      buf_shader_binding_table;

//! Descriptor pool for ray tracing descriptor sets.
static VkDescriptorPool      rt_descriptor_pool;
//! Descriptor sets for ray tracing (TLAS bindings, particle/beam/sprite buffers).
static VkDescriptorSet       rt_descriptor_set[MAX_FRAMES_IN_FLIGHT];
//! Descriptor set layout for ray tracing.
static VkDescriptorSetLayout rt_descriptor_set_layout;
//! Pipeline layout for ray tracing (includes UBO, textures, vertex buffer descriptors).
static VkPipelineLayout      rt_pipeline_layout;
//! Ray tracing pipelines (one per pass type).
static VkPipeline            rt_pipelines[PIPELINE_COUNT];

//! Console variable to enable/disable particle rendering.
cvar_t*                      cvar_pt_enable_particles = NULL;
//! Console variable to enable/disable beam rendering.
cvar_t*                      cvar_pt_enable_beams = NULL;
//! Console variable to enable/disable sprite rendering.
cvar_t*                      cvar_pt_enable_sprites = NULL;

//! Console variable for caustics enable (defined in another module).
extern cvar_t *cvar_pt_caustics;
//! Console variable for reflection/refraction enable (defined in another module).
extern cvar_t *cvar_pt_reflect_refract;



/**
*
*
*
*	Instance Management (Internal):
*
*
*
**/
/**
*	@brief	Geometry instance structure for TLAS builds.
*	@note	This structure matches VkAccelerationStructureInstanceKHR layout but uses
*			a custom name for clarity. Contains transformation matrix, instance mask,
*			shader binding table offset, and BLAS device address.
*/
typedef struct QvkGeometryInstance_s {
	float    transform[12];                 //! Row-major 3x4 transformation matrix (no scale).
	uint32_t instance_id     : 24;          //! Custom instance index (used for vertex buffer selection).
	uint32_t mask            :  8;          //! Instance mask for ray visibility filtering.
	uint32_t instance_offset : 24;          //! Shader binding table offset (for hit group selection).
	uint32_t flags           :  8;          //! Instance flags (opaque, cull mode, etc).
	VkDeviceAddress acceleration_structure; //! Device address of the BLAS referenced by this instance.
} QvkGeometryInstance_t;

//! Current number of instances in the global instance buffer.
static uint32_t g_num_instances;
//! Global instance buffer for building the main geometry TLAS.
static QvkGeometryInstance_t g_instances[MAX_TLAS_INSTANCES];

/**
*	@brief	Push constants for ray tracing shaders.
*	@note	Passed to ray generation shaders for multi-GPU support and bounce indexing.
*/
typedef struct {
	int gpu_index;  //! GPU index for multi-GPU rendering (-1 for single GPU).
	int bounce;     //! Current bounce index (0 for primary rays, 1+ for indirect).
} pt_push_constants_t;


/**
*	@brief	Memory barrier macro for acceleration structure builds.
*	@note	Ensures AS write operations complete before subsequent ray tracing reads.
*			Required between AS builds and ray tracing dispatch commands.
*/
#define MEM_BARRIER_BUILD_ACCEL(cmd_buf, ...) \
	do { \
		VkMemoryBarrier mem_barrier = {};  \
		mem_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;  \
		mem_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR \
									| VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR; \
		mem_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR; \
		__VA_ARGS__  \
	 \
		vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, \
				VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, \
				&mem_barrier, 0, 0, 0, 0); \
	} while(0)



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
*	@brief	Initializes the path tracer module.
*	@return	VK_SUCCESS on success, Vulkan error code on failure.
*	@note	Creates descriptor sets, pipeline layout, scratch buffers, and instance buffers.
*			Must be called during renderer initialization before creating pipelines.
*/
VkResult
vkpt_pt_init()
{
	// Query ray tracing properties from the physical device
	VkPhysicalDeviceAccelerationStructurePropertiesKHR accel_struct_properties = {};
	accel_struct_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
	accel_struct_properties.pNext = NULL;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_pipeline_properties = {};
	ray_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	ray_pipeline_properties.pNext = &accel_struct_properties;

	VkPhysicalDeviceProperties2 dev_props2 = {};
	dev_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	dev_props2.pNext = &ray_pipeline_properties;

	vkGetPhysicalDeviceProperties2(qvk.physical_device, &dev_props2);

	// Store RT properties for shader binding table and AS build alignment
	shaderGroupBaseAlignment = ray_pipeline_properties.shaderGroupBaseAlignment;
	shaderGroupHandleSize = ray_pipeline_properties.shaderGroupHandleSize;
	minAccelerationStructureScratchOffsetAlignment = accel_struct_properties.minAccelerationStructureScratchOffsetAlignment;

	// Create shared scratch buffer for acceleration structure builds (32 MB)
	// This buffer is reused for all BLAS and TLAS builds within a single frame
	buffer_create(&buf_accel_scratch, SIZE_SCRATCH_BUFFER, 
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Create instance buffers for TLAS builds (one per frame in flight)
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		buffer_create(buf_instances + i, MAX_TLAS_INSTANCES * sizeof(QvkGeometryInstance_t), 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	/* Create descriptor set layout for ray tracing */
	VkDescriptorSetLayoutBinding bindings[] = {
		{
			// Binding 0: Acceleration structures (TLAS array)
			RAY_GEN_ACCEL_STRUCTURE_BINDING_IDX,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			TLAS_COUNT,
			qvk.use_ray_query ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			nullptr
		},
		{
			// Binding 1: Particle color buffer (for particle any-hit shader)
			RAY_GEN_PARTICLE_COLOR_BUFFER_BINDING_IDX,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			1,
			qvk.use_ray_query ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			nullptr
		},
		{
			// Binding 2: Beam color buffer (for beam any-hit shader)
			RAY_GEN_BEAM_COLOR_BUFFER_BINDING_IDX,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			1,
			qvk.use_ray_query ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			nullptr
		},
		{
			// Binding 3: Sprite info buffer (for sprite any-hit shader)
			RAY_GEN_SPRITE_INFO_BUFFER_BINDING_IDX,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			1,
			qvk.use_ray_query ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			nullptr
		},
		{
			// Binding 4: Beam intersection buffer (for beam intersection shader)
			RAY_GEN_BEAM_INTERSECT_BUFFER_BINDING_IDX,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			1,
			qvk.use_ray_query ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
			nullptr
		},
	};

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = LENGTH(bindings);
	layout_info.pBindings = bindings;
	_VK(vkCreateDescriptorSetLayout(qvk.device, &layout_info, NULL, &rt_descriptor_set_layout));
	ATTACH_LABEL_VARIABLE(rt_descriptor_set_layout, DESCRIPTOR_SET_LAYOUT);


	VkDescriptorSetLayout desc_set_layouts[] = {
		rt_descriptor_set_layout,       // Set 0: RT-specific (TLAS, particle/beam/sprite buffers)
		qvk.desc_set_layout_ubo,        // Set 1: Uniform buffer objects
		qvk.desc_set_layout_textures,   // Set 2: Material textures
		qvk.desc_set_layout_vertex_buffer // Set 3: Vertex/index buffers
	};

	/* Create pipeline layout */
	VkPushConstantRange push_constant_range = {};
	push_constant_range.stageFlags = qvk.use_ray_query ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(pt_push_constants_t);

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = LENGTH(desc_set_layouts);
	pipeline_layout_create_info.pSetLayouts = desc_set_layouts;
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;

	_VK(vkCreatePipelineLayout(qvk.device, &pipeline_layout_create_info, NULL, &rt_pipeline_layout));
	ATTACH_LABEL_VARIABLE(rt_pipeline_layout, PIPELINE_LAYOUT);

	/* Create descriptor pool */
	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * LENGTH(bindings) },
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT }
	};

	VkDescriptorPoolCreateInfo pool_create_info = {};
	pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_create_info.maxSets = MAX_FRAMES_IN_FLIGHT;
	pool_create_info.poolSizeCount = LENGTH(pool_sizes);
	pool_create_info.pPoolSizes = pool_sizes;

	_VK(vkCreateDescriptorPool(qvk.device, &pool_create_info, NULL, &rt_descriptor_pool));
	ATTACH_LABEL_VARIABLE(rt_descriptor_pool, DESCRIPTOR_POOL);

	/* Allocate descriptor sets (one per frame in flight) */
	VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {};
	descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptor_set_alloc_info.descriptorPool = rt_descriptor_pool;
	descriptor_set_alloc_info.descriptorSetCount = 1;
	descriptor_set_alloc_info.pSetLayouts = &rt_descriptor_set_layout;

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_VK(vkAllocateDescriptorSets(qvk.device, &descriptor_set_alloc_info, rt_descriptor_set + i));
		ATTACH_LABEL_VARIABLE(rt_descriptor_set[i], DESCRIPTOR_SET);
	}

	// Register console variables for runtime enable/disable of effect rendering
	cvar_pt_enable_particles = Cvar_Get("pt_enable_particles", "1", 0);
	cvar_pt_enable_beams = Cvar_Get("pt_enable_beams", "1", 0);
	cvar_pt_enable_sprites= Cvar_Get("pt_enable_sprites", "1", 0);

	return VK_SUCCESS;
}

/**
*	@brief	Updates descriptor set bindings for ray tracing.
*	@param	idx	Frame index (0 to MAX_FRAMES_IN_FLIGHT-1).
*	@return	VK_SUCCESS on success.
*	@note	Called per frame to update TLAS bindings and particle/beam/sprite buffer views.
*			Must be called after TLAS builds complete.
*/
VkResult
vkpt_pt_update_descripter_set_bindings(int idx)
{
	/* Update descriptor set bindings for TLAS and effect buffers */
	const VkAccelerationStructureKHR tlas[TLAS_COUNT] = {
		[TLAS_INDEX_GEOMETRY] = tlas_geometry[idx].accel,  // Geometry TLAS (world, models)
		[TLAS_INDEX_EFFECTS] = tlas_effects[idx].accel     // Effects TLAS (particles, beams, sprites)
	};
	
	VkWriteDescriptorSetAccelerationStructureKHR desc_accel_struct = {};
	desc_accel_struct.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	desc_accel_struct.accelerationStructureCount = TLAS_COUNT;
	desc_accel_struct.pAccelerationStructures = tlas;
	
	// Get buffer views for particles, beams, and sprites from transparency module
	VkBufferView particle_color_buffer_view = get_transparency_particle_color_buffer_view();
	VkBufferView beam_color_buffer_view = get_transparency_beam_color_buffer_view();
	VkBufferView sprite_info_buffer_view = get_transparency_sprite_info_buffer_view();
	VkBufferView beam_intersect_buffer_view = get_transparency_beam_intersect_buffer_view();

	VkWriteDescriptorSet writes[] = {
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			(const void*)&desc_accel_struct,
			rt_descriptor_set[idx],
			RAY_GEN_ACCEL_STRUCTURE_BINDING_IDX,
			0,
			TLAS_COUNT,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			nullptr,
			nullptr,
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			nullptr,
			rt_descriptor_set[idx],
			RAY_GEN_PARTICLE_COLOR_BUFFER_BINDING_IDX,
			0,
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			nullptr,
			nullptr,
			&particle_color_buffer_view
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			nullptr,
			rt_descriptor_set[idx],
			RAY_GEN_BEAM_COLOR_BUFFER_BINDING_IDX,
			0,
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			nullptr,
			nullptr,
			&beam_color_buffer_view
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			nullptr,
			rt_descriptor_set[idx],
			RAY_GEN_SPRITE_INFO_BUFFER_BINDING_IDX,
			0,
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			nullptr,
			nullptr,
			&sprite_info_buffer_view
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			nullptr,
			rt_descriptor_set[idx],
			RAY_GEN_BEAM_INTERSECT_BUFFER_BINDING_IDX,
			0,
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			nullptr,
			nullptr,
			&beam_intersect_buffer_view
		},
	};

	vkUpdateDescriptorSets(qvk.device, LENGTH(writes), writes, 0, NULL);

	return VK_SUCCESS;
}



/**
*
*
*
*	Acceleration Structure Management (Internal):
*
*
*
**/
/**
*	@brief	Destroys an acceleration structure and frees its memory.
*	@param	blas	Acceleration structure to destroy.
*	@note	Resets all match info fields to zero after destruction.
*/
static void destroy_accel_struct(accel_struct_t* blas)
{
	buffer_destroy(&blas->mem);

	if (blas->accel)
	{
		qvkDestroyAccelerationStructureKHR(qvk.device, blas->accel, NULL);
		blas->accel = VK_NULL_HANDLE;
	}
	
	blas->match.fast_build = 0;
	blas->match.index_count = 0;
	blas->match.vertex_count = 0;
	blas->match.aabb_count = 0;
	blas->match.instance_count = 0;
}

/**
*	@brief	Destroys all dynamic BLAS for a given frame index.
*	@param	idx	Frame index (0 to MAX_FRAMES_IN_FLIGHT-1).
*	@note	Called during cleanup or when recreating acceleration structures.
*/
static void vkpt_pt_destroy_dynamic(int idx)
{
	destroy_accel_struct(&blas_dynamic[idx]);
	destroy_accel_struct(&blas_transparent_models[idx]);
	destroy_accel_struct(&blas_masked_models[idx]);
	destroy_accel_struct(&blas_viewer_models[idx]);
	destroy_accel_struct(&blas_viewer_weapon[idx]);
	destroy_accel_struct(&blas_explosions[idx]);
	destroy_accel_struct(&blas_particles[idx]);
	destroy_accel_struct(&blas_beams[idx]);
	destroy_accel_struct(&blas_sprites[idx]);
}

/**
*	@brief	Checks if an existing triangle BLAS matches required parameters.
*	@param	match			Match info from existing BLAS.
*	@param	fast_build		Whether fast build is required.
*	@param	vertex_count	Required vertex capacity.
*	@param	index_count		Required index capacity.
*	@return	Non-zero if BLAS can be reused, 0 if rebuild needed.
*	@note	For dynamic geometry, allocated capacity may exceed current count to avoid frequent reallocations.
*/
static inline int accel_matches(accel_match_info_t *match,
								int fast_build,
								uint32_t vertex_count,
								uint32_t index_count) {
	return match->fast_build == fast_build &&
		   match->vertex_count >= vertex_count &&
		   match->index_count >= index_count;
}

/**
*	@brief	Checks if an existing AABB BLAS matches required parameters.
*	@param	match			Match info from existing BLAS.
*	@param	fast_build		Whether fast build is required.
*	@param	aabb_count		Required AABB capacity.
*	@return	Non-zero if BLAS can be reused, 0 if rebuild needed.
*	@note	Used for procedural geometry like cylindrical beams.
*/
static inline int accel_matches_aabb(accel_match_info_t *match,
								int fast_build,
								uint32_t aabb_count) {
	return match->fast_build == fast_build &&
		   match->aabb_count >= aabb_count;
}

/**
*	@brief	Checks if an existing TLAS matches required parameters.
*	@param	match			Match info from existing TLAS.
*	@param	fast_build		Whether fast build is required.
*	@param	instance_count	Required instance capacity.
*	@return	Non-zero if TLAS can be reused, 0 if rebuild needed.
*/
static inline int accel_matches_top_level(accel_match_info_t *match,
								int fast_build,
								uint32_t instance_count) {
	return match->fast_build == fast_build &&
		   match->instance_count >= instance_count;
}

//! How much to over-allocate dynamic geometry to avoid frequent reallocations (2x capacity).
#define DYNAMIC_GEOMETRY_BLOAT_FACTOR 2



/**
*
*
*
*	BLAS Building and Management:
*
*
*
**/
/**
*	@brief	Creates or updates a triangle BLAS from vertex and index buffers.
*	@param	cmd_buf			Command buffer for AS build commands.
*	@param	buffer_vertex	Vertex buffer containing positions (vec3).
*	@param	offset_vertex	Byte offset into vertex buffer.
*	@param	buffer_index	Index buffer (uint16), or NULL for non-indexed triangles.
*	@param	offset_index	Byte offset into index buffer.
*	@param	num_vertices	Number of vertices (for non-indexed) or max vertex index + 1.
*	@param	num_indices		Number of indices (for indexed geometry), or 0.
*	@param	blas			BLAS container to create/update.
*	@param	is_dynamic		Whether geometry changes frequently (enables bloat factor).
*	@param	fast_build		Whether to prefer fast build over fast trace.
*	@note	For dynamic geometry, allocates 2x capacity to reduce reallocation frequency.
*			Uses shared scratch buffer for temporary build data.
*/
static void
vkpt_pt_create_accel_bottom(
	VkCommandBuffer cmd_buf,
	BufferResource_t* buffer_vertex,
	VkDeviceAddress offset_vertex,
	BufferResource_t* buffer_index,
	VkDeviceAddress offset_index,
	uint32_t num_vertices,
	uint32_t num_indices,
	accel_struct_t* blas,
	bool is_dynamic,
	bool fast_build)
{
	assert(blas);

	if (num_vertices == 0)
	{
		blas->present = false;
		return;
	}
	
	assert(buffer_vertex->address);
	if (buffer_index) assert(buffer_index->address);

	// Setup triangle geometry for BLAS
	VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = buffer_vertex->address + offset_vertex;
	triangles.vertexStride = sizeof(float) * 3;
	triangles.maxVertex = max(num_vertices, 1) - 1;
	triangles.indexData.deviceAddress = buffer_index ? (buffer_index->address + offset_index) : 0;
	triangles.indexType = buffer_index ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_NONE_KHR;

	VkAccelerationStructureGeometryDataKHR geometry_data = {}; 
	geometry_data.triangles = triangles;

	const VkAccelerationStructureGeometryKHR geometry = {
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		nullptr,
		VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		geometry_data,
		0
	};

	const VkAccelerationStructureGeometryKHR* geometries = &geometry;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo;

	// Prepare build info now, acceleration structure handle is filled later
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.pNext = NULL;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	// Fast build: prioritize build speed for dynamic geometry
	// Fast trace: prioritize ray tracing performance for static geometry
	buildInfo.flags = fast_build ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = geometries;
	buildInfo.ppGeometries = NULL;

	int doFree = 0;
	int doAlloc = 0;

	// Check if we can reuse existing BLAS or need to reallocate
	if (!is_dynamic || !accel_matches(&blas->match, fast_build, num_vertices, num_indices) || blas->accel == VK_NULL_HANDLE)
	{
		doAlloc = 1;
		doFree = (blas->accel != VK_NULL_HANDLE);
	}

	if (doFree)
	{
		destroy_accel_struct(blas);
	}

	// Query required size for acceleration structure
	uint32_t max_primitive_count = max(num_vertices, num_indices) / 3; // number of triangles
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR(qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &max_primitive_count, &sizeInfo);

	if (doAlloc)
	{
		uint32_t num_vertices_to_allocate = num_vertices;
		uint32_t num_indices_to_allocate = num_indices;

		// Allocate more memory / larger BLAS for dynamic objects to reduce reallocation frequency
		if (is_dynamic)
		{
			num_vertices_to_allocate *= DYNAMIC_GEOMETRY_BLOAT_FACTOR;
			num_indices_to_allocate *= DYNAMIC_GEOMETRY_BLOAT_FACTOR;

			// Recompute size info with bloated capacity
			max_primitive_count = max(num_vertices_to_allocate, num_indices_to_allocate) / 3;
			qvkGetAccelerationStructureBuildSizesKHR(qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &max_primitive_count, &sizeInfo);
		}

		// Create acceleration structure object
		VkAccelerationStructureCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = sizeInfo.accelerationStructureSize;

		// Create the buffer backing the acceleration structure
		buffer_create(&blas->mem, sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		createInfo.buffer = blas->mem.buffer;

		// Create the acceleration structure
		qvkCreateAccelerationStructureKHR(qvk.device, &createInfo, NULL, &blas->accel);

		// Store match info for future rebuild detection
		blas->match.fast_build = fast_build;
		blas->match.vertex_count = num_vertices_to_allocate;
		blas->match.index_count = num_indices_to_allocate;
		blas->match.aabb_count = 0;
		blas->match.instance_count = 0;
	}

	// Set destination acceleration structure
	buildInfo.dstAccelerationStructure = blas->accel;

	// Allocate scratch buffer space for AS build (reused across multiple builds per frame)
	buildInfo.scratchData.deviceAddress = buf_accel_scratch.address + scratch_buf_ptr;
	assert(buf_accel_scratch.address);

	// Update the scratch buffer pointer and ensure alignment
	scratch_buf_ptr += sizeInfo.buildScratchSize;
	scratch_buf_ptr = align(scratch_buf_ptr, minAccelerationStructureScratchOffsetAlignment);
	assert(scratch_buf_ptr < SIZE_SCRATCH_BUFFER);

	// Build range info specifies the number of primitives to build
	VkAccelerationStructureBuildRangeInfoKHR offset = {};
	offset.primitiveCount = max(num_vertices, num_indices) / 3;
	const VkAccelerationStructureBuildRangeInfoKHR* offsets = &offset;

	// Execute AS build on GPU
	qvkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &buildInfo, &offsets);

	blas->present = true;
}

/**
*	@brief	Creates or updates an AABB BLAS for procedural geometry.
*	@param	cmd_buf			Command buffer for AS build commands.
*	@param	buffer_aabb		Buffer containing AABBs (VkAabbPositionsKHR).
*	@param	offset_aabb		Byte offset into AABB buffer.
*	@param	num_aabbs		Number of AABBs.
*	@param	blas			BLAS container to create/update.
*	@param	is_dynamic		Whether geometry changes frequently.
*	@param	fast_build		Whether to prefer fast build over fast trace.
*	@note	Used for cylindrical beam effects with custom intersection shaders.
*/

/**
*	@brief	Creates or updates an AABB BLAS for procedural geometry.
*	@param	cmd_buf			Command buffer for AS build commands.
*	@param	buffer_aabb		Buffer containing AABBs (VkAabbPositionsKHR).
*	@param	offset_aabb		Byte offset into AABB buffer.
*	@param	num_aabbs		Number of AABBs.
*	@param	blas			BLAS container to create/update.
*	@param	is_dynamic		Whether geometry changes frequently.
*	@param	fast_build		Whether to prefer fast build over fast trace.
*	@note	Used for cylindrical beam effects with custom intersection shaders.
*/
static void
vkpt_pt_create_accel_bottom_aabb(
	VkCommandBuffer cmd_buf,
	BufferResource_t* buffer_aabb,
	VkDeviceAddress offset_aabb,
	int num_aabbs,
	accel_struct_t* blas,
	bool is_dynamic,
	bool fast_build)
{
	assert(blas);

	if (num_aabbs == 0)
	{
		blas->present = false;
		return;
	}

	assert(buffer_aabb->address);

	const VkAccelerationStructureGeometryAabbsDataKHR aabbs = {
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
		nullptr,
		{buffer_aabb->address + offset_aabb},
		sizeof(VkAabbPositionsKHR)
	};

	const VkAccelerationStructureGeometryDataKHR geometry_data = { 
		aabbs
	};

	const VkAccelerationStructureGeometryKHR geometry = {
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		nullptr,
		VK_GEOMETRY_TYPE_AABBS_KHR,
		geometry_data,
		0
	};

	const VkAccelerationStructureGeometryKHR* geometries = &geometry;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo;

	// Prepare build info now, acceleration is filled later
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.pNext = NULL;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = fast_build ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = geometries;
	buildInfo.ppGeometries = NULL;

	int doFree = 0;
	int doAlloc = 0;

	if (!is_dynamic || !accel_matches_aabb(&blas->match, fast_build, num_aabbs) || blas->accel == VK_NULL_HANDLE)
	{
		doAlloc = 1;
		doFree = (blas->accel != VK_NULL_HANDLE);
	}

	if (doFree)
	{
		destroy_accel_struct(blas);
	}

	// Find size to build on the device
	uint32_t max_primitive_count = num_aabbs;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR(qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &max_primitive_count, &sizeInfo);

	if (doAlloc)
	{
		int num_aabs_to_allocate = num_aabbs;

		// Allocate more memory / larger BLAS for dynamic objects
		if (is_dynamic)
		{
			num_aabs_to_allocate *= DYNAMIC_GEOMETRY_BLOAT_FACTOR;

			max_primitive_count = num_aabs_to_allocate;
			qvkGetAccelerationStructureBuildSizesKHR(qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &max_primitive_count, &sizeInfo);
		}

		// Create acceleration structure
		VkAccelerationStructureCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = sizeInfo.accelerationStructureSize;

		// Create the buffer for the acceleration structure
		buffer_create(&blas->mem, sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		createInfo.buffer = blas->mem.buffer;

		// Create the acceleration structure
		qvkCreateAccelerationStructureKHR(qvk.device, &createInfo, NULL, &blas->accel);

		blas->match.fast_build = fast_build;
		blas->match.vertex_count = 0;
		blas->match.index_count = 0;
		blas->match.aabb_count = num_aabs_to_allocate;
		blas->match.instance_count = 0;
	}

	// set where the build lands
	buildInfo.dstAccelerationStructure = blas->accel;

	// Use shared scratch buffer for holding the temporary data of the acceleration structure builder
	buildInfo.scratchData.deviceAddress = buf_accel_scratch.address + scratch_buf_ptr;
	assert(buf_accel_scratch.address);

	// Update the scratch buffer ptr
	scratch_buf_ptr += sizeInfo.buildScratchSize;
	scratch_buf_ptr = align(scratch_buf_ptr, minAccelerationStructureScratchOffsetAlignment);
	assert(scratch_buf_ptr < SIZE_SCRATCH_BUFFER);

	// build offset
	VkAccelerationStructureBuildRangeInfoKHR offset = {};
	offset.primitiveCount = num_aabbs;
	const VkAccelerationStructureBuildRangeInfoKHR* offsets = &offset;

	qvkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &buildInfo, &offsets);

	blas->present = true;
}



/**
*
*
*
*	BLAS Building and Management:
*
*
*
**/
/**
*	@brief	Creates or updates all dynamic BLAS for a frame.
*	@param	cmd_buf			Command buffer for AS build commands.
*	@param	idx				Frame index (0 to MAX_FRAMES_IN_FLIGHT-1).
*	@param	upload_info		Entity upload info with primitive counts and offsets.
*	@return	VK_SUCCESS on success.
*	@note	Builds separate BLAS for opaque, transparent, masked, viewer models/weapons,
*			explosions, particles, beams, and sprites. Uses shared scratch buffer for
*			all builds within this function. Resets scratch pointer at start and end.
*			Each BLAS uses fast build flags for dynamic geometry.
*/
VkResult
vkpt_pt_create_all_dynamic(
	VkCommandBuffer cmd_buf,
	int idx, 
	const EntityUploadInfo* upload_info)
{
	scratch_buf_ptr = 0;

	uint64_t offset_vertex_base = 0;
	uint64_t offset_vertex = offset_vertex_base;
	uint64_t offset_index = 0;
	vkpt_pt_create_accel_bottom(cmd_buf, &qvk.buf_positions_instanced, offset_vertex, NULL, offset_index,
		upload_info->opaque_prim_count * 3, 0, blas_dynamic + idx, true, true);

	offset_vertex = offset_vertex_base + upload_info->transparent_prim_offset * sizeof(prim_positions_t);
	vkpt_pt_create_accel_bottom(cmd_buf, &qvk.buf_positions_instanced, offset_vertex, NULL, offset_index,
		upload_info->transparent_prim_count * 3, 0, blas_transparent_models + idx, true, true);

	offset_vertex = offset_vertex_base + upload_info->masked_prim_offset * sizeof(prim_positions_t);
	vkpt_pt_create_accel_bottom(cmd_buf, &qvk.buf_positions_instanced, offset_vertex, NULL, offset_index,
		upload_info->masked_prim_count * 3, 0, blas_masked_models + idx, true, true);

	offset_vertex = offset_vertex_base + upload_info->viewer_model_prim_offset * sizeof(prim_positions_t);
	vkpt_pt_create_accel_bottom(cmd_buf, &qvk.buf_positions_instanced, offset_vertex, NULL, offset_index,
		upload_info->viewer_model_prim_count * 3, 0, blas_viewer_models + idx, true, true);

	offset_vertex = offset_vertex_base + upload_info->viewer_weapon_prim_offset * sizeof(prim_positions_t);
	vkpt_pt_create_accel_bottom(cmd_buf, &qvk.buf_positions_instanced, offset_vertex, NULL, offset_index,
		upload_info->viewer_weapon_prim_count * 3, 0, blas_viewer_weapon + idx, true, true);

	offset_vertex = offset_vertex_base + upload_info->explosions_prim_offset * sizeof(prim_positions_t);
	vkpt_pt_create_accel_bottom(cmd_buf, &qvk.buf_positions_instanced, offset_vertex, NULL, offset_index,
		upload_info->explosions_prim_count * 3, 0, blas_explosions + idx, true, true);

	BufferResource_t* buffer_vertex = NULL;
	BufferResource_t* buffer_index = NULL;
	uint32_t num_vertices = 0;
	uint32_t num_indices = 0;
	vkpt_get_transparency_buffers(VKPT_TRANSPARENCY_PARTICLES, &buffer_vertex, &offset_vertex, &buffer_index, &offset_index, &num_vertices, &num_indices);
	vkpt_pt_create_accel_bottom(cmd_buf, buffer_vertex, offset_vertex, buffer_index, offset_index, num_vertices, num_indices, blas_particles + idx, true, true);

	BufferResource_t *buffer_aabb = NULL;
	uint64_t offset_aabb = 0;
	uint32_t num_aabbs = 0;
	vkpt_get_beam_aabb_buffer(&buffer_aabb, &offset_aabb, &num_aabbs);
	vkpt_pt_create_accel_bottom_aabb(cmd_buf, buffer_aabb, offset_aabb, num_aabbs, blas_beams + idx, true, true);
	
	vkpt_get_transparency_buffers(VKPT_TRANSPARENCY_SPRITES, &buffer_vertex, &offset_vertex, &buffer_index, &offset_index, &num_vertices, &num_indices);
	vkpt_pt_create_accel_bottom(cmd_buf, buffer_vertex, offset_vertex, buffer_index, offset_index, num_vertices, num_indices, blas_sprites + idx, true, true);

	MEM_BARRIER_BUILD_ACCEL(cmd_buf);
	scratch_buf_ptr = 0;

	return VK_SUCCESS;
}



/**
*
*
*
*	TLAS (Top-Level Acceleration Structure) Management:
*
*
*
**/
/**
*	@brief	Appends a BLAS instance to the instance buffer for TLAS construction.
*	@param	instances		Instance array to append to.
*	@param	num_instances	Current number of instances (incremented by this function).
*	@param	blas			BLAS to instance.
*	@param	vbo_index		Vertex buffer index for hit shader.
*	@param	prim_offset		Primitive offset in vertex buffer.
*	@param	mask			Instance mask for ray visibility filtering.
*	@param	flags			Instance flags (opaque, cull mode, etc).
*	@param	sbt_offset		Shader binding table offset for hit group selection.
*	@note	Uses identity transform matrix. Only appends if BLAS contains valid geometry.
*			Stores primitive offset and model index in uniform instance buffer for shaders.
*/
static void
append_blas(QvkGeometryInstance_t *instances, uint32_t *num_instances, accel_struct_t* blas, int vbo_index, uint prim_offset, int mask, int flags, int sbt_offset)
{
	// Skip if BLAS has no geometry this frame
	if (!blas->present)
		return;

	// Create instance with identity transform
	QvkGeometryInstance_t instance = {
		{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
		},
		vbo_index,
		mask,
		sbt_offset,
		flags,
		0
	};
	
	// Query BLAS device address for instance reference
	VkAccelerationStructureDeviceAddressInfoKHR  as_device_address_info = {};
	as_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	as_device_address_info.accelerationStructure = blas->accel;

	instance.acceleration_structure = qvkGetAccelerationStructureDeviceAddressKHR(qvk.device, &as_device_address_info);
	
	// Append instance to global array
	assert(*num_instances < MAX_TLAS_INSTANCES);
	memcpy(instances + *num_instances, &instance, sizeof(instance));
	
	// Store metadata for shader access
	vkpt_refdef.uniform_instance_buffer.tlas_instance_prim_offsets[*num_instances] = prim_offset;
	vkpt_refdef.uniform_instance_buffer.tlas_instance_model_indices[*num_instances] = -1;
	++*num_instances;
}



/**
*
*
*
*	Instance Management (Public API):
*
*
*
**/
/**
*	@brief	Resets the global instance counter for a new frame.
*	@note	Must be called at the start of each frame before adding model instances.
*/
void vkpt_pt_reset_instances()
{
	g_num_instances = 0;
}

/**
*	@brief	Adds a model BLAS instance to the global instance buffer.
*	@param	geom					Model geometry containing BLAS and render parameters.
*	@param	transform				4x4 transformation matrix (transposed for Vulkan).
*	@param	buffer_idx				Vertex buffer index for hit shader.
*	@param	model_instance_index	Model instance index for shader lookup.
*	@param	override_instance_mask	Optional instance mask override (0 = use geometry mask).
*	@note	Used for instancing static and dynamic models with custom transforms.
*			Stores model index in uniform buffer for shader material/animation lookup.
*/
void vkpt_pt_instance_model_blas(const model_geometry_t* geom, const mat4 transform, uint32_t buffer_idx, int model_instance_index, uint32_t override_instance_mask)
{
	// Skip if geometry has no BLAS (not loaded or incompatible)
	if (!geom->accel)
		return;

	// Create instance with transposed transform matrix (Vulkan expects row-major 3x4)
	QvkGeometryInstance_t gpu_instance = {
		{ // transpose the matrix
			transform[0][0], transform[1][0], transform[2][0], transform[3][0],
			transform[0][1], transform[1][1], transform[2][1], transform[3][1],
			transform[0][2], transform[1][2], transform[2][2], transform[3][2]
		},
		buffer_idx,
		override_instance_mask ? override_instance_mask : geom->instance_mask,
		geom->sbt_offset,
		geom->instance_flags,
		geom->blas_device_address,
	};

	// Append to global instance array
	assert(g_num_instances < MAX_TLAS_INSTANCES);
	memcpy(g_instances + g_num_instances, &gpu_instance, sizeof(gpu_instance));
	
	// Store metadata for shader material/animation lookup
	vkpt_refdef.uniform_instance_buffer.tlas_instance_prim_offsets[g_num_instances] = geom->prim_offsets[0];
	vkpt_refdef.uniform_instance_buffer.tlas_instance_model_indices[g_num_instances] = model_instance_index;
	++g_num_instances;
}

/**
*	@brief	Builds a top-level acceleration structure from instance data.
*	@param	cmd_buf			Command buffer for AS build commands.
*	@param	as				TLAS container to create/update.
*	@param	instance_data	Device address of instance buffer.
*	@param	num_instances	Number of instances in buffer.
*	@note	Uses fast build flags (prefer build speed). Reallocates TLAS if instance
*			count increases. Uses shared scratch buffer for temporary build data.
*/
static void
build_tlas(VkCommandBuffer cmd_buf, accel_struct_t* as, VkDeviceAddress instance_data, uint32_t num_instances)
{
	// Setup instance geometry for TLAS
	VkAccelerationStructureGeometryInstancesDataKHR instances_data = {};
	instances_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instances_data.data.deviceAddress = instance_data;
	
	VkAccelerationStructureGeometryDataKHR geometry = {};
	geometry.instances = instances_data;

	VkAccelerationStructureGeometryKHR topASGeometry = {};
	topASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	topASGeometry.geometry = geometry;

	// Prepare build info for size query
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR; // Fast build for dynamic TLAS
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &topASGeometry;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	// Query required TLAS size
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR(qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &num_instances, &sizeInfo);
	assert(sizeInfo.accelerationStructureSize < SIZE_SCRATCH_BUFFER);

	// Recreate TLAS if instance count increased beyond allocated capacity
	if (!accel_matches_top_level(&as->match, true, num_instances))
	{
		destroy_accel_struct(as);

		// Create buffer backing the TLAS
		buffer_create(&as->mem, sizeInfo.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Create TLAS object
		VkAccelerationStructureCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		createInfo.size = sizeInfo.accelerationStructureSize;
		createInfo.buffer = as->mem.buffer;

		qvkCreateAccelerationStructureKHR(qvk.device, &createInfo, NULL, &as->accel);

		// Store match info for future rebuild detection
		as->match.fast_build = true;
		as->match.index_count = 0;
		as->match.vertex_count = 0;
		as->match.aabb_count = 0;
		as->match.instance_count = num_instances;
	}

	// Set destination TLAS and scratch buffer
	buildInfo.dstAccelerationStructure = as->accel;
	buildInfo.scratchData.deviceAddress = buf_accel_scratch.address + scratch_buf_ptr;
	assert(buf_accel_scratch.address);
	
	// Update scratch buffer pointer with alignment
	scratch_buf_ptr += sizeInfo.buildScratchSize;
	scratch_buf_ptr = align(scratch_buf_ptr, minAccelerationStructureScratchOffsetAlignment);
	assert(scratch_buf_ptr < SIZE_SCRATCH_BUFFER);

	// Specify instance range for build
	VkAccelerationStructureBuildRangeInfoKHR offset = {};
	offset.primitiveCount = num_instances;

	const VkAccelerationStructureBuildRangeInfoKHR* offsets = &offset;

	// Execute TLAS build on GPU
	qvkCmdBuildAccelerationStructuresKHR(
		cmd_buf,
		1,
		&buildInfo,
		&offsets);
}

/**
*	@brief	Creates top-level acceleration structures for geometry and effects.
*	@param	cmd_buf				Command buffer for AS build commands.
*	@param	idx					Frame index (0 to MAX_FRAMES_IN_FLIGHT-1).
*	@param	upload_info			Entity upload info with primitive offsets.
*	@param	weapon_left_handed	Whether first-person weapon is left-handed (flips winding).
*	@return	VK_SUCCESS on success.
*	@note	Builds two TLAS:
*			- Geometry TLAS: opaque/transparent/masked geometry, viewer models/weapon
*			- Effects TLAS: explosions, particles, beams, sprites
*			Appends all BLAS instances from global instance buffer, copies to device,
*			and builds both TLAS using shared scratch buffer. Memory barrier ensures
*			builds complete before ray tracing.
*/
VkResult
vkpt_pt_create_toplevel(VkCommandBuffer cmd_buf, int idx, const EntityUploadInfo* upload_info, bool weapon_left_handed)
{
	// Append opaque dynamic geometry (BSP world, dynamic models)
	append_blas(g_instances, &g_num_instances, &blas_dynamic[idx], VERTEX_BUFFER_INSTANCED, 0,
		AS_FLAG_OPAQUE, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR, SBTO_OPAQUE);

	// Append transparent models (glass, water) - treated as opaque for primary rays
	append_blas(g_instances, &g_num_instances, &blas_transparent_models[idx], VERTEX_BUFFER_INSTANCED, upload_info->transparent_prim_offset,
		AS_FLAG_TRANSPARENT, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR, SBTO_OPAQUE);

	// Append masked models (fences, foliage) - uses any-hit shader for alpha testing
	append_blas(g_instances, &g_num_instances, &blas_masked_models[idx], VERTEX_BUFFER_INSTANCED, upload_info->masked_prim_offset,
		AS_FLAG_OPAQUE, VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, SBTO_MASKED);

	// Append first-person weapon - uses masked shader to support muzzleflash alpha testing
	// Flip winding order for left-handed weapons
	append_blas( g_instances, &g_num_instances, &blas_viewer_weapon[ idx ], VERTEX_BUFFER_INSTANCED, upload_info->viewer_weapon_prim_offset,
		AS_FLAG_VIEWER_WEAPON, VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR | ( weapon_left_handed ? VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR : 0 ), SBTO_MASKED );

	// Append first-person body model (only in first-person mode)
	if (cl_player_model->integer == CL_PLAYER_MODEL_FIRST_PERSON)
	{
		append_blas(g_instances, &g_num_instances, &blas_viewer_models[idx], VERTEX_BUFFER_INSTANCED, upload_info->viewer_model_prim_offset,
			AS_FLAG_VIEWER_MODELS, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, SBTO_OPAQUE);
	}
	
	// All geometry instances added - mark split point
	uint32_t num_instances_geometry = g_num_instances;

	// Append explosion sprites (uses custom primitive addressing via vbo_index field)
	append_blas(g_instances, &g_num_instances, &blas_explosions[idx], (int)upload_info->explosions_prim_offset, 0,
		AS_FLAG_EFFECTS, 0, SBTO_EXPLOSION);
	
	// Append particle system (if enabled)
	if (cvar_pt_enable_particles->integer != 0)
	{
		append_blas(g_instances, &g_num_instances, &blas_particles[idx], 0, 0,
			AS_FLAG_EFFECTS, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, SBTO_PARTICLE);
	}

	// Append cylindrical beams (if enabled)
	if (cvar_pt_enable_beams->integer != 0)
	{
		append_blas(g_instances, &g_num_instances, &blas_beams[idx], 0, 0,
			AS_FLAG_EFFECTS, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, SBTO_BEAM);
	}

	// Append billboard sprites (if enabled)
	if (cvar_pt_enable_sprites->integer != 0)
	{
		append_blas(g_instances, &g_num_instances, &blas_sprites[idx], 0, 0,
			AS_FLAG_EFFECTS, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, SBTO_SPRITE);
	}

	// Calculate effect instance count
	uint32_t num_instances_effects = g_num_instances - num_instances_geometry;
	
	// Copy instance data to device-visible buffer
	void *instance_data = buffer_map(buf_instances + idx);
	memcpy(instance_data, &g_instances, sizeof(QvkGeometryInstance_t) * g_num_instances);

	buffer_unmap(buf_instances + idx);
	instance_data = NULL;

	// Build separate TLAS for geometry and effects
	// Effects TLAS allows independent masking/culling for particles/beams/sprites
	scratch_buf_ptr = 0;
	build_tlas(cmd_buf, &tlas_geometry[idx], buf_instances[idx].address, num_instances_geometry);
	build_tlas(cmd_buf, &tlas_effects[idx], buf_instances[idx].address + num_instances_geometry * sizeof(QvkGeometryInstance_t), num_instances_effects);

	// Memory barrier ensures TLAS builds complete before ray tracing
	MEM_BARRIER_BUILD_ACCEL(cmd_buf);

	return VK_SUCCESS;
}



/**
*
*
*
*	Ray Tracing Pipeline Setup and Execution:
*
*
*
**/
/**
*	@brief	Image memory barrier macro for compute passes.
*	@note	Ensures shader writes complete before subsequent compute reads/writes.
*			Keeps image in GENERAL layout for compute shader access.
*/
#define BARRIER_COMPUTE(cmd_buf, img) \
	do { \
		VkImageSubresourceRange subresource_range = {}; \
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; \
		subresource_range.baseMipLevel = 0; \
		subresource_range.levelCount = 1; \
		subresource_range.baseArrayLayer = 0; \
		subresource_range.layerCount = 1; \
		IMAGE_BARRIER(cmd_buf, \
				.image            = img, \
				.subresourceRange = subresource_range, \
				.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT, \
				.dstAccessMask    = VK_ACCESS_SHADER_WRITE_BIT, \
				.oldLayout        = VK_IMAGE_LAYOUT_GENERAL, \
				.newLayout        = VK_IMAGE_LAYOUT_GENERAL, \
		); \
	} while(0)

/**
*	@brief	Binds ray tracing pipeline and descriptor sets.
*	@param	cmd_buf		Command buffer for pipeline binding.
*	@param	bind_point	Pipeline bind point (compute or ray tracing).
*	@param	index		Pipeline index to bind.
*	@note	Binds all required descriptor sets: RT-specific (Set 0), UBO (Set 1),
*			textures (Set 2), and vertex buffer (Set 3).
*/
static void setup_rt_pipeline(VkCommandBuffer cmd_buf, VkPipelineBindPoint bind_point, pipeline_index_t index)
{
	// Bind the ray tracing pipeline
	vkCmdBindPipeline(cmd_buf, bind_point, rt_pipelines[index]);

	// Set 0: RT-specific descriptors (TLAS, particle/beam/sprite buffers)
	vkCmdBindDescriptorSets(cmd_buf, bind_point,
		rt_pipeline_layout, 0, 1, rt_descriptor_set + qvk.current_frame_index, 0, 0);

	// Set 1: Uniform buffer objects (global constants, frame data)
	vkCmdBindDescriptorSets(cmd_buf, bind_point,
		rt_pipeline_layout, 1, 1, &qvk.desc_set_ubo, 0, 0);

	// Set 2: Material textures (diffuse, normal, emissive, etc)
	VkDescriptorSet desc_set_textures = qvk_get_current_desc_set_textures();
	vkCmdBindDescriptorSets(cmd_buf, bind_point,
		rt_pipeline_layout, 2, 1, &desc_set_textures, 0, 0);

	// Set 3: Vertex and index buffers for geometry
	vkCmdBindDescriptorSets(cmd_buf, bind_point,
		rt_pipeline_layout, 3, 1, &qvk.desc_set_vertex_buffer, 0, 0);
}

/**
*	@brief	Dispatches ray tracing work for a pipeline.
*	@param	cmd_buf			Command buffer for dispatch.
*	@param	pipeline_index	Pipeline to execute.
*	@param	push			Push constants (GPU index, bounce count).
*	@param	width			Dispatch width in pixels.
*	@param	height			Dispatch height in pixels.
*	@param	depth			Dispatch depth (1 for mono, 2 for stereo).
*	@note	Uses compute dispatch for ray query mode, or vkCmdTraceRaysKHR for
*			ray tracing pipeline mode. Shader binding table is constructed with
*			per-pipeline offsets for different geometry types.
*/
static void
dispatch_rays(VkCommandBuffer cmd_buf, pipeline_index_t pipeline_index, pt_push_constants_t push, uint32_t width, uint32_t height, uint32_t depth)
{
	// Ray Query mode: Use compute shaders with inline ray queries
	if (qvk.use_ray_query)
	{
		setup_rt_pipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_index);

		vkCmdPushConstants(cmd_buf, rt_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

		// Dispatch compute work groups (8x8 threads per group)
		vkCmdDispatch(cmd_buf, (width + 7) / 8, (height + 7) / 8, depth);
	}
	// Ray Tracing Pipeline mode: Use dedicated RT shaders and SBT
	else
	{
		setup_rt_pipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_index);

		vkCmdPushConstants(cmd_buf, rt_pipeline_layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(push), &push);

		// Calculate SBT offset for this pipeline (each pipeline has SBT_ENTRIES_PER_PIPELINE shader groups)
		uint32_t sbt_offset = SBT_ENTRIES_PER_PIPELINE * pipeline_index * shaderGroupBaseAlignment;
		
		assert(buf_shader_binding_table.address);

		// Ray generation shader region (single entry)
		VkStridedDeviceAddressRegionKHR raygen = {};
		raygen.deviceAddress = buf_shader_binding_table.address + sbt_offset;
		raygen.stride = shaderGroupBaseAlignment;
		raygen.size = shaderGroupBaseAlignment;

		// Miss and hit shader regions (all other SBT entries for this pipeline)
		VkStridedDeviceAddressRegionKHR miss_and_hit = {};
		miss_and_hit.deviceAddress = buf_shader_binding_table.address + sbt_offset;
		miss_and_hit.stride = shaderGroupBaseAlignment;
		miss_and_hit.size = (VkDeviceSize)shaderGroupBaseAlignment * SBT_ENTRIES_PER_PIPELINE;

		// No callable shaders used
		VkStridedDeviceAddressRegionKHR callable = {};
		callable.deviceAddress = VK_NULL_HANDLE;
		callable.stride = 0;
		callable.size = 0;

		// Dispatch ray tracing work
		qvkCmdTraceRaysKHR(cmd_buf,
			&raygen,
			&miss_and_hit,
			&miss_and_hit,
			&callable,
			width, height, depth);
	}
}

/**
*	@brief	Traces primary visibility rays from the camera.
*	@param	cmd_buf	Command buffer for ray tracing commands.
*	@return	VK_SUCCESS on success.
*	@note	Executes the first ray tracing pass to determine visible surfaces.
*			Outputs visibility buffers (primitive ID, barycentric coordinates),
*			G-buffers (base color, normals, metallic), motion vectors, depth,
*			and transparency for subsequent rendering passes. Supports multi-GPU
*			rendering by dispatching work to all GPUs. Uses stereo rendering with
*			half-width dispatches (depth=2) for VR, or full-width mono (depth=1).
*/
VkResult
vkpt_pt_trace_primary_rays(VkCommandBuffer cmd_buf)
{
	int frame_idx = qvk.frame_counter & 1;
	
	// Prepare readback buffer for GPU-to-CPU data transfer (sun visibility, statistics)
	BUFFER_BARRIER(cmd_buf,
			.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			.buffer = qvk.buf_readback.buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
	);

	BEGIN_PERF_MARKER(cmd_buf, PROFILER_PRIMARY_RAYS);

	// Multi-GPU rendering: dispatch work to each GPU
	for(int i = 0; i < qvk.device_count; i++)
	{
		set_current_gpu(cmd_buf, i);

		pt_push_constants_t push;
		push.gpu_index = qvk.device_count == 1 ? -1 : i; // -1 for single GPU (all pixels), GPU index for multi-GPU (interleaved pixels)
		push.bounce = 0; // Primary rays (bounce 0)

		// Dispatch primary rays (half-width for stereo, full-width for mono)
		dispatch_rays(cmd_buf, PIPELINE_PRIMARY_RAYS, push, qvk.extent_render.width / 2, qvk.extent_render.height, qvk.device_count == 1 ? 2 : 1);
	}

	set_current_gpu(cmd_buf, ALL_GPUS);

	END_PERF_MARKER(cmd_buf, PROFILER_PRIMARY_RAYS);

	// Memory barriers ensure primary ray outputs complete before subsequent passes read them
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_VISBUF_PRIM_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_VISBUF_BARY_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_TRANSPARENT]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_MOTION]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_SHADING_POSITION]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_VIEW_DIRECTION]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_THROUGHPUT]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_BOUNCE_THROUGHPUT]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_GODRAYS_THROUGHPUT_DIST]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_BASE_COLOR_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_METALLIC_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_CLUSTER_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_VIEW_DEPTH_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_NORMAL_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_ASVGF_RNG_SEED_A + frame_idx]);

	return VK_SUCCESS;
}

/**
*	@brief	Traces reflection and refraction rays for specular surfaces.
*	@param	cmd_buf	Command buffer for ray tracing commands.
*	@param	bounce	Bounce index (0 for first bounce, 1 for second bounce).
*	@return	VK_SUCCESS on success.
*	@note	Executes reflection/refraction pass for mirror-like and glass surfaces.
*			Uses PIPELINE_REFLECT_REFRACT_1 for first bounce, PIPELINE_REFLECT_REFRACT_2
*			for second bounce. Updates shading position, throughput, and G-buffers
*			for reflected/refracted surfaces. Supports multi-GPU rendering.
*/
VkResult
vkpt_pt_trace_reflections(VkCommandBuffer cmd_buf, int bounce)
{
	int frame_idx = qvk.frame_counter & 1;

	// Multi-GPU rendering: dispatch to each GPU
	for (int i = 0; i < qvk.device_count; i++)
    {
        set_current_gpu(cmd_buf, i);

		// Select pipeline based on bounce index (different specialization constants)
        pipeline_index_t pipeline = (bounce == 0) ? PIPELINE_REFLECT_REFRACT_1 : PIPELINE_REFLECT_REFRACT_2;

        pt_push_constants_t push;
        push.gpu_index = qvk.device_count == 1 ? -1 : i;
        push.bounce = bounce;

        dispatch_rays(cmd_buf, pipeline, push, qvk.extent_render.width / 2, qvk.extent_render.height, qvk.device_count == 1 ? 2 : 1);
	}

	set_current_gpu(cmd_buf, ALL_GPUS);

	// Memory barriers ensure reflection/refraction outputs complete before subsequent passes
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_TRANSPARENT]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_MOTION]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_SHADING_POSITION]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_VIEW_DIRECTION]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_THROUGHPUT]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_GODRAYS_THROUGHPUT_DIST]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_BASE_COLOR_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_METALLIC_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_CLUSTER_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_VIEW_DEPTH_A + frame_idx]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_NORMAL_A + frame_idx]);

	return VK_SUCCESS;
}

/**
*	@brief	Traces lighting rays for direct and indirect illumination.
*	@param	cmd_buf			Command buffer for ray tracing commands.
*	@param	num_bounce_rays	Number of indirect lighting bounces (0-2).
*	@return	VK_SUCCESS on success.
*	@note	Executes multi-pass lighting:
*			1. Direct Lighting: Samples light sources for direct illumination.
*			   Uses caustics pipeline variant if enabled (specialization constant).
*			2. Indirect Lighting: Traces diffuse bounce rays for global illumination.
*			   - First bounce (PIPELINE_INDIRECT_LIGHTING_FIRST): Includes particles
*			   - Second bounce (PIPELINE_INDIRECT_LIGHTING_SECOND): Geometry only
*			   - Supports fractional bounces (0.5 = half resolution height)
*			Outputs separated into frequency bands: LF (low-frequency SH + CoCg),
*			HF (high-frequency detail), and specular. Supports multi-GPU rendering.
*/
VkResult
vkpt_pt_trace_lighting(VkCommandBuffer cmd_buf, float num_bounce_rays)
{
	// === Direct Lighting Pass ===
	BEGIN_PERF_MARKER(cmd_buf, PROFILER_DIRECT_LIGHTING);

	for (int i = 0; i < qvk.device_count; i++)
	{
		set_current_gpu(cmd_buf, i);

		// Select direct lighting pipeline variant (with or without caustics)
		pipeline_index_t pipeline = (cvar_pt_caustics->value != 0) ? PIPELINE_DIRECT_LIGHTING_CAUSTICS : PIPELINE_DIRECT_LIGHTING;

		pt_push_constants_t push;
		push.gpu_index = qvk.device_count == 1 ? -1 : i;
		push.bounce = 0;

		dispatch_rays(cmd_buf, pipeline, push, qvk.extent_render.width / 2, qvk.extent_render.height, qvk.device_count == 1 ? 2 : 1);
	}

	set_current_gpu(cmd_buf, ALL_GPUS);

	END_PERF_MARKER(cmd_buf, PROFILER_DIRECT_LIGHTING);

	// Memory barriers for direct lighting outputs
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_LF_SH]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_LF_COCG]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_HF]);
	BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_SPEC]);

	// Prepare readback buffer for GPU-to-CPU data transfer
	BUFFER_BARRIER(cmd_buf,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		.buffer = qvk.buf_readback.buffer,
		.offset = 0,
		.size = VK_WHOLE_SIZE,
	);

	// === Indirect Lighting Passes ===
	BEGIN_PERF_MARKER(cmd_buf, PROFILER_INDIRECT_LIGHTING);

	assert(num_bounce_rays <= 2);
	if (num_bounce_rays > 0)
	{
		for (int i = 0; i < qvk.device_count; i++)
		{
			set_current_gpu(cmd_buf, i);

			// Calculate height for fractional bounces (0.5 = half resolution)
            int height;
            if (num_bounce_rays == 0.5f)
                height = qvk.extent_render.height / 2;
            else
                height = qvk.extent_render.height;

			// Execute indirect lighting bounces (1 or 2 bounces)
			for (int bounce_ray = 0; bounce_ray < (int)ceilf(num_bounce_rays); bounce_ray++)
            {
				BEGIN_PERF_MARKER(cmd_buf, PROFILER_INDIRECT_LIGHTING_0 + bounce_ray);
				
				// First bounce includes particles, second bounce is geometry only
                pipeline_index_t pipeline = (bounce_ray == 0) ? PIPELINE_INDIRECT_LIGHTING_FIRST : PIPELINE_INDIRECT_LIGHTING_SECOND;

                pt_push_constants_t push;
                push.gpu_index = qvk.device_count == 1 ? -1 : i;
                push.bounce = 0;

                dispatch_rays(cmd_buf, pipeline, push, qvk.extent_render.width / 2, height, qvk.device_count == 1 ? 2 : 1);

				// Memory barriers between indirect lighting bounces
				BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_LF_SH]);
				BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_LF_COCG]);
				BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_HF]);
				BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_COLOR_SPEC]);
				BARRIER_COMPUTE(cmd_buf, qvk.images[VKPT_IMG_PT_BOUNCE_THROUGHPUT]);

				END_PERF_MARKER(cmd_buf, PROFILER_INDIRECT_LIGHTING_0 + bounce_ray);
			}
		}
	}

	END_PERF_MARKER(cmd_buf, PROFILER_INDIRECT_LIGHTING);

	set_current_gpu(cmd_buf, ALL_GPUS);

	return VK_SUCCESS;
}



/**
*
*
*
*	Pipeline Creation and Shader Binding Table:
*
*
*
**/
/**
*	@brief	Cleans up path tracer resources.
*	@return	VK_SUCCESS on success.
*	@note	Destroys all BLAS, TLAS, scratch buffers, descriptor sets, and layouts.
*			Called during renderer shutdown or device recreation.
*/
VkResult
vkpt_pt_destroy()
{
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		destroy_accel_struct(tlas_geometry + i);
		destroy_accel_struct(tlas_effects + i);
		buffer_destroy(buf_instances + i);
		vkpt_pt_destroy_dynamic(i);
	}
	
	buffer_destroy(&buf_accel_scratch);
	vkDestroyDescriptorSetLayout(qvk.device, rt_descriptor_set_layout, NULL);
	vkDestroyPipelineLayout(qvk.device, rt_pipeline_layout, NULL);
	vkDestroyDescriptorPool(qvk.device, rt_descriptor_pool, NULL);

	return VK_SUCCESS;
}

/**
*	@brief	Creates all ray tracing pipelines and shader binding table.
*	@return	VK_SUCCESS on success, Vulkan error code on failure.
*	@note	Creates 7 specialized ray tracing pipelines:
*			- PIPELINE_PRIMARY_RAYS: Primary visibility from camera
*			- PIPELINE_REFLECT_REFRACT_1/2: Reflection/refraction bounces (1st/2nd)
*			- PIPELINE_DIRECT_LIGHTING: Direct lighting without caustics
*			- PIPELINE_DIRECT_LIGHTING_CAUSTICS: Direct lighting with caustics
*			- PIPELINE_INDIRECT_LIGHTING_FIRST: First indirect bounce (includes particles)
*			- PIPELINE_INDIRECT_LIGHTING_SECOND: Second indirect bounce (geometry only)
*			
*			Each pipeline has different shader configurations and specialization constants.
*			Shader stages include: ray generation (RGEN), miss (RMISS), closest hit (RCHIT),
*			any hit (RAHIT) for particles/explosions/sprites/masked geometry, and
*			intersection (RINT) for procedural beam geometry.
*			
*			Shader Binding Table Layout (per pipeline):
*			- SBT_RGEN: Ray generation shader
*			- SBT_RMISS_EMPTY: Miss shader (sky/environment)
*			- SBT_RCHIT_GEOMETRY: Closest hit for opaque/transparent geometry
*			- SBT_RAHIT_MASKED: Any hit for alpha-tested surfaces
*			- SBT_RCHIT_EFFECTS: Closest hit for effects (unused placeholder)
*			- SBT_RAHIT_PARTICLE: Any hit for particle quads
*			- SBT_RAHIT_EXPLOSION: Any hit for explosion sprites
*			- SBT_RAHIT_SPRITE: Any hit for billboard sprites
*			- SBT_RINT_BEAM: Intersection + any hit for cylindrical beams
*			
*			In Ray Query mode, creates compute pipelines instead of RT pipelines.
*			SBT is not used in Ray Query mode (inline ray queries in compute shaders).
*/
VkResult
vkpt_pt_create_pipelines()
{
	VkSpecializationMapEntry specEntry = {};
	specEntry.constantID = 0;
	specEntry.offset = 0;
	specEntry.size = sizeof(uint32_t);

	uint32_t numbers[2] = { 0, 1 };

	VkSpecializationInfo specInfo[2] = {};
	specInfo[0].mapEntryCount = 1;
	specInfo[0].pMapEntries = &specEntry;
	specInfo[0].dataSize = sizeof(uint32_t);
	specInfo[0].pData = &numbers[0];
	
	specInfo[1].mapEntryCount = 1;
	specInfo[1].pMapEntries = &specEntry;
	specInfo[1].dataSize = sizeof(uint32_t);
	specInfo[1].pData = &numbers[1];

	uint32_t num_shader_groups = SBT_ENTRIES_PER_PIPELINE * PIPELINE_COUNT;
	char* shader_handles = alloca(num_shader_groups * shaderGroupHandleSize);
	memset(shader_handles, 0, num_shader_groups * shaderGroupHandleSize);

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		// Stages used by all pipelines. Count must match num_base_shader_stages below!
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			VK_NULL_HANDLE,  // Shader module is set below
			"main",
			nullptr
		},
		SHADER_STAGE(QVK_MOD_PATH_TRACER_RMISS,               VK_SHADER_STAGE_MISS_BIT_KHR),
		SHADER_STAGE(QVK_MOD_PATH_TRACER_RCHIT,               VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
		SHADER_STAGE(QVK_MOD_PATH_TRACER_MASKED_RAHIT,        VK_SHADER_STAGE_ANY_HIT_BIT_KHR),
		// Stages used by all pipelines that consider transparency
		SHADER_STAGE(QVK_MOD_PATH_TRACER_PARTICLE_RAHIT,      VK_SHADER_STAGE_ANY_HIT_BIT_KHR),
		SHADER_STAGE(QVK_MOD_PATH_TRACER_EXPLOSION_RAHIT,     VK_SHADER_STAGE_ANY_HIT_BIT_KHR),
		SHADER_STAGE(QVK_MOD_PATH_TRACER_SPRITE_RAHIT,        VK_SHADER_STAGE_ANY_HIT_BIT_KHR),
		// Must be last
		SHADER_STAGE(QVK_MOD_PATH_TRACER_BEAM_RAHIT,          VK_SHADER_STAGE_ANY_HIT_BIT_KHR),
		SHADER_STAGE(QVK_MOD_PATH_TRACER_BEAM_RINT,           VK_SHADER_STAGE_INTERSECTION_BIT_KHR),
	};
	const unsigned num_base_shader_stages = 5;
	const unsigned num_transparent_no_beam_shader_stages = 8;

	for (pipeline_index_t index = 0; index < PIPELINE_COUNT; index++)
	{
		bool needs_beams = index <= PIPELINE_REFLECT_REFRACT_2;
		bool needs_transparency = needs_beams || index == PIPELINE_INDIRECT_LIGHTING_FIRST;
		unsigned int num_shader_stages = needs_beams ? LENGTH(shader_stages) : (needs_transparency ? num_transparent_no_beam_shader_stages : num_base_shader_stages);

		switch (index)
		{
		case PIPELINE_PRIMARY_RAYS:
			shader_stages[0].module = qvk.shader_modules[QVK_MOD_PRIMARY_RAYS_RGEN];
			shader_stages[0].pSpecializationInfo = NULL;
			break;
		case PIPELINE_REFLECT_REFRACT_1:
			shader_stages[0].module = qvk.shader_modules[QVK_MOD_REFLECT_REFRACT_RGEN];
			shader_stages[0].pSpecializationInfo = &specInfo[0];
			break;
		case PIPELINE_REFLECT_REFRACT_2:
			shader_stages[0].module = qvk.shader_modules[QVK_MOD_REFLECT_REFRACT_RGEN];
			shader_stages[0].pSpecializationInfo = &specInfo[1];
			break;
		case PIPELINE_DIRECT_LIGHTING:
			shader_stages[0].module = qvk.shader_modules[QVK_MOD_DIRECT_LIGHTING_RGEN];
			shader_stages[0].pSpecializationInfo = &specInfo[0];
			break;
		case PIPELINE_DIRECT_LIGHTING_CAUSTICS:
			shader_stages[0].module = qvk.shader_modules[QVK_MOD_DIRECT_LIGHTING_RGEN];
			shader_stages[0].pSpecializationInfo = &specInfo[1];
			break;
		case PIPELINE_INDIRECT_LIGHTING_FIRST:
			shader_stages[0].module = qvk.shader_modules[QVK_MOD_INDIRECT_LIGHTING_RGEN];
			shader_stages[0].pSpecializationInfo = &specInfo[0];
			break;
		case PIPELINE_INDIRECT_LIGHTING_SECOND:
			shader_stages[0].module = qvk.shader_modules[QVK_MOD_INDIRECT_LIGHTING_RGEN];
			shader_stages[0].pSpecializationInfo = &specInfo[1];
			break;
		default:
			assert(!"invalid pipeline index");
			break;
		}

		if (qvk.use_ray_query)
		{
			VkPipelineShaderStageCreateInfo stage_info = {};
			stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			stage_info.pName = "main";
			stage_info.module = shader_stages[0].module;
			stage_info.pSpecializationInfo = shader_stages[0].pSpecializationInfo;
			
			VkComputePipelineCreateInfo compute_pipeline_info = {};
			compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			compute_pipeline_info.layout = rt_pipeline_layout;
			compute_pipeline_info.stage = stage_info;

			VkResult res = vkCreateComputePipelines(qvk.device, 0, 1, &compute_pipeline_info, NULL, &rt_pipelines[index]);

			if (res != VK_SUCCESS)
			{
				Com_EPrintf("Failed to create ray tracing compute pipeline #%d, vkCreateComputePipelines error code is %s\n", index, qvk_result_to_string(res));
				return res;
			}
		}
		else
		{
			VkRayTracingShaderGroupCreateInfoKHR rt_shader_group_info[SBT_ENTRIES_PER_PIPELINE] = {};
			
			// SBT_RGEN
			rt_shader_group_info[SBT_RGEN].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RGEN].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			rt_shader_group_info[SBT_RGEN].generalShader = 0;
			rt_shader_group_info[SBT_RGEN].closestHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RGEN].anyHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RGEN].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RMISS_EMPTY
			rt_shader_group_info[SBT_RMISS_EMPTY].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RMISS_EMPTY].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			rt_shader_group_info[SBT_RMISS_EMPTY].generalShader = 1;
			rt_shader_group_info[SBT_RMISS_EMPTY].closestHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RMISS_EMPTY].anyHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RMISS_EMPTY].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RCHIT_GEOMETRY
			rt_shader_group_info[SBT_RCHIT_GEOMETRY].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RCHIT_GEOMETRY].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			rt_shader_group_info[SBT_RCHIT_GEOMETRY].generalShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RCHIT_GEOMETRY].closestHitShader = 2;
			rt_shader_group_info[SBT_RCHIT_GEOMETRY].anyHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RCHIT_GEOMETRY].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RAHIT_MASKED
			rt_shader_group_info[SBT_RAHIT_MASKED].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RAHIT_MASKED].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			rt_shader_group_info[SBT_RAHIT_MASKED].generalShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RAHIT_MASKED].closestHitShader = 2;
			rt_shader_group_info[SBT_RAHIT_MASKED].anyHitShader = 3;
			rt_shader_group_info[SBT_RAHIT_MASKED].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RCHIT_EFFECTS
			rt_shader_group_info[SBT_RCHIT_EFFECTS].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RCHIT_EFFECTS].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			rt_shader_group_info[SBT_RCHIT_EFFECTS].generalShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RCHIT_EFFECTS].closestHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RCHIT_EFFECTS].anyHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RCHIT_EFFECTS].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RAHIT_PARTICLE
			rt_shader_group_info[SBT_RAHIT_PARTICLE].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RAHIT_PARTICLE].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			rt_shader_group_info[SBT_RAHIT_PARTICLE].generalShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RAHIT_PARTICLE].closestHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RAHIT_PARTICLE].anyHitShader = 4;
			rt_shader_group_info[SBT_RAHIT_PARTICLE].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RAHIT_EXPLOSION
			rt_shader_group_info[SBT_RAHIT_EXPLOSION].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RAHIT_EXPLOSION].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			rt_shader_group_info[SBT_RAHIT_EXPLOSION].generalShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RAHIT_EXPLOSION].closestHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RAHIT_EXPLOSION].anyHitShader = 5;
			rt_shader_group_info[SBT_RAHIT_EXPLOSION].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RAHIT_SPRITE
			rt_shader_group_info[SBT_RAHIT_SPRITE].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RAHIT_SPRITE].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			rt_shader_group_info[SBT_RAHIT_SPRITE].generalShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RAHIT_SPRITE].closestHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RAHIT_SPRITE].anyHitShader = 6;
			rt_shader_group_info[SBT_RAHIT_SPRITE].intersectionShader = VK_SHADER_UNUSED_KHR;
			
			// SBT_RINT_BEAM
			rt_shader_group_info[SBT_RINT_BEAM].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			rt_shader_group_info[SBT_RINT_BEAM].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
			rt_shader_group_info[SBT_RINT_BEAM].generalShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RINT_BEAM].closestHitShader = VK_SHADER_UNUSED_KHR;
			rt_shader_group_info[SBT_RINT_BEAM].anyHitShader = 7;
			rt_shader_group_info[SBT_RINT_BEAM].intersectionShader = 8;

			unsigned int num_shader_groups = needs_beams ? SBT_ENTRIES_PER_PIPELINE : (needs_transparency ? SBT_RINT_BEAM : SBT_FIRST_TRANSPARENCY);

			VkPipelineLibraryCreateInfoKHR library_info = {};
			library_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
			
			VkRayTracingPipelineCreateInfoKHR rt_pipeline_info = {};
			rt_pipeline_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
			rt_pipeline_info.pNext = NULL;
			rt_pipeline_info.flags = 0;
			rt_pipeline_info.stageCount = num_shader_stages;
			rt_pipeline_info.pStages = shader_stages;
			rt_pipeline_info.groupCount = num_shader_groups;
			rt_pipeline_info.pGroups = rt_shader_group_info;
			rt_pipeline_info.maxPipelineRayRecursionDepth = 1;
			rt_pipeline_info.pLibraryInfo = &library_info;
			rt_pipeline_info.pLibraryInterface = NULL;
			rt_pipeline_info.pDynamicState = NULL;
			rt_pipeline_info.layout = rt_pipeline_layout;
			rt_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
			rt_pipeline_info.basePipelineIndex = 0;

			assert(LENGTH(rt_shader_group_info) == SBT_ENTRIES_PER_PIPELINE);

			VkResult res = qvkCreateRayTracingPipelinesKHR(qvk.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rt_pipeline_info, NULL, &rt_pipelines[index]);
			
			if (res != VK_SUCCESS)
			{
				Com_EPrintf("Failed to create ray tracing pipeline #%d, vkCreateRayTracingPipelinesKHR error code is %s\n", index, qvk_result_to_string(res));
				return res;
			}

			_VK(qvkGetRayTracingShaderGroupHandlesKHR(
				qvk.device, rt_pipelines[index], 0, num_shader_groups,
				/* dataSize = */ num_shader_groups * shaderGroupHandleSize,
				/* pData = */ shader_handles + SBT_ENTRIES_PER_PIPELINE * shaderGroupHandleSize * index));
		}
	}

	if (qvk.use_ray_query)
	{
		// No SBT in RQ mode, just return
		return VK_SUCCESS;
	}

	// create the SBT buffer
	uint32_t shader_binding_table_size = num_shader_groups * shaderGroupBaseAlignment;
	_VK(buffer_create(&buf_shader_binding_table, shader_binding_table_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

	// copy/unpack the shader handles into the SBT:
	// shaderGroupBaseAlignment is likely greater than shaderGroupHandleSize (64 vs 32 on NV)
	char* shader_binding_table = (char*)buffer_map(&buf_shader_binding_table);
	for (uint32_t group = 0; group < num_shader_groups; group++)
	{
		memcpy(
			shader_binding_table + group * shaderGroupBaseAlignment,
			shader_handles + group * shaderGroupHandleSize,
			shaderGroupHandleSize);
	}
	buffer_unmap(&buf_shader_binding_table);
	shader_binding_table = NULL;

	return VK_SUCCESS;
}

/**
*	@brief	Destroys all ray tracing pipelines and shader binding table.
*	@return	VK_SUCCESS on success.
*	@note	Called during cleanup or when recreating pipelines (shader reload, etc).
*/
VkResult
vkpt_pt_destroy_pipelines()
{
	buffer_destroy(&buf_shader_binding_table);

	// Destroy all pipeline objects
	for (pipeline_index_t index = 0; index < PIPELINE_COUNT; index++)
	{
		vkDestroyPipeline(qvk.device, rt_pipelines[index], NULL);
		rt_pipelines[index] = VK_NULL_HANDLE;
	}

	return VK_SUCCESS;
}
