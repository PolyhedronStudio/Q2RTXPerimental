/********************************************************************
*
*
*	SharedGame: Player SlideBox Implementation
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/pmove/sg_pmove_slidemove.h"

//! Uncomment for enabling second best hit plane tracing results.
//#define SECOND_PLANE_TRACE

//! Enables proper speed clamping instead of going full stop if the PM_STOP_EPSILON is reached.
//#define PM_SLIDEMOVE_CLIPVELOCITY_CLAMPING

/**
*
*
*	Touch Entities List:
*
*
**/
/**
*	@brief	As long as numberOfTraces does not exceed MAX_TOUCH_TRACES, and there is not a duplicate trace registered,
*			this function adds the trace into the touchTraceList array and increases the numberOfTraces.
**/
void PM_RegisterTouchTrace( pm_touch_trace_list_t &touchTraceList, cm_trace_t &trace ) {
	// Escape function if we are exceeding maximum touch traces.
	if ( touchTraceList.numberOfTraces >= MAX_TOUCH_TRACES ) {
		return;
	}

	// Iterate for possible duplicates.
	for ( int32_t i = 0; i < touchTraceList.numberOfTraces; i++ ) {
		// Escape function if duplicate.
		if ( touchTraceList.traces[ i ].entityNumber == trace.entityNumber ) {
			return;
		}
	}

	// Add trace to list.
	touchTraceList.traces[ touchTraceList.numberOfTraces++ ] = trace;
}



/**
*
*
*	PMove Step SlideMove:
*
*
**/
/**
*	@brief	Clips the velocity to surface normal.
*			returns the blocked flags (1 = floor, 2 = step / wall)
**/
const pm_clipflags_t PM_BounceClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const double overbounce ) {
	// Whether we're actually blocked or not.
	pm_clipflags_t blocked = PM_VELOCITY_CLIPPED_NONE;

	// <Q2RTXP>: WID: TODO: Properly implement the clipping flags.
	//// If the plane that is blocking us has a positive z component, then assume it's a floor.
	//if ( normal.z > 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
	//	blocked |= PM_VELOCITY_CLIPPED_FLOOR;
	//}
	//// If the plane has no Z, it is vertical Wall/Step:
	//if ( normal.z == 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
	//	blocked |= PM_VELOCITY_CLIPPED_WALL_OR_STEP;
	//}

	// Determine how far to slide based on the incoming direction.
	// Finish it by scaling with overBounce factor.
	double backoff = QM_Vector3DotProduct( in, normal );

	if ( backoff < 0. ) {
		backoff *= overbounce;
	} else {
		backoff /= overbounce;
	}

	for ( int32_t i = 0; i < 3; i++ ) {
		const float change = normal[ i ] * backoff;
		out[ i ] = in[ i ] - change;
	}

	// Return blocked by flag(s).
	return blocked;
}

