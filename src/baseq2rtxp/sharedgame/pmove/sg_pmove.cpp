/********************************************************************
*
*
*	SharedGame: Player Movement
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_misc.h"
#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/pmove/sg_pmove_slidemove.h"

// TODO: FIX CLAMP BEING NAMED CLAMP... preventing std::clamp
#undef clamp

//! Defining this will keep the player's viewangles always >= 0 && < 360.
//! Also affects the pitch angle for ladders, since it is then determined differently.
//#define PM_CLAMP_VIEWANGLES_0_TO_360

//! Defining this will enable Q2 Rerelease style fixing of being stuck.
//#define PMOVE_Q2RE_STUCK_FIXES

//! Defining this will enable 'material physics' such as per example: Taking the friction value from the ground material.
#define PMOVE_USE_MATERIAL_FRICTION

//! Defining this will enable easing of bbox maxs z and viewheight.
//#define PMOVE_EASE_BBOX_AND_VIEWHEIGHT

//! Allow for 'Wall Jumping'. ( Disabled by default. )
//#define PMOVE_ENABLE_WALLJUMPING

//! Defining this allows for HL like acceleration. Where friction is applied to the acceleration formula.
//#define HL_LIKE_ACCELERATION

//! Stop Epsilon for switching bob cycle into idle..
static constexpr double PM_BOB_CYCLE_IDLE_EPSILON = 0.1; // WID: Old was 0.1

//! Stop Epsilon for toggling animation idle mode.
static constexpr double PM_ANIMATION_IDLE_EPSILON = 0.25; // WID: Old was 0.1


//! An actual pointer to the pmove object that we're moving.
pmove_t *pm;
//! Contains our local in-moment move variables.
pml_t pml;

//! An actual pointer to the pmove parameters object for use with moving.
static pmoveParams_t *pmp;



/**
*
*
*
*	PM Clip/Trace:
*
*
*
**/
/**
*	@brief	Clips trace against world only.
**/
const cm_trace_t PM_Clip( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const cm_contents_t contentMask ) {
	return pm->clip( &start.x, &mins.x, &maxs.x, &end.x, contentMask );
}
/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
const cm_trace_t PM_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, cm_contents_t contentMask ) {
	// Spectators only clip against world, so use clip instead.
	if ( pm->state->pmove.pm_type == PM_SPECTATOR ) {
		return PM_Clip( start, mins, maxs, end, CM_CONTENTMASK_SOLID );
	}

	if ( contentMask == CONTENTS_NONE ) {
		if ( pm->state->pmove.pm_type == PM_DEAD || pm->state->pmove.pm_type == PM_GIB ) {
			contentMask = CM_CONTENTMASK_DEADSOLID;
		} else if ( pm->state->pmove.pm_type == PM_SPECTATOR ) {
			contentMask = CM_CONTENTMASK_SOLID;
		} else {
			contentMask = CM_CONTENTMASK_PLAYERSOLID;
		}

		//if ( pm->s.pm_flags & PMF_IGNORE_PLAYER_COLLISION )
		//	mask &= ~CONTENTS_PLAYER;
	}

	return pm->trace( &start.x, &mins.x, &maxs.x, &end.x, pm->playerEdict, contentMask );
}
/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
const cm_trace_t PM_TraceCorrectSolid( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, cm_contents_t contentMask ) {
	// Spectators only clip against world, so use clip instead.
	if ( pm->state->pmove.pm_type == PM_SPECTATOR ) {
		return PM_Clip( start, mins, maxs, end, CM_CONTENTMASK_SOLID );
	}

	if ( contentMask == CONTENTS_NONE ) {
		if ( pm->state->pmove.pm_type == PM_DEAD || pm->state->pmove.pm_type == PM_GIB ) {
			contentMask = CM_CONTENTMASK_DEADSOLID;
		} else if ( pm->state->pmove.pm_type == PM_SPECTATOR ) {
			contentMask = CM_CONTENTMASK_SOLID;
		} else {
			contentMask = CM_CONTENTMASK_PLAYERSOLID;
		}

		//if ( pm->s.pm_flags & PMF_IGNORE_PLAYER_COLLISION )
		//	mask &= ~CONTENTS_PLAYER;
	}

	const vec3_t offsets = { 0.f, 1.f, -1.f };

	// Jitter around
	for ( int32_t i = -1; i < 1; i++ ) {
		for ( int32_t j = -1; j < 1; j++ ) {
			for ( int32_t k = -1; k < 1; k++ ) {
				// Calculate start.
				const Vector3 point = start + vec3_t{
					(float)i,
					(float)j,
					(float)k//offsets[ i ],
					//offsets[ j ],
					//offsets[ k ]
				};

				cm_trace_t trace = pm->trace( &point.x, &mins.x, &maxs.x, &end.x, pm->playerEdict, contentMask );
				if ( !trace.startsolid ) {
					return trace;
				}
			}
		}
	}
	return pm->trace( &start.x, &mins.x, &maxs.x, &end.x, pm->playerEdict, contentMask );
}



/**
*
*
*
*	Misc
*
*
*
**/
/**
*	@brief	Inline-wrapper to for convenience.
**/
void PM_AddEvent( const uint8_t newEvent, const uint8_t parameter ) {
	SG_PlayerState_AddPredictableEvent( newEvent, parameter, pm->state );
}



