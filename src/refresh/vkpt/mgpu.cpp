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

/********************************************************************
*
*
*	Module Name: Multi-GPU (MGPU) Image Operations
*
*	This module provides image copy and synchronization primitives for
*	Vulkan Device Groups (multi-GPU configurations). It enables efficient
*	data transfer between GPUs in a device group and ensures proper memory
*	coherency across devices.
*
*	Device Group Support:
*	  - Conditional compilation via VKPT_DEVICE_GROUPS define
*	  - Global memory barriers across all GPUs in the group
*	  - Cross-GPU image copy operations with device-local images
*	  - Automatic single-GPU optimization (barriers/copies no-op)
*
*	Functions:
*	  - vkpt_mgpu_global_barrier: Full memory barrier across all devices
*	  - vkpt_mgpu_image_copy: Copy image region between different GPUs
*	  - vkpt_image_copy: Single-GPU image copy (non-MGPU fallback)
*
*	Implementation Notes:
*	  - Uses VK_DEPENDENCY_DEVICE_GROUP_BIT for cross-device synchronization
*	  - Switches active GPU context using set_current_gpu() for device-local ops
*	  - All operations use VK_IMAGE_LAYOUT_GENERAL for simplicity
*	  - Memory barriers ensure read-after-write hazards are resolved
*
*
********************************************************************/

#include "vkpt.h"

#ifdef VKPT_DEVICE_GROUPS

/**
*
*
*
*	Multi-GPU Operations:
*
*
*
**/

/**
*	@brief	Execute a global memory barrier across all GPUs in the device group.
*
*	Ensures that all memory writes on all devices are visible to all devices
*	before subsequent operations begin. No-op on single-GPU configurations.
*
*	@param	cmd_buf		Command buffer to record barrier into.
*
*	@note	Uses VK_DEPENDENCY_DEVICE_GROUP_BIT for cross-device visibility.
**/
void vkpt_mgpu_global_barrier( VkCommandBuffer cmd_buf )
{
	if( qvk.device_count == 1 ) {
		return;
	}

	VkMemoryBarrier mem_barrier = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT
	};

	vkCmdPipelineBarrier( cmd_buf,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_DEPENDENCY_DEVICE_GROUP_BIT,
		1, &mem_barrier,
		0, nullptr,
		0, nullptr );
}

/**
*	@brief	Copy an image region between two different GPUs in the device group.
*
*	Performs a cross-device image copy by switching to the source GPU context,
*	issuing a memory barrier, then copying from source GPU's device-local image
*	to destination GPU's device-local image. Switches back to all-GPU context
*	after completion.
*
*	@param	cmd_buf			Command buffer to record copy into.
*	@param	src_image_index	Source image index in qvk.images_local array.
*	@param	dst_image_index	Destination image index in qvk.images_local array.
*	@param	src_gpu_index	Source GPU device index.
*	@param	dst_gpu_index	Destination GPU device index.
*	@param	src_offset		Source image offset (x, y).
*	@param	dst_offset		Destination image offset (x, y).
*	@param	size			Region size (width, height) to copy.
*
*	@note	Both images must be in VK_IMAGE_LAYOUT_GENERAL layout.
*	@note	Only copies mip level 0, array layer 0, color aspect.
**/
void vkpt_mgpu_image_copy(
	VkCommandBuffer cmd_buf,
	int src_image_index,
	int dst_image_index,
	int src_gpu_index,
	int dst_gpu_index,
	VkOffset2D src_offset,
	VkOffset2D dst_offset,
	VkExtent2D size )
{
	VkImageSubresourceLayers subresource_layers = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkImageCopy copy_region = {
		.srcSubresource = subresource_layers,
		.srcOffset.x = src_offset.x,
		.srcOffset.y = src_offset.y,
		.srcOffset.z = 0,
		.dstSubresource = subresource_layers,
		.dstOffset.x = dst_offset.x,
		.dstOffset.y = dst_offset.y,
		.dstOffset.z = 0,
		.extent.width = size.width,
		.extent.height = size.height,
		.extent.depth = 1
	};

	// Switch to source GPU for the copy operation
	set_current_gpu( cmd_buf, src_gpu_index );

	VkMemoryBarrier mem_barrier = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
	};

	vkCmdPipelineBarrier( cmd_buf,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0,
		1, &mem_barrier,
		0, nullptr,
		0, nullptr );

	vkCmdCopyImage( cmd_buf,
		qvk.images_local[src_gpu_index][src_image_index], VK_IMAGE_LAYOUT_GENERAL,
		qvk.images_local[dst_gpu_index][dst_image_index], VK_IMAGE_LAYOUT_GENERAL,
		1, &copy_region );

	// Restore all-GPU context
	set_current_gpu( cmd_buf, ALL_GPUS );
}
#endif

/**
*
*
*
*	Single-GPU Image Operations:
*
*
*
**/

/**
*	@brief	Copy an image region within a single GPU (non-MGPU version).
*
*	Standard image copy for single-GPU configurations or same-device copies.
*	Issues a memory barrier before the copy to ensure proper synchronization.
*
*	@param	cmd_buf			Command buffer to record copy into.
*	@param	src_image_index	Source image index in qvk.images array.
*	@param	dst_image_index	Destination image index in qvk.images array.
*	@param	src_offset		Source image offset (x, y).
*	@param	dst_offset		Destination image offset (x, y).
*	@param	size			Region size (width, height) to copy.
*
*	@note	Both images must be in VK_IMAGE_LAYOUT_GENERAL layout.
*	@note	Only copies mip level 0, array layer 0, color aspect.
**/
void vkpt_image_copy(
	VkCommandBuffer cmd_buf,
	int src_image_index,
	int dst_image_index,
	VkOffset2D src_offset,
	VkOffset2D dst_offset,
	VkExtent2D size )
{
	VkImageSubresourceLayers subresource_layers = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkImageCopy copy_region = {
		.srcSubresource = subresource_layers,
		.srcOffset.x = src_offset.x,
		.srcOffset.y = src_offset.y,
		.srcOffset.z = 0,
		.dstSubresource = subresource_layers,
		.dstOffset.x = dst_offset.x,
		.dstOffset.y = dst_offset.y,
		.dstOffset.z = 0,
		.extent.width = size.width,
		.extent.height = size.height,
		.extent.depth = 1
	};

	VkMemoryBarrier mem_barrier = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
	};

	vkCmdPipelineBarrier( cmd_buf,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0,
		1, &mem_barrier,
		0, nullptr,
		0, nullptr );

	vkCmdCopyImage( cmd_buf,
		qvk.images[src_image_index], VK_IMAGE_LAYOUT_GENERAL,
		qvk.images[dst_image_index], VK_IMAGE_LAYOUT_GENERAL,
		1, &copy_region );
}
