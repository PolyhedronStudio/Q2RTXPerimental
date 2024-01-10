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

// TODO: FIX CLAMP BEING NAMED CLAMP... preventing std::clamp
#undef clamp

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
/*
==================
PM_StepSlideMove

==================
*/
void PM_StepSlideMove() {
	vec3_t	start_o, start_v;
	vec3_t	down_o, down_v;
	trace_t trace;
	float	down_dist, up_dist;
	//	vec3_t		delta;

	VectorCopy( pml.origin, start_o );//start_o = pml.origin;
	VectorCopy( pml.velocity, start_v );//start_v = pml.velocity;

	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frametime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

	VectorCopy( pml.origin, down_o );
	VectorCopy( pml.velocity, down_v );

	//up = start_o;
	//up[ 2 ] += STEPSIZE;
	vec3_t up = { start_o[ 0 ], start_o[ 1 ], start_o[ 2 ] + STEPSIZE };

	trace = PM_Trace( start_o, pm->mins, pm->maxs, up );
	if ( trace.allsolid ) {
		return; // can't step up
	}

	float stepSize = trace.endpos[ 2 ] - start_o[ 2 ];

	// try sliding above
	VectorCopy( trace.endpos, pml.origin );//pml.origin = trace.endpos;
	VectorCopy( start_v, pml.velocity ); // pml.velocity = start_v;

	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frametime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

	// push down the final amount
	//down = pml.origin;
	//down[ 2 ] -= stepSize;
	vec3_t down = { pml.origin[ 0 ], pml.origin[ 1 ], pml.origin[ 2 ] - stepSize };

	// [Paril-KEX] jitspoe suggestion for stair clip fix; store
	// the old down position, and pick a better spot for downwards
	// trace if the start origin's Z position is lower than the down end pt.
	vec3_t original_down = { down[ 0 ], down[ 1 ], down[ 2 ] };//vec3_t original_down = down;

	if ( start_o[ 2 ] < down[ 2 ] ) {
		down[ 2 ] = start_o[ 2 ] - 1.f;
	}

	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );
	if ( !trace.allsolid ) {
		// [Paril-KEX] from above, do the proper trace now
		trace_t real_trace = PM_Trace( pml.origin, pm->mins, pm->maxs, original_down );
		VectorCopy( real_trace.endpos, pml.origin );//pml.origin = real_trace.endpos;

		// only an upwards jump is a stair clip
		if ( pml.velocity[ 2 ] > 0.f ) {
			pm->step_clip = true;
		}
	}

	VectorCopy( pml.origin, up );  //up = pml.origin;

	// decide which one went farther
	down_dist = ( down_o[ 0 ] - start_o[ 0 ] ) * ( down_o[ 0 ] - start_o[ 0 ] ) + ( down_o[ 1 ] - start_o[ 1 ] ) * ( down_o[ 1 ] - start_o[ 1 ] );
	up_dist = ( up[ 0 ] - start_o[ 0 ] ) * ( up[ 0 ] - start_o[ 0 ] ) + ( up[ 1 ] - start_o[ 1 ] ) * ( up[ 1 ] - start_o[ 1 ] );

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < MIN_STEP_NORMAL ) {
		VectorCopy( down_o, pml.origin ); // pml.origin = down_o;
		VectorCopy( down_v, pml.velocity ); //pml.velocity = down_v;
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (pm->s.pm_flags & PMF_ON_GROUND)
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		pml.velocity[ 2 ] = down_v[ 2 ];
	}

	// Paril: step down stairs/slopes
	if ( ( pm->s.pm_flags & PMF_ON_GROUND ) && !( pm->s.pm_flags & PMF_ON_LADDER ) &&
		( pm->waterlevel < WATER_WAIST || ( !( pm->cmd.buttons & BUTTON_JUMP ) && pml.velocity[ 2 ] <= 0 ) ) ) {
		vec3_t down = { pml.origin[ 0 ], pml.origin[ 1 ], pml.origin[ 2 ] - STEPSIZE };
		//down[ 2 ] -= STEPSIZE;
		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );
		if ( trace.fraction < 1.f ) {
			VectorCopy( trace.endpos, pml.origin ); // pml.origin = trace.endpos;
		}
	}
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
void PM_Friction() {
	float *vel;
	float  speed, newspeed, control;
	float  friction;
	float  drop;

	vel = pml.velocity;

	speed = sqrt( vel[ 0 ] * vel[ 0 ] + vel[ 1 ] * vel[ 1 ] + vel[ 2 ] * vel[ 2 ] );
	if ( speed < 1 ) {
		vel[ 0 ] = 0;
		vel[ 1 ] = 0;
		return;
	}

	drop = 0;

	// apply ground friction
	if ( ( pm->groundentity && pml.groundsurface && !( pml.groundsurface->flags & SURF_SLICK ) ) || ( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		friction = pm_friction;
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;
	}

	// apply water friction
	if ( pm->waterlevel && !( pm->s.pm_flags & PMF_ON_LADDER ) )
		drop += speed * pm_waterfriction * (float)pm->waterlevel * pml.frametime;

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
void PM_Accelerate( const vec3_t &wishdir, float wishspeed, float accel ) {
	int	  i;
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct( pml.velocity, wishdir ); //currentspeed = pml.velocity.dot( wishdir );
	addspeed = wishspeed - currentspeed;
	if ( addspeed <= 0 ) {
		return;
	}
	accelspeed = accel * pml.frametime * wishspeed;
	if ( accelspeed > addspeed ) {
		accelspeed = addspeed;
	}

	for ( i = 0; i < 3; i++ ) {
		pml.velocity[ i ] += accelspeed * wishdir[ i ];
	}
}

void PM_AirAccelerate( const vec3_t &wishdir, float wishspeed, float accel ) {
	int	  i;
	float addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if ( wishspd > 30 ) {
		wishspd = 30;
	}
	currentspeed = DotProduct( pml.velocity, wishdir );//currentspeed = pml.velocity.dot( wishdir );
	addspeed = wishspd - currentspeed;
	if ( addspeed <= 0 ) {
		return;
	}
	accelspeed = accel * wishspeed * pml.frametime;
	if ( accelspeed > addspeed ) {
		accelspeed = addspeed;
	}

	for ( i = 0; i < 3; i++ ) {
		pml.velocity[ i ] += accelspeed * wishdir[ i ];
	}
}

/*
=============
PM_AddCurrents
=============
*/
void PM_AddCurrents( vec3_t &wishvel ) {
	vec3_t v;
	float  s;

	//
	// account for ladders
	//

	if ( pm->s.pm_flags & PMF_ON_LADDER ) {
		if ( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) {
			// [Paril-KEX]: if we're underwater, use full speed on ladders
			float ladder_speed = pm->waterlevel >= WATER_WAIST ? pm_maxspeed : 200;

			if ( pm->cmd.buttons & BUTTON_JUMP ) {
				wishvel[ 2 ] = ladder_speed;
			} else if ( pm->cmd.buttons & BUTTON_CROUCH ) {
				wishvel[ 2 ] = -ladder_speed;
			}
		} else if ( pm->cmd.forwardmove ) {
			// [Paril-KEX] clamp the speed a bit so we're not too fast
			float ladder_speed = std::clamp( (float)pm->cmd.forwardmove, -200.f, 200.f );

			if ( pm->cmd.forwardmove > 0 ) {
				if ( pm->viewangles[ PITCH ] < 15 ) {
					wishvel[ 2 ] = ladder_speed;
				} else {
					wishvel[ 2 ] = -ladder_speed;
				}
			}
			// [Paril-KEX] allow using "back" arrow to go down on ladder
			else if ( pm->cmd.forwardmove < 0 ) {
				// if we haven't touched ground yet, remove x/y so we don't
				// slide off of the ladder
				if ( !pm->groundentity ) {
					wishvel[ 0 ] = wishvel[ 1 ] = 0;
				}

				wishvel[ 2 ] = ladder_speed;
			}
		} else {
			wishvel[ 2 ] = 0;
		}

		// limit horizontal speed when on a ladder
		// [Paril-KEX] unless we're on the ground
		if ( !pm->groundentity ) {
			// [Paril-KEX] instead of left/right not doing anything,
			// have them move you perpendicular to the ladder plane
			if ( pm->cmd.sidemove ) {
				// clamp side speed so it's not jarring...
				float ladder_speed = std::clamp( (float)pm->cmd.sidemove, -150.f, 150.f );

				if ( pm->waterlevel < WATER_WAIST )
					ladder_speed *= pm_laddermod;

				// check for ladder
				vec3_t flatforward = { 
					pml.forward[ 0 ],
					pml.forward[ 1 ],
					0.f 
				};
				VectorNormalize( flatforward );
				//flatforward.normalize();

				//spot = pml.origin + ( flatforward * 1 );
				vec3_t spot = { 
					pml.origin[ 0 ] + ( flatforward[ 0 ] * 1 ),
					pml.origin[ 1 ] + ( flatforward[ 1 ] * 1 ),
					pml.origin[ 2 ] + ( flatforward[ 2 ] * 1 ) 
				};
				trace_t trace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );

				if ( trace.fraction != 1.f && ( trace.contents & CONTENTS_LADDER ) ) {
					vec3_t crossV2 = { 0.f, 0.f, 1.f };
					vec3_t right;//vec3_t right = trace.plane.normal.cross( { 0, 0, 1 } );
					CrossProduct( trace.plane.normal, crossV2, right );
					wishvel[ 0 ] = wishvel[ 1 ] = 0;

					//wishvel += ( right * -ladder_speed );
					wishvel[ 0 ] += ( right[ 0 ] * -ladder_speed );
					wishvel[ 1 ] += ( right[ 1 ] * -ladder_speed );
					wishvel[ 2 ] += ( right[ 2 ] * -ladder_speed );
				}
			} else {
				if ( wishvel[ 0 ] < -25 ) {
					wishvel[ 0 ] = -25;
				} else if ( wishvel[ 0 ] > 25 ) {
					wishvel[ 0 ] = 25;
				}

				if ( wishvel[ 1 ] < -25 ) {
					wishvel[ 1 ] = -25;
				} else if ( wishvel[ 1 ] > 25 ) {
					wishvel[ 1 ] = 25;
				}
			}
		}
	}

	//
	// add water currents
	//

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
		if ( ( pm->waterlevel == WATER_FEET ) && ( pm->groundentity ) ) {
			s /= 2;
		}

		//wishvel += ( v * s );
		wishvel[ 0 ] += ( v[ 0 ] * s );
		wishvel[ 1 ] += ( v[ 1 ] * s );
		wishvel[ 2 ] += ( v[ 2 ] * s );
	}

	//
	// add conveyor belt velocities
	//

	if ( pm->groundentity ) {
		VectorClear( v ); //v = {};

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

		//wishvel += v * 100;
		wishvel[ 0 ] += ( v[ 0 ] * 100.f );
		wishvel[ 1 ] += ( v[ 1 ] * 100.f );
		wishvel[ 2 ] += ( v[ 2 ] * 100.f );
	}
}

