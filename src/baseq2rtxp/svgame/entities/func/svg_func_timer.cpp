/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_timer'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_timer.h"



/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
"wait"          base time between triggering all targets, default is 1
"random"        wait variance(0 to #max), default is 0

so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"delay"         delay before first firing when turned on, default is 0

"pausetime"     additional delay used only the very first time
                and only if spawned with START_ON

These can used but not touched.
*/
void func_timer_think( svg_base_edict_t *self ) {
    SVG_UseTargets( self, self->activator );
    self->nextthink = level.time + QMTime::FromSeconds( self->wait + crandom() * self->random );
}

void func_timer_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->activator = activator;

    // if on, turn it off
    if ( self->nextthink ) {
        self->nextthink = 0_ms;
        return;
    }

    // turn it on
    if ( self->delay )
        self->nextthink = level.time + QMTime::FromSeconds( self->delay );
    else
        func_timer_think( self );
}

void SP_func_timer( svg_base_edict_t *self ) {
    if ( !self->wait )
        self->wait = 1.0f;

    self->SetUseCallback( func_timer_use );
    self->SetThinkCallback( func_timer_think );

    if ( self->random >= self->wait ) {
        self->random = self->wait - gi.frame_time_s;
        gi.dprintf( "func_timer at %s has random >= wait\n", vtos( self->s.origin ) );
    }

    if ( self->spawnflags & 1 ) {
        self->nextthink = level.time + 1_sec + QMTime::FromMilliseconds( self->pausetime.seconds() + self->delay + self->wait + crandom() * self->random);
        self->activator = self;
    }

    self->svFlags = SVF_NOCLIENT;
}