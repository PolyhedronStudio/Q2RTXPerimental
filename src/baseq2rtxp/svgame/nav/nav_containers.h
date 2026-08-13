#pragma once

#include "nav_core.h"
#include <utility>
#include <new>
#include <vector>
#include <unordered_map>
#include <cmath>

/**
*	@brief	Small engine-backed vector used by the nav system.
*	@note	This avoids STL allocations for containers persisted through the engine tag allocator.
**/
template <typename T>
class nav_vector_t {
private:
	//! Pointer to engine-tagged allocated memory.
	T *data = nullptr;
	//! Allocated capacity in element count.
	size_t _capacity = 0;
	//! Active element count.
	size_t _size = 0;

	/**
	*	@brief	Perform deep copy of backing array elements from another nav_vector_t.
	*	@param	other	Vector to copy from.
	**/
	void CopyFrom( const nav_vector_t &other ) {
		if ( data ) {
			gi.TagFree( data );
			data = nullptr;
		}
		_capacity = other._capacity;
		_size = other._size;
		if ( _capacity > 0 ) {
			data = ( T * )gi.TagMalloc( _capacity * sizeof( T ), TAG_SVGAME_NAVMESH );
			if ( _size > 0 ) {
				memcpy( data, other.data, _size * sizeof( T ) );
			}
		}
	}

public:
	/**
	*	@brief	Construct an empty nav vector.
	**/
	nav_vector_t() = default;

	/**
	*	@brief	Release the backing storage.
	**/
	~nav_vector_t() {
		if ( data ) {
			gi.TagFree( data );
			data = nullptr;
		}
	}

	/**
	*	@brief	Copy constructor performing deep copy of heap elements.
	*	@param	other	Source vector to copy.
	**/
	nav_vector_t( const nav_vector_t &other ) {
		CopyFrom( other );
	}

	/**
	*	@brief	Copy assignment operator performing deep copy of heap elements.
	*	@param	other	Source vector to copy.
	*	@return	Reference to this vector.
	**/
	nav_vector_t &operator=( const nav_vector_t &other ) {
		if ( this != &other ) {
			CopyFrom( other );
		}
		return *this;
	}


	/**
	*	@brief	Move-construct from an existing nav_vector_t.
	*	@param	other	Vector to move backing memory from.
	**/
	nav_vector_t( nav_vector_t &&other ) noexcept {
		data = other.data;
		_capacity = other._capacity;
		_size = other._size;
		other.data = nullptr;
		other._capacity = 0;
		other._size = 0;
	}

	/**
	*	@brief	Move-assign from an existing nav_vector_t.
	*	@param	other	Vector to steal backing memory from.
	*	@return	Reference to this vector.
	**/
	nav_vector_t &operator=( nav_vector_t &&other ) noexcept {
		if ( this != &other ) {
			if ( data ) {
				gi.TagFree( data );
			}
			data = other.data;
			_capacity = other._capacity;
			_size = other._size;
			other.data = nullptr;
			other._capacity = 0;
			other._size = 0;
		}
		return *this;
	}

	/**
	*	@brief	Pre-allocate backing storage to hold at least new_cap elements.
	*	@param	new_cap	Minimum capacity to ensure.
	**/
	void reserve( size_t new_cap ) {
		if ( new_cap <= _capacity ) {
			return;
		}
		T *new_data = ( T * )gi.TagMalloc( new_cap * sizeof( T ), TAG_SVGAME_NAVMESH );
		if ( data ) {
			if ( _size > 0 ) {
				memcpy( new_data, data, _size * sizeof( T ) );
			}
			gi.TagFree( data );
		}
		data = new_data;
		_capacity = new_cap;
	}

	/**
	*	@brief	Resize vector, growing capacity if needed and updating active size.
	*	@param	new_size	New active element count.
	**/
	void resize( size_t new_size ) {
		if ( new_size > _capacity ) {
			reserve( new_size );
		}
		_size = new_size;
	}

	/**
	*	@brief	Append one element, growing the container when needed.
	*	@param	item	Value to copy into the array.
	**/

	void push_back( const T &item ) {
		if ( _size >= _capacity ) {
			size_t new_cap = ( _capacity == 0 ) ? 16 : _capacity * 2;
			reserve( new_cap );
		}
		data[ _size++ ] = item;
	}