/*
===================
PM_WaterMove

===================
*/
void PM_WaterMove() {
	int	   i;
	vec3_t wishvel;
	float  wishspeed;
	vec3_t wishdir;

	//
	// user intentions
	//
	for ( i = 0; i < 3; i++ )
		wishvel[ i ] = pml.forward[ i ] * pm->cmd.forwardmove + pml.right[ i ] * pm->cmd.sidemove;

	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove &&
		!( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) ) {
		if ( !pm->groundentity ) {
			wishvel[ 2 ] -= 60; // drift towards bottom
		}
	} else {
		if ( pm->cmd.buttons & BUTTON_CROUCH ) {
			wishvel[ 2 ] -= pm_waterspeed * 0.5f;
		} else if ( pm->cmd.buttons & BUTTON_JUMP ) {
			wishvel[ 2 ] += pm_waterspeed * 0.5f;
		}
	}

	PM_AddCurrents( wishvel );

	//vec3_t wishdir = { wishvel[ 0 ], wishvel[ 1 ], wishvel[ 2 ] }; // wishdir = wishvel
	wishdir[ 0 ] = wishvel[ 0 ];
	wishdir[ 1 ] = wishvel[ 1 ];
	wishdir[ 2 ] = wishvel[ 2 ];
	wishspeed = VectorNormalize( wishdir ); // wishspeed = wishdir.normalize();

	if ( wishspeed > pm_maxspeed ) {
		//wishvel *= pm_maxspeed / wishspeed;
		wishvel[ 0 ] *= pm_maxspeed / wishspeed;
		wishvel[ 1 ] *= pm_maxspeed / wishspeed;
		wishvel[ 2 ] *= pm_maxspeed / wishspeed;
		wishspeed = pm_maxspeed;
	}
	wishspeed *= 0.5f;

	if ( ( pm->s.pm_flags & PMF_DUCKED ) && wishspeed > pm_duckspeed ) {
		//wishvel *= pm_duckspeed / wishspeed;
		wishvel[ 0 ] *= pm_duckspeed / wishspeed;
		wishvel[ 1 ] *= pm_duckspeed / wishspeed;
		wishvel[ 2 ] *= pm_duckspeed / wishspeed;
		wishspeed = pm_duckspeed;
	}

	PM_Accelerate( wishdir, wishspeed, pm_wateraccelerate );

	PM_StepSlideMove();
}

