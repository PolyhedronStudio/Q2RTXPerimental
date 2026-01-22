/********************************************************************
*
*
*	VKPT Renderer: Buddy Allocator
*
*	Implements a binary buddy memory allocator for efficient sub-allocation
*	within large memory blocks. Uses a hierarchical free-list structure
*	where each level represents blocks of a specific size (powers of 2).
*
*	Algorithm:
*	- Memory is organized in a binary tree of blocks
*	- Each level contains blocks of size: block_size * 2^level
*	- Allocation: Find smallest suitable block, split if necessary
*	- Deallocation: Free block, merge with buddy if both free
*
*	Data Structures:
*	- free_block_lists: Per-level linked lists of free blocks
*	- block_states: Array tracking state of each block (FREE/SPLIT/ALLOCATED)
*	- free_items: Pre-allocated pool of list nodes to avoid dynamic allocation
*
*	Buddy System Properties:
*	- Fast O(log n) allocation and deallocation
*	- Automatic coalescing of adjacent free blocks
*	- Minimal fragmentation for power-of-2 sized allocations
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
#include "common/zone.h"
#include "buddy_allocator.h"
#include <assert.h>



/**
*
*
*
*	Block States and Data Structures:
*
*
*
**/
//! Possible states for blocks in the buddy allocator.
typedef enum AllocatorBlockState {
	BLOCK_NONE = 0,       //! Uninitialized state.
	BLOCK_FREE,           //! Block is available for allocation.
	BLOCK_SPLIT,          //! Block has been subdivided into smaller blocks.
	BLOCK_ALLOCATED       //! Block is currently allocated to a user.
} AllocatorBlockState;

//! Free list node structure for tracking available blocks.
typedef struct AllocatorFreeListItem {
	//! Index of this block in the allocator's block array.
	uint32_t block_index;
	//! Next free block in this level's free list.
	struct AllocatorFreeListItem* next;
} AllocatorFreeListItem;

//! Buddy allocator main structure.
typedef struct BuddyAllocator {
	//! Minimum block size (smallest allocation unit).
	uint32_t block_size;
	//! Number of levels in the hierarchy (log2 of capacity/block_size + 1).
	uint32_t level_num;
	//! Array of free lists, one per level.
	struct AllocatorFreeListItem** free_block_lists;
	//! State array for all blocks.
	uint8_t* block_states;
	//! Pre-allocated pool of free list items.
	struct AllocatorFreeListItem* free_items;
} BuddyAllocator;



/**
*
*
*
*	Forward Declarations:
*
*
*
**/
static inline size_t _align( size_t value, size_t alignment );
static inline uint64_t div_ceil( uint64_t a, uint64_t b );
static inline int32_t uint_log2( uint64_t x );
static inline int32_t uint_log2_ceil( uint64_t x );
static inline AllocatorFreeListItem* allocate_list_item( BuddyAllocator* allocator );
static inline void free_list_item( BuddyAllocator* allocator, AllocatorFreeListItem* item );
static inline void write_free_block_to_list( BuddyAllocator* allocator, uint32_t level, uint32_t block_index );
static inline uint32_t get_level_offset( BuddyAllocator* allocator, uint32_t level );
static inline uint32_t get_next_level_offset( BuddyAllocator* allocator, uint32_t level, uint32_t offset );

void subdivide_block( BuddyAllocator* allocator, uint32_t src_level, uint32_t dst_level );
bool merge_blocks( BuddyAllocator* allocator, uint32_t level, uint32_t block_index );
void remove_block_from_free_list( BuddyAllocator* allocator, uint32_t level, uint32_t block_index );