	/**
	*	@brief	Construct an element in-place at the end of the container.
	*	@param	args	Arguments forwarded to element constructor.
	*	@return	Reference to the newly constructed element.
	**/
	template <typename... Args>
	T &emplace_back( Args &&... args ) {
		if ( _size >= _capacity ) {
			size_t new_cap = ( _capacity == 0 ) ? 16 : _capacity * 2;
			reserve( new_cap );
		}
		new ( &data[ _size ] ) T( std::forward<Args>( args )... );
		return data[ _size++ ];
	}

	/**
	*	@brief	Reallocate backing buffer to fit active element count exactly.
	**/
	void shrink_to_fit() {
		if ( _size == _capacity ) {
			return;
		}
		if ( _size == 0 ) {
			if ( data ) {
				gi.TagFree( data );
				data = nullptr;
			}
			_capacity = 0;
			return;
		}
		T *new_data = ( T * )gi.TagMalloc( _size * sizeof( T ), TAG_SVGAME_NAVMESH );
		memcpy( new_data, data, _size * sizeof( T ) );
		gi.TagFree( data );
		data = new_data;
		_capacity = _size;
	}

	/**
	*	@brief	Access an element by index.
	**/
	T &operator[]( size_t index ) { return data[ index ]; }
	/**
	*	@brief	Access an element by index.
	**/
	const T &operator[]( size_t index ) const { return data[ index ]; }

	/**
	*	@brief	Return a reference to the first element in the container.
	**/
	T &front() { return data[ 0 ]; }
	/**
	*	@brief	Return a const reference to the first element in the container.
	**/
	const T &front() const { return data[ 0 ]; }

	/**
	*	@brief	Return a reference to the last element in the container.
	**/
	T &back() { return data[ _size - 1 ]; }
	/**
	*	@brief	Return a const reference to the last element in the container.
	**/
	const T &back() const { return data[ _size - 1 ]; }

	/**
	*	@brief	Remove the last element in the container.
	**/
	void pop_back() {
		if ( _size > 0 ) {
			_size--;
		}
	}


	/**
	*	@brief	Return the number of active elements.
	**/
	size_t size() const { return _size; }
	/**
	*	@brief	Return the allocated capacity.
	**/
	size_t capacity() const { return _capacity; }
	/**
	*	@brief	Reset the logical size without freeing memory.
	**/
	void clear() { _size = 0; }
	/**
	*	@brief	Return true when the container has no active elements.
	**/
	bool empty() const { return _size == 0; }

	/**
	*	@brief	Return a pointer to the first element.
	**/
	T *begin() { return data; }
	/**
	*	@brief	Return one-past-the-end for iteration.
	**/
	T *end() { return data + _size; }
	/**
	*	@brief	Return a const pointer to the first element.
	**/
	const T *begin() const { return data; }
	/**
	*	@brief	Return one-past-the-end for iteration.
	**/
	const T *end() const { return data + _size; }
	/**
	*	@brief	Return the raw backing pointer.
	**/
	T *get_data() { return data; }
	/**
	*	@brief	Return the raw backing pointer.
	**/
	const T *get_data() const { return data; }

	/**
	*	@brief	Remove an element at the given index by shifting subsequent elements.
	*	@param	index	Zero-based index of the element to remove.
	**/
	void erase_at( size_t index ) {
		if ( index >= _size ) {
			return;
		}
		if ( index < _size - 1 ) {
			memmove( data + index, data + index + 1, ( _size - 1 - index ) * sizeof( T ) );
		}
		_size--;
	}

	/**
	*	@brief	Remove an element pointed to by an iterator or pointer.
	*	@param	it	Pointer to the element to remove.
	**/
	void erase( const T *it ) {
		if ( !it || it < data || it >= data + _size ) {
			return;
		}
		size_t index = static_cast< size_t >( it - data );
		erase_at( index );
	}
};