/*
===================
PM_AirMove

===================
*/
void PM_AirMove() {
	int	   i;
	vec3_t wishvel;
	float  wishspeed;
	float  maxspeed;

	float fmove = pm->cmd.forwardmove;
	float smove = pm->cmd.sidemove;

	for ( i = 0; i < 2; i++ )
		wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;
	wishvel[ 2 ] = 0;

	PM_AddCurrents( wishvel );

	vec3_t wishdir = { wishvel[ 0 ], wishvel[ 1 ], wishvel[ 2 ] }; // wishdir = wishvel
	wishspeed = VectorNormalize( wishdir ); // wishspeed = wishdir.normalize();

	//
	// clamp to server defined max speed
	//
	maxspeed = ( pm->s.pm_flags & PMF_DUCKED ) ? pm_duckspeed : pm_maxspeed;

	if ( wishspeed > maxspeed ) {
		//wishvel *= maxspeed / wishspeed;
		wishvel[ 0 ] *= maxspeed / wishspeed;
		wishvel[ 1 ] *= maxspeed / wishspeed;
		wishvel[ 2 ] *= maxspeed / wishspeed;
		wishspeed = maxspeed;
	}

	if ( pm->s.pm_flags & PMF_ON_LADDER ) {
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );
		if ( !wishvel[ 2 ] ) {
			if ( pml.velocity[ 2 ] > 0 ) {
				pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;
				if ( pml.velocity[ 2 ] < 0 ) {
					pml.velocity[ 2 ] = 0;
				}
			} else {
				pml.velocity[ 2 ] += pm->s.gravity * pml.frametime;
				if ( pml.velocity[ 2 ] > 0 ) {
					pml.velocity[ 2 ] = 0;
				}
			}
		}
		PM_StepSlideMove();
	} else if ( pm->groundentity ) {						 // walking on ground
		pml.velocity[ 2 ] = 0; //!!! this is before the accel
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

		// PGM	-- fix for negative trigger_gravity fields
		//		pml.velocity[2] = 0;
		if ( pm->s.gravity > 0 ) {
			pml.velocity[ 2 ] = 0;
		} else {
			pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;
		}
		// PGM

		if ( !pml.velocity[ 0 ] && !pml.velocity[ 1 ] ) {
			return;
		}
		PM_StepSlideMove();
	} else { // not on ground, so little effect on velocity
		if ( pmp->airaccelerate ) {//if ( pm_config.airaccel )
			PM_AirAccelerate( wishdir, wishspeed, pmp->airaccelerate );
		} else {
			PM_Accelerate( wishdir, wishspeed, 1 );
		}

		// add gravity
		if ( pm->s.pm_type != PM_GRAPPLE ) {
			pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;
		}

		PM_StepSlideMove();
	}
}

