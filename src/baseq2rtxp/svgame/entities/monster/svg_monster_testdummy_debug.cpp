/********************************************************************
*
*    ServerGame: TestDummy Debug Monster Edict (A* only) - Implementation
*
********************************************************************/

#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

// TODO: Move elsewhere.. ?
#include "refresh/shared_types.h"

// Entities
#include "sharedgame/sg_entities.h"

// SharedGame UseTargetHints.
#include "sharedgame/sg_usetarget_hints.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Player trail (Q2/Q2RTX pursuit trail)
#include "svgame/player/svg_player_trail.h"

#include "svgame/entities/monster/svg_monster_testdummy_debug.h"
// TestDummy helper functions live in the puppet TU and are not visible here by default.
// Declare the helpers we use so this TU can call them without pulling in the entire puppet CPP.
void SVG_TestDummy_GetNavAgentBBox( const svg_monster_testdummy_t *ent, Vector3 *out_mins, Vector3 *out_maxs );
bool SVG_TestDummy_TryQueueNavRebuild( svg_monster_testdummy_t *self, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, const bool force = false );
void SVG_TestDummy_ResetNavPath( svg_monster_testdummy_t *self );
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav.h"


// Navigation cluster routing (coarse tile routing pre-pass).
#include "svgame/nav/svg_nav_clusters.h"
// Async navigation queue helpers.
#include "svgame/nav/svg_nav_request.h"
// Traversal helpers required for path invalidation.
#include "svgame/nav/svg_nav_traversal.h"

// (Prototypes already declared above.)

//! Optional debug toggle for emitting async queue statistics.
extern cvar_t *s_nav_nav_async_log_stats;

// Local debug toggle for noisy per-frame prints in this test monster.
#ifndef DEBUG_PRINTS
#define DEBUG_PRINTS 1
#endif

#ifdef DEBUG_PRINTS
static constexpr int32_t DUMMY_NAV_DEBUG = 1;
#else
static constexpr int32_t DUMMY_NAV_DEBUG = 0;
#endif


/**
*	@brief	For this debug variant, we override the spawn and think callbacks to always attempt async A* to the activator.
*			Spawn for debug testdummy: call base onSpawn then set think to our simple loop.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn )( svg_monster_testdummy_debug_t *self ) -> void {
    // Call base spawn to initialize model/physics/etc.
    svg_monster_testdummy_t::onSpawn( self );

	// Set our monster flag so we can be detected as a monster by any relevant code (e.g. cluster routing).
	self->svFlags |= SVF_MONSTER;
	self->isActivated = false;
	self->SetUseCallback( &svg_monster_testdummy_debug_t::onUse );

    // Always run our debug thinker.
    self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
    self->nextthink = level.time + FRAME_TIME_MS;

	// Clear any pending async navigation state so we start clean when spawned/activated.
	SVG_TestDummy_ResetNavPath( self );
}

//=================================================================================================

/**
*	@brief	A* specific thinker: always attempt async A* to activator if present(and if it goes LOS, sets think to onThink_AStarPursuitTrail.), otherwise go idle.
*
*	@details	Will always check for player presence first, and if not present will check for trail presence.
*				If player is present, will attempt async A* to player.
*				If trail is present, sets nextThink to onThink_AStarPursuitTrail and attempt async A* to the trail's last known position.
*				If neither are present, will set nextThink to onThink_Idle.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarToPlayer )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*	Maintain base state and check liveness.
	**/
	if ( !self->OnThinkGeneric( ) ) {
		return;
	}

	if ( DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "=============================== onThink_AStarToPlayer ===============================\n" );
		const char *activatorName = "nullptr";
		if ( self->activator ) {
			activatorName = ( const char * )self->activator->classname;
		}

		gi.dprintf( "[NAV DEBUG] %s: time=%.2f, activator=%s, visible=%d\n",
			__func__, level.time.Seconds<double>(),
			activatorName,
			self->activator ? ( int32_t )SVG_Entity_IsVisible( self, self->activator ) : 0 );
	}

	if ( !self->isActivated ) {
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Sanity: if we have no valid activator (player) any more, stop trail pursuit.
	*		Breadcrumbs are only meaningful relative to a known player target.
	**/
	if ( !self->activator ) {
		self->trail_target = nullptr;
		self->goalentity = nullptr;
		SVG_TestDummy_ResetNavPath( self );
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    Exclusively use A* to target the activator.
	**/
	bool pursuing = false;
	if ( self->activator ) {
		if ( self->goalentity != self->activator ) {
			// If we just switched to a new activator goal, reset any pending path state
			// so we do not reuse stale async requests/path buffers.
			self->goalentity = self->activator;
         // Clear any breadcrumb target so trail-follow state does not fight direct pursuit.
			self->trail_target = nullptr;
			SVG_TestDummy_ResetNavPath( self );

			// While directly targeting the activator, ignore breadcrumb trail spots
			// by marking the trail_time to now. This mirrors direct-pursuit behavior
			// used in the puppet testdummy so PickFirst will prefer newer trail
			// entries once LOS is lost.
			self->trail_time = level.time;
		}

		// Always attempt to move to activator origin while in this state.
		pursuing = self->MoveAStarToOrigin( self->activator->currentOrigin );
	}

	/**
	*    Transition to trail-following if LOS is lost or A* fails.
	**/
	/**
	*    If the activator went out of sight and we have no pending async nav work,
	*    transition to trail-following or idle. Use the async request query helper
	*    so we respect both queued and running states (replace semantics in the
	*    nav queue means rebuild_in_progress alone is not sufficient).
	**/
	if ( self->activator && !SVG_Entity_IsVisible( self, self->activator ) && !SVG_Nav_IsRequestPending( &self->navPathProcess ) ) {
       /**
		*    Activator just went out of sight:
		*        - Stop using the direct-pursuit goal.
		*        - Seed trail-follow with a stable first breadcrumb.
		*        - Clear any pending async request/path built for the activator so it
		*          cannot keep driving motion/orientation after we switch modes.
		**/
		// Acquire the freshest breadcrumb entry to pursue.
		svg_base_edict_t *trailSpot = PlayerTrail_PickFirst( self );
		if ( trailSpot ) {
			// Cache the breadcrumb so trail pursuit does not thrash between spots.
			self->trail_target = trailSpot;
			self->trail_time = trailSpot->timestamp;
			self->goalentity = trailSpot;

			// Reset nav state so trail pursuit starts with a clean async request.
			SVG_TestDummy_ResetNavPath( self );

			// Switch to breadcrumb pursuit.
			self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_AStarPursuitTrail );
		} else {
			// Nothing to pursue; go idle.
			self->trail_target = nullptr;
			self->goalentity = nullptr;
			SVG_TestDummy_ResetNavPath( self );
			self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
		}
	}

	/**
	*    Physics and collision.
	**/
	const int32_t blockedMask = self->PerformSlideMove( );
	// Ensure the authoritative yaw/angles are applied to the entity state after movement
	// so rendering and networking see the updated orientation. We only update the
	// entityState angles here (assignToEntityState = true) to keep currentAngles authoritative.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	/**
	*    Obstruction recovery.
	**/
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		self->navPathProcess.next_rebuild_time = 0_ms;
	}

	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	A* specific thinker: always attempt async A* to activator trail if present, and otherwise go idle.