/**
*
*
* 
*	Categorization of liquid/ground information:
*
*
* 
**/
/**
*	@brief	Determine the vieworg's brush contents, apply Blend effect based on results.
*			Also determines whether the view is underwater and to render it as such.
**/
static void PM_UpdateScreenContents() {
	// Add for contents
	Vector3 vieworg = {
		pm->state->pmove.origin.x + pm->state->viewoffset.x,
		pm->state->pmove.origin.y + pm->state->viewoffset.y,
		pm->state->pmove.origin.z + pm->state->viewoffset.z + (float)pm->state->pmove.viewheight
	};
	const cm_contents_t contents = pm->pointcontents( &vieworg.x );//cm_contents_t contents = pm->pointcontents( vieworg );

	// Under a 'water' like solid:
	if ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) {
		pm->state->rdflags |= RDF_UNDERWATER;
		// Or not:
	} else {
		pm->state->rdflags &= ~RDF_UNDERWATER;
	}

	// Prevent it from adding screenblend if we're inside a client entity, by checking
	// if its brush has CONTENTS_PLAYER set in its clipmask.
	if ( !( contents & CONTENTS_PLAYER )
		&& ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) ) ) {
		SG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, pm->state->screen_blend );
	} else if ( contents & CONTENTS_SLIME ) {
		SG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, pm->state->screen_blend );
	} else if ( contents & CONTENTS_WATER ) {
		SG_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, pm->state->screen_blend );
	}
}
/**
*	@brief	Will fill in the cm_liquid_level_t and cm_contents_t references with the data 
*			found at the specified position.
**/
static inline void PM_GetWaterLevel( const Vector3 &position, cm_liquid_level_t &level, cm_contents_t &type ) {
	//
	// get liquid.level, accounting for ducking
	//
	level = cm_liquid_level_t::LIQUID_NONE;
	type = CONTENTS_NONE;

	int32_t sample2 = (int)( pm->state->pmove.viewheight - pm->mins.z );
	int32_t sample1 = sample2 / 2;

	Vector3 point = position;

	point.z += pm->mins.z + 1;

	cm_contents_t contentType = pm->pointcontents( &point.x );

	if ( contentType & CM_CONTENTMASK_WATER ) {
		sample2 = pm->state->pmove.viewheight - pm->mins.z;
		sample1 = sample2 / 2;

		type = contentType;
		level = cm_liquid_level_t::LIQUID_FEET;
		point.z = pm->state->pmove.origin.z + pm->mins.z + sample1;
		contentType = pm->pointcontents( &point.x );
		if ( contentType & CM_CONTENTMASK_WATER ) {
			level = cm_liquid_level_t::LIQUID_WAIST;
			point.z = pm->state->pmove.origin.z + pm->mins.z + sample2;
			contentType = pm->pointcontents( &point.x );
			if ( contentType & CM_CONTENTMASK_WATER ) {
				level = cm_liquid_level_t::LIQUID_UNDER;
			}
		}
	}
}
/**
*	@brief	Determine whether we're above water, or not.
**/
static inline const bool PM_AboveWater() {
	// Testing point.
	const Vector3 below = pm->state->pmove.origin + Vector3{ 0, 0, -8 };
	// We got solid below, not water:
	bool solid_below = pm->trace( &pm->state->pmove.origin.x, &pm->mins.x, &pm->maxs.x, &below.x, pm->playerEdict, CM_CONTENTMASK_SOLID ).fraction < 1.0f;
	if ( solid_below ) {
		return false;
	}

	// We're above water:
	bool water_below = pm->trace( &pm->state->pmove.origin.x, &pm->mins.x, &pm->maxs.x, &below.x, pm->playerEdict, CM_CONTENTMASK_WATER ).fraction < 1.0f;
	if ( water_below ) {
		return true;
	}

	// No water below.
	return false;
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
	const float currentspeed = QM_Vector3DotProduct( pm->state->pmove.velocity, wishDirection );
	const float addSpeed = wishSpeed - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	// HL like acceleration.
	#ifdef HL_LIKE_ACCELERATION
	float drop = 0;
	float friction = 1;
	if ( ( pm->ground.entity != nullptr
		&& ( pml.groundTrace.surface != nullptr
			&& !( pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) ) )
		|| ( pm->state->pmove.pm_flags & PMF_ON_LADDER )
		// Get the material to fetch friction from.
		cm_material_t * ground_material = ( pml.groundTrace.surface != nullptr ? pml.groundTrace.surface->material : nullptr );
		friction = ( ground_material ? ground_material->physical.friction : 1 );
		//const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		//drop += control * friction * pml.frameTime;
}
float accelerationSpeed = acceleration * pml.frameTime * wishSpeed * friction;
if ( accelerationSpeed > addSpeed ) {
	accelerationSpeed = addSpeed;
}
#endif

float accelerationSpeed = acceleration * pml.frameTime * wishSpeed;
if ( accelerationSpeed > addSpeed ) {
	accelerationSpeed = addSpeed;
}

pm->state->pmove.velocity = QM_Vector3MultiplyAdd( pm->state->pmove.velocity, accelerationSpeed, wishDirection );
}
/**
*	@brief	Handles user intended 'mid-air' acceleration.
**/
static void PM_AirAccelerate( const Vector3 &wishDirection, const float wishSpeed, const float acceleration ) {
	float wishspd = wishSpeed;
	if ( wishspd > 30 ) {
		wishspd = 30;
	}

	const float currentspeed = QM_Vector3DotProduct( pm->state->pmove.velocity, wishDirection );
	const float addSpeed = wishspd - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	float accelerationSpeed = acceleration * wishSpeed * pml.frameTime;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	pm->state->pmove.velocity = QM_Vector3MultiplyAdd( pm->state->pmove.velocity, accelerationSpeed, wishDirection );
}
/**
*	@brief	Handles both ground friction and water friction
**/
static void PM_Friction() {
	// Set us to a halt, if our speed is too low, otherwise we'll see
	// ourselves sort of 'drifting'.
	const float speed = QM_Vector3Length( pm->state->pmove.velocity );//sqrtf( pm->state->pmove.velocity.x * pm->state->pmove.velocity.x + pm->state->pmove.velocity.y * pm->state->pmove.velocity.y + pm->state->pmove.velocity.z * pm->state->pmove.velocity.z );
	if ( speed < 1 ) {
		pm->state->pmove.velocity.x = 0;
		pm->state->pmove.velocity.y = 0;
		return;
	}

	// Apply ground friction if on-ground.
	#ifdef PMOVE_USE_MATERIAL_FRICTION
	float drop = 0;
	if ( ( pm->ground.entity != nullptr
		&& ( pml.groundTrace.surface != nullptr
			&& !( pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) ) )
		|| ( pm->state->pmove.pm_flags & PMF_ON_LADDER )
		) {
		// Get the material to fetch friction from.
		cm_material_t *ground_material = ( pml.groundTrace.surface != nullptr ? pml.groundTrace.surface->material : nullptr );
		float friction = ( ground_material ? ground_material->physical.friction : pmp->pm_friction );
		const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		drop += control * friction * pml.frameTime;
	}
	#else
		// Apply ground friction if on-ground.
	float drop = 0;
	if ( ( pm->ground.entity && pml.groundTrace.surface && !( pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) ) || ( pm->state->pmove.pm_flags & PMF_ON_LADDER ) ) {
		const float friction = pmp->pm_friction;
		const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		drop += control * friction * pml.frameTime;
	}
	#endif
	// Apply water friction, and not off-ground yet on a ladder.
	if ( pm->liquid.level && !( pm->state->pmove.pm_flags & PMF_ON_LADDER ) ) {
		drop += speed * pmp->pm_water_friction * (float)pm->liquid.level * pml.frameTime;
	}

	// Scale the velocity.
	float newspeed = speed - drop;
	if ( newspeed <= 0 ) {
		newspeed = 0;
		pm->state->pmove.velocity = {};
	} else {
		newspeed /= speed;
		pm->state->pmove.velocity *= newspeed;
	}
}
/**
*	@brief	Handles the velocities for 'ladders', as well as water and conveyor belt by applying their 'currents'.
**/
static void PM_AddCurrents( Vector3 &wishVelocity ) {
	// Account for ladders.
	if ( pm->state->pmove.pm_flags & PMF_ON_LADDER ) {
		if ( pm->cmd.buttons & ( BUTTON_JUMP | BUTTON_CROUCH ) ) {
			// [Paril-KEX]: if we're underwater, use full speed on ladders
			const float ladder_speed = pm->liquid.level >= cm_liquid_level_t::LIQUID_WAIST ? pmp->pm_max_speed : pmp->pm_ladder_speed;
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
				if ( pm->state->viewangles[ PITCH ] >= 271 && pm->state->viewangles[ PITCH ] < 345 ) {
					wishVelocity.z = ladder_speed;
				} else if ( pm->state->viewangles[ PITCH ] < 271 && pm->state->viewangles[ PITCH ] >= 15 ) {
					wishVelocity.z = -ladder_speed;
				}
				#else
				if ( pm->state->viewangles[ PITCH ] < 15 ) {
					wishVelocity.z = ladder_speed;
				} else if ( pm->state->viewangles[ PITCH ] < 271 && pm->state->viewangles[ PITCH ] >= 15 ) {
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

				if ( pm->liquid.level < cm_liquid_level_t::LIQUID_WAIST ) {
					ladder_speed *= pmp->pm_ladder_mod;
				}

				// Check for ladder.
				Vector3 flatforward = QM_Vector3Normalize( {
					pml.forward.x,
					pml.forward.y,
					0.f
					} );
				Vector3 spot = pm->state->pmove.origin + ( flatforward * 1 );
				cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );

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
	if ( pm->liquid.type & CM_CONTENTMASK_CURRENT ) {
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
		if ( ( pm->liquid.level == cm_liquid_level_t::LIQUID_FEET ) && ( pm->ground.entity ) ) {
			speed /= 2;
		}

		wishVelocity = QM_Vector3MultiplyAdd( wishVelocity, speed, velocity );
	}

	// Add conveyor belt velocities.
	if ( pm->ground.entity ) {
		Vector3 velocity = QM_Vector3Zero();

		if ( pml.groundTrace.contents & CONTENTS_CURRENT_0 ) {
			velocity.x += 1;
		}
		if ( pml.groundTrace.contents & CONTENTS_CURRENT_90 ) {
			velocity.y += 1;
		}
		if ( pml.groundTrace.contents & CONTENTS_CURRENT_180 ) {
			velocity.x -= 1;
		}
		if ( pml.groundTrace.contents & CONTENTS_CURRENT_270 ) {
			velocity.y -= 1;
		}
		if ( pml.groundTrace.contents & CONTENTS_CURRENT_UP ) {
			velocity.z += 1;
		}
		if ( pml.groundTrace.contents & CONTENTS_CURRENT_DOWN ) {
			velocity.z -= 1;
		}

		wishVelocity = QM_Vector3MultiplyAdd( wishVelocity, 100.f, velocity );
	}
}


/**
*
*
*
*	Special Movement: Jump and Duck!
*
*
*
**/
/**
*	@brief	Returns the scale factor to apply to user input command movements
*			This allows the clients to use axial -pmp_max_speed to pmp_max_speed values for all directions
*			without getting a sqrt(2) distortion in speed.
**/
static double PM_CmdScale( const usercmd_t *cmd ) {
	double max = fabs( cmd->forwardmove );
	const double fabsSideMove = std::fabs( cmd->sidemove );
	if ( fabsSideMove > max ) {
		max = fabsSideMove;
	}
	const double fabsUpMove = std::fabs( cmd->upmove );
	if ( fabsUpMove > max ) {
		max = fabsUpMove;
	}

	double scale = 0.;
	if ( max ) {
		const double total = std::sqrt( cmd->forwardmove * cmd->forwardmove
			+ cmd->sidemove * cmd->sidemove + cmd->upmove * cmd->upmove );
		scale = pm->state->pmove.speed * max / ( /* 127 */ pmp->pm_max_speed * total );
	}

	return scale;
}

/**
*	@brief	Sets the player's movement bounding box depending on various state factors.
**/
static void PM_SetDimensions() {
	// Start with the default bbox for standing straight up.
	pm->mins = PM_BBOX_STANDUP_MINS;
	pm->maxs = PM_BBOX_STANDUP_MAXS;
	pm->state->pmove.viewheight = PM_VIEWHEIGHT_STANDUP;

	// Specifical gib treatment.
	if ( pm->state->pmove.pm_type == PM_GIB ) {
		pm->mins = PM_BBOX_GIBBED_MINS;
		pm->maxs = PM_BBOX_GIBBED_MAXS;
		pm->state->pmove.viewheight = PM_VIEWHEIGHT_GIBBED;
		return;
	}

	// Dead BBox:
	if ( ( pm->state->pmove.pm_flags & PMF_DUCKED ) || pm->state->pmove.pm_type == PM_DEAD ) {
		//! Set bounds to ducked.
		pm->mins = PM_BBOX_DUCKED_MINS;
		pm->maxs = PM_BBOX_DUCKED_MAXS;
		// Viewheight to ducked.
		pm->state->pmove.viewheight = PM_VIEWHEIGHT_DUCKED;
	}
}
/**
*	@brief	Decide whether to duck, and/or unduck.
**/
static inline const bool PM_CheckDuck() {
	// Can't duck if gibbed.
	if ( pm->state->pmove.pm_type == PM_GIB /*|| pm->state->pmove.pm_type == PM_DEAD*/ ) {
		return false;
	}

	cm_trace_t trace;
	bool flags_changed = false;

	// Dead:
	if ( pm->state->pmove.pm_type == PM_DEAD ) {
		// TODO: This makes no sense, since the actual check in SetDimensions
		// does the same for PM_DEAD as it does for being DUCKED.
		if ( !( pm->state->pmove.pm_flags & PMF_DUCKED ) ) {
			pm->state->pmove.pm_flags |= PMF_DUCKED;
			flags_changed = true;
		}
		// Duck:
	} else if (
		( pm->cmd.buttons & BUTTON_CROUCH ) &&
		( /*pm->ground.entity*/ pml.isWalking || ( pm->liquid.level <= cm_liquid_level_t::LIQUID_FEET && !PM_AboveWater() ) ) &&
		!( pm->state->pmove.pm_flags & PMF_ON_LADDER ) ) {
		if ( !( pm->state->pmove.pm_flags & PMF_DUCKED ) ) {
			// check that duck won't be blocked
			Vector3 check_maxs = PM_BBOX_DUCKED_MAXS;// { pm->maxs.x, pm->maxs.y, PM_BBOX_DUCKED_MAXS.z };
			trace = PM_Trace( pm->state->pmove.origin, pm->mins, check_maxs, pm->state->pmove.origin );
			if ( !trace.allsolid ) {
				pm->state->pmove.pm_flags |= PMF_DUCKED;
				flags_changed = true;
			}
		}
		// Try and get out of the ducked state, stand up, if possible.
	} else {
		if ( pm->state->pmove.pm_flags & PMF_DUCKED ) {
			// try to stand up
			Vector3 check_maxs = PM_BBOX_STANDUP_MAXS;// { pm->maxs.x, pm->maxs.y, PM_BBOX_STANDUP_MAXS.z };
			trace = PM_Trace( pm->state->pmove.origin, pm->mins, check_maxs, pm->state->pmove.origin );
			if ( !trace.allsolid ) {
				pm->state->pmove.pm_flags &= ~PMF_DUCKED;
				flags_changed = true;
			}
		}
	}

	// Escape if nothing has changed.
	if ( !flags_changed ) {
		return false;
	} else {
		//// (Re-)Initialize the duck ease.
		//#ifdef PMOVE_EASE_BBOX_AND_VIEWHEIGHT
		//pm->easeDuckHeight = QMEaseState::new_ease_state( pm->simulationTime, pmp->pm_duck_viewheight_speed );
		//#endif
	}

	// Determine the possible new dimensions since our flags have changed.
	PM_SetDimensions();

	// We're ducked.
	return true;
}
/**
*	@brief	Will engage into the jump movement.
*	@return	True if we engaged into jumping movement.
**/
static const bool PM_CheckJump() {
	// Not holding jump.
	if ( !( pm->cmd.buttons & BUTTON_JUMP ) ) {
		pm->state->pmove.pm_flags &= ~PMF_JUMP_HELD;
		return false;
	}
	// Player must wait for jump to be released.
	if ( pm->state->pmove.pm_flags & PMF_JUMP_HELD ) {
		// <Q2RTXP>: WID: Clear so cmdscale doesn't lower running speed.
		pm->cmd.upmove = 0;
		return false;
	}
	// Hasn't been long enough since landing to jump again.
	if ( pm->state->pmove.pm_flags & PMF_TIME_LAND ) {
		return false;
	}
	// Hasn't been long enough since landing to jump again.
	if ( pm->state->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		return false;
	}
	// Can't jump while ducked.
	if ( pm->state->pmove.pm_flags & PMF_DUCKED ) {
		return false;
	}
	// In-air/liquid, so no effect, can't re-jump without ground.
	if ( pm->ground.entity == nullptr ) {
		return false;
	}

	// Acquire the velocity for jump height.
	const double jump_height = pmp->pm_jump_height;

	// We got none of these.
	pml.hasGroundPlane = false;
	pml.isWalking = false;
	// We are Aerial however.
	pml.isAerial = true;

	// Play jump sound.
	pm->jump_sound = true;
	// Unset ground.
	pm->ground.entity = nullptr;

	// Adjust our pmove state to engage in the act of jumping.
	pm->state->pmove.pm_flags |= PMF_JUMP_HELD;
	pm->state->pmove.velocity.z += jump_height;
	// If still too low, force jump height velocity upwards.
	if ( pm->state->pmove.velocity.z < jump_height ) {
		pm->state->pmove.velocity.z = jump_height;
	}

	// Add 'Predictable' Event for Jumping Up.
	PM_AddEvent( PS_EV_JUMP_UP, 0 );

	return true;
}




/**
*
* 
* 
*	Ground Tracing:
* 
* 
* 
**/
/**
*	@brief	
**/
static const int32_t PM_CorrectAllSolid( cm_trace_t *trace ) {
	int32_t	i, j, k;

	//if ( pm->debugLevel ) {
	//	Com_Printf( "%i:allsolid\n", c_pmove );
	//}
	Vector3 testPoint = {};

	// jitter around
	for ( i = -1; i <= 1; i++ ) {
		for ( j = -1; j <= 1; j++ ) {
			for ( k = -1; k <= 1; k++ ) {
				testPoint = pm->state->pmove.origin;
				testPoint[ 0 ] += (float)i;
				testPoint[ 1 ] += (float)j;
				testPoint[ 2 ] += (float)k;
				*trace = PM_Trace( testPoint, pm->mins, pm->maxs, testPoint );
				if ( !trace->allsolid ) {
					Vector3 down_point = pm->state->pmove.origin - Vector3{ 0.f, 0.f, 0.25f };
					*trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, down_point );
					pml.groundTrace = *trace;
					return true;
				}
			}
		}
	}

	// Not walking or on ground.
	pml.hasGroundPlane = false;
	pml.isWalking = false;
	pml.isAerial = true;
	pm->ground.entity = nullptr;

	return false;
}

/**
*	@brief	The ground trace didn't hit a surface, so we are in freefall
**/
static void PM_GroundTraceMissed( void ) {
	Vector3		point = {};

	if ( pm->ground.entity != nullptr/*pm->ps->groundEntityNum != ENTITYNUM_NONE*/ ) {
		//// we just transitioned into freefall
		//if ( pm->debugLevel ) {
		//	Com_Printf( "%i:lift\n", c_pmove );
		//}
		#if 0
		// if they aren't in a jumping animation and the ground is a ways away, force into it
		// if we didn't do the trace, the player would be backflipping down staircases
		VectorCopy( pm->ps->origin, point );
		point[ 2 ] -= 64;
		trace_t		trace;
		pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );
		if ( trace.fraction == 1.0 ) {
			if ( pm->cmd.forwardmove >= 0 ) {
				PM_ForceLegsAnim( LEGS_JUMP );
				pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
			} else {
				PM_ForceLegsAnim( LEGS_JUMPB );
				pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
			}
		}
		#endif
	}
}
/**
*	@brief	Called before, and after moving, to catagorize which entity/surface/plane we're standing
*			on if any at all.
**/
static void PM_GroundTrace( void ) {
	// Trace downwards to find ground.
	Vector3 point = pm->state->pmove.origin - Vector3{ 0.f, 0.f, PM_STEP_GROUND_DIST };
	cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, point );
	// Assign the ground trace.
	pml.groundTrace = trace;

	// Do something corrective if the trace starts in a solid.
	if ( trace.allsolid ) {
		// Did not find valid ground.
		if ( !PM_CorrectAllSolid( &trace ) ) {
			return;
		}
	}

	// If the trace didn't hit anything, we are in free fall.
	if ( trace.fraction == 1.0 ) {
		PM_GroundTraceMissed();

		pm->ground.entity = nullptr;
		pml.hasGroundPlane = false;
		pml.isWalking = false;
		pml.isAerial = true;
		return;
	}

	// Check if getting thrown off the ground
	if ( pm->state->pmove.velocity.z > 0 && QM_Vector3DotProduct( pm->state->pmove.velocity, trace.plane.normal ) > 10. ) {
		//if ( pm->debugLevel ) {
		//	Com_Printf( "%i:kickoff\n", c_pmove );
		//}
		#if 0
		// go into jump animation
		if ( pm->cmd.forwardmove >= 0 ) {
			PM_ForceLegsAnim( LEGS_JUMP );
			pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
		} else {
			PM_ForceLegsAnim( LEGS_JUMPB );
			pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
		}
		#endif
		pm->ground.entity = nullptr;
		pml.hasGroundPlane = false;
		pml.isWalking = false;
		pml.isAerial = true;
		return;
	}

	// slopes that are too steep and thus will not be considered onground
	if ( trace.plane.normal[ 2 ] < PM_MIN_STEP_NORMAL ) {
		//if ( pm->debugLevel ) {
		//	Com_Printf( "%i:steep\n", c_pmove );
		//}
		
		// FIXME: if they can't slide down the slope, let them
		// walk (sharp crevices)
		pm->ground.entity = nullptr;
		pml.hasGroundPlane = true;
		pml.isWalking = false;
		pml.isAerial = false;
		return;
	}

	pml.hasGroundPlane = true;
	pml.isWalking = true;
	pml.isAerial = false;

	// Hitting solid ground will end a waterjump.
	if ( pm->state->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		pm->state->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND );
		pm->state->pmove.pm_time = 0;
	}

	if ( pm->ground.entity == nullptr ) {
		//// just hit the ground
		//if ( pm->debugLevel ) {
		//	Com_Printf( "%i:Land\n", c_pmove );
		//}

		#if 0
		PM_CrashLand();
		#endif

		//Ddon't do landing time if we were just going down a slope.
		if ( pml.previousVelocity.z < -200. ) {
			// don't allow another jump for a little while
			pm->state->pmove.pm_flags |= PMF_TIME_LAND;
			pm->state->pmove.pm_time = 8;
		}
	}

	// If we have an entity, or a ground plane, assign the remaining ground properties.
	if ( pml.hasGroundPlane || pm->ground.entity ) {
		pm->ground.plane = pml.groundTrace.plane;
		pm->ground.contents = pml.groundTrace.contents;
		pm->ground.material = ( pml.groundTrace.material ? pml.groundTrace.material : nullptr );
		pm->ground.surface = ( pml.groundTrace.surface ? *pml.groundTrace.surface : cm_surface_t() );
	}
	// Always fetch the actual entity found by the ground trace.
	pm->ground.entity = (edict_ptr_t *)( SG_GetEntityForNumber( trace.entityNumber ) );

	//pm->ps->groundEntityNum = trace.entityNum;

	// don't reset the z velocity for slopes
