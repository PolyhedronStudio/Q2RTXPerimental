

/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_save.h"
#include "svgame/entities/svg_player_edict.h"



/**
*
* 
*
*   Save Descriptor Field Setup: svg_player_edict_t
*
* 
*
**/
/**
*   @brief  Save descriptor array definition for all the members of svg_player_edict_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_player_edict_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_player_edict_t, testVar, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_player_edict_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_player_edict_t, svg_base_edict_t );

/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_player_edict_t::Reset( bool retainDictionary ) {
    // Call upon the base class.
    svg_base_edict_t::Reset( retainDictionary );
	// Reset the edict's save descriptor fields.
    testVar = 1337;
    //testVar2 = {};
}

/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_player_edict_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    svg_base_edict_t::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_player_edict_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_player_edict_t::Restore( struct game_read_context_t *ctx ) {
	// Restore parent class fields.
    svg_base_edict_t::Restore( ctx );
	// Restore all the members of this entity type.
    ctx->read_fields( svg_player_edict_t::saveDescriptorFields, this );
}