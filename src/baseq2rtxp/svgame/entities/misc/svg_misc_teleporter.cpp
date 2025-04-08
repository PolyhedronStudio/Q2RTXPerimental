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


/**
*   @brief
**/
void teleporter_touch( edict_t *self, edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
    edict_t *dest;
    int         i;

    if ( !other->client )
        return;
    dest = SVG_Entities_Find( NULL, FOFS_GENTITY( targetname ), (const char *)self->targetNames.target );
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
    self->owner->s.event = EV_PLAYER_TELEPORT;
    other->s.event = EV_PLAYER_TELEPORT;

    // set angles
    for ( i = 0; i < 3; i++ ) {
        other->client->ps.pmove.delta_angles[ i ] = /*ANGLE2SHORT*/QM_AngleMod( dest->s.angles[ i ] - other->client->resp.cmd_angles[ i ] );
    }

    VectorClear( other->s.angles );
    VectorClear( other->client->ps.viewangles );
    VectorClear( other->client->viewMove.viewAngles );
    QM_AngleVectors( other->client->viewMove.viewAngles, &other->client->viewMove.viewForward, nullptr, nullptr );

    // kill anything at the destination
    SVG_Util_KillBox( other, !!other->client );

    gi.linkentity( other );
}

/*QUAKED misc_teleporter (1 0 0) (-32 -32 -24) (32 32 -16)
Stepping onto this disc will teleport players to the targeted misc_teleporter_dest object.
*/
/**
*   @brief
**/
void SP_misc_teleporter( edict_t *ent ) {
    edict_t *trig;

    if ( !ent->targetNames.target ) {
        gi.dprintf( "teleporter without a target.\n" );
        SVG_FreeEdict( ent );
        return;
    }

    gi.setmodel( ent, "models/objects/dmspot/tris.md2" );
    ent->s.skinnum = 1;
    ent->s.effects = EF_TELEPORTER;
    ent->s.renderfx = RF_NOSHADOW;
    //ent->s.sound = gi.soundindex("world/amb10.wav");
    ent->solid = SOLID_BOUNDS_BOX;

    VectorSet( ent->mins, -32, -32, -24 );
    VectorSet( ent->maxs, 32, 32, -16 );
    gi.linkentity( ent );

    trig = SVG_AllocateEdict();
    trig->touch = teleporter_touch;
    trig->solid = SOLID_TRIGGER;
    trig->s.entityType = ET_TELEPORT_TRIGGER;
    trig->targetNames.target = ent->targetNames.target;
    trig->owner = ent;
    VectorCopy( ent->s.origin, trig->s.origin );
    VectorSet( trig->mins, -8, -8, 8 );
    VectorSet( trig->maxs, 8, 8, 24 );
    gi.linkentity( trig );

}