//	pm->ps->velocity[2] = 0;
	
	// Register the touch trace.
	PM_RegisterTouchTrace( pm->touchTraces, trace );
	#if 0
	PM_AddTouchEnt( trace.entityNum );
	#endif
}



/**
*
* 
*
*	Animations/(XY/XYZ-)speed and BobCycle:
* 
*
* 
**/
/**
*	@brief	Adjust the player's animation states depending on move direction speed and bobCycle.
**/
static void PM_RefreshAnimationState() {
	//
	// Determing whether we're crouching, AND (running OR walking).
	// 
	// Animation State: We're crouched.
	if ( pm->cmd.buttons & BUTTON_CROUCH || pm->state->pmove.pm_flags & PMF_DUCKED ) {
		pm->state->animation.isCrouched = true;
		// Animation State: We're NOT crouched.
	} else if ( !( pm->cmd.buttons & BUTTON_CROUCH || pm->state->pmove.pm_flags & PMF_DUCKED ) ) {
		pm->state->animation.isCrouched = false;
	}
	// Animation State: We're running, NOT walking:
	if ( !( pm->cmd.buttons & BUTTON_WALK ) ) {
		pm->state->animation.isWalking = false;
		// Animation State: We're walking, NOT running:
	} else if ( ( pm->cmd.buttons & BUTTON_WALK ) ) {
		pm->state->animation.isWalking = true;
	}

	// Airborne leaves cycle intact, but doesn't advance either.
	if ( pm->ground.entity == nullptr ) {
		return;
	}

	// If not trying to move:
	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove ) {
		// Check both xySpeed, and xyzSpeed. The last one is to prevent
		// view wobbling when crouched, yet idle.
		if ( pm->state->xySpeed < PM_BOB_CYCLE_IDLE_EPSILON && pm->state->xyzSpeed < PM_BOB_CYCLE_IDLE_EPSILON ) {
			// Start at beginning of cycle again:
			pm->state->bobCycle = 0;
			// Idling at last.
			//pm->state->animation.isIdle = true;
			// Not idling yet, there's still velocities at play.
		} else {
			//pm->state->animation.isIdle = false;
		}

		// Now check for animation idle playback animation.
		if ( pm->state->xySpeed < PM_ANIMATION_IDLE_EPSILON && pm->state->xyzSpeed < PM_ANIMATION_IDLE_EPSILON ) {
			// Idling at last.
			pm->state->animation.isIdle = true;
			// Not idling yet, there's still velocities at play.
		} else {
			pm->state->animation.isIdle = false;
		}
		return;
		// We're not idling that's for sure.
	} else {
		pm->state->animation.isIdle = false;
	}
}
/**
*	@brief	Keeps track of the player's xySpeed and bobCycle:
**/
//static void PM_Footsteps( void ) {
// Determine the animation move direction.
static void PM_Animation_SetMovementDirection();
static void PM_CycleBob() {
	//float		bobMove = 0.f;
	int32_t		oldBobCycle = 0;
	// Defaults to false and checked for later on:
	qboolean	footStep = false;

	//
	// Calculate the speed and cycle to be used for all cyclic walking effects.
	//
	pm->state->xySpeed = QM_Vector2Length( QM_Vector2FromVector3( pm->state->pmove.velocity ) );
	pm->state->xyzSpeed = QM_Vector3Length( pm->state->pmove.velocity );

	// Reset bobmove.
	//pm->state->bobMove = 0.f; // WID: Doing this just.. gives jitter like gameplay feel, but enable if you wanna...

	// Determine(refresh) the animation state.
	PM_RefreshAnimationState();

	// Determine the animation move direction.
	PM_Animation_SetMovementDirection();

	// Airborne leaves cycle intact, but doesn't advance either.
	if ( pm->ground.entity == nullptr ) {
		return;
	}

	// Default to no footstep:
	footStep = false;

	// Determine the bobMove factor:
	if ( pm->state->xySpeed >= PM_BOB_CYCLE_IDLE_EPSILON && pm->state->xyzSpeed >= PM_BOB_CYCLE_IDLE_EPSILON ) {
		// Ducked:
		if ( pm->state->pmove.pm_flags & PMF_DUCKED ) {
			// Ducked characters bob even slower than walking:
			pm->state->bobMove = 0.2;
			// Standing up:
		} else {
			if ( !( pm->cmd.buttons & BUTTON_WALK ) ) {
				// Running bobs faster:
				pm->state->bobMove = 0.4;
				// And applies footstep sounds:
				footStep = true;
			} else {
				// Walking bobs slower:
				pm->state->bobMove = 0.3;
			}
		}
	}

	// If not trying to move, slowly derade the bobMove factor.
	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove ) {
		// Check both xySpeed, and xyzSpeed. The last one is to prevent
		// view wobbling when crouched, yet idle.
		if ( pm->state->xySpeed < PM_BOB_CYCLE_IDLE_EPSILON && pm->state->xyzSpeed < PM_BOB_CYCLE_IDLE_EPSILON ) {
			pm->state->bobMove = 0.;
		}
	}

	// Check for footstep / splash sounds.
	oldBobCycle = pm->state->bobCycle;
	pm->state->bobCycle = (int32_t)( ( oldBobCycle + pm->state->bobMove * pm->cmd.msec ) /* pml.msec */ ) & 255;

	//SG_DPrintf( "%s: pm->state->bobCycle(%i), oldBobCycle(%i), bobMove(%f)\n", __func__, pm->state->bobCycle, oldBobCycle, bobMove );

	// if we just crossed a cycle boundary, play an appropriate footstep event
	//if ( ( ( oldBobCycle + 64 ) ^ ( pm->state->bobCycle + 64 ) ) & 128 ) {
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
*	@brief	Determine the rotation of the legs relative to the facing dir.
**/
static void PM_Animation_SetMovementDirection( void ) {
	// if it's moving to where is looking, it's moving forward
	// The desired yaw for the lower body.
	static constexpr float DIR_EPSILON = 0.3f;
	static constexpr float WALK_EPSILON = 5.0f;
	static constexpr float RUN_EPSILON = 100.f;

	// Angle Vectors.
	const Vector3 vForward = pml.forward;
	const Vector3 vRight = pml.right;

	// Get the move direction vector.
	Vector2 xyMoveDir = QM_Vector2FromVector3( pm->state->pmove.velocity );
	// Normalized move direction vector.
	Vector3 xyMoveDirNormalized = QM_Vector3FromVector2( QM_Vector2Normalize( xyMoveDir ) );

	// Dot products.
	const float xDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vRight );
	const float yDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vForward );

	// Resulting move flags.
	pm->state->animation.moveDirection = 0;

	// Are we even moving enough?
	if ( pm->state->xySpeed > WALK_EPSILON ) {
		// Forward:
		if ( ( yDotResult > DIR_EPSILON ) || ( pm->cmd.forwardmove > 0 ) ) {
			pm->state->animation.moveDirection |= PM_MOVEDIRECTION_FORWARD;
			// Back:
		} else if ( ( -yDotResult > DIR_EPSILON ) || ( pm->cmd.forwardmove < 0 ) ) {
			pm->state->animation.moveDirection |= PM_MOVEDIRECTION_BACKWARD;
		}
		// Right: (Only if the dotproduct is so, or specifically only side move is pressed.)
		if ( ( xDotResult > DIR_EPSILON ) || ( !pm->cmd.forwardmove && pm->cmd.sidemove > 0 ) ) {
			pm->state->animation.moveDirection |= PM_MOVEDIRECTION_RIGHT;
			// Left: (Only if the dotproduct is so, or specifically only side move is pressed.)
		} else if ( ( -xDotResult > DIR_EPSILON ) || ( !pm->cmd.forwardmove && pm->cmd.sidemove < 0 ) ) {
			pm->state->animation.moveDirection |= PM_MOVEDIRECTION_LEFT;
		}

		// Running:
		if ( pm->state->xySpeed > RUN_EPSILON ) {
			pm->state->animation.moveDirection |= PM_MOVEDIRECTION_RUN;
			// Walking:
		} else if ( pm->state->xySpeed > WALK_EPSILON ) {
			pm->state->animation.moveDirection |= PM_MOVEDIRECTION_WALK;
		}
	}

	#if 0
	// Determine the move direction for animations based on the user input state.
	// ( If strafe left / right or forward / backward is pressed then.. )
	if ( pm->cmd.forwardmove || pm->cmd.sidemove ) {
		// Forward:
		if ( pm->cmd.sidemove == 0 && pm->cmd.forwardmove > 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_FORWARD;
			// Forward Left:
		} else if ( pm->cmd.sidemove < 0 && pm->cmd.forwardmove > 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_LEFT;
			// Left:
		} else if ( pm->cmd.sidemove < 0 && pm->cmd.forwardmove == 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_LEFT;
			// Backward Left:
		} else if ( pm->cmd.sidemove < 0 && pm->cmd.forwardmove < 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_BACKWARD_LEFT;
			// Backward:
		} else if ( pm->cmd.sidemove == 0 && pm->cmd.forwardmove < 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_BACKWARD;
			// Backward Right:
		} else if ( pm->cmd.sidemove > 0 && pm->cmd.forwardmove < 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_BACKWARD_RIGHT;
			// Right:
		} else if ( pm->cmd.sidemove > 0 && pm->cmd.forwardmove == 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_RIGHT;
			// Forward Right:
		} else if ( pm->cmd.sidemove > 0 && pm->cmd.forwardmove > 0 ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_RIGHT;
		}
		// if they aren't actively going directly sideways, change the animation to the diagonal so they
		// don't stop too crooked:
	} else {
		if ( pm->state->animation.moveDirection == PM_MOVEDIRECTION_LEFT ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_LEFT;
		} else if ( pm->state->animation.moveDirection == PM_MOVEDIRECTION_RIGHT ) {
			pm->state->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_RIGHT;
		}
	}
	#endif
}





