/********************************************************************
*
*
*	ServerGame: TestDummy Debug NPC Edict (A* only)
*	File: svg_monster_testdummy_debug.h
*	Description:
*		Lightweight debug variant of the TestDummy that uses svg_monster_base_t
*		for A* navigation, corridor tracking, and physics execution.
*
*
********************************************************************/
#pragma once

// Monster Base
#include "svgame/entities/monster/svg_monster_base.h"

/**
*	@brief	Behavioral mood defining high-level monster temperament.
**/
enum class svg_monster_mood_type_t : uint32_t {
	//! Default pursuit/combat behavior.
	MOOD_TYPE_NORMAL = 0,
	//! Scared / Fleeing: constantly seeks crouch cover to hide from the player.
	MOOD_TYPE_SCARED = 1,
	//! Aggressive: relentless assault and flanking.
	MOOD_TYPE_AGGRESSIVE = 2
};

/**
*	@brief	Debug TestDummy Entity: always A* to activator
**/
struct svg_monster_testdummy_debug_t : public svg_monster_base_t {
	//! Default constructor.
	svg_monster_testdummy_debug_t() = default;
	//! Constructor with entityDictionary pointer.
	svg_monster_testdummy_debug_t( const cm_entity_t *ed ) : Super( ed ) {}
	//! Destructor.
	virtual ~svg_monster_testdummy_debug_t() = default;

	/**
	*	Register this spawn class as a world-spawnable entity.
	**/
	DefineWorldSpawnClass(
		"monster_testdummy_debug", svg_monster_testdummy_debug_t, svg_monster_base_t,
		EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
		svg_monster_testdummy_debug_t::onSpawn
	);

	/**
	*
	*	Save Descriptor Fields:
	*
	**/
	SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION();

	/**
	*
	*	Core:
	*
	**/
	//! Reconstructs the object, optionally retaining the entityDictionary.
	virtual void Reset( const bool retainDictionary = false ) override;
	//! Save the entity into a file using game_write_context.
	virtual void Save( struct game_write_context_t *ctx ) override;
	//! Restore the entity from a loadgame read context.
	virtual void Restore( struct game_read_context_t *ctx ) override;

