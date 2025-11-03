/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_object'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_object.h"

#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_means_of_death.h"


/*QUAKED func_object (0 .5 .8) ? TRIGGER_SPAWN ANIMATED ANIMATED_FAST
This is solid bmodel that will fall if it's support it removed.
*/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_func_object_t, onTouch )( svg_func_object_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    // only squash thing we fall on top of
    if ( !plane ) {
        return;
    }
    if ( plane->normal[ 2 ] <= 1.0f ) {
        return;
    }
    if ( other && other->takedamage == DAMAGE_NO ) {
        return;
    }
    SVG_DamageEntity( other, self, self, vec3_origin, self->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_object_t, onThink_ReleaseObject )( svg_func_object_t *self ) -> void {
	// Make visible and solid.
    self->solid = SOLID_BSP;
    // Send to clients.
    self->svFlags &= ~SVF_NOCLIENT;
    // Set movetype to toss.
    self->movetype = MOVETYPE_TOSS;
    // Proper onTouch.
    self->SetTouchCallback( &svg_func_object_t::onTouch );
    // Setup clipMask.
    self->clipMask = CM_CONTENTMASK_MONSTERSOLID;
    // Link it.
    gi.linkentity( self );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_object_t, onUse )( svg_func_object_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    // Don't allow monsters to use this.
    if ( self->spawnflags & SPAWNFLAG_NOMONSTER ) {
		if ( other && other->svFlags & SVF_MONSTER ) {
			return;
		}
    }
    
    // Reset use callback.
    self->SetUseCallback( nullptr );
    // Perform a killbox.
    SVG_Util_KillBox( self, false, MEANS_OF_DEATH_CRUSHED );
	// Set release the object by setting movetype to toss.
    self->onThink_ReleaseObject( self );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_func_object_t, onPostSpawn )( svg_func_object_t *self ) -> void {
    if ( !( self->spawnflags & SPAWNFLAG_RELEASE_AFTER_TRIGGER_USE ) ) {
        self->SetThinkCallback( &svg_func_object_t::onThink_ReleaseObject );
        self->nextthink = level.time + 40_hz;
    }
    // Set model.
    gi.setmodel( self, self->model );
    self->svFlags &= SVF_HULL;
    // Expand the bounds a bit.
    self->mins[ 0 ] += 1;
    self->mins[ 1 ] += 1;
    self->mins[ 2 ] += 1;
    self->maxs[ 0 ] -= 1;
    self->maxs[ 1 ] -= 1;
    self->maxs[ 2 ] -= 1;
    // Link it.
    gi.linkentity( self );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_object_t, onSpawn )( svg_func_object_t *self ) -> void {
    // Set default dmg if unset.
    if ( !self->dmg ) {
        self->dmg = 100;
    }

	// Hide the object till triggered.
    if ( self->spawnflags & SPAWNFLAG_HIDDEN_UNTIL_TRIGGER_USE ) {
        // Set invisible and not solid.
        self->solid = SOLID_NOT;
        self->svFlags |= SVF_NOCLIENT;
    } else {
        // Set solid.
        self->solid = SOLID_BSP;
        // Set clipMask.
        self->clipMask = CM_CONTENTMASK_MONSTERSOLID;
    }

    // Move Pusher.
    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;

	// Setup to drop after being trigger used.
    self->SetUseCallback( &svg_func_object_t::onUse );
    self->SetPostSpawnCallback( &svg_func_object_t::onPostSpawn );


    if ( self->spawnflags & SPAWNFLAG_ANIMATED ) {
        self->s.entityFlags |= EF_ANIM_ALL;
    }
    if ( self->spawnflags & SPAWNFLAG_ANIMATED_FAST ) {
        self->s.entityFlags |= EF_ANIM_ALLFAST;
    }
}