/********************************************************************
*
*
*	SharedGame: sg_string_t type, dynamically allocates the required
*				space and copies over the assigned char* string value.
* 
*				Also supports move operator, [] index access, as well 
*				as direct *ptr.
* 
*				When this object goes out of scope, or is assigned a
*				nullptr or an empty string, it will automatically do
*				a deallocation of the allocated memory.
*
*
********************************************************************/
#pragma once



template<typename T, int32_t tag>
struct sg_qtag_string_t {
	//! The actual pointer storing our string.
	T *ptr;
	//! The size in characters.
	size_t	count;

	/**
	*	Default constructor.
	**/
	constexpr sg_qtag_string_t() noexcept {
		ptr = nullptr;
		count = 0;
	}
	/**
	*	Default constructor, expecting a const T*.
	**/
	constexpr sg_qtag_string_t( const T *charStr ) noexcept {
		if ( !charStr ) {
			ptr = nullptr;
			count = 0;
			return;
		}
		// Acquire the count, add + 1 for end of string.
		count = strlen( charStr ) + 1;
		// Reallocate the tag ptr if we already had space allocated for it.
		if ( ptr ) {
			ptr = reinterpret_cast<T *>( SG_Z_TagReMalloc( ptr, size() ) );
		// Otherwise, allocate a new tag block.
		} else {
			ptr = reinterpret_cast<T *>( SG_Z_TagMallocz( size(), tag ) );
		}

		// Copy over string.
		if ( ptr ) {
			memcpy( ptr, charStr, size() );
			// Should not really ever happen!
		} else {
			count = 0;
			ptr = nullptr;
			SG_DPrintf( "%s: failed to allocate ptr for assigning string[\"%s\"], releasing this object now!\n", __func__, charStr );
		}

		//move.ptr = nullptr;
		//move.count = 0;
	}

private:
	/**
	*	Used for copy assignment.
	**/
	constexpr sg_qtag_string_t( T *ptr, size_t count ) noexcept :
		ptr( ptr ),
		count( count ) {
	}

public:
	/**
	*	Destructor, releases by gi.FreeTag
	**/
	inline ~sg_qtag_string_t() noexcept {
		release();
	}

	// Disable copying.
#if 0	
	constexpr sg_qtag_string_t( const sg_qtag_string_t & ) = delete;
	constexpr sg_qtag_string_t &operator=( const sg_qtag_string_t & ) = delete;
#else
	/**
	*	@brief	Will allocate enough space, and copy in the contents of charstr.
	**/
	constexpr sg_qtag_string_t( const sg_qtag_string_t &charstr ) noexcept {
		*this = sg_qtag_string_t<T, tag>::from_char_str( (const T *)charstr.ptr );
	}
	/**
	*	@brief	Will allocate enough space, and copy in the contents of charstr.
	**/
	constexpr sg_qtag_string_t &operator=( const sg_qtag_string_t &charstr ) noexcept {
		return *this = sg_qtag_string_t<T, tag>::from_char_str( (const T *)charstr.ptr );
	}
#endif

	/**
	*	@brief	Free move memory operation.
	**/
	constexpr explicit sg_qtag_string_t( sg_qtag_string_t &&move ) noexcept {
		ptr = move.ptr;
		count = move.count;

		move.ptr = nullptr;
		move.count = 0;
	}
	/**
	*	@brief	Free move assignment operation.
	**/
	constexpr sg_qtag_string_t &operator=( sg_qtag_string_t &&move ) noexcept {
		ptr = move.ptr;
		count = move.count;

		move.ptr = nullptr;
		move.count = 0;

		return *this;
	}

	/**
	*	@brief	Copy from const char* assignment operator.
	**/
	constexpr sg_qtag_string_t &operator=( const T *charStr ) noexcept {
		// Return this.
		if ( !charStr ) {
			// Release.
			release();
			// Return.
			return *this;
		}

		#if 0	// Dun work.
		*this = sg_qtag_string_t<T, tag>( charStr );
		#else
		// Acquire the count, add + 1 for end of string.
		count = strlen( charStr ) + 1;
		// Reallocate the tag ptr if we already had space allocated for it.
		if ( ptr ) {
			ptr = reinterpret_cast<T *>( SG_Z_TagReMalloc( ptr, size() ) );
		// Otherwise, allocate a new tag block.
		} else {
			ptr = reinterpret_cast<T *>( SG_Z_TagMallocz( size(), tag ) );
		}

		// Copy over string.
		if ( ptr ) {
			memcpy( ptr, charStr, size() );
		// Should not really ever happen!
		} else {
			count = 0;
			ptr = nullptr;
			SG_DPrintf( "%s: failed to allocate ptr for assigning string[\"%s\"], releasing this object now!\n", __func__, charStr );
		}
		#endif

		// Return.
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

	/**
	*	@brief	Allocates a block of sg_qtag_string_t for the specified count of characters.
	**/
	//inline sg_qtag_string_t<T, tag> from_char_str( const char *charStr ) {
	static inline sg_qtag_string_t<T, tag> new_for_size( size_t _count ) noexcept {
		// Empty null string.
		if ( _count <= 0 ) {
			return { nullptr, 0 };
		}
		// Allocate the space for the specified amount of characters and pass it into the sg_qtag_string_t.
		return { reinterpret_cast<T *>( SG_Z_TagMallocz( sizeof( T ) * _count + 1, tag ) ), _count };
	}
	/**
	*	@brief	Allocates a block of sg_qtag_string_t with a copy of the string argument.
	*	@note	For const char*
	**/
	//template<typename T = char, int32_t tag>
	//inline sg_qtag_string_t<T, tag> from_char_str( const char *charStr ) {
	static inline sg_qtag_string_t<T, tag> from_char_str( const char *charStr ) noexcept {
		// Empty null string.
		if ( !charStr ) {
			return { nullptr, 0 };
		}
		// Acquire size.
		size_t _count = strlen( charStr );
		// Empty null string.
		if ( _count <= 0 ) {
			return { nullptr, 0 };
		}
		// Add one for eol.
		//count += 1;

		// Allocate the space for the specified amount of characters.
		T *_ptr = reinterpret_cast<T *>( SG_Z_TagMallocz( sizeof( T ) * _count + 1, tag ) );
		// Copy over the string data.
		memcpy( _ptr, charStr, sizeof( T ) * _count );
		// Return a charstring_t with the designated pointer.
		return { _ptr, _count + 1 };
	}
	/**
	*	@brief	Allocates a block of sg_qtag_string_t with a copy of the string argument.
	*	@brief	For std::string.
	**/
	//inline sg_qtag_string_t<T, tag> from_char_str( const char *charStr ) {
	static inline sg_qtag_string_t<T, tag> from_std_string( const std::string &charStr ) noexcept {
		return sg_qtag_string_t<T, tag>::from_char_str( charStr.c_str() );
	}
};