*
*	@details	Will always check for player presence first, and if not present will check for trail presence.
*			If trail is present, will attempt async A* to the trail's last known position.
*			If the current breadcrumb is reached, invalid, or its async path expires, this thinker transitions to the next trail spot.
*			If player is present, sets nextThink to onThink_AStarToPlayer and attempt async A* to player.
*			If neither are present, will set nextThink to onThink_Idle.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarPursuitTrail )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*	Maintain base state and check liveness.
	**/
	if ( !self->OnThinkGeneric( ) ) {
		return;
	}

	if ( DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "=============================== onThink_AStarPursuitTrail ===============================\n" );

		const char *trailName = "nullptr";
		if ( self->trail_target ) {
			trailName = ( const char * )self->trail_target->classname;
		}

		gi.dprintf( "[NAV DEBUG] %s: time=%.2f, trail_target=%s\n",
			__func__, level.time.Seconds<double>(),
			trailName );
	}

	if ( !self->isActivated ) {
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    If the player becomes visible again, return to direct A* pursuit.
	**/
    if ( self->activator
		&& SVG_Entity_IsInFrontOf( self, self->activator, { 1.0f, 1.0f, 0.0f }, 0.35f )
		&& SVG_Entity_IsVisible( self, self->activator ) )
	{
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_AStarToPlayer );
		// Immediate attempt.
		self->goalentity = self->activator;
		// Reset pending async path requests when immediately switching to direct pursuit
		// to avoid stale async results interfering with the new goal.
		SVG_TestDummy_ResetNavPath( self );
		// Immediate attempt.
		self->MoveAStarToOrigin( self->activator->currentOrigin );
		 // Skip trail logic if we transitioned.
		goto physics;
	}

   {
		/**
		*    Helper for assigning a breadcrumb target and enqueuing its path.
		**/
		auto assignTrailTarget = [&]( svg_base_edict_t *spot, const bool forceRebuild ) -> void {
			if ( !spot ) {
				return;
			}
			self->goalentity = spot;
			self->trail_target = spot;
			self->trail_time = spot->timestamp;
			if ( forceRebuild ) {
				SVG_TestDummy_ResetNavPath( self );
			}
			self->MoveAStarToOrigin( spot->currentOrigin, forceRebuild );
		};

		/**
		*    Helper for advancing to the next breadcrumb when the current one is exhausted.
		**/
		auto advanceTrailTarget = [&]() -> bool {
			svg_base_edict_t *nextSpot = PlayerTrail_PickNext( self );
			if ( !nextSpot ) {
				return false;
			}
			if ( nextSpot == self->trail_target || nextSpot->timestamp <= self->trail_time ) {
				return false;
			}
			assignTrailTarget( nextSpot, true );
			return true;
		};

		/**
		*    Determine when to progress to the next breadcrumb.
		**/
     auto shouldAdvanceTrailTarget = [&]() -> bool {
			/**
			*	Advance policy:
			*		Only advance to the next breadcrumb when we are physically close to the
			*		current breadcrumb. Advancing just because a path is pending/expired is a
			*		major source of oscillation: we can swap goals while still far away, which
			*		causes the mover to alternate between two attractive breadcrumbs.
			**/
			if ( !self->trail_target ) {
				return false;
			}
			// Compute 2D distance to current breadcrumb.
			const Vector3 toTrail = QM_Vector3Subtract( self->trail_target->currentOrigin, self->currentOrigin );
			const float horizontalDist2 = ( toTrail.x * toTrail.x ) + ( toTrail.y * toTrail.y );
			// Use the same radius we use for nav waypoint advancement, with a sane fallback.
			const float arrivalRadius = ( self->navPathPolicy.waypoint_radius > 0.0f ) ? self->navPathPolicy.waypoint_radius : 32.0f;
			return horizontalDist2 <= ( arrivalRadius * arrivalRadius );
		};

     /**
		*    Ensure we have a breadcrumb target before attempting movement.
		*
		*    IMPORTANT:
		*        Do not call PickFirst every frame once we have a trail target.
		*        PickFirst chooses the "best" breadcrumb relative to current state.
		*        As we move/turn that ranking can change, causing oscillation.
		*        We only (re)select when the current breadcrumb is exhausted.
		**/
		svg_base_edict_t *trailSpot = self->trail_target;
		bool justAssignedTrailSpot = false;
		if ( !trailSpot ) {
			// Acquire the freshest breadcrumb entry to pursue.
			trailSpot = PlayerTrail_PickFirst( self );
			if ( !trailSpot ) {
				self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
				goto physics;
			}
            // Start following the newly selected breadcrumb immediately.
			// Avoid forcing a full async rebuild on every assignment to prevent
			// flooding the async queue when breadcrumbs are acquired frequently.
			// Let the normal debounce/refresh behavior handle minor changes.
			assignTrailTarget( trailSpot, false );
			justAssignedTrailSpot = true;
		} else {
			/**
			*    Validate that the cached breadcrumb still makes sense.
			*    If the cached spot is older than our recorded trail_time (or has no
			*    meaningful timestamp), reacquire so we do not chase stale spots.
			**/
			if ( trailSpot->timestamp <= 0_ms || trailSpot->timestamp < self->trail_time ) {
				trailSpot = PlayerTrail_PickFirst( self );
				if ( !trailSpot ) {
					self->trail_target = nullptr;
					self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
					goto physics;
				}
                // Reacquire a fresher breadcrumb and request a rebuild but do not
				// force it immediately. This prevents per-frame forced rebuilds
				// when timestamps drift slightly.
				assignTrailTarget( trailSpot, false );
				justAssignedTrailSpot = true;
			} else {
				// Continue working toward the cached breadcrumb.
				self->goalentity = trailSpot;
				self->MoveAStarToOrigin( trailSpot->currentOrigin );
			}
		}

		/**
		*    Move on to the next breadcrumb when we arrived or lost the current path.
		**/
		if ( !justAssignedTrailSpot && shouldAdvanceTrailTarget() ) {
			if ( !advanceTrailTarget() ) {
				self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
			}
		}
	}

