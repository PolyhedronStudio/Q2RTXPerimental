/*******************************************************************
*
*
*	ServerGame: SpotLight
*
*
********************************************************************/
#include "svgame/svg_local.h"


/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light.
Default light value is 300.
Default style is 0.
If targeted, will toggle between on and off. Firing other set target also.
Default _cone value is 10 (used to set size of light for spotlights)
*/

static constexpr int32_t START_OFF = 1;

/**
*   @brief
**/
static void light_off( svg_entity_t *self ) {
    //if ( !SVG_HasSpawnFlags( self, START_OFF ) ) {
    gi.configstring( CS_LIGHTS + self->style, "a" );
    self->spawnflags |= START_OFF;
    //}
}
/**
*   @brief
**/
static void light_on( svg_entity_t *self ) {
    //if ( SVG_HasSpawnFlags( self, START_OFF ) ) {
    if ( self->customLightStyle ) {
        gi.configstring( CS_LIGHTS + self->style, self->customLightStyle );
    } else {
        gi.configstring( CS_LIGHTS + self->style, "m" );
    }
    self->spawnflags &= ~START_OFF;
    //}
}
/**
*   @brief
**/
static void light_toggle( svg_entity_t *self ) {
    if ( SVG_HasSpawnFlags( self, START_OFF ) ) {
        light_on( self );
    } else {
        light_off( self );
    }
}
/**
*   @brief
**/
void light_use( svg_entity_t *self, svg_entity_t *other, svg_entity_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
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
            light_on( self );
        } else {
            // If it is on let it be.
            light_off( self );
        }
    } else {
        if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
            light_toggle( self );
        } else {
            if ( useType == ENTITY_USETARGET_TYPE_OFF ) {
                light_off( self );
            } else if ( useType == ENTITY_USETARGET_TYPE_ON ) {
                light_on( self );
            }
        }
    }

    // Fire set target.
    SVG_UseTargets( self, activator );
}
/**
*   @brief
**/
void SP_light( svg_entity_t *self ) {
    #if 0
    // no targeted lights in deathmatch, because they cause global messages
    if ( !self->targetname || deathmatch->value ) {
        SVG_FreeEdict( self );
        return;
    }
    #endif

    //if ( self->style >= 32 ) {
        self->use = light_use;
    // Set on or off depending on spawnflags.
    if ( SVG_HasSpawnFlags( self, START_OFF ) ) {
        light_off( self );
    } else {
        light_on( self );
    }
    //}
}