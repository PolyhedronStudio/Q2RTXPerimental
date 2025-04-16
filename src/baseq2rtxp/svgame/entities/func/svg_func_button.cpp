/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_button'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "sharedgame/sg_usetarget_hints.h"

#include "svgame/svg_utils.h"
#include "svgame/svg_lua.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_button.h"


// WID: TODO: The audio file we got is ugly so lol...
#define FUNC_BUTTON_ENABLE_END_SOUND 1



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
void button_press_move_done( svg_base_edict_t *self );
void button_unpress_move_done( svg_base_edict_t *self );
void button_think_return( svg_base_edict_t *self );
void button_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
void button_killed( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );



/**
*
*
*
*   UseTargetHint ID-ing:
*
*
*
**/
/**
*   @brief  Called by spawn, move begin, and move end functions in order to determine 
*           the appropriate UseTargetHint ID to use.
**/
void button_determine_usetarget_hint_id( svg_base_edict_t *self ) {
    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
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

    // Press Buttons:
    if ( isPressUseTarget ) {
        // None if meant to remain pressed.
        if ( stayPressed ) {
            SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NONE );
        // Otherwise:
        } else {
            // Are we Pressed?
            if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_UNPRESS );
            // We are UnPressed:
            } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_PRESS );
            // We are transitioning:
            } else {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NONE );
            }
        }
    }
    // Toggle Buttons:
    else if ( isToggleUseTarget ) {
        // None if meant to remain pressed.
        if ( stayPressed ) {
            SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NONE );
        // Otherwise:
        } else {
            // Are we Pressed?
            if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_UNTOGGLE );
            // We are UnPressed:
            } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_TOGGLE );
            // We are transitioning:
            } else {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NONE );
            }
        }
    }
    // Continuous Buttons:
    else if ( isContinuousUseTarget ) {
        //// None if meant to remain pressed.
        //if ( stayPressed ) {
        //    SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NONE );
        //    // Otherwise:
        //} else {
            // Are we Pressed?
            if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_UNHOLD );
                // We are UnPressed:
            } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_HOLD );
                // We are transitioning:
            } else {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NONE );
            }
        //}
    }
}
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
void button_unpress_move_done( svg_base_edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE );

    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
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

    // Determine the UseTargetHint ID.
    button_determine_usetarget_hint_id( self );

    /**
    *   Adjust animation and/or frame.
    **/
    // We're unpressed, thus use the 'start' frame.
    //self->s.frame = ( !isStartPressed ? self->pushMoveInfo.endFrame : self->pushMoveInfo.startFrame );
    self->s.frame = ( isStartPressed ? self->pushMoveInfo.endFrame : self->pushMoveInfo.startFrame );
    // Adjust entity animation effects for the client to display.
    // We use EF_ANIM_CYCLE2_2HZ at spawn instead.
    #if 0
    if ( self->s.effects & EF_ANIM23 ) {
        self->s.effects &= ~EF_ANIM23;
        self->s.effects |= EF_ANIM01;
    } else if ( self->s.effects & EF_ANIM01 ) {
        self->s.effects &= ~EF_ANIM01;
        self->s.effects |= EF_ANIM23;
    }
    #endif

    /**
    *   Respond to the UnPressing of the button.
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

    // Signal its unpress.
    if ( isContinuousUseTarget && !isContinuousState ) {
        // Fire use targets.
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
        // Dispatch a signal.
        SVG_SignalOut( self, self->other, self->activator, "OnContinuousUnPressed" );
    }

    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnUnPressed" );
}

/**
*   @brief  PushMoveInfo callback for when the button reaches its 'Pressed' state.
**/
void button_press_move_done( svg_base_edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE );

    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
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
    //self->s.frame = ( !isStartPressed ? self->pushMoveInfo.endFrame : self->pushMoveInfo.startFrame );
    self->s.frame = ( isStartPressed ? self->pushMoveInfo.startFrame : self->pushMoveInfo.endFrame );
    // Adjust entity animation effects for the client to display.
    // We use EF_ANIM_CYCLE2_2HZ at spawn instead.
    #if 0
    if ( self->s.effects & EF_ANIM23 ) {
        self->s.effects &= ~EF_ANIM23;
        self->s.effects |= EF_ANIM01;
    } else if ( self->s.effects & EF_ANIM01 ) {
        self->s.effects &= ~EF_ANIM01;
        self->s.effects |= EF_ANIM23;
    }
    #endif

    if ( isTouchButton ) {
        // If it is a touch button, reassign the touch callback.
        if ( !isToggleButton ) {
            self->touch = nullptr;
        }
    }
    
    // If button has to remain pushed.
    if ( stayPressed 
        // or is a 'Touchable',
        /* || isTouchButton || */
        // or is a 'Toggleable',
        || ( isToggleButton && isToggleUseTarget ) 
        // or a 'Continuous Usable':
        || ( isContinuousState || isContinuousUseTarget ) )
        //|| ( isContinuousUseTarget ) ) 
    {
            // Don't do anything in regards to applying the wait time.
    // If a wait time has been set, use it for when to trigger the button's return(unpressing).
    } else {
        // Is press button?
        if ( self->nextthink < level.time ) {
            if ( self->pushMoveInfo.wait >= 0 ) {
                self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
                self->think = button_think_return;
            }
        }
    }

    // Determine the initial UseTargetHint ID.
    button_determine_usetarget_hint_id( self );

    // Trigger UseTargets
    if ( !isContinuousUseTarget ) {
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 1 );
    }
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
void button_press_move( svg_base_edict_t *self ) {
    // Adjust movement state info.
    self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_PRESSED_STATE;

    // Determine the UseTargetHint ID.
    button_determine_usetarget_hint_id( self );

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
    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );
    // Signal its unpress.
    if ( isContinuousUseTarget && !isContinuousState ) {
        // Dispatch a signal.
        SVG_SignalOut( self, self->other, self->activator, "OnContinuousPress" );
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
void button_unpress_move( svg_base_edict_t *self ) {
    // Adjust movement state info.
    self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_UNPRESSED_STATE;

    // Determine the UseTargetHint ID.
    button_determine_usetarget_hint_id( self );

    // Adjust entity animation effects for the client to display, 
    // 
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
    
    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );
    // Signal its unpress.
    if ( isContinuousUseTarget && !isContinuousState ) {
        // Dispatch a signal.
        SVG_SignalOut( self, self->other, self->activator, "OnContinuousUnPress" );
    }
}

