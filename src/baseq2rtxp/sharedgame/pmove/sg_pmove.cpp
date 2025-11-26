/********************************************************************
*
*
*	SharedGame: Player Movement
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_entity_events.h"
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

//! Needed for ground traces' impact detection.
static void PM_CrashLand( void );



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
	return pm->clip( &start, &mins, &maxs, &end, contentMask );
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

	return pm->trace( &start, &mins, &maxs, &end, pm->playerEdict, contentMask );
}

/**
*	@brief	Returns the contents at a given point.
**/
const cm_contents_t PM_PointContents( const Vector3 &point ) {
	return pm->pointcontents( &point );
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
*	@brief	Returns the scale factor to apply to user input command movements
*			This allows the clients to use axial -pmp_max_speed to pmp_max_speed values for all directions
*			without getting a sqrt(2) distortion in speed.
**/
static double PM_CmdScale( const usercmd_t *cmd ) {
	/**
	*	Determine the maximum velocity direction force.
	**/
	// Forward move.
	double max = std::fabs( cmd->forwardmove );
	// Side move.
	const double fabsSideMove = std::fabs( cmd->sidemove );
	if ( fabsSideMove > max ) {
		max = fabsSideMove;
	}
	// Up move.
	const double fabsUpMove = std::fabs( cmd->upmove );
	if ( fabsUpMove > max ) {
		max = fabsUpMove;
	}
	/**
	*	Now calculate the scale factor to use for applying to movement so we remain capped <= pmp->pm_max_speed.
	**/
	double scale = 0.;
	if ( max ) {
		// Movement direction vector.
		const Vector3 vDirection = { cmd->forwardmove, cmd->sidemove, cmd->upmove };
		// Get the total speed factor.
		const double total = QM_Vector3LengthDP( vDirection );
		// Calculate actual scale.
		scale = pm->state->pmove.speed * max / ( /* 127 */ pmp->pm_max_speed * total );
	}
	// Bada bing bada boom.
	return scale;
}

/**
*	@brief	Sets the player's movement bounding box depending on various state factors.
**/
static void PM_UpdateBoundingBox() {
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
	// Give the spectator/noclipper a small 8x8x8 bounding box.
	if ( pm->state->pmove.pm_type == PM_SPECTATOR || pm->state->pmove.pm_type == PM_NOCLIP ) {
		pm->mins = PM_BBOX_FLYING_MINS;
		pm->maxs = PM_BBOX_FLYING_MAXS;
		pm->state->pmove.viewheight = PM_VIEWHEIGHT_FLYING;
		return;
	}

	// Ducked AND Death BBox:
	if ( pm->state->pmove.pm_type == PM_DEAD || ( pm->state->pmove.pm_flags & PMF_DUCKED ) ) {
		//! Set bounds to ducked.
		pm->mins = PM_BBOX_DUCKED_MINS;
		pm->maxs = PM_BBOX_DUCKED_MAXS;
		// Viewheight to ducked.
		pm->state->pmove.viewheight = PM_VIEWHEIGHT_DUCKED;
	}
}

/**
*	@brief	Determine the vieworg's brush contents, apply Blend effect based on results.
*			Also determines whether the view is underwater and to render it as such.
**/
static void PM_UpdateScreenContents() {
	// Compute view origin (eye position)
	const Vector3 viewOrigin = {
		pm->state->pmove.origin.x + pm->state->viewoffset.x,
		pm->state->pmove.origin.y + pm->state->viewoffset.y,
		pm->state->pmove.origin.z + pm->state->viewoffset.z + (float)pm->state->pmove.viewheight
	};

	// Get contents at view position (fallback/secondary source)
	const cm_contents_t viewContents = PM_PointContents( viewOrigin );

	// Prefer the body-level liquid classification when available, because it is sampled
	// in PM_GetLiquidContentsForPoint and is more stable while the body is inside a liquid brush.
	cm_contents_t liquidType = pm->liquid.type;
	cm_liquid_level_t liquidLevel = pm->liquid.level;

	// If we don't have a reliable liquid type (edge cases), fallback to the view sample.
	if ( liquidType == CONTENTS_NONE ) {
		liquidType = viewContents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER );
	}

	// RDF_UNDERWATER: prefer pm->liquid.level (accurate body sampling). fallback to viewContents check.
	if ( pm->liquid.level == cm_liquid_level_t::LIQUID_UNDER || ( viewContents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) ) {
		pm->state->rdflags |= RDF_UNDERWATER;
	} else {
		pm->state->rdflags &= ~RDF_UNDERWATER;
	}

	// Prevent adding screenblend if we're inside a client entity brush (CONTENTS_PLAYER)
	// Use liquidType for blend selection, fallback to viewContents if needed.
	const bool insidePlayerBrush = ( viewContents & CONTENTS_PLAYER ) != 0;

	// If inside a player brush, skip blends; otherwise apply blends based on liquidType (or viewContents fallback).
	if ( !insidePlayerBrush && ( viewContents & ( CONTENTS_SOLID | CONTENTS_LAVA ) ) ) {
		// Inside a solid or lava at view position (keep old behaviour to handle solid-lava cases)
		SG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, pm->state->screen_blend );
	} else if ( ( liquidLevel == LIQUID_UNDER && liquidType & CONTENTS_SLIME ) ) {
		SG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, pm->state->screen_blend );
	} else if ( ( liquidLevel == LIQUID_UNDER && liquidType & CONTENTS_WATER ) || viewContents & CONTENTS_WATER ) {
		SG_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, pm->state->screen_blend );
	}
}

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
*	Friction/Acceleration/Currents:
*
*
*
**/
/**
*	@brief	Handles friction for the player, covering all scenarios.
**/
static void PM_Friction() {
	// Get velocity.
	const Vector3 velocity = pm->state->pmove.velocity;

	// Set us to a halt, if our speed is too low, otherwise we'll see
	// ourselves sort of 'drifting'.
	//
	// Allow for sinking underwater,so don't touch Z.
	const double speed = QM_Vector3Length( pm->state->pmove.velocity );
	// Allow sinking underwater.
	if ( speed < 1. ) {
		pm->state->pmove.velocity.x = 0.;
		pm->state->pmove.velocity.y = 0.;
		return;
	}

	// Drop value.
	double drop = 0.;
	const double control = pmp->pm_stop_speed;

	// Apply ground friction if on-ground.
	#ifdef PMOVE_USE_MATERIAL_FRICTION
		// Ensure we are on-ground.
		if ( ( pm->ground.entity != nullptr
			&& ( ( pml.groundTrace.surface == nullptr ) || !( pml.groundTrace.surface && pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) )
			) || ( pm->state->pmove.pm_flags & PMF_ON_LADDER )
		) {
			// Get the material to fetch friction from.
			cm_material_t *ground_material = ( pml.groundTrace.surface != nullptr ? pml.groundTrace.surface->material : nullptr );
			double friction = ( ground_material ? ground_material->physical.friction : pmp->pm_friction );
			const double control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
			drop += control * friction * pml.frameTime;
		}
	// Apply ground friction if on-ground.
	#else
		if ( ( pm->ground.entity && pml.groundTrace.surface
			&& !( pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) )
			|| ( pm->state->pmove.pm_flags & PMF_ON_LADDER )
		) {
			const float friction = pmp->pm_friction;
			const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
			drop += control * friction * pml.frameTime;
		}
	#endif

	//const double control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );

	// Apply water friction, and not off-ground yet on a ladder.
	if ( pm->liquid.level /*&& !( pm->state->pmove.pm_flags & PMF_ON_LADDER )*/ ) {
		drop += speed * pmp->pm_water_friction * (float)pm->liquid.level * pml.frameTime;
	}
	// Apply ladder friction, if climbing a ladder and in-water.
	if ( pm->liquid.level && ( pm->state->pmove.pm_flags & PMF_ON_LADDER ) ) {
		drop *= ( pmp->pm_ladder_mod / (float)pm->liquid.level ) * pml.frameTime;
	}
	// Apply flying friction for spectator/noclip.
	if ( pm->state->pmove.pm_type == PM_SPECTATOR || pm->state->pmove.pm_type == PM_NOCLIP ) {
		// Spectator friction.
		drop += control * pmp->pm_fly_friction * pml.frameTime;
	}

	// Scale the velocity.
	double newSpeed = speed - drop;
	if ( newSpeed < 0. ) {
		newSpeed = 0.;
	}
	newSpeed /= speed;
	pm->state->pmove.velocity *= newSpeed;
}

