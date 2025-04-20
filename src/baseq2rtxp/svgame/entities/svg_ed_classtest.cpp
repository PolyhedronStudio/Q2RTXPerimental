

/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_save.h"
#include "svgame/entities/svg_ed_classtest.h"



/**
*
*
*
*   Save Descriptor Field Setup: svg_classtest_edict_t
*
*
*
**/
/**
*   @brief  Save descriptor array definition for all the members of svg_classtest_edict_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_classtest_edict_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_classtest_edict_t, testVar, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_classtest_edict_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_classtest_edict_t, svg_base_edict_t );



/**
*   @brief
**/
//void svg_classtest_edict_t::Spawn() {
//
//}

/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_classtest_edict_t::Reset( const bool retainDictionary ) {
    // Call upon the base class.
    Base::Reset( retainDictionary );
    // Print
    gi.dprintf( "%s: Resetting classname: %s\n", __func__, classname );
    // Reset the edict's save descriptor fields.
    testVar = 1337;
    //testVar2 = {};
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_classtest_edict_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Base::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_classtest_edict_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_classtest_edict_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Base::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_classtest_edict_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_classtest_edict_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    // Nothing.
    return true;
}