inline void PM_GetWaterLevel( const vec3_t &position, water_level_t &level, int32_t &type ) {
	//
	// get waterlevel, accounting for ducking
	//
	level = WATER_NONE;
	type = CONTENTS_NONE;

	int32_t sample2 = (int)( pm->s.viewheight - pm->mins[ 2 ] );
	int32_t sample1 = sample2 / 2;

	vec3_t point = { position[ 0 ], position[ 1 ], position[ 2 ] };

	point[ 2 ] += pm->mins[ 2 ] + 1;

	int32_t cont = pm->pointcontents( point );

	if ( cont & MASK_WATER ) {
		type = cont;
		level = WATER_FEET;
		point[ 2 ] = pml.origin[ 2 ] + pm->mins[ 2 ] + sample1;
		cont = pm->pointcontents( point );
		if ( cont & MASK_WATER ) {
			level = WATER_WAIST;
			point[ 2 ] = pml.origin[ 2 ] + pm->mins[ 2 ] + sample2;
			cont = pm->pointcontents( point );
			if ( cont & MASK_WATER )
				level = WATER_UNDER;
		}
	}
}

/*
=============
PM_CatagorizePosition
=============
*/
void PM_CategorizePosition() {
	vec3_t	   point;
	trace_t	   trace;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid
	point[ 0 ] = pml.origin[ 0 ];
	point[ 1 ] = pml.origin[ 1 ];
	point[ 2 ] = pml.origin[ 2 ] - 0.25f;

	if ( pml.velocity[ 2 ] > 180 || pm->s.pm_type == PM_GRAPPLE ) //!!ZOID changed from 100 to 180 (ramp accel)
	{
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = nullptr;
	} else {
		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, point );
		pm->groundplane = trace.plane;
		pml.groundsurface = trace.surface;
		pml.groundcontents = trace.contents;

		// [Paril-KEX] to attempt to fix edge cases where you get stuck
		// wedged between a slope and a wall (which is irrecoverable
		// most of the time), we'll allow the player to "stand" on
		// slopes if they are right up against a wall
		bool slanted_ground = trace.fraction < 1.0f && trace.plane.normal[ 2 ] < MIN_STEP_NORMAL;

		if ( slanted_ground ) {
			vec3_t slantTraceEnd = {
				pml.origin[ 0 ] + trace.plane.normal[ 0 ],
				pml.origin[ 1 ] + trace.plane.normal[ 1 ],
				pml.origin[ 2 ] + trace.plane.normal[ 2 ]
			};
			trace_t slant = PM_Trace( pml.origin, pm->mins, pm->maxs, slantTraceEnd );

			if ( slant.fraction < 1.0f && !slant.startsolid ) {
				slanted_ground = false;
			}
		}

		if ( trace.fraction == 1.0f || ( slanted_ground && !trace.startsolid ) ) {
			pm->groundentity = nullptr;
			pm->s.pm_flags &= ~PMF_ON_GROUND;
		} else {
			pm->groundentity = trace.ent;

			// hitting solid ground will end a waterjump
			if ( pm->s.pm_flags & PMF_TIME_WATERJUMP ) {
				pm->s.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
				pm->s.pm_time = 0;
			}

			if ( !( pm->s.pm_flags & PMF_ON_GROUND ) ) {
				// just hit the ground

				// [Paril-KEX]
				if ( /*!pm_config.n64_physics &&*/ pml.velocity[ 2 ] >= 100.f && pm->groundplane.normal[ 2 ] >= 0.9f && !( pm->s.pm_flags & PMF_DUCKED ) ) {
					pm->s.pm_flags |= PMF_TIME_TRICK_JUMP;
					pm->s.pm_time = 64;
				}

				// [Paril-KEX] calculate impact delta; this also fixes triple jumping
				vec3_t clipped_velocity;
				PM_ClipVelocity( pml.velocity, pm->groundplane.normal, clipped_velocity, 1.01f );

				pm->impact_delta = pml.start_velocity[ 2 ] - clipped_velocity[ 2 ];

				pm->s.pm_flags |= PMF_ON_GROUND;

				// WID: Commenting this allows to jump again after having done a jump + crouch.
				// really weird.
				//if ( /*pm_config.n64_physics ||*/ ( pm->s.pm_flags & PMF_DUCKED ) ) {
				//	pm->s.pm_flags |= PMF_TIME_LAND;
				//	pm->s.pm_time = 128;
				//}
			}
		}

		PM_RegisterTouchTrace( pm->touchTraces, trace );
	}

	//
	// get waterlevel, accounting for ducking
	//
	PM_GetWaterLevel( pml.origin, pm->waterlevel, pm->watertype );
}

