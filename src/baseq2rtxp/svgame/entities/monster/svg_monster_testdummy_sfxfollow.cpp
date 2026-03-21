/********************************************************************
*
*    ServerGame: TestDummy Sound-Follow NPC Edict - Implementation
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

// Reusable NPC sound helpers.
#include "svgame/entities/svg_npc_sound_helper.h"

// Legacy movement policy definition is still required locally while step-slide consumes the older policy shape.
#include "svgame/nav2/nav2_types.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Removed oldnav headers to keep the testdummy on nav2 query interfaces.
// Keeping the following includes for nav2 functionality.
#include "svgame/nav2/nav2_query_iface.h"
#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_policy.h"

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy_sfxfollow.h"

#include <algorithm>


//! Optional debug toggle for emitting async queue statistics.
extern cvar_t *s_nav_nav_async_log_stats;

// Local debug toggle for noisy per-frame prints in this test monster.
#ifndef DEBUG_PRINTS
#define DEBUG_PRINTS 1
#endif

/**
*    Debug compile-time toggle for route-filter isolation.
*        When enabled, this sound-follow NPC disables the coarse cluster tile-route
*        restriction so A* neighbor diagnostics reflect pure StepTest behavior.
**/
//#define MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER 1
#ifdef MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER
#define MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER 1
#endif

#define DEBUG_PRINTS 1
#ifdef DEBUG_PRINTS
static constexpr int32_t DUMMY_NAV_DEBUG = 1;
#else
static constexpr int32_t DUMMY_NAV_DEBUG = 0;
#endif

static QMTime s_dummy_nav_debug_next_log_time = 0_ms;

/**
*	@brief	Forward declaration for the staged Task 3.2 corridor seam helper.
*	@param	self		Monster requesting the corridor seam.
*	@param	startOrigin	Current feet-origin start point.
*	@param	goalOrigin	Resolved feet-origin goal point.
*	@return	True when the debug corridor seam built successfully.
**/
static bool Dummy_TryBuildDebugCorridor( svg_monster_testdummy_sfxfollow_t *self, const Vector3 &startOrigin, const Vector3 &goalOrigin );

/**
*	@brief	Forward declaration for the local rate-limited nav debug logger gate.
*	@return	True when the caller may emit another debug log this frame window.
**/
static bool Dummy_ShouldEmitNavDebugLog( void );

/**
*	@brief	Build and optionally debug-print a staged nav2 corridor for the current sound-follow request.
*	@param	self		Monster requesting the corridor seam.
*	@param	startOrigin	Current feet-origin start point.
*	@param	goalOrigin	Resolved feet-origin goal point.
*	@return	True when the legacy coarse builder produced a corridor mirrored into nav2 types.
*	@note	This Task 3.2 seam intentionally leaves oldnav refinement behavior untouched while exposing
*			explicit corridor commitments for later fine-search adoption and debug visualization.
**/
static bool Dummy_TryBuildDebugCorridor( svg_monster_testdummy_sfxfollow_t *self, const Vector3 &startOrigin, const Vector3 &goalOrigin ) {
	/**
	*	Sanity checks: require a valid entity and active query mesh before attempting corridor generation.
	**/
	if ( !self ) {
		return false;
	}
	const nav2_query_mesh_t *mesh = SVG_Nav2_GetQueryMesh();
	if ( !mesh ) {
		return false;
	}

	/**
	*	Resolve the monster's agent bounds so corridor endpoint lookup matches the current query seam.
	**/
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};
	self->GetNavigationAgentBounds( &agent_mins, &agent_maxs );
	if ( !( agent_maxs.x > agent_mins.x ) || !( agent_maxs.y > agent_mins.y ) || !( agent_maxs.z > agent_mins.z ) ) {
		return false;
	}

	/**
	*	Build the explicit nav2 corridor through the low-risk legacy adapter seam.
	**/
	nav2_corridor_t corridor = {};
	if ( !SVG_Nav2_BuildCorridorForEndpoints( mesh, startOrigin, goalOrigin, &self->pathNavigationState.policy, agent_mins, agent_maxs, &corridor ) ) {
		return false;
	}

	/**
	*	Keep corridor diagnostics rate-limited and opt-in so Task 3.2 does not reintroduce log spam.
	**/
	if ( DUMMY_NAV_DEBUG != 0 && Dummy_ShouldEmitNavDebugLog() ) {
		SVG_Nav2_DebugPrintCorridor( corridor );
	}
	return true;
}

static bool Dummy_ShouldEmitNavDebugLog( void ) {
	const QMTime now = level.time;
	if ( now >= s_dummy_nav_debug_next_log_time ) {
		s_dummy_nav_debug_next_log_time = now + 200_ms;
		return true;
	}
	return false;
}

#define DUMMY_NAV_DEBUG_PRINT( ... ) \
	do { \
		if ( DUMMY_NAV_DEBUG != 0 && Dummy_ShouldEmitNavDebugLog() ) { \
			gi.dprintf( __VA_ARGS__ ); \
		} \
	} while ( 0 )

//! Maximum age of a sound event that can trigger investigation.
static constexpr QMTime DUMMY_SOUND_INVESTIGATE_MAX_AGE = 24_sec;
//! Arrival radius used for ending sound investigation behavior.
static constexpr double DUMMY_SOUND_INVESTIGATE_REACHED_DIST = 4.0;
//! Idle yaw scan step in degrees per think.
static constexpr double DUMMY_IDLE_SCAN_STEP_DEG = 45.0;
//! Interval for flipping idle scan yaw direction.
static constexpr QMTime DUMMY_IDLE_SCAN_FLIP_INTERVAL = 500_ms;

/**
*   @brief	Determine whether a refreshed sound target should hard-reset the current async nav request.
*   @param	self		Sound-follow testdummy evaluating the refreshed sound.
*   @param	newGoalOrigin	World-space sound origin proposed by the latest audible event.
*   @return	True when the new sound goal moved far enough that the running async request should be cancelled.
*   @note	This prevents identical or near-identical sound refreshes from starving the nav queue by repeatedly
*   			cancelling in-flight work before `pathOk=1` can ever appear.
**/
static bool Dummy_ShouldResetSoundInvestigationGoal( const svg_monster_testdummy_sfxfollow_t *self, const Vector3 &newGoalOrigin ) {
	/**
	*   Sanity checks: without an entity or an existing cached target we must treat the update as a hard refresh.
	**/
	if ( !self || !self->stateSoundCan.hasOrigin ) {
		return true;
	}

	/**
	*   Reuse the caller's rebuild thresholds so sound-follow goal refreshes stay aligned with the nav API.
	**/
 const nav2_query_policy_t &policy = self->pathNavigationState.policy;
	const double rebuildGoal2D = std::max( policy.rebuild_goal_dist_2d, 32.0 );
	const double rebuildGoal3D = ( policy.rebuild_goal_dist_3d > 0.0 )
		? std::max( policy.rebuild_goal_dist_3d, rebuildGoal2D )
		: 0.0;

	/**
	*   Measure how far the refreshed sound goal moved relative to the currently investigated origin.
	**/
	const Vector3 goalDelta = QM_Vector3Subtract( newGoalOrigin, self->stateSoundCan.origin );
	const double goalDelta2DSqr = ( goalDelta.x * goalDelta.x ) + ( goalDelta.y * goalDelta.y );
	const double goalDelta3DSqr = goalDelta2DSqr + ( goalDelta.z * goalDelta.z );

	/**
	*   Hard-reset only when the refreshed sound moved enough to warrant replacing the current async search.
	**/
	if ( goalDelta2DSqr > ( rebuildGoal2D * rebuildGoal2D ) ) {
		return true;
	}
	if ( std::abs( rebuildGoal3D ) > 0.0 && goalDelta3DSqr > ( rebuildGoal3D * rebuildGoal3D ) ) {
		return true;
	}

	/**
	*   Stable same-goal refresh: keep the running request alive and let the nav queue continue its work.
	**/
	return false;
}

