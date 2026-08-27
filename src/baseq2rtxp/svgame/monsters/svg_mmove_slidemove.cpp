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
//#define SECOND_PLANE_TRACE



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
void SVG_MMove_RegisterTouchTrace( mm_touch_trace_list_t &touchTraceList, svg_trace_t &trace ) {
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
//! Dot-product tolerance for recognizing the same analytical contact plane again.
static constexpr float MM_PLANE_REPEAT_DOT = 0.9999f;
//! More forgiving repeated-plane tolerance for nearly vertical wall contacts.
static constexpr float MM_VERTICAL_PLANE_REPEAT_DOT = 0.99f;
//! Normalized entering threshold used to avoid clipping planes that movement is leaving.
static constexpr float MM_PLANE_ENTER_THRESHOLD = -0.001f;
//! Minimum velocity magnitude required for a meaningful plane-entering test.
static constexpr float MM_MIN_PLANE_CHECK_SPEED = 1e-6f;

/**
* @brief Determine whether velocity is entering a collision plane.
* @param velocity Current movement velocity.
* @param normal Normalized collision-plane normal.
* @return True when the velocity points into the plane by more than the noise threshold.
**/
static bool SVG_MMove_IsEnteringPlane( const Vector3 &velocity, const Vector3 &normal ) {
    /**
    * Normalize the velocity projection so the decision is independent of movement speed.
    **/
    const float speed = QM_Vector3Length( velocity );
    if ( speed <= MM_MIN_PLANE_CHECK_SPEED ) {
        return false;
    }

    return QM_Vector3DotProduct( velocity, normal ) / speed < MM_PLANE_ENTER_THRESHOLD;
}

/**
 * @brief Clips the velocity to surface normal.
 **/
const int32_t SVG_MMove_ClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const float overbounce ) {
	// Whether we're actually blocked or not.
	int32_t blocked = MM_VELOCITY_CLIPPED_NONE;
	// If the plane that is blocking us has a positive z component, then assume it's a floor.
	if ( normal.z > 0 /*PM_MIN_WALL_NORMAL_Z*/ && normal.z != 1 ) {
		blocked |= MM_VELOCITY_CLIPPED_FLOOR;
	}
	// A plane with no upward support is vertical Wall/Step:
	if ( normal.z == 0.0f ) {
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
const mm_slide_move_flags_t SVG_MMove_SlideMove( Vector3 &origin, Vector3 &velocity, const float frametime, const Vector3 &mins, const Vector3 &maxs, svg_base_edict_t *passEntity, mm_touch_trace_list_t &touch_traces, const bool has_time, mm_trace_shape_t override_shape ) {
	Vector3 dir = {};

	Vector3 planes[ MM_MAX_CLIP_PLANES ] = {};

	svg_trace_t	trace = {};
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
		trace = SVG_MMove_Trace( origin, mins, maxs, end, passEntity, CONTENTS_NONE, override_shape );

		if ( trace.allsolid ) {
			// Entity is trapped in another solid, DON'T build up falling damage.
			velocity[ 2 ] = 0;
			// Save entity for contact.
			SVG_MMove_RegisterTouchTrace( touch_traces, trace );
			// Return trapped mask.
			return MM_SLIDEMOVEFLAG_TRAPPED;
		}

		if ( trace.startsolid ) {
			origin += Vector3( trace.plane.normal ) * 0.25f;
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

		/**
		* Treat near-identical analytical normals as one plane so floating-point variation
		* does not consume the clip-plane budget at a stair seam.
		**/
		for ( i = 0; i < numplanes; i++ ) {
			const bool isVerticalWall = std::fabs( trace.plane.normal[ 2 ] ) < 0.1f && std::fabs( planes[ i ].z ) < 0.1f;
			const float repeatDot = isVerticalWall ? MM_VERTICAL_PLANE_REPEAT_DOT : MM_PLANE_REPEAT_DOT;
			if ( QM_Vector3DotProduct( trace.plane.normal, planes[ i ] ) > repeatDot ) {
				velocity += trace.plane.normal;
				break;
			}
		}
		if ( i < numplanes ) {
			// Repeat the trace after nudging velocity away from the already-known plane.
			continue;
		}

		/**
		* Store the new contact plane before solving the one-, two-, and three-plane cases.
		**/
		if ( numplanes >= MM_MAX_CLIP_PLANES ) {
			// Exhausting the plane budget means the mover is genuinely wedged.
			velocity = {};
			blockedMask = MM_SLIDEMOVEFLAG_TRAPPED;
			break;
		}
		planes[ numplanes ] = trace.plane.normal;
		numplanes++;

		/**
		* Find a velocity that leaves every accumulated plane without clipping against surfaces
		* that the mover is already leaving.
		**/
		const Vector3 original_velocity = velocity;
		Vector3 clipVelocity = {};
		bool foundClipVelocity = false;
		for ( int32_t clipPlaneIndex = 0; clipPlaneIndex < numplanes; clipPlaneIndex++ ) {
			if ( !SVG_MMove_IsEnteringPlane( original_velocity, planes[ clipPlaneIndex ] ) ) {
				continue;
			}
			foundClipVelocity = true;

			SVG_MMove_ClipVelocity( original_velocity, planes[ clipPlaneIndex ], clipVelocity, 1.01f );
			int32_t secondPlaneIndex = 0;
			for ( secondPlaneIndex = 0; secondPlaneIndex < numplanes; secondPlaneIndex++ ) {
				if ( secondPlaneIndex == clipPlaneIndex || !SVG_MMove_IsEnteringPlane( clipVelocity, planes[ secondPlaneIndex ] ) ) {
					continue;
				}

				SVG_MMove_ClipVelocity( clipVelocity, planes[ secondPlaneIndex ], clipVelocity, 1.01f );
				if ( QM_Vector3DotProduct( clipVelocity, planes[ clipPlaneIndex ] ) >= 0.0f ) {
					continue;
				}

				/**
				* Move along the crease of two blocking planes, then distinguish a valid flat-ground
				* corner from a true three-plane wedge.
				**/
				dir = QM_Vector3CrossProduct( planes[ clipPlaneIndex ], planes[ secondPlaneIndex ] );
				d = QM_Vector3DotProduct( dir, original_velocity );
				clipVelocity = QM_Vector3Scale( dir, d );

				for ( int32_t thirdPlaneIndex = 0; thirdPlaneIndex < numplanes; thirdPlaneIndex++ ) {
					if ( thirdPlaneIndex == clipPlaneIndex || thirdPlaneIndex == secondPlaneIndex || !SVG_MMove_IsEnteringPlane( clipVelocity, planes[ thirdPlaneIndex ] ) ) {
						continue;
					}

					const bool firstIsVertical = std::fabs( planes[ clipPlaneIndex ].z ) < 0.1f;
					const bool secondIsVertical = std::fabs( planes[ secondPlaneIndex ].z ) < 0.1f;
					if ( firstIsVertical && secondIsVertical && planes[ thirdPlaneIndex ].z >= MM_MIN_STEP_NORMAL ) {
						// A pair of walls plus a walkable floor is a valid flat-ground corner.
						SVG_MMove_ClipVelocity( clipVelocity, planes[ thirdPlaneIndex ], clipVelocity, 1.01f );
						continue;
					}

					// A third entering plane with no walkable floor is a genuine wedge trap.
					velocity = {};
					return blockedMask | MM_SLIDEMOVEFLAG_TRAPPED;
				}
			}
		}

		if ( !foundClipVelocity ) {
			/**
			* A contact that the mover is grazing or leaving is not a blocking plane.
			* Preserve that valid velocity and let the next trace clear the contact;
			* zeroing it here makes edge-following movement stick at fraction zero.
			**/
			if ( QM_Vector3Length( original_velocity ) <= MM_MIN_PLANE_CHECK_SPEED ) {
				velocity = {};
				break;
			}
			velocity = original_velocity;
			continue;
		}

		/**
		* If the clipped velocity magnitude is below the stop threshold, stop sliding.
		**/
		if ( QM_Vector3LengthSqr( clipVelocity ) < ( MM_STOP_EPSILON * MM_STOP_EPSILON ) ) {
			velocity = {};
			break;
		}

		// Commit the resolved slide direction and continue with the remaining frame time.
		velocity = clipVelocity;
	}

	if ( has_time ) {
		velocity = primal_velocity;
	}

	return blockedMask;
}
