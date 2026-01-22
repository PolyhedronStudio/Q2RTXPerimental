/********************************************************************
*
*
*	VKPT Renderer: Uniform Buffer Management
*
*	Manages global uniform buffers used across all shaders in the VKPT
*	renderer. Creates host-visible staging buffers for CPU updates and
*	device-local buffers for optimal GPU access. Implements a double/
*	triple buffering scheme to avoid synchronization stalls between
*	CPU and GPU during frame rendering.
*
*	Architecture:
*	- Per-frame host staging buffers (CPU-writable, host-coherent)
*	- Single device buffer (GPU-optimal, device-local memory)
*	- Buffer contains: QVKUniformBuffer_t + InstanceBuffer
*	- Descriptor set binds device buffer to shaders
*
*	Update Flow:
*	1. CPU writes to host_uniform_buffers[frame_index]
*	2. GPU copies from host to device_uniform_buffer
*	3. Pipeline barrier ensures visibility before shader access
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

#include <assert.h>



/**
*
*
*
*	Module State:
*
*
*
**/
//! Per-frame host-visible staging buffers for CPU uniform updates.
static BufferResource_t host_uniform_buffers[MAX_FRAMES_IN_FLIGHT];
//! Device-local uniform buffer for optimal GPU access.
static BufferResource_t device_uniform_buffer;
//! Descriptor pool for uniform buffer descriptors.
static VkDescriptorPool desc_pool_ubo;
//! Minimum alignment requirement for uniform buffer offsets.
static size_t ubo_alignment = 0;



/**
*
*
*
*	Uniform Buffer Creation & Destruction:
*
*
*
**/
/**
*	@brief	Creates uniform buffer resources and descriptor sets.
*	@return	VK_SUCCESS on success, Vulkan error code on failure.
*	@note	This function allocates host staging buffers (one per frame in flight),
*			a device-local buffer, creates descriptor set layout with two bindings
*			(uniform buffer + storage buffer), and updates descriptors to point to
*			the device buffer. The buffer size is calculated to hold both the main
*			uniform data and instance buffer, properly aligned per device limits.
**/
VkResult vkpt_uniform_buffer_create() {
	// Define descriptor set layout bindings for uniform and instance buffers.
	VkDescriptorSetLayoutBinding ubo_layout_bindings[2] = { 0 };

	// Binding 0: Global uniform buffer (read-only in shaders).
	ubo_layout_bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_bindings[0].descriptorCount  = 1;
	ubo_layout_bindings[0].binding  = GLOBAL_UBO_BINDING_IDX;
	ubo_layout_bindings[0].stageFlags  = VK_SHADER_STAGE_ALL;

	// Binding 1: Instance buffer (storage buffer for dynamic instance data).
	ubo_layout_bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ubo_layout_bindings[1].descriptorCount  = 1;
	ubo_layout_bindings[1].binding  = GLOBAL_INSTANCE_BUFFER_BINDING_IDX;
	ubo_layout_bindings[1].stageFlags  = VK_SHADER_STAGE_ALL;


	// Create descriptor set layout.
	VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = LENGTH( ubo_layout_bindings ),
		.pBindings    = ubo_layout_bindings,
	};

	_VK( vkCreateDescriptorSetLayout( qvk.device, &layout_info, NULL, &qvk.desc_set_layout_ubo ) );

	// Memory property flags for staging (host) and device buffers.
	const VkMemoryPropertyFlags host_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	const VkMemoryPropertyFlags device_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// Query device properties for uniform buffer offset alignment requirement.
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties( qvk.physical_device, &properties );
	ubo_alignment = properties.limits.minUniformBufferOffsetAlignment;

	// Calculate buffer size: QVKUniformBuffer_t (aligned) + InstanceBuffer.
	const size_t buffer_size = align( sizeof( QVKUniformBuffer_t ), ubo_alignment ) + sizeof( InstanceBuffer );

	// Create per-frame host staging buffers for CPU writes.
	for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
		buffer_create( host_uniform_buffers + i, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host_memory_flags );
	}

	// Create device-local buffer for GPU reads.
	buffer_create( &device_uniform_buffer, buffer_size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		device_memory_flags );

	// Create descriptor pool for uniform buffer descriptors.
	VkDescriptorPoolSize pool_size = {
		.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 1,
		.pPoolSizes    = &pool_size,
		.maxSets       = MAX_FRAMES_IN_FLIGHT,
	};

	_VK( vkCreateDescriptorPool( qvk.device, &pool_info, NULL, &desc_pool_ubo ) );

	// Allocate descriptor set.
	VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = desc_pool_ubo,
		.descriptorSetCount = 1,
		.pSetLayouts = &qvk.desc_set_layout_ubo,
	};

	_VK( vkAllocateDescriptorSets( qvk.device, &descriptor_set_alloc_info, &qvk.desc_set_ubo ) );
	
	// Describe buffer ranges for descriptor writes.
	VkDescriptorBufferInfo buf_info = {
		.buffer = device_uniform_buffer.buffer,
		.offset = 0,
		.range  = sizeof( QVKUniformBuffer_t ),
	};

	VkDescriptorBufferInfo buf1_info = {
		.buffer = device_uniform_buffer.buffer,
		.offset = align( sizeof( QVKUniformBuffer_t ), ubo_alignment ),
		.range  = sizeof( InstanceBuffer ),
	};

	// Update descriptor set to point to device buffer regions.
	VkWriteDescriptorSet writes[2] = { 0 };

	writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	writes[0].dstSet          = qvk.desc_set_ubo,
	writes[0].dstBinding      = GLOBAL_UBO_BINDING_IDX,
	writes[0].dstArrayElement = 0,
	writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	writes[0].descriptorCount = 1,
	writes[0].pBufferInfo     = &buf_info,

	writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	writes[1].dstSet          = qvk.desc_set_ubo,
	writes[1].dstBinding      = GLOBAL_INSTANCE_BUFFER_BINDING_IDX,
	writes[1].dstArrayElement = 0,
	writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	writes[1].descriptorCount = 1,
	writes[1].pBufferInfo     = &buf1_info,

	vkUpdateDescriptorSets( qvk.device, LENGTH( writes ), writes, 0, NULL );

	return VK_SUCCESS;
}