/*
=============
PM_CheckJump
=============
*/
void PM_CheckJump() {
	if ( pm->s.pm_flags & PMF_TIME_LAND ) { // hasn't been long enough since landing to jump again
		return;
	}

	if ( !( pm->cmd.buttons & BUTTON_JUMP ) ) { // not holding jump
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// must wait for jump to be released
	if ( pm->s.pm_flags & PMF_JUMP_HELD )
		return;

	if ( pm->s.pm_type == PM_DEAD )
		return;

	if ( pm->waterlevel >= WATER_WAIST ) { // swimming, not jumping
		pm->groundentity = nullptr;
		return;
	}

	if ( pm->groundentity == nullptr )
		return; // in air, so no effect

	pm->s.pm_flags |= PMF_JUMP_HELD;
	pm->jump_sound = true;
	pm->groundentity = nullptr;
	pm->s.pm_flags &= ~PMF_ON_GROUND;

	float jump_height = 270.f;

	pml.velocity[ 2 ] += jump_height;
	if ( pml.velocity[ 2 ] < jump_height )
		pml.velocity[ 2 ] = jump_height;
}

/*
=============
PM_CheckSpecialMovement
=============
*/
void PM_CheckSpecialMovement() {
	trace_t trace;

	if ( pm->s.pm_time ) {
		return;
	}

	pm->s.pm_flags &= ~PMF_ON_LADDER;

	// check for ladder
	//flatforward[ 0 ] = pml.forward[ 0 ];
	//flatforward[ 1 ] = pml.forward[ 1 ];
	//flatforward[ 2 ] = 0;
	//flatforward.normalize();
	vec3_t flatforward = {
		pml.forward[ 0 ],
		pml.forward[ 1 ],
		0.f
	};
	VectorNormalize( flatforward );

	//spot = pml.origin + ( flatforward * 1 );
	vec3_t spot = {
		pml.origin[ 0 ] + ( flatforward[ 0 ] * 1 ),
		pml.origin[ 1 ] + ( flatforward[ 1 ] * 1 ),
		pml.origin[ 2 ] + ( flatforward[ 2 ] * 1 )
	};
	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) && pm->waterlevel < WATER_WAIST ) {
		pm->s.pm_flags |= PMF_ON_LADDER;
	}

	if ( !pm->s.gravity ) {
		return;
	}

	// check for water jump
	// [Paril-KEX] don't try waterjump if we're moving against where we'll hop
	if ( !( pm->cmd.buttons & BUTTON_JUMP )
		&& pm->cmd.forwardmove <= 0 ) {
		return;
	}

	if ( pm->waterlevel != WATER_WAIST ) {
		return;
	}
	// [Paril-KEX]
	//else if ( pm->watertype & CONTENTS_NO_WATERJUMP ) {
	//	return;
	//}

	// quick check that something is even blocking us forward
	vec3_t blockTraceEnd = {
		pml.origin[ 0 ] + ( flatforward[ 0 ] * 40 ),
		pml.origin[ 1 ] + ( flatforward[ 1 ] * 40 ),
		pml.origin[ 2 ] + ( flatforward[ 2 ] * 40 )
	};
	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, blockTraceEnd, MASK_SOLID );

	// we aren't blocked, or what we're blocked by is something we can walk up
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] >= MIN_STEP_NORMAL )
		return;

	// [Paril-KEX] improved waterjump
	//vec3_t waterjump_vel = flatforward * 50;
	//waterjump_vel.z = 350;
	vec3_t waterjump_vel = {
		flatforward[ 0 ] * 50,
		flatforward[ 1 ] * 50,
		350.f
	};
	// simulate what would happen if we jumped out here, and
	// if we land on a dry spot we're good!
	// simulate 1 sec worth of movement
	pm_touch_trace_list_t touches;
	vec3_t waterjump_origin = { pml.origin[ 0 ], pml.origin[ 1 ], pml.origin[ 2 ] };//vec3_t waterjump_origin = pml.origin;
	float time = 0.1f;
	bool has_time = true;

	for ( size_t i = 0; i < min( 50, (int32_t)( 10 * ( 800.f / pm->s.gravity ) ) ); i++ ) {
		waterjump_vel[ 2 ] -= pm->s.gravity * time;

		if ( waterjump_vel[ 2 ] < 0 ) {
			has_time = false;
		}

		PM_StepSlideMove_Generic( waterjump_origin, waterjump_vel, time, pm->mins, pm->maxs, touches, has_time );
	}

	// snap down to ground
	vec3_t snapTraceEnd = { waterjump_origin[ 0 ], waterjump_origin[ 1 ], waterjump_origin[ 2 ] - 2.f };
	trace = PM_Trace( waterjump_origin, pm->mins, pm->maxs, snapTraceEnd, MASK_SOLID );
	//trace = PM_Trace( waterjump_origin, pm->mins, pm->maxs, waterjump_origin - vec3_t{ 0, 0, 2.f }, MASK_SOLID );

	// can't stand here
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] < MIN_STEP_NORMAL ||
		trace.endpos[ 2 ]< pml.origin[ 2 ] )
		return;

	// we're currently standing on ground, and the snapped position
	// is a step
	if ( pm->groundentity && fabsf( pml.origin[ 2 ] - trace.endpos[2] ) <= STEPSIZE ) {
		return;
	}

	water_level_t level;
	int32_t type;

	PM_GetWaterLevel( trace.endpos, level, type );

	// the water jump spot will be under water, so we're
	// probably hitting something weird that isn't important
	if ( level >= WATER_WAIST )
		return;

	// valid waterjump!
	// jump out of water
	//pml.velocity = flatforward * 50;
	pml.velocity[ 0 ] = flatforward[ 0 ] * 50;
	pml.velocity[ 1 ] = flatforward[ 1 ] * 50;
	pml.velocity[ 2 ] = 350;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 2048;
}