/**
*
*
*
*	MoveType: Dead Move
*
*
*
**/
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
	forward = QM_Vector3Length( pm->state->pmove.velocity );
	forward -= 20;
	if ( forward <= 0 ) {
		pm->state->pmove.velocity = QM_Vector3Zero();
	} else {
		// Normalize.
		pm->state->pmove.velocity = QM_Vector3Normalize( pm->state->pmove.velocity );
		// Scale by old velocity derived length.
		pm->state->pmove.velocity *= forward;
	}
}



/**
*
*
* 
*	MoveType: Ladder Move
* 
* 
* 
**/
/**
*	@brief	Performs movement when latched onto a ladder.
**/
static void PM_LadderMove( void ) {
	static qboolean ladderforward = false; ladderforward = false;
	static Vector3 laddervec = {}; laddervec = {};

	float wishspeed, scale;
	Vector3 wishdir, wishvel;
	float upscale;

	if ( ladderforward ) {
		// move towards the ladder
		VectorScale( laddervec, -200.0, wishvel );
		pm->state->pmove.velocity[ 0 ] = wishvel[ 0 ];
		pm->state->pmove.velocity[ 1 ] = wishvel[ 1 ];
	}

	upscale = ( pml.forward[ 2 ] + 0.5 ) * 2.5;
	if ( upscale > 1.0 ) {
		upscale = 1.0;
	} else if ( upscale < -1.0 ) {
		upscale = -1.0;
	}

	// forward/right should be horizontal only
	pml.forward[ 2 ] = 0;
	pml.right[ 2 ] = 0;
	VectorNormalize( &pml.forward.x );
	VectorNormalize( &pml.right.x );

	// move depending on the view, if view is straight forward, then go up
	// if view is down more then X degrees, start going down
	// if they are back pedalling, then go in reverse of above
	scale = PM_CmdScale( &pm->cmd );
	VectorClear( wishvel );

	if ( pm->cmd.forwardmove ) {
		wishvel[ 2 ] = 0.9 * upscale * scale * (float)pm->cmd.forwardmove;
	}
	//Com_Printf("wishvel[2] = %i, fwdmove = %i\n", (int)wishvel[2], (int)pm->cmd.forwardmove );

	if ( pm->cmd.sidemove ) {
		// strafe, so we can jump off ladder
		Vector3 ladder_right, ang;
		ang = QM_Vector3ToAngles( laddervec );
		QM_AngleVectors( ang, NULL, &ladder_right, NULL );

		// if we are looking away from the ladder, reverse the right vector
		if ( DotProduct( laddervec, pml.forward ) < 0 ) {
			VectorInverse( ladder_right );
		}

		// Allows for strafing on ladder.
		#if 1
		VectorMA( wishvel, 0.5 * scale * (float)pm->cmd.sidemove, pml.right, wishvel );
		// Moves without strafe capability on ladder.
		#else
		VectorMA( wishvel, 0.5 * scale * (float)pm->cmd.sidemove, ladder_right, wishvel );
		#endif
	}

	// do strafe friction
	PM_Friction();

	if ( pm->state->pmove.velocity[ 0 ] < 1 && pm->state->pmove.velocity[ 0 ] > -1 ) {
		pm->state->pmove.velocity[ 0 ] = 0;
	}
	if ( pm->state->pmove.velocity[ 1 ] < 1 && pm->state->pmove.velocity[ 1 ] > -1 ) {
		pm->state->pmove.velocity[ 1 ] = 0;
	}

	wishspeed = VectorNormalize2( &wishvel.x, &wishdir.x );

	PM_Accelerate( wishdir, wishspeed, pmp->pm_accelerate );
	if ( !wishvel.z ) {
		// Apply gravity as a form of 'friction' to prevent 'racing/sliding' against the ladder.
		if ( pm->state->pmove.velocity.z > 0 ) {
			pm->state->pmove.velocity.z -= pm->state->pmove.gravity * pml.frameTime;
			if ( pm->state->pmove.velocity.z < 0 ) {
				pm->state->pmove.velocity.z = 0;
			}
		} else {
			pm->state->pmove.velocity.z += pm->state->pmove.gravity * pml.frameTime;
			if ( pm->state->pmove.velocity.z > 0 ) {
				pm->state->pmove.velocity.z = 0;
			}
		}
	}
	PM_StepSlideMove_Generic(
		pm,
		&pml,
		false
	);
}



