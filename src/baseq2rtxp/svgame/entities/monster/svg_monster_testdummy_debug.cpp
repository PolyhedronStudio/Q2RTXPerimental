/********************************************************************
*
*    ServerGame: TestDummy Debug Monster Edict (A* only) - Implementation
*
********************************************************************/

#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_skeletal_hitboxes.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

// TODO: Move elsewhere.. ?
#include "refresh/shared_types.h"

// Entities
#include "sharedgame/sg_entities.h"

// KD-Tree pathfinding
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_path.h"

// SharedGame UseTargetHints.
#include "sharedgame/sg_usetarget_hints.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Player trail (Q2/Q2RTX pursuit trail)
#include "svgame/player/svg_player_trail.h"

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"

// Navigation
#include "svgame/nav/nav_path.h"

static constexpr QMTime PATH_RECALC_INTERVAL_MS = 250_ms;



//! Optional debug toggle for emitting async queue statistics.
extern cvar_t *s_nav_nav_async_log_stats;

// Local debug toggle for noisy per-frame prints in this test monster.
#ifndef DEBUG_PRINTS
#define DEBUG_PRINTS 0
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

//! Maximum allowed breadcrumb age before trail-follow is considered stale.
static constexpr QMTime DUMMY_TRAIL_MAX_AGE = 6_sec;
//! Maximum age of a sound event that can trigger investigation.
static constexpr QMTime DUMMY_SOUND_INVESTIGATE_MAX_AGE = 2400_ms;
//! Maximum horizontal distance for reacting to sound events.
static constexpr double DUMMY_SOUND_INVESTIGATE_MAX_DIST = 8192.0;//1536.0 * 2.0;
//! Arrival radius used for ending sound investigation behavior.
static constexpr double DUMMY_SOUND_INVESTIGATE_REACHED_DIST = 2.0;
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
	const bool requestPending = false;

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
		0,
		0,
		self->goalentity ? 1 : 0,
		(int32_t)self->navPath.size(),
		self->pathPos );
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

	// Server-side skeletal hitbox debug rendering was removed in favor of client-side overlays.

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
bool svg_monster_testdummy_debug_t::CheckForAudibleSounds() {
	svg_base_edict_t *foundAudibleEntity = nullptr;

	//! We want to react to the freshest event regardless of which slot it is in, so we compare timestamps to find the most recent.
	if ( SVG_Entity_IsActive( level.weapon_sound_entity ) && level.weapon_sound_entity->last_sound_time > this->stateSoundCan.lastTime ) {
		// Start with sound_entity as the freshest sound if it is newer than our last processed sound time.
		foundAudibleEntity = level.weapon_sound_entity;
	}
	// If impact_sound_entity is even newer, use that instead.
	if ( SVG_Entity_IsActive( level.impact_sound_entity ) && level.impact_sound_entity->last_sound_time > this->stateSoundCan.lastTime ) {
		// If we don't have a fresh sound yet, or if sound2_entity is newer than the current freshest sound, use sound2_entity.
		if ( !foundAudibleEntity 
			|| ( foundAudibleEntity && ( level.impact_sound_entity && level.impact_sound_entity->last_sound_time > foundAudibleEntity->last_sound_time ) ) ) {
			// sound2_entity is fresher than sound_entity, so use sound2_entity as the freshest sound.
			foundAudibleEntity = level.impact_sound_entity;
		}
	}
	// If personal_sound_entity is even newer, use that instead.
	if ( SVG_Entity_IsActive( level.personal_sound_entity ) && level.personal_sound_entity->last_sound_time > this->stateSoundCan.lastTime ) {
		// If we don't have a fresh sound yet, or if sound2_entity is newer than the current freshest sound, use sound2_entity.
		if ( !foundAudibleEntity
			|| ( foundAudibleEntity && ( level.personal_sound_entity && level.personal_sound_entity->last_sound_time > foundAudibleEntity->last_sound_time ) ) ) {
			// sound2_entity is fresher than sound_entity, so use sound2_entity as the freshest sound.
			foundAudibleEntity = level.personal_sound_entity;
		}
	}

	svg_base_edict_t *audibleEntity = ( foundAudibleEntity && SVG_Util_IsEntityAudibleByPHS( this, foundAudibleEntity, true, DUMMY_NAV_DEBUG ) ) ? foundAudibleEntity : nullptr;

	if ( audibleEntity ) {
		const QMTime soundAge = level.time - audibleEntity->last_sound_time;
		const double soundDist3D = std::sqrt( QM_Vector3DistanceSqr( audibleEntity->currentOrigin, this->currentOrigin ) );
		
		if ( soundAge <= DUMMY_SOUND_INVESTIGATE_MAX_AGE && soundDist3D <= DUMMY_SOUND_INVESTIGATE_MAX_DIST ) {
			this->stateSoundCan.origin = audibleEntity->currentOrigin;
			this->stateSoundCan.hasOrigin = true;
			this->stateSoundCan.lastTime = audibleEntity->last_sound_time;
			this->ResetNavigationPath();
			Dummy_SetState( this, svg_monster_testdummy_debug_t::AIThinkState::InvestigateSound );
			this->nextthink = level.time + FRAME_TIME_MS;
			return true;
		}
	}
	return false;
}

