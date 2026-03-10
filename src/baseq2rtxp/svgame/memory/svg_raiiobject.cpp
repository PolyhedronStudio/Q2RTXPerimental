#include "svgame/svg_local.h"
#include "svgame/memory/svg_raiiobject.hpp"

// Provide external definitions to satisfy linkers when QRAIIObject is instantiated
// in translation units that do not inline the header implementation.
void *QTagAllocator::Allocate( const size_t size, const memtag_t tag ) {
	return gi.TagMalloc( ( uint32_t )size, ( uint32_t )tag );
}

void QTagDeleter::Deallocate( void *ptr ) {
	gi.TagFree( ptr );
}