/**
*
*
*
*	MoveType: Water Move
*
*
*
**/
/**
*	@brief
**/
static const bool PM_CheckWaterJump( void ) {
	Vector3 spot = {};
	cm_contents_t cont;
	Vector3 flatforward = {};

	// Already got time set, occupied with something else.
	if ( pm->state->pmove.pm_time ) {
		return false;
	}

	// Check for ladder.
	flatforward[ 0 ] = pml.forward[ 0 ];
	flatforward[ 1 ] = pml.forward[ 1 ];
	flatforward[ 2 ] = 0;
	VectorNormalize( &flatforward.x );
	// Trace to possibly attach to ladder.
	spot = pm->state->pmove.origin + ( flatforward * 1 );
	cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) && pm->liquid.level < LIQUID_WAIST ) {
		pm->state->pmove.pm_flags |= PMF_ON_LADDER;
	}

	// Need gravity.
	if ( !pm->state->pmove.gravity ) {
		return false;
	}
	// Don't try waterjump if we're moving against where we'll hop.
	if ( !( pm->cmd.buttons & BUTTON_JUMP )	&& pm->cmd.forwardmove <= 0 ) {
		return false;
	}
	// Check for water jump.
	if ( pm->liquid.level != LIQUID_WAIST ) {
		return false;
	}

	// Quick check that something is even blocking us forward.
	static constexpr double blockTraceDist = 40.;
	trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, pm->state->pmove.origin + ( flatforward * blockTraceDist ), CM_CONTENTMASK_SOLID );

	// We aren't blocked, or what we're blocked by is something we can walk up.
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] >= 0.7f ) {
		return false;
	}

	//VectorMA( pm->state->pmove.origin, 30, flatforward, spot );
	//spot[ 2 ] += 4;
	//cont = pm->pointcontents( &spot.x );//cont = pm->pointcontents( spot, pm->ps->clientNum );
	//if ( !( cont & CONTENTS_SOLID ) /*&& !( cont & CONTENTS_PLAYER)*/ ) {
	//	return false;
	//}

	//spot[ 2 ] += 16;
	//cont = pm->pointcontents( &spot.x ); //cont = pm->pointcontents( spot, pm->ps->clientNum );
	//if ( cont/* && !( cont & CONTENTS_PLAYER )*/ ) {
	//	return false;
	//}
	
	// Test if we can predict to reach where we want to be.
	// Jump up and forward out of the water
	pm->state->pmove.velocity = flatforward * 200;//VectorScale( pml.forward, 200, pm->state->pmove.velocity );
	pm->state->pmove.velocity.z = 350;
	// Time water jump and land.
	pm->state->pmove.pm_flags |= PMF_TIME_WATERJUMP | PMF_TIME_LAND;
	pm->state->pmove.pm_time = 8;
	// 'Fake' the jump key held, so there won't be any immediate jump upon the
	// landing on surface. (ground surface, floor entity, you know?)
	pm->state->pmove.pm_flags |= PMF_JUMP_HELD;

	if ( PM_PredictStepMove( pm, &pml ) ) {
		// Jump up and forward out of the water
		pm->state->pmove.velocity = flatforward * 200;//VectorScale( pml.forward, 200, pm->state->pmove.velocity );
		pm->state->pmove.velocity.z = 350;
		// Time water jump and land.
		pm->state->pmove.pm_flags |= PMF_TIME_WATERJUMP | PMF_TIME_LAND;
		pm->state->pmove.pm_time = 8;
		// 'Fake' the jump key held, so there won't be any immediate jump upon the
		// landing on surface. (ground surface, floor entity, you know?)
		pm->state->pmove.pm_flags |= PMF_JUMP_HELD;
	} else {
		return false;
	}
	return true;
}
/**
*	@brief	
**/
static void PM_WaterJumpMove( void ) {
	// waterjump has no control, but falls
	PM_StepSlideMove_Generic(
		pm,
		&pml,
		true
	);

	pm->state->pmove.velocity[ 2 ] -= pm->state->pmove.gravity * pml.frameTime;
	//if ( pm->state->pmove.velocity[ 2 ] < 0 ) {
	if ( pm->state->pmove.velocity[ 2 ] < 0 ) {
		// cancel as soon as we are falling down again
		pm->state->pmove.pm_flags &= ~PMF_ALL_TIMES;
		pm->state->pmove.pm_time = 0;
	}

}
/**
*	@brief	Performs in-water movement.
**/
static void PM_WaterMove() {
	int32_t i = 0;
	Vector3 wishvel = {};
	double wishspeed = 0.;
	Vector3 wishdir = {};
	double scale = 0.;
	double vel = 0.;

	if ( PM_CheckWaterJump() ) {
		PM_WaterJumpMove();
		return;
	}

	#if 1
	PM_Friction();

	scale = PM_CmdScale( &pm->cmd );
	//
	// user intentions
	//
	if ( !scale ) {
		wishvel[ 0 ] = 0;
		wishvel[ 1 ] = 0;
		wishvel[ 2 ] = -60;		// sink towards bottom
	} else {
		for ( i = 0; i < 3; i++ )
			wishvel[ i ] = scale * pml.forward[ i ] * pm->cmd.forwardmove + scale * pml.right[ i ] * pm->cmd.sidemove;

		wishvel[ 2 ] += scale * pm->cmd.upmove;
	}

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( &wishdir.x );

	constexpr double pm_swimScale = 0.5;
	if ( wishspeed > pm->state->pmove.speed * pm_swimScale ) {
		wishspeed = pm->state->pmove.speed * pm_swimScale;
	}

	PM_Accelerate( wishdir, wishspeed, pmp->pm_water_accelerate );

	// make sure we can go up slopes easily under water
	if ( pml.hasGroundPlane && DotProduct( pm->state->pmove.velocity, pml.groundTrace.plane.normal ) < 0 ) {
		vel = VectorLength( pm->state->pmove.velocity );
		// slide along the ground plane
		PM_ClipVelocity( pm->state->pmove.velocity, pml.groundTrace.plane.normal,
			pm->state->pmove.velocity, PM_OVERCLIP );

		VectorNormalize( &pm->state->pmove.velocity.x );
		VectorScale( pm->state->pmove.velocity, vel, pm->state->pmove.velocity );
	}
	#else
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
	wishspeed = QM_Vector3NormalizeLength( wishDirection ); // wishspeed = wishDirection.normalize();
	if ( wishspeed > pmp->pm_max_speed ) {
		wishVelocity *= pmp->pm_max_speed / wishspeed;
		wishspeed = pmp->pm_max_speed;
	}
	wishspeed *= 0.5f;

	// Adjust speed to if/being ducked.
	if ( ( pm->state->pmove.pm_flags & PMF_DUCKED ) && wishspeed > pmp->pm_crouch_move_speed ) {
		wishVelocity *= pmp->pm_crouch_move_speed / wishspeed;
		wishspeed = pmp->pm_crouch_move_speed;
	}

	// Accelerate through water.
	PM_Accelerate( wishDirection, wishspeed, pmp->pm_water_accelerate );
	#endif
	// Step Slide.
	PM_StepSlideMove_Generic(
		pm,
		&pml,
		false
	);
}



