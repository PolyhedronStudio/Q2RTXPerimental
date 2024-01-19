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

#include "sg_shared.h"
#include "sg_pmove.h"
#include "sg_pmove_slidemove.h"

// TODO: FIX CLAMP BEING NAMED CLAMP... preventing std::clamp
#undef clamp


/**
*	@brief	Actual in-moment local move variables.
*
*			All of the locals will be zeroed before each pmove, just to make damn sure we don't have
*			any differences when running on client or server
**/
struct pml_t {
	//! Obvious origin and velocity.
	Vector3	origin = {};
	Vector3 velocity = {};

	//! Forward, right and up vectors.
	Vector3		forward = {}, right = {}, up = {};
	//! Move frametime.
	float		frametime = 0.f;

	//! Ground information.
	csurface_t *groundsurface = nullptr;
	//! Contents of ground brush.
	int32_t		groundcontents = 0;

	//! Used to reset ourselves in case we are truly stuck.
	Vector3		previous_origin = {};
	//! Used for calculating the fall impact with.
	Vector3		start_velocity = {};
};

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
void PM_StepSlideMove() {
	trace_t trace;
	Vector3 startOrigin = pml.origin;
	Vector3 startVelocity = pml.velocity;

	// Perform an actual 'Step Slide'.
	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frametime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

	Vector3 downOrigin = pml.origin;
	Vector3 downVelocity = pml.velocity;

	// Perform 'up-trace' to see whether we can step up at all.
	Vector3 up = startOrigin + Vector3{ 0.f, 0.f, PM_STEPSIZE };
	trace = PM_Trace( startOrigin, pm->mins, pm->maxs, up );
	if ( trace.allsolid ) {
		return; // can't step up
	}

	const float stepSize = trace.endpos[ 2 ] - startOrigin.z;

	// We can step up. Try sliding above.
	pml.origin = trace.endpos;
	pml.velocity = startVelocity;

	// Perform an actual 'Step Slide'.
	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frametime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

	// Push down the final amount.
	Vector3 down = pml.origin - Vector3{ 0.f, 0.f, stepSize };

	// [Paril-KEX] jitspoe suggestion for stair clip fix; store
	// the old down position, and pick a better spot for downwards
	// trace if the start origin's Z position is lower than the down end pt.
	Vector3 original_down = down;

	if ( startOrigin.z < down.z ) {
		down.z = startOrigin.z - 1.f;
	}

	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );
	if ( !trace.allsolid ) {
		// [Paril-KEX] from above, do the proper trace now
		trace_t real_trace = PM_Trace( pml.origin, pm->mins, pm->maxs, original_down );
		pml.origin = real_trace.endpos;

		// Only an upwards jump is a stair clip.
		if ( pml.velocity.z > 0.f ) {
			pm->step_clip = true;
		}
	}

	up = pml.origin;

	// Decide which one went farther, use 'Vector2Length', ignore the Z axis.
	const float down_dist = ( downOrigin.x - startOrigin.x ) * ( downOrigin.y - startOrigin.y ) + ( downOrigin.y - startOrigin.y ) * ( downOrigin.y - startOrigin.y );
	const float up_dist = ( up.x - startOrigin.x ) * ( up.x - startOrigin.x ) + ( up.y - startOrigin.y ) * ( up.y - startOrigin.y );

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < MIN_STEP_NORMAL ) {
		pml.origin = downOrigin;
		pml.velocity = downVelocity;
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (pm->s.pm_flags & PMF_ON_GROUND)
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		pml.velocity.z = downVelocity.z;
	}

	// Paril: step down stairs/slopes
	if ( ( pm->s.pm_flags & PMF_ON_GROUND ) && !( pm->s.pm_flags & PMF_ON_LADDER ) &&
		( pm->waterlevel < water_level_t::WATER_WAIST || ( !( pm->cmd.buttons & BUTTON_JUMP ) && pml.velocity.z <= 0 ) ) ) {
		Vector3 down = pml.origin - Vector3{ 0.f, 0.f, PM_STEPSIZE }; // { pml.origin[ 0 ], pml.origin[ 1 ], pml.origin[ 2 ] - PM_STEPSIZE };
		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );
		if ( trace.fraction < 1.f ) {
			pml.origin = trace.endpos;
		}
	}
}

