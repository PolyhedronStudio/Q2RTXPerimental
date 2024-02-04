/**
*
*
*   Only to be included by qray_math.h: file contains math utility function implementations.
*
*
**/
#pragma once

// Clamp float value
RMAPI const float QM_Clamp( const float value, const float min, const float max ) {
    const float result = ( value < min ) ? min : value;
    //if ( result > max ) result = max;
    //result;
    return ( result > max ) ? max : result;
}

// slightly faster lerp that may not be as precise
#define FASTLERP(a, b, c)   ((a)+(c)*((b)-(a)))
// slower lerp, but you specify back & front lerp separately
#define LERP2(a, b, c, d)   ((a)*(c)+(b)*(d))
// slower lerp but is more mathematically precise
#define LERP(a, b, c)       LERP2((a), (b), (1.0f - (c)), (c))

//                cl.refdef.fog.p = LERP2(cl.fog.start.p, cl.fog.end.p, fog_backlerp, fog_frontlerp)

// Slightly faster Lerp, might not be as precise.
RMAPI const float QM_FastLerp( const float start, const float end, const float amount ) {
    const float result = start + amount * ( end - start );

    return result;
}

// Slower lerp, but you specify back & front lerp separately.
RMAPI const float QM_LerpBackFront( const float start, const float end, const float backLerp, const float frontLerp ) {
    const float result = ( (start) * (backLerp) +  (end) * (frontLerp) );

    return result;
}

// Slower lerp but is more mathematically precise.
RMAPI const float QM_Lerp( const float start, const float end, const float amount ) {
    const float result = ( ( start ) * (1.0f - amount)+( end ) * ( amount ) );

    return result;
}

// Normalize input value within input range
RMAPI const float QM_Normalize( const float value, const float start, const float end ) {
    const float result = ( value - start ) / ( end - start );

    return result;
}

// Remap input value within input range to output range
RMAPI const float QM_Remap( const float value, const float inputStart, const float inputEnd, const float outputStart, const float outputEnd ) {
    const float result = ( value - inputStart ) / ( inputEnd - inputStart ) * ( outputEnd - outputStart ) + outputStart;
    
    return result;
}

// Wrap int input value from min to max
RMAPI const int32_t QM_Wrapi( const int32_t value, const int32_t min, const int32_t max ) {
    const int32_t result = value - ( max - min ) * floorf( ( value - min ) / ( max - min ) );
    return result;
}
// Wrap float input value from min to max
RMAPI const float QM_Wrapf( const float value, const float min, const float max ) {
    const float result = value - ( max - min ) * floorf( ( value - min ) / ( max - min ) );
    return result;
}

// Check whether two given floats are almost equal
RMAPI int QM_FloatEquals( const float x, const float y ) {
    #if !defined(QM_EPSILON)
    #define QM_EPSILON 0.000001f
    #endif

    int result = ( fabsf( x - y ) ) <= ( QM_EPSILON * fmaxf( 1.0f, fmaxf( fabsf( x ), fabsf( y ) ) ) );

    return result;
}

/**
*   @brief  Returns the angle by clamping it within 0 to 359 degrees using modulation.
*           Useful to maintain the floating point angles precision.
**/
RMAPI const float QM_AngleMod( const float a ) {
    // Float based method:
    const float v = fmod( a, 360.0f );

    if ( v < 0 ) {
        return 360.f + v;
    }

    return v;
}