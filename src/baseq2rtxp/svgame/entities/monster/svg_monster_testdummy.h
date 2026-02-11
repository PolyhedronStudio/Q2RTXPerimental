/********************************************************************
*
*
*	ServerGame: TestDummy Monster Edict
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


// Needed.
#include "svgame/entities/svg_base_edict.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_path_process.h"


/**
*
*
*   TestDummy Entity Monster Structure:
*
*
**/
struct svg_monster_testdummy_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_monster_testdummy_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_monster_testdummy_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_monster_testdummy_t() = default;


    /**
    *
    *	Define this as: "monster_testdummy_puppet" = svg_base_edict -> svg_monster_testdummy_t
    *
    **/
    DefineWorldSpawnClass( 
        // classname:               classType:               superClassType:
        "monster_testdummy_puppet", svg_monster_testdummy_t, svg_base_edict_t, 
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, 
        // spawnFunc:
        svg_monster_testdummy_t::onSpawn 
    );



    /**
    *
    *   Save Descriptor Fields:
    *
    **/
    //! Declare the save descriptor field handling function implementations.
    SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION();



    /**
    *
    *   Core:
    *
    **/
    /**
    *   Reconstructs the object, optionally retaining the entityDictionary.
    **/
    virtual void Reset( const bool retainDictionary = false ) override;

    /**
    *   @brief  Save the entity into a file using game_write_context.
    *   @note   Make sure to call the base parent class' Restore() function.
    **/
    virtual void Save( struct game_write_context_t *ctx ) override;
    /**
    *   @brief  Restore the entity from a loadgame read context.
    *   @note   Make sure to call the base parent class' Restore() function.
    **/
    virtual void Restore( struct game_read_context_t *ctx ) override;

    /**
    *   @brief  Called for each cm_entity_t key/value pair for this entity.
    *           If not handled, or unable to be handled by the derived entity type, it will return
    *           set errorStr and return false. True otherwise.
    **/
    virtual const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) override;



    /**
    *
    *   TestDummy Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_monster_testdummy_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_t, onThink );

    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_monster_testdummy_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_monster_testdummy_t, onUse );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_t, onPain );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_t, onDie );



	/**
	*
	*	Think Actions:
	*
	**/
	/**
	*	@brief	Path following using A* (trail/noise).
	*	@return	True if path following is possible, false if should deactivate.
	**/
	const bool ThinkPathFollow();

	/**
	*	@brief	Try to pathfind to the player using A* (even if not visible).
	*	@return	True if a path is found and movement is set, false otherwise.
	**/
	const bool ThinkAStarToPlayer();

	/**
	*	@brief	Direct pursuit of the player (LOS).
	**/
	void ThinkDirectPursuit();
	/**
	*	@brief	Follow the player trail (breadcrumbs).
	*	@return	True if a trail spot is found and movement is set, false otherwise.
	**/
	const bool ThinkFollowTrail();
	/**
	*	@brief	Idle behavior: stop movement and play idle animation.
	**/
	void ThinkIdle();

	/**
	*    @brief    Helper: compute an absolute frame index for a root motion animation.
	*    @note    Uses the global `level.time` to choose the frame based on `animHz`.
	*/
	int32_t ComputeAnimFrameFromRootMotion( const skm_rootmotion_t *rootMotion, float animHz ) const;

	/**
	*    @brief    Helper: face a horizontal target smoothly by blending between a hint direction and the exact target direction.
	*/
	void FaceTargetHorizontal( const Vector3 &directionHint, const Vector3 &targetPoint, float blendFactor, float yawSpeed );



    /**
    *
    *   Member Variables:
    *
    **/
    // For savegame/loadgame testing hehe :-)
    double summedDistanceTraversed = 0.f;
    // Monster variables.
    int testVar = 100;
    Vector3 testVar2 = {};
    //---------------------------
    // <TEMPORARY FOR TESTING>
    //---------------------------
    //static sg_skm_rootmotion_set_t rootMotionSet;
    skm_rootmotion_set_t *rootMotionSet = nullptr;
    
    /**
    *   Navigation path following state:
    **/
    svg_nav_path_process_t navPathProcess = {};
    svg_nav_path_policy_t navPathPolicy = {};
    //! Most recent sound_entity noise time this monster consumed.
    QMTime last_sound_time_seen = 0_ms;
    //! Most recent sound2_entity noise time this monster consumed.
    QMTime last_sound2_time_seen = 0_ms;
    //! Last heard sound origin (set by sound handling code if available).
    Vector3 last_sound_origin = {};
    //! Last heard sound2 origin.
    Vector3 last_sound2_origin = {};
	//! Cached last valid 3D navigation direction (used as a short fallback when queries fail).
	Vector3 last_nav_dir3d = {};
	//! Time at which the cached navigation direction was last updated.
	QMTime last_nav_dir_time = 0_ms;

    
    /**
    *
    *   Const Expressions:
    *
    **/
    //! For when dummy is standing straight up.
    static constexpr Vector3 DUMMY_BBOX_STANDUP_MINS = { -16.f, -16.f, 0.f };
    static constexpr Vector3 DUMMY_BBOX_STANDUP_MAXS = { 16.f, 16.f, 72.f };
    static constexpr float   DUMMY_VIEWHEIGHT_STANDUP = 30.f;
    //! For when dummy is crouching.
    static constexpr Vector3 DUMMY_BBOX_DUCKED_MINS = { -16.f, -16.f, -36.f };
    static constexpr Vector3 DUMMY_BBOX_DUCKED_MAXS = { 16.f, 16.f, 8.f };
    static constexpr float   DUMMY_VIEWHEIGHT_DUCKED = 4.f;
    //! For when dummy is dead.
    static constexpr Vector3 DUMMY_BBOX_DEAD_MINS = { -16.f, -16.f, -36.f };
    static constexpr Vector3 DUMMY_BBOX_DEAD_MAXS = { 16.f, 16.f, 8.f };
    static constexpr float   DUMMY_VIEWHEIGHT_DEAD = 8.f;
};
