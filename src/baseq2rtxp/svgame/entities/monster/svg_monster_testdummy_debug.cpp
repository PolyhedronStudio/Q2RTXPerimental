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

#include "svgame/nav/svg_nav.h"
// Navigation cluster routing (coarse tile routing pre-pass).
#include "svgame/nav/svg_nav_clusters.h"
// Async navigation queue helpers.
#include "svgame/nav/svg_nav_request.h"
// Traversal helpers required for path invalidation.
#include "svgame/nav/svg_nav_traversal.h"

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"


//! Optional debug toggle for emitting async queue statistics.
extern cvar_t *s_nav_nav_async_log_stats;

// Local debug toggle for noisy per-frame prints in this test monster.
#ifndef DEBUG_PRINTS
#define DEBUG_PRINTS 1
#endif

     /**
*    Debug compile-time toggle for route-filter isolation.
*        When enabled, this debug monster disables the coarse cluster tile-route
*        restriction so A* neighbor diagnostics reflect pure StepTest behavior.
**/
#ifndef MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER
//#define MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER 1
#endif

#ifdef DEBUG_PRINTS
static constexpr int32_t DUMMY_NAV_DEBUG = 1;
#else
static constexpr int32_t DUMMY_NAV_DEBUG = 0;
#endif

// Debug constants for tuning the test dummy's behavior and debug output.
#define DUMMY_SEEK_IMPACT_SOUND_ONLY 1

//! Maximum horizontal distance allowed for direct player pursuit before falling back.
static constexpr double DUMMY_PLAYER_PURSUIT_MAX_DIST = 2048.0;
//! Maximum allowed breadcrumb age before trail-follow is considered stale.
static constexpr QMTime DUMMY_TRAIL_MAX_AGE = 6_sec;
//! Maximum time after LOS loss where breadcrumb fallback is still allowed.
static constexpr QMTime DUMMY_BREADCRUMB_LOS_FALLBACK_WINDOW = 2_sec;
//! Maximum age of a sound event that can trigger investigation.
static constexpr QMTime DUMMY_SOUND_INVESTIGATE_MAX_AGE = 1200_ms;
//! Maximum horizontal distance for reacting to sound events.
static constexpr double DUMMY_SOUND_INVESTIGATE_MAX_DIST = 1536.0;
//! Arrival radius used for ending sound investigation behavior.
static constexpr double DUMMY_SOUND_INVESTIGATE_REACHED_DIST = 4.0;
//! Idle yaw scan step in degrees per think.
static constexpr double DUMMY_IDLE_SCAN_STEP_DEG = 45.0;
//! Interval for flipping idle scan yaw direction.
static constexpr QMTime DUMMY_IDLE_SCAN_FLIP_INTERVAL = 500_ms;



/**
*
*
*
*
*	Debugging Routines:
*
*
*
*
**/
/**
*   @brief	Return a readable name for a debug AI state.
**/
static inline const char *Dummy_DebugAIStateName( const svg_monster_testdummy_debug_t::AIThinkState state ) {
	switch ( state ) {
		case svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer:
			return "PursuePlayer";
		case svg_monster_testdummy_debug_t::AIThinkState::PursueBreadcrumb:
			return "PursueBreadcrumb";
		case svg_monster_testdummy_debug_t::AIThinkState::InvestigateSound:
			return "InvestigateSound";
		case svg_monster_testdummy_debug_t::AIThinkState::IdleLookout:
		default:
			return "IdleLookout";
	}
}

/**
*   @brief	Emit a compact per-think state + gate input line for fast diagnosis.
*   @note	Only called when `DUMMY_NAV_DEBUG` is enabled.
**/
static inline void Dummy_DebugLogStateGateInputs( svg_monster_testdummy_debug_t *self ) {
	/**
	*   Sanity check: require valid entity pointer before reading state.
	**/
	if ( !self ) {
		return;
	}

	// Early out if not active to reduce noise and avoid invalid state reads (e.g. activator).
	if ( !self->isActivated ) {
		return;
	}

	/**
	*   Compute lightweight gate inputs used by state handlers.
	**/
	const bool hasActivator = ( self->activator != nullptr );
	const bool activatorVisible = hasActivator ? SVG_Entity_IsVisible( self, self->activator ) : false;
	const double activatorDist2D = hasActivator
		? std::sqrt( QM_Vector2DistanceSqr( self->activator->currentOrigin, self->currentOrigin ) )
		: -1.0;
	const bool requestPending = SVG_Nav_IsRequestPending( &self->pathNavigationState.process );

	/**
	*   Emit a compact, single-line state snapshot for this think tick.
	**/
	gi.dprintf( "[NAV DEBUG][ThinkGate] ent=%d state=%s active=%d has_act=%d vis=%d dist2d=%.1f has_trail=%d has_sound=%d pending=%d handle=%d rebuild=%d goal=%d path_pts=%d path_idx=%d\n",
		self->s.number,
		Dummy_DebugAIStateName( self->thinkAIState ),
		self->isActivated ? 1 : 0,
		hasActivator ? 1 : 0,
		activatorVisible ? 1 : 0,
		activatorDist2D,
		self->stateNavigationTrail.targetEntity ? 1 : 0,
		self->stateSoundCan.hasOrigin ? 1 : 0,
		requestPending ? 1 : 0,
		self->pathNavigationState.process.pending_request_handle,
		self->pathNavigationState.process.rebuild_in_progress ? 1 : 0,
		self->goalentity ? 1 : 0,
		self->pathNavigationState.process.path.num_points,
		self->pathNavigationState.process.path_index );
}

//#define Com_Printf(...) Com_LPrintf(PRINT_ALL, __VA_ARGS__)
//=============================================================================================
//=============================================================================================




//=============================================================================================
//=============================================================================================




/**
*   @brief	Set explicit debug state.
**/
static inline void Dummy_SetState( svg_monster_testdummy_debug_t *self, const svg_monster_testdummy_debug_t::AIThinkState newState ) {
	// Sanity: require valid entity.
	if ( !self ) {
		return;
	}

	/**
	*   Emit deterministic transition logs only when the state actually changes.
	**/
	const svg_monster_testdummy_debug_t::AIThinkState oldState = self->thinkAIState;
	if ( oldState != newState && DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "[NAV DEBUG][StateTransition] ent=%d %s -> %s\n",
			self->s.number,
			Dummy_DebugAIStateName( oldState ),
			Dummy_DebugAIStateName( newState ) );
	}

	// Assign the next deterministic debug state.
	self->thinkAIState = newState;
}

/**
*   @brief	Central state dispatcher for debug AI states.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink )( svg_monster_testdummy_debug_t *self ) -> void {
	// Sanity: require valid entity pointer.
	if ( !self ) {
		return;
	}

	/**
	*   Emit one compact per-think snapshot to correlate state transitions with
	*   gating inputs while diagnosing stalls.
	**/
	if ( DUMMY_NAV_DEBUG != 0 ) {
		Dummy_DebugLogStateGateInputs( self );
	}

	/**
	*   Dispatch to the active explicit state behavior. (Simple 'state machine'.)
	*   Each state handler enforces its own activation/transition guards so this dispatcher stays stateless.
	**/
	switch ( self->thinkAIState ) {
		case svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer:
			svg_monster_testdummy_debug_t::onThink_AStarToPlayer( self );
			break;
		case svg_monster_testdummy_debug_t::AIThinkState::PursueBreadcrumb:
			svg_monster_testdummy_debug_t::onThink_AStarPursuitTrail( self );
			break;
		case svg_monster_testdummy_debug_t::AIThinkState::InvestigateSound:
			svg_monster_testdummy_debug_t::onThink_InvestigateSound( self );
			break;
		case svg_monster_testdummy_debug_t::AIThinkState::IdleLookout:
		default:
			svg_monster_testdummy_debug_t::onThink_Idle( self );
			break;
	}
}


/**
*	@brief	For this debug variant, we override the spawn and think callbacks to always attempt async A* to the activator.
*			Spawn for debug testdummy: call base onSpawn then set think to our simple loop.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn )( svg_monster_testdummy_debug_t *self ) -> void {
	Super::onSpawn( self );

	/**
	*    Basic entity type and movement properties.
	**/
	self->s.entityType = ET_MONSTER;

	self->solid = SOLID_BOUNDS_BOX;
	self->movetype = MOVETYPE_WALK;


	/**
	*    Load model and cache root-motion set for animations.
	**/
	self->model = svg_level_qstring_t::from_char_str( "models/characters/mixadummy/tris.iqm" );
	self->s.modelindex = gi.modelindex( self->model );
	const char *modelname = self->model;
	const model_t *model_forname = gi.GetModelDataForName( modelname );
	self->rootMotionSet = &model_forname->skmConfig->rootMotion;


	/**
	*    Collision bbox and physics defaults.
	**/
	VectorCopy( svg_monster_testdummy_debug_t::DUMMY_BBOX_STANDUP_MINS, self->mins );
	VectorCopy( svg_monster_testdummy_debug_t::DUMMY_BBOX_STANDUP_MAXS, self->maxs );
	// Very important to set in order for its AI navigation to work properly.
	self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;
	// Important to set for physics interactions.
	self->mass = 200;


	/**
	*    Default attribute fallbacks for spawned entity.
	**/
	if ( !self->health ) {
		self->health = 200;
	}
	if ( !self->dmg ) {
		self->dmg = 150;
	}
	if ( !self->gravity ) {
		self->gravity = 1.0f;
	}


	/**
	*    Entity flags and render properties.
	**/
	self->svFlags &= ~SVF_DEADENTITY;
	self->svFlags |= SVF_MONSTER;

	self->s.skinnum = 0;

	self->takedamage = DAMAGE_AIM;

	self->airFinishedBreathTime = level.time + 12_sec;
	self->max_health = self->health;

	self->clipMask = CM_CONTENTMASK_MONSTERSOLID;

	self->takedamage = DAMAGE_YES;
	self->lifeStatus = LIFESTATUS_ALIVE;


	/**
	*    Interaction hooks and think scheduling.
	**/
	self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;

	self->nextthink = level.time + 20_hz;
	self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink );

	/**
	*	Callback Hooks:
	**/
	self->SetDieCallback( &svg_monster_testdummy_debug_t::onDie );
	self->SetPainCallback( &svg_monster_testdummy_debug_t::onPain );
	self->SetPostSpawnCallback( &svg_monster_testdummy_debug_t::onPostSpawn );
	self->SetTouchCallback( &svg_monster_testdummy_debug_t::onTouch );
	self->SetUseCallback( &svg_monster_testdummy_debug_t::onUse );

	// Ensure we have a valid use target hint for engaging with the player, which may be used by the player to determine whether they can interact with this entity and may also be used by our own AI code for gating certain behaviors that require player interaction.
	SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_ENGAGE );

	// Link the entity so it's active in the world and can be interacted with.
	gi.linkentity( self );

	// Set our monster flag so we can be detected as a monster by any relevant code (e.g. cluster routing).
	self->svFlags |= SVF_MONSTER;
	self->isActivated = false;

	/**
	*	Sound and Player Investigation Properties:
	**/
	// We will use the activator field to track our player target for simplicity, 
	// but we need to make sure to clear it on spawn since the base spawn may have 
	// set it to a non-null value if we were triggered by something.
	self->stateSoundCan.hasOrigin = false;
	// We will use the stateNavigationTrail.targetEntity field to track our breadcrumb trail target for simplicity, 
	// but we need to make sure to clear it on spawn since we may have a stale value from a previous life if we are respawning.
	self->stateSoundCan.lastTime = 0_ms;
	self->lastPlayerVisibleTime = 0_ms;

	/**
	*	Idle Scan Properties:
	**/
	// Initialize idle scan state.
	self->stateIdleScan.yawScanDirection = 1.0f;
	// We will use stateIdleScan.nextFlipTime to track when we should flip our idle scan direction.
	self->stateIdleScan.nextFlipTime = level.time + DUMMY_IDLE_SCAN_FLIP_INTERVAL;
 // Start at heading index 0 (0 degrees).
	self->stateIdleScan.headingIndex = 0;
	// Initialize explicit debug state machine.
	Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );

	/**
	*	Setup the initial monsterMove state.
	**/
	// First recategorize our current position in terms of ground and liquid so we have accurate state for the monster move code to work with right away.
	self->RecategorizeGroundAndLiquidState();

	// Apply the monster move properties so we can use the monster move code for all of our movement and collision handling
	// including during pathfinding pursuit.
	self->monsterMove = {
			.monster = self,
			.frameTime = gi.frame_time_s,
			.mins = self->mins,
			.maxs = self->maxs,
			.state = {
			.mm_type = MM_NORMAL,
			// Ensure mm_flags uses the expected 16-bit storage without narrowing warnings.
			.mm_flags = static_cast<uint16_t>( self->groundInfo.entityNumber != ENTITYNUM_NONE ? MMF_ON_GROUND : MMF_NONE ),
				.mm_time = 0,
				.gravity = ( int16_t )( self->gravity * sv_gravity->value ),
				.origin = self->currentOrigin,
				.velocity = self->velocity,
				.previousOrigin = self->currentOrigin,
				.previousVelocity = self->velocity,
			},
			.ground = self->groundInfo,
			.liquid = self->liquidInfo,
	};

	/**
	*	Finish by setting neccessary callbacks and initial think time for our main thinker loop, 
	//	which will handle all the debug AI states and transitions between them. 
	//	We do this after initializing all properties to ensure that our thinker has a consistent starting state when it first runs.
	**/
	// Set use callback so we can be activated by the player.
	self->SetUseCallback( &svg_monster_testdummy_debug_t::onUse );
    // Always run our central state dispatcher thinker.
	self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink );
    self->nextthink = level.time + FRAME_TIME_MS;

	// Clear any pending async navigation state so we start clean when spawned/activated.
	self->ResetNavigationPath( );
}

