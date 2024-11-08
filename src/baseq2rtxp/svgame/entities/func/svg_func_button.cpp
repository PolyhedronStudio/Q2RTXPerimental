/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_button'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_button.h"


// WID: TODO: The audio file we got is ugly so lol...
#define FUNC_BUTTON_ENABLE_END_SOUND 1

/**
*   Button SpawnFlags:
**/
//! Button starts pressed.
static constexpr int32_t BUTTON_SPAWNFLAG_START_PRESSED     = BIT( 0 );
//! Button can't be triggered by monsters.
static constexpr int32_t BUTTON_SPAWNFLAG_NO_MONSTERS       = BIT( 1 );
//! Button fires targets when touched.
static constexpr int32_t BUTTON_SPAWNFLAG_TOUCH_ACTIVATES   = BIT( 2 );
//! Button fires targets when damaged.
static constexpr int32_t BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES  = BIT( 3 );
//! Button is locked from spawn, so it can't be used.
static constexpr int32_t BUTTON_SPAWNFLAG_TOGGLEABLE        = BIT( 4 );
//! Button is locked from spawn, so it can't be used.
static constexpr int32_t BUTTON_SPAWNFLAG_LOCKED            = BIT( 5 );


/**
*   For readability's sake:
**/
static constexpr int32_t BUTTON_STATE_PRESSED                   = PUSHMOVE_STATE_TOP;
static constexpr int32_t BUTTON_STATE_UNPRESSED                 = PUSHMOVE_STATE_BOTTOM;
static constexpr int32_t BUTTON_STATE_MOVING_TO_PRESSED_STATE   = PUSHMOVE_STATE_MOVING_UP;
static constexpr int32_t BUTTON_STATE_MOVING_TO_UNPRESSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;


/**
*   These are the button frames, pressed/unpressed will toggle from pressed_0 to unpressed_0, leaving the 
*   pressed_1 and unpressed_1 as an animation frame in case they are set.
* 
*   The first and second(if animating) frame for PRESSED state.
*   BUTTON_FRAME_PRESSED_0 -> This should be "+0texturename"
*   BUTTON_FRAME_PRESSED_1 -> This should be "+1texturename"
* 
*   The first and second(if animating) frame for UNPRESSED state.
*   BUTTON_FRAME_UNPRESSED_0 -> This should be "+2texturename"
*   BUTTON_FRAME_UNPRESSED_1 -> This should be "+3texturename"
**/
static constexpr int32_t BUTTON_FRAME_PRESSED_0     = 0;
static constexpr int32_t BUTTON_FRAME_PRESSED_1     = 1;
static constexpr int32_t BUTTON_FRAME_UNPRESSED_0   = 2;
static constexpr int32_t BUTTON_FRAME_UNPRESSED_1   = 3;



/*QUAKED func_button (0 .5 .8) ?
When a button is use targetted(+usetarget), it moves some distance in the direction of it's angle, triggers all of it's targets.
There it will wait till the wait time is over, or if toggleable, until it is toggled again.

"angle"     determines the opening direction
"target"    all entities with a matching targetname will be used
"speed"     override the default 40 speed
"wait"      override the default 1 second wait (-1 = never return)
"lip"       override the default 4 pixel lip remaining at end of move
"health"    if set, the button must be killed instead of touched
"moveparent" the parent entity it'll relatively stay offset and move along the origin with.
"sounds"
1) silent
2) steam metal
3) wooden clunk
4) metallic click
5) in-out
*/
void button_press_move_done( edict_t *self );
void button_unpress_move_done( edict_t *self );
void button_think_return( edict_t *self );
void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );



/**
*
*
*
*   PushMove - EndMove CallBacks:
*
*
*
**/
/**
*   @brief  PushMoveInfo callback for when the button returned to its initial 'Unpressed' state.
**/
void button_unpress_move_done( edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE );

    // Continous button?
    const bool isContinuousUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Damage button?
    const bool isDamageButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );
    // Toggle button?
    const bool isToggleButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOGGLEABLE );
    // Start pressed button?
    const bool isStartPressed = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // Engage 'Unpressed' state.
    self->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;

    /**
    *   Adjust animation and/or frame.
    **/
    // We're unpressed, thus use the 'start' frame.
    self->s.frame = ( isStartPressed ? self->pushMoveInfo.endFrame : self->pushMoveInfo.startFrame );
    // Adjust entity animation effects for the client to display.
    //self->s.effects &= ~EF_ANIM23;
    //self->s.effects |= EF_ANIM01;

    /**
    *   Respond to the Pressing of the button.
    **/
    if ( isToggleUseTarget ) {
        #if 0
        // Play sound.
        if ( self->pushMoveInfo.sounds.end && !( self->flags & FL_TEAMSLAVE ) ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        }
        #endif
        // Fire use targets.
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
    }

    // If it is a touch button, reassign the touch callback.
    if ( isTouchButton ) {
        if ( !isToggleButton ) {
            self->touch = nullptr;
        } else {
            self->touch = button_touch;
        }
    }

    // If it is a damage button, reassign the killed callback
    if ( isDamageButton ) {
        //if ( !isToggleButton ) {
        //    self->die = nullptr;
        //} else {
            self->die = button_killed;
        //}
    }

    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnUnPressed" );
}

/**
*   @brief  PushMoveInfo callback for when the button reaches its 'Pressed' state.
**/
void button_press_move_done( edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE );

    // Continous button?
    const bool isContinuousUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Damage button?
    const bool isDamageButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );
    // Toggle button?
    const bool isToggleButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOGGLEABLE );
    // Start pressed button?
    const bool isStartPressed = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_START_PRESSED);
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    /**
    *   Respond to the pressing of the button.
    **/
    // Apply "Pressed" state.
    self->pushMoveInfo.state = BUTTON_STATE_PRESSED;

    /**
    *   Adjust animation and/or frame.
    **/
    // We're pressed, thus use the 'end' frame.
    self->s.frame = ( isStartPressed ? self->pushMoveInfo.startFrame : self->pushMoveInfo.endFrame );

    if ( isTouchButton ) {
        // If it is a touch button, reassign the touch callback.
        if ( !isToggleButton ) {
            self->touch = nullptr;
        }
    }
    
    // If button has to remain pushed, or is a 'Toggleable':
    if ( stayPressed || ( isToggleButton && isToggleUseTarget ) || /*isTouchButton || */( isContinuousState && isContinuousUseTarget ) ) {

    // If a wait time has been set, use it for when to trigger the button's return(unpressing).
    } else {
        // Is press button?
        if ( self->nextthink < level.time ) {
            if ( self->pushMoveInfo.wait >= 0 ) {
                self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
                self->think = button_think_return;
            }
        }
    }

    // Trigger UseTargets
    SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 1 );
    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnPressed" );
}



/**
*
*
*
*   Press/UnPress Initiators:
*
*
*
**/
/**
*   @brief  Engages moving into the pressed state, after which at arrival, it calls upon 'button_wait'.
**/
void button_press_move( edict_t *self ) {
    // Adjust movement state info.
    self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_PRESSED_STATE;

    // Play sound.
    if ( self->pushMoveInfo.sounds.start && !( self->flags & FL_TEAMSLAVE ) ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
    }
    // Calculate and begin moving to the button's 'Pressed' state end origin.
    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.endOrigin, button_press_move_done );

    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnPress" );
    // Also a special one for touch buttons.
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );
    if ( isTouchButton ) {
        SVG_SignalOut( self, self->other, self->activator, "OnTouchPress" );
    }
    // Signal Testing:
    #if 0
    // Signal arguments.
    std::vector<svg_signal_argument_t> signalArguments = {
        {.type = SIGNAL_ARGUMENT_TYPE_STRING,  .key = "stringArg",     .value = {.str = "Hello World!" } },
        {.type = SIGNAL_ARGUMENT_TYPE_INTEGER, .key = "integerArg",    .value = {.integer = 1337 } },
        {.type = SIGNAL_ARGUMENT_TYPE_NUMBER,  .key = "numberArg",     .value = {.number = 13.37f } },
    };
    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnPress", signalArguments );
    #endif
}
/**
*   @brief  Engages moving into the 'Unpressed' state, after which at arrival, it calls upon 'button_unpress_move_done'.
**/
void button_unpress_move( edict_t *self ) {
    // Adjust movement state info.
    self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_UNPRESSED_STATE;
    // Adjust entity animation effects for the client to display, 
    // by determining the new material texture animation.
    //button_determine_animation( self, self->pushMoveInfo.state );
    #ifdef FUNC_BUTTON_ENABLE_END_SOUND
    // Play sound.
    if ( self->pushMoveInfo.sounds.end && !( self->flags & FL_TEAMSLAVE ) ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
    }
    #endif
    // Calculate and begin moving back to the button's 'Unpressed' state start origin.
    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.startOrigin, button_unpress_move_done );
    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnUnPress" );
    // Also a special one for touch buttons.
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );
    if ( isTouchButton ) {
        SVG_SignalOut( self, self->other, self->activator, "OnTouchUnPress" );
    }
}



