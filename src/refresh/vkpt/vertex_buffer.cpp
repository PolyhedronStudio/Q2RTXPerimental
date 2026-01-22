/********************************************************************
*
*
*	VKPT Renderer: Vertex Buffer and Geometry Management
*
*	Manages vertex buffer resources and geometry for BSP world rendering
*	and dynamic models in the hardware ray tracing pipeline. Handles
*	vertex data uploads, Bottom-Level Acceleration Structure (BLAS)
*	creation, and geometry batching for efficient ray tracing. Supports
*	both static geometry (BSP world, static models) and dynamic animated
*	geometry (skeletal models, material animations).
*
*	Architecture:
*	- VBO Allocation: Separate buffers for world, models, and dynamic instances
*	  - World Buffer: BSP geometry (opaque, transparent, masked, sky)
*	  - Model VBOs: Individual buffers for each static model
*	  - Instance Buffers: Dynamic animated primitives with material animation
*	- BLAS Management: Bottom-Level Acceleration Structures per geometry type
*	  - Static BLAS: Prefer fast trace, built once per level load
*	  - Dynamic BLAS: Prefer fast build, rebuilt each frame
*	  - Per-Material BLAS: Separate acceleration structures for opaque/transparent/masked
*	- Geometry Batching: Multiple meshes packed into single BLAS
*	  - BSP models: Embedded in world buffer with individual BLAS
*	  - IQM models: Separate VBOs with skeletal animation support
*
*	Algorithm Flow (Initialization):
*	1. Upload BSP Mesh: Transfer BSP geometry to world buffer
*	2. Build Static BLAS: Create acceleration structures for world geometry
*	3. Upload Models: Transfer model meshes to individual VBOs
*	4. Build Model BLAS: Create acceleration structures for static models
*	5. Create Dynamic Buffers: Allocate animated primitive buffers
*
*	Algorithm Flow (Per Frame):
*	1. Instance Geometry: Copy and transform dynamic primitives
*	2. Animate Materials: Update texture coordinates for animated surfaces
*	3. Build Dynamic BLAS: Rebuild acceleration structures for animated geometry
*	4. Update Descriptors: Bind buffers to shader descriptor sets
*
*	Features:
*	- Dynamic primitive buffer resizing based on scene complexity
*	- Material animation via compute shader (scrolling/warping textures)
*	- Separate handling of opaque, transparent, and masked geometry
*	- Instance mask and SBT offset configuration for ray tracing
*	- Light buffer management for dynamic lighting
*	- IQM skeletal animation matrix uploads
*	- Model VBO invalidation and rebuild on material changes
*
*	Configuration:
*	- PRIMBUF_SIZE_MIN: Minimum animated primitive buffer size (64K primitives)
*	- PRIMBUF_SIZE_MAX: Maximum animated primitive buffer size (64M primitives)
*	- PRIMBUF_SIZE_DEFAULT: Default startup size (1M primitives)
*	- ACCEL_STRUCT_ALIGNMENT: Required alignment for BLAS buffers (256 bytes)
*	- MAX_MODELS: Maximum number of models supported (2048)
*	- pt_primbuf: Cvar controlling initial primitive buffer size
*
*	Performance Optimizations:
*	- Static model VBO caching to avoid redundant uploads
*	- Primitive buffer growth strategy to minimize reallocation stutter
*	- Compute shader material animation instead of CPU updates
*	- Scratch buffer reuse across BLAS builds
*	- Batch descriptor updates for model VBOs
*
*	Memory Layout:
*	World Buffer:
*	  [0..N] BSP Primitives (VboPrimitive)
*	  [N..M] BSP Vertex Positions (prim_positions_t)
*	  [M..] BLAS Storage (opaque, transparent, masked, sky, models)
*	Instance Buffer:
*	  [0..K] Dynamic Primitives (VboPrimitive)
*	Position Buffer:
*	  [0..K] Dynamic Vertex Positions (prim_positions_t)
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

#include "vkpt.h"

#include "shader/vertex_buffer.h"
#include "material.h"

#include <assert.h>
#include "conversion.h"




/**
*
*
*
*	Module State and Configuration:
*
*
*
**/
//! Descriptor pool for vertex buffer descriptor sets.
static VkDescriptorPool desc_pool_vertex_buffer;
//! Compute pipeline for geometry instancing (transforms and copies dynamic primitives).
static VkPipeline       pipeline_instance_geometry;
//! Compute pipeline for material animation (scrolling/warping texture coordinates).
static VkPipeline       pipeline_animate_materials;
//! Pipeline layout for instance geometry compute pipelines.
static VkPipelineLayout pipeline_layout_instance_geometry;

//! Per-model vertex buffer data (VBO handles, BLAS, metadata).
model_vbo_t model_vertex_data[MAX_MODELS];
//! Null buffer resource used as placeholder for uninitialized model VBOs.
static BufferResource_t null_buffer;

//! Cvar controlling the initial animated primitive buffer size at startup.
//! The buffer can grow later if necessary, but that causes frame stutter.
static cvar_t* cvar_pt_primbuf = NULL;
//! Current size of the animated primitive buffer in number of primitives.
static uint32_t current_primbuf_size = 0;

//! Minimum size for animated primitive buffer (65,536 primitives = 8 MB VboPrimitive data).
#define PRIMBUF_SIZE_MIN (1 << 16)
//! Maximum size for animated primitive buffer (67,108,864 primitives = 8 GB VboPrimitive data).
#define PRIMBUF_SIZE_MAX (1 << 26)
//! Default startup size for animated primitive buffer (1,048,576 primitives = 128 MB VboPrimitive data).
#define PRIMBUF_SIZE_DEFAULT (1 << 20)

//! Per Vulkan spec, acceleration structure buffer offset must be aligned to 256 bytes.
//! @see https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkAccelerationStructureCreateInfoKHR.html
#define ACCEL_STRUCT_ALIGNMENT 256





/**
*
*
*
*	Model Geometry Management - Initialization and Allocation:
*
*
*
**/
/**
*	@brief	Initializes a model_geometry_t structure with storage for acceleration structure data.
*	@param	info			Pointer to model_geometry_t to initialize.
*	@param	max_geometries	Maximum number of geometry entries (meshes) to allocate.
*	@note	Allocates memory for VkAccelerationStructureGeometryKHR, build ranges, and primitive counts.
*	@note	Sets up geometries, build_ranges, prim_counts, and prim_offsets array pointers.
*	@note	Does nothing if max_geometries is 0 or if geometry_storage is already allocated.
**/
void vkpt_init_model_geometry(model_geometry_t* info, uint32_t max_geometries)
{
	assert(info->geometry_storage == NULL); // avoid double allocation

	if (max_geometries == 0)
		return;

	size_t size_geometries = max_geometries * sizeof(VkAccelerationStructureGeometryKHR);
	size_t size_build_ranges = max_geometries * sizeof(VkAccelerationStructureBuildRangeInfoKHR);
	size_t size_prims = max_geometries * sizeof(uint32_t);

	info->geometry_storage = Z_Mallocz(size_geometries + size_build_ranges + size_prims * 2);

	info->geometries = (VkAccelerationStructureGeometryKHR*)info->geometry_storage;
	info->build_ranges = (VkAccelerationStructureBuildRangeInfoKHR*)(info->geometry_storage + size_geometries);
	info->prim_counts = (uint32_t*)(info->geometry_storage + size_geometries + size_build_ranges);
	info->prim_offsets = (uint32_t*)(info->geometry_storage + size_geometries + size_build_ranges + size_prims);

	info->max_geometries = max_geometries;
}

/**
*	@brief	Destroys a model_geometry_t structure and frees its resources.
*	@param	info	Pointer to model_geometry_t to destroy.
*	@note	Frees geometry storage memory and destroys the acceleration structure if present.
*	@note	Resets all pointers to NULL after cleanup.
**/
void vkpt_destroy_model_geometry(model_geometry_t* info)
{
	if (!info->geometry_storage)
		return;

	Z_Freep((void**)&info->geometry_storage);
	info->geometries = NULL;
	info->build_ranges = NULL;
	info->prim_counts = NULL;
	info->prim_offsets = NULL;

	if (info->accel)
	{
		qvkDestroyAccelerationStructureKHR(qvk.device, info->accel, NULL);
		info->accel = NULL;
	}
}

/**
*	@brief	Appends a geometry entry (mesh) to a model_geometry_t structure.
*	@param	info		Pointer to model_geometry_t to append to.
*	@param	num_prims	Number of triangles in this geometry.
*	@param	prim_offset	Offset (in triangles) into the vertex buffer where this geometry starts.
*	@param	model_name	Name of the model for error reporting.
*	@note	Does nothing if num_prims is 0 or if max_geometries limit is exceeded.
*	@note	Configures triangle geometry with VK_FORMAT_R32G32B32_SFLOAT vertices and no indices.
**/
void vkpt_append_model_geometry(model_geometry_t* info, uint32_t num_prims, uint32_t prim_offset, const char* model_name)
{
	if (num_prims == 0)
		return;

	if (info->num_geometries >= info->max_geometries)
	{
		Com_WPrintf("Model '%s' exceeds the maximum supported number of meshes (%d)\n", model_name, info->max_geometries);
		return;
	}

	VkAccelerationStructureGeometryKHR geometry = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
			.geometry = {
				.triangles = {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
					.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
					.vertexStride = sizeof(vec3),
					.maxVertex = num_prims * 3,
					.indexType = VK_INDEX_TYPE_NONE_KHR
				}
		}
	};

	VkAccelerationStructureBuildRangeInfoKHR build_range = {
		.primitiveCount = num_prims
	};

	uint32_t geom_index = info->num_geometries;

	info->geometries[geom_index] = geometry;
	info->build_ranges[geom_index] = build_range;
	info->prim_counts[geom_index] = num_prims;
	info->prim_offsets[geom_index] = prim_offset;

	++info->num_geometries;
}