physics:
	/**
	*    Physics and collision.
	**/
    const int32_t blockedMask = self->PerformSlideMove( );
	// Ensure the authoritative yaw/angles are applied to the entity state after movement
	// so rendering and networking see the updated orientation. Keep currentAngles authoritative.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	/**
	*    Obstruction recovery.
	**/
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		self->navPathProcess.next_rebuild_time = 0_ms;
	}

	self->nextthink = level.time + FRAME_TIME_MS;
}

//=================================================================================================


/**
*	@brief	Always looks for activator presence, or its trail, and otherwise does nothing.
*
*	@details	If we are in idle state, it means we failed to find a valid target to pursue in the previous think.
*				If activator is present, sets nextThink to onThink_AStarToPlayer and attempt async A* to player.
*				If trail is present, sets nextThink to onThink_AStarPursuitTrail and attempt async A* to the trail's last known position.
*				If neither are present, will keep nextThink as onThink_Idle and do nothing.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Idle )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*	Generic think logic.
	**/
	if ( !self->OnThinkGeneric( ) ) {
		return;
	}

	if ( DUMMY_NAV_DEBUG != 0 && self->isActivated ) {
		gi.dprintf( "=============================== onThink_Idle ===============================\n" );

		gi.dprintf( "[NAV DEBUG] %s: time=%.2f, searching for target...\n",
			__func__, level.time.Seconds<double>() );
	}

	/**
	*	Default idle behavior: manual animation selection and zero horizontal velocity.
	**/
	if ( self->rootMotionSet && self->rootMotionSet->motions[ 1 ] ) {
		skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 1 ]; // IDLE
		const double t = level.time.Seconds<double>();
		const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
		const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
		self->s.frame = rootMotion->firstFrameIndex + localFrame;
	}
	self->velocity.x = self->velocity.y = 0.0f;

	/**
	*	Clear goalentity and navigation state while idling.
	**/
	{
		/**
		*	Idle means we should not be pursuing any previous goal.
		*		Clear cached breadcrumb/goal as well as any async/path state so that
		*		reactivations and LOS transitions cannot reuse stale steering.
		**/
		self->trail_target = nullptr;
		self->goalentity = nullptr;
		SVG_TestDummy_ResetNavPath( self );
	}

	if ( !self->isActivated ) {
		SVG_Util_SetEntityAngles( self, self->currentAngles, true );
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Always look for the player(activator) so we can react immediately when they appear.
	**/
	if ( self->activator 
		&& SVG_Entity_IsInFrontOf( self, self->activator, { 1.0f, 1.0f, 0.0f }, 0.2f ) 
		&& SVG_Entity_IsVisible( self, self->activator ) ) {
		// Immediate action.
		self->goalentity = self->activator;
		// Set the nextThink to AStarToPlayer so we start chasing the player right away.
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_AStarToPlayer );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Trail-follow acquisition:
	*		Breadcrumb trail spots are position markers, not enemies. Requiring them to be
	*		"in front" or "visible" makes the selection unstable (as we move/turn we can
	*		flip between different breadcrumbs), which presents as back-and-forth jitter.
	*		Instead, once the player is not directly visible, we may pursue the freshest
	*		breadcrumb unconditionally.
	**/
	// 
	svg_base_edict_t *trailTarget = PlayerTrail_PickFirst( self );
	// 
	if ( trailTarget ) {
		// Cache the breadcrumb target so pursuit mode starts with a stable goal.
		self->trail_target = trailTarget;
		self->trail_time = trailTarget->timestamp;

		self->goalentity = trailTarget;

		// Reset nav path so we do not reuse a path built for a different goal.
		SVG_TestDummy_ResetNavPath( self );

		// Set the nextThink to PursuitTrail so we start following the player's trail right away.
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_AStarPursuitTrail );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	// Animation/angles always applied.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	/**
	*	Set the nextThink to Idle so we keep looking for the player or their trail instead of trying to pursue a non-visible target.
	**/
	self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
	self->nextthink = level.time + FRAME_TIME_MS;
}

