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
*   Based on Q2RE.
**/
extern std::mt19937_64 mt_rand;

// uniform float/double [0, 1)
[[nodiscard]] inline const float frandom() {
    return std::uniform_real_distribution<float>()( mt_rand );
}
[[nodiscard]] inline const double drandom() {
    return std::uniform_real_distribution<double>()( mt_rand );
}

// uniform float/double [min_inclusive, max_exclusive)
[[nodiscard]] inline const float frandom( const float min_inclusive, const float max_exclusive ) {
    return std::uniform_real_distribution<float>( min_inclusive, max_exclusive )( mt_rand );
}
[[nodiscard]] inline const double drandom( const double min_inclusive, const double max_exclusive ) {
    return std::uniform_real_distribution<double>( min_inclusive, max_exclusive )( mt_rand );
}

// uniform float/double [0, max_exclusive)
[[nodiscard]] inline const float frandom( const float max_exclusive ) {
    return std::uniform_real_distribution<float>( 0, max_exclusive )( mt_rand );
}
[[nodiscard]] inline const double drandom( const double max_exclusive ) {
    return std::uniform_real_distribution<double>( 0, max_exclusive )( mt_rand );
}

// uniform float/double [-1, 1)
// note: closed on min but not max
// to match vanilla behavior
[[nodiscard]] inline const float crandomf() {
    return std::uniform_real_distribution<float>( -1.f, 1.f )( mt_rand );
}
[[nodiscard]] inline const double crandomd() {
    return std::uniform_real_distribution<double>( -1., 1. )( mt_rand );
}
// uniform float/double (-1, 1)
[[nodiscard]] inline const float crandom_openf() {
    return std::uniform_real_distribution<float>( std::nextafterf( -1.f, 0.f ), 1.f )( mt_rand );
}
[[nodiscard]] inline const double crandom_opend() {
    return std::uniform_real_distribution<double>( std::nextafterf( -1., 0. ), 1. )( mt_rand );
}


// raw unsigned int32 value from random
[[nodiscard]] inline uint32_t i32random() {
    return mt_rand();
}
// raw unsigned int32 value from random
[[nodiscard]] inline uint64_t i64random() {
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