/**
*   @brief	Project a sound-follow goal onto the nearest walkable nav Z while preserving XY.
*   @param	self		Monster requesting the projection.
*   @param	goalOrigin	Feet-origin goal proposed by sound investigation.
*   @param	outGoalOrigin	[out] Projected feet-origin goal when a walkable layer is found.
*   @return	True when the goal was projected onto a walkable nav layer.
*   @note	This keeps the testdummy's reusable movement API working with world-space sound goals.
**/
static bool Dummy_TryProjectGoalToWalkableZ( svg_monster_testdummy_sfxfollow_t *self, const Vector3 &goalOrigin, Vector3 *outGoalOrigin ) {
	/**
	*   Sanity checks: require an entity, navmesh, and output storage.
	**/
	if ( !self || !outGoalOrigin ) {
		return false;
	}

  const nav2_query_mesh_t *mesh = SVG_Nav2_GetQueryMesh();
	if ( !mesh ) {
		return false;
	}

	/**
	*   Resolve the navigation agent hull used by this monster for feet-to-center conversion.
	**/
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};
	self->GetNavigationAgentBounds( &agent_mins, &agent_maxs );
	if ( !( agent_maxs.x > agent_mins.x ) || !( agent_maxs.y > agent_mins.y ) || !( agent_maxs.z > agent_mins.z ) ) {
		return false;
	}

	/**
 *   Resolve the best ranked BSP-aware candidate endpoint so this NPC can stop relying on a single blended-Z projection.
	**/
   nav2_goal_candidate_t selected_candidate = {};
	nav2_goal_candidate_list_t candidate_list = {};
 const bool resolvedCandidate = SVG_Nav2_ResolveBestGoalOrigin( mesh, self->currentOrigin, goalOrigin, agent_mins, agent_maxs,
		outGoalOrigin, &selected_candidate, &candidate_list );

	/**
	*   Keep debug logging rate-limited and concise so candidate-selection diagnostics do not reintroduce log spam.
	**/
   if ( resolvedCandidate && DUMMY_NAV_DEBUG != 0 && Dummy_ShouldEmitNavDebugLog() ) {
		gi.dprintf( "[NAV2][GoalSelection] raw=(%.1f %.1f %.1f) resolved=(%.1f %.1f %.1f) type=%d candidates=%d rejections=%d\n",
			goalOrigin.x, goalOrigin.y, goalOrigin.z,
			outGoalOrigin->x, outGoalOrigin->y, outGoalOrigin->z,
			( int32_t )selected_candidate.type,
			candidate_list.candidate_count,
			candidate_list.rejection_count );
	}

	/**
	*	Build the staged corridor seam for diagnostics and future refinement integration without changing current behavior.
	**/
	if ( resolvedCandidate ) {
		(void)Dummy_TryBuildDebugCorridor( self, self->currentOrigin, *outGoalOrigin );
	}
 
 	return resolvedCandidate;
}

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
*   @brief	Return a readable name for a sound-follow AI state.
**/
static inline const char *Dummy_DebugAIStateName( const svg_monster_testdummy_sfxfollow_t::AIThinkState state ) {
	switch ( state ) {
	case svg_monster_testdummy_sfxfollow_t::AIThinkState::Idle:
		return "Idle";
    case svg_monster_testdummy_sfxfollow_t::AIThinkState::IdleLookout:
		return "IdleLookout";
   case svg_monster_testdummy_sfxfollow_t::AIThinkState::PursueSoundInvestigation:
		return "PursueSoundInvestigation";
	default:
		return "Unknown";
	}
}

/**
*   @brief	Emit a compact per-think state + gate input line for fast diagnosis.
*   @note	Only called when `DUMMY_NAV_DEBUG` is enabled.
**/
static inline void Dummy_DebugLogStateGateInputs( svg_monster_testdummy_sfxfollow_t *self ) {
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
 const bool requestPending = SVG_Nav2_IsRequestPending( &self->pathNavigationState.process );

 /**
	*   Emit a compact, single-line state snapshot for this think tick.
	**/
	DUMMY_NAV_DEBUG_PRINT( "[NAV DEBUG][ThinkGate] ent=%d state=%s active=%d has_act=%d vis=%d dist2d=%.1f has_sound=%d pending=%d handle=%d rebuild=%d goal=%d path_pts=%d path_idx=%d\n",
		self->s.number,
		Dummy_DebugAIStateName( self->thinkAIState ),
		self->isActivated ? 1 : 0,
		hasActivator ? 1 : 0,
		activatorVisible ? 1 : 0,
		activatorDist2D,
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
*   @brief	Set the explicit sound-follow AI state.
**/
static inline void Dummy_SetState( svg_monster_testdummy_sfxfollow_t *self, const svg_monster_testdummy_sfxfollow_t::AIThinkState newState ) {
	// Sanity: require valid entity.
	if ( !self ) {
		return;
	}

	/**
	*   Emit deterministic transition logs only when the state actually changes.
	**/
	const svg_monster_testdummy_sfxfollow_t::AIThinkState oldState = self->thinkAIState;
	if ( oldState != newState && DUMMY_NAV_DEBUG != 0 ) {
		gi.dprintf( "[NAV DEBUG][StateTransition] ent=%d %s -> %s\n",
			self->s.number,
			Dummy_DebugAIStateName( oldState ),
			Dummy_DebugAIStateName( newState ) );
	}

   // Assign the next deterministic sound-follow state.
	self->thinkAIState = newState;
}

/**
*   @brief	Central state dispatcher for the sound-follow AI states.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink )( svg_monster_testdummy_sfxfollow_t *self ) -> void {
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
	case svg_monster_testdummy_sfxfollow_t::AIThinkState::Idle:
		svg_monster_testdummy_sfxfollow_t::onThink_Idle( self );
		break;
	case svg_monster_testdummy_sfxfollow_t::AIThinkState::IdleLookout:
		svg_monster_testdummy_sfxfollow_t::onThink_IdleLookout( self );
		break;
   case svg_monster_testdummy_sfxfollow_t::AIThinkState::PursueSoundInvestigation:
		svg_monster_testdummy_sfxfollow_t::onThink_PursueSoundInvestigation( self );
		break;
	default:
		svg_monster_testdummy_sfxfollow_t::onThink_Idle( self );
		break;
	}
}


/**
*	@brief	Initialize the sound-follow test dummy and route it through the explicit AI-state dispatcher.
*			Spawn for the sound-follow variant: call base spawn, initialize movement/navigation state, then start the central think loop.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_sfxfollow_t, onSpawn )( svg_monster_testdummy_sfxfollow_t *self ) -> void {
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
	VectorCopy( svg_monster_testdummy_sfxfollow_t::DUMMY_BBOX_STANDUP_MINS, self->mins );
	VectorCopy( svg_monster_testdummy_sfxfollow_t::DUMMY_BBOX_STANDUP_MAXS, self->maxs );
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
	self->SetThinkCallback( &svg_monster_testdummy_sfxfollow_t::onThink );

	/**
	*	Callback Hooks:
	**/
    self->SetDieCallback( &svg_monster_testdummy_sfxfollow_t::onDie );
	self->SetPainCallback( &svg_monster_testdummy_sfxfollow_t::onPain );
	self->SetPostSpawnCallback( &svg_monster_testdummy_sfxfollow_t::onPostSpawn );
	self->SetTouchCallback( &svg_monster_testdummy_sfxfollow_t::onTouch );
	self->SetUseCallback( &svg_monster_testdummy_sfxfollow_t::onUse );

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
	// Clear any cached sound investigation target from prior lives or reused entity slots.
	self->stateSoundCan.hasOrigin = false;
	// Reset the last processed sound time so the first fresh sound can be consumed immediately.
	self->stateSoundCan.lastTime = 0_ms;
	// Reset the public AI timestamps used by the sound-follow state machine.
	self->aiLastSoundHeard = 0_ms;
	self->aiLastPlayerSeen = 0_ms;

	/**
	*	Idle Scan Properties:
	**/
	// Initialize idle scan state.
	self->stateIdleScan.yawScanDirection = 1.0f;
	// We will use stateIdleScan.nextFlipTime to track when we should flip our idle scan direction.
	self->stateIdleScan.nextFlipTime = level.time + DUMMY_IDLE_SCAN_FLIP_INTERVAL;
 // Start at heading index 0 (0 degrees).
	self->stateIdleScan.headingIndex = 0;
 // Initialize the explicit AI state machine as deactivated idle.
	Dummy_SetState( self, svg_monster_testdummy_sfxfollow_t::AIThinkState::Idle );

	/**
	*	Setup the initial monsterMoveState state.
	**/
	// First recategorize our current position in terms of ground and liquid so we have accurate state for the monster move code to work with right away.
	self->RecategorizeGroundAndLiquidState();

	// Apply the monster move properties so we can use the monster move code for all of our movement and collision handling
	// including during pathfinding pursuit.
  self->monsterMoveState.monster = self;
	self->monsterMoveState.frameTime = gi.frame_time_s;
	self->monsterMoveState.mins = self->mins;
	self->monsterMoveState.maxs = self->maxs;
	self->monsterMoveState.state.mm_type = MM_NORMAL;
	// Ensure mm_flags uses the expected 16-bit storage without narrowing warnings.
	self->monsterMoveState.state.mm_flags = static_cast< uint16_t >( self->groundInfo.entityNumber != ENTITYNUM_NONE ? MMF_ON_GROUND : MMF_NONE );
	self->monsterMoveState.state.mm_time = 0;
	self->monsterMoveState.state.gravity = ( int16_t )( self->gravity * sv_gravity->value );
	self->monsterMoveState.state.origin = self->currentOrigin;
	self->monsterMoveState.state.velocity = self->velocity;
	self->monsterMoveState.state.previousOrigin = self->currentOrigin;
	self->monsterMoveState.state.previousVelocity = self->velocity;
	self->monsterMoveState.ground = self->groundInfo;
	self->monsterMoveState.liquid = self->liquidInfo;

	/**
	*	Finish by setting neccessary callbacks and initial think time for our main thinker loop,
	//	which will handle all the debug AI states and transitions between them.
	//	We do this after initializing all properties to ensure that our thinker has a consistent starting state when it first runs.
	**/
	// Set use callback so we can be activated by the player.
    self->SetUseCallback( &svg_monster_testdummy_sfxfollow_t::onUse );
	// Always run our central state dispatcher thinker.
    self->SetThinkCallback( &svg_monster_testdummy_sfxfollow_t::onThink );
	self->nextthink = level.time + FRAME_TIME_MS;

	// Clear any pending async navigation state so we start clean when spawned/activated.
	self->ResetNavigationPath();
}

/**
*   @brief  Post-Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_monster_testdummy_sfxfollow_t, onPostSpawn )( svg_monster_testdummy_sfxfollow_t *self ) -> void {
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
DEFINE_MEMBER_CALLBACK_TOUCH( svg_monster_testdummy_sfxfollow_t, onTouch )( svg_monster_testdummy_sfxfollow_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	gi.dprintf( "onTouch\n" );
}

/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_sfxfollow_t, onDie )( svg_monster_testdummy_sfxfollow_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
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
*	@param	self	This sound-follow testdummy instance.
*	@param	other	The entity that sent the use event (usually the trigger or world).
*	@param	activator	The entity that activated the use (usually the player/client).
*	@param	useType	Type of use action (toggle, press, etc.).
*	@param	useValue	Value associated with the use action (e.g. 1 for on).
*	@note	This function centralizes follow/unfollow toggling and resets navigation
*			state when activation changes. Only a single assignment to `isActivated`
*			is performed later to keep activation semantics deterministic.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_monster_testdummy_sfxfollow_t, onUse )( svg_monster_testdummy_sfxfollow_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
 /**
	*    Cache the caller context first.
	**/
	// Store the latest activator/other pointers so sound reactions can still know who enabled us.
	self->activator = activator;
	self->other = other;

	/**
	*    Determine whether this use action enables sound-follow mode.
	**/
 // Only a toggle-on from a client activates the sound-follow NPC.
	const bool activating = ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE
		&& useValue == 1
		&& activator
		&& activator->client );

	/**
	*    Reset navigation and investigation state for the new activation mode.
	**/
	// Apply the activation state before rebuilding the AI state machine.
	self->isActivated = activating;
	// Clear any non-entity pursuit goal because this NPC only paths to cached sound origins.
	self->goalentity = nullptr;
	/**
	*	Discard any cached investigation target so reactivation starts from a clean listen state.
	**/
	self->stateSoundCan.hasOrigin = false;
	self->stateSoundCan.origin = self->currentOrigin;
	/**
	*	Snapshot the current world sound-slot time on activation so pre-existing sounds are ignored.
	**/
	// If activating, use the current world sound time as the baseline so only new sounds will be investigated. 
	// If deactivating, reset to 0 so any sound can trigger investigation once reactivated.
	const QMTime activationSoundBaseline = activating ? self->last_sound_time : 0_ms;
	// Reset the per-investigation timestamp so only sounds emitted after activation can start pursuit.
	self->stateSoundCan.lastTime = activationSoundBaseline;
	/**
	*	Reset the public AI timestamps to match the new activation mode.
	**/
	self->aiLastSoundHeard = activationSoundBaseline;
	self->aiLastPlayerSeen = 0_ms;
	/**
	*	Reinitialize idle scan bookkeeping so lookout begins from a predictable heading.
	**/
	self->stateIdleScan.yawScanDirection = 1.0;
	self->stateIdleScan.headingIndex = 0;
	self->stateIdleScan.targetYaw = self->ideal_yaw;
	self->stateIdleScan.nextFlipTime = level.time + DUMMY_IDLE_SCAN_FLIP_INTERVAL;
	// Cancel any in-flight async work so the new state does not inherit stale pathing.
	self->ResetNavigationPath();

	/**
	*    Update interaction hints and fire UseTargets.
	**/
	// Show the appropriate use hint based on whether the NPC is currently listening for sounds.
	SVG_Entity_SetUseTargetHintByID( self, activating ? USETARGET_HINT_ID_NPC_DISENGAGE : USETARGET_HINT_ID_NPC_ENGAGE );
	// Fire linked targets after the activation mode has been finalized.
	SVG_UseTargets( self, activator );

	/**
	*	Update explicit AI state.
	**/
	// Choose the state that matches the new activation mode.
	Dummy_SetState( self, activating
		? svg_monster_testdummy_sfxfollow_t::AIThinkState::IdleLookout
		: svg_monster_testdummy_sfxfollow_t::AIThinkState::Idle );

	/**
	*    Emit the existing compact debug print and schedule the next think.
	**/
	// Keep the existing activation print so sessions can verify toggle state quickly.
	if ( DUMMY_NAV_DEBUG != 0 ) {
		const char *activatorName = activator ? ( const char * )activator->classname : "nullptr";
		gi.dprintf( "[NAV DEBUG] %s: isActivated=%d, activator=%s\n",
			__func__, ( int32_t )self->isActivated, activatorName );
	}

	// Schedule the central dispatcher for the next frame.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_sfxfollow_t, onPain )( svg_monster_testdummy_sfxfollow_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {

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


