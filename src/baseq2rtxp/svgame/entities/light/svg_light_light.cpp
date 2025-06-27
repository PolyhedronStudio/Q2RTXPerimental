/*******************************************************************
*
*
*	ServerGame: SpotLight
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"
#include "svgame/entities/light/svg_light_light.h"


/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light.
Default light value is 300.
Default style is 0.
If targeted, will toggle between on and off. Firing other set target also.
Default _cone value is 10 (used to set size of light for spotlights)
*/




/**
*   @brief  Switches light on/off depending on lightOn value.
**/
void svg_light_light_t::SwitchLight( const bool lightOn ) {
    if ( lightOn ) {
        if ( customLightStyle ) {
            gi.configstring( CS_LIGHTS + style, customLightStyle );
        } else {
            gi.configstring( CS_LIGHTS + style, "m" );
        }
        spawnflags &= ~svg_light_light_t::SPAWNFLAG_START_OFF;
    } else {
        gi.configstring( CS_LIGHTS + style, "a" );
        spawnflags |= svg_light_light_t::SPAWNFLAG_START_OFF;
    }
}
/**
*   @brief  Will toggle between light on/off states.
**/
void svg_light_light_t::Toggle() {
    if ( SVG_HasSpawnFlags( this, svg_light_light_t::SPAWNFLAG_START_OFF ) ) {
        SwitchLight( true );
    } else {
        SwitchLight( false );
    }
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_light_light_t, onUse )( svg_light_light_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    //if ( self->spawnflags & START_OFF ) {
    //    if ( self->customLightStyle ) {
    //        gi.configstring( CS_LIGHTS + self->style, self->customLightStyle );
    //    } else {
    //        gi.configstring( CS_LIGHTS + self->style, "m" );
    //    }
    //    self->spawnflags &= ~START_OFF;
    //} else {
    //    gi.configstring( CS_LIGHTS + self->style, "a" );
    //    self->spawnflags |= START_OFF;
    //}


    // Apply activator.
    self->activator = activator;
    self->other = other;

    //gi.dprintf( "%s: useType=(%d) useValue=(%d)\n", __func__, useType, useValue );
    if ( useType == ENTITY_USETARGET_TYPE_SET ) {
        if ( useValue != 0 ) {
            // If it is on let it be.
            self->SwitchLight( true );
        } else {
            // If it is on let it be.
            self->SwitchLight( false );
        }
    } else {
        if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
            self->Toggle( );
        } else {
            if ( useType == ENTITY_USETARGET_TYPE_OFF ) {
                self->SwitchLight( false );
            } else if ( useType == ENTITY_USETARGET_TYPE_ON ) {
                self->SwitchLight( true );
            }
        }
    }

    // Fire set target.
    SVG_UseTargets( self, activator );
}
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_light_light_t, onSpawn )( svg_light_light_t *self ) -> void {
    #if 0
    // no targeted lights in deathmatch, because they cause global messages
    if ( !self->targetname || deathmatch->value ) {
        SVG_FreeEdict( self );
        return;
    }
    #endif

    //if ( self->style >= 32 ) {
    //self->SetUseCallback( &svg_light_light_t::onUse );
    //}
    self->SetUseCallback( &svg_light_light_t::onUse );
    if ( SVG_HasSpawnFlags( self, svg_light_light_t::SPAWNFLAG_START_OFF ) ) {
        self->SwitchLight( false );
    } else {
        self->SwitchLight( true );
    }
    // Set on or off depending on spawnflags.
    //if ( SVG_HasSpawnFlags( self, START_OFF ) ) {
    //    light_off( self );
    //} else {
    //    light_on( self );
    //}
    //}
}