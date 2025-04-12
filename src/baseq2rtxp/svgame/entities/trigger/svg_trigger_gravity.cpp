/********************************************************************
*
*
*	ServerGame: Path Entity 'trigger_gravity'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/trigger/svg_trigger_gravity.h"


/***
*
*
*	trigger_gravity
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_GRAVITY_BRUSH_CLIP = 32;

/**
*	@brief	Touch callback in order to change the gravity of 'other', the touching entity.
**/
void trigger_gravity_touch( svg_edict_t *self, svg_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
	if ( self->spawnflags & SPAWNFLAG_TRIGGER_GRAVITY_BRUSH_CLIP ) {
		svg_trace_t clip = SVG_Clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, SVG_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	other->gravity = self->gravity;
}

/*QUAKED trigger_gravity (.5 .5 .5) ?
Changes the touching entites gravity to
the value of "gravity".  1.0 is standard
gravity for the level.
*/
void SP_trigger_gravity( svg_edict_t *self ) {
	if ( st.gravity == NULL ) {
		gi.dprintf( "trigger_gravity without gravity set at %s\n", vtos( self->s.origin ) );
		SVG_FreeEdict( self );
		return;
	}

	SVG_Util_InitTrigger( self );

	self->gravity = atof( st.gravity );
	self->touch = trigger_gravity_touch;

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_GRAVITY_BRUSH_CLIP ) {
		self->svflags |= SVF_HULL;
	}
}