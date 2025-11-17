/********************************************************************
*
*
*	ServerGame: Misc Entity 'target_crosslevel_target'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"
#include "svgame/player/svg_player_hud.h"

#include "svgame/entities/target/svg_target_crosslevel_target.h"

#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_tempentity_events.h"


#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_target_crosslevel_target_t )
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_crosslevel_target_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_crosslevel_target_t, testVar, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_target_crosslevel_target_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_target_crosslevel_target_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_target_crosslevel_target_t::Reset( const bool retainDictionary ) {
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
void svg_target_crosslevel_target_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_target_crosslevel_target_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_target_crosslevel_target_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_target_crosslevel_target_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_target_crosslevel_target_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}
#endif // #if 0


/*QUAKED target_crosslevel_target (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW ORANGE FAT
When triggered, fires a laser.  You can either set a target
or a direction.
*/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_target_crosslevel_target_t, onSpawn ) ( svg_target_crosslevel_target_t *self ) -> void {
    // Always spawn Super class.
    Super::onSpawn( self );

    if ( !self->delay )
        self->delay = 1;
    self->svFlags = SVF_NOCLIENT;

    self->SetThinkCallback( &svg_target_crosslevel_target_t::onThink );
    self->nextthink = level.time + QMTime::FromSeconds( self->delay );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_target_crosslevel_target_t, onThink )( svg_target_crosslevel_target_t *self ) -> void {
    if ( self->spawnflags == ( game.serverflags & SFL_CROSS_TRIGGER_MASK & self->spawnflags ) ) {
        SVG_UseTargets( self, self, ENTITY_USETARGET_TYPE_TOGGLE, 1/*self->spawnflags & svg_target_crosslevel_target_t::SPAWNFLAG_START_ON*/ );
        SVG_FreeEdict( self );
    }
}
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_TOUCH( svg_target_crosslevel_target_t, onTouch )( svg_target_crosslevel_target_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void { }

/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_PAIN( svg_target_crosslevel_target_t, onPain )( svg_target_crosslevel_target_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void { }
/**
*   @brief
**/
//DEFINE_MEMBER_CALLBACK_DIE( svg_target_crosslevel_target_t, onDie )( svg_target_crosslevel_target_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void { }