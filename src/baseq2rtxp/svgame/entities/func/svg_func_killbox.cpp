/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_killbox'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_killbox.h"




/*QUAKED func_killbox (1 0 0) ?
Kills everything inside when fired, irrespective of protection.
*/
static constexpr int32_t SPAWNFLAG_KILLBOX_TRIGGER_BRUSH_CLIP = 32;
void use_killbox( svg_entity_t *self, svg_entity_t *other, svg_entity_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->solid = SOLID_TRIGGER;
    gi.linkentity( self );

    SVG_Util_KillBox( self, self->spawnflags & SPAWNFLAG_KILLBOX_TRIGGER_BRUSH_CLIP );

    self->solid = SOLID_NOT;
    gi.linkentity( self );
}

void SP_func_killbox( svg_entity_t *ent ) {
    gi.setmodel( ent, ent->model );
    ent->use = use_killbox;
    ent->svflags = SVF_NOCLIENT;
}