/**
*
*
*
*	BLAS Management - Allocation, Creation, and Building:
*
*
*
**/
/**
*	@brief	Calculates BLAS memory requirements and suballocates space within a VBO.
*	@param	info		Pointer to model_geometry_t containing geometry descriptors.
*	@param	vbo_size	Pointer to VBO size; updated to include BLAS storage (aligned).
*	@param	model_name	Name of the model for error reporting.
*	@note	Queries Vulkan for acceleration structure size and scratch buffer requirements.
*	@note	Uses VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR for static geometry.
*	@note	Resets num_geometries to 0 if scratch buffer is insufficient.
**/
static void suballocate_model_blas_memory(model_geometry_t* info, size_t* vbo_size, const char* model_name)
{
	VkAccelerationStructureBuildSizesInfoKHR build_sizes = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
	};

	info->build_sizes = build_sizes;

	if (info->num_geometries == 0)
		return;

	VkAccelerationStructureBuildGeometryInfoKHR blasBuildinfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.geometryCount = info->num_geometries,
		.pGeometries = info->geometries
	};

	qvkGetAccelerationStructureBuildSizesKHR(qvk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&blasBuildinfo, info->prim_counts, &info->build_sizes);

	if (info->build_sizes.buildScratchSize > buf_accel_scratch.size)
	{
		Com_WPrintf("Model '%s' requires %lu bytes scratch buffer to build its BLAS, while only %zu are available.\n",
			model_name, info->build_sizes.buildScratchSize, buf_accel_scratch.size);

		info->num_geometries = 0;
	}
	else
	{
		*vbo_size = align(*vbo_size, ACCEL_STRUCT_ALIGNMENT);

		info->blas_data_offset = *vbo_size;
		*vbo_size += info->build_sizes.accelerationStructureSize;
	}
}

/**
*	@brief	Creates a VkAccelerationStructureKHR for a model geometry.
*	@param	info	Pointer to model_geometry_t with suballocated BLAS memory.
*	@param	buffer	VkBuffer that will contain the acceleration structure data.
*	@param	name	Debug name for the acceleration structure (can be NULL).
*	@note	Does nothing if num_geometries is 0.
*	@note	Sets info->accel and info->blas_device_address on success.
*	@note	Attaches a debug label if name is provided.
**/
static void create_model_blas(model_geometry_t* info, VkBuffer buffer, const char* name)
{
	if (info->num_geometries == 0)
		return;

	VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.buffer = buffer,
		.offset = info->blas_data_offset,
		.size = info->build_sizes.accelerationStructureSize,
	};

	_VK(qvkCreateAccelerationStructureKHR(qvk.device, &blasCreateInfo, NULL, &info->accel));
	
	VkAccelerationStructureDeviceAddressInfoKHR  as_device_address_info = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = info->accel
	};

	info->blas_device_address = qvkGetAccelerationStructureDeviceAddressKHR(qvk.device, &as_device_address_info);

	if (name)
		ATTACH_LABEL_VARIABLE_NAME(info->accel, ACCELERATION_STRUCTURE_KHR, name);
}

/**
*	@brief	Builds (or rebuilds) a BLAS by recording build commands into a command buffer.
*	@param	cmd_buf				Command buffer to record into.
*	@param	info				Pointer to model_geometry_t with acceleration structure.
*	@param	first_vertex_offset	Byte offset to the start of vertex position data in the buffer.
*	@param	buffer				Buffer resource containing vertex position data.
*	@note	Does nothing if info->accel is NULL.
*	@note	Sets vertex data device addresses for all geometries before building.
*	@note	Inserts a memory barrier after build to ensure AS reads are safe.
**/
static void build_model_blas(VkCommandBuffer cmd_buf, model_geometry_t* info, size_t first_vertex_offset, const BufferResource_t* buffer)
{
	if (!info->accel)
		return;

	assert(buffer->address);

	uint32_t total_prims = 0;

	for (uint32_t index = 0; index < info->num_geometries; index++)
	{
		VkAccelerationStructureGeometryKHR* geometry = info->geometries + index;

		geometry->geometry.triangles.vertexData.deviceAddress = buffer->address
			+ info->prim_offsets[index] * sizeof(prim_positions_t) + first_vertex_offset;

		total_prims += info->prim_counts[index];
	}

	VkAccelerationStructureBuildGeometryInfoKHR blasBuildinfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.geometryCount = info->num_geometries,
		.pGeometries = info->geometries,
		.dstAccelerationStructure = info->accel,
		.scratchData = {
			.deviceAddress = buf_accel_scratch.address
		}
	};

	const VkAccelerationStructureBuildRangeInfoKHR* pBlasBuildRange = info->build_ranges;

	qvkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &blasBuildinfo, &pBlasBuildRange);

	VkMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
					   | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
	};

	vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1,
		&barrier, 0, 0, 0, 0);
}



/**
*
*
*
*	BSP Mesh Upload and Management:
*
*
*
**/
/**
*	@brief	Uploads BSP world geometry to GPU and builds all associated BLAS.
*	@param	bsp_mesh	Pointer to bsp_mesh_t containing BSP world geometry data.
*	@return	VK_SUCCESS on success, or Vulkan error code on failure.
*	@note	Destroys the previous world buffer before uploading new geometry.
*	@note	Uploads primitives and vertex positions to world buffer via staging buffer.
*	@note	Creates and builds BLAS for opaque, transparent, masked, sky, and custom_sky geometry.
*	@note	Creates and builds individual BLAS for each embedded BSP model.
*	@note	Configures instance masks and SBT offsets for ray tracing.
*	@note	Waits for device idle before and after to ensure synchronization.
**/
VkResult
vkpt_vertex_buffer_upload_bsp_mesh(bsp_mesh_t* bsp_mesh)
{
	assert(bsp_mesh);

	vkDeviceWaitIdle(qvk.device);

	// Destroy the world buffer from the previous map.
	buffer_destroy(&qvk.buf_world);
	size_t vbo_size = bsp_mesh->num_primitives * sizeof(VboPrimitive);
	bsp_mesh->vertex_data_offset = vbo_size;
	vbo_size += bsp_mesh->num_primitives * sizeof(prim_positions_t);
	size_t staging_size = vbo_size;

	suballocate_model_blas_memory(&bsp_mesh->geom_opaque,      &vbo_size, "bsp:opaque");
	suballocate_model_blas_memory(&bsp_mesh->geom_transparent, &vbo_size, "bsp:transparent");
	suballocate_model_blas_memory(&bsp_mesh->geom_masked,      &vbo_size, "bsp:masked");
	suballocate_model_blas_memory(&bsp_mesh->geom_sky,         &vbo_size, "bsp:sky");
	suballocate_model_blas_memory(&bsp_mesh->geom_custom_sky,  &vbo_size, "bsp:custom_sky");

	char name[MAX_QPATH];

	for (int i = 0; i < bsp_mesh->num_models; i++)
	{
		bsp_model_t* model = bsp_mesh->models + i;

		Q_snprintf(name, sizeof(name), "bsp:models[%d]", i);
		
		suballocate_model_blas_memory(&model->geometry, &vbo_size, name);
	}
	
	VkResult res = buffer_create(&qvk.buf_world, vbo_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	if (res != VK_SUCCESS) return res;

	ATTACH_LABEL_VARIABLE(qvk.buf_world.buffer, BUFFER);
	ATTACH_LABEL_VARIABLE(qvk.buf_world.memory, DEVICE_MEMORY);

	BufferResource_t staging_buffer;

	res = buffer_create(&staging_buffer, staging_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (res != VK_SUCCESS) return res;

	create_model_blas(&bsp_mesh->geom_opaque,      qvk.buf_world.buffer, "bsp:opaque");
	create_model_blas(&bsp_mesh->geom_transparent, qvk.buf_world.buffer, "bsp:transparent");
	create_model_blas(&bsp_mesh->geom_masked,      qvk.buf_world.buffer, "bsp:masked");
	create_model_blas(&bsp_mesh->geom_sky,         qvk.buf_world.buffer, "bsp:sky");
	create_model_blas(&bsp_mesh->geom_custom_sky,  qvk.buf_world.buffer, "bsp:custom_sky");

	for (int i = 0; i < bsp_mesh->num_models; i++)
	{
		bsp_model_t* model = bsp_mesh->models + i;

		Q_snprintf(name, sizeof(name), "bsp:models[%d]", i);

		create_model_blas(&model->geometry, qvk.buf_world.buffer, name);
	}

	uint8_t* staging_data = buffer_map(&staging_buffer);
	memcpy(staging_data, bsp_mesh->primitives, bsp_mesh->num_primitives * sizeof(VboPrimitive));

	prim_positions_t* positions = (prim_positions_t*)(staging_data + bsp_mesh->vertex_data_offset);  // NOLINT(clang-diagnostic-cast-align)
	for (uint32_t prim = 0; prim < bsp_mesh->num_primitives; ++prim)
	{
		VectorCopy(bsp_mesh->primitives[prim].pos0, positions[prim][0]);
		VectorCopy(bsp_mesh->primitives[prim].pos1, positions[prim][1]);
		VectorCopy(bsp_mesh->primitives[prim].pos2, positions[prim][2]);
	}

	buffer_unmap(&staging_buffer);
	
	VkCommandBuffer cmd_buf = vkpt_begin_command_buffer(&qvk.cmd_buffers_graphics);

	VkBufferCopy copyRegion = {
		.size = staging_buffer.size,
	};
	vkCmdCopyBuffer(cmd_buf, staging_buffer.buffer, qvk.buf_world.buffer, 1, &copyRegion);
	
	BUFFER_BARRIER(cmd_buf,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		.buffer = qvk.buf_world.buffer,
		.offset = 0,
		.size = VK_WHOLE_SIZE,
	);

	build_model_blas(cmd_buf, &bsp_mesh->geom_opaque,      bsp_mesh->vertex_data_offset, &qvk.buf_world);
	build_model_blas(cmd_buf, &bsp_mesh->geom_transparent, bsp_mesh->vertex_data_offset, &qvk.buf_world);
	build_model_blas(cmd_buf, &bsp_mesh->geom_masked,      bsp_mesh->vertex_data_offset, &qvk.buf_world);
	build_model_blas(cmd_buf, &bsp_mesh->geom_sky,         bsp_mesh->vertex_data_offset, &qvk.buf_world);
	build_model_blas(cmd_buf, &bsp_mesh->geom_custom_sky,  bsp_mesh->vertex_data_offset, &qvk.buf_world);

	bsp_mesh->geom_opaque.instance_mask = AS_FLAG_OPAQUE;
	bsp_mesh->geom_opaque.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	bsp_mesh->geom_opaque.sbt_offset = SBTO_OPAQUE;

	bsp_mesh->geom_transparent.instance_mask = AS_FLAG_TRANSPARENT;
	bsp_mesh->geom_transparent.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	bsp_mesh->geom_transparent.sbt_offset = SBTO_OPAQUE;

	bsp_mesh->geom_masked.instance_mask = AS_FLAG_OPAQUE;
	bsp_mesh->geom_masked.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	bsp_mesh->geom_masked.sbt_offset = SBTO_MASKED;

	bsp_mesh->geom_sky.instance_mask = AS_FLAG_SKY;
	bsp_mesh->geom_sky.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	bsp_mesh->geom_sky.sbt_offset = SBTO_OPAQUE;

	bsp_mesh->geom_custom_sky.instance_mask = AS_FLAG_CUSTOM_SKY;
	bsp_mesh->geom_custom_sky.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	bsp_mesh->geom_custom_sky.sbt_offset = SBTO_OPAQUE;

	for (int i = 0; i < bsp_mesh->num_models; i++)
	{
		bsp_model_t* model = bsp_mesh->models + i;
		build_model_blas(cmd_buf, &model->geometry, bsp_mesh->vertex_data_offset, &qvk.buf_world);

		model->geometry.instance_mask = model->transparent ? bsp_mesh->geom_transparent.instance_mask : bsp_mesh->geom_opaque.instance_mask;
		model->geometry.instance_flags = model->masked ? bsp_mesh->geom_masked.instance_flags : model->transparent ? bsp_mesh->geom_transparent.instance_flags : bsp_mesh->geom_opaque.instance_flags;
		model->geometry.sbt_offset = model->masked ? bsp_mesh->geom_masked.sbt_offset : bsp_mesh->geom_opaque.sbt_offset;
	}

	if (qvk.buf_light_stats[0].buffer)
	{
		vkCmdFillBuffer(cmd_buf, qvk.buf_light_stats[0].buffer, 0, qvk.buf_light_stats[0].size, 0);
		vkCmdFillBuffer(cmd_buf, qvk.buf_light_stats[1].buffer, 0, qvk.buf_light_stats[1].size, 0);
		vkCmdFillBuffer(cmd_buf, qvk.buf_light_stats[2].buffer, 0, qvk.buf_light_stats[2].size, 0);
	}

	vkpt_submit_command_buffer(cmd_buf, qvk.queue_graphics, (1 << qvk.device_count) - 1, 0, NULL, NULL, NULL, 0, NULL, NULL, NULL);

	vkDeviceWaitIdle(qvk.device);

	buffer_destroy(&staging_buffer);


	VkDescriptorBufferInfo buf_info = {
		.buffer = qvk.buf_world.buffer,
		.offset = 0,
		.range = bsp_mesh->num_primitives * sizeof(VboPrimitive),
	};

	VkWriteDescriptorSet output_buf_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = qvk.desc_set_vertex_buffer,
		.dstBinding = PRIMITIVE_BUFFER_BINDING_IDX,
		.dstArrayElement = VERTEX_BUFFER_WORLD,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &buf_info,
	};

	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);


	return VK_SUCCESS;
}