//=============================================================================================

/**
*    @brief    Handle use toggles so the debug pursuit only runs when activated.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_monster_testdummy_debug_t, onUse )( svg_monster_testdummy_debug_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	svg_monster_testdummy_t::onUse( self, other, activator, useType, useValue );

	const bool activating = ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE
		&& useValue == 1
		&& activator
		&& activator->client );

	self->isActivated = activating;

	/**
	*	Activation state change handling:
	*		- When disabling, stop any pursuit immediately and clear nav/trail state.
	*		- When enabling, start from idle so we acquire player/trail cleanly.
	**/
	if ( !self->isActivated ) {
		// Clear any cached breadcrumb/goal so we do not keep pursuing after disabling.
		self->trail_target = nullptr;
		self->goalentity = nullptr;
		// Cancel/clear any async nav request/path so it cannot keep steering motion.
		SVG_TestDummy_ResetNavPath( self );
		// Return to idle thinker. (So it can scan for a player/trail goal.)
		self->nextthink = level.time + FRAME_TIME_MS;
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
	} else {
		// On activation, reset nav state and reacquire targets from scratch.
		self->trail_target = nullptr;
		self->goalentity = nullptr;
		// Cancel/clear any async nav request/path so it cannot keep steering motion.
		SVG_TestDummy_ResetNavPath( self );
		// Return to idle thinker. (So it can scan for a player/trail goal.)
		self->nextthink = level.time + FRAME_TIME_MS;
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
	}

	if ( DUMMY_NAV_DEBUG != 0 ) {
		const char *activatorName = "nullptr";
		if ( activator ) {
			activatorName = ( const char * )activator->classname;
		}

		gi.dprintf( "[NAV DEBUG] %s: isActivated=%d, activator=%s\n",
			__func__, ( int32_t )self->isActivated, activatorName );
	}

	//self->trail_target = nullptr;
	//SVG_TestDummy_ResetNavPath( self );

	//if ( self->isActivated ) {
	//	self->goalentity = activator;
	//	self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_AStarToPlayer );
	//} else {
	//	self->goalentity = nullptr;
	//	self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
	//}

	self->nextthink = level.time + FRAME_TIME_MS;
}

//=============================================================================================

/**
*	@brief	Set when dead. Does nothing.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Dead )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*	Recategorize position and check grounding so we don't get stuck in invalid geometry and so 
	*	the corpse can interact with the world properly.
	**/
	// Clear visual flags that don't make sense for a corpse.
	self->s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );
	// Recategorize position and check grounding so we don't get stuck in invalid geometry and so the corpse can interact with the world properly.
	self->RecategorizeGroundAndLiquidState();

	// Set SVF_DEADENTITY and clear SVF_MONSTER so we don't get treated as a living monster by the world and other entities, 
	// but still get treated as an entity that can interact with the world and be hit by traces, etc.
	self->svFlags &= ~( SVF_MONSTER );
	self->svFlags |= SVF_DEADENTITY;

	/**
	*	Friction: dampen horizontal velocity.
	**/
	self->velocity.x *= 0.8f;
	self->velocity.y *= 0.8f;
	if ( std::fabs( self->velocity.x ) < 0.1f ) self->velocity.x = 0.0f;
	if ( std::fabs( self->velocity.y ) < 0.1f ) self->velocity.y = 0.0f;

	if ( self->lifeStatus != LIFESTATUS_ALIVE ) {
	// Dead: play death anims as before.
		if ( self->s.frame >= 512 && self->s.frame < 642 ) {
			self->s.frame++;
			if ( self->s.frame >= 642 ) {
				self->s.frame = 641;
			}
		} else if ( self->s.frame >= 642 && self->s.frame < 801 ) {
			self->s.frame++;
			if ( self->s.frame >= 801 ) {
				self->s.frame = 800;
			}
		} else if ( self->s.frame >= 801 && self->s.frame < 928 ) {
			self->s.frame++;
			if ( self->s.frame >= 928 ) {
				self->s.frame = 927;
			}
		}
	}

	// Stay dead.
	self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Dead );
	self->nextthink = level.time + FRAME_TIME_MS;
}


