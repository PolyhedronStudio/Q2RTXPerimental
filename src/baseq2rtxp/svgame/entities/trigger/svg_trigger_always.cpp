/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_counter'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"

#include "svgame/entities/trigger/svg_trigger_counter.h"


/***
*
*
*	trigger_always
*
*
***/
/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always( svg_base_edict_t *ent ) {
	// we must have some delay to make sure our use targets are present
	if ( !ent->delay ) {
		ent->delay = 1.f;
	}
	SVG_UseTargets( ent, ent );
}