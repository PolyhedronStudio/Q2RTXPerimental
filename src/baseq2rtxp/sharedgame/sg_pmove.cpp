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

static constexpr float STEPSIZE = 18.f;

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
	//! Contents of ground brush.
	int         groundcontents;

	//! Used to reset ourselves in case we are truly stuck.
	vec3_t      previous_origin;
	//! Used for calculating the fall impact with.
	vec3_t		start_velocity;
} pml_t;

//! An actual pointer to the pmove object that we're moving.
pmove_t *pm;
//! An actual pointer to the pmove parameters object for use with moving.
static pmoveParams_t *pmp;
//! Contains our local in-moment move variables.
static pml_t pml;

// movement parameters
static constexpr float pm_stopspeed = 100.f;
static constexpr float pm_maxspeed = 300.f;
static constexpr float pm_ladderspeed = 200.f;
static constexpr float pm_ladder_sidemove_speed = 150.f;
static constexpr float pm_duckspeed = 100.f;
static constexpr float pm_accelerate = 10.f;
static constexpr float pm_wateraccelerate = 10.f;
static constexpr float pm_friction = 6.f;
static constexpr float pm_waterfriction = 1.f;
static constexpr float pm_waterspeed = 400.f;
static constexpr float pm_laddermod = 0.5f;



/**
*	@brief	Each intersection will try to step over the obstruction instead of
*			sliding along it.
* 
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
static void PM_StepSlideMove( void ) {
	vec3_t      start_o, start_v;
	vec3_t      down_o, down_v;
	trace_t     trace;
	float       down_dist, up_dist;
	vec3_t      up, down;

	VectorCopy( pml.origin, start_o );
	VectorCopy( pml.velocity, start_v );

	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frametime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

	VectorCopy( pml.origin, down_o );
	VectorCopy( pml.velocity, down_v );

	VectorCopy( start_o, up );
	up[ 2 ] += STEPSIZE;

	trace = PM_Trace( up, pm->mins, pm->maxs, up );
	// Can't step up.
	if ( trace.allsolid ) {
		return;
	}

	// Try sliding above.
	VectorCopy( up, pml.origin );
	VectorCopy( start_v, pml.velocity );

	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frametime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

	// Push down the final amount.
	VectorCopy( pml.origin, down );
	down[ 2 ] -= STEPSIZE;

	// [Paril-KEX] jitspoe suggestion for stair clip fix; store
	// the old down position, and pick a better spot for downwards
	// trace if the start origin's Z position is lower than the down end pt.
	vec3_t original_down = { down[ 0 ], down[ 1 ], down[ 2 ] };

	if ( start_o[ 2 ] < down[ 2 ] ) {
		down[ 2 ] = start_o[ 2 ] - 1.f; // 1.f ?
	}

	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );
	if ( !trace.allsolid ) {
		// From above, do the proper trace now.
		trace_t realTrace = PM_Trace( pml.origin, pm->mins, pm->maxs, original_down );
		VectorCopy( realTrace.endpos, pml.origin );

		// Only upwards jumps are stair clips.
		if ( pml.velocity[ 2 ] > 0.f ) {
			pm->step_clip = true;
		}
	}

	VectorCopy( pml.origin, up );

	// Decide which one went farther.
	down_dist = ( down_o[ 0 ] - start_o[ 0 ] ) * ( down_o[ 0 ] - start_o[ 0 ] ) + ( down_o[ 1 ] - start_o[ 1 ] ) * ( down_o[ 1 ] - start_o[ 1 ] );
	up_dist = ( up[ 0 ] - start_o[ 0 ] ) * ( up[ 0 ] - start_o[ 0 ] ) + ( up[ 1 ] - start_o[ 1 ] ) * ( up[ 1 ] - start_o[ 1 ] );

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < MIN_STEP_NORMAL ) {
		VectorCopy( down_o, pml.origin );
		VectorCopy( down_v, pml.velocity );
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (pm->s.pm_flags & PMF_ON_GROUND) {
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		pml.velocity[ 2 ] = down_v[ 2 ];
	}

	// Paril: step down stairs/slopes
	if ( ( pm->s.pm_flags & PMF_ON_GROUND ) && !( pm->s.pm_flags & PMF_ON_LADDER ) &&
		( pm->waterlevel < water_level_t::WATER_WAIST || ( !( pm->cmd.upmove/*pm->cmd.buttons & BUTTON_JUMP*/ ) && pml.velocity[ 2 ] <= 0 ) ) ) {
		VectorCopy( pml.origin, down );
		down[ 2 ] -= STEPSIZE;
		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );
		if ( trace.fraction < 1.f ) {
			VectorCopy( trace.endpos, pml.origin );
		}
	}
}