/**
*	@brief	Generic 3D spatial hash grid container for fast spatial proximity queries.
*	@tparam	TItem	Type of identifier or data stored in grid cells.
**/
template <typename TItem>
class nav_spatial_grid_t {
private:
	//! Grid cell spatial extents (in Quake units).
	double cell_size = 128.0;
	//! Backing map mapping 64-bit spatial cell key to item vectors.
	std::unordered_map<uint64_t, std::vector<TItem>> grid;

	/**
	*	@brief	Compute 64-bit hash key from 3D integer cell coordinates.
	**/
	inline uint64_t HashCellKey( int64_t cx, int64_t cy, int64_t cz ) const {
		return ( static_cast<uint64_t>( cx ) * 73856093ULL ) ^
			( static_cast<uint64_t>( cy ) * 19349663ULL ) ^
			( static_cast<uint64_t>( cz ) * 83492791ULL );
	}

public:
	nav_spatial_grid_t() = default;
	explicit nav_spatial_grid_t( double in_cell_size ) : cell_size( in_cell_size ) {}

	/**
	*	@brief	Reset grid contents and set cell dimension.
	**/
	void Initialize( double in_cell_size ) {
		cell_size = in_cell_size;
		grid.clear();
	}

	/**
	*	@brief	Clear all spatial items from grid cells.
	**/
	void Clear() {
		grid.clear();
	}

	/**
	*	@brief	Insert an item at a specific 3D point location.
	**/
	void InsertPoint( const Vector3DP &point, const TItem &item ) {
		const int64_t cx = static_cast<int64_t>( std::floor( point.x / cell_size ) );
		const int64_t cy = static_cast<int64_t>( std::floor( point.y / cell_size ) );
		const int64_t cz = static_cast<int64_t>( std::floor( point.z / cell_size ) );
		grid[ HashCellKey( cx, cy, cz ) ].push_back( item );
	}

	/**
	*	@brief	Insert an item into all grid cells overlapping an AABB range.
	**/
	void InsertBox( const Vector3DP &mins, const Vector3DP &maxs, const TItem &item ) {
		const int64_t min_x = static_cast<int64_t>( std::floor( mins.x / cell_size ) );
		const int64_t min_y = static_cast<int64_t>( std::floor( mins.y / cell_size ) );
		const int64_t min_z = static_cast<int64_t>( std::floor( mins.z / cell_size ) );

		const int64_t max_x = static_cast<int64_t>( std::floor( maxs.x / cell_size ) );
		const int64_t max_y = static_cast<int64_t>( std::floor( maxs.y / cell_size ) );
		const int64_t max_z = static_cast<int64_t>( std::floor( maxs.z / cell_size ) );

		for ( int64_t cx = min_x; cx <= max_x; ++cx ) {
			for ( int64_t cy = min_y; cy <= max_y; ++cy ) {
				for ( int64_t cz = min_z; cz <= max_z; ++cz ) {
					grid[ HashCellKey( cx, cy, cz ) ].push_back( item );
				}
			}
		}
	}

	/**
	*	@brief	Execute a callback for all items located in cells overlapping an AABB range.
	*	@tparam	Visitor	Lambda or function object accepting (const TItem &).
	**/
	template <typename Visitor>
	void QueryBox( const Vector3DP &mins, const Vector3DP &maxs, Visitor &&visitor ) const {
		const int64_t min_x = static_cast<int64_t>( std::floor( mins.x / cell_size ) );
		const int64_t min_y = static_cast<int64_t>( std::floor( mins.y / cell_size ) );
		const int64_t min_z = static_cast<int64_t>( std::floor( mins.z / cell_size ) );

		const int64_t max_x = static_cast<int64_t>( std::floor( maxs.x / cell_size ) );
		const int64_t max_y = static_cast<int64_t>( std::floor( maxs.y / cell_size ) );
		const int64_t max_z = static_cast<int64_t>( std::floor( maxs.z / cell_size ) );

		for ( int64_t cx = min_x; cx <= max_x; ++cx ) {
			for ( int64_t cy = min_y; cy <= max_y; ++cy ) {
				for ( int64_t cz = min_z; cz <= max_z; ++cz ) {
					auto it = grid.find( HashCellKey( cx, cy, cz ) );
					if ( it != grid.end() ) {
						for ( const auto &item : it->second ) {
							visitor( item );
						}
					}
				}
			}
		}
	}
};

