/**
*
*
*   Only to be included by qray_math.h: file contains math utility function implementations.
*
*
**/
#pragma once



/**
*
*
*   Templated Utilities:
*
*
**/
/**
*   @brief  Templated Epsilon types.
**/
template <typename T>
struct qm_epsilon_type { };
template <> struct qm_epsilon_type<float> { static constexpr float value{ QM_FLOAT_EPSILON }; };
template <> struct qm_epsilon_type<double> { static constexpr double value{ QM_DOUBLE_EPSILON }; };

/**
*   @brief  Templated Type Comparisons.
**/
//! Returns false.
template<template<typename...> class TypeA, template<typename...> class TypeB>
struct qm_is_same_template_type : std::false_type { };
//! Returns true.
template<template<typename...> class Type>
struct qm_is_same_template_type<Type, Type> : std::true_type { };



/**
*
*
*   Math Utilities:
*
*
**/
/**
*   @brief  Returns 'min' if 'value' < min, otherwise returns 'value'.
**/
template<typename T>
QM_API_CONSTEXPR T QM_Min( const T &min, const T &value ) {
    return value < min ? min : value;
}
/**
*   @brief  Returns 'max' if 'value' > max, otherwise returns 'value'.
**/
template<typename T>
QM_API_CONSTEXPR T QM_Max( const T &min, const T &value ) {
    return value < min ? min : value;
}



/**
*   @brief  Clamps given value type within -min/+max range.
**/
template<typename T> 
QM_API_CONSTEXPR T QM_Clamp( const T &value, const T &min, const T &max ) {
	return value < min ? min : ( value > max ? max : value );
}
/**
*   @brief  Clamps given value type within >= 0 and <= 1 range.
**/
template<typename T>
QM_API_CONSTEXPR T QM_Clamp01( const T &value ) {
    //return value < min ? min : ( value > max ? max : value );
	return QM_Clamp<T>( value, static_cast<T>(0.0), static_cast<T>( 1.0 ) );
}
/**
*   @brief  Clamps given value type within >= 0 and (0+min/0+max) range.
**/
template<typename T, typename T2, typename T3>
QM_API_CONSTEXPR T QM_ClampUnsigned( const T &value, const T2 &min, const T3 &max ) {
    // We need the return type to be unsigned.
    static_assert( std::is_unsigned_v<T> == true, "Typename T is required to be of unsigned type." );
    //return value < min ? min : ( value > max ? max : value );
    return std::clamp<T>( value, static_cast<T>( min < 0 ? 0 : 0 + min ), static_cast<T>( max ) );
}


/**
*   @brief  Normalize input value within input range
**/
template<typename T>
QM_API_CONSTEXPR T QM_Normalize( const T &value, const T &start, const T &end ) {
    return static_cast<T>( ( value - start ) / ( end - start ) );
}
/**
*   @brief  Remap input value within input range to output range
**/
template<typename T>
QM_API_CONSTEXPR T QM_Remap( const T &value, const T &inputStart, const T &inputEnd, const T &outputStart, const T &outputEnd ) {
    return static_cast<T>( ( value - inputStart ) / ( inputEnd - inputStart ) * ( outputEnd - outputStart ) + outputStart );
}
/**
*   @brief  Wrap float input value from min to max
**/
template<typename T>
QM_API_CONSTEXPR T QM_Wrap( const T &value, const T &min, const T &max ) {
    return value - ( max - min ) * std::floor( static_cast<T>( ( value - min ) / ( max - min ) ) );
}



/**
*   @brief  Slightly faster Lerp, might not be as precise.
**/
template<typename T, typename T1, typename T2, typename T3>
QM_API_CONSTEXPR T QM_FastLerp( const T1 &start, const T2 &end, const T3 &amount ) {
    return static_cast<T>( start + amount * ( end - start ) );
}
/**
*   @brief  Slower lerp, but you specify back & front lerp separately.
**/
template<typename T, typename T1, typename T2, typename T3, typename T4>
QM_API_CONSTEXPR T QM_LerpBackFront( const T1 &start, const T2 &end, const T3 &backLerp, const T4 &frontLerp ) {
    return static_cast<T>( ( start ) * (backLerp)+( end ) * ( frontLerp ) );
}
/**
*   @brief  Slower lerp but is more mathematically precise.
**/
template<typename T, typename T1, typename T2, typename T3>
QM_API_CONSTEXPR T QM_Lerp( const T1 &start, const T2 &end, const T3 &amount ) {
    return static_cast<T>( ( start ) * ( static_cast<T>( 1.0 ) - amount ) + ( end ) * ( amount ) );
}