/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
const pm_slidemove_flags_t PM_SlideMove_Generic( 
	Vector3 &origin, Vector3 &velocity, double &impactSpeed,
	const Vector3 &mins, const Vector3 &maxs,

	pm_touch_trace_list_t &touch_traces,

	const bool &groundPlane, const cm_trace_t &groundTrace,

	const bool gravity, 

	const double frameTime,	const double hasTime
) {	
	pm_slidemove_flags_t blockedMask = PM_SLIDEMOVEFLAG_NONE;

	Vector3 planes[ PM_MAX_CLIP_PLANES ] = {};
	Vector3 dir = {};
	Vector3	end = {};

	cm_trace_t trace = {};
	// Number of planes we have touched.
	int32_t numPlanes = 0;

	// Used for clipping velocity.
	double d = 0.;
	double timeLeft = 0.;
	double into = 0.;

	// For loops.
	int32_t i = 0;
	int32_t j = 0;

	// Times we bumped.
	int32_t bumpCount = 0;
	// Maximum number of bumps we can do.
	int32_t numBumps = PM_MAX_CLIP_PLANES - 1;

	// Primal velocity is used to determine the original direction of the player.
	Vector3 primalVelocity = velocity;

	// Clip velocity and end velocity are used to determine the new velocity after clipping.
	Vector3 clipVelocity = {};
	Vector3 endVelocity = {};
	Vector3 endClipVelocity = {};

	/**
	*	Handle Gravity
	**/
	if ( gravity ) {
		endVelocity = velocity;
		endVelocity.z -= gravity * frameTime;
		velocity.z = ( velocity.z + endVelocity.z ) * 0.5f;
		primalVelocity.z = endVelocity.z;
		if ( groundPlane) {
			// Slide along the ground plane.
			PM_BounceClipVelocity( velocity, groundTrace.plane.normal, velocity, PM_OVERCLIP );
		}
	}

	// Setup the frametime.
	timeLeft = frameTime;

	// Now prevent us from turning and clipping against the ground plane, by simply adding the trace 
	// beforehand.
	if ( groundPlane ) {
		numPlanes = 1;
		planes[ 0 ] = groundTrace.plane.normal;
	} else {
		numPlanes = 0;
	}

	// Never turn against original velocity. (We assing to stop compiler from warnings about unused return value.)
	const float l = QM_Vector3NormalizeLength2( velocity, planes[ numPlanes ] );
	numPlanes++;

	for ( bumpCount = 0; bumpCount < numBumps; bumpCount++ ) {
		// Calculate the end position in which we're trying to move to.
		VectorMA( origin, timeLeft, velocity, end );
		// See if we can trace up to it in one go.
		trace = PM_Trace( origin, mins, maxs, end );
		// Entity is trapped in another solid, DON'T build up falling damage.
		if ( trace.allsolid ) {
			velocity[ 2 ] = 0;
			// Save entity for contact.
			//PM_RegisterTouchTrace( touch_traces, trace );
			// Return trapped mask.
			return PM_SLIDEMOVEFLAG_TRAPPED;
			//return static_cast<pm_slidemove_flags_t>( true );
		}

		// We did cover some distance, so update the origin.
		if ( trace.fraction > 0 ) {
			origin = trace.endpos;
		}

		// We moved the entire distance, so no need to loop on, break out instead.
		if ( trace.fraction == 1 ) {
			blockedMask = PM_SLIDEMOVEFLAG_MOVED;
			break;     // moved the entire distance
		} else {
			// Touched a plane.
			blockedMask |= PM_SLIDEMOVEFLAG_PLANE_TOUCHED;
		}
		// Save entity for contact.
		PM_RegisterTouchTrace( touch_traces, trace );

		// Calculate movement time.
		timeLeft -= timeLeft * trace.fraction;

		// This should technically never happen, but if it does, we are blocked.
		if ( numPlanes >= PM_MAX_CLIP_PLANES ) {
			// This shouldn't really happen.
			velocity = {}; // Clear out velocity.
			blockedMask |= PM_SLIDEMOVEFLAG_TRAPPED;
			return blockedMask;
		}

		// At this point we are blocked but not trapped.
		blockedMask |= PM_SLIDEMOVEFLAG_BLOCKED;
		// Is it a vertical wall?
		if ( trace.plane.normal[ 2 ] < PM_MIN_WALL_NORMAL_Z ) {
			blockedMask |= PM_SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		// If this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		for ( i = 0; i < numPlanes; i++ ) {
			if ( QM_Vector3DotProduct( trace.plane.normal, planes[ i ] ) > 0.99 ) {
				velocity += trace.plane.normal; // VectorAdd( trace.plane.normal, velocity, velocity );
				break;
			}
		}
		// Found a repeated plane, so don't add it, just repeat the trace.
		if ( i < numPlanes ) {
			continue;
		}
		// Add unique plane to the list of planes.
		planes[ numPlanes ] = trace.plane.normal;
		numPlanes++;

		/**
		*	Modify original_velocity so it parallels all of the clip planes
		**/

		// Find a plane it enters.
		for ( i = 0; i < numPlanes; i++ ) {
			// Determine if we bumped into it.
			into = QM_Vector3DotProduct( velocity, planes[ i ] );
			// Not entering this plane.
			if ( into >= 0.1 ) {
				continue;
			}

			// See how hard we hit into things.
			if ( -into > pml.impactSpeed ) {
				// Set the impact speed.
				pml.impactSpeed = -into;
			}

			// Slide velocity along the plane.
			PM_BounceClipVelocity( velocity, planes[ i ], clipVelocity, PM_OVERCLIP );
			// Slide endVelocity along the plane.
			PM_BounceClipVelocity( endVelocity, planes[ i ], endClipVelocity, PM_OVERCLIP );

			// See if there is a second plane that the new move enters.
			for ( j = 0; j < numPlanes; j++ ) {
				// Skip the same plane.
				if ( j == i ) {
					continue;
				}
				// Move doesn't interact with the plane.
				if ( QM_Vector3DotProduct( clipVelocity, planes[ j ] ) >= 0.1 ) {
					continue;
				}
				// Try clipping the move to the plane.
				PM_BounceClipVelocity( clipVelocity, planes[ j ], clipVelocity, PM_OVERCLIP );
				PM_BounceClipVelocity( endClipVelocity, planes[ j ], endClipVelocity, PM_OVERCLIP );

				// See if it goes back into the first clip plane.
				if ( QM_Vector3DotProduct( clipVelocity, planes[ i ] ) >= 0 ) {
					continue;
				}

				// Now we can slide the original velocity along the crease.
				dir = QM_Vector3CrossProduct( planes[ i ], planes[ j ] );
				dir = QM_Vector3Normalize( dir );
				d = QM_Vector3DotProduct( dir, velocity );
				clipVelocity = QM_Vector3Scale( dir, d );

				dir = QM_Vector3CrossProduct( planes[ i ], planes[ j ] );
				dir = QM_Vector3Normalize( dir );
				d = QM_Vector3DotProduct( dir, endVelocity );
				endClipVelocity = QM_Vector3Scale( dir, d );

				// See if there is a third plane that the new move enters.
				for ( int32_t k = 0; k < numPlanes; k++ ) {
					// Skip the planes we already checked.
					if ( k == i || k == j ) {
						continue;
					}
					// Move doesn't interact with the plane.
					if ( QM_Vector3DotProduct( clipVelocity, planes[ k ] ) >= 0.1 ) {
						continue;
					}
					// Stop dead at a tripple plane interaction.
					velocity = {};
					blockedMask |= PM_SLIDEMOVEFLAG_TRAPPED;
					return blockedMask;// static_cast<pm_slidemove_flags_t>( true );
				}
			}

			// If we have fixed all interactions, try another move.
			velocity = clipVelocity;
			endVelocity = endClipVelocity;
			break;
		}
	}

	// Gravity is enabled, so we need to set the velocity to the end velocity.
	if ( gravity ) {
		velocity = endVelocity;
	}
	// Don't set the velocity if we are in a timer.
	// <Q2RTXP>: WID: TODO: This necessary?
	if ( hasTime > 0 ) {
		velocity = primalVelocity;
	}

	return static_cast<pm_slidemove_flags_t>( bumpCount != 0 );// || blockedMask == PM_SLIDEMOVEFLAG_MOVED ? PM_SLIDEMOVEFLAG_MOVED : blockedMask );
}

