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
//! Reference to the wind sound index.
qhandle_t svg_trigger_push_t::windSound = 0;

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_trigger_push_t, onTouch ) ( svg_trigger_push_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	if ( self->spawnflags & svg_trigger_push_t::SPAWNFLAG_BRUSH_CLIP ) {
		svg_trace_t clip = SVG_Clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {//!= 1.0f || clip.allsolid == true || clip.startsolid == true ) {
			return;
		}
	}

	// Special check for grenades. They dun have health per se.
	// TODO: Just give em health? lol.
	#if 0
	if ( strcmp( (const char *)other->classname, "grenade" ) == 0 ) {
		VectorScale( self->movedir, self->speed * 40, other->velocity );
	} else if ( other->health > 0 ) {
	#else
	if ( other->health > 0 ) {
		VectorScale( self->movedir, self->speed * 40, other->velocity );

		if ( other->client ) {
			// don't take falling damage immediately from this.
			VectorCopy( other->velocity, other->client->oldvelocity );
			other->client->oldgroundentity = other->groundInfo.entity;

			// Play wind sound.
			if ( other->fly_sound_debounce_time < level.time ) {
				other->fly_sound_debounce_time = level.time + 1.5_sec;
				gi.sound( other, CHAN_AUTO, svg_trigger_push_t::windSound, 1, ATTN_NORM, 0 );
			}
		}
	}
	#endif // #if 0

	// Immediately remove itself after pushing the toucher.
	if ( self->spawnflags & svg_trigger_push_t::SPAWNFLAG_PUSH_ONCE ) {
		SVG_FreeEdict( self );
	}
}


/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE
Pushes the player
"speed"     defaults to 1000
*/
/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_trigger_push_t, onSpawn ) ( svg_trigger_push_t *self ) -> void {
	// Always spawn Super class.
	Super::onSpawn( self );

	// WID: Initialize triggers properly.
	SVG_Util_InitTrigger( self );

	if ( !svg_trigger_push_t::windSound ) {
		svg_trigger_push_t::windSound = gi.soundindex( "misc/trigger_push_wind.wav" );
	}
	self->SetTouchCallback( &svg_trigger_push_t::onTouch );
	if ( !self->speed ) {
		self->speed = 1000;
	}
	gi.linkentity( self );

	if ( self->spawnflags & svg_trigger_push_t::SPAWNFLAG_BRUSH_CLIP ) {
		self->svflags |= SVF_HULL;
	}
}