/**
*
*
*
*	MoveType: Fly Move
*
*
*
**/
/**
*	@brief
**/
static void PM_FlyMove( bool doclip ) {
	float drop = 0.f;

	// When clipping don 't adjust viewheight, if no-clipping, default a viewheight of 22.
	pm->state->pmove.viewheight = doclip ? 0 : PM_VIEWHEIGHT_STANDUP;

	// Calculate friction
	const float speed = QM_Vector3Length( pm->state->pmove.velocity );
	if ( speed < 1 ) {
		pm->state->pmove.velocity = QM_Vector3Zero();
	} else {
		drop = 0;

		const float friction = pmp->pm_friction * 1.5f; // extra friction
		const float control = speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed;
		drop += control * friction * pml.frameTime;

		// Scale the velocity
		#if 1
		// Scale the velocity
		float newspeed = speed - drop;
		if ( newspeed < 0 ) {
			newspeed = 0;
		}
		newspeed /= speed;

		pm->state->pmove.velocity *= newspeed;
		//float newspeed = speed - drop;
		//if ( newspeed < 0 ) {
		//	newspeed = 0;
		//}
		//newspeed /= speed;

		//pm->state->pmove.velocity *= newspeed;
		#else
		// Scale the velocity.
		float newspeed = speed - drop;
		if ( newspeed <= 0 ) {
			newspeed = 0;
			pm->state->pmove.velocity = {};
		} else {
			newspeed /= speed;
			pm->state->pmove.velocity *= newspeed;
		}
		#endif
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
	wishspeed *= 1.5;

	const float currentspeed = QM_Vector3DotProduct( pm->state->pmove.velocity, wishdir );
	float addspeed = wishspeed - currentspeed;

	if ( addspeed > 0 ) {
		float accelspeed = pmp->pm_accelerate * pml.frameTime * wishspeed;
		if ( accelspeed > addspeed ) {
			accelspeed = addspeed;
		}
		pm->state->pmove.velocity += accelspeed * wishdir;
	}

	if ( doclip ) {
		/*for (i = 0; i < 3; i++)
			end[i] = pm->state->pmove.origin[i] + pml.frameTime * pm->state->pmove.velocity[i];

		trace = PM_Trace(pm->state->pmove.origin, pm->mins, pm->maxs, end);

		pm->state->pmove.origin = trace.endpos;*/
		// Step Slide.
		// Step Slide.
		PM_StepSlideMove_Generic(
			pm,
			&pml,
			false
		);
	} else {
		// Hard set origin in order to move.
		pm->state->pmove.origin += ( pm->state->pmove.velocity * pml.frameTime );
	}
}



/**
*
*
*
*	MoveType: Walk Move
*
*
*
**/
static void PM_AirMove();
/**
*	@brief
**/
static void PM_WalkMove( const bool canJump ) {
	int32_t i = 0;
	Vector3 wishvel = {};
	double fmove = 0., smove = 0.;
	double wishspeed = 0.;
	Vector3 wishdir = {};
	double scale = 0.;
	usercmd_t cmd = {};
	double accelerate = 0.;
	double vel = 0.;

	#if 0
	if ( pm->liquid.level > LIQUID_WAIST && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 ) {
		// begin swimming
		PM_WaterMove();
		return;
	}

	#if 1
	if ( canJump ) {
		// jumped away
		if ( pm->liquid.level > LIQUID_FEET ) {
			PM_WaterMove();
		} else {
			PM_AirMove();
		}
		return;
	}
	#endif
	#endif
	// Watermove.
	if ( pm->liquid.level > LIQUID_WAIST && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 ) {
		PM_WaterMove();
		return;
	// Jump up/down if we can.
	}
	if ( PM_CheckJump() ) {
		if ( pm->liquid.level > LIQUID_FEET ) {
			PM_WaterMove();
		} else {
			PM_AirMove();
		}
		return;
	}

	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

	// set the movementDir so clients can rotate the legs for strafing
	PM_Animation_SetMovementDirection();

	// project moves down to flat plane
	pml.forward[ 2 ] = 0;
	pml.right[ 2 ] = 0;

	// project the forward and right directions onto the ground plane
	PM_ClipVelocity( pml.forward, pml.groundTrace.plane.normal, pml.forward, PM_OVERCLIP );
	PM_ClipVelocity( pml.right, pml.groundTrace.plane.normal, pml.right, PM_OVERCLIP );
	//
	VectorNormalize( &pml.forward.x );
	VectorNormalize( &pml.right.x );

	for ( i = 0; i < 3; i++ ) {
		wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;
	}
	// when going up or down slopes the wish velocity should Not be zero
//	wishvel[2] = 0;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( &wishdir.x );
	wishspeed *= scale;

	// clamp the speed lower if ducking
	if ( pm->state->pmove.pm_flags & PMF_DUCKED ) {
		constexpr double pm_duckScale = 0.25;
		if ( wishspeed > pm->state->pmove.speed * pm_duckScale ) {
			wishspeed = pm->state->pmove.speed * pm_duckScale;
		}
	}

	// clamp the speed lower if wading or walking on the bottom
	if ( pm->liquid.level ) {
		float	waterScale;
		constexpr double pm_swimScale = 0.5;
		waterScale = (double)( pm->liquid.level ) / 3.0;
		waterScale = 1.0 - ( 1.0 - pm_swimScale ) * waterScale;
		if ( wishspeed > pm->state->pmove.speed * waterScale ) {
			wishspeed = pm->state->pmove.speed * waterScale;
		}
	}

	// when a player gets hit, they temporarily lose
	// full control, which allows them to be moved a bit
	if ( ( pml.groundTrace.surface && pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) || pm->state->pmove.pm_flags & PMF_TIME_KNOCKBACK ) {
		accelerate = pmp->pm_air_accelerate;
	} else {
		accelerate = pmp->pm_accelerate;
	}

	PM_Accelerate( wishdir, wishspeed, accelerate );

	//Com_Printf("velocity = %1.1f %1.1f %1.1f\n", pm->ps->velocity[0], pm->ps->velocity[1], pm->ps->velocity[2]);
	//Com_Printf("velocity1 = %1.1f\n", VectorLength(pm->ps->velocity));

	if ( ( pml.groundTrace.surface && pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) || pm->state->pmove.pm_flags & PMF_TIME_KNOCKBACK ) {
		pm->state->pmove.velocity.z -= pm->state->pmove.gravity * pml.frameTime;
	} else {
		// don't reset the z velocity for slopes
//		pm->ps->velocity[2] = 0;
	}

	vel = VectorLength( pm->state->pmove.velocity );

	// slide along the ground plane
	PM_ClipVelocity( pm->state->pmove.velocity, pml.groundTrace.plane.normal,
		pm->state->pmove.velocity, PM_OVERCLIP );

	// don't decrease velocity when going up or down a slope
	VectorNormalize( &pm->state->pmove.velocity.x );
	VectorScale( pm->state->pmove.velocity, vel, pm->state->pmove.velocity );

	// don't do anything if standing still
	static constexpr double STOP_EPSILON = 0.05;
	if ( !( pm->state->pmove.velocity[ 0 ] ) && !( pm->state->pmove.velocity[ 1 ] ) ) {
		return;
	}

	PM_StepSlideMove_Generic(
		pm,
		&pml,
		false
	);
	//Com_Printf("velocity2 = %1.1f\n", VectorLength(pm->ps->velocity));
}



/**
*
*
*
*	MoveType: Air Move
*
*
*
**/
/**
*	@brief
**/
static void PM_AirMove( void ) {
	int32_t i = 0;
	Vector3 wishvel = {};
	double fmove = 0., smove = 0.;
	double wishspeed = 0.;
	Vector3 wishdir = {};
	double scale = 0.;
	double vel = 0.;
	usercmd_t cmd = {};

	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

	// set the movementDir so clients can rotate the legs for strafing
	PM_Animation_SetMovementDirection();

	// project moves down to flat plane
	pml.forward[ 2 ] = 0;
	pml.right[ 2 ] = 0;
	VectorNormalize( &pml.forward.x );
	VectorNormalize( &pml.right.x );

	for ( i = 0; i < 2; i++ ) {
		wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;
	}
	wishvel[ 2 ] = 0;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( &wishdir.x );
	wishspeed *= scale;

	// not on ground, so little effect on velocity
	//PM_Accelerate( wishdir, wishspeed, pmp->pm_air_accelerate );
	// <Q2RTXP>: WID: This pm_air_accelerate is a boolean cvar lol, we can't use it like q3.

	PM_Accelerate( wishdir, wishspeed, pmp->pm_air_accelerate );

	// we may have a ground plane that is very steep, even
	// though we don't have a groundentity
	// slide along the steep plane
	if ( pml.hasGroundPlane ) {
		PM_ClipVelocity( pm->state->pmove.velocity, pml.groundTrace.plane.normal,
			pm->state->pmove.velocity, PM_OVERCLIP );
	}

	#if 0
	//ZOID:  If we are on the grapple, try stair-stepping
	//this allows a player to use the grapple to pull himself
	//over a ledge
	if ( pm->ps->pm_flags & PMF_GRAPPLE_PULL )
		PM_StepSlideMove( qtrue );
	else
		PM_SlideMove( qtrue );
	#endif

	#if 0
	PM_SlideMove_Generic(
		pm,
		&pml,
		true
	);
	#else
	PM_StepSlideMove_Generic(
		pm,
		&pml,
		true
	);
	#endif
}



/**
*
* 
* 
*
*	(Initial-)Position Snapping/Validation:
*
* 
* 
*
**/
/**
*	@brief	True if we're standing in a legitimate non solid position.
**/
static inline const bool PM_GoodPosition() {
	// Position is always valid if no-clipping.
	if ( pm->state->pmove.pm_type == PM_NOCLIP ) {
		return true;
	}
	// Perform the solid trace.
	const cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, pm->state->pmove.origin );
	return !trace.allsolid && !trace.startsolid;
}
/**
*	@brief	On exit, the origin will have a value that is pre-quantized to the PMove
*			precision of the network channel and in a valid position.
**/
static void PM_SnapPosition() {
	if ( PM_GoodPosition() ) {
		return;
	} else {
		pm->state->pmove.origin = pml.previousOrigin;
		pm->state->pmove.velocity = pml.previousVelocity;
		pm->liquid = pml.previousLiquid;
	}
}



