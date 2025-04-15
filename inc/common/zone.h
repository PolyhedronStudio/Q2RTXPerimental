/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef ZONE_H
#define ZONE_H

// Extern C
QEXTERN_C_OPEN

//! Copy String Utility.
#define Z_CopyString(string)    Z_TagCopyString(string, TAG_GENERAL)
//! Copy Struct Utility.
#define Z_CopyStruct(ptr)       memcpy(Z_Malloc(sizeof(*ptr)), ptr, sizeof(*ptr))

void    Z_Init(void);
/**
*   @brief  Frees the memory block pointed at by ptr.
*   @param  ptr The pointer to the memory block to free.
*   @note   This function checks if the pointer is NULL before attempting to free it.
*           It also validates the memory block by checking its magic number and tag.
*           If the block is not static, it removes the block from the linked list and
*           sets its magic number to 0 and tag to TAG_FREE before freeing the memory.
**/
void    Z_Free(void *ptr);
/**
*   @brief  Frees the memory block pointed at by (*ptr), if that's nonzero, and sets (*ptr) to zero.
**/
void    Z_Freep(void **ptr);
/**
*   @brief  Reallocates a block of memory to a new size.
*   @param  ptr The pointer to the memory block to reallocate.
*   @param  size The new size of the memory block, in bytes.
*   @return A pointer to the reallocated memory block, or NULL if the allocation fails.
*   @note   The contents of the memory block are preserved during the reallocation.
*           If the pointer is NULL, a new block of memory is allocated.
*           If the size is zero, the memory block is freed and NULL is returned.
*           If the size is less than the current size, the memory block is shrunk.
*           If the size is greater than the current size, the memory block is expanded.
**/
void    *Z_Realloc(void *ptr, size_t size);

/**
*   @brief   Allocates a block of memory with a specified tag.
*   @param   size The size of the memory block to allocate, in bytes.
*   @param   tag The memory tag used to categorize the allocation.
*   @return  A pointer to the allocated memory block, or NULL if the allocation fails.
**/
void    *Z_Malloc(size_t size) q_malloc;
/**
*   @brief  Allocates a block of memory with a specified tag and initializes it to zero.
*   @param  size The size of the memory block to allocate, in bytes.
*   @param  tag The memory tag used to categorize the allocation.
*   @return A pointer to the allocated and zero-initialized memory block.
**/
void    *Z_Mallocz(size_t size) q_malloc;
/**
*   @brief   Allocates a block of memory with a specified tag.
*   @param   size The size of the memory block to allocate, in bytes.
*   @param   tag The memory tag used to categorize the allocation.
*   @return  A pointer to the allocated memory block, or NULL if the allocation fails.
**/
void    *Z_TagMalloc(size_t size, memtag_t tag) q_malloc;
/**
*   @brief  Allocates a block of memory with a specified tag and initializes it to zero.
*   @param  size The size of the memory block to allocate, in bytes.
*   @param  tag The memory tag used to categorize the allocation.
*   @return A pointer to the allocated and zero-initialized memory block.
**/
void    *Z_TagMallocz(size_t size, memtag_t tag) q_malloc;
/**
*   @brief  Copies a string to a new memory block and returns a pointer to it.
**/
char    *Z_TagCopyString(const char *in, memtag_t tag) q_malloc;
/**
*   @brief   Frees all memory blocks with the specified tag.
*   @param   tag The memory tag used to identify the blocks to free.
**/
void    Z_FreeTags(memtag_t tag);
/**
*   @brief  Tests for memory leaks by iterating through the linked list of memory blocks.
*   @param  tag The memory tag used to identify the blocks to check for leaks.
*   @note   This function counts the number of leaked memory blocks and their total size.
*           It prints a message if any leaks are found, including the tag name, size, and count.
*           The function also validates each memory block by checking its magic number and tag.
*           If the tag is TAG_FREE, it checks for blocks that have been freed but not properly.
*           The function is intended for debugging and monitoring memory usage.
**/
void    Z_LeakTest(memtag_t tag);
/**
*   @brief  Prints the memory allocation statistics for each tag type.
*   @note   This function is intended for debugging and monitoring memory usage.
*           It iterates through the z_stats array and prints the number of bytes
*           and blocks allocated for each tag type. It also prints the total number
*           of bytes and blocks allocated across all tag types.
**/
void    Z_Stats_f(void);

// may return pointer to static memory
/**
*   @brief  Copies a string to a static buffer and returns a pointer to it.
*   @param  in The input string to copy.
*   @return A pointer to the copied string in static storage, or NULL if the input string is NULL.
*   @note   The function uses a static buffer to store the copied string, which means that
*           the returned pointer will always point to the same location in memory.
**/
char    *Z_CvarCopyString(const char *in);

// Extern C
QEXTERN_C_CLOSE

#endif // ZONE_H