/**
*	@brief	Cleans up BSP mesh geometry and destroys all associated BLAS.
*	@param	bsp_mesh	Pointer to bsp_mesh_t to clean up.
*	@note	Destroys acceleration structures for all geometry types (opaque, transparent, etc.).
*	@note	Destroys acceleration structures for all embedded BSP models.
*	@note	Frees geometry storage memory.
**/
void vkpt_vertex_buffer_cleanup_bsp_mesh(bsp_mesh_t* bsp_mesh)
{
	vkpt_destroy_model_geometry(&bsp_mesh->geom_opaque);
	vkpt_destroy_model_geometry(&bsp_mesh->geom_transparent);
	vkpt_destroy_model_geometry(&bsp_mesh->geom_masked);
	vkpt_destroy_model_geometry(&bsp_mesh->geom_sky);
	vkpt_destroy_model_geometry(&bsp_mesh->geom_custom_sky);
	
	for (int i = 0; i < bsp_mesh->num_models; i++)
	{
		bsp_model_t* model = bsp_mesh->models + i;

		vkpt_destroy_model_geometry(&model->geometry);
	}
}



/**
*
*
*
*	Light Buffer Management:
*
*
*
**/
/**
*	@brief	Uploads light buffer data from staging buffer to device-local buffer.
*	@param	cmd_buf	Command buffer to record copy command into.
*	@return	VK_SUCCESS on success.
*	@note	Copies from per-frame staging buffer to shared device-local light buffer.
*	@note	Clears light statistics buffer for current frame modulo 3.
**/
VkResult
vkpt_light_buffer_upload_staging(VkCommandBuffer cmd_buf)
{
	BufferResource_t* staging = qvk.buf_light_staging + qvk.current_frame_index;

	assert(!staging->is_mapped);

	VkBufferCopy copyRegion = {
		.size = sizeof(LightBuffer),
	};
	vkCmdCopyBuffer(cmd_buf, staging->buffer, qvk.buf_light.buffer, 1, &copyRegion);

	int buffer_idx = qvk.frame_counter % 3;
	if (qvk.buf_light_stats[buffer_idx].buffer)
	{
		vkCmdFillBuffer(cmd_buf, qvk.buf_light_stats[buffer_idx].buffer, 0, qvk.buf_light_stats[buffer_idx].size, 0);
	}

	return VK_SUCCESS;
}

/**
*	@brief	Uploads IQM skeletal animation matrices from staging buffer to device-local buffer.
*	@param	cmd_buf	Command buffer to record copy command into.
*	@return	VK_SUCCESS on success.
*	@note	Copies from per-frame staging buffer to shared device-local IQM matrix buffer.
*	@note	Used for skeletal animation of IQM models.
**/
VkResult
vkpt_iqm_matrix_buffer_upload_staging(VkCommandBuffer cmd_buf)
{
	BufferResource_t* staging = qvk.buf_iqm_matrices_staging + qvk.current_frame_index;

	assert(!staging->is_mapped);

	VkBufferCopy copyRegion = {
		.size = sizeof(IqmMatrixBuffer),
	};
	vkCmdCopyBuffer(cmd_buf, staging->buffer, qvk.buf_iqm_matrices.buffer, 1, &copyRegion);
	
	return VK_SUCCESS;
}

//! Per-cluster local light counts for light buffer management.
static int local_light_counts[MAX_MAP_CLUSTERS];
//! Per-cluster total light counts (local + BSP) for light buffer management.
static int cluster_light_counts[MAX_MAP_CLUSTERS];
//! Per-cluster light list tail indices for dynamic light injection.
static int light_list_tails[MAX_MAP_CLUSTERS];
//! Maximum number of model lights added this frame.
static int max_model_lights;

/**
*	@brief	Resets per-frame light buffer statistics and counts.
*	@note	Call this at the start of each frame before building light lists.
**/
void vkpt_light_buffer_reset_counts()
{
	max_model_lights = 0;
}

/**
*	@brief	Copies BSP lights from bsp_mesh into the light buffer.
*	@param	bsp_mesh	Pointer to BSP mesh containing cluster lights.
*	@param	lbo			Pointer to light buffer to copy into.
*	@note	Copies light lists and offsets from BSP data.
*	@note	Initializes per-cluster statistics arrays.
**/
static void copy_bsp_lights(bsp_mesh_t* bsp_mesh, LightBuffer *lbo)
{
	// Copy the BSP light lists verbatim
	memcpy(lbo->light_list_lights, bsp_mesh->cluster_lights, sizeof(uint32_t) * bsp_mesh->cluster_light_offsets[bsp_mesh->num_clusters]);
	memcpy(lbo->light_list_offsets, bsp_mesh->cluster_light_offsets, sizeof(uint32_t) * (bsp_mesh->num_clusters + 1));
	// Store the light counts in the light counts history entry for the current frame
	uint history_index = qvk.frame_counter % LIGHT_COUNT_HISTORY;
	uint *sample_light_counts = (uint *)buffer_map(qvk.buf_light_counts_history + history_index);
	for (int c = 0; c < bsp_mesh->num_clusters; c++)
	{
		sample_light_counts[c] = bsp_mesh->cluster_light_offsets[c + 1] - bsp_mesh->cluster_light_offsets[c];
	}
	buffer_unmap(qvk.buf_light_counts_history + history_index);
}

