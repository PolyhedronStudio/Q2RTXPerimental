/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_object'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_object.h"



/*QUAKED func_object (0 .5 .8) ? TRIGGER_SPAWN ANIMATED ANIMATED_FAST
This is solid bmodel that will fall if it's support it removed.
*/
void func_object_touch( svg_entity_t *self, svg_entity_t *other, cm_plane_t *plane, cm_surface_t *surf ) {
    // only squash thing we fall on top of
    if ( !plane ) {
        return;
    }
    if ( plane->normal[ 2 ] < 1.0f ) {
        return;
    }
    if ( other && other->takedamage == DAMAGE_NO ) {
        return;
    }
    SVG_TriggerDamage( other, self, self, vec3_origin, self->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}

void func_object_release( svg_entity_t *self ) {
    self->movetype = MOVETYPE_TOSS;
    self->touch = func_object_touch;
}

void func_object_use( svg_entity_t *self, svg_entity_t *other, svg_entity_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->solid = SOLID_BSP;
    self->svflags &= ~SVF_NOCLIENT;
    self->use = NULL;
    SVG_Util_KillBox( self, false );
    func_object_release( self );
}

void SP_func_object( svg_entity_t *self ) {
    gi.setmodel( self, self->model );

    self->mins[ 0 ] += 1;
    self->mins[ 1 ] += 1;
    self->mins[ 2 ] += 1;
    self->maxs[ 0 ] -= 1;
    self->maxs[ 1 ] -= 1;
    self->maxs[ 2 ] -= 1;

    if ( !self->dmg )
        self->dmg = 100;

    if ( self->spawnflags == 0 ) {
        self->solid = SOLID_BSP;
        self->movetype = MOVETYPE_PUSH;
        self->s.entityType = ET_PUSHER;
        self->think = func_object_release;
        self->nextthink = level.time + 20_hz;
    } else {
        self->solid = SOLID_NOT;
        self->movetype = MOVETYPE_PUSH;
        self->s.entityType = ET_PUSHER;
        self->use = func_object_use;
        self->svflags |= SVF_NOCLIENT;
    }

    if ( self->spawnflags & 2 )
        self->s.effects |= EF_ANIM_ALL;
    if ( self->spawnflags & 4 )
        self->s.effects |= EF_ANIM_ALLFAST;

    self->clipmask = MASK_MONSTERSOLID;

    gi.linkentity( self );
}