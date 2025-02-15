/**
*
*
*   QMath: C++ Only, Templated Easing Methods
*	- Source of Original: https://raw.githubusercontent.com/warrenm/AHEasing/refs/heads/master/AHEasing/easing.c
*
*
*
**/
#pragma once


#ifdef __cplusplus

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
	#ifdef __cplusplus
		#define EASING_TYPE const T
		#define EASING_IN_TYPE EASING_TYPE&
		#define EASING_OUT_TYPE template<typename T> ##EASING_TYPE
	#else
		#define EASING_IN_TYPE double
		#define EASING_OUT_TYPE double
		#define EASING_TYPE double
	#endif
#else
	#ifdef __cplusplus
		#define EASING_TYPE const T
		#define EASING_IN_TYPE EASING_TYPE&
		#define EASING_OUT_TYPE template<typename T> ##EASING_TYPE
	#else
		#define EASING_IN_TYPE float
		#define EASING_OUT_TYPE float
	#endif
#endif
/**
*	@brief	Modeled after the line y = x
**/
EASING_OUT_TYPE inline QM_LinearInterpolationEase( EASING_IN_TYPE p ) {
	return p;
}
/**
*	@brief	Modeled after the parabola y = x^2
**/
EASING_OUT_TYPE inline QM_QuadraticEaseIn( EASING_IN_TYPE p ) {
	return p * p;
}
/**
*	@brief	Modeled after the parabola y = -x^2 + 2x
**/
EASING_OUT_TYPE inline QM_QuadraticEaseOut( EASING_IN_TYPE p ) {
	return -( p * ( p - 2 ) );
}
/**
*	@brief	Modeled after the piecewise quadratic
*			y = (1/2)((2x)^2)             ; [0, 0.5)
*			y = -(1/2)((2x-1)*(2x-3) - 1) ; [0.5, 1]
**/
EASING_OUT_TYPE inline QM_QuadraticEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		return 2 * p * p;
	} else {
		return ( -2 * p * p ) + ( 4 * p ) - 1;
	}
}

/**
*	@brief	Modeled after the cubic y = x^3
**/
EASING_OUT_TYPE inline QM_CubicEaseIn( EASING_IN_TYPE p ) {
	return p * p * p;
}
/**
*	@brief	Modeled after the cubic y = (x - 1)^3 + 1
**/
EASING_OUT_TYPE inline QM_CubicEaseOut( EASING_IN_TYPE p ) {
	EASING_TYPE f = ( p - 1 );
	return f * f * f + 1;
}
/**
*	@brief	Modeled after the piecewise cubic
*			y = (1/2)((2x)^3)       ; [0, 0.5)
*			y = (1/2)((2x-2)^3 + 2) ; [0.5, 1]
**/
EASING_OUT_TYPE inline QM_CubicEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		return 4 * p * p * p;
	} else {
		EASING_TYPE f = ( ( 2 * p ) - 2 );
		return 0.5 * f * f * f + 1;
	}
}

/**
*	@brief	Modeled after the quartic x^4
**/
EASING_OUT_TYPE inline QM_QuarticEaseIn( EASING_IN_TYPE p ) {
	return p * p * p * p;
}
/**
*	@brief	Modeled after the quartic y = 1 - (x - 1)^4
**/
EASING_OUT_TYPE inline QM_QuarticEaseOut( EASING_IN_TYPE p ) {
	EASING_TYPE f = ( p - 1 );
	return f * f * f * ( 1 - p ) + 1;
}
/**
*	@brief	Modeled after the piecewise quartic
*			y = (1/2)((2x)^4)        ; [0, 0.5)
*			y = -(1/2)((2x-2)^4 - 2) ; [0.5, 1]
**/
EASING_OUT_TYPE inline QM_QuarticEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		return 8 * p * p * p * p;
	} else {
		EASING_TYPE f = ( p - 1 );
		return -8 * f * f * f * f + 1;
	}
}

/**
*	@brief	Modeled after the quintic y = x^5
**/
EASING_OUT_TYPE inline QM_QuinticEaseIn( EASING_IN_TYPE p ) {
	return p * p * p * p * p;
}
/**
*	@brief	Modeled after the quintic y = (x - 1)^5 + 1
**/
EASING_OUT_TYPE inline QM_QuinticEaseOut( EASING_IN_TYPE p ) {
	EASING_TYPE f = ( p - 1 );
	return f * f * f * f * f + 1;
}
/**
*	@brief	Modeled after the piecewise quintic
*			y = (1/2)((2x)^5)       ; [0, 0.5)
*			y = (1/2)((2x-2)^5 + 2) ; [0.5, 1]
**/
EASING_OUT_TYPE inline QM_QuinticEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		return 16 * p * p * p * p * p;
	} else {
		EASING_TYPE f = ( ( 2 * p ) - 2 );
		return  0.5 * f * f * f * f * f + 1;
	}
}

/**
*	@brief	Modeled after quarter-cycle of sine wave
**/
EASING_OUT_TYPE inline QM_SineEaseIn( EASING_IN_TYPE p ) {
	return sin( ( p - 1 ) * M_PI_2 ) + 1;
}
/**
*	@brief	Modeled after quarter-cycle of sine wave (different phase)
**/
EASING_OUT_TYPE inline QM_SineEaseOut( EASING_IN_TYPE p ) {
	return sin( p * M_PI_2 );
}
/**
*	@brief	Modeled after half sine wave
**/
EASING_OUT_TYPE inline QM_SineEaseInOut( EASING_IN_TYPE p ) {
	return 0.5 * ( 1 - cos( p * M_PI ) );
}

