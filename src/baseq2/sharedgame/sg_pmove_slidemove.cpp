/********************************************************************
*
*
*	SharedGame: Player SlideBox Implementation
*
*
********************************************************************/
#include "shared/shared.h"

#include "sg_pmove.h"
#include "sg_pmove_slidemove.h"

//! An actual pointer to the pmove object that we're moving.
extern pmove_t *pm;

//! Uncomment for enabling second best hit plane tracing results.
#define SECOND_PLANE_TRACE

/**
*
*
*	PM Clip/Trace:
*
*
**/
/**
*	@brief	Clips trace against world only.
**/
const trace_t PM_Clip( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const contents_t contentMask ) {
	return pm->clip( QM_Vector3ToQFloatV( start ).v, QM_Vector3ToQFloatV( mins ).v, QM_Vector3ToQFloatV( maxs ).v, QM_Vector3ToQFloatV( end ).v, contentMask );
}

/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
const trace_t PM_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, contents_t contentMask ) {
	// Spectators only clip against world, so use clip instead.
	if ( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		return PM_Clip( start, mins, maxs, end, MASK_SOLID );
	}

	if ( contentMask == CONTENTS_NONE ) {
		if ( pm->playerState->pmove.pm_type == PM_DEAD || pm->playerState->pmove.pm_type == PM_GIB ) {
			contentMask = MASK_DEADSOLID;
		} else if ( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
			contentMask = MASK_SOLID;
		} else {
			contentMask = MASK_PLAYERSOLID;
		}

		//if ( pm->s.pm_flags & PMF_IGNORE_PLAYER_COLLISION )
		//	mask &= ~CONTENTS_PLAYER;
	}

	return pm->trace( QM_Vector3ToQFloatV( start ).v, QM_Vector3ToQFloatV( mins ).v, QM_Vector3ToQFloatV( maxs ).v, QM_Vector3ToQFloatV( end ).v, pm->player, contentMask );
}



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
void PM_RegisterTouchTrace( pm_touch_trace_list_t &touchTraceList, trace_t &trace ) {
	// Escape function if we are exceeding maximum touch traces.
	if ( touchTraceList.numberOfTraces >= MAX_TOUCH_TRACES ) {
		return;
	}

	// Iterate for possible duplicates.
	for ( int32_t i = 0; i < touchTraceList.numberOfTraces; i++ ) {
		// Escape function if duplicate.
		if ( touchTraceList.traces[ i ].ent == trace.ent ) {
			return;
		}
	}

	// Add trace to list.
	touchTraceList.traces[ touchTraceList.numberOfTraces++ ] = trace;
}



/**
*
*
*	Touch Entities List:
*
*
**/
/**
*	@brief	Slide off of the impacting object.
*			returns the blocked flags (1 = floor, 2 = step / wall)
**/
// Epsilon to 'halt' at.
static constexpr float STOP_EPSILON = 0.1f;
/**
*	@brief	Clips the velocity to surface normal.
**/
void PM_ClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const float overbounce ) {
	float   backoff;
	float   change;
	int     i;

	backoff = DotProduct( in, normal ) * overbounce;

	for ( i = 0; i < 3; i++ ) {
		change = normal[ i ] * backoff;
		out[ i ] = in[ i ] - change;
		if ( out[ i ] > -STOP_EPSILON && out[ i ] < STOP_EPSILON )
			out[ i ] = 0;
	}
}

