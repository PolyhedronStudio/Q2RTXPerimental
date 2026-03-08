/********************************************************************
*
*
*	SharedGame: Velocity (Clipping-)Utilities:
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/math/sg_math_velocity.h"

//! Enables proper speed clamping instead of going full stop if the SG_STOP_EPSILON is reached.
//#define SG_SLIDEMOVE_CLIPVELOCITY_CLAMPING
//! Enables proper speed clamping instead of going full stop if the SG_STOP_EPSILON is reached.
//#define SG_SLIDEMOVE_BOUNCEVELOCITY_CLAMPING



/**
*
*
*	Bounce("Bouncy Reflection") and Sliding("Clipping") velocity
*	to Surface Normal utilities.
*
*	"SG_BounceClipVelocity" usage examples:
*		- Ground movement with slight bounce.
*		- Collision responce that needs elasticity.
*		- "Step" movement where a slight bounce helps prevent sticking.
*
*	"SG_SlideClipVelocity" usage examples:
*		- Wall sliding, no bounce.
*		- Surface alignment, no bounce.
*		- Preventing penetration without bounce.
*
*
**/
/**
*	@brief	Bounces("Bouncy reflection") the velocity off the surface normal.
*	@details	Overbounce > 1 = bouncier, < 1 = less bouncy.
*				Usage examples:
*					- Ground movement with slight bounce.
*					- Collision responce that needs elasticity.
*					- "Step" movement where a slight bounce helps prevent sticking.
*	@return	The clipped flags SG_VELOCITY_* flags.
**/
const SGVelocityClipFlags SG_BounceClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const double overbounce ) {
	// Whether we're actually blocked or not.
	SGVelocityClipFlags blocked = SG_VELOCITY_CLIPPED_NONE;

	/**
	*    Classify the surface by its Z normal and set the appropriate clip flags.
	*    - Flat/gentle slopes that meet or exceed SG_STEP_MIN_NORMAL are floors.
	*    - Slightly sloped floors (not perfectly flat) also get SLOPE_FLOOR.
	*    - Slopes steeper than SG_STEP_MIN_NORMAL but above SG_MIN_WALL_NORMAL_Z are SLOPE_STEEP.
	*    - Very vertical surfaces (<= SG_MIN_WALL_NORMAL_Z) are walls/steps and SLOPE_STEEP.
	**/
	if ( normal.z >= SG_STEP_MIN_NORMAL ) {
		blocked |= SG_VELOCITY_CLIPPED_FLOOR;
		if ( normal.z < 1.0 - 1e-6 ) {
			blocked |= SG_VELOCITY_CLIPPED_SLOPE_FLOOR;
		}
	} else if ( normal.z > SG_MIN_WALL_NORMAL_Z ) {
		blocked |= SG_VELOCITY_CLIPPED_SLOPE_STEEP;
	} else {
		blocked |= SG_VELOCITY_CLIPPED_SLOPE_STEEP;
		// Very vertical surface: treat as a wall/step only. Do not also mark as SLOPE_STEEP
		// because a near-vertical wall is distinct from a steep traversable slope.
		blocked |= SG_VELOCITY_CLIPPED_WALL_OR_STEP;
	}

	// Determine how far to slide based on the incoming direction.
	// Finish it by scaling by the overBounce factor.
	double backOff = QM_Vector3DotProductDP( in, normal );

	// Amplifies reverse direction for bounce:
	if ( backOff < 0. ) {
		backOff *= overbounce;
	// Reduces along-surface movement:
	} else {
		backOff /= overbounce;
	}
	// Reflect the velocity along the normal.
	out = in - ( normal * backOff );

	#ifdef SG_SLIDEMOVE_BOUNCEVELOCITY_CLAMPING
	{
		const double oldSpeed = QM_Vector3Length( in );
		const double newSpeed = QM_Vector3Length( out );
		if ( newSpeed > oldSpeed ) {
			out = QM_Vector3Normalize( out );
			out = QM_Vector3Scale( oldSpeed, out );
		}
	}
	#endif

	// Return blocked by flag(s).
	return blocked;
}

/**
*	@brief	Slides("Clips"), the velocity (strictly-)along the surface normal.
*	@details	Usage examples:
*					- Wall sliding, no bounce.
*					- Surface alignment, no bounce.
*					- Preventing penetration without bounce.
*	@return	The clipped flags SG_VELOCITY_* flags.
**/
const SGVelocityClipFlags SG_SlideClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out ) {
	// Whether we're actually blocked or not.
    SGVelocityClipFlags blocked = SG_VELOCITY_CLIPPED_NONE;

	/**
	*    Classify the surface by its Z normal and set the appropriate clip flags.
	*    - Flat/gentle slopes that meet or exceed SG_STEP_MIN_NORMAL are floors.
	*    - Slightly sloped floors (not perfectly flat) also get SLOPE_FLOOR.
	*    - Slopes steeper than SG_STEP_MIN_NORMAL but above SG_MIN_WALL_NORMAL_Z are SLOPE_STEEP.
	*    - Very vertical surfaces (<= SG_MIN_WALL_NORMAL_Z) are walls/steps and SLOPE_STEEP.
	**/
	if ( normal.z >= SG_STEP_MIN_NORMAL ) {
		blocked |= SG_VELOCITY_CLIPPED_FLOOR;
		if ( normal.z < 1.0 - 1e-6 ) {
			blocked |= SG_VELOCITY_CLIPPED_SLOPE_FLOOR;
		}
	} else if ( normal.z > SG_MIN_WALL_NORMAL_Z ) {
		blocked |= SG_VELOCITY_CLIPPED_SLOPE_STEEP;
    } else {
		// Very vertical surface: treat as a wall/step only. Do not also mark as SLOPE_STEEP
		// because a near-vertical wall is distinct from a steep traversable slope.
		blocked |= SG_VELOCITY_CLIPPED_WALL_OR_STEP;
	}

	// Determine how far to slide based on the incoming direction and project velocity onto plane.
	const double backOff = QM_Vector3DotProductDP( in, normal );
	out = in - ( normal * backOff );

#ifdef SG_SLIDEMOVE_CLIPVELOCITY_CLAMPING
	{
		const double oldSpeed = QM_Vector3Length( in );
		const double newSpeed = QM_Vector3Length( out );
		if ( newSpeed > oldSpeed ) {
			out = QM_Vector3Normalize( out );
			out = QM_Vector3Scale( oldSpeed, out );
		}
	}
#endif

	// Return blocked by flag(s).
	return blocked;
}
