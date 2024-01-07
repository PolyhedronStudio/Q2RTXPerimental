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

#define STEPSIZE    18

/**
*	@brief	Actual in-moment local move variables.
*
*			All of the locals will be zeroed before each pmove, just to make damn sure we don't have
*			any differences when running on client or server
**/
typedef struct {
	//! Obvious origin and velocity.
	vec3_t      origin;
	vec3_t      velocity;

	//! Forward, right and up vectors.
	vec3_t      forward, right, up;
	//! Move frametime.
	float       frametime;

	//! Ground information.
	csurface_t *groundsurface;
	cplane_t    groundplane;
	int         groundcontents;

	//! Used to reset ourselves in case we are truly stuck.
	vec3_t      previous_origin;
	//! Used for calculating the fall impact with.
	vec3_t		start_velocity;
} pml_t;

//! An actual pointer to the pmove object that we're moving.
static pmove_t *pm;
//! An actual pointer to the pmove parameters object for use with moving.
static pmoveParams_t *pmp;
//! Contains our local in-moment move variables.
static pml_t pml;

// movement parameters
float pm_stopspeed = 100;
float pm_maxspeed = 300;
float pm_duckspeed = 100;
float pm_accelerate = 10;
float pm_wateraccelerate = 10;
float pm_friction = 6;
float pm_waterfriction = 1;
float pm_waterspeed = 400;
float pm_laddermod = 0.5f;


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
trace_t PM_Trace( const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, int32_t contentMask = 0 ) {
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
static inline void PM_RegisterTouchTrace( pm_touch_trace_list_t &touchTraceList, trace_t &trace ) {
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

/*
  walking up a step should kill some velocity
*/

/*
==================
PM_ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define STOP_EPSILON    0.1f

static void PM_ClipVelocity( const vec3_t in, const vec3_t normal, vec3_t out, float overbounce ) {
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
#define MIN_STEP_NORMAL 0.7f    // can't step up onto very steep slopes
#define MAX_CLIP_PLANES 5

static void PM_StepSlideMove_( void ) {
	int         bumpcount, numbumps;
	vec3_t      dir;
	float       d;
	int         numplanes;
	vec3_t      planes[ MAX_CLIP_PLANES ];
	vec3_t      primal_velocity;
	int         i, j;
	trace_t trace;
	vec3_t      end;
	float       time_left;

	numbumps = 4;

	VectorCopy( pml.velocity, primal_velocity );
	numplanes = 0;

	time_left = pml.frametime;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {
		for ( i = 0; i < 3; i++ )
			end[ i ] = pml.origin[ i ] + time_left * pml.velocity[ i ];

		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, end );

		if ( trace.allsolid ) {
			// entity is trapped in another solid
			pml.velocity[ 2 ] = 0;    // don't build up falling damage

			// Save entity for contact.
			PM_RegisterTouchTrace( pm->touchTraces, trace );
			return;
		}

		if ( trace.fraction > 0 ) {
			// actually covered some distance
			VectorCopy( trace.endpos, pml.origin );
			numplanes = 0;
		}

		if ( trace.fraction == 1 )
			break;     // moved the entire distance

		// Save entity for contact.
		PM_RegisterTouchTrace( pm->touchTraces, trace );
		//if ( pm->numtouch < MAX_TOUCH_TRACES && trace.ent ) {
		//	pm->touchents[ pm->numtouch ] = trace.ent;
		//	pm->numtouch++;
		//}

		time_left -= time_left * trace.fraction;

		// slide along this plane
		if ( numplanes >= MAX_CLIP_PLANES ) {
			// this shouldn't really happen
			VectorClear( pml.velocity );
			break;
		}

		VectorCopy( trace.plane.normal, planes[ numplanes ] );
		numplanes++;

//
// modify original_velocity so it parallels all of the clip planes
//
		for ( i = 0; i < numplanes; i++ ) {
			PM_ClipVelocity( pml.velocity, planes[ i ], pml.velocity, 1.01f );
			for ( j = 0; j < numplanes; j++ )
				if ( j != i ) {
					if ( DotProduct( pml.velocity, planes[ j ] ) < 0 )
						break;  // not ok
				}
			if ( j == numplanes )
				break;
		}

		if ( i != numplanes ) {
			// go along this plane
		} else {
			// go along the crease
			if ( numplanes != 2 ) {
				VectorClear( pml.velocity );
				break;
			}
			CrossProduct( planes[ 0 ], planes[ 1 ], dir );
			d = DotProduct( dir, pml.velocity );
			VectorScale( dir, d, pml.velocity );
		}

		//
		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		//
		if ( DotProduct( pml.velocity, primal_velocity ) <= 0 ) {
			VectorClear( pml.velocity );
			break;
		}
	}

	if ( pm->s.pm_time )
		VectorCopy( primal_velocity, pml.velocity );
}

/*
==================
PM_StepSlideMove

==================
*/
static void PM_StepSlideMove( void ) {
	vec3_t      start_o, start_v;
	vec3_t      down_o, down_v;
	trace_t     trace;
	float       down_dist, up_dist;
	vec3_t      up, down;

	VectorCopy( pml.origin, start_o );
	VectorCopy( pml.velocity, start_v );

	PM_StepSlideMove_( );

	VectorCopy( pml.origin, down_o );
	VectorCopy( pml.velocity, down_v );

	VectorCopy( start_o, up );
	up[ 2 ] += STEPSIZE;

	trace = PM_Trace( up, pm->mins, pm->maxs, up );
	if ( trace.allsolid )
		return;     // can't step up

	// try sliding above
	VectorCopy( up, pml.origin );
	VectorCopy( start_v, pml.velocity );

	PM_StepSlideMove_( );

	// push down the final amount
	VectorCopy( pml.origin, down );
	down[ 2 ] -= STEPSIZE;
	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );
	if ( !trace.allsolid )
		VectorCopy( trace.endpos, pml.origin );

	VectorCopy( pml.origin, up );

	// decide which one went farther
	down_dist = ( down_o[ 0 ] - start_o[ 0 ] ) * ( down_o[ 0 ] - start_o[ 0 ] )
		+ ( down_o[ 1 ] - start_o[ 1 ] ) * ( down_o[ 1 ] - start_o[ 1 ] );
	up_dist = ( up[ 0 ] - start_o[ 0 ] ) * ( up[ 0 ] - start_o[ 0 ] )
		+ ( up[ 1 ] - start_o[ 1 ] ) * ( up[ 1 ] - start_o[ 1 ] );

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < MIN_STEP_NORMAL ) {
		VectorCopy( down_o, pml.origin );
		VectorCopy( down_v, pml.velocity );
		return;
	}
	//!! Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity[ 2 ] = down_v[ 2 ];
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction( void ) {
	float *vel;
	float   speed, newspeed, control;
	float   friction;
	float   drop;

	vel = pml.velocity;

	speed = VectorLength( vel );
	if ( speed < 1 ) {
		vel[ 0 ] = 0;
		vel[ 1 ] = 0;
		return;
	}

	drop = 0;

// apply ground friction
	if ( ( pm->groundentity && pml.groundsurface && !( pml.groundsurface->flags & SURF_SLICK ) ) || ( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		friction = pmp->friction;
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;
	}

// apply water friction
	if ( pm->waterlevel > water_level_t::WATER_NONE && !( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		drop += speed * pmp->waterfriction * pm->waterlevel * pml.frametime;
	}

// scale the velocity
	newspeed = speed - drop;
	if ( newspeed < 0 ) {
		newspeed = 0;
	}
	newspeed /= speed;

	vel[ 0 ] = vel[ 0 ] * newspeed;
	vel[ 1 ] = vel[ 1 ] * newspeed;
	vel[ 2 ] = vel[ 2 ] * newspeed;
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
static void PM_Accelerate( const vec3_t wishdir, float wishspeed, float accel ) {
	int         i;
	float       addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if ( addspeed <= 0 )
		return;
	accelspeed = accel * pml.frametime * wishspeed;
	if ( accelspeed > addspeed )
		accelspeed = addspeed;

	for ( i = 0; i < 3; i++ )
		pml.velocity[ i ] += accelspeed * wishdir[ i ];
}

static void PM_AirAccelerate( const vec3_t wishdir, float wishspeed, float accel ) {
	int         i;
	float       addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if ( wishspd > 30 )
		wishspd = 30;
	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspd - currentspeed;
	if ( addspeed <= 0 )
		return;
	accelspeed = accel * wishspeed * pml.frametime;
	if ( accelspeed > addspeed )
		accelspeed = addspeed;

	for ( i = 0; i < 3; i++ )
		pml.velocity[ i ] += accelspeed * wishdir[ i ];
}

/*
=============
PM_AddCurrents
=============
*/
static void PM_AddCurrents( vec3_t wishvel ) {
	vec3_t  v;
	float   s;

	//
	// account for ladders
	//

	if ( ( pm->s.pm_flags & PMF_ON_LADDER ) && fabsf( pml.velocity[ 2 ] ) <= 200 ) {
		if ( ( pm->viewangles[ PITCH ] <= -15 ) && ( pm->cmd.forwardmove > 0 ) ) {
			wishvel[ 2 ] = 200;
		} else if ( ( pm->viewangles[ PITCH ] >= 15 ) && ( pm->cmd.forwardmove > 0 ) ) {
			wishvel[ 2 ] = -200;
		} else if ( pm->cmd.upmove > 0 ) {
			wishvel[ 2 ] = 200;
		} else if ( pm->cmd.upmove < 0 ) {
			wishvel[ 2 ] = -200;
		} else {
			wishvel[ 2 ] = 0;
		}
		// limit horizontal speed when on a ladder
		clamp( wishvel[ 0 ], -25, 25 );
		clamp( wishvel[ 1 ], -25, 25 );
	}

	//
	// add water currents
	//

	if ( pm->watertype & MASK_CURRENT ) {
		VectorClear( v );

		if ( pm->watertype & CONTENTS_CURRENT_0 )
			v[ 0 ] += 1;
		if ( pm->watertype & CONTENTS_CURRENT_90 )
			v[ 1 ] += 1;
		if ( pm->watertype & CONTENTS_CURRENT_180 )
			v[ 0 ] -= 1;
		if ( pm->watertype & CONTENTS_CURRENT_270 )
			v[ 1 ] -= 1;
		if ( pm->watertype & CONTENTS_CURRENT_UP )
			v[ 2 ] += 1;
		if ( pm->watertype & CONTENTS_CURRENT_DOWN )
			v[ 2 ] -= 1;

		s = pm_waterspeed;
		if ( ( pm->waterlevel == water_level_t::WATER_FEET ) && ( pm->groundentity ) )
			s /= 2;

		VectorMA( wishvel, s, v, wishvel );
	}

	//
	// add conveyor belt velocities
	//

	if ( pm->groundentity ) {
		VectorClear( v );

		if ( pml.groundcontents & CONTENTS_CURRENT_0 )
			v[ 0 ] += 1;
		if ( pml.groundcontents & CONTENTS_CURRENT_90 )
			v[ 1 ] += 1;
		if ( pml.groundcontents & CONTENTS_CURRENT_180 )
			v[ 0 ] -= 1;
		if ( pml.groundcontents & CONTENTS_CURRENT_270 )
			v[ 1 ] -= 1;
		if ( pml.groundcontents & CONTENTS_CURRENT_UP )
			v[ 2 ] += 1;
		if ( pml.groundcontents & CONTENTS_CURRENT_DOWN )
			v[ 2 ] -= 1;

		VectorMA( wishvel, 100 /* pm->groundentity->speed */, v, wishvel );
	}
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove( void ) {
	int     i;
	vec3_t  wishvel;
	float   wishspeed;
	vec3_t  wishdir;

//
// user intentions
//
	for ( i = 0; i < 3; i++ )
		wishvel[ i ] = pml.forward[ i ] * pm->cmd.forwardmove + pml.right[ i ] * pm->cmd.sidemove;

	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove && !pm->cmd.upmove )
		wishvel[ 2 ] -= 60;       // drift towards bottom
	else
		wishvel[ 2 ] += pm->cmd.upmove;

	PM_AddCurrents( wishvel );

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	if ( wishspeed > pmp->maxspeed ) {
		VectorScale( wishvel, pmp->maxspeed / wishspeed, wishvel );
		wishspeed = pmp->maxspeed;
	}
	wishspeed *= pmp->watermult;

	PM_Accelerate( wishdir, wishspeed, pm_wateraccelerate );

	PM_StepSlideMove( );
}

/*
===================
PM_AirMove

===================
*/
static void PM_AirMove( void ) {
	int         i;
	vec3_t      wishvel;
	float       fmove, smove;
	vec3_t      wishdir;
	float       wishspeed;
	float       maxspeed;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	for ( i = 0; i < 2; i++ )
		wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;
	wishvel[ 2 ] = 0;

	PM_AddCurrents( wishvel );

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

//
// clamp to server defined max speed
//
	maxspeed = ( pm->s.pm_flags & PMF_DUCKED ) ? pm_duckspeed : pmp->maxspeed;

	if ( wishspeed > maxspeed ) {
		VectorScale( wishvel, maxspeed / wishspeed, wishvel );
		wishspeed = maxspeed;
	}

	if ( pm->s.pm_flags & PMF_ON_LADDER ) {
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );
		if ( !wishvel[ 2 ] ) {
			if ( pml.velocity[ 2 ] > 0 ) {
				pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;
				if ( pml.velocity[ 2 ] < 0 )
					pml.velocity[ 2 ] = 0;
			} else {
				pml.velocity[ 2 ] += pm->s.gravity * pml.frametime;
				if ( pml.velocity[ 2 ] > 0 )
					pml.velocity[ 2 ] = 0;
			}
		}
		PM_StepSlideMove( );
	} else if ( pm->groundentity ) {
		// walking on ground
		pml.velocity[ 2 ] = 0; //!!! this is before the accel
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

// PGM  -- fix for negative trigger_gravity fields
//      pml.velocity[2] = 0;
		if ( pm->s.gravity > 0 )
			pml.velocity[ 2 ] = 0;
		else
			pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;
// PGM

		if ( !pml.velocity[ 0 ] && !pml.velocity[ 1 ] )
			return;
		PM_StepSlideMove( );
	} else {
		// not on ground, so little effect on velocity
		if ( pmp->airaccelerate )
			PM_AirAccelerate( wishdir, wishspeed, pm_accelerate );
		else
			PM_Accelerate( wishdir, wishspeed, 1 );
		// add gravity
		pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;
		PM_StepSlideMove( );
	}
}

/*
=============
PM_CategorizePosition
=============
*/
static void PM_CategorizePosition( void ) {
	vec3_t      point;
	int         cont;
	trace_t     trace;
	int         sample1;
	int         sample2;

// if the player hull point one unit down is solid, the player
// is on ground

// see if standing on something solid
	point[ 0 ] = pml.origin[ 0 ];
	point[ 1 ] = pml.origin[ 1 ];
	point[ 2 ] = pml.origin[ 2 ] - 0.25f;
	if ( pml.velocity[ 2 ] > 180 ) { //!!ZOID changed from 100 to 180 (ramp accel)
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = NULL;
	} else {
		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, point );
		pm->groundplane = trace.plane;
		pml.groundsurface = trace.surface;
		pml.groundcontents = trace.contents;

		if ( !trace.ent || ( trace.plane.normal[ 2 ] < 0.7f && !trace.startsolid ) ) {
			pm->groundentity = NULL;
			pm->s.pm_flags &= ~PMF_ON_GROUND;
		} else {
			pm->groundentity = trace.ent;

			// hitting solid ground will end a waterjump
			if ( pm->s.pm_flags & PMF_TIME_WATERJUMP ) {
				pm->s.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
				pm->s.pm_time = 0;
			}

			if ( !( pm->s.pm_flags & PMF_ON_GROUND ) ) {
				// just hit the ground
				pm->s.pm_flags |= PMF_ON_GROUND;
				// don't do landing time if we were just going down a slope
				if ( pml.velocity[ 2 ] < -200 && !pmp->strafehack ) {
					pm->s.pm_flags |= PMF_TIME_LAND;
					// don't allow another jump for a little while
					if ( pml.velocity[ 2 ] < -400 )
						pm->s.pm_time = 25;
					else
						pm->s.pm_time = 18;
				}
			}
		}

		// Save entity for contact.
		PM_RegisterTouchTrace( pm->touchTraces, trace );
	}

//
// get waterlevel, accounting for ducking
//
	pm->waterlevel = water_level_t::WATER_NONE;
	pm->watertype = 0;

	sample2 = pm->s.viewheight - pm->mins[ 2 ];
	sample1 = sample2 / 2;

	point[ 2 ] = pml.origin[ 2 ] + pm->mins[ 2 ] + 1;
	cont = pm->pointcontents( point );

	if ( cont & MASK_WATER ) {
		pm->watertype = cont;
		pm->waterlevel = water_level_t::WATER_FEET;
		point[ 2 ] = pml.origin[ 2 ] + pm->mins[ 2 ] + sample1;
		cont = pm->pointcontents( point );
		if ( cont & MASK_WATER ) {
			pm->waterlevel = water_level_t::WATER_WAIST;
			point[ 2 ] = pml.origin[ 2 ] + pm->mins[ 2 ] + sample2;
			cont = pm->pointcontents( point );
			if ( cont & MASK_WATER )
				pm->waterlevel = water_level_t::WATER_UNDER;
		}
	}
}

/*
=============
PM_CheckJump
=============
*/
static void PM_CheckJump( void ) {
	if ( pm->s.pm_flags & PMF_TIME_LAND ) {
		// hasn't been long enough since landing to jump again
		return;
	}

	if ( pm->cmd.upmove < 10 ) {
		// not holding jump
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// must wait for jump to be released
	if ( pm->s.pm_flags & PMF_JUMP_HELD )
		return;

	if ( pm->s.pm_type == PM_DEAD )
		return;

	if ( pm->waterlevel >= water_level_t::WATER_WAIST ) {
		// swimming, not jumping
		pm->groundentity = NULL;

		if ( pmp->waterhack )
			return;

		if ( pml.velocity[ 2 ] <= -300 )
			return;

		// FIXME: makes velocity dependent on client FPS,
		// even causes prediction misses
		if ( pm->watertype == CONTENTS_WATER )
			pml.velocity[ 2 ] = 100;
		else if ( pm->watertype == CONTENTS_SLIME )
			pml.velocity[ 2 ] = 80;
		else
			pml.velocity[ 2 ] = 50;
		return;
	}

	if ( pm->groundentity == NULL )
		return;     // in air, so no effect

	pm->s.pm_flags |= PMF_JUMP_HELD;

	pm->groundentity = NULL;
	pm->s.pm_flags &= ~PMF_ON_GROUND;
	pml.velocity[ 2 ] += 270;
	if ( pml.velocity[ 2 ] < 270 )
		pml.velocity[ 2 ] = 270;
}

/*
=============
PM_CheckSpecialMovement
=============
*/
static void PM_CheckSpecialMovement( void ) {
	vec3_t  spot;
	int     cont;
	vec3_t  flatforward;
	trace_t trace;

	if ( pm->s.pm_time )
		return;

	// Remove on ladder flag.
	pm->s.pm_flags &= ~PMF_ON_LADDER;

	// check for ladder
	flatforward[ 0 ] = pml.forward[ 0 ];
	flatforward[ 1 ] = pml.forward[ 1 ];
	flatforward[ 2 ] = 0;
	VectorNormalize( flatforward );

	VectorMA( pml.origin, 1, flatforward, spot );
	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot );
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) ) {
		// Add ON_LADDER flag.
		pm->s.pm_flags |= PMF_ON_LADDER;
	}

	// check for water jump
	if ( pm->waterlevel != water_level_t::WATER_WAIST )
		return;

	VectorMA( pml.origin, 30, flatforward, spot );
	spot[ 2 ] += 4;
	cont = pm->pointcontents( spot );
	if ( !( cont & CONTENTS_SOLID ) )
		return;

	spot[ 2 ] += 16;
	cont = pm->pointcontents( spot );
	if ( cont )
		return;
	// jump out of water
	VectorScale( flatforward, 50, pml.velocity );
	pml.velocity[ 2 ] = 350;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 255;
}

