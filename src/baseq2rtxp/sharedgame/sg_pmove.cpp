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

//! Defining this will keep the player's viewangles always >= 0 && < 360.
//! Also affects the pitch angle for ladders, since it is then determined differently.
//#define PM_CLAMP_VIEWANGLES_0_TO_360

//! Defining this will enable 'material physics' such as per example: Taking the friction value from the ground material.
#define PMOVE_USE_MATERIAL_FRICTION

/**
*	@brief	Actual in-moment local move variables.
*
*			All of the locals will be zeroed before each pmove, just to make damn sure we don't have
*			any differences when running on client or server
**/
struct pml_t {
	//! Actual in-move origin and velocity.
	Vector3	origin = {};
	Vector3 velocity = {};

	//! Forward, right and up vectors.
	Vector3	forward = {}, right = {}, up = {};

	//! Move frameTime.
	float	frameTime = 0.f;

	struct pml_ground_info_s {
		//! Pointer to the actual ground entity we are on. (nullptr if none).
		//struct edict_s *entity;

		//! A copy of the plane data from the ground entity.
		//cplane_t	plane;
		//! A pointer to the ground plane's surface data. (nullptr if none).
		csurface_t	*surface;
		//! A copy of the contents data from the ground entity's brush.
		contents_t	contents;
		////! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
		//cm_material_t *material;
	} ground;

	//! Used to reset ourselves in case we are truly stuck.
	Vector3		previousOrigin = {};
	//! Used for calculating the fall impact with.
	Vector3		startVelocity = {};
};



//! An actual pointer to the pmove object that we're moving.
pmove_t *pm;
//! An actual pointer to the pmove parameters object for use with moving.
static pmoveParams_t *pmp;
//! Contains our local in-moment move variables.
static pml_t pml;



typedef struct default_pmoveParams_s {
	//! Stop speed.
	static constexpr float pm_stop_speed = 100.f;
	//! Server determined maximum speed.
	static constexpr float pm_max_speed = 300.f;
	//! Velocity that is set for jumping. (Determines the height we aim for.)
	static constexpr float pm_jump_height = 270.f;

	//! General up/down movespeed for on a ladder.
	static constexpr float pm_ladder_speed = 200.f;
	//! Maximum 'strafe' side move speed while on a ladder.
	static constexpr float pm_ladder_sidemove_speed = 150.f;
	//! Ladder modulation scalar for when being in-water and climbing a ladder.
	static constexpr float pm_ladder_mod = 0.5f;

	//! Speed for when ducked and crawling on-ground.
	static constexpr float pm_duck_speed = 100.f;
	//! Speed for when moving in water(swimming).
	static constexpr float pm_water_speed = 400.f;
	//! Speed for when flying.
	static constexpr float pm_fly_speed = 400.f;

	//! General acceleration.
	static constexpr float pm_accelerate = 10.f;
	//! General water acceleration.
	static constexpr float pm_water_accelerate = 10.f;

	//! General friction.
	static constexpr float pm_friction = 6.f;
	//! General water friction.
	static constexpr float pm_water_friction = 1.f;
} default_pmoveParams_t;

/**
*	@brief	Updates the player move ground info based on the trace results.
**/
static void PM_UpdateGroundFromTrace( const trace_t *trace ) {
	if ( trace == nullptr || trace->ent == nullptr ) {
		pm->ground.entity = nullptr;
		pm->ground.plane = {};
		pm->ground.surface = {};
		pm->ground.contents = CONTENTS_NONE;
		pm->ground.material = nullptr;
	} else {
		pm->ground.entity = trace->ent;
		pm->ground.plane = trace->plane;
		pm->ground.surface = *trace->surface;
		pm->ground.contents = trace->contents;
		pm->ground.material = trace->material;
	}
}
/**
*
*
*	Step Slide Move:
*
*
**/
/**
*	@return True if the trace yielded a step, false otherwise.
**/
static bool PM_CheckStep( const trace_t *trace ) {
	// If not solid:
	if ( !trace->allsolid ) {
		// If trace clipped to an entity and the plane we hit its normal is sane for stepping:
		if ( trace->ent && trace->plane.normal[2] >= MIN_STEP_NORMAL) {
			// We just traversed a step of sorts.
			return true;
		}
	}

	// It wasn't a step, return false.
	return false;
}

/**
*	@brief	Will step to the trace its end position, calculating the height difference and
*			setting it as our step_height if it is equal or above the minimal step size.
**/
static void PM_StepDown( const trace_t *trace ) {
	// Apply the trace endpos as the new origin.
	pml.origin = trace->endpos;

	// Determine the step height based on the new, and previous origin.
	const float step_height = pml.origin.z - pml.previousOrigin.z;

	// If its absolute(-/+) value >= PM_MIN_STEP_SIZE(4.0) then we got an official step.
	if ( fabsf( step_height ) >= PM_MIN_STEP_SIZE ) {
		pm->step_height = step_height;
	}
}

