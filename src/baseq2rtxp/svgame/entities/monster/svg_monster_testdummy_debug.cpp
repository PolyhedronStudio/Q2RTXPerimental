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
#include <algorithm>
#include <vector>

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


/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_debug_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_monster_testdummy_debug_t )
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, isActivated, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, lastPlayerVisibleTime, SD_FIELD_TYPE_FRAMETIME ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, thinkAIState, SD_FIELD_TYPE_INT32 ),

	// StateIdleScan_t
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateIdleScan.yawScanDirection, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateIdleScan.nextFlipTime, SD_FIELD_TYPE_FRAMETIME ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateIdleScan.headingIndex, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateIdleScan.targetYaw, SD_FIELD_TYPE_FLOAT ),

	// StateNavigationTrail_t
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateNavigationTrail.targetEntity, SD_FIELD_TYPE_EDICT ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateNavigationTrail.trailTimeStamp, SD_FIELD_TYPE_FRAMETIME ),

	// StateSoundScan_t
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_monster_testdummy_debug_t, stateSoundCan.origin, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateSoundCan.hasOrigin, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateSoundCan.lastTime, SD_FIELD_TYPE_FRAMETIME ),

	// mood
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, mood, SD_FIELD_TYPE_INT32 ),

	// stateCover
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.activeCoverIdx, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_monster_testdummy_debug_t, stateCover.coverWorldPos, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.coverSelectTime, SD_FIELD_TYPE_FRAMETIME ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.nextExposureCheckTime, SD_FIELD_TYPE_FRAMETIME ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.isHidingInCover, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_monster_testdummy_debug_t, stateCover.recentCoverIndices, SD_FIELD_TYPE_INT32, 16 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_monster_testdummy_debug_t, stateCover.recentCoverBanTimes, SD_FIELD_TYPE_FRAMETIME, 16 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.recentCoverHead, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.lastCatchReactionTime, SD_FIELD_TYPE_FRAMETIME ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.nextPeekTime, SD_FIELD_TYPE_FRAMETIME ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, stateCover.peekTargetYaw, SD_FIELD_TYPE_FLOAT ),

	// initialCrowdParams
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.lateralSpacing, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.longitudinalSpacing, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.maxCoverDistance, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.minCoverDistance, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.arrivalRadius, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.maxTimeToSeek, SD_FIELD_TYPE_FRAMETIME ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.orientationMode, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.fixedYaw, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.pathStaggerMs, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdParams.coverExclusionRadius, SD_FIELD_TYPE_DOUBLE ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_debug_t, initialCrowdStyle, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_monster_testdummy_debug_t, svg_monster_base_t );

//! Precached image indices for crowd skins.
int32_t svg_monster_testdummy_debug_t::crowdSkinIndices[ svg_monster_testdummy_debug_t::CS_CUSTOMIMAGE_CROWD_MAX + 1 ] = {};

/**
*	@brief	Register all crowd skins with the image precache system.
**/
void svg_monster_testdummy_debug_t::RegisterCrowdIDSkins( void ) {
	/**
	*	Register crowd skin textures into the engine's image table via gi.imageindex.
	*	This allocates slots in CS_IMAGES so clients precache them into image_precache.
	**/
	crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_NEUTRAL ] = gi.imageindex( "textures/models/testdummy/skin_grey.tga" );
	crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_ORANGE ]  = gi.imageindex( "textures/models/testdummy/skin_orange.tga" );
	crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_BLUE ]    = gi.imageindex( "textures/models/testdummy/skin_blue.tga" );
	crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_GREEN ]   = gi.imageindex( "textures/models/testdummy/skin_green.tga" );
}

/**
*	@brief	Get the precached skin image index corresponding to a crowd group identifier.
*	@param	crowdID	Crowd group ID (-1 or 0 for neutral, 1=orange, 2=blue, 3=green, etc.).
*	@return	Image index registered with gi.imageindex.
**/
const int32_t svg_monster_testdummy_debug_t::GetCrowdSkinImageIndex( const int32_t crowdID ) {
	/**
	*	Map crowd ID to the registered skin image index.
	**/
	// Neutral / civilian crowd group.
	if ( crowdID <= 0 ) {
		return crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_NEUTRAL ];
	}
	// Orange squad.
	if ( crowdID == CS_CUSTOMIMAGE_CROWD_ORANGE ) {
		return crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_ORANGE ];
	}
	// Blue squad.
	if ( crowdID == CS_CUSTOMIMAGE_CROWD_BLUE ) {
		return crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_BLUE ];
	}
	// Green squad.
	if ( crowdID == CS_CUSTOMIMAGE_CROWD_GREEN ) {
		return crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_GREEN ];
	}

	// For higher crowd IDs, fallback to blue squad skin.
	return crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_NEUTRAL ];
}


/**
*
*
*
*	Core:
*
*
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_monster_testdummy_debug_t::Reset( const bool retainDictionary ) {
    IMPLEMENT_EDICT_RESET_BY_COPY_ASSIGNMENT( Super, SelfType, retainDictionary );
}

/**
*   @brief  Save the entity into a file using game_write_context.
**/
void svg_monster_testdummy_debug_t::Save( struct game_write_context_t *ctx ) {
    Super::Save( ctx );
    ctx->write_fields( svg_monster_testdummy_debug_t::saveDescriptorFields, this );
}

/**
*   @brief  Restore the entity from a loadgame read context.
**/
void svg_monster_testdummy_debug_t::Restore( struct game_read_context_t *ctx ) {
    Super::Restore( ctx );
    ctx->read_fields( svg_monster_testdummy_debug_t::saveDescriptorFields, this );
}


/**
*
*
*
*	Entity Callbacks:
*
*
*
**/
// Navigation
#include "svgame/nav/nav_path.h"

// Crowd & Crew
#include "svgame/crowd/svg_crowd_manager.h"

static constexpr QMTime PATH_RECALC_INTERVAL_MS = 100_ms;



//! Optional debug toggle for emitting async queue statistics.
extern cvar_t *s_nav_nav_async_log_stats;