/*
===============
PM_FlyMove
===============
*/
static void PM_FlyMove( const bool performNoClipping ) {
	float   speed, drop, friction, control, newspeed;
	float   currentspeed, addspeed, accelspeed;
	int         i;
	vec3_t      wishvel;
	float       fmove, smove;
	vec3_t      wishdir;
	float       wishspeed;

	pm->s.viewheight = performNoClipping ? 0 : 22;

	// friction
	speed = VectorLength( pml.velocity );
	if ( speed < 1 ) {
		VectorClear( pml.velocity );
	} else {
		drop = 0;

		friction = pmp->flyfriction;
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if ( newspeed < 0 )
			newspeed = 0;
		newspeed /= speed;

		VectorScale( pml.velocity, newspeed, pml.velocity );
	}

	// accelerate
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for ( i = 0; i < 3; i++ )
		wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;
	wishvel[ 2 ] += pm->cmd.upmove;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	//
	// clamp to server defined max speed
	//
	if ( wishspeed > pmp->maxspeed ) {
		VectorScale( wishvel, pmp->maxspeed / wishspeed, wishvel );
		wishspeed = pmp->maxspeed;
	}

	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if ( addspeed <= 0 ) {
		if ( !pmp->flyhack ) {
			return; // original buggy behaviour
		}
	} else {
		accelspeed = pm_accelerate * pml.frametime * wishspeed;
		if ( accelspeed > addspeed )
			accelspeed = addspeed;

		for ( i = 0; i < 3; i++ )
			pml.velocity[ i ] += accelspeed * wishdir[ i ];
	}

	if ( performNoClipping ) {
		PM_StepSlideMove( );
	} else {
		// move
		VectorMA( pml.origin, pml.frametime, pml.velocity, pml.origin );
	}
}