/**
*	@brief	Each intersection will try to step over the obstruction instead of
*			sliding along it.
* 
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
static void PM_StepSlideMove() {
	trace_t trace;
	Vector3 startOrigin = pml.origin;
	Vector3 startVelocity = pml.velocity;

	// Perform an actual 'Step Slide'.
	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frameTime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

	Vector3 downOrigin = pml.origin;
	Vector3 downVelocity = pml.velocity;

	// Perform 'up-trace' to see whether we can step up at all.
	Vector3 up = startOrigin + Vector3{ 0.f, 0.f, PM_MAX_STEP_SIZE };
	trace = PM_Trace( startOrigin, pm->mins, pm->maxs, up );
	if ( trace.allsolid ) {
		return; // can't step up
	}

	// Determine step size to test with.
	const float stepSize = trace.endpos[ 2 ] - startOrigin.z;

	// We can step up. Try sliding above.
	pml.origin = trace.endpos;
	pml.velocity = startVelocity;

	// Perform an actual 'Step Slide'.
	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frameTime, pm->mins, pm->maxs, pm->touchTraces, pm->s.pm_time );

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
		//pml.origin = real_trace.endpos;

		// WID: Use proper stair step checking.
		if ( PM_CheckStep( &trace ) ) {
			// Only an upwards jump is a stair clip.
			if ( pml.velocity.z > 0.f ) {
				pm->step_clip = true;
			}
			// Step down to the new found ground.
			PM_StepDown( &real_trace );
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
		Vector3 down = pml.origin - Vector3{ 0.f, 0.f, PM_MAX_STEP_SIZE };
		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, down );

		// WID: Use proper stair step checking.
		// Check for stairs:
		if ( PM_CheckStep( &trace ) ) {
			// Step down stairs:
			PM_StepDown( &trace );
		// We're expecting it to be a slope, step down the slope instead:
		} else if ( trace.fraction < 1.f ) {
			pml.origin = trace.endpos;
		}
	}
}

/**
*	@brief	Handles both ground friction and water friction
**/
static void PM_Friction() {
	// Set us to a halt, if our speed is too low, otherwise we'll see
	// ourselves sort of 'drifting'.
	const float speed = QM_Vector3Length( pml.velocity );//sqrtf( pml.velocity.x * pml.velocity.x + pml.velocity.y * pml.velocity.y + pml.velocity.z * pml.velocity.z );
	if ( speed < 1 ) {
		pml.velocity.x = 0;
		pml.velocity.y = 0;
		return;
	}

	// Apply ground friction if on-ground.
#ifdef PMOVE_USE_MATERIAL_FRICTION
	float drop = 0;
	if ( ( pm->ground.entity != nullptr && pml.ground.surface != nullptr && !( pml.ground.surface->flags & SURF_SLICK ) ) || ( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		// Get the material to fetch friction from.
		cm_material_t *ground_material = ( pml.ground.surface != nullptr ? pml.ground.surface->material : nullptr );
		float friction = ( ground_material ? ground_material->physical.friction : pmp->pm_friction );
		const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		drop += control * friction * pml.frameTime;
	}
#else
	// Apply ground friction if on-ground.
	float drop = 0;
	if ( ( pm->ground.entity && pml.ground.surface && !( pml.ground.surface->flags & SURF_SLICK ) ) || ( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		const float friction = pmp->pm_friction;
		const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		drop += control * friction * pml.frameTime;
	}
#endif
	// Apply water friction, and not off-ground yet on a ladder.
	if ( pm->waterlevel && !( pm->s.pm_flags & PMF_ON_LADDER ) ) {
		drop += speed * pmp->pm_water_friction * (float)pm->waterlevel * pml.frameTime;
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
static void PM_Accelerate( const Vector3 &wishDirection, const float wishSpeed, const float acceleration ) {
	const float currentspeed = QM_Vector3DotProduct( pml.velocity, wishDirection );
	const float addSpeed = wishSpeed - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	float accelerationSpeed = acceleration * pml.frameTime * wishSpeed;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	pml.velocity = QM_Vector3MultiplyAdd( wishDirection, accelerationSpeed, pml.velocity );
}

/**
*	@brief	Handles user intended 'mid-air' acceleration.
**/
static void PM_AirAccelerate( const Vector3 &wishDirection, const float wishSpeed, const float acceleration ) {
	float wishspd = wishSpeed;
	if ( wishspd > 30 ) {
		wishspd = 30;
	}

	const float currentspeed = QM_Vector3DotProduct( pml.velocity, wishDirection );
	const float addSpeed = wishspd - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	float accelerationSpeed = acceleration * wishSpeed * pml.frameTime;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	pml.velocity = QM_Vector3MultiplyAdd( wishDirection, accelerationSpeed, pml.velocity );
}

/**
*	@brief	Handles the velocities for 'ladders', as well as water and conveyor belt by applying their 'currents'.
**/
static void PM_AddCurrents( Vector3 &wishVelocity ) {
	// Account for ladders.
	if ( pm->s.pm_flags & PMF_ON_LADDER ) {
		if ( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) {
			// [Paril-KEX]: if we're underwater, use full speed on ladders
			const float ladder_speed = pm->waterlevel >= water_level_t::WATER_WAIST ? pmp->pm_max_speed : pmp->pm_ladder_speed;
			if ( pm->cmd.buttons & BUTTON_JUMP ) {
				wishVelocity.z = ladder_speed;
			} else if ( pm->cmd.buttons & BUTTON_CROUCH ) {
				wishVelocity.z = -ladder_speed;
			}
		} else if ( pm->cmd.forwardmove ) {
			// [Paril-KEX] clamp the speed a bit so we're not too fast
			const float ladder_speed = std::clamp( (float)pm->cmd.forwardmove, -pmp->pm_ladder_speed, pmp->pm_ladder_speed );
			if ( pm->cmd.forwardmove > 0 ) {
				#ifdef PM_CLAMP_VIEWANGLES_0_TO_360
				if ( pm->viewangles[ PITCH ] >= 271 && pm->viewangles[ PITCH ] < 345 ) {
					wishVelocity.z = ladder_speed;
				} else if ( pm->viewangles[ PITCH ] < 271 && pm->viewangles[ PITCH ] >= 15 ) {
					wishVelocity.z = -ladder_speed;
				}
				#else
				if ( pm->viewangles[ PITCH ] < 15 ) {
					wishVelocity.z = ladder_speed;
				} else if ( pm->viewangles[ PITCH ] < 271 && pm->viewangles[ PITCH ] >= 15 ) {
					wishVelocity.z = -ladder_speed;
				}
				#endif
			}
			// [Paril-KEX] allow using "back" arrow to go down on ladder
			else if ( pm->cmd.forwardmove < 0 ) {
				// if we haven't touched ground yet, remove x/y so we don't
				// slide off of the ladder
				if ( !pm->ground.entity ) {
					wishVelocity.x = wishVelocity.y = 0;
				}

				wishVelocity.z = ladder_speed;
			}
		} else {
			wishVelocity.z = 0;
		}

		// limit horizontal speed when on a ladder
		// [Paril-KEX] unless we're on the ground
		if ( !pm->ground.entity ) {
			// [Paril-KEX] instead of left/right not doing anything,
			// have them move you perpendicular to the ladder plane
			if ( pm->cmd.sidemove ) {
				// clamp side speed so it's not jarring...
				float ladder_speed = std::clamp( (float)pm->cmd.sidemove, -pmp->pm_ladder_sidemove_speed, pmp->pm_ladder_sidemove_speed );

				if ( pm->waterlevel < water_level_t::WATER_WAIST ) {
					ladder_speed *= pmp->pm_ladder_mod;
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
					wishVelocity = QM_Vector3MultiplyAdd( right, -ladder_speed, wishVelocity );
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
		Vector3 velocity = QM_Vector3Zero();

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

		float speed = pmp->pm_water_speed;
		// Walking in water, against a current, so slow down our 
		if ( ( pm->waterlevel == water_level_t::WATER_FEET ) && ( pm->ground.entity ) ) {
			speed /= 2;
		}

		wishVelocity = QM_Vector3MultiplyAdd( velocity, speed, wishVelocity );
	}

	// Add conveyor belt velocities.
	if ( pm->ground.entity ) {
		Vector3 velocity = QM_Vector3Zero();

		if ( pml.ground.contents & CONTENTS_CURRENT_0 ) {
			velocity.x += 1;
		}
		if ( pml.ground.contents & CONTENTS_CURRENT_90 ) {
			velocity.y += 1;
		}
		if ( pml.ground.contents & CONTENTS_CURRENT_180 ) {
			velocity.x -= 1;
		}
		if ( pml.ground.contents & CONTENTS_CURRENT_270 ) {
			velocity.y -= 1;
		}
		if ( pml.ground.contents & CONTENTS_CURRENT_UP ) {
			velocity.z += 1;
		}
		if ( pml.ground.contents & CONTENTS_CURRENT_DOWN ) {
			velocity.z -= 1;
		}

		wishVelocity = QM_Vector3MultiplyAdd( velocity, 100.f, wishVelocity );
	}
}


/**
*	@brief
**/
static void PM_AirMove() {
	const float forwardMove = pm->cmd.forwardmove;
	const float sidewardMove = pm->cmd.sidemove;

	// Calculate wish velocity.
	Vector3 wishVelocity = pml.forward * forwardMove + pml.right * sidewardMove;
	wishVelocity.z = 0.f;

	// Add velocity from ladder/currents if needed.
	PM_AddCurrents( wishVelocity );

	// Determine wish direction and speed.
	Vector3 wishDirection = wishVelocity;
	float wishSpeed = QM_Vector3NormalizeLength( wishDirection ); // wishspeed = wishdir.normalize();
	
	// Clamp speeds to to server defined max speed.
	const float maxSpeed = ( pm->s.pm_flags & PMF_DUCKED ) ? pmp->pm_duck_speed : pmp->pm_max_speed;

	if ( wishSpeed > maxSpeed ) {
		wishVelocity *= maxSpeed / wishSpeed;
		wishSpeed = maxSpeed;
	}

	// Perform ladder movement.
	if ( pm->s.pm_flags & PMF_ON_LADDER ) {
		PM_Accelerate( wishDirection, wishSpeed, pmp->pm_accelerate );
		if ( !wishVelocity.z ) {
			// Apply gravity as a form of 'friction' to prevent 'racing/sliding' against the ladder.
			if ( pml.velocity.z > 0 ) {
				pml.velocity.z -= pm->s.gravity * pml.frameTime;
				if ( pml.velocity.z < 0 ) {
					pml.velocity.z = 0;
				}
			} else {
				pml.velocity.z += pm->s.gravity * pml.frameTime;
				if ( pml.velocity.z > 0 ) {
					pml.velocity.z = 0;
				}
			}
		}
		PM_StepSlideMove();
	// Perform ground movement. (Walking on-ground):
	} else if ( pm->ground.entity ) {
		// Erase Z before accelerating.
		pml.velocity.z = 0; //!!! this is before the accel
		PM_Accelerate( wishDirection, wishSpeed, pmp->pm_accelerate );

		// PGM	-- fix for negative trigger_gravity fields
		//		pml.velocity[2] = 0;
		if ( pm->s.gravity > 0 ) {
			pml.velocity.z = 0;
		} else {
			pml.velocity.z -= pm->s.gravity * pml.frameTime;
		}
		// PGM

		// Escape and prevent step slide moving if we have no x/y velocity.
		if ( !pml.velocity.x && !pml.velocity.y ) {
			return;
		}

		// Step Slide.
		PM_StepSlideMove();
	// Not on ground, so litte effect on velocity. Perform air acceleration in case it is enabled(has any value stored.):
	} else {
		// Accelrate appropriately.
		if ( pmp->pm_air_accelerate ) {
			PM_AirAccelerate( wishDirection, wishSpeed, pmp->pm_air_accelerate );
		} else {
			PM_Accelerate( wishDirection, wishSpeed, 1 );
		}

		// Add gravity in case we're not in grappling mode.
		if ( pm->s.pm_type != PM_GRAPPLE ) {
			pml.velocity.z -= pm->s.gravity * pml.frameTime;
		}

		// Step Slide.
		PM_StepSlideMove();
	}
}

/**
*	@brief
**/
static void PM_WaterMove() {
	//
	// user intentions
	//
	Vector3 wishVelocity = pml.forward * pm->cmd.forwardmove + pml.right * pm->cmd.sidemove;

	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove && !( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) ) {
		if ( !pm->ground.entity ) {
			wishVelocity.z -= 60; // drift towards bottom
		}
	} else {
		if ( pm->cmd.buttons & BUTTON_CROUCH ) {
			wishVelocity.z -= pmp->pm_water_speed * 0.5f;
		} else if ( pm->cmd.buttons & BUTTON_JUMP ) {
			wishVelocity.z += pmp->pm_water_speed * 0.5f;
		}
	}

	PM_AddCurrents( wishVelocity );

	Vector3 wishDirection = wishVelocity;
	float wishspeed = QM_Vector3NormalizeLength( wishDirection ); // wishspeed = wishDirection.normalize();
	if ( wishspeed > pmp->pm_max_speed ) {
		wishVelocity *= pmp->pm_max_speed / wishspeed;
		wishspeed = pmp->pm_max_speed;
	}
	wishspeed *= 0.5f;

	// Adjust speed to if/being ducked.
	if ( ( pm->s.pm_flags & PMF_DUCKED ) && wishspeed > pmp->pm_duck_speed ) {
		wishVelocity *= pmp->pm_duck_speed / wishspeed;
		wishspeed = pmp->pm_duck_speed;
	}

	// Accelerate through water.
	PM_Accelerate( wishDirection, wishspeed, pmp->pm_water_accelerate );

	// Step Slide.
	PM_StepSlideMove();
}

/**
*	@brief
**/
static inline void PM_GetWaterLevel( const Vector3 &position, water_level_t &level, contents_t &type ) {
	//
	// get waterlevel, accounting for ducking
	//
	level = water_level_t::WATER_NONE;
	type = CONTENTS_NONE;

	int32_t sample2 = (int)( pm->s.viewheight - pm->mins.z );
	int32_t sample1 = sample2 / 2;

	Vector3 point = position;

	point.z += pm->mins.z + 1;

	contents_t contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );

	if ( contentType & MASK_WATER ) {
		type = contentType;
		level = water_level_t::WATER_FEET;
		point.z = pml.origin.z + pm->mins.z + sample1;
		contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );
		if ( contentType & MASK_WATER ) {
			level = water_level_t::WATER_WAIST;
			point.z = pml.origin.z + pm->mins.z + sample2;
			contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );
			if ( contentType & MASK_WATER ) {
				level = water_level_t::WATER_UNDER;
			}
		}
	}
}

/**
*	@brief
**/
static void PM_CategorizePosition() {
	trace_t	   trace;

	// If the player hull point is 0.25 units down is solid, the player is on ground.
	// See if standing on something solid
	Vector3 point = pml.origin + Vector3{ 0.f, 0.f, -0.25f };

	if ( pml.velocity.z > 180 || pm->s.pm_type == PM_GRAPPLE ) { //!!ZOID changed from 100 to 180 (ramp accel)
		// We are going off-ground due to a too high velocit.
		pm->s.pm_flags &= ~PMF_ON_GROUND;

		// Ensure the player move ground data is updated accordingly.
		pm->ground.entity = nullptr;
		pm->ground.plane = {};
		pm->ground.surface = {};
		pm->ground.contents = CONTENTS_NONE;
		pm->ground.material = nullptr;
	} else {
		trace = PM_Trace( pml.origin, pm->mins, pm->maxs, point );
		pm->ground.plane = trace.plane;
		//pml.groundSurface = trace.surface;
		pm->ground.surface = *( pml.ground.surface = trace.surface );
		//pml.groundContents = trace.contents;
		pm->ground.contents = (contents_t)( pml.ground.contents = trace.contents );

		// [Paril-KEX] to attempt to fix edge cases where you get stuck
		// wedged between a slope and a wall (which is irrecoverable
		// most of the time), we'll allow the player to "stand" on
		// slopes if they are right up against a wall
		bool slanted_ground = trace.fraction < 1.0f && trace.plane.normal[ 2 ] < MIN_STEP_NORMAL;

		if ( slanted_ground ) {
			Vector3 slantTraceEnd = pml.origin + trace.plane.normal;
			trace_t slant = PM_Trace( pml.origin, pm->mins, pm->maxs, slantTraceEnd );

			if ( slant.fraction < 1.0f && !slant.startsolid ) {
				slanted_ground = false;
			}
		}

		if ( trace.fraction == 1.0f || ( slanted_ground && !trace.startsolid ) ) {
			//if ( trace.fraction == 1.0f ) {
			//	pm->groundplane = { };
			//} else {
			//	pm->groundplane = trace.plane;
			//}
			pml.ground.surface = nullptr;
			pml.ground.contents = CONTENTS_NONE;

			PM_UpdateGroundFromTrace( nullptr );
			//pm->ground.entity = nullptr;
			//pm->ground.plane = {};
			//pm->ground.surface = {}; 
			//pm->ground.contents = CONTENTS_NONE;
			//pm->ground.material = nullptr;

			pm->s.pm_flags &= ~PMF_ON_GROUND;
		} else {
			PM_UpdateGroundFromTrace( &trace );
			//pm->ground.entity = trace.ent;
			//pm->ground.plane = trace.plane;
			//pm->ground.surface = *trace.surface;
			//pm->ground.contents = trace.contents;
			//pm->ground.material = trace.material;

			// hitting solid ground will end a waterjump
			if ( pm->s.pm_flags & PMF_TIME_WATERJUMP ) {
				pm->s.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
				pm->s.pm_time = 0;
			}

			if ( !( pm->s.pm_flags & PMF_ON_GROUND ) ) {
				// just hit the ground

				// [Paril-KEX]
				if ( /*!pm_config.n64_physics &&*/ pml.velocity.z >= 100.f && pm->ground.plane.normal[ 2 ] >= 0.9f && !( pm->s.pm_flags & PMF_DUCKED ) ) {
					pm->s.pm_flags |= PMF_TIME_TRICK_JUMP;
					pm->s.pm_time = 64;
				}

				// [Paril-KEX] calculate impact delta; this also fixes triple jumping
				Vector3 clipped_velocity = QM_Vector3Zero();
				PM_ClipVelocity( pml.velocity, pm->ground.plane.normal, clipped_velocity, 1.01f );

				pm->impact_delta = pml.startVelocity.z - clipped_velocity.z;

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

	// Get waterlevel, accounting for ducking.
	PM_GetWaterLevel( pml.origin, pm->waterlevel, pm->watertype );
}

/**
*	@brief
**/
static void PM_CheckJump() {
	// Hasn't been long enough since landing to jump again.
	if ( pm->s.pm_flags & PMF_TIME_LAND ) {
		return;
	}

	// Can't jump while ducked.
	if ( pm->s.pm_flags & PMF_DUCKED ) {
		return;
	}

	// Player has let go of jump button.
	if ( !( pm->cmd.buttons & BUTTON_JUMP ) ) { 
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// Player must wait for jump to be released.
	if ( pm->s.pm_flags & PMF_JUMP_HELD ) {
		return;
	}

	// Can't jump while dead.
	if ( pm->s.pm_type == PM_DEAD ) {
		return;
	}

	// We're swimming, not jumping, so unset ground entity.
	if ( pm->waterlevel >= water_level_t::WATER_WAIST ) { 
		//pm->ground.entity = nullptr;
		// Unset ground.
		PM_UpdateGroundFromTrace( nullptr );
		return;
	}

	// In-air/liquid, so no effect, can't re-jump without ground.
	if ( pm->ground.entity == nullptr ) {
		return;
	}

	// Adjust our pmove state to engage in the act of jumping.
	pm->s.pm_flags |= PMF_JUMP_HELD;
	pm->jump_sound = true;
	//pm->groundEntity = nullptr;
	//pm->groundPlane = {};
		// Unset ground.
	PM_UpdateGroundFromTrace( nullptr );
	pm->s.pm_flags &= ~PMF_ON_GROUND;

	const float jump_height = pmp->pm_jump_height;

	pml.velocity.z += jump_height;
	if ( pml.velocity.z < jump_height ) {
		pml.velocity.z = jump_height;
	}
}

/**
*	@brief
**/
static void PM_CheckSpecialMovement() {
	// Having time means we're already doing some form of 'special' movement.
	if ( pm->s.pm_time ) {
		return;
	}

	// Remove ladder flag.
	pm->s.pm_flags &= ~PMF_ON_LADDER;

	// Re-Check for a ladder.
	Vector3 flatforward = QM_Vector3Normalize( {
		pml.forward.x,
		pml.forward.y,
		0.f
	} );
	const Vector3 spot = pml.origin + ( flatforward * 1 );
	trace_t trace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot, CONTENTS_SOLID | CONTENTS_LADDER );
	// WID: TODO: Technically, the trace below should work, but for some reason it does not. And yet, the above DOES, which is merely by adding CONTENTS_SOLID to it.
	//trace_t trace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER);
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) && pm->waterlevel < water_level_t::WATER_WAIST ) {
		pm->s.pm_flags |= PMF_ON_LADDER;
	}

	// Don't do any 'special movement' if we're not having any gravity.
	if ( !pm->s.gravity ) {
		return;
	}

	// check for water jump
	// [Paril-KEX] don't try waterjump if we're moving against where we'll hop
	if ( !( pm->cmd.buttons & BUTTON_JUMP )	&& pm->cmd.forwardmove <= 0 ) {
		return;
	}

	if ( pm->waterlevel != water_level_t::WATER_WAIST ) {
		return;
	}
	// [Paril-KEX]
	//else if ( pm->watertype & CONTENTS_NO_WATERJUMP ) {
	//	return;
	//}

	// Quick check that something is even blocking us forward
	Vector3 blockTraceEnd = pml.origin + flatforward * 40.f;
	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, blockTraceEnd, MASK_SOLID );

	// we aren't blocked, or what we're blocked by is something we can walk up
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] >= MIN_STEP_NORMAL ) {
		return;
	}

	// [Paril-KEX] improved waterjump
	Vector3 waterjump_vel = {
		flatforward.x * 50,
		flatforward.y * 50,
		350.f
	};
	// simulate what would happen if we jumped out here, and
	// if we land on a dry spot we're good!
	// simulate 1 sec worth of movement
	pm_touch_trace_list_t touches;
	Vector3 waterjump_origin = { pml.origin.x, pml.origin.y, pml.origin.z };//vec3_t waterjump_origin = pml.origin;
	float time = 0.1f;
	bool has_time = true;

	for ( size_t i = 0; i < min( 50, (int32_t)( 10 * ( 800.f / pm->s.gravity ) ) ); i++ ) {
		waterjump_vel.z -= pm->s.gravity * time;

		if ( waterjump_vel.z < 0 ) {
			has_time = false;
		}

		PM_StepSlideMove_Generic( waterjump_origin, waterjump_vel, time, pm->mins, pm->maxs, touches, has_time );
	}

	// snap down to ground
	Vector3 snapTraceEnd = waterjump_origin + Vector3{ 0.f, 0.f, -2.f };
	trace = PM_Trace( waterjump_origin, pm->mins, pm->maxs, snapTraceEnd, MASK_SOLID );

	// Can't stand here.
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] < MIN_STEP_NORMAL || trace.endpos[ 2 ] < pml.origin.z ) {
		return;
	}

	// We're currently standing on ground, and the snapped position is a step.
	if ( pm->ground.entity && fabsf( pml.origin.z - trace.endpos[2] ) <= PM_MAX_STEP_SIZE ) {
		return;
	}

	// Get water level.
	water_level_t level;
	contents_t type;
	PM_GetWaterLevel( trace.endpos, level, type );

	// The water jump spot will be under water, so we're probably hitting something weird that isn't important
	if ( level >= water_level_t::WATER_WAIST ) {
		return;
	}

	// Valid waterjump! Jump out of water
	pml.velocity = flatforward * 50;
	pml.velocity.z = 350;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 2048;
}

