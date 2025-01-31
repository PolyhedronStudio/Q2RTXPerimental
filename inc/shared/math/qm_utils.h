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
RMAPI const float QM_Clampf( const float value, const float min, const float max ) {
	const float result = ( value < min ) ? min : value;
	//if ( result > max ) result = max;
	//result;
	return ( result > max ) ? max : result;
}
/**
*   @brief  Normalize input value within input range
**/
RMAPI const float QM_Normalizef( const float value, const float start, const float end ) {
	const float result = ( value - start ) / ( end - start );

	return result;
}
/**
*   @brief  Remap input value within input range to output range
**/
RMAPI const float QM_Remapf( const float value, const float inputStart, const float inputEnd, const float outputStart, const float outputEnd ) {
	const float result = ( value - inputStart ) / ( inputEnd - inputStart ) * ( outputEnd - outputStart ) + outputStart;

	return result;
}
/**
*   @brief  Wrap float input value from min to max
**/
RMAPI const float QM_Wrapf( const float value, const float min, const float max ) {
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
#undef max
RMAPI int QM_DoubleEquals( const double x, const double y ) {
    #if !defined(QM_DOUBLE_EPSILON)
    #define QM_EPSILON DBL_EPSILON
    #endif

    int result = ( abs( x - y ) ) <= ( QM_EPSILON * std::max( 1.0, std::max( abs( x ), abs( y ) ) ) );

    return result;
}

/**
*   @brief  Clamps double value within -min/+max range. 
**/
RMAPI const double QM_Clampd( const double value, const double min, const double max ) {
    const double result = ( value < min ) ? min : value;
    //if ( result > max ) result = max;
    //result;
    return ( result > max ) ? max : result;
}
/**
*   @brief  Normalize input value within input range
**/
RMAPI const double QM_Normalized( const double value, const double start, const double end ) {
    const double result = ( value - start ) / ( end - start );

    return result;
}
/**
*   @brief  Remap input value within input range to output range
**/
RMAPI const double QM_Remapd( const double value, const double inputStart, const double inputEnd, const double outputStart, const double outputEnd ) {
    const double result = ( value - inputStart ) / ( inputEnd - inputStart ) * ( outputEnd - outputStart ) + outputStart;

    return result;
}
/**
*   @brief  Wrap double input value from min to max
**/
RMAPI const double QM_Wrapd( const double value, const double min, const double max ) {
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
RMAPI const int32_t QM_ClampInt32( const int32_t value, const int32_t min, const int32_t max ) {
    const int32_t result = ( value < min || value < 0 ) ? min : value;
    return ( result > max ) ? max : result;
}
/**
*   @brief  Clamps Int32 value within -min/+max range.
**/
RMAPI const uint32_t QM_ClampUint32( const uint32_t value, const uint32_t min, const uint32_t max ) {
    const uint32_t result = ( value < min /*|| value < 0*/ ) ? min : value;
    return ( result > max ) ? max : result;
}

/**
*   @brief  Wrap int input value from min to max
**/
RMAPI const int32_t QM_WrapInt32( const int32_t value, const int32_t min, const int32_t max ) {
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
RMAPI const float QM_FastLerp( const float start, const float end, const float amount ) {
    const float result = start + amount * ( end - start );

    return result;
}
/**
*   @brief  Slower lerp, but you specify back & front lerp separately.
**/
RMAPI const float QM_LerpBackFront( const float start, const float end, const float backLerp, const float frontLerp ) {
    const float result = ( (start) * (backLerp) + (end) * (frontLerp) );

    return result;
}
/**
*   @brief  Slower lerp but is more mathematically precise.
**/
RMAPI const float QM_Lerp( const float start, const float end, const float amount ) {
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
RMAPI const float QM_AngleMod( const float a ) {
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
RMAPI const float QM_LerpAngle( float angle2, float angle1, const float fraction ) {
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
RMAPI const float QM_Angle_Normalize180( const float angle ) {
    float _angle = fmod( ( angle ), 360.0f );
    if ( _angle > 180.0 ) {
        _angle -= 360.0;
    }
    return _angle;
}
/**
*   @brief  returns the normalized to range [-180 < angle <= 180] delta from angle1 to angle2
**/
RMAPI const float QM_AngleDelta( const float angle1, const float angle2 ) {
    return QM_Angle_Normalize180( angle1 - angle2 );
}



/**
*
*
*
*   Easing Utilities:
*	- Source of Original: https://raw.githubusercontent.com/warrenm/AHEasing/refs/heads/master/AHEasing/easing.c
*
*
**/
#define USE_DOUBLE_PRECISION_EASING
#ifdef USE_DOUBLE_PRECISION_EASING
#define EASING_FLOAT double
#else
#define EASING_FLOAT float
#endif
/**
*	@brief	Modeled after the line y = x
**/
static inline const EASING_FLOAT QM_LinearInterpolation( EASING_FLOAT p ) {
	return p;
}
/**
*	@brief	Modeled after the parabola y = x^2
**/
static inline const EASING_FLOAT QM_QuadraticEaseIn( EASING_FLOAT p ) {
	return p * p;
}
/**
*	@brief	Modeled after the parabola y = -x^2 + 2x
**/
static inline const EASING_FLOAT QM_QuadraticEaseOut( EASING_FLOAT p ) {
	return -( p * ( p - 2 ) );
}
/**
*	@brief	Modeled after the piecewise quadratic
*			y = (1/2)((2x)^2)             ; [0, 0.5)
*			y = -(1/2)((2x-1)*(2x-3) - 1) ; [0.5, 1]
**/
static inline const EASING_FLOAT QM_QuadraticEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		return 2 * p * p;
	} else {
		return ( -2 * p * p ) + ( 4 * p ) - 1;
	}
}

/**
*	@brief	Modeled after the cubic y = x^3
**/
static inline const EASING_FLOAT QM_CubicEaseIn( EASING_FLOAT p ) {
	return p * p * p;
}
/**
*	@brief	Modeled after the cubic y = (x - 1)^3 + 1
**/
static inline const EASING_FLOAT QM_CubicEaseOut( EASING_FLOAT p ) {
	EASING_FLOAT f = ( p - 1 );
	return f * f * f + 1;
}
/**
*	@brief	Modeled after the piecewise cubic
*			y = (1/2)((2x)^3)       ; [0, 0.5)
*			y = (1/2)((2x-2)^3 + 2) ; [0.5, 1]
**/
static inline const EASING_FLOAT QM_CubicEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		return 4 * p * p * p;
	} else {
		EASING_FLOAT f = ( ( 2 * p ) - 2 );
		return 0.5 * f * f * f + 1;
	}
}

/**
*	@brief	Modeled after the quartic x^4
**/
static inline const EASING_FLOAT QM_QuarticEaseIn( EASING_FLOAT p ) {
	return p * p * p * p;
}
/**
*	@brief	Modeled after the quartic y = 1 - (x - 1)^4
**/
static inline const EASING_FLOAT QM_QuarticEaseOut( EASING_FLOAT p ) {
	EASING_FLOAT f = ( p - 1 );
	return f * f * f * ( 1 - p ) + 1;
}
/**
*	@brief	Modeled after the piecewise quartic
*			y = (1/2)((2x)^4)        ; [0, 0.5)
*			y = -(1/2)((2x-2)^4 - 2) ; [0.5, 1]
**/
static inline const EASING_FLOAT QM_QuarticEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		return 8 * p * p * p * p;
	} else {
		EASING_FLOAT f = ( p - 1 );
		return -8 * f * f * f * f + 1;
	}
}