/**
*
*
*
*	Allocator Creation:
*
*
*
**/
/**
*	@brief	Creates a new buddy allocator with specified capacity and block size.
*	@param	capacity	Total size of memory to manage (must be power-of-2 * block_size).
*	@param	block_size	Minimum allocation size (must be power of 2).
*	@return	Pointer to newly created allocator, or NULL on failure.
*	@note	Allocates a single contiguous block of memory for the allocator structure,
*			free lists, block states, and free list item pool. Initially, all memory
*			is in one large free block at the top level.
**/
BuddyAllocator* create_buddy_allocator( uint64_t capacity, uint64_t block_size ) {
	// Validate: capacity must be a multiple of block_size.
	assert( ( capacity % block_size ) == 0 );
	const uint32_t level_num = uint_log2( capacity / block_size ) + 1;

	// Validate: capacity must be a *power-of-2* multiple of block size.
	assert( capacity == ( block_size << ( level_num - 1 ) ) );

	// Calculate total number of blocks across all levels.
	uint32_t block_num = 0;
	for ( uint32_t i = 0; i < level_num; i++ ) {
		block_num += 1 << ( ( level_num - 1 ) - i );
	}

	// Calculate memory layout with proper alignment.
	const size_t alignment = 16;
	const size_t allocator_size = _align( sizeof( BuddyAllocator ), alignment );
	const size_t free_list_array_size = _align( level_num * sizeof( AllocatorFreeListItem* ), alignment );
	const size_t free_item_buffer_size = _align( block_num * sizeof( AllocatorFreeListItem ), alignment );
	const size_t block_state_size = _align( block_num * sizeof( uint8_t ), alignment );

	// Allocate single contiguous memory block for all allocator data.
	char* memory = (char*)Z_Mallocz( allocator_size + free_list_array_size + free_item_buffer_size + block_state_size );

	// Set up allocator structure and sub-arrays.
	BuddyAllocator* allocator = (BuddyAllocator*)memory;
	allocator->block_size = block_size;
	allocator->level_num = level_num;
	allocator->free_block_lists = (AllocatorFreeListItem**)( memory + allocator_size );
	allocator->free_items = (AllocatorFreeListItem*)( memory + allocator_size + free_list_array_size );
	allocator->block_states = (uint8_t*)( memory + allocator_size + free_list_array_size + free_item_buffer_size );

	// Initialize free item pool as a linked list.
	for ( uint32_t i = 0; i < block_num; i++ ) {
		allocator->free_items[i].next = allocator->free_items + i + 1;
	}
	allocator->free_items[block_num - 1].next = NULL;

	// Initialize with one large free block at the highest level.
	allocator->block_states[block_num - 1] = BLOCK_FREE;
	write_free_block_to_list( allocator, level_num - 1, 0 );

	return allocator;
}



/**
*
*
*
*	Allocation & Deallocation:
*
*
*
**/
/**
*	@brief	Allocates memory from the buddy allocator.
*	@param	allocator	Buddy allocator instance.
*	@param	size		Requested allocation size in bytes.
*	@param	alignment	Required alignment (must be power of 2).
*	@param	offset		Output: offset of allocated memory from allocator base.
*	@return	BA_SUCCESS on success, BA_NOT_ENOUGH_MEMORY if allocation fails.
*	@note	Finds the smallest suitable block, subdividing larger blocks if necessary.
*			The allocation is aligned to the specified alignment requirement.
**/
BAResult buddy_allocator_allocate( BuddyAllocator* allocator, uint64_t size, uint64_t alignment, uint64_t* offset ) {
	const uint32_t level = uint_log2_ceil(div_ceil(size, allocator->block_size));

	// The requested size exceeds the allocator capacity
	if (level >= allocator->level_num)
		return BA_NOT_ENOUGH_MEMORY;

	// Every block is aligned to its size
	const uint64_t block_size = (uint64_t)(1 << level) * allocator->block_size;
	const uint64_t alignment_size = block_size % alignment;

	if (alignment_size != 0)
		return BA_INVALID_ALIGNMENT;

	uint32_t i = level;
	for (; i < allocator->level_num && allocator->free_block_lists[i] == NULL; i++)
	{
	}

	if (i == allocator->level_num)
		return BA_NOT_ENOUGH_MEMORY;

	if (i > level)
		subdivide_block(allocator, i, level);

	AllocatorFreeListItem* item = allocator->free_block_lists[level];
	allocator->free_block_lists[level] = item->next;

	const uint32_t level_block_offset = get_level_offset(allocator, level);
	allocator->block_states[level_block_offset + item->block_index] = BLOCK_ALLOCATED;

	*offset = item->block_index * block_size;
	free_list_item(allocator, item);

	return BA_SUCCESS;
}

void buddy_allocator_free(BuddyAllocator* allocator, uint64_t offset, uint64_t size)
{
	const uint32_t level = uint_log2_ceil(div_ceil(size, allocator->block_size));
	const uint64_t block_size = (uint64_t)(1 << level) * allocator->block_size;
	const uint32_t block_index = offset / block_size;

	const uint32_t level_block_offset = get_level_offset(allocator, level);
	assert(allocator->block_states[level_block_offset + block_index] == BLOCK_ALLOCATED);
	allocator->block_states[level_block_offset + block_index] = BLOCK_FREE;

	if (!merge_blocks(allocator, level, block_index))
		write_free_block_to_list(allocator, level, block_index);
}

void destroy_buddy_allocator(BuddyAllocator* allocator)
{
	Z_Free(allocator);
}