/**
*	@brief
**/
static void PM_FlyMove( bool doclip ) {
	float drop = 0.f;

	// When clipping don 't adjust viewheight, if no-clipping, default a viewheight of 22.
	pm->s.viewheight = doclip ? 0 : 22;

	// Calculate friction
	const float speed = QM_Vector3Length( pml.velocity );
	if ( speed < 1 ) {
		pml.velocity = QM_Vector3Zero();
	} else {
		drop = 0;

		const float friction = pmp->pm_friction * 1.5f; // extra friction
		const float control = speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed;
		drop += control * friction * pml.frameTime;

		// Scale the velocity
		float newspeed = speed - drop;
		if ( newspeed < 0 ) {
			newspeed = 0;
		}
		newspeed /= speed;

		pml.velocity *= newspeed;
	}

	// Accelerate
	const float forwardMove = pm->cmd.forwardmove;
	const float sidewardMove = pm->cmd.sidemove;
	pml.forward	= QM_Vector3Normalize( pml.forward );
	pml.right	= QM_Vector3Normalize( pml.right );
	Vector3 wishvel = pml.forward * forwardMove + pml.right * sidewardMove;

	if ( pm->cmd.buttons & BUTTON_JUMP ) {
		wishvel.z += ( pmp->pm_fly_speed * 0.5f );
	}
	if ( pm->cmd.buttons & BUTTON_CROUCH ) {
		wishvel.z -= ( pmp->pm_fly_speed * 0.5f );
	}

	Vector3 wishdir = wishvel;
	float wishspeed = QM_Vector3NormalizeLength( wishdir );

	// Clamp to server defined max speed.
	if ( wishspeed > pmp->pm_max_speed ) {
		wishvel *= pmp->pm_max_speed / wishspeed;
		wishspeed = pmp->pm_max_speed;
	}

	// Paril: newer clients do this.
	wishspeed *= 2;

	const float currentspeed = QM_Vector3DotProduct( pml.velocity, wishdir );
	float addspeed = wishspeed - currentspeed;

	if ( addspeed > 0 ) {
		float accelspeed = pmp->pm_accelerate * pml.frameTime * wishspeed;
		if ( accelspeed > addspeed ) {
			accelspeed = addspeed;
		}
		pml.velocity += accelspeed * wishdir;
	}

	if ( doclip ) {
		/*for (i = 0; i < 3; i++)
			end[i] = pml.origin[i] + pml.frameTime * pml.velocity[i];

		trace = PM_Trace(pml.origin, pm->mins, pm->maxs, end);

		pml.origin = trace.endpos;*/
		// Step Slide.
		PM_StepSlideMove();
	} else {
		// Hard set origin in order to move.
		pml.origin += ( pml.velocity * pml.frameTime );
	}
}

