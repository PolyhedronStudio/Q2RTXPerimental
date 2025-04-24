/********************************************************************
*
*
*	ServerGame: Info Null Edict.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


// Needed for save descriptor fields and functions.
#include "svgame/svg_save.h"

// Needed for inheritance.
#include "svgame/entities/svg_base_edict.h"

// Forward declare.
typedef struct cm_entity_s cm_entity_t;


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
    svg_info_notnull_t( const cm_entity_t *ed ) : Base( ed ) { };
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
        svg_info_notnull_t::info_notnull_spawn 
    );

    /**
    *
    *   info_player_start:
    *
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void info_notnull_spawn( svg_info_notnull_t *self );
};
