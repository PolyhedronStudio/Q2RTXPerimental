/********************************************************************
*
*
*	C++(20) Specific 'shared.h' header, included once near the top
*   of the C 'shared.h' file.
*
*
********************************************************************/
#pragma once

#include <iterator>
#include <random>
#include <type_traits>
#include <vector>



/****
*
*
*
*   Neatly wraps up the enclosing and/or opening and closing of a block of
*   code that demands us to require linking as extern "C".
*
*
*
****/
#define QEXTERN_C_ENCLOSE(ENCLOSED_CODE) \
extern "C" {                             \
        ENCLOSED_CODE                    \
};                                       \
                                         \

#define QEXTERN_C_OPEN          \
extern "C" {                    \
                                \

#define QEXTERN_C_CLOSE         \
};                              \
                                \


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
#define QENUM_BIT_FLAGS(E)                                                                                      \
    inline constexpr E operator~( const E &tFirst ) {                                                          \
        return static_cast<E>( ~static_cast<std::underlying_type<E>::type>( tFirst ) );                         \
    }                                                                                                           \
    inline constexpr E operator |( const E &tFirst, const E &tSecond ) {                                                                        \
        return static_cast<E>( static_cast<std::underlying_type<E>::type>( tFirst ) | static_cast<std::underlying_type<E>::type>( tSecond ) );  \
    }                                                                                                                                           \
    inline constexpr E operator &( const E &tFirst, const E &tSecond ) {                                                                        \
        return static_cast<E>( static_cast<std::underlying_type<E>::type>( tFirst ) & static_cast<std::underlying_type<E>::type>( tSecond ) );  \
    }                                                                                                                                           \
    inline constexpr E operator ^( const E &tFirst, const E &tSecond ) {                                                                        \
        return static_cast<E>( static_cast<std::underlying_type<E>::type>( tFirst ) ^ static_cast<std::underlying_type<E>::type>( tSecond ) );  \
    }                                                                                                                                           \
    template< typename E2 = E, typename = std::enable_if_t< std::is_same_v< E2, E > > >             \
    inline constexpr E& operator|=( E &e, const E &e2 )                                             \
    {                                                                                               \
        e = e | e2;                                                                                 \
        return e;                                                                                   \
    }                                                                                               \
    template< typename E2 = E, typename = std::enable_if_t< std::is_same_v< E2, E > > >             \
    inline constexpr E& operator&=( E &e, const E &e2 )                                             \
    {                                                                                               \
        e = e & e2;                                                                                 \
        return e;                                                                                   \
    }                                                                                               \
    template< typename E2 = E, typename = std::enable_if_t< std::is_same_v< E2, E > > >             \
    inline constexpr E& operator^=( E &e, const E &e2 )                                             \
    {                                                                                               \
        e = e ^ e2;                                                                                 \
        return e;                                                                                   \
    }                                                                                               \
    template< typename E2 = E, typename = std::enable_if_t< std::is_same_v< E2, E > > >             \
    inline constexpr E& operator|=( E &e, E &e2 )                                             \
    {                                                                                               \
        e = e | e2;                                                                                 \
        return e;                                                                                   \
    }



/**
* 
* 
* 
*   Based on Q2RE:
* 
*	Random Number Generator Utilities:
*	Used to generate random numbers in a consistent way across the codebase, 
*	with a single global PRNG engine and helper functions for common distributions and ranges.
* 
* 
* 
**/
//! Global pseudo-random number generator engine used by the helper functions in this header.
//! Guard access to this engine if calling the helpers concurrently from multiple threads.
extern std::mt19937_64 mt_rand;

/**
*	@brief	Generate a uniformly distributed random `float` in the half-open range [0, 1).
*	@return	A `float` sampled uniformly from [0, 1).
*	@note	Uses the global `mt_rand` PRNG via `std::uniform_real_distribution<float>`. This function is not thread-safe — guard `mt_rand` if called concurrently.
*	@example
*	@code
*	float v = frandom();
*	if ( v >= 0.0f && v < 1.0f ) {
*		// use v
*	}
*	@endcode
*/
[[nodiscard]] inline const float frandom() {
    return std::uniform_real_distribution<float>()( mt_rand );
}
/**
*	@brief	Generate a uniformly distributed random `double` in the half-open range [0, 1).
*	@return	A `double` sampled uniformly from [0, 1).
*	@note	Uses the global `mt_rand` PRNG via `std::uniform_real_distribution<double>`. This function is not thread-safe — guard `mt_rand` if called concurrently.
*	@example
*	@code
*	double d = drandom();
*	// d is in [0,1)
*	@endcode
*/
[[nodiscard]] inline const double drandom() {
    return std::uniform_real_distribution<double>()( mt_rand );
}