// Local debug toggle for noisy per-frame prints in this test monster.
#ifndef DEBUG_PRINTS
//#define DEBUG_PRINTS 0
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
		case svg_monster_testdummy_debug_t::AIThinkState::HideInCover:
			return "HideInCover";
		case svg_monster_testdummy_debug_t::AIThinkState::CrowdFormation:
			return "CrowdFormation";
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
		case svg_monster_testdummy_debug_t::AIThinkState::HideInCover:
			svg_monster_testdummy_debug_t::onThink_HideInCover( self );
			break;
		case svg_monster_testdummy_debug_t::AIThinkState::CrowdFormation:
			svg_monster_testdummy_debug_t::onThink_CrowdFormation( self );
			break;
		case svg_monster_testdummy_debug_t::AIThinkState::IdleLookout:
		default:
			svg_monster_testdummy_debug_t::onThink_Idle( self );
			break;
	}
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
**/
const bool svg_monster_testdummy_debug_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
	const std::string keyStr = keyValuePair->key;

	if ( keyStr == "crowd_id" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) ) {
		this->crowd.crowdID = keyValuePair->integer;
		return true;
	} else if ( keyStr == "crowd_role" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) ) {
		this->crowd.role = static_cast<crowd_member_role_t>( keyValuePair->integer );
		return true;
	} else if ( keyStr == "crowd_style" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) ) {
		this->initialCrowdStyle = static_cast<crowd_chase_target_type_t>( keyValuePair->integer );
		return true;
	} else if ( keyStr == "crowd_lat_spacing" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) ) {
		this->initialCrowdParams.lateralSpacing = static_cast<double>( keyValuePair->value );
		return true;
	} else if ( keyStr == "crowd_lon_spacing" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) ) {
		this->initialCrowdParams.longitudinalSpacing = static_cast<double>( keyValuePair->value );
		return true;
	} else if ( keyStr == "crowd_max_cover_dist" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) ) {
		this->initialCrowdParams.maxCoverDistance = static_cast<double>( keyValuePair->value );
		return true;
	} else if ( keyStr == "crowd_min_cover_dist" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) ) {
		this->initialCrowdParams.minCoverDistance = static_cast<double>( keyValuePair->value );
		return true;
	} else if ( keyStr == "crowd_arrival_radius" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) ) {
		this->initialCrowdParams.arrivalRadius = static_cast<double>( keyValuePair->value );
		return true;
	}

	return Super::KeyValue( keyValuePair, errorStr );
}

/**
*	@brief	For this debug variant, we override the spawn and think callbacks to always attempt async A* to the activator.
*			Spawn for debug testdummy: call base onSpawn then set think to our simple loop.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn )( svg_monster_testdummy_debug_t *self ) -> void {
	Super::onSpawn( self );

	/**
	*	Register member with crowd manager if assigned to a crowd group.
	**/
	// Register into squad or civilian group and configure parameters.
	if ( self->crowd.crowdID >= 0 ) {
		const int32_t cid = self->crowd.crowdID;
		const crowd_member_role_t role = self->crowd.role;
		SVG_Crowd_RegisterMember( self, cid );
		// Preserve role parsed from entity key values if explicitly assigned.
		if ( role != crowd_member_role_t::ROLE_UNASSIGNED ) {
			self->crowd.role = role;
		}
		// Apply map-defined crowd parameters and initial style to the squad group.
		if ( cid > 0 ) {
			SVG_Crowd_SetCrowdParams( cid, self->initialCrowdParams );
			if ( self->initialCrowdStyle != crowd_chase_target_type_t::CROWD_STYLE_ARROW ) {
				SVG_Crowd_SetCrowdStyle( cid, self->initialCrowdStyle );
			}
		}
	}

	/**
	*	Set custom skin renderfx and skinnum based on crowd membership.
	**/
	// Assign custom skin according to crowd membership (-1 and 0 default to neutral grey skin).
	self->s.renderfx |= RF_CUSTOMSKIN;
	self->s.skinnum = svg_monster_testdummy_debug_t::GetCrowdSkinImageIndex( self->crowd.crowdID );

	/**
	*    Basic entity type and movement properties.
	**/
	self->s.entityType = ET_MONSTER;

	self->solid = SOLID_CAPSULE;
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
	//self->pathNavigationState.policy.include_water = false;
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
		// Activated dummy always returns to mood == 0 (MOOD_TYPE_NORMAL) following behavior!
		self->mood = svg_monster_mood_type_t::MOOD_TYPE_NORMAL;
		self->mins = DUMMY_BBOX_STANDUP_MINS;
		self->maxs = DUMMY_BBOX_STANDUP_MAXS;
		self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
	} else {
		self->goalentity = nullptr;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
	}

	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*   @brief  Pain routine.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_debug_t, onPain )( svg_monster_testdummy_debug_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {
	if ( ( self->lifeStatus & LIFESTATUS_DEAD ) == LIFESTATUS_DEAD ) {
		return;
	}

	// Always trigger scared mood on hit
	const bool wasScared = ( self->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED );
	self->mood = svg_monster_mood_type_t::MOOD_TYPE_SCARED;

	if ( other && other->client ) {
		self->activator = other;
	} else if ( !self->activator ) {
		self->activator = g_edicts[ 1 ]; // Player
	}

	if ( !wasScared && self->activator && self->activator->client ) {
		gi.centerprintf( self->activator, "I'm scared!! GAAHH!!!" );
	}

	// If hit while occupying or running to a cover point, ban that compromised point
	if ( self->stateCover.activeCoverIdx >= 0 ) {
		self->stateCover.BanRecentCover( self->stateCover.activeCoverIdx, svg_monster_testdummy_debug_t::COVER_BAN_DURATION );
		Nav_SetCoverPointCooldown( self->stateCover.activeCoverIdx, 30_sec );
		Nav_ReleaseCoverPoint( self->stateCover.activeCoverIdx, self->s.number );
		self->stateCover.activeCoverIdx = -1;
	}

	self->stateCover.isHidingInCover = false;
	// Force immediate fresh cover evaluation on the very next think frame
	self->stateCover.nextExposureCheckTime = 0_ms;
	self->stateCover.nextRetreatProbeTime = 0_ms;
	self->ResetNavigationPath();
	Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::HideInCover );
	self->nextthink = level.time + FRAME_TIME_MS;
}


//=================================================================================================
//=================================================================================================