//=================================================================================================

/**
*	@brief	Deactivated idle thinker.
*	@details	Keeps the sound-follow NPC stationary until a valid use-toggle enables listening mode.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_Idle )( svg_monster_testdummy_sfxfollow_t *self ) -> void {
	/**
	*    Maintain base state and liveness.
	**/
	// Stop immediately if the shared setup says this entity should not keep thinking.
	if ( !self->GenericThinkBegin() ) {
		return;
	}

	/**
	*    Transition back into lookout mode as soon as activation is restored.
	**/
	// Hand off to the activated listener state on the next frame.
	if ( self->isActivated ) {
		Dummy_SetState( self, svg_monster_testdummy_sfxfollow_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    Maintain idle animation, zero horizontal movement, and clear stale async state.
	**/
	// Use the idle animation while the entity is deactivated.
	if ( self->rootMotionSet && self->rootMotionSet->motions[ 1 ] ) {
		skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 1 ];
		const double t = level.time.Seconds<double>();
		const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
		const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
		self->s.frame = rootMotion->firstFrameIndex + localFrame;
	}
	// Keep the deactivated NPC from applying any horizontal steering.
	self->velocity.x = self->velocity.y = 0.0f;
	self->monsterMoveState.state.velocity.x = 0.0f;
	self->monsterMoveState.state.velocity.y = 0.0f;
	// Drop any cached sound target and cancel any in-flight async request.
	self->stateSoundCan.hasOrigin = false;
	self->goalentity = nullptr;
	self->ResetNavigationPath();

	/**
	*    Preserve grounding while standing still.
	**/
	// Let the shared slide-move finish keep us grounded on slopes and stairs.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	self->GenericThinkFinish( true, blockedMask );
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );
	self->nextthink = level.time + FRAME_TIME_MS;
}

//=================================================================================================

