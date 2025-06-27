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
        svg_info_null_t::onSpawn
    );

    /**
    *
    *   info_null:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_info_null_t, onSpawn );
};

