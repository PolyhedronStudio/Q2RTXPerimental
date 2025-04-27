/********************************************************************
*
*
*	ServerGame: Base Player Edict
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
*   Player Game Entity Structure:
*
*
**/
// This inherits from:
//
//struct svg_base_edict_t : public sv_shared_edict_t<svg_base_edict_t, svg_client_t>
struct svg_player_edict_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_player_edict_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_player_edict_t( const cm_entity_t *ed ) : Base( ed ) { };
    //! Destructor.
    virtual ~svg_player_edict_t() = default;



    /**
    *
    *	Define this as: "player" = svg_base_edict -> svg_player_edict_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:    classType:         superClassType:
        "player", svg_player_edict_t, svg_base_edict_t, 
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_GameSpawn, 
        // spawnFunc:
        svg_player_edict_t::player_edict_spawn 
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
    *   Player
    * 
    **/
    /**
    *   @brief  Spawn routine.
    **/
    static void player_edict_spawn( svg_player_edict_t *self );


    /**
    *
    *   Member Variables:
    *
    **/
    // Player variables.
    int testVar = 100;
    Vector3 testVar2 = {};
};