/**
*	@brief	Modeled after the quintic y = x^5
**/
static inline const EASING_FLOAT QM_QuinticEaseIn( EASING_FLOAT p ) {
	return p * p * p * p * p;
}
/**
*	@brief	Modeled after the quintic y = (x - 1)^5 + 1
**/
static inline const EASING_FLOAT QM_QuinticEaseOut( EASING_FLOAT p ) {
	EASING_FLOAT f = ( p - 1 );
	return f * f * f * f * f + 1;
}
/**
*	@brief	Modeled after the piecewise quintic
*			y = (1/2)((2x)^5)       ; [0, 0.5)
*			y = (1/2)((2x-2)^5 + 2) ; [0.5, 1]
**/
static inline const EASING_FLOAT QM_QuinticEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		return 16 * p * p * p * p * p;
	} else {
		EASING_FLOAT f = ( ( 2 * p ) - 2 );
		return  0.5 * f * f * f * f * f + 1;
	}
}

/**
*	@brief	Modeled after quarter-cycle of sine wave
**/
static inline const EASING_FLOAT QM_SineEaseIn( EASING_FLOAT p ) {
	return sin( ( p - 1 ) * M_PI_2 ) + 1;
}
/**
*	@brief	Modeled after quarter-cycle of sine wave (different phase)
**/
static inline const EASING_FLOAT QM_SineEaseOut( EASING_FLOAT p ) {
	return sin( p * M_PI_2 );
}
/**
*	@brief	Modeled after half sine wave
**/
static inline const EASING_FLOAT QM_SineEaseInOut( EASING_FLOAT p ) {
	return 0.5 * ( 1 - cos( p * M_PI ) );
}