/**
*
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
	// Scared monsters prioritize tactical cover hiding and do not seek out sounds.
	if ( this->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED ) {
		return false;
	}

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
			// Nearby gunshot or explosive event: trigger mood == 1 (scared) and retreat to cover!
			this->mood = svg_monster_mood_type_t::MOOD_TYPE_SCARED;
			if ( audibleEntity->owner && audibleEntity->owner->client ) {
				this->activator = audibleEntity->owner;
			} else if ( !this->activator ) {
				this->activator = g_edicts[ 1 ]; // Player
			}

			if ( this->activator && this->activator->client ) {
				gi.centerprintf( this->activator, "I'm scared!! GAAHH!!!" );
			}

			if ( this->stateCover.activeCoverIdx >= 0 ) {
				this->stateCover.BanRecentCover( this->stateCover.activeCoverIdx, svg_monster_testdummy_debug_t::COVER_BAN_DURATION );
				Nav_SetCoverPointCooldown( this->stateCover.activeCoverIdx, 30_sec );
				Nav_ReleaseCoverPoint( this->stateCover.activeCoverIdx, this->s.number );
				this->stateCover.activeCoverIdx = -1;
			}

			this->stateCover.isHidingInCover = false;
			// Force immediate fresh cover evaluation on the very next think frame
			this->stateCover.nextExposureCheckTime = 0_ms;
			this->ResetNavigationPath();
			Dummy_SetState( this, svg_monster_testdummy_debug_t::AIThinkState::HideInCover );
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

    // If mood is scared, immediately divert to cover hiding behavior!
    if ( self->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED ) {
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::HideInCover );
        svg_monster_testdummy_debug_t::onThink_HideInCover( self );
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

    // Check for audible sounds before we move (optional realism step)
    if ( !activatorVisible && self->CheckForAudibleSounds() ) {
        return;
    }

    // If we can see the player and are in melee attack range (both 2D and Z), halt and attack/face.
    if ( activatorVisible && dist2d <= 44.0 && distZ < 48.0 ) {
        self->velocity.x = 0;
        self->velocity.y = 0;
        self->monsterMove.state.velocity.x = 0;
        self->monsterMove.state.velocity.y = 0;
        
		Vector3 dir = self->activator->currentOrigin - self->currentOrigin;
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
        // Direct A* pursuit to player's current origin via NavMesh path following.
        // MoveAStarToOrigin sets the ideal_yaw and velocity to navigate around walls and corners.
        self->MoveAStarToOrigin( self->activator->currentOrigin );
    }

    int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
    self->GenericThinkFinish( true, blockedMask );
    SVG_Util_SetEntityAngles( self, self->currentAngles, true );
    
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

    // If mood is scared, divert to cover hiding immediately.
    if ( self->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED ) {
        Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::HideInCover );
        svg_monster_testdummy_debug_t::onThink_HideInCover( self );
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
	// Track blocked/trapped streaks and force a route refresh if we keep scraping the same corner.
	self->UpdateBlockedNavigationRecovery( blockedMask );

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

	// If mood is scared, divert to cover hiding immediately.
	if ( self->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED ) {
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::HideInCover );
		svg_monster_testdummy_debug_t::onThink_HideInCover( self );
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
	// Track blocked/trapped streaks and force a route refresh if we keep scraping the same corner.
	self->UpdateBlockedNavigationRecovery( blockedMask );
	// Schedule the next think.
	self->nextthink = level.time + FRAME_TIME_MS;
}

//=================================================================================================

/**
*	@brief		Find the best tactical cover point prioritizing crouch cover over standing cover (Vector3DP precision).
*	@param	threat_origin	Position of the enemy/player to hide from in Vector3DP.
*	@return	Index of the chosen cover point in g_nav_cover_points, or -1 if none found.
**/
const int32_t svg_monster_testdummy_debug_t::FindBestScaredCover( const Vector3DP &threat_origin ) {
	/**
	*	Sanity checks: Ensure cover points exist in active navmesh.
	**/
	const int32_t num_points = Nav_GetCoverPointCount();
	if ( num_points <= 0 ) {
		return -1;
	}

	const Vector3DP monster_origin( currentOrigin );

	// Resolve threat horizontal forward aim direction.
	Vector3DP threat_forward{ 0.0, 0.0, 0.0 };
	if ( activator ) {
		Vector3 fwd = {};
		const Vector3 &threat_angles = ( activator->client )
			? activator->client->viewMove.viewAngles
			: activator->currentAngles;
		QM_AngleVectors( threat_angles, &fwd, nullptr, nullptr );
		fwd.z = 0.0f;
		if ( QM_Vector3LengthSqr( fwd ) > 0.001f ) {
			threat_forward = Vector3DP( QM_Vector3Normalize( fwd ) );
		}
	}
	const bool has_threat_forward = ( QM_Vector3LengthSqrDP( threat_forward ) > 0.001 );

	// Compute 2D retreat vector away from threat.
	Vector3DP to_threat = threat_origin - monster_origin;
	to_threat.z = 0.0;
	const double dist_to_threat = QM_Vector3LengthDP( to_threat );
	Vector3DP retreat_dir{ 0.0, 0.0, 0.0 };
	if ( dist_to_threat > 1.0 ) {
		retreat_dir = to_threat * ( -1.0 / dist_to_threat );
	}

	/**
	*	Evaluator lambda: strictly filters and scores candidate cover points using Vector3DP.
	*	Rejects banned points, points in the threat's forward aim cone, and points heading toward the threat.
	**/
	auto EvaluateCandidate = [&]( const int32_t cp_idx, double *out_score ) -> bool {
		// Reject if point was visited recently and remains banned.
		if ( stateCover.IsCoverBanned( cp_idx ) ) {
			return false;
		}

		const nav_cover_point_t *cp = Nav_GetCoverPoint( cp_idx );
		if ( !cp ) {
			return false;
		}

		Vector3DP world_pos = {}, world_normal = {};
		if ( !Nav_GetCoverPointWorldDP( *cp, &world_pos, &world_normal ) ) {
			return false;
		}

		// 1. Minimum threat distance check: do not choose cover right on top of the threat (within 128 units)
		Vector3DP to_threat_from_cover = threat_origin - world_pos;
		to_threat_from_cover.z = 0.0;
		const double dist_cover_to_threat = QM_Vector3LengthDP( to_threat_from_cover );
		if ( dist_cover_to_threat < 128.0 ) {
			return false;
		}

		// 2. Compute flee distance gain (positive = increases separation from threat)
		const double flee_gain = dist_cover_to_threat - dist_to_threat;

		// 3. Check movement vector toward cover point
		Vector3DP move_to_cover = world_pos - monster_origin;
		move_to_cover.z = 0.0;
		const double move_dist = QM_Vector3LengthDP( move_to_cover );
		double approach_threat_dot = 0.0;
		if ( move_dist > 1.0 && dist_to_threat > 1.0 ) {
			const Vector3DP move_dir = move_to_cover * ( 1.0 / move_dist );
			const Vector3DP threat_dir = to_threat * ( 1.0 / dist_to_threat );
			approach_threat_dot = QM_Vector3DotProductDP( move_dir, threat_dir );
		}

		// 4. Calculate tactical score: heavily favor deep flee progression, directional occlusion, and crouch posture
		double score = 500.0;
		score += std::clamp( flee_gain, -200.0, 1200.0 ) * 2.0;

		// Soft penalty for moves that temporarily head toward threat (e.g. escaping through a doorway where threat is nearby)
		if ( approach_threat_dot > 0.0 ) {
			score -= approach_threat_dot * 250.0;
		}

		// Prioritize cover points away from the player's direct forward gaze (-1 to +1 range mapped to 0..300)
		if ( has_threat_forward && dist_cover_to_threat > 1.0 ) {
			Vector3DP dir_threat_to_cover = world_pos - threat_origin;
			dir_threat_to_cover.z = 0.0;
			dir_threat_to_cover = QM_Vector3NormalizeDP( dir_threat_to_cover );
			const double aim_dot = QM_Vector3DotProductDP( threat_forward, dir_threat_to_cover );
			score += ( 1.0 - aim_dot ) * 150.0;
		}

		if ( dist_cover_to_threat > 1.0 ) {
			const Vector3DP to_threat_dir = to_threat_from_cover * ( 1.0 / dist_cover_to_threat );
			const double wall_alignment = QM_Vector3DotProductDP( to_threat_dir, world_normal * -1.0 );
			score += wall_alignment * 150.0;
		}

		if ( cp->cover_type == NAV_COVER_LOW ) {
			score += 150.0;
		}

		if ( out_score ) {
			*out_score = score;
		}
		return true;
	};

	auto PickBestCandidate = [&]( const std::vector<int32_t> &indices ) -> int32_t {
		std::vector<std::pair<int32_t, double>> valid_scored = {};
		for ( const int32_t idx : indices ) {
			double s = 0.0;
			if ( EvaluateCandidate( idx, &s ) ) {
				valid_scored.push_back( { idx, s } );
			}
		}

		if ( valid_scored.empty() ) {
			return -1;
		}

		std::sort( valid_scored.begin(), valid_scored.end(), []( const auto &a, const auto &b ) {
			return a.second > b.second;
		} );

		// Pick randomly among top 2 best candidates to add slight natural variety without picking poor spots.
		const int32_t top_count = std::min<int32_t>( 2, static_cast<int32_t>( valid_scored.size() ) );
		const int32_t chosen = ( top_count > 1 ) ? irandom( top_count ) : 0;
		return valid_scored[ chosen ].first;
	};

	/**
	*	Phase 1: Local Search (radius 768.0, centered on agent, all postures).
	**/
	std::vector<int32_t> candidate_indices = {};
	if ( Nav_FindCoverPoints( monster_origin, threat_origin, 768.0, s.number, &candidate_indices, NAV_COVER_NONE, threat_forward, 16 ) ) {
		const int32_t chosen = PickBestCandidate( candidate_indices );
		if ( chosen >= 0 ) {
			return chosen;
		}
	}

	/**
	*	Phase 2: Medium Vicinity Search (radius 1536.0, centered on agent, all postures).
	**/
	if ( Nav_FindCoverPoints( monster_origin, threat_origin, 1536.0, s.number, &candidate_indices, NAV_COVER_NONE, threat_forward, 24 ) ) {
		const int32_t chosen = PickBestCandidate( candidate_indices );
		if ( chosen >= 0 ) {
			return chosen;
		}
	}

	/**
	*	Phase 3: Deep World-Wide Search (radius 3072.0, centered on agent, all postures).
	**/
	if ( Nav_FindCoverPoints( monster_origin, threat_origin, 3072.0, s.number, &candidate_indices, NAV_COVER_NONE, threat_forward, 32 ) ) {
		const int32_t chosen = PickBestCandidate( candidate_indices );
		if ( chosen >= 0 ) {
			return chosen;
		}
	}

	// Do NOT fall back to banned cover points! Return -1 to allow the monster to sprint along the NavMesh into new map areas.
	return -1;
}

