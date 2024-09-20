/********************************************************************
*
*
*	ServerGame: Trigger Entity 'trigger_elevator'.
*
*   TODO: Do we really still need this? It is func_train specific also.
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_train.h"
#include "svgame/entities/trigger/svg_trigger_elevator.h"



/*QUAKED trigger_elevator (0.3 0.1 0.6) (-8 -8 -8) (8 8 8)
*/
void trigger_elevator_use( edict_t *self, edict_t *other, edict_t *activator ) {
    edict_t *target;

    if ( self->movetarget->nextthink ) {
        //      gi.dprintf("elevator busy\n");
        return;
    }

    if ( !other->targetNames.path ) {
        gi.dprintf( "elevator used with no targetNames.path\n" );
        return;
    }

    target = SVG_PickTarget( other->targetNames.path );
    if ( !target ) {
        gi.dprintf( "elevator used with bad targetNames.path: %s\n", other->targetNames.path );
        return;
    }

    self->movetarget->targetEntities.target = target;
    train_resume( self->movetarget );
}

void trigger_elevator_init( edict_t *self ) {
    if ( !self->targetNames.target ) {
        gi.dprintf( "trigger_elevator has no target\n" );
        return;
    }
    self->movetarget = SVG_PickTarget( self->targetNames.target );
    if ( !self->movetarget ) {
        gi.dprintf( "trigger_elevator unable to find target %s\n", self->targetNames.target );
        return;
    }
    if ( strcmp( self->movetarget->classname, "func_train" ) != 0 ) {
        gi.dprintf( "trigger_elevator target %s is not a train\n", self->targetNames.target );
        return;
    }

    self->use = trigger_elevator_use;
    self->svflags = SVF_NOCLIENT;
}

void SP_trigger_elevator( edict_t *self ) {
    self->s.entityType = ET_PUSH_TRIGGER;
    self->think = trigger_elevator_init;
    self->nextthink = level.time + FRAME_TIME_S;
}