//=============================================================================================
//=============================================================================================


/**
*	@brief	Generic support routine taking care of the base logic that each onThink implementation relies on. 
*	@return	True if the caller should proceed with its specific think logic, or false if it should return early and skip the specific think logic.
**/
const bool svg_monster_testdummy_debug_t::OnThinkGeneric() {
	/**
	*	Clear visual flags.
	**/
	s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

	/**
	*	Setup A* Navigation Policy: stairs, drops, and obstruction jumping.
	**/
	navPathPolicy.waypoint_radius = 32.0f;
	navPathPolicy.max_step_height = 18.0;
	navPathPolicy.max_drop_height = 128.0;
	navPathPolicy.cap_drop_height = true;
	navPathPolicy.drop_cap = ( nav_drop_cap && nav_drop_cap->value > 0.0f ) ? nav_drop_cap->value : 64;
	navPathPolicy.enable_goal_z_layer_blend = true;
	navPathPolicy.blend_start_dist = 0.0f;
	navPathPolicy.blend_full_dist = 18.0f;
	navPathPolicy.allow_small_obstruction_jump = true;
	navPathPolicy.max_obstruction_jump_height = 36.0;

	/**
	*    Recategorize position and check grounding.
	**/
	RecategorizeGroundAndLiquidState();

	/**
	*    Liveness check.
	**/
	if ( health <= 0 || lifeStatus != LIFESTATUS_ALIVE ) {
		// Transition and remain in the dead thinker and do nothing if we are dead.
		SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Dead );
		nextthink = level.time + FRAME_TIME_MS;
		// Return false to indicate the caller should skip its specific think logic since we are now dead and should only be running the dead thinker.
		return false;
	}

	// Return true to indicate the caller can proceed with its specific think logic after this generic logic is done.
	return true;
}



//=============================================================================================
//=============================================================================================



