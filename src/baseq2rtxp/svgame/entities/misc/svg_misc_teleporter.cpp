/********************************************************************
*
*
*	ServerGame: Misc Entity 'misc_teleporter'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/misc/svg_misc_teleporter.h"

#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_means_of_death.h"


#if 0
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_misc_teleporter_t )
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_teleporter_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_teleporter_t, testVar, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_DEFINE_FIELD( svg_misc_teleporter_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_misc_teleporter_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_misc_teleporter_t::Reset( const bool retainDictionary ) {
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
void svg_misc_teleporter_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_misc_teleporter_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_misc_teleporter_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_misc_teleporter_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_misc_teleporter_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}
#endif // #if 0



/*QUAKED misc_teleporter (0 .5 .8) (-16 -16 0) (16 16 40)
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/
/**
*   @brief  Non member callback, because the trig is an svg_base_edict_t.
**/
DECLARE_GLOBAL_CALLBACK_TOUCH( svg_misc_teleporter_onTouch );
DEFINE_GLOBAL_CALLBACK_TOUCH( svg_misc_teleporter_onTouch )( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    svg_base_edict_t *dest;
    int         i;

    if ( !other->client )
        return;
    dest = SVG_Entities_Find( NULL, q_offsetof( svg_base_edict_t, targetname ), (const char *)self->targetNames.target );
    if ( !dest ) {
        gi.dprintf( "Couldn't find destination\n" );
        return;
    }

    // unlink to make sure it can't possibly interfere with SVG_Util_KillBox
    gi.unlinkentity( other );

    VectorCopy( dest->s.origin, other->s.origin );
    VectorCopy( dest->s.origin, other->s.old_origin );
    other->s.origin[ 2 ] += 10;

    // clear the velocity and hold them in place briefly
    VectorClear( other->velocity );
    other->client->ps.pmove.pm_time = 160 >> 3;     // hold time
    other->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

    // draw the teleport splash at source and on the player
    SVG_Util_AddEvent( self->owner, EV_PLAYER_TELEPORT, 0 );//self->owner->s.event = EV_PLAYER_TELEPORT;
    SVG_Util_AddEvent( other, EV_PLAYER_TELEPORT, 0 );//other->s.event = EV_PLAYER_TELEPORT;

    // set angles
    for ( i = 0; i < 3; i++ ) {
        other->client->ps.pmove.delta_angles[ i ] = /*ANGLE2SHORT*/QM_AngleMod( dest->s.angles[ i ] - other->client->resp.cmd_angles[ i ] );
    }

    VectorClear( other->s.angles );
    VectorClear( other->client->ps.viewangles );
    VectorClear( other->client->viewMove.viewAngles );
    QM_AngleVectors( other->client->viewMove.viewAngles, &other->client->viewMove.viewForward, nullptr, nullptr );

    // kill anything at the destination
    SVG_Util_KillBox( other, !!other->client, MEANS_OF_DEATH_TELEFRAGGED );

    gi.linkentity( other );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_misc_teleporter_t, onSpawn ) ( svg_misc_teleporter_t *self ) -> void {
    svg_base_edict_t *trig;

    if ( !self->targetNames.target ) {
        gi.dprintf( "teleporter without a target.\n" );
        SVG_FreeEdict( self );
        return;
    }

    gi.setmodel( self, "models/objects/dmspot/tris.md2" );
    self->s.skinnum = 1;
    self->s.effects = EF_TELEPORTER;
    self->s.renderfx = RF_NOSHADOW;
    //self->s.sound = gi.soundindex("world/amb10.wav");
    self->solid = SOLID_BOUNDS_BOX;

    VectorSet( self->mins, -32, -32, -24 );
    VectorSet( self->maxs, 32, 32, -16 );
    gi.linkentity( self );

    trig = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
    trig->SetTouchCallback( &svg_misc_teleporter_onTouch );
    trig->solid = SOLID_TRIGGER;
    trig->s.entityType = ET_TELEPORT_TRIGGER;
    trig->targetNames.target = self->targetNames.target;
    trig->owner = self;
    VectorCopy( self->s.origin, trig->s.origin );
    VectorSet( trig->mins, -8, -8, 8 );
    VectorSet( trig->maxs, 8, 8, 24 );
    gi.linkentity( trig );
}