/**
*	@brief	Destroys uniform buffer resources.
*	@return	VK_SUCCESS on success.
*	@note	Cleans up descriptor pool, descriptor set layout, and all buffer resources.
**/
VkResult vkpt_uniform_buffer_destroy() {
	vkDestroyDescriptorPool( qvk.device, desc_pool_ubo, NULL );
	vkDestroyDescriptorSetLayout( qvk.device, qvk.desc_set_layout_ubo, NULL );
	desc_pool_ubo = VK_NULL_HANDLE;
	qvk.desc_set_layout_ubo = VK_NULL_HANDLE;

	// Destroy all per-frame host buffers.
	for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
		buffer_destroy( host_uniform_buffers + i );
	}

	// Destroy device buffer.
	buffer_destroy( &device_uniform_buffer );

	return VK_SUCCESS;
}



/**
*
*
*
*	Uniform Buffer Update Operations:
*
*
*
**/
/**
*	@brief	Uploads uniform data to the host staging buffer for the current frame.
*	@return	VK_SUCCESS on success, VK_ERROR_MEMORY_MAP_FAILED if mapping fails.
*	@note	Maps the current frame's host buffer, copies QVKUniformBuffer_t and
*			InstanceBuffer data from vkpt_refdef, and unmaps. This data will be
*			copied to the device buffer in vkpt_uniform_buffer_copy_from_staging().
**/
VkResult vkpt_uniform_buffer_upload_to_staging() {
	BufferResource_t* ubo = host_uniform_buffers + qvk.current_frame_index;
	assert( ubo );
	assert( ubo->memory != VK_NULL_HANDLE );
	assert( ubo->buffer != VK_NULL_HANDLE );
	assert( qvk.current_frame_index < MAX_FRAMES_IN_FLIGHT );

	// Map host buffer for CPU write.
	QVKUniformBuffer_t* mapped_ubo = (QVKUniformBuffer_t*)buffer_map( ubo );
	assert( mapped_ubo );
	if ( !mapped_ubo ) {
		return VK_ERROR_MEMORY_MAP_FAILED;
	}

	// Copy uniform buffer data.
	memcpy( mapped_ubo, &vkpt_refdef.uniform_buffer, sizeof( QVKUniformBuffer_t ) );

	// Copy instance buffer data at aligned offset.
	const size_t offset = align( sizeof( QVKUniformBuffer_t ), ubo_alignment );
	memcpy( (uint8_t*)mapped_ubo + offset, &vkpt_refdef.uniform_instance_buffer, sizeof( InstanceBuffer ) );

	buffer_unmap( ubo );

	return VK_SUCCESS;
}

/**
*	@brief	Copies uniform data from host staging buffer to device buffer.
*	@param	command_buffer	Command buffer to record the copy operation.
*	@note	Issues a buffer copy command and a pipeline barrier to ensure the data
*			is visible to shaders before use. The barrier transitions from TRANSFER_WRITE
*			to UNIFORM_READ|SHADER_READ access.
**/
void vkpt_uniform_buffer_copy_from_staging( VkCommandBuffer command_buffer ) {
	BufferResource_t* ubo = host_uniform_buffers + qvk.current_frame_index;

	// Copy entire buffer content from host to device.
	VkBufferCopy copy = { 0 };
	copy.size = align( sizeof( QVKUniformBuffer_t ), ubo_alignment ) + sizeof( InstanceBuffer );
	vkCmdCopyBuffer( command_buffer, ubo->buffer, device_uniform_buffer.buffer, 1, &copy );

	// Ensure transfer completes before shader access.
	VkBufferMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = device_uniform_buffer.buffer,
		.size = copy.size
	};

	vkCmdPipelineBarrier( command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 1, &barrier, 0, NULL );
}
