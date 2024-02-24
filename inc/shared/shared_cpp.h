/********************************************************************
*
*
*	C++(20) Specific 'shared.h' header, included once near the top
*   of the C 'shared.h' file.
*
*
********************************************************************/
#pragma once

/****
*
*
*	Q_ functions that need C++-ification.
*
*
****/
/**
*	For C++, to use Q_concat_array for its C++ vector container version. See Q_concat below:
**/
extern "C" size_t Q_concat_array(char* dest, size_t size, const char** arr);
static inline size_t Q_concat_stdarray(char* dest, size_t size, std::vector<const char*> arr) {
	return Q_concat_array(dest, size, arr.data());
}
/**
*	The final define for concat implementation:
**/
#define Q_concat(dest, size, ...) \
	Q_concat_stdarray(dest, size, {__VA_ARGS__, NULL})


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