/**
*	@brief	Activated lookout thinker.
*	@details	Scans in place for fresh audible player-noise events and transitions into
*				`onThink_PursueSoundInvestigation` once a valid sound origin is chosen.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_IdleLookout )( svg_monster_testdummy_sfxfollow_t *self ) -> void {
	/**
	*    Maintain base state and liveness.
	**/
	// Stop immediately if the shared setup says this entity should not keep thinking.
	if ( !self->GenericThinkBegin() ) {
		return;
	}

	/**
	*    Return to deactivated idle immediately when use toggles this NPC off.
	**/
	// Drop back to idle without preserving any investigation target.
	if ( !self->isActivated ) {
		self->stateSoundCan.hasOrigin = false;
		self->goalentity = nullptr;
		self->ResetNavigationPath();
		Dummy_SetState( self, svg_monster_testdummy_sfxfollow_t::AIThinkState::Idle );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    Keep optional player-visibility bookkeeping up to date.
	**/
	// Record the last confirmed visible player without changing the sound-follow state.
	if ( self->activator && SVG_Entity_IsVisible( self, self->activator ) ) {
		self->aiLastPlayerSeen = level.time;
	}

	/**
	*    Listen for the freshest new audible sound event.
	**/
   // Query for a new sound newer than the last one we already acknowledged.
	svg_base_edict_t *audibleEntity = SVG_NPC_FindFreshestAudibleSound( self, self->aiLastSoundHeard, 0, true, DUMMY_NAV_DEBUG );
	if ( audibleEntity ) {
		// Record that we heard this sound so it is not reconsidered every frame.
		self->aiLastSoundHeard = audibleEntity->last_sound_time;
	  // Compute freshness for whether this sound is still worth investigating.
		const QMTime soundAge = level.time - audibleEntity->last_sound_time;
	 // Investigate any fresh audible sound regardless of range; nav/path policy owns the route response.
		if ( soundAge <= DUMMY_SOUND_INVESTIGATE_MAX_AGE ) {
			// Cache the chosen investigation origin and timestamp.
			self->stateSoundCan.origin = audibleEntity->currentOrigin;
			self->stateSoundCan.hasOrigin = true;
			self->stateSoundCan.lastTime = audibleEntity->last_sound_time;
			// Reset async state so the first pursuit frame rebuilds against this sound target.
			self->ResetNavigationPath();
			Dummy_SetState( self, svg_monster_testdummy_sfxfollow_t::AIThinkState::PursueSoundInvestigation );
		}
	}

	/**
	*    Begin pursuing immediately when we acquired a sound this frame.
	**/
	// Start the first async pursuit frame right away instead of waiting an extra think.
 if ( self->thinkAIState == svg_monster_testdummy_sfxfollow_t::AIThinkState::PursueSoundInvestigation && self->stateSoundCan.hasOrigin ) {
		self->DetermineGoalZBlendPolicyState( self->stateSoundCan.origin );
		self->goalentity = nullptr;
		self->MoveAStarToOrigin( self->stateSoundCan.origin, true );

		// Apply movement immediately so the first reaction frame feels responsive.
		int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
		self->GenericThinkFinish( true, blockedMask );
		SVG_Util_SetEntityAngles( self, self->currentAngles, true );
		if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
			self->pathNavigationState.process.next_rebuild_time = 0_ms;
		}
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    Maintain lookout animation, scanning yaw, and grounding while waiting.
	**/
	// Use the idle animation while listening for sound cues.
	if ( self->rootMotionSet && self->rootMotionSet->motions[ 1 ] ) {
		skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 1 ];
		const double t = level.time.Seconds<double>();
		const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
		const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
		self->s.frame = rootMotion->firstFrameIndex + localFrame;
	}
	// Keep horizontal velocity at zero while scanning in place.
	self->velocity.x = self->velocity.y = 0.0f;
	self->monsterMoveState.state.velocity.x = 0.0f;
	self->monsterMoveState.state.velocity.y = 0.0f;
	self->goalentity = nullptr;
	self->DetermineGoalZBlendPolicyState( self->currentOrigin );

	/**
	*    Advance the discrete idle-scan heading when the scan timer expires.
	**/
	// Step through eight headings so the sound-follow NPC visibly sweeps the room while listening.
	if ( self->stateIdleScan.nextFlipTime <= level.time ) {
		self->stateIdleScan.headingIndex += ( int32_t )self->stateIdleScan.yawScanDirection;
		if ( self->stateIdleScan.headingIndex < 0 ) {
			self->stateIdleScan.headingIndex = 7;
		} else if ( self->stateIdleScan.headingIndex > 7 ) {
			self->stateIdleScan.headingIndex = 0;
		}
		if ( self->stateIdleScan.headingIndex == 0 || self->stateIdleScan.headingIndex == 31 ) {
			self->stateIdleScan.yawScanDirection = -self->stateIdleScan.yawScanDirection;
		}
		self->stateIdleScan.targetYaw = ( float )( self->stateIdleScan.headingIndex * DUMMY_IDLE_SCAN_STEP_DEG );
		self->stateIdleScan.nextFlipTime = level.time + random_time( 100_ms, 300_ms );
	}

	/**
	*    Smoothly rotate toward the current lookout heading.
	**/
	// Blend toward the current lookout yaw instead of snapping for easier visual debugging.
	const double currentYaw = self->ideal_yaw;
	double deltaYaw = QM_AngleDelta( self->stateIdleScan.targetYaw, currentYaw );
	//double deltaYaw = QM_AngleDelta( self->stateIdleScan.targetYaw - currentYaw );
	//while ( deltaYaw > 180.0f ) {
	//	deltaYaw -= 360.0f;
	//}
	//while ( deltaYaw < -180.0f ) {
	//	deltaYaw += 360.0f;
	//}
	const float lerpFactor = QM_Clamp( ( float )( gi.frame_time_s * 6.0 ), 0.05f, 0.5f );
	self->ideal_yaw = currentYaw + deltaYaw * lerpFactor;
	self->yaw_speed = 12.0f;
	SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );

	/**
	*    Preserve grounding/physics while scanning in place.
	**/
	// Let the shared slide-move finish keep us grounded on slopes and stairs.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	self->GenericThinkFinish( true, blockedMask );
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );
	self->nextthink = level.time + FRAME_TIME_MS;
}

//=================================================================================================

/**
*	@brief	Pursue the cached sound investigation target with async A*.
*	@details	Maintains a stable world-space sound goal until arrival, expiry, or a newer
*				audible sound event replaces the cached target.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_PursueSoundInvestigation )( svg_monster_testdummy_sfxfollow_t *self ) -> void {
	/**
	*    Maintain base state and liveness.
	**/
	// Stop immediately if the shared setup says this entity should not keep thinking.
	if ( !self->GenericThinkBegin() ) {
		return;
	}

	/**
	*    Abort pursuit immediately when the NPC is no longer activated.
	**/
	// Drop back to deactivated idle and clear the cached sound target.
	if ( !self->isActivated ) {
		self->stateSoundCan.hasOrigin = false;
		self->goalentity = nullptr;
		self->ResetNavigationPath();
        Dummy_SetState( self, svg_monster_testdummy_sfxfollow_t::AIThinkState::Idle );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    Validate that the cached sound target still exists and is still fresh enough.
	**/
	// Fall back to lookout when we no longer have a usable sound origin to investigate.
	if ( !self->stateSoundCan.hasOrigin || ( level.time - self->stateSoundCan.lastTime ) > DUMMY_SOUND_INVESTIGATE_MAX_AGE ) {
		self->stateSoundCan.hasOrigin = false;
		self->goalentity = nullptr;
		self->ResetNavigationPath();
     Dummy_SetState( self, svg_monster_testdummy_sfxfollow_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*    Keep optional player-visibility bookkeeping up to date.
	**/
	// Record the last confirmed visible player without changing the sound-follow goal.
	if ( self->activator && SVG_Entity_IsVisible( self, self->activator ) ) {
		self->aiLastPlayerSeen = level.time;
	}

	/**
	*    Refresh the cached target only when a newer audible sound event arrives.
	**/
	// Track whether the investigation goal changed so we can force a rebuild only in that case.
	bool forceRebuild = false;
	svg_base_edict_t *audibleEntity = SVG_NPC_FindFreshestAudibleSound( self, self->stateSoundCan.lastTime, 512., true, DUMMY_NAV_DEBUG );
	if ( audibleEntity ) {
		self->aiLastSoundHeard = audibleEntity->last_sound_time;
		const QMTime soundAge = level.time - audibleEntity->last_sound_time;
		if ( soundAge <= DUMMY_SOUND_INVESTIGATE_MAX_AGE ) {
			/**
			*    Determine whether this refreshed sound meaningfully changed the active investigation goal.
			**/
			const bool shouldResetGoal = Dummy_ShouldResetSoundInvestigationGoal( self, audibleEntity->currentOrigin );

			/**
			*    Always keep the cached sound metadata fresh so expiry and future comparisons remain correct.
			**/
			self->stateSoundCan.origin = audibleEntity->currentOrigin;
			self->stateSoundCan.hasOrigin = true;
			self->stateSoundCan.lastTime = audibleEntity->last_sound_time;

			/**
			*    Only hard-reset async nav when the new sound actually moved enough to replace the current search.
			**/
			if ( shouldResetGoal ) {
				self->ResetNavigationPath();
				forceRebuild = true;
			}
		}
	}

	/**
	*    Tune goal-Z blending and issue the async A* move.
	**/
	// Bias goal-Z blending toward the current sound investigation origin.
	self->DetermineGoalZBlendPolicyState( self->stateSoundCan.origin );
	// Sound pursuit uses a pure world-space goal, not an entity target.
	self->goalentity = nullptr;
	// Continue following the cached sound, forcing a rebuild only when a newer sound replaced it.
	self->MoveAStarToOrigin( self->stateSoundCan.origin, forceRebuild );

	/**
	*    Leave pursuit once we have effectively reached the sound location.
	**/
	// Use a 3D arrival check so stair and ledge investigations still terminate once reached.
	const double soundDist3DSqr = QM_Vector3DistanceSqr( self->stateSoundCan.origin, self->currentOrigin );
	if ( soundDist3DSqr <= ( DUMMY_SOUND_INVESTIGATE_REACHED_DIST * DUMMY_SOUND_INVESTIGATE_REACHED_DIST ) ) {
		self->stateSoundCan.hasOrigin = false;
     Dummy_SetState( self, svg_monster_testdummy_sfxfollow_t::AIThinkState::IdleLookout );
	}

	/**
	*    Apply physics and schedule the next frame.
	**/
	// Let the shared slide-move finish apply steering, steps, and slope handling.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	self->GenericThinkFinish( true, blockedMask );
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );
	// Force an immediate rebuild attempt next frame when movement got blocked or trapped.
	if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		self->pathNavigationState.process.next_rebuild_time = 0_ms;
	}
	self->nextthink = level.time + FRAME_TIME_MS;
}

