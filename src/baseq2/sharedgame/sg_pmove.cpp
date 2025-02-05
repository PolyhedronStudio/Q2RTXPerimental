/********************************************************************
*
*
*	SharedGame: Player Movement
*
*
********************************************************************/
#include "shared/shared.h"

#include "sg_shared.h"
#include "sg_misc.h"
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
//! Pointer to the pmove's playerState.
static player_state_t *ps;
//! An actual pointer to the pmove parameters object for use with moving.
static pmoveParams_t *pmp;
//! Contains our local in-moment move variables.
static pml_t pml;


/**
*	@brief	Default player movement parameter constants.
**/
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
	static constexpr float pm_crouch_move_speed = 100.f;
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
*
*
*	Misc:
* 
* 
**/
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
*	@brief	Sets the player's movement bounding box depending on various state factors.
**/
static void PM_SetDimensions() {
	// Start with a {-16,-16, 0}, {16,16, 0}, and set Z after.
	pm->mins.x = -16;
	pm->mins.y = -16;
	pm->maxs.x = 16;
	pm->maxs.y = 16;

	// Specifical gib treatment.
	if ( ps->pmove.pm_type == PM_GIB ) {
		pm->mins.z = 0;
		pm->maxs.z = 16;
		ps->pmove.viewheight = 8;
		return;
	}

	// Mins for standing/crouching/dead.
	pm->mins.z = -24;

	// Dead, and Ducked bbox:
	if ( ( ps->pmove.pm_flags & PMF_DUCKED ) || ps->pmove.pm_type == PM_DEAD ) {
		pm->maxs.z = 4;
		ps->pmove.viewheight = -2;
		// Alive and kicking bbox:
	} else {
		pm->maxs.z = 32;
		ps->pmove.viewheight = 22;
	}
}
/**
*	@brief	Keeps track of the player's xySpeed and bobCycle:  
**/
//static void PM_Footsteps( void ) {
static void PM_CycleBob() {
	float		bobMove = 0.f;
	int32_t		oldBobCycle = 0;
	// Defaults to false and checked for later on:
	qboolean	footStep = false;

	//
	// Calculate the speed and cycle to be used for all cyclic walking effects.
	//
	pm->xySpeed = sqrtf( ps->pmove.velocity[ 0 ] * ps->pmove.velocity[ 0 ]
		+ ps->pmove.velocity[ 1 ] * ps->pmove.velocity[ 1 ] );

	// Airborne leaves cycle intact, but doesn't advance either.
	if ( pm->ground.entity == nullptr ) {

		//if ( ps->powerups[ PW_INVULNERABILITY ] ) {
		//	PM_ContinueLegsAnim( LEGS_IDLECR );
		//}
		//// Airborne leaves position in cycle intact, but doesn't advance:
		//if ( pm->liquid.level > LIQUID_FEET ) {
		//	PM_ContinueLegsAnim( LEGS_SWIM );
		//}
		return;
	}

	// If not trying to move:
	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove ) {
		if ( pm->xySpeed < 5 ) {
			// Start at beginning of cycle again:
			ps->bobCycle = 0;
			//if ( ps->pmove.pm_flags & PMF_DUCKED ) {
			//	PM_ContinueLegsAnim( LEGS_IDLECR );
			//} else {
			//	PM_ContinueLegsAnim( LEGS_IDLE );
			//}
		}
		return;
	}

	// Default to no footstep:
	footStep = false;

	if ( ps->pmove.pm_flags & PMF_DUCKED ) {
		// Ducked characters bob much faster:
		bobMove = 0.5;	
		//if ( ps->pmove.pm_flags & PMF_BACKWARDS_RUN ) {
		//	PM_ContinueLegsAnim( LEGS_BACKCR );
		//} else {
		//	PM_ContinueLegsAnim( LEGS_WALKCR );
		//}
		// Ducked characters never play footsteps.
	//*
	//} else 	if ( pm->ps->pm_flags & PMF_BACKWARDS_RUN ) {
	//	if ( !( pm->cmd.buttons & BUTTON_WALKING ) ) {
	//		bobMove = 0.4;	// faster speeds bob faster
	//		footStep = true;
	//	} else {
	//		bobMove = 0.3;
	//	}
	//	PM_ContinueLegsAnim( LEGS_BACK );
	//*/
	} else {
		if ( !( pm->cmd.buttons & BUTTON_WALK ) ) {
			// Faster speeds bob faster:
			bobMove = 0.4f;
			//if ( ps->pmove.pm_flags & PMF_BACKWARDS_RUN ) {
			//	PM_ContinueLegsAnim( LEGS_BACK );
			//} else {
			//	PM_ContinueLegsAnim( LEGS_RUN );
			//}
			footStep = true;
		} else {
			// Walking bobs slow:
			bobMove = 0.3f;
			//if ( ps->pmove.pm_flags & PMF_BACKWARDS_RUN ) {
			//	PM_ContinueLegsAnim( LEGS_BACKWALK );
			//} else {
			//	PM_ContinueLegsAnim( LEGS_WALK );
			//}
		}
	}

	// check for footstep / splash sounds
	oldBobCycle = ps->bobCycle;
	ps->bobCycle = (int32_t)( (oldBobCycle + bobMove * pm->cmd.msec) /* pml.msec */) & 255;
	
	//SG_DPrintf( "%s: ps->bobCycle(%i), oldBobCycle(%i), bobMove(%f)\n", __func__, ps->bobCycle, oldBobCycle, bobMove );
	
	// if we just crossed a cycle boundary, play an appropriate footstep event
	//if ( ( ( oldBobCycle + 64 ) ^ ( ps->bobCycle + 64 ) ) & 128 ) {
	//	// On-ground will only play sounds if running:
	//	if ( pm->liquid.level == LIQUID_NONE ) {
	//		if ( footStep && !pm->noFootsteps ) {
	//			PM_AddEvent( PM_FootstepForSurface() );
	//		}
	//	// Splashing:
	//	} else if ( pm->liquid.level == LIQUID_FEET ) {
	//		PM_AddEvent( EV_FOOTSPLASH );
	//	// Wading / Swimming at surface.
	//	} else if ( pm->liquid.level == LIQUID_WAIST ) {
	//		PM_AddEvent( EV_SWIM );
	//	// None:
	//	} else if ( pm->liquid.level == LIQUID_UNDER ) {
	//		// No sound when completely underwater. Lol.
	//	}
	//}
}
/**
*	@brief	Inline-wrapper to for convenience.
**/
//void PM_AddEvent( const uint8_t newEvent, const uint8_t parameter ) {
//	SG_PlayerState_AddPredictableEvent( newEvent, parameter, &ps->pmove );
//}



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
		if ( trace->ent && trace->plane.normal[2] >= PM_MIN_STEP_NORMAL) {
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
		// Store non absolute but exact step height.
		pm->step_height = step_height;

		// Add predicted step event.
		//PM_AddEvent( 1 /*PM_EVENT_STEP*/, fabs( step_height ) );
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
	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frameTime, pm->mins, pm->maxs, pm->touchTraces, ps->pmove.pm_time );

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
	PM_StepSlideMove_Generic( pml.origin, pml.velocity, pml.frameTime, pm->mins, pm->maxs, pm->touchTraces, ps->pmove.pm_time );

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

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < PM_MIN_STEP_NORMAL ) {
		pml.origin = downOrigin;
		pml.velocity = downVelocity;
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (ps->pmove.pm_flags & PMF_ON_GROUND)
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		pml.velocity.z = downVelocity.z;
	}

	// Paril: step down stairs/slopes
	if ( ( ps->pmove.pm_flags & PMF_ON_GROUND ) && !( ps->pmove.pm_flags & PMF_ON_LADDER ) &&
		( pm->liquid.level < liquid_level_t::LIQUID_WAIST || ( !( pm->cmd.buttons & BUTTON_JUMP ) && pml.velocity.z <= 0 ) ) ) {
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
*
*
*	Acceleration/Friction/Currents:
*
*
**/
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

	pml.velocity = QM_Vector3MultiplyAdd( pml.velocity, accelerationSpeed, wishDirection );
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

	pml.velocity = QM_Vector3MultiplyAdd( pml.velocity, accelerationSpeed, wishDirection );
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
	if ( ( pm->ground.entity != nullptr && pml.ground.surface != nullptr && !( pml.ground.surface->flags & SURF_SLICK ) ) || ( ps->pmove.pm_flags & PMF_ON_LADDER ) ) {
		// Get the material to fetch friction from.
		cm_material_t *ground_material = ( pml.ground.surface != nullptr ? pml.ground.surface->material : nullptr );
		float friction = ( ground_material ? ground_material->physical.friction : pmp->pm_friction );
		const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		drop += control * friction * pml.frameTime;
	}
#else
	// Apply ground friction if on-ground.
	float drop = 0;
	if ( ( pm->ground.entity && pml.ground.surface && !( pml.ground.surface->flags & SURF_SLICK ) ) || ( ps->pmove.pm_flags & PMF_ON_LADDER ) ) {
		const float friction = pmp->pm_friction;
		const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		drop += control * friction * pml.frameTime;
	}
#endif
	// Apply water friction, and not off-ground yet on a ladder.
	if ( pm->liquid.level && !( ps->pmove.pm_flags & PMF_ON_LADDER ) ) {
		drop += speed * pmp->pm_water_friction * (float)pm->liquid.level * pml.frameTime;
	}

	// Scale the velocity.
	float newspeed = speed - drop;
	if ( newspeed <= 0 ) {
		newspeed = 0;
		pml.velocity = {};
	} else {
		newspeed /= speed;
		pml.velocity *= newspeed;
	}
}
/**
*	@brief	Handles the velocities for 'ladders', as well as water and conveyor belt by applying their 'currents'.
**/
static void PM_AddCurrents( Vector3 &wishVelocity ) {
	// Account for ladders.
	if ( ps->pmove.pm_flags & PMF_ON_LADDER ) {
		if ( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) {
			// [Paril-KEX]: if we're underwater, use full speed on ladders
			const float ladder_speed = pm->liquid.level >= liquid_level_t::LIQUID_WAIST ? pmp->pm_max_speed : pmp->pm_ladder_speed;
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
				if ( ps->viewangles[ PITCH ] >= 271 && ps->viewangles[ PITCH ] < 345 ) {
					wishVelocity.z = ladder_speed;
				} else if ( ps->viewangles[ PITCH ] < 271 && ps->viewangles[ PITCH ] >= 15 ) {
					wishVelocity.z = -ladder_speed;
				}
				#else
				if ( ps->viewangles[ PITCH ] < 15 ) {
					wishVelocity.z = ladder_speed;
				} else if ( ps->viewangles[ PITCH ] < 271 && ps->viewangles[ PITCH ] >= 15 ) {
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

				if ( pm->liquid.level < liquid_level_t::LIQUID_WAIST ) {
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
					wishVelocity = QM_Vector3MultiplyAdd( wishVelocity, -ladder_speed, right );
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
	if ( pm->liquid.type & MASK_CURRENT ) {
		Vector3 velocity = QM_Vector3Zero();

		if ( pm->liquid.type & CONTENTS_CURRENT_0 ) {
			velocity.x += 1;
		}
		if ( pm->liquid.type & CONTENTS_CURRENT_90 ) {
			velocity.y += 1;
		}
		if ( pm->liquid.type & CONTENTS_CURRENT_180 ) {
			velocity.x -= 1;
		}
		if ( pm->liquid.type & CONTENTS_CURRENT_270 ) {
			velocity.y -= 1;
		}
		if ( pm->liquid.type & CONTENTS_CURRENT_UP ) {
			velocity.z += 1;
		}
		if ( pm->liquid.type & CONTENTS_CURRENT_DOWN ) {
			velocity.z -= 1;
		}

		float speed = pmp->pm_water_speed;
		// Walking in water, against a current, so slow down our 
		if ( ( pm->liquid.level == liquid_level_t::LIQUID_FEET ) && ( pm->ground.entity ) ) {
			speed /= 2;
		}

		wishVelocity = QM_Vector3MultiplyAdd( wishVelocity, speed, velocity );
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

		wishVelocity = QM_Vector3MultiplyAdd( wishVelocity, 100.f, velocity );
	}
}



/**
*
*
*	Various MoveType Specific Implementations:
*
*
**/
/**
*	@brief
**/
static void PM_GenericMove() {
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
	const float maxSpeed = ( ps->pmove.pm_flags & PMF_DUCKED ) ? pmp->pm_crouch_move_speed : pmp->pm_max_speed;

	if ( wishSpeed > maxSpeed ) {
		wishVelocity *= maxSpeed / wishSpeed;
		wishSpeed = maxSpeed;
	}

	// Perform ladder movement.
	if ( ps->pmove.pm_flags & PMF_ON_LADDER ) {
		PM_Accelerate( wishDirection, wishSpeed, pmp->pm_accelerate );
		if ( !wishVelocity.z ) {
			// Apply gravity as a form of 'friction' to prevent 'racing/sliding' against the ladder.
			if ( pml.velocity.z > 0 ) {
				pml.velocity.z -= ps->pmove.gravity * pml.frameTime;
				if ( pml.velocity.z < 0 ) {
					pml.velocity.z = 0;
				}
			} else {
				pml.velocity.z += ps->pmove.gravity * pml.frameTime;
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
		if ( ps->pmove.gravity > 0 ) {
			pml.velocity.z = 0;
		} else {
			pml.velocity.z -= ps->pmove.gravity * pml.frameTime;
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
		if ( ps->pmove.pm_type != PM_GRAPPLE ) {
			pml.velocity.z -= ps->pmove.gravity * pml.frameTime;
		}

		// Step Slide.
		PM_StepSlideMove();
	}
}
/**
*	@brief	Performs in-water movement.
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
	if ( ( ps->pmove.pm_flags & PMF_DUCKED ) && wishspeed > pmp->pm_crouch_move_speed ) {
		wishVelocity *= pmp->pm_crouch_move_speed / wishspeed;
		wishspeed = pmp->pm_crouch_move_speed;
	}

	// Accelerate through water.
	PM_Accelerate( wishDirection, wishspeed, pmp->pm_water_accelerate );

	// Step Slide.
	PM_StepSlideMove();
}
/**
*	@brief	Performs out of water jump movement. Apply gravity, check whether to cancel water jump mode, and slide move right after.
**/
static void PM_WaterJumpMove() {
	// Apply downwards gravity to the velocity.
	pml.velocity.z -= ps->pmove.gravity * pml.frameTime;

	// Cancel the WaterJump mode as soon as we are falling down again. (Velocity < 0).
	if ( pml.velocity.z < 0 ) { // cancel as soon as we are falling down again
		ps->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
		ps->pmove.pm_time = 0;
	}

	// Step slide move.
	PM_StepSlideMove();
}
/**
*	@brief
**/
static void PM_FlyMove( bool doclip ) {
	float drop = 0.f;

	// When clipping don 't adjust viewheight, if no-clipping, default a viewheight of 22.
	ps->pmove.viewheight = doclip ? 0 : 22;

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
		//float newspeed = speed - drop;
		//if ( newspeed < 0 ) {
		//	newspeed = 0;
		//}
		//newspeed /= speed;

		//pml.velocity *= newspeed;
		// Scale the velocity.
		float newspeed = speed - drop;
		if ( newspeed <= 0 ) {
			newspeed = 0;
			pml.velocity = {};
		} else {
			newspeed /= speed;
			pml.velocity *= newspeed;
		}
	}

	// Accelerate
	const float forwardMove = pm->cmd.forwardmove;
	const float sidewardMove = pm->cmd.sidemove;
	pml.forward = QM_Vector3Normalize( pml.forward );
	pml.right = QM_Vector3Normalize( pml.right );
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
*
*
*	(Brush Contents-)Position Categorization:
*
*
**/
/**
*	@brief
**/
static inline void PM_GetWaterLevel( const Vector3 &position, liquid_level_t &level, contents_t &type ) {
	//
	// get liquid.level, accounting for ducking
	//
	level = liquid_level_t::LIQUID_NONE;
	type = CONTENTS_NONE;

	int32_t sample2 = (int)( ps->pmove.viewheight - pm->mins.z );
	int32_t sample1 = sample2 / 2;

	Vector3 point = position;

	point.z += pm->mins.z + 1;

	contents_t contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );

	if ( contentType & MASK_WATER ) {
		type = contentType;
		level = liquid_level_t::LIQUID_FEET;
		point.z = pml.origin.z + pm->mins.z + sample1;
		contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );
		if ( contentType & MASK_WATER ) {
			level = liquid_level_t::LIQUID_WAIST;
			point.z = pml.origin.z + pm->mins.z + sample2;
			contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );
			if ( contentType & MASK_WATER ) {
				level = liquid_level_t::LIQUID_UNDER;
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

	if ( pml.velocity.z > 180 || ps->pmove.pm_type == PM_GRAPPLE ) { //!!ZOID changed from 100 to 180 (ramp accel)
		// We are going off-ground due to a too high velocit.
		ps->pmove.pm_flags &= ~PMF_ON_GROUND;

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
		//pml.groundMaterial = trace.material;
		pm->ground.material = trace.material;

		// [Paril-KEX] to attempt to fix edge cases where you get stuck
		// wedged between a slope and a wall (which is irrecoverable
		// most of the time), we'll allow the player to "stand" on
		// slopes if they are right up against a wall
		bool slanted_ground = trace.fraction < 1.0f && trace.plane.normal[ 2 ] < PM_MIN_STEP_NORMAL;

		if ( slanted_ground ) {
			Vector3 slantTraceEnd = pml.origin + trace.plane.normal;
			trace_t slant = PM_Trace( pml.origin, pm->mins, pm->maxs, slantTraceEnd );

			if ( slant.fraction < 1.0f && !slant.startsolid ) {
				slanted_ground = false;
			}
		}

		if ( trace.fraction == 1.0f || ( slanted_ground && !trace.startsolid ) ) {
			pm->ground.entity = nullptr;
			ps->pmove.pm_flags &= ~PMF_ON_GROUND;
		} else {
			PM_UpdateGroundFromTrace( &trace );

			// hitting solid ground will end a waterjump
			if ( ps->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
				ps->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
				ps->pmove.pm_time = 0;
			}

			// Just hit the ground.
			if ( !( ps->pmove.pm_flags & PMF_ON_GROUND ) ) {

				// [Paril-KEX]
				if ( pml.velocity.z >= 100.f && pm->ground.plane.normal[ 2 ] >= 0.9f && !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
					ps->pmove.pm_flags |= PMF_TIME_TRICK_JUMP;
					ps->pmove.pm_time = 64;
				}

				// [Paril-KEX] calculate impact delta; this also fixes triple jumping
				Vector3 clipped_velocity = QM_Vector3Zero();
				PM_ClipVelocity( pml.velocity, pm->ground.plane.normal, clipped_velocity, 1.01f );

				pm->impact_delta = pml.startVelocity.z - clipped_velocity.z;

				ps->pmove.pm_flags |= PMF_ON_GROUND;

				if ( ( ps->pmove.pm_flags & PMF_DUCKED ) ) {
					ps->pmove.pm_flags |= PMF_TIME_LAND;
					ps->pmove.pm_time = 128;
				}
			}
		}

		PM_RegisterTouchTrace( pm->touchTraces, trace );
	}

	// Get liquid.level, accounting for ducking.
	PM_GetWaterLevel( pml.origin, pm->liquid.level, pm->liquid.type );
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
*	@brief	Determine the vieworg's brush contents, apply Blend effect based on results.
*			Also determines whether the view is underwater and to render it as such.
**/
static void PM_ScreenEffects() {
	// Add for contents
	Vector3 vieworg = {
		pml.origin.x + ps->viewoffset.x,
		pml.origin.y + ps->viewoffset.y,
		pml.origin.z + ps->viewoffset.z + (float)ps->pmove.viewheight
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
	if ( !( contents & CONTENTS_PLAYER )
		&& ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) ) ) {
		SG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, &pm->screen_blend.x );
	} else if ( contents & CONTENTS_SLIME ) {
		SG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, &pm->screen_blend.x );
	} else if ( contents & CONTENTS_WATER ) {
		SG_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, &pm->screen_blend.x );
	}
}



/**
*
*
*	Checks for: Ducking, (Trick-)Jumping, Out of Water Jumping, and Ladder movement.
*
*
**/
/**
*	@brief
**/
static void PM_CheckSpecialMovement() {
	// Having time means we're already doing some form of 'special' movement.
	if ( ps->pmove.pm_time ) {
		return;
	}

	// Remove ladder flag.
	ps->pmove.pm_flags &= ~PMF_ON_LADDER;

	// Re-Check for a ladder.
	Vector3 flatforward = QM_Vector3Normalize( {
		pml.forward.x,
		pml.forward.y,
		0.f
		} );
	const Vector3 spot = pml.origin + ( flatforward * 1 );
	trace_t trace = PM_Trace( pml.origin, pm->mins, pm->maxs, spot, (contents_t)( CONTENTS_LADDER ) );
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) && pm->liquid.level < liquid_level_t::LIQUID_WAIST ) {
		ps->pmove.pm_flags |= PMF_ON_LADDER;
	}

	// Don't do any 'special movement' if we're not having any gravity.
	if ( !ps->pmove.gravity ) {
		return;
	}

	// check for water jump
	// [Paril-KEX] don't try waterjump if we're moving against where we'll hop
	if ( !( pm->cmd.buttons & BUTTON_JUMP ) && pm->cmd.forwardmove <= 0 ) {
		return;
	}

	if ( pm->liquid.level != liquid_level_t::LIQUID_WAIST ) {
		return;
	}
	// [Paril-KEX]
	else if ( pm->liquid.type & CONTENTS_NO_WATERJUMP ) {
		return;
	}

	// Quick check that something is even blocking us forward
	Vector3 blockTraceEnd = pml.origin + flatforward * 40.f;
	trace = PM_Trace( pml.origin, pm->mins, pm->maxs, blockTraceEnd, MASK_SOLID );

	// we aren't blocked, or what we're blocked by is something we can walk up
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] >= PM_MIN_STEP_NORMAL ) {
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

	for ( size_t i = 0; i < min( 50, (int32_t)( 10 * ( 800.f / ps->pmove.gravity ) ) ); i++ ) {
		waterjump_vel.z -= ps->pmove.gravity * time;

		if ( waterjump_vel.z < 0 ) {
			has_time = false;
		}

		PM_StepSlideMove_Generic( waterjump_origin, waterjump_vel, time, pm->mins, pm->maxs, touches, has_time );
	}

	// snap down to ground
	Vector3 snapTraceEnd = waterjump_origin + Vector3{ 0.f, 0.f, -2.f };
	trace = PM_Trace( waterjump_origin, pm->mins, pm->maxs, snapTraceEnd, MASK_SOLID );

	// Can't stand here.
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] < PM_MIN_STEP_NORMAL || trace.endpos[ 2 ] < pml.origin.z ) {
		return;
	}

	// We're currently standing on ground, and the snapped position is a step.
	if ( pm->ground.entity && fabsf( pml.origin.z - trace.endpos[ 2 ] ) <= PM_MAX_STEP_SIZE ) {
		return;
	}

	// Get water level.
	liquid_level_t level;
	contents_t type;
	PM_GetWaterLevel( trace.endpos, level, type );

	// The water jump spot will be under water, so we're probably hitting something weird that isn't important
	if ( level >= liquid_level_t::LIQUID_WAIST ) {
		return;
	}

	// Valid waterjump! Set velocity to Jump out of water
	pml.velocity = flatforward * 50;
	pml.velocity.z = 350;
	// Engage PMF_TIME_WATERJUMP timer mode.
	ps->pmove.pm_flags |= PMF_TIME_WATERJUMP;
	ps->pmove.pm_time = 2048;
}
/**
*	@brief
**/
static void PM_CheckJump() {
	// Hasn't been long enough since landing to jump again.
	if ( ps->pmove.pm_flags & PMF_TIME_LAND ) {
		return;
	}

	// Can't jump while ducked.
	if ( ps->pmove.pm_flags & PMF_DUCKED ) {
		return;
	}

	// Player has let go of jump button.
	if ( !( pm->cmd.buttons & BUTTON_JUMP ) ) {
		ps->pmove.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// Player must wait for jump to be released.
	if ( ps->pmove.pm_flags & PMF_JUMP_HELD ) {
		return;
	}

	// Can't jump while dead.
	if ( ps->pmove.pm_type == PM_DEAD ) {
		return;
	}

	// We're swimming, not jumping, so unset ground entity.
	if ( pm->liquid.level >= liquid_level_t::LIQUID_WAIST ) {
		// Unset ground.
		pm->ground.entity = nullptr;
		return;
	}

	// In-air/liquid, so no effect, can't re-jump without ground.
	if ( pm->ground.entity == nullptr ) {
		return;
	}

	// Adjust our pmove state to engage in the act of jumping.
	ps->pmove.pm_flags |= PMF_JUMP_HELD;
	pm->jump_sound = true;
	pm->ground.entity = nullptr;
	// Unset ground.
//PM_UpdateGroundFromTrace( nullptr );
	ps->pmove.pm_flags &= ~PMF_ON_GROUND;

	const float jump_height = pmp->pm_jump_height;

	pml.velocity.z += jump_height;
	if ( pml.velocity.z < jump_height ) {
		pml.velocity.z = jump_height;
	}
}
/**
*	@brief	Decide whether to duck, and/or unduck.
**/
static inline const bool PM_CheckDuck() {
	// Can't duck if gibbed.
	if ( ps->pmove.pm_type == PM_GIB /*|| ps->pmove.pm_type == PM_DEAD*/ ) {
		return false;
	}

	trace_t trace;
	bool flags_changed = false;

	// Dead:
	if ( ps->pmove.pm_type == PM_DEAD ) {
		// TODO: This makes no sense, since the actual check in SetDimensions
		// does the same for PM_DEAD as it does for being DUCKED.
		if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
			ps->pmove.pm_flags |= PMF_DUCKED;
			flags_changed = true;
		}
	// Duck:
	} else if (
		( pm->cmd.buttons & BUTTON_CROUCH ) &&
		( pm->ground.entity || ( pm->liquid.level <= liquid_level_t::LIQUID_FEET && !PM_AboveWater() ) ) &&
		!( ps->pmove.pm_flags & PMF_ON_LADDER ) ) { 
		if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
			// check that duck won't be blocked
			Vector3 check_maxs = { pm->maxs.x, pm->maxs.y, 4 };
			trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin );
			if ( !trace.allsolid ) {
				ps->pmove.pm_flags |= PMF_DUCKED;
				flags_changed = true;
			}
		}
	// Try and get out of the ducked state, stand up, if possible.
	} else {
		if ( ps->pmove.pm_flags & PMF_DUCKED ) {
			// try to stand up
			Vector3 check_maxs = { pm->maxs.x, pm->maxs.y, 32 };
			trace = PM_Trace( pml.origin, pm->mins, check_maxs, pml.origin );
			if ( !trace.allsolid ) {
				ps->pmove.pm_flags &= ~PMF_DUCKED;
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
*
*
*	Position (Initial-)Snapping and Validation:
*
*
**/
/**
*	@brief	True if we're standing in a legitimate non solid position.
**/
static inline const bool PM_GoodPosition() {
	// Position is always valid if no-clipping.
	if ( ps->pmove.pm_type == PM_NOCLIP ) {
		return true;
	}
	// Perform the solid trace.
	const trace_t trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, ps->pmove.origin );
	return !trace.allsolid;
}
/**
*	@brief	On exit, the origin will have a value that is pre-quantized to the PMove
*			precision of the network channel and in a valid position.
**/
static void PM_SnapPosition() {
	ps->pmove.velocity = pml.velocity;
	ps->pmove.origin = pml.origin;
	if ( PM_GoodPosition() ) {
		return;
	}
	//if ( G_FixStuckObject_Generic( ps->pmove.origin, pm->mins, pm->maxs  ) == stuck_result_t::NO_GOOD_POSITION ) {
	//	ps->pmove.origin = pml.previousOrigin;
	//	return;
	//}
}
/**
*	@brief	Try and find, then snap, to a valid non solid position.
**/
static void PM_InitialSnapPosition() {
	constexpr int32_t offset[ 3 ] = { 0, -1, 1 };
	const Vector3 base = ps->pmove.origin;
	
	for ( int32_t z = 0; z < 3; z++ ) {
		ps->pmove.origin.z = base.z + offset[ z ];
		for ( int32_t y = 0; y < 3; y++ ) {
			ps->pmove.origin.y = base.y + offset[ y ];
			for ( int32_t x = 0; x < 3; x++ ) {
				ps->pmove.origin.x = base.x + offset[ x ];
				if ( PM_GoodPosition() ) {
					pml.origin = ps->pmove.origin;
					pml.previousOrigin = ps->pmove.origin;
					return;
				}
			}
		}
	}
}



/**
*
*
*	Core Logic and Entry Points:
*
*
**/
/**
*	@brief	Clamp view angles within range (0, 360).
**/
static void PM_UpdateViewAngles( player_state_t *playerState, const usercmd_t *userCommand ) {
	if ( ps->pmove.pm_type == PM_INTERMISSION || ps->pmove.pm_type == PM_SPINTERMISSION ) {
		return;		// no view changes at all
	}
	//if ( ps->pm_type != PM_SPECTATOR && ps->stats[ STAT_HEALTH ] <= 0 ) {
	//	return;		// no view changes at all
	//}

	if ( playerState->pmove.pm_flags & PMF_TIME_TELEPORT ) {
		playerState->viewangles[ YAW ] = AngleMod( userCommand->angles[ YAW ] + playerState->pmove.delta_angles[ YAW ] );
		playerState->viewangles[ PITCH ] = 0;
		playerState->viewangles[ ROLL ] = 0;
	} else {
		// Circularly clamp the angles with deltas,
		playerState->viewangles = QM_Vector3AngleMod( userCommand->angles + playerState->pmove.delta_angles );

		// Don't let the player look up or down more than 90 degrees.
		#ifdef PM_CLAMP_VIEWANGLES_0_TO_360
		if ( playerState->viewangles[ PITCH ] >= 90 && playerState->viewangles[ PITCH ] <= 180 ) {
			playerState->viewangles[ PITCH ] = 90;
		} else if ( playerState->viewangles[ PITCH ] <= 270 && playerState->viewangles[ PITCH ] >= 180 ) {
			playerState->viewangles[ PITCH ] = 270;
		}
		#else
		if ( playerState->viewangles[ PITCH ] > 90 && playerState->viewangles[ PITCH ] < 270 ) {
			playerState->viewangles[ PITCH ] = 90;
		} else if ( playerState->viewangles[ PITCH ] <= 360 && playerState->viewangles[ PITCH ] >= 270 ) {
			playerState->viewangles[ PITCH ] -= 360;
		}
		#endif
	}

	// Calculate angle vectors derived from current viewing angles.
	QM_AngleVectors( playerState->viewangles, &pml.forward, &pml.right, &pml.up );
}
/**
*	@brief	Used by >= PM_DEAD movetypes, ensure user input is cleared so it has no further influence.
**/
static void PM_EraseInputCommandState() {
	// Idle our directional user input.
	pm->cmd.forwardmove = 0;
	pm->cmd.sidemove = 0;
	pm->cmd.upmove = 0;
	// Unset these two buttons so other code won't respond to it.
	pm->cmd.buttons &= ~( BUTTON_JUMP | BUTTON_CROUCH );
}
/**
*
**/
static void PM_DropTimers() {
	if ( ps->pmove.pm_time ) {
		const int32_t msec = pm->cmd.msec;

		if ( msec >= ps->pmove.pm_time ) {
			// Somehow need this, Q2RE does not. If we don't do so, the code piece in this comment that resides above in PM_CategorizePosition
			// causes us to remain unable to jump.
			ps->pmove.pm_flags &= ~PMF_ALL_TIMES;
			ps->pmove.pm_time = 0;
		} else {
			ps->pmove.pm_time -= msec;
		}
	}
}
/**
*	@brief	Can be called by either the server or the client game codes.
**/
void SG_PlayerMove( pmove_t *pmove, pmoveParams_t *params ) {
	// Store pointers to the pmove object and the parameters supplied for this move.
	pm = pmove;
	ps = pm->playerState;
	pmp = params;

	// Clear out several member variables which require a fresh state before performing the move.
	pm->touchTraces = {};
	//pm->playerState.viewangles = {};
	//ps->pmove.viewheight = 0;
	pm->ground = {};
	pm->liquid = {
		.type = CONTENTS_NONE,
		.level = liquid_level_t::LIQUID_NONE
	},
	pm->screen_blend = {};
	pm->rdflags = 0;
	pm->jump_sound = false;
	pm->step_clip = false;
	pm->step_height = 0;
	pm->impact_delta = 0;

	// Clear all pmove local vars
	pml = {};

	// Store the origin and velocity in pmove local.
	pml.origin = ps->pmove.origin;
	pml.velocity = ps->pmove.velocity;
	// Save the start velocity.
	pml.startVelocity = ps->pmove.velocity;
	// Save the origin as 'old origin' for in case we get stuck.
	pml.previousOrigin = ps->pmove.origin;

	// Calculate frameTime.
	pml.frameTime = pm->cmd.msec * 0.001f;

	// Clamp view angles.
	PM_UpdateViewAngles( ps, &pm->cmd );

	/**
	*	PM_SPECTATOR/PM_NOCLIP:
	**/
	// Performs fly move, only clips in case of spectator mode, noclips otherwise.
	if ( ps->pmove.pm_type == PM_SPECTATOR || ps->pmove.pm_type == PM_NOCLIP ) {
		// Re-ensure no flags are set anymore.
		ps->pmove.pm_flags = PMF_NONE;

		// Give the spectator a small 8x8x8 bounding box.
		if ( ps->pmove.pm_type == PM_SPECTATOR ) {
			pm->mins.x = -8;
			pm->mins.y = -8;
			pm->mins.z = -8;
			pm->maxs.x = 8;
			pm->maxs.y = 8;
			pm->maxs.z = 8;
		}

		// Get moving.
		PM_FlyMove( ps->pmove.pm_type == PM_SPECTATOR );
		// Snap to position.
		PM_SnapPosition( );
		return;
	}

	/**
	*	PM_FREEZE:
	**/
	if ( ps->pmove.pm_type == PM_FREEZE ) {
		return;     // no movement at all
	}
	/**
	*	PM_FREEZE:
	**/
	if ( ps->pmove.pm_type == PM_INTERMISSION || ps->pmove.pm_type == PM_SPINTERMISSION ) {
		return;		// no movement at all
	}

	/**
	*	PM_DEAD:
	**/
	// Erase all input command state when dead, we don't want to allow moving our dead body.
	if ( ps->pmove.pm_type >= PM_DEAD ) {
		PM_EraseInputCommandState();
	}

	// Set mins, maxs, and viewheight.
	PM_SetDimensions();
	// General position categorize.
	PM_CategorizePosition();
	// If pmove values were changed outside of the pmove code, resnap to position first before continuing.
	if ( pm->snapinitial ) {
		PM_InitialSnapPosition();
	}
	// Recategorize if we're ducked. ( Set groundentity, liquid.type, and liquid.level. )
	if ( PM_CheckDuck() ) {
		PM_CategorizePosition();
	}
	// When dead, perform dead movement.
	if ( ps->pmove.pm_type == PM_DEAD ) {
		PM_DeadMove();
	}
	// Performs the 'on-ladder' check as well as when to engage into the out of Water Jump movement.
	PM_CheckSpecialMovement();

	/**
	*	Now all that is settled, start dropping the input command timing counter(s).
	**/
	PM_DropTimers();

	// Do Nothing ( Teleport pause stays exactly in place ):
	if ( ps->pmove.pm_flags & PMF_TIME_TELEPORT ) {
		// ...
	// WaterJump Move ( Has no control, but falls by gravity influences ):
	} else if ( ps->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		PM_WaterJumpMove();
	// Generic Move & Water Move:
	} else {
		// Check for jumping.
		PM_CheckJump( );
		// Apply friction. ( If on-ground. )
		PM_Friction( );

		// Determine water level and pursue to WaterMove if deep in above waist.
		if ( pm->liquid.level >= liquid_level_t::LIQUID_WAIST ) {
			PM_WaterMove( );
		// Otherwise default to generic move code.
		} else {
			// Different pitch handling.
			Vector3 angles = ps->viewangles;
			if ( angles[ PITCH ] > 180 ) {
				angles[ PITCH ] = angles[ PITCH ] - 360;
			}
			angles[ PITCH ] /= 3;

			// Recalculate angle vectors.
			QM_AngleVectors( angles, &pml.forward, &pml.right, &pml.up );
			
			// Regular 'generic move'. Determines whether on-ladder, on-ground, or in-air, and
			// performs the move to that accordingly.
			PM_GenericMove( );
		}
	}

	// Recategorize position for contents, ground, and/or liquid since we've made a move.
	PM_CategorizePosition();
	// Determine whether we can pull a trick jump, and if so, perform the jump.
	if ( ps->pmove.pm_flags & PMF_TIME_TRICK_JUMP ) {
		PM_CheckJump();
	}

	// Apply contents and other screen effects.
	PM_ScreenEffects();

	// Weapons.
	//PM_Weapon();
	// Torso animation.
	//PM_TorsoAnimation();
	// Bob Cycle / Footstep events / legs animations.
	//PM_Footsteps();
	PM_CycleBob();
	//// Entering / Leaving water splashes.
	//PM_WaterEvents();

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

	pmp->pm_crouch_move_speed = default_pmoveParams_t::pm_crouch_move_speed;
	pmp->pm_water_speed = default_pmoveParams_t::pm_water_speed;
	pmp->pm_fly_speed = default_pmoveParams_t::pm_fly_speed;

	pmp->pm_accelerate = default_pmoveParams_t::pm_accelerate;
	pmp->pm_water_accelerate = default_pmoveParams_t::pm_water_accelerate;

	pmp->pm_friction = default_pmoveParams_t::pm_friction;
	pmp->pm_water_friction = default_pmoveParams_t::pm_water_friction;
}