/**
*	@brief	Injects dynamic model lights into per-cluster light lists using PVS.
*	@param	bsp_mesh				Pointer to BSP mesh containing cluster data.
*	@param	bsp					Pointer to BSP structure for PVS queries.
*	@param	num_model_lights		Number of dynamic model lights to inject.
*	@param	transformed_model_lights	Array of transformed model lights.
*	@param	model_light_offset		Offset into global light array where model lights start.
*	@param	lbo					Pointer to light buffer to write into.
*	@note	Uses PVS to determine which clusters can see each model light.
*	@note	Expands per-cluster light lists to include visible model lights.
*	@note	Falls back to BSP lights only if insufficient buffer space.
**/
static void
inject_model_lights(bsp_mesh_t* bsp_mesh, bsp_t* bsp, int num_model_lights, light_poly_t* transformed_model_lights, int model_light_offset, LightBuffer *lbo)
{
	uint32_t *dst_list_offsets = lbo->light_list_offsets;
	uint32_t *dst_lists = lbo->light_list_lights;

	memset(local_light_counts, 0, bsp_mesh->num_clusters * sizeof(int));
	memset(cluster_light_counts, 0, bsp_mesh->num_clusters * sizeof(int));

	// Count the number of model lights per cluster

	for (int nlight = 0; nlight < num_model_lights; nlight++)
	{
		local_light_counts[transformed_model_lights[nlight].cluster]++;
	}

	// Count the number of model lights visible from each cluster, using the PVS

	for (int c = 0; c < bsp_mesh->num_clusters; c++)
	{
		if (local_light_counts[c])
		{
			const byte* mask = BSP_GetPvs(bsp, c);

			for (int j = 0; j < bsp->visrowsize; j++) {
				if (mask[j]) {
					for (int k = 0; k < 8; ++k) {
						if (mask[j] & (1 << k))
							cluster_light_counts[j * 8 + k] += local_light_counts[c];
					}
				}
			}
		}
	}

	// Count the total required list size

	int required_size = bsp_mesh->cluster_light_offsets[bsp_mesh->num_clusters];
	for (int c = 0; c < bsp_mesh->num_clusters; c++)
	{
		required_size += cluster_light_counts[c];
	}

	// See if we have enough room in the interaction buffer
	
	if (required_size > MAX_LIGHT_LIST_NODES)
	{
		Com_WPrintf("Insufficient light interaction buffer size (%d needed). Increase MAX_LIGHT_LIST_NODES.\n", required_size);

		// Copy the BSP light lists verbatim
		copy_bsp_lights(bsp_mesh, lbo);

		return;
	}
	
	// Store the light counts in the light counts history entry for the current frame
	uint history_index = qvk.frame_counter % LIGHT_COUNT_HISTORY;
	uint *sample_light_counts = (uint *)buffer_map(qvk.buf_light_counts_history + history_index);

	// Copy the static light lists, and make room in these lists to inject the model lights

	int tail = 0;
	for (int c = 0; c < bsp_mesh->num_clusters; c++)
	{
		int original_size = bsp_mesh->cluster_light_offsets[c + 1] - bsp_mesh->cluster_light_offsets[c];

		dst_list_offsets[c] = tail;
		memcpy(dst_lists + tail, bsp_mesh->cluster_lights + bsp_mesh->cluster_light_offsets[c], sizeof(uint32_t) * original_size);
		tail += original_size;
		
		assert(tail + cluster_light_counts[c] < MAX_LIGHT_LIST_NODES);
		
		light_list_tails[c] = tail;
		tail += cluster_light_counts[c];

		sample_light_counts[c] = original_size + cluster_light_counts[c];
	}
	dst_list_offsets[bsp_mesh->num_clusters] = tail;

	buffer_unmap(qvk.buf_light_counts_history + history_index);

	// Write the model light indices into the light lists

	for (int nlight = 0; nlight < num_model_lights; nlight++)
	{
		const byte* mask = BSP_GetPvs(bsp, transformed_model_lights[nlight].cluster);

		for (int j = 0; j < bsp->visrowsize; j++) {
			if (mask[j]) {
				for (int k = 0; k < 8; ++k) {
					if (mask[j] & (1 << k))
					{
						int other_cluster = j * 8 + k;
						int list_index = light_list_tails[other_cluster]++;
						// assert we're not writing into the space reserved for following cluster
						assert(list_index < dst_list_offsets[other_cluster + 1]);
						dst_lists[list_index] = model_light_offset + nlight;
					}
				}
			}
		}
	}

#if defined(USE_DEBUG)
	// Verify tight packing
	for (int c = 0; c < bsp_mesh->num_clusters; c++)
	{
		int list_start = dst_list_offsets[c];
		int list_end = dst_list_offsets[c + 1];
		int original_size = bsp_mesh->cluster_light_offsets[c + 1] - bsp_mesh->cluster_light_offsets[c];
		assert(list_end - list_start == original_size + cluster_light_counts[c]);
	}
#endif
}

/**
*	@brief	Copies a single light polygon into the light buffer with style and material scaling.
*	@param	light			Pointer to source light_poly_t.
*	@param	vblight			Pointer to destination in light buffer (float array).
*	@param	sky_radiance	Sky radiance values for sky lights.
*	@note	Applies light style scaling (animated lights, flickering, etc.).
*	@note	Applies material emissive factor scaling.
*	@note	Sky lights (negative color) use sky_radiance instead.
**/
static inline void
copy_light(const light_poly_t* light, float* vblight, const float* sky_radiance)
{
	float style_scale = 1.f;
	float prev_style = 1.f;
	if (light->style != 0 && vkpt_refdef.fd->lightstyles)
	{
		style_scale = vkpt_refdef.fd->lightstyles[light->style].white;
		style_scale = max(0.f, min(2.f, style_scale));

		prev_style = vkpt_refdef.prev_lightstyles[light->style].white;
		prev_style = max(0.f, min(2.f, prev_style));
	}

	float mat_scale = light->material ? light->material->emissive_factor : 1.f;

	VectorCopy(light->positions + 0, vblight + 0);
	VectorCopy(light->positions + 3, vblight + 4);
	VectorCopy(light->positions + 6, vblight + 8);

	if (light->color[0] < 0.f)
	{
		vblight[3] = -sky_radiance[0] * 0.5f;
		vblight[7] = -sky_radiance[1] * 0.5f;
		vblight[11] = -sky_radiance[2] * 0.5f;
	}
	else
	{
		vblight[3] = light->color[0] * mat_scale;
		vblight[7] = light->color[1] * mat_scale;
		vblight[11] = light->color[2] * mat_scale;
	}

	vblight[12] = style_scale;
	vblight[13] = prev_style;
	vblight[14] = 0.f;
	vblight[15] = 0.f;
}

//! Debug cluster mask for PVS visualization (external variable).
extern char cluster_debug_mask[VIS_MAX_BYTES];

/**
*	@brief	Uploads light buffer data to staging buffer for GPU transfer.
*	@param	render_world			Whether to render world lights (false for menu/no-world).
*	@param	bsp_mesh				Pointer to BSP mesh with static lights.
*	@param	bsp					Pointer to BSP structure for PVS queries.
*	@param	num_model_lights		Number of dynamic model lights.
*	@param	transformed_model_lights	Array of transformed model lights.
*	@param	sky_radiance			Sky radiance values for sky lights.
*	@return	VK_SUCCESS on success.
*	@note	Combines BSP static lights with dynamic model lights.
*	@note	Uses PVS to inject model lights into per-cluster lists.
*	@note	Writes to per-frame staging buffer (indexed by current_frame_index).
**/
VkResult
vkpt_light_buffer_upload_to_staging(bool render_world, bsp_mesh_t *bsp_mesh, bsp_t* bsp, int num_model_lights, light_poly_t* transformed_model_lights, const float* sky_radiance)
{
	assert(bsp_mesh);

	BufferResource_t* staging = qvk.buf_light_staging + qvk.current_frame_index;

	LightBuffer *lbo = (LightBuffer *)buffer_map(staging);
	assert(lbo);

	if (render_world)
	{
		assert(bsp_mesh->num_clusters + 1 < MAX_LIGHT_LISTS);
		assert(bsp_mesh->num_cluster_lights < MAX_LIGHT_LIST_NODES);

		int total_lights = bsp_mesh->num_light_polys + num_model_lights;
		static bool warning_printed = false;
		if (total_lights >= MAX_LIGHT_POLYS)
		{
			if (!warning_printed)
			{
				Com_WPrintf("The map has %d light polygons, which is more than the maximum count of %d.\n"
							"Some lights were removed. Increase MAX_LIGHT_POLYS in the source code.\n",
							total_lights, MAX_LIGHT_POLYS);
				warning_printed = true;
			}
		}
		else
		{
			warning_printed = false;
		}

		int model_light_offset = bsp_mesh->num_light_polys;
		max_model_lights = max(max_model_lights, num_model_lights);

		if(max_model_lights > 0)
		{
			// If any of the BSP models contain lights, inject these lights right into the visibility lists.
			// The shader doesn't know that these lights are dynamic.

			inject_model_lights(bsp_mesh, bsp, num_model_lights, transformed_model_lights, model_light_offset, lbo);
		}
		else
		{
			copy_bsp_lights(bsp_mesh, lbo);
		}

		for (int nlight = 0; nlight < bsp_mesh->num_light_polys && nlight < MAX_LIGHT_POLYS; nlight++)
		{
			light_poly_t* light = bsp_mesh->light_polys + nlight;
			float* vblight = *(lbo->light_polys + nlight * LIGHT_POLY_VEC4S);
			copy_light(light, vblight, sky_radiance);
		}

		for (int nlight = 0; nlight < num_model_lights && nlight + model_light_offset < MAX_LIGHT_POLYS; nlight++)
		{
			light_poly_t* light = transformed_model_lights + nlight;
			float* vblight = *(lbo->light_polys + (nlight + model_light_offset) * LIGHT_POLY_VEC4S);
			copy_light(light, vblight, sky_radiance);
		}
	}
	else
	{
		lbo->light_list_offsets[0] = 0;
		lbo->light_list_offsets[1] = 0;
	}

	/* effects.c declares this - hence the assert below:
		typedef struct clightstyle_s {
			...
			float   map[MAX_QPATH];
		} clightstyle_t;
	*/

	// WID: BSPX: LMSTYLE:
	//assert(MAX_LIGHT_STYLES == MAX_QPATH);
	assert(MAX_LIGHT_STYLES == 256); // used to be, MAX_OSPATH but it is now: MAX_OSPATH = 260(Windows) or 4096(Unix)
	for (int nstyle = 0; nstyle < MAX_LIGHT_STYLES; nstyle++)
	{
		float style_scale = 1.f;
		if (vkpt_refdef.fd->lightstyles)
		{
			style_scale = vkpt_refdef.fd->lightstyles[nstyle].white;
			style_scale = max(0, min(2.f, style_scale));
		}
		lbo->light_styles[nstyle] = style_scale;
	}

	// materials
	
	for (int nmat = 0; nmat < MAX_PBR_MATERIALS; nmat++)
	{
		pbr_material_t const * material = r_materials + nmat;
		
		uint32_t* mat_data = lbo->material_table + nmat * MATERIAL_UINTS;
		memset(mat_data, 0, sizeof(uint32_t) * MATERIAL_UINTS);

		if (material->registration_sequence == 0)
			continue;

		if (material->image_base) mat_data[0] |= (material->image_base - r_images);
		if (material->image_normals) mat_data[0] |= (material->image_normals - r_images) << 16;
		if (material->image_emissive) mat_data[1] |= (material->image_emissive - r_images);
		if (material->image_mask) mat_data[1] |= (material->image_mask - r_images) << 16;
		
		mat_data[2] = floatToHalf(material->bump_scale);
		mat_data[2] |= floatToHalf(material->roughness_override) << 16;
		mat_data[3] = floatToHalf(material->metalness_factor);
		mat_data[3] |= floatToHalf(material->emissive_factor) << 16;
		mat_data[4] |= (material->num_frames & 0xffff);
		mat_data[4] |= (material->next_frame & 0xffff) << 16;
		mat_data[5] = floatToHalf(material->specular_factor);
		mat_data[5] |= floatToHalf(material->base_factor) << 16;
	}

	memcpy(lbo->cluster_debug_mask, cluster_debug_mask, MAX_LIGHT_LISTS / 8);
	memcpy(lbo->sky_visibility, bsp_mesh->sky_visibility, MAX_LIGHT_LISTS / 8);

	buffer_unmap(staging);
	lbo = NULL;

	return VK_SUCCESS;
}