/**
*	@brief	Handles user intended 'mid-air' acceleration.
**/
static void PM_AirAccelerate( const Vector3 &wishDirection, const double wishSpeed, const double acceleration ) {
	// Cap air acceleration speed if set.
	const double air_wish_speed_cap = pmp->pm_air_wish_speed_cap;
	const double wishspd = ( wishSpeed > air_wish_speed_cap && air_wish_speed_cap ? air_wish_speed_cap : wishSpeed );

	// See if we are changing direction a bit.
	const double currentspeed = QM_Vector3DotProduct( pm->state->pmove.velocity, wishDirection );
	const double addSpeed = wishspd - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	// Use the same acceleration as on-ground, but scaled down.
	double accelerationSpeed = acceleration * wishSpeed * pml.frameTime;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	// Adjust the velocity.
	pm->state->pmove.velocity = QM_Vector3MultiplyAdd( pm->state->pmove.velocity, accelerationSpeed, wishDirection );
}

// HL like Acceleration:
#ifdef HL_LIKE_ACCELERATION
/**
*	@brief	Handles user intended acceleration(Supposed HL formula).
**/
static void PM_Accelerate( const Vector3 &wishDirection, const double wishSpeed, const double acceleration ) {
	const double currentspeed = QM_Vector3DotProductDP( pm->state->pmove.velocity, wishDirection );
	const double addSpeed = wishSpeed - currentspeed;
	if ( addSpeed <= 0. ) {
		return;
	}

	// HL like acceleration.
	double drop = 0.;
	double friction = 1.;
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
	double accelerationSpeed = acceleration * pml.frameTime * wishSpeed * friction;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	double accelerationSpeed = acceleration * pml.frameTime * wishSpeed;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	pm->state->pmove.velocity = QM_Vector3MultiplyAdd( pm->state->pmove.velocity, accelerationSpeed, wishDirection );
}
// Vanilla Q2RTX Acceleration:
#else
/**
*	@brief	Handles user intended acceleration.
**/
static void PM_Accelerate( const Vector3 &wishDirection, const double wishSpeed, const double acceleration ) {
	// Q2 Acceleration:
	#if 1
		const double currentspeed = QM_Vector3DotProductDP( pm->state->pmove.velocity, wishDirection );
		const double addSpeed = wishSpeed - currentspeed;
		if ( addSpeed <= 0. ) {
			return;
		}

		double accelerationSpeed = acceleration * pml.frameTime * wishSpeed;
		if ( accelerationSpeed > addSpeed ) {
			accelerationSpeed = addSpeed;
		}

		pm->state->pmove.velocity = QM_Vector3MultiplyAdd( pm->state->pmove.velocity, accelerationSpeed, wishDirection );
	// Q3: Prevents Jump Strafe "bug" but claimed to feel bad by some.
	#else
		const Vector3 wishVelocity = QM_Vector3Scale( wishDirection, wishSpeed );
		Vector3 pushDirection = wishVelocity - pm->state->pmove.velocity;
		const double pushLength = QM_Vector3NormalizeLength( pushDirection );
		double canPush = acceleration * pml.frameTime * wishSpeed;
		if ( canPush > pushLength ) {
			canPush = pushLength;
		}
		pm->state->pmove.velocity = QM_Vector3MultiplyAdd( pm->state->pmove.velocity, canPush, pushDirection );
	#endif
}
#endif



