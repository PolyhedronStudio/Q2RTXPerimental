/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_gravity'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_trigger.h"

#include "svgame/entities/trigger/svg_trigger_multiple.h"
#include "svgame/entities/trigger/svg_trigger_gravity.h"


/***
*
*
*	trigger_gravity
*
*
***/
/**
*	@brief	Touch callback in order to change the gravity of 'other', the touching entity.
**/
/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_trigger_gravity_t, onTouch )( svg_trigger_gravity_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	if ( self->spawnflags & svg_trigger_gravity_t::SPAWNFLAG_BRUSH_CLIP ) {
		svg_trace_t clip = SVG_Clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f )
			return;
	}

	other->gravity = self->gravity;
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_trigger_gravity_t, onUse )( svg_trigger_gravity_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	if ( self->solid == SOLID_NOT ) {
		self->solid = SOLID_TRIGGER;
	} else {
		self->solid = SOLID_NOT;
	}
	gi.linkentity( self );
}

/*QUAKED trigger_gravity (.5 .5 .5) ?
Changes the touching entites gravity to
the value of "gravity".  1.0 is standard
gravity for the level.
*/
/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_trigger_gravity_t, onSpawn )( svg_trigger_gravity_t *self ) -> void {
	// Always spawn Super class.
	Super::onSpawn( self );

	// Check for gravity value.
	if ( !self->gravity ) {
		gi.dprintf( "trigger_gravity without gravity set at %s\n", vtos( self->s.origin ) );
		SVG_FreeEdict( self );
		return;
	}
	// Init trigger.
	SVG_Util_InitTrigger( self );
	// Set callbacks.
	if ( SVG_HasSpawnFlags( self, svg_trigger_gravity_t::SPAWNFLAG_TOGGLE ) ) {
		self->SetUseCallback( &svg_trigger_gravity_t::onUse );
	}
	if ( SVG_HasSpawnFlags( self, svg_trigger_gravity_t::SPAWNFLAG_START_OFF ) ) {
		self->SetUseCallback( &svg_trigger_gravity_t::onUse );
		self->solid = SOLID_NOT;
	}
	self->SetTouchCallback( &svg_trigger_gravity_t::onTouch );
	gi.linkentity( self );

	if ( self->spawnflags & svg_trigger_gravity_t::SPAWNFLAG_BRUSH_CLIP ) {
		self->svFlags |= SVF_HULL;
	}
}