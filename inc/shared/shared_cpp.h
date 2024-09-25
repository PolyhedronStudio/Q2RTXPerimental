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
// Operator: !=
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline constexpr bool operator != ( E keydestA, const int32_t keydestB ) {
    return static_cast<int32_t>( keydestA ) != static_cast<int32_t>( keydestB );
};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline constexpr bool operator != ( E keydestA, const uint32_t keydestB ) {
    return static_cast<uint32_t>( keydestA ) != static_cast<uint32_t>( keydestB );
};
// Operator: ==
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline constexpr bool operator == ( E keydestA, const int32_t keydestB ) {
    return static_cast<int32_t>( keydestA ) == static_cast<int32_t>( keydestB );
};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline constexpr bool operator == ( E keydestA, const uint32_t keydestB ) {
    return static_cast<uint32_t>( keydestA ) == static_cast<uint32_t>( keydestB );
};
// Operator: |
template <class E, class = std::enable_if_t < std::is_enum<E>{} >> 
static inline constexpr E operator | ( E &keydestA, E keydestB ) {
	return static_cast<E>( static_cast<int32_t>( keydestA ) | static_cast<int32_t>( keydestB ) );
};
//template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
//static inline E operator | ( E &keydestA, const E keydestB ) {
//    return static_cast<E>( static_cast<int>( keydestA ) | static_cast<int>( keydestB ) );
//};
// Operator: &
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline constexpr E operator & ( E& keydestA, E& keydestB ) {
	return static_cast<E>( static_cast<int32_t>(keydestA) & static_cast<int32_t>( keydestB ) );
};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline const constexpr E operator & ( const E& keydestA, const E& keydestB) {
	return static_cast<E>(static_cast<int32_t>(keydestA) & static_cast<int32_t>(keydestB));
};

// Operator: ~
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline constexpr E operator ~ (E& keydestA) {
	return static_cast<E>(~static_cast<int32_t>(keydestA));
};
template <class E, class = std::enable_if_t < std::is_enum<E>{} >>
static inline constexpr E & operator ~ (E& keydestA) {
	return static_cast<E>(~static_cast<int32_t>(keydestA));
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


/**
*   Q2RE: Random Number Utilities - For C++ translation units we want to use
*   these instead. Thus, residing in shared/shared_cpp.
**/
extern std::mt19937 mt_rand;

// uniform float [0, 1)
[[nodiscard]] inline float frandom() {
    return std::uniform_real_distribution<float>()( mt_rand );
}

// uniform float [min_inclusive, max_exclusive)
[[nodiscard]] inline float frandom( float min_inclusive, float max_exclusive ) {
    return std::uniform_real_distribution<float>( min_inclusive, max_exclusive )( mt_rand );
}

// uniform float [0, max_exclusive)
[[nodiscard]] inline float frandom( float max_exclusive ) {
    return std::uniform_real_distribution<float>( 0, max_exclusive )( mt_rand );
}

// uniform float [-1, 1)
// note: closed on min but not max
// to match vanilla behavior
[[nodiscard]] inline float crandom() {
    return std::uniform_real_distribution<float>( -1.f, 1.f )( mt_rand );
}

// uniform float (-1, 1)
[[nodiscard]] inline float crandom_open() {
    return std::uniform_real_distribution<float>( std::nextafterf( -1.f, 0.f ), 1.f )( mt_rand );
}

// raw unsigned int32 value from random
[[nodiscard]] inline uint32_t irandom() {
    return mt_rand();
}

// uniform int [min, max)
// always returns min if min == (max - 1)
// undefined behavior if min > (max - 1)
[[nodiscard]] inline int32_t irandom( int32_t min_inclusive, int32_t max_exclusive ) {
    if ( min_inclusive == max_exclusive - 1 )
        return min_inclusive;

    return std::uniform_int_distribution<int32_t>( min_inclusive, max_exclusive - 1 )( mt_rand );
}

// uniform int [0, max)
// always returns 0 if max <= 0
// note for Q2 code:
// - to fix rand()%x, do irandom(x)
// - to fix rand()&x, do irandom(x + 1)
[[nodiscard]] inline int32_t irandom( int32_t max_exclusive ) {
    if ( max_exclusive <= 0 )
        return 0;

    return irandom( 0, max_exclusive );
}

// uniform random index from given container
template<typename T>
[[nodiscard]] inline int32_t random_index( const T &container ) {
    return irandom( std::size( container ) );
}

// uniform random element from given container
template<typename T>
[[nodiscard]] inline auto random_element( T &container ) -> decltype( *std::begin( container ) ) {
    return *( std::begin( container ) + random_index( container ) );
}

// flip a coin
[[nodiscard]] inline bool brandom() {
    return irandom( 2 ) == 0;
}