/**
*
*
* 
* 
*	Core Logic and Entry Points:
*
* 
* 
*
**/
/**
*	@brief	Clamp view angles within range (0, 360).
**/
static void PM_UpdateViewAngles( player_state_t *playerState, const usercmd_t *userCommand ) {
	// No view changes at all in intermissions.
	if ( pm->state->pmove.pm_type == PM_INTERMISSION || pm->state->pmove.pm_type == PM_SPINTERMISSION ) {
		return;		// no view changes at all
	}
	// Dead, or Gibbed.
	if ( pm->state->pmove.pm_type != PM_SPECTATOR && pm->state->stats[ STAT_HEALTH ] <= 0 ) {
		return;		// no view changes at all
	}

	if ( playerState->pmove.pm_flags & PMF_TIME_TELEPORT ) {
		playerState->viewangles[ YAW ] = QM_AngleMod( userCommand->angles[ YAW ] + playerState->pmove.delta_angles[ YAW ] );
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
	// Unset these buttons so other code won't respond to it.
	pm->cmd.buttons &= ~( BUTTON_JUMP | BUTTON_CROUCH | BUTTON_USE_TARGET | BUTTON_RELOAD );
}
/**
*	@brief
**/
static void PM_CheckSpecialMovement() {
	// Having time means we're already doing some form of 'special' movement.
	if ( pm->state->pmove.pm_time ) {
		return;
	}

	// Remove ladder flag.
	pm->state->pmove.pm_flags &= ~PMF_ON_LADDER;

	// Re-Check for a ladder.
	Vector3 flatforward = QM_Vector3Normalize( {
		pml.forward.x,
		pml.forward.y,
		0.f
		} );
	const Vector3 spot = pm->state->pmove.origin + ( flatforward * 1 );
	cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, spot, (cm_contents_t)( CONTENTS_LADDER ) );
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) 
		// Uncomment to disable underwater ladders.
		/*&& pm->liquid.level < cm_liquid_level_t::LIQUID_WAIST*/ ) {
		pm->state->pmove.pm_flags |= PMF_ON_LADDER;
	}

	// Don't do any 'special movement' if we're not having any gravity.
	if ( !pm->state->pmove.gravity ) {
		return;
	}
}
/**
*
**/
static void PM_DropTimers() {
	if ( pm->state->pmove.pm_time ) {
		//int32_t msec = (int32_t)pm->cmd.msec >> 3;
		//double msec = ( pm->cmd.msec < 0. ? ( 1.0 / 0.125 ) : (pm->cmd.msec / 0.125 ) );
		double msec = ( pm->cmd.msec < 0. ? ( 1.0 / 8 ) : ( pm->cmd.msec / 8 ) );
		//if ( pm->cmd.msec < 0. ) { // This was <= 0, this seemed to cause a slight jitter in the player movement.
		//	msec = 1.0 / 8.; // 8 ms = 1 unit. (At 10hz.)
		//}

		if ( msec >= pm->state->pmove.pm_time ) {
			pm->state->pmove.pm_flags &= ~( PMF_ALL_TIMES );
			pm->state->pmove.pm_time = 0;
		} else {
			pm->state->pmove.pm_time -= msec;
		}
	}
}
/**
*	@brief	Can be called by either the server or the client game codes.
**/
void SG_PlayerMove_Frame() {
	// Clear out several member variables which require a fresh state before performing the move.
	#if 1
	// Player State variables.
	pm->state->viewangles = {};
	//pm->state->pmove.viewheight = 0;
	pm->state->screen_blend[ 0 ] = pm->state->screen_blend[ 1 ] = pm->state->screen_blend[ 2 ] = pm->state->screen_blend[ 3 ] = 0;
	pm->state->rdflags = refdef_flags_t::RDF_NONE;
	// Player State Move variables.
	pm->touchTraces = {};
	pm->ground = {};
	pm->liquid = {
		.type = CONTENTS_NONE,
		.level = cm_liquid_level_t::LIQUID_NONE
	},
	pm->jump_sound = false;
	pm->step_clip = false;
	pm->step_height = 0;
	pm->impact_delta = 0;
	#else
	pm->touchTraces = {};
	pm->ground = {};
	pm->liquid = {
		.type = CONTENTS_NONE,
		.level = cm_liquid_level_t::LIQUID_NONE
	},
	pm->jump_sound = false;
	pm->step_clip = false;
	pm->step_height = 0;
	pm->impact_delta = 0;
	#endif

	// Save the origin as 'old origin' for in case we get stuck.
	pml.previousOrigin = pm->state->pmove.origin;
	// Save the start velocity.
	pml.previousVelocity = pm->state->pmove.velocity;

	// Calculate frameTime.
	pml.frameTime = (double)pm->cmd.msec * 0.001;

	// Clamp view angles.
	PM_UpdateViewAngles( pm->state, &pm->cmd );

	/**
	*	Key Stuff:
	**/
	#if 1
	// Player has let go of jump button.
	if ( !( pm->cmd.buttons & BUTTON_JUMP ) && pm->cmd.upmove < 10 ) {
		pm->state->pmove.pm_flags &= ~PMF_JUMP_HELD;
	}
	#endif

	/**
	*	PM_DEAD:
	**/
	// Erase all input command state when dead, we don't want to allow user input to be moving our dead body.
	if ( pm->state->pmove.pm_type >= PM_DEAD ) {
		PM_EraseInputCommandState();
	}

	/**
	*	PM_SPECTATOR/PM_NOCLIP:
	**/
	// Performs fly move, only clips in case of spectator mode, noclips otherwise.
	if ( pm->state->pmove.pm_type == PM_SPECTATOR || pm->state->pmove.pm_type == PM_NOCLIP ) {
		//pml.frameTime = pmp->pm_fly_speed * pm->cmd.msec * 0.001f;

		// Re-ensure no flags are set anymore.
		pm->state->pmove.pm_flags = PMF_NONE;

		// Give the spectator a small 8x8x8 bounding box.
		if ( pm->state->pmove.pm_type == PM_SPECTATOR ) {
			pm->mins = { -8.f, -8.f, -8.f };
			pm->maxs = { -8.f, -8.f,-8.f };
		}

		// Get moving.
		PM_FlyMove( pm->state->pmove.pm_type == PM_SPECTATOR );
		// Drop timers.
		PM_DropTimers();
		// Snap to position.
		//PM_SnapPosition();
 		return;
	}

	/**
	*	PM_FREEZE:
	**/
	if ( pm->state->pmove.pm_type == PM_FREEZE ) {
		return;     // no movement at all
	}
	/**
	*	PM_FREEZE:
	**/
	if ( pm->state->pmove.pm_type == PM_INTERMISSION || pm->state->pmove.pm_type == PM_SPINTERMISSION ) {
		return;		// no movement at all
	}

	// Set mins, maxs, and viewheight.
	PM_SetDimensions();
	// Get liquid.level, accounting for ducking.
	PM_GetWaterLevel( pm->state->pmove.origin, pm->liquid.level, pm->liquid.type );
	// Back it up as the first previous liquid. (Start of frame position.)
	pml.previousLiquid = pm->liquid;

	#if 0
	// If pmove values were changed outside of the pmove code, resnap to position first before continuing.
	if ( pm->snapInitialPosition ) {
		PM_InitialSnapPosition();
	}
	#endif
	// Recategorize if we're ducked. ( Set groundentity, liquid.type, and liquid.level. )
	if ( PM_CheckDuck() ) {
		// Back it up as the first previous liquid. (Start of frame position.)
		pml.previousLiquid = pm->liquid;
		//PM_CategorizePosition();
		// Get liquid.level, accounting for ducking.
		PM_GetWaterLevel( pm->state->pmove.origin, pm->liquid.level, pm->liquid.type );
	}
	// Get and set ground entity.
	PM_GroundTrace();

	/**
	*	PM_DEAD:
	**/
	// When dead, perform dead movement.
	if ( pm->state->pmove.pm_type == PM_DEAD ) {
		PM_DeadMove();
	}

	PM_CheckSpecialMovement();

	/**
	*	Drop Timers:
	**/
	PM_DropTimers();

	// Do Nothing:
	if ( pm->state->pmove.pm_flags & PMF_TIME_TELEPORT ) {
		// --- ( Teleport pause stays exactly in place ) ---
	// WaterJump Move ( Has no control, but falls by gravity influences ):
	} else if ( pm->state->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		PM_WaterJumpMove();
		// Water Move:
	} else if ( pm->liquid.level > LIQUID_FEET ) {
		PM_WaterMove();
	// Ladder Move ( Can strafe on ladder. )
	} else if ( pm->state->pmove.pm_flags & PMF_ON_LADDER ) {
		PM_LadderMove();
	// Ground Walk Move.
	} else {
		// Different pitch handling.
		Vector3 angles = pm->state->viewangles;
		if ( angles[ PITCH ] > 180. ) {
			angles[ PITCH ] = angles[ PITCH ] - 360.;
		}
		angles[ PITCH ] /= 3.;
		// Recalculate angle vectors.
		QM_AngleVectors( angles, &pml.forward, &pml.right, &pml.up );

		if ( pml.isWalking ) {
			//// Watermove.
			//if ( pm->liquid.level > LIQUID_WAIST && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 ) {
			//	PM_WaterMove();
			//// Jump up/down if we can.
			//} else if ( PM_CheckJump() ) {
			//	if ( pm->liquid.level > LIQUID_FEET ) {
			//		PM_WaterMove();
			//	} else {
			//		PM_AirMove();
			//	}
			//} else {
				PM_WalkMove( true );
			//}
		} else {
			if ( !( pm->state->pmove.pm_flags & PMF_TIME_WATERJUMP) ) {
				PM_CheckJump();
			}
			// Perform move.
			PM_AirMove();
		}
	}

	#if 0
	// Recategorize position for contents, ground, and/or liquid since we've made a move.
	PM_CategorizePosition();
	#endif
	// Get and set ground entity.
	PM_GroundTrace();
	// Get liquid.level, accounting for ducking.
	PM_GetWaterLevel( pm->state->pmove.origin, pm->liquid.level, pm->liquid.type );
	#if 0
	// Determine whether we can pull a trick jump, and if so, perform the jump.
	if ( pm->state->pmove.pm_flags & PMF_TIME_TRICK_JUMP ) {
		PM_CheckJump();
	}
	#endif
	// Apply contents and other screen effects.
	PM_UpdateScreenContents();

	// Weapons.
	//PM_Weapon();
	// Torso animation.
	//PM_TorsoAnimation();
	// Bob Cycle / Footstep events / legs animations.
	//PM_Footsteps();
	PM_CycleBob();
	// Entering / Leaving water splashes.
	//PM_WaterEvents();
	// Snap us back into a validated position.
	//PM_SnapPosition();
}
/**
*	@brief	Can be called by either the server or the client game codes.
**/
void SG_PlayerMove( pmove_s *pmove, pmoveParams_s *params ) {
	// Clear the player move locals.
	pml = {};

	// Store pointers.
	pm = pmove;
	pmp = params;

	// Iterate the player movement for yet another single frame.
	SG_PlayerMove_Frame();
}

/**
*	@brief	Default player move configuration parameters.
**/
void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp ) {
	// Q2RTXPerimental Defaults:
	pmp->pm_air_accelerate = atof( (const char *)SG_GetConfigString( CS_AIRACCEL ) );

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

	//pmp->pm_duck_viewheight_speed = default_pmoveParams_t::pm_duck_viewheight_speed;
	pmp->pm_crouch_move_speed = default_pmoveParams_t::pm_crouch_move_speed;
	pmp->pm_water_speed = default_pmoveParams_t::pm_water_speed;
	pmp->pm_fly_speed = default_pmoveParams_t::pm_fly_speed;

	pmp->pm_accelerate = default_pmoveParams_t::pm_accelerate;
	pmp->pm_water_accelerate = default_pmoveParams_t::pm_water_accelerate;
	if ( pmp->pm_air_accelerate <= 0 ) {
		pmp->pm_air_accelerate = default_pmoveParams_t::pm_air_accelerate;
	}

	pmp->pm_friction = default_pmoveParams_t::pm_friction;
	pmp->pm_water_friction = default_pmoveParams_t::pm_water_friction;
}