/**
*
*
*
*   func_button state 'Think' methods:
*
*
*
**/
/**
*   @brief  Think method called to determine when/whether to move back to its unpressed state.
**/
void button_think_return( edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_PRESSED );

    // Continous button?
    const bool isContinuousUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Try again next frame.
    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED && isContinuousUseTarget && isContinuousState ) {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->think = button_think_return;
        // Trigger UseTargets
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_SET, 1 );

        // Dispatch a signal.
        SVG_SignalOut( self, self->other, self->activator, "OnContinuousPress" );

        // Get out.
        return;
    }

    // Move back to the button's 'Unpressed' state.
    if ( !SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
        // Unpress itself.
        button_unpress_move( self );
        // If it is a continuous button...
        if ( isContinuousUseTarget ) {
            // Dispatch a signal.
            //button_lua_signal( self, "OnContinuousUnPress" );
        }

        // For damage based buttons.
        if ( SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES ) && self->max_health ) {
            // Allow it to take damage again.
            self->takedamage = DAMAGE_YES;
            self->health = self->max_health;
            self->die = button_killed;
            // Dispatch a signal.
            SVG_SignalOut( self, self->other, self->activator, "OnRevive" );
        }
    //}
    // It is continuous and still held, so maintain this function as our 'think' callback.
    } else {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->think = button_think_return;
    }
}

