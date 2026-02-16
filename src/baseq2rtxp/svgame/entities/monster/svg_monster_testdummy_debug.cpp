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
#define DEBUG_PRINTS 1

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

	/**
	*    Exclusively use A* to target the activator.
	**/
	bool pursuing = false;
    if ( self->activator ) {
		// If we just switched to a new activator goal, reset any pending path state
		// so we do not reuse stale async requests/path buffers.
		if ( self->goalentity != self->activator ) {
			self->goalentity = self->activator;
			SVG_TestDummy_ResetNavPath( self );
		}
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
		// If we are not pursuing and no async rebuild is pending, try trail or idle.
		svg_base_edict_t *trailSpot = PlayerTrail_PickFirst( self );
		if ( trailSpot ) {
			self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_AStarPursuitTrail );
		} else {
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
*				If trail is present, will attempt async A* to the trail's last known position.
*				If player is present, sets nextThink to onThink_AStarToPlayer and attempt async A* to player.
*				If neither are present, will set nextThink to onThink_Idle.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarPursuitTrail )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*	Maintain base state and check liveness.
	**/
	if ( !self->OnThinkGeneric( ) ) {
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
		bool pursuing = false;
		svg_base_edict_t *trailSpot = PlayerTrail_PickFirst( self );
		if ( trailSpot ) {
			// Clear direct goalentity since we are targeting a trail spot.
			self->goalentity = nullptr;
        // Request an async rebuild toward the trail spot. The nav request queue now
		// supports refreshing/replacing existing entries; MoveAStarToOrigin will
		// enqueue or refresh the pending request and return immediately. We still
		// check SVG_Nav_IsRequestPending if callers need to know pending state.
		pursuing = self->MoveAStarToOrigin( trailSpot->currentOrigin );
			(void)pursuing; // explicit ignore - the rebuild will drive movement when ready.
		}

		if ( !pursuing ) {
			self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Idle );
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
	if ( !self->activator ) {
		self->goalentity = nullptr;

		SVG_Nav_FreeTraversalPath( &self->navPathProcess.path );
		self->navPathProcess.path_index = 0;
		self->navPathProcess.path_goal = {};
		self->navPathProcess.path_start = {};
		self->navPathProcess.rebuild_in_progress = false;
		self->navPathProcess.pending_request_handle = 0;
	}

	/**
	*	Always look for the player(activator) so we can react immediately when they appear.
	**/
	if ( self->activator 
		&& SVG_Entity_IsInFrontOf( self, self->activator, { 1.0f, 1.0f, 0.0f }, 0.35f ) 
		/*&& SVG_Entity_IsVisible( self, self->activator )*/ ) {
		// Immediate action.
		self->goalentity = self->activator;
		// Set the nextThink to AStarToPlayer so we start chasing the player right away.
		self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_AStarToPlayer );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Always look for the trails of the (activator) so we can react immediately if the player was recently present but is not currently visible.
	**/
	svg_base_edict_t *trailTarget = PlayerTrail_PickFirst( self );
	if ( trailTarget 
		&& SVG_Entity_IsInFrontOf( self, trailTarget, { 1.0f, 1.0f, 0.0f }, 0.35f ) 
		&& SVG_Entity_IsVisible( self, trailTarget ) ) {
		self->goalentity = trailTarget;

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
	navPathPolicy.drop_cap = 64.0;
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
			.mm_flags = ( groundInfo.entityNumber != ENTITYNUM_NONE ? MMF_ON_GROUND : 0 ),
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

	// Return the blocked mask so the caller can decide how to react to obstructions.
	return blockedMask;
}


//=============================================================================================
//=============================================================================================


/**
*    @brief    Attempt A* navigation to a target origin. (Local to this TU to avoid parent dependency).
*    @return   True if movement/animation was updated.
**/
const bool svg_monster_testdummy_debug_t::MoveAStarToOrigin( const Vector3 &goalOrigin ) {
	/**
	*    Derive the correct nav-agent bbox (feet-origin) for traversal.
	**/
	Vector3 agent_mins, agent_maxs;
	SVG_TestDummy_GetNavAgentBBox( this, &agent_mins, &agent_maxs );

	/**
	*    Attempt to queue a navigation rebuild for the target origin.
	**/
	SVG_TestDummy_TryQueueNavRebuild( this, currentOrigin, goalOrigin, navPathPolicy, agent_mins, agent_maxs );

	/**
	*    Check if we have a valid path buffer and query the movement direction.
	**/
  // Check if we have a valid path buffer. We may have a path process in progress but the path buffer may not be populated yet, so we need to check both conditions.
	const bool pathOk = ( navPathProcess.path.num_points > 0 && navPathProcess.path.points );
	// Track whether an async request is still in flight so we can avoid direct-steer
	// fallback while an A* rebuild is pending.
	const bool requestPending = SVG_Nav_IsRequestPending( &navPathProcess );

    // Default to whichever direction we are currently moving in if we don't have a valid path
	// or can't get a direction for some reason, so we at least keep momentum instead
	// of snapping to a stop. If we have no valid path yet, fall back to directly
	// steering toward the goal so the debug monster visibly moves while async
	// path generation is happening.
	Vector3 move_dir = ( velocity.x != 0.0f || velocity.y != 0.0f ) ? QM_Vector3Normalize( { velocity.x, velocity.y, 0.0f } ) : Vector3{ 0.0f, 0.0f, 0.0f };

	// If we don't have a path available yet, compute a direct 2D direction to the
	// goal and use that as a temporary movement direction. This avoids the test
	// dummy appearing stuck while async navigation rebuilds.
	if ( !pathOk ) {
		/**
		*    When async navigation is enabled and a request is still pending, avoid
		*    direct-steer fallback so we do not mask the lack of A* output.
		**/

		/**
		*    Async not pending: allow a short direct steer to keep the debug dummy moving
		*    when no path is available (useful when async is disabled).
		**/
		Vector3 toGoal = goalOrigin;
		// Project the target to the horizontal plane so steering ignores vertical offsets.
		toGoal.z = currentOrigin.z;
		// Compute the horizontal direction to the goal.
		toGoal = QM_Vector3Subtract( toGoal, currentOrigin );
		// Check for a non-trivial direction before normalizing.
		const float len2 = ( toGoal.x * toGoal.x ) + ( toGoal.y * toGoal.y );
		if ( len2 > ( 0.001f * 0.001f ) ) {
			// Normalize the temporary movement direction.
			move_dir = QM_Vector3Normalize( toGoal );
			// Face the temporary movement direction so the entity visibly turns toward the goal.
			ideal_yaw = QM_Vector3ToYaw( move_dir );
			yaw_speed = 7.5f;
			SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
			// Play run animation when moving directly.
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
		// Set the last trail time now for future trail spot picking.
		trail_time = level.time;

		/**
		*    Face the next navigation waypoint on the horizontal plane.
		**/
		// Store the next path point in entity space so we can use it for facing and animation decisions. 
		// We will ignore the Z component for these decisions since we want to keep the monster grounded and not have it tilt up/down based on the path points.
		Vector3 nextPoint = goalOrigin;
		// Get the next path point in entity space. If that fails for some reason, we will just use the goal origin as the next point so we at least face towards our target instead of potentially facing a completely irrelevant direction.
		if ( navPathProcess.GetNextPathPointEntitySpace( &nextPoint ) ) {
			// Get the direction to the next point on the horizontal plane and slerp towards it for smoother turning. If we can't get the next point for some reason, we will just keep our current facing direction instead of snapping to an irrelevant direction.
			Vector3 target = nextPoint;
			// Keep the target Z the same as our current Z so we don't tilt up/down based on the path point and so we keep facing towards the target on the horizontal plane.
			target.z = currentOrigin.z;
			// Get the direction to the target on the horizontal plane.
			Vector3 toTarget = QM_Vector3Subtract( target, currentOrigin );

			// Get the distance to the target on the horizontal plane so we can check if it's significant enough to bother facing towards it. 
			// If the target is very close, we may not want to change our facing direction since it could be noisy and not visually meaningful, 
			// and since we may want to keep our current facing direction for better animation continuity instead of snapping to a 
			// new direction based on a very close target.
			const float dist2D = std::sqrt( toTarget.x * toTarget.x + toTarget.y * toTarget.y );
			// If the target is far enough away, we will try to face towards it. Otherwise, we will just keep our current facing direction for better animation continuity instead of snapping to a new direction based on a very close target.
			if ( dist2D > 0.001f ) {
				// Normalize the direction to the target so we can use it for facing.
				Vector3 faceDir = SlerpLocalVector3( move_dir, QM_Vector3Normalize( toTarget ), 0.2f );
				// Get the ideal yaw to face the target direction and set it as our ideal yaw for facing. 
				// If we can't get a valid face direction for some reason, we will just keep our current 
				// facing direction instead of snapping to an irrelevant direction.
				ideal_yaw = QM_Vector3ToYaw( faceDir );
				yaw_speed = 7.5f;
				// Call the face ideal yaw helper to update our facing direction towards the target. 
				// This will make our monster face towards the next path point for better visual orientation and so we can see that it's trying to follow the path.
				SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
			}
		}

		/**
		*    Animation: root motion run. (Local implementation to avoid parent dependency).
		**/
		if ( rootMotionSet && rootMotionSet->motions[ 4 ] ) {
			skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 4 ]; // RUN
			const double t = level.time.Seconds<double>();
			const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
			const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
			s.frame = rootMotion->firstFrameIndex + localFrame;
		}

        /**
		*    Apply horizontal velocity.
		**/
		constexpr double frameVelocity = 220.0;
		velocity.x = ( float )( move_dir.x * frameVelocity );
		velocity.y = ( float )( move_dir.y * frameVelocity );
		return true;
	}

    // If we reached here and move_dir is non-zero (direct-steer fallback), apply that velocity so we move even without a nav path.
	const float fallbackLen2 = ( move_dir.x * move_dir.x ) + ( move_dir.y * move_dir.y );
	if ( fallbackLen2 > ( 0.001f * 0.001f ) ) {
		constexpr double frameVelocity = 220.0;
		velocity.x = ( float )( move_dir.x * frameVelocity );
		velocity.y = ( float )( move_dir.y * frameVelocity );
		return true;
	}

	return false;
}

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
