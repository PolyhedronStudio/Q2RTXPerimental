/**
*
*
*   Collision Model: Plane Functions
*
*
**/
#include "shared/shared.h"
#include "common/bsp.h"
#include "common/collisionmodel.h"
#include "common/common.h"

#if 0
const BBox3 CM_CalculateBrushBounds( mbrush_t *brush ) {
	if ( !brush->firstbrushside ) {
		return QM_BBox3Zero();
	}

	brush->absmins[ 0 ] = -brush->firstbrushside[ 0 ].plane->dist;
	brush->absmaxs[ 0 ] = brush->firstbrushside[ 1 ].plane->dist;

	brush->absmins[ 1 ] = -brush->firstbrushside[ 2 ].plane->dist;
	brush->absmaxs[ 1 ] = brush->firstbrushside[ 3 ].plane->dist;

	brush->absmins[ 2 ] = -brush->firstbrushside[ 4 ].plane->dist;
	brush->absmaxs[ 2 ] = brush->firstbrushside[ 5 ].plane->dist;

	return { brush->absmins, brush->absmaxs };
}
/**
*	@return The PLANE_ type for the given normal vector.
**/
const int32_t CM_PlaneTypeForNormal( const vec3_t normal ) {
	// Plane types are defined as follows:
	// PLANE_X: normal = (1, 0, 0) or (-1, 0, 0)
	// PLANE_Y: normal = (0, 1, 0) or (0, -1, 0)
	// PLANE_Z: normal = (0, 0, 1) or (0, 0, -1)
	// PLANE_ANY_X: normal = (x, 0, 0) where |x| > 0.707
	// PLANE_ANY_Y: normal = (0, y, 0) where |y| > 0.707
	// PLANE_ANY_Z: normal = (0, 0, z) where |z| > 0.707
	#if 0
	const float x = std::fabsf( normal[ 0 ] );
	if ( x > 1.f - FLT_EPSILON ) {
		return PLANE_X;
	}

	const float y = std::fabsf( normal[ 1 ] );
	if ( y > 1.f - FLT_EPSILON ) {
		return PLANE_Y;
	}

	const float z = std::fabsf( normal[ 2 ] );
	if ( z > 1.f - FLT_EPSILON ) {
		return PLANE_Z;
	}

	if ( x >= y && x >= z ) {
		return PLANE_ANYX;
	}
	if ( y >= x && y >= z ) {
		return PLANE_ANYY;
	}

	return PLANE_ANYZ;
	
	#else
	#if 0
	// NOTE: should these have an epsilon around 1.0?
	if ( static_cast<double>( std::fabs<double>( normal[ 0 ] ) ) >= 1.0 - DBL_EPSILON ) {
		return PLANE_X;
	}
	if ( static_cast<double>( std::fabs<double>( normal[ 1 ] ) ) >= 1.0 - DBL_EPSILON ) {
		return PLANE_Y;
	}
	if ( static_cast<double>( std::fabs<double>( normal[ 2 ] ) ) >= 1.0 - DBL_EPSILON ) {
		return PLANE_Z;
	}
	#else
	// NOTE: should these have an epsilon around 1.0?
	if ( normal[ 0 ] >= 1.0 ) {
		return PLANE_X;
	}
	if ( normal[ 1 ] >= 1.0 ) {
		return PLANE_Y;
	}
	if ( normal[ 2 ] >= 1.0 ) {
		return PLANE_Z;
	}
	#endif

	return PLANE_NONAXIAL;
	#endif

}

/**
*	@return A bit mask hinting at the sign of each normal vector component. This
*			can be used to optimize plane side tests.
**/
const int32_t CM_SignBitsForNormal( const vec3_t normal ) {
	// 
	int32_t bits = 0;
	// Check each component of the normal vector and set the corresponding bit if the component is negative.
	for ( int32_t i = 0; i < 3; i++ ) {
		if ( normal[ i ] < -FLT_EPSILON ) {
			bits |= 1 << i;
		} else if ( normal[ i ] > FLT_EPSILON ) {
			// Do nothing, the bit remains 0
		} else {
			// If the component is zero, we do not set any bit for that component
			// This is important for planes that are exactly aligned with axes
		}

		//if ( normal.xyz[ i ] < 0.0f ) {
		//	bits |= 1 << i;
		//}
	}

	return bits;
}
#endif