/**
*	@brief	Monster-specific edge cost evaluator for A* navigation pathfinding.
*	@details Overrides svg_monster_base_t to prioritize stair transitions when scared and fleeing.
*	@param	fromFaceIdx	Source polygon index.
*	@param	toFaceIdx	Target polygon index.
*	@param	he			Half-edge connecting fromFace to toFace.
*	@param	baseCost	Standard geometric cost (distance * slope * clearance).
*	@return	Adjusted edge traversal cost.
**/
double svg_monster_testdummy_debug_t::OnNavEvaluateEdgeCost( const int32_t fromFaceIdx, const int32_t toFaceIdx, const nav_halfedge_t &he, const double baseCost ) {
	// Base class applies corridor hysteresis (15% commitment discount for current corridor)
	double cost = svg_monster_base_t::OnNavEvaluateEdgeCost( fromFaceIdx, toFaceIdx, he, baseCost );

	// When scared and fleeing, prioritize vertical step transitions (staircases and stepped levels)
	if ( this->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED ) {
		const double zDelta = std::fabs( he.z_diff );
		if ( zDelta >= 4.0 && zDelta <= NAV_MAX_STEP_HEIGHT ) {
			// 20% stairway preference bonus to decisively break ties in favor of stairs
			cost *= 0.80;
		}
	}

	return cost;
}