/**
*   @brief  Used for "Toggle" Signalling.
**/
void button_toggle_move( svg_base_edict_t *self ) {
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM || self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN ) {
        button_unpress_move( self );
    } else {
        button_press_move( self );
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
void button_think_return( svg_base_edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_PRESSED );

    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Try again next frame.
    if (/* self->pushMoveInfo.state == BUTTON_STATE_PRESSED &&*/ isContinuousUseTarget && isContinuousState ) {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->think = button_think_return;

        // Trigger UseTargets
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_SET, 1 );

        // Dispatch a signal.
        SVG_SignalOut( self, self->other, self->activator, "OnContinuousPressed" );

        // Determine the UseTargetHint ID.
        button_determine_usetarget_hint_id( self );

        // Get out.
        return;
    }

    // Move back to the button's 'Unpressed' state.
    if ( !isContinuousState ) {
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
void button_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
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
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continuous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
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
void button_usetarget_continuous_press( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Only react if it is a client entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_Entity_IsClient( activator, /* healthCheck=*/true ) ) {
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

    // The client let go of the use key.
    if ( useType == ENTITY_USETARGET_TYPE_SET && useValue == 0 ) {
        // Engage in unpress movement.
        button_unpress_move( self );
        return;
    }

    // Ignore triggers calling into fire when the button is still actively moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }
    
    // Continuous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continuous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

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
            SVG_SignalOut( self, self->other, self->activator, "OnContinuousPressed" );

            // Determine the UseTargetHint ID.
            button_determine_usetarget_hint_id( self );
            return;
        }

        // Unpress if demanded:
        if ( !stayPressed && ( ( isContinuousUseTarget && !isContinuousState ) ) ) {
            // Engage in unpress movement.
            button_unpress_move( self );
        }
    } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
        //if ( !isContinuousState ) {
            // Engage in press movement.
            button_press_move( self );
        //}
    }
}
/**
*   @brief  For when '+usetarget' press triggered.
**/
void button_usetarget_press( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Don't act if still moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }

    // Only react if it is a client entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_Entity_IsClient( activator, /* healthCheck=*/true ) ) {
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
            self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
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
void button_usetarget_toggle( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Only react if it is a client entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_Entity_IsClient( activator, /* healthCheck=*/true ) ) {
        return;
    }

    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }

    // Ignore triggers calling into fire when the button is still actively moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }

    // Set activator.
    self->activator = activator;
    // Set other.
    self->other = other;

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
        // Engage in unpress movement.
        button_unpress_move( self );
    } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
        // Engage in press movement.
        button_press_move( self );
    }
}
/**
*   @brief  For when 'Touch' triggered.
**/
void button_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
    // Do nothing if the button is still moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        // Do nothing:
        return;
    }

    // Only react if it is a client entity, or a monster entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_Entity_IsClient( other, /* healthCheck=*/true ) 
        && ( SVG_HasSpawnFlags( other, BUTTON_SPAWNFLAG_NO_MONSTERS ) && !SVG_Entity_IsMonster( other ) ) ) {
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
void button_killed( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point ) {
    // Do nothing if the button is still moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        // Do nothing:
        return;
    }

    // Only react if it is a client entity, or a monster entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_Entity_IsClient( attacker, /* healthCheck=*/true )
        && ( SVG_HasSpawnFlags( attacker, BUTTON_SPAWNFLAG_NO_MONSTERS ) && !SVG_Entity_IsMonster( attacker ) ) ) {
        return;
    }

    // WID: TODO: ?? Dead == Dead...
    #if 0 
    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }
    #endif

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
void button_lock( svg_base_edict_t *self ) {
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
void button_unlock( svg_base_edict_t *self ) {
    // Of course it has to be locked if we want to play a sound.
    if ( self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.unlockingSound ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.unlockingSound, 1, ATTN_STATIC, 0 );
    }
    // Last but not least, unlock its state.
    self->pushMoveInfo.lockState.isLocked = false;
}