/**
*	@brief	Update the player's bounding box dimensions.
**/
static void PM_SetDimensions() {
	pm->mins[ 0 ] = -16;
	pm->mins[ 1 ] = -16;

	pm->maxs[ 0 ] = 16;
	pm->maxs[ 1 ] = 16;

	if ( pm->s.pm_type == PM_GIB ) {
		pm->mins[ 2 ] = 0;
		pm->maxs[ 2 ] = 16;
		pm->s.viewheight = 8;
		return;
	}

	pm->mins[ 2 ] = -24;

	if ( ( pm->s.pm_flags & PMF_DUCKED ) || pm->s.pm_type == PM_DEAD ) {
		pm->maxs[ 2 ] = 4;
		pm->s.viewheight = -2;
	}
	else {
		pm->maxs[ 2 ] = 32;
		pm->s.viewheight = 22;
	}
}

/**
*	@return	True if we're still 'heads above water'. False otherwise.
**/
static inline bool PM_AboveWater() {
	const vec3_t below = { pml.origin[ 0 ], pml.origin[ 1 ], pml.origin[ 2 ] - 8.f };

	bool solid_below = pm->trace( pml.origin, pm->mins, pm->maxs, below, (const void *)pm->player, MASK_SOLID ).fraction < 1.0f;

	if ( solid_below )
		return false;

	bool water_below = pm->trace( pml.origin, pm->mins, pm->maxs, below, (const void*)pm->player, MASK_WATER ).fraction < 1.0f;

	if ( water_below )
		return true;

	return false;
}