//=============================================================================================

/**
*	@brief	Set when dead. Does nothing.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_Dead )( svg_monster_testdummy_sfxfollow_t *self ) -> void {
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
	//SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	// Stay dead.
   self->SetThinkCallback( &svg_monster_testdummy_sfxfollow_t::onThink_Dead );
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
const bool svg_monster_testdummy_sfxfollow_t::GenericThinkBegin() {
	/**
	*	Clear visual flags.
	**/
	s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

	/**
	*	Setup A* Navigation Policy: stairs, drops, and obstruction jumping.
	**/
   pathNavigationState.policy.waypoint_radius = NAV2_DEFAULT_WAYPOINT_RADIUS;
	pathNavigationState.policy.min_step_height = NAV2_DEFAULT_STEP_MIN_SIZE;

 pathNavigationState.policy.max_step_height = NAV2_DEFAULT_STEP_MAX_SIZE;
	pathNavigationState.policy.max_drop_height = NAV2_DEFAULT_MAX_DROP_HEIGHT;
	pathNavigationState.policy.enable_max_drop_height_cap = true;
   pathNavigationState.policy.max_drop_height_cap = SVG_Nav2_Policy_GetMaxDropHeightCap();
	pathNavigationState.policy.enable_goal_z_layer_blend = true;
	pathNavigationState.policy.enable_cluster_route_filter = true;
 pathNavigationState.policy.blend_start_dist = NAV2_DEFAULT_BLEND_DIST_START;
	pathNavigationState.policy.blend_full_dist = NAV2_DEFAULT_BLEND_DIST_FULL;
	// No blending seems to work!
	//pathNavigationState.policy.enable_goal_z_layer_blend = false;
	//pathNavigationState.policy.blend_start_dist = PHYS_STEP_MAX_SIZE;
	//pathNavigationState.policy.blend_full_dist = 128.0;
	pathNavigationState.policy.allow_small_obstruction_jump = true;
 pathNavigationState.policy.max_obstruction_jump_height = NAV2_DEFAULT_MAX_OBSTRUCTION_JUMP_SIZE;

	/**
	*   Keep the monster move policy pointer synchronized with the active path-follow policy.
	*	Step-slide movement consumes `monsterMoveState.navPolicy`, so bind it every think before
	*   movement begins to keep stairs, drops, and jump allowances aligned with navigation.
	**/
   monsterMoveState.navPolicy = nullptr;

	/**
	*    Recategorize position and check grounding.
	**/
	RecategorizeGroundAndLiquidState();

	/**
	*    Liveness check.
	**/
	if ( health <= 0 || ( lifeStatus & LIFESTATUS_ALIVE ) != LIFESTATUS_ALIVE ) {
		// Transition and remain in the dead thinker and do nothing if we are dead.
     SetThinkCallback( &svg_monster_testdummy_sfxfollow_t::onThink_Dead );
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
const bool svg_monster_testdummy_sfxfollow_t::GenericThinkFinish( const bool processSlideMove, int32_t &blockedMask ) {
	// Perform the slide move and get the blocked mask describing the result of the movement attempt.
	blockedMask = ( processSlideMove ? ProcessSlideMove() : MM_SLIDEMOVEFLAG_NONE );

	// If we are not blocked or trapped, we can update our position and grounding info. 
	// Otherwise, we will rely on the next think to attempt recovery and not update our position 
	// so we don't get stuck in invalid geometry.
	if ( !( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) ) {
		// Update velocity to the new velocity resulting from the movement attempt, which is likely modified.
		velocity = monsterMoveState.state.velocity;
		// Update position and grounding info.
		groundInfo = monsterMoveState.ground;
		liquidInfo = monsterMoveState.liquid;
		// Update the entity's origin to the new position resulting from the movement attempt.
		SVG_Util_SetEntityOrigin( this, monsterMoveState.state.origin, true );
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
*		Explicit NPC State Management:
*
*
*
*
**/
/**
*	@brief	NPC Goal Z Blend Policy Adjustment Helper:
**/
void svg_monster_testdummy_sfxfollow_t::DetermineGoalZBlendPolicyState( const Vector3 &goalOrigin ) {
 /**
	*    Forward the caller-selected goal directly into the shared policy helper.
	**/
	// Sound-follow mode only needs a single resolved world-space goal for goal-Z blending.
	AdjustGoalZBlendPolicy( goalOrigin );
}

/**
*	@brief	Adjust the active goal-Z blend policy for the current resolved pursuit goal.
*	@param	goalOrigin	World-space feet-origin goal position used to bias layer selection.
*	@note	This keeps the sound-follow monster on nav2-owned defaults while widening the blend window for
*			farther goals so multi-level routing can keep a stable preferred height band.
**/
void svg_monster_testdummy_sfxfollow_t::AdjustGoalZBlendPolicy( const Vector3 &goalOrigin ) {
	/**
	*	Measure current horizontal and vertical separation to the resolved pursuit goal.
	**/
	const double goalDist2D = std::sqrt( QM_Vector2DistanceSqr( currentOrigin, goalOrigin ) );
	const double goalDeltaZ = std::fabs( goalOrigin.z - currentOrigin.z );

	/**
	*	Start from the nav2-owned default blend window each time so per-think adjustments remain deterministic.
	**/
	pathNavigationState.policy.enable_goal_z_layer_blend = true;
	pathNavigationState.policy.blend_start_dist = NAV2_DEFAULT_BLEND_DIST_START;
	pathNavigationState.policy.blend_full_dist = NAV2_DEFAULT_BLEND_DIST_FULL;

	/**
	*	For farther or more vertically separated goals, widen the blend-full distance so layer selection
	*	can bias toward the destination height earlier and remain stable across longer multi-level pursuits.
	**/
	if ( goalDist2D > 128.0 || goalDeltaZ > NAV2_DEFAULT_STEP_MAX_SIZE ) {
		// Use the larger of the existing default and a distance-aware widened blend window.
		pathNavigationState.policy.blend_full_dist = std::max( NAV2_DEFAULT_BLEND_DIST_FULL, std::min( goalDist2D * 0.25, 128.0 ) );
	}
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
const mm_slide_move_flags_t svg_monster_testdummy_sfxfollow_t::ProcessSlideMove() {
   /**
	*	Mirror the currently active nav2 policy fields into the legacy movement-policy shape that the older step-slide helper still consumes.
	**/
	nav2_path_policy_t legacyPolicy = {};
	legacyPolicy.min_step_normal = pathNavigationState.policy.min_step_normal;
	legacyPolicy.min_step_height = pathNavigationState.policy.min_step_height;
	legacyPolicy.max_step_height = pathNavigationState.policy.max_step_height;
	legacyPolicy.allow_small_obstruction_jump = pathNavigationState.policy.allow_small_obstruction_jump;
	legacyPolicy.max_obstruction_jump_height = pathNavigationState.policy.max_obstruction_jump_height;
	legacyPolicy.enable_max_drop_height_cap = pathNavigationState.policy.enable_max_drop_height_cap;
	legacyPolicy.max_drop_height = pathNavigationState.policy.max_drop_height;
	legacyPolicy.max_drop_height_cap = pathNavigationState.policy.max_drop_height_cap;

	/**
	*	Forward the mirrored movement policy into the staged step-slide helper while gameplay still relies on the older movement implementation.
	**/
	// Perform the slide move and get the blocked mask describing the result of the movement attempt.
	const mm_slide_move_flags_t blockedMask = SVG_MMove_StepSlideMove( &monsterMoveState, legacyPolicy );

	// Return the blocked mask so the caller can decide how to react to obstructions.
	return blockedMask;
}

/**
*    @brief    Slerp direction helper. (Local to this TU to avoid parent dependency).
**/
const Vector3 svg_monster_testdummy_sfxfollow_t::SlerpDirectionVector3( const Vector3 &from, const Vector3 &to, float t ) {
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
const void svg_monster_testdummy_sfxfollow_t::RecategorizeGroundAndLiquidState() {
	// Get the mask for checking ground and recategorizing position.
	const cm_contents_t mask = SVG_GetClipMask( this );
	// Check for ground and recategorize position so we can settle on the floor and interact with the world properly instead of being stuck in the air or in a wall.
	M_CheckGround( this, mask );
	// Recategorize position so we can update our liquid level/type and so we can properly interact with the world instead of being stuck in the air or in a wall.
	M_CatagorizePosition( this, currentOrigin, liquidInfo.level, liquidInfo.type );
}


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
void svg_monster_testdummy_sfxfollow_t::GetNavigationAgentBounds( Vector3 *out_mins, Vector3 *out_maxs ) {
	/**
	*	Sanity checks: require both output vectors before publishing any agent bounds.
	**/
	if ( !out_mins || !out_maxs ) {
		return;
	}

	/**
	*	Prefer mesh-derived public metadata first so active nav2 runtime bounds stay authoritative for the caller.
	**/
	const nav2_query_mesh_t *mesh = SVG_Nav2_GetQueryMesh();
	const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( mesh );
	if ( meshMeta.HasAgentBounds() ) {
		*out_mins = meshMeta.agent_mins;
		*out_maxs = meshMeta.agent_maxs;
		return;
	}

	/**
	*	Fall back to the nav2-owned agent profile snapshot when active mesh metadata is unavailable.
	**/
	const nav2_query_agent_profile_t agentProfile = SVG_Nav2_BuildAgentProfileFromCvars();
	const bool profileAgentValid = ( agentProfile.maxs.z > agentProfile.mins.z )
		&& ( agentProfile.maxs.x > agentProfile.mins.x )
		&& ( agentProfile.maxs.y > agentProfile.mins.y );
	if ( profileAgentValid ) {
		*out_mins = agentProfile.mins;
		*out_maxs = agentProfile.maxs;
		return;
	}

	/**
	*	Use the entity bounding box as the final fallback so sound-follow movement always has usable bounds.
	**/
	*out_mins = mins;
	*out_maxs = maxs;
}
/**
*   @brief	Clear stale async nav request state when no navmesh is loaded.
*   @param	self	Sound-follow testdummy owning the async path process.
*   @return	True when navmesh is unavailable and caller should early-return.
*   @note	Prevents repeated queue refresh/debounce loops on maps without navmesh.
**/
const bool svg_monster_testdummy_sfxfollow_t::GuardForNullNavMesh() {
	/**
	*   Fast path: navmesh exists, caller may continue normal request flow.
	**/
   if ( SVG_Nav2_GetQueryMesh() ) {
		return false;
	}

	/**
	*   Determine whether there is any async state worth tearing down.
	**/
	const bool hadPendingState = ( pathNavigationState.process.pending_request_handle > 0 )
		|| pathNavigationState.process.rebuild_in_progress
        || SVG_Nav2_IsRequestPending( &pathNavigationState.process );

	/**
	*   Cancel tracked handle first so queue state transitions to terminal.
	**/
	if ( pathNavigationState.process.pending_request_handle > 0 ) {
        SVG_Nav2_CancelRequest( ( nav2_query_handle_t )pathNavigationState.process.pending_request_handle );
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
*    @note     Direct fallback steering is only used when no navmesh is loaded. When navigation is available,
*              this helper waits for a valid async path instead of walking straight through blocked geometry.
**/
const bool svg_monster_testdummy_sfxfollow_t::MoveAStarToOrigin( const Vector3 &goalOrigin, bool force ) {
	/**
	*    Derive the correct nav-agent bbox (feet-origin) for traversal.
	**/
	Vector3 agent_mins, agent_maxs;
	GetNavigationAgentBounds( &agent_mins, &agent_maxs );
	Q_assert( agent_maxs.x > agent_mins.x && agent_maxs.y > agent_mins.y && agent_maxs.z > agent_mins.z );

	/**
	*    Resolve one consistent walkable-goal snapshot for this frame.
	*        Steering, queueing, and final-approach slowdown should all reference the same projected
	*        walkable goal so cross-level routes do not oscillate between raw sound origins and a snapped nav target.
	**/
	Vector3 resolvedGoalOrigin = goalOrigin;
	Dummy_TryProjectGoalToWalkableZ( this, goalOrigin, &resolvedGoalOrigin );

	// Pre-calculate animation frame basics once per think.
	const double currentTime = level.time.Seconds<double>();
	const int32_t animFrameGlobal = ( int32_t )std::floor( ( float )( currentTime * 40.0f ) );

   // Debug print summarizing the request state for diagnostics.
	DUMMY_NAV_DEBUG_PRINT( "[NAV DEBUG] %s: goal=(%.1f %.1f %.1f) force=%d pathOk=%d pending=%d\n",
		__func__, resolvedGoalOrigin.x, resolvedGoalOrigin.y, resolvedGoalOrigin.z, ( int32_t )force,
		( int32_t )( pathNavigationState.process.path.num_points > 0 ),
      ( int32_t )SVG_Nav2_IsRequestPending( &pathNavigationState.process ) );

	/**
	*    Sanity / arrival check: stop moving if we are effectively at the goal already to prevent jitter.
	**/
	Vector3 toGoalDist = QM_Vector3Subtract( resolvedGoalOrigin, currentOrigin );
	 // Only consider horizontal distance for arrival.
	toGoalDist.z = 0.0f;
	// Use squared length for arrival check to save a square root.
	constexpr float arrivalThreshold = 0.03125f;
	if ( QM_Vector3LengthSqr( toGoalDist ) < ( arrivalThreshold * arrivalThreshold ) ) {
		// Zero horizontal velocity to prevent micro-jitter.
		velocity.x = velocity.y = 0.0f;
		return false;
	}

	/**
	*   Local helper for direct-steer fallback when no A* direction exists.
	*	Keep sound-follow NPC behavior responsive on maps without navmesh.
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
			skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 4 ];
			const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrameGlobal % rootMotion->frameCount ) : 0;
			s.frame = rootMotion->firstFrameIndex + localFrame;
		}

		constexpr double frameVelocity = 220.0;
		velocity.x = ( float )( moveDir.x * frameVelocity );
		velocity.y = ( float )( moveDir.y * frameVelocity );
		monsterMoveState.state.velocity.x = velocity.x;
		monsterMoveState.state.velocity.y = velocity.y;
		return true;
		};

		/**
		*   Guard: when no navmesh is loaded, clear stale async state and rely on
		*       direct fallback movement so the debug state machine keeps functioning.
		**/
	if ( GuardForNullNavMesh() ) {
		return applyDirectFallback( resolvedGoalOrigin );
	}

	/**
	*    Inspect current nav-consumer state before requesting any new async work.
	*        The nav path-process now owns rebuild throttling/progress preservation, so this
	*        entity should prefer consuming an existing path or waiting on a pending request
	*        instead of re-queueing equivalent direct paths every think.
	**/
   const bool requestPending = SVG_Nav2_IsRequestPending( &pathNavigationState.process );
	const bool hasPathPoints = ( pathNavigationState.process.path.num_points > 0 && pathNavigationState.process.path.points );
	const bool pathExpired = hasPathPoints && pathNavigationState.process.path_index >= pathNavigationState.process.path.num_points;
	const bool pathOk = hasPathPoints && !pathExpired;

	/**
    *	Queue async work only when the consumer truly needs a fresh request.
	*		- Forced requests always replace/refresh the current search.
	*		- Non-forced requests are issued only when no usable path exists and no request is already pending.
	*	This keeps flat-ground direct-shortcut paths from being rebuilt every frame while the mover is already following them.
	**/
   const bool queueModeEnabled = SVG_Nav2_Policy_IsAsyncQueueEnabled();
   const bool asyncNavEnabled = SVG_Nav2_IsAsyncNavEnabled();

	bool queueAttempted = false;
	bool queueResult = false;

    // Preserve explicit force requests so the first sound reaction and any blocked retrigger can bypass throttles immediately.
	const bool effectiveForce = force;

	/**
  *    Build the queue policy snapshot for this request.
	**/
	#ifdef MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER
	// Route-filter isolation mode: explicitly disable coarse tile filtering so neighbor diagnostics reflect pure StepTest traversal behavior.
	pathNavigationState.policy.enable_cluster_route_filter = false;
	#endif

	// Only queue when the consumer has no usable path yet, or when the caller explicitly forces a refresh.
	const bool shouldQueueRequest = effectiveForce || ( !pathOk && !requestPending );
	if ( shouldQueueRequest ) {
		queueAttempted = true;
		const bool queued = TryRebuildNavigationInQueue( currentOrigin, resolvedGoalOrigin, pathNavigationState.policy, agent_mins, agent_maxs, effectiveForce );
		queueResult = queued;
	}

	/**
	*    Keep the goal snapshot current only after we have either a usable path or an accepted async request.
	*        This keeps the entity-side bookkeeping descriptive without letting it become a second rebuild-policy layer.
	**/
	if ( pathOk || queueResult || requestPending ) {
		pathNavigationState.lastGoal.origin = resolvedGoalOrigin;
		pathNavigationState.lastGoal.isValid = true;
		pathNavigationState.lastGoal.isVisible = false;
	}

	/**
	*    Rate-limited per-entity queue status logging for navigation verification.
	**/
	const QMTime now = level.time;
	if ( DUMMY_NAV_DEBUG != 0 && now >= pathNavigationState.nextQueueStatusLogTime ) {
		pathNavigationState.nextQueueStatusLogTime = now + 500_ms;

		/**
		*	Calculate rebuild heuristics only when logging is active to save frames.
		**/
       const bool canRebuild = pathNavigationState.process.CanRebuild( pathNavigationState.policy );
		const bool waitingOnPendingRequest = requestPending && !pathOk;

       gi.dprintf( "[NAV STATUS] ent=%d attempt=%d result=%d canRebuild=%d hasPath=%d pending=%d waiting=%d async=%d queueMode=%d\n",
			s.number,
			queueAttempted ? 1 : 0,
			queueResult ? 1 : 0,
			canRebuild ? 1 : 0,
            pathOk ? 1 : 0,
			requestPending ? 1 : 0,
			waitingOnPendingRequest ? 1 : 0,
			asyncNavEnabled ? 1 : 0,
			queueModeEnabled ? 1 : 0 );
	}

	/**
	*    Keep a movement direction container for either path-follow or fallback.
	**/
	Vector3 move_dir = { 0.0f, 0.0f, 0.0f };

	// If we have a valid path, query and follow the shared navigation follow-state.
    nav2_query_process_t::follow_state_t followState = {};
	if ( pathOk && pathNavigationState.process.QueryFollowState( currentOrigin, pathNavigationState.policy, &followState ) ) {
	 /**
		*    Record the last successfully followed goal for future rebuild heuristics.
		**/
		// Keep the last goal snapshot current so future forced rebuild checks have a stable reference.
		pathNavigationState.lastGoal.origin = resolvedGoalOrigin;
		pathNavigationState.lastGoal.isValid = true;
		pathNavigationState.lastGoal.isVisible = ( activator != nullptr ) && SVG_Entity_IsVisible( this, activator );

	/**
		*    Resolve the currently active waypoint in entity space so yaw and velocity can converge on that anchor.
		*        This keeps the monster's bbox center closer to each stair/corner waypoint instead of treating only the
		*        final goal as important for centering.
		**/
		move_dir = followState.move_direction3d;
		const bool hasActiveWaypoint = followState.has_direction;
		const Vector3 activeWaypointOrigin = followState.active_waypoint_origin;
		const double activeWaypointDist2D = followState.active_waypoint_dist2d;
		const double activeWaypointDeltaZ = followState.active_waypoint_delta_z;
		const bool waypointNeedsPreciseCentering = followState.waypoint_needs_precise_centering;
		const bool steppedVerticalCorridorAhead = followState.stepped_vertical_corridor_ahead;
		const bool corridorNeedsPreciseCentering = waypointNeedsPreciseCentering || steppedVerticalCorridorAhead;
		Vector3 toActiveWaypoint = QM_Vector3Subtract( activeWaypointOrigin, currentOrigin );
		toActiveWaypoint.z = 0.0f;
		const Vector3 waypointDir = ( activeWaypointDist2D > 0.001 ) ? QM_Vector3Normalize( toActiveWaypoint ) : Vector3{};

		/**
	   *    Blend the queried path direction toward the active waypoint anchor before we face or move.
		*        This keeps the path direction still guides broad navigation, but crowded stairs need a stronger pull toward the actual
		*        anchor point so the bbox center does not skim the stair edge.
		**/
		Vector3 centeredMoveDir = followState.move_direction3d;
		centeredMoveDir.z = 0.0f;
		if ( hasActiveWaypoint && activeWaypointDist2D > 0.001 ) {
			  // Keep stair and slope anchors influential without letting every short riser force a near-pure face-the-anchor turn.
			const double waypointBlend = corridorNeedsPreciseCentering
				? ( activeWaypointDeltaZ > 0.0 ? 0.75 : 0.50 )
				: 0.35;
			const double centeredMoveLen2 = ( centeredMoveDir.x * centeredMoveDir.x ) + ( centeredMoveDir.y * centeredMoveDir.y );
			if ( centeredMoveLen2 > ( 0.001 * 0.001 ) ) {
				centeredMoveDir = QM_Vector3Normalize( SlerpDirectionVector3( QM_Vector3Normalize( centeredMoveDir ), waypointDir, ( float )waypointBlend ) );
			} else {
				centeredMoveDir = waypointDir;
			}
		}

		/**
	   *    Face the centered movement direction on the horizontal plane.
		*        Raise yaw speed when a near or vertical waypoint needs tighter centering, and reduce translation when yaw
		*        is still catching up so the monster does not drift wide off crowded stairs or blocked routes.
		**/
		Vector3 faceDir = centeredMoveDir;
		faceDir.z = 0.0f;
		double yawDeltaAbs = 0.0;
		if ( ( faceDir.x * faceDir.x + faceDir.y * faceDir.y ) > 0.001f ) {
			ideal_yaw = QM_Vector3ToYaw( faceDir );
			const double currentYaw = QM_AngleMod( currentAngles[ YAW ] );
			yawDeltaAbs = std::fabs( QM_AngleDelta( ideal_yaw, currentYaw ) );
			const double yawSpeedBase = corridorNeedsPreciseCentering ? 32.0 : 16.0;
			const double yawSpeedCloseBoost = ( hasActiveWaypoint && activeWaypointDist2D <= 32.0 ) ? 6.0 : 0.0;
			const double yawSpeedTurnBoost = yawDeltaAbs * 0.10;
			yaw_speed = ( float )QM_Clamp( yawSpeedBase + yawSpeedCloseBoost + yawSpeedTurnBoost, 10.0, 45.0 );
			SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
		}

		// Run animation while following a path.
		if ( rootMotionSet && rootMotionSet->motions[ 4 ] ) {
			skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 4 ]; // RUN
			const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrameGlobal % rootMotion->frameCount ) : 0;
			s.frame = rootMotion->firstFrameIndex + localFrame;
		}

	  /**
	 *    Apply horizontal velocity derived from the centered movement direction.
			*        Use waypoint-centric slowdown plus yaw-alignment slowdown so the monster's center converges more closely
		*        on each active waypoint, which helps crowded stair bands and narrow turns.
		**/
		double frameVelocity = 220.0;

		/**
		*    Slow translation while yaw is still aligning to avoid skating wide across a corner or stair edge.
		**/
		// Keep enough forward motion on stairs and slopes that the monster can keep climbing while its yaw converges.
		const double yawSlowdownDenominator = corridorNeedsPreciseCentering ? 85.0 : 100.0;
		const double yawAlignmentScale = ( yawSlowdownDenominator > 0.0 )
			? QM_Clamp( 1.0 - ( yawDeltaAbs / yawSlowdownDenominator ), corridorNeedsPreciseCentering ? 0.45 : 0.55, 1.0 )
			: 1.0;
		frameVelocity *= yawAlignmentScale;

		/**
		*    Ease into the active waypoint so the bbox center lands closer to the waypoint anchor instead of clipping past it.
		**/
		if ( hasActiveWaypoint ) {
			const double centeringRange = corridorNeedsPreciseCentering
				? std::max( pathNavigationState.policy.waypoint_radius * 4.0, 64.0 )
				: std::max( pathNavigationState.policy.waypoint_radius * 2.0, 24.0 );
			const double centeringScale = ( centeringRange > 0.0 )
				? QM_Clamp( ( activeWaypointDist2D + ( pathNavigationState.policy.waypoint_radius * 0.5 ) ) / centeringRange,
					corridorNeedsPreciseCentering ? 0.45 : 0.50,
					1.0 )
				: 1.0;
			frameVelocity *= centeringScale;
		}

		/**
		*    Keep the final-leg slowdown as an extra guard so destination settling remains clean after intermediate centering.
		**/
		const bool approachingFinalGoal = followState.approaching_final_goal;
		if ( approachingFinalGoal ) {
			/**
			*    Compute a conservative final-approach speed scale from horizontal goal distance.
			**/
			Vector3 toFinalGoal = QM_Vector3Subtract( resolvedGoalOrigin, currentOrigin );
			toFinalGoal.z = 0.0f;
			const double goalDist2D = std::sqrt( ( toFinalGoal.x * toFinalGoal.x ) + ( toFinalGoal.y * toFinalGoal.y ) );

			/**
			*    Begin easing down inside a short final-approach window so the monster can finish centered.
			**/
			constexpr double finalSlowdownRange = 64.0;
			constexpr double finalGoalStopPadding = 0.25;
			const double approachDist = std::max( 0.0, goalDist2D - finalGoalStopPadding );
			const double speedScale = ( finalSlowdownRange > 0.0 )
				? QM_Clamp( approachDist / finalSlowdownRange, 0.0, 1.0 )
				: 1.0;
			frameVelocity *= speedScale;
		}

		velocity.x = ( float )( centeredMoveDir.x * frameVelocity );
		velocity.y = ( float )( centeredMoveDir.y * frameVelocity );

		/**
		*    Mirror the accepted path-follow velocity into the authoritative monster move state.
		*        `GenericThinkFinish()` ultimately advances through `SVG_MMove_StepSlideMove()`, which
		*        consumes `monsterMoveState.state.velocity` rather than the entity `velocity` field alone.
		**/
		monsterMoveState.state.velocity.x = velocity.x;
		monsterMoveState.state.velocity.y = velocity.y;
		return true;
	}

	/**
	*    Navigation-available fallback policy:
	*        Do not keep direct-steering through the world once navmesh pathing is active.
	*        Repeated async failures should pause movement until a valid path exists instead
	*        of letting the debug NPC walk straight through corners or blocked routes.
	**/
	velocity.x = velocity.y = 0.0f;
	monsterMoveState.state.velocity.x = 0.0f;
	monsterMoveState.state.velocity.y = 0.0f;

	// While waiting on async pathing, keep facing the intended horizontal goal direction.
	Vector3 faceGoal = QM_Vector3Subtract( resolvedGoalOrigin, currentOrigin );
	faceGoal.z = 0.0f;
	const float faceLen2 = ( faceGoal.x * faceGoal.x ) + ( faceGoal.y * faceGoal.y );
	if ( faceLen2 > ( 0.001f * 0.001f ) ) {
		const Vector3 faceDir = QM_Vector3Normalize( faceGoal );
		ideal_yaw = QM_Vector3ToYaw( faceDir );
		yaw_speed = 10.0f;
		SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
	}

	if ( rootMotionSet && rootMotionSet->motions[ 1 ] ) {
		skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 1 ];
		const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrameGlobal % rootMotion->frameCount ) : 0;
		s.frame = rootMotion->firstFrameIndex + localFrame;
	}

	const float horizLen2 = ( move_dir.x * move_dir.x ) + ( move_dir.y * move_dir.y );
	if ( horizLen2 > ( 0.001f * 0.001f ) ) {
		Vector3 faceDir = move_dir;
		faceDir.z = 0.0f;
		ideal_yaw = QM_Vector3ToYaw( faceDir );
       yaw_speed = SVG_Nav2_IsRequestPending( &pathNavigationState.process ) ? 10.0f : 15.0f;
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
const bool svg_monster_testdummy_sfxfollow_t::TryRebuildNavigationInQueue( const Vector3 &start_origin,
	const Vector3 &goal_origin, const nav2_query_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, const bool force ) {
	/**
  *    Attempt to enqueue an asynchronous navigation rebuild for this entity.
	*        This wrapper is intentionally thin: the nav-folder path-process/request queue owns
	*        rebuild throttling, refresh suppression, and progress preservation. The monster only
	*        resolves its goal and forwards a request when higher-level behavior decides one is needed.
	**/
    // Guard: only enqueue when the async queue mode is explicitly enabled.
    if ( !SVG_Nav2_Policy_IsAsyncQueueEnabled() ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: async queue mode disabled, cannot enqueue. ent=%d\n", s.number );
		}
		return false;
	}

   if ( !SVG_Nav2_IsAsyncNavEnabled() ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: async nav globally disabled, ent=%d\n", s.number );
		}
		return false;
	}

	/**
    *    Resolve the requested sound goal against the navmesh before queueing async work.
	*        This keeps the consumer aligned with nav-folder feet-origin projection helpers.
	**/
	Vector3 adjusted_goal = goal_origin;
	Dummy_TryProjectGoalToWalkableZ( this, goal_origin, &adjusted_goal );

   /**
	*    Replace any outstanding request only when the caller explicitly forces a fresh search.
	*		Non-forced dedupe/refresh behavior should remain inside the nav-folder request API.
	**/
   if ( SVG_Nav2_IsRequestPending( &pathNavigationState.process ) ) {
        // Keep the existing in-flight request alive unless the caller requested an explicit replacement.
		if ( !force ) {
			return true;
		}

		// Forced replacement: cancel the old request first so a fresh queued entry can be created immediately.
		ResetNavigationPath();
	}

	/**
 *    Enqueue the rebuild request and record the handle for diagnostics.
	*        We pass through the nav-folder policy/bounds unchanged and let the request API
	*        decide whether the new request should be queued or internally collapsed.
	**/
  // Keep enough tolerance to preserve in-flight worker progress across ordinary locomotion drift.
	constexpr double startIgnoreThresholdForQueue = 24.0;
    const nav2_query_handle_t handle = SVG_Nav2_RequestPathAsync( &pathNavigationState.process, start_origin, adjusted_goal, policy, agent_mins, agent_maxs, force, startIgnoreThresholdForQueue );
	if ( !handle.IsValid() ) {
		if ( DUMMY_NAV_DEBUG ) {
          gi.dprintf( "[DEBUG] TryQueueNavRebuild: enqueue failed (handle=%d) ent=%d\n", handle.value, s.number );
		}
		return false;
	}

	// Record that a rebuild is in progress for diagnostics and possible cancellation.
	// Note: the request queue will have already set the process markers during
	// PrepareAStarForEntry when the entry transitions to Running. Setting them
	// here ensures the entity has the handle immediately for early cancellation
	// if the caller chooses to abort before the queue tick processes it.
	pathNavigationState.process.rebuild_in_progress = true;
    pathNavigationState.process.pending_request_handle = handle.value;
	DUMMY_NAV_DEBUG_PRINT( "[DEBUG] TryQueueNavRebuild: queued rebuild handle=%d ent=%d force=%d\n", handle.value, s.number, force );
	// Also print the converted nav-center origins so we can correlate node resolution.
  const nav2_query_mesh_t *mesh = SVG_Nav2_GetQueryMesh();
	if ( mesh ) {
       const Vector3 start_center = SVG_Nav2_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
		const Vector3 goal_center = SVG_Nav2_ConvertFeetToCenter( mesh, adjusted_goal, &agent_mins, &agent_maxs );
		DUMMY_NAV_DEBUG_PRINT( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
			start_origin.x, start_origin.y, start_origin.z,
			start_center.x, start_center.y, start_center.z,
			adjusted_goal.x, adjusted_goal.y, adjusted_goal.z,
			goal_center.x, goal_center.y, goal_center.z );
	} else {
		DUMMY_NAV_DEBUG_PRINT( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) (no mesh)\n",
			start_origin.x, start_origin.y, start_origin.z,
			adjusted_goal.x, adjusted_goal.y, adjusted_goal.z );
	}
	return true;
}
/**
*    @brief	Reset cached navigation path state when no navmesh is loaded.
*    @param	self	Monster whose path state should be cleared.
*    @note	Cancels any queued async request and clears cached path buffers.
**/
void svg_monster_testdummy_sfxfollow_t::ResetNavigationPath() {
	/**
	*	Cancel any pending async request so we do not reuse stale results.
	**/
	if ( pathNavigationState.process.pending_request_handle > 0 ) {
		SVG_Nav2_CancelRequest( SVG_Nav2_QueryMakeHandle( pathNavigationState.process.pending_request_handle ) );
	}

	/**
	*    Reset the shared path-process state through its canonical helper.
	*		This clears cached path buffers, traversal bookkeeping, async generations,
	*        center offsets, and failure/backoff history in one consistent place.
	**/
	pathNavigationState.process.Reset();

	   // Clear the last goal snapshot so heuristics treat the next goal as new and avoid suppressing rebuilds.
	pathNavigationState.lastGoal.origin = {};
	pathNavigationState.lastGoal.isValid = false;
	pathNavigationState.lastGoal.isVisible = false;
}

