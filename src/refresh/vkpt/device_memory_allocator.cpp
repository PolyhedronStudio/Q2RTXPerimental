/********************************************************************
*
*
*	VKPT Renderer: Device Memory Allocator
*
*	Implements a memory allocation system for Vulkan device memory using
*	a two-tier architecture: sub-allocators manage large memory blocks,
*	and buddy allocators handle individual allocations within those blocks.
*
*	Architecture:
*	- One sub-allocator per Vulkan memory type (up to VK_MAX_MEMORY_TYPES)
*	- Each sub-allocator uses a buddy allocator for sub-allocation
*	- Default capacity: 88 MB per sub-allocator (optimized for 4K textures)
*	- Block size: 44 KB (power-of-2, optimal for texture mip chains)
*
*	Allocation Strategy:
*	1. Find or create sub-allocator for requested memory type
*	2. Attempt allocation from existing sub-allocator via buddy allocator
*	3. If full, create new sub-allocator and retry
*	4. Track total allocated and used memory for profiling
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

#include "shared/shared.h"

#include <vulkan/vulkan.h>
#include <assert.h>

#include "device_memory_allocator.h"
#include "buddy_allocator.h"

#include "vkpt.h"



/**
*
*
*
*	Memory Allocator Configuration:
*
*
*
**/
//! Block size optimized for placement of texture mip chains with up to 4 KB alignment.
#define ALLOCATOR_BLOCK_SIZE ( 44 * 1024 )

//! Power-of-2 capacity allowing placement of up to 4096x4096 RGBA8 textures with mip chains.
#define ALLOCATOR_CAPACITY ( ALLOCATOR_BLOCK_SIZE * 2048 )



/**
*
*
*
*	Internal Data Structures:
*
*
*
**/
/**
*	@brief	Sub-allocator managing a single Vulkan device memory allocation.
*	@note	Each sub-allocator uses a buddy allocator to manage sub-allocations
*			within its memory block. Multiple sub-allocators can exist per memory
*			type, forming a linked list.
**/
typedef struct SubAllocator {
	//! Vulkan device memory handle.
	VkDeviceMemory memory;
	//! Buddy allocator for managing sub-allocations within this memory.
	BuddyAllocator* buddy_allocator;
	//! Total bytes currently allocated from this sub-allocator.
	size_t memory_used;
	//! Next sub-allocator in the linked list (for same memory type).
	struct SubAllocator* next;
} SubAllocator;

/**
*	@brief	Top-level device memory allocator.
*	@note	Manages sub-allocators for all Vulkan memory types. Tracks global
*			memory usage statistics.
**/
typedef struct DeviceMemoryAllocator {
	//! Sub-allocator linked lists, one per Vulkan memory type.
	SubAllocator* sub_allocators[VK_MAX_MEMORY_TYPES];
	//! Vulkan logical device handle.
	VkDevice device;
	//! Total bytes allocated from the driver.
	size_t total_memory_allocated;
	//! Total bytes actually used by allocations.
	size_t total_memory_used;
} DeviceMemoryAllocator;



/**
*
*
*
*	Forward Declarations:
*
*
*
**/
int create_sub_allocator( DeviceMemoryAllocator* allocator, uint32_t memory_type, uint32_t alignment );



/**
*
*
*
*	Allocator Creation & Destruction:
*
*
*
**/
/**
*	@brief	Creates a new device memory allocator.
*	@param	device	Vulkan logical device.
*	@return	Pointer to newly created allocator, or NULL on failure.
*	@note	Allocates and zero-initializes the allocator structure.
**/
DeviceMemoryAllocator* create_device_memory_allocator( VkDevice device ) {
	char* memory = (char*)Z_Mallocz( sizeof( DeviceMemoryAllocator ) );

	DeviceMemoryAllocator* allocator = (DeviceMemoryAllocator*)memory;
	allocator->device = device;

	return allocator;
}