/**
*	@brief	Generate a uniformly distributed `float` in the half-open range [min_inclusive, max_exclusive).
*	@param	min_inclusive	Lower bound of the range (inclusive).
*	@param	max_exclusive	Upper bound of the range (exclusive).
*	@return	A `float` sampled uniformly from [min_inclusive, max_exclusive).
*	@note	If `min_inclusive >= max_exclusive` the behavior is undefined. Uses global `mt_rand`.
*	@example
*	@code
*	float v = frandom( -1.0f, 1.0f );
*	// v in [-1, 1)
*	@endcode
*/
[[nodiscard]] inline const float frandom( const float min_inclusive, const float max_exclusive ) {
    return std::uniform_real_distribution<float>( min_inclusive, max_exclusive )( mt_rand );
}
/**
*	@brief	Generate a uniformly distributed `double` in the half-open range [min_inclusive, max_exclusive).
*	@param	min_inclusive	Lower bound of the range (inclusive).
*	@param	max_exclusive	Upper bound of the range (exclusive).
*	@return	A `double` sampled uniformly from [min_inclusive, max_exclusive).
*	@note	If `min_inclusive >= max_exclusive` the behavior is undefined. Uses global `mt_rand`.
*	@example
*	@code
*	double d = drandom( 0.5, 2.0 );
*	@endcode
*/
[[nodiscard]] inline const double drandom( const double min_inclusive, const double max_exclusive ) {
    return std::uniform_real_distribution<double>( min_inclusive, max_exclusive )( mt_rand );
}

/**
*	@brief	Generate a uniformly distributed `float` in the half-open range [0, max_exclusive).
*	@param	max_exclusive	Upper bound of the range (exclusive).
*	@return	A `float` sampled uniformly from [0, max_exclusive).
*	@note	If `max_exclusive <= 0` behavior is undefined for this overload; prefer calling with a positive `max_exclusive`.
*	@example
*	@code
*	float v = frandom( 5.0f ); // v in [0, 5)
*	@endcode
*/
[[nodiscard]] inline const float frandom( const float max_exclusive ) {
    return std::uniform_real_distribution<float>( 0, max_exclusive )( mt_rand );
}
/**
*	@brief	Generate a uniformly distributed `double` in the half-open range [0, max_exclusive).
*	@param	max_exclusive	Upper bound of the range (exclusive).
*	@return	A `double` sampled uniformly from [0, max_exclusive).
*	@example
*	@code
*	double d = drandom( 10.0 ); // d in [0, 10)
*	@endcode
*/
[[nodiscard]] inline const double drandom( const double max_exclusive ) {
    return std::uniform_real_distribution<double>( 0, max_exclusive )( mt_rand );
}

/**
*	@brief	Generate a `float` uniformly distributed in [-1, 1) (closed on -1, open on 1).
*	@return	A `float` in [-1, 1).
*	@note	Matches the classic "vanilla" behavior where -1 is possible but +1 is not.
*	@example
*	@code
*	float v = crandomf();
*	@endcode
*/
[[nodiscard]] inline const float crandomf() {
    return std::uniform_real_distribution<float>( -1.f, 1.f )( mt_rand );
}
/**
*	@brief	Generate a `double` uniformly distributed in [-1, 1) (closed on -1, open on 1).
*	@return	A `double` in [-1, 1).
*	@example
*	@code
*	double d = crandomd();
*	@endcode
*/
[[nodiscard]] inline const double crandomd() {
    return std::uniform_real_distribution<double>( -1., 1. )( mt_rand );
}
/**
*	@brief	Generate a `float` uniformly distributed in the open interval (-1, 1).
*	@return	A `float` in (-1, 1).
*	@note	This variant excludes both endpoints to avoid exactly -1 or +1 values.
*	@example
*	@code
*	float v = crandom_openf();
*	@endcode
*/
[[nodiscard]] inline const float crandom_openf() {
    return std::uniform_real_distribution<float>( std::nextafterf( -1.f, 0.f ), 1.f )( mt_rand );
}
/**
*	@brief	Generate a `double` uniformly distributed in the open interval (-1, 1).
*	@return	A `double` in (-1, 1).
*	@example
*	@code
*	double d = crandom_opend();
*	@endcode
*/
[[nodiscard]] inline const double crandom_opend() {
    return std::uniform_real_distribution<double>( std::nextafterf( -1., 0. ), 1. )( mt_rand );
}

