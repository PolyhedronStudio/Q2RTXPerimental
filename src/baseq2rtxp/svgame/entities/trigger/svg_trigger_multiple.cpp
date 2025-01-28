/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_multiple'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/trigger/svg_trigger_multiple.h"


/***
*
*
*	trigger_multiple
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_MONSTER = 1;
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_NOT_PLAYER = 2;
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_TRIGGERED = 4;
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_BRUSH_CLIP = 32;
/**
*	@brief	The wait time has passed, so set back up for another activation
**/
void multi_wait( edict_t *ent ) {
	ent->nextthink = 0_ms;
}


/**
*	@brief	The trigger was just activated
*			ent->activator should be set to the activator so it can be held through a delay
*			so wait for the delay time before firing
**/
void multi_trigger( edict_t *ent ) {
	if ( ent->nextthink )
		return;     // already been triggered

	SVG_UseTargets( ent, ent->activator );

	if ( ent->wait > 0 ) {
		ent->think = multi_wait;
		ent->nextthink = level.time + sg_time_t::from_sec( ent->wait );
	} else {
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = NULL;
		ent->nextthink = level.time + 10_hz;
		ent->think = SVG_FreeEdict;
	}
}

/**
*	@brief
**/
void Use_Multi( edict_t *ent, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	ent->activator = activator;
	multi_trigger( ent );
}

/**
*	@brief
**/
void Touch_Multi( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
	if ( other->client ) {
		if ( self->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_NOT_PLAYER ) {
			return;
		}
	} else if ( other->svflags & SVF_MONSTER ) {
		if ( !( self->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_MONSTER ) ) {
			return;
		}
	} else {
		return;
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_BRUSH_CLIP ) {
		trace_t clip = gi.clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( !VectorEmpty( self->movedir ) ) {
		vec3_t  forward;

		AngleVectors( other->s.angles, forward, NULL, NULL );
		if ( DotProduct( forward, self->movedir ) < 0 ) {
			return;
		}
	}

	self->activator = other;
	multi_trigger( self );
}

/**
*	@brief
**/
void trigger_enable( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	self->solid = SOLID_TRIGGER;
	self->use = Use_Multi;
	gi.linkentity( self );
}

/*QUAKED trigger_multiple (.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
sounds
1)  secret
2)  beep beep
3)  large switch
4)
set "message" to text string
*/
void SP_trigger_multiple( edict_t *ent ) {
	if ( ent->sounds )
		ent->noise_index = gi.soundindex( "hud/chat01.wav" );
	//if ( ent->sounds == 1 )
	//	ent->noise_index = gi.soundindex( "misc/secret.wav" );
	//else if ( ent->sounds == 2 )
	//	ent->noise_index = gi.soundindex( "misc/talk.wav" );
	//else if ( ent->sounds == 3 )
	//	ent->noise_index = gi.soundindex( "misc/trigger1.wav" );

	if ( !ent->wait )
		ent->wait = 0.2f;

	// WID: Initialize triggers properly.
	SVG_Util_InitTrigger( ent );

	ent->touch = Touch_Multi;
	ent->movetype = MOVETYPE_NONE;
	ent->svflags |= SVF_NOCLIENT;

	if ( ent->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_TRIGGERED ) {
		ent->solid = SOLID_NOT;
		ent->use = trigger_enable;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->use = Use_Multi;
	}

	if ( !VectorEmpty( ent->s.angles ) )
		SVG_Util_SetMoveDir( ent->s.angles, ent->movedir );

	gi.linkentity( ent );

	if ( ent->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_BRUSH_CLIP ) {
		ent->svflags |= SVF_HULL;
	}
}