/**
*   @brief  For when 'Use' triggered.
**/
void button_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // button_use is never called by usage from monsters or clients, enabling this would get in the way of the remaining entities.
    #if 0
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }
    #endif

    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }

    // Continuous button?
    const bool isContinuousUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continuous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Touch button?
    const bool isStartPressed = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // Start Unpressed Path( Default ):
    if ( isStartPressed ) {
        if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
            // Try again next frame:
            if ( isContinuousUseTarget && isContinuousState ) {
                // Reinitiate this function.
                self->nextthink = level.time + FRAME_TIME_MS;
                self->think = button_think_return;
                // Trigger UseTargets
                SVG_UseTargets( self, self->activator, useType, useValue );
                // Dispatch a signal.
                SVG_SignalOut( self, self->other, self->activator, "OnContinuousPress" );
                return;
            }

            // Unpress if demanded:
            if ( !stayPressed ) {
                // Engage in unpress movement.
                button_unpress_move( self );
            }
        } else {
            // Engage in press movement.
            button_press_move( self );
        }
    // Start pressed Path:
    } else {
        if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
            // Engage in press movement.
            button_press_move( self );
        } else {
            // Unpress if demanded:
            if ( !stayPressed ) {
                // Engage in unpress movement.
                button_unpress_move( self );
            }
        }
    }
}
/**
*   @brief  For when '+usetarget' press triggered.
**/
void button_usetarget_continuous_press( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Only react if it is a client entity which is touch triggering this button.
// If it is a dead client, ignore triggering it.
    if ( !SVG_IsClientEntity( activator, /* healthCheck=*/true ) ) {
        return;
    }

    // Ignore triggers calling into fire when the button is still actively moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        //return;
    }

    // Set activator.
    self->activator = activator;
    // Set other.
    self->other = other;

    // The client let go of the use key.
    if ( useType == ENTITY_USETARGET_TYPE_SET && useValue == 0 ) {
        // Engage in unpress movement.
        button_unpress_move( self );
        return;
    }

    
    // Continuous button?
    const bool isContinuousUseTarget = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continuous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
        // Try again next frame:
        if ( isContinuousState ) {
            // Reinitiate this function.
            self->nextthink = level.time + FRAME_TIME_MS;
            self->think = button_think_return;
            // Trigger UseTargets
            SVG_UseTargets( self, self->activator, useType, useValue );
            // Dispatch a signal.
            SVG_SignalOut( self, self->other, self->activator, "OnContinuousPress" );
            return;
        }

        // Unpress if demanded:
        if ( !stayPressed && ( ( isContinuousUseTarget && !isContinuousState ) ) ) {
            // Engage in unpress movement.
            button_unpress_move( self );
        }
    } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ){
        //if ( !isContinuousState ) {
            // Engage in press movement.
            button_press_move( self );
        //}
    }
}
/**
*   @brief  For when '+usetarget' press triggered.
**/
void button_usetarget_press( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Don't act if still moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }

    // Only react if it is a client entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_IsClientEntity( activator, /* healthCheck=*/true ) ) {
        return;
    }

    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }
    
    // Set activator.
    self->activator = activator;
    // Set other.
    self->other = other;

    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
        // Unpress if demanded, make sure next think is lower than level.time so we can't press spam the button.
        if ( !stayPressed && self->nextthink < level.time ) {
            // Reinitiate this function.
            self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
            self->think = button_think_return;
        }
    } else {
        // Engage in press movement.
        button_press_move( self );
    }
}
/**
*   @brief  For when '+usetarget' press triggered.
**/
void button_usetarget_toggle( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Ignore triggers calling into fire when the button is still actively moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }

    // Only react if it is a client entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_IsClientEntity( activator, /* healthCheck=*/true ) ) {
        return;
    }

    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }

    // Set activator.
    self->activator = activator;
    // Set other.
    self->other = other;

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
        // Engage in unpress movement.
        button_unpress_move( self );
    } else {
        // Engage in press movement.
        button_press_move( self );
    }
}
/**
*   @brief  For when 'Touch' triggered.
**/
void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    // Do nothing if the button is still moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        // Do nothing:
        return;
    }

    // Only react if it is a client entity, or a monster entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_IsClientEntity( other, /* healthCheck=*/true ) 
        && ( SVG_HasSpawnFlags( other, BUTTON_SPAWNFLAG_NO_MONSTERS ) && !SVG_IsMonsterEntity( other ) ) ) {
        return;
    }

    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }

    // Is it toggleable?
    
    // Was it pressed from the start(i.e reversed).
    const bool startPressed = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // If it is a toggleable..
    self->touch = nullptr;

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED && !stayPressed ) {
        // Unpress,
        self->activator = other;
        self->other = other;
        button_unpress_move( self );
    } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED && !stayPressed ) {
        // Press,
        self->activator = other;
        self->other = other;
        button_press_move( self );
    }
}
/**
*   @brief  For when 'Death' triggered.
**/
void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    // Do nothing if the button is still moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        // Do nothing:
        return;
    }

    // Only react if it is a client entity, or a monster entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_IsClientEntity( attacker, /* healthCheck=*/true )
        && ( SVG_HasSpawnFlags( attacker, BUTTON_SPAWNFLAG_NO_MONSTERS ) && !SVG_IsMonsterEntity( attacker ) ) ) {
        return;
    }

    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }

    // Of course, only process if we're dealing with a damage trigger button.
    if ( SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES ) && self->max_health ) {
        // Reset its health.
        self->health = self->max_health;
        // But do not allow it to take damage when 'Pressed'.
        self->takedamage = DAMAGE_NO;
    }

    // Was it pressed from the start(i.e reversed).
    const bool startPressed = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED && !stayPressed ) {
        // Unpress,
        self->activator = attacker;
        self->other = attacker;
        button_unpress_move( self );
    } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED && !stayPressed ) {
        // Press,
        self->activator = attacker;
        self->other = attacker;
        button_press_move( self );
    }

    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnKilled" );
}