/**
*	@brief	Performs the actual SlideMove processing and updates the final origin if successful.
**/
const int32_t svg_monster_testdummy_debug_t::PerformSlideMove() {
	// Setup the move structure with our current state and the movement policy we want to use for this move.
	mm_move_t monsterMove = {
		.monster = this,
		.frameTime = gi.frame_time_s,
		.mins = mins,
		.maxs = maxs,
		.state = {
			.mm_type = MM_NORMAL,
			.mm_flags = ( groundInfo.entityNumber != ENTITYNUM_NONE ? MMF_ON_GROUND : MMF_NONE ),
			.mm_time = 0,
			.gravity = ( int16_t )( gravity * sv_gravity->value ),
			.origin = currentOrigin,
			.velocity = velocity,
			.previousOrigin = currentOrigin,
			.previousVelocity = velocity,
		},
		.ground = groundInfo,
		.liquid = liquidInfo,
	};

	// Perform the slide move and get the blocked mask describing the result of the movement attempt.
	const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove, navPathPolicy );
	// After blockedMask is computed (in PerformSlideMove)
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_WALL_BLOCKED ) ) != 0 ) {
		navPathProcess.next_rebuild_time = 0_ms;

		if ( navPathPolicy.allow_small_obstruction_jump &&
			( monsterMove.state.mm_flags & MMF_ON_GROUND ) ) {
			const float g = ( float )( gravity * sv_gravity->value );
			const float h = ( float )navPathPolicy.max_obstruction_jump_height;

			if ( g > 0.0f && h > 0.0f ) {
				// v = sqrt(2 * g * h)
				const float jumpVel = std::sqrt( 2.0f * g * h );
				// Apply only if we’re not already moving upward
				if ( velocity.z < jumpVel ) {
					velocity.z = jumpVel;

					monsterMove.state.mm_flags &= ~MMF_ON_GROUND; // We are now airborne due to the jump, so clear the on-ground flag to prevent multiple jumps in a row without landing.
				}
			}
		}
	}
	#if 0
	// After blockedMask is computed (in PerformSlideMove)
	if (
		// In case of an actual wall block obstruction.
		( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_WALL_BLOCKED ) ) != 0
		//|| ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) 
		) {
		navPathProcess.next_rebuild_time = 0_ms;

		if ( navPathPolicy.allow_small_obstruction_jump &&
			( monsterMove.state.mm_flags & MMF_ON_GROUND ) ) {
			const float g = ( float )( gravity * sv_gravity->value );
			const float h = ( float )navPathPolicy.max_obstruction_jump_height;

			if ( g > 0.0f && h > 0.0f ) {
				// v = sqrt(2 * g * h)
				const float jumpVel = std::sqrt( 2.0f * g * h );
				// Apply only if we’re not already moving upward
				if ( monsterMove.velocity.z < jumpVel ) {
					monsterMove.velocity.z = jumpVel;
				}
			}
		}
	}

	// After SVG_MMove_StepSlideMove(...) and before applying final origin
	//if ( monsterMove.ground.entityNumber != ENTITYNUM_NONE ) {
		const bool willBeAirborne = ( monsterMove.ground.entityNumber == ENTITYNUM_NONE );
		if ( willBeAirborne ) {
			Vector3 start = monsterMove.state.origin;
			Vector3 forwardDir = QM_Vector3Normalize( monsterMove.state.velocity );
			Vector3 end = start + forwardDir * 24.0f;
			end.z -= navPathPolicy.max_drop_height;

			svg_trace_t tr = gi.trace( &start, &monsterMove.mins, &monsterMove.maxs, &end, this, CM_CONTENTMASK_SOLID );
			const float drop = start.z - tr.endpos[ 2 ];

			const float policyDropLimit = ( navPathPolicy.drop_cap > 0.0f )
				? ( float )navPathPolicy.drop_cap
				: ( navPathPolicy.cap_drop_height ? ( float )navPathPolicy.max_drop_height : 0.0f );

			if ( tr.fraction < 1.0f && policyDropLimit > 0.0f && drop > policyDropLimit ) {
				monsterMove.state.origin = monsterMove.state.origin - ( forwardDir * 24.f );
				monsterMove.state.velocity.x = 0.0f;
				monsterMove.state.velocity.y = 0.0f;
				monsterMove.ground = groundInfo;
			}
		}
	//}
	#endif
	// If we are not blocked or trapped, we can update our position and grounding info. Otherwise, we will rely on the next think to attempt recovery and not update our position so we don't get stuck in invalid geometry.
	if ( !( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) ) {
		// Update position and grounding info.
		groundInfo = monsterMove.ground;
		liquidInfo = monsterMove.liquid;
		// Update the entity's origin to the new position after movement, and update velocity to the new velocity after movement.
		velocity = monsterMove.state.velocity;
		SVG_Util_SetEntityOrigin( this, monsterMove.state.origin, true );
		// Update the entity's link in the world after changing its position.
		gi.linkentity( this );
	}

	// After blockedMask is computed (in PerformSlideMove)
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		navPathProcess.next_rebuild_time = 0_ms;

		if ( navPathPolicy.allow_small_obstruction_jump &&
			( monsterMove.state.mm_flags & MMF_ON_GROUND ) ) {
			const float g = ( float )( gravity * sv_gravity->value );
			const float h = ( float )navPathPolicy.max_obstruction_jump_height;

			if ( g > 0.0f && h > 0.0f ) {
				// v = sqrt(2 * g * h)
				const float jumpVel = std::sqrt( 2.0f * g * h );
				// Apply only if we’re not already moving upward
				if ( velocity.z < jumpVel ) {
					velocity.z = jumpVel;
					monsterMove.state.mm_flags &= ~MMF_ON_GROUND; // We are now airborne due to the jump, so clear the on-ground flag to prevent multiple jumps in a row without landing.
				}
			}
		}
	}

	// Return the blocked mask so the caller can decide how to react to obstructions.
	return blockedMask;
}


//=============================================================================================
//=============================================================================================


