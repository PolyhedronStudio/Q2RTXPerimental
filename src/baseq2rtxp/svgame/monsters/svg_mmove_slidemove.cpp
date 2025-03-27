/********************************************************************
*
*
*	SharedGame: Player SlideBox Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svg_mmove.h"
#include "svg_mmove_slidemove.h"

//! An actual pointer to the pmove object that we're moving.
//extern pmove_t *pm;

//! Uncomment for enabling second best hit plane tracing results.
#define SECOND_PLANE_TRACE



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
void SVG_MMove_RegisterTouchTrace( mm_touch_trace_list_t &touchTraceList, trace_t &trace ) {
	// Escape function if we are exceeding maximum touch traces.
	if ( touchTraceList.numberOfTraces >= MM_MAX_TOUCH_TRACES ) {
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
*	Monster Step SlideMove:
*
*
**/
/**
*	@brief	Slide off of the impacting object.
**/
// Epsilon to 'halt' at.
static constexpr float MM_STOP_EPSILON = 0.1f;

/**
*	@brief	Clips the velocity to surface normal.
**/
const int32_t SVG_MMove_ClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const float overbounce ) {
	// Whether we're actually blocked or not.
	int32_t blocked = MM_VELOCITY_CLIPPED_NONE;
	// If the plane that is blocking us has a positive z component, then assume it's a floor.
	if ( normal.z > 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
		blocked |= MM_VELOCITY_CLIPPED_FLOOR;
	}
	// If the plane has no Z, it is vertical Wall/Step:
	if ( normal.z == 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
		blocked |= MM_VELOCITY_CLIPPED_WALL_OR_STEP;
	}
	// Determine how far to slide based on the incoming direction.
	// Finish it by scaling with overBounce factor.
	const float backoff = QM_Vector3DotProduct( in, normal ) * overbounce;

	for ( int32_t i = 0; i < 3; i++ ) {
		const float change = normal[ i ] * backoff;
		out[ i ] = in[ i ] - change;
		if ( out[ i ] > -MM_STOP_EPSILON && out[ i ] < MM_STOP_EPSILON ) {
			out[ i ] = 0;
		}
	}

	// Return blocked by flag(s).
	return blocked;
}

/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
const int32_t SVG_MMove_SlideMove( Vector3 &origin, Vector3 &velocity, const float frametime, const Vector3 &mins, const Vector3 &maxs, svg_edict_t *passEntity, mm_touch_trace_list_t &touch_traces, const bool has_time ) {
	Vector3 dir = {};

	Vector3 planes[ MM_MAX_CLIP_PLANES ] = {};

	trace_t	trace = {};
	Vector3	end = {};

	float d = 0;
	float time_left = 0.f;

	int i = 0, j = 0;
	int32_t bumpcount = 0;
	int32_t numbumps = MM_MAX_CLIP_PLANES - 1;

	Vector3 primal_velocity = velocity;
	Vector3 last_valid_origin = origin;
	int32_t numplanes = 0;

	int32_t blockedMask = 0;

	time_left = frametime;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {
		VectorMA( origin, time_left, velocity, end );
		//for ( i = 0; i < 3; i++ )
		//	end[ i ] = origin[ i ] + time_left * velocity[ i ];
		trace = SVG_MMove_Trace( origin, mins, maxs, end, passEntity );

		if ( trace.allsolid ) {
			// Entity is trapped in another solid, DON'T build up falling damage.
			velocity[ 2 ] = 0;
			// Save entity for contact.
			SVG_MMove_RegisterTouchTrace( touch_traces, trace );
			// Return trapped mask.
			return MM_SLIDEMOVEFLAG_TRAPPED;
		}

		// [Paril-KEX] experimental attempt to fix stray collisions on curved
		// surfaces; easiest to see on q2dm1 by running/jumping against the sides
		// of the curved map.
		#ifdef SECOND_PLANE_TRACE
		if ( trace.surface2 ) {
			Vector3 clipped_a = QM_Vector3Zero();
			Vector3 clipped_b = QM_Vector3Zero();
			SVG_MMove_ClipVelocity( velocity, trace.plane.normal, clipped_a, 1.01f );
			SVG_MMove_ClipVelocity( velocity, trace.plane2.normal, clipped_b, 1.01f );

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
				trace.material = trace.material2;
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
			blockedMask = MM_SLIDEMOVEFLAG_MOVED;
			break;     // moved the entire distance
		}

		// Save entity for contact.
		SVG_MMove_RegisterTouchTrace( touch_traces, trace );

		// At this point we are blocked but not trapped.
		blockedMask |= MM_SLIDEMOVEFLAG_BLOCKED;
		// Is it a vertical wall?
		if ( trace.plane.normal[ 2 ] < MM_MIN_WALL_NORMAL_Z ) {
			blockedMask |= MM_SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		// Subtract the fraction of time used, from the whole fraction of the move.
		time_left -= time_left * trace.fraction;

		// if this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		for ( i = 0; i < numplanes; i++ ) {
			if ( QM_Vector3DotProduct( trace.plane.normal, planes[ i ] ) > ( 1.0f - FLT_EPSILON ) ) {//0.99f ) {/*( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON )*/  
				velocity += trace.plane.normal; // VectorAdd( trace.plane.normal, velocity, velocity );
				break;
			}
		}
		if ( i < numplanes ) { // found a repeated plane, so don't add it, just repeat the trace
			continue;
		}

		// Slide along this plane
		if ( numplanes >= MM_MAX_CLIP_PLANES ) {
			// Zero out velocity. This should never happen though.
			velocity = {};
			blockedMask = MM_SLIDEMOVEFLAG_TRAPPED;
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
			SVG_MMove_ClipVelocity( velocity, planes[ i ], velocity, 1.01f );
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
