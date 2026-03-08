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

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

#include "svgame/nav/svg_nav.h"
// Navigation cluster routing (coarse tile routing pre-pass).
#include "svgame/nav/svg_nav_clusters.h"
// Async navigation queue helpers.
#include "svgame/nav/svg_nav_request.h"
// Traversal helpers required for path invalidation.
#include "svgame/nav/svg_nav_traversal.h"

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy_sfxfollow.h"


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

//! Maximum age of a sound event that can trigger investigation.
static constexpr QMTime DUMMY_SOUND_INVESTIGATE_MAX_AGE = 24_sec;
//! Maximum horizontal distance for reacting to sound events.
static constexpr double DUMMY_SOUND_INVESTIGATE_MAX_DIST = CM_MAX_WORLD_SIZE;
//! Arrival radius used for ending sound investigation behavior.
static constexpr double DUMMY_SOUND_INVESTIGATE_REACHED_DIST = 4.0;
//! Idle yaw scan step in degrees per think.
static constexpr double DUMMY_IDLE_SCAN_STEP_DEG = 45.0 / 4.;
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
	const svg_nav_path_policy_t &policy = self->pathNavigationState.policy;
	const double rebuildGoal2D = std::max( policy.rebuild_goal_dist_2d, 8.0 );
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
	if ( rebuildGoal3D > 0.0 && goalDelta3DSqr > ( rebuildGoal3D * rebuildGoal3D ) ) {
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

	const nav_mesh_t *mesh = g_nav_mesh.get();
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
	*   Convert the sound goal into nav-center space and inspect the current XY cell.
	**/
	const Vector3 center_origin = SVG_Nav_ConvertFeetToCenter( mesh, goalOrigin, &agent_mins, &agent_maxs );
	const nav_tile_cluster_key_t tile_key = SVG_Nav_GetTileKeyForPosition( mesh, center_origin );
	const nav_world_tile_key_t world_tile_key = { .tile_x = tile_key.tile_x, .tile_y = tile_key.tile_y };
	const auto tile_it = mesh->world_tile_id_of.find( world_tile_key );
	if ( tile_it == mesh->world_tile_id_of.end() ) {
		return false;
	}

	const nav_tile_t *tile = &mesh->world_tiles[ tile_it->second ];
	const auto cells_view = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t *cells = cells_view.first;
	const int32_t cell_count = cells_view.second;
	if ( !cells || cell_count <= 0 ) {
		return false;
	}

	const double tile_world_size = ( double )mesh->tile_size * mesh->cell_size_xy;
	const double tile_origin_x = ( double )tile->tile_x * tile_world_size;
	const double tile_origin_y = ( double )tile->tile_y * tile_world_size;
	const double local_x = center_origin.x - tile_origin_x;
	const double local_y = center_origin.y - tile_origin_y;
	if ( local_x < 0.0 || local_y < 0.0 ) {
		return false;
	}

	const int32_t cell_x = std::clamp( ( int32_t )( local_x / mesh->cell_size_xy ), 0, mesh->tile_size - 1 );
	const int32_t cell_y = std::clamp( ( int32_t )( local_y / mesh->cell_size_xy ), 0, mesh->tile_size - 1 );
	const int32_t cell_index = ( cell_y * mesh->tile_size ) + cell_x;
	if ( cell_index < 0 || cell_index >= cell_count ) {
		return false;
	}

	const nav_xy_cell_t *cell = &cells[ cell_index ];
	if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
		return false;
	}

	int32_t layer_index = -1;
	if ( !Nav_SelectLayerIndex( mesh, cell, center_origin.z, &layer_index ) || layer_index < 0 || layer_index >= cell->num_layers ) {
		return false;
	}

	/**
	*   Convert the chosen walkable layer back into feet-origin space for the request queue.
	**/
	const float center_offset_z = ( agent_mins.z + agent_maxs.z ) * 0.5f;
	const double projected_center_z = ( double )cell->layers[ layer_index ].z_quantized * mesh->z_quant;
	*outGoalOrigin = goalOrigin;
	outGoalOrigin->z = ( float )( projected_center_z - center_offset_z );
	return true;
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
	const bool requestPending = SVG_Nav_IsRequestPending( &self->pathNavigationState.process );

	/**
	*   Emit a compact, single-line state snapshot for this think tick.
	**/
	gi.dprintf( "[NAV DEBUG][ThinkGate] ent=%d state=%s active=%d has_act=%d vis=%d dist2d=%.1f has_sound=%d pending=%d handle=%d rebuild=%d goal=%d path_pts=%d path_idx=%d\n",
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
	self->monsterMoveState = {
			.monster = self,
			.frameTime = gi.frame_time_s,
			.mins = self->mins,
			.maxs = self->maxs,
			.state = {
			.mm_type = MM_NORMAL,
			// Ensure mm_flags uses the expected 16-bit storage without narrowing warnings.
			.mm_flags = static_cast< uint16_t >( self->groundInfo.entityNumber != ENTITYNUM_NONE ? MMF_ON_GROUND : MMF_NONE ),
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
		// Compute freshness and distance gates for whether this sound is worth investigating.
		const QMTime soundAge = level.time - audibleEntity->last_sound_time;
		const double soundDist3D = std::sqrt( QM_Vector3DistanceSqr( audibleEntity->currentOrigin, self->currentOrigin ) );
		// Only transition when the sound is both fresh and reasonably nearby.
		if ( soundAge <= DUMMY_SOUND_INVESTIGATE_MAX_AGE && soundDist3D <= DUMMY_SOUND_INVESTIGATE_MAX_DIST ) {
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
	svg_base_edict_t *audibleEntity = SVG_NPC_FindFreshestAudibleSound( self, self->stateSoundCan.lastTime, 0., true, DUMMY_NAV_DEBUG );
	if ( audibleEntity ) {
		self->aiLastSoundHeard = audibleEntity->last_sound_time;
		const QMTime soundAge = level.time - audibleEntity->last_sound_time;
		const double soundDist3D = std::sqrt( QM_Vector3DistanceSqr( audibleEntity->currentOrigin, self->currentOrigin ) );
		if ( soundAge <= DUMMY_SOUND_INVESTIGATE_MAX_AGE && soundDist3D <= DUMMY_SOUND_INVESTIGATE_MAX_DIST ) {
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
	pathNavigationState.policy.waypoint_radius = NAV_DEFAULT_WAYPOINT_RADIUS;
	pathNavigationState.policy.min_step_height = NAV_DEFAULT_STEP_MIN_SIZE;

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
	*   Keep the monster move policy pointer synchronized with the active path-follow policy.
	*	Step-slide movement consumes `monsterMoveState.navPolicy`, so bind it every think before
	*   movement begins to keep stairs, drops, and jump allowances aligned with navigation.
	**/
	monsterMoveState.navPolicy = &pathNavigationState.policy;

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
	// Perform the slide move and get the blocked mask describing the result of the movement attempt.
	const mm_slide_move_flags_t blockedMask = SVG_MMove_StepSlideMove( &monsterMoveState, pathNavigationState.policy );

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
void svg_monster_testdummy_sfxfollow_t::GetNavigationAgentBounds( Vector3 *out_mins, Vector3 *out_maxs ) {
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
*   @param	self	Sound-follow testdummy owning the async path process.
*   @return	True when navmesh is unavailable and caller should early-return.
*   @note	Prevents repeated queue refresh/debounce loops on maps without navmesh.
**/
const bool svg_monster_testdummy_sfxfollow_t::GuardForNullNavMesh() {
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
		monsterMoveState.state.velocity.x = velocity.x;
		monsterMoveState.state.velocity.y = velocity.y;
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
   // Only honor an explicit force request when the goal moved beyond the configured rebuild threshold.
	bool effectiveForce = force;
	if ( force ) {
		// Compute the delta relative to the last goal so we do not re-force for negligible motion.
		const Vector3 &referenceGoal = pathNavigationState.lastGoal.isValid ? pathNavigationState.lastGoal.origin : pathNavigationState.process.path_goal_position;
		const double dx = QM_Vector3LengthDP( QM_Vector3Subtract( goalOrigin, referenceGoal ) );
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
	**/
	#ifdef MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER
	// Route-filter isolation mode: explicitly disable coarse tile filtering so neighbor diagnostics reflect pure StepTest traversal behavior.
	pathNavigationState.policy.enable_cluster_route_filter = false;
	#endif
	queueAttempted = true;
	const bool queued = TryRebuildNavigationInQueue( currentOrigin, goalOrigin, pathNavigationState.policy, agent_mins, agent_maxs, effectiveForce );
	queueResult = queued;
	if ( queued ) {
		pathNavigationState.lastGoal.origin = goalOrigin;
		pathNavigationState.lastGoal.isValid = true;
		pathNavigationState.lastGoal.isVisible = false;
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
		*    Record the last successfully followed goal for future rebuild heuristics.
		**/
		// Keep the last goal snapshot current so future forced rebuild checks have a stable reference.
		pathNavigationState.lastGoal.origin = goalOrigin;
		pathNavigationState.lastGoal.isValid = true;
		pathNavigationState.lastGoal.isVisible = ( activator != nullptr ) && SVG_Entity_IsVisible( this, activator );

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
	Vector3 faceGoal = QM_Vector3Subtract( goalOrigin, currentOrigin );
	faceGoal.z = 0.0f;
	const float faceLen2 = ( faceGoal.x * faceGoal.x ) + ( faceGoal.y * faceGoal.y );
	if ( faceLen2 > ( 0.001f * 0.001f ) ) {
		const Vector3 faceDir = QM_Vector3Normalize( faceGoal );
		ideal_yaw = QM_Vector3ToYaw( faceDir );
		yaw_speed = 10.0f;
		SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
	}

	// Keep the idle animation while we are stationary and waiting for a path.
	if ( rootMotionSet && rootMotionSet->motions[ 1 ] ) {
		skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 1 ];
		const double t = level.time.Seconds<double>();
		const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
		const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
		s.frame = rootMotion->firstFrameIndex + localFrame;
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
const bool svg_monster_testdummy_sfxfollow_t::TryRebuildNavigationInQueue( const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, const bool force ) {
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
	*        let callers keep using current path without forcing sync rebuilds.
	**/
	// Force bypass ensures explicit sound-goal refreshes can still queue new work.
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
   // Force bypass ensures explicit sound-goal refreshes bypass movement heuristics.
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
 /**
	*    Validate the requested sound goal against the navmesh before queuing async work.
	**/
	Vector3 adjusted_goal = goal_origin;
    if ( !Dummy_TryProjectGoalToWalkableZ( this, goal_origin, &adjusted_goal ) ) {
		// Fall back to our current feet Z when the goal is not projectable into a walkable layer.
		adjusted_goal.z = currentOrigin.z;
	}

   if ( SVG_Nav_IsRequestPending( &pathNavigationState.process ) ) {
		/**
		*    If a request is already pending we should not refresh it for small
		*    start-origin drifts. Only permit a non-forced refresh when the
		*    *goal* moved beyond the configured rebuild threshold. Forced
		*    replacements still cancel the outstanding request so a fresh entry
		*    is created immediately.
		**/
		if ( !force ) {
			// Choose the most appropriate reference goal snapshot available so
			// comparisons remain stable across lifecycle stages.
			const Vector3 &referenceGoal = pathNavigationState.lastGoal.isValid
				? pathNavigationState.lastGoal.origin
				: pathNavigationState.process.path_goal_position;
			// Compute the delta between the requested goal and the currently
			// tracked goal and compare against the policy's 2D rebuild radius.
			const double goalDx = QM_Vector3LengthDP( QM_Vector3Subtract( adjusted_goal, referenceGoal ) );
			if ( goalDx <= pathNavigationState.policy.rebuild_goal_dist_2d ) {
				if ( DUMMY_NAV_DEBUG ) {
					gi.dprintf( "[DEBUG] TryQueueNavRebuild: skipping refresh because pending_handle=%d and goalDx=%.2f <= thresh=%.2f ent=%d\n",
						pathNavigationState.process.pending_request_handle,
						goalDx,
						pathNavigationState.policy.rebuild_goal_dist_2d,
						s.number );
				}
				// Keep the existing in-flight request alive.
				return true;
			}
		} else {
			// Forced replacement: cancel the old request first so a fresh queued
			// entry can be created with the force flag set.
			ResetNavigationPath();
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
	constexpr double startIgnoreThresholdForQueue = 32.0;
	const nav_request_handle_t handle = SVG_Nav_RequestPathAsync( &pathNavigationState.process, start_origin, adjusted_goal, policy, agent_mins, agent_maxs, force, startIgnoreThresholdForQueue );
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
			const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, adjusted_goal, &agent_mins, &agent_maxs );
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
				start_origin.x, start_origin.y, start_origin.z,
				start_center.x, start_center.y, start_center.z,
				adjusted_goal.x, adjusted_goal.y, adjusted_goal.z,
				goal_center.x, goal_center.y, goal_center.z );
		} else {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) (no mesh)\n",
				start_origin.x, start_origin.y, start_origin.z,
				adjusted_goal.x, adjusted_goal.y, adjusted_goal.z );
		}
	}
	return true;
}
/**
*    @brief	Reset cached navigation path state for the test dummy.
*    @param	self	Monster whose path state should be cleared.
*    @note	Cancels any queued async request and clears cached path buffers.
**/
void svg_monster_testdummy_sfxfollow_t::ResetNavigationPath() {
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

	// Clear the last goal snapshot so heuristics treat the next goal as new and avoid suppressing rebuilds.
	pathNavigationState.lastGoal.origin = {};
	pathNavigationState.lastGoal.isValid = false;
	pathNavigationState.lastGoal.isVisible = false;
}

/**
*	@brief	Member wrapper that forwards to the TU-local AdjustGoalZBlendPolicy helper.
*	@param	goalOrigin	World-space feet-origin goal position used to bias layer selection.
*	@note	Called each think after `GenericThinkBegin()` to keep `pathNavigationState.policy`
*			tuned to current pursuit conditions (distance, vertical delta, failures, visibility).
**/
void svg_monster_testdummy_sfxfollow_t::AdjustGoalZBlendPolicy( const Vector3 &goalOrigin ) {
    /**
	*    Local geometry ratios used to derive blend thresholds from the active navmesh and movement policy.
	*        These remain dimensionless on purpose so the actual world-unit distances stay anchored to
	*        step sizes, cell sizes, and agent height instead of brittle per-map literals.
	**/
	constexpr double kBlendStartCellFraction = 0.5;
	constexpr double kBlendFullCellMultiplier = 2.0;
	constexpr double kBlendAggressiveFullCellMultiplier = 1.25;
	constexpr double kBlendAmbiguityCellMultiplier = 1.5;
	constexpr double kBlendFailureGoalCellMultiplier = 2.0;

	/**
	*    Cache policy/process state and gather the current goal geometry.
	**/
	auto &pathPolicy = pathNavigationState.policy;
	auto &pathProcess = pathNavigationState.process;
	const nav_mesh_t *mesh = g_nav_mesh.get();
	const double horizDist = std::sqrt( QM_Vector2DistanceSqr( goalOrigin, currentOrigin ) );
	const double deltaZ = ( double )goalOrigin.z - ( double )currentOrigin.z;
	const double fabsDeltaZ = std::fabs( deltaZ );

	/**
	*    Derive the movement and nav geometry scales that should drive Z-layer bias.
	*        We intentionally reuse step sizes, ground slack, mesh cell size, mesh quantization,
	*        and agent height so the policy adapts to the generated navmesh instead of fixed world values.
	**/
	const double stepMin = std::max( pathPolicy.min_step_height, PHYS_STEP_MIN_SIZE );
	const double stepMax = std::max( pathPolicy.max_step_height, PHYS_STEP_MAX_SIZE );
	const double groundSlack = std::max( PHYS_STEP_GROUND_DIST, stepMin * 0.125 );
	const double meshCellSize = ( mesh && mesh->cell_size_xy > 0.0 )
		? mesh->cell_size_xy
		: std::max( pathPolicy.waypoint_radius, NAV_DEFAULT_WAYPOINT_RADIUS );
	const double meshLayerQuant = ( mesh && mesh->z_quant > 0.0 )
		? mesh->z_quant
		: std::max( stepMin, PHYS_STEP_GROUND_DIST );
	const double meshStepHeight = ( mesh && mesh->max_step > 0.0 ) ? mesh->max_step : stepMax;
    const double policyAgentHeight = std::max( ( double )pathPolicy.agent_maxs.z - ( double )pathPolicy.agent_mins.z, 0.0 );
	const double entityAgentHeight = std::max( ( double )maxs.z - ( double )mins.z, 0.0 );
	const double meshAgentHeight = ( mesh != nullptr )
      ? std::max( ( double )mesh->agent_maxs.z - ( double )mesh->agent_mins.z, 0.0 )
		: 0.0;
	double agentHeight = std::max( policyAgentHeight, entityAgentHeight );
	agentHeight = std::max( agentHeight, meshAgentHeight );
	agentHeight = std::max( agentHeight, meshStepHeight * 2.0 );

	/**
	*    Derive resolution and failure heuristics from nearby geometry instead of the activator.
	*        The async worker already performs boundary-origin rescue and same-cell layer rescue,
	*        so this policy only needs to bias endpoint selection more aggressively when the goal
	*        geometry or recent failures suggest that those recovery paths will matter.
	**/
	const double nearLevelThreshold = meshStepHeight + groundSlack;
	const bool nearLevelGoal = ( fabsDeltaZ <= nearLevelThreshold );
	const bool recentFailure = ( pathProcess.consecutive_failures > 0 )
		&& ( ( level.time - pathProcess.last_failure_time ) <= 2_sec );
  const double failureGoalRadius = std::max( meshCellSize * kBlendFailureGoalCellMultiplier,
		std::max( pathPolicy.rebuild_goal_dist_3d, nearLevelThreshold + meshLayerQuant ) );
	const bool failureNearGoal = recentFailure
		&& ( QM_Vector3DistanceSqr( pathProcess.last_failure_pos, goalOrigin ) <= ( failureGoalRadius * failureGoalRadius ) );
 const bool likelyLayerAmbiguity = ( horizDist <= std::max( meshCellSize * kBlendAmbiguityCellMultiplier,
		pathPolicy.waypoint_radius + meshStepHeight ) )
		&& ( fabsDeltaZ > nearLevelThreshold );
	const bool aggressiveBlend = failureNearGoal || likelyLayerAmbiguity;

	/**
	*    Keep same-cell layer preference aligned with the actual goal elevation.
	*        Upward pursuits should prefer top layers, while downward pursuits should stop forcing
	*        top-layer bias once we know the goal sits beneath us.
	**/
	pathPolicy.layer_select_prefer_top = ( deltaZ >= 0.0 );
	const double preferThresholdUpperBound = std::max( nearLevelThreshold * 2.0, agentHeight * 0.5 );
	pathPolicy.layer_select_prefer_z_threshold = std::clamp( std::max( meshLayerQuant, nearLevelThreshold ),
		stepMin,
		preferThresholdUpperBound );

	/**
	*    Keep same-level goals on the current layer unless recent failures indicate we picked the wrong layer.
	*        This avoids unnecessary cross-floor bias for ordinary flat-ground movement while still allowing
	*        retry escalation near recently failing sound goals.
	**/
	if ( nearLevelGoal && !failureNearGoal ) {
		pathPolicy.enable_goal_z_layer_blend = false;
        pathPolicy.blend_start_dist = std::max( meshStepHeight, meshCellSize * kBlendStartCellFraction );
		pathPolicy.blend_full_dist = std::max( pathPolicy.blend_start_dist + meshStepHeight, NAV_DEFAULT_BLEND_DIST_FULL );
		return;
	}

	/**
	*    Build geometry-driven blend distances for genuine cross-layer goals.
	*        Start blending once we have moved about half a cell or a meaningful stair step away,
	*        then reach full bias over a distance derived from cell span, vertical delta, and agent height.
	**/
	pathPolicy.enable_goal_z_layer_blend = true;
   double blendStartDist = std::max( stepMin + groundSlack, meshCellSize * kBlendStartCellFraction );
	blendStartDist = std::min( blendStartDist, std::max( meshStepHeight, horizDist * 0.25 ) );
	blendStartDist = std::max( blendStartDist, stepMin );

    double blendFullDist = std::max( meshCellSize * kBlendFullCellMultiplier,
		std::max( fabsDeltaZ + meshLayerQuant, nearLevelThreshold + meshStepHeight ) );
	if ( deltaZ > 0.0 ) {
		blendFullDist = std::max( blendFullDist, horizDist * 0.75 );
	} else {
		blendFullDist = std::max( blendFullDist, horizDist * 0.6 );
	}
	blendFullDist = std::min( blendFullDist, std::max( meshCellSize * 4.0, agentHeight * 1.5 ) );
	blendFullDist = std::max( blendFullDist, blendStartDist + meshStepHeight );

	/**
	*    Escalate to faster goal-Z bias when geometry or recent failures look ambiguous.
	*        This is intentionally narrower than a full fallback strategy because the async worker already
	*        probes nearby boundary-origin nodes and rescues better-connected same-cell layers.
	**/
	if ( aggressiveBlend ) {
		blendStartDist = stepMin;
        blendFullDist = std::max( meshCellSize * kBlendAggressiveFullCellMultiplier,
			nearLevelThreshold + meshLayerQuant );
		blendFullDist = std::max( blendFullDist, blendStartDist + stepMin );
	}

	/**
	*    Commit the final geometry-driven blend distances.
	**/
	pathPolicy.blend_start_dist = blendStartDist;
	pathPolicy.blend_full_dist = blendFullDist;
}