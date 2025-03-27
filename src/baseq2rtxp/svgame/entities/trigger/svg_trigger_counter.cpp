/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_counter'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/trigger/svg_trigger_multiple.h"
#include "svgame/entities/trigger/svg_trigger_counter.h"


/***
*
*
*	trigger_counter
*
*
***/
static constexpr int32_t SPAWNPFLAG_TRIGGER_COUNTER_NO_MESSAGE = 1;

/**
*	@brief
**/
void trigger_counter_use( svg_entity_t *self, svg_entity_t *other, svg_entity_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	if ( self->count == 0 ) {
		return;
	}

	self->count--;

	if ( self->count ) {
		if ( !( self->spawnflags & SPAWNPFLAG_TRIGGER_COUNTER_NO_MESSAGE ) ) {
			gi.centerprintf( activator, "%i more to go...", self->count );
			gi.sound( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
		}
		return;
	}

	if ( !( self->spawnflags & SPAWNPFLAG_TRIGGER_COUNTER_NO_MESSAGE ) ) {
		gi.centerprintf( activator, "Sequence completed!" );
		gi.sound( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
	}
	self->activator = activator;
	multi_trigger( self );
}

/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.

If nomessage is not set, t will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.
*/
void SP_trigger_counter( svg_entity_t *self ) {
	self->wait = -1;
	if ( !self->count ) {
		self->count = 2;
	}

	self->use = trigger_counter_use;
}