/**
*   @brief  Post-Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_monster_testdummy_debug_t, onPostSpawn )( svg_monster_testdummy_debug_t *self ) -> void {
	// Make sure to fall to floor.
	if ( !self->activator ) {
		const cm_contents_t mask = SVG_GetClipMask( self );
		M_CheckGround( self, mask );
		M_droptofloor( self );


	}
}

/**
*   @brief  Touched.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_monster_testdummy_debug_t, onTouch )( svg_monster_testdummy_debug_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	gi.dprintf( "onTouch\n" );
}

/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_debug_t, onDie )( svg_monster_testdummy_debug_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
	if ( ( self->lifeStatus & LIFESTATUS_DEAD ) == LIFESTATUS_DEAD ) {
		return;
	}

	if ( ( self->lifeStatus & LIFESTATUS_DYING ) == LIFESTATUS_DYING ) {
		// Gib Death:
		if ( self->health < GIB_DEATH_HEALTH ) {
			// Play gib sound.
			//gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
			SVG_EntityEvent_GeneralSoundEx( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), ATTN_NORM );
			//! Throw 4 small meat gibs around.
			for ( int32_t n = 0; n < 4; n++ ) {
				SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
			}
			// Turn ourself into the thrown head entity.
			SVG_Misc_ThrowHead( self, "models/objects/gibs/head2/tris.md2", damage, GIB_TYPE_ORGANIC );

			// Gibs don't take damage, but fade away as time passes.
			self->takedamage = DAMAGE_NO;
			// Set lifeStatus.
			self->lifeStatus = LIFESTATUS_DEAD;
		}
	}
	// Set activator.
	self->activator = attacker;

	//---------------------------
	// <TEMPORARY FOR TESTING>
	//---------------------------
	if ( ( self->lifeStatus & LIFESTATUS_ALIVE ) == LIFESTATUS_ALIVE ) {
		// Pick a random death animation.
		int32_t deathanim = irandom( 3 );
		if ( deathanim == 0 ) {
			self->s.frame = 512;
		} else if ( deathanim == 1 ) {
			self->s.frame = 642;
		} else {
			self->s.frame = 801;
		}

		self->lifeStatus = LIFESTATUS_DYING;
		// Set this here so the entity does not block traces while playing death animation.
		self->svFlags |= SVF_DEADENTITY;
	} else if ( self->s.frame == 643 ) {
		// Monster Corpse Entity Type:
		self->s.entityType = ET_MONSTER_CORPSE;
	} else if ( self->s.frame == 800 ) {
		// Monster Corpse Entity Type:
		self->s.entityType = ET_MONSTER_CORPSE;
	} else if ( self->s.frame == 937 ) {
		// Monster Corpse Entity Type:
		self->s.entityType = ET_MONSTER_CORPSE;
	}
	//---------------------------
	// </TEMPORARY FOR TESTING>
	//---------------------------
	// Stop playing any sounds.
	self->s.sound = 0;
	// Setup the death bounding box.
	self->mins = DUMMY_BBOX_DEAD_MINS;
	self->maxs = DUMMY_BBOX_DEAD_MAXS;

	// Make sure to relink.
	gi.linkentity( self );
}

/**
*	@brief	Handle player "use" interactions and toggle activation state.
*	@param	self	This debug testdummy instance.
*	@param	other	The entity that sent the use event (usually the trigger or world).
*	@param	activator	The entity that activated the use (usually the player/client).
*	@param	useType	Type of use action (toggle, press, etc.).
*	@param	useValue	Value associated with the use action (e.g. 1 for on).
*	@note	This function centralizes follow/unfollow toggling and resets navigation
*			state when activation changes. Only a single assignment to `isActivated`
*			is performed later to keep activation semantics deterministic.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_monster_testdummy_debug_t, onUse )( svg_monster_testdummy_debug_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	// Apply activator.
	self->activator = activator;
	self->other = other;

	// "Toggle" between follow/unfollow.
	// Cheap hack.
	if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE ) {
		if ( useValue == 1 ) {
			if ( activator && activator->client ) {
				self->goalentity = activator;

				// Get the root motion.
				skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 4 ]; // [1] == RUN_FORWARD_PISTOL
				// Transition to its animation.
				self->s.frame = rootMotion->firstFrameIndex;

				// Set to disengagement mode usehint. (Yes this is a cheap hack., it is not client specific.)
				SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_DISENGAGE );
				// Fall through: apply activation below so the toggle branch does not
				// early-return and leave `isActivated`/state inconsistent with the
				// visual/activator changes we just applied.
			}
		}
	}

	// Reset to engagement mode usehint. (Yes this is a cheap hack., it is not client specific.)
	SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_ENGAGE );

	//self->goalentity = nullptr;
	//self->activator = nullptr;
	//self->other = nullptr;

	// Fire set target.
	SVG_UseTargets( self, activator );

	// First, determine whether we are activating or deactivating based on the useType and useValue.
	const bool activating = ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE
		&& useValue == 1
		&& activator
		&& activator->client );

	// Set the activation state based on the use action.
	self->isActivated = activating;
	// When we toggle activation, we want to reset all of our state so that we can start fresh when we reactivate, 
	// and so that we do not keep pursuing stale targets when deactivated.
	self->stateSoundCan.hasOrigin = false;
	// Set the investigate sound origin to our current position so that if we do get a sound event while deactivated, we have a valid origin to investigate instead of random/uninitialized memory. 
	// This also means that if we get a sound event while deactivated, we will just investigate it right where we are instead of trying to move toward it, 
	// which is a reasonable fallback behavior.
	self->stateSoundCan.origin = self->currentOrigin;
	// Reset idle scan state so that when we toggle activation, we start with a consistent idle sweep behavior.
	self->stateIdleScan.yawScanDirection = 1.0f;
	// Start from a defined heading index and schedule the first flip.
	self->stateIdleScan.headingIndex = 0;
	// Set the next flip time to now + interval so that when we toggle activation, we start with a consistent idle sweep behavior.
	self->stateIdleScan.nextFlipTime = level.time + DUMMY_IDLE_SCAN_FLIP_INTERVAL;
	// Clear any cached breadcrumb/goal so we do not keep pursuing after deactivating.
	if ( !self->isActivated ) {
		// When deactivating, also clear the last processed sound time so that we can react to sound events immediately if we get any while deactivated, instead of ignoring them because they are older than the last processed time.
		self->stateSoundCan.lastTime = 0_ms;
		// When deactivating, also clear the last player visible time so that we can react to player presence immediately if we toggle back on, instead of ignoring it because it is older than the last visible time.
		self->lastPlayerVisibleTime = 0_ms;
	}

	/**
	*	Activation state change handling:
	*		- When disabling, stop any pursuit immediately and clear nav/trail state.
	*		- When enabling, start from idle so we acquire player/trail cleanly.
	**/
	if ( !self->isActivated ) {
		// Clear any cached breadcrumb/goal so we do not keep pursuing after disabling.
		self->stateNavigationTrail.targetEntity = nullptr;
		self->goalentity = nullptr;
		// Cancel/clear any async nav request/path so it cannot keep steering motion.
		self->ResetNavigationPath();
		// Return to idle thinker. (So it can scan for a player/trail goal.)
		self->nextthink = level.time + FRAME_TIME_MS;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
	} else {
		// On activation, reset nav state and reacquire targets from scratch.
		self->stateNavigationTrail.targetEntity = nullptr;
		self->goalentity = nullptr;
		// Cancel/clear any async nav request/path so it cannot keep steering motion.
		self->ResetNavigationPath();
		// Return to idle thinker. (So it can scan for a player/trail goal.)
		self->nextthink = level.time + FRAME_TIME_MS;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
	}

	if ( DUMMY_NAV_DEBUG != 0 ) {
		const char *activatorName = "nullptr";
		if ( activator ) {
			activatorName = ( const char * )activator->classname;
		}

		gi.dprintf( "[NAV DEBUG] %s: isActivated=%d, activator=%s\n",
			__func__, ( int32_t )self->isActivated, activatorName );
	}

	//self->stateNavigationTrail.targetEntity = nullptr;
	//ResetNavigationPath( );

	if ( self->isActivated ) {
		self->goalentity = activator;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
	} else {
		self->goalentity = nullptr;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
	}

	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_debug_t, onPain )( svg_monster_testdummy_debug_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {

}


//=================================================================================================
//=================================================================================================


/**
*
*
*
*		Entity 'onThink' State Routines:
*			- Each of these is set as the nextThink for the onThink callback,
*			and thus determines the behavior of the next think frame.
*
*
*
**/
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
	if ( !self->GenericThinkBegin() ) {
		return;
	}

	#if 0
	/* Tune goal-Z blending for this think tick. */
	{
		Vector3 likelyGoal = self->activator ? self->activator->currentOrigin : self->currentOrigin;
		self->AdjustGoalZBlendPolicy( likelyGoal );
	}

	/* Tune goal-Z blending for this think tick. */
	{
		Vector3 likelyGoal = self->stateNavigationTrail.targetEntity ? self->stateNavigationTrail.targetEntity->currentOrigin : ( self->activator ? self->activator->currentOrigin : self->currentOrigin );
		self->AdjustGoalZBlendPolicy( likelyGoal );
	}

	/*
	* Tune goal-Z blending based on current likely goal so nav policy is
	* appropriate before any path rebuilds are queued this frame.
	*/
	{
		Vector3 likelyGoal = self->stateNavigationTrail.targetEntity ? self->stateNavigationTrail.targetEntity->currentOrigin : ( self->activator ? self->activator->currentOrigin : self->currentOrigin );
		self->AdjustGoalZBlendPolicy( likelyGoal );
	}

	/**
	*\tTune goal-Z blending based on current likely goal so nav policy is
	*\tappropriate before any path rebuilds are queued this frame.
	**/
	{
		Vector3 likelyGoal = self->activator ? self->activator->currentOrigin : ( self->stateNavigationTrail.targetEntity ? self->stateNavigationTrail.targetEntity->currentOrigin : self->currentOrigin );
		self->AdjustGoalZBlendPolicy( likelyGoal );
	}

	/**
	*	Tune goal-Z blending based on current likely goal so nav policy is
	*	appropriate before any path rebuilds are queued this frame.
	**/
	{
		Vector3 likelyGoal = self->activator ? self->activator->currentOrigin : self->currentOrigin;
		self->AdjustGoalZBlendPolicy( likelyGoal );
	}
	#else
	{
		Vector3 likelyGoal = self->activator ? self->activator->currentOrigin : self->currentOrigin;
		self->DetermineGoalZBlendPolicyState( likelyGoal );
	}
	#endif

	if ( DUMMY_NAV_DEBUG != 0 && self->isActivated ) {
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

	// If we are not activated, we should not be pursuing anything. Transition to idle until we are activated.
	if ( !self->isActivated ) {
		// Clear any pending path state so we do not try to pursue a stale target if we later get activated.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		// Set nextthink to now so we immediately process the transition on the next frame.
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Sanity: if we have no valid activator (player) any more, stop trail pursuit.
	*		Breadcrumbs are only meaningful relative to a known player target.
	**/
	if ( !self->activator ) {
		// Clear any pending path state so we do not try to pursue a stale target if we later get a new activator.
		self->stateNavigationTrail.targetEntity = nullptr;
		// Clear the goalentity so we do not accidentally pursue a stale target if we later get a new activator.
		self->goalentity = nullptr;
		// Reset nav state so we start fresh when/if we later get a new activator.
		self->ResetNavigationPath( );
		// Transition to idle since we have no target to pursue.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		// Set nextthink to now so we immediately process the transition on the next frame.
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*   Player pursuit gate:
	*       Direct pursuit requires both LOS and a reasonable horizontal distance.
	*       If either condition fails, trail/idle handling below takes over.
	**/
	// Store whether the activator is currently visible and its horizontal distance so we can gate direct pursuit and also for debug printing below.
	const bool activatorVisible = SVG_Entity_IsVisible( self, self->activator );
    if ( activatorVisible ) {
		// Keep a LOS timestamp so breadcrumb fallback can be constrained to recent loss events.
		self->lastPlayerVisibleTime = level.time;
	}
    // Note: we use horizontal distance for the pursuit gate to allow for some verticality without completely breaking pursuit, 
	// but this also means that in cases where the activator is directly above or below the test dummy, the pursuit gate will not function as intended and may allow pursuit at greater actual distances. This is a tradeoff to allow for more forgiving pursuit in typical cases while still providing a hard cutoff for very long-distance pursuits that are unlikely to succeed.
	const double activatorDist2D = std::sqrt( QM_Vector2DistanceSqr( self->activator->currentOrigin, self->currentOrigin ) );
	// Breadcrumb fallback rule: direct player pursuit only while LOS is valid.
	// On LOS loss, this state must hand off to breadcrumb/idle logic below.
	// If the activator is not visible or is beyond the defined pursuit distance, we will not attempt direct pursuit 
	// and will instead fall back to trail-following or idle behavior as appropriate below. 
	// This ensures that we do not attempt to use A* to pursue an activator that is out of sight or too far away, which would likely fail and could lead to undesirable behavior such as getting stuck or behaving erratically while trying to reach an unreachable target.
	if ( !activatorVisible && activatorDist2D > DUMMY_PLAYER_PURSUIT_MAX_DIST ) {
		if ( DUMMY_NAV_DEBUG != 0 ) {
			gi.dprintf( "[NAV DEBUG] %s: direct-pursuit gate blocked visible=%d dist2D=%.1f max=%.1f\n",
			__func__, activatorVisible ? 1 : 0, activatorDist2D, DUMMY_PLAYER_PURSUIT_MAX_DIST );
		}
	}

	/**
	*    Exclusively use A* to target the activator.
	**/
 // Maintain direct pursuit only with current LOS.
	const bool pursuePlayerGate = activatorVisible;
	bool pursuing = false;
	// Only attempt to pursue if we have a valid activator that is within the direct pursuit gate conditions. 
	// If the activator is out of sight or too far away, we will fall back to trail-following or idle behavior below,
	// so we do not want to attempt A* towards the activator in those cases.
    if ( self->activator && pursuePlayerGate ) {
		if ( self->goalentity != self->activator ) {
			// If we just switched to a new activator goal, reset any pending path state
			// so we do not reuse stale async requests/path buffers.
			self->goalentity = self->activator;
			// Clear any breadcrumb target so trail-follow state does not fight direct pursuit.
			self->stateNavigationTrail.targetEntity = nullptr;
			// Reset nav state so we start fresh with the new goal.
			self->ResetNavigationPath( );

			// While directly targeting the activator, ignore breadcrumb trail spots
			// by marking the trail_time to now. This mirrors direct-pursuit behavior
			// used in the puppet testdummy so PickFirst will prefer newer trail
			// entries once LOS is lost.
			self->stateNavigationTrail.trailTimeStamp = level.time;
		}

		// Always attempt to move to activator origin while in this state.
		pursuing = self->MoveAStarToOrigin( self->activator->currentOrigin );
	}

	/**
	*    Transition to trail-following if LOS is lost or A* fails.
	**/
	/**
	*    Transition-gate hardening:
	*        Normalize stale request markers before evaluating visibility transitions.
	*        This prevents stale handles from blocking the player->trail handoff.
	**/
	// Is there an active pending request according to the queue?
    const bool requestPending = SVG_Nav_IsRequestPending( &self->pathNavigationState.process );
	// If the queue reports no pending request, but we have a pending request handle or rebuild state, clear it to prevent stale state from interfering with our transition handling below. 
	// This can happen if the queue processes and completes a request, but for some reason the completion callback does not get called to clear our pending request handle (e.g. if the request was cancelled or if there was an error during processing). By checking the queue state first and only clearing if the queue reports no pending request, we avoid accidentally clearing truly active requests that are still in-flight during the transition.
	if ( !requestPending && ( self->pathNavigationState.process.pending_request_handle != 0 || self->pathNavigationState.process.rebuild_in_progress ) ) {
		/**
		*    Clear stale markers when queue reports no active request for this process.
		**/
		self->pathNavigationState.process.pending_request_handle = 0;
		// Clear any rebuild state so it does not interfere with our transition handling below.
		self->pathNavigationState.process.rebuild_in_progress = false;
	}

	/**
	*    If the activator went out of sight, transition to trail-following or idle.
	*    Pending activator requests are cancelled/reset during transition so stale
	*    async work cannot gate or re-steer this mode switch.
	**/
	if ( self->activator && !pursuePlayerGate ) {
		// Breadcrumb fallback is only valid for a short period after confirmed LOS loss.
		const bool withinLosFallbackWindow = ( self->lastPlayerVisibleTime > 0_ms )
			&& ( ( level.time - self->lastPlayerVisibleTime ) <= DUMMY_BREADCRUMB_LOS_FALLBACK_WINDOW );
		/**
		*    Activator just went out of sight:
		*        - Stop using the direct-pursuit goal.
		*        - Seed trail-follow with a stable first breadcrumb.
		*        - Clear any pending async request/path built for the activator so it
		*          cannot keep driving motion/orientation after we switch modes.
		**/
		/**
		*    Cancel any currently tracked pending request before mode switch.
		*    This guarantees the transition does not wait on stale handles.
		**/
		// Note: we check the queue state above and only cancel if the queue reports no pending request, 
		// so we do not cancel any truly active requests that are in-flight during the transition.
		// This allows us to safely transition even if the async request for the activator is still being processed, without worrying about canceling an active request that is already steering us towards the last known player position.
		if ( self->pathNavigationState.process.pending_request_handle != 0 ) {
			// Cancel any pending request for the player target since we are switching to a new trail-following target.
			// This also prevents any in-flight async work for the player target from coming back and interfering with 
			// our new trail-following target after we switch modes.
			SVG_Nav_CancelRequest( ( nav_request_handle_t )self->pathNavigationState.process.pending_request_handle );
			// Clear pending request state so it does not interfere with our new trail-following target.
			self->pathNavigationState.process.pending_request_handle = 0;
			// Clear any rebuild state so it does not interfere with our new trail-following target.
			self->pathNavigationState.process.rebuild_in_progress = false;
		}

		// Acquire the freshest breadcrumb entry to pursue.
        svg_base_edict_t *trailSpot = withinLosFallbackWindow ? PlayerTrail_PickFirst( self ) : nullptr;
		// Note: if the trail is completely stale, PickFirst will return nullptr and we will correctly transition 
		// to idle until a new breadcrumb is created by the player movement.
		if ( trailSpot && ( level.time - trailSpot->timestamp ) <= DUMMY_TRAIL_MAX_AGE ) {
			// Cache the breadcrumb so trail pursuit does not thrash between spots.
			self->stateNavigationTrail.targetEntity = trailSpot;
			// Mark the trail time to now so that PickFirst will prefer newer entries if we lose LOS again before we can pick a new trail spot on the next think. This also prevents us from picking stale trail spots that are no longer relevant to our current position.
			self->stateNavigationTrail.trailTimeStamp = trailSpot->timestamp;
			// Set the breadcrumb as our goal so A* will target it.
			self->goalentity = trailSpot;

			// Reset nav state so trail pursuit starts with a clean async request.
			self->ResetNavigationPath( );

			// Switch to breadcrumb pursuit.
			Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursueBreadcrumb );
		} else {
			// Nothing to pursue; go idle.
			self->stateNavigationTrail.targetEntity = nullptr;
			// Clear any goal so we do not attempt to pursue stale targets if we get LOS again before the next think.
			self->goalentity = nullptr;
			// Reset nav state so idle does not get blocked by stale async requests.
			self->ResetNavigationPath( );
			// Switch to idle.
			Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		}
	}

	/**
	*    Physics and collision.
	**/
	// For storing the results of the slide move.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	// Perform movement and capture any blocking results for recovery handling below.
	const bool moved = self->GenericThinkFinish( true, blockedMask );
	// Ensure the authoritative yaw/angles are applied to the entity state after movement
	// so rendering and networking see the updated orientation. Keep currentAngles authoritative.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	/**
	*    Obstruction recovery.
	**/
	// If we are blocked by an obstacle or trapped in a solid somehow corner,
	// trigger an immediate replan on the next think to attempt to recover.
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		// Clear any pending async request so it cannot interfere with our recovery attempt.
		self->pathNavigationState.process.next_rebuild_time = 0_ms;
	}
	// Set next think to continue pursuit or handle transitions.
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
	if ( !self->GenericThinkBegin( ) ) {
		return;
	}

	{
		Vector3 likelyGoal = self->activator ? self->activator->currentOrigin : self->currentOrigin;
		self->DetermineGoalZBlendPolicyState( likelyGoal );
	}

	if ( DUMMY_NAV_DEBUG != 0 && self->isActivated ) {
		gi.dprintf( "=============================== onThink_AStarPursuitTrail ===============================\n" );
		const char *trailName = "nullptr";
		if ( self->stateNavigationTrail.targetEntity ) {
			trailName = ( const char * )self->stateNavigationTrail.targetEntity->classname;
		}
		gi.dprintf( "[NAV DEBUG] %s: time=%.2f, stateNavigationTrail.targetEntity=%s\n", __func__, level.time.Seconds<double>(), trailName );
	}

	// If we are not activated, go idle. This can happen if we were pursuing the trail but the player came 
	// back into sight, so we switched to direct pursuit, but then the player disappeared again before we 
	// could switch back to trail pursuit, leaving us in limbo without a valid goal or trail target until 
	// we check this and go idle.
	if ( !self->isActivated ) {
		// SetSet next think to idle so we do not get stuck in this pursuit state without a valid goal.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    If the player becomes visible again, return to direct A* pursuit.
	**/
    // Compute 2D distance to activator for pursuit gating.
	const double activatorDist2D = std::sqrt( QM_Vector2DistanceSqr( self->activator ? self->activator->currentOrigin : self->currentOrigin, self->currentOrigin ) );
    // In breadcrumb mode, return to direct pursuit only on confirmed LOS.
    if ( self->activator && SVG_Entity_IsVisible( self, self->activator ) )
	{
		// Switch to direct pursuit.
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
		// Immediate attempt.
		self->goalentity = self->activator;
		// Reset pending async path requests when immediately switching to direct pursuit
		// to avoid stale async results interfering with the new goal.
		self->ResetNavigationPath( );
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
			// Cache the breadcrumb so we do not thrash between spots.
			self->goalentity = spot;
			// Trail time is used to gate the freshness of breadcrumbs we will consider for pursuit.
			self->stateNavigationTrail.targetEntity = spot;
			// When assigning a new trail target, we have the option to force an immediate rebuild or let the normal async debounce logic handle it.
			self->stateNavigationTrail.trailTimeStamp = spot->timestamp;
			// Reset nav state so pursuit starts with a clean async request for this new target. We can optionally force an immediate rebuild here, but we may want to delay it slightly if we are just reacquiring the same target to prevent flooding the async queue with rebuilds when breadcrumbs are frequently updated.
			if ( forceRebuild ) {
				// If we are forcing a rebuild, cancel any pending request for the current target so it cannot interfere with the new target.
				self->ResetNavigationPath( );
			}
			// Immediate attempt to move to the new target's position. 
			// This may fail if the async request is still being processed, 
			// but it will at least update our orientation immediately toward the new target 
			// and enqueue a new async request if we forced a reset above.
			self->MoveAStarToOrigin( spot->currentOrigin, forceRebuild );
		};

		/**
		*    Helper for advancing to the next breadcrumb when the current one is exhausted.
		**/
		auto advanceTrailTarget = [&]() -> bool {
			// Pick the next breadcrumb spot to pursue.
			svg_base_edict_t *nextSpot = PlayerTrail_PickNext( self );
			// If there is no next spot, we are done with the trail.
			if ( !nextSpot ) {
				return false;
			}
			// If the next spot is the same as our current target or is older than our trail_time, 
			// it is not a valid next target and we should stop pursuing the trail.
			if ( nextSpot == self->stateNavigationTrail.targetEntity || nextSpot->timestamp <= self->stateNavigationTrail.trailTimeStamp ) {
				return false;
			}
			// If the next spot is too old, it is not a valid target and we should stop pursuing the trail.
			assignTrailTarget( nextSpot, true );
			// Successfully advanced to the next trail target.
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
			if ( !self->stateNavigationTrail.targetEntity ) {
				return false;
			}
			// Compute 2D distance to current breadcrumb.
			Vector3 toTrail = QM_Vector3Subtract( self->stateNavigationTrail.targetEntity->currentOrigin, self->currentOrigin );
			double horizontalDist2 = ( toTrail.x * toTrail.x ) + ( toTrail.y * toTrail.y );
			// Use the same radius we use for nav waypoint advancement, with a sane fallback.
			double arrivalRadius = ( self->pathNavigationState.policy.waypoint_radius > 0. ) ? self->pathNavigationState.policy.waypoint_radius : NAV_DEFAULT_WAYPOINT_RADIUS;
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
		svg_base_edict_t *trailSpot = self->stateNavigationTrail.targetEntity;
		bool justAssignedTrailSpot = false;
		if ( !trailSpot ) {
			// Acquire the freshest breadcrumb entry to pursue.
			trailSpot = PlayerTrail_PickFirst( self );
			// If there is no breadcrumb to pursue, go idle.
			if ( !trailSpot ) {
				// Set think to idle before going to physics to ensure we do not miss a think cycle for processing the new trail target if one appears immediately after.
				Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
				// Skip the rest of the logic and go idle immediately.
				goto physics;
			}
			// Ignore stale breadcrumbs so trail pursuit reacts only to recent LOS loss.
			if ( ( level.time - trailSpot->timestamp ) > DUMMY_TRAIL_MAX_AGE ) {
				// Set trail target to null before going to idle to ensure we do not miss a think cycle for processing the new trail target if one appears immediately after.
				self->stateNavigationTrail.targetEntity = nullptr;
				// Set think to idle before going to physics to ensure we do not miss a think cycle for processing the new trail target if one appears immediately after.
				Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
				// Skip the rest of the logic and go idle immediately.
				goto physics;
			}
			// Start following the newly selected breadcrumb immediately.
			// Avoid forcing a full async rebuild on every assignment to prevent
			// flooding the async queue when breadcrumbs are acquired frequently.
			// Let the normal debounce/refresh behavior handle minor changes.
			assignTrailTarget( trailSpot, false );
			// Mark that we just assigned a trail spot so we do not immediately advance it in the next block.
			justAssignedTrailSpot = true;
		} else {
			/**
			*    Validate that the cached breadcrumb still makes sense.
			*    If the cached spot is older than our recorded trail_time (or has no
			*    meaningful timestamp), reacquire so we do not chase stale spots.
			**/
			// Note: we check the trail_time here to ensure that we do not consider breadcrumbs that are older than when we started pursuing the trail, which can happen if the player has been in and out of LOS frequently and we have lost track of which breadcrumbs are relevant to our current pursuit.
			if ( trailSpot->timestamp <= 0_ms || trailSpot->timestamp < self->stateNavigationTrail.trailTimeStamp || ( level.time - trailSpot->timestamp ) > DUMMY_TRAIL_MAX_AGE ) {
				// Cached breadcrumb is stale or invalid; try to reacquire a fresh one.
				trailSpot = PlayerTrail_PickFirst( self );
				// If we cannot reacquire a valid breadcrumb, go idle.
				if ( !trailSpot ) {
					// Set trail target to null before going to idle to ensure we do not miss a think cycle for processing the new trail target if one appears immediately after.
					self->stateNavigationTrail.targetEntity = nullptr;
					// Set think to idle before going to physics to ensure we do not miss a think cycle for processing the new trail target if one appears immediately after.
					Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
					// Skip the rest of the logic and go idle immediately.
					goto physics;
				}
				// If the reacquired breadcrumb is too old, give up on the trail and go idle.
				if ( ( level.time - trailSpot->timestamp ) > DUMMY_TRAIL_MAX_AGE ) {
					// Set trail target to null before going to idle to ensure we do not 
					// miss a think cycle for processing the new trail target if one appears immediately after.
					self->stateNavigationTrail.targetEntity = nullptr;
					// Set think to idle before going to physics to ensure we do not miss a think cycle for processing the new trail target if one appears immediately after.
					Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
					// Skip the rest of the logic and go idle immediately.
					goto physics;
				}
				// Reacquire a fresher breadcrumb and request a rebuild but do not
				// force it immediately. This prevents per-frame forced rebuilds
				// when timestamps drift slightly.
				assignTrailTarget( trailSpot, false );
				// Mark that we just assigned a trail spot so we do not immediately advance it in the next block.
				justAssignedTrailSpot = true;
			} else {
				// Continue working toward the cached breadcrumb.
				self->goalentity = trailSpot;
				// Immediate attempt to move to the breadcrumb's position. 
				// This may fail if the async request is still being processed, 
				// but it will at least update our orientation immediately toward the target 
				// and enqueue a new async request if we forced a reset above.
				self->MoveAStarToOrigin( trailSpot->currentOrigin );
			}
		}

		/**
		*    Move on to the next breadcrumb when we arrived or lost the current path.
		**/
		// Note: we check shouldAdvanceTrailTarget before advancing to ensure we only advance when we are actually close 
		// to the current breadcrumb, rather than advancing just because the async request expired or something. 
		// This prevents oscillation when we are still far from the current target.
		if ( !justAssignedTrailSpot && shouldAdvanceTrailTarget() ) {
			// Advance to the next breadcrumb if we are close enough to the current one.
			if ( !advanceTrailTarget() ) {
				// No valid next breadcrumb, go idle.
				Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
			}
		}
	}

/**
*    Physics and collision.
**/

physics:
	// For storing the results of the slide move.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	// Perform movement and capture any blocking results for recovery handling below.
	const bool moved = self->GenericThinkFinish( true, blockedMask );
	// Ensure the authoritative yaw/angles are applied to the entity state after movement
	// so rendering and networking see the updated orientation. Keep currentAngles authoritative.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	/**
	*    Obstruction recovery.
	**/
	// If we are blocked or trapped, our path is no longer valid. Force an immediate rebuild so we can recover.
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		// Clear any pending async request since it is no longer relevant when we are blocked.
		self->pathNavigationState.process.next_rebuild_time = 0_ms;
	}

	/**
	*	Schedule the next think.
	**/
	self->nextthink = level.time + FRAME_TIME_MS;
}

//=================================================================================================

/**
*   @brief	Investigate the most recent sound target and then fall back to idle/tracking.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_InvestigateSound )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*   Maintain base state and liveness.
	**/
	if ( !self->GenericThinkBegin() ) {
		return;
	}

	// If we are not activated, give up on any sound target and go idle. 
	// This can happen if we got activated by a sound but then lost interest before we processed the investigate logic.
	if ( !self->isActivated ) {
		// Remove interest in the sound target since we are not active anymore. 
		// This also prevents us from accidentally pursuing a stale sound target 
		// if we got activated by a sound but then lost interest before processing the investigate logic.
		self->stateSoundCan.hasOrigin = false;
		// No need to reset nav state here since we will do so when we next attempt to pursue something, 
		// but we should clear the goalentity so we do not accidentally pursue a stale sound target 
		// if we got activated by a sound but then lost interest before processing the investigate logic.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		// Skip the rest of the logic and go idle immediately.
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	{
		Vector3 likelyGoal = self->activator ? self->activator->currentOrigin : self->currentOrigin;
		self->DetermineGoalZBlendPolicyState( likelyGoal );
	}

	/**
	*   Player reacquire gate while investigating sound.
	**/
    if ( self->activator ) {
		// Sound investigation should only be preempted by confirmed LOS.
		if ( SVG_Entity_IsVisible( self, self->activator ) ) {
			// Player is a valid target to interrupt sound investigation, switch to direct pursuit.
			Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
			// Immediate attempt to target the player.
			self->nextthink = level.time + FRAME_TIME_MS;
			return;
		}
	}

	/**
	*   Validate we still have a useful sound target.
	**/
	// If we do not have a sound origin to investigate, or if the sound origin is too old, give up and go idle.
	if ( !self->stateSoundCan.hasOrigin || ( level.time - self->stateSoundCan.lastTime ) > DUMMY_SOUND_INVESTIGATE_MAX_AGE ) {
		// Clear interest in the sound target since it is no longer relevant.
		self->stateSoundCan.hasOrigin = false;
		// Set next think to idle since we have nothing to investigate anymore.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		// Skip the rest of the logic and go idle immediately.
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*   Navigate toward the cached sound origin.
	**/
	// We unset the goalentity here to ensure that any async request associated with a previous 
	// goal cannot interfere with our navigation toward the sound target.
	self->goalentity = nullptr;
	// Immediate attempt to move to the sound origin. This may fail if there is still a 
	// pending async request for a previous goal, but it will at least update our orientation 
	// toward the sound and enqueue a new async request if we cleared the old one above.
	self->MoveAStarToOrigin( self->stateSoundCan.origin );

	/**
	*   Leave investigate mode once we reached the sound location.
	**/
	// Compute 2D distance to sound origin for arrival checking.
	const double soundDist3DSqr = QM_Vector3DistanceSqr( self->stateSoundCan.origin, self->currentOrigin );
	// If we are close enough to the sound origin, consider that we have arrived and go idle.
	if ( soundDist3DSqr <= ( DUMMY_SOUND_INVESTIGATE_REACHED_DIST * DUMMY_SOUND_INVESTIGATE_REACHED_DIST ) ) {
		// Clear interest in the sound target since we have arrived.
		self->stateSoundCan.hasOrigin = false;
		// Set next think to idle since we have nothing to investigate anymore.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
	}

	/**
	*   Physics and collision.
	**/
	// For storing the results of the slide move.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	// Perform movement and capture any blocking results for recovery handling below.
	const bool moved = self->GenericThinkFinish( true, blockedMask );
	// Ensure the authoritative yaw/angles are applied to the entity state after movement
	// so rendering and networking see the updated orientation. Keep currentAngles authoritative.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );
	// If we are blocked or trapped, our path is no longer valid. Force an immediate rebuild so we can recover.
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		// Clear any pending async request since it is no longer relevant when we are blocked.
		self->pathNavigationState.process.next_rebuild_time = 0_ms;
	}
	// Schedule the next think.
	self->nextthink = level.time + FRAME_TIME_MS;
}

//=================================================================================================

/**
*	@brief		Always looks for activator presence, or its trail, and otherwise does nothing.
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
	if ( !self->GenericThinkBegin( ) ) {
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
		self->stateNavigationTrail.targetEntity = nullptr;
		self->goalentity = nullptr;
		self->ResetNavigationPath( );
	}

	if ( !self->isActivated ) {
		SVG_Util_SetEntityAngles( self, self->currentAngles, true );
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Always look for the player(activator) so we can react immediately when they appear.
	**/
    if ( self->activator && SVG_Entity_IsVisible( self, self->activator ) ) {
		// Immediate action.
		self->goalentity = self->activator;
		// Set the nextThink to AStarToPlayer so we start chasing the player right away.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
		self->nextthink = level.time + FRAME_TIME_MS;
		// Skip all other idle logic if we just found the player.
		return;
	}

	/**
	*   Sound-investigation acquisition:
	*       If a fresh noise event was emitted nearby, investigate that position
	*       before trying stale breadcrumb trail follow.
	**/
	{
		// Find the freshest sound event between the two possible sound entities.
		svg_base_edict_t *foundAudibleEntity = nullptr;
		//! Note that `weapon_sound_entity` and `impact_sound_entity` and `personal_sound_entity` 
		//! are separate slots for noise events,
		//! 
		//! But they can come from the same source (e.g. player footsteps can trigger both). 
		//! 
		#ifdef DUMMY_SEEK_IMPACT_SOUND_ONLY
			// If impact_sound_entity is even newer, use that instead.
			if ( SVG_Entity_IsActive( level.impact_sound_entity ) && level.impact_sound_entity->last_sound_time > self->stateSoundCan.lastTime ) {
				// If we don't have a fresh sound yet, or if sound2_entity is newer than the current freshest sound, use sound2_entity.
				if ( !foundAudibleEntity
					|| ( foundAudibleEntity && ( level.impact_sound_entity && level.impact_sound_entity->last_sound_time > foundAudibleEntity->last_sound_time ) ) ) {
					// sound2_entity is fresher than sound_entity, so use sound2_entity as the freshest sound.
					foundAudibleEntity = level.impact_sound_entity;
				}
			}
		#else
			//! We want to react to the freshest event regardless of which slot it is in, so we compare timestamps to find the most recent.
			if ( SVG_Entity_IsActive( level.weapon_sound_entity ) && level.weapon_sound_entity->last_sound_time > self->stateSoundCan.lastTime ) {
				// Start with sound_entity as the freshest sound if it is newer than our last processed sound time.
				foundAudibleEntity = level.weapon_sound_entity;
			}
			// If impact_sound_entity is even newer, use that instead.
			if ( SVG_Entity_IsActive( level.impact_sound_entity ) && level.impact_sound_entity->last_sound_time > self->stateSoundCan.lastTime ) {
				// If we don't have a fresh sound yet, or if sound2_entity is newer than the current freshest sound, use sound2_entity.
				if ( !foundAudibleEntity 
					|| ( foundAudibleEntity && ( level.impact_sound_entity && level.impact_sound_entity->last_sound_time > foundAudibleEntity->last_sound_time ) ) ) {
					// sound2_entity is fresher than sound_entity, so use sound2_entity as the freshest sound.
					foundAudibleEntity = level.impact_sound_entity;
				}
			}
			// If impact_sound_entity is even newer, use that instead.
			if ( SVG_Entity_IsActive( level.personal_sound_entity ) && level.personal_sound_entity->last_sound_time > self->stateSoundCan.lastTime ) {
				// If we don't have a fresh sound yet, or if sound2_entity is newer than the current freshest sound, use sound2_entity.
				if ( !foundAudibleEntity
					|| ( foundAudibleEntity && ( level.personal_sound_entity && level.personal_sound_entity->last_sound_time > foundAudibleEntity->last_sound_time ) ) ) {
					// sound2_entity is fresher than sound_entity, so use sound2_entity as the freshest sound.
					foundAudibleEntity = level.personal_sound_entity;
				}
			}
		#endif // DUMMY_SEEK_IMPACT_SOUND_ONLY

		/**
		*	Will use the utility function to determine whether the sound source is actually audible for
		*	the monster using the PHS so sound events that are behind walls or otherwise not truly audible 
		*	will not trigger investigation. 
		*
		*	This prevents the monster from reacting to sound events that it should not be able to hear, which can be 
		*	especially common for sound events that are triggered by the player but are not actually audible to the 
		*	monster due to obstacles or distance.
		**/
		svg_base_edict_t *audibleEntity = ( foundAudibleEntity && SVG_Util_IsEntityAudibleByPHS( self, foundAudibleEntity, true, DUMMY_NAV_DEBUG ) ) ? foundAudibleEntity : nullptr;

		/**
		*	If there is a fresh sound event, and it is reasonably close and recent, pursue it with high priority over trail - following.
		*	This allows the monster to react to player - generated noise events even when the player is not visible, but prevents it 
		*	from chasing old sounds across the map.
		**/
		if ( audibleEntity ) {
			// Compute the age of the sound event.
			const QMTime soundAge = level.time - audibleEntity->last_sound_time;
			// Compute 3D distance to the sound source.
			const double soundDist3D = std::sqrt( QM_Vector3DistanceSqr( audibleEntity->currentOrigin, self->currentOrigin ) );
			// Only react to the sound if it is reasonably fresh and nearby. This prevents the monster from chasing old sounds across the map.
			if ( soundAge <= DUMMY_SOUND_INVESTIGATE_MAX_AGE && soundDist3D <= DUMMY_SOUND_INVESTIGATE_MAX_DIST ) {
				// Cache the sound origin so we can investigate it over multiple frames if needed.
				self->stateSoundCan.origin = audibleEntity->currentOrigin;
				// Mark that we have a valid sound origin to investigate so the investigate thinker does not immediately discard it.
				self->stateSoundCan.hasOrigin = true;
				// Cache the sound time so we do not react multiple times to the same event.
				self->stateSoundCan.lastTime = audibleEntity->last_sound_time;
				// Reset nav state so we do not reuse stale async requests/paths if we were previously pursuing a different target.
				self->ResetNavigationPath( );
				// Set the nextThink to InvestigateSound so we start moving toward the sound right away.
				Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::InvestigateSound );
				self->nextthink = level.time + FRAME_TIME_MS;
				return;
			}
		}
	}

	/**
	*	Trail-follow acquisition:
	*		Breadcrumb trail spots are position markers, not enemies. Requiring them to be
	*		"in front" or "visible" makes the selection unstable (as we move/turn we can
	*		flip between different breadcrumbs), which presents as back-and-forth jitter.
	*		Instead, once the player is not directly visible, we may pursue the freshest
	*		breadcrumb unconditionally.
	**/
	// Pick the freshest breadcrumb entry to pursue. We will validate it is not stale before pursuing, 
	// but we want to pick it first so we can cache it and avoid PickFirst jitter.
	svg_base_edict_t *trailTarget = PlayerTrail_PickFirst( self );
	// Ifthere is a breadcrumb, but it is too old, ignore it. This prevents the monster from chasing old breadcrumbs across 
	// the map when the player has been out of sight for a while. 
	if ( trailTarget ) {
		// Reject stale breadcrumbs so trail mode does not chase old patrol history.
		if ( ( level.time - trailTarget->timestamp ) > DUMMY_TRAIL_MAX_AGE ) {
			trailTarget = nullptr;
		}
	}
	// If we have a valid breadcrumb target, pursue it. Otherwise, stay idle and keep looking.
	if ( trailTarget ) {
		// Cache the breadcrumb target so pursuit mode starts with a stable goal.
		self->stateNavigationTrail.targetEntity = trailTarget;
		self->stateNavigationTrail.trailTimeStamp = trailTarget->timestamp;
		// Set the goalentity to the breadcrumb so movement and orientation will target it.
		self->goalentity = trailTarget;
		// Reset nav path so we do not reuse a path built for a different goal.
		self->ResetNavigationPath( );
		// Set the nextThink to PursuitTrail so we start following the player's trail right away.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursueBreadcrumb );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*   Idle lookout behavior:
	*       Sweep yaw while waiting so we periodically look for reacquisition.
	**/
    // Idle scan: step among 8 fixed headings (0..315 in 45deg increments) but
	// smoothly interpolate (lerp) our facing towards the discrete target yaw so
	// the monster turns naturally instead of snapping.
	if ( self->stateIdleScan.nextFlipTime <= level.time ) {
		// Advance discrete heading index in the current direction.
		self->stateIdleScan.headingIndex += ( int32_t )self->stateIdleScan.yawScanDirection;
		// Wrap index into 0..7 range.
		if ( self->stateIdleScan.headingIndex < 0 ) {
			self->stateIdleScan.headingIndex = 7;
		} else if ( self->stateIdleScan.headingIndex > 7 ) {
			self->stateIdleScan.headingIndex = 0;
		}
		// Occasionally flip overall sweep direction to avoid bias. Flip when we hit ends.
		if ( self->stateIdleScan.headingIndex == 0 || self->stateIdleScan.headingIndex == 7 ) {
			self->stateIdleScan.yawScanDirection = -self->stateIdleScan.yawScanDirection;
		}
		// Compute the new discrete target yaw in degrees for this heading.
		self->stateIdleScan.targetYaw = ( float )( self->stateIdleScan.headingIndex * DUMMY_IDLE_SCAN_STEP_DEG );
		// Randomize next flip in [100ms,300ms) range to make scanning less synchronous.
		self->stateIdleScan.nextFlipTime = level.time + random_time( 100_ms, 300_ms );
	}

    /**
	*    Smoothly interpolate the `ideal_yaw` toward the discrete `stateIdleScan.targetYaw`.
	*    This produces natural turning motion between headings instead of instant snapping.
	*    We compute the shortest angular difference and lerp a small fraction each frame
	*    scaled by frame time so turns remain consistent across variable frame rates.
	**/
	const float currentYaw = self->ideal_yaw;
	float deltaYaw = self->stateIdleScan.targetYaw - currentYaw;
	// Normalize to [-180,180] to choose shortest rotation direction.
	while ( deltaYaw > 180.0f ) deltaYaw -= 360.0f;
	while ( deltaYaw < -180.0f ) deltaYaw += 360.0f;
	// Lerp factor: bias based on frame time for consistent smoothing across frame rates.
	const float lerpFactor = QM_Clamp( ( float )( gi.frame_time_s * 6.0 ), 0.05f, 0.5f );
	self->ideal_yaw = currentYaw + deltaYaw * lerpFactor;
	// Use a moderate yaw speed to allow the facing helper to finish the turn smoothly.
	self->yaw_speed = 12.0f;
	SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );
	// Animation/angles always applied.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	// For storing the results of the slide move.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	// Perform movement and capture any blocking results for recovery handling below.
	const bool moved = self->GenericThinkFinish( true, blockedMask );
	// Ensure the authoritative yaw/angles are applied to the entity state after movement
	// so rendering and networking see the updated orientation. Keep currentAngles authoritative.
	//SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	/**
	*	Set the nextThink to Idle so we keep looking for the player or their trail instead of trying to pursue a non-visible target.
	**/
	Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
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

	// For storing the results of the slide move.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	// Perform movement and capture any blocking results for recovery handling below.
	const bool moved = self->GenericThinkFinish( true, blockedMask );
	// Ensure the authoritative yaw/angles are applied to the entity state after movement
	// so rendering and networking see the updated orientation. Keep currentAngles authoritative.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	// Stay dead.
	self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink_Dead );
	self->nextthink = level.time + FRAME_TIME_MS;
}


