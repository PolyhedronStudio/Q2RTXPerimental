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
*   Float Utilities:
*
*
**/
/**
*   @brief  Check whether two given floats are almost equal
**/
RMAPI int QM_FloatEquals( const float x, const float y ) {
	#if !defined(QM_EPSILON)
	#define QM_EPSILON 0.000001f
	#endif

	int result = ( fabsf( x - y ) ) <= ( QM_EPSILON * fmaxf( 1.0f, fmaxf( fabsf( x ), fabsf( y ) ) ) );

	return result;
}

/**
*   @brief  Clamps float value within -min/+max range.
**/
RMAPI float QM_Clampf( const float value, const float min, const float max ) {
	const float result = ( value < min ) ? min : value;
	//if ( result > max ) result = max;
	//result;
	return ( result > max ) ? max : result;
}
/**
*   @brief  Normalize input value within input range
**/
RMAPI float QM_Normalizef( const float value, const float start, const float end ) {
	const float result = ( value - start ) / ( end - start );

	return result;
}
/**
*   @brief  Remap input value within input range to output range
**/
RMAPI float QM_Remapf( const float value, const float inputStart, const float inputEnd, const float outputStart, const float outputEnd ) {
	const float result = ( value - inputStart ) / ( inputEnd - inputStart ) * ( outputEnd - outputStart ) + outputStart;

	return result;
}
/**
*   @brief  Wrap float input value from min to max
**/
RMAPI float QM_Wrapf( const float value, const float min, const float max ) {
	const float result = value - ( max - min ) * floorf( ( value - min ) / ( max - min ) );
	return result;
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
// WID: TODO: Oh boy..
RMAPI int QM_DoubleEquals( const double x, const double y ) {
    #if !defined(QM_DOUBLE_EPSILON)
    #define QM_DOUBLE_EPSILON QM_EPSILON
    #endif
	
	#ifdef __cplusplus
    int result = ( abs( x - y ) ) <= ( QM_DOUBLE_EPSILON * std::max<double>( 1.0, std::max<double>( abs( x ), abs( y ) ) ) );
	#else
	int result = ( abs( x - y ) ) <= ( QM_DOUBLE_EPSILON * max( 1.0, max( abs( x ), abs( y ) ) ) );
	#endif

    return result;
}

/**
*   @brief  Clamps double value within -min/+max range. 
**/
RMAPI double QM_Clampd( const double value, const double min, const double max ) {
    const double result = ( value < min ) ? min : value;
    //if ( result > max ) result = max;
    //result;
    return ( result > max ) ? max : result;
}
/**
*   @brief  Normalize input value within input range
**/
RMAPI double QM_Normalized( const double value, const double start, const double end ) {
    const double result = ( value - start ) / ( end - start );

    return result;
}
/**
*   @brief  Remap input value within input range to output range
**/
RMAPI double QM_Remapd( const double value, const double inputStart, const double inputEnd, const double outputStart, const double outputEnd ) {
    const double result = ( value - inputStart ) / ( inputEnd - inputStart ) * ( outputEnd - outputStart ) + outputStart;

    return result;
}
/**
*   @brief  Wrap double input value from min to max
**/
RMAPI double QM_Wrapd( const double value, const double min, const double max ) {
    const double result = value - ( max - min ) * floor( ( value - min ) / ( max - min ) );
    return result;
}



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
*   @brief  Clamps Int32 value within -min/+max range.
**/
RMAPI int32_t QM_ClampInt32( const int32_t value, const int32_t min, const int32_t max ) {
    const int32_t result = ( value < min || value < 0 ) ? min : value;
    return ( result > max ) ? max : result;
}
/**
*   @brief  Clamps Int32 value within -min/+max range.
**/
RMAPI uint32_t QM_ClampUint32( const uint32_t value, const uint32_t min, const uint32_t max ) {
    const uint32_t result = ( value < min /*|| value < 0*/ ) ? min : value;
    return ( result > max ) ? max : result;
}

/**
*   @brief  Wrap int input value from min to max
**/
RMAPI int32_t QM_WrapInt32( const int32_t value, const int32_t min, const int32_t max ) {
    const int32_t result = value - ( max - min ) * floor( ( value - min ) / ( max - min ) );
    return result;
}



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
/**
*   @brief  Slightly faster Lerp, might not be as precise.
**/
RMAPI float QM_FastLerp( const float start, const float end, const float amount ) {
    const float result = start + amount * ( end - start );

    return result;
}
/**
*   @brief  Slower lerp, but you specify back & front lerp separately.
**/
RMAPI float QM_LerpBackFront( const float start, const float end, const float backLerp, const float frontLerp ) {
    const float result = ( (start) * (backLerp) + (end) * (frontLerp) );

    return result;
}
/**
*   @brief  Slower lerp but is more mathematically precise.
**/
RMAPI float QM_Lerp( const float start, const float end, const float amount ) {
    const float result = ( ( start ) * (1.0f - amount)+( end ) * ( amount ) );

    return result;
}



/**
*
*
*   Angle(s) Utilities:
*
*
**/
/**
*   @brief  Returns the angle by clamping it within 0 to 359 degrees using modulation.
*           Useful to maintain the floating point angles precision.
**/
RMAPI float QM_AngleMod( const float a ) {
    // Float based method:
    const float v = fmod( a, 360.0f );

    if ( v < 0 ) {
        return 360.f + v;
    }

    return v;
}

/**
*   @brief  Will lerp between the euler angle, a2 and a1.
**/
RMAPI float QM_LerpAngle( float angle2, float angle1, const float fraction ) {
    if ( angle1 - angle2 > 180 ) {
        angle1 -= 360;
    }
    if ( angle1 - angle2 < -180 ) {
        angle1 += 360;
    }
    return angle2 + fraction * ( angle1 - angle2 );
}

/**
*   @brief  returns angle normalized to the range [-180 < angle <= 180]
**/
RMAPI float QM_Angle_Normalize180( const float angle ) {
    float _angle = fmod( ( angle ), 360.0f );
    if ( _angle > 180.0 ) {
        _angle -= 360.0;
    }
    return _angle;
}
/**
*   @brief  returns the normalized to range [-180 < angle <= 180] delta from angle1 to angle2
**/
RMAPI float QM_AngleDelta( const float angle1, const float angle2 ) {
    return QM_Angle_Normalize180( angle1 - angle2 );
}