/**
*	@brief	Check if we can duck, if so, adjust player boundingbox dimensions.
**/
static const bool PM_CheckDuck( void ) {
	// Gibs can't duck.
	if ( pm->s.pm_type == PM_GIB ) {
		return false;
	}
	trace_t trace;
	bool hasFlagsChanged = false;

	if ( pm->s.pm_type == PM_DEAD ) {
		if ( !( pm->s.pm_flags & PMF_DUCKED ) ) {
			pm->s.pm_flags |= PMF_DUCKED;
			hasFlagsChanged = true;
		}
	} else if ( ( pm->cmd.upmove < 0 ) &&//( pm->cmd.buttons & BUTTON_CROUCH ) &&
		( pm->groundentity || ( pm->waterlevel <= water_level_t::WATER_FEET && !PM_AboveWater() ) ) &&
		!( pm->s.pm_flags & PMF_ON_LADDER ) ) {
			if ( !( pm->s.pm_flags & PMF_DUCKED ) ) {
				// check that duck won't be blocked
				vec3_t check_maxs = { pm->maxs[ 0 ], pm->maxs[ 1 ], 4 };
				trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin );
				if ( !trace.allsolid ) {
					pm->s.pm_flags |= PMF_DUCKED;
					hasFlagsChanged = true;
				}
			}
	} else { // stand up if possible
		if ( pm->s.pm_flags & PMF_DUCKED ) {
			// try to stand up
			vec3_t check_maxs = { pm->maxs[ 0 ], pm->maxs[ 1 ], 32 };
			trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin ); //trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin );
			if ( !trace.allsolid ) {
				pm->s.pm_flags &= ~PMF_DUCKED;
				hasFlagsChanged = true;
			}
		}
	}

	if ( !hasFlagsChanged ) {
		return false;
	}

	PM_SetDimensions();
	return true;
}