DMAResult allocate_device_memory(DeviceMemoryAllocator* allocator, DeviceMemory* device_memory)
{
	const uint32_t memory_type = device_memory->memory_type;
	if (allocator->sub_allocators[memory_type] == NULL)
	{
		if (!create_sub_allocator(allocator, memory_type, device_memory->alignment))
			return DMA_NOT_ENOUGH_MEMORY;
	}

	assert(device_memory->size <= ALLOCATOR_CAPACITY);

	BAResult result = BA_NOT_ENOUGH_MEMORY;
	SubAllocator* sub_allocator = allocator->sub_allocators[memory_type];

	while (result != BA_SUCCESS)
	{
		result = buddy_allocator_allocate(sub_allocator->buddy_allocator, device_memory->size,
			device_memory->alignment, &device_memory->memory_offset);

		if (result != BA_SUCCESS)
		{
			if (sub_allocator->next != NULL)
			{
				sub_allocator = sub_allocator->next;
			}
			else
			{
				if (!create_sub_allocator(allocator, memory_type, device_memory->alignment))
				{
					device_memory->memory = VK_NULL_HANDLE;
					return DMA_NOT_ENOUGH_MEMORY;
				}

				sub_allocator = allocator->sub_allocators[memory_type];
				//Com_DDPrintf("Created sub-allocator at 0x%p\n", sub_allocator);
			}
		}
	}

	sub_allocator->memory_used += device_memory->size;
	device_memory->memory = sub_allocator->memory;

	//Com_DDPrintf("Allocated %.2f MB, total usage is %.2f in sub-allocator at 0x%p\n", 
	//	(float)device_memory->size / 1048576.f, (float)sub_allocator->memory_usage / 1048576.f , sub_allocator);
	allocator->total_memory_used += device_memory->size;

	return DMA_SUCCESS;
}

void free_device_memory(DeviceMemoryAllocator* allocator, const DeviceMemory* device_memory)
{
	SubAllocator* sub_allocator = allocator->sub_allocators[device_memory->memory_type];

	while (sub_allocator->memory != device_memory->memory)
		sub_allocator = sub_allocator->next;

	buddy_allocator_free(sub_allocator->buddy_allocator, device_memory->memory_offset, device_memory->size);

	sub_allocator->memory_used -= device_memory->size;
	allocator->total_memory_used -= device_memory->size;
}

void destroy_device_memory_allocator(DeviceMemoryAllocator* allocator)
{
	assert(allocator->total_memory_used == 0);

	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		SubAllocator* sub_allocator = allocator->sub_allocators[i];
		while (sub_allocator != NULL)
		{
			vkFreeMemory(allocator->device, sub_allocator->memory, NULL);
			SubAllocator* next = sub_allocator->next;
			Z_Free(sub_allocator);
			sub_allocator = next;
		}
	}

	Z_Free(allocator);
}

int create_sub_allocator(DeviceMemoryAllocator* allocator, uint32_t memory_type, uint32_t alignment)
{
	SubAllocator* sub_allocator = (SubAllocator*)Z_Mallocz(sizeof(SubAllocator));

	uint32_t block_size = ALLOCATOR_BLOCK_SIZE;
	uint32_t capacity = ALLOCATOR_CAPACITY;

	if (alignment > block_size)
	{
		block_size = alignment;
		capacity = 1;

		while (capacity < ALLOCATOR_CAPACITY)
			capacity *= 2;
	}

	VkMemoryAllocateInfo memory_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = capacity,
		.memoryTypeIndex = memory_type
	};

#ifdef VKPT_DEVICE_GROUPS
	VkMemoryAllocateFlagsInfo mem_alloc_flags = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT,
		.deviceMask = (1 << qvk.device_count) - 1
	};

	if (qvk.device_count > 1) {
		memory_allocate_info.pNext = &mem_alloc_flags;
	}
#endif

	const VkResult result = vkAllocateMemory(allocator->device, &memory_allocate_info, NULL, &sub_allocator->memory);
	if (result != VK_SUCCESS)
		return 0;

	sub_allocator->buddy_allocator = create_buddy_allocator(capacity, block_size);
	sub_allocator->next = allocator->sub_allocators[memory_type];
	allocator->sub_allocators[memory_type] = sub_allocator;

	allocator->total_memory_allocated += capacity;

	return 1;
}

void get_device_malloc_stats(DeviceMemoryAllocator* allocator, size_t* memory_allocated, size_t* memory_used)
{
    if (memory_allocated) *memory_allocated = allocator->total_memory_allocated;
    if (memory_used) *memory_used = allocator->total_memory_used;
}
