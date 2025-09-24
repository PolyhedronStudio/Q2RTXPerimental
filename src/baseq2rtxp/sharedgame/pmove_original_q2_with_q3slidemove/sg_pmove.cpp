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

//! Pointer to the pmove's playerState.
static player_state_t *ps;
//! An actual pointer to the pmove parameters object for use with moving.
static pmoveParams_t *pmp;

//static pml_t pml;

#ifdef PMOVE_Q2RE_STUCK_FIXES
enum class stuck_result_t {
	GOOD_POSITION,
	FIXED,
	NO_GOOD_POSITION
};

//using stuck_object_trace_fn_t = trace_t( const vec3_t &, const vec3_t &, const vec3_t &, const vec3_t & );

// [Paril-KEX] generic code to detect & fix a stuck object
stuck_result_t G_FixStuckObject_Generic( Vector3 &origin, const Vector3 &own_mins, const Vector3 &own_maxs ) {
	if ( !PM_Trace( origin, own_mins, own_maxs, origin ).startsolid )
		return stuck_result_t::GOOD_POSITION;

	struct {
		float distance;
		Vector3 origin;
	} good_positions[ 6 ];
	size_t num_good_positions = 0;

	constexpr struct {
		Vector3 normal;
		Vector3 mins, maxs;
	} side_checks[] = {
		{ { 0, 0, 1 }, { -1, -1, 0 }, { 1, 1, 0 } },
		{ { 0, 0, -1 }, { -1, -1, 0 }, { 1, 1, 0 } },
		{ { 1, 0, 0 }, { 0, -1, -1 }, { 0, 1, 1 } },
		{ { -1, 0, 0 }, { 0, -1, -1 }, { 0, 1, 1 } },
		{ { 0, 1, 0 }, { -1, 0, -1 }, { 1, 0, 1 } },
		{ { 0, -1, 0 }, { -1, 0, -1 }, { 1, 0, 1 } },
	};

	for ( size_t sn = 0; sn < q_countof( side_checks ); sn++ ) {
		auto &side = side_checks[ sn ];
		Vector3 start = origin;
		Vector3 mins{}, maxs{};

		for ( size_t n = 0; n < 3; n++ ) {
			if ( side.normal[ n ] < 0 )
				start[ n ] += own_mins[ n ];
			else if ( side.normal[ n ] > 0 )
				start[ n ] += own_maxs[ n ];

			if ( side.mins[ n ] == -1 )
				mins[ n ] = own_mins[ n ];
			else if ( side.mins[ n ] == 1 )
				mins[ n ] = own_maxs[ n ];

			if ( side.maxs[ n ] == -1 )
				maxs[ n ] = own_mins[ n ];
			else if ( side.maxs[ n ] == 1 )
				maxs[ n ] = own_maxs[ n ];
		}

		cm_trace_t tr = PM_Trace( start, mins, maxs, start );

		int8_t needed_epsilon_fix = -1;
		int8_t needed_epsilon_dir;

		if ( tr.startsolid ) {
			for ( size_t e = 0; e < 3; e++ ) {
				if ( side.normal[ e ] != 0 )
					continue;

				Vector3 ep_start = start;
				ep_start[ e ] += 1;

				tr = PM_Trace( ep_start, mins, maxs, ep_start );

				if ( !tr.startsolid ) {
					start = ep_start;
					needed_epsilon_fix = e;
					needed_epsilon_dir = 1;
					break;
				}

				ep_start[ e ] -= 2;
				tr = PM_Trace( ep_start, mins, maxs, ep_start );

				if ( !tr.startsolid ) {
					start = ep_start;
					needed_epsilon_fix = e;
					needed_epsilon_dir = -1;
					break;
				}
			}
		}

		// no good
		if ( tr.startsolid )
			continue;

		Vector3 opposite_start = origin;
		auto &other_side = side_checks[ sn ^ 1 ];

		for ( size_t n = 0; n < 3; n++ ) {
			if ( other_side.normal[ n ] < 0 )
				opposite_start[ n ] += own_mins[ n ];
			else if ( other_side.normal[ n ] > 0 )
				opposite_start[ n ] += own_maxs[ n ];
		}

		if ( needed_epsilon_fix >= 0 )
			opposite_start[ needed_epsilon_fix ] += needed_epsilon_dir;

		// potentially a good side; start from our center, push back to the opposite side
		// to find how much clearance we have
		tr = PM_Trace( start, mins, maxs, opposite_start );

		// ???
		if ( tr.startsolid )
			continue;

		// check the delta
		Vector3 end = tr.endpos;
		// push us very slightly away from the wall
		end += Vector3{ (float)side.normal[ 0 ], (float)side.normal[ 1 ], (float)side.normal[ 2 ] } *0.125f;

		// calculate delta
		const Vector3 delta = end - opposite_start;
		Vector3 new_origin = origin + delta;

		if ( needed_epsilon_fix >= 0 )
			new_origin[ needed_epsilon_fix ] += needed_epsilon_dir;

		tr = PM_Trace( new_origin, own_mins, own_maxs, new_origin );

		// bad
		if ( tr.startsolid )
			continue;

		good_positions[ num_good_positions ].origin = new_origin;
		good_positions[ num_good_positions ].distance = QM_Vector3LengthSqr(delta);
		num_good_positions++;
	}

	if ( num_good_positions ) {
		std::sort( &good_positions[ 0 ], &good_positions[ num_good_positions - 1 ], []( const auto &a, const auto &b ) { return a.distance < b.distance; } );

		origin = good_positions[ 0 ].origin;

		return stuck_result_t::FIXED;
	}

	return stuck_result_t::NO_GOOD_POSITION;
}
#endif // PMOVE_Q2RE_STUCK_FIXES



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
static void PM_UpdateGroundFromTrace( const cm_trace_t *trace ) {
	if ( trace == nullptr || trace->entityNumber == ENTITYNUM_NONE ) {
		pm->ground.entity = nullptr;
		pm->ground.plane = {};
		pm->ground.surface = {};
		pm->ground.contents = CONTENTS_NONE;
		pm->ground.material = nullptr;

		//pml.groundTrace.surface = nullptr;
		//pml.groundTrace.contents = CONTENTS_NONE;
	} else {
		pm->ground.entity = (struct edict_ptr_t*)SG_GetEntityForNumber( trace->entityNumber );
		pm->ground.plane = trace->plane;
		pm->ground.surface = *trace->surface;
		pm->ground.contents = trace->contents;
		pm->ground.material = trace->material;

		//pml.groundTrace.surface = &pm->ground.surface;
		//pml.groundTrace.contents = trace->contents;
	}
}
/**
*	@brief	Sets the player's movement bounding box depending on various state factors.
**/
static void PM_SetDimensions() {
	// Start with the default bbox for standing straight up.
	pm->mins = PM_BBOX_STANDUP_MINS;
	pm->maxs = PM_BBOX_STANDUP_MAXS;
	ps->pmove.viewheight = PM_VIEWHEIGHT_STANDUP;

	// Specifical gib treatment.
	if ( ps->pmove.pm_type == PM_GIB ) {
		pm->mins = PM_BBOX_GIBBED_MINS;
		pm->maxs = PM_BBOX_GIBBED_MAXS;
		ps->pmove.viewheight = PM_VIEWHEIGHT_GIBBED;
		return;
	}

	#ifndef PMOVE_EASE_BBOX_AND_VIEWHEIGHT
	// Dead BBox:
	if ( ( ps->pmove.pm_flags & PMF_DUCKED ) || ps->pmove.pm_type == PM_DEAD ) {
		//! Set bounds to ducked.
		pm->mins = PM_BBOX_DUCKED_MINS;
		pm->maxs = PM_BBOX_DUCKED_MAXS;
		// Viewheight to ducked.
		ps->pmove.viewheight = PM_VIEWHEIGHT_DUCKED;
	}
	#else
	// Eased Lerp Factor.
	double easeLerpFactor = 0.f;

	// Ducking Down:
	if ( ( ps->pmove.pm_flags & PMF_DUCKED ) || ps->pmove.pm_type == PM_DEAD ) {
		// Duck BBox:
		pm->mins = PM_BBOX_DUCKED_MINS;
		pm->maxs = PM_BBOX_DUCKED_MAXS;
		pm->state->pmove.viewheight = PM_VIEWHEIGHT_DUCKED;
		// Ease in.
		if ( pm->easeDuckHeight.GetEaseMode() <= QMEaseState::QM_EASE_STATE_MODE_DONE ) {
			easeLerpFactor = pm->easeDuckHeight.EaseIn( pm->simulationTime, QM_CubicEaseIn );
		}
	// Standing Up:
	} else {
		// Standing BBox:
		pm->mins = PM_BBOX_STANDUP_MINS;
		pm->maxs = PM_BBOX_STANDUP_MAXS;
		pm->state->pmove.viewheight = PM_VIEWHEIGHT_STANDUP;
		// Ease out.
		if ( pm->easeDuckHeight.GetEaseMode() <= QMEaseState::QM_EASE_STATE_MODE_DONE ) {
			easeLerpFactor = pm->easeDuckHeight.EaseOut( pm->simulationTime, QM_CubicEaseOut );
		}
	}

	//! Set bounds to stamdup.
	//pm->mins.z = pm->mins.z * easeLerpFactor;
	pm->maxs.z = pm->maxs.z * easeLerpFactor;
	//pm->maxs.z = pm->maxs.z * easeLerpFactor;
	// Viewheight to ducked.
	// Lerp it.
	ps->pmove.viewheight = ps->pmove.viewheight * easeLerpFactor;// PM_VIEWHEIGHT_DUCKED + ( easeLerpFactor * ( PM_VIEWHEIGHT_STANDUP - PM_VIEWHEIGHT_DUCKED ) );
	SG_DPrintf( "%s: easeLerpFactor(%f)\n", __func__,  easeLerpFactor );
	#endif
}
#if 1
/**
*	@brief	Inline-wrapper to for convenience.
**/
void PM_AddEvent( const uint8_t newEvent, const uint8_t parameter ) {
	SG_PlayerState_AddPredictableEvent( newEvent, parameter, ps );
}
#endif