void subdivide_block(BuddyAllocator* allocator, uint32_t src_level, uint32_t dst_level)
{
	assert(dst_level < src_level);

	{
		// Task free block from the list
		AllocatorFreeListItem* item = allocator->free_block_lists[src_level];
		allocator->free_block_lists[src_level] = item->next;

		// Find offsets for states
		const uint32_t previous_level_block_offset = get_level_offset(allocator, src_level - 1);
		const uint32_t current_level_block_offset = get_next_level_offset(allocator, src_level - 1, previous_level_block_offset);

		const uint32_t block_index = item->block_index;

		// Change state of the blocks
		assert(allocator->block_states[current_level_block_offset + block_index] == BLOCK_FREE);
		allocator->block_states[current_level_block_offset + block_index] = BLOCK_SPLIT;
		allocator->block_states[previous_level_block_offset + block_index * 2] = BLOCK_FREE;
		allocator->block_states[previous_level_block_offset + block_index * 2 + 1] = BLOCK_FREE;

		// Add blocks to free list
		AllocatorFreeListItem* item0 = allocate_list_item(allocator);
		AllocatorFreeListItem* item1 = allocate_list_item(allocator);
		item0->block_index = block_index * 2;
		item0->next = item1;
		item1->block_index = block_index * 2 + 1;
		item1->next = allocator->free_block_lists[src_level - 1];
		allocator->free_block_lists[src_level - 1] = item0;
	}

	if (src_level - 1 != dst_level)
		subdivide_block(allocator, src_level - 1, dst_level);
}

bool merge_blocks(BuddyAllocator* allocator, uint32_t level, uint32_t block_index)
{
	if (level == allocator->level_num - 1)
	{
		write_free_block_to_list(allocator, level, block_index);
		return true;
	}

	const uint32_t level_block_offset = get_level_offset(allocator, level);
	const uint32_t buddy_block_index = (block_index % 2) == 0 ? block_index + 1 : block_index - 1;

	if (allocator->block_states[level_block_offset + buddy_block_index] != BLOCK_FREE)
		return false;

	remove_block_from_free_list(allocator, level, buddy_block_index);

	const uint32_t next_level_block_offset = get_level_offset(allocator, level + 1);
	const uint32_t next_level_block_index = block_index / 2;
	assert(allocator->block_states[next_level_block_offset + next_level_block_index] == BLOCK_SPLIT);
	allocator->block_states[next_level_block_offset + next_level_block_index] = BLOCK_FREE;

	if (!merge_blocks(allocator, level + 1, next_level_block_index))
		write_free_block_to_list(allocator, level + 1, next_level_block_index);

	return true;
}

void remove_block_from_free_list(BuddyAllocator* allocator, uint32_t level, uint32_t block_index)
{
	AllocatorFreeListItem** prev_next = &allocator->free_block_lists[level];
	AllocatorFreeListItem* item = allocator->free_block_lists[level];

	assert(item != NULL);
	while (item->block_index != block_index)
	{
		prev_next = &item->next;
		item = item->next;
		assert(item != NULL);
	}

	*prev_next = item->next;
	free_list_item(allocator, item);
}

static inline size_t _align(size_t value, size_t alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

static inline uint64_t div_ceil(uint64_t a, uint64_t b)
{
	return (a + b - 1) / b;
}

static inline int32_t uint_log2(uint64_t x)
{
	uint32_t result = 0;
	while (x >>= 1)
		result++;
	return result;
}

static inline int32_t uint_log2_ceil(uint64_t x)
{
	int32_t log_x = uint_log2(x);

	if (x > (1ull << log_x))
		log_x += 1;

	return log_x;
}

static inline AllocatorFreeListItem* allocate_list_item(BuddyAllocator* allocator)
{
	AllocatorFreeListItem* item = allocator->free_items;
	allocator->free_items = item->next;
	return item;
}

static inline void free_list_item(BuddyAllocator* allocator, AllocatorFreeListItem* item)
{
	item->next = allocator->free_items;
	allocator->free_items = item;
}

static inline void write_free_block_to_list(BuddyAllocator* allocator, uint32_t level, uint32_t block_index)
{
	AllocatorFreeListItem* item = allocate_list_item(allocator);
	item->block_index = block_index;
	item->next = allocator->free_block_lists[level];
	allocator->free_block_lists[level] = item;
}

static inline uint32_t get_level_offset(BuddyAllocator* allocator, uint32_t level)
{
	uint32_t offset = 0;
	for (int32_t i = 0; i < level; i++)
		offset += 1 << ((allocator->level_num - 1) - i);

	return offset;
}

static inline uint32_t get_next_level_offset(BuddyAllocator* allocator, uint32_t prev_level, uint32_t offset)
{
	return offset + (1 << ((allocator->level_num - 1) - prev_level));
}
