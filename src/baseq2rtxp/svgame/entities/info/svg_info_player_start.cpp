/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_player_start'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level (Singleplayer, and as last resort if missing gamemode specific spawn points.).
*/
void SP_info_player_start( svg_base_edict_t *self ) {
    if ( !coop->value )
        return;
}

#include "svgame/svg_save.h"
#include "svgame/entities/info/svg_info_player_start.h"



/**
*
*
*
*   Save Descriptor Field Setup: svg_info_player_start_t
*
*
*
**/
/**
*   @brief  Save descriptor array definition for all the members of svg_info_player_start_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_info_player_start_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_info_player_start_t, preventCompilerError, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_info_player_start_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_info_player_start_t::Reset( const bool retainDictionary ) {
    // Call upon the base class.
    Base::Reset( retainDictionary );
    // Print
    gi.dprintf( "%s: Resetting classname: %s\n", __func__, classname );
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_info_player_start_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Base::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_info_player_start_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_info_player_start_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Base::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_info_player_start_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_info_player_start_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    // Ease of use.
    const std::string keyStr = keyValuePair->key;


    // Give Base class a shot.
    return Base::KeyValue( keyValuePair, errorStr );
}



/**
*
*   info_player_start:
*
**/
/**
*   @brief  Spawn routine.
**/
void svg_info_player_start_t::info_player_start_spawn( svg_info_player_start_t *self ) {

}