/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_conveyor'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_conveyor.h"



/*QUAKED func_conveyor (0 .5 .8) ? START_ON TOGGLE
Conveyors are stationary brushes that move what's on them.
The brush should be have a surface with at least one current content enabled.
speed   default 100
*/

void func_conveyor_use( edict_t *self, edict_t *other, edict_t *activator ) {
    if ( self->spawnflags & 1 ) {
        self->speed = 0;
        self->spawnflags &= ~1;
    } else {
        self->speed = self->count;
        self->spawnflags |= 1;
    }

    if ( !( self->spawnflags & 2 ) )
        self->count = 0;
}

void SP_func_conveyor( edict_t *self ) {
    if ( !self->speed )
        self->speed = 100;

    if ( !( self->spawnflags & 1 ) ) {
        self->count = self->speed;
        self->speed = 0;
    }

    self->use = func_conveyor_use;

    gi.setmodel( self, self->model );
    self->solid = SOLID_BSP;
    gi.linkentity( self );
}