/**
*	@brief	Modeled after shifted quadrant IV of unit circle
**/
static inline const EASING_FLOAT QM_CircularEaseIn( EASING_FLOAT p ) {
	return 1 - sqrt( 1 - ( p * p ) );
}
/**
*	@brief	Modeled after shifted quadrant II of unit circle
**/
static inline const EASING_FLOAT QM_CircularEaseOut( EASING_FLOAT p ) {
	return sqrt( ( 2 - p ) * p );
}
/**
*	@brief	Modeled after the piecewise circular function
*			y = (1/2)(1 - sqrt(1 - 4x^2))           ; [0, 0.5)
*			y = (1/2)(sqrt(-(2x - 3)*(2x - 1)) + 1) ; [0.5, 1]
**/
static inline const EASING_FLOAT QM_CircularEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		return 0.5 * ( 1 - sqrt( 1 - 4 * ( p * p ) ) );
	} else {
		return 0.5 * ( sqrt( -( ( 2 * p ) - 3 ) * ( ( 2 * p ) - 1 ) ) + 1 );
	}
}

/**
*	@brief	Modeled after the exponential function y = 2^(10(x - 1))
**/
static inline const EASING_FLOAT QM_ExponentialEaseIn( EASING_FLOAT p ) {
	return ( p == 0.0 ) ? p : pow( 2, 10 * ( p - 1 ) );
}
/**
*	@brief	Modeled after the exponential function y = -2^(-10x) + 1
**/
static inline const EASING_FLOAT QM_ExponentialEaseOut( EASING_FLOAT p ) {
	return ( p == 1.0 ) ? p : 1 - pow( 2, -10 * p );
}
/**
*	@brief	Modeled after the piecewise exponential
*			y = (1/2)2^(10(2x - 1))         ; [0,0.5)
*			y = -(1/2)*2^(-10(2x - 1))) + 1 ; [0.5,1]
**/
static inline const EASING_FLOAT QM_ExponentialEaseInOut( EASING_FLOAT p ) {
	if ( p == 0.0 || p == 1.0 ) return p;

	if ( p < 0.5 ) {
		return 0.5 * pow( 2, ( 20 * p ) - 10 );
	} else {
		return -0.5 * pow( 2, ( -20 * p ) + 10 ) + 1;
	}
}

