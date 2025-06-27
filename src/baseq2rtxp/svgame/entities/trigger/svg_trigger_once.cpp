/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_once'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/entities/trigger/svg_trigger_multiple.h"
#include "svgame/entities/trigger/svg_trigger_once.h"



/***
*
*
*	trigger_once
*
*
***/
/*QUAKED trigger_once (.5 .5 .5) ? x x TRIGGERED
Triggers once, then removes itself.
You must set the key "target" to the name of another object in the level that has a matching "targetname".

If TRIGGERED, this trigger must be triggered before it is live.

sounds
 1) secret
 2) beep beep
 3) large switch
 4)

"message"   string to be displayed when triggered
*/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_trigger_once_t, onSpawn) ( svg_trigger_once_t *self ) -> void {
	// Set wait to - 1 to prevent triggering.
	self->wait = -1;
	// Continue to spawn like trigger_multiple.
	Super::onSpawn( self );
}