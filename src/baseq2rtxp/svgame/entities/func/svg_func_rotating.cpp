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



/**
*   For readability's sake:
**/
static constexpr svg_pushmove_state_t ROTATING_STATE_DECELERATE_END     = PUSHMOVE_STATE_TOP;
static constexpr svg_pushmove_state_t ROTATING_STATE_ACCELERATE_END     = PUSHMOVE_STATE_BOTTOM;
static constexpr svg_pushmove_state_t ROTATING_STATE_MOVE_DECELERATING  = PUSHMOVE_STATE_MOVING_UP;
static constexpr svg_pushmove_state_t ROTATING_STATE_MOVE_ACCELERATING  = PUSHMOVE_STATE_MOVING_DOWN;


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
*   Sound Handling:
*
*
*
**/
/**
*   @brief
**/
void rotating_sound_play_start( svg_entity_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.start ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
    }
}
/**
*   @brief  
**/
void rotating_sound_play_end( svg_entity_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.end ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        }
    }
}



/**
*
*
*
*   Lock/UnLock:
*
*
*
**/
/**
*   @brief
**/
void rotating_lock( svg_entity_t *self ) {
    // Of course it has to be locked if we want to play a sound.
    if ( !self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.lockingSound ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.lockingSound, 1, ATTN_STATIC, 0 );
    }
    // Last but not least, unlock its state.
    self->pushMoveInfo.lockState.isLocked = true;
}
/**
*   @brief
**/
void rotating_unlock( svg_entity_t *self ) {
    // Of course it has to be locked if we want to play a sound.
    if ( self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.unlockingSound ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.unlockingSound, 1, ATTN_STATIC, 0 );
    }
    // Last but not least, unlock its state.
    self->pushMoveInfo.lockState.isLocked = false;
}



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
void rotating_halt( svg_entity_t *self ) {

}
/**
*   @brief
**/
void rotating_full_speeded( svg_entity_t *self ) {

}
/**
*   @brief
**/
void rotating_touch( svg_entity_t *self, svg_entity_t *other, cm_plane_t *plane, cm_surface_t *surf );
void rotating_accelerate( svg_entity_t *self ) {
    // Get angular velocity speed.
    const float current_speed = QM_Vector3Length( self->avelocity );

    // If locked, maintain current speed.
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // Calculate new speed based movedir velocity.
        const float new_speed = current_speed;
        self->avelocity = self->movedir * new_speed;
        // Setup nextthink.
        self->think = rotating_accelerate;
        self->nextthink = level.time + FRAME_TIME_S;
        return;
    }

    // It has finished Accelerating.
    if ( current_speed >= ( self->speed - self->accel ) ) {
        // Decelerated state.
        self->pushMoveInfo.state = ROTATING_STATE_ACCELERATE_END;
        // 'Moving' sound.
        self->s.sound = self->pushMoveInfo.sounds.middle;
        // Set velocity.
        self->avelocity = self->movedir * self->speed;
        // On:
        //SVG_UseTargets( self, self, useType, useValue );
        SVG_UseTargets( self, self, ENTITY_USETARGET_TYPE_ON, 1 );
    // 'Starts/Still is' Accelerating:
    } else {
        // Moving sound.
        self->s.sound = self->pushMoveInfo.sounds.middle;
        // Calculate new speed based movedir velocity.
        const float new_speed = current_speed + self->accel;
        self->avelocity = self->movedir * new_speed;
        // Apply nextthink.
        self->think = rotating_accelerate;
        self->nextthink = level.time + FRAME_TIME_S;
    }
    // Reapply touch for blocking.
    if ( self->spawnflags & FUNC_ROTATING_SPAWNFLAG_PAIN_ON_TOUCH ) {
        self->touch = rotating_touch;
    }
}
/**
*   @brief
**/
void rotating_accelerate_start( svg_entity_t *self ) {
    // Already Accelerating:
    if ( self->pushMoveInfo.state == ROTATING_STATE_MOVE_ACCELERATING ) {// || self->pushMoveInfo.state == ROTATING_STATE_MOVE_ACCELERATING ) {
        return;
    }

    // Locked.
    if ( self->pushMoveInfo.lockState.isLocked ) {
        return;
    }

    // We are now accelerating.
    self->pushMoveInfo.state = ROTATING_STATE_MOVE_ACCELERATING;

    // Play start sound.
    rotating_sound_play_start( self );

    // Start accelerating/immediate start.
    rotating_accelerate( self );
}
/**
*   @brief
**/
void rotating_decelerate( svg_entity_t *self ) {
    // Get angular velocity speed.
    const float current_speed = QM_Vector3Length( self->avelocity );

    // If locked, maintain current speed.
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // Calculate new speed based movedir velocity.
        const float new_speed = current_speed;
        self->avelocity = self->movedir * new_speed;
        // Setup nextthink.
        self->think = rotating_decelerate;
        self->nextthink = level.time + FRAME_TIME_S;
        return;
    }

    // It has finished Decelerating.
    if ( current_speed <= self->decel ) {
        // Decelerated state.
        self->pushMoveInfo.state = ROTATING_STATE_DECELERATE_END;
        // End sound.
        rotating_sound_play_end( self );
        // No sound.
        self->s.sound = 0;
        // Set velocity.
        self->avelocity = {};
        // Off:
        //SVG_UseTargets( self, self, useType, useValue );
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_OFF, 0 );
        self->touch = nullptr;
        // 'Starts/Still is' Decelerating:
    } else {
        // Set sound.
        self->s.sound = self->pushMoveInfo.sounds.middle;
        // Calculate new speed based movedir velocity.
        const float new_speed = current_speed - self->decel;
        self->avelocity = self->movedir * new_speed;
        // Setup nextthink.
        self->think = rotating_decelerate;
        self->nextthink = level.time + FRAME_TIME_S;
    }
}
/**
*   @brief
**/
void rotating_decelerate_start( svg_entity_t *self ) {
    // Already decelerating.
    if ( self->pushMoveInfo.state == ROTATING_STATE_MOVE_DECELERATING ) { // } || self->pushMoveInfo.state == ROTATING_STATE_MOVE_DECELERATING ) {
        return;
    }

    // Locked.
    if ( self->pushMoveInfo.lockState.isLocked ) {
        return;
    }
    
    // We are now accelerating.
    self->pushMoveInfo.state = ROTATING_STATE_MOVE_DECELERATING;

    // End decelerating/immediate halt.
    rotating_decelerate( self );
}

