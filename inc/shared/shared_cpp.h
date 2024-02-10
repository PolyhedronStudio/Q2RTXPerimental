#pragma once

#if HAVE_CONFIG_H
#include "config_cpp.h"
#endif // HAVE_CONFIG_H

/********************************************************************
*
*
*	C++(20) Specific 'shared.h' header, included once near the top
*   of the C 'shared.h' file.
*
*
********************************************************************/
#include <type_traits>
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <numeric>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <random>
#include <string_view>
#include <vector>
#include <chrono>


/****
*
*	Typedefs/Types.
*
****/
// Byte type remains the same, just included here as well as in the else statement for consistency.
typedef unsigned char byte;

// QBoolean support(treat it as an int to remain compatible with the C code parts).
typedef int32_t qboolean;
static constexpr int32_t qtrue = true;
static constexpr int32_t qfalse = false;
//#define qfalse false;
//#define qtrue true;

// qhandle_t
typedef int qhandle_t;



/****
*
*
*	Q_ functions that need C++-ification.
*
*
****/
//extern "C" {
extern "C" size_t Q_concat_array(char* dest, size_t size, const char** arr);
static inline size_t Q_concat_stdarray(char* dest, size_t size, std::vector<const char*> arr) {
	return Q_concat_array(dest, size, arr.data());
}

// WID: The define replacement is found in shared_cpp.h for C++
#define Q_concat(dest, size, ...) \
	Q_concat_stdarray(dest, size, {__VA_ARGS__, NULL})
//};

/****
* 
*   Enum/Class Enum - Operator Overloads.
* 
*	A special template method in order to apply a set of operators to actual C enum types.
*   reducing the need to do specific and possibly wrong casts everywhere.
****/
// Operator: |
template <class E, class = std::enable_if_t < std::is_enum<E>{} >> 
static inline E operator | ( E & keydestA, E & keydestB ) {
	return static_cast<E>( static_cast<int>( keydestA ) | static_cast<int>( keydestB ) );
};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline E& operator | (E& keydestA, E& keydestB) {
	return static_cast<E>(static_cast<int>(keydestA) | static_cast<int>(keydestB));
};


// Operator: &
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline E operator & ( E& keydestA, E& keydestB ) {
	return static_cast<E>( static_cast<int>(keydestA) & static_cast<int>( keydestB ) );
};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline E& operator & (E& keydestA, E& keydestB) {
	return static_cast<E>(static_cast<int>(keydestA) & static_cast<int>(keydestB));
};

// Operator: ~
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline E operator ~ (E& keydestA) {
	return static_cast<E>(~static_cast<int>(keydestA));
};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline E & operator ~ (E& keydestA) {
	return static_cast<E>(~static_cast<int>(keydestA));
};

// Operator: |=
//template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
//E operator |= ( E& keydestA, E& keydestB ) {
//	return keydestA = static_cast<E>( static_cast<int>( keydestA ) | static_cast<int>( keydestB ) );
//};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline E& operator |= (E& keydestA, E& keydestB) {
	return keydestA = static_cast<E>(static_cast<int>(keydestA) | static_cast<int>(keydestB));
};
