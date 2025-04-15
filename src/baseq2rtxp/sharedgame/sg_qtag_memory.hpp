/********************************************************************
*
*
*	SharedGame: sg_qtag_memory_t type, a scoped utility for use with
*				dynamically allocated tag zone 'typed arrays'. 
*				
*				Allows for more easily saving variable sized array
*				data.
*
*				Also supports move operator, [] index access, as well
*				as direct *ptr.
*
*				When this object goes out of scope, or is assigned a
*				nullptr, it will automatically do a deallocation of 
*				the allocated memory.
*
*
********************************************************************/
#pragma once



template<typename T, const int32_t tag>
struct sg_qtag_memory_t {
	//! The actual pointer storing our block of memory..
	T *ptr = nullptr;
	//! The size in bytes.
	size_t	count = 0;

	/**
	*	Default constructor.
	**/
	constexpr sg_qtag_memory_t() {
		ptr = nullptr;
		count = 0;
	}

	/**
	*	Used for copy assignment.
	**/
	constexpr sg_qtag_memory_t( T *ptr, size_t count ) :
		ptr( ptr ),
		count( count ) {
	}

public:
	/**
	*	Destructor, releases by gi.FreeTag
	**/
	inline ~sg_qtag_memory_t() noexcept {
		release();
	}

	// Disable copying.
	//constexpr sg_qtag_memory_t( const sg_qtag_memory_t & ) = delete;
	//constexpr sg_qtag_memory_t &operator=( const sg_qtag_memory_t & ) = delete;

	/**
	*	@brief	Free move memory operation.
	**/
	constexpr sg_qtag_memory_t( sg_qtag_memory_t &&move ) noexcept {
		ptr = move.ptr;
		count = move.count;

		move.ptr = nullptr;
		move.count = 0;
	}
	/**
	*	@brief	Free move assignment operation.
	**/
	constexpr sg_qtag_memory_t &operator=( sg_qtag_memory_t &&move ) noexcept {
		ptr = move.ptr;
		count = move.count;

		move.ptr = nullptr;
		move.count = 0;

		return *this;
	}


	/**
	*	@brief	Releases tag allocated memory if we had any.
	**/
	inline void release() noexcept {
		if ( ptr ) {
			SG_Z_TagFree( ptr );
			count = 0;
			ptr = nullptr;
		}
	}

	/**
	*	Pointer Operators:
	**/
	constexpr explicit operator T *( ) { return ptr; }
	constexpr explicit operator const T *( ) { return ptr; }
	constexpr explicit operator const T *( ) const { return ptr; }


	/**
	*	Array Indexing Operators:
	**/
	constexpr std::add_lvalue_reference_t<T> operator[]( const size_t index ) { return ptr[ index ]; }
	constexpr const std::add_lvalue_reference_t<T> operator[]( const size_t index ) const { return ptr[ index ]; }

	/**
	*	Bool Equation Operator:
	**/
	constexpr operator bool() const { return !!ptr; }

	/**
	*	@brief	Return the size of the contained char array memory block.s
	**/
	constexpr size_t size() const noexcept { return count * sizeof( T ); }
};

/**
*	@brief	Allocates a block of sg_qtag_memory_t for the specified count of characters.
**/
//inline sg_qtag_memory_t<T, tag> from_char_str( const char *charStr ) {
template <typename _T, const int32_t _tag>
static inline sg_qtag_memory_t<_T, _tag> *allocate_qtag_memory( sg_qtag_memory_t<_T, _tag> *ptr, size_t _count ) noexcept {
	// Empty null string.
	if ( _count <= 0 ) {
		// Free what we had.
		SG_Z_TagFree( ptr->ptr );
		ptr->count = 0;
		return ptr;
	}
	// Acquire the count, add + 1 for end of string.
	ptr->count = _count;
	// Reallocate the tag ptr if we already had space allocated for it.
	if ( ptr->ptr ) {
		ptr->ptr = reinterpret_cast<_T *>( SG_Z_TagReMalloc( ptr->ptr, ptr->size() ) );
		// Otherwise, allocate a new tag block.
	} else {
		ptr->ptr = reinterpret_cast<_T *>( SG_Z_TagMallocz( ptr->size(), _tag ) );
	}

	// Return.
	return ptr;
}