/**
*	@brief	Return raw 32-bit unsigned random bits by sampling the global engine.
*	@return	A `uint32_t` containing pseudo-random bits.
*	@note	The underlying `mt_rand` is 64-bit; this function truncates to 32 bits.
*	@example
*	@code
*	uint32_t r = i32random();
*	@endcode
*/
[[nodiscard]] inline uint32_t i32random() {
    return mt_rand();
}
/**
*	@brief	Return raw 64-bit unsigned random bits by sampling the global engine.
*	@return	A `uint64_t` containing pseudo-random bits.
*	@example
*	@code
*	uint64_t r = i64random();
*	@endcode
*/
[[nodiscard]] inline uint64_t i64random() {
    return mt_rand();
}


/**
*	@brief	Generate a uniformly distributed 32-bit integer in the half-open range [min_inclusive, max_exclusive).
*	@param	min_inclusive	Lower bound inclusive.
*	@param	max_exclusive	Upper bound exclusive.
*	@return	An `int32_t` in [min_inclusive, max_exclusive).
*	@note	If `min_inclusive == max_exclusive - 1` this function always returns `min_inclusive`.
*	@note	Behavior is undefined if `min_inclusive > (max_exclusive - 1)`.
*	@example
*	@code
*	int32_t v = irandom( -5, 5 ); // v in [-5, 4]
*	@endcode
*/
[[nodiscard]] inline int32_t irandom( int32_t min_inclusive, int32_t max_exclusive ) {
    if ( min_inclusive == max_exclusive - 1 )
        return min_inclusive;

    return std::uniform_int_distribution<int32_t>( min_inclusive, max_exclusive - 1 )( mt_rand );
}



/**
*	@brief	Generate a uniformly distributed 32-bit integer in the half-open range [0, max_exclusive).
*	@param	max_exclusive	Upper bound exclusive.
*	@return	An `int32_t` in [0, max_exclusive). Returns 0 when `max_exclusive <= 0`.
*	@note	Always returns 0 if max <= 0
*	@note	For porting old Q2 maths: To fix rand()%x, do irandom(x)
*	@note	For porting old Q2 maths: To fix rand()&x, do irandom(x + 1)
*	@example
*	@code
*	int32_t v = irandom( 10 ); // v in [0,9]
*	@endcode
*/
[[nodiscard]] inline int32_t irandom( int32_t max_exclusive ) {
    if ( max_exclusive <= 0 )
        return 0;

    return irandom( 0, max_exclusive );
}

/**
*	@brief	Return a uniformly distributed random index for the given container.
*	@param	container	Container whose size will be used as the exclusive upper bound.
*	@tparam	T	Type of the container (must support `std::size`).
*	@return	An `int32_t` index in the range [0, std::size(container)).
*	@note	If the container is empty the behavior of `irandom(0)` yields 0 by the integer overload semantics.
*	@example
*	@code
*	std::vector<int> v = {1,2,3};
*	int32_t idx = random_index( v ); // idx in [0,3)
*	@endcode
*/
template<typename T>
[[nodiscard]] inline int32_t random_index( const T &container ) {
    return irandom( std::size( container ) );
}

/**
*	@brief	Return a random element from the provided container selected uniformly.
*	@param	container	Container to sample from. Must be indexable via iterator arithmetic.
*	@tparam	T	Type of the container.
*	@return	A reference to an element from the container (type deduced from `*std::begin(container)`).
*	@note	The container must be non-empty or the behavior is undefined.
*	@example
*	@code
*	std::vector<int> v = {10,20,30};
*	int &sample = random_element( v );
*	@endcode
*/
template<typename T>
[[nodiscard]] inline auto random_element( T &container ) -> decltype( *std::begin( container ) ) {
    return *( std::begin( container ) + random_index( container ) );
}

/**
*	@brief	Flip a fair pseudo-random coin.
*	@return	`true` or `false` with approximately equal probability.
*	@note	Implemented as `irandom(2) == 0` which samples {0,1} uniformly.
*	@example
*	@code
*	if ( brandom() ) {
*		// heads
*	} else {
*		// tails
*	}
*	@endcode
*/
[[nodiscard]] inline bool brandom() {
    return irandom( 2 ) == 0;
}
