/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_multiple'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/trigger/svg_trigger_multiple.h"


/***
*
*
*	trigger_multiple
*
*
***/
/**
*	@brief	The wait time has passed, so set back up for another activation
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_trigger_multiple_t, onThink_Wait )( svg_trigger_multiple_t *self ) -> void {
	self->nextthink = 0_ms;
}


/**
*	@brief	The trigger was just activated
*			self->activator should be set to the activator so it can be held through a delay
*			so wait for the delay time before firing
**/
void svg_trigger_multiple_t::ProcessTriggerLogic( /*svg_trigger_multiple_t *self*/ ) {
	// Already been triggered.
	if ( nextthink ) {
		return;
	}

	SVG_UseTargets( this, activator );

	if ( wait > 0 ) {
		SetThinkCallback( &svg_trigger_multiple_t::onThink_Wait );
		nextthink = level.time + QMTime::FromSeconds( wait );
	} else {
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		SetTouchCallback( nullptr );
		nextthink = level.time + 10_hz;
		SetThinkCallback( SVG_FreeEdict );
	}
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_trigger_multiple_t, onUse )( svg_trigger_multiple_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	self->activator = activator;
	self->other = other;
	self->ProcessTriggerLogic( /*self*/ );
}
/**
*   @brief  For enabling it when used.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_trigger_multiple_t, onUse_Enable )( svg_trigger_multiple_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	self->solid = SOLID_TRIGGER;
	self->SetUseCallback( &svg_trigger_multiple_t::onUse );
	gi.linkentity( self );
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_trigger_multiple_t, onTouch )( svg_trigger_multiple_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	if ( other->client ) {
		if ( self->spawnflags & svg_trigger_multiple_t::SPAWNFLAG_NOT_PLAYER ) {
			return;
		}
	} else if ( other->svflags & SVF_MONSTER ) {
		if ( !( self->spawnflags & svg_trigger_multiple_t::SPAWNFLAG_MONSTER ) ) {
			return;
		}
	} else {
		return;
	}

	if ( self->spawnflags & svg_trigger_multiple_t::SPAWNFLAG_BRUSH_CLIP ) {
		svg_trace_t clip = SVG_Clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( !VectorEmpty( self->movedir ) ) {
		Vector3  forward;

		QM_AngleVectors( other->s.angles, &forward, NULL, NULL );
		if ( DotProduct( forward, self->movedir ) < 0 ) {
			return;
		}
	}

	self->activator = other;
	self->ProcessTriggerLogic( );
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
DEFINE_MEMBER_CALLBACK_SPAWN( svg_trigger_multiple_t, onSpawn ) ( svg_trigger_multiple_t *self ) -> void {
	// Base spawn.
	Super::onSpawn( self );

	if ( self->sounds ) {
		self->noise_index = gi.soundindex( "hud/chat01.wav" );
	}
	//if ( ent->sounds == 1 )
	//	ent->noise_index = gi.soundindex( "misc/secret.wav" );
	//else if ( ent->sounds == 2 )
	//	ent->noise_index = gi.soundindex( "misc/talk.wav" );
	//else if ( ent->sounds == 3 )
	//	ent->noise_index = gi.soundindex( "misc/trigger1.wav" );

	if ( !self->wait ) {
		self->wait = 0.2f;
	}

	// WID: Initialize triggers properly.
	SVG_Util_InitTrigger( self );

	self->SetTouchCallback( &svg_trigger_multiple_t::onTouch );

	if ( self->spawnflags & svg_trigger_multiple_t::SPAWNFLAG_TRIGGERED ) {
		self->solid = SOLID_NOT;
		self->SetUseCallback( &svg_trigger_multiple_t::onUse_Enable );
	} else {
		self->solid = SOLID_TRIGGER;
		self->SetUseCallback( &svg_trigger_multiple_t::onUse );
	}

	if ( self->spawnflags & svg_trigger_multiple_t::SPAWNFLAG_BRUSH_CLIP ) {
		self->svflags |= SVF_HULL;
	}
}