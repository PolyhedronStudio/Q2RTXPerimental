/********************************************************************
*
* 
*
*	CppLib: Base Library for the C++ Portion of Q2RTXPerimental
*
*	shObject: Object Base Class.
*
*	Objects that rely on Zone(Tag) allocating should derive from
*	this class.
* 
*	It provides the means of allocating and freeing objects using
*	the zone tag allocator.
*			
*
*
********************************************************************/
#pragma once


/**
*	@brief	
**/
//! Implements new/delete operator overloading, which caches the 
//! file and line of (de-)allocation.
#if _DEBUG
	#define _SHOBJECT_DEBUG_ZONE_TAG_MEMORY 1
#else
	#define _SHOBJECT_DEBUG_ZONE_TAG_MEMORY 0
#endif


/**
*	Function pointers intend to be set to a wrapper function, allocating 
*	memory into the designated Tag Zone.	
**/
//! Resize an already TagMalloced block of memory.

using shFnPtrTagReMalloc = void *( * )( void *ptr, const uint32_t newsize );
//! Allocate a tagged memory block.
using shFnPtrTagMalloc = void *( * )( uint32_t size, const memtag_t tag );
//! Free a tag allocated memory block.
using shFnPtrTagFree = void ( * )( void *block );
//! Free all SVGAME_ related tag memory blocks. (Essentially resetting memory.)
//using shFnPtrFreeTags	= void *( * )( memtag_t tag );


/**
*	@brief	Special object to wrap around a zone tag allocator.
**/
template< const memtag_t Z, 
	shFnPtrTagMalloc fnMalloc, 
	shFnPtrTagReMalloc fnReMalloc,
	shFnPtrTagFree fnFree >
class shZoneTagAllocator {
public:
	//! The zone tag that this allocator allocates into.
	static constexpr memtag_t _zoneTag = Z;

	//! Zone Tag Malloc:
	static constexpr shFnPtrTagMalloc Malloc = fnMalloc;
	//! Zone Tag ReMalloc:
	static constexpr shFnPtrTagReMalloc ReMalloc = fnReMalloc;
	//! Zone Tag Free:
	static constexpr shFnPtrTagFree Free = fnFree;
};



/**
*	@brief	Base Object Class. It overloads the new/delete operators to allocate 
*			memory using the specified zone tag allocator object.
* 
*			Any stack objects that are to be allocated in a zone tag memory should 
*			derive from this class.
**/
template <typename TagAllocator>
class shBaseObject {
	//! Static assert in case the TagAllocator is not derived from shZoneTagAllocator.
	static_assert( std::is_base_of<shZoneTagAllocator<TagAllocator::_zoneTag,
		TagAllocator::Malloc,
		TagAllocator::ReMalloc,
		TagAllocator::Free>,
	TagAllocator>::value );

private:
	//! The tag allocator object.
	//static inline TagAllocator _tagAllocator = TagAllocator();

	//! The zone tag that this object has been allocated in.
	static constexpr memtag_t _zoneTag = TagAllocator::_zoneTag;

public:
	/**
	*	For inheritance needs and proper constructing/destructing.
	**/
	//! Default Constructor.
	[[nodiscard]] shBaseObject() = default;
	//! Destructor.
	virtual ~shBaseObject() = default;

	/**
	*	Operator overloading so we can allocate the object using the Zone(Tag) memory.
	**/
	//! New Operator Overload.
	void *operator new( size_t size ) {
		return TagAllocator::Malloc( size, TagAllocator::_zoneTag );
	}
	//! Delete Operator Overload.
	void operator delete( void *ptr ) {
		TagAllocator::Free( ptr );
	}
	//! New Operator Overload.
	void *operator new[]( size_t size ) {
		return TagAllocator::Malloc( size, TagAllocator::_zoneTag );
	}
	//! Delete Operator Overload.
	void operator delete[]( void *ptr ) {
		TagAllocator::Z_TagFree( ptr );
	}
	#ifdef _SHOBJECT_DEBUG_ZONE_TAG_MEMORY
		//! New Operator Overload.
		void *operator new( size_t size, const char *file, int line ) {
			// Debug about the allocation.
			//gi.dprintf( "Allocating %d bytes in %s:%d\n", size, file, line );
			// Allocate.
			return TagAllocator::Malloc( size, TagAllocator::_zoneTag );
		}
		//! Delete Operator Overload.
		void operator delete( void *ptr, const char *file, int line ) {
			// Debug about the deallocation.
			//gi.dprintf( "Freeing %p in %s:%d\n", ptr, file, line );
			// Deallocate.
			TagAllocator::Free( ptr );
		}
		//! New Operator Overload.
		void *operator new[]( size_t size, const char *file, int line ) {
			// Debug about the allocation.
			//gi.dprintf( "Allocating %d bytes in %s:%d\n", size, file, line );
			// Allocate.
			return TagAllocator::Malloc( size, TagAllocator::_zoneTag );
		}
		//! Delete Operator Overload.
		void operator delete[]( void *ptr, const char *file, int line ) {
			// Debug about the deallocation.
			//gi.dprintf( "Freeing %p in %s:%d\n", ptr, file, line );
			// Deallocate.
			TagAllocator::Z_TagFree( ptr );
		}
	#endif
};