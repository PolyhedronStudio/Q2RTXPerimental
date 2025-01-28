/********************************************************************
*
*
*	ServerGame: Path Entity 'path_corner'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/entities/path/svg_path_corner.h"


/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8) TELEPORT
Target: next path corner
Pathtarget: gets used when an entity that has
    this path_corner targeted touches it
*/
/**
*   @brief
**/
void path_corner_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    vec3_t      v;
    edict_t *next;

    if ( other->movetarget != self )
        return;

    if ( other->enemy )
        return;

    if ( self->targetNames.path ) {
        char *savetarget;

        savetarget = (char *)self->targetNames.target;
        self->targetNames.target = self->targetNames.path;
        SVG_UseTargets( self, other );
        self->targetNames.target = savetarget;
    }

    if ( self->targetNames.target )
        next = SVG_PickTarget( (const char *)self->targetNames.target );
    else
        next = NULL;

    if ( ( next ) && ( next->spawnflags & 1 ) ) {
        VectorCopy( next->s.origin, v );
        v[ 2 ] += next->mins[ 2 ];
        v[ 2 ] -= other->mins[ 2 ];
        VectorCopy( v, other->s.origin );
        next = SVG_PickTarget( (const char *)next->targetNames.target );
        other->s.event = EV_OTHER_TELEPORT;
    }

    other->goalentity = other->movetarget = next;

    if ( self->wait ) {
        // WID: TODO: Monster Reimplement.
        //other->monsterinfo.pause_time = level.time + sg_time_t::from_sec( self->wait );
        //other->monsterinfo.stand(other);
        return;
    }

    if ( !other->movetarget ) {
        // WID: TODO: Monster Reimplement.
        //other->monsterinfo.pause_time = HOLD_FOREVER;
        //other->monsterinfo.stand(other);
    } else {
        VectorSubtract( other->goalentity->s.origin, other->s.origin, v );
        other->ideal_yaw = QM_Vector3ToYaw( v );
    }
}

/**
*   @brief
**/
void SP_path_corner( edict_t *self ) {
    if ( !self->targetname ) {
        gi.dprintf( "path_corner with no targetname at %s\n", vtos( self->s.origin ) );
        SVG_FreeEdict( self );
        return;
    }

    self->solid = SOLID_TRIGGER;
    self->touch = path_corner_touch;
    VectorSet( self->mins, -8, -8, -8 );
    VectorSet( self->maxs, 8, 8, 8 );
    self->svflags |= SVF_NOCLIENT;
    gi.linkentity( self );
}