/***
*
*
*
*   Signal Handling:
*
*
*
***/
/**
*   @brief  Signal Receiving:
**/
void button_onsignalin( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    /**
    *   Press/UnPress/Toggle:
    **/
    // Press:
    if ( Q_strcasecmp( signalName, "Press" ) == 0 ) {
        // Unpress,
        self->activator = activator;
        self->other = other;
        button_press_move( self );
    }
    // UnPress:
    if ( Q_strcasecmp( signalName, "UnPress" ) == 0 ) {
        // Unpress,
        self->activator = activator;
        self->other = other;
        button_unpress_move( self );
    }
    // ButtonToggle:
    if ( Q_strcasecmp( signalName, "Toggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Toggle Move.
        button_toggle_move( self );
    }

    /**
    *   Lock/UnLock:
    **/
    // RotatingLock:
    if ( Q_strcasecmp( signalName, "Lock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Lock itself.
        button_lock( self );
    }
    // RotatingUnlock:
    if ( Q_strcasecmp( signalName, "Unlock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Unlock itself.
        button_unlock( self );
    }
    // RotatingLockToggle:
    if ( Q_strcasecmp( signalName, "LockToggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Lock if unlocked:
        if ( !self->pushMoveInfo.lockState.isLocked ) {
            button_lock( self );
        // Unlock if locked:
        } else {
            button_unlock( self );
        }
    }

    // WID: Useful for debugging.
    #if 0
    const int32_t otherNumber = ( other ? other->s.number : -1 );
    const int32_t activatorNumber = ( activator ? activator->s.number : -1 );
    gi.dprintf( "button_onsignalin[ self(#%d), \"%s\", other(#%d), activator(%d) ]\n", self->s.number, signalName, otherNumber, activatorNumber );
    #endif
}



/***
*
*
*
*   Spawn:
*
*
*
***/
/**
*   @brief  Spawn function.
**/
void SP_func_button( svg_base_edict_t *ent ) {
    // PushMove Entity Basics:
    SVG_Util_SetMoveDir( ent->s.angles, ent->movedir );
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
    ent->onsignalin = button_onsignalin;
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
    const bool startPressed = SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_START_PRESSED );
    if ( startPressed ) {
        VectorCopy( ent->pos2, ent->s.origin );
        ent->pos2 = ent->pos1;
        ent->pos1 = ent->s.origin;
        // Initial pressed state.
        ent->pushMoveInfo.state = BUTTON_STATE_PRESSED;
        // Initial texture position state animations frame setup.
        ent->pushMoveInfo.startFrame = BUTTON_FRAME_PRESSED_0;
        ent->pushMoveInfo.endFrame = BUTTON_FRAME_UNPRESSED_0;

        // Initial start frame.
        ent->s.frame = ent->pushMoveInfo.startFrame;
        // Initial animation. ( Cycles between 0 and 1. )
        // <Q2RTXP>: WID: TODO: Guess it's nice if you can determine animation style yourself, right?
        // if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_ANIMATED ) ) {
        ent->s.effects |= EF_ANIM_CYCLE2_2HZ;
        // }
    } else {
        // Initial 'unpressed' state.
        ent->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;
        // Initial texture frame setup.
        ent->pushMoveInfo.startFrame = BUTTON_FRAME_UNPRESSED_0;
        ent->pushMoveInfo.endFrame = BUTTON_FRAME_PRESSED_0;

        // Initial start frame.
        ent->s.frame = ent->pushMoveInfo.startFrame;
        // Initial animation. ( Cycles between 0 and 1. )
        // <Q2RTXP>: WID: TODO: Guess it's nice if you can determine animation style yourself, right?
        // if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_ANIMATED ) ) {
        ent->s.effects |= EF_ANIM_CYCLE2_2HZ;
        // }
    }

    //ent->pushMoveInfo.startOrigin = ent->pos1;
    ent->pushMoveInfo.startAngles = ent->s.angles;
    //ent->pushMoveInfo.endOrigin = ent->pos2;
    ent->pushMoveInfo.endAngles = ent->s.angles;

    // Default trigger callback.
    ent->use = button_use;

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
        ent->think = SVG_PushMove_Think_CalculateMoveSpeed;
    // Touch based button:
    } else if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES ) ) {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = SVG_PushMove_Think_CalculateMoveSpeed;
        // Trigger use callback.
        ent->touch = button_touch;
    // Otherwise check for +usetarget features of this button:
    } else {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = SVG_PushMove_Think_CalculateMoveSpeed;

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
            ent->useTarget.flags |= ENTITY_USETARGET_FLAG_DISABLED;
        }

        // Determine the initial UseTargetHint ID.
        button_determine_usetarget_hint_id( ent );
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