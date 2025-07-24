/********************************************************************
*
*
*	ServerGame: Trigger Entity 'trigger_hurt'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "sharedgame/sg_means_of_death.h"

#include "svgame/entities/trigger/svg_trigger_hurt.h"



/***
*
*
*	trigger_hurt
*
*
***/
/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_trigger_hurt_t, onUse )( svg_trigger_hurt_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	if ( self->solid == SOLID_NOT ) {
		self->solid = SOLID_TRIGGER;
	} else {
		self->solid = SOLID_NOT;
	}
	gi.linkentity( self );

	if ( !( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_TOGGLE ) ) {
		self->SetUseCallback( nullptr );
	}
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_trigger_hurt_t, onTouch )( svg_trigger_hurt_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	entity_damageflags_t dflags;

	if ( !other->takedamage ) {
		return;
	}

	if ( self->timestamp > level.time ) {
		return;
	}

	if ( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_BRUSH_CLIP ) {
		svg_trace_t clip = SVG_Clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_SLOW_HURT ) {
		self->timestamp = level.time + 1_sec;
	} else {
		self->timestamp = level.time + 10_hz;
	}

	if ( !( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_SILENT ) ) {
		if ( self->fly_sound_debounce_time < level.time ) {
			gi.sound( other, CHAN_AUTO, self->noise_index, 1, ATTN_NORM, 0 );
			self->fly_sound_debounce_time = level.time + 1_sec;
		}
	}

	if ( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_NO_PROTECTION ) {
		dflags = DAMAGE_NO_PROTECTION;
	} else {
		dflags = DAMAGE_NONE;
	}

	// Perform the damage.
	SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, self->dmg, dflags, MEANS_OF_DEATH_TRIGGER_HURT );
}

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT          supresses playing the sound
SLOW            changes the damage rate to once per second
NO_PROTECTION   *nothing* stops the damage

"dmg"           default 5 (whole numbers only)

*/
/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_trigger_hurt_t, onSpawn )( svg_trigger_hurt_t *self ) -> void {
	// Always spawn Super class.
	Super::onSpawn( self );

	// WID: Initialize triggers properly.
	SVG_Util_InitTrigger( self );

	self->noise_index = gi.soundindex( "world/lashit01.wav" );
	self->SetTouchCallback( &svg_trigger_hurt_t::onTouch );

	if ( !self->dmg )
		self->dmg = 5;

	if ( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_START_OFF )
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if ( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_TOGGLE )
		self->SetUseCallback( &svg_trigger_hurt_t::onUse );

	gi.linkentity( self );

	if ( self->spawnflags & svg_trigger_hurt_t::SPAWNFLAG_BRUSH_CLIP ) {
		self->svflags |= SVF_HULL;
	}
}


