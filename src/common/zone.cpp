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

#include "shared/shared.h"
#include "shared/util/util_list.h"
#include "common/common.h"
#include "common/zone.h"

#define Z_MAGIC     0x1d0d

/**
*   @brief  Prepended to each malloced block. Used to determine what group of memory the
*           specified allocation belongs to. As well as for storing the size of the allocation,
*           and a pointer to the next block in the list.
**/
typedef struct {
    uint16_t        magic;
    uint16_t        tag;        // for group free
    size_t          size;
    list_t          entry;
} zhead_t;

/**
*   @brief  Used to store the static memory allocation.
**/
typedef struct {
    zhead_t     z;
    char        data[2];
} zstatic_t;

/**
*   @brief  Used to store the memory allocation statistics.
**/
typedef struct {
    size_t      count;
    size_t      bytes;
} zstats_t;

//! The actual zone allocator chain.
static list_t       z_chain;
//! The actual stats for each tag type.
static zstats_t     z_stats[18];

/**
*   @brief  The static memory allocation for the digits 0-9 and the null terminator.
**/
#define S(d) \
    { .z = { .magic = Z_MAGIC, .tag = TAG_STATIC, .size = sizeof(zstatic_t) }, .data = d }

//! The static memory allocated for the digits 0-9 and the null terminator.
static const zstatic_t z_static[11] = {
    S("0"), S("1"), S("2"), S("3"), S("4"), S("5"), S("6"), S("7"), S("8"), S("9"), S("")
};
//! Undefine the macro to avoid conflicts.
#undef S

//! String matching names in accordance to the enum memtag_t.
static const char *const z_tagnames[ TAG_MAX ] = {
	"free",
    "static",

    "general",
    "cmd",
    "cvar",
    "fs",
    "refresh",
    "ui",
    "server",
    "sound",
    "cmodel",

    "svgame",
	"svgame_level",
	"svgame_edicts",
	"svgame_lua",

	"clgame",
	"clgame_level"
};

#define TAG_INDEX(tag)  ((tag) < TAG_MAX ? (tag) : TAG_FREE)

/**
*   @brief  This function updates the statistics for the specified tag by decrementing
*           the count and bytes allocated for that tag.
*   @param  z The pointer to the memory block to free.
**/
static inline void Z_CountFree(const zhead_t *z)
{
    zstats_t *s = &z_stats[TAG_INDEX(z->tag)];
    s->count--;
    s->bytes -= z->size;
}

/**
*   @brief  This function updates the statistics for the specified tag by incrementing
*           the count and bytes allocated for that tag.
*   @param  z The pointer to the memory block to count.
**/
static inline void Z_CountAlloc(const zhead_t *z)
{
    zstats_t *s = &z_stats[TAG_INDEX(z->tag)];
    s->count++;
    s->bytes += z->size;
}

#define Z_Validate(z) \
    Q_assert((z)->magic == Z_MAGIC && (z)->tag != TAG_FREE)

/**
*   @brief  Tests for memory leaks by iterating through the linked list of memory blocks.
*   @param  tag The memory tag used to identify the blocks to check for leaks.
*   @note   This function counts the number of leaked memory blocks and their total size.
*           It prints a message if any leaks are found, including the tag name, size, and count.
*           The function also validates each memory block by checking its magic number and tag.
*           If the tag is TAG_FREE, it checks for blocks that have been freed but not properly.
*           The function is intended for debugging and monitoring memory usage.
**/
void Z_LeakTest(memtag_t tag)
{
    zhead_t *z;
    size_t numLeaks = 0, numBytes = 0;

    LIST_FOR_EACH(zhead_t, z, &z_chain, entry) {
        Z_Validate(z);
        if (z->tag == tag || (tag == TAG_FREE && z->tag >= TAG_MAX)) {
            numLeaks++;
            numBytes += z->size;
        }
    }

    if (numLeaks) {
        Com_WPrintf("************* Z_LeakTest *************\n"
                    "%s leaked %zu bytes of memory (%zu object%s)\n"
                    "**************************************\n",
                    z_tagnames[TAG_INDEX(tag)],
                    numBytes, numLeaks, numLeaks == 1 ? "" : "s");
    }
}

