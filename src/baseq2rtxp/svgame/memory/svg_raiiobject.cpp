#include "svgame/svg_local.h"
#include "svgame/memory/svg_raiiobject.hpp"

// Provide external definitions to satisfy linkers when QRAIIObject is instantiated
// in translation units that do not inline the header implementation.
/**
*	@brief		Allocate memory using TagMalloc.
*	@details	Provides the actual implementation for the static Allocate method declared in QTagAllocator.
*	@note		Calls the server's TagMalloc function with the provided size and tag, ensuring proper memory tracking.
**/
void *QTagAllocator::Allocate( const size_t size, const memtag_t tag ) {
	return gi.TagMalloc( ( uint32_t )size, ( uint32_t )tag );
}

/**
*	@brief		Free memory using TagFree.
*	@details	Provides the actual implementation for the static Deallocate method declared in QTagDeleter.
*	@note		Calls the server's TagFree function with the provided pointer, ensuring proper memory deallocation and tracking.
**/
void QTagDeleter::Deallocate( void *ptr ) {
	gi.TagFree( ptr );
}
