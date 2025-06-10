/********************************************************************
*
*
*	ServerGame: Info Null Edict.
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
*   Info (Not-)Null Game Entity Structures:
*
*
**/
/**
*   @brief  Info point to point to.
**/
struct svg_info_notnull_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_info_notnull_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_info_notnull_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_info_notnull_t() = default;

    /**
    *
    *	WorldSpawn Class Definition:
    *
    **/
    DefineWorldSpawnClass( 
        // classname:    classType:         superClassType:
        "info_notnull", svg_info_notnull_t, svg_base_edict_t, 
		// typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, 
		// spawnFunc:
        svg_info_notnull_t::onSpawn 
    );

    /**
    *
    *   info_notnul:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_info_notnull_t, onSpawn );
};


/**
*   @brief  For func_groups.
*           Removes itself on spawn.
**/
struct svg_func_group_t : public svg_info_notnull_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_group_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_group_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_group_t() = default;

    /**
    *
    *	WorldSpawn Class Definition:
    *
    **/
    DefineWorldSpawnClass(
        // classname:    classType:         superClassType:
        "func_group", svg_func_group_t, svg_info_notnull_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_group_t::onSpawn
    );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_group_t, onSpawn );
};