/**
*	@brief	Handles both ground friction and water friction
**/
void PM_Friction() {
	
	const float speed = QM_Vector3Length( pml.velocity );//sqrtf( pml.velocity.x * pml.velocity.x + pml.velocity.y * pml.velocity.y + pml.velocity.z * pml.velocity.z );
	if ( speed < 1 ) {
		pml.velocity.x = 0;
		pml.velocity.y = 0;
		return;
	}


	// Apply ground friction.
	float drop = 0;
	if ( ( pm->groundentity && pml.groundsurface && !( pml.groundsurface->flags & SURF_SLICK ) ) || ( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		const float friction = pm_friction;
		const float control = ( speed < pm_stopspeed ? pm_stopspeed : speed );
		drop += control * friction * pml.frametime;
	}

	// Apply water friction.
	if ( pm->waterlevel && !( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		drop += speed * pm_waterfriction * (float)pm->waterlevel * pml.frametime;
	}

	// Scale the velocity.
	float newspeed = speed - drop;
	if ( newspeed < 0 ) {
		newspeed = 0;
	}
	newspeed /= speed;

	// Apply newspeed to gain our new velocity.
	pml.velocity *= newspeed;
}

/**
*	@brief	Handles user intended acceleration.
**/
void PM_Accelerate( const Vector3 &wishDirection, const float wishSpeed, const float acceleration ) {
	const float currentspeed = QM_Vector3DotProduct( pml.velocity, wishDirection );
	const float addSpeed = wishSpeed - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	float accelerationSpeed = acceleration * pml.frametime * wishSpeed;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	pml.velocity += accelerationSpeed * wishDirection;
}

/**
*	@brief	Handles user intended 'mid-air' acceleration.
**/
void PM_AirAccelerate( const Vector3 &wishDirection, const float wishSpeed, const float acceleration ) {
	float wishspd = wishSpeed;
	if ( wishspd > 30 ) {
		wishspd = 30;
	}

	const float currentspeed = QM_Vector3DotProduct( pml.velocity, wishDirection );//currentspeed = pml.velocity.dot( wishdir );
	const float addSpeed = wishspd - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	float accelerationSpeed = acceleration * wishSpeed * pml.frametime;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	pml.velocity += accelerationSpeed * wishDirection;
}

/**
*	@brief
**/
void PM_AddCurrents( Vector3 &wishVelocity ) {
	Vector3 velocity = QM_Vector3Zero();
	float speed = 0.f;

	// Account for ladders.
	if ( pm->s.pm_flags & PMF_ON_LADDER ) {
		if ( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) {
			// [Paril-KEX]: if we're underwater, use full speed on ladders
			const float ladder_speed = pm->waterlevel >= water_level_t::WATER_WAIST ? pm_maxspeed : 200;
			if ( pm->cmd.buttons & BUTTON_JUMP ) {
				wishVelocity.z = ladder_speed;
			} else if ( pm->cmd.buttons & BUTTON_CROUCH ) {
				wishVelocity.z = -ladder_speed;
			}
		} else if ( pm->cmd.forwardmove ) {
			// [Paril-KEX] clamp the speed a bit so we're not too fast
			const float ladder_speed = std::clamp( (float)pm->cmd.forwardmove, -200.f, 200.f );
			if ( pm->cmd.forwardmove > 0 ) {
				if ( pm->viewangles[ PITCH ] >= 271 && pm->viewangles[ PITCH ] < 345 ) {
					wishVelocity.z = ladder_speed;
				} else if ( pm->viewangles[ PITCH ] < 271 && pm->viewangles[ PITCH ] >= 15 ) {
					wishVelocity.z = -ladder_speed;
				}
			}
			// [Paril-KEX] allow using "back" arrow to go down on ladder
			else if ( pm->cmd.forwardmove < 0 ) {
				// if we haven't touched ground yet, remove x/y so we don't
				// slide off of the ladder
				if ( !pm->groundentity ) {
					wishVelocity.x = wishVelocity.y = 0;
				}

				wishVelocity.z = ladder_speed;
			}
		} else {
			wishVelocity.z = 0;
		}

		// limit horizontal speed when on a ladder
		// [Paril-KEX] unless we're on the ground
		if ( !pm->groundentity ) {
			// [Paril-KEX] instead of left/right not doing anything,
			// have them move you perpendicular to the ladder plane
			if ( pm->cmd.sidemove ) {
				// clamp side speed so it's not jarring...
				float ladder_speed = std::clamp( (float)pm->cmd.sidemove, -150.f, 150.f );

				if ( pm->waterlevel < water_level_t::WATER_WAIST ) {
					ladder_speed *= pm_laddermod;
				}

				// Check for ladder.
				Vector3 flatforward = QM_Vector3Normalize( {
					pml.forward.x,
					pml.forward.y,
					0.f 
				} );
				Vector3 spot = pml.origin + ( flatforward * 1 );
				trace_t trace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );

				if ( trace.fraction != 1.f && ( trace.contents & CONTENTS_LADDER ) ) {
					Vector3 right = QM_Vector3CrossProduct( trace.plane.normal, { 0.f, 0.f, 1.f } );
					wishVelocity.x = wishVelocity.y = 0;
					wishVelocity += ( right * -ladder_speed );
				}
			} else {
				if ( wishVelocity.x < -25 ) {
					wishVelocity.x = -25.f;
				} else if ( wishVelocity.x > 25 ) {
					wishVelocity.x = 25.f;
				}

				if ( wishVelocity.y < -25 ) {
					wishVelocity.y = -25.f;
				} else if ( wishVelocity.y > 25 ) {
					wishVelocity.y = 25.f;
				}
			}
		}
	}

	// Add water currents.
	if ( pm->watertype & MASK_CURRENT ) {
		velocity = QM_Vector3Zero();

		if ( pm->watertype & CONTENTS_CURRENT_0 ) {
			velocity.x += 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_90 ) {
			velocity.y += 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_180 ) {
			velocity.x -= 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_270 ) {
			velocity.y -= 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_UP ) {
			velocity.z += 1;
		}
		if ( pm->watertype & CONTENTS_CURRENT_DOWN ) {
			velocity.z -= 1;
		}

		speed = pm_waterspeed;
		if ( ( pm->waterlevel == water_level_t::WATER_FEET ) && ( pm->groundentity ) ) {
			speed /= 2;
		}

		wishVelocity += ( velocity * speed );
	}

	// Add conveyor belt velocities.
	if ( pm->groundentity ) {
		velocity = QM_Vector3Zero();

		if ( pml.groundcontents & CONTENTS_CURRENT_0 ) {
			velocity.x += 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_90 ) {
			velocity.y += 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_180 ) {
			velocity.x -= 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_270 ) {
			velocity.y -= 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_UP ) {
			velocity.z += 1;
		}
		if ( pml.groundcontents & CONTENTS_CURRENT_DOWN ) {
			velocity.z -= 1;
		}

		wishVelocity += velocity * 100.f;
	}
}


/**
*	@brief
**/
void PM_AirMove() {
	int	   i;
	Vector3 wishvel;
	float  wishspeed;
	float  maxspeed;

	const float fmove = pm->cmd.forwardmove;
	const float smove = pm->cmd.sidemove;

	//for ( i = 0; i < 2; i++ )
	//	wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;
	wishvel = pml.forward * fmove + pml.right * smove;
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

/**
*	@brief
**/
void PM_WaterMove() {
	int	   i;
	Vector3 wishvel;
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

/**
*	@brief
**/
inline void PM_GetWaterLevel( const Vector3 &position, water_level_t &level, int32_t &type ) {
	//
	// get waterlevel, accounting for ducking
	//
	level = water_level_t::WATER_NONE;
	type = CONTENTS_NONE;

	int32_t sample2 = (int)( pm->s.viewheight - pm->mins[ 2 ] );
	int32_t sample1 = sample2 / 2;

	vec3_t point = { position[ 0 ], position[ 1 ], position[ 2 ] };

	point[ 2 ] += pm->mins[ 2 ] + 1;

	int32_t cont = pm->pointcontents( point );

	if ( cont & MASK_WATER ) {
		type = cont;
		level = water_level_t::WATER_FEET;
		point[ 2 ] = pml.origin[ 2 ] + pm->mins[ 2 ] + sample1;
		cont = pm->pointcontents( point );
		if ( cont & MASK_WATER ) {
			level = water_level_t::WATER_WAIST;
			point[ 2 ] = pml.origin[ 2 ] + pm->mins[ 2 ] + sample2;
			cont = pm->pointcontents( point );
			if ( cont & MASK_WATER )
				level = water_level_t::WATER_UNDER;
		}
	}
}

/**
*	@brief
**/
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
				Vector3 clipped_velocity = QM_Vector3Zero();
				PM_ClipVelocity( pml.velocity, pm->groundplane.normal, clipped_velocity, 1.01f );

				pm->impact_delta = pml.start_velocity[ 2 ] - clipped_velocity[ 2 ];

				pm->s.pm_flags |= PMF_ON_GROUND;

				// WID: Commenting this allows to jump again after having done a jump + crouch.
				// really weird.
				if ( /*pm_config.n64_physics ||*/ ( pm->s.pm_flags & PMF_DUCKED ) ) {
					pm->s.pm_flags |= PMF_TIME_LAND;
					pm->s.pm_time = 128;
				}
			}
		}

		PM_RegisterTouchTrace( pm->touchTraces, trace );
	}

	//
	// get waterlevel, accounting for ducking
	//
	PM_GetWaterLevel( pml.origin, pm->waterlevel, pm->watertype );
}