	/**
	*
	*	Entity Callbacks:
	*
	**/
	DECLARE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn );
	DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_monster_testdummy_debug_t, onPostSpawn );
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink );
	DECLARE_MEMBER_CALLBACK_TOUCH( svg_monster_testdummy_debug_t, onTouch );
	DECLARE_MEMBER_CALLBACK_USE( svg_monster_testdummy_debug_t, onUse );
	DECLARE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_debug_t, onPain );
	DECLARE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_debug_t, onDie );

	const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) override;

	/**
	*
	*	Entity 'onThink' State Routines:
	*
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarToPlayer );
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarPursuitTrail );
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_InvestigateSound );
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Idle );
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_HideInCover );
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Dead );

	/**
	*
	*	Explicit NPC State Management:
	*
	**/
	//! Tracks whether the NPC has been enabled by the player.
	bool isActivated = false;
	//! Last server time when the activator was confirmed visible.
	QMTime lastPlayerVisibleTime = 0_ms;

	/**
	*	Explicit AI states.
	**/
	enum class AIThinkState {
		IdleLookout,
		PursuePlayer,
		PursueBreadcrumb,
		InvestigateSound,
		HideInCover
	};
	//! Determines the thinking state callback to fire for the frame.
	AIThinkState thinkAIState = AIThinkState::IdleLookout;

	/**
	*
	*	Animation Processing:
	*
	**/
	skm_rootmotion_set_t *rootMotionSet = nullptr;

	/**
	*	@brief	Update entity animation frame / state.
	*	@param	animId	Animation identifier (0=None, 1=Idle, 2=Walk, 3=Walk_Aim, 4=Run, 5=Duck_Idle, 6=Duck_Walk, 7=Dead).
	**/
	virtual void UpdateAnim( const int32_t animId ) override;

	/**
	*
	*	Behavioral NPC States:
	*
	**/
	/**
	*	@brief	State information for the idle lookout scanning behavior.
	**/
	struct StateIdleScan_t {
		//! Idle scan yaw direction (+1 / -1) used for lookout sweeping.
		double yawScanDirection = 1.0;
		//! Time when idle scan should flip direction.
		QMTime nextFlipTime = 0_ms;
		//! Current discrete idle heading index (0..7) used for 45deg stepped scanning.
		int32_t headingIndex = 0;
		//! Target yaw degrees for the current idle heading.
		float targetYaw = 0.0f;
	} stateIdleScan = {};

	/**
	*	@brief	Navigation state for when pursuing the activator's breadcrumb trail.
	**/
	struct StateNavigationTrail_t {
		//! Current breadcrumb we are attempting to chase when following the trail.
		svg_base_edict_t *targetEntity = nullptr;
		//! Time marker used by the player-trail system.
		QMTime trailTimeStamp = 0_ms;
	} stateNavigationTrail = {};

	/**
	*	@brief	Information about the most recently heard sound event.
	**/
	struct StateSoundScan_t {
		//! Cached investigation target from the latest heard sound event.
		Vector3 origin = { 0.0f, 0.0f, 0.0f };
		//! Whether stateSoundCan.origin currently contains a valid target.
		bool hasOrigin = false;
		//! Last processed sound timestamp so we do not re-investigate the same event.
		QMTime lastTime = 0_ms;
	} stateSoundCan = {};

	/**
	*	@brief	Current mood / behavioral temperament of the testdummy.
	**/
	svg_monster_mood_type_t mood = svg_monster_mood_type_t::MOOD_TYPE_SCARED;

	/**
	*	@brief	State information for tactical cover hiding when scared.
	**/
	struct StateCover_t {
		//! Index of currently claimed cover point in g_nav_cover_points (-1 if none).
		int32_t activeCoverIdx = -1;
		//! Resolved world-space feet position of the active cover point.
		Vector3 coverWorldPos = { 0.0f, 0.0f, 0.0f };
		//! Server time when the cover spot was selected.
		QMTime coverSelectTime = 0_ms;
		//! Server time of the next threat exposure check.
		QMTime nextExposureCheckTime = 0_ms;
		//! Whether the monster has arrived at the cover spot and is currently crouching.
		bool isHidingInCover = false;
		//! History of recently visited cover point indices to prevent ping-pong looping.
		int32_t recentCoverIndices[ 4 ] = { -1, -1, -1, -1 };
		//! Expiration timestamps for each banned recent cover point.
		QMTime recentCoverBanTimes[ 4 ] = { 0_ms, 0_ms, 0_ms, 0_ms };
		//! Circular index for recent cover history.
		int32_t recentCoverHead = 0;
		//! Timestamp of the last time we reacted to being caught or chased.
		QMTime lastCatchReactionTime = 0_ms;
		//! Server time when the next nervous peek direction should be chosen.
		QMTime nextPeekTime = 0_ms;
		//! Current targeted look yaw when peeking around nervously in cover.
		float peekTargetYaw = 0.0f;

		inline void BanRecentCover( const int32_t idx, const QMTime duration ) {
			if ( idx < 0 ) {
				return;
			}
			recentCoverIndices[ recentCoverHead ] = idx;
			recentCoverBanTimes[ recentCoverHead ] = level.time + duration;
			recentCoverHead = ( recentCoverHead + 1 ) % 4;
		}

		inline const bool IsCoverBanned( const int32_t idx ) const {
			if ( idx < 0 ) {
				return false;
			}
			for ( int32_t k = 0; k < 4; k++ ) {
				if ( recentCoverIndices[ k ] == idx && level.time < recentCoverBanTimes[ k ] ) {
					return true;
				}
			}
			return false;
		}
	} stateCover = {};

	/**
	*	@brief	Find the best tactical cover point prioritizing crouch cover over standing cover.
	*	@param	threat_origin	Position of the enemy/player to hide from.
	*	@return	Index of the chosen cover point in g_nav_cover_points, or -1 if none found.
	**/
	const int32_t FindBestScaredCover( const Vector3 &threat_origin );

	/**
	*	@brief	Checks for audible sounds and transitions to InvestigateSound if a fresh one is found.
	*	@return	True if a sound was heard and state changed, false otherwise.
	**/
	bool CheckForAudibleSounds();

	/**
	*
	*	Map Spawn Configurations:
	*
	**/
	//! Initial crowd parameters parsed from the map.
	svg_crowd_params_t initialCrowdParams = {};
	//! Initial formation style parsed from the map.
	crowd_chase_target_type_t initialCrowdStyle = crowd_chase_target_type_t::CROWD_STYLE_LINE;

	//! Offset into the entity's skin configstring for the skin to use when the dummy is part of a crowd.
	static constexpr int32_t CS_CUSTOMIMAGE_CROWD_NEUTRAL = 0;
	static constexpr int32_t CS_CUSTOMIMAGE_CROWD_ORANGE = 1;
	static constexpr int32_t CS_CUSTOMIMAGE_CROWD_BLUE = 2;
	static constexpr int32_t CS_CUSTOMIMAGE_CROWD_GREEN = 3;
	static constexpr int32_t CS_CUSTOMIMAGE_CROWD_MAX = CS_CUSTOMIMAGE_CROWD_GREEN;

	//! Precached image indices for crowd skins.
	static int32_t crowdSkinIndices[ CS_CUSTOMIMAGE_CROWD_MAX + 1 ];

	/**
	*	@brief	Register all crowd skins with the image precache system.
	**/
	static void RegisterCrowdIDSkins( void );
	/**
	*	@brief	Get the precached skin image index corresponding to a crowd group identifier.
	*	@param	crowdID	Crowd group ID.
	*	@return	Image index registered with gi.imageindex.
	**/
	static const int32_t GetCrowdSkinImageIndex( const int32_t crowdID );

	//! For when dummy is standing straight up.
	static constexpr const Vector3 DUMMY_BBOX_STANDUP_MINS	= PHYS_DEFAULT_BBOX_STANDUP_MINS;
	static constexpr const Vector3 DUMMY_BBOX_STANDUP_MAXS	= PHYS_DEFAULT_BBOX_STANDUP_MAXS;
	static constexpr const double DUMMY_VIEWHEIGHT_STANDUP	= PHYS_DEFAULT_VIEWHEIGHT_STANDUP;
	//! For when dummy is crouching.
	static constexpr const Vector3 DUMMY_BBOX_DUCKED_MINS	= PHYS_DEFAULT_BBOX_DUCKED_MINS;
	static constexpr const Vector3 DUMMY_BBOX_DUCKED_MAXS	= PHYS_DEFAULT_BBOX_DUCKED_MAXS;
	static constexpr const double DUMMY_VIEWHEIGHT_DUCKED	= PHYS_DEFAULT_VIEWHEIGHT_DUCKED;
	//! For when dummy is dead.
	static constexpr const Vector3 DUMMY_BBOX_DEAD_MINS		= { -16., -16., -36. };
	static constexpr const Vector3 DUMMY_BBOX_DEAD_MAXS		= { 16., 16., 8. };
	static constexpr double	DUMMY_VIEWHEIGHT_DEAD			= 8.;
};