/*
===============
PM_FlyMove
===============
*/
void PM_FlyMove( bool doclip ) {
	float	speed, drop, friction, control, newspeed;
	float	currentspeed, addspeed, accelspeed;
	int		i;
	vec3_t	wishvel;
	float	fmove, smove;
	vec3_t	wishdir;
	float	wishspeed;

	pm->s.viewheight = doclip ? 0 : 22;

	// friction

	speed = VectorLength( pml.velocity );//speed = pml.velocity.length();
	if ( speed < 1 ) {
		VectorClear( pml.velocity ); //pml.velocity = vec3_origin;
	} else {
		drop = 0;

		friction = pm_friction * 1.5f; // extra friction
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if ( newspeed < 0 )
			newspeed = 0;
		newspeed /= speed;

		//pml.velocity *= newspeed;
		pml.velocity[ 0 ] *= newspeed;
		pml.velocity[ 1 ] *= newspeed;
		pml.velocity[ 2 ] *= newspeed;
	}

	// accelerate
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	VectorNormalize( pml.forward );//pml.forward.normalize();
	VectorNormalize( pml.right );//pml.right.normalize();

	for ( i = 0; i < 3; i++ ) {
		wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;
	}

	if ( pm->cmd.buttons & BUTTON_JUMP ) {
		wishvel[ 2 ] += ( pm_waterspeed * 0.5f );
	}
	if ( pm->cmd.buttons & BUTTON_CROUCH ) {
		wishvel[ 2 ] -= ( pm_waterspeed * 0.5f );
	}

	wishdir[ 0 ] = wishvel[ 0 ]; // wishdir = wishvel
	wishdir[ 1 ] = wishvel[ 1 ];
	wishdir[ 2 ] = wishvel[ 2 ];
	wishspeed = VectorNormalize( wishdir ); // wishspeed = wishdir.normalize();

	//
	// clamp to server defined max speed
	//
	if ( wishspeed > pm_maxspeed ) {
		//wishvel *= pm_maxspeed / wishspeed;
		wishvel[ 0 ] *= pm_maxspeed / wishspeed;
		wishvel[ 1 ] *= pm_maxspeed / wishspeed;
		wishvel[ 2 ] *= pm_maxspeed / wishspeed;
		wishspeed = pm_maxspeed;
	}

	// Paril: newer clients do this
	wishspeed *= 2;

	currentspeed = DotProduct( pml.velocity, wishdir );//currentspeed = pml.velocity.dot( wishdir );
	addspeed = wishspeed - currentspeed;

	if ( addspeed > 0 ) {
		accelspeed = pm_accelerate * pml.frametime * wishspeed;
		if ( accelspeed > addspeed )
			accelspeed = addspeed;

		for ( i = 0; i < 3; i++ )
			pml.velocity[ i ] += accelspeed * wishdir[ i ];
	}

	if ( doclip ) {
		/*for (i = 0; i < 3; i++)
			end[i] = pml.origin[i] + pml.frametime * pml.velocity[i];

		trace = PM_Trace(pml.origin, pm->mins, pm->maxs, end);

		pml.origin = trace.endpos;*/

		PM_StepSlideMove();
	} else {
		// move
		//pml.origin += ( pml.velocity * pml.frametime );
		VectorMA( pml.origin, pml.frametime, pml.velocity, pml.origin );
	}
}