/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove( void ) {
	float   forward;

	if ( !pm->groundentity )
		return;

	// extra friction
	forward = VectorLength( pml.velocity );
	forward -= 20;
	if ( forward <= 0 ) {
		VectorClear( pml.velocity );
	} else {
		VectorNormalize( pml.velocity );
		VectorScale( pml.velocity, forward, pml.velocity );
	}
}

/**
*	@brief	Determine whether the position is allsolid or not. In case of PM_NOCLIP it is always a good position.
**/
static bool PM_GoodPosition( void ) {
	// Always valid for PM_NOCLIP mode.
	if ( pm->s.pm_type == PM_NOCLIP ) {
		return true;
	}

	// Base the outcome on whether we're in an 'all-solid' situation or not.
	trace_t trace = PM_Trace( pm->s.origin, pm->mins, pm->maxs, pm->s.origin );
	return !trace.allsolid;
}

/**
*	@brief	On exit, the origin will have a value that is pre-quantized to 
*			the network channel and in a valid position.
**/
static void PM_SnapPosition( void ) {
	VectorCopy( pml.origin, pm->s.origin );
	VectorCopy( pml.velocity, pm->s.velocity );

	if ( PM_GoodPosition( ) ) {
		return;
	}

	VectorCopy( pml.previous_origin, pm->s.origin );
}