/**
*	@brief
**/
void PM_CheckJump() {
	if ( pm->s.pm_flags & PMF_TIME_LAND ) { // hasn't been long enough since landing to jump again
		return;
	}

	if ( !( pm->cmd.buttons & BUTTON_JUMP ) ) { // not holding jump
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

	if ( pm->waterlevel >= water_level_t::WATER_WAIST ) { // swimming, not jumping
		pm->groundentity = nullptr;
		return;
	}

	if ( pm->groundentity == nullptr ) {
		return; // in air, so no effect
	}

	pm->s.pm_flags |= PMF_JUMP_HELD;
	pm->jump_sound = true;
	pm->groundentity = nullptr;
	pm->s.pm_flags &= ~PMF_ON_GROUND;

	float jump_height = 270.f;

	pml.velocity[ 2 ] += jump_height;
	if ( pml.velocity[ 2 ] < jump_height )
		pml.velocity[ 2 ] = jump_height;
}

/**
*	@brief
**/
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
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) && pm->waterlevel < water_level_t::WATER_WAIST ) {
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

	if ( pm->waterlevel != water_level_t::WATER_WAIST ) {
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
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] >= MIN_STEP_NORMAL ) {
		return;
	}

	// [Paril-KEX] improved waterjump
	//vec3_t waterjump_vel = flatforward * 50;
	//waterjump_vel.z = 350;
	Vector3 waterjump_vel = {
		flatforward[ 0 ] * 50,
		flatforward[ 1 ] * 50,
		350.f
	};
	// simulate what would happen if we jumped out here, and
	// if we land on a dry spot we're good!
	// simulate 1 sec worth of movement
	pm_touch_trace_list_t touches;
	Vector3 waterjump_origin = { pml.origin[ 0 ], pml.origin[ 1 ], pml.origin[ 2 ] };//vec3_t waterjump_origin = pml.origin;
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
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] < MIN_STEP_NORMAL || trace.endpos[ 2 ] < pml.origin[ 2 ] ) {
		return;
	}

	// we're currently standing on ground, and the snapped position
	// is a step
	if ( pm->groundentity && fabsf( pml.origin[ 2 ] - trace.endpos[2] ) <= PM_STEPSIZE ) {
		return;
	}

	water_level_t level;
	int32_t type;

	PM_GetWaterLevel( trace.endpos, level, type );

	// the water jump spot will be under water, so we're
	// probably hitting something weird that isn't important
	if ( level >= water_level_t::WATER_WAIST )
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

