/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_push'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/trigger/svg_trigger_push.h"


/***
*
*
*	trigger_push
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_PUSH_PUSH_ONCE = 1;
static constexpr int32_t SPAWNFLAG_TRIGGER_PUSH_BRUSH_CLIP = 32;

static int windsound = 0;

/**
*	@brief
**/
void trigger_push_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf ) {

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_PUSH_BRUSH_CLIP ) {
		trace_t clip = gi.clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( strcmp( (const char *)other->classname, "grenade" ) == 0 ) {
		VectorScale( self->movedir, self->speed * 10, other->velocity );
	} else if ( other->health > 0 ) {
		VectorScale( self->movedir, self->speed * 10, other->velocity );

		if ( other->client ) {
			// don't take falling damage immediately from this
			VectorCopy( other->velocity, other->client->oldvelocity );
			other->client->oldgroundentity = other->groundInfo.entity;

			if ( other->fly_sound_debounce_time < level.time ) {
				other->fly_sound_debounce_time = level.time + 1.5_sec;
				gi.sound( other, CHAN_AUTO, windsound, 1, ATTN_NORM, 0 );
			}
		}
	}
	if ( self->spawnflags & SPAWNFLAG_TRIGGER_PUSH_PUSH_ONCE ) {
		SVG_FreeEdict( self );
	}
}


/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE
Pushes the player
"speed"     defaults to 1000
*/
void SP_trigger_push( svg_edict_t *self ) {
	// WID: Initialize triggers properly.
	SVG_Util_InitTrigger( self );

	if ( !windsound ) {
		windsound = gi.soundindex( "misc/windfly.wav" );
	}
	self->touch = trigger_push_touch;
	if ( !self->speed ) {
		self->speed = 1000;
	}
	gi.linkentity( self );

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_PUSH_BRUSH_CLIP ) {
		self->svflags |= SVF_HULL;
	}
}