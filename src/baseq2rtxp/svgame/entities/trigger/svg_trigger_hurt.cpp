/********************************************************************
*
*
*	ServerGame: Trigger Entity 'trigger_hurt'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"



/***
*
*
*	trigger_hurt
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_START_OFF = 1;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_TOGGLE = 2;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_SILENT = 4;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_NO_PROTECTION = 8;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_SLOW_HURT = 16;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_BRUSH_CLIP = 32;

/**
*	@brief
**/
void hurt_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	if ( self->solid == SOLID_NOT ) {
		self->solid = SOLID_TRIGGER;
	} else {
		self->solid = SOLID_NOT;
	}
	gi.linkentity( self );

	if ( !( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_TOGGLE ) ) {
		self->SetUseCallback( nullptr );
	}
}

/**
*	@brief
**/
void hurt_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
	entity_damageflags_t dflags;

	if ( !other->takedamage ) {
		return;
	}

	if ( self->timestamp > level.time ) {
		return;
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_BRUSH_CLIP ) {
		svg_trace_t clip = SVG_Clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_SLOW_HURT ) {
		self->timestamp = level.time + 1_sec;
	} else {
		self->timestamp = level.time + 10_hz;
	}

	if ( !( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_SILENT ) ) {
		if ( self->fly_sound_debounce_time < level.time ) {
			gi.sound( other, CHAN_AUTO, self->noise_index, 1, ATTN_NORM, 0 );
			self->fly_sound_debounce_time = level.time + 1_sec;
		}
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_NO_PROTECTION )
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = DAMAGE_NONE;
	SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, self->dmg, dflags, MEANS_OF_DEATH_TRIGGER_HURT );
}

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT          supresses playing the sound
SLOW            changes the damage rate to once per second
NO_PROTECTION   *nothing* stops the damage

"dmg"           default 5 (whole numbers only)

*/
void SP_trigger_hurt( svg_base_edict_t *self ) {
	// WID: Initialize triggers properly.
	SVG_Util_InitTrigger( self );

	self->noise_index = gi.soundindex( "world/lashit01.wav" );
	self->SetTouchCallback( hurt_touch );

	if ( !self->dmg )
		self->dmg = 5;

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_START_OFF )
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_TOGGLE )
		self->SetUseCallback( hurt_use );

	gi.linkentity( self );

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_BRUSH_CLIP ) {
		self->svflags |= SVF_HULL;
	}
}


