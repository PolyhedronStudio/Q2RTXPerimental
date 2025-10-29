/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_wall'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_wall.h"

#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_means_of_death.h"



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
*   @brief  'Turns on'/'Shows' the wall.
**/
void svg_func_wall_t::TurnOn() {
	// Set the wall to be solid.
	solid = SOLID_BSP;
	// Enable entity to be send to clients.
	svFlags &= ~SVF_NOCLIENT;
    // Unlink the entity.
    gi.unlinkentity( this );
    // Perform a KillBox clipped to BSP for the entity.
	SVG_Util_KillBox( this, true, sg_means_of_death_t::MEANS_OF_DEATH_TELEFRAGGED );
    // Relink the entity.
    gi.linkentity( this );
}

/**
*   @brief  'Turns off'/'Hides' the wall.
**/
void svg_func_wall_t::TurnOff( ) {
	// Set the wall to be non-solid.
	solid = SOLID_NOT;
	// Disable entity from being send to clients.
	svFlags |= SVF_NOCLIENT;
	// Relink the entity.
	gi.linkentity( this );
}
/**
*   @brief  Will toggle the wall's state between on and off.
**/
void svg_func_wall_t::Toggle( ) {
	// If the wall is currently solid, turn it off.
	if ( solid == SOLID_NOT ) {
		// Turn on the wall.
        TurnOn();
    // Otherwise, turn it off.
	} else {
		// Turn off the wall.
        TurnOff();
	}
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_wall_t, onUse )( svg_func_wall_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    //
    // usetype: TOGGLE
    //
    if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_ON ) {
        self->TurnOn();
    } else if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_OFF ) {
        self->TurnOff();
    // Otherwise, regular toggle behavior.
    } else {
        self->Toggle();
    }

	// Unset the use function if the wall is not supposed to be toggled.
    if ( !( self->spawnflags & svg_func_wall_t::SPAWNFLAG_TOGGLE ) ) {
        self->SetUseCallback( nullptr );
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
DEFINE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_wall_t, onSignalIn )( svg_func_wall_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void {
    /**
    *   Hide/Show:
    **/
    // Show:
    if ( Q_strcasecmp( signalName, "Show" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Turn it on, showing itself.
        self->TurnOn();
    }
    // Hide:
    if ( Q_strcasecmp( signalName, "Hide" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Turn it off, hiding itself.
        self->TurnOff();
    }
    /**
	*   Toggle:
    **/
    if ( Q_strcasecmp( signalName, "Toggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Turn it off, hiding itself.
        self->Toggle();
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
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_wall_t, onSpawn )( svg_func_wall_t *self ) -> void {
	// Set the movetype.
    self->movetype = MOVETYPE_PUSH;
	// Set the entity type.
    self->s.entityType = ET_PUSHER;
	// Set the model.
    gi.setmodel( self, self->model );

    // Animate all frames of the wall brush.
    if ( self->spawnflags & svg_func_wall_t::SPAWNFLAG_ANIMATE ) {
        self->s.effects |= EF_ANIM_ALL;
    }
    // Aniamte them quickly.
    if ( self->spawnflags & svg_func_wall_t::SPAWNFLAG_ANIMATE_FAST ) {
        self->s.effects |= EF_ANIM_ALLFAST;
    }

    // just a wall
    if ( ( self->spawnflags & ( svg_func_wall_t::SPAWNFLAG_START_ON | svg_func_wall_t::SPAWNFLAG_TOGGLE | svg_func_wall_t::SPAWNFLAG_TRIGGER_SPAWN ) ) == 0 ) {
        self->solid = SOLID_BSP;
        gi.linkentity( self );
        return;
    }

    // It must be TRIGGER_SPAWN.
    if ( !( self->spawnflags & svg_func_wall_t::SPAWNFLAG_TRIGGER_SPAWN ) ) {
        //      gi.dprintf("func_wall missing TRIGGER_SPAWN\n");
        self->spawnflags |= svg_func_wall_t::SPAWNFLAG_TRIGGER_SPAWN;
    }

    // Yell if the spawnflags are odd.
    if ( self->spawnflags & svg_func_wall_t::SPAWNFLAG_START_ON ) {
        if ( !( self->spawnflags & svg_func_wall_t::SPAWNFLAG_TOGGLE ) ) {
            gi.dprintf( "func_wall START_ON without TOGGLE\n" );
            self->spawnflags |= svg_func_wall_t::SPAWNFLAG_TOGGLE;
        }
    }

	// Set the use function.
    self->SetUseCallback( &svg_func_wall_t::onUse );
    // Set signalin callback.
    self->SetOnSignalInCallback( &svg_func_wall_t::onSignalIn );
    // Start on, thus solid.
    if ( self->spawnflags & svg_func_wall_t::SPAWNFLAG_START_ON ) {
        self->solid = SOLID_BSP;
    // Start off, non solid, non send to clients.
    } else {
        self->solid = SOLID_NOT;
        self->svFlags |= SVF_NOCLIENT;
    }
    // Link it in.
    gi.linkentity( self );
}