//=============================================================================================
//=============================================================================================


/**
*
*
*
*		(Generic-) NPC Entity Think Support Routines:
*
*
*
**/
/**
	*	@brief	Generic support routine taking care of the base logic that each onThink implementation relies on.
	*			( Setup navPolicy, recategorize ground and liquid information,  check for being alive,
	*			  check for activator presence, etc).
	*	@return	True if the caller should proceed with its specific think logic, or false if it should return early and skip the specific think logic.
**/
const bool svg_monster_testdummy_debug_t::GenericThinkBegin() {
	/**
	*	Clear visual flags.
	**/
	s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

	/**
	*	Setup A* Navigation Policy: stairs, drops, and obstruction jumping.
	**/
	pathNavigationState.policy.waypoint_radius = NAV_DEFAULT_WAYPOINT_RADIUS;
	pathNavigationState.policy.max_step_height = NAV_DEFAULT_STEP_MAX_SIZE;
	pathNavigationState.policy.max_drop_height = NAV_DEFAULT_MAX_DROP_HEIGHT;
	pathNavigationState.policy.enable_max_drop_height_cap = true;
	pathNavigationState.policy.max_drop_height_cap = ( nav_max_drop_height_cap && nav_max_drop_height_cap->value > 0.0f ) ? nav_max_drop_height_cap->value : NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;
	pathNavigationState.policy.enable_goal_z_layer_blend = true;
	pathNavigationState.policy.blend_start_dist = PHYS_STEP_MAX_SIZE;
	pathNavigationState.policy.blend_full_dist = NAV_DEFAULT_BLEND_DIST_FULL;
	// No blending seems to work!
	//pathNavigationState.policy.enable_goal_z_layer_blend = false;
	//pathNavigationState.policy.blend_start_dist = PHYS_STEP_MAX_SIZE;
	//pathNavigationState.policy.blend_full_dist = 128.0;
	pathNavigationState.policy.allow_small_obstruction_jump = true;
	pathNavigationState.policy.max_obstruction_jump_height = NAV_DEFAULT_MAX_OBSTRUCTION_JUMP_SIZE;

	/**
	*    Recategorize position and check grounding.
	**/
	RecategorizeGroundAndLiquidState();

	/**
	*    Liveness check.
	**/
	if ( health <= 0 || ( lifeStatus & LIFESTATUS_ALIVE ) != LIFESTATUS_ALIVE ) {
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
*	@brief	Generic support routine taking care of the finishing logic that each onThink implementation relies on.
*			( Deal with the slidemove process, stepping stairs, jumping over obstructions, crouching under obstructions. ).
*	@param	processSlideMove	When true, will perform the slide move and all the associated logic for handling blocked/trapped results.
*								When false, will skip the slide move and just return false so the caller can handle it in its specific way
*								(e.g., the caller may want to try a different movement approach if we are blocked, or may want to ignore being blocked if we are just trying to adjust our position to better see the player).
*	@param	blockedMask			The blockedMask result from the slide move, which is important for the caller to determine if we should do any special handling
*								for being trapped (e.g., try to jump, pick a new path, etc).
*	@return	False if we didn't move, true if we did.
**/
const bool svg_monster_testdummy_debug_t::GenericThinkFinish( const bool processSlideMove, int32_t &blockedMask ) {
	// Perform the slide move and get the blocked mask describing the result of the movement attempt.
	blockedMask = ( processSlideMove ? ProcessSlideMove() : MM_SLIDEMOVEFLAG_NONE );

	// If we are not blocked or trapped, we can update our position and grounding info. 
	// Otherwise, we will rely on the next think to attempt recovery and not update our position 
	// so we don't get stuck in invalid geometry.
	if ( !( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) ) {
		// Update velocity to the new velocity resulting from the movement attempt, which is likely modified.
		velocity = monsterMove.state.velocity;
		// Update position and grounding info.
		groundInfo = monsterMove.ground;
		liquidInfo = monsterMove.liquid;
		// Update the entity's origin to the new position resulting from the movement attempt.
		SVG_Util_SetEntityOrigin( this, monsterMove.state.origin, true );
		// Update the entity's link in the world after changing its position.
		gi.linkentity( this );
	} else {
		// We failed to move, we're trapped, this is no good.
		return false;
	}

	// We moved successfully, return true.
	return true;
}


//=============================================================================================
//=============================================================================================


/**
*
*
*
*
*	Explicit NPC State Management:
*
*
*
*
**/
//! Tracks whether the NPC has been enabled(By use, with the intend for it to follow the player) by the player.
//bool isActivated = false;
////! Last server time when the activator was confirmed visible.
//QMTime lastPlayerVisibleTime = 0_ms;
//
///**
//*   Explicit AI states.
//**/
//enum class AIThinkState {
//	IdleLookout,
//	PursuePlayer,
//	PursueBreadcrumb,
//	InvestigateSound
//};
////! Determines the thinking state callback to fire for the frame.
//AIThinkState thinkAIState = AIThinkState::IdleLookout;
/**
*	@brief	NPC Goal Z Blend Policy Adjustment Helper:
**/
void svg_monster_testdummy_debug_t::DetermineGoalZBlendPolicyState( const Vector3 &goalOrigin ) {
	/* Tune goal-Z blending for this think tick. */
	{
		Vector3 likelyGoal = activator ? activator->currentOrigin : currentOrigin;
		AdjustGoalZBlendPolicy( likelyGoal );
	}

	///* Tune goal-Z blending for this think tick. */
	//{
	//	Vector3 likelyGoal = stateNavigationTrail.targetEntity ? stateNavigationTrail.targetEntity->currentOrigin : ( activator ? activator->currentOrigin : currentOrigin );
	//	AdjustGoalZBlendPolicy( likelyGoal );
	//}

	/*
	* Tune goal-Z blending based on current likely goal so nav policy is
	* appropriate before any path rebuilds are queued this frame.
	*/
	{
		Vector3 likelyGoal = stateNavigationTrail.targetEntity ? stateNavigationTrail.targetEntity->currentOrigin : ( activator ? activator->currentOrigin : currentOrigin );
		AdjustGoalZBlendPolicy( likelyGoal );
	}

	/**
	*\tTune goal-Z blending based on current likely goal so nav policy is
	*\tappropriate before any path rebuilds are queued this frame.
	**/
	{
		Vector3 likelyGoal = activator ? activator->currentOrigin : ( stateNavigationTrail.targetEntity ? stateNavigationTrail.targetEntity->currentOrigin : currentOrigin );
		AdjustGoalZBlendPolicy( likelyGoal );
	}

	///**
	//*	Tune goal-Z blending based on current likely goal so nav policy is
	//*	appropriate before any path rebuilds are queued this frame.
	//**/
	//{
	//	Vector3 likelyGoal = activator ? activator->currentOrigin : currentOrigin;
	//	AdjustGoalZBlendPolicy( likelyGoal );
	//}
}


/**
*
*
*
*	Animation Processing Work:
*
*
*
**/
//skm_rootmotion_set_t *rootMotionSet = nullptr;


//=============================================================================================
//=============================================================================================

/**
*
*
*
*	Basic AI Physics Movement(-State):
*
*
*
**/
/**
*	@brief	Performs the actual SlideMove processing and updates the final origin if successful.
**/
const int32_t svg_monster_testdummy_debug_t::ProcessSlideMove() {

	// Perform the slide move and get the blocked mask describing the result of the movement attempt.
	const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove, pathNavigationState.policy );

	// Return the blocked mask so the caller can decide how to react to obstructions.
	return blockedMask;
}

/**
*    @brief    Slerp direction helper. (Local to this TU to avoid parent dependency).
**/
const Vector3 svg_monster_testdummy_debug_t::SlerpDirectionVector3( const Vector3 &from, const Vector3 &to, float t ) {
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


//=============================================================================================
//=============================================================================================


/**
*
*
*
*	Internal Navigation Queueing and Path Following Logic API:
*
*
*
**/
/**
*	@brief	Retrieve the appropriate navigation agent bounds for the entity, prioritizing navmesh-defined bounds, then nav-agent-profile-defined bounds, and finally falling back to entity-defined bounds if necessary.
**/
void svg_monster_testdummy_debug_t::GetNavigationAgentBounds( Vector3 *out_mins, Vector3 *out_maxs ) {
	if ( !out_mins || !out_maxs ) {
		return;
	}

	// First priority: navmesh-defined agent bounds if available and valid.
	const nav_mesh_t *mesh = g_nav_mesh.get();
	const bool meshAgentValid = mesh != nullptr
		&& ( mesh->agent_maxs.z > mesh->agent_mins.z )
		&& ( mesh->agent_maxs.x > mesh->agent_mins.x )
		&& ( mesh->agent_maxs.y > mesh->agent_mins.y );
	// Second priority: nav-agent-profile-defined bounds if valid.
	if ( meshAgentValid ) {
		*out_mins = mesh->agent_mins;
		*out_maxs = mesh->agent_maxs;
		return;
	}
	// Third priority: entity-defined bounds as a fallback to ensure we always have some kind of valid bounds to work with.
	const nav_agent_profile_t agentProfile = SVG_Nav_BuildAgentProfileFromCvars();
	// Check if the agent profile bounds are valid (maxs greater than mins in all dimensions).
	const bool profileValid = ( agentProfile.maxs.z > agentProfile.mins.z )
		&& ( agentProfile.maxs.x > agentProfile.mins.x )
		&& ( agentProfile.maxs.y > agentProfile.mins.y );
	// If the agent profile bounds are valid, use them. Otherwise, fall back to the entity's mins and maxs.
	if ( profileValid ) {
		*out_mins = agentProfile.mins;
		*out_maxs = agentProfile.maxs;
		return;
	}
	// Final fallback: use the entity's mins and maxs, which should always be valid for a properly initialized entity.
	*out_mins = mins;
	*out_maxs = maxs;
}
/**
*   @brief	Clear stale async nav request state when no navmesh is loaded.
*   @param	self	Debug testdummy owning the async path process.
*   @return	True when navmesh is unavailable and caller should early-return.
*   @note	Prevents repeated queue refresh/debounce loops on maps without navmesh.
**/
const bool svg_monster_testdummy_debug_t::GuardForNullNavMesh() {
	/**
	*   Fast path: navmesh exists, caller may continue normal request flow.
	**/
	if ( g_nav_mesh.get() ) {
		return false;
	}

	/**
	*   Determine whether there is any async state worth tearing down.
	**/
	const bool hadPendingState = ( pathNavigationState.process.pending_request_handle > 0 )
		|| pathNavigationState.process.rebuild_in_progress
		|| SVG_Nav_IsRequestPending( &pathNavigationState.process );

	/**
	*   Cancel tracked handle first so queue state transitions to terminal.
	**/
	if ( pathNavigationState.process.pending_request_handle > 0 ) {
		SVG_Nav_CancelRequest( ( nav_request_handle_t )pathNavigationState.process.pending_request_handle );
	}

	/**
	*   Clear local markers so callers stop reporting pending async work.
	**/
	pathNavigationState.process.pending_request_handle = 0;
	pathNavigationState.process.rebuild_in_progress = false;

	/**
	*   Emit a debug message only when we actually cleaned stale state.
	**/
	if ( hadPendingState && DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "[NAV DEBUG] %s: navmesh unavailable, cleared pending async state and skipped queueing.\n", __func__ );
	}

	/**
	*   No navmesh loaded; caller should skip path request/query work.
	**/
	return true;
}

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
	GetNavigationAgentBounds( &agent_mins, &agent_maxs );
	Q_assert( agent_maxs.x > agent_mins.x && agent_maxs.y > agent_mins.y && agent_maxs.z > agent_mins.z );

	// Debug print summarizing the request state for diagnostics.
	if ( DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "[NAV DEBUG] %s: goal=(%.1f %.1f %.1f) force=%d pathOk=%d pending=%d\n",
			__func__, goalOrigin.x, goalOrigin.y, goalOrigin.z, ( int32_t )force,
			( int32_t )( pathNavigationState.process.path.num_points > 0 ),
			( int32_t )SVG_Nav_IsRequestPending( &pathNavigationState.process ) );
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
	*   Local helper for direct-steer fallback when no A* direction exists.
	*       Keep debug monster behavior responsive on maps without navmesh.
	**/
	auto applyDirectFallback = [&]( const Vector3 &fallbackGoal ) -> bool {
		// Compute a horizontal direction toward the fallback goal.
		Vector3 toGoal = QM_Vector3Subtract( fallbackGoal, currentOrigin );
		toGoal.z = 0.0f;
		const float len2 = ( toGoal.x * toGoal.x ) + ( toGoal.y * toGoal.y );
		if ( len2 <= ( 0.001f * 0.001f ) ) {
			velocity.x = velocity.y = 0.0f;
			return false;
		}

		Vector3 moveDir = QM_Vector3Normalize( toGoal );
		ideal_yaw = QM_Vector3ToYaw( moveDir );
		yaw_speed = 10.0f;
		SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );

		if ( rootMotionSet && rootMotionSet->motions[ 4 ] ) {
			skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 4 ]; // RUN
			const double t = level.time.Seconds<double>();
			const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
			const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
			s.frame = rootMotion->firstFrameIndex + localFrame;
		}

		constexpr double frameVelocity = 220.0;
		velocity.x = ( float )( moveDir.x * frameVelocity );
		velocity.y = ( float )( moveDir.y * frameVelocity );
		// Mirror the fallback velocity onto the monster move state so the slide move will carry it.
		monsterMove.state.velocity.x = velocity.x;
		monsterMove.state.velocity.y = velocity.y;
		return true;
	};

	/**
	*   Guard: when no navmesh is loaded, clear stale async state and rely on
	*       direct fallback movement so the debug state machine keeps functioning.
	**/
	if ( GuardForNullNavMesh() ) {
		return applyDirectFallback( goalOrigin );
	}

	/**
	*	Attempt to queue a navigation rebuild for the target origin.
	*	Force ensures the helpers bypass throttles/heuristics when the caller demands an immediate rebuild.
	*	The helper will respect internal throttles unless `force` is true.
	**/
	// Keep queue suppression aligned with pursue-player gating used by state logic:
	// only suppress when the activator is both invisible and outside the proximity gate.
	const bool activatorVisible = ( activator && SVG_Entity_IsVisible( this, activator ) );
	const double activatorDist2D = activator
		? std::sqrt( QM_Vector2DistanceSqr( activator->currentOrigin, currentOrigin ) )
		: 0.0;
	const bool activatorWithinProximity = activator && ( activatorDist2D <= DUMMY_PLAYER_PURSUIT_MAX_DIST );

	/**
	*    Gather and cache queue-state helpers for the upcoming logging block.
	**/
	const bool queueModeEnabled = ( nav_nav_async_queue_mode && nav_nav_async_queue_mode->integer != 0 );
	const bool asyncNavEnabled = SVG_Nav_IsAsyncNavEnabled();
	const bool canRebuild = pathNavigationState.process.CanRebuild( pathNavigationState.policy );
	const bool movementWarrantsRebuild =
		pathNavigationState.process.ShouldRebuildForGoal2D( goalOrigin, pathNavigationState.policy )
		|| pathNavigationState.process.ShouldRebuildForGoal3D( goalOrigin, pathNavigationState.policy )
		|| pathNavigationState.process.ShouldRebuildForStart2D( currentOrigin, pathNavigationState.policy )
		|| pathNavigationState.process.ShouldRebuildForStart3D( currentOrigin, pathNavigationState.policy );

	bool queueAttempted = false;
	bool queueResult = false;
	if ( goalentity == activator && activator && !activatorVisible && !activatorWithinProximity && !force ) {
		// Skip queuing a new/refresh request only when direct pursuit is no longer valid.
		// This keeps queue behavior consistent with state selection and avoids
		// freezing in PursuePlayer while invisible-but-near.
		if ( DUMMY_NAV_DEBUG != 0 ) {
			gi.dprintf( "[NAV DEBUG] %s: skipping queue to invisible+far activator dist2D=%.1f max=%.1f\n",
				__func__, activatorDist2D, DUMMY_PLAYER_PURSUIT_MAX_DIST );
		}

		// If there is a pending async request for our path process, cancel it
		// and advance the generation so any in-flight results are ignored.
		if ( SVG_Nav_IsRequestPending( &pathNavigationState.process ) || pathNavigationState.process.rebuild_in_progress || pathNavigationState.process.pending_request_handle != 0 ) {
			if ( pathNavigationState.process.pending_request_handle != 0 ) {
				if ( DUMMY_NAV_DEBUG != 0 ) {
					gi.dprintf( "[NavAsync][Cancel] Cancelling pending request handle=%d for ent_process=%p\n",
						pathNavigationState.process.pending_request_handle, ( void * )&pathNavigationState.process );
				}
				SVG_Nav_CancelRequest( ( nav_request_handle_t )pathNavigationState.process.pending_request_handle );
			}

			// Mark that we have no pending request and that a rebuild is not in progress.
			pathNavigationState.process.pending_request_handle = 0;
			pathNavigationState.process.rebuild_in_progress = false;

			// Bump generation to ensure any late async results are discarded.
			++pathNavigationState.process.request_generation;

			// Apply a conservative backoff so we do not immediately requeue and thrash the async queue.
			pathNavigationState.process.backoff_until = level.time + pathNavigationState.policy.fail_backoff_base;
			// Also throttle the next rebuild attempt slightly using the standard rebuild interval.
			pathNavigationState.process.next_rebuild_time = level.time + pathNavigationState.policy.rebuild_interval;
		}
	} else {
		// Only honor an explicit force request when the goal moved beyond the
		// configured rebuild thresholds. This prevents callers from forcing each
		// frame for negligible movements.
		bool effectiveForce = force;
		if ( force ) {
			// Compute 2D and 3D movement deltas relative to our last recorded
			// nav goal (if available) so we only force when movement is meaningful.
			const Vector3 &referenceGoal = pathNavigationState.lastGoal.isValid ? pathNavigationState.lastGoal.origin : pathNavigationState.process.path_goal_position;
			const double dx = QM_Vector3LengthDP( QM_Vector3Subtract( goalOrigin, referenceGoal ) );
			// If the goal moved less than the 2D threshold, do not force.
			if ( dx <= pathNavigationState.policy.rebuild_goal_dist_2d ) {
				effectiveForce = false;
				if ( DUMMY_NAV_DEBUG != 0 ) {
					gi.dprintf( "[NAV DEBUG] %s: suppressed force rebuild (dx=%.2f <= thresh=%.2f)\n",
						__func__, dx, pathNavigationState.policy.rebuild_goal_dist_2d );
				}
			}
		}

		/**
		*    Build the queue policy snapshot for this request.
		*        Keep default behavior unless the dedicated debug define asks us to
		*        bypass the hierarchical route filter for StepTest isolation.
		**/
		svg_nav_path_policy_t &queuePolicy = pathNavigationState.policy;
		#if MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER
		// Route-filter isolation mode: explicitly disable coarse tile filtering so
		// neighbor diagnostics reflect pure StepTest traversal behavior.
		queuePolicy.enable_cluster_route_filter = false;
		#endif
		queueAttempted = true;
		const bool queued = TryRebuildNavigationInQueue( currentOrigin, goalOrigin, queuePolicy, agent_mins, agent_maxs, effectiveForce );
		queueResult = queued;
		// If we successfully requested a rebuild and the caller intended a force,
		// record the last forced goal so subsequent small deltas do not re-force.
		if ( queued && force ) {
			pathNavigationState.lastGoal.origin = goalOrigin;
			pathNavigationState.lastGoal.isValid = true;
		}
	}

	/**
	*    Rate-limited per-entity queue status logging for navigation verification.
	**/
	const QMTime now = level.time;
	if ( DUMMY_NAV_DEBUG != 0 && now >= pathNavigationState.nextQueueStatusLogTime ) {
		pathNavigationState.nextQueueStatusLogTime = now + 500_ms;
		gi.dprintf( "[NAV STATUS] ent=%d attempt=%d result=%d canRebuild=%d movementWarrants=%d async=%d queueMode=%d\n",
			s.number,
			queueAttempted ? 1 : 0,
			queueResult ? 1 : 0,
			canRebuild ? 1 : 0,
			movementWarrantsRebuild ? 1 : 0,
			asyncNavEnabled ? 1 : 0,
			queueModeEnabled ? 1 : 0 );
	}

	/**
	*    Path availability and request state checks.
	**/
	// We may have a path process in flight but no populated path buffer yet; check both.
	const bool hasPathPoints = ( pathNavigationState.process.path.num_points > 0 && pathNavigationState.process.path.points );
	const bool pathExpired = hasPathPoints && pathNavigationState.process.path_index >= pathNavigationState.process.path.num_points;
	// Valid usable path only when points exist and we haven't consumed them all.
	const bool pathOk = hasPathPoints && !pathExpired;

	/**
	*    Keep a movement direction container for either path-follow or fallback.
	**/
	Vector3 move_dir = { 0.0f, 0.0f, 0.0f };

	// If we have a valid path, query and follow its movement direction.
	if ( pathOk && pathNavigationState.process.QueryDirection3D( currentOrigin, pathNavigationState.policy, &move_dir ) ) {
		/**
		*    Preserve trail bookkeeping while following breadcrumbs and only
		*    advance the trail marker to "now" when directly chasing the
		*    activator. Using level.time unconditionally made us treat every
		*    existing breadcrumb as stale, which caused PickFirst/PickNext to
		*    skip the trail entirely.
		*/
		if ( stateNavigationTrail.targetEntity && goalentity == stateNavigationTrail.targetEntity ) {
			stateNavigationTrail.trailTimeStamp = stateNavigationTrail.targetEntity->timestamp;
		} else {
			stateNavigationTrail.trailTimeStamp = level.time;
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
	*    If no path direction is currently available, use direct fallback so we
	*    keep moving while async pathing catches up.
	**/
	/**
	*    Backoff-aware fallback suppression:
	*        If no path is available and no async request is currently pending,
	*        do not keep direct-steering during an active failure backoff window.
	*
	*        This prevents the debug monster from appearing to be permanently in
	*        fallback pursuit while path rebuild is intentionally throttled.
	**/
	const bool requestPending = SVG_Nav_IsRequestPending( &pathNavigationState.process );
	const bool inFailureBackoff = ( pathNavigationState.process.backoff_until > level.time );
	if ( !pathOk && !requestPending && inFailureBackoff ) {
		// Stop horizontal fallback steering while waiting for the next allowed rebuild time.
		velocity.x = 0.0f;
		velocity.y = 0.0f;
		monsterMove.state.velocity.x = 0.0f;
		monsterMove.state.velocity.y = 0.0f;

		if ( DUMMY_NAV_DEBUG != 0 ) {
			gi.dprintf( "[NAV DEBUG] %s: suppressing direct fallback while in backoff (until=%lld) with no path/pending request\n",
				__func__, ( long long )pathNavigationState.process.backoff_until.Milliseconds() );
		}

		return false;
	}

	if ( applyDirectFallback( goalOrigin ) ) {
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
		yaw_speed = SVG_Nav_IsRequestPending( &pathNavigationState.process ) ? 10.0f : 15.0f;
		SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
	}

	return false;
}

/**
*	@brief	Enqueue a navigation rebuild request when the async queue is enabled.
*	@param	self	Monster owning the path process state.
*	@param	start_origin	Current feet-origin start position.
*	@param	goal_origin	Desired feet-origin goal position.
*	@param	policy	Path-follow policy tuning rebuild heuristics.
*	@param	agent_mins	Feet-origin agent bbox minimums.
*	@param	agent_maxs	Feet-origin agent bbox maximums.
*	@param	force	When true, bypass throttles/heuristics and rebuild immediately.
*	@return	True if the queue accepted the request or already had one pending.
*	@note	When this returns true the path process relies on the queued rebuild instead
*			of immediate synchronous execution so we do not spam blocking calls.
**/
const bool svg_monster_testdummy_debug_t::TryRebuildNavigationInQueue( const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, const bool force )
{
	/**
	*    @brief	Attempt to enqueue an asynchronous navigation rebuild for this entity.
	*    @note	Lightweight diagnostic logging is emitted when DUMMY_NAV_DEBUG is enabled
	*
	*    Guard: only enqueue when the async queue mode is explicitly enabled.
	**/
	if ( !nav_nav_async_queue_mode || nav_nav_async_queue_mode->integer == 0 ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: async queue mode disabled, cannot enqueue. ent=%d\n", s.number );
		}
		return false;
	}

	if ( !SVG_Nav_IsAsyncNavEnabled() ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: async nav globally disabled, ent=%d\n", s.number );
		}
		return false;
	}

	/**
	*    Throttle guard:
	*        If the path process is not allowed to rebuild yet, skip enqueuing and
	*        let callers keep following the current path without forcing sync rebuilds.
	**/
	// Force bypass ensures explicit breadcrumb goals always queue new work.
	if ( !force && !pathNavigationState.process.CanRebuild( policy ) ) {
		// Movement throttled/backoff prevents enqueuing now; callers should keep using current path.
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: CanRebuild() == false, throttled/backoff. ent=%d next_rebuild=%lld backoff_until=%lld\n",
				s.number, ( long long )pathNavigationState.process.next_rebuild_time.Milliseconds(), ( long long )pathNavigationState.process.backoff_until.Milliseconds() );
		}
		return true;
	}

	/**
	*    Movement heuristic:
	*        Only queue rebuilds when the path process thinks goal/start movement
	*        warrants it; this prevents re-queueing every frame for static goals.
	**/
	const bool movementWarrantsRebuild =
		pathNavigationState.process.ShouldRebuildForGoal2D( goal_origin, policy )
		|| pathNavigationState.process.ShouldRebuildForGoal3D( goal_origin, policy )
		|| pathNavigationState.process.ShouldRebuildForStart2D( start_origin, policy )
		|| pathNavigationState.process.ShouldRebuildForStart3D( start_origin, policy );
	// Force bypass ensures explicit breadcrumb goals bypass movement heuristics.
	if ( !force && !movementWarrantsRebuild ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: movement does not warrant rebuild, ent=%d\n", s.number );
		}
		return true;
	}

	/**
	*    Allow the request queue to deduplicate / refresh existing requests.
	*    Instead of bailing out early we call the enqueue helper which will
	*    refresh an existing entry or create a new one. This ensures the
	*    path_process pending handle is always up-to-date and avoids the
	*    repeated "request already pending" spam seen in logs.
	**/
	// If a request is already pending, allow the async queue helper to
	// deduplicate / refresh the existing entry rather than bailing out early.
	// This avoids the repeated "request already pending" spam and ensures
	// up-to-date goals get applied to the outstanding queued entry.
	if ( SVG_Nav_IsRequestPending( &pathNavigationState.process ) ) {
		if ( force ) {
			// When forced, cancel the outstanding request so a fresh entry is
			// created immediately with the force flag set.
			ResetNavigationPath();
		} else {
			// If a pending handle exists for this process and our movement
			// heuristics do not warrant a rebuild, skip refreshing the
			// in-flight request. This comparison is per-handle so callers
			// that intentionally replaced the pending handle are not blocked.
			if ( pathNavigationState.process.pending_request_handle != 0 && !movementWarrantsRebuild ) {
				if ( DUMMY_NAV_DEBUG ) {
					gi.dprintf( "[DEBUG] TryQueueNavRebuild: skipping refresh because pending_handle=%d and movement doesn't warrant it. ent=%d\n",
						pathNavigationState.process.pending_request_handle, s.number );
				}
				return true;
			}
		}
	}

	/**
	*  Stronger protection: when a request is already running (rebuild_in_progress)
	*  small changes to the start position should not trigger a refresh. The
	*  nav queue already defers re-init via `needs_refresh` but callers that
	*  drift the start each frame can still cause repeated worker prep. Here
	*  we ignore small start deltas while a request is running so the in-flight
	*  search can make progress.
	**/
	//if ( !force && pathNavigationState.process.rebuild_in_progress ) {
	//    // Use the last prep start as a stable reference if available.
	//    const Vector3 referenceStart = ( pathNavigationState.process.last_prep_time > 0_ms ) ? pathNavigationState.process.last_prep_start : pathNavigationState.process.path_start_position;
	//    const double startDx = QM_Vector3LengthDP( QM_Vector3Subtract( start_origin, referenceStart ) );
	//    // Threshold chosen to ignore small per-frame motion but still react to real relocation.
	//    constexpr double startIgnoreThreshold = 16.0; // units
	//    if ( startDx <= startIgnoreThreshold ) {
	//        if ( DUMMY_NAV_DEBUG ) {
	//            gi.dprintf( "[DEBUG] TryQueueNavRebuild: suppressed refresh while running (startDx=%.2f <= %.2f) ent=%d\n",
	//                startDx, startIgnoreThreshold, s.number );
	//        }
	//        return true;
	//    }
	//}

	/**
	*	Enqueue the rebuild request and record the handle for diagnostics.
	*	Note: If a request is already pending and actively being prepared / running
	*	(this entity's `pathNavigationState.process.rebuild_in_progress == true`), this helper
	*	will avoid refreshing the existing running entry unless `force == true`
	*	or the movement heuristics indicate a rebuild is warranted. This prevents
	*	repeated re-initialization of in-flight searches which can starve the
	*	incremental A* stepper and cause visible steering issues.
	**/
	// Propagate a small per-request start-ignore threshold to the async queue so
	// the queue can avoid re-preparing entries when the start drifts slightly
	// while a search is running. We choose the same 16-unit threshold used
	// locally above to keep behavior consistent.
	constexpr double startIgnoreThresholdForQueue = 16.0;
	const nav_request_handle_t handle = SVG_Nav_RequestPathAsync( &pathNavigationState.process, start_origin, goal_origin, policy, agent_mins, agent_maxs, force, startIgnoreThresholdForQueue );
	if ( handle <= 0 ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: enqueue failed (handle=%d) ent=%d\n", handle, s.number );
		}
		return false;
	}

	// Record that a rebuild is in progress for diagnostics and possible cancellation.
	// Note: the request queue will have already set the process markers during
	// PrepareAStarForEntry when the entry transitions to Running. Setting them
	// here ensures the entity has the handle immediately for early cancellation
	// if the caller chooses to abort before the queue tick processes it.
	pathNavigationState.process.rebuild_in_progress = true;
	pathNavigationState.process.pending_request_handle = handle;
	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf( "[DEBUG] TryQueueNavRebuild: queued rebuild handle=%d ent=%d force=%d\n", handle, s.number, force ? 1 : 0 );
		// Also print the converted nav-center origins so we can correlate node resolution.
		const nav_mesh_t *mesh = g_nav_mesh.get();
		if ( mesh ) {
			const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
			const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
				start_origin.x, start_origin.y, start_origin.z,
				start_center.x, start_center.y, start_center.z,
				goal_origin.x, goal_origin.y, goal_origin.z,
				goal_center.x, goal_center.y, goal_center.z );
		} else {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) (no mesh)\n",
				start_origin.x, start_origin.y, start_origin.z,
				goal_origin.x, goal_origin.y, goal_origin.z );
		}
	}
	return true;
}
/**
*    @brief	Reset cached navigation path state for the test dummy.
*    @param	self	Monster whose path state should be cleared.
*    @note	Cancels any queued async request and clears cached path buffers.
**/
void svg_monster_testdummy_debug_t::ResetNavigationPath() {
	/**
	*    Cancel any pending async request so we do not reuse stale results.
	**/
	if ( pathNavigationState.process.pending_request_handle > 0 ) {
		SVG_Nav_CancelRequest( pathNavigationState.process.pending_request_handle );
	}

	/**
	*    Clear cached path buffers and reset traversal bookkeeping.
	**/
	SVG_Nav_FreeTraversalPath( &pathNavigationState.process.path );
	pathNavigationState.process.path_index = 0;
	pathNavigationState.process.path_goal_position = {};
	pathNavigationState.process.path_start_position = {};
	pathNavigationState.process.rebuild_in_progress = false;
	pathNavigationState.process.pending_request_handle = 0;
}