/**
*	@brief	Modeled after the damped sine wave y = sin(13pi/2*x)*pow(2, 10 * (x - 1))
**/
static inline const EASING_FLOAT QM_ElasticEaseIn( EASING_FLOAT p ) {
	return sin( 13 * M_PI_2 * p ) * pow( 2, 10 * ( p - 1 ) );
}
/**
*	@brief	Modeled after the damped sine wave y = sin(-13pi/2*(x + 1))*pow(2, -10x) + 1
**/
static inline const EASING_FLOAT QM_ElasticEaseOut( EASING_FLOAT p ) {
	return sin( -13 * M_PI_2 * ( p + 1 ) ) * pow( 2, -10 * p ) + 1;
}
/**
*	@brief	Modeled after the piecewise exponentially-damped sine wave:
*			y = (1/2)*sin(13pi/2*(2*x))*pow(2, 10 * ((2*x) - 1))      ; [0,0.5)
*			y = (1/2)*(sin(-13pi/2*((2x-1)+1))*pow(2,-10(2*x-1)) + 2) ; [0.5, 1]
**/
static inline const EASING_FLOAT QM_ElasticEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		return 0.5 * sin( 13 * M_PI_2 * ( 2 * p ) ) * pow( 2, 10 * ( ( 2 * p ) - 1 ) );
	} else {
		return 0.5 * ( sin( -13 * M_PI_2 * ( ( 2 * p - 1 ) + 1 ) ) * pow( 2, -10 * ( 2 * p - 1 ) ) + 2 );
	}
}

/**
*	@brief	Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
**/
static inline const EASING_FLOAT QM_BackEaseIn( EASING_FLOAT p ) {
	return p * p * p - p * sin( p * M_PI );
}
/**
*	@brief	Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
**/
static inline const EASING_FLOAT QM_BackEaseOut( EASING_FLOAT p ) {
	EASING_FLOAT f = ( 1 - p );
	return 1 - ( f * f * f - f * sin( f * M_PI ) );
}
/**
*	@brief	Modeled after the piecewise overshooting cubic function:
*			y = (1/2)*((2x)^3-(2x)*sin(2*x*pi))           ; [0, 0.5)
*			y = (1/2)*(1-((1-x)^3-(1-x)*sin((1-x)*pi))+1) ; [0.5, 1]
**/
static inline const EASING_FLOAT QM_BackEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		EASING_FLOAT f = 2 * p;
		return 0.5 * ( f * f * f - f * sin( f * M_PI ) );
	} else {
		EASING_FLOAT f = ( 1 - ( 2 * p - 1 ) );
		return 0.5 * ( 1 - ( f * f * f - f * sin( f * M_PI ) ) ) + 0.5;
	}
}

static inline const EASING_FLOAT QM_BounceEaseOut( EASING_FLOAT p );
/**
*	@brief
**/
static inline const EASING_FLOAT QM_BounceEaseIn( EASING_FLOAT p ) {
	return 1 - QM_BounceEaseOut( 1 - p );
}
/**
*	@brief
**/
static inline const EASING_FLOAT QM_BounceEaseOut( EASING_FLOAT p ) {
	if ( p < 4 / 11.0 ) {
		return ( 121 * p * p ) / 16.0;
	} else if ( p < 8 / 11.0 ) {
		return ( 363 / 40.0 * p * p ) - ( 99 / 10.0 * p ) + 17 / 5.0;
	} else if ( p < 9 / 10.0 ) {
		return ( 4356 / 361.0 * p * p ) - ( 35442 / 1805.0 * p ) + 16061 / 1805.0;
	} else {
		return ( 54 / 5.0 * p * p ) - ( 513 / 25.0 * p ) + 268 / 25.0;
	}
}
/**
*	@brief
**/
static inline const EASING_FLOAT QM_BounceEaseInOut( EASING_FLOAT p ) {
	if ( p < 0.5 ) {
		return 0.5 * QM_BounceEaseIn( p * 2 );
	} else {
		return 0.5 * QM_BounceEaseOut( p * 2 - 1 ) + 0.5;
	}
}
// Undefine it again, prevent clutter.
#undef EASING_FLOAT