/**
*    @brief    Attempt A* navigation to a target origin and apply local movement/animation.
*    @param    goalOrigin    World-space destination used for the rebuild request (feet-origin).
*    @param    force         When true, bypass throttles/heuristics and rebuild immediately.
*    @return   True if movement/animation was updated (caller can expect velocity/frames to have changed).
*    @note     This implementation provides a responsive direct-steer fallback while async path generation
*              is queued so the debug monster remains visually responsive even when a path has not yet
*              been produced by the async nav system.
**/
const bool svg_monster_testdummy_debug_t::MoveAStarToOrigin( const Vector3 &goalOrigin, bool force ) {
	/**
	*    Derive the correct nav-agent bbox (feet-origin) for traversal.
	**/
	Vector3 agent_mins, agent_maxs;
	SVG_TestDummy_GetNavAgentBBox( this, &agent_mins, &agent_maxs );

	// Debug print summarizing the request state for diagnostics.
	if ( DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "[NAV DEBUG] %s: goal=(%.1f %.1f %.1f) force=%d pathOk=%d pending=%d\n",
			__func__, goalOrigin.x, goalOrigin.y, goalOrigin.z, ( int32_t )force,
			( int32_t )( navPathProcess.path.num_points > 0 ),
			( int32_t )SVG_Nav_IsRequestPending( &navPathProcess ) );
	}

	/**
	*    Sanity / arrival check: stop moving if we are effectively at the goal already to prevent jitter.
	**/
	Vector3 toGoalDist = QM_Vector3Subtract( goalOrigin, currentOrigin );
	// Only consider horizontal distance for arrival.
	toGoalDist.z = 0.0f;
	if ( QM_Vector3Length( toGoalDist ) < 0.03125f ) {
		// Zero horizontal velocity to prevent micro-jitter.
		velocity.x = velocity.y = 0.0f;
		return false;
	}

	/**
	*	Attempt to queue a navigation rebuild for the target origin.
	*	Force ensures the helpers bypass throttles/heuristics when the caller demands an immediate rebuild.
	*	The helper will respect internal throttles unless `force` is true.
	**/
	if ( goalentity == activator && activator && !SVG_Entity_IsVisible( this, activator ) && !force ) {
		// Skip queuing a new/refresh request toward an activator we cannot see.
		// Additionally: aggressively cancel any outstanding async request that
		// was targeted at the now-invisible activator so we can transition to
		// trail-following without waiting for a stale queued result to complete.
		if ( DUMMY_NAV_DEBUG != 0 ) {
			gi.dprintf( "[NAV DEBUG] %s: skipping queue to invisible activator\n", __func__ );
		}

		// If there is a pending async request for our path process, cancel it
		// and advance the generation so any in-flight results are ignored.
		if ( SVG_Nav_IsRequestPending( &navPathProcess ) || navPathProcess.rebuild_in_progress || navPathProcess.pending_request_handle != 0 ) {
			if ( navPathProcess.pending_request_handle != 0 ) {
				if ( DUMMY_NAV_DEBUG != 0 ) {
					gi.dprintf( "[NavAsync][Cancel] Cancelling pending request handle=%d for ent_process=%p\n",
						navPathProcess.pending_request_handle, ( void * )&navPathProcess );
				}
				SVG_Nav_CancelRequest( ( nav_request_handle_t )navPathProcess.pending_request_handle );
			}

			// Mark that we have no pending request and that a rebuild is not in progress.
			navPathProcess.pending_request_handle = 0;
			navPathProcess.rebuild_in_progress = false;

			// Bump generation to ensure any late async results are discarded.
			++navPathProcess.request_generation;

			// Apply a conservative backoff so we do not immediately requeue and thrash the async queue.
			navPathProcess.backoff_until = level.time + navPathPolicy.fail_backoff_base;
			// Also throttle the next rebuild attempt slightly using the standard rebuild interval.
			navPathProcess.next_rebuild_time = level.time + navPathPolicy.rebuild_interval;
		}
    } else {
		SVG_TestDummy_TryQueueNavRebuild( this, currentOrigin, goalOrigin, navPathPolicy, agent_mins, agent_maxs, force );
		//// Only honor an explicit force request when the goal moved beyond the
		//// configured rebuild thresholds. This prevents callers from forcing each
		//// frame for negligible movements.
		//bool effectiveForce = force;
		//if ( force ) {
		//	// Compute 2D and 3D movement deltas relative to our last recorded
		//	// nav goal (if available) so we only force when movement is meaningful.
		//	const Vector3 &referenceGoal = last_nav_goal_valid ? last_nav_goal_origin : navPathProcess.path_goal;
		//	const double dx = QM_Vector3LengthDP( QM_Vector3Subtract( goalOrigin, referenceGoal ) );
		//	// If the goal moved less than the 2D threshold, do not force.
		//	if ( dx <= navPathPolicy.rebuild_goal_dist_2d ) {
		//		effectiveForce = false;
		//		if ( DUMMY_NAV_DEBUG != 0 ) {
		//			gi.dprintf( "[NAV DEBUG] %s: suppressed force rebuild (dx=%.2f <= thresh=%.2f)\n",
		//				__func__, dx, navPathPolicy.rebuild_goal_dist_2d );
		//		}
		//	}
		//}

		//const bool queued = SVG_TestDummy_TryQueueNavRebuild( this, currentOrigin, goalOrigin, navPathPolicy, agent_mins, agent_maxs, effectiveForce );
		//// If we successfully requested a rebuild and the caller intended a force,
		//// record the last forced goal so subsequent small deltas do not re-force.
		//if ( queued && force ) {
		//	last_nav_goal_origin = goalOrigin;
		//	last_nav_goal_valid = true;
		//}
	}

	/**
	*    Path availability and request state checks.
	**/
	// We may have a path process in flight but no populated path buffer yet; check both.
	const bool hasPathPoints = ( navPathProcess.path.num_points > 0 && navPathProcess.path.points );
	const bool pathExpired = hasPathPoints && navPathProcess.path_index >= navPathProcess.path.num_points;
	// Valid usable path only when points exist and we haven't consumed them all.
	const bool pathOk = hasPathPoints && !pathExpired;
	// Track whether an async request is still in flight so we can adjust fallback/turning behavior.
	const bool requestPending = SVG_Nav_IsRequestPending( &navPathProcess );

	// If we don't have a path available yet, compute a direct 2D direction to the
	// goal and use that as a temporary movement direction. This avoids the test
	// dummy appearing stuck while async navigation rebuilds.
	/**
	*    Movement direction selection:
	*        1) Default to current horizontal velocity (momentum) if available.
	*        2) If no valid path is available, compute a direct 2D steer to the goal as a responsive fallback.
	*        3) If a valid path exists, query its 3D direction and follow that.
	**/
	Vector3 move_dir = ( velocity.x != 0.0f || velocity.y != 0.0f )
		? QM_Vector3Normalize( { velocity.x, velocity.y, 0.0f } )
		: Vector3{ 0.0f, 0.0f, 0.0f };

	/**
	*    Async fallback: aim directly toward the goal to keep movement responsive.
	**/
	if ( !pathOk ) {
		Vector3 toGoal = goalOrigin;
		// Project goal to our horizontal plane so vertical offsets do not affect steering.
		toGoal.z = currentOrigin.z;
		// Compute the horizontal direction to the goal.
		toGoal = QM_Vector3Subtract( toGoal, currentOrigin );
		// Check for a non-trivial direction before normalizing.
		const float len2 = ( toGoal.x * toGoal.x ) + ( toGoal.y * toGoal.y );
		if ( len2 > ( 0.001f * 0.001f ) ) {
			// Normalize and face temporary movement direction.
			move_dir = QM_Vector3Normalize( toGoal );
			ideal_yaw = QM_Vector3ToYaw( move_dir );
			yaw_speed = 7.5f;
			SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );

			// Play run animation while directly steering.
			if ( rootMotionSet && rootMotionSet->motions[ 4 ] ) {
				skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 4 ]; // RUN
				const double t = level.time.Seconds<double>();
				const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
				const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
				s.frame = rootMotion->firstFrameIndex + localFrame;
			}
		}
	}

	// If we have a valid path, try to get the movement direction from it. If that fails for some reason, we will just keep our current move_dir which is based on our current velocity or the direct-steer fallback above.
	if ( pathOk && navPathProcess.QueryDirection3D( currentOrigin, navPathPolicy, &move_dir ) ) {
		/**
		*    Preserve trail bookkeeping while following breadcrumbs and only
		*    advance the trail marker to "now" when directly chasing the
		*    activator. Using level.time unconditionally made us treat every
		*    existing breadcrumb as stale, which caused PickFirst/PickNext to
		*    skip the trail entirely.
		*/
		if ( trail_target && goalentity == trail_target ) {
			trail_time = trail_target->timestamp;
		} else {
			trail_time = level.time;
		}

		// Face the movement direction on the horizontal plane with snappier turning.
		Vector3 faceDir = move_dir;
		faceDir.z = 0.0f;
		if ( ( faceDir.x * faceDir.x + faceDir.y * faceDir.y ) > 0.001f ) {
			ideal_yaw = QM_Vector3ToYaw( faceDir );
			yaw_speed = 15.0f; // Snappier turning for the debug variant.
			SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
		}

		// Run animation while following a path.
		if ( rootMotionSet && rootMotionSet->motions[ 4 ] ) {
			skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 4 ]; // RUN
			const double t = level.time.Seconds<double>();
			const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
			const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
			s.frame = rootMotion->firstFrameIndex + localFrame;
		}

		// Apply horizontal velocity derived from the path direction.
		constexpr double frameVelocity = 220.0;
		velocity.x = ( float )( move_dir.x * frameVelocity );
		velocity.y = ( float )( move_dir.y * frameVelocity );
		return true;
	}

	/**
	*    Apply the fallback direction when the async queue is idle so we do not
	*    mask a pending nav rebuild with direct steering.
	*    Fallback movement application: if we have a non-trivial fallback direction
	*    (either from momentum or direct-steer), apply it so the entity moves while
	*    waiting for async nav results.
	**/
	const float fallbackLen2 = ( move_dir.x * move_dir.x ) + ( move_dir.y * move_dir.y );
	if ( fallbackLen2 > ( 0.001f * 0.001f ) ) {
		constexpr double frameVelocity = 220.0;
		velocity.x = ( float )( move_dir.x * frameVelocity );
		velocity.y = ( float )( move_dir.y * frameVelocity );
		return true;
	}

	/**
	*    Face the current horizontal move direction even when awaiting async paths.
	**/
	const float horizLen2 = ( move_dir.x * move_dir.x ) + ( move_dir.y * move_dir.y );
	if ( horizLen2 > ( 0.001f * 0.001f ) ) {
		Vector3 faceDir = move_dir;
		faceDir.z = 0.0f;
		ideal_yaw = QM_Vector3ToYaw( faceDir );
		yaw_speed = requestPending ? 10.0f : 15.0f;
		SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
	}

	return false;
}