/**
*   @brief  
**/
void rotating_toggle( svg_entity_t *self, const int32_t useValue ) {
    if ( useValue != 0 ) {
        // Accelerate.
        rotating_accelerate_start( self );
    } else {
        // Decelerate.
        rotating_decelerate_start( self );
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
void rotating_blocked( svg_entity_t *self, svg_entity_t *other ) {
    // Only do damage if we had any set.
    if ( !self->dmg ) {
        return;
    }
    // Debounce time to prevent trigger damaging the entity too rapidly each frame.
    if ( level.time < self->touch_debounce_time ) {
        return;
    }
    // Take 100ms before going at it again.
    self->touch_debounce_time = level.time + 10_hz;
    // Perform damaging.
    SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}

/**
*   @brief
**/
void rotating_touch( svg_entity_t *self, svg_entity_t *other, cm_plane_t *plane, cm_surface_t *surf ) {
    // Exit if touching isn't hurting.
    if ( !( self->spawnflags & FUNC_ROTATING_SPAWNFLAG_PAIN_ON_TOUCH ) ) {
        return;
    }

    // Perform damage if we got angular velocity.
    if ( !VectorEmpty( self->avelocity ) ) {
        SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
    }
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
void rotating_use( svg_entity_t *self, svg_entity_t *other, svg_entity_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Default to 'self' being its activator if this was called with none.
    self->activator = ( activator != nullptr ? activator : self );
    // Store other.
    self->other = other;

    // Continuous useage support:
    if ( useType == ENTITY_USETARGET_TYPE_SET ) {
        // On:
        if ( useValue != 0 ) {
            // Accelerate.
            rotating_accelerate_start( self );
        // Off:
        } else {
            // Decelerate.
            rotating_decelerate_start( self );
        }
        // Return.
        return;
    // On/Off and Toggle usage support:
    } else {
        // Toggle:
        if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
            rotating_toggle( self, useValue );
        // On/Off:
        } else {
            // On:
            if ( useType == ENTITY_USETARGET_TYPE_ON ) {
                // Accelerate.
                rotating_accelerate_start( self );
            // Off:
            } else if ( useType == ENTITY_USETARGET_TYPE_OFF ) {
                // Decelerate.
                rotating_decelerate_start( self );
            }
        }
    }
}



/**
* 
* 
* 
*   SignalIn:
* 
* 
* 
**/
void rotating_onsignalin( svg_entity_t *self, svg_entity_t *other, svg_entity_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    /**
    *   Accelerate/Decelerate:
    **/
    // RotatingAccelerate:
    if ( Q_strcasecmp( signalName, "Accelerate" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Starts accelerating if it is not doing so yet.
        rotating_accelerate_start( self );
    }
    // RotatingDecelerate:
    if ( Q_strcasecmp( signalName, "Decelerate" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Starts decelerating if it is not doing so yet.
        rotating_decelerate_start( self );
    }

    /**
    *   Pause/Continue:
    **/
    // WID: TODO: Mayhaps? 
    //// RotatingPause:
    //if ( Q_strcasecmp( signalName, "RotatingPause" ) == 0 ) {
    //    self->activator = activator;
    //    self->other = other;
    //}
    //// RotatingContinue:
    //if ( Q_strcasecmp( signalName, "RotatingContinue" ) == 0 ) {
    //    self->activator = activator;
    //    self->other = other;
    //}
    //// RotatingHalt:
    //if ( Q_strcasecmp( signalName, "RotatingHalt" ) == 0 ) {
    //    self->activator = activator;
    //    self->other = other;
    //}

    /**
    *   Lock/UnLock:
    **/
    // RotatingLock:
    if ( Q_strcasecmp( signalName, "Lock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        rotating_lock( self );
    }
    // RotatingUnlock:
    if ( Q_strcasecmp( signalName, "Unlock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        rotating_unlock( self );
    }
    // RotatingLockToggle:
    if ( Q_strcasecmp( signalName, "LockToggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Lock if unlocked:
        if ( !self->pushMoveInfo.lockState.isLocked ) {
            rotating_lock( self );
            // Unlock if locked:
        } else {
            rotating_unlock( self );
        }
    }

    // WID: Useful for debugging.
    #if 0
    const int32_t otherNumber = ( other ? other->s.number : -1 );
    const int32_t activatorNumber = ( activator ? activator->s.number : -1 );
    gi.dprintf( "rotating_onsignalin[ self(#%d), \"%s\", other(#%d), activator(%d) ]\n", self->s.number, signalName, otherNumber, activatorNumber );
    #endif
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
void SP_func_rotating( svg_entity_t *ent ) {
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
    // Default state.
    ent->pushMoveInfo.state = ROTATING_STATE_DECELERATE_END;

    // Set the axis of rotation.
    ent->movedir = {};
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
        // Negate the move direction.
        ent->movedir = QM_Vector3Negate( ent->movedir );
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
    ent->onsignalin = rotating_onsignalin;

    // Blockading entities get damaged if dmg is set.
    if ( ent->dmg ) {
        ent->blocked = rotating_blocked;
    }

    // Start already rotating.
    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_START_ON ) {
        ent->use( ent, NULL, NULL, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_SET, 1 );
    }

    // Animation.
    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_ANIMATE_ALL ) {
        ent->s.effects |= EF_ANIM_ALL;
    }
    if ( ent->spawnflags & FUNC_ROTATING_SPAWNFLAG_ANIMATE_ALL_FAST ) {
        ent->s.effects |= EF_ANIM_ALLFAST;
    }

    // PGM
    // Has acceleration and deceleration support.
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

    // <Q2RTXP>: WID: TODO: Get unique audio for func_rotating.
    if ( ent->sounds ) {
        ent->pushMoveInfo.sounds.start = gi.soundindex( "pushers/rotating_start_01.wav" );
        ent->pushMoveInfo.sounds.middle = gi.soundindex( "pushers/rotating_mid_01.wav" );
        ent->pushMoveInfo.sounds.end = gi.soundindex( "pushers/rotating_end_01.wav" );
    } else {
        ent->pushMoveInfo.sounds = {};
    }

    // Set (Assumed)brush model and thus also its bounds.
    gi.setmodel( ent, ent->model );
    // Link it in.
    gi.linkentity( ent );
}