/**
*	@brief	Applies handling both ground friction and water friction
**/
static void PM_Friction( void ) {
	// Pointer to velocity for value adjustment.
	float *vel = pml.velocity;

	float speed = VectorLength( vel );
	if ( speed < 1 ) {
		vel[ 0 ] = 0;
		vel[ 1 ] = 0;
		return;
	}

	float drop = 0;
	float friction = 0;
	float control = 0;

	// Apply ground friction.
	if ( ( pm->groundentity && pml.groundsurface && !( pml.groundsurface->flags & SURF_SLICK ) ) || ( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		friction = pm_friction;
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;
	}

	// Apply water friction.
	if ( pm->waterlevel > water_level_t::WATER_NONE && !( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		drop += speed * pm_waterfriction * (float)pm->waterlevel * pml.frametime;
	}

	// Scale the velocity.
	float newspeed = speed - drop;
	if ( newspeed < 0 ) {
		newspeed = 0;
	}
	newspeed /= speed;

	// Apply friction based newspeed to velocity.
	vel[ 0 ] = vel[ 0 ] * newspeed;
	vel[ 1 ] = vel[ 1 ] * newspeed;
	vel[ 2 ] = vel[ 2 ] * newspeed;
}

/**
*	@brief	Handles user intended Ground acceleration
**/
static void PM_Accelerate( const vec3_t wishDirection, const float wishSpeed, const float acceleration ) {
	float currentSpeed = DotProduct( pml.velocity, wishDirection );
	float addSpeed = wishSpeed - currentSpeed;
	if ( addSpeed <= 0 ) {
		return;
	}
	float accelSpeed = acceleration * pml.frametime * wishSpeed;
	if ( accelSpeed > addSpeed ) {
		accelSpeed = addSpeed;
	}

	for ( int32_t i = 0; i < 3; i++ ) {
		pml.velocity[ i ] += accelSpeed * wishDirection[ i ];
	}
}

/**
*	@brief	Handles user intended Air acceleration
**/
static void PM_AirAccelerate( const vec3_t wishDirection, float wishSpeed, const float acceleration ) {
	// Upper clamp.
	if ( wishSpeed > 30 ) {
		wishSpeed = 30;
	}

	float currentspeed = DotProduct( pml.velocity, wishDirection );
	float addSpeed = wishSpeed - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	float acceleratedSpeed = acceleration * wishSpeed * pml.frametime;
	if ( acceleratedSpeed > addSpeed ) {
		acceleratedSpeed = addSpeed;
	}

	for ( int32_t i = 0; i < 3; i++ ) {
		pml.velocity[ i ] += acceleratedSpeed * wishDirection[ i ];
	}
}

/**
*	@brief	Account for surface 'currents', as well as ladders.
*/
static void PM_AddCurrents( vec3_t wishVelocity ) {
	vec3_t  v;
	float   s;

	/**
	*	Account for Ladder movement.
	**/
	if ( pm->s.pm_flags & PMF_ON_LADDER ) {
		// Check for Up(Jump) or Down(Crouch) actions:
		if ( pm->cmd.upmove > 0 || pm->cmd.upmove < 0 ) {//if ( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) {
			// Use full speed on ladders if we're underwater.
			const float ladderSpeed = ( pm->waterlevel >= water_level_t::WATER_WAIST ? pm_maxspeed : pm_ladderspeed );
		
			if ( pm->cmd.upmove > 0 ) {
				wishVelocity[ 2 ] = ladderSpeed;
			} else if ( pm->cmd.upmove < 0 ) {
				wishVelocity[ 2 ] = -ladderSpeed;
			}
		// Situation for where 'forwardmove' is in use.
		} else if ( pm->cmd.forwardmove ) {
			// Clamp the speed to prevent it from being 'too fast'.
			const float ladderSpeed = clamp( pm->cmd.forwardmove, -pm_ladderspeed, pm_ladderspeed );

			// Determine whether to move up/down based on pitch when pressing 'Forward'.
			if ( pm->cmd.forwardmove > 0 ) {
				// Determine by pitch whether to move up/down.
				if ( pm->viewangles[ PITCH ] < 15 ) {
					wishVelocity[ 2 ] = ladderSpeed;
				} else {
					wishVelocity[ 2 ] = -ladderSpeed;
				}
			// Allow the 'Back' button to let us climb down.
			} else if ( pm->cmd.forwardmove < 0 ) {
				// Remove x/y from the wishVelocity if we haven't touched ground yet, to prevent
				// sliding off the ladder.
				if ( !pm->groundentity ) {
					wishVelocity[ 0 ] = wishVelocity[ 1 ] = 0;
				}

				wishVelocity[ 2 ] = ladderSpeed;
			}
		// Otherwise, remain idle, unset Z velocity.
		} else {
			wishVelocity[ 2 ] = 0;
		}

		// Limit horizontal speed when on a ladder, unless we're still on the ground.
		if ( !pm->groundentity ) {
			// Allow left/right to move perpendicular to the ladder plane.
			if ( pm->cmd.sidemove ) {
				// Clamp the speed 'so it is not jarring':
				float ladderSpeed = clamp( pm->cmd.sidemove, -pm_ladder_sidemove_speed, pm_ladder_sidemove_speed );
				
				// Modulate speed based on depth of water level.
				if ( pm->waterlevel < water_level_t::WATER_WAIST ) {
					ladderSpeed *= pm_laddermod;
				}

				// Check for a ladder:
				vec3_t flatForward = { pml.forward[ 0 ], pml.forward[ 1 ], 0.f };
				VectorNormalize( flatForward );
				
				vec3_t spot = { 0.f, 0.f, 0.f };
				VectorAdd( pml.origin, flatForward, spot );//VectorMA( pml.origin, 1, flatForward, spot );
				trace_t ladderTrace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );

				// If we hit it, adjust our velocity.
				if ( ladderTrace.fraction != 1.f && ( ladderTrace.contents & CONTENTS_LADDER ) ) {
					vec3_t right = { 0.f, 0.f, 0.f };
					vec3_t v2 = { 0.f, 0.f, 1.f };
					CrossProduct( ladderTrace.plane.normal, v2, right );

					// Adjust wish velocity.
					wishVelocity[ 0 ] = wishVelocity[ 1 ] = 0;
					
					// Scale right
					vec3_t vLadderSpeed = { -ladderSpeed , -ladderSpeed , -ladderSpeed };
					VectorVectorScale( right, vLadderSpeed, right );
					VectorAdd( wishVelocity, right, wishVelocity );
				}
			// Otherwise, ensure the wish velocity is clamped properly.
			} else {
				clamp( wishVelocity[ 0 ], -25, 25 );
				clamp( wishVelocity[ 1 ], -25, 25 );
			}
		}
	}

	/**
	*	Add Water Currents.
	**/
	if ( pm->watertype & MASK_CURRENT ) {
		VectorClear( v );

		if ( pm->watertype & CONTENTS_CURRENT_0 ) {
			v[ 0 ] += 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_90 ) {
			v[ 1 ] += 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_180 ) {
			v[ 0 ] -= 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_270 ) {
			v[ 1 ] -= 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_UP ) {
			v[ 2 ] += 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_DOWN ) {
			v[ 2 ] -= 1;
		}

		s = pm_waterspeed;
		if ( ( pm->waterlevel == water_level_t::WATER_FEET ) && ( pm->groundentity ) ) {
			s /= 2;
		}

		VectorMA( wishVelocity, s, v, wishVelocity );
	}

	/**
	*	Add conveyor belt velocities (They change the velocities direction.)
	**/
	if ( pm->groundentity ) {
		VectorClear( v );

		if ( pml.groundcontents & CONTENTS_CURRENT_0 ) {
			v[ 0 ] += 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_90 ) {
			v[ 1 ] += 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_180 ) {
			v[ 0 ] -= 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_270 ) {
			v[ 1 ] -= 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_UP ) {
			v[ 2 ] += 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_DOWN ) {
			v[ 2 ] -= 1;
		}
		
		VectorMA( wishVelocity, 100 /* pm->groundentity->speed */, v, wishVelocity );
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
	if ( pm->s.pm_flags & PMF_JUMP_HELD ) {
		return;
	}

	if ( pm->s.pm_type == PM_DEAD ) {
		return;
	}

	if ( pm->waterlevel >= water_level_t::WATER_WAIST ) {
		// Swimming, not jumping
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
*	@return	True if we're heads above water, false otherwise.
**/
static inline bool PM_AboveWater() {
	const vec3_t below = { pml.origin[ 0 ], pml.origin[ 1 ], pml.origin[ 2 ] - 8.f };
	
	// Test if we got solid below.
	bool solid_below = pm->trace( pml.origin, pm->mins, pm->maxs, below, (const void *)pm->player, MASK_SOLID ).fraction < 1.0f;
	if ( solid_below ) {
		return false;
	}

	// No solid below us, is it water instead?
	bool water_below = pm->trace( pml.origin, pm->mins, pm->maxs, below, (const void*)pm->player, MASK_WATER ).fraction < 1.0f;
	if ( water_below ) {
		return true;
	}

	// A different type of solid was found, so we're definitely not dealing with water. (Thus, we're above it.)
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

	if ( !pm->groundentity ) {
		return;
	}

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
		// Circularly clamp the angles with deltas
		VectorAdd( pm->cmd.angles, pm->s.delta_angles, pm->viewangles );

		// Don't let the player look up or down more than 90 degrees.
		if ( pm->viewangles[ PITCH ] > 89 && pm->viewangles[ PITCH ] < 180 ) {
			pm->viewangles[ PITCH ] = 89;
		} else if ( pm->viewangles[ PITCH ] < 271 && pm->viewangles[ PITCH ] >= 180 ) {
			pm->viewangles[ PITCH ] = 271;
		}
	}

	// Calculate angle vectors based on view angles.
	AngleVectors( pm->viewangles, pml.forward, pml.right, pml.up );
}

/*
================
PM_ScreenEffects

================
*/
static void PM_ScreenEffects( ) {
	// add for contents
	vec3_t vieworg = {}; // pml.origin + pm->viewoffset + vec3_t{ 0, 0, (float)pm->s.viewheight };
	VectorAdd( pml.origin, pm->viewoffset, vieworg );
	vieworg[ 2 ] += pm->s.viewheight;

	int32_t contents = pm->pointcontents( vieworg );

	if ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) )
		pm->rdflags |= RDF_UNDERWATER;
	else
		pm->rdflags &= ~RDF_UNDERWATER;

//	if ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) )
//		G_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, pm->screen_blend );
//	else if ( contents & CONTENTS_SLIME )
//		G_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, pm->screen_blend );
//	else if ( contents & CONTENTS_WATER )
//		G_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, pm->screen_blend );
}

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
	Vector4Clear( pm->screen_blend );
	pm->rdflags = 0;
	pm->jump_sound = false;
	pm->step_clip = false;
	pm->impact_delta = false;

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
			// Different pitch handling.
			vec3_t  angles;
			VectorCopy( pm->viewangles, angles );
			if ( angles[ PITCH ] > 180 )
				angles[ PITCH ] = angles[ PITCH ] - 360;
			angles[ PITCH ] /= 3;

			AngleVectors( angles, pml.forward, pml.right, pml.up );
			
			// Regular 'air move'.
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
	PM_ScreenEffects();

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

