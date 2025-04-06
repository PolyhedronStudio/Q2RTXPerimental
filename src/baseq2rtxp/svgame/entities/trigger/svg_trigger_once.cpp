/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_once'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/entities/trigger/svg_trigger_once.h"


void SP_trigger_multiple( edict_t *ent );

/***
*
*
*	trigger_once
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_ONCE_TRIGGERED = 4;

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
void SP_trigger_once( edict_t *ent ) {
	// make old maps work because I messed up on flag assignments here
	// triggered was on bit 1 when it should have been on bit 4
	if ( ent->spawnflags & 1 ) {
		vec3_t  v;

		VectorMA( ent->mins, 0.5f, ent->size, v );
		ent->spawnflags &= ~1;
		ent->spawnflags |= SPAWNFLAG_TRIGGER_ONCE_TRIGGERED;
		gi.dprintf( "fixed TRIGGERED flag on %s at %s\n", (const char *)ent->classname, vtos( v ) );
	}

	ent->wait = -1;
	SP_trigger_multiple( ent );
}