/*
==================
PM_StepSlideMove

Each intersection will try to step over the obstruction instead of
sliding along it.

Returns a new origin, velocity, and contact entity
Does not modify any world state?
==================
*/
/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
const int32_t PM_StepSlideMove_Generic( Vector3 &origin, Vector3 &velocity, const float frametime, const Vector3 &mins, const Vector3 &maxs, pm_touch_trace_list_t &touch_traces, const bool has_time ) {
	Vector3 dir = {};

	Vector3 planes[ MAX_CLIP_PLANES ] = {};

	trace_t	trace = {};
	Vector3	end = {};

	float d = 0;
	float time_left = 0.f;

	int i = 0, j = 0;
	int32_t bumpcount = 0;
	int32_t numbumps = MAX_CLIP_PLANES - 1;

	Vector3 primal_velocity = velocity;
	Vector3 last_valid_origin = origin;
	int32_t numplanes = 0;

	int32_t blockedMask = 0;

	time_left = frametime;



	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {
		VectorMA( origin, time_left, velocity, end );
		//for ( i = 0; i < 3; i++ )
		//	end[ i ] = origin[ i ] + time_left * velocity[ i ];
		trace = PM_Trace( origin, mins, maxs, end );

		if ( trace.allsolid ) {
			// Entity is trapped in another solid, DON'T build up falling damage.
			velocity[ 2 ] = 0;    
			// Save entity for contact.
			PM_RegisterTouchTrace( touch_traces, trace );
			// Return trapped mask.
			return PM_SLIDEMOVEFLAG_TRAPPED;
		}

		// [Paril-KEX] experimental attempt to fix stray collisions on curved
		// surfaces; easiest to see on q2dm1 by running/jumping against the sides
		// of the curved map.
#ifdef SECOND_PLANE_TRACE
		if ( trace.surface2 ) {
			Vector3 clipped_a = QM_Vector3Zero();
			Vector3 clipped_b = QM_Vector3Zero();
			PM_ClipVelocity( velocity, trace.plane.normal, clipped_a, 1.01f );
			PM_ClipVelocity( velocity, trace.plane2.normal, clipped_b, 1.01f );

			bool better = false;

			for ( int i = 0; i < 3; i++ ) {
				if ( fabsf( clipped_a[ i ] ) < fabsf( clipped_b[ i ] ) ) {
					better = true;
					break;
				}
			}

			if ( better ) {
				trace.plane = trace.plane2;
				trace.surface = trace.surface2;
			}
		}
#endif

		if ( trace.fraction > 0 ) {
			// actually covered some distance
			origin = trace.endpos;
			last_valid_origin = trace.endpos;

			//numplanes = 0;
		}

		if ( trace.fraction == 1 ) {
			blockedMask = PM_SLIDEMOVEFLAG_MOVED;
			break;     // moved the entire distance
		}

		// Save entity for contact.
		PM_RegisterTouchTrace( touch_traces, trace );
		
		// At this point we are blocked but not trapped.
		blockedMask |= PM_SLIDEMOVEFLAG_BLOCKED;
		// Is it a vertical wall?
		if ( trace.plane.normal[ 2 ] < 0.03125 ) {
			blockedMask |= PM_SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		// Subtract the fraction of time used, from the whole fraction of the move.
		time_left -= time_left * trace.fraction;

		// if this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		for ( i = 0; i < numplanes; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[ i ] ) > 0.99f ) {/*( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON )*/  
				VectorAdd( trace.plane.normal, velocity, velocity );
				break;
			}
		}
		if ( i < numplanes ) { // found a repeated plane, so don't add it, just repeat the trace
			continue;
		}

		// Slide along this plane
		if ( numplanes >= MAX_CLIP_PLANES ) {
			// Zero out velocity. This should never happen though.
			velocity = {};
			blockedMask = PM_SLIDEMOVEFLAG_TRAPPED;
			break;
		}

		//
		// if this is the same plane we hit before, nudge origin
		// out along it, which fixes some epsilon issues with
		// non-axial planes (xswamp, q2dm1 sometimes...)
		//
		//for ( i = 0; i < numplanes; i++ ) {
		//	if ( QM_Vector3DotProduct( trace.plane.normal, planes[ i ] ) > 0.99f ) {
		//		pml.origin.x += trace.plane.normal.x * 0.01f;
		//		pml.origin.y += trace.plane.normal.y * 0.01f;
		//		G_FixStuckObject_Generic( pml.origin, mins, maxs, trace_func );
		//		break;
		//	}
		//}

		//if ( i < numplanes )
		//	continue;

		planes[ numplanes ] = trace.plane.normal;
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//
		int32_t i = 0;
		int32_t j = 0;
		for ( i = 0; i < numplanes; i++ ) {
			PM_ClipVelocity( velocity, planes[ i ], velocity, 1.01f );
			for ( j = 0; j < numplanes; j++ ) {
				if ( j != i ) {
					if ( QM_Vector3DotProduct( velocity, planes[ j ] ) < 0 ) {
						break;  // not ok
					}
				}
			}
			if ( j == numplanes ) {
				break;
			}
		}

		if ( i != numplanes ) {
			// go along this plane
		} else {
			// go along the crease
			if ( numplanes != 2 ) {
				velocity = {}; // Clear out velocity.
				break;
			}
			dir = QM_Vector3CrossProduct( planes[ 0 ], planes[ 1 ] );
			d = QM_Vector3DotProduct( dir, velocity );
			velocity = QM_Vector3Scale( dir, d );
		}

		//
		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		//
		if ( QM_Vector3DotProduct( velocity, primal_velocity ) <= 0 ) {
			velocity = {};
			break;
		}
	}

	if ( has_time ) {
		velocity = primal_velocity;
	}

	return blockedMask;
}
