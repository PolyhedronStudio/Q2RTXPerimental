/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_wall'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_wall.h"



//! Has the train moving instantly after spawning.
static constexpr int32_t FUNC_WALL_TRIGGER_SPAWN = BIT( 0 );
//! Only valid for TRIGGER_SPAWN walls. This allows the wall to be turned on and off.
static constexpr int32_t FUNC_WALL_TOGGLE = BIT( 1 );
//! Only valid for TRIGGER_SPAWN walls. The wall will initially be present.
static constexpr int32_t FUNC_WALL_START_ON = BIT( 2 );


/**
*   @brief
**/
void func_wall_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    //
    // usetype: TOGGLE
    //
    if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_ON ) {
        self->solid = SOLID_BSP;
        self->svflags &= ~SVF_NOCLIENT;
        KillBox( self, false );
    } else if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_OFF ) {
        self->solid = SOLID_NOT;
        self->svflags |= SVF_NOCLIENT;
        // Otherwise, regular toggle behavior.
    } else {
        if ( self->solid == SOLID_NOT ) {
            self->solid = SOLID_BSP;
            self->svflags &= ~SVF_NOCLIENT;
            KillBox( self, false );
        } else {
            self->solid = SOLID_NOT;
            self->svflags |= SVF_NOCLIENT;
        }

        gi.linkentity( self );
    }

    if ( !( self->spawnflags & 2 ) )
        self->use = NULL;
}

/**
*   @brief
**/
void SP_func_wall( edict_t *self ) {
    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;
    gi.setmodel( self, self->model );

    if ( self->spawnflags & 8 )
        self->s.effects |= EF_ANIM_ALL;
    if ( self->spawnflags & 16 )
        self->s.effects |= EF_ANIM_ALLFAST;

    // just a wall
    if ( ( self->spawnflags & 7 ) == 0 ) {
        self->solid = SOLID_BSP;
        gi.linkentity( self );
        return;
    }

    // it must be TRIGGER_SPAWN
    if ( !( self->spawnflags & 1 ) ) {
        //      gi.dprintf("func_wall missing TRIGGER_SPAWN\n");
        self->spawnflags |= 1;
    }

    // yell if the spawnflags are odd
    if ( self->spawnflags & 4 ) {
        if ( !( self->spawnflags & 2 ) ) {
            gi.dprintf( "func_wall START_ON without TOGGLE\n" );
            self->spawnflags |= 2;
        }
    }

    self->use = func_wall_use;
    if ( self->spawnflags & 4 ) {
        self->solid = SOLID_BSP;
    } else {
        self->solid = SOLID_NOT;
        self->svflags |= SVF_NOCLIENT;
    }
    gi.linkentity( self );
}