/**
*	@brief	If intersecting a brush surface, will try to step over the obstruction
*			instead of sliding along it.
*
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
const void PM_StepSlideMove_Generic(
	Vector3 &origin, Vector3 &velocity, 
	double &stepHeight, double &impactSpeed,
	const Vector3 &mins, const Vector3 &maxs,

	pm_touch_trace_list_t &touch_traces,

	const bool &groundPlane, const cm_trace_t &groundTrace,

	const bool gravity,

	const double frameTime, const double hasTime
) {
	/**
	*	Try a full slide move first, and if it fails, try stepping over the obstruction.
	**/
	// Start origin and velocity.
	const Vector3 startOrigin = origin;
	const Vector3 startVelocity = velocity;

	// If we can slide move, do it.
	const pm_slidemove_flags_t moveMask = PM_SLIDEMOVEFLAG_NONE;// PM_SLIDEMOVEFLAG_PLANE_TOUCHED | PM_SLIDEMOVEFLAG_BLOCKED;
	if ( (
		PM_SlideMove_Generic( origin, velocity, impactSpeed, mins, maxs, touch_traces, groundPlane, groundTrace, gravity, frameTime, hasTime ) == moveMask ) ) {
		return; // We got exactly where we wanted to go first try.
	}

	// If we cannot slide move, try stepping over the obstruction.
	Vector3 downPoint = startOrigin + Vector3{ 0.f, 0.f, -PM_STEP_MAX_SIZE };
	// Trace down to see if we can step down.
	cm_trace_t trace = PM_Trace( startOrigin, mins, maxs, downPoint );
	// For velocity step up clip touch testing.
	constexpr Vector3 vUp = { 0.f, 0.f, 1.f };
	// Never step up when you still have up velocity.
	if ( velocity.z > 0 
			&& ( trace.fraction == 1.0 
				|| QM_Vector3DotProduct( trace.plane.normal, vUp) < PM_STEP_MIN_NORMAL ) ) 
	{
		return;
	}

	// Copy over current origin and velocity as the down origin and velocity.
	Vector3 downOrigin = origin;
	Vector3 downVelocity = velocity;

	// Test the player position above the obstruction, if they were a stepheight higher.
	Vector3 upPoint = startOrigin + Vector3{ 0.f, 0.f, PM_STEP_MAX_SIZE };
	trace = PM_Trace( startOrigin, mins, maxs, upPoint );
	if ( trace.allsolid ) {
		SG_DPrintf( "%s: bend can't step\n", __func__ );
		return;
	}

	// Determine the step size.
	const float stepSize = trace.endpos[2] - startOrigin.z;

	// Try sliding above the obstruction.
	origin = trace.endpos;
	velocity = startVelocity;

	PM_SlideMove_Generic( origin, velocity, impactSpeed, mins, maxs, touch_traces, groundPlane, groundTrace, gravity, frameTime, hasTime );

	// Push down the final amount.
	downPoint = origin + Vector3{ 0.f, 0.f, -stepSize };
	trace = PM_Trace( origin, mins, maxs, downPoint );
	// If we are not solid, then we can step down.
	if ( !trace.allsolid ) {
		origin = trace.endpos;
	}
	// Otherwise, we clip along the obstruction.
	if ( trace.fraction < 1.0 ) {
		PM_BounceClipVelocity( velocity, trace.plane.normal, velocity, PM_OVERCLIP );
	}

	stepHeight = origin.z - startOrigin.z;
}