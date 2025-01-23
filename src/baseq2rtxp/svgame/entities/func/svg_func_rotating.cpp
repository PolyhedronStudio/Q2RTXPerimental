/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_rotating'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_rotating.h"



/*QUAKED func_rotating (0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP ANIMATED ANIMATED_FAST

You need to have an origin brush as part of this entity. The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default. You can
check either the X_AXIS or Y_AXIS box to change that.

"speed" determines how fast it moves; default value is 100.
"dmg"   damage to inflict when blocked (2 default)

REVERSE will cause the it to rotate in the opposite direction.
STOP mean it will stop moving instead of pushing entities
*/


/**
*
*
* 
*   Accelerated Velocity Handling:
*
*
* 
**/
/**
*   @brief
**/
void rotating_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
void rotating_accelerate( edict_t *self ) {
    #if 1
    // Get angular velocity speed.
    const float current_speed = QM_Vector3Length( self->avelocity );
    // It has finished Accelerating.
    if ( current_speed >= ( self->speed - self->accel ) )
    {
        self->avelocity = self->movedir * self->speed;
        // On:
        SVG_UseTargets( self, self, ENTITY_USETARGET_TYPE_ON, 1 );
    // 'Starts/Still is' Accelerating:
    } else {
        const float new_speed = current_speed + self->accel;
        self->avelocity = self->movedir * new_speed;
        self->think = rotating_accelerate;
        self->nextthink = level.time + FRAME_TIME_S;
    }
    #else
    // Set sound.
    self->s.sound = self->pushMoveInfo.sounds.middle;
    // Scale speed into movedir velocity.
    VectorScale( self->movedir, self->speed, self->avelocity );
    // Reapply touch for blocking.
    if ( self->spawnflags & FUNC_ROTATING_SPAWNFLAG_PAIN_ON_TOUCH ) {
        self->touch = rotating_touch;
    }
    #endif
}
/**
*   @brief
**/
void rotating_decelerate( edict_t *self ) {
    #if 1
    // Get angular velocity speed.
    const float current_speed = QM_Vector3Length( self->avelocity );
    // It has finished Decelerating.
    if ( current_speed <= self->decel ) {
        self->avelocity = {};
        // Off:
        SVG_UseTargets( self, self, ENTITY_USETARGET_TYPE_OFF, 0 );
        self->touch = nullptr;
    // 'Starts/Still is' Decelerating:
    } else {
        const float new_speed = current_speed - self->decel;
        self->avelocity = self->movedir * new_speed;
        self->think = rotating_decelerate;
        self->nextthink = level.time + FRAME_TIME_S;
    }
    #else
    if ( !VectorEmpty( self->avelocity ) ) {
        self->s.sound = 0;
        VectorClear( self->avelocity );
        self->touch = nullptr;
    }
    #endif
}
void rotating_toggle( edict_t *self ) {
    if ( VectorEmpty( self->avelocity ) ) {
        rotating_accelerate( self );
    } else {
        rotating_decelerate( self );
    }
}



/**
*
*
* 
*   Blocked/Touch Callbacks:
*
* 
*
**/
/**
*   @brief
**/
void rotating_blocked( edict_t *self, edict_t *other ) {
    SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}
/**
*   @brief
**/
void rotating_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !( self->spawnflags & FUNC_ROTATING_SPAWNFLAG_PAIN_ON_TOUCH ) ) {
        return;
    }

    if ( !VectorEmpty( self->avelocity ) )
        SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}



/**
* 
*
*
*   Use Callbacks:
*
* 
*
**/
/**
*   @brief
**/
void rotating_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Continuous useage support:
    if ( useType == ENTITY_USETARGET_TYPE_SET ) {
        if ( useValue != 0 ) {
            rotating_accelerate( self );
        } else {
            rotating_decelerate( self );
        }
        // Return.
        return;
    // On/Off and Toggle usage support:
    } else {
        if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
            rotating_toggle( self );
        } else {
            if ( useType == ENTITY_USETARGET_TYPE_ON ) {
                rotating_accelerate( self );
            } else if ( useType == ENTITY_USETARGET_TYPE_OFF ) {
                rotating_decelerate( self );
            }
        }
    }
}



/**
*
*
*   func_rotating:
*
*
**/
/**
*   @brief
**/
void SP_func_rotating( edict_t *ent ) {
    // Solid.
    ent->solid = SOLID_BSP;
    // Blocking STOPS the rotation:
    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_BLOCK_STOPS ) {
        ent->movetype = MOVETYPE_STOP;
    // Rotation PUSHES the entities if blocking the mover.
    } else {
        ent->movetype = MOVETYPE_PUSH;
    }
    // Pusher Type.
    ent->s.entityType = ET_PUSHER;

    // Set the axis of rotation.
    VectorClear( ent->movedir );
    // X Axis:
    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_ROTATE_X ) {
        ent->movedir[ 2 ] = 1.0f;
    // Y Axis:
    } else if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_ROTATE_Y ) {
        ent->movedir[ 0 ] = 1.0f;
    // Z_AXIS:
    } else { 
        ent->movedir[ 1 ] = 1.0f;
    }

    // Check for reverse rotation.
    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_START_REVERSE ) {
        VectorNegate( ent->movedir, ent->movedir );
    }

    // Apply default speed.
    if ( !ent->speed ) {
        ent->speed = 100;
    }
    // Apply default damage for blocking.
    if ( !ent->dmg ) {
        ent->dmg = 2;
    }

    //  ent->pushMoveInfo.sounds.middle = "doors/hydro1.wav";
    ent->touch = rotating_touch;
    ent->use = rotating_use;

    if ( ent->dmg ) {
        ent->blocked = rotating_blocked;
    }

    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_START_ON ) {
        ent->use( ent, NULL, NULL, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_SET, 1 );
    }

    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_ANIMATE_ALL ) {
        ent->s.effects |= EF_ANIM_ALL;
    }
    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_ANIMATE_ALL_FAST ) {
        ent->s.effects |= EF_ANIM_ALLFAST;
    }

    // PGM
    if ( SVG_HasSpawnFlags( ent, FUNC_ROTATING_SPAWNFLAG_ACCELERATED ) ) {
        // Acceleration:
        if ( !ent->accel ) {
            ent->accel = 1;
        } else if ( ent->accel > ent->speed ) {
            ent->accel = ent->speed;
        }
        // Deceleration:
        if ( !ent->decel ) {
            ent->decel = 1;
        } else if ( ent->decel > ent->speed ) {
            ent->decel = ent->speed;
        }
    }
    // PGM

    gi.setmodel( ent, ent->model );
    gi.linkentity( ent );
}