/**
*	@brief	Sets the player's movement bounding box depending on various state factors.
**/
static void PM_SetDimensions() {
	// Start with a {-16,-16, 0}, {16,16, 0}, and set Z after.
	pm->mins.x = -16;
	pm->mins.y = -16;
	pm->maxs.x = 16;
	pm->maxs.y = 16;

	// Specifical gib treatment.
	if ( pm->s.pm_type == PM_GIB ) {
		pm->mins.z = 0;
		pm->maxs.z = 16;
		pm->s.viewheight = 8;
		return;
	}

	// Mins for standing/crouching/dead.
	pm->mins.z = -24;

	// Dead, and Ducked bbox:
	if ( ( pm->s.pm_flags & PMF_DUCKED ) || pm->s.pm_type == PM_DEAD ) {
		pm->maxs.z = 4;
		pm->s.viewheight = -2;
	// Alive and kicking bbox:
	} else {
		pm->maxs.z = 32;
		pm->s.viewheight = 22;
	}
}

/**
*	@brief	Determine whether we're above water, or not.
**/
static inline const bool PM_AboveWater() {
	// Testing point.
	const Vector3 below = pml.origin + Vector3{ 0, 0, -8 };
	// We got solid below, not water:
	bool solid_below = pm->trace( QM_Vector3ToQFloatV( pml.origin ).v, QM_Vector3ToQFloatV( pm->mins ).v, QM_Vector3ToQFloatV( pm->maxs ).v, QM_Vector3ToQFloatV( below ).v, pm->player, MASK_SOLID ).fraction < 1.0f;
	if ( solid_below ) {
		return false;
	}

	// We're above water:
	bool water_below = pm->trace( QM_Vector3ToQFloatV( pml.origin ).v, QM_Vector3ToQFloatV( pm->mins ).v, QM_Vector3ToQFloatV( pm->maxs ).v, QM_Vector3ToQFloatV( below ).v, pm->player, MASK_WATER ).fraction < 1.0f;
	if ( water_below ) {
		return true;
	}

	// No water below.
	return false;
}

