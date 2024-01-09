/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "shared/shared.h"
#include "sg_pmove.h"
#include "sg_pmove_slidemove.h"

//! An actual pointer to the pmove object that we're moving.
extern pmove_t *pm;

// movement parameters
//float pm_stopspeed = 100;
//float pm_maxspeed = 300;
//float pm_duckspeed = 100;
//float pm_accelerate = 10;
//float pm_wateraccelerate = 10;
//float pm_friction = 6;
//float pm_waterfriction = 1;
//float pm_waterspeed = 400;
//float pm_laddermod = 0.5f;


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
trace_t PM_Clip( const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, int32_t contentMask ) {
	return pm->clip( start, mins, maxs, end, contentMask );
}

/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
trace_t PM_Trace( const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, int32_t contentMask ) {
	// Spectators only clip against world, so use clip instead.
	if ( pm->s.pm_type == PM_SPECTATOR ) {
		return PM_Clip( start, mins, maxs, end, MASK_SOLID );
	}

	if ( contentMask == 0 ) {
		if ( pm->s.pm_type == PM_DEAD || pm->s.pm_type == PM_GIB ) {
			contentMask = MASK_DEADSOLID;
		} else if ( pm->s.pm_type == PM_SPECTATOR ) {
			contentMask = MASK_SOLID;
		} else {
			contentMask = MASK_PLAYERSOLID;
		}

		//if ( pm->s.pm_flags & PMF_IGNORE_PLAYER_COLLISION )
		//	mask &= ~CONTENTS_PLAYER;
	}

	return pm->trace( start, mins, maxs, end, pm->player, contentMask );
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
inline void PM_RegisterTouchTrace( pm_touch_trace_list_t &touchTraceList, trace_t &trace ) {
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
void PM_ClipVelocity( const vec3_t in, const vec3_t normal, vec3_t out, float overbounce ) {
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
void PM_StepSlideMove_Generic( vec3_t &origin, vec3_t &velocity, float frametime, const vec3_t &mins, const vec3_t &maxs, pm_touch_trace_list_t &touch_traces, bool has_time ) {
	int bumpcount = 0, numbumps = 0;
	vec3_t dir = {};
	float d = 0;
	int numplanes = 0;
	vec3_t planes[ MAX_CLIP_PLANES ] = {};
	vec3_t primal_velocity = {};
	int i = 0, j = 0;
	trace_t trace = {};
	vec3_t end = {};
	float time_left = 0.f;

	numbumps = 4;

	VectorCopy( velocity, primal_velocity );
	numplanes = 0;

	time_left = frametime;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {
		VectorMA( origin, time_left, velocity, end );
		//for ( i = 0; i < 3; i++ )
		//	end[ i ] = origin[ i ] + time_left * velocity[ i ];
		trace = PM_Trace( origin, mins, maxs, end );

		if ( trace.allsolid ) {
			// entity is trapped in another solid
			velocity[ 2 ] = 0;    // don't build up falling damage

			// Save entity for contact.
			PM_RegisterTouchTrace( touch_traces, trace );
			return;
		}

		// [Paril-KEX] experimental attempt to fix stray collisions on curved
		// surfaces; easiest to see on q2dm1 by running/jumping against the sides
		// of the curved map.
		if ( trace.surface2 ) {
			vec3_t clipped_a, clipped_b;
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

		if ( trace.fraction > 0 ) {
			// actually covered some distance
			VectorCopy( trace.endpos, origin );
			numplanes = 0;
		}

		if ( trace.fraction == 1 ) {
			break;     // moved the entire distance
		}

		// Save entity for contact.
		PM_RegisterTouchTrace( touch_traces, trace );
		
		time_left -= time_left * trace.fraction;

		// slide along this plane
		if ( numplanes >= MAX_CLIP_PLANES ) {
			// this shouldn't really happen
			VectorClear( velocity );
			break;
		}

		VectorCopy( trace.plane.normal, planes[ numplanes ] );
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//
		for ( i = 0; i < numplanes; i++ ) {
			PM_ClipVelocity( velocity, planes[ i ], velocity, 1.01f );
			for ( j = 0; j < numplanes; j++ ) {
				if ( j != i ) {
					if ( DotProduct( velocity, planes[ j ] ) < 0 ) {
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
				VectorClear( velocity );
				break;
			}
			CrossProduct( planes[ 0 ], planes[ 1 ], dir );
			d = DotProduct( dir, velocity );
			VectorScale( dir, d, velocity );
		}

		//
		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		//
		if ( DotProduct( velocity, primal_velocity ) <= 0 ) {
			VectorClear( velocity );
			break;
		}
	}

	if ( has_time ) {
		VectorCopy( primal_velocity, velocity );
	}
}