/**
*	@brief	A* specific thinker: always attempt async A* to activator if present(and if it goes LOS, sets think to onThink_AStarPursuitTrail.), otherwise go idle.
*
*	@details	Will always check for player presence first, and if not present will check for trail presence.
*				If player is present, will attempt async A* to player.
*				If trail is present, sets nextThink to onThink_AStarPursuitTrail and attempt async A* to the trail's last known position.
*				If neither are present, will set nextThink to onThink_Idle.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarToPlayer )( svg_monster_testdummy_debug_t *self ) -> void {
    if ( !self->GenericThinkBegin() ) return;
    
    if ( !self->isActivated ) {
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    if ( !self->activator ) {
        self->stateNavigationTrail.targetEntity = nullptr;
        self->goalentity = nullptr;
        self->ResetNavigationPath();
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    const bool activatorVisible = SVG_Entity_IsVisible( self, self->activator );
    if ( activatorVisible ) {
        self->lastPlayerVisibleTime = level.time;
    }

    /**
    *    Unconditionally use A* to pathfind to the activator's current origin.
    *    Since we have a NavMesh, we don't need line-of-sight or breadcrumbs to follow the player!
    **/
    if ( self->goalentity != self->activator ) {
        self->goalentity = self->activator;
        self->stateNavigationTrail.targetEntity = nullptr;
        self->ResetNavigationPath();
        self->stateNavigationTrail.trailTimeStamp = level.time;
    }
    
    const double dist2d = std::sqrt( QM_Vector2DistanceSqr( self->activator->currentOrigin, self->currentOrigin ) );
    const double distZ = std::abs( self->activator->currentOrigin.z - self->currentOrigin.z );
    // Player is 16 radius, monster is 16 radius. 32 is exact touch.
    // Also enforce a reasonable Z height difference (e.g. 48 units, enough for jumping/stairs but not balconies)
    const bool physicallyTouching = ( dist2d <= 40.0 ) && ( distZ < 48.0 );

    // Check for audible sounds before we move (optional realism step). Ignore if touching.
    if ( !activatorVisible && !physicallyTouching && self->CheckForAudibleSounds() ) {
        return;
    }

    // If we can see the player and are in attack range (both 2D and Z), OR if we are physically touching them (bypassing strict LOS), halt and attack/face.
    if ( physicallyTouching || ( activatorVisible && dist2d < 64.0 && distZ < 48.0 ) ) {
        self->velocity.x = 0;
        self->velocity.y = 0;
        self->monsterMove.state.velocity.x = 0;
        self->monsterMove.state.velocity.y = 0;
        
        Vector3 dir = QM_Vector3Subtract( self->activator->currentOrigin, self->currentOrigin );
        dir.z = 0;
        if ( QM_Vector3LengthSqr(dir) > 0.001f ) {
            self->ideal_yaw = QM_Vector3ToYaw( dir );
            const double currentYaw = QM_AngleMod( self->currentAngles[ YAW ] );
            const double yawDeltaAbs = std::fabs( QM_AngleDelta( self->ideal_yaw, currentYaw ) );
            // Distance to player influences turn aggressiveness.
            float dist = std::sqrt( QM_Vector2DistanceSqr( self->activator->currentOrigin, self->currentOrigin ) );
            float distFactor = std::clamp( dist / 200.0f, 0.5f, 1.5f );
            self->yaw_speed = (float)QM_Clamp( 12.0 + ( yawDeltaAbs * 0.12f * distFactor ), 8.0, 50.0 );
            SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );
        }
    } else {
        // Direct A* pursuit to player's current origin!
        // MoveAStarToOrigin properly sets the ideal_yaw and velocity to follow the path.
        // We DO NOT override ideal_yaw here anymore, so the monster will actually look where it's going!
        self->MoveAStarToOrigin( self->activator->currentOrigin );
    }

    int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
    self->GenericThinkFinish( true, blockedMask );
    SVG_Util_SetEntityAngles( self, self->currentAngles, true );

    if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
        // We shouldn't clear navPath here because slide movement normally scrapes walls.
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
    if ( !self->GenericThinkBegin() ) return;

    if ( !self->isActivated ) {
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    if ( self->activator && SVG_Entity_IsVisible( self, self->activator ) ) {
        self->stateNavigationTrail.targetEntity = nullptr;
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    if ( self->CheckForAudibleSounds() ) {
        return;
    }

    svg_base_edict_t *spot = self->stateNavigationTrail.targetEntity;
    if ( !spot || ( level.time - spot->timestamp ) > DUMMY_TRAIL_MAX_AGE ) {
        spot = PlayerTrail_PickFirst( (self) );
        self->stateNavigationTrail.targetEntity = spot;
        if ( spot ) {
            self->stateNavigationTrail.trailTimeStamp = spot->timestamp;
        }
    }

    if ( spot ) {
        double dist2D = std::sqrt( QM_Vector2DistanceSqr( spot->currentOrigin, self->currentOrigin ) );
        if ( dist2D <= DUMMY_SOUND_INVESTIGATE_REACHED_DIST * DUMMY_SOUND_INVESTIGATE_REACHED_DIST ) { // Reached breadcrumb
            spot = PlayerTrail_PickNext( (self) );
            self->stateNavigationTrail.targetEntity = spot;
            if ( spot ) {
                self->stateNavigationTrail.trailTimeStamp = spot->timestamp;
            }
        }
    }

    if ( spot ) {
        self->goalentity = spot;
        self->MoveAStarToOrigin( spot->currentOrigin );
    } else {
        self->stateNavigationTrail.targetEntity = nullptr;
        self->goalentity = nullptr;
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
    }

    int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
    self->GenericThinkFinish( true, blockedMask );
    SVG_Util_SetEntityAngles( self, self->currentAngles, true );

    if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
        // We shouldn't clear navPath here because slide movement normally scrapes walls.
		// Set angles perpendicular to the wall facing normal
		//const Vector3 wallNormal = QM_Vector3Normalize( self->currentAngles );
		//const Vector3 perpendicularDirection = QM_Vector3CrossProduct( wallNormal, Vector3{ 0, 0, 1 } );
		//SVG_Util_SetEntityAngles( self, QM_Vector3ToAngles( perpendicularDirection ), true );
    }

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
    // If we are close enough to the sound origin, consider it investigated but continue listening for new sounds.
    if ( soundDist3DSqr <= ( DUMMY_SOUND_INVESTIGATE_REACHED_DIST * DUMMY_SOUND_INVESTIGATE_REACHED_DIST ) ) {
        // Keep sound state active to allow detection of new sound events.
        // Remain in InvestigateSound state; no state transition.
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
	*	Always look for the player(activator) so we can react immediately when they appear,
	*   or pursue them continuously if we are activated.
	**/
    if ( self->activator ) {
		// Immediate action.
		self->goalentity = self->activator;
		// Set the nextThink to AStarToPlayer so we start chasing the player right away using the navmesh.
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
		self->nextthink = level.time + FRAME_TIME_MS;
		// Skip all other idle logic if we have an activator to pursue.
		return;
	}

	/**
	*   Sound-investigation acquisition:
	*       If a fresh noise event was emitted nearby, investigate that position.
	**/
	if ( self->CheckForAudibleSounds() ) {
		return;
	}

	/**
	*   Idle lookout behavior:
	*       Sweep yaw while waiting so we periodically look for reacquisition.
	**/
    // Idle scan: step among 8 fixed headings (0..315 in 45deg increments) but
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
	*    Recategorize position and check grounding.
	**/
	RecategorizeGroundAndLiquidState();

	// Sync physics state with entity state (origin may have changed during grounding/snapping).
	monsterMove.state.origin = currentOrigin;
	monsterMove.state.velocity = velocity;
	monsterMove.ground = groundInfo;
	monsterMove.liquid = liquidInfo;

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

	// For storing the results of the slide move.
	nav_path_policy_t pathPolicy = {};
	pathPolicy.max_step_height = NAV_MAX_STEP_SIZE;
	pathPolicy.max_drop_height = PM_DROPOFF_MAX_SIZE;
	pathPolicy.enable_max_drop_height_cap = true;
	pathPolicy.max_drop_height_cap = PM_DROPOFF_ALLOWED_SIZE;
	monsterMove.navPolicy = &pathPolicy;
	// Perform the slide move and get the blocked mask describing the result of the movement attempt.
	const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove, pathPolicy );

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

	// We must ensure that we aren't completely trapped by a dynamic mesh rebuild.
	// Since we are decoupling Nav2, mesh checks are no longer needed here.

	// Third priority: entity-defined bounds as a fallback to ensure we always have some kind of valid bounds to work with.
	// Use the entity's mins and maxs, which should always be valid for a properly initialized entity.
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
	*   Determine whether there is any async state worth tearing down.
	**/	const bool hadPendingState = false;

	if ( DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "[NAV DEBUG] %s: Canceling all pending paths/requests (hadPending=%d).\n",
			__func__, hadPendingState ? 1 : 0 );
	}

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

