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

// Calculate linear interpolation between two floats
RMAPI const float QM_Lerp( const float start, const float end, const float amount ) {
    const float result = start + amount * ( end - start );

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

// Wrap input value from min to max
RMAPI const float QM_Wrap( const float value, const float min, const float max ) {
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