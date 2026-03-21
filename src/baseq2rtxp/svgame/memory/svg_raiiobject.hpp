/**
*
*
*
*	TagMalloc/TagFree Allocator and Deleter:
*
*
*
**/
#pragma once

// Pull in shared tag allocator APIs used by the RAII helpers.
#include "sharedgame/sg_shared.h"


/**
*
*
*
*	SVGame(-Level) Tag Memory `Allocator` and `Deleter`:
*
*
*
**/
/**
*	@brief	Allocator functor for TagMalloc.
*	@note	Provides static Allocate method compatible with QRAIIObject.
**/
struct QTagAllocator {
	/**
	*	@brief	Allocate memory using TagMalloc.
	*	@param	size	Size of memory block to allocate.
	*	@param	tag	Memory tag for tracking (e.g., TAG_SVGAME_LEVEL).
	*	@return	Pointer to allocated memory or nullptr on failure.
	**/
	static void *Allocate( const size_t size, const memtag_t tag );
};
/**
*	@brief	Deleter functor for TagFree.
*	@note	Provides static Deallocate method compatible with QRAIIObject.
**/
struct QTagDeleter {
	/**
	*	@brief	Free memory using TagFree.
	*	@param	ptr	Pointer to memory block to free.
	**/
	static void Deallocate( void *ptr );
};



/**
*
*
*
*	RAII Helper for SVGame(-Level) Tagged Memory:
*
*
*
**/
/**
*	@brief	Type alias for RAII objects using TagMalloc/TagFree.
*	@tparam	T	Type of object to manage.
*	@note	Provides convenient shorthand for common case of TagMalloc/TagFree management.
*	@example
*		qtag_owner_t<nav2_mesh_t> mesh_owner;
*		if ( mesh_owner.create( TAG_SVGAME_LEVEL ) ) {
*			mesh_owner->total_tiles = 0;
*		}
**/
template<typename T>
using SVG_RAIIObject = QRAIIObject<T, QTagAllocator, QTagDeleter>;