/**
*   @brief  Frees the memory block pointed at by ptr.
*   @param  ptr The pointer to the memory block to free.
*   @note   This function checks if the pointer is NULL before attempting to free it.
*           It also validates the memory block by checking its magic number and tag.
*           If the block is not static, it removes the block from the linked list and
*           sets its magic number to 0 and tag to TAG_FREE before freeing the memory.
**/
void Z_Free(void *ptr)
{
    zhead_t *z;

    if (!ptr) {
        return;
    }

    z = (zhead_t *)ptr - 1;

    Z_Validate(z);

    Z_CountFree(z);

    if (z->tag != TAG_STATIC) {
        List_Remove(&z->entry);
        z->magic = 0xdead;
        z->tag = TAG_FREE;
        free(z);
    }
}

/**
*   @brief  Frees the memory block pointed at by (*ptr), if that's nonzero, and sets (*ptr) to zero.
**/
void Z_Freep(void **ptr)
{
    Q_assert(ptr);
    if (*ptr) {
        Z_Free(*ptr);
        *ptr = NULL;
    }
}

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
void *Z_Realloc(void *ptr, size_t size)
{
    zhead_t *z;

    if (!ptr) {
        return Z_Malloc(size);
    }

    if (!size) {
        Z_Free(ptr);
        return NULL;
    }

    z = (zhead_t *)ptr - 1;

    Z_Validate(z);

    Q_assert(size <= INT_MAX);

    size += sizeof(*z);
    if (z->size == size) {
        return z + 1;
    }

    Q_assert(z->tag != TAG_STATIC);

    Z_CountFree(z);

    z = static_cast<zhead_t*>( realloc( z, size ) ); // WID: C++20: Added cast.
    if (!z) {
        Com_Error( ERR_FATAL, "%s: couldn't realloc %zu bytes", __func__, size );
        return nullptr;
    }

    z->size = size;
    List_Relink(&z->entry);

    Z_CountAlloc(z);

    return z + 1;
}

/**
*   @brief  Prints the memory allocation statistics for each tag type.
*   @note   This function is intended for debugging and monitoring memory usage.
*           It iterates through the z_stats array and prints the number of bytes
*           and blocks allocated for each tag type. It also prints the total number
*           of bytes and blocks allocated across all tag types.
**/
void Z_Stats_f(void)
{
    size_t bytes = 0, count = 0;
    zstats_t *s;
    int i;

	Com_Printf( "Tag Zone Memory Allocations:\n----------- ------ ----------\n");
    Com_Printf("    bytes   blocks tag name\n"
               "----------- ------ ---------\n");

	// Stores size type string.
    std::string sizeTypeString = "";

	// Iterate through the stats array and print the statistics for each tag type.
    for (i = 0, s = z_stats; i < TAG_MAX; i++, s++) {
        if (!s->count) {
            //Com_Printf( "%9zu %6zu %s\n", s->bytes, s->count, z_tagnames[ i ] );
            continue;
        }
        // Format print string.
        std::string sizeValueString = Q_Str_FormatSizeString( s->bytes, sizeTypeString );
        sizeValueString += " " + sizeTypeString;
        // Print.
        Com_Printf( "%9s %6zu %s\n", sizeValueString.c_str(), s->count, z_tagnames[i] );

        // Sum up the totals.
        bytes += s->bytes;
        count += s->count;
    }

	// Format and print the total summed up memory usage stats:
    std::string sizeValueString = Q_Str_FormatSizeString( bytes, sizeTypeString );
    sizeValueString += " " + sizeTypeString;
    Com_Printf("----------- ------ ---------\n"
                "%9s %6zu total\n", sizeValueString.c_str(), count );
}

/**
*   @brief   Frees all memory blocks with the specified tag.
*   @param   tag The memory tag used to identify the blocks to free.
**/
void Z_FreeTags(memtag_t tag)
{
    zhead_t *z, *n;

    LIST_FOR_EACH_SAFE(zhead_t, z, n, &z_chain, entry) {
        Z_Validate(z);
        if (z->tag == tag) {
            Z_Free(z + 1);
        }
    }
}