static int32_t Dummy_FindClosestFaceInLeaf( const Vector3 &point ) {
    // We bypass the KD-Tree here because the NavMesh KD-Tree generation groups faces strictly by their centroid.
    // If a face crosses a KD-Tree splitting plane, it only exists in the leaf node matching its centroid.
    // If the entity is standing on the face but on the opposite side of the splitting plane, 
    // Nav_FindLeafNode returns a leaf that does NOT contain the face, causing the entity to lose its path!
    // Since the navmesh is small, a global search is effectively instantaneous and much more robust.
    return Nav_FindClosestPolyGlobal( point );
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
    const double t = level.time.Seconds<double>();
    const int32_t animFrameGlobal = ( int32_t )std::floor( ( float )( t * 40.0f ) );
    
    auto UpdateAnim = [&]( int32_t rootMotionIndex ) {
        if ( this->rootMotionSet && this->rootMotionSet->motions[ rootMotionIndex ] ) {
            skm_rootmotion_t *rootMotion = this->rootMotionSet->motions[ rootMotionIndex ];
            const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrameGlobal % rootMotion->frameCount ) : 0;
            this->s.frame = rootMotion->firstFrameIndex + localFrame;
        }
    };

    if ( !ComputePathTo( goalOrigin ) ) {
        // Direct fallback removed. Halt!
        velocity.x = velocity.y = 0.0f;
        monsterMove.state.velocity.x = velocity.x;
        monsterMove.state.velocity.y = velocity.y;
        UpdateAnim( 1 ); // IDLE

        // Face the goal even if pathing failed
        Vector3 toGoal = QM_Vector3Subtract( goalOrigin, currentOrigin );
        toGoal.z = 0.0f;
        if ( QM_Vector3LengthSqr( toGoal ) > 0.0001f ) {
            Vector3 moveDir = QM_Vector3Normalize( toGoal );
            ideal_yaw = QM_Vector3ToYaw( moveDir );
            const double currentYaw = QM_AngleMod( currentAngles[ YAW ] );
            const double yawDeltaAbs = std::fabs( QM_AngleDelta( ideal_yaw, currentYaw ) );
            yaw_speed = ( float )QM_Clamp( 16.0 + (yawDeltaAbs * 0.10), 10.0, 45.0 );
            SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
        }

        return false;
    }

    Vector3 nextWay = NextWaypoint( goalOrigin );
    Vector3 toGoal = QM_Vector3Subtract( nextWay, currentOrigin );
    toGoal.z = 0.0f;
    if ( QM_Vector3LengthSqr( toGoal ) < 0.0001f ) {
        velocity.x = velocity.y = 0.0f;
        monsterMove.state.velocity.x = velocity.x;
        monsterMove.state.velocity.y = velocity.y;
        UpdateAnim( 1 ); // IDLE
        return false;
    }

    Vector3 moveDir = QM_Vector3Normalize( toGoal );
    ideal_yaw = QM_Vector3ToYaw( moveDir );
    const double currentYaw = QM_AngleMod( currentAngles[ YAW ] );
    const double yawDeltaAbs = std::fabs( QM_AngleDelta( ideal_yaw, currentYaw ) );
    yaw_speed = ( float )QM_Clamp( 16.0 + (yawDeltaAbs * 0.10), 10.0, 45.0 );
    SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );

    constexpr double frameVelocity = 220.0;
    velocity.x = ( float )( moveDir.x * frameVelocity );
    velocity.y = ( float )( moveDir.y * frameVelocity );
    monsterMove.state.velocity.x = velocity.x;
    monsterMove.state.velocity.y = velocity.y;
    
    UpdateAnim( 4 ); // RUN
    
    return true;
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
/**
*    @brief	Reset cached navigation path state for the test dummy.
*    @param	self	Monster whose path state should be cleared.
*    @note	Cancels any queued async request and clears cached path buffers.
**/
void svg_monster_testdummy_debug_t::ResetNavigationPath() {
    navPath.clear();
    pathPos = 0;
    cachedLeaf = -1;
    cachedPoly = -1;
}