/**
*	@brief	Member wrapper that forwards to the TU-local AdjustGoalZBlendPolicy helper.
*	@param	goalOrigin	World-space feet-origin goal position used to bias layer selection.
*	@note	Called each think after `GenericThinkBegin()` to keep `pathNavigationState.policy`
*			tuned to current pursuit conditions (distance, vertical delta, failures, visibility).
**/
void svg_monster_testdummy_debug_t::AdjustGoalZBlendPolicy( const Vector3 &goalOrigin ) {
	auto &pathPolicy = pathNavigationState.policy;
	auto &pathProcess = pathNavigationState.process;

	const double horizDist = std::sqrt( QM_Vector2DistanceSqr( goalOrigin, currentOrigin ) );
	const double deltaZ = ( double )goalOrigin.z - ( double )currentOrigin.z;
	const double absDz = std::fabs( deltaZ );

	// If we recently had failures, disable blending briefly to avoid repeating bad layer choices.
	const bool recentFailure = ( pathProcess.consecutive_failures > 0 ) && ( ( level.time - pathProcess.last_failure_time ) <= 2_sec );
	if ( recentFailure ) {
		pathPolicy.enable_goal_z_layer_blend = false;
		pathPolicy.blend_start_dist = 128.0;
		pathPolicy.blend_full_dist = 256.0;
		return;
	}

	// Thresholds and guards
	constexpr double kStairVerticalThreshold = 32.0; // world units
	constexpr double kMinBlendStart = 32.0;
	constexpr double kMinBlendFull = 64.0;

	const bool activatorVisible = ( activator != nullptr && SVG_Entity_IsVisible( this, activator ) );
	const double activatorDist2D = activator ? std::sqrt( QM_Vector2DistanceSqr( activator->currentOrigin, currentOrigin ) ) : DBL_MAX;
	const bool activatorNearby = ( activatorDist2D <= 64/*DUMMY_PLAYER_PURSUIT_MAX_DIST*/ );

	// If vertical difference is small, prefer no bias (stay on start layer).
	if ( absDz <= kStairVerticalThreshold ) {
		pathPolicy.enable_goal_z_layer_blend = false;
		pathPolicy.blend_start_dist = 64.0;
		pathPolicy.blend_full_dist = 128.0;
		return;
	}

	// If activator is visible/nearby, be conservative about switching layers.
	if ( activatorVisible || activatorNearby ) {
		pathPolicy.enable_goal_z_layer_blend = true;
		pathPolicy.blend_start_dist = std::clamp( horizDist * 0.25, kMinBlendStart, 256.0 );
		pathPolicy.blend_full_dist = std::clamp( horizDist * 0.5, kMinBlendFull, 384.0 );
		return;
	}

	// Otherwise, prefer enabling blending for upward goals so the pathfinder can climb stairs when appropriate.
	if ( deltaZ > 0. ) {
		pathPolicy.enable_goal_z_layer_blend = true;
		pathPolicy.blend_start_dist = std::clamp( horizDist - 64.0, kMinBlendStart, 256.0 );
		pathPolicy.blend_full_dist = std::clamp( std::max( horizDist * 0.25, 128.0 ), kMinBlendFull, 512.0 );
	} else {
		pathPolicy.enable_goal_z_layer_blend = false;
		pathPolicy.blend_start_dist = 64.0;
		pathPolicy.blend_full_dist = 128.0;
	}
}


//=============================================================================================
//=============================================================================================


/**
*
*
*
*
*	Explicit NPC State Management:
*
*
*
*
**/