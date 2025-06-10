/********************************************************************
*
*
*	ServerGame: Misc Entity 'misc_teleporter_dest'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/misc/svg_misc_teleporter_dest.h"

#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_means_of_death.h"


#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_misc_teleporter_dest_t )
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_teleporter_dest_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_teleporter_dest_t, testVar, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_teleporter_dest_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_misc_teleporter_dest_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_misc_teleporter_dest_t::Reset( const bool retainDictionary ) {
    // Call upon the base class.
    Super::Reset( retainDictionary );
    // Reset the edict's save descriptor fields.
    //testVar = 1337;
    //testVar2 = {};
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_misc_teleporter_dest_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_misc_teleporter_dest_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_misc_teleporter_dest_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_misc_teleporter_dest_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_misc_teleporter_dest_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}
#endif // #if 0



/*QUAKED misc_teleporter_dest (0 .5 .8) (-16 -16 0) (16 16 40)
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_misc_teleporter_dest_t, onSpawn ) ( svg_misc_teleporter_dest_t *self ) -> void {
    gi.setmodel( self, "models/objects/dmspot/tris.md2" );
    self->s.skinnum = 0;
    self->solid = SOLID_BOUNDS_BOX;
    self->s.renderfx |= RF_NOSHADOW;
    VectorSet( self->mins, -32, -32, -24 );
    VectorSet( self->maxs, 32, 32, -16 );
    gi.linkentity( self );
}