/**
*   @brief  Spawn function.
**/
void SP_func_button( edict_t *ent ) {
    // PushMove Entity Basics:
    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    #if 0
    if ( ent->targetNames.movewith ) {
        ent->movetype = MOVETYPE_PUSH;
    } else {
        ent->movetype = MOVETYPE_STOP;
    }
    #endif
    ent->movetype = MOVETYPE_STOP;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( ent, ent->model );

    // Default sounds:
    if ( ent->sounds != 1 ) {
        // 'Mechanic':
        ent->pushMoveInfo.sounds.start = gi.soundindex( "buttons/button_mechanic_press.wav" );
        ent->pushMoveInfo.sounds.end = gi.soundindex( "buttons/button_mechanic_unpress.wav" );
    }

    // PushMove defaults:
    if ( !ent->speed ) {
        ent->speed = 40;
    }
    if ( !ent->accel ) {
        ent->accel = ent->speed;
    }
    if ( !ent->decel ) {
        ent->decel = ent->speed;
    }
    if ( !st.lip ) {
        st.lip = 4;
    }
    // Trigger defaults:
    if ( !ent->wait ) {
        ent->wait = 3;
    }

    // Calculate absolute move distance to get from pos1 to pos2.
    const Vector3 fabsMoveDirection = QM_Vector3Fabs( ent->movedir );
    ent->pushMoveInfo.distance = QM_Vector3DotProduct( fabsMoveDirection, ent->size ) - st.lip;
    // Translate the determined move distance into the move direction to get pos2, our move end origin.
    ent->pos1 = ent->s.origin;
    ent->pos2 = QM_Vector3MultiplyAdd( ent->pos1, ent->pushMoveInfo.distance, ent->movedir );

    // if it starts open, switch the positions
    if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_START_PRESSED ) ) {
        VectorCopy( ent->pos2, ent->s.origin );
        ent->pos2 = ent->pos1;
        ent->pos1 = ent->s.origin;
        // Initial pressed state.
        ent->pushMoveInfo.state = BUTTON_STATE_PRESSED;
        // Initial texture frame setup.
        ent->pushMoveInfo.startFrame = BUTTON_FRAME_PRESSED_0;
        ent->pushMoveInfo.endFrame = BUTTON_FRAME_UNPRESSED_0;
    } else {
        // Initial 'unpressed' state.
        ent->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;
        // Initial texture frame setup.
        ent->pushMoveInfo.startFrame = BUTTON_FRAME_UNPRESSED_0;
        ent->pushMoveInfo.endFrame = BUTTON_FRAME_PRESSED_0;
    }
    // Initial start frame.
    ent->s.frame = ent->pushMoveInfo.startFrame;
    //ent->pushMoveInfo.startOrigin = ent->pos1;
    ent->pushMoveInfo.startAngles = ent->s.angles;
    //ent->pushMoveInfo.endOrigin = ent->pos2;
    ent->pushMoveInfo.endAngles = ent->s.angles;

    // Default trigger callback.
    ent->use = button_use;
    // Default animation effects.
    ent->s.effects |= EF_ANIM_CYCLE2_2HZ;

    // Used for condition checking, if we got a damage activating button we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based button:
    if ( damageActivates ) {
        // Set max health, also used to reinitialize the button to revive.
        ent->max_health = ent->health;
        // Let it take damage.
        ent->takedamage = DAMAGE_YES;
        // Die callback.
        ent->die = button_killed;
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_CalcMoveSpeed;
        // Touch based button:
    } else if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES ) ) {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_CalcMoveSpeed;
        // Trigger use callback.
        ent->touch = button_touch;
        // Otherwise check for +usetarget features of this button:
    } else {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_CalcMoveSpeed;

        // This button acts on a single press and fires its targets when reaching its end destination.
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            // Setup single press usage.
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            ent->use = button_usetarget_press;
        // This button is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            // Setup toggle press usage.
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            ent->use = button_usetarget_toggle;
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            // Setup continuous press usage.
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            ent->use = button_usetarget_continuous_press;
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_DISABLED ) ) {
            ent->useTarget.flags = (entity_usetarget_flags_t)( ent->useTarget.flags | ENTITY_USETARGET_FLAG_DISABLED );
        }
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_START_PRESSED ) ) {
        ent->pushMoveInfo.startOrigin = ent->pos2;
        ent->pushMoveInfo.startAngles = ent->s.angles;
        ent->pushMoveInfo.endOrigin = ent->pos1;
        ent->pushMoveInfo.endAngles = ent->s.angles;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        ent->pushMoveInfo.startOrigin = ent->pos1;
        ent->pushMoveInfo.startAngles = ent->s.angles;
        ent->pushMoveInfo.endOrigin = ent->pos2;
        ent->pushMoveInfo.endAngles = ent->s.angles;
    }

    // Link it in.
    gi.linkentity( ent );
}