/*
================
PM_InitialSnapPosition

================
*/
static void PM_InitialSnapPosition( void ) {
	// Do a check for a valid position to copy back as, into 'player move last previous origin'.
	if ( PM_GoodPosition( ) ) {
		VectorCopy( pm->s.origin, pml.origin );
		VectorCopy( pm->s.origin, pml.previous_origin );
	}
}

/*
================
PM_ClampAngles

================
*/
static void PM_ClampAngles( void ) {
	if ( pm->s.pm_flags & PMF_TIME_TELEPORT ) {
		pm->viewangles[ YAW ] = pm->cmd.angles[ YAW ] + ( pm->s.delta_angles[ YAW ] );
		pm->viewangles[ PITCH ] = 0;
		pm->viewangles[ ROLL ] = 0;
	} else {
		// circularly clamp the angles with deltas
		VectorAdd( pm->cmd.angles, pm->s.delta_angles, pm->viewangles );

		// don't let the player look up or down more than 90 degrees
		if ( pm->viewangles[ PITCH ] > 89 && pm->viewangles[ PITCH ] < 180 ) {
			pm->viewangles[ PITCH ] = 89;
		} else if ( pm->viewangles[ PITCH ] < 271 && pm->viewangles[ PITCH ] >= 180 ) {
			pm->viewangles[ PITCH ] = 271;
		}
	}

	AngleVectors( pm->viewangles, pml.forward, pml.right, pml.up );
}

