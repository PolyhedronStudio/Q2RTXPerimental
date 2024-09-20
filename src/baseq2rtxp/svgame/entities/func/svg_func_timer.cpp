/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_timer'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_timer.h"



/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
"wait"          base time between triggering all targets, default is 1
"random"        wait variance, default is 0

so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"delay"         delay before first firing when turned on, default is 0

"pausetime"     additional delay used only the very first time
                and only if spawned with START_ON

These can used but not touched.
*/
void func_timer_think( edict_t *self ) {
    SVG_UseTargets( self, self->activator );
    self->nextthink = level.time + sg_time_t::from_sec( self->wait + crandom() * self->random );
}

void func_timer_use( edict_t *self, edict_t *other, edict_t *activator ) {
    self->activator = activator;

    // if on, turn it off
    if ( self->nextthink ) {
        self->nextthink = 0_ms;
        return;
    }

    // turn it on
    if ( self->delay )
        self->nextthink = level.time + sg_time_t::from_sec( self->delay );
    else
        func_timer_think( self );
}

void SP_func_timer( edict_t *self ) {
    if ( !self->wait )
        self->wait = 1.0f;

    self->use = func_timer_use;
    self->think = func_timer_think;

    if ( self->random >= self->wait ) {
        self->random = self->wait - gi.frame_time_s;
        gi.dprintf( "func_timer at %s has random >= wait\n", vtos( self->s.origin ) );
    }

    if ( self->spawnflags & 1 ) {
        self->nextthink = level.time + 1_sec + sg_time_t::from_sec( st.pausetime + self->delay + self->wait + crandom() * self->random );
        self->activator = self;
    }

    self->svflags = SVF_NOCLIENT;
}