/**
*
*
*
*	Model VBO Management:
*
*
*
**/
/**
*	@brief	Writes a model VBO descriptor to the descriptor set.
*	@param	index	Model index (offset from VERTEX_BUFFER_FIRST_MODEL).
*	@param	buffer	VkBuffer containing the model geometry.
*	@param	size	Size of the buffer in bytes.
*	@note	Updates the PRIMITIVE_BUFFER_BINDING_IDX descriptor array element.
**/
static void write_model_vbo_descriptor(int index, VkBuffer buffer, VkDeviceSize size)
{
	VkDescriptorBufferInfo descriptor_buffer_info = {
		.buffer = buffer,
		.offset = 0,
		.range = size,
	};

	VkWriteDescriptorSet write_descriptor_set = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = qvk.desc_set_vertex_buffer,
		.dstBinding = 0,
		.dstArrayElement = VERTEX_BUFFER_FIRST_MODEL + index,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &descriptor_buffer_info,
	};

	vkUpdateDescriptorSets(qvk.device, 1, &write_descriptor_set, 0, NULL);
}

/**
*	@brief	Destroys a model VBO and frees all associated resources.
*	@param	vbo	Pointer to model_vbo_t to destroy.
*	@note	Destroys all geometry (opaque, transparent, masked) and their BLAS.
*	@note	Destroys the buffer and resets the structure to zero.
**/
static void destroy_model_vbo(model_vbo_t* vbo)
{
	vkpt_destroy_model_geometry(&vbo->geom_opaque);
	vkpt_destroy_model_geometry(&vbo->geom_transparent);
	vkpt_destroy_model_geometry(&vbo->geom_masked);

	buffer_destroy(&vbo->buffer);
	
	memset(vbo, 0, sizeof(model_vbo_t));
}

/**
*	@brief	Stages (copies) mesh primitive data into staging buffer for upload.
*	@param	staging_data		Pointer to staging buffer memory.
*	@param	p_write_ptr			Pointer to write offset (in primitives), updated on return.
*	@param	p_vertex_write_ptr	Pointer to vertex write position (float*), updated on return (can be NULL).
*	@param	model				Pointer to model structure.
*	@param	m					Pointer to mesh within the model.
*	@note	Copies all frames of the mesh (for animation).
*	@note	Extracts vertex positions, normals, tangents, and texture coordinates.
*	@note	Computes material indices from skin pointers.
**/
static void
stage_mesh_primitives(uint8_t* staging_data, int* p_write_ptr, float** p_vertex_write_ptr, const model_t* model, const maliasmesh_t* m)
{
	int write_ptr = *p_write_ptr;
	float* vertex_write_ptr = *p_vertex_write_ptr;

	for (int frame = 0; frame < model->numframes; frame++)
	{
		for (int tri = 0; tri < m->numtris; tri++)
		{
			VboPrimitive* dst = (VboPrimitive*)staging_data + write_ptr;

			int i0 = m->indices[tri * 3 + 0];
			int i1 = m->indices[tri * 3 + 1];
			int i2 = m->indices[tri * 3 + 2];

			i0 += frame * m->numverts;
			i1 += frame * m->numverts;
			i2 += frame * m->numverts;

			VectorCopy(m->positions[i0], dst->pos0);
			VectorCopy(m->positions[i1], dst->pos1);
			VectorCopy(m->positions[i2], dst->pos2);

			if (vertex_write_ptr)
			{
				VectorCopy(m->positions[i0], vertex_write_ptr); vertex_write_ptr += 3;
				VectorCopy(m->positions[i1], vertex_write_ptr); vertex_write_ptr += 3;
				VectorCopy(m->positions[i2], vertex_write_ptr); vertex_write_ptr += 3;
			}

			dst->normals[0] = encode_normal(m->normals[i0]);
			dst->normals[1] = encode_normal(m->normals[i1]);
			dst->normals[2] = encode_normal(m->normals[i2]);
			
			dst->tangents[0] = encode_normal(m->tangents[i0]);
			dst->tangents[1] = encode_normal(m->tangents[i1]);
			dst->tangents[2] = encode_normal(m->tangents[i2]);

			dst->uv0[0] = m->tex_coords[i0][0];
			dst->uv0[1] = m->tex_coords[i0][1];
			dst->uv1[0] = m->tex_coords[i1][0];
			dst->uv1[1] = m->tex_coords[i1][1];
			dst->uv2[0] = m->tex_coords[i2][0];
			dst->uv2[1] = m->tex_coords[i2][1];

			if (m->blend_indices && m->blend_weights)
			{
				dst->custom0[0] = m->blend_indices[i0];
				dst->custom0[1] = m->blend_weights[i0];
				dst->custom1[0] = m->blend_indices[i1];
				dst->custom1[1] = m->blend_weights[i1];
				dst->custom2[0] = m->blend_indices[i2];
				dst->custom2[1] = m->blend_weights[i2];
			}

			dst->emissive_and_alpha = 0x3c003c00; // (1.0f, 1.0f)
			dst->cluster = -1;

			++write_ptr;
		}
	}
	
	*p_write_ptr = write_ptr;
	*p_vertex_write_ptr = vertex_write_ptr;

#if 0
	for (int j = 0; j < num_verts; j++)
		Com_Printf("%f %f %f\n",
			m->positions[j][0],
			m->positions[j][1],
			m->positions[j][2]);

	for (int j = 0; j < m->numtris; j++)
		Com_Printf("%d %d %d\n",
			m->indices[j * 3 + 0],
			m->indices[j * 3 + 1],
			m->indices[j * 3 + 2]);
#endif

#if 0
	char buf[1024];
	snprintf(buf, sizeof buf, "model_%04d.obj", i);
	FILE* f = fopen(buf, "wb+");
	assert(f);
	for (int j = 0; j < m->numverts; j++) {
		fprintf(f, "v %f %f %f\n",
			m->positions[j][0],
			m->positions[j][1],
			m->positions[j][2]);
	}
	for (int j = 0; j < m->numindices / 3; j++) {
		fprintf(f, "f %d %d %d\n",
			m->indices[j * 3 + 0] + 1,
			m->indices[j * 3 + 1] + 1,
			m->indices[j * 3 + 2] + 1);
	}
	fclose(f);
#endif
}

/**
*	@brief	Invalidates static model VBOs that use a specific material.
*	@param	material_index	Index of the material that changed.
*	@note	Destroys VBOs for static models using this material so they will be rebuilt.
*	@note	Does not affect animated models (they don't have prebuilt BLAS).
*	@note	Waits for device idle before destroying VBOs.
**/
void vkpt_vertex_buffer_invalidate_static_model_vbos(int material_index)
{
	vkDeviceWaitIdle(qvk.device);

	pbr_material_t* mat = MAT_ForIndex(material_index);

	for (int i = 0; i < MAX_MODELS; i++)
	{
		const model_t* model = &r_models[i];
		model_vbo_t* vbo = model_vertex_data + i;

		// Only look at valid static meshes.
		// Animated meshes don't need to be updated when their mateirals change because
		// they don't have prebuilt and pre-categorized BLAS.
		if (model->meshes && vbo->is_static)
		{
			// Look for the material being used in any of the meshes of this model
			bool found = false;
			for (int i_mesh = 0; i_mesh < model->nummeshes; i_mesh++)
			{
				maliasmesh_t* mesh = model->meshes + i_mesh;

				if (mesh->materials[0] == mat)
				{
					found = true;
					break;
				}
			}

			// Invalidate and later re-upload the VBO if the material is used
			if (found)
			{
				write_model_vbo_descriptor(i, null_buffer.buffer, null_buffer.size);
				destroy_model_vbo(vbo);
			}
		}
	}
}