/**
*   @brief  Will lerp between the euler angle, a2 and a1.
**/
template<typename T>
QM_API_CONSTEXPR T QM_LerpAngle( const T &angle2, const T &angle1, const T &fraction ) {
    //T _angle1 = angle1;
    //if ( angle1 - angle2 > 180. ) {
    //    _angle1 -= 360.;
    //}
    //if ( angle1 - angle2 < -180 ) {
    //    _angle1 += 360;
    //}
    return angle2 + fraction * (
        ( angle1 - angle2 > 180 ? angle1 -= static_cast<T>(360.) : angle1 ) 
        - ( angle1 - angle2 < -180 ? angle1 += static_cast<T>(360.) : angle1 )
        - angle2 
    );
}



/**
*
*
*   Double/Float Epsilon Checks( Expensive, only use if necessary for precision. ):
*
*
**/
/**
*   @brief  Check whether two values are almost equal based on their Epislon types.
**/
template<typename T, T Epsilon = qm_epsilon_type<T>::value>
QM_API_CONSTEXPR T QM_EqualsEpsilon( const T &x, const T &y ) {
    using std::abs;
    using std::max;
    using std::min;
    //static constexpr T Epsilon = qm_epsilon_type<T>::value;
    return static_cast<T>( std::abs( x - y ) ) <= ( Epsilon * std::max( static_cast<T>(1.0), std::max( std::abs( x ), std::abs( y ) ) ) );
}



/**
*
*
*   Double Utilities:
*
*
**/
/**
*   @brief  Check whether two given floats are almost equal
**/
//// WID: TODO: Oh boy..
//QM_API int QM_DoubleEquals( const double x, const double y ) {
//    #if !defined(QM_DOUBLE_EPSILON)
//    #define QM_DOUBLE_EPSILON QM_EPSILON
//    #endif
//	
//	#ifdef __cplusplus
//    int result = ( abs( x - y ) ) <= ( QM_DOUBLE_EPSILON * std::max<double>( 1.0, std::max<double>( abs( x ), abs( y ) ) ) );
//	#else
//	int result = ( abs( x - y ) ) <= ( QM_DOUBLE_EPSILON * max( 1.0, max( abs( x ), abs( y ) ) ) );
//	#endif
//
//    return result;
//}



/**
*
*
*   Int16/Uint16/ Utilities:
*
*
**/
// ..



/**
*
*
*   Int32/Uint32/ Utilities:
*
*
**/




/**
*
*
*   Int64/Uint64/ Utilities:
*
*
**/
// ..



/**
*
*
*   LERP Utilities:
*
*
**/
///**
//*   @brief  Slightly faster Lerp, might not be as precise.
//**/
//QM_API float QM_FastLerp( const float start, const float end, const float amount ) {
//    const float result = start + amount * ( end - start );
//
//    return result;
//}
///**
//*   @brief  Slower lerp, but you specify back & front lerp separately.
//**/
//QM_API float QM_LerpBackFront( const float start, const float end, const float backLerp, const float frontLerp ) {
//    const float result = ( (start) * (backLerp) + (end) * (frontLerp) );
//
//    return result;
//}
///**
//*   @brief  Slower lerp but is more mathematically precise.
//**/
//QM_API float QM_Lerp( const float start, const float end, const float amount ) {
//    const float result = ( ( start ) * (1.0f - amount)+( end ) * ( amount ) );
//
//    return result;
//}



/**
*
*
*   Angle(s) Utilities:
*
*
**/
/**
*   @brief  Returns the angle by clamping it within 0 to < 360. degrees using modulation.
*           Useful to maintain the floating point angles precision.
**/
template<typename T>
QM_API_CONSTEXPR T QM_AngleMod( const T &value ) {
    const T v = std::fmod<T>( value, static_cast<T>( 360.0 ) );
    if ( v < static_cast<T>( 0. ) ) {
        return static_cast<T>( 360. ) + v;
    }
    return v;//return value < ( static_cast<T>( 0. ) ? static_cast<T>( 360. ) : static_cast<T>( 0. ) ) + value;
}



/**
*   @brief  returns angle normalized to the range [-180 < angle <= 180]
**/
template<typename T>
QM_API_CONSTEXPR T QM_Angle_Normalize180( const T &angle ) {
    T _angle = std::fmod<T>( ( angle ), static_cast<T>( 360.0 ) );
    if ( _angle > static_cast<T>( 180.0 ) ) {
        _angle -= static_cast<T>( 360.0 );
    }
    return _angle;
}
/**
*   @brief  returns the normalized to range [-180 < angle <= 180] delta from angle1 to angle2
**/
template<typename T>
QM_API_CONSTEXPR T QM_AngleDelta( const T &angle1, const T &angle2 ) {
    return QM_Angle_Normalize180<T>( angle1 - angle2 );
}
