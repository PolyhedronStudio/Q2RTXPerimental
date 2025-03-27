/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_relay'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/entities/trigger/svg_trigger_relay.h"


/***
*
*
*	trigger_relay
*
*
***/
/**
*	@brief
**/
void trigger_relay_use( svg_entity_t *self, svg_entity_t *other, svg_entity_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	SVG_UseTargets( self, activator, useType, useValue );
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.
*/
void SP_trigger_relay( svg_entity_t *self ) {
	self->use = trigger_relay_use;
}