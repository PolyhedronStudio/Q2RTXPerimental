/********************************************************************
*
*
*   Bounds Encoding/Packing:
*
*
********************************************************************/
#pragma once


#include "shared/zone_tags.h"

/**
*
*
*
*	Default System Memory Allocator and Deleter:
*
*
*
**/
/**
*	@brief	Allocator functor for TagMalloc.
*	@note	Provides static Allocate method compatible with QRAIIObject.
**/
template< typename TObject >
struct QTDefaultAllocator {
	/**
	*	@brief	Allocate memory using TagMalloc.
	*	@param	size	Size of memory block to allocate.
	*	@param	tag	Memory tag for tracking (e.g., TAG_SVGAME_LEVEL).
	*	@return	Pointer to allocated memory or nullptr on failure.
	**/
	static void *Allocate( const size_t size, const memtag_t tag = TAG_GENERAL ) {
		return new TObject[ size ];
	}
};

/**
*	@brief	Deleter functor for TagFree.
*	@note	Provides static Deallocate method compatible with QRAIIObject.
**/
struct QTDefaultDeleter {
	/**
	*	@brief	Free memory using TagFree.
	*	@param	ptr	Pointer to memory block to free.
	**/
	static void Deallocate( void *ptr ) {
		delete[ ] static_cast< char * >( ptr );
	}
};

/**
*
*
*
*	Generic RAII Helper for Tagged Memory:
*
*
*
**/
/**
*	@brief	Generic RAII helper for objects allocated via TagMalloc/TagFree.
*	@tparam	T	Type of object to manage (must have constructor/destructor).
*	@param	TAllocator	Allocator type (must provide static Allocate(size, tag) method).
* 	@param	TDeleter	Deleter type (must provide static Deallocate(ptr) method).
*	@note	Prevents raw TagMalloc/TagFree misuse by centralizing lifecycle management.
*			Ensures proper construction via placement-new and destruction before freeing.
*			Works with any type including those with STL members (vectors, maps, etc.).
*	@example
*		QRAIIObject<MyClass> owner;
*		if ( owner.create( TAG_SVGAME_LEVEL ) ) {
*			owner->DoSomething();
*		}
**/
template< typename TObject, typename TAllocator = QTDefaultAllocator<TObject>, typename TDeleter = QTDefaultDeleter >
class QRAIIObject {
public:
	/**
	*	Construction/Destruction:
	**/
	QRAIIObject() = default;

	~QRAIIObject() {
		reset();
	}

	/**
	*	Non-copyable:
	**/
	QRAIIObject( const QRAIIObject & ) = delete;
	QRAIIObject &operator=( const QRAIIObject & ) = delete;

	/**
	*	Movable:
	**/
	QRAIIObject( QRAIIObject &&other ) noexcept : _ptr( other._ptr ) {
		other._ptr = nullptr;
	}

	QRAIIObject &operator=( QRAIIObject &&other ) noexcept {
		if ( this != &other ) {
			reset();
			_ptr = other._ptr;
			other._ptr = nullptr;
		}
		return *this;
	}

	/**
	*	@brief	Create a new instance with default construction.
	*	@tparam	TTag	Tag type (e.g., memtag_t for TagMalloc).
	*	@param	tag	Memory tag for allocation.
	*	@return	True if allocation and construction succeeded.
	**/
	template<typename TTag>
	bool create( const TTag tag ) {
		/**
		*	Destroy any existing instance before creating a new one.
		**/
		reset();

		/**
		*	Allocate raw memory via allocator.
		**/
		void *const raw = TAllocator::Allocate( sizeof( TObject ), tag );
		if ( !raw ) {
			return false;
		}

		/**
		*	Construct via placement-new to run constructor.
		**/
		_ptr = new ( raw ) TObject();

		return true;
	}

	/**
	*	@brief	Create a new instance with custom initialization callback.
	*	@tparam	TTag	Tag type (e.g., memtag_t for TagMalloc).
	*	@tparam	InitFunc	Callable type (lambda, function pointer, etc.).
	*	@param	tag	Memory tag for allocation.
	*	@param	init_fn	Callback invoked after construction for custom initialization.
	*	@return	True if allocation, construction, and initialization succeeded.
	*	@example
	*		owner.create( TAG_SVGAME_LEVEL, []( nav_mesh_t *mesh ) {
	*			mesh->occupancy_frame = -1;
	*		} );
	**/
	template<typename TTag, typename InitFunc>
	bool create( const TTag tag, InitFunc &&init_fn ) {
		/**
		*	Perform default construction first.
		**/
		if ( !create( tag ) ) {
			return false;
		}

		/**
		*	Invoke custom initialization callback.
		**/
		init_fn( _ptr );

		return true;
	}

	/**
	*	@brief	Destroy the current instance if any.
	*	@note	Explicitly runs destructor before freeing memory to ensure proper cleanup.
	*			This is critical for types with non-trivial destructors (STL containers, etc.).
	**/
	void reset() {
		if ( _ptr ) {
			/**
			*	Explicitly invoke destructor to clean up object state.
			*	For types with STL members, this releases internal allocations.
			**/
			_ptr->~TObject();

			/**
			*	Free the tagged memory block via deleter.
			**/
			TDeleter::Deallocate( _ptr );
			_ptr = nullptr;
		}
	}

	/**
	*	@brief	Get raw pointer to the managed object.
	*	@return	Pointer to object or nullptr if not allocated.
	**/
	TObject *get() const {
		return _ptr;
	}

	/**
	*	@brief	Check if an object is allocated.
	*	@return	True if object exists.
	**/
	explicit operator bool() const {
		return _ptr != nullptr;
	}

	/**
	*	@brief	Access object members via pointer syntax.
	**/
	TObject *operator->() const {
		return _ptr;
	}

	/**
	*	@brief	Dereference to access object directly.
	**/
	TObject &operator*() const {
		return *_ptr;
	}

private:
	//! Owned object pointer (nullptr if not allocated).
	TObject *_ptr = nullptr;
};