void PM_SetDimensions() {
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
	} else {
		pm->maxs[ 2 ] = 32;
		pm->s.viewheight = 22;
	}
}

inline bool PM_AboveWater() {
	//const vec3_t below = pml.origin - vec3_t{ 0, 0, 8 };
	const vec3_t below = {
		pml.origin[ 0 ],
		pml.origin[ 1 ],
		pml.origin[ 2 ] - 8
	};

	bool solid_below = pm->trace( pml.origin, pm->mins, pm->maxs, below, pm->player, MASK_SOLID ).fraction < 1.0f;
	if ( solid_below ) {
		return false;
	}

	bool water_below = pm->trace( pml.origin, pm->mins, pm->maxs, below, pm->player, MASK_WATER ).fraction < 1.0f;
	if ( water_below ) {
		return true;
	}

	return false;
}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->viewheight
==============
*/
bool PM_CheckDuck() {
	if ( pm->s.pm_type == PM_GIB ) {
		return false;
	}

	trace_t trace;
	bool flags_changed = false;

	if ( pm->s.pm_type == PM_DEAD ) {
		if ( !( pm->s.pm_flags & PMF_DUCKED ) ) {
			pm->s.pm_flags |= PMF_DUCKED;
			flags_changed = true;
		}
	} else if (
		( pm->cmd.buttons & BUTTON_CROUCH ) &&
		( pm->groundentity || ( pm->waterlevel <= WATER_FEET && !PM_AboveWater() ) ) &&
		!( pm->s.pm_flags & PMF_ON_LADDER ) /*&&
		!pm_config.n64_physics*/ ) { // duck
		if ( !( pm->s.pm_flags & PMF_DUCKED ) ) {
			// check that duck won't be blocked
			vec3_t check_maxs = { pm->maxs[ 0 ], pm->maxs[ 1 ], 4 };
			trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin );
			if ( !trace.allsolid ) {
				pm->s.pm_flags |= PMF_DUCKED;
				flags_changed = true;
			}
		}
	} else { // stand up if possible
		if ( pm->s.pm_flags & PMF_DUCKED ) {
			// try to stand up
			vec3_t check_maxs = { pm->maxs[ 0 ], pm->maxs[ 1 ], 32 };
			trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin );
			if ( !trace.allsolid ) {
				pm->s.pm_flags &= ~PMF_DUCKED;
				flags_changed = true;
			}
		}
	}

	if ( !flags_changed )
		return false;

	PM_SetDimensions();
	return true;
}

