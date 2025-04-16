/********************************************************************
*
*
*	ServerGame: Edicts Entity Data
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


#include "svgame/svg_save.h"

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
    //! Constructor. 
    svg_player_edict_t() = default;
    //! Destructor.
    virtual ~svg_player_edict_t() = default;

    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_player_edict_t( const cm_entity_t *ed ) : svg_base_edict_t( ed ) { };



    /**
    *
    *
    *   TypeInfo Related:
    *
    *
    **/
    //! Custom player edict save descriptor field set.
    static svg_save_descriptor_field_t saveDescriptorFields[];

    //! Declare the save descriptor field handling function implementations.
    SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION();


    /**
    *
    *
    *   Core:
    *
    *
    **/
    /**
    *   Reconstructs the object, optionally retaining the entityDictionary.
    **/
    virtual void Reset( bool retainDictionary = false ) override;


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