/*
================
PM_ScreenEffects

================
*/
//static void PM_ScreenEffects( ) {
//	// add for contents
//	vec3_t vieworg = pml.origin + pm->viewoffset + vec3_t{ 0, 0, (float)pm->s.viewheight };
//	int32_t contents = pm->pointcontents( vieworg );
//
//	if ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) )
//		pm->rdflags |= RDF_UNDERWATER;
//	else
//		pm->rdflags &= ~RDF_UNDERWATER;
//
//	if ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) )
//		G_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, pm->screen_blend );
//	else if ( contents & CONTENTS_SLIME )
//		G_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, pm->screen_blend );
//	else if ( contents & CONTENTS_WATER )
//		G_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, pm->screen_blend );
//}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void SG_PlayerMove( pmove_t *pmove, pmoveParams_t *params ) {
	pm = pmove;
	pmp = params;

	// Clear out previous old pointer members for a new move.
	pm->touchTraces.numberOfTraces = 0;
	VectorClear( pm->viewangles );//pm->viewangles = {};
	pm->s.viewheight = 0;
	pm->groundentity = nullptr;
	pm->watertype = 0;
	pm->waterlevel = water_level_t::WATER_NONE;
	//pm->screenblend = {};
	//pm->rdflags = 0;
	//pm->jump_sound = false;
	//pm->step_clip = false;
	//pm->impact_delta = false;

	// Clear all pmove local vars
	pml = {};

	// Store the origin and velocity in pmove local.
	VectorCopy( pm->s.origin, pml.origin );
	VectorCopy( pm->s.velocity, pml.velocity );
	// Save the origin as 'old origin' for in case we get stuck.
	VectorCopy( pm->s.origin, pml.previous_origin );
	// Save the start velocity.
	VectorCopy( pm->s.velocity, pml.start_velocity );

	// Calculate frametime.
	pml.frametime = pm->cmd.msec * 0.001f;

	// Clamp view angles.
	PM_ClampAngles( );

	if ( pm->s.pm_type == PM_SPECTATOR || pm->s.pm_type == PM_NOCLIP ) {
		// Updated speed multiplied frametime for spectator and noclip move types.
		pml.frametime = pmp->speedmult * pm->cmd.msec * 0.001f;

		// Re-ensure no flags are set anymore.
		pm->s.pm_flags = PMF_NONE;

		// Give the spectator a small 8x8x8 bounding box.
		if ( pm->s.pm_type == PM_SPECTATOR ) {
			pm->mins[ 0 ] = -8;
			pm->mins[ 1 ] = -8;
			pm->mins[ 2 ] = -8;
			pm->maxs[ 0 ] = 8;
			pm->maxs[ 1 ] = 8;
			pm->maxs[ 2 ] = 8;
		}

		PM_FlyMove( pm->s.pm_type == PM_SPECTATOR );
		PM_SnapPosition( );
		return;
	}

	if ( pm->s.pm_type >= PM_DEAD ) {
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
	}

	if ( pm->s.pm_type == PM_FREEZE ) {
		return;     // no movement at all
	}

	// set mins, maxs, and viewheight
	PM_SetDimensions();

	// catagorize for ducking
	PM_CategorizePosition();

	if ( pm->snapinitial ) {
		PM_InitialSnapPosition();
	}

	// set groundentity, watertype, and waterlevel
	if ( PM_CheckDuck() ) {
		PM_CategorizePosition();
	}

	if ( pm->s.pm_type == PM_DEAD ) {
		PM_DeadMove();
	}

	PM_CheckSpecialMovement();

	// Drop timing counter.
	if ( pm->s.pm_time ) {
		int32_t msec = pm->cmd.msec; //int msec = pm->cmd.msec >> 3;
		if ( !msec ) {
			msec = 0; // msec = 1;
		}

		if ( msec >= pm->s.pm_time ) {
			pm->s.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
			pm->s.pm_time = 0;
		} else {
			pm->s.pm_time -= msec;
		}
	}

	// Teleport pause stays exactly in place:
	if ( pm->s.pm_flags & PMF_TIME_TELEPORT ) {
		
	// Waterjump has no control, but falls:
	} else if ( pm->s.pm_flags & PMF_TIME_WATERJUMP ) {
		pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;
		// Cancel as soon as we are falling down again.
		if ( pml.velocity[ 2 ] < 0 ) { // cancel as soon as we are falling down again
			pm->s.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
			pm->s.pm_time = 0;
		}

		PM_StepSlideMove();
	} else {
		// Check for jumping.
		PM_CheckJump( );
		// Apply friction.
		PM_Friction( );

		// Determine water level and pursue to WaterMove if deep in above waist.
		if ( pm->waterlevel >= water_level_t::WATER_WAIST ) {
			PM_WaterMove( );
		} else {
			// Regular 'air move'.
			vec3_t  angles;

			VectorCopy( pm->viewangles, angles );
			if ( angles[ PITCH ] > 180 )
				angles[ PITCH ] = angles[ PITCH ] - 360;
			angles[ PITCH ] /= 3;

			AngleVectors( angles, pml.forward, pml.right, pml.up );

			PM_AirMove( );
		}
	}

	// set groundentity, watertype, and waterlevel for final spot
	PM_CategorizePosition();

	// trick jump
	if ( pm->s.pm_flags & PMF_TIME_TRICK_JUMP ) {
		PM_CheckJump();
	}

	// [Paril-KEX]
	//PM_ScreenEffects();

	PM_SnapPosition();
}

void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp ) {
	// Q2RTXPerimental Defaults:
	pmp->speedmult = 2;
	pmp->watermult = 0.5f;
	pmp->maxspeed = 320;
	pmp->friction = 6;
	pmp->waterfriction = 1;
	pmp->flyfriction = 4;
}

