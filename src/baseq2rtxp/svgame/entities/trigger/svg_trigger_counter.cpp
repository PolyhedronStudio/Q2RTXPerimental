/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_counter'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_usetargets.h"
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
/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_trigger_counter_t, onUse )( svg_trigger_counter_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	if ( self->count == 0 ) {
		return;
	}

	self->count--;

	if ( self->count ) {
		if ( !( self->spawnflags & svg_trigger_counter_t::SPAWNFLAG_NO_MESSAGE ) ) {
			gi.centerprintf( activator, "%i more to go...", self->count );
			//gi.sound( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
			SVG_EntityEvent_GeneralSoundEx( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), ATTN_NORM );
		}
		return;
	}

	if ( !( self->spawnflags & svg_trigger_counter_t::SPAWNFLAG_NO_MESSAGE ) ) {
		gi.centerprintf( activator, "Sequence completed!" );
		//gi.sound( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
		SVG_EntityEvent_GeneralSoundEx( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), ATTN_NORM );
		//SVG_EntityEvent_GeneralSoundEx( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, ATTN_STATIC );
	}
	self->activator = activator;
	self->ProcessTriggerLogic();
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_trigger_counter_t, onTouch )( svg_trigger_counter_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	if ( other->client ) {
		if ( self->spawnflags & svg_trigger_multiple_t::SPAWNFLAG_NOT_PLAYER ) {
			return;
		}
	} else if ( other->svFlags & SVF_MONSTER ) {
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

	if ( level.time < self->touch_debounce_time ) {
		return;
	}

	self->touch_debounce_time = level.time + QMTime::FromSeconds( self->wait );

	if ( !VectorEmpty( self->movedir ) ) {
		Vector3  forward;

		QM_AngleVectors( other->s.angles, &forward, NULL, NULL );
		if ( DotProduct( forward, self->movedir ) < 0 ) {
			return;
		}
	}
	self->activator = other;
	self->DispatchUseCallback( other, self->activator, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, other->useTarget.state ^ ENTITY_USETARGET_STATE_TOGGLED );
}

/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.

If nomessage is not set, t will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.
*/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_trigger_counter_t, onSpawn )( svg_trigger_counter_t *self ) -> void {
	// Base spawn.
	Super::onSpawn( self );

	if ( !self->count ) {
		self->count = 2;
	}

	self->SetUseCallback( &svg_trigger_counter_t::onUse );

	if ( SVG_HasSpawnFlags( self, svg_trigger_counter_t::SPAWNFLAG_TOUCHABLE ) != 0 ) {
		self->SetTouchCallback( &svg_trigger_counter_t::onTouch );
	} else {
		self->SetTouchCallback( nullptr );
	}
}