/**
*	@brief	Modeled after shifted quadrant IV of unit circle
**/
EASING_OUT_TYPE inline QM_CircularEaseIn( EASING_IN_TYPE p ) {
	return 1 - sqrt( 1 - ( p * p ) );
}
/**
*	@brief	Modeled after shifted quadrant II of unit circle
**/
EASING_OUT_TYPE inline QM_CircularEaseOut( EASING_IN_TYPE p ) {
	return sqrt( ( 2 - p ) * p );
}
/**
*	@brief	Modeled after the piecewise circular function
*			y = (1/2)(1 - sqrt(1 - 4x^2))           ; [0, 0.5)
*			y = (1/2)(sqrt(-(2x - 3)*(2x - 1)) + 1) ; [0.5, 1]
**/
EASING_OUT_TYPE inline QM_CircularEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		return 0.5 * ( 1 - sqrt( 1 - 4 * ( p * p ) ) );
	} else {
		return 0.5 * ( sqrt( -( ( 2 * p ) - 3 ) * ( ( 2 * p ) - 1 ) ) + 1 );
	}
}

/**
*	@brief	Modeled after the exponential function y = 2^(10(x - 1))
**/
EASING_OUT_TYPE inline QM_ExponentialEaseIn( EASING_IN_TYPE p ) {
	return ( p == 0.0 ) ? p : pow( 2, 10 * ( p - 1 ) );
}
/**
*	@brief	Modeled after the exponential function y = -2^(-10x) + 1
**/
EASING_OUT_TYPE inline QM_ExponentialEaseOut( EASING_IN_TYPE p ) {
	return ( p == 1.0 ) ? p : 1 - pow( 2, -10 * p );
}
/**
*	@brief	Modeled after the piecewise exponential
*			y = (1/2)2^(10(2x - 1))         ; [0,0.5)
*			y = -(1/2)*2^(-10(2x - 1))) + 1 ; [0.5,1]
**/
EASING_OUT_TYPE inline QM_ExponentialEaseInOut( EASING_IN_TYPE p ) {
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
EASING_OUT_TYPE inline QM_ElasticEaseIn( EASING_IN_TYPE p ) {
	return sin( 13 * M_PI_2 * p ) * pow( 2, 10 * ( p - 1 ) );
}
/**
*	@brief	Modeled after the damped sine wave y = sin(-13pi/2*(x + 1))*pow(2, -10x) + 1
**/
EASING_OUT_TYPE inline QM_ElasticEaseOut( EASING_IN_TYPE p ) {
	return sin( -13 * M_PI_2 * ( p + 1 ) ) * pow( 2, -10 * p ) + 1;
}
/**
*	@brief	Modeled after the piecewise exponentially-damped sine wave:
*			y = (1/2)*sin(13pi/2*(2*x))*pow(2, 10 * ((2*x) - 1))      ; [0,0.5)
*			y = (1/2)*(sin(-13pi/2*((2x-1)+1))*pow(2,-10(2*x-1)) + 2) ; [0.5, 1]
**/
EASING_OUT_TYPE inline QM_ElasticEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		return 0.5 * sin( 13 * M_PI_2 * ( 2 * p ) ) * pow( 2, 10 * ( ( 2 * p ) - 1 ) );
	} else {
		return 0.5 * ( sin( -13 * M_PI_2 * ( ( 2 * p - 1 ) + 1 ) ) * pow( 2, -10 * ( 2 * p - 1 ) ) + 2 );
	}
}

/**
*	@brief	Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
**/
EASING_OUT_TYPE inline QM_BackEaseIn( EASING_IN_TYPE p ) {
	return p * p * p - p * sin( p * M_PI );
}
/**
*	@brief	Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
**/
EASING_OUT_TYPE inline QM_BackEaseOut( EASING_IN_TYPE p ) {
	EASING_TYPE f = ( 1 - p );
	return 1 - ( f * f * f - f * sin( f * M_PI ) );
}
/**
*	@brief	Modeled after the piecewise overshooting cubic function:
*			y = (1/2)*((2x)^3-(2x)*sin(2*x*pi))           ; [0, 0.5)
*			y = (1/2)*(1-((1-x)^3-(1-x)*sin((1-x)*pi))+1) ; [0.5, 1]
**/
EASING_OUT_TYPE inline QM_BackEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		EASING_TYPE f = 2 * p;
		return 0.5 * ( f * f * f - f * sin( f * M_PI ) );
	} else {
		EASING_TYPE f = ( 1 - ( 2 * p - 1 ) );
		return 0.5 * ( 1 - ( f * f * f - f * sin( f * M_PI ) ) ) + 0.5;
	}
}

/**
*	@brief
**/
EASING_OUT_TYPE inline QM_BounceEaseOut( EASING_IN_TYPE p ) {
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
EASING_OUT_TYPE inline QM_BounceEaseIn( EASING_IN_TYPE p ) {
	return 1 - QM_BounceEaseOut( 1 - p );
}

/**
*	@brief
**/
EASING_OUT_TYPE inline QM_BounceEaseInOut( EASING_IN_TYPE p ) {
	if ( p < 0.5 ) {
		return 0.5 * QM_BounceEaseIn( p * 2 );
	} else {
		return 0.5 * QM_BounceEaseOut( p * 2 - 1 ) + 0.5;
	}
}
// Undefine it again, prevent clutter.
#undef OUT_TYPE
#undef EASING_OUT_TYPE
#undef EASING_IN_TYPE

#endif // #ifdef __cplusplus