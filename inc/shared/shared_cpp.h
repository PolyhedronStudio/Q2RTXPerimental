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
#if 1
//#define QENUM_OPERATORS( T ) \
//    constexpr T operator &( const T tFirst, const T tSecond ) {   \
//        return T( std::underlying_type<T>::type( tFirst ) & std::underlying_type<T>::type( tSecond ) );   \
//    }   \
//    constexpr T operator |( const T tFirst, const T tSecond ) {   \
//        return T( std::underlying_type<T>::type( tFirst ) | std::underlying_type<T>::type( tSecond ) );   \
//    }   \
//    constexpr T operator ^( const T tFirst, const T tSecond ) {   \
//        return T( std::underlying_type<T>::type( tFirst ) ^ std::underlying_type<T>::type( tSecond ) );   \
//    }   \
//    T operator ~( const T tFirst ) {   \
//        return T( ~std::underlying_type<T>::type( tFirst ) );   \
//    }   \
//    T &operator &=( T &tFirst, T &tSecond ) {   \
//        return tFirst = T( std::underlying_type<T>::type( tFirst ) & std::underlying_type<T>::type( tSecond ) );   \
//    }   \
//    T &operator |=( T &tFirst, T &tSecond ) {   \
//        return tFirst = T( std::underlying_type<T>::type( tFirst ) | std::underlying_type<T>::type( tSecond ) );   \
//    }   \
//    T &operator ^=( T &tFirst, T &tSecond ) {   \
//        return tFirst = T( std::underlying_type<T>::type( tFirst ) ^ std::underlying_type<T>::type( tSecond ) );   \
//    }

template<typename E, class = std::enable_if_t < std::is_enum< E >{} >> 
    inline constexpr E operator &( E tFirst, E tSecond ) {
    return E( std::underlying_type<const E>::type( tFirst ) & std::underlying_type<E>::type( tSecond ) );
}
template<typename E, class = std::enable_if_t < std::is_enum< E >{} >>
    inline constexpr E operator |( E tFirst, E tSecond ) {
    return E( std::underlying_type<const E>::type( tFirst ) | std::underlying_type<E>::type( tSecond ) );
}
template<typename E, class = std::enable_if_t < std::is_enum< E >{} >>
    inline constexpr E operator ^( E tFirst, E tSecond ) {
    return E( std::underlying_type<const E>::type( tFirst ) ^ std::underlying_type<E>::type( tSecond ) );
}
template<typename T, class = std::enable_if_t < std::is_enum< T >{} >>
    inline constexpr T operator ~( const T tFirst ) {
    return T( ~std::underlying_type<const T>::type( tFirst ) );
}
template<typename E, class = std::enable_if_t < std::is_enum< E >{} >>
    inline constexpr E & operator &=( E &tFirst, E &tSecond ) {
    return tFirst = E( std::underlying_type<E>::type( tFirst ) & std::underlying_type<E>::type( tSecond ) );
}
template<typename E, class = std::enable_if_t < std::is_enum< E >{} >>
    inline constexpr E &operator |=( E &tFirst, E &tSecond ) {
    return tFirst = E( std::underlying_type<E>::type( tFirst ) | std::underlying_type<E>::type( tSecond ) );
}
template<typename E, class = std::enable_if_t < std::is_enum< E >{}>> 
    inline constexpr E &operator ^=( E &tFirst, E &tSecond ) {
    return tFirst = E( std::underlying_type<E>::type( tFirst ) ^ std::underlying_type<E>::type( tSecond ) );
}
#else
// Operator: ==
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline const E operator == ( const E lha, const E rhb ) {
    return E( std::underlying_type<const E>::type( lha ) == std::underlying_type<const E>::type( rhb ) );
};
// Operator: !=
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline const E operator != ( const E lha, const E rhb ) {
    return E( std::underlying_type<const E>::type( lha ) != std::underlying_type<const E>::type( rhb ) );
};
// Operator: |
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline constexpr const E operator | ( const E lha, const E rhb ) {
    return E( std::underlying_type<const E>::type( lha ) | std::underlying_type<const E>::type( rhb ) );
};
// Operator: &
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline constexpr const E operator & ( const E lha, const E rhb ) {
    return E( std::underlying_type<const E>::type( lha ) & std::underlying_type<const E>::type( rhb ) );
};
// Operator: ~
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline constexpr E const &operator ~ ( E &lha ) {
    return lha = ~std::underlying_type<E>::type( E( std::underlying_type<E>::type( lha ) ) );
};
// Operator: ^
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline constexpr const E &operator ^ ( E &lha, E &rhb ) {
    return lha = E( std::underlying_type<const E>::type( lha ) ^ std::underlying_type<const E>::type( rhb ) );
};
// Operator: |=
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline E &operator |= ( E &lha, E &rhb ) {
    return lha = E( std::underlying_type<const E>::type( lha ) | std::underlying_type<const E>::type( rhb ) );
};
// Operator: &=
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline E &operator &= ( E &lha, E &rhb ) {
    return lha = E( std::underlying_type<const E>::type( lha ) & std::underlying_type<const E>::type( rhb ) );
};
// Operator: ^=
template <class E, class = std::enable_if_t < std::is_enum< const E >{} >>
    static inline E &operator ^= ( E &lha, E &rhb ) {
    return lha = E( std::underlying_type<const E>::type( lha ) ^ std::underlying_type<const E>::type( rhb ) );
};
#endif


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