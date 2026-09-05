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
#include "svgame/entities/light/svg_light_brush.h"


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
void svg_light_brush_t::SwitchLight( const bool lightOn ) {
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
void svg_light_brush_t::Toggle() {
    if ( SVG_HasSpawnFlags( this, svg_light_light_t::SPAWNFLAG_START_OFF ) ) {
        SwitchLight( true );
    } else {
        SwitchLight( false );
    }
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_light_brush_t, onUse )( svg_light_brush_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    
	// Call upon the super class its OnUse() function to handle default light onUse behavior.
	Super::onUse( self, other, activator, useType, useValue );

	// Fire set target. Already done in svg_light_light_t::onUse() via Super::onUse() call above.
    //SVG_UseTargets( self, activator );
}
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_light_brush_t, onSpawn )( svg_light_brush_t *self ) -> void {
    // Always spawn Super class.
    Super::onSpawn( self );

	self->SetUseCallback( &svg_light_brush_t::onUse );
	if ( SVG_HasSpawnFlags( self, svg_light_light_t::SPAWNFLAG_START_OFF ) ) {
		self->SwitchLight( false );
	} else {
		self->SwitchLight( true );
	}
}