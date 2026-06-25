#pragma once

#include "nav_core.h"

/**
* @brief Small engine-backed vector used by the nav system.
* @note This avoids STL allocations for the containers that are persisted through the engine tag allocator.
**/
template <typename T>
class nav_vector_t {
private:
    //! Backing storage owned by the engine tag allocator.
    T *data = nullptr;
    //! Allocated element capacity.
    size_t _capacity = 0;
    //! Active element count.
    size_t _size = 0;

public:
    /**
    * @brief Construct an empty nav vector.
    **/
    nav_vector_t() = default;

    /**
    * @brief Release the backing storage.
    **/
    ~nav_vector_t() {
        if ( data ) {
            gi.TagFree( data );
            data = nullptr;
        }
    }

    /**
    * @brief Disable copying so ownership stays explicit.
    **/
    nav_vector_t( const nav_vector_t & ) = delete;
    /**
    * @brief Disable copying so ownership stays explicit.
    **/
    nav_vector_t &operator=( const nav_vector_t & ) = delete;

    /**
    * @brief Append one element, growing the container when needed.
    * @param item Value to copy into the array.
    **/
    void push_back( const T &item ) {
        if ( _size >= _capacity ) {
            size_t new_cap = ( _capacity == 0 ) ? 16 : _capacity * 2;
            T *new_data = ( T * )gi.TagMalloc( new_cap * sizeof( T ), TAG_SVGAME_NAVMESH );
            if ( data ) {
                memcpy( new_data, data, _size * sizeof( T ) );
                gi.TagFree( data );
            }
            data = new_data;
            _capacity = new_cap;
        }
        data[ _size++ ] = item;
    }

    /**
    * @brief Access an element by index.
    **/
    T &operator[]( size_t index ) { return data[ index ]; }
    /**
    * @brief Access an element by index.
    **/
    const T &operator[]( size_t index ) const { return data[ index ]; }

    /**
    * @brief Return the number of active elements.
    **/
    size_t size() const { return _size; }
    /**
    * @brief Return the allocated capacity.
    **/
    size_t capacity() const { return _capacity; }
    /**
    * @brief Reset the logical size without freeing memory.
    **/
    void clear() { _size = 0; }
    /**
    * @brief Return true when the container has no active elements.
    **/
    bool empty() const { return _size == 0; }

    /**
    * @brief Return a pointer to the first element.
    **/
    T *begin() { return data; }
    /**
    * @brief Return one-past-the-end for iteration.
    **/
    T *end() { return data + _size; }
    /**
    * @brief Return a const pointer to the first element.
    **/
    const T *begin() const { return data; }
    /**
    * @brief Return one-past-the-end for iteration.
    **/
    const T *end() const { return data + _size; }
    /**
    * @brief Return the raw backing pointer.
    **/
    T *get_data() { return data; }
    /**
    * @brief Return the raw backing pointer.
    **/
    const T *get_data() const { return data; }
};
