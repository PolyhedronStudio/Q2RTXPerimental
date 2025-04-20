/********************************************************************
*
*
*	ServerGame: TestDummy Monster Edict
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
    *	Define this as: "worldspawn" = svg_base_edict -> svg_ed_worldspawn_t
    *
    **/
    DefineWorldSpawnClass( "monster_testdummy_puppet", svg_monster_testdummy_t, svg_base_edict_t, EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn );



    /**
    *
    *   Save Descriptor Fields:
    *
    **/
    //! Custom player edict save descriptor field set.
    static svg_save_descriptor_field_t saveDescriptorFields[];

    //! Declare the save descriptor field handling function implementations.
    SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION();



    /**
    *
    *   Core:
    *
    **/
    /**
    *   @brief
    **/
    virtual void Spawn() override;
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
    virtual const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr );



    /**
    *
    *
    *   Member Variables:
    *
    *
    **/
    // Monster variables.
    int testVar = 100;
    Vector3 testVar2 = {};
};