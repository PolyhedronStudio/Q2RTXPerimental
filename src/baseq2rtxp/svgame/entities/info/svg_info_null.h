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
*   @brief  Info point to point to.
*           Removes itself on spawn.
**/
struct svg_info_null_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_info_null_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_info_null_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_info_null_t() = default;

    /**
    *
    *	WorldSpawn Class Definition:
    *
    **/
    DefineWorldSpawnClass(
        // classname:    classType:         superClassType:
        "info_null", svg_info_null_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_info_null_t::info_null_spawn
    );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void info_null_spawn( svg_info_null_t *self );
};

/**
*   @brief  For func_groups.
*           Removes itself on spawn.
**/
struct svg_func_group_t : public svg_info_null_t {
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
    *	Define this as: "worldspawn" = svg_base_edict -> svg_worldspawn_edict_t
    *
    **/
    DefineWorldSpawnClass( "func_group", svg_func_group_t, svg_info_null_t, EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, svg_func_group_t::func_group_spawn );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void func_group_spawn( svg_func_group_t *self );
};