/**
*	@brief	Member wrapper that forwards to the TU-local AdjustGoalZBlendPolicy helper.
*	@param	goalOrigin	World-space feet-origin goal position used to bias layer selection.
*	@note	Called each think after `GenericThinkBegin()` to keep `pathNavigationState.policy`
*			tuned to current pursuit conditions (distance, vertical delta, failures, visibility).
**/



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
/**
*    @brief    Check if the path should be recalculated based on distance and time.
**/
const bool svg_monster_testdummy_debug_t::ShouldRecalcPath( const Vector3 &pos ) {
    const uint64_t now = level.time.Milliseconds();
    if ( now - lastPathCalcTime.Milliseconds() > PATH_RECALC_INTERVAL_MS.Milliseconds() ) return true;
    return false;
}



/**
*    @brief    Compute an A* path to the target origin.
**/
const bool svg_monster_testdummy_debug_t::ComputePathTo( const Vector3 &target ) {
    // Offset origins to feet level. The monster's center is 24-36 units in the air, which can cause
    // the system to mistakenly snap to a higher stair step if the monster is near a stair riser!
    Vector3 myFeet = currentOrigin;
    myFeet.z += this->mins.z;
    
    // The target is also a center origin (e.g. player). Subtract 24 units as a solid 
    // approximation of the distance from the bounding box center to the floor.
    Vector3 targetFeet = target;
    targetFeet.z -= 24.0f;

    int32_t startFace = Dummy_FindClosestFaceInLeaf( myFeet );
    int32_t goalFace = Dummy_FindClosestFaceInLeaf( targetFeet );
    
    if ( startFace == -1 || goalFace == -1 ) {
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf("ComputePathTo: failed to find face! startFace=%" PRId32 ", goalFace=%" PRId32 "\n", startFace, goalFace);
        }
        navPath.clear();
        lastPathCalcTime = level.time; // Debounce even on complete failure
        return false; // BUGFIX: Halt if we can't find a valid starting or goal face, to prevent blind suicide steering.
    }

    if ( !navPath.empty() ) {
        // If we already have a path, see if it is still perfectly valid.
        // We consider it valid if the monster hasn't wandered completely off the path.
        bool stillOnPath = false;
        // Check if our current physical face is anywhere near us in the path (including slightly behind)
        // We must check slightly behind pathPos because NextWaypoint advances pathPos BEFORE the monster's 
        // physical center crosses the portal boundary (due to advanceDist).
        size_t searchStart = std::max<size_t>( 0, pathPos - 3 );
        for ( size_t i = searchStart; i < navPath.size(); ++i ) {
            if ( navPath[i] == startFace ) {
                stillOnPath = true;
                // If we advanced faces physically, gently push pathPos forward so we don't backtrack!
                // Notice we do NOT push it backward if we match a face behind pathPos.
                if ( (int32_t)i > pathPos ) {
                    pathPos = static_cast<int32_t>( i );
                }
                break;
            }
        }
        
        if ( stillOnPath ) {
            // The monster is still on the path. Throttle recalculations heavily to prevent
            // A* ping-ponging when the player crosses small KD-Tree node boundaries.
            const uint64_t now = level.time.Milliseconds();
            if ( goalFace == navPath.back() ) {
                // Goal is in the exact same destination face. Hold path for up to 2 seconds.
                if ( now - lastPathCalcTime.Milliseconds() < 2000 ) {
                    return true;
                }
            } else {
                // Goal has moved to a different face. Hold path for up to 1 second
                // to prevent twitching when the player just crosses a boundary.
                if ( now - lastPathCalcTime.Milliseconds() < 1000 ) {
                    return true;
                }
            }
        }
    }

    if ( !ShouldRecalcPath( currentOrigin ) ) {
        // Throttle rapid recalculations when the path is invalid or empty.
        return !navPath.empty(); 
    }
    
    if ( Nav_FindPath( startFace, goalFace, navPath, pathNavigationState.policy ) ) {
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf("[NAV DEBUG] Path found: %zu nodes - ", navPath.size());
            for (int32_t idx : navPath) {
                gi.dprintf("%d ", idx);
            }
            gi.dprintf("\n");
        }
        pathPos = 0;
        lastPathCalcTime = level.time;
        return true;
    }
    
    if ( DUMMY_NAV_DEBUG ) {
        gi.dprintf("ComputePathTo: Nav_FindPath failed from %d to %d\n", startFace, goalFace);
    }
    navPath.clear();
    lastPathCalcTime = level.time; // Prevent spamming failed paths every frame
    // BUGFIX: Return false when path completely fails so the monster halts instead of suiciding off a cliff!
    return false; 
}