/**
*	@brief	Uploads all loaded models to GPU and builds their BLAS.
*	@return	VK_SUCCESS on success, or Vulkan error code on failure.
*	@note	Only uploads models that have meshes and don't already have VBOs.
*	@note	Categorizes geometry into opaque, transparent, and masked based on materials.
*	@note	Static models get prebuilt BLAS; animated models are uploaded but BLAS is rebuilt per-frame.
*	@note	Waits for device idle before and after upload to ensure synchronization.
**/
VkResult
vkpt_vertex_buffer_upload_models()
{
	bool any_models_to_upload = false;

	for(int i = 0; i < MAX_MODELS; i++)
	{
		const model_t* model = &r_models[i];
		model_vbo_t* vbo = model_vertex_data + i;

		if (!model->meshes && vbo->buffer.buffer) {
			// model unloaded, destroy the VBO
			write_model_vbo_descriptor(i, null_buffer.buffer, null_buffer.size);
			destroy_model_vbo(vbo);
			//Com_Printf("Unloaded model[%d]\n", i);
			continue;
		}

		if(!model->meshes) {
			// model does not exist
			continue;
		}

		if (model->registration_sequence <= vbo->registration_sequence && vbo->buffer.buffer) {
			// VBO is valid, nothing to do
			continue;
		}

		// Destroy the old buffers if they exist.
		// This may happen when a model is unloaded and then another model
		// is loaded in the same slot when changing a map.
		destroy_model_vbo(vbo);

		memset(vbo, 0, sizeof(model_vbo_t));

        assert(model->numframes > 0);

		bool model_is_static = model->numframes == 1 && (!model->skmData || !model->skmData->blend_indices);
		vbo->is_static = model_is_static;
		vbo->total_tris = 0;

		if (model_is_static)
		{
			// Count the geometries of all supported kinds

			uint32_t geom_count_opaque = 0;
			uint32_t geom_count_transparent = 0;
			uint32_t geom_count_masked = 0;

			for (int nmesh = 0; nmesh < model->nummeshes; nmesh++)
			{
				maliasmesh_t* m = model->meshes + nmesh;

				if (MAT_IsTransparent(m->materials[0]->flags))
				{
					++geom_count_transparent;
				}
				else if (MAT_IsMasked(m->materials[0]->flags))
				{
					++geom_count_masked;
				}
				else
				{
					++geom_count_opaque;
				}
			}

			vkpt_init_model_geometry(&vbo->geom_opaque, geom_count_opaque);
			vkpt_init_model_geometry(&vbo->geom_transparent, geom_count_transparent);
			vkpt_init_model_geometry(&vbo->geom_masked, geom_count_masked);
		}

		// Count the triangles and create the geometry descriptors for static geometries.
		// *Note*: the descriptor creation depends on the running value of vbo->total_tris.
		for (int nmesh = 0; nmesh < model->nummeshes; nmesh++)
		{
			maliasmesh_t *m = model->meshes + nmesh;

			if (model_is_static)
			{
				if (MAT_IsTransparent(m->materials[0]->flags))
				{
					vkpt_append_model_geometry(&vbo->geom_transparent, m->numtris, vbo->total_tris, model->name);
				}
				else if (MAT_IsMasked(m->materials[0]->flags))
				{
					vkpt_append_model_geometry(&vbo->geom_masked, m->numtris, vbo->total_tris, model->name);
				}
				else
				{
					vkpt_append_model_geometry(&vbo->geom_opaque, m->numtris, vbo->total_tris, model->name);
				}
			}

			vbo->total_tris += m->numtris * model->numframes;
		}

		vbo->vertex_data_offset = 0;

		size_t vbo_size = vbo->total_tris * sizeof(VboPrimitive);
		size_t staging_size = vbo_size;
		
		if (model_is_static)
		{
			vbo->vertex_data_offset = vbo_size;
			vbo_size += vbo->total_tris * sizeof(vec3) * 3;
			staging_size = vbo_size;

			suballocate_model_blas_memory(&vbo->geom_opaque, &vbo_size, model->name);
			suballocate_model_blas_memory(&vbo->geom_masked, &vbo_size, model->name);
			suballocate_model_blas_memory(&vbo->geom_transparent, &vbo_size, model->name);
		}

		const VkBufferUsageFlags accel_usage = 
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		buffer_create(&vbo->buffer, vbo_size, 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			(model_is_static ? accel_usage : 0),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		buffer_create(&vbo->staging_buffer, staging_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		ATTACH_LABEL_VARIABLE_NAME(vbo->buffer.buffer, BUFFER, model->name);
		ATTACH_LABEL_VARIABLE_NAME(vbo->buffer.memory, DEVICE_MEMORY, model->name);

		if (model_is_static)
		{
			char name[MAX_QPATH + 16];

			Q_snprintf(name, sizeof(name), "%s:opaque", model->name);
			create_model_blas(&vbo->geom_opaque, vbo->buffer.buffer, name);

			Q_snprintf(name, sizeof(name), "%s:masked", model->name);
			create_model_blas(&vbo->geom_masked, vbo->buffer.buffer, name);

			Q_snprintf(name, sizeof(name), "%s:transparent", model->name);
			create_model_blas(&vbo->geom_transparent, vbo->buffer.buffer, name);
		}

		uint8_t* staging_data = buffer_map(&vbo->staging_buffer);
		memset(staging_data, 0, vbo->staging_buffer.size);
		int write_ptr = 0;
		float* vertex_write_ptr = model_is_static ? (float*)(staging_data + vbo->vertex_data_offset) : NULL;
		
		for (int nmesh = 0; nmesh < model->nummeshes; nmesh++)
		{
			maliasmesh_t* m = model->meshes + nmesh;
			
			m->tri_offset = write_ptr;

			stage_mesh_primitives(staging_data, &write_ptr, &vertex_write_ptr, model, m);
		}

		buffer_unmap(&vbo->staging_buffer);

		vbo->registration_sequence = model->registration_sequence;
		any_models_to_upload = true;
	}

	if (any_models_to_upload)
	{
		VkCommandBuffer cmd_buf = vkpt_begin_command_buffer(&qvk.cmd_buffers_graphics);

		for (int i = 0; i < MAX_MODELS; i++)
		{
			model_vbo_t* vbo = model_vertex_data + i;

			if (vbo->staging_buffer.buffer)
			{
				VkBufferCopy copyRegion = {
					.size = vbo->staging_buffer.size
				};

				vkCmdCopyBuffer(cmd_buf, vbo->staging_buffer.buffer, vbo->buffer.buffer, 1, &copyRegion);

				if (vbo->is_static)
				{
					BUFFER_BARRIER(cmd_buf,
						.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
						.buffer = vbo->buffer.buffer,
						.offset = 0,
						.size = VK_WHOLE_SIZE);
				}

				build_model_blas(cmd_buf, &vbo->geom_opaque, vbo->vertex_data_offset, &vbo->buffer);
				build_model_blas(cmd_buf, &vbo->geom_transparent, vbo->vertex_data_offset, &vbo->buffer);
				build_model_blas(cmd_buf, &vbo->geom_masked, vbo->vertex_data_offset, &vbo->buffer);

				vbo->geom_opaque.instance_mask = AS_FLAG_OPAQUE;
				vbo->geom_opaque.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
				vbo->geom_opaque.sbt_offset = SBTO_OPAQUE;

				vbo->geom_transparent.instance_mask = AS_FLAG_TRANSPARENT;
				vbo->geom_transparent.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
				vbo->geom_transparent.sbt_offset = SBTO_OPAQUE;

				vbo->geom_masked.instance_mask = AS_FLAG_OPAQUE;
				vbo->geom_masked.instance_flags = VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR | VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
				vbo->geom_masked.sbt_offset = SBTO_MASKED;

			}
		}

		vkpt_submit_command_buffer_simple(cmd_buf, qvk.queue_graphics, true);
		vkQueueWaitIdle(qvk.queue_graphics);

		for (int i = 0; i < MAX_MODELS; i++)
		{
			model_vbo_t* vbo = model_vertex_data + i;

			if (vbo->staging_buffer.buffer)
			{
				buffer_destroy(&vbo->staging_buffer);

				// Just uploaded - create the descriptor, but after vkQueueWaitIdle:
				// otherwise, the descriptor set might be still in use by in-flight shaders.
				write_model_vbo_descriptor(i, vbo->buffer.buffer, vbo->buffer.size);
			}
		}
	}

	return VK_SUCCESS;
}



/**
*
*
*
*	Animated Primitive Buffer Management:
*
*
*
**/
/**
*	@brief	Creates the animated primitive buffers (primitives and positions).
*	@note	Reads size from cvar_pt_primbuf, clamped to [PRIMBUF_SIZE_MIN, PRIMBUF_SIZE_MAX].
*	@note	Creates two device-local buffers: buf_primitive_instanced and buf_positions_instanced.
*	@note	Updates descriptor sets to bind the new buffers.
**/
void create_primbuf(void)
{
	int primbuf_size = Cvar_ClampInteger(cvar_pt_primbuf, PRIMBUF_SIZE_MIN, PRIMBUF_SIZE_MAX);

	buffer_create(&qvk.buf_primitive_instanced, sizeof(VboPrimitive) * primbuf_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_create(&qvk.buf_positions_instanced, sizeof(prim_positions_t) * primbuf_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDescriptorBufferInfo buf_info = { 0 };

	VkWriteDescriptorSet output_buf_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = qvk.desc_set_vertex_buffer,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &buf_info,
	};
	
	output_buf_write.dstBinding = PRIMITIVE_BUFFER_BINDING_IDX;
	output_buf_write.dstArrayElement = VERTEX_BUFFER_INSTANCED;
	buf_info.buffer = qvk.buf_primitive_instanced.buffer;
	buf_info.range = qvk.buf_primitive_instanced.size;
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	output_buf_write.dstBinding = POSITION_BUFFER_BINDING_IDX;
	output_buf_write.dstArrayElement = 0;
	buf_info.buffer = qvk.buf_positions_instanced.buffer;
	buf_info.range = qvk.buf_positions_instanced.size;
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	current_primbuf_size = primbuf_size;
}

/**
*	@brief	Destroys the animated primitive buffers.
*	@note	Destroys buf_primitive_instanced and buf_positions_instanced.
**/
void destroy_primbuf(void)
{
	buffer_destroy(&qvk.buf_primitive_instanced);
	buffer_destroy(&qvk.buf_positions_instanced);
}

/**
*	@brief	Ensures the animated primitive buffer is large enough for the given primitive count.
*	@param	prim_count	Required number of primitives.
*	@note	If current buffer is too small, destroys and recreates it at a larger size.
*	@note	Waits for device idle before recreating buffer.
*	@note	Prints a warning suggesting the user increase pt_primbuf cvar to avoid stutter.
**/
void vkpt_vertex_buffer_ensure_primbuf_size(uint32_t prim_count)
{
	if (prim_count <= current_primbuf_size)
		return;
	
	vkDeviceWaitIdle(qvk.device);

	destroy_primbuf();

	prim_count = (uint32_t)align(prim_count, PRIMBUF_SIZE_MIN);
	Cvar_SetInteger(cvar_pt_primbuf, (int)prim_count, FROM_CODE);

	Com_DPrintf("Resizing the animation buffers to fit all meshes. Set pt_primbuf to at least %d to avoid this.\n", prim_count);

	create_primbuf();
}



/**
*
*
*
*	Vertex Buffer System Initialization and Cleanup:
*
*
*
**/
/**
*	@brief	Creates the vertex buffer system: descriptor sets, buffers, and resources.
*	@return	VK_SUCCESS on success, or Vulkan error code on failure.
*	@note	Initializes cvar_pt_primbuf for configuring animated primitive buffer size.
*	@note	Creates descriptor set layout and pool for vertex buffer bindings.
*	@note	Allocates and binds all buffer resources (light, IQM matrices, readback, etc.).
*	@note	Creates null buffer placeholder for uninitialized model VBOs.
*	@note	Initializes animated primitive buffers via create_primbuf().
**/
VkResult
vkpt_vertex_buffer_create()
{
	char primbuf_initial_value[16];
	Q_snprintf(primbuf_initial_value, sizeof(primbuf_initial_value), "%d", PRIMBUF_SIZE_DEFAULT);
	cvar_pt_primbuf = Cvar_Get("pt_primbuf", primbuf_initial_value, CVAR_ARCHIVE);

	VkDescriptorSetLayoutBinding vbo_layout_bindings[] = {
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = VERTEX_BUFFER_FIRST_MODEL + MAX_MODELS,
			.binding = PRIMITIVE_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.binding = POSITION_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.binding = LIGHT_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = LIGHT_COUNT_HISTORY,
			.binding = LIGHT_COUNTS_HISTORY_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.binding = IQM_MATRIX_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.binding = READBACK_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.binding = TONE_MAPPING_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.binding = SUN_COLOR_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.binding = SUN_COLOR_UBO_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		},
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 3,
			.binding = LIGHT_STATS_BUFFER_BINDING_IDX,
			.stageFlags = VK_SHADER_STAGE_ALL,
		}
	};

	VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = LENGTH(vbo_layout_bindings),
		.pBindings    = vbo_layout_bindings,
	};

	_VK(vkCreateDescriptorSetLayout(qvk.device, &layout_info, NULL, &qvk.desc_set_layout_vertex_buffer));
	
	buffer_create(&qvk.buf_light, sizeof(LightBuffer),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
	{
		buffer_create(qvk.buf_light_staging + frame, sizeof(LightBuffer),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	buffer_create(&qvk.buf_readback, sizeof(ReadbackBuffer),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_create(&qvk.buf_iqm_matrices, sizeof(IqmMatrixBuffer),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
	{
		buffer_create(qvk.buf_iqm_matrices_staging + frame, sizeof(IqmMatrixBuffer),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	qvk.iqm_matrices_shadow = Z_Mallocz(sizeof(IqmMatrixBuffer));
	qvk.iqm_matrices_prev = Z_Mallocz(sizeof(IqmMatrixBuffer));

	buffer_create(&qvk.buf_tonemap, sizeof(ToneMappingBuffer),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_create(&qvk.buf_sun_color, sizeof(SunColorBuffer),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
	{
		buffer_create(qvk.buf_readback_staging + frame, sizeof(ReadbackBuffer),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	buffer_create(&null_buffer, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDescriptorPoolSize pool_size = {
		.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = LENGTH(vbo_layout_bindings) + MAX_MODELS + 128,
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 1,
		.pPoolSizes    = &pool_size,
		.maxSets       = 2,
	};

	_VK(vkCreateDescriptorPool(qvk.device, &pool_info, NULL, &desc_pool_vertex_buffer));

	VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool     = desc_pool_vertex_buffer,
		.descriptorSetCount = 1,
		.pSetLayouts        = &qvk.desc_set_layout_vertex_buffer,
	};

	_VK(vkAllocateDescriptorSets(qvk.device, &descriptor_set_alloc_info, &qvk.desc_set_vertex_buffer));

	VkDescriptorBufferInfo buf_info = {
		.buffer = null_buffer.buffer,
		.offset = 0,
		.range  = null_buffer.size,
	};

	VkWriteDescriptorSet output_buf_write = {
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = qvk.desc_set_vertex_buffer,
		.dstBinding      = PRIMITIVE_BUFFER_BINDING_IDX,
		.dstArrayElement = VERTEX_BUFFER_WORLD,
		.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo     = &buf_info,
	};

	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);
	
	output_buf_write.dstBinding = LIGHT_BUFFER_BINDING_IDX;
	output_buf_write.dstArrayElement = 0;
	buf_info.buffer = qvk.buf_light.buffer;
	buf_info.range = sizeof(LightBuffer);
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	output_buf_write.dstBinding = IQM_MATRIX_BUFFER_BINDING_IDX;
	buf_info.buffer = qvk.buf_iqm_matrices.buffer;
	buf_info.range = sizeof(IqmMatrixBuffer);
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	output_buf_write.dstBinding = READBACK_BUFFER_BINDING_IDX;
	buf_info.buffer = qvk.buf_readback.buffer;
	buf_info.range = sizeof(ReadbackBuffer);
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	output_buf_write.dstBinding = TONE_MAPPING_BUFFER_BINDING_IDX;
	buf_info.buffer = qvk.buf_tonemap.buffer;
	buf_info.range = sizeof(ToneMappingBuffer);
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	output_buf_write.dstBinding = SUN_COLOR_BUFFER_BINDING_IDX;
	buf_info.buffer = qvk.buf_sun_color.buffer;
	buf_info.range = sizeof(SunColorBuffer);
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	output_buf_write.dstBinding = SUN_COLOR_UBO_BINDING_IDX;
	output_buf_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	buf_info.buffer = qvk.buf_sun_color.buffer;
	buf_info.range = sizeof(SunColorBuffer);
	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	create_primbuf();
	
	memset(model_vertex_data, 0, sizeof(model_vertex_data));

	for (int i = 0; i < MAX_MODELS; i++)
	{
		write_model_vbo_descriptor(i, null_buffer.buffer, null_buffer.size);
	}

	return VK_SUCCESS;
}

/**
*	@brief	Reads back GPU data from the readback buffer into CPU memory.
*	@param	dst	Pointer to ReadbackBuffer structure to receive data.
*	@return	VK_SUCCESS on success, or VK_ERROR_MEMORY_MAP_FAILED if mapping fails.
*	@note	Maps the readback staging buffer for the current frame and copies data.
*	@note	Used for debugging and UI display of GPU-computed values.
**/
VkResult
vkpt_readback(ReadbackBuffer* dst)
{
	BufferResource_t* buffer = &qvk.buf_readback_staging[qvk.current_frame_index];
	void* mapped = buffer_map(buffer);

	if (mapped == NULL)
		return VK_ERROR_MEMORY_MAP_FAILED;

	memcpy(dst, mapped, sizeof(ReadbackBuffer));

	buffer_unmap(buffer);

	return VK_SUCCESS;
}

/**
*	@brief	Destroys the vertex buffer system and frees all resources.
*	@return	VK_SUCCESS on success.
*	@note	Destroys descriptor pool and layout.
*	@note	Destroys all model VBOs and animated primitive buffers.
*	@note	Destroys all buffer resources (world, light, IQM matrices, readback, etc.).
*	@note	Should be called during engine shutdown or before reinitializing the system.
**/
VkResult
vkpt_vertex_buffer_destroy()
{
	vkDestroyDescriptorPool(qvk.device, desc_pool_vertex_buffer, NULL);
	vkDestroyDescriptorSetLayout(qvk.device, qvk.desc_set_layout_vertex_buffer, NULL);
	desc_pool_vertex_buffer = VK_NULL_HANDLE;
	qvk.desc_set_layout_vertex_buffer = VK_NULL_HANDLE;

	destroy_primbuf();

	for (int model = 0; model < MAX_MODELS; model++)
	{
		destroy_model_vbo(&model_vertex_data[model]);
	}

	buffer_destroy(&null_buffer);

	buffer_destroy(&qvk.buf_world);
	buffer_destroy(&qvk.buf_light);
	buffer_destroy(&qvk.buf_iqm_matrices);
	buffer_destroy(&qvk.buf_readback);
	for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
	{
		buffer_destroy(qvk.buf_light_staging + frame);
		buffer_destroy(qvk.buf_iqm_matrices_staging + frame);
		buffer_destroy(qvk.buf_readback_staging + frame);
	}

	buffer_destroy(&qvk.buf_tonemap);
	buffer_destroy(&qvk.buf_sun_color);

	Z_Freep((void**)&qvk.iqm_matrices_shadow);
	Z_Freep((void**)&qvk.iqm_matrices_prev);

	return VK_SUCCESS;
}

/**
*	@brief	Creates light buffers for storing BSP light data and statistics.
*	@param	bsp_mesh	Pointer to BSP mesh containing light polygon data.
*	@return	VK_SUCCESS on success, or Vulkan error code on failure.
*	@note	Destroys previous light buffers before creating new ones.
*	@note	Creates light statistics buffers (3 buffers for triple-buffering).
*	@note	Creates light counts history buffers for temporal light sampling.
*	@note	Updates descriptor sets to bind the new buffers.
**/
VkResult vkpt_light_buffers_create(bsp_mesh_t *bsp_mesh)
{
	vkpt_light_buffers_destroy();

	// Light statistics: 2 uints (shadowed, unshadowed) per light per surface orientation (6) per cluster.
	uint32_t num_stats = bsp_mesh->num_clusters * bsp_mesh->num_light_polys * 6 * 2;

	// Handle rare cases when the map has zero lights
	if (num_stats == 0)
		num_stats = 1;

	for (int frame = 0; frame < NUM_LIGHT_STATS_BUFFERS; frame++)
	{
		buffer_create(qvk.buf_light_stats + frame, sizeof(uint32_t) * num_stats,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	assert(NUM_LIGHT_STATS_BUFFERS == 3);

	VkDescriptorBufferInfo light_stats_buf_info[] = { {
			.buffer = qvk.buf_light_stats[0].buffer,
			.offset = 0,
			.range = qvk.buf_light_stats[0].size,
		}, {
			.buffer = qvk.buf_light_stats[1].buffer,
			.offset = 0,
			.range = qvk.buf_light_stats[1].size,
		}, {
			.buffer = qvk.buf_light_stats[2].buffer,
			.offset = 0,
			.range = qvk.buf_light_stats[2].size,
		} };

	VkWriteDescriptorSet output_buf_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = qvk.desc_set_vertex_buffer,
		.dstBinding = LIGHT_STATS_BUFFER_BINDING_IDX,
		.dstArrayElement = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = LENGTH(light_stats_buf_info),
		.pBufferInfo = light_stats_buf_info,
	};

	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	// Set up buffers for light counts history
	VkDescriptorBufferInfo light_counts_buf_info[LIGHT_COUNT_HISTORY];
	for (int h = 0; h < LIGHT_COUNT_HISTORY; h++)
	{
		buffer_create(qvk.buf_light_counts_history + h, sizeof(uint32_t) * bsp_mesh->num_clusters,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		light_counts_buf_info[h].buffer = qvk.buf_light_counts_history[h].buffer;
		light_counts_buf_info[h].offset = 0;
		light_counts_buf_info[h].range = qvk.buf_light_counts_history[h].size;
	}

	output_buf_write.dstBinding = LIGHT_COUNTS_HISTORY_BUFFER_BINDING_IDX;
	output_buf_write.descriptorCount = LENGTH(light_counts_buf_info);
	output_buf_write.pBufferInfo = light_counts_buf_info;

	vkUpdateDescriptorSets(qvk.device, 1, &output_buf_write, 0, NULL);

	return VK_SUCCESS;
}

/**
*	@brief	Destroys light buffers and frees their resources.
*	@return	VK_SUCCESS on success.
*	@note	Destroys light statistics buffers and light counts history buffers.
*	@note	Called during map change or engine shutdown.
**/
VkResult vkpt_light_buffers_destroy()
{
	for (int frame = 0; frame < NUM_LIGHT_STATS_BUFFERS; frame++)
	{
		buffer_destroy(qvk.buf_light_stats + frame);
	}

	for (int h = 0; h < LIGHT_COUNT_HISTORY; h++)
	{
		buffer_destroy(qvk.buf_light_counts_history + h);
	}

	return VK_SUCCESS;
}



/**
*
*
*
*	Compute Pipeline Management:
*
*
*
**/
/**
*	@brief	Creates compute pipelines for geometry instancing and material animation.
*	@return	VK_SUCCESS on success, or Vulkan error code on failure.
*	@note	Creates pipeline_instance_geometry for transforming dynamic primitives.
*	@note	Creates pipeline_animate_materials for updating texture coordinates.
*	@note	Creates shared pipeline layout for both pipelines.
**/
VkResult
vkpt_vertex_buffer_create_pipelines()
{
	assert(!pipeline_instance_geometry);
	assert(!pipeline_animate_materials);
	assert(!pipeline_layout_instance_geometry);

	assert(qvk.desc_set_layout_ubo);
	assert(qvk.desc_set_layout_vertex_buffer); 

	VkDescriptorSetLayout desc_set_layouts[] = {
		qvk.desc_set_layout_ubo,
		qvk.desc_set_layout_vertex_buffer
	};

	CREATE_PIPELINE_LAYOUT(qvk.device, &pipeline_layout_instance_geometry, 
		.setLayoutCount = LENGTH(desc_set_layouts),
		.pSetLayouts    = desc_set_layouts,
	);

	VkComputePipelineCreateInfo compute_pipeline_info[] = {
		{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = SHADER_STAGE(QVK_MOD_INSTANCE_GEOMETRY_COMP, VK_SHADER_STAGE_COMPUTE_BIT),
			.layout = pipeline_layout_instance_geometry
		},
		{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = SHADER_STAGE(QVK_MOD_ANIMATE_MATERIALS_COMP, VK_SHADER_STAGE_COMPUTE_BIT),
			.layout = pipeline_layout_instance_geometry
		},
	};

	VkPipeline pipelines[2];
	_VK(vkCreateComputePipelines(qvk.device, 0, LENGTH(compute_pipeline_info), compute_pipeline_info, 0, pipelines));

	pipeline_instance_geometry = pipelines[0];
	pipeline_animate_materials = pipelines[1];

	return VK_SUCCESS;
}

/**
*	@brief	Destroys compute pipelines and their layouts.
*	@return	VK_SUCCESS on success.
*	@note	Destroys pipeline_instance_geometry and pipeline_animate_materials.
*	@note	Destroys pipeline_layout_instance_geometry.
**/
VkResult
vkpt_vertex_buffer_destroy_pipelines()
{
	assert(pipeline_instance_geometry);
	assert(pipeline_animate_materials);
	assert(pipeline_layout_instance_geometry);

	vkDestroyPipeline(qvk.device, pipeline_instance_geometry, NULL);
	vkDestroyPipeline(qvk.device, pipeline_animate_materials, NULL);
	vkDestroyPipelineLayout(qvk.device, pipeline_layout_instance_geometry, NULL);

	pipeline_instance_geometry = VK_NULL_HANDLE;
	pipeline_animate_materials = VK_NULL_HANDLE;
	pipeline_layout_instance_geometry = VK_NULL_HANDLE;

	return VK_SUCCESS;
}



/**
*
*
*
*	Geometry Instancing and Material Animation:
*
*
*
**/
/**
*	@brief	Dispatches compute shaders to instance geometry and animate materials.
*	@param	cmd_buf					Command buffer to record commands into.
*	@param	num_instances			Number of geometry instances to process.
*	@param	update_world_animations	Whether to update material animations for world geometry.
*	@return	VK_SUCCESS on success.
*	@note	Runs instance_geometry compute shader to copy/transform dynamic primitives.
*	@note	Optionally runs animate_materials compute shader for scrolling/warping textures.
*	@note	Inserts buffer barrier to ensure shader writes complete before reads.
**/
VkResult
vkpt_instance_geometry(VkCommandBuffer cmd_buf, uint32_t num_instances, bool update_world_animations)
{
	VkDescriptorSet desc_sets[] = {
		qvk.desc_set_ubo,
		qvk.desc_set_vertex_buffer
	};
	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_instance_geometry);
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
		pipeline_layout_instance_geometry, 0, LENGTH(desc_sets), desc_sets, 0, 0);

	vkCmdDispatch(cmd_buf, num_instances, 1, 1);

	if (update_world_animations)
	{
		vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_animate_materials);

		const bsp_mesh_t* wm = &vkpt_refdef.bsp_mesh_world;
		uint32_t num_static_primitives = 0;
		if (wm->geom_opaque.prim_counts)      num_static_primitives += wm->geom_opaque.prim_counts[0];
		if (wm->geom_transparent.prim_counts) num_static_primitives += wm->geom_transparent.prim_counts[0];
		if (wm->geom_masked.prim_counts)      num_static_primitives += wm->geom_masked.prim_counts[0];

		if (num_static_primitives != 0)
		{
			uint num_groups = (num_static_primitives + 255) / 256;
			vkCmdDispatch(cmd_buf, num_groups, 1, 1);
		}
	}

	VkBufferMemoryBarrier barrier = {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
		.buffer              = qvk.buf_primitive_instanced.buffer,
		.size                = qvk.buf_primitive_instanced.size,
		.srcQueueFamilyIndex = qvk.queue_idx_graphics,
		.dstQueueFamilyIndex = qvk.queue_idx_graphics
	};

	vkCmdPipelineBarrier(
			cmd_buf,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, NULL,
			1, &barrier,
			0, NULL);

	return VK_SUCCESS;
}



/**
*
*
*
*	Model Query Functions:
*
*
*
**/
/**
*	@brief	Checks if a model is static (has prebuilt BLAS).
*	@param	model	Pointer to model structure.
*	@return	True if model is static, false otherwise.
*	@note	Static models have prebuilt BLAS for opaque/transparent/masked geometry.
*	@note	Animated models (MD2/IQM with skeletal animation) are not static.
**/
bool vkpt_model_is_static(const model_t* model)
{
	if (!model)
		return false;

	size_t model_index = model - r_models;
	const model_vbo_t* vbo = &model_vertex_data[model_index];

	return vbo->is_static;
}

/**
*	@brief	Gets the VBO data for a model.
*	@param	model	Pointer to model structure.
*	@return	Pointer to model_vbo_t containing VBO and BLAS data, or NULL if model is invalid.
*	@note	Returns the model_vertex_data entry corresponding to the model index.
**/
const model_vbo_t* vkpt_get_model_vbo(const model_t* model)
{
	if (!model)
		return NULL;

	size_t model_index = model - r_models;
	
	return &model_vertex_data[model_index];
}

// vim: shiftwidth=4 noexpandtab tabstop=4 cindent
