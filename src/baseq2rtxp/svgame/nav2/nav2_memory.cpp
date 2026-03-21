/********************************************************************
*
*
*	ServerGame: Nav2 Memory Ownership Helpers - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_memory.h"

#include "svgame/svg_local.h"


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
memtag_t SVG_Nav2_Memory_DefaultTag( const nav2_memory_lifetime_t lifetime ) {
    /**
    *    Map each nav2 lifetime category onto the closest existing svgame tag ownership bucket.
    **/
    switch ( lifetime ) {
    case nav2_memory_lifetime_t::PersistentAsset:
        return TAG_SVGAME;
    case nav2_memory_lifetime_t::QueryLifetime:
        return TAG_SVGAME;
    case nav2_memory_lifetime_t::WorkerScratch:
        return TAG_SVGAME;
    case nav2_memory_lifetime_t::SerializationBuffer:
        return TAG_SVGAME;
    case nav2_memory_lifetime_t::DebugScratch:
        return TAG_SVGAME;
    case nav2_memory_lifetime_t::FilesystemBuffer:
        return TAG_MAX;
    case nav2_memory_lifetime_t::Count:
    default:
        break;
    }

    /**
    *    Fall back to the primary svgame tag when an unknown category is encountered.
    **/
    return TAG_SVGAME;
}

/**
*	@brief	Build a nav2 memory policy from a lifetime category using the default tag mapping.
*	@param	lifetime	Nav2 lifetime class to resolve.
*	@return	Memory policy containing both the lifetime and recommended tag.
**/
nav2_memory_policy_t SVG_Nav2_Memory_DefaultPolicy( const nav2_memory_lifetime_t lifetime ) {
    /**
    *    Materialize a compact policy object so call sites can pass intent and tag choice together.
    **/
    nav2_memory_policy_t policy = {};
    policy.lifetime = lifetime;
    policy.tag = SVG_Nav2_Memory_DefaultTag( lifetime );
    return policy;
}

/**
*	@brief	Allocate a zero-initialized tagged buffer for nav2 code.
*	@param	size	Size of the allocation in bytes.
*	@param	policy	Memory policy describing the intended lifetime and tag.
*	@return	Tagged allocation pointer, or `nullptr` when the request size is zero.
*	@note	This is a thin helper over `gi.TagMallocz` so call sites can stay self-documenting.
**/
void *SVG_Nav2_Memory_TagMallocz( const size_t size, const nav2_memory_policy_t &policy ) {
    /**
    *    Treat zero-sized allocation requests as a no-op so callers do not receive ambiguous non-null pointers.
    **/
    if ( size == 0 ) {
        return nullptr;
    }

    /**
    *    Filesystem buffers are not tag-owned and must not be allocated through the tagged allocator path.
    **/
    if ( policy.lifetime == nav2_memory_lifetime_t::FilesystemBuffer ) {
        return nullptr;
    }

    /**
    *    Allocate zero-initialized memory through the canonical svgame tagged allocator.
    **/
    return gi.TagMallocz( ( uint32_t )size, policy.tag );
}

/**
*	@brief	Resize an existing tagged buffer for nav2 code.
*	@param	ptr	Existing tagged allocation, or `nullptr` to allocate a new block.
*	@param	newSize	New allocation size in bytes.
*	@param	policy	Memory policy describing the intended lifetime and tag.
*	@return	Resized tagged allocation pointer, or `nullptr` when the new size is zero.
*	@note	Callers should use this only for true growable buffers; object-like ownership should prefer RAII owners.
**/
void *SVG_Nav2_Memory_TagReMalloc( void *ptr, const size_t newSize, const nav2_memory_policy_t &policy ) {
    /**
    *    Treat zero-sized resize requests as an explicit free so callers do not retain stale pointers.
    **/
    if ( newSize == 0 ) {
        if ( ptr ) {
            gi.TagFree( ptr );
        }
        return nullptr;
    }

    /**
    *    Filesystem buffers are not tag-owned and must not be resized through the tagged allocator path.
    **/
    if ( policy.lifetime == nav2_memory_lifetime_t::FilesystemBuffer ) {
        return nullptr;
    }

    /**
    *    Allocate a new zero-initialized buffer when the caller did not provide an existing tagged block.
    **/
    if ( !ptr ) {
        return gi.TagMallocz( ( uint32_t )newSize, policy.tag );
    }

    /**
    *    Resize the tagged allocation through the canonical svgame realloc helper.
    **/
    return gi.TagReMalloc( ptr, ( unsigned )newSize );
}

/**
*	@brief	Free a tagged nav2 allocation and optionally null the caller's pointer.
*	@param	ptr	Pointer slot holding a tagged allocation.
*	@note	This helper exists to reduce accidental mismatches between tagged allocations and filesystem frees.
**/
void SVG_Nav2_Memory_TagFreeP( void **ptr ) {
    /**
    *    Guard against null pointer slots before attempting to release any memory.
    **/
    if ( !ptr || !*ptr ) {
        return;
    }

    /**
    *    Release the tagged allocation and clear the caller's slot in one explicit helper.
    **/
    gi.TagFreeP( ptr );
}

/**
*	@brief	Free an engine-filesystem-loaded nav2 buffer and optionally null the caller's pointer.
*	@param	ptr	Pointer slot holding an `FS_LoadFile` buffer.
*	@note	This helper makes the FS-vs-tag ownership split explicit at call sites.
**/
void SVG_Nav2_Memory_FSFreeFileP( void **ptr ) {
    /**
    *    Guard against null pointer slots before attempting to release any filesystem-owned buffer.
    **/
    if ( !ptr || !*ptr ) {
        return;
    }

    /**
    *    Release the engine filesystem buffer through the matching FS ownership API and clear the slot.
    **/
    gi.FS_FreeFile( *ptr );
    *ptr = nullptr;
}