/**
*	@brief	Decide whether to duck, and/or unduck.
**/
static inline const bool PM_CheckDuck() {
	// Can't duck if gibbed.
	if ( pm->s.pm_type == PM_GIB /*|| pm->s.pm_type == PM_DEAD*/ ) {
		return false;
	}

	trace_t trace;
	bool flags_changed = false;

	// Dead:
	if ( pm->s.pm_type == PM_DEAD ) {
		// TODO: This makes no sense, since the actual check in SetDimensions
		// does the same for PM_DEAD as it does for being DUCKED.
		if ( !( pm->s.pm_flags & PMF_DUCKED ) ) {
			pm->s.pm_flags |= PMF_DUCKED;
			flags_changed = true;
		}
	// Duck:
	} else if (
		( pm->cmd.buttons & BUTTON_CROUCH ) &&
		( pm->ground.entity || ( pm->waterlevel <= water_level_t::WATER_FEET && !PM_AboveWater() ) ) &&
		!( pm->s.pm_flags & PMF_ON_LADDER ) ) { 
		if ( !( pm->s.pm_flags & PMF_DUCKED ) ) {
			// check that duck won't be blocked
			Vector3 check_maxs = { pm->maxs.x, pm->maxs.y, 4 };
			trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin );
			if ( !trace.allsolid ) {
				pm->s.pm_flags |= PMF_DUCKED;
				flags_changed = true;
			}
		}
	// Try and get out of the ducked state, stand up, if possible.
	} else {
		if ( pm->s.pm_flags & PMF_DUCKED ) {
			// try to stand up
			Vector3 check_maxs = { pm->maxs.x, pm->maxs.y, 32 };
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

	// Determine the possible new dimensions since our flags have changed.
	PM_SetDimensions();

	// We're ducked.
	return true;
}

/**
*	@brief	No control over movement, just go where velocity brings us.
*			Apply extra friction of the dead body is on-ground.
**/
static void PM_DeadMove() {
	float forward;

	if ( !pm->ground.entity ) {
		return;
	}

	// Add extra friction for our dead body.
	forward = QM_Vector3Length( pml.velocity );
	forward -= 20;
	if ( forward <= 0 ) {
		pml.velocity = QM_Vector3Zero();
	} else {
		// Normalize.
		pml.velocity = QM_Vector3Normalize( pml.velocity );
		// Scale by old velocity derived length.
		pml.velocity *= forward;
	}
}

/**
*	@brief	True if we're standing in a legitimate non solid position.
**/
static inline const bool PM_GoodPosition() {
	// Position is always valid if no-clipping.
	if ( pm->s.pm_type == PM_NOCLIP ) {
		return true;
	}

	const trace_t trace = PM_Trace( pm->s.origin, pm->mins, pm->maxs, pm->s.origin );
	return !trace.allsolid;
}

/**
*	@brief	On exit, the origin will have a value that is pre-quantized to the PMove
*			precision of the network channel and in a valid position.
**/
static void PM_SnapPosition() {
	pm->s.velocity = pml.velocity;
	pm->s.origin = pml.origin;

	if ( PM_GoodPosition() ) {
		return;
	}

	//if ( G_FixStuckObject_Generic( pm->s.origin, pm->mins, pm->maxs  ) == stuck_result_t::NO_GOOD_POSITION ) {
	//	pm->s.origin = pml.previousOrigin;
	//	return;
	//}
}

/**
*	@brief
**/
static void PM_InitialSnapPosition() {
	//int32_t x, y, z;
	constexpr int	offset[ 3 ] = { 0, -1, 1 };
	const Vector3 base = pm->s.origin;
	
	for ( int32_t z = 0; z < 3; z++ ) {
		pm->s.origin.z = base.z + offset[ z ];
		for ( int32_t y = 0; y < 3; y++ ) {
			pm->s.origin.y = base.y + offset[ y ];
			for ( int32_t x = 0; x < 3; x++ ) {
				pm->s.origin.x = base.x + offset[ x ];
				if ( PM_GoodPosition() ) {
					pml.origin = pm->s.origin;
					pml.previousOrigin = pm->s.origin;
					return;
				}
			}
		}
	}
}

/**
*	@brief	
**/
static void PM_ClampAngles() {
	if ( pm->s.pm_flags & PMF_TIME_TELEPORT ) {
		pm->viewangles[ YAW ] = AngleMod( pm->cmd.angles[ YAW ] + pm->s.delta_angles[ YAW ] );
		pm->viewangles[ PITCH ] = 0;
		pm->viewangles[ ROLL ] = 0;
	} else {
		// Circularly clamp the angles with deltas,
		pm->viewangles = QM_Vector3AngleMod( pm->cmd.angles + pm->s.delta_angles );

		// Don't let the player look up or down more than 90 degrees.
		#ifdef PM_CLAMP_VIEWANGLES_0_TO_360
		if ( pm->viewangles[ PITCH ] >= 90 && pm->viewangles[ PITCH ] <= 180 ) {
			pm->viewangles[ PITCH ] = 90;
		} else if ( pm->viewangles[ PITCH ] <= 270 && pm->viewangles[ PITCH ] >= 180 ) {
			pm->viewangles[ PITCH ] = 270;
		}
		#else
		if ( pm->viewangles[ PITCH ] > 90 && pm->viewangles[ PITCH ] < 270 ) {
			pm->viewangles[ PITCH ] = 90;
		} else if ( pm->viewangles[ PITCH ] <= 360 && pm->viewangles[ PITCH ] >= 270 ) {
			pm->viewangles[ PITCH ] -= 360;
		}
		#endif
	}

	// Calculate angle vectors derived from current viewing angles.
	QM_AngleVectors( pm->viewangles, &pml.forward, &pml.right, &pml.up );
}

/**
*	@brief	
**/
static void PM_ScreenEffects() {
	// Add for contents
	Vector3 vieworg = {
		pml.origin.x + pm->viewoffset.x,
		pml.origin.y + pm->viewoffset.y,
		pml.origin.z + pm->viewoffset.z + (float)pm->s.viewheight
	};
	const int32_t contents = pm->pointcontents( QM_Vector3ToQFloatV( vieworg ).v );//contents_t contents = pm->pointcontents( vieworg );

	// Under a 'water' like solid:
	if ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) {
		pm->rdflags |= RDF_UNDERWATER;
	// Or not:
	} else {
		pm->rdflags &= ~RDF_UNDERWATER;
	}

	// Prevent it from adding screenblend if we're inside a client entity, by checking
	// if its brush has CONTENTS_PLAYER set in its clipmask.
	if ( /*!( contents & CONTENTS_PLAYER )
		&& */( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) ) ) {
		SG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, &pm->screen_blend.x );
	} else if ( contents & CONTENTS_SLIME ) {
		SG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, &pm->screen_blend.x );
	} else if ( contents & CONTENTS_WATER ) {
		SG_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, &pm->screen_blend.x );
	}
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
	pm->viewangles = {};
	//pm->s.viewheight = 0;
	//pm->groundentity = nullptr;
	pm->watertype = CONTENTS_NONE;
	pm->waterlevel = water_level_t::WATER_NONE;
	pm->screen_blend = {};
	pm->rdflags = 0;
	pm->jump_sound = false;
	pm->step_clip = false;
	pm->step_height = 0;
	pm->impact_delta = 0;

	// Clear all pmove local vars
	pml = {};

	// Store the origin and velocity in pmove local.
	pml.origin = pm->s.origin;
	pml.velocity = pm->s.velocity;
	// Save the start velocity.
	pml.startVelocity = pm->s.velocity;
	// Save the origin as 'old origin' for in case we get stuck.
	pml.previousOrigin = pm->s.origin;

	// Calculate frameTime.
	pml.frameTime = pm->cmd.msec * 0.001f;

	// Clamp view angles.
	PM_ClampAngles( );

	// Remove the ground entity changed flag.
	//pm->s.pm_flags &= ~PMF_GROUNDENTITY_CHANGED;

	// Performs fly move, only clips in case of spectator mode, noclips otherwise.
	if ( pm->s.pm_type == PM_SPECTATOR || pm->s.pm_type == PM_NOCLIP ) {
		// Re-ensure no flags are set anymore.
		pm->s.pm_flags = PMF_NONE;

		// Give the spectator a small 8x8x8 bounding box.
		if ( pm->s.pm_type == PM_SPECTATOR ) {
			pm->mins.x = -8;
			pm->mins.y = -8;
			pm->mins.z = -8;
			pm->maxs.x = 8;
			pm->maxs.y = 8;
			pm->maxs.z = 8;
		}

		// Get moving.
		PM_FlyMove( pm->s.pm_type == PM_SPECTATOR );
		// Snap to position.
		PM_SnapPosition( );
		return;
	}

	// Erase all input command state when dead, we don't want to allow moving our dead body.
	if ( pm->s.pm_type >= PM_DEAD ) {
		// Idle our directional user input.
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
		// Unset these two buttons so other code won't respond to it.
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
		const int32_t msec = pm->cmd.msec;

		if ( msec >= pm->s.pm_time ) {
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
		pml.velocity.z -= pm->s.gravity * pml.frameTime;

		// Cancel as soon as we are falling down again.
		if ( pml.velocity.z < 0 ) { // cancel as soon as we are falling down again
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
		// Otherwise AirMove(Which, also, does ground move.)
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
}

void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp ) {
	// Q2RTXPerimental Defaults:
	pmp->pm_air_accelerate = atof( (const char *)SG_GetConfigString( CS_AIRACCEL ) );
	
	//#ifdef CLGAME_INCLUDE
	//SG_DPrintf( "[CLGame]: CS_AIRACCELL=%f\n", pmp->pm_air_accelerate );
	//#else
	////SG_DPrintf( "[SVGame]: CS_AIRACCELL=%f\n", pmp->pm_air_accelerate );
	//#endif
	//
	// Configure the defaults, however, here one could for example use the
	// player move stats by adding a 'class' slot, and basing movement parameters
	// on that.
	//
	pmp->pm_stop_speed = default_pmoveParams_t::pm_stop_speed;
	pmp->pm_max_speed = default_pmoveParams_t::pm_max_speed;
	pmp->pm_jump_height = default_pmoveParams_t::pm_jump_height;

	pmp->pm_ladder_speed = default_pmoveParams_t::pm_ladder_speed;
	pmp->pm_ladder_sidemove_speed = default_pmoveParams_t::pm_ladder_sidemove_speed;
	pmp->pm_ladder_mod = default_pmoveParams_t::pm_ladder_mod;

	pmp->pm_duck_speed = default_pmoveParams_t::pm_duck_speed;
	pmp->pm_water_speed = default_pmoveParams_t::pm_water_speed;
	pmp->pm_fly_speed = default_pmoveParams_t::pm_fly_speed;

	pmp->pm_accelerate = default_pmoveParams_t::pm_accelerate;
	pmp->pm_water_accelerate = default_pmoveParams_t::pm_water_accelerate;

	pmp->pm_friction = default_pmoveParams_t::pm_friction;
	pmp->pm_water_friction = default_pmoveParams_t::pm_water_friction;
}

