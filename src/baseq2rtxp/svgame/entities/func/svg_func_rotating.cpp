/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_rotating'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_rotating.h"

#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_means_of_death.h"





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
void svg_func_rotating_t::StartSoundPlayback( ) {
    if ( !( flags & FL_TEAMSLAVE ) ) {
        if ( pushMoveInfo.sounds.start ) {
			//SVG_TempEventEntity_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.start, ATTN_STATIC );
            SVG_EntityEvent_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.start, ATTN_STATIC );
        }
    }
}
/**
*   @brief  
**/
void svg_func_rotating_t::EndSoundPlayback( ) {
    if ( !( flags & FL_TEAMSLAVE ) ) {
        if ( pushMoveInfo.sounds.end ) {
            //SVG_TempEventEntity_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.end, ATTN_STATIC );
            SVG_EntityEvent_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.end, ATTN_STATIC );
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
void svg_func_rotating_t::LockState( ) {
    // Of course it has to be locked if we want to play a sound.
    if ( !pushMoveInfo.lockState.isLocked && pushMoveInfo.lockState.lockingSound ) {
        //SVG_TempEventEntity_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.lockState.lockingSound, ATTN_STATIC );
        SVG_EntityEvent_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.lockState.lockingSound, ATTN_STATIC );
    }
    // Last but not least, unlock its state.
    SetLockState( true );
}
/**
*   @brief
**/
void svg_func_rotating_t::UnLockState( ) {
    // Of course it has to be locked if we want to play a sound.
    if ( pushMoveInfo.lockState.isLocked && pushMoveInfo.lockState.unlockingSound ) {
        //SVG_TempEventEntity_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.lockState.unlockingSound, ATTN_STATIC );
        SVG_EntityEvent_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.lockState.unlockingSound, ATTN_STATIC );
    }
    // Last but not least, unlock its state.
    SetLockState( false );
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
*   @brief  Start the acceleration process.
**/
void svg_func_rotating_t::StartAcceleration() {
    // Already Accelerating:
    if ( pushMoveInfo.state == ROTATING_STATE_MOVE_ACCELERATING ) {// || pushMoveInfo.state == ROTATING_STATE_MOVE_ACCELERATING ) {
        return;
    }

    // Locked.
    if ( pushMoveInfo.lockState.isLocked ) {
        return;
    }

    // We are now accelerating.
    pushMoveInfo.state = ROTATING_STATE_MOVE_ACCELERATING;

    // Play start sound.
	StartSoundPlayback();

    // Start accelerating/immediate start.
    onThink_ProcessAcceleration( this );
}
/**
*   @brief  Start moving into the deceleration process.
**/
void svg_func_rotating_t::StartDeceleration() {
    // Already decelerating.
    if ( pushMoveInfo.state == ROTATING_STATE_MOVE_DECELERATING ) { // } || pushMoveInfo.state == ROTATING_STATE_MOVE_DECELERATING ) {
        return;
    }

    // Locked.
    if ( pushMoveInfo.lockState.isLocked ) {
        return;
    }

    // We are now accelerating.
    pushMoveInfo.state = ROTATING_STATE_MOVE_DECELERATING;

    // End decelerating/immediate halt.
    onThink_ProcessDeceleration( this );
}
/**
*   @brief  Process the acceleration.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_rotating_t, onThink_ProcessAcceleration )( svg_func_rotating_t *self ) -> void {
    // Get angular velocity speed.
    const float current_speed = QM_Vector3Length( self->avelocity );

    // If locked, maintain current speed.
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // Calculate new speed based movedir velocity.
        const float new_speed = current_speed;
        self->avelocity = self->movedir * new_speed;
        // Setup nextthink.
        self->SetThinkCallback( &svg_func_rotating_t::onThink_ProcessAcceleration );
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
        self->SetThinkCallback( &svg_func_rotating_t::onThink_ProcessAcceleration );
        self->nextthink = level.time + FRAME_TIME_S;
    }
    // Reapply touch for blocking.
    if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_PAIN_ON_TOUCH ) {
        self->SetTouchCallback( &svg_func_rotating_t::onTouch );
    }
}
/**
*   @brief  Process the deceleration.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_rotating_t, onThink_ProcessDeceleration )( svg_func_rotating_t *self ) -> void {
    // Get angular velocity speed.
    const float current_speed = QM_Vector3Length( self->avelocity );

    // If locked, maintain current speed.
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // Calculate new speed based movedir velocity.
        const float new_speed = current_speed;
        self->avelocity = self->movedir * new_speed;
        // Setup nextthink.
        self->SetThinkCallback( &svg_func_rotating_t::onThink_ProcessDeceleration );
        self->nextthink = level.time + FRAME_TIME_S;
        return;
    }

    // It has finished Decelerating.
    if ( current_speed <= self->decel ) {
        // Decelerated state.
        self->pushMoveInfo.state = svg_func_rotating_t::ROTATING_STATE_DECELERATE_END;
        // End sound.
        self->EndSoundPlayback();
        // No sound.
        self->s.sound = 0;
        // Set velocity.
        self->avelocity = {};
        // Off:
        //SVG_UseTargets( self, self, useType, useValue );
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_OFF, 0 );
        self->SetTouchCallback( nullptr );
        // 'Starts/Still is' Decelerating:
    } else {
        // Set sound.
        self->s.sound = self->pushMoveInfo.sounds.middle;
        // Calculate new speed based movedir velocity.
        const float new_speed = current_speed - self->decel;
        self->avelocity = self->movedir * new_speed;
        // Setup nextthink.
        self->SetThinkCallback( &svg_func_rotating_t::onThink_ProcessDeceleration );
        self->nextthink = level.time + FRAME_TIME_S;
    }
}


/**
*   @brief  Togles moving into the acceleration/deceleration process.
**/
void svg_func_rotating_t::ToggleAcceleration( const int32_t useValue ) {
    if ( useValue != 0 ) {
        // Accelerate.
		StartAcceleration();
    } else {
        // Decelerate.
        StartDeceleration();
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
DEFINE_MEMBER_CALLBACK_BLOCKED( svg_func_rotating_t, onBlocked )( svg_func_rotating_t *self, svg_base_edict_t *other ) -> void {
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
    SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_func_rotating_t, onTouch )( svg_func_rotating_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    // Exit if touching isn't hurting.
    if ( !( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_PAIN_ON_TOUCH ) ) {
        return;
    }

    // Perform damage if we got angular velocity.
    if ( !VectorEmpty( self->avelocity ) ) {
        SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
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
DEFINE_MEMBER_CALLBACK_USE( svg_func_rotating_t, onUse )( svg_func_rotating_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    // Default to 'self' being its activator if this was called with none.
    self->activator = ( activator != nullptr ? activator : self );
    // Store other.
    self->other = other;

    // Continuous useage support:
    if ( useType == ENTITY_USETARGET_TYPE_SET ) {
        // On:
        if ( useValue != 0 ) {
            // Accelerate.
            self->StartAcceleration( );
        // Off:
        } else {
            // Decelerate.
            self->StartDeceleration( );
        }
        // Return.
        return;
    // On/Off and Toggle usage support:
    } else {
        // Toggle:
        if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
            self->ToggleAcceleration( useValue );
        // On/Off:
        } else {
            // On:
            if ( useType == ENTITY_USETARGET_TYPE_ON ) {
                // Accelerate.
                self->StartAcceleration( );
            // Off:
            } else if ( useType == ENTITY_USETARGET_TYPE_OFF ) {
                // Decelerate.
                self->StartDeceleration( );
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
DEFINE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_rotating_t, onSignalIn )( svg_func_rotating_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void {
    /**
    *   Accelerate/Decelerate:
    **/
    // RotatingAccelerate:
    if ( Q_strcasecmp( signalName, "Accelerate" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Starts accelerating if it is not doing so yet.
        self->StartAcceleration( );
    }
    // RotatingDecelerate:
    if ( Q_strcasecmp( signalName, "Decelerate" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Starts decelerating if it is not doing so yet.
        self->StartDeceleration( );
    }

    /**
    *   Pause/Continue:
    **/
    // WID: TODO: Mayhaps? 
    //// RotatingPause:
    //if ( Q_strcasecmp( signalName, "RotatingPause" ) == 0 ) {
    //    activator = activator;
    //    other = other;
    //}
    //// RotatingContinue:
    //if ( Q_strcasecmp( signalName, "RotatingContinue" ) == 0 ) {
    //    activator = activator;
    //    other = other;
    //}
    //// RotatingHalt:
    //if ( Q_strcasecmp( signalName, "RotatingHalt" ) == 0 ) {
    //    activator = activator;
    //    other = other;
    //}

    /**
    *   Lock/UnLock:
    **/
    // RotatingLock:
    if ( Q_strcasecmp( signalName, "Lock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        self->LockState( );
    }
    // RotatingUnlock:
    if ( Q_strcasecmp( signalName, "Unlock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        self->UnLockState( );
    }
    // RotatingLockToggle:
    if ( Q_strcasecmp( signalName, "LockToggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Lock if unlocked:
        if ( !self->pushMoveInfo.lockState.isLocked ) {
            self->LockState( );
            // Unlock if locked:
        } else {
            self->UnLockState( );
        }
    }

    // WID: Useful for debugging.
    #if 0
    const int32_t otherNumber = ( other ? other->s.number : -1 );
    const int32_t activatorNumber = ( activator ? activator->s.number : -1 );
    gi.dprintf( "rotating_onsignalin[ self(#%d), \"%s\", other(#%d), activator(%d) ]\n", s.number, signalName, otherNumber, activatorNumber );
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
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_rotating_t, onSpawn )( svg_func_rotating_t *self ) -> void {
    // Solid.
    self->solid = SOLID_BSP;

    // Blocking STOPS the rotation:
    if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_BLOCK_STOPS ) {
        self->movetype = MOVETYPE_STOP;
    // Rotation PUSHES the selfities if blocking the mover.
    } else {
        self->movetype = MOVETYPE_PUSH;
    }

    // Pusher Type.
    self->s.entityType = ET_PUSHER;
    // Default state.
    self->pushMoveInfo.state = ROTATING_STATE_DECELERATE_END;

    // Set the axis of rotation.
    self->movedir = {};
    // X Axis:
    if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_ROTATE_X ) {
        self->movedir[ 2 ] = 1.0f;
    // Y Axis:
    } else if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_ROTATE_Y ) {
        self->movedir[ 0 ] = 1.0f;
    // Z_AXIS:
    } else { 
        self->movedir[ 1 ] = 1.0f;
    }

    // Check for reverse rotation.
    if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_START_REVERSE ) {
        // Negate the move direction.
        self->movedir = QM_Vector3Negate( self->movedir );
    }

    // Apply default speed.
    if ( !self->speed ) {
        self->speed = 100;
    }
    // Apply default damage for blocking.
    if ( !self->dmg ) {
        self->dmg = 2;
    }

    //  self->pushMoveInfo.sounds.middle = "doors/hydro1.wav";
    self->SetTouchCallback( &svg_func_rotating_t::onTouch );
    self->SetUseCallback( &svg_func_rotating_t::onUse );
    self->SetOnSignalInCallback( &svg_func_rotating_t::onSignalIn );

    // Blockading selfities get damaged if dmg is set.
    if ( self->dmg ) {
        self->SetBlockedCallback( &svg_func_rotating_t::onBlocked );
    }

    // Start already rotating.
    if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_START_ON ) {
        self->DispatchUseCallback( nullptr, nullptr, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_SET, 1 );
    }

    // Animation.
    if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_ANIMATE_ALL ) {
        self->s.effects |= EF_ANIM_ALL;
    }
    if ( self->spawnflags & svg_func_rotating_t::SPAWNFLAG_ANIMATE_ALL_FAST ) {
        self->s.effects |= EF_ANIM_ALLFAST;
    }

    // PGM
    // Has acceleration and deceleration support.
    if ( SVG_HasSpawnFlags( self, svg_func_rotating_t::SPAWNFLAG_ACCELERATED ) ) {
        // Acceleration:
        if ( !self->accel ) {
            self->accel = 1;
        } else if ( self->accel > self->speed ) {
            self->accel = self->speed;
        }
        // Deceleration:
        if ( !self->decel ) {
            self->decel = 1;
        } else if ( self->decel > self->speed ) {
            self->decel = self->speed;
        }
    }
    // PGM

    // <Q2RTXP>: WID: TODO: Get unique audio for func_rotating.
    if ( self->sounds ) {
        self->pushMoveInfo.sounds.start = gi.soundindex( "pushers/rotating_start_01.wav" );
        self->pushMoveInfo.sounds.middle = gi.soundindex( "pushers/rotating_mid_01.wav" );
        self->pushMoveInfo.sounds.end = gi.soundindex( "pushers/rotating_end_01.wav" );
    } else {
        self->pushMoveInfo.sounds = {};
    }

    // Set (Assumed)brush model and thus also its bounds.
    gi.setmodel( self, self->model );
    // Link it in.
    gi.linkentity( self );
}