/**
*	@brief	Adjust the player's animation states depending on move direction speed and bobCycle.
**/
static void PM_RefreshAnimationState() {
	//
	// Determing whether we're crouching, AND (running OR walking).
	// 
	// Animation State: We're crouched.
	if ( pm->cmd.buttons & BUTTON_CROUCH || ps->pmove.pm_flags & PMF_DUCKED ) {
		ps->animation.isCrouched = true;
		// Animation State: We're NOT crouched.
	} else if ( !( pm->cmd.buttons & BUTTON_CROUCH || ps->pmove.pm_flags & PMF_DUCKED ) ) {
		ps->animation.isCrouched = false;
	}
	// Animation State: We're running, NOT walking:
	if ( !( pm->cmd.buttons & BUTTON_WALK ) ) {
		ps->animation.isWalking = false;
		// Animation State: We're walking, NOT running:
	} else if ( ( pm->cmd.buttons & BUTTON_WALK ) ) {
		ps->animation.isWalking = true;
	}

	// Airborne leaves cycle intact, but doesn't advance either.
	if ( pm->ground.entity == nullptr ) {
		return;
	}

	// If not trying to move:
	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove ) {
		// Check both xySpeed, and xyzSpeed. The last one is to prevent
		// view wobbling when crouched, yet idle.
		if ( ps->xySpeed < PM_BOB_CYCLE_IDLE_EPSILON && ps->xyzSpeed < PM_BOB_CYCLE_IDLE_EPSILON ) {
			// Start at beginning of cycle again:
			ps->bobCycle = 0;
			// Idling at last.
			//ps->animation.isIdle = true;
			// Not idling yet, there's still velocities at play.
		} else {
			//ps->animation.isIdle = false;
		}

		// Now check for animation idle playback animation.
		if ( ps->xySpeed < PM_ANIMATION_IDLE_EPSILON && ps->xyzSpeed < PM_ANIMATION_IDLE_EPSILON ) {
			// Idling at last.
			ps->animation.isIdle = true;
			// Not idling yet, there's still velocities at play.
		} else {
			ps->animation.isIdle = false;
		}
		return;
		// We're not idling that's for sure.
	} else {
		ps->animation.isIdle = false;
	}
}
/**
*	@brief	Keeps track of the player's xySpeed and bobCycle:  
**/
//static void PM_Footsteps( void ) {
static void 		// Determine the animation move direction.
PM_Animation_SetMovementDirection();
static void PM_CycleBob() {
	//float		bobMove = 0.f;
	int32_t		oldBobCycle = 0;
	// Defaults to false and checked for later on:
	qboolean	footStep = false;

	//
	// Calculate the speed and cycle to be used for all cyclic walking effects.
	//
	ps->xySpeed = QM_Vector2Length( QM_Vector2FromVector3( ps->pmove.velocity ) );
	ps->xyzSpeed = QM_Vector3Length( ps->pmove.velocity );

	// Reset bobmove.
	//ps->bobMove = 0.f; // WID: Doing this just.. gives jitter like gameplay feel, but enable if you wanna...
	
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
	if ( ps->xySpeed >= PM_BOB_CYCLE_IDLE_EPSILON && ps->xyzSpeed >= PM_BOB_CYCLE_IDLE_EPSILON ) {
		// Ducked:
		if ( ps->pmove.pm_flags & PMF_DUCKED ) {
			// Ducked characters bob even slower than walking:
			ps->bobMove = 0.2;
		// Standing up:
		} else {
			if ( !( pm->cmd.buttons & BUTTON_WALK ) ) {
				// Running bobs faster:
				ps->bobMove = 0.4;
				// And applies footstep sounds:
				footStep = true;
			} else {
				// Walking bobs slower:
				ps->bobMove = 0.3;
			}
		}
	}
	
	// If not trying to move, slowly derade the bobMove factor.
	if ( !pm->cmd.forwardmove && !pm->cmd.sidemove ) {
		// Check both xySpeed, and xyzSpeed. The last one is to prevent
		// view wobbling when crouched, yet idle.
		if ( ps->xySpeed < PM_BOB_CYCLE_IDLE_EPSILON && ps->xyzSpeed < PM_BOB_CYCLE_IDLE_EPSILON ) {
			ps->bobMove = 0.;
		}
	}
	
	// Check for footstep / splash sounds.
	oldBobCycle = ps->bobCycle;
	ps->bobCycle = (int32_t)( ( oldBobCycle + ps->bobMove * pm->cmd.msec ) /* pml.msec */ ) & 255;

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
	Vector2 xyMoveDir = QM_Vector2FromVector3( ps->pmove.velocity );
	// Normalized move direction vector.
	Vector3 xyMoveDirNormalized = QM_Vector3FromVector2( QM_Vector2Normalize( xyMoveDir ) );

	// Dot products.
	const float xDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vRight );
	const float yDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vForward );

	// Resulting move flags.
	ps->animation.moveDirection = 0;

	// Are we even moving enough?
	if ( ps->xySpeed > WALK_EPSILON ) {
		// Forward:
		if ( ( yDotResult > DIR_EPSILON ) || ( pm->cmd.forwardmove > 0 ) ) {
			ps->animation.moveDirection |= PM_MOVEDIRECTION_FORWARD;
		// Back:
		} else if ( ( -yDotResult > DIR_EPSILON ) || ( pm->cmd.forwardmove < 0 ) ) {
			ps->animation.moveDirection |= PM_MOVEDIRECTION_BACKWARD;
		}
		// Right: (Only if the dotproduct is so, or specifically only side move is pressed.)
		if ( ( xDotResult > DIR_EPSILON ) || ( !pm->cmd.forwardmove && pm->cmd.sidemove > 0 ) ) {
			ps->animation.moveDirection |= PM_MOVEDIRECTION_RIGHT;
		// Left: (Only if the dotproduct is so, or specifically only side move is pressed.)
		} else if ( ( -xDotResult > DIR_EPSILON ) || ( !pm->cmd.forwardmove && pm->cmd.sidemove < 0 ) ) {
			ps->animation.moveDirection |= PM_MOVEDIRECTION_LEFT;
		}

		// Running:
		if ( pm->state->xySpeed > RUN_EPSILON ) {
			ps->animation.moveDirection |= PM_MOVEDIRECTION_RUN;
		// Walking:
		} else if ( pm->state->xySpeed > WALK_EPSILON ) {
			ps->animation.moveDirection |= PM_MOVEDIRECTION_WALK;
		}
	}

	#if 0
	// Determine the move direction for animations based on the user input state.
	// ( If strafe left / right or forward / backward is pressed then.. )
	if ( pm->cmd.forwardmove || pm->cmd.sidemove ) {
		// Forward:
		if ( pm->cmd.sidemove == 0 && pm->cmd.forwardmove > 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_FORWARD;
		// Forward Left:
		} else if ( pm->cmd.sidemove < 0 && pm->cmd.forwardmove > 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_LEFT;
		// Left:
		} else if ( pm->cmd.sidemove < 0 && pm->cmd.forwardmove == 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_LEFT;
		// Backward Left:
		} else if ( pm->cmd.sidemove < 0 && pm->cmd.forwardmove < 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_BACKWARD_LEFT;
		// Backward:
		} else if ( pm->cmd.sidemove == 0 && pm->cmd.forwardmove < 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_BACKWARD;
		// Backward Right:
		} else if ( pm->cmd.sidemove > 0 && pm->cmd.forwardmove < 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_BACKWARD_RIGHT;
		// Right:
		} else if ( pm->cmd.sidemove > 0 && pm->cmd.forwardmove == 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_RIGHT;
		// Forward Right:
		} else if ( pm->cmd.sidemove > 0 && pm->cmd.forwardmove > 0 ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_RIGHT;
		}
	// if they aren't actively going directly sideways, change the animation to the diagonal so they
	// don't stop too crooked:
	} else {
		if ( ps->animation.moveDirection == PM_MOVEDIRECTION_LEFT ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_LEFT;
		} else if ( ps->animation.moveDirection == PM_MOVEDIRECTION_RIGHT ) {
			ps->animation.moveDirection = PM_MOVEDIRECTION_FORWARD_RIGHT;
		}
	}
	#endif
}


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

	return pm->trace( &start.x, &mins.x, &maxs.x, &end.x, pm->player, contentMask );
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
static bool PM_CheckStep( const cm_trace_t *trace ) {
	// If not solid:
	if ( !trace->allsolid ) {
		// If trace clipped to an entity and the plane we hit its normal is sane for stepping:
		if ( trace->entityNumber != ENTITYNUM_NONE && trace->plane.normal[2] >= PM_STEP_MIN_NORMAL) {
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
static void PM_StepDown( const cm_trace_t *trace ) {
	// Apply the trace endpos as the new origin.
	ps->pmove.origin = trace->endpos;

	// Determine the step height based on the new, and previous origin.
	const float step_height = ps->pmove.origin.z - pml.previousOrigin.z;

	// If its absolute(-/+) value >= PM_STEP_MIN_SIZE(4.0) then we got an official step.
	if ( fabsf( step_height ) >= PM_STEP_MIN_SIZE ) {
		// Store non absolute but exact step height.
		pm->step_height = step_height;

		// Add predicted step event.
		//PM_AddEvent( 1 /*PM_EVENT_STEP*/, fabs( step_height ) );
	}
}

#if 0
/**
*	@brief	If intersecting a brush surface, will try to step over the obstruction 
*			instead of sliding along it.
* 
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
static void PM_StepSlideMove() {
	cm_trace_t trace;
	Vector3 startOrigin = ps->pmove.origin;
	Vector3 startVelocity = ps->pmove.velocity;

	// Perform an actual 'Step Slide'.
	PM_StepSlideMove_Generic( ps->pmove.origin, ps->pmove.velocity, pml.frameTime, pm->mins, pm->maxs, pm->touchTraces, ps->pmove.pm_time );

	Vector3 downOrigin = ps->pmove.origin;
	Vector3 downVelocity = ps->pmove.velocity;

	// Perform 'up-trace' to see whether we can step up at all.
	Vector3 up = startOrigin + Vector3{ 0.f, 0.f, PM_STEP_MAX_SIZE };
	trace = PM_Trace( startOrigin, pm->mins, pm->maxs, up );
	if ( trace.allsolid ) {
		return; // can't step up
	}

	// Determine step size to test with.
	const float stepSize = trace.endpos[ 2 ] - startOrigin.z;

	// We can step up. Try sliding above.
	ps->pmove.origin = trace.endpos;
	ps->pmove.velocity = startVelocity;

	// Perform an actual 'Step Slide'.
	PM_StepSlideMove_Generic( ps->pmove.origin, ps->pmove.velocity, pml.frameTime, pm->mins, pm->maxs, pm->touchTraces, ps->pmove.pm_time );

	// Push down the final amount.
	Vector3 down = ps->pmove.origin - Vector3{ 0.f, 0.f, stepSize };

	// [Paril-KEX] jitspoe suggestion for stair clip fix; store
	// the old down position, and pick a better spot for downwards
	// trace if the start origin's Z position is lower than the down end pt.
	Vector3 original_down = down;

	if ( startOrigin.z < down.z ) {
		down.z = startOrigin.z - 1.f;
	}

	trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, down );
	if ( !trace.allsolid ) {
		// [Paril-KEX] from above, do the proper trace now
		cm_trace_t real_trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, original_down );
		//ps->pmove.origin = real_trace.endpos;

		// WID: Use proper stair step checking.
		if ( PM_CheckStep( &trace ) ) {
			// Only an upwards jump is a stair clip.
			if ( ps->pmove.velocity.z > 0.f ) {
				pm->step_clip = true;
			}
			// Step down to the new found ground.
			PM_StepDown( &real_trace );
		}
	}

	up = ps->pmove.origin;

	// Decide which one went farther, use 'Vector2Length', ignore the Z axis.
	const float down_dist = ( downOrigin.x - startOrigin.x ) * ( downOrigin.y - startOrigin.y ) + ( downOrigin.y - startOrigin.y ) * ( downOrigin.y - startOrigin.y );
	const float up_dist = ( up.x - startOrigin.x ) * ( up.x - startOrigin.x ) + ( up.y - startOrigin.y ) * ( up.y - startOrigin.y );

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < PM_STEP_MIN_NORMAL ) {
		ps->pmove.origin = downOrigin;
		ps->pmove.velocity = downVelocity;
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (ps->pmove.pm_flags & PMF_ON_GROUND)
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		ps->pmove.velocity.z = downVelocity.z;
	}

	// Paril: step down stairs/slopes
	if ( ( ps->pmove.pm_flags & PMF_ON_GROUND ) && !( ps->pmove.pm_flags & PMF_ON_LADDER ) &&
		( pm->liquid.level < cm_liquid_level_t::LIQUID_WAIST || ( !( pm->cmd.buttons & BUTTON_JUMP ) && ps->pmove.velocity.z <= 0 ) ) ) {
		Vector3 down = ps->pmove.origin - Vector3{ 0.f, 0.f, PM_STEP_MAX_SIZE };
		trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, down );

		// WID: Use proper stair step checking.
		// Check for stairs:
		if ( PM_CheckStep( &trace ) ) {
			// Step down stairs:
			PM_StepDown( &trace );
		// We're expecting it to be a slope, step down the slope instead:
		} else if ( trace.fraction < 1.f ) {
			ps->pmove.origin = trace.endpos;
		}
	}
}
#endif


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
	const float currentspeed = QM_Vector3DotProduct( ps->pmove.velocity, wishDirection );
	const float addSpeed = wishSpeed - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

// HL like acceleration.
#ifdef HL_LIKE_ACCELERATION
	float drop = 0;
	float friction = 1;
	if ( ( pm->ground.entity != nullptr && pml.groundTrace.surface != nullptr && !( pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) ) || ( ps->pmove.pm_flags & PMF_ON_LADDER ) ) {
		// Get the material to fetch friction from.
		cm_material_t *ground_material = ( pml.groundTrace.surface != nullptr ? pml.groundTrace.surface->material : nullptr );
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

	ps->pmove.velocity = QM_Vector3MultiplyAdd( ps->pmove.velocity, accelerationSpeed, wishDirection );
}
/**
*	@brief	Handles user intended 'mid-air' acceleration.
**/
static void PM_AirAccelerate( const Vector3 &wishDirection, const float wishSpeed, const float acceleration ) {
	float wishspd = wishSpeed;
	if ( wishspd > 30 ) {
		wishspd = 30;
	}

	const float currentspeed = QM_Vector3DotProduct( ps->pmove.velocity, wishDirection );
	const float addSpeed = wishspd - currentspeed;
	if ( addSpeed <= 0 ) {
		return;
	}

	float accelerationSpeed = acceleration * wishSpeed * pml.frameTime;
	if ( accelerationSpeed > addSpeed ) {
		accelerationSpeed = addSpeed;
	}

	ps->pmove.velocity = QM_Vector3MultiplyAdd( ps->pmove.velocity, accelerationSpeed, wishDirection );
}
/**
*	@brief	Handles both ground friction and water friction
**/
static void PM_Friction() {
	// Set us to a halt, if our speed is too low, otherwise we'll see
	// ourselves sort of 'drifting'.
	const float speed = QM_Vector3Length( ps->pmove.velocity );//sqrtf( ps->pmove.velocity.x * ps->pmove.velocity.x + ps->pmove.velocity.y * ps->pmove.velocity.y + ps->pmove.velocity.z * ps->pmove.velocity.z );
	if ( speed < 1 ) {
		ps->pmove.velocity.x = 0;
		ps->pmove.velocity.y = 0;
		return;
	}

	// Apply ground friction if on-ground.
#ifdef PMOVE_USE_MATERIAL_FRICTION
	float drop = 0;
	if ( ( pm->ground.entity != nullptr && pml.groundTrace.surface != nullptr && !( pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) ) || ( ps->pmove.pm_flags & PMF_ON_LADDER ) ) {
		// Get the material to fetch friction from.
		cm_material_t *ground_material = ( pml.groundTrace.surface != nullptr ? pml.groundTrace.surface->material : nullptr );
		float friction = ( ground_material ? ground_material->physical.friction : pmp->pm_friction );
		const float control = ( speed < pmp->pm_stop_speed ? pmp->pm_stop_speed : speed );
		drop += control * friction * pml.frameTime;
	}
#else
	// Apply ground friction if on-ground.
	float drop = 0;
	if ( ( pm->ground.entity && pml.groundTrace.surface && !( pml.groundTrace.surface->flags & CM_SURFACE_FLAG_SLICK ) ) || ( ps->pmove.pm_flags & PMF_ON_LADDER ) ) {
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
		ps->pmove.velocity = {};
	} else {
		newspeed /= speed;
		ps->pmove.velocity *= newspeed;
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

				if ( pm->liquid.level < cm_liquid_level_t::LIQUID_WAIST ) {
					ladder_speed *= pmp->pm_ladder_mod;
				}

				// Check for ladder.
				Vector3 flatforward = QM_Vector3Normalize( {
					pml.forward.x,
					pml.forward.y,
					0.f 
				} );
				Vector3 spot = ps->pmove.origin + ( flatforward * 1 );
				cm_trace_t trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER );

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
			if ( ps->pmove.velocity.z > 0 ) {
				ps->pmove.velocity.z -= ps->pmove.gravity * pml.frameTime;
				if ( ps->pmove.velocity.z < 0 ) {
					ps->pmove.velocity.z = 0;
				}
			} else {
				ps->pmove.velocity.z += ps->pmove.gravity * pml.frameTime;
				if ( ps->pmove.velocity.z > 0 ) {
					ps->pmove.velocity.z = 0;
				}
			}
		}
		PM_StepSlideMove_Generic(
			ps->pmove.origin, ps->pmove.velocity,
			pm->step_height, pm->impact_delta,
			pm->mins, pm->maxs,
			pm->touchTraces,
			pml.groundTrace.surface || pml.groundTrace.entityNumber != ENTITYNUM_NONE || pml.groundTrace.fraction < 1.0,
			pml.groundTrace,
			false,
			pml.frameTime,
			ps->pmove.pm_time
		);
	// Perform ground movement. (Walking on-ground):
	} else if ( ps->pmove.pm_flags & PMF_ON_GROUND ) {
		// Erase Z before accelerating.
		//ps->pmove.velocity.z = 0; //!!! this is before the accel
		PM_Accelerate( wishDirection, wishSpeed, pmp->pm_accelerate );

		// PGM	-- fix for negative trigger_gravity fields
		//		ps->pmove.velocity[2] = 0;
		//if ( ps->pmove.gravity > 0 ) {
		//	ps->pmove.velocity.z = 0;
		//} else {
			ps->pmove.velocity.z -= ps->pmove.gravity * pml.frameTime;
		//}
		// PGM

		// Escape and prevent step slide moving if we have no x/y velocity.
		//if ( !ps->pmove.velocity.x && !ps->pmove.velocity.y ) {
		//	return;
		//}

		// Step Slide.
		PM_StepSlideMove_Generic(
			ps->pmove.origin, ps->pmove.velocity,
			pm->step_height, pm->impact_delta,
			pm->mins, pm->maxs,
			pm->touchTraces,
			true,// /*pml.groundTrace.surface || */pml.groundTrace.entityNumber != ENTITYNUM_NONE /*|| pml.groundTrace.fraction < 1.0*/,
			pml.groundTrace,
			false,
			pml.frameTime,
			ps->pmove.pm_time
		);
	// Not on ground, so litte effect on velocity. Perform air acceleration in case it is enabled(has any value stored.):
	} else {
		// Accelerate appropriately.
		if ( pmp->pm_air_accelerate ) {
			PM_AirAccelerate( wishDirection, wishSpeed, pmp->pm_air_accelerate );
		} else {
			PM_Accelerate( wishDirection, wishSpeed, 1 );
		}

		// Add gravity in case we're not in grappling mode.
		if ( ps->pmove.pm_type != PM_GRAPPLE ) {
			ps->pmove.velocity.z -= ps->pmove.gravity * pml.frameTime;
		}

		// Step Slide.
		PM_StepSlideMove_Generic(
			ps->pmove.origin, ps->pmove.velocity,
			pm->step_height, pm->impact_delta,
			pm->mins, pm->maxs,
			pm->touchTraces,
			false,//pml.groundTrace.surface || pml.groundTrace.entityNumber != ENTITYNUM_NONE || pml.groundTrace.fraction < 1.0,
			pml.groundTrace,
			true,
			pml.frameTime,
			ps->pmove.pm_time
		);
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
	PM_StepSlideMove_Generic(
		ps->pmove.origin, ps->pmove.velocity,
		pm->step_height, pm->impact_delta,
		pm->mins, pm->maxs,
		pm->touchTraces,
		pml.groundTrace.surface || pml.groundTrace.entityNumber != ENTITYNUM_NONE || pml.groundTrace.fraction < 1.0,
		pml.groundTrace,
		true,
		pml.frameTime,
		ps->pmove.pm_time
	);
}
/**
*	@brief	Performs out of water jump movement. Apply gravity, check whether to cancel water jump mode, and slide move right after.
**/
static void PM_WaterJumpMove() {
	// Apply downwards gravity to the velocity.
	//ps->pmove.velocity.z -= ps->pmove.gravity * pml.frameTime;

	// Cancel the WaterJump mode as soon as we are falling down again. (Velocity < 0).
	if ( ps->pmove.velocity.z < 0 ) { // cancel as soon as we are falling down again
		ps->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
		ps->pmove.pm_time = 0;
	}

	// Step Slide.
	PM_StepSlideMove_Generic(
		ps->pmove.origin, ps->pmove.velocity,
		pm->step_height, pm->impact_delta,
		pm->mins, pm->maxs,
		pm->touchTraces,
		pml.groundTrace.surface || pml.groundTrace.entityNumber != ENTITYNUM_NONE || pml.groundTrace.fraction < 1.0,
		pml.groundTrace,
		true,
		pml.frameTime,
		ps->pmove.pm_time
	);
}
/**
*	@brief
**/
static void PM_FlyMove( bool doclip ) {
	float drop = 0.f;

	// When clipping don 't adjust viewheight, if no-clipping, default a viewheight of 22.
	ps->pmove.viewheight = doclip ? 0 : PM_VIEWHEIGHT_STANDUP;

	// Calculate friction
	const float speed = QM_Vector3Length( ps->pmove.velocity );
	if ( speed < 1 ) {
		ps->pmove.velocity = QM_Vector3Zero();
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

		ps->pmove.velocity *= newspeed;
		//float newspeed = speed - drop;
		//if ( newspeed < 0 ) {
		//	newspeed = 0;
		//}
		//newspeed /= speed;

		//ps->pmove.velocity *= newspeed;
		#else
		// Scale the velocity.
		float newspeed = speed - drop;
		if ( newspeed <= 0 ) {
			newspeed = 0;
			ps->pmove.velocity = {};
		} else {
			newspeed /= speed;
			ps->pmove.velocity *= newspeed;
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

	const float currentspeed = QM_Vector3DotProduct( ps->pmove.velocity, wishdir );
	float addspeed = wishspeed - currentspeed;

	if ( addspeed > 0 ) {
		float accelspeed = pmp->pm_accelerate * pml.frameTime * wishspeed;
		if ( accelspeed > addspeed ) {
			accelspeed = addspeed;
		}
		ps->pmove.velocity += accelspeed * wishdir;
	}

	if ( doclip ) {
		/*for (i = 0; i < 3; i++)
			end[i] = ps->pmove.origin[i] + pml.frameTime * ps->pmove.velocity[i];

		trace = PM_Trace(ps->pmove.origin, pm->mins, pm->maxs, end);

		ps->pmove.origin = trace.endpos;*/
		// Step Slide.
		// Step Slide.
		PM_StepSlideMove_Generic(
			ps->pmove.origin, ps->pmove.velocity,
			pm->step_height, pm->impact_delta,
			pm->mins, pm->maxs,
			pm->touchTraces,
			pml.groundTrace.surface || pml.groundTrace.entityNumber != ENTITYNUM_NONE || pml.groundTrace.fraction < 1.0,
			pml.groundTrace,
			false,
			pml.frameTime,
			ps->pmove.pm_time
		);
	} else {
		// Hard set origin in order to move.
		ps->pmove.origin += ( ps->pmove.velocity * pml.frameTime );
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
	forward = QM_Vector3Length( ps->pmove.velocity );
	forward -= 20;
	if ( forward <= 0 ) {
		ps->pmove.velocity = QM_Vector3Zero();
	} else {
		// Normalize.
		ps->pmove.velocity = QM_Vector3Normalize( ps->pmove.velocity );
		// Scale by old velocity derived length.
		ps->pmove.velocity *= forward;
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
static inline void PM_GetWaterLevel( const Vector3 &position, cm_liquid_level_t &level, cm_contents_t &type ) {
	//
	// get liquid.level, accounting for ducking
	//
	level = cm_liquid_level_t::LIQUID_NONE;
	type = CONTENTS_NONE;

	int32_t sample2 = (int)( ps->pmove.viewheight - pm->mins.z );
	int32_t sample1 = sample2 / 2;

	Vector3 point = position;

	point.z += pm->mins.z + 1;

	cm_contents_t contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );

	if ( contentType & CM_CONTENTMASK_WATER ) {
		type = contentType;
		level = cm_liquid_level_t::LIQUID_FEET;
		point.z = ps->pmove.origin.z + pm->mins.z + sample1;
		contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );
		if ( contentType & CM_CONTENTMASK_WATER ) {
			level = cm_liquid_level_t::LIQUID_WAIST;
			point.z = ps->pmove.origin.z + pm->mins.z + sample2;
			contentType = pm->pointcontents( QM_Vector3ToQFloatV( point ).v );
			if ( contentType & CM_CONTENTMASK_WATER ) {
				level = cm_liquid_level_t::LIQUID_UNDER;
			}
		}
	}
}
/**
*	@brief
**/
static void PM_CategorizePosition() {
	cm_trace_t	   trace;

	// If the player hull point is 0.25 units down is solid, the player is on ground.
	// See if standing on something solid
	Vector3 point = ps->pmove.origin + Vector3{ 0.f, 0.f, -0.25f };

	if ( ps->pmove.velocity.z > 180 || ps->pmove.pm_type == PM_GRAPPLE ) { //!!ZOID changed from 100 to 180 (ramp accel)
		// We are going off-ground due to a too high velocit.
		ps->pmove.pm_flags &= ~PMF_ON_GROUND;

		// Ensure the player move ground data is updated accordingly.
		pm->ground.entity = nullptr;
		pm->ground.plane = {};
		pm->ground.surface = {};
		pm->ground.contents = CONTENTS_NONE;
		pm->ground.material = nullptr;

		pml.groundTrace.surface = nullptr;
		pml.groundTrace.contents = CONTENTS_NONE;

		// We are in air.
		ps->animation.inAir = true;
	} else {
		trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, point );
		pm->ground.plane = trace.plane;
		//pml.groundSurface = trace.surface;
		pm->ground.surface = *( pml.groundTrace.surface = trace.surface );
		//pml.groundContents = trace.contents;
		pm->ground.contents = (cm_contents_t)( pml.groundTrace.contents = trace.contents );
		//pml.groundMaterial = trace.material;
		pm->ground.material = trace.material;

		#ifdef PMOVE_Q2RE_STUCK_FIXES
		// [Paril-KEX] to attempt to fix edge cases where you get stuck
		// wedged between a slope and a wall (which is irrecoverable
		// most of the time), we'll allow the player to "stand" on
		// slopes if they are right up against a wall
		bool slanted_ground = trace.fraction < 1.0f && trace.plane.normal[ 2 ] < PM_STEP_MIN_NORMAL;

		if ( slanted_ground ) {
			Vector3 slantTraceEnd = ps->pmove.origin + trace.plane.normal;
			cm_trace_t slant = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, slantTraceEnd );

			if ( slant.fraction < 1.0f && !slant.startsolid ) {
				slanted_ground = false;
			}
		}
		#else
		const bool slanted_ground = false;
		#endif

		// If the player hull point is 0.25 units down is solid, the player is on ground.
		// See if standing on something solid
		Vector3 landEndPoint = ps->pmove.origin + Vector3{ 0., 0., -30. };
		cm_trace_t jumpLandTrace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, landEndPoint );

		if ( trace.fraction == 1.0f || ( slanted_ground && !trace.startsolid ) ) {
			pm->ground.entity = nullptr;
			ps->pmove.pm_flags &= ~PMF_ON_GROUND;

			// When not crouched, set inAir state.
			if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
				ps->animation.inAir = true;
			// When crouched and in air, allow for crouch animations, thus inAir = false.
			} else {
				ps->animation.inAir = false;
			}
		} else {
			// Update ground.
			PM_UpdateGroundFromTrace( &trace );

			// hitting solid ground will end a waterjump
			if ( ps->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
				ps->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP );
				ps->pmove.pm_time = 0;
			}

			// Just hit the ground.
			if ( !( ps->pmove.pm_flags & PMF_ON_GROUND ) ) {
				//if ( jumpLandTrace.fraction != 1.0 ) {
				//	SG_PlayerState_AddPredictableEvent( PS_EV_JUMP_LAND, pm->impact_delta, ps );
				//}
				// Set in case we already applied upward jump event.
				bool isJumpUpEventSet = false;

				// [Paril-KEX]
				if ( ps->pmove.velocity.z >= 100.f && pm->ground.plane.normal[ 2 ] >= 0.9f && !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
					ps->pmove.pm_flags |= PMF_TIME_TRICK_JUMP;
					ps->pmove.pm_time = 64;

					// Jump up.
					PM_AddEvent( PS_EV_JUMP_UP, 0 );
					// Jumped.
					isJumpUpEventSet = true;
				}

				// [Paril-KEX] calculate impact delta; this also fixes triple jumping
				Vector3 clipped_velocity = QM_Vector3Zero();
				// First clip the velocity.
				PM_BounceVelocity( ps->pmove.velocity, pm->ground.plane.normal, clipped_velocity, 1.01f );
				// Calculate impact delta.
				pm->impact_delta = pml.previousVelocity.z - clipped_velocity.z;

				// On ground.
				ps->pmove.pm_flags |= PMF_ON_GROUND;
				// Not in air anymore.
				//ps->animation.inAir = false;

				// The duck check is handled client-side for the view player and in G_SetClientFrame for other player entities.
				if ( !isJumpUpEventSet /*&& !( ps->pmove.pm_flags & PMF_DUCKED ) */) {
					PM_AddEvent( PS_EV_JUMP_LAND, pm->impact_delta );
				}

				if ( ( ps->pmove.pm_flags & PMF_DUCKED ) ) {
					ps->pmove.pm_flags |= PMF_TIME_LAND;
					ps->pmove.pm_time = 32;
				}
			// Setting inAir to false right here, will cause the player to still be 'in air' while
			// it sends the landing event. Only the frame after that(where onground is set) do we
			// want to notify animations of inAir is false.
			} else if ( ( ps->pmove.pm_flags & PMF_ON_GROUND ) ) {
				// Hit ground.
				ps->animation.inAir = false;
			}
		}

		// Register the touch trace.
		PM_RegisterTouchTrace( pm->touchTraces, trace );
	}

	// Get liquid.level, accounting for ducking.
	PM_GetWaterLevel( ps->pmove.origin, pm->liquid.level, pm->liquid.type );
}
/**
*	@brief	Determine whether we're above water, or not.
**/
static inline const bool PM_AboveWater() {
	// Testing point.
	const Vector3 below = ps->pmove.origin + Vector3{ 0, 0, -8 };
	// We got solid below, not water:
	bool solid_below = pm->trace( QM_Vector3ToQFloatV( ps->pmove.origin ).v, QM_Vector3ToQFloatV( pm->mins ).v, QM_Vector3ToQFloatV( pm->maxs ).v, QM_Vector3ToQFloatV( below ).v, pm->player, CM_CONTENTMASK_SOLID ).fraction < 1.0f;
	if ( solid_below ) {
		return false;
	}

	// We're above water:
	bool water_below = pm->trace( QM_Vector3ToQFloatV( ps->pmove.origin ).v, QM_Vector3ToQFloatV( pm->mins ).v, QM_Vector3ToQFloatV( pm->maxs ).v, QM_Vector3ToQFloatV( below ).v, pm->player, CM_CONTENTMASK_WATER ).fraction < 1.0f;
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
		ps->pmove.origin.x + ps->viewoffset.x,
		ps->pmove.origin.y + ps->viewoffset.y,
		ps->pmove.origin.z + ps->viewoffset.z + (float)ps->pmove.viewheight
	};
	const int32_t contents = pm->pointcontents( QM_Vector3ToQFloatV( vieworg ).v );//cm_contents_t contents = pm->pointcontents( vieworg );

	// Under a 'water' like solid:
	if ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) {
		ps->rdflags |= RDF_UNDERWATER;
		// Or not:
	} else {
		ps->rdflags &= ~RDF_UNDERWATER;
	}

	// Prevent it from adding screenblend if we're inside a client entity, by checking
	// if its brush has CONTENTS_PLAYER set in its clipmask.
	if ( !( contents & CONTENTS_PLAYER )
		&& ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) ) ) {
		SG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, ps->screen_blend );
	} else if ( contents & CONTENTS_SLIME ) {
		SG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, ps->screen_blend );
	} else if ( contents & CONTENTS_WATER ) {
		SG_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, ps->screen_blend );
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
	const Vector3 spot = ps->pmove.origin + ( flatforward * 1 );
	cm_trace_t trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, spot, (cm_contents_t)( CONTENTS_LADDER ) );
	if ( ( trace.fraction < 1 ) && ( trace.contents & CONTENTS_LADDER ) && pm->liquid.level < cm_liquid_level_t::LIQUID_WAIST ) {
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

	if ( pm->liquid.level != cm_liquid_level_t::LIQUID_WAIST ) {
		return;
	}
	// [Paril-KEX]
	else if ( pm->liquid.type & CONTENTS_NO_WATERJUMP ) {
		return;
	}

	// Quick check that something is even blocking us forward
	Vector3 blockTraceEnd = ps->pmove.origin + flatforward * 40.f;
	trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, blockTraceEnd, CM_CONTENTMASK_SOLID );

	// we aren't blocked, or what we're blocked by is something we can walk up
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] >= PM_STEP_MIN_NORMAL ) {
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
	Vector3 waterjump_origin = { ps->pmove.origin.x, ps->pmove.origin.y, ps->pmove.origin.z };//vec3_t waterjump_origin = ps->pmove.origin;
	double time = 0.1;
	bool has_time = true;

	for ( size_t i = 0; i < std::min( 50, (int32_t)( 40 * ( 800.f / ps->pmove.gravity ) ) ); i++ ) {
		waterjump_vel.z -= ps->pmove.gravity * time;

		if ( waterjump_vel.z < 0 ) {
			has_time = false;
		}

		#if 0
		PM_StepSlideMove_Generic( 
			waterjump_origin, waterjump_vel, 
			time, 
			pm->mins, pm->maxs, 
			touches, has_time );
		#else
		PM_StepSlideMove_Generic(
			waterjump_origin, waterjump_vel,
			pm->step_height, pm->impact_delta,
			pm->mins, pm->maxs,
			touches,
			false, //pml.groundTrace.surface || pml.groundTrace.entityNumber || pml.groundTrace.fraction < 1.0,
			pml.groundTrace,
			false,
			pml.frameTime,
			has_time
		);

		#endif
	}

	// snap down to ground
	Vector3 snapTraceEnd = waterjump_origin + Vector3{ 0.f, 0.f, -2.f };
	trace = PM_Trace( waterjump_origin, pm->mins, pm->maxs, snapTraceEnd, CM_CONTENTMASK_SOLID );

	// Can't stand here.
	if ( trace.fraction == 1.0f || trace.plane.normal[ 2 ] < PM_STEP_MIN_NORMAL || trace.endpos[ 2 ] < ps->pmove.origin.z ) {
		return;
	}

	// We're currently standing on ground, and the snapped position is a step.
	if ( pm->ground.entity && fabsf( ps->pmove.origin.z - trace.endpos[ 2 ] ) <= PM_STEP_MAX_SIZE ) {
		return;
	}

	// Get water level.
	cm_liquid_level_t level;
	cm_contents_t type;
	PM_GetWaterLevel( trace.endpos, level, type );

	// The water jump spot will be under water, so we're probably hitting something weird that isn't important
	if ( level >= cm_liquid_level_t::LIQUID_WAIST ) {
		return;
	}

	// Valid waterjump! Set velocity to Jump out of water
	ps->pmove.velocity = flatforward * 50;
	ps->pmove.velocity.z = 350;
	// Engage PMF_TIME_WATERJUMP timer mode.
	ps->pmove.pm_flags |= PMF_TIME_WATERJUMP;
	ps->pmove.pm_time = 2048;
}
#ifdef PMOVE_ENABLE_WALLJUMPING
const Vector3 ProjectPointOnPlane( const Vector3 &point, const Vector3 &normal ) {
	Vector3 dst = {};
	float d = -DotProduct( point, normal );
	VectorMA( point, d, normal, dst );
	return dst;
}
/**
*	@brief	Performs the engagement of Wall Jumping.
**/
static const bool PM_WallJump() {
	Vector3 movedir = {};
	Vector3 refNormal = { 0.f, 0.f, 1.f }; // Default reference normal for the wall jump.
	movedir = ProjectPointOnPlane( pml.forward, refNormal );
	//VectorNormalize( movedir );
	movedir = QM_Vector3Normalize( movedir );

	if ( pm->cmd.forwardmove < 0 ) {
		VectorNegate( movedir, movedir );
	}

	//allow strafe transitions
	if ( pm->cmd.sidemove ) {
		VectorCopy( pml.right, movedir );

		if ( pm->cmd.sidemove < 0 ) {
			VectorNegate( movedir, movedir );
		}
	}

	//trace into direction we are moving
	constexpr float jump_trace_distance = 2.0f; // 0.25f
	Vector3 point = {};
	VectorMA( ps->pmove.origin , jump_trace_distance, movedir, point );
	cm_trace_t trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, point, CM_CONTENTMASK_SOLID );

	// Calculate the 'wishvel' which is our desired direction vector.
	//float jump_trace_distance = 2.0f;
	Vector3 wishvel = QM_Vector3Normalize( pml.forward * pm->cmd.forwardmove + pml.right * pm->cmd.sidemove ) * jump_trace_distance;
	Vector3 spot = ps->pmove.origin + wishvel;
	//cm_trace_t trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, spot, CM_CONTENTMASK_SOLID );
	// Trace must've hit something, and NOT BE STUCK inside of a brush either.
	if ( trace.fraction < 1.0f && !trace.allsolid && !trace.startsolid
		// Ensure that the plane's normal is an actual wall.
		// Negative slopes, would have a normal as < 0.
		// Walls have a normal of 0.
		// Slopes that are too steep to stand on PM_STEP_MIN_NORMAL(0.7).
		&& ( trace.plane.normal[ 2 ] >= 0 && trace.plane.normal[ 2 ] <= PM_STEP_MIN_NORMAL )
		// Prevent us from wall jumping in skies.
		&& ( trace.surface && !( trace.surface->flags & CM_SURFACE_FLAG_SKY ) )
		// Prevent us from doing this when on-ground so that trick jumping keeps working AND
		// we don't false positively remove PMF_JUMP_HELD or mess with velocities.
		&& ( !( ps->pmove.pm_flags & PMF_ON_GROUND ) && !(ps->pmove.pm_flags & PMF_JUMP_HELD ) ) ) {
		
		// Adjust our pmove state to engage in the act of jumping.
		ps->pmove.pm_flags |= PMF_JUMP_HELD;
		// Play jump sound ofc.
		pm->jump_sound = true;
		// Unset ground.
		pm->ground.entity = nullptr; //PM_UpdateGroundFromTrace( nullptr );
		ps->pmove.pm_flags &= ~PMF_ON_GROUND;

		// Jump height velocity for the wall jump..
		constexpr float jump_height = 280;

		// For use with the excluding Z component, and/or prevent walljumps if exceeding certain Z velocities.
		//ps->pmove.velocity[ 2 ] += jump_height * QM_Min( 0.23f, trace.plane.normal[ 2 ] );
		// No cap? :-)
		//if ( ps->pmove.velocity[ 2 ] > jump_height ) {
		//	ps->pmove.velocity[ 2 ] = jump_height;
		//}
		
		// Default Z component to straight up.
		Vector3 jumpNormal = QM_Vector3Zero();
		jumpNormal[ 0 ] = trace.plane.normal[ 0 ];
		jumpNormal[ 1 ] = trace.plane.normal[ 1 ];
		jumpNormal[ 2 ] = 1;
		//const float xy_speed = 240. * QM_Minf( 0.23, trace.plane.normal[ 2 ] );
		ps->pmove.velocity = ps->pmove.velocity + ( jumpNormal * jump_height );

		// One can choose to exclude the jump_height addition up above and use this below instead.
		//ps->pmove.velocity += Vector2( trace.plane.normal ) * 240.0f;

		// Add 'Predictable' Event for Jumping Up.
		PM_AddEvent( PS_EV_JUMP_UP, 0 );

		// Success to jump.
		return true;
	}

	// Failure to jump.
	return false;
}
#endif
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
	if ( pm->liquid.level >= cm_liquid_level_t::LIQUID_WAIST ) {
		// Unset ground.
		pm->ground.entity = nullptr;
		return;
	}

	#ifdef PMOVE_ENABLE_WALLJUMPING
	// We moved Trick Jumping here.
	if ( pm->ground.entity == nullptr &&
		// We want the jump button to be pressed but not the PMF_JUMP_HELD flag to be set yet.
		( pm->cmd.buttons & BUTTON_JUMP ) && !( ps->pmove.pm_flags & PMF_JUMP_HELD ) ) {
		// Return if we managed to initiate a wall jump.
		if ( PM_WallJump() ) {
			return;
		}
	}
	#endif

	// In-air/liquid, so no effect, can't re-jump without ground.
	if ( pm->ground.entity == nullptr || pml.groundTrace.entityNumber == ENTITYNUM_NONE ) {
		return;
	}

	// Adjust our pmove state to engage in the act of jumping.
	ps->pmove.pm_flags |= PMF_JUMP_HELD;
	pm->jump_sound = true;
	// Unset ground.
	pm->ground.entity = nullptr; //PM_UpdateGroundFromTrace( nullptr );
	ps->pmove.pm_flags &= ~PMF_ON_GROUND;

	// Apply upwards velocity.
	const float jump_height = pmp->pm_jump_height;
	ps->pmove.velocity.z += jump_height;
	// If still too low, force jump height velocity upwards.
	if ( ps->pmove.velocity.z < jump_height ) {
		ps->pmove.velocity.z = jump_height;
	}

	// Add 'Predictable' Event for Jumping Up.
	PM_AddEvent( PS_EV_JUMP_UP, 0 );
}
/**
*	@brief	Decide whether to duck, and/or unduck.
**/
static inline const bool PM_CheckDuck() {
	// Can't duck if gibbed.
	if ( ps->pmove.pm_type == PM_GIB /*|| ps->pmove.pm_type == PM_DEAD*/ ) {
		return false;
	}

	cm_trace_t trace;
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
		( pm->ground.entity || ( pm->liquid.level <= cm_liquid_level_t::LIQUID_FEET && !PM_AboveWater() ) ) &&
		!( ps->pmove.pm_flags & PMF_ON_LADDER ) ) { 
		if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
			// check that duck won't be blocked
			Vector3 check_maxs = { pm->maxs.x, pm->maxs.y, PM_BBOX_DUCKED_MAXS.z };
			trace = PM_Trace( ps->pmove.origin, pm->mins, check_maxs, ps->pmove.origin );
			if ( !trace.allsolid ) {
				ps->pmove.pm_flags |= PMF_DUCKED;
				flags_changed = true;
			}
		}
	// Try and get out of the ducked state, stand up, if possible.
	} else {
		if ( ps->pmove.pm_flags & PMF_DUCKED ) {
			// try to stand up
			Vector3 check_maxs = { pm->maxs.x, pm->maxs.y, PM_BBOX_STANDUP_MAXS.z };
			trace = PM_Trace( ps->pmove.origin, pm->mins, check_maxs, ps->pmove.origin );
			if ( !trace.allsolid ) {
				ps->pmove.pm_flags &= ~PMF_DUCKED;
				flags_changed = true;
			}
		}
	}

	// Escape if nothing has changed.
	if ( !flags_changed ) {
		return false;
	} else {
		// (Re-)Initialize the duck ease.
		#ifdef PMOVE_EASE_BBOX_AND_VIEWHEIGHT
		pm->easeDuckHeight = QMEaseState::new_ease_state( pm->simulationTime, pmp->pm_duck_viewheight_speed );
		#endif
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
	const cm_trace_t trace = PM_Trace( ps->pmove.origin, pm->mins, pm->maxs, ps->pmove.origin );
	return !trace.allsolid;
}
/**
*	@brief	On exit, the origin will have a value that is pre-quantized to the PMove
*			precision of the network channel and in a valid position.
**/
static void PM_SnapPosition() {
	// Original with fixed stuck object snapping.
	#if 0
	ps->pmove.velocity = ps->pmove.velocity;
	ps->pmove.origin = ps->pmove.origin;
	if ( PM_GoodPosition() ) {
		return;
	} else {
		// <Q2RTXP>: WID: Q2RE really loves a 40fps drop?
		#if PMOVE_Q2RE_STUCK_FIXES
		// If we are not in a good position, we will try to snap to the previous origin.
		if ( G_FixStuckObject_Generic( ps->pmove.origin, pm->mins, pm->maxs ) == stuck_result_t::NO_GOOD_POSITION ) {
			// If we are still not in a good position, we will try to snap to the previous origin.
			ps->pmove.origin = pml.previousOrigin;
		}
		//if ( G_FixStuckObject_Generic( ps->pmove.origin, pm->mins, pm->maxs  ) == stuck_result_t::NO_GOOD_POSITION ) {
		//	ps->pmove.origin = pml.previousOrigin;
		//	return;
		//}
		#else
		ps->pmove.origin = pml.previousOrigin;
		#endif
	}
	#else
	ps->pmove.velocity = ps->pmove.velocity;
	ps->pmove.origin = ps->pmove.origin;
	if ( PM_GoodPosition() ) {
		return;
	} else {
		ps->pmove.origin = ps->pmove.origin = pml.previousOrigin;
		ps->pmove.velocity = ps->pmove.velocity = pml.previousVelocity;
	}
	#endif
}
/**
*	@brief	Try and find, then snap, to a valid non solid position.
**/
static void PM_InitialSnapPosition() {
	constexpr int32_t offset[ 3 ] = { 0, -1, 1 };
	const Vector3 baseOrigin = ps->pmove.origin;
	const Vector3 baseVelocity = ps->pmove.origin;
	
	for ( int32_t z = 0; z < 3; z++ ) {
		ps->pmove.origin.z = baseOrigin.z + offset[ z ];
		for ( int32_t y = 0; y < 3; y++ ) {
			ps->pmove.origin.y = baseOrigin.y + offset[ y ];
			for ( int32_t x = 0; x < 3; x++ ) {
				ps->pmove.origin.x = baseOrigin.x + offset[ x ];
				if ( PM_GoodPosition() ) {
					pml.previousOrigin = ps->pmove.origin;
					pml.previousVelocity = ps->pmove.velocity;
					return;
				}
			}
		}
	}

	ps->pmove.origin = baseOrigin;
	ps->pmove.velocity = baseVelocity;

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
	// No view changes at all in intermissions.
	if ( ps->pmove.pm_type == PM_INTERMISSION || ps->pmove.pm_type == PM_SPINTERMISSION ) {
		return;		// no view changes at all
	}
	// Dead, or Gibbed.
	if ( ps->pmove.pm_type != PM_SPECTATOR && ps->stats[ STAT_HEALTH ] <= 0 ) {
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
*
**/
static void PM_DropTimers() {
	if ( ps->pmove.pm_time ) {
			//int32_t msec = (int32_t)pm->cmd.msec >> 3;
			//double msec = ( pm->cmd.msec < 0. ? ( 1.0 / 0.125 ) : (pm->cmd.msec / 0.125 ) );
			double msec = ( pm->cmd.msec < 0. ? ( 1.0 / 8 ) : ( pm->cmd.msec / 8 ) );
			//if ( pm->cmd.msec < 0. ) { // This was <= 0, this seemed to cause a slight jitter in the player movement.
			//	msec = 1.0 / 8.; // 8 ms = 1 unit. (At 10hz.)
			//}

			if ( msec >= ps->pmove.pm_time ) {
				ps->pmove.pm_flags &= ~( PMF_ALL_TIMES );
				ps->pmove.pm_time = 0;
			} else {
				ps->pmove.pm_time -= msec;
			}
	}
}
/**
*	@brief	Can be called by either the server or the client game codes.
**/
void SG_PlayerMove( pmove_s *pmove, pmoveParams_s *params ) {
	// Store pointers to the pmove object and the parameters supplied for this move.
	pm = pmove;
	ps = pm->state;
	pmp = reinterpret_cast<pmoveParams_t *>( params );

	// Clear out several member variables which require a fresh state before performing the move.
	#if 1
		// Player State variables.
		ps->viewangles = {};
		//ps->pmove.viewheight = 0;
		ps->screen_blend[ 0 ] = ps->screen_blend[ 1 ] = ps->screen_blend[ 2 ] = ps->screen_blend[ 3 ] = 0;
		ps->rdflags = refdef_flags_t::RDF_NONE;
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
	// Clear all pmove local vars
	pml = {};

	// Store the origin and velocity in pmove local.
	//ps->pmove.origin = ps->pmove.origin;
	//ps->pmove.velocity = ps->pmove.velocity;
	// Save the origin as 'old origin' for in case we get stuck.
	pml.previousOrigin = ps->pmove.origin;
	// Save the start velocity.
	pml.previousVelocity = ps->pmove.velocity;

	// Calculate frameTime.
	pml.frameTime = (double)pm->cmd.msec * 0.001;

	// Clamp view angles.
	PM_UpdateViewAngles( ps, &pm->cmd );

	/**
	*	PM_SPECTATOR/PM_NOCLIP:
	**/
	// Performs fly move, only clips in case of spectator mode, noclips otherwise.
	if ( ps->pmove.pm_type == PM_SPECTATOR || ps->pmove.pm_type == PM_NOCLIP ) {
		//pml.frameTime = pmp->pm_fly_speed * pm->cmd.msec * 0.001f;

		// Re-ensure no flags are set anymore.
		ps->pmove.pm_flags = PMF_NONE;

		// Give the spectator a small 8x8x8 bounding box.
		if ( ps->pmove.pm_type == PM_SPECTATOR ) {
			pm->mins = { -8.f, -8.f, -8.f };
			pm->maxs = { -8.f, -8.f,-8.f };
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
	// Erase all input command state when dead, we don't want to allow user input to be moving our dead body.
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

		// Determine the animation move direction.
		PM_Animation_SetMovementDirection();

		// Determine water level and pursue to WaterMove if deep in above waist.
		if ( pm->liquid.level >= cm_liquid_level_t::LIQUID_WAIST ) {
			PM_WaterMove( );
		// Otherwise default to generic move code.
		} else {
			// Different pitch handling.
			Vector3 angles = ps->viewangles;
			if ( angles[ PITCH ] > 180. ) {
				angles[ PITCH ] = angles[ PITCH ] - 360.;
			}
			angles[ PITCH ] /= 3.;

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
	// Entering / Leaving water splashes.
	//PM_WaterEvents();
	// Snap us back into a validated position.
	PM_SnapPosition();
}

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

	pmp->pm_friction = default_pmoveParams_t::pm_friction;
	pmp->pm_water_friction = default_pmoveParams_t::pm_water_friction;
}

