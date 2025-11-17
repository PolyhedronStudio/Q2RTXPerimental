/********************************************************************
*
*
*	ServerGame: Path Entity 'path_corner'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/path/svg_path_corner.h"


/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8) TELEPORT
Target: next path corner
Pathtarget: gets used when an entity that has
    this path_corner targeted touches it
*/
/**
*   @brief
**/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_path_corner_t, onTouch )( svg_path_corner_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    vec3_t      v;
    svg_base_edict_t *next;

    if ( other->movetarget != self ) {
        return;
    }

    if ( other->enemy ) {
        return;
    }

    if ( self->targetNames.path ) {
        svg_level_qstring_t savetarget = self->targetNames.target;
        svg_base_edict_t *saveedicttarget = self->targetEntities.target;
        self->targetNames.target = self->targetNames.path;
        self->targetEntities.target = SVG_PickTarget( (const char *)self->targetNames.target );
        SVG_UseTargets( self, other, ENTITY_USETARGET_TYPE_ON, 1 );
        self->targetNames.target = savetarget;
        self->targetEntities.target = saveedicttarget;
    }

    // see m_move; this is just so we don't needlessly check it
    self->flags |= FL_PARTIALGROUND;

    if ( self->targetNames.target ) {
        next = SVG_PickTarget( (const char *)self->targetNames.target );
    } else {
        next = NULL;
    }

    if ( next /*&& next->GetTypeInfo()->IsSubClassType<svg_path_corner_t>()*/ ) {
        if ( ( next->spawnflags & svg_path_corner_t::SPAWNFLAG_TELEPORT_PUSHER ) ) {
            VectorCopy( next->s.origin, v );
            v[ 2 ] += next->mins[ 2 ];
            v[ 2 ] -= other->mins[ 2 ];
            VectorCopy( v, other->s.origin );
            next = SVG_PickTarget( (const char *)next->targetNames.target );
            SVG_Util_AddEvent( other, EV_OTHER_TELEPORT, 0 ); //other->s.event = EV_OTHER_TELEPORT;
        }
    }

    other->goalentity = other->movetarget = next;

    if ( self->wait ) {
        // WID: TODO: Monster Reimplement.
        //other->pausetime = level.time + QMTime::FromSeconds( self->wait );
        //other->monsterinfo.pause_time = level.time + QMTime::FromMilliseconds( self->wait );
        //other->monsterinfo.stand(other);
        return;
    }

    if ( !other->movetarget ) {
        // WID: TODO: Monster Reimplement.
        other->pausetime = HOLD_FOREVER;
        //other->monsterinfo.stand(other);
    } else {
        VectorSubtract( other->goalentity->s.origin, other->s.origin, v );
        other->ideal_yaw = QM_Vector3ToYaw( v );
    }
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_path_corner_t, onSpawn )( svg_path_corner_t *self ) -> void {
    // Always spawn Super class.
    Super::onSpawn( self );

    if ( !self->targetname ) {
        gi.dprintf( "path_corner with no targetname at %s\n", vtos( self->s.origin ) );
        SVG_FreeEdict( self );
        return;
    }

    self->solid = SOLID_TRIGGER;
    self->SetTouchCallback( &svg_path_corner_t::onTouch );
    VectorSet( self->mins, -8, -8, -8 );
    VectorSet( self->maxs, 8, 8, 8 );
    self->svFlags |= SVF_NOCLIENT;
    gi.linkentity( self );
}