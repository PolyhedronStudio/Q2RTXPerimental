/********************************************************************
*
*
*	ServerGame: Edicts Entity Data
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
*   Player Game Entity Structure:
*
*
**/
// This inherits from:
//
//struct svg_base_edict_t : public sv_shared_edict_t<svg_base_edict_t, svg_client_t>
struct svg_classtest_edict_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_classtest_edict_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_classtest_edict_t( const cm_entity_t *ed ) : svg_base_edict_t( ed ) { };
    //! Destructor.
    virtual ~svg_classtest_edict_t() = default;



    /**
    *
	*	Define this as: "classtest" = svg_base_edict -> svg_classtest_edict_t
    *
    **/
    DefineWorldSpawnClass( "classtest", svg_classtest_edict_t, svg_base_edict_t, EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn, svg_classtest_edict_t::classtest_edict_spawn  );



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
    *   @brief  
    **/
    //virtual void Spawn() override;
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
    *   Classtest:
    *
    **/
    static void classtest_edict_spawn( svg_classtest_edict_t *ent );


    /**
    *
    *
    *   Member Variables:
    *
    *
    **/
    // Player variables.
    int testVar = 100;
    Vector3 testVar2 = {};
};