/********************************************************************
*
*
*	ServerGame: Nav2 Memory Ownership Helpers
*
*
********************************************************************/
#pragma once

#include <cstddef>

#include "svgame/memory/svg_raiiobject.hpp"


/**
*
*
*	Nav2 Memory Type Aliases:
*
*
**/
/**
*	@brief	Canonical RAII owner alias for long-lived nav2 helper objects allocated from tagged svgame memory.
*	@tparam	T	Object type owned through `SVG_RAIIObject`.
*	@note	Nav2 code should prefer this alias when the allocation is object-like, single-owner, and benefits
*			from deterministic cleanup without manual `TagFree` calls in control-flow-heavy code.
**/
template<typename T>
using nav2_owner_t = SVG_RAIIObject<T>;


/**
*
*
*	Nav2 Memory Data Structures:
*
*
**/
/**
*	@brief	Descriptor that documents which tagged-memory ownership pattern a nav2 allocation should follow.
*	@note	This is intended for helper selection, debug assertions, and self-documenting call sites rather than
*			as a second allocator system.
**/
enum class nav2_memory_lifetime_t : uint8_t {
    //! Long-lived map or subsystem asset rebuilt explicitly on teardown.
    PersistentAsset = 0,
    //! Query-lifetime state owned by a scheduler job or staged request.
    QueryLifetime,
    //! Short-lived worker-slice scratch or builder-local temporary data.
    WorkerScratch,
    //! Serializer, deserializer, or file-blob staging buffer.
    SerializationBuffer,
    //! Debug or benchmark-only temporary state.
    DebugScratch,
    //! Filesystem-owned buffer that must be released with `FS_FreeFile`.
    FilesystemBuffer,
    Count
};

/**
*	@brief	Structured memory policy selection for nav2 helper calls.
*	@note	This keeps tag choice and intent explicit at the call site without overcomplicating allocation APIs.
**/
struct nav2_memory_policy_t {
    //! Tagged-memory lifetime classification for the allocation.
    nav2_memory_lifetime_t lifetime = nav2_memory_lifetime_t::PersistentAsset;
    //! Underlying svgame tag used for allocation.
    memtag_t tag = TAG_SVGAME;
};


/**
*
*
*	Nav2 Memory Public API:
*
*
**/
/**
*	@brief	Resolve the recommended svgame tag for a nav2 memory lifetime category.
*	@param	lifetime	Nav2 lifetime class to resolve.
*	@return	Recommended svgame tag to use for allocations in that category.
**/
memtag_t SVG_Nav2_Memory_DefaultTag( const nav2_memory_lifetime_t lifetime );

/**
*	@brief	Build a nav2 memory policy from a lifetime category using the default tag mapping.
*	@param	lifetime	Nav2 lifetime class to resolve.
*	@return	Memory policy containing both the lifetime and recommended tag.
**/
nav2_memory_policy_t SVG_Nav2_Memory_DefaultPolicy( const nav2_memory_lifetime_t lifetime );

/**
*	@brief	Allocate a zero-initialized tagged buffer for nav2 code.
*	@param	size	Size of the allocation in bytes.
*	@param	policy	Memory policy describing the intended lifetime and tag.
*	@return	Tagged allocation pointer, or `nullptr` when the request size is zero.
*	@note	This is a thin helper over `gi.TagMallocz` so call sites can stay self-documenting.
**/
void *SVG_Nav2_Memory_TagMallocz( const size_t size, const nav2_memory_policy_t &policy );

/**
*	@brief	Resize an existing tagged buffer for nav2 code.
*	@param	ptr	Existing tagged allocation, or `nullptr` to allocate a new block.
*	@param	newSize	New allocation size in bytes.
*	@param	policy	Memory policy describing the intended lifetime and tag.
*	@return	Resized tagged allocation pointer, or `nullptr` when the new size is zero.
*	@note	Callers should use this only for true growable buffers; object-like ownership should prefer RAII owners.
**/
void *SVG_Nav2_Memory_TagReMalloc( void *ptr, const size_t newSize, const nav2_memory_policy_t &policy );

/**
*	@brief	Free a tagged nav2 allocation and optionally null the caller's pointer.
*	@param	ptr	Pointer slot holding a tagged allocation.
*	@note	This helper exists to reduce accidental mismatches between tagged allocations and filesystem frees.
**/
void SVG_Nav2_Memory_TagFreeP( void **ptr );

/**
*	@brief	Free an engine-filesystem-loaded nav2 buffer and optionally null the caller's pointer.
*	@param	ptr	Pointer slot holding an `FS_LoadFile` buffer.
*	@note	This helper makes the FS-vs-tag ownership split explicit at call sites.
**/
void SVG_Nav2_Memory_FSFreeFileP( void **ptr );