/**
*	@brief	Cover hiding thinker: seeks and holds crouch cover when scared.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_HideInCover )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*	Generic think initialization.
	**/
	if ( !self->GenericThinkBegin() ) {
		return;
	}

	// Deactivate if not activated.
	if ( !self->isActivated ) {
		self->mins = DUMMY_BBOX_STANDUP_MINS;
		self->maxs = DUMMY_BBOX_STANDUP_MAXS;
		self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	// Disable gap jumping to avoid unnatural jumping while navigating to cover.
	self->pathNavigationState.policy.allow_gap_jumping = false;

	// Target threat is the player/activator.
	if ( !self->activator ) {
		self->stateNavigationTrail.targetEntity = nullptr;
		self->goalentity = nullptr;
		self->ResetNavigationPath();
		self->mins = DUMMY_BBOX_STANDUP_MINS;
		self->maxs = DUMMY_BBOX_STANDUP_MAXS;
		self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Check if caught by any player client:
	*	Being caught means in-sight from within a 128 unit radius.
	*	Only evaluate catch if actively hiding in cover, or after a catch reaction cooldown (1500ms).
	*	This prevents monsters who are already sprinting from thrashing/resetting their A* path every frame.
	**/
	// Catching only applies when actively crouching/hiding at a cover point.
	// When actively sprinting towards a goal (!isHidingInCover), the monster is already fleeing
	// and must NOT cancel its path or ban its destination when passing by the player.
	const bool can_be_caught = self->stateCover.isHidingInCover && ( level.time >= self->stateCover.lastCatchReactionTime + 1500_ms );
	svg_base_edict_t *catching_player = nullptr;

	if ( can_be_caught ) {
		for ( int32_t i = 1; i <= game.maxclients; i++ ) {
			svg_base_edict_t *player = g_edict_pool.EdictForNumber( i );
			if ( !player || !player->inUse || !player->client || player->health <= 0 ) {
				continue;
			}

			// Distance check: within 128 units
			const double dist = QM_Vector3DistanceDP( Vector3DP( self->currentOrigin ), Vector3DP( player->currentOrigin ) );
			if ( dist <= 128.0 ) {
				// In-sight check: direct line of sight between player and monster
				if ( SVG_Entity_IsVisible( player, self ) ) {
					catching_player = player;
					break;
				}
			}
		}
	}

	if ( catching_player ) {
		self->stateCover.lastCatchReactionTime = level.time;

		// Centerprint to the catching client
		gi.centerprintf( catching_player, "[LULZ]: Caught a scared testdummy :-(" );

		// Target threat is now the catching player
		self->activator = catching_player;

		// Ban old compromised cover point with long duration and release claim.
		if ( self->stateCover.activeCoverIdx >= 0 ) {
			self->stateCover.BanRecentCover( self->stateCover.activeCoverIdx, svg_monster_testdummy_debug_t::COVER_BAN_DURATION );
			Nav_SetCoverPointCooldown( self->stateCover.activeCoverIdx, 30_sec );
			Nav_ReleaseCoverPoint( self->stateCover.activeCoverIdx, self->s.number );
			self->stateCover.activeCoverIdx = -1;
		}

		// Stand up to run away to the new cover!
		self->stateCover.isHidingInCover = false;
		self->mins = DUMMY_BBOX_STANDUP_MINS;
		self->maxs = DUMMY_BBOX_STANDUP_MAXS;
		self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;
		self->ResetNavigationPath();

		// Immediately pick a new tactical cover spot away from this catching player
		const int32_t new_cover_idx = self->FindBestScaredCover( Vector3DP( catching_player->currentOrigin ) );
		if ( new_cover_idx >= 0 ) {
			const nav_cover_point_t *cp = Nav_GetCoverPoint( new_cover_idx );
			if ( cp ) {
				Vector3DP w_posDP = {}, w_normalDP = {};
				if ( Nav_GetCoverPointWorldDP( *cp, &w_posDP, &w_normalDP ) ) {
					Nav_ClaimCoverPoint( new_cover_idx, self->s.number, 15000_ms );
					self->stateCover.activeCoverIdx = new_cover_idx;
					self->stateCover.coverWorldPos = QM_Vector3FromDP( w_posDP );
					self->stateCover.coverSelectTime = level.time;
					self->stateCover.isHidingInCover = false;
					self->stateCover.nextExposureCheckTime = level.time + 1500_ms;
					self->goalentity = nullptr;
					self->ResetNavigationPath();

					// Immediately start running to new cover!
					self->MoveAStarToOrigin( self->stateCover.coverWorldPos, true );
				}
			}
		} else {
			// Rate-limit next cover search attempt if none could be found
			self->stateCover.nextExposureCheckTime = level.time + 500_ms;
		}
	}

	const Vector3 threat_origin = self->activator->currentOrigin;

	/**
	*	Direct line-of-sight & exposure check when sitting in cover:
	*	If the player spots the crouching testdummy, it screams "OH NOT AGAIN!@" and flees to another cover spot!
	**/
	bool spotted_in_cover = false;
	if ( self->stateCover.isHidingInCover && self->activator ) {
		Vector3 dummy_eye = self->currentOrigin;
		dummy_eye.z += self->viewheight;
		Vector3 player_eye = self->activator->currentOrigin;
		player_eye.z += ( ( self->activator->viewheight != 0.0f ) ? self->activator->viewheight : 22.0f );
		const svg_trace_t los_tr = SVG_Trace( dummy_eye, vec3_origin, vec3_origin, player_eye, self, CM_CONTENTMASK_OPAQUE );
		if ( los_tr.fraction >= 1.0f && !los_tr.startsolid && !los_tr.allsolid ) {
			// Direct unobstructed line-of-sight: we have been spotted!
			spotted_in_cover = true;
		}
	}

	/**
	*	Exposure and cover validity check:
	*	Re-evaluate cover if we don't have one (rate-limited), or when spotted in cover,
	*	or periodically once actively hiding in cover.
	**/
	const bool needs_cover_eval = ( self->stateCover.activeCoverIdx == -1 && level.time >= self->stateCover.nextExposureCheckTime ) ||
		spotted_in_cover ||
		( self->stateCover.isHidingInCover && level.time >= self->stateCover.nextExposureCheckTime );

	if ( needs_cover_eval && !catching_player ) {
		self->stateCover.nextExposureCheckTime = level.time + 1500_ms;

		bool cover_valid = false;
		if ( self->stateCover.activeCoverIdx >= 0 && !spotted_in_cover ) {
			// Check if cover point is still usable and protecting from the threat.
			const float protection = Nav_EvaluateCoverForThreat( self->stateCover.activeCoverIdx, threat_origin, true );
			if ( protection > 0.0f ) {
				cover_valid = true;
			}
		}

		// If current cover is invalid/exposed or not set, find a new crouch cover spot!
		if ( !cover_valid ) {
			if ( self->stateCover.isHidingInCover && self->activator ) {
				// Scream to the player when spotted while sitting at our cover spot!
				gi.centerprintf( self->activator, "OH NOT AGAIN!@" );
			}

			// Ban compromised cover point with long duration and place on cooldown.
			if ( self->stateCover.activeCoverIdx >= 0 ) {
				self->stateCover.BanRecentCover( self->stateCover.activeCoverIdx, svg_monster_testdummy_debug_t::COVER_BAN_DURATION );
				Nav_SetCoverPointCooldown( self->stateCover.activeCoverIdx, 30_sec );
				Nav_ReleaseCoverPoint( self->stateCover.activeCoverIdx, self->s.number );
				self->stateCover.activeCoverIdx = -1;
			}

			// Stand up for running to new cover.
			self->stateCover.isHidingInCover = false;
			self->mins = DUMMY_BBOX_STANDUP_MINS;
			self->maxs = DUMMY_BBOX_STANDUP_MAXS;
			self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;

			// Find best crouch cover spot away from threat.
			const int32_t new_cover_idx = self->FindBestScaredCover( Vector3DP( threat_origin ) );
			if ( new_cover_idx >= 0 ) {
				const nav_cover_point_t *cp = Nav_GetCoverPoint( new_cover_idx );
				if ( cp ) {
					Vector3DP w_posDP = {}, w_normalDP = {};
					if ( Nav_GetCoverPointWorldDP( *cp, &w_posDP, &w_normalDP ) ) {
						Nav_ClaimCoverPoint( new_cover_idx, self->s.number, 15000_ms );
						self->stateCover.activeCoverIdx = new_cover_idx;
						self->stateCover.coverWorldPos = QM_Vector3FromDP( w_posDP );
						self->stateCover.coverSelectTime = level.time;
						self->stateCover.isHidingInCover = false;
						self->stateCover.nextExposureCheckTime = level.time + 1500_ms;
						self->goalentity = nullptr;
						self->ResetNavigationPath();

						// Immediately start running to new cover!
						self->MoveAStarToOrigin( self->stateCover.coverWorldPos, true );
					}
				}
			} else {
				// Rate-limit next cover search attempt if none could be found
				self->stateCover.nextExposureCheckTime = level.time + 500_ms;
			}
		}
	}

	/**
	*	If no cover could be found, retreat to a safe walkable face away from the player into the map/world.
	**/
	if ( self->stateCover.activeCoverIdx < 0 ) {
		self->stateCover.isHidingInCover = false;
		self->mins = DUMMY_BBOX_STANDUP_MINS;
		self->maxs = DUMMY_BBOX_STANDUP_MAXS;
		self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;

		const Vector3DP monster_origin( self->currentOrigin );
		const Vector3DP threat_originDP( threat_origin );
		Vector3DP flee_dir = monster_origin - threat_originDP;
		flee_dir.z = 0.0;
		if ( QM_Vector3LengthSqrDP( flee_dir ) > 0.01 ) {
			flee_dir = QM_Vector3NormalizeDP( flee_dir );

			// Goal commitment: if already actively navigating toward an unreached retreat goal, continue along it
			bool has_active_retreat_goal = false;
			if ( self->pathNavigationState.lastGoal.isValid && !self->stringPulledPath.empty() ) {
				const Vector3 curGoal = self->pathNavigationState.lastGoal.origin;
				const float distToGoalSq = QM_Vector3DistanceSqr( self->currentOrigin, curGoal );
				if ( distToGoalSq > ( 32.0f * 32.0f ) ) {
					has_active_retreat_goal = true;
				}
			}

			if ( !has_active_retreat_goal ) {
				// Rate-limit swept retreat probing so we do not execute 15 physics sweeps on every 10ms frame
				if ( level.time >= self->stateCover.nextRetreatProbeTime ) {
					self->stateCover.nextRetreatProbeTime = level.time + 300_ms;

					// Determine retreat destination using step-aware and slope-aware swept shape probing
					Vector3 flee_goal = self->currentOrigin;
					bool found_flee_dest = false;

					// Try primary retreat direction (away from threat), then diagonal and side escape corridors.
					// If cornered in a dead-end corridor, probe past the threat through the entrance.
					const Vector3 f_dir = static_cast<Vector3>( flee_dir );
					const Vector3 l_dir = Vector3{ -f_dir.y, f_dir.x, 0.0f };
					const Vector3 r_dir = Vector3{ f_dir.y, -f_dir.x, 0.0f };
					const Vector3 bl_dir = QM_Vector3Normalize( Vector3{ f_dir.x + l_dir.x, f_dir.y + l_dir.y, 0.0f } );
					const Vector3 br_dir = QM_Vector3Normalize( Vector3{ f_dir.x + r_dir.x, f_dir.y + r_dir.y, 0.0f } );
					const Vector3 pass_l = QM_Vector3Normalize( Vector3{ -f_dir.x * 0.85f + l_dir.x * 0.25f, -f_dir.y * 0.85f + l_dir.y * 0.25f, 0.0f } );
					const Vector3 pass_r = QM_Vector3Normalize( Vector3{ -f_dir.x * 0.85f + r_dir.x * 0.25f, -f_dir.y * 0.85f + r_dir.y * 0.25f, 0.0f } );
					const Vector3 pass_fwd = Vector3{ -f_dir.x, -f_dir.y, 0.0f };

					const Vector3 retreat_dirs[ 8 ] = {
						f_dir,
						bl_dir,
						br_dir,
						l_dir,
						r_dir,
						pass_l,
						pass_r,
						pass_fwd
					};

					constexpr float flee_distances[ 4 ] = { 384.0f, 256.0f, 160.0f, 96.0f };
					for ( const Vector3 &r_dir_test : retreat_dirs ) {
						if ( found_flee_dest ) {
							break;
						}
						for ( const float dist : flee_distances ) {
							const Vector3 test_target = self->currentOrigin + ( r_dir_test * dist );
							Vector3 probe_ground = {};
							// StepProbe sweeps native analytical shape, checking step-ups over stairs/curbs and downward slopes
							if ( SVG_MMove_StepProbe( self->currentOrigin, self->mins, self->maxs, test_target, self, &probe_ground, self->pathNavigationState.policy.max_step_height, self->pathNavigationState.policy.max_drop_height ) ) {
								const float distFromMeSqr = QM_Vector3DistanceSqr( self->currentOrigin, probe_ground );
								if ( distFromMeSqr >= ( 48.0f * 48.0f ) ) {
									const int32_t face_idx = Nav_FindClosestFaceInLeaf( Vector3DP( probe_ground ) );
									if ( face_idx >= 0 && static_cast<size_t>( face_idx ) < g_nav_faces.size() ) {
										flee_goal = probe_ground;
										found_flee_dest = true;
										break;
									}
								}
							}
						}
					}

					if ( found_flee_dest ) {
						self->goalentity = nullptr;
						self->MoveAStarToOrigin( flee_goal );
						// Rate-limit cover query to 500ms so monster continuously seeks fresh cover as it transitions rooms
						self->stateCover.nextExposureCheckTime = level.time + 500_ms;
					} else {
						self->velocity.x = self->velocity.y = 0.0f;
						self->monsterMove.state.velocity.x = self->monsterMove.state.velocity.y = 0.0f;
						self->stateCover.nextExposureCheckTime = level.time + 1000_ms;
						self->stateCover.nextRetreatProbeTime = level.time + 500_ms;
					}
				}
			} else {
				// Continue navigating along the current committed retreat path
				self->MoveAStarToOrigin( self->pathNavigationState.lastGoal.origin );
			}
		}
		int32_t blockedMask = 0;
		self->GenericThinkFinish( true, blockedMask );
		self->currentAngles[ PITCH ] = 0.0f;
		self->currentAngles[ ROLL ] = 0.0f;
		SVG_Util_SetEntityAngles( self, self->currentAngles, true );
		self->UpdateBlockedNavigationRecovery( blockedMask );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	/**
	*	Navigate to the active cover spot or hold and crouch behind it.
	**/
	const Vector3DP to_cover = Vector3DP( self->stateCover.coverWorldPos ) - Vector3DP( self->currentOrigin );
	const double dist_to_cover_2d = std::sqrt( ( to_cover.x * to_cover.x ) + ( to_cover.y * to_cover.y ) );
	constexpr double arrival_threshold = 28.0;

	if ( !self->stateCover.isHidingInCover && dist_to_cover_2d <= arrival_threshold ) {
		// Arrived at cover: latch hiding state so entity does not flap back and forth in yaw/movement.
		self->stateCover.isHidingInCover = true;
	}

	if ( !self->stateCover.isHidingInCover ) {
		// Still en route to cover: stand up, run towards cover position using A*.
		self->mins = DUMMY_BBOX_STANDUP_MINS;
		self->maxs = DUMMY_BBOX_STANDUP_MAXS;
		self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;
		self->goalentity = nullptr;
		self->MoveAStarToOrigin( self->stateCover.coverWorldPos );
	} else {
		// Arrived at cover: halt movement and crouch!
		self->velocity.x = 0.0f;
		self->velocity.y = 0.0f;
		self->monsterMove.state.velocity.x = 0.0f;
		self->monsterMove.state.velocity.y = 0.0f;

		// Shrink bounding box to crouch height.
		self->mins = DUMMY_BBOX_DUCKED_MINS;
		self->maxs = DUMMY_BBOX_DUCKED_MAXS;
		self->viewheight = DUMMY_VIEWHEIGHT_DUCKED;

		// If vision of the entity is obstructed behind cover, randomly peek around interpolating yaw from and to various directions, pretending to be scared!
		const nav_cover_point_t *cp = Nav_GetCoverPoint( self->stateCover.activeCoverIdx );
		if ( cp ) {
			Vector3DP w_posDP = {}, w_normalDP = {};
			if ( Nav_GetCoverPointWorldDP( *cp, &w_posDP, &w_normalDP ) ) {
				const float base_yaw = static_cast<float>( QM_Vector3ToYawDP( w_normalDP ) );

				// Pick a new nervous look direction at randomized intervals (400ms - 900ms)
				if ( level.time >= self->stateCover.nextPeekTime ) {
					// Random peek angle offset between -55 and +55 degrees relative to cover outward normal
					const float angle_offset = crandom_openf() * 55.0f;
					self->stateCover.peekTargetYaw = QM_AngleMod( base_yaw + angle_offset );
					self->stateCover.nextPeekTime = level.time + random_time( 400_ms, 900_ms );
				}

				// Smoothly interpolate ideal_yaw toward peekTargetYaw
				self->ideal_yaw = self->stateCover.peekTargetYaw;
				self->yaw_speed = 18.0f;
				self->currentAngles[ PITCH ] = 0.0f;
				self->currentAngles[ ROLL ] = 0.0f;
				SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );
			}
		}

		// Play crouch idle animation.
		if ( self->rootMotionSet && self->rootMotionSet->motions[ 1 ] ) {
			skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 1 ]; // IDLE
			const double t = level.time.Seconds<double>();
			const int32_t animFrame = ( int32_t )std::floor( ( float )( t * 40.0f ) );
			const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
			self->s.frame = rootMotion->firstFrameIndex + localFrame;
		}
	}

	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	self->GenericThinkFinish( true, blockedMask );
	self->currentAngles[ PITCH ] = 0.0f;
	self->currentAngles[ ROLL ] = 0.0f;
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );
	self->UpdateBlockedNavigationRecovery( blockedMask );
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

	/**
	*	Check active crowd / tactical squad movement orders:
	**/
	if ( self->crowd.crowdID >= 0 ) {
		const svg_crowd_group_t *crowdGroup = SVG_Crowd_GetGroup( self->crowd.crowdID );
		if ( crowdGroup && ( crowdGroup->isMoving || self->crowd.slotIndex >= 0 ) ) {
			Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::CrowdFormation );
			svg_monster_testdummy_debug_t::onThink_CrowdFormation( self );
			return;
		}
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
		if ( self->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED ) {
			Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::HideInCover );
		} else {
			// Set the nextThink to AStarToPlayer so we start chasing the player right away using the navmesh.
			Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::PursuePlayer );
		}
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
*	@brief	Handles movement, station-keeping, and tactical cover for entities in a squad formation.
*	@param	self	Test dummy entity executing think cycle.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_CrowdFormation )( svg_monster_testdummy_debug_t *self ) -> void {
	/**
	*	Sanity checks / early returns: ensure entity pointer is valid and alive.
	**/
	if ( !self->GenericThinkBegin() ) {
		return;
	}

	/**
	*	Validate active crowd membership:
	*	If no longer assigned to any crowd group, immediately return to standard idle lookout.
	**/
	if ( self->crowd.crowdID < 0 ) {
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	// Retrieve active crowd coordination group record.
	const svg_crowd_group_t *group = SVG_Crowd_GetGroup( self->crowd.crowdID );
	if ( !group ) {
		Dummy_SetState( self, svg_monster_testdummy_debug_t::AIThinkState::IdleLookout );
		self->nextthink = level.time + FRAME_TIME_MS;
		return;
	}

	const Vector3 goalOrigin = self->crowd.assignedGoalOrigin;
	const double arrivalRadius = ( group->params.arrivalRadius > 0.0 ) ? std::max( group->params.arrivalRadius, group->params.separationRadius ) : CROWD_DEFAULT_ARRIVAL_RADIUS;

	// Vector to assigned formation slot
	Vector3 toGoal = goalOrigin - self->currentOrigin;
	const double zDiff = std::fabs( toGoal.z );
	toGoal.z = 0.0f;
	const double dist2D = QM_Vector3Length( toGoal );

	/**
	*	Arrival check: determine whether we have arrived within the slot tolerance circle.
	**/
	if ( dist2D <= arrivalRadius && zDiff <= CROWD_ARRIVAL_MAX_Z_DIFF ) {
		self->crowd.reachedGoal = true;
		self->crowd.blockedStartTime = 0_ms;
	} else if ( dist2D <= ( arrivalRadius * CROWD_BLOCKED_ARRIVAL_RADIUS_FACTOR ) && zDiff <= CROWD_ARRIVAL_MAX_Z_DIFF ) {
		// Only treat as arrived if the agent has been continuously stalled near its slot for a sustained duration (2.5s)
		const double horizSpeedSq = ( self->velocity.x * self->velocity.x ) + ( self->velocity.y * self->velocity.y );
		if ( horizSpeedSq < CROWD_BLOCKED_STATIONARY_SPEED_SQ ) {
			if ( self->crowd.blockedStartTime == 0_ms ) {
				self->crowd.blockedStartTime = level.time;
			} else if ( ( level.time - self->crowd.blockedStartTime ) >= CROWD_BLOCKED_ARRIVAL_STALL_TIME ) {
				self->crowd.reachedGoal = true;
			}
		} else {
			self->crowd.blockedStartTime = 0_ms;
		}
	} else {
		self->crowd.reachedGoal = false;
		self->crowd.blockedStartTime = 0_ms;
	}

	/**
	*	Behavior execution: arrived station-keeping vs en-route navigation.
	**/
	if ( self->crowd.reachedGoal ) {
		// Stop horizontal movement
		self->velocity.x = 0.0f;
		self->velocity.y = 0.0f;
		self->monsterMove.state.velocity.x = 0.0f;
		self->monsterMove.state.velocity.y = 0.0f;

		// Orient to slot's prescribed relative heading (or match group heading)
		double desiredYaw = group->currentHeadingYaw;
		if ( self->crowd.slotIndex >= 0 && self->crowd.slotIndex < static_cast<int32_t>( group->slots.size() ) ) {
			desiredYaw = QM_AngleMod( group->currentHeadingYaw + group->slots[ self->crowd.slotIndex ].relativeYawDeg );
		}
		self->ideal_yaw = static_cast<float>( desiredYaw );
		SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, 45.0f );

		// If occupying a tactical cover point, crouch into ducked idle
		if ( self->crowd.activeCoverIdx >= 0 ) {
			self->UpdateAnim( 5 ); // DUCK_IDLE
			self->mins = DUMMY_BBOX_DUCKED_MINS;
			self->maxs = DUMMY_BBOX_DUCKED_MAXS;
			self->viewheight = static_cast<float>( DUMMY_VIEWHEIGHT_DUCKED );
		} else {
			self->UpdateAnim( 1 ); // IDLE
			self->mins = DUMMY_BBOX_STANDUP_MINS;
			self->maxs = DUMMY_BBOX_STANDUP_MAXS;
			self->viewheight = static_cast<float>( DUMMY_VIEWHEIGHT_STANDUP );
		}
	} else {
		/**
		*	En route: drive movement to assigned slot using navigation mesh.
		*	Pass force = false to allow ComputePathTo to reuse the cached path corridor!
		**/
		self->mins = DUMMY_BBOX_STANDUP_MINS;
		self->maxs = DUMMY_BBOX_STANDUP_MAXS;
		self->viewheight = static_cast<float>( DUMMY_VIEWHEIGHT_STANDUP );

		self->MoveAStarToOrigin( goalOrigin, false );
	}

	// Execute standard physics slide move and angle synchronization.
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;
	self->GenericThinkFinish( true, blockedMask );
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

	// Throttle think frequency when arrived and located far from the player to conserve CPU.
	if ( self->crowd.reachedGoal ) {
		const svg_base_edict_t *player = g_edict_pool.EdictForNumber( 1 );
		if ( player && SVG_Entity_IsActive( player ) ) {
			const double distToPlayerSq = QM_Vector3DistanceSqr( self->currentOrigin, player->currentOrigin );
			if ( distToPlayerSq > ( CROWD_DORMANT_THROTTLE_DIST * CROWD_DORMANT_THROTTLE_DIST ) ) {
				self->nextthink = level.time + CROWD_THROTTLE_THINK_INTERVAL;
				return;
			}
		}
	}

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

/**
*	@brief	Update entity animation frame / state.
*	@param	animId	Animation identifier (0=None, 1=Idle, 2=Walk, 3=Walk_Aim, 4=Run, 5=Duck_Idle, 6=Duck_Walk, 7=Dead).
**/
void svg_monster_testdummy_debug_t::UpdateAnim( const int32_t animId ) {
	const double t = level.time.Seconds<double>();
	const int32_t animFrameGlobal = static_cast<int32_t>( std::floor( static_cast<float>( t * 40.0f ) ) );
	if ( this->rootMotionSet != nullptr && animId >= 0 && this->rootMotionSet->motions[ animId ] != nullptr ) {
		skm_rootmotion_t *rootMotion = this->rootMotionSet->motions[ animId ];
		const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrameGlobal % rootMotion->frameCount ) : 0;
		this->s.frame = rootMotion->firstFrameIndex + localFrame;
	}
}