//=============================================================================================
//=============================================================================================



/**
*    @brief    Slerp direction helper. (Local to this TU to avoid parent dependency).
**/
const Vector3 svg_monster_testdummy_debug_t::SlerpLocalVector3( const Vector3 &from, const Vector3 &to, float t ) {
	float dot = QM_Vector3DotProduct( from, to );
	float aFactor, bFactor;
	if ( std::fabs( dot ) > 0.9995f ) {
		aFactor = 1.0f - t;
		bFactor = t;
	} else {
		float ang = std::acos( dot );
		float sinOmega = std::sin( ang );
		aFactor = std::sin( ( 1.0f - t ) * ang ) / sinOmega;
		bFactor = std::sin( t * ang ) / sinOmega;
	}
	return from * aFactor + to * bFactor;
}

/**
*	@brief	Recategorize the entity's ground/liquid and ground states.
**/
const void svg_monster_testdummy_debug_t::RecategorizeGroundAndLiquidState() {
	// Get the mask for checking ground and recategorizing position.
	const cm_contents_t mask = SVG_GetClipMask( this );
	// Check for ground and recategorize position so we can settle on the floor and interact with the world properly instead of being stuck in the air or in a wall.
	M_CheckGround( this, mask );
	// Recategorize position so we can update our liquid level/type and so we can properly interact with the world instead of being stuck in the air or in a wall.
	M_CatagorizePosition( this, currentOrigin, liquidInfo.level, liquidInfo.type );
}