/**
*	@brief
**/
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

	pml.forward	= QM_Vector3Normalize( pml.forward );
	pml.right	= QM_Vector3Normalize( pml.right );

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

/**
*	@brief
**/
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

/**
*	@brief
**/
inline const bool PM_AboveWater() {
	//const vec3_t below = pml.origin - vec3_t{ 0, 0, 8 };
	const vec3_t below = {
		pml.origin[ 0 ],
		pml.origin[ 1 ],
		pml.origin[ 2 ] - 8.f
	};

	bool solid_below = pm->trace( QM_Vector3ToQFloatV( pml.origin ).v, QM_Vector3ToQFloatV( pm->mins ).v, QM_Vector3ToQFloatV( pm->maxs ).v, below, pm->player, MASK_SOLID ).fraction < 1.0f;
	if ( solid_below ) {
		return false;
	}

	bool water_below = pm->trace( QM_Vector3ToQFloatV( pml.origin ).v, QM_Vector3ToQFloatV( pm->mins ).v, QM_Vector3ToQFloatV( pm->maxs ).v, below, pm->player, MASK_WATER ).fraction < 1.0f;
	if ( water_below ) {
		return true;
	}

	return false;
}

/**
*	@brief 
**/
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
		( pm->groundentity || ( pm->waterlevel <= water_level_t::WATER_FEET && !PM_AboveWater() ) ) &&
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

	// Escape if nothing has changed.
	if ( !flags_changed ) {
		return false;
	}

	// Reset dimensions since our ducking state has changed.
	PM_SetDimensions();

	// We're ducked.
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
		pml.velocity = QM_Vector3Normalize( pml.velocity );
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
		pm->viewangles[ YAW ] = AngleMod( pm->cmd.angles[ YAW ] + pm->s.delta_angles[ YAW ] );
		pm->viewangles[ PITCH ] = 0;
		pm->viewangles[ ROLL ] = 0;
	} else {
		// circularly clamp the angles with deltas
		//pm->viewangles = pm->cmd.angles + pm->s.delta_angles;
		pm->viewangles[ 0 ] = AngleMod( pm->cmd.angles[ 0 ] + pm->s.delta_angles[ 0 ] );
		pm->viewangles[ 1 ] = AngleMod( pm->cmd.angles[ 1 ] + pm->s.delta_angles[ 1 ] );
		pm->viewangles[ 2 ] = AngleMod( pm->cmd.angles[ 2 ] + pm->s.delta_angles[ 2 ] );

		// don't let the player look up or down more than 90 degrees
		if ( pm->viewangles[ PITCH ] > 89 && pm->viewangles[ PITCH ] < 180 )
			pm->viewangles[ PITCH ] = 89;
		else if ( pm->viewangles[ PITCH ] < 271 && pm->viewangles[ PITCH ] >= 180 )
			pm->viewangles[ PITCH ] = 271;
	}
	#if 0
	// DEBUG:
	//SG_DPrintf( "pm->cmd.forwardmove=%f, viewangles[PITCH]=(%f), viewangles[YAW]=(%f), viewangles[ROLL]=(%f)\n", pm->cmd.forwardmove, pm->viewangles[ PITCH ], pm->viewangles[ YAW ], pm->viewangles[ ROLL ] );
	// EOF DEBUG:
	#endif
	QM_AngleVectors( pm->viewangles, &pml.forward, &pml.right, &pml.up );
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

	if ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) )
		SG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, pm->screen_blend );
	else if ( contents & CONTENTS_SLIME )
		SG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, pm->screen_blend );
	else if ( contents & CONTENTS_WATER )
		SG_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, pm->screen_blend );
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void SG_PlayerMove( pmove_t *pmove, pmoveParams_t *params ) {
	// Store pointers to the pmove object and the parameters supplied for this move.
	pm = pmove;
	pmp = params;

	// Clear out several member variables which require a fresh state before performing the move.
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
	pml.origin = pm->s.origin;
	pml.velocity = pm->s.velocity;
	// Save the start velocity.
	pml.start_velocity = pm->s.velocity;
	// Save the origin as 'old origin' for in case we get stuck.
	pml.previous_origin = pm->s.origin;

	// Calculate frametime.
	pml.frametime = pm->cmd.msec * 0.001f;

	// Clamp view angles.
	PM_ClampAngles( );

	// Performs fly move, only clips in case of spectator mode, noclips otherwise.
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

		// Get moving.
		PM_FlyMove( pm->s.pm_type == PM_SPECTATOR );
		// Snap to position.
		PM_SnapPosition( );
		return;
	}

	// Erase all input command state when dead, we don't want to allow moving our dead body.
	if ( pm->s.pm_type >= PM_DEAD ) {
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
		pm->cmd.buttons &= ~( BUTTON_JUMP | BUTTON_CROUCH );
	}

	if ( pm->s.pm_type == PM_FREEZE ) {
		return;     // no movement at all
	}

	// Set mins, maxs, and viewheight.
	PM_SetDimensions();

	// General position categorize.
	PM_CategorizePosition();

	// If pmove values were changed outside of the pmove code, resnap to position first before continuing.
	if ( pm->snapinitial ) {
		PM_InitialSnapPosition();
	}

	// Recategorize if we're ducked. ( Set groundentity, watertype, and waterlevel. )
	if ( PM_CheckDuck() ) {
		PM_CategorizePosition();
	}

	// When dead, perform dead movement.
	if ( pm->s.pm_type == PM_DEAD ) {
		PM_DeadMove();
	}

	// Currently performs the water jump.
	PM_CheckSpecialMovement();

	// Drop timing counter.
	if ( pm->s.pm_time ) {
		int32_t msec = pm->cmd.msec; //int msec = pm->cmd.msec >> 3;
		//if ( !msec ) {
		////	msec = 0; // msec = 1;
		//	msec = 1;
		//}

		if ( msec >= pm->s.pm_time ) {
			//if ( /*pm_config.n64_physics ||*/ ( pm->s.pm_flags & PMF_DUCKED ) ) {
			//	pm->s.pm_flags |= PMF_TIME_LAND;
			//	pm->s.pm_time = 128;
			//}
			
			// Somehow need this, Q2RE does not. If we don't do so, the code piece in this comment that resides above in PM_CategorizePosition
			// causes us to remain unable to jump.
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
		// Apply downwards gravity to the velocity.
		pml.velocity[ 2 ] -= pm->s.gravity * pml.frametime;

		// Cancel as soon as we are falling down again.
		if ( pml.velocity[ 2 ] < 0 ) { // cancel as soon as we are falling down again
			pm->s.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
			pm->s.pm_time = 0;
		}

		// Step slide move.
		PM_StepSlideMove();
	} else {
		// Check for jumping.
		PM_CheckJump( );
		// Apply friction. ( If on-ground. )
		PM_Friction( );

		// Determine water level and pursue to WaterMove if deep in above waist.
		if ( pm->waterlevel >= water_level_t::WATER_WAIST ) {
			PM_WaterMove( );
		} else {
			// Different pitch handling.
			Vector3 angles = pm->viewangles;
			if ( angles[ PITCH ] > 180 ) {
				angles[ PITCH ] = angles[ PITCH ] - 360;
			}
			angles[ PITCH ] /= 3;

			// Recalculate angle vectors.
			QM_AngleVectors( angles, &pml.forward, &pml.right, &pml.up );
			
			// Regular 'air move'. Uses air acceleration/regular acceleration based on on-ground.
			PM_AirMove( );
		}
	}

	// Final recategorization: Set groundentity, watertype, and waterlevel for final spot.
	PM_CategorizePosition();

	// Perform Trick Jump.
	if ( pm->s.pm_flags & PMF_TIME_TRICK_JUMP ) {
 		PM_CheckJump();
	}

	// Apply screen effects. (Predicted partially if compiled for client-side.)
	PM_ScreenEffects();

	// Snap us back into a validated position.
	PM_SnapPosition();

	// Temporary Vector3 test.
	Vector3 _origin = { pm->s.origin[ 0 ], pm->s.origin[ 1 ], pm->s.origin[ 2 ] };
	Vector3 _offset = { 0, 0, 24 };
	Vector3 _sum = _origin;
	_sum += _offset; 
	_sum -= _offset;
//	SG_DPrintf( "_sum(%f, %f, %f)\n", _sum.x, _sum.y, _sum.z );
//	if ( _offset != Vector3( 0.f, 0.f, 0.f ) ) {
//		SG_DPrintf( "wooowww\n" );
//	}
	Vector3 traceStart = pm->s.origin;
	Vector3 traceEnd = traceStart + Vector3{ 0.f, 0.f, -20.f };
	trace_t tresult = PM_Trace( QM_Vector3ToQFloatV( traceStart ).v, pm->mins, pm->maxs, QM_Vector3ToQFloatV( traceEnd ).v, 0 );

	SG_DPrintf( "tresult.ent=(%f)\n", tresult.fraction );
	// EOF Temporary Vector3 test.
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

