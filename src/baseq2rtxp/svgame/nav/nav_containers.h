#pragma once

#include "nav_core.h"

// A simple generic vector container utilizing the engine's memory allocation API.
// Avoids the STL dependency and ensures memory is tracked by the engine (TAG_SVGAME_NAVMESH).
template <typename T>
class nav_vector_t {
private:
    T* data;
    size_t _capacity;
    size_t _size;

public:
    nav_vector_t() : data(nullptr), _capacity(0), _size(0) {}
    
    ~nav_vector_t() {
        if (data) {
            gi.TagFree(data);
            data = nullptr;
        }
    }

    // Disable copying for safety, or implement properly if needed later.
    nav_vector_t(const nav_vector_t&) = delete;
    nav_vector_t& operator=(const nav_vector_t&) = delete;

    void push_back(const T& item) {
        if (_size >= _capacity) {
            size_t new_cap = _capacity == 0 ? 16 : _capacity * 2;
            T* new_data = (T*)gi.TagMalloc(new_cap * sizeof(T), TAG_SVGAME_NAVMESH);
            if (data) {
                // Since T is guaranteed to be POD for our navmesh types, memcpy is safe.
                memcpy(new_data, data, _size * sizeof(T));
                gi.TagFree(data);
            }
            data = new_data;
            _capacity = new_cap;
        }
        data[_size++] = item;
    }

    T& operator[](size_t index) { return data[index]; }
    const T& operator[](size_t index) const { return data[index]; }

    size_t size() const { return _size; }
    size_t capacity() const { return _capacity; }
    void clear() { _size = 0; }
    bool empty() const { return _size == 0; }

    T* begin() { return data; }
    T* end() { return data + _size; }
    const T* begin() const { return data; }
    const T* end() const { return data + _size; }
    T* get_data() { return data; }
    const T* get_data() const { return data; }
};