/**
*
*
* 
*	Touch Entities List:
*
* 
*
**/
/**
*	@brief	As long as numberOfTraces does not exceed MAX_TOUCH_TRACES, and there is not a duplicate trace registered,
*			this function adds the trace into the touchTraceList array and increases the numberOfTraces.
**/
void PM_RegisterTouchTrace( pm_touch_trace_list_t &touchTraceList, cm_trace_t &trace ) {
	// Don't add world.
	if ( trace.entityNumber == ENTITYNUM_WORLD ) {
		return;
	}
	// Escape function if we are exceeding maximum touch traces.
	if ( touchTraceList.count >= MAX_TOUCH_TRACES ) {
		return;
	}

	// Iterate for possible duplicates.
	for ( int32_t i = 0; i < touchTraceList.count; i++ ) {
		// Escape function if duplicate.
		if ( touchTraceList.traces[ i ].entityNumber == trace.entityNumber ) {
			return;
		}
	}

	// Add trace to list.
	touchTraceList.traces[ touchTraceList.count ] = trace;
	touchTraceList.count++;
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
*	@brief	Determine whether we're above water, or not.
**/
static inline const bool PM_AboveLiquid() {
	// Testing point.
	const Vector3 below = pm->state->pmove.origin + Vector3{ 0, 0, -8 };
	// We got solid below, not water:
	bool solid_below = pm->trace( &pm->state->pmove.origin, &pm->mins, &pm->maxs, &below, pm->playerEdict, CM_CONTENTMASK_SOLID ).fraction < 1.0f;
	if ( solid_below ) {
		return false;
	}

	// We're above water:
	bool water_below = pm->trace( &pm->state->pmove.origin, &pm->mins, &pm->maxs, &below, pm->playerEdict, CM_CONTENTMASK_LIQUID ).fraction < 1.0f;
	if ( water_below ) {
		return true;
	}

	// No water below.
	return false;
}
/**
*	@brief	Will fill in the cm_liquid_level_t and cm_contents_t BSP/Hull data,
*			for the the specified position.
**/
static inline void PM_GetLiquidContentsForPoint( const Vector3 &position, cm_liquid_level_t &level, cm_contents_t &type ) {
	// Default to none.
	level = cm_liquid_level_t::LIQUID_NONE;
	type = CONTENTS_NONE;

	// Determine the height of the samples.
	double sample2 = (int)( pm->state->pmove.viewheight - pm->mins.z );
	double sample1 = sample2 / 2.;

	// First sample point, just above feet.
	Vector3 point = position;
	point.z += pm->mins.z + 1.;
	// Get contents at feet.
	cm_contents_t contentType = PM_PointContents( point );

	// If we found a liquid, sample further up.
	if ( contentType & CM_CONTENTMASK_LIQUID ) {
		// Second sample point, halfway up body.
		sample2 = pm->state->pmove.viewheight - pm->mins.z;
		sample1 = sample2 / 2.;
		// Set the content type and the level to feet.
		type = contentType;
		level = cm_liquid_level_t::LIQUID_FEET;

		// Sample halfway up body.
		point.z = pm->state->pmove.origin.z + pm->mins.z + sample1;
		contentType = PM_PointContents( point );
		// If we found a liquid, sample further up.
		if ( contentType & CM_CONTENTMASK_LIQUID ) {
			// Set level to waist.
			level = cm_liquid_level_t::LIQUID_WAIST;

			// Sample at eye level.
			point.z = pm->state->pmove.origin.z + pm->mins.z + sample2;
			contentType = PM_PointContents( point );
			// If we found a liquid, set level to eyes.
			if ( contentType & CM_CONTENTMASK_LIQUID ) {
				level = cm_liquid_level_t::LIQUID_UNDER;
			}
		}
	}
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
*	@brief	Corrects a allsolid ground trace by jittering around the current position.
*			Returns true if a valid ground trace was found.
*	@note	In case of success the pml.groundTrace will be set to the new valid trace.
*			When this function returns false, the player is in freefall.
**/
static const int32_t PM_CorrectAllSolidGround( cm_trace_t *trace ) {
	int32_t	i, j, k;

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
	Vector3 point = pm->state->pmove.origin - Vector3{ 0.f, 0.f, (float)PM_STEP_GROUND_DIST };
	cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, point );
	// Assign the ground trace.
	pml.groundTrace = trace;

	// Do something corrective if the trace starts in a solid.
	if ( trace.allsolid ) {
		// Did not find valid ground.
		if ( !PM_CorrectAllSolidGround( &trace ) ) {
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
	if ( trace.plane.normal[ 2 ] < PM_STEP_MIN_NORMAL ) {
		//if ( pm->debugLevel ) {
		//	Com_Printf( "%i:steep\n", c_pmove );
		//}

		// <Q2RTXP>: This is what Q3 said anyhow.
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

		// Damage crash landing.
		PM_CrashLand();

		// Don't reset the z velocity for slopes.
		//	pm->state->velocity[2] = 0;

		// Don't do landing time if we were just going down a slope.
		if ( pml.previousVelocity.z < -200. ) {
			// Don't allow another jump for a little while
			pm->state->pmove.pm_flags |= PMF_TIME_LAND;
			pm->state->pmove.pm_time = 24;
		}
	}

	// If we have an entity, or a ground plane, assign the remaining ground properties.
	if ( pml.hasGroundPlane || pm->ground.entity ) {
		if ( pml.hasGroundPlane ) {
			pm->ground.plane = pml.groundTrace.plane;
		// <Q2RTXP>: WID: This one is already zerod out at pmove preparation.
		} else {
			pm->ground.plane = {};
		}
		pm->ground.contents = pml.groundTrace.contents;
		pm->ground.material = ( pml.groundTrace.material ? pml.groundTrace.material : nullptr );
		pm->ground.surface = ( pml.groundTrace.surface ? *pml.groundTrace.surface : cm_surface_t() );
	} else {
		//pm->ground = {};
	}
	// Always fetch the actual entity found by the ground trace.
	pm->ground.entity = ( SG_GetEntityForNumber( trace.entityNumber ) );

	// Don't reset the z velocity for slopes.
//	pm->ps->velocity[2] = 0;

	// Register the touch trace.
	PM_RegisterTouchTrace( pm->touchTraces, trace );
}

/**
*	@brief	Handles the water and conveyor belt by applying their 'currents'.
**/
static void PM_AddCurrents( Vector3 & wishVelocity ) {
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

		double speed = pmp->pm_water_speed;
		// Walking in water, against a current, so slow down our 
		if ( ( pm->liquid.level == cm_liquid_level_t::LIQUID_FEET ) && ( pm->ground.entity ) ) {
			speed /= 2.;
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
*	Special Movement: Duck, Jump, and Jump (Crash-)landing!
*
*
*
**/
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
		( /*pm->ground.entity*/ pml.isWalking || ( pm->liquid.level <= cm_liquid_level_t::LIQUID_FEET && !PM_AboveLiquid() ) ) &&
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
	// Stand Up: If possible, try to get out of the "ducked" state.
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
	PM_UpdateBoundingBox();

	// We're ducked.
	return true;
}

/**
*	@brief	Checks for hard landings that generate sound events and impact damage.
**/
static void PM_CrashLand( void ) {
	#if 0
	// Decide which landing animation to use.
	if ( pm->state->pmove->pm_flags & PMF_JUMP_HELD ) {
		PM_ForceLegsAnim( LEGS_LANDB );
	} else if ( pm->state->pmove.pm_flags & PMF_TIME_KNOCKBACK ) {
		PM_ForceLegsAnim( LEGS_LAND );
	}
	// Set legs timer.
	pm->ps->legsTimer = TIMER_LAND;
	#endif

	// First make sure to reset it just in case.
	pm->impact_delta = 0.;

	/**
	*	Calculate the exact velocity on landing.
	**/
	// Displacement in Z between current origin and the previous frame origin (how far the player moved vertically).
	const double zDistance = pm->state->pmove.origin.z - pml.previousOrigin.z;
	// Vertical velocity at the start of the move (v0).
	const double zVelocity = pml.previousVelocity.z;
	// Gravity is positive in pmove, so negative here is the acceleration acting on velocity (v' = v + a*t).
	const double acceleration = -pm->state->pmove.gravity;

	// Replace solving the quadratic for t with the kinematic energy relation:
	//   v^2 = v0^2 + 2 * a * s
	// This is numerically simpler, avoids division-by-zero when gravity is zero,
	// and yields the final impact speed squared directly.
	const double impactVelSq = zVelocity * zVelocity + 2.0 * acceleration * zDistance;
	if ( impactVelSq < 0.0 ) {
		// No valid real impact velocity (numerical or pathological case) — bail out.
		return;
	}

	// Use the impact energy-scaled value the original code expected (delta = v^2 * 0.0001).
	double delta = impactVelSq * 0.0001;

	// Ducking while falling doubles damage.
	if ( pm->state->pmove.pm_flags & PMF_DUCKED ) {
		delta *= 2.;
	}
	// Vever take falling damage if completely underwater.
	if ( pm->liquid.level == LIQUID_UNDER ) {
		return;
	}
	// Reduce falling damage if there is with the waist standing in the water.
	if ( pm->liquid.level == LIQUID_WAIST ) {
		delta *= 0.25;
	}
	// Reduce falling damage if there is with the feet standing in the water.
	if ( pm->liquid.level == LIQUID_FEET ) {
		delta *= 0.5;
	}
	// Ignore if negative delta.
	if ( delta < 1. ) {
		return;
	}
	// Apply impact delta.
	pm->impact_delta = delta;

	#if 1
	// create a local entity event to play the sound

	// SURF_NODAMAGE is used for bounce pads where you don't ever
	// want to take damage or play a crunch sound
	//if ( !( pml.groundTrace.surface && pml.groundTrace.surface.flags & SURF_NODAMAGE ) ) {
	{
		if ( delta < 15 ) {
			if ( delta > 7 && !( pm->state->pmove.pm_flags & PMF_ON_LADDER ) ) {
				if ( pm->state->pmove.pm_type == PM_NORMAL ) {
					PM_AddEvent( EV_PLAYER_FOOTSTEP, 0 );
				}
			} else {
				// start footstep cycle over
				//pm->state->bobCycle = 0;
				//SG_DPrintf( "[" SG_GAME_MODULE_STR "]: %s: --  #0  --  pm->state->bobCycle = pm->state->bobCycle;\n", __func__ );
			}
			// To prevent 'landing' events and bobCycle reset. (Slopes.)
			return;
		}

		if ( delta > 30 ) {
			if ( delta >= 55 ) {
				PM_AddEvent( EV_FALL_FAR, delta );
			} else {
				// this is a pain grunt, so don't play it if dead
				//if ( pm->state->stats[ STAT_HEALTH ] > delta ) {
					PM_AddEvent( EV_FALL_MEDIUM, delta );
				//}
			}
		} else {
			PM_AddEvent( EV_FALL_SHORT, delta );
		}
	}
	#endif
	
	
	// Start footstep cycle over
	pm->state->bobCycle = 0;
	//SG_DPrintf( "[" SG_GAME_MODULE_STR "]: %s: --  #1  --  pm->state->bobCycle = 0;\n", __func__ );
}
/**
*	@brief	Will engage into the jump movement.
*	@return	True if we engaged into jumping movement.
**/
static const bool PM_CheckJump() {
	// Hasn't been long enough since landing to jump again.
	if ( pm->state->pmove.pm_flags & PMF_TIME_LAND ) {
		return false;
	}
	// Hasn't been long enough since landing to jump again.
	if ( pm->state->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		return false;
	}

	// Not holding jump.
	//if ( !( pm->cmd.buttons & BUTTON_JUMP ) ) {
	//	pm->state->pmove.pm_flags &= ~PMF_JUMP_HELD;
	//	return false;
	//}
	// Player has let go of jump button.
	if ( !( pm->cmd.buttons & BUTTON_JUMP ) && pm->cmd.upmove < 10 ) {
		pm->state->pmove.pm_flags &= ~PMF_JUMP_HELD;
		return false;
	}
	// Player must wait for jump to be released.
	if ( pm->state->pmove.pm_flags & PMF_JUMP_HELD ) {
		// <Q2RTXP>: WID: Clear so cmdscale doesn't lower running speed.
		pm->cmd.upmove = 0;
		return false;
	}
	// Can't jump while ducked.
	if ( pm->state->pmove.pm_flags & PMF_DUCKED ) {
		return false;
	}
	if ( pm->liquid.level >= LIQUID_WAIST ) {
		pm->ground.entity = nullptr;
		return false;
	}
	// In-air/liquid, so no effect, can't re-jump without ground.
	if ( pm->ground.entity == nullptr 
		|| pml.groundTrace.entityNumber == ENTITYNUM_NONE
		|| ( !pml.hasGroundPlane ) || ( !pml.isWalking ) ) {
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
	PM_AddEvent( EV_JUMP_UP, 0 );

	return true;
}



/**
*
* 
*
*	Animations / BobCycle / (XY/XYZ-)Speed(s):
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
*	@brief	Keeps track of the player's xySpeed and bobCycle:
**/
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
	pm->state->bobCycle = (int32_t)( oldBobCycle + ( pm->state->bobMove * pm->cmd.msec ) /* pml.msec */ ) & 255;

	//SG_DPrintf( "%s: pm->state->bobCycle(%i), oldBobCycle(%i), bobMove(%f)\n", __func__, pm->state->bobCycle, oldBobCycle, bobMove );

	// If we just crossed a cycle boundary, play an appropriate footstep event.
	if ( ( ( oldBobCycle + 64 ) ^ ( pm->state->bobCycle + 64 ) ) & 128 ) {
		// On-ground will only play sounds if running:
		if ( pm->liquid.level == LIQUID_NONE || pm->liquid.level == LIQUID_FEET ) {
			if ( footStep && pm->state->xySpeed > pmp->pm_stop_speed /* && pm->state->xySpeed > 225 */ /*&& !pm->noFootsteps*/ ) {
				//if ( pm->state->pmove.pm_flags & PMF_ON_LADDER ) {
				//	PM_AddEvent( EV_FOOTSTEP_LADDER, 0 );
				//	SG_DPrintf( "[" SG_GAME_MODULE_STR "%s: pm->state->bobCycle(% i), oldBobCycle(% i), bobMove(%lf), Event(EV_FOOTSTEP_LADDER), Time(%" PRId64 ")\n", __func__, pm->state->bobCycle, oldBobCycle, pm->state->bobMove, pm->simulationTime.Milliseconds() );
				//} else {
				if ( pm->state->pmove.pm_type == PM_NORMAL ) {
					PM_AddEvent( EV_PLAYER_FOOTSTEP, 0 /*PM_FootstepForSurface()*/ );
				}
					//SG_DPrintf( "[" SG_GAME_MODULE_STR "%s: pm->state->bobCycle(%i), oldBobCycle(%i), bobMove(%lf), Event(EV_FOOTSTEP), Time(%" PRId64 ")\n", __func__, pm->state->bobCycle, oldBobCycle, pm->state->bobMove, pm->simulationTime.Milliseconds() );
				//}
			}
		}
		// Splashing:
		//} else if ( pm->liquid.level == LIQUID_FEET ) {
		//	PM_AddEvent( EV_FOOTSPLASH );
		//// Wading / Swimming at surface.
		//} else if ( pm->liquid.level == LIQUID_WAIST ) {
		//	PM_AddEvent( EV_SWIM );
		//// None:
		//} else if ( pm->liquid.level == LIQUID_UNDER ) {
		//	// No sound when completely underwater. Lol.
		//}
	}
}

/**
*	@brief	Handles water entry/exit events.
**/
static void PM_WaterEvents( void ) {		// FIXME?
	/**
	*	If just entered a water volume, play a sound
	**/
	if ( !pml.previousLiquid.level && pm->liquid.level ) {
		// Feet Deep Splash:
		if ( pm->liquid.level == cm_liquid_level_t::LIQUID_FEET ) {
			PM_AddEvent( EV_WATER_ENTER_FEET, pm->liquid.type );
		// Waist Deep Splash:
		} else if ( pm->liquid.level >= cm_liquid_level_t::LIQUID_WAIST ) {
			PM_AddEvent( EV_WATER_ENTER_WAIST, pm->liquid.type );
		}
	}
	/**
	*	If just completely exited a water volume, play a sound
	**/
	if ( pml.previousLiquid.level && !pm->liquid.level ) {
		// Feet Deep Splash:
		if ( pml.previousLiquid.level == cm_liquid_level_t::LIQUID_FEET ) {
			PM_AddEvent( EV_WATER_LEAVE_FEET, pml.previousLiquid.type );
			// Waist Deep Splash:
		} else if ( pml.previousLiquid.level >= cm_liquid_level_t::LIQUID_WAIST ) {
			PM_AddEvent( EV_WATER_LEAVE_WAIST, pml.previousLiquid.type );
		}
	}
	/**
	*	Check for head just going under water
	**/
	if ( pml.previousLiquid.level < cm_liquid_level_t::LIQUID_UNDER && pm->liquid.level == cm_liquid_level_t::LIQUID_UNDER ) {
		PM_AddEvent( EV_WATER_ENTER_HEAD, pm->liquid.type );
	}
	/**
	*	check for head just coming out of water
	**/
	if ( pml.previousLiquid.level == cm_liquid_level_t::LIQUID_UNDER && pm->liquid.level != cm_liquid_level_t::LIQUID_UNDER ) {
		PM_AddEvent( EV_WATER_LEAVE_HEAD, pm->liquid.type );
	}
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
	// We want ground before we 'slide' like a dead boss yo dawg.
	if ( !pm->ground.entity ) {
		return;
	}

	// Add extra friction for our dead body.
	const double forwardScalar = QM_Vector3Length( pm->state->pmove.velocity ) - 20.;

	// Stop the dead body completely if forward becomes negative.
	if ( forwardScalar <= 0 ) {
		pm->state->pmove.velocity = QM_Vector3Zero();
	// Still have some velocity left, so scale it down.
	} else {
		// Normalize.
		pm->state->pmove.velocity = QM_Vector3Normalize( pm->state->pmove.velocity );
		// Scale by old velocity derived length.
		pm->state->pmove.velocity *= forwardScalar;
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
	Vector3 wishDirection = {};

	// Calculate the vertical scale based on the view pitch .
	// (Forward Angle Vector Z Component).
	double upScale = ( pml.forward.z + 0.5 ) * 2.5;
	// Scale it to not be too fast.
	if ( upScale > 1.0 ) {
		upScale = 1.0;
	} else if ( upScale < -1.0 ) {
		upScale = -1.0;
	}

	// Only use horizontal components for forward/right anglevectors..
	pml.forward = QM_Vector3Normalize( Vector2( pml.forward ) );
	pml.right	= QM_Vector3Normalize( Vector2( pml.right ) );

	// Scale the user command movements.
	const double cmdScale = PM_CmdScale( &pm->cmd );
	// Clamp the forwardmove to not be faster than the ladder speed.
	const double forwardMove = std::clamp( (double)pm->cmd.forwardmove, -pmp->pm_ladder_speed, pmp->pm_ladder_speed );

	// Determine the wish velocity.
	Vector3 wishVelocity = {};
	if ( forwardMove ) {
		wishVelocity[ 2 ] = 0.9 * upScale * cmdScale * (float)forwardMove;
	}
	// Allows for strafing on ladder.
	if ( pm->cmd.sidemove ) {
		// Clamp the speed to not be faster than the ladder side speed.
		const double sideMove = std::clamp( (double)pm->cmd.sidemove, -pmp->pm_ladder_sidemove_speed, pmp->pm_ladder_sidemove_speed );
		// Add the side movement.
		wishVelocity = QM_Vector3MultiplyAdd( wishVelocity, 0.5 * cmdScale * sideMove, pml.right );
	}

	// Apply friction
	PM_Friction();
	// Snap very small velocities to zero
	if ( pm->state->pmove.velocity.x < 1 && pm->state->pmove.velocity.x > -1 ) {
		pm->state->pmove.velocity.x = 0;
	}
	if ( pm->state->pmove.velocity.y < 1 && pm->state->pmove.velocity.y > -1 ) {
		pm->state->pmove.velocity.y = 0;
	}

	// Calculate the wish direction.
	const double wishSpeed = VectorNormalize2( &wishVelocity.x, &wishDirection.x );
	// Accelerate.
	PM_Accelerate( wishDirection, wishSpeed, pmp->pm_accelerate );

	// Limit the horizontal speed to the ladder speed.
	if ( !wishVelocity.z ) {
		// Apply gravity as a form of 'friction' to prevent 'racing/sliding' against the ladder.
		if ( pm->state->pmove.velocity.z > 0 ) {
			pm->state->pmove.velocity.z -= pm->state->pmove.gravity * pml.frameTime;
			if ( pm->state->pmove.velocity.z < 0 ) {
				pm->state->pmove.velocity.z = 0;
			}
		// If we're not trying to go up, then slide down.
		} else {
			pm->state->pmove.velocity.z += pm->state->pmove.gravity * pml.frameTime;
			if ( pm->state->pmove.velocity.z > 0 ) {
				pm->state->pmove.velocity.z = 0;
			}
		}
	}

	// Last but not least, step it.
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
	// Already got time set, occupied with something else.
	if ( pm->state->pmove.pm_time ) {
		return false;
	}

	pm->state->pmove.pm_flags &= ~PMF_ON_LADDER;

	// Check for ladder.
	Vector3 flatForward = QM_Vector3Normalize( Vector2( pml.forward ) );
	// Spot to test for possible ladder we're riding.
	Vector3 spot = pm->state->pmove.origin + ( flatForward * 1 );
	// Perform trace.
	cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );
	if ( ( trace.fraction < 1. ) && ( trace.contents & CONTENTS_LADDER ) && pm->liquid.level < LIQUID_WAIST ) {
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
	//if ( pm->liquid.level < LIQUID_WAIST ) {
		return false;
	}

	/**
	*	Check whether something is blocking us up/forward.
	**/
	static constexpr double PM_WATERJUMP_FORWARD_VELOCITY = 100.;
	static constexpr double PM_WATERJUMP_UP_VELOCITY = 350.;
	static constexpr double PM_WATERJUMP_TIME = 24.;

	// The distance ahead to check for obstructions.
	double PM_WATERJUMP_PREDICT_DIST = QM_BBox3Distance( BBox3( Vector2( pm->mins ), Vector2( pm->maxs ) ) );
	
	// Perform the trace.
	trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, pm->state->pmove.origin + ( flatForward * PM_WATERJUMP_PREDICT_DIST ), CM_CONTENTMASK_SOLID );
	// We aren't blocked, or what we're blocked by is something we can walk up.
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] >= PM_STEP_MIN_NORMAL ) {
		return false;
	}

	// Test for any obstructions above us near the forward destination spot.
	spot = QM_Vector3MultiplyAdd( pm->state->pmove.origin, PM_WATERJUMP_PREDICT_DIST, flatForward );
	spot[ 2 ] += 4;
	cm_contents_t destSpotContents = PM_PointContents( spot /*, skip = playerEntity */ );
	if ( !( destSpotContents & CONTENTS_SOLID ) /*&& !( cont & CONTENTS_PLAYER)*/ ) {
		return false;
	}
	// Another test, but a little higher.
	spot[ 2 ] += 16;
	destSpotContents = PM_PointContents( spot /*, skip = playerEntity */ );
	if ( destSpotContents /* && !( cont & CONTENTS_PLAYER )*/ ) {
		return false;
	}

	/**
	*	Test if we can predict to reach where we want to be.
	**/
	// Jump up and forwards and out of the water
	pm->state->pmove.velocity = flatForward * PM_WATERJUMP_FORWARD_VELOCITY;
	pm->state->pmove.velocity.z = PM_WATERJUMP_UP_VELOCITY;
	#if 1
	// Time water.
	pm->state->pmove.pm_time = PM_WATERJUMP_TIME;
	// WaterJump and land flags.
	pm->state->pmove.pm_flags |= PMF_TIME_WATERJUMP | PMF_TIME_LAND;
	// 'Fake' the jump key held, so there won't be any immediate jump upon the
	// landing on surface. (ground surface, floor entity, you know?)
	pm->state->pmove.pm_flags |= PMF_JUMP_HELD;
	#endif
	// Predict the move ahead in time, no gravity applied.
	pm_slidemove_flags_t predictMoveResults = PM_PredictStepMove( pm, &pml, false );
	// What matters is only, did we step up to a surface from mid-air?
	if ( ( predictMoveResults & PM_SLIDEMOVEFLAG_STEPPED ) != 0 && pm->liquid.level >= LIQUID_WAIST ) {
		#if 1
		// Jump up and forwards and out of the water
		pm->state->pmove.velocity = flatForward * PM_WATERJUMP_FORWARD_VELOCITY;
		pm->state->pmove.velocity.z = PM_WATERJUMP_UP_VELOCITY;
		#endif
		// Time water.
		pm->state->pmove.pm_time = PM_WATERJUMP_TIME;
		// WaterJump and land flags.
		pm->state->pmove.pm_flags |= PMF_TIME_WATERJUMP | PMF_TIME_LAND;
		// 'Fake' the jump key held, so there won't be any immediate jump upon the
		// landing on surface. (ground surface, floor entity, you know?)
		pm->state->pmove.pm_flags |= PMF_JUMP_HELD;
	} else {
		//pm->state->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_JUMP_HELD );
		//pm->state->pmove.pm_time = 0;
		//if ( pm->state->pmove.velocity[ 2 ] < 0 ) {
		//	// cancel as soon as we are falling down again
		//	pm->state->pmove.pm_flags &= ~PMF_ALL_TIMES;
		//	pm->state->pmove.pm_time = 0;
		//}
		return false;
	}
	return true;
}
/**
*	@brief	
**/
static void PM_WaterJumpMove( void ) {
	// Waterjump has no control, but falls.
	PM_StepSlideMove_Generic(
		pm,
		&pml,
		false
	);
	// Decrease gravity (not in stepslide move).
	pm->state->pmove.velocity.z-= pm->state->pmove.gravity * pml.frameTime;
	//if ( pm->state->pmove.velocity[ 2 ] < 0 ) {
	if ( pm->state->pmove.velocity.z < 0. ) {
		// cancel as soon as we are falling down again
		pm->state->pmove.pm_flags &= ~PMF_ALL_TIMES;
		pm->state->pmove.pm_time = 0.;
	}
}
/**
*	@brief	Performs in-water movement.
**/
static void PM_WaterMove() {
	// First check if we are (going-) in a water jump.
	if ( PM_CheckWaterJump() ) {
		// Perform water jump movement.
		PM_WaterJumpMove();
		// Escape.
		return;
	}

	// Apply Friction.
	PM_Friction();

	// Scale the cmd.
	usercmd_t scaledCmd = pm->cmd;
	// Get the movement scale.
	const double cmdScale = PM_CmdScale( &scaledCmd );
	// Get forward and side move.
	double forwardMove = scaledCmd.forwardmove;
	double sideMove = scaledCmd.sidemove;

	//
	// user cmd intentions
	//
	Vector3 wishVelocity = QM_Vector3Zero();
	if ( cmdScale ) {
		// Scale wish velocity.
		wishVelocity = cmdScale * pml.forward * pm->cmd.forwardmove + cmdScale * pml.right * pm->cmd.sidemove;
		// Ensure to add up move.
		wishVelocity[ 2 ] += cmdScale * pm->cmd.upmove;
	// Sink towards bottom by default:
	} else {
		// Only if not on ground. Otherwise it'll nudge the origin up and down, causing jitter.
		if ( !pml.hasGroundPlane && pm->ground.entity == nullptr ) {
			wishVelocity[ 2 ] = -60.;
		}
	}

	// Add currents.
	PM_AddCurrents( wishVelocity );

	// Determine the speed of the movement.
	Vector3 wishDirection = wishVelocity;
	double wishSpeed = QM_Vector3NormalizeLength( wishDirection );
	// Cap speed.
	constexpr double pm_swimScale = 0.5;
	if ( wishSpeed > pm->state->pmove.speed * pm_swimScale ) {
		wishSpeed = pm->state->pmove.speed * pm_swimScale;
	}

	// Water acceleration.
	PM_Accelerate( wishDirection, wishSpeed, pmp->pm_water_accelerate );

	// Make sure we can go up slopes easily under water.
	if ( pml.hasGroundPlane && QM_Vector3DotProduct( pm->state->pmove.velocity, pml.groundTrace.plane.normal ) < 0. ) {
		// Get velocity length.
		const double velocityLength = QM_Vector3Length( pm->state->pmove.velocity );
		// slide along the ground plane
		PM_BounceClipVelocity( pm->state->pmove.velocity, pml.groundTrace.plane.normal,
			pm->state->pmove.velocity, PM_OVERCLIP );
		// Now normalize it.
		pm->state->pmove.velocity = QM_Vector3Normalize( pm->state->pmove.velocity );
		// And scale it accordingly.
		pm->state->pmove.velocity = QM_Vector3Scale( pm->state->pmove.velocity, velocityLength );
	}

	// Step Slide.
	PM_SlideMove_Generic(
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
static void PM_FlyMove( bool noClip = false ) {
	// Apply Friction.
	PM_Friction();

	// Scale the cmd.
	usercmd_t scaledCmd = pm->cmd;
	// Get the movement scale.
	const double cmdScale = PM_CmdScale( &scaledCmd );
	// Get forward and side move.
	double forwardMove = scaledCmd.forwardmove;
	double sideMove = scaledCmd.sidemove;

	// project moves down to flat plane
	Vector3 wishVelocity = {};
	if ( cmdScale ) {
		wishVelocity = cmdScale * pml.forward * forwardMove + cmdScale * pml.right * sideMove;
		wishVelocity[ 2 ] += cmdScale * scaledCmd.upmove;
	}

	// Determine the speed of the movement.
	Vector3 wishDirection = wishVelocity;
	const double wishSpeed = QM_Vector3NormalizeLength( wishDirection );
	//wishSpeed *= cmdScale;
	 
	// <Q2RTXP>: WID: This pm_air_accelerate is a boolean cvar lol, we can't use it like q3.
	//PM_Accelerate( wishDirection, wishSpeed, pmp->pm_air_accelerate );
	
	// not on ground, so little effect on velocity
	PM_Accelerate( wishDirection, wishSpeed, pmp->pm_fly_accelerate );

	if ( !noClip ) {
		// We may have a ground plane that is very steep.
		// Even though we don't have a groundentity, slide along the steep plane
		if ( pml.hasGroundPlane ) {
			PM_BounceClipVelocity( pm->state->pmove.velocity, pml.groundTrace.plane.normal,
				pm->state->pmove.velocity, PM_OVERCLIP );
		}

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
*	MoveType: Air Move
*
*
*
**/
/**
*	@brief
**/
static void PM_AirMove( void ) {
	// Apply Friction.
	PM_Friction();

	// Scale the cmd.
	usercmd_t scaledCmd = pm->cmd;
	// Get the movement scale.
	const double cmdScale = PM_CmdScale( &scaledCmd );
	// Get forward and side move.
	double forwardMove = scaledCmd.forwardmove;
	double sideMove = scaledCmd.sidemove;

	// Project moves down to flat plane.
	//Vector3 wishVelocity = {};
	//if ( cmdScale ) {
	//	wishVelocity = cmdScale * pml.forward * forwardMove + cmdScale * pml.right * sideMove;
	//	wishVelocity[ 2 ] += cmdScale * scaledCmd.upmove;
	//}

	// set the movementDir so clients can rotate the legs for strafing
	//PM_Animation_SetMovementDirection();

	// project moves down to flat plane
	pml.forward = QM_Vector3Normalize( Vector2( pml.forward ) );
	pml.right = QM_Vector3Normalize( Vector2( pml.right ) );
	Vector3 wishVelocity = {};
	for ( int32_t i = 0; i < 2; i++ ) {
		wishVelocity[ i ] = pml.forward[ i ] * forwardMove + pml.right[ i ] * sideMove;
	}
	wishVelocity[ 2 ] = 0;

	// Add currents.
	PM_AddCurrents( wishVelocity );
	
	// set the movementDir so clients can rotate the legs for strafing
	PM_Animation_SetMovementDirection();

	// Determine the speed of the movement.
	Vector3 wishDirection = wishVelocity;
	double wishSpeed = QM_Vector3NormalizeLength( wishDirection );
	// Scale it.
	wishSpeed *= cmdScale;

	// not on ground, so little effect on velocity
	//PM_Accelerate( wishDirection, wishSpeed, pmp->pm_air_accelerate );
	// <Q2RTXP>: WID: This pm_air_accelerate is a boolean cvar lol, we can't use it like q3.

	PM_Accelerate( wishDirection, wishSpeed, pmp->pm_air_accelerate );

	// we may have a ground plane that is very steep, even
	// though we don't have a groundentity
	// slide along the steep plane
	if ( pml.hasGroundPlane ) {
		PM_BounceClipVelocity( pm->state->pmove.velocity, pml.groundTrace.plane.normal,
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

	PM_StepSlideMove_Generic(
		pm,
		&pml,
		true
	);
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
/**
*	@brief
**/
static void PM_WalkMove( const bool canJump ) {
	// Watermove.
	//if ( pm->liquid.level > LIQUID_WAIST && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 ) {
	if ( pm->liquid.level >= LIQUID_WAIST && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 ) {
		PM_WaterMove();
		return;
	}
	// If we CAN jump, and want to, do so.
	// Then engage into jumping, to then pursuit either water(- move/jumping) or airmoving.
	if ( canJump && PM_CheckJump() ) {
		if ( pm->liquid.level > LIQUID_FEET ) {
			PM_WaterMove();
		} else {
			PM_AirMove();
		}
		return;
	}

	// Always apply friction.
	PM_Friction();
	// Get the movement amounts.
	double forwardMove = pm->cmd.forwardmove;
	double sideMove = pm->cmd.sidemove;

	// Scale the cmd.
	usercmd_t scaledUserCmd = pm->cmd;
	double cmdScale = PM_CmdScale( &scaledUserCmd );

	// set the movementDir so clients can rotate the legs for strafing
	PM_Animation_SetMovementDirection();

	// Project moves down to flat plane
	pml.forward = Vector2( pml.forward );
	pml.right = Vector2( pml.right );
	// project the forward and right directions onto the ground plane
	PM_BounceClipVelocity( pml.forward, pml.groundTrace.plane.normal, pml.forward, PM_OVERCLIP );
	PM_BounceClipVelocity( pml.right, pml.groundTrace.plane.normal, pml.right, PM_OVERCLIP );
	// Normalize them.
	pml.forward = QM_Vector3Normalize( pml.forward );
	pml.right = QM_Vector3Normalize( pml.right );

	Vector3 wishVelocity = pml.forward * forwardMove + pml.right * sideMove;

	// when going up or down slopes the wish velocity should Not be zero
	//	wishVelocity[2] = 0;
	// 
	// Add currents.
	PM_AddCurrents( wishVelocity );

	// Determine the speed of the movement.
	Vector3 wishDirection = wishVelocity;
	double wishSpeed = QM_Vector3NormalizeLength( wishDirection );
	wishSpeed *= cmdScale;

	// Clamp the speed lower if ducking
	if ( pm->state->pmove.pm_flags & PMF_DUCKED ) {
		constexpr double pm_duckScale = 0.25;
		if ( wishSpeed > pm->state->pmove.speed * pm_duckScale ) {
			wishSpeed = pm->state->pmove.speed * pm_duckScale;
		}
	}
	// Clamp the speed lower if wading or walking on the bottom.
	if ( pm->liquid.level ) {
		constexpr double pm_swimScale = 0.5;
		double waterScale = (double)( pm->liquid.level ) / 3.0;
		waterScale = 1.0 - ( 1.0 - pm_swimScale ) * waterScale;
		if ( wishSpeed > pm->state->pmove.speed * waterScale ) {
			wishSpeed = pm->state->pmove.speed * waterScale;
		}
	}

	// Default acceleration.
	double accelerate = pmp->pm_accelerate;
	// When a player gets hit, they temporarily lose full control, which allows them to be moved a bit.
	if ( ( pml.groundTrace.surface && pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) || pm->state->pmove.pm_flags & PMF_TIME_KNOCKBACK ) {
		accelerate = pmp->pm_air_accelerate;
	}
	// Accelerate.
	PM_Accelerate( wishDirection, wishSpeed, accelerate );

	//Com_Printf("velocity = %1.1f %1.1f %1.1f\n", pm->ps->velocity[0], pm->ps->velocity[1], pm->ps->velocity[2]);
	//Com_Printf("velocity1 = %1.1f\n", VectorLength(pm->ps->velocity));

	if ( ( pml.groundTrace.surface && pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) || pm->state->pmove.pm_flags & PMF_TIME_KNOCKBACK ) {
		pm->state->pmove.velocity.z -= pm->state->pmove.gravity * pml.frameTime;
	} else {
		// don't reset the z velocity for slopes
//		pm->ps->velocity[2] = 0;
	}

	const double velocityLength = QM_Vector3Length( pm->state->pmove.velocity );

	// slide along the ground plane
	PM_BounceClipVelocity( pm->state->pmove.velocity, pml.groundTrace.plane.normal,
		pm->state->pmove.velocity, PM_OVERCLIP );

	// don't decrease velocity when going up or down a slope
	VectorNormalize( &pm->state->pmove.velocity.x );
	pm->state->pmove.velocity = QM_Vector3Scale( pm->state->pmove.velocity, velocityLength );

	// don't do anything if standing still
	static constexpr double STOP_EPSILON = 0.05;
	if ( !( pm->state->pmove.velocity[ 0 ] ) && !( pm->state->pmove.velocity[ 1 ] ) ) {
		return;
	}
	
	// Step Slide.
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
	if ( playerState->pmove.pm_type == PM_INTERMISSION || playerState->pmove.pm_type == PM_SPINTERMISSION ) {
		return;		// no view changes at all
	}
	// Dead, or Gibbed.
	if ( playerState->pmove.pm_type != PM_SPECTATOR && playerState->stats[ STAT_HEALTH ] <= 0 ) {
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
*	@brief	Will look for whether we're latching on to a ladder.
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
	cm_trace_t trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );
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
*	@brief	Drops the movement pm_time factor for movement states, and clears its flags when time is up.
**/
static void PM_DropTimers() {
	if ( pm->state->pmove.pm_time ) {

		//--
		// This is troublesome also, for obvious reasons.
		//double msec = ( pm->cmd.msec < 0. ? ( 1.0 / 0.125 ) : (pm->cmd.msec / 0.125 ) );
		//--
		// This obviously isn't going to help for the accuracy we needed.
		//int32_t msec = (int32_t)pm->cmd.msec >> 3;
		//--
		// This was <= 0, this seemed to cause a slight jitter in the player movement.
		//if ( pm->cmd.msec < 0. ) {
		//	msec = 1.0 / 8.; // 8 ms = 1 unit. (At 10hz.)
		//}
		//--
		// So we came up with this instead.
		const double msec = ( pm->cmd.msec < 0. ? ( 1.0 / 8. ) : ( pm->cmd.msec / 8. ) );
		//--
		
		// If we went past the set time, reset it and all related time flags.
		if ( msec >= pm->state->pmove.pm_time ) {
			// Remove all time related flags.
			pm->state->pmove.pm_flags &= ~( PMF_ALL_TIMES );
			// Reset time engagement for the player movement state.
			pm->state->pmove.pm_time = 0;
		// Otherwise, substract it by msec units.
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

	// Save the origin as 'old origin' for in case we get stuck.
	pml.previousOrigin = pm->state->pmove.origin;
	// Save the start velocity.
	pml.previousVelocity = pm->state->pmove.velocity;

	// Calculate frameTime.
	pml.frameTime = pm->cmd.msec * 0.001;

	// Clamp view angles.
	PM_UpdateViewAngles( pm->state, &pm->cmd );

	/**
	*	PM_DEAD:
	**/
	// Erase all input command state when dead, we don't want to allow user input to be moving our dead body.
	if ( pm->state->pmove.pm_type >= PM_DEAD ) {
		PM_EraseInputCommandState();
	}

	/**
	*	PM_SPECTATOR / PM_NOCLIP:
	**/
	// Performs fly move, only clips in case of spectator mode, noclips otherwise.
	if ( pm->state->pmove.pm_type == PM_SPECTATOR || pm->state->pmove.pm_type == PM_NOCLIP ) {
		// Re-ensure no flags are set anymore.
		pm->state->pmove.pm_flags = PMF_NONE;
		// Set mins, maxs, and viewheight.
		PM_UpdateBoundingBox();
		// Get moving.
		//pml.frameTime = pmp->pm_fly_speed * pm->cmd.msec * 0.001f;
		PM_FlyMove( pm->state->pmove.pm_type == PM_NOCLIP );
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
		// No movement at all.
		return;
	}
	/**
	*	PM_INTERMISSION / PM_SPINTERMISSION:
	**/
	if ( pm->state->pmove.pm_type == PM_INTERMISSION || pm->state->pmove.pm_type == PM_SPINTERMISSION ) {
		// No movement at all.
		return;
	}

	/**
	*	Recatagorize bounding box, and the liquid(contents) state.
	**/
	// Set mins, maxs, and viewheight.
	PM_UpdateBoundingBox();
	// Get liquid.level, accounting for ducking.
	PM_GetLiquidContentsForPoint( pm->state->pmove.origin, pm->liquid.level, pm->liquid.type );
	// Back it up as the first previous liquid. (Start of frame position.)
	pml.previousLiquid = pm->liquid;

	/**
	*	Check if we want, and CAN engage into ducking movement. 
	*	Recatagorize bounding box, and the liquid(contents) state.
	**/
	if ( PM_CheckDuck() ) {
		// Set mins, maxs, and viewheight.
		PM_UpdateBoundingBox();
		// Get liquid.level, accounting for ducking.
		PM_GetLiquidContentsForPoint( pm->state->pmove.origin, pm->liquid.level, pm->liquid.type );
		// Back it up as the first previous liquid. (Start of frame position.)
		pml.previousLiquid = pm->liquid;
	}
	/**
	*	Seek for ground.
	**/
	// Determine and update the current ground entity.
	PM_GroundTrace();

	/**
	*	PM_DEAD:
	**/
	// When dead, perform dead movement.
	if ( pm->state->pmove.pm_type == PM_DEAD ) {
		PM_DeadMove();
	}

	/**
	*	Actual Movement:
	**/
	// Look for whether we're latching on to a ladder.
	PM_CheckSpecialMovement();
	// Drop Timers:
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
	// Air/Ground(Walk-) Move.
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
			PM_WalkMove( true );
		} else {
			// Perform move.
			PM_AirMove();
		}
	}

	/**
	*	Recategorize position for ground, and/or liquid(contents) since we've made a move.
	**/
	// Get and set ground entity.
	PM_GroundTrace();
	// Get liquid.level, accounting for ducking.
	PM_GetLiquidContentsForPoint( pm->state->pmove.origin, pm->liquid.level, pm->liquid.type );

	/**
	*	Apply the screen contents effects.
	**/
	// Apply contents and other screen effects.
	PM_UpdateScreenContents();

	/**
	*	Weapons.
	**/
	//PM_Weapon();

	/**
	*	Torso animation.
	**/
	//PM_TorsoAnimation();

	/**
	*	Bob Cycle / Footstep events / legs animations.
	**/
	//PM_Footsteps();
	PM_CycleBob();

	/**
	*	Entering / Leaving water splashes.
	**/
	PM_WaterEvents();

	/**
	*	Snap us back into a validated position.
	**/
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
	pmp->pm_air_accelerate = atof( (const char *)SG_GetConfigString( CS_AIRACCEL ) );
	if ( pmp->pm_air_accelerate <= 0 ) {
		pmp->pm_air_accelerate = default_pmoveParams_t::pm_air_accelerate;
	}
	pmp->pm_fly_accelerate = default_pmoveParams_t::pm_fly_accelerate;
	pmp->pm_water_accelerate = default_pmoveParams_t::pm_water_accelerate;

	pmp->pm_friction = default_pmoveParams_t::pm_friction;
	pmp->pm_water_friction = default_pmoveParams_t::pm_water_friction;
	pmp->pm_fly_friction = default_pmoveParams_t::pm_fly_friction;
}