/**
*   @brief   Allocates a block of memory with a specified tag.
*   @param   size The size of the memory block to allocate, in bytes.
*   @param   tag The memory tag used to categorize the allocation.
*   @param   init If true, the allocated memory is initialized to zero.
*   @return  A pointer to the allocated memory block, or NULL if the allocation fails.
**/
static void *Z_TagMallocInternal(size_t size, memtag_t tag, bool init)
{
    zhead_t *z;

    if (!size) {
        return NULL;
    }

    Q_assert(size <= INT_MAX);
    Q_assert(tag > TAG_FREE && tag <= UINT16_MAX);

    size += sizeof(*z);
    z = static_cast<zhead_t *>( init ? calloc(1, size) : malloc(size) );
    if (!z) {
        Com_Error(ERR_FATAL, "%s: couldn't allocate %zu bytes", __func__, size);
        return nullptr;
    }
    z->magic = Z_MAGIC;
    z->tag = tag;
    z->size = size;

    List_Insert(&z_chain, &z->entry);

#if USE_TESTS
    if (!init && z_perturb && z_perturb->integer) {
        memset(z + 1, z_perturb->integer, size - sizeof(*z));
    }
#endif

    Z_CountAlloc(z);

    return z + 1;
}

/**
*   @brief   Allocates a block of memory with a specified tag.
*   @param   size The size of the memory block to allocate, in bytes.
*   @param   tag The memory tag used to categorize the allocation.
*   @return  A pointer to the allocated memory block, or NULL if the allocation fails.
**/
void *Z_TagMalloc(size_t size, memtag_t tag)
{
    return Z_TagMallocInternal(size, tag, false);
}
/**
*   @brief  Allocates a block of memory with a specified tag and initializes it to zero.
*   @param  size The size of the memory block to allocate, in bytes.
*   @param  tag The memory tag used to categorize the allocation.
*   @return A pointer to the allocated and zero-initialized memory block.
**/
void *Z_TagMallocz(size_t size, memtag_t tag)
{
    return Z_TagMallocInternal(size, tag, true);
}
/**
*   @brief  Allocates a block of memory within the TAG_GENERAL space.
**/
void *Z_Malloc(size_t size)
{
    return Z_TagMalloc(size, TAG_GENERAL);
}
/**
*   @brief  Allocates a block of memory within the TAG_GENERAL space and initializes it to zero.
**/
void *Z_Mallocz(size_t size)
{
    return Z_TagMallocz(size, TAG_GENERAL);
}

/**
*   @brief  Initializes the zone memory allocator.
**/
void Z_Init(void)
{
    List_Init(&z_chain);
}

/**
*   @brief  Copies a string to a new memory block and returns a pointer to it.
**/
char *Z_TagCopyString(const char *in, memtag_t tag)
{
    size_t len;

    if (!in) {
        return NULL;
    }

    len = strlen(in) + 1;
    return static_cast<char*>( memcpy(Z_TagMalloc(len, tag), in, len) ); // WID: C++20: Added cast.
}

/**
*   @brief  Copies a string to a static buffer and returns a pointer to it.
*   @param  in The input string to copy.
*   @return A pointer to the copied string in static storage, or NULL if the input string is NULL.
*   @note   The function uses a static buffer to store the copied string, which means that
*           the returned pointer will always point to the same location in memory.
**/
char *Z_CvarCopyString(const char *in)
{
    const zstatic_t *z;
    int i;

    if (!in) {
        return NULL;
    }

	// Check for static storage of "".
    if (!in[0]) {
        i = 10;
    // Pick a digit that is stored statically.
    } else if (!in[1] && Q_isdigit(in[0])) {
        i = in[0] - '0';
    // Otherwise, copy the string into TAG_CVAR.
    } else {
        return Z_TagCopyString( in, TAG_CVAR );
    }

    // Return static storage string instead.
    z = &z_static[i];
    Z_CountAlloc(&z->z);
    return (char *)z->data;
}
