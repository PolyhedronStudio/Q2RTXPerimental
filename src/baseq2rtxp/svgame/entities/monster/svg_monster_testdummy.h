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


/**
*
*
*   Player Start Game Entity Structures:
*
*
**/
// This inherits from:
//
//struct svg_base_edict_t : public sv_shared_edict_t<svg_base_edict_t, svg_client_t>
struct svg_monster_testdummy_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_monster_testdummy_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_monster_testdummy_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_monster_testdummy_t() = default;


    /**
    *
    *
    *   Const Expressions:
    *
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



    /**
    *
    *	Define this as: "worldspawn" = svg_base_edict -> svg_worldspawn_edict_t
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
    *   TestDummy
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
    ///**
    //*   @brief
    //**/
    //static void monster_testdummy_puppet_die( svg_monster_testdummy_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_t, onDie );



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
};