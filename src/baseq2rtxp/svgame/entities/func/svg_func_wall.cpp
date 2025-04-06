/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_wall'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_wall.h"




//! Spawn the wall and trigger at spawn.
static constexpr int32_t FUNC_WALL_TRIGGER_SPAWN= BIT( 0 );
//! Only valid for TRIGGER_SPAWN walls. This allows the wall to be turned on and off.
static constexpr int32_t FUNC_WALL_TOGGLE       = BIT( 1 );
//! Only valid for TRIGGER_SPAWN walls. The wall will initially be present.
static constexpr int32_t FUNC_WALL_START_ON     = BIT( 2 );
//! The wall will animate.
static constexpr int32_t FUNC_WALL_ANIMATE      = BIT( 3 );
//! The wall will animate quickly.
static constexpr int32_t FUNC_WALL_ANIMATE_FAST = BIT( 4 );



/**
*
*
*
*   Generic Func_Wall Stuff:
*
*
*
**/
/**
*   @brief  'Turns on' the wall.
**/
static void func_wall_turn_on( edict_t *self ) {
	// Set the wall to be solid.
	self->solid = SOLID_BSP;
	// Enable entity to be send to clients.
	self->svflags &= ~SVF_NOCLIENT;
    // Unlink the entity.
    gi.unlinkentity( self );
    // Perform a KillBox clipped to BSP for the entity.
	SVG_Util_KillBox( self, true );
    // Relink the entity.
    gi.linkentity( self );
}

/**
*   @brief  'Turns off' the wall.
**/
static void func_wall_turn_off( edict_t *self ) {
	// Set the wall to be non-solid.
	self->solid = SOLID_NOT;
	// Disable entity from being send to clients.
	self->svflags |= SVF_NOCLIENT;
	// Relink the entity.
	gi.linkentity( self );
}
/**
*   @brief  Will toggle the wall's state between on and off.
**/
static void func_wall_toggle( edict_t *self ) {
	// If the wall is currently solid, turn it off.
	if ( self->solid == SOLID_NOT ) {
		// Turn on the wall.
		func_wall_turn_on( self );
    // Otherwise, turn it off.
	} else {
		// Turn off the wall.
		func_wall_turn_off( self );
	}
}

/**
*   @brief
**/
void func_wall_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    //
    // usetype: TOGGLE
    //
    if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_ON ) {
        func_wall_turn_on( self );
    } else if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_OFF ) {
        func_wall_turn_off( self );
    // Otherwise, regular toggle behavior.
    } else {
        func_wall_toggle( self );
    }

	// Unset the use function if the wall is not supposed to be toggled.
    if ( !( self->spawnflags & FUNC_WALL_TOGGLE ) ) {
        self->use = NULL;
    }
}



/**
*
*
*
*   SignalIn:
*
*
*
**/
/**
*   @brief  Signal Receiving:
**/
void func_wall_onsignalin( edict_t *self, edict_t *other, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    /**
    *   Hide/Show:
    **/
    // Show:
    if ( Q_strcasecmp( signalName, "Show" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Turn it on, showing itself.
		func_wall_turn_on( self );
    }
    // Hide:
    if ( Q_strcasecmp( signalName, "Hide" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Turn it off, hiding itself.
        func_wall_turn_off( self );
    }
    /**
	*   Toggle:
    **/
    if ( Q_strcasecmp( signalName, "Toggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Turn it off, hiding itself.
        func_wall_toggle( self );
    }

    // WID: Useful for debugging.
    #if 0
    const int32_t otherNumber = ( other ? other->s.number : -1 );
    const int32_t activatorNumber = ( activator ? activator->s.number : -1 );
    gi.dprintf( "door_onsignalin[ self(#%d), \"%s\", other(#%d), activator(%d) ]\n", self->s.number, signalName, otherNumber, activatorNumber );
    #endif
}



/**
*
*
*
*   func_wall Spawn:
*
*
*
**/
/**
*   @brief
**/
void SP_func_wall( edict_t *self ) {
	// Set the movetype.
    self->movetype = MOVETYPE_PUSH;
	// Set the entity type.
    self->s.entityType = ET_PUSHER;
	// Set the model.
    gi.setmodel( self, self->model );

    // Animate all frames of the wall brush.
    if ( self->spawnflags & FUNC_WALL_ANIMATE ) {
        self->s.effects |= EF_ANIM_ALL;
    }
    // Aniamte them quickly.
    if ( self->spawnflags & FUNC_WALL_ANIMATE_FAST ) {
        self->s.effects |= EF_ANIM_ALLFAST;
    }

    // just a wall
    if ( ( self->spawnflags & ( FUNC_WALL_START_ON | FUNC_WALL_TOGGLE | FUNC_WALL_TRIGGER_SPAWN ) ) == 0 ) {
        self->solid = SOLID_BSP;
        gi.linkentity( self );
        return;
    }

    // It must be TRIGGER_SPAWN.
    if ( !( self->spawnflags & FUNC_WALL_TRIGGER_SPAWN ) ) {
        //      gi.dprintf("func_wall missing TRIGGER_SPAWN\n");
        self->spawnflags |= FUNC_WALL_TRIGGER_SPAWN;
    }

    // Yell if the spawnflags are odd.
    if ( self->spawnflags & FUNC_WALL_START_ON ) {
        if ( !( self->spawnflags & FUNC_WALL_TOGGLE ) ) {
            gi.dprintf( "func_wall START_ON without TOGGLE\n" );
            self->spawnflags |= FUNC_WALL_TOGGLE;
        }
    }

	// Set the use function.
    self->use = func_wall_use;
    // Set signalin callback.
    self->onsignalin = func_wall_onsignalin;
    // Start on, thus solid.
    if ( self->spawnflags & FUNC_WALL_START_ON ) {
        self->solid = SOLID_BSP;
    // Start off, non solid, non send to clients.
    } else {
        self->solid = SOLID_NOT;
        self->svflags |= SVF_NOCLIENT;
    }
    // Link it in.
    gi.linkentity( self );
}