/**
*    @brief    Get the next waypoint from the navigation path, defaulting to finalGoal when finished.
**/
const Vector3 svg_monster_testdummy_debug_t::NextWaypoint( const Vector3 &finalGoal ) {
    // If we have exhausted the path, head directly to the final goal.
    if ( pathPos >= navPath.size() )
        return finalGoal;

    // If we are on the last face, evaluate the final goal as the waypoint.
    if ( pathPos == static_cast<int32_t>( navPath.size() ) - 1 ) {
        if ( QM_Vector3DistanceSqr( currentOrigin, finalGoal ) < WAYPOINT_EPS_SQR ) {
            ++pathPos;
            return finalGoal;
        }
        return finalGoal;
    }

    // Determine TRUE portal midpoint between current and next face.
    Vector3 portalMidpoint;
    Vector3 edgeVec = { 0.0f, 0.0f, 0.0f };
    Vector3 v0, v1;
    bool foundPortal = false;
    float portal_z_diff = 0.0f;
    
    if ( !Nav_GetPortalEndpoints( navPath[pathPos], navPath[pathPos + 1], &v0, &v1 ) ) {
        // Fallback: use next face center.
        portalMidpoint = g_nav_faces[ navPath[pathPos + 1] ].center;
    } else {
        foundPortal = true;
        
        // Extract z_diff to know the true height of the step if we are approaching a stair
        const nav_face_t &faceA = g_nav_faces[ navPath[pathPos] ];
        for ( int e = 0; e < faceA.num_edges; e++ ) {
            const nav_halfedge_t &he = g_nav_halfedges[ faceA.first_edge_idx + e ];
            if ( he.twin_idx != -1 && g_nav_halfedges[ he.twin_idx ].face_idx == navPath[pathPos + 1] ) {
                portal_z_diff = he.z_diff;
                break;
            }
        }
        
        edgeVec = QM_Vector3Subtract( v1, v0 );
        
        // Closest point on segment
        Vector3 ab = edgeVec;
        Vector3 ap = QM_Vector3Subtract( currentOrigin, v0 );
        
        ab.z = 0.0f;
        ap.z = 0.0f;
        
        float ab_len2 = static_cast<float>(QM_Vector3DotProduct(ab, ab));
        float ab_len = std::sqrt(ab_len2);
        if (ab_len > 0.0001f) {
            float t = static_cast<float>(QM_Vector3DotProduct(ap, ab)) / ab_len2;
            
            // Dynamic clearance for wall scraping:
            // Ensure the steer point is at least 'clearance' units away from the actual corner/vertex
            // so the bounding box doesn't scrape or clip into the physical wall corner.
            // A 32x32 bounding box has a radius of 16, but we need extra padding to avoid scraping
            // inner stair corners where the higher step might block the climb.
            float clearance = 24.0f;
            float clearance_t = clearance / ab_len;
            
            // If the portal itself is narrower than 2x clearance, just aim for the geometric center.
            if (clearance_t * 2.0f >= 1.0f) {
                t = 0.5f;
            } else {
                t = QM_Clamp(t, clearance_t, 1.0f - clearance_t);
            }
            
            portalMidpoint = QM_Vector3Add(v0, QM_Vector3Scale(QM_Vector3Subtract(v1, v0), t));
        } else {
            portalMidpoint = v0;
        }
    }

    // Advance to next polygon once we are sufficiently close to the TRUE portal.
    const float dist2DSqr = QM_Vector2DistanceSqr( currentOrigin, portalMidpoint );
    
    // Calculate the physical feet origin of the monster (since currentOrigin is the center of the bounding box).
    float feetOriginZ = currentOrigin.z + this->mins.z;

    // Determine if the monster needs to step up to reach the next polygon.
    // We MUST use the portal's physical Z height (the lip of the step).
    // Note: Nav_GetPortalEndpoints already sets portalMidpoint.z to the maxZ of the two faces,
    // so we don't need to add portal_z_diff again (which would double the height and cause moonwalking!).
    float targetZ = portalMidpoint.z;
    
    bool needsStepUp = (targetZ - feetOriginZ) > 8.0f;
    
    // Has the monster successfully completed the step up?
    bool hasSteppedUp = !needsStepUp || (feetOriginZ >= targetZ - 4.0f);

    // If we haven't stepped up yet, we must wait until we physically climb it.
    // Use a tight advance distance to ensure we don't cut corners on small landings (e.g., 32x32).
    // The monster is physically driven through the portal by the 32-unit push, so we rely 
    // heavily on the plane-crossing check above, or this tight distance fallback.
    float advanceDist = 12.0f; 
    
    if ( dist2DSqr < (advanceDist * advanceDist) ) {
        // Only advance if we have actually completed the vertical step, or if it's flat/downhill.
        if ( hasSteppedUp || (feetOriginZ > portalMidpoint.z + NAV_MAX_STEP_SIZE) ) {
            ++pathPos;
            return NextWaypoint( finalGoal );
        }
    }

    // Secondary plane-crossing check using TRUE portal: if we have moved past the portal plane relative to the path direction.
    // We only allow this if we don't have a pending physical vertical step up to perform.
    if ( hasSteppedUp && (edgeVec.x != 0.0f || edgeVec.y != 0.0f || edgeVec.z != 0.0f) ) {
        Vector3 toWaypoint = QM_Vector3Subtract( portalMidpoint, currentOrigin );
        toWaypoint.z = 0.0f; // 2D approach vector
        Vector3 expectedDir = QM_Vector3Subtract( g_nav_faces[ navPath[pathPos + 1] ].center, portalMidpoint );
        expectedDir.z = 0.0f;
        
        if ( QM_Vector3LengthSqr(expectedDir) > 0.001f && QM_Vector3LengthSqr(toWaypoint) > 0.001f ) {
            float dot = QM_Vector3DotProduct( QM_Vector3Normalize(toWaypoint), QM_Vector3Normalize(expectedDir) );
            if ( dot < 0.0f ) {
                ++pathPos;
                return NextWaypoint( finalGoal );
            }
        }
    }
    
    // PUSH THE WAYPOINT:
    // If the monster targets the EXACT portal edge, it will oscillate or stop once it reaches the edge
    // if it hasn't stepped up yet, preventing SV_WalkMove from initiating the step up.
    // To fix this, we push the returned waypoint INTO the next polygon so it has a confident forward driving vector!
    // Pushing towards the NEXT polygon's center (expectedDir) naturally steers the monster around L-turns seamlessly.
    if ( foundPortal && pathPos + 1 < navPath.size() ) {
        Vector3 expectedDir = QM_Vector3Subtract( g_nav_faces[ navPath[pathPos + 1] ].center, portalMidpoint );
        expectedDir.z = 0.0f; // Keep the push horizontal
        float expLen = QM_Vector3Length(expectedDir);
        if (expLen > 0.001f) {
            expectedDir = QM_Vector3Scale(expectedDir, 1.0f / expLen);
            // Cap the push distance to the distance to the next center to avoid pushing through far walls.
            // BUGFIX: If we have NOT stepped up yet, limit the push distance so we don't steer into the 
            // SECOND stair step too early and clip the corner!
            float pushDist = hasSteppedUp ? std::min(32.0f, expLen) : std::min(12.0f, expLen);
            return QM_Vector3Add(portalMidpoint, QM_Vector3Scale(expectedDir, pushDist));
        }
    }

    return portalMidpoint;
}
