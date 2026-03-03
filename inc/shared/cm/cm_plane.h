/********************************************************************
*
*
*   Shared Collision Detection: Clipping Plane.
*
*
********************************************************************/
#pragma once



/**
*   @brief  BSP Brush plane_t structure. To acquire the origin,
*           scale normal by dist.
**/
typedef struct cm_plane_s {
    vec3_t  normal;
    float   dist;
    byte    type;           // for fast side tests
    byte    signbits;       // signx + (signy<<1) + (signz<<1)
    byte    pad[ 2 ];
} cm_plane_t;

//! 0-2 are axial planes.
#define PLANE_X         0
#define PLANE_Y         1
#define PLANE_Z         2

//struct IVec3 { int64_t x, y, z; };

/**
*	@brief	Used for storing unique BSP collision plane face IDs.
**/
typedef struct uid_plane_key_s {
	/**
	*	Reduced integer coefficients(gcd - normalized) for ax + by + cz = d
	**/
	#ifndef __cplusplus
	int64_t a, b, c, d;
	#else
	int64_t a = 0, b = 0, c = 0, d = 0;
	/**
	*	@brief	`IsEqual` operator
	*	@param o `PlaneKey` to compare with
	*	@return	True if `a == o.a && b == o.b && c == o.c && d == o.d;`
	**/
	const bool operator==( uid_plane_key_s const &o ) const noexcept {
		return a == o.a && b == o.b && c == o.c && d == o.d;
	}
	/**
	*	@brief	`NotEqual` operator
	*	@param o `PlaneKey` to compare with
	*	@return	True if `!( *this == o )`
	**/
	const bool operator!=( uid_plane_key_s const &o ) const noexcept { return !( *this == o ); }

	/**
	*	@brief	Convert the PlaneKey to a string representation for debugging.
	*	@return	 
	**/
	inline std::string toString() const {
		return "PlaneKey{a=" + std::to_string( a ) + ", b=" + std::to_string( b ) + ", c=" + std::to_string( c ) + ", d=" + std::to_string( d ) + "}";
	}
#endif
} uid_plane_key_t;


#if 0
//! 3-5 are non-axial planes snapped to the nearest.
#define PLANE_ANYX      3
#define PLANE_ANYY      4
#define PLANE_ANYZ      5
#define PLANE_NON_AXIAL 6
#else
//! Planes (x&~1) and (x&~1)+1 are always opposites.
#define PLANE_NON_AXIAL 3
#endif