/*
==============
PM_DeadMove
==============
*/
void PM_DeadMove() {
	float forward;

	if ( !pm->groundentity ) {
		return;
	}

	// extra friction

	//forward = pml.velocity.length();
	//forward -= 20;
	//if ( forward <= 0 ) {
	//	pml.velocity = {};
	//} else {
	//	pml.velocity.normalize();
	//	pml.velocity *= forward;
	//}
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

bool PM_GoodPosition() {
	if ( pm->s.pm_type == PM_NOCLIP ) {
		return true;
	}

	trace_t trace = PM_Trace( pm->s.origin, pm->mins, pm->maxs, pm->s.origin );

	return !trace.allsolid;
}

/*
================
PM_SnapPosition

On exit, the origin will have a value that is pre-quantized to the PMove
precision of the network channel and in a valid position.
================
*/
static void PM_SnapPosition() {
	VectorCopy( pml.velocity, pm->s.velocity ); //pm->s.velocity = pml.velocity;
	VectorCopy( pml.origin, pm->s.origin );// pm->s.origin = pml.origin;

	if ( PM_GoodPosition() ) {
		return;
	}

	//if ( G_FixStuckObject_Generic( pm->s.origin, pm->mins, pm->maxs  ) == stuck_result_t::NO_GOOD_POSITION ) {
	//	pm->s.origin = pml.previous_origin;
	//	return;
	//}
}

/*
================
PM_InitialSnapPosition

================
*/
void PM_InitialSnapPosition() {
	int					   x, y, z;
	vec3_t				base;
	constexpr int		   offset[ 3 ] = { 0, -1, 1 };

	//base = pm->s.origin;
	VectorCopy( pm->s.origin, base );

	for ( z = 0; z < 3; z++ ) {
		pm->s.origin[ 2 ] = base[ 2 ] + offset[ z ];
		for ( y = 0; y < 3; y++ ) {
			pm->s.origin[ 1 ] = base[ 1 ] + offset[ y ];
			for ( x = 0; x < 3; x++ ) {
				pm->s.origin[ 0 ] = base[ 0 ] + offset[ x ];
				if ( PM_GoodPosition() ) {
					VectorCopy( pm->s.origin, pml.origin ); // pml.origin = pm->s.origin;
					VectorCopy( pm->s.origin, pml.previous_origin ); // pml.origin = pm->s.origin;//pml.previous_origin = pm->s.origin;
					return;
				}
			}
		}
	}
}

/*
================
PM_ClampAngles

================
*/
static void PM_ClampAngles() {
	if ( pm->s.pm_flags & PMF_TIME_TELEPORT ) {
		pm->viewangles[ YAW ] = pm->cmd.angles[ YAW ] + pm->s.delta_angles[ YAW ];
		pm->viewangles[ PITCH ] = 0;
		pm->viewangles[ ROLL ] = 0;
	} else {
		// circularly clamp the angles with deltas
		//pm->viewangles = pm->cmd.angles + pm->s.delta_angles;
		pm->viewangles[ 0 ] = pm->cmd.angles[ 0 ] + pm->s.delta_angles[ 0 ];
		pm->viewangles[ 1 ] = pm->cmd.angles[ 1 ] + pm->s.delta_angles[ 1 ];
		pm->viewangles[ 2 ] = pm->cmd.angles[ 2 ] + pm->s.delta_angles[ 2 ];

		// don't let the player look up or down more than 90 degrees
		if ( pm->viewangles[ PITCH ] > 89 && pm->viewangles[ PITCH ] < 180 )
			pm->viewangles[ PITCH ] = 89;
		else if ( pm->viewangles[ PITCH ] < 271 && pm->viewangles[ PITCH ] >= 180 )
			pm->viewangles[ PITCH ] = 271;
	}
	AngleVectors( pm->viewangles, pml.forward, pml.right, pml.up );
}

// [Paril-KEX]
static void PM_ScreenEffects() {
	// add for contents
	//vec3_t vieworg = pml.origin + pm->viewoffset + vec3_t{ 0, 0, (float)pm->s.viewheight };
	vec3_t vieworg = {
		pml.origin[ 0 ] + pm->viewoffset[ 0 ],
		pml.origin[ 1 ] + pm->viewoffset[ 1 ],
		pml.origin[ 2 ] + pm->viewoffset[ 2 ] + (float)pm->s.viewheight
	};
	int32_t contents = pm->pointcontents( vieworg );//contents_t contents = pm->pointcontents( vieworg );

	if ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) )
		pm->rdflags |= RDF_UNDERWATER;
	else
		pm->rdflags &= ~RDF_UNDERWATER;

	//if ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) )
	//	G_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, pm->screen_blend );
	//else if ( contents & CONTENTS_SLIME )
	//	G_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, pm->screen_blend );
	//else if ( contents & CONTENTS_WATER )
	//	G_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, pm->screen_blend );
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
	VectorClear( pm->viewangles );
	pm->s.viewheight = 0;
	pm->groundentity = nullptr;
	pm->watertype = CONTENTS_NONE;
	pm->waterlevel = water_level_t::WATER_NONE;
	Vector4Clear( pm->screen_blend );
	pm->rdflags = 0;
	pm->jump_sound = false;
	pm->step_clip = false;
	pm->impact_delta = 0;

	// Clear all pmove local vars
	pml = {};

	// Store the origin and velocity in pmove local.
	VectorCopy( pm->s.origin, pml.origin );
	VectorCopy( pm->s.velocity, pml.velocity );
	// Save the start velocity.
	VectorCopy( pm->s.velocity, pml.start_velocity );
	// Save the origin as 'old origin' for in case we get stuck.
	VectorCopy( pm->s.origin, pml.previous_origin );

	// Calculate frametime.
	pml.frametime = pm->cmd.msec * 0.001f;

	// Clamp view angles.
	PM_ClampAngles( );

	if ( pm->s.pm_type == PM_SPECTATOR || pm->s.pm_type == PM_NOCLIP ) {
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
		pm->cmd.buttons &= ~( BUTTON_JUMP | BUTTON_CROUCH );
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
		//if ( !msec ) {
		//	msec = 0; // msec = 1;
		//}

		if ( msec >= pm->s.pm_time ) {
			//pm->s.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
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
	pmp->airaccelerate = 8; // TODO: Has to use the ConfigString, determined by sv_airaccelerate.
	pmp->speedmult = 2;
	pmp->watermult = 0.5f;
	pmp->maxspeed = 320;
	pmp->friction = 6;
	pmp->waterfriction = 1;
	pmp->flyfriction = 4;
}

