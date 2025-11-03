/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_button'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_trigger.h"

#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_usetarget_hints.h"

#include "svgame/svg_utils.h"

#include "svgame/entities/func/svg_func_button.h"


// WID: TODO: The audio file we got is ugly so lol...
#define FUNC_ENABLE_END_SOUND 1







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
//void button_press_move_done( svg_base_edict_t *self );
//void button_unpress_move_done( svg_base_edict_t *self );
//void button_think_return( svg_base_edict_t *self );
//void button_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
//void button_killed( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point );



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
void button_determine_usetarget_hint_id( svg_func_button_t *self ) {
    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Damage button?
    const bool isDamageButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOUCH_ACTIVATES );
    // Toggle button?
    const bool isToggleButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOGGLEABLE );
    // Start pressed button?
    const bool isStartPressed = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED );
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
            if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_UNPRESS );
            // We are UnPressed:
            } else if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED ) {
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
            if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_UNTOGGLE );
            // We are UnPressed:
            } else if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED ) {
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
            if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED ) {
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_BUTTON_UNHOLD );
                // We are UnPressed:
            } else if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED ) {
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
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_button_t, onUnPressEndMove )( svg_func_button_t *self ) -> void {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE );

    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Damage button?
    const bool isDamageButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOUCH_ACTIVATES );
    // Toggle button?
    const bool isToggleButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOGGLEABLE );
    // Start pressed button?
    const bool isStartPressed = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // Engage 'Unpressed' state.
    self->pushMoveInfo.state = svg_func_button_t::STATE_UNPRESSED;

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
        //if ( !( self->flags & FL_TEAMSLAVE ) && self->pushMoveInfo.sounds.end ) {
        //    gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        //}
        if ( !( self->flags & FL_TEAMSLAVE ) && pushMoveInfo.sounds.end ) {
            //SVG_TempEventEntity_GeneralSoundEx( self, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.end, ATTN_STATIC );
            SVG_Util_AddGeneralSoundExEvent( this, EV_GENERAL_SOUND_EX, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.end, ATTN_STATIC );
        }
        #endif
        // Fire use targets.
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
    }

    // If it is a touch button, reassign the touch callback.
    if ( isTouchButton ) {
        if ( !isToggleButton ) {
            self->SetTouchCallback( nullptr );
        } else {
            self->SetTouchCallback( &svg_func_button_t::onTouch );
        }
    }

    // If it is a damage button, reassign the killed callback
    if ( isDamageButton ) {
        //if ( !isToggleButton ) {
        //    self->die = nullptr;
        //} else {
        self->SetDieCallback( &svg_func_button_t::onDie );
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
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_button_t, onPressEndMove )( svg_func_button_t *self ) -> void {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE );

    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Pressable button?
    const bool isPressUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );

    // Toggleable button?
    const bool isToggleUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Damage button?
    const bool isDamageButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOUCH_ACTIVATES );
    // Toggle button?
    const bool isToggleButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOGGLEABLE );
    // Start pressed button?
    const bool isStartPressed = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED);
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    /**
    *   Respond to the pressing of the button.
    **/
    // Apply "Pressed" state.
    self->pushMoveInfo.state = svg_func_button_t::STATE_PRESSED;

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
            self->SetTouchCallback( nullptr );
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
                self->SetThinkCallback( &svg_func_button_t::onThink_Return );
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
DEFINE_MEMBER_CALLBACK_THINK(svg_func_button_t, onThink_PressMove)( svg_func_button_t *self ) -> void {
    // Adjust movement state info.
    self->pushMoveInfo.state = svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE;

    // Determine the UseTargetHint ID.
    button_determine_usetarget_hint_id( self );

    // Play sound.
    if ( !( self->flags & FL_TEAMSLAVE ) && self->pushMoveInfo.sounds.start ) {
        //SVG_TempEventEntity_GeneralSoundEx( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, ATTN_STATIC );
        SVG_EntityEvent_GeneralSoundEx( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, ATTN_STATIC );
    }
    // Calculate and begin moving to the button's 'Pressed' state end origin.
    self->CalculateDirectionalMove( self->pushMoveInfo.endOrigin, reinterpret_cast<svg_pushmove_endcallback>( &SelfType::onPressEndMove ) );

    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnPress" );
    // Also a special one for touch buttons.
    const bool isTouchButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOUCH_ACTIVATES );
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
DEFINE_MEMBER_CALLBACK_THINK( svg_func_button_t, onThink_UnPressMove )( svg_func_button_t *self ) -> void {
    // Adjust movement state info.
    self->pushMoveInfo.state = svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE;

    // Determine the UseTargetHint ID.
    button_determine_usetarget_hint_id( self );

    // Adjust entity animation effects for the client to display, 
    // 
    // by determining the new material texture animation.
    //button_determine_animation( self, self->pushMoveInfo.state );
    #ifdef FUNC_ENABLE_END_SOUND
        // Play sound.
        if ( !( self->flags & FL_TEAMSLAVE ) && self->pushMoveInfo.sounds.end ) {
            //SVG_TempEventEntity_GeneralSoundEx( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, ATTN_STATIC );
            //SVG_Util_AddGeneralSoundExEvent( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, ATTN_STATIC );
            SVG_EntityEvent_GeneralSoundEx( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, ATTN_STATIC );
        }
    #endif
    // Calculate and begin moving back to the button's 'Unpressed' state start origin.
    self->CalculateDirectionalMove( self->pushMoveInfo.startOrigin, reinterpret_cast<svg_pushmove_endcallback>( &SelfType::onUnPressEndMove ) );
    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnUnPress" );
    // Also a special one for touch buttons.
    const bool isTouchButton = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOUCH_ACTIVATES );
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
*   @brief  
**/
/**
*   @brief  If idle, will toggle between the pressed and unpressed state. Used for "Toggle" Signalling.
**/
void svg_func_button_t::ToggleMove() {
    if ( pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM || pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN ) {
        svg_func_button_t::onThink_UnPressMove( this );
    } else {
        svg_func_button_t::onThink_PressMove( this );
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
DEFINE_MEMBER_CALLBACK_THINK( svg_func_button_t, onThink_Return )( svg_func_button_t *self ) -> void {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED );

    // Continous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Try again next frame.
    if (/* self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED &&*/ isContinuousUseTarget && isContinuousState ) {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->SetThinkCallback( &svg_func_button_t::onThink_Return );

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
        svg_func_button_t::onThink_UnPressMove( self );
        // If it is a continuous button...
        if ( isContinuousUseTarget ) {
            // Dispatch a signal.
            //button_lua_signal( self, "OnContinuousUnPress" );
        }

        // For damage based buttons.
        if ( SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_DAMAGE_ACTIVATES ) && self->max_health ) {
            // Allow it to take damage again.
            self->takedamage = DAMAGE_YES;
            self->health = self->max_health;
            self->SetDieCallback( &svg_func_button_t::onDie );
            // Dispatch a signal.
            SVG_SignalOut( self, self->other, self->activator, "OnRevive" );

            // Determine the UseTargetHint ID.
            button_determine_usetarget_hint_id( self );
        }
    //}
    // It is continuous and still held, so maintain this function as our 'think' callback.
    } else {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->SetThinkCallback( &svg_func_button_t::onThink_Return );

        // Determine the UseTargetHint ID.
        button_determine_usetarget_hint_id( self );
    }
}

/**
*   @brief  For when 'Use' triggered.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse )( svg_func_button_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    // button_use is never called by usage from monsters or clients, enabling this would get in the way of the remaining entities.
    #if 0
    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE ) {
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
    const bool isStartPressed = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // Start Unpressed Path( Default ):
    if ( isStartPressed ) {
        if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED ) {
            // Try again next frame:
            if ( isContinuousUseTarget && isContinuousState ) {
                // Reinitiate this function.
                self->nextthink = level.time + FRAME_TIME_MS;
                self->SetThinkCallback( &svg_func_button_t::onThink_Return );
                // Trigger UseTargets
                SVG_UseTargets( self, self->activator, useType, useValue );
                // Dispatch a signal.
                SVG_SignalOut( self, self->other, self->activator, "OnContinuousPress" );
                return;
            }

            // Unpress if demanded:
            if ( !stayPressed ) {
                // Engage in unpress movement.
                svg_func_button_t::onThink_UnPressMove( self );
            }
        } else {
            // Engage in press movement.
            svg_func_button_t::onThink_PressMove( self );
        }
    // Start pressed Path:
    } else {
        if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED ) {
            // Engage in press movement.
            svg_func_button_t::onThink_PressMove( self );
        } else {
            // Unpress if demanded:
            if ( !stayPressed ) {
                // Engage in unpress movement.
                svg_func_button_t::onThink_UnPressMove( self );
            }
        }
    }
}
/**
*   @brief  For when '+usetarget' press triggered.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse_UseTarget_Continuous_Press )( svg_func_button_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
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
        svg_func_button_t::onThink_UnPressMove( self );
        return;
    }

    // Ignore triggers calling into fire when the button is still actively moving.
    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }
    
    // Continuous button?
    const bool isContinuousUseTarget = SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continuous active?
    const bool isContinuousState = SVG_Entity_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED ) {
        // Try again next frame:
        if ( isContinuousState ) {
            // Reinitiate this function.
            self->nextthink = level.time + FRAME_TIME_MS;
            self->SetThinkCallback( &svg_func_button_t::onThink_Return );
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
            svg_func_button_t::onThink_UnPressMove( self );
        }
    } else if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED ) {
        //if ( !isContinuousState ) {
            // Engage in press movement.
        svg_func_button_t::onThink_PressMove( self );
        //}
    }
}
/**
*   @brief  For when '+usetarget' press triggered.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse_UseTarget_Press )( svg_func_button_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    // Don't act if still moving.
    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE ) {
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

    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED ) {
        // Unpress if demanded, make sure next think is lower than level.time so we can't press spam the button.
        if ( !stayPressed && self->nextthink < level.time ) {
            // Reinitiate this function.
            self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
            self->SetThinkCallback( &svg_func_button_t::onThink_Return );
        }
    } else {
        // Engage in press movement.
        svg_func_button_t::onThink_PressMove( self );
    }
}
/**
*   @brief  For when '+usetarget' press triggered.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse_UseTarget_Toggle )( svg_func_button_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
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
    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }

    // Set activator.
    self->activator = activator;
    // Set other.
    self->other = other;

    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED ) {
        // Engage in unpress movement.
        svg_func_button_t::onThink_UnPressMove( self );
    } else if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED ) {
        // Engage in press movement.
        svg_func_button_t::onThink_PressMove( self );
    }
}
/**
*   @brief  For when 'Touch' triggered.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_func_button_t, onTouch )( svg_func_button_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    // Do nothing if the button is still moving.
    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE ) {
        // Do nothing:
        return;
    }

    // Only react if it is a client entity, or a monster entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_Entity_IsClient( other, /* healthCheck=*/true ) 
        && ( SVG_HasSpawnFlags( other, svg_func_button_t::SPAWNFLAG_NO_MONSTERS ) && !SVG_Entity_IsMonster( other ) ) ) {
        return;
    }

    // Are we locked?
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // TODO: Play some locked sound.
        return;
    }

    // Is it toggleable?
    
    // Was it pressed from the start(i.e reversed).
    const bool startPressed = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // If it is a toggleable..
    self->SetTouchCallback( nullptr );

    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED && !stayPressed ) {
        // Unpress,
        self->activator = other;
        self->other = other;
        svg_func_button_t::onThink_UnPressMove( self );
    } else if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED && !stayPressed ) {
        // Press,
        self->activator = other;
        self->other = other;
        svg_func_button_t::onThink_PressMove( self );
    }
}
/**
*   @brief  For when 'Death' triggered.
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_func_button_t, onDie )( svg_func_button_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
    // Do nothing if the button is still moving.
    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == svg_func_button_t::STATE_MOVING_TO_UNPRESSED_STATE ) {
        // Do nothing:
        return;
    }

    // Only react if it is a client entity, or a monster entity which is touch triggering this button.
    // If it is a dead client, ignore triggering it.
    if ( !SVG_Entity_IsClient( attacker, /* healthCheck=*/true )
        && ( SVG_HasSpawnFlags( attacker, svg_func_button_t::SPAWNFLAG_NO_MONSTERS ) && !SVG_Entity_IsMonster( attacker ) ) ) {
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
    if ( SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_DAMAGE_ACTIVATES ) && self->max_health ) {
        // Reset its health.
        self->health = self->max_health;
        // But do not allow it to take damage when 'Pressed'.
        self->takedamage = DAMAGE_NO;
    }

    // Was it pressed from the start(i.e reversed).
    const bool startPressed = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    if ( self->pushMoveInfo.state == svg_func_button_t::STATE_PRESSED && !stayPressed ) {
        // Unpress,
        self->activator = attacker;
        self->other = attacker;
        svg_func_button_t::onThink_UnPressMove( self );
    } else if ( self->pushMoveInfo.state == svg_func_button_t::STATE_UNPRESSED && !stayPressed ) {
        // Press,
        self->activator = attacker;
        self->other = attacker;
        svg_func_button_t::onThink_PressMove( self );
    }

    // Dispatch a signal.
    SVG_SignalOut( self, self->other, self->activator, "OnKilled" );
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
DEFINE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_button_t, onSignalIn )( svg_func_button_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void {
    /**
    *   Press/UnPress/Toggle:
    **/
    // Press:
    if ( Q_strcasecmp( signalName, "Press" ) == 0 ) {
        // Unpress,
        self->activator = activator;
        self->other = other;
        svg_func_button_t::onThink_PressMove( self );
    }
    // UnPress:
    if ( Q_strcasecmp( signalName, "UnPress" ) == 0 ) {
        // Unpress,
        self->activator = activator;
        self->other = other;
        svg_func_button_t::onThink_UnPressMove( self );
    }
    // ButtonToggle:
    if ( Q_strcasecmp( signalName, "Toggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Toggle Move.
        self->ToggleMove();
    }

    /**
    *   Lock/UnLock:
    **/
    // RotatingLock:
    if ( Q_strcasecmp( signalName, "Lock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Lock itself.
        self->SetLockState( true );
    }
    // RotatingUnlock:
    if ( Q_strcasecmp( signalName, "Unlock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Unlock itself.
        self->SetLockState( false );
    }
    // RotatingLockToggle:
    if ( Q_strcasecmp( signalName, "LockToggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Lock if unlocked:
        if ( !self->pushMoveInfo.lockState.isLocked ) {
            self->SetLockState( true );
        // Unlock if locked:
        } else {
            self->SetLockState( false );
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
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_button_t, onSpawn )( svg_func_button_t *self ) -> void {
    Super::onSpawn( self );

    // PushMove Entity Basics:
    SVG_Util_SetMoveDir( self->s.angles, self->movedir );
    #if 0
    if ( self->targetNames.movewith ) {
        self->movetype = MOVETYPE_PUSH;
    } else {
        self->movetype = MOVETYPE_STOP;
    }
    #endif
    self->movetype = MOVETYPE_STOP;
    self->solid = SOLID_BSP;
    self->s.entityType = ET_PUSHER;
    self->SetOnSignalInCallback( &svg_func_button_t::onSignalIn );
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( self, self->model );

    // Default sounds:
    if ( self->sounds != 1 ) {
        // 'Mechanic':
        self->pushMoveInfo.sounds.start = gi.soundindex( "buttons/button_mechanic_press.wav" );
        self->pushMoveInfo.sounds.end = gi.soundindex( "buttons/button_mechanic_unpress.wav" );
    }

    // PushMove defaults:
    if ( !self->speed ) {
        self->speed = 40;
    }
    if ( !self->accel ) {
        self->accel = self->speed;
    }
    if ( !self->decel ) {
        self->decel = self->speed;
    }
    if ( !self->lip ) {
        self->lip = 4;
    }
    // Trigger defaults:
    if ( !self->wait ) {
        self->wait = 3;
    }

    // Calculate absolute move distance to get from pos1 to pos2.
    const Vector3 fabsMoveDirection = QM_Vector3Fabs( self->movedir );
    self->pushMoveInfo.distance = QM_Vector3DotProduct( fabsMoveDirection, self->size ) - self->lip;
    // Translate the determined move distance into the move direction to get pos2, our move end origin.
    self->pos1 = self->s.origin;
    self->pos2 = QM_Vector3MultiplyAdd( self->pos1, self->pushMoveInfo.distance, self->movedir );

    // if it starts open, switch the positions
    const bool startPressed = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED );
    if ( startPressed ) {
        VectorCopy( self->pos2, self->s.origin );
        self->pos2 = self->pos1;
        self->pos1 = self->s.origin;
        // Initial pressed state.
        self->pushMoveInfo.state = svg_func_button_t::STATE_PRESSED;
        // Initial texture position state animations frame setup.
        self->pushMoveInfo.startFrame = svg_func_button_t::FRAME_PRESSED_0;
        self->pushMoveInfo.endFrame = svg_func_button_t::FRAME_UNPRESSED_0;

        // Initial start frame.
        self->s.frame = self->pushMoveInfo.startFrame;
        // Initial animation. ( Cycles between 0 and 1. )
        // <Q2RTXP>: WID: TODO: Guess it's nice if you can determine animation style yourself, right?
        // if ( SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_ANIMATED ) ) {
        self->s.effects |= EF_ANIM_CYCLE2_2HZ;
        // }
    } else {
        // Initial 'unpressed' state.
        self->pushMoveInfo.state = svg_func_button_t::STATE_UNPRESSED;
        // Initial texture frame setup.
        self->pushMoveInfo.startFrame = svg_func_button_t::FRAME_UNPRESSED_0;
        self->pushMoveInfo.endFrame = svg_func_button_t::FRAME_PRESSED_0;

        // Initial start frame.
        self->s.frame = self->pushMoveInfo.startFrame;
        // Initial animation. ( Cycles between 0 and 1. )
        // <Q2RTXP>: WID: TODO: Guess it's nice if you can determine animation style yourself, right?
        // if ( SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_ANIMATED ) ) {
        self->s.effects |= EF_ANIM_CYCLE2_2HZ;
        // }
    }

    //self->pushMoveInfo.startOrigin = self->pos1;
    self->pushMoveInfo.startAngles = self->s.angles;
    //self->pushMoveInfo.endOrigin = self->pos2;
    self->pushMoveInfo.endAngles = self->s.angles;

    // Default trigger callback.
    self->SetUseCallback( &svg_func_button_t::onUse );

    // Used for condition checking, if we got a damage activating button we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based button:
    if ( damageActivates ) {
        // Set max health, also used to reinitialize the button to revive.
        self->max_health = self->health;
        // Let it take damage.
        self->takedamage = DAMAGE_YES;
        // Die callback.
        self->SetDieCallback( &svg_func_button_t::onDie );
        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_button_t::SVG_PushMove_Think_CalculateMoveSpeed );
    // Touch based button:
    } else if ( SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_TOUCH_ACTIVATES ) ) {
        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_button_t::SVG_PushMove_Think_CalculateMoveSpeed );
        // Trigger use callback.
        self->SetTouchCallback( &svg_func_button_t::onTouch );
    // Otherwise check for +usetarget features of this button:
    } else {
        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_button_t::SVG_PushMove_Think_CalculateMoveSpeed );

        // This button acts on a single press and fires its targets when reaching its end destination.
        if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            // Setup single press usage.
            self->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            self->SetUseCallback( &svg_func_button_t::onUse_UseTarget_Press );
        // This button is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            // Setup toggle press usage.
            self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            self->SetUseCallback( &svg_func_button_t::onUse_UseTarget_Toggle );
        } else if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            // Setup continuous press usage.
            self->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            self->SetUseCallback( &svg_func_button_t::onUse_UseTarget_Continuous_Press );
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_DISABLED ) ) {
            self->useTarget.flags |= ENTITY_USETARGET_FLAG_DISABLED;
        }

        // Determine the initial UseTargetHint ID.
        button_determine_usetarget_hint_id( self );
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    self->pushMoveInfo.speed = self->speed;
    self->pushMoveInfo.accel = self->accel;
    self->pushMoveInfo.decel = self->decel;
    self->pushMoveInfo.wait = self->wait;
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( self, svg_func_button_t::SPAWNFLAG_START_PRESSED ) ) {
        self->pushMoveInfo.startOrigin = self->pos2;
        self->pushMoveInfo.startAngles = self->s.angles;
        self->pushMoveInfo.endOrigin = self->pos1;
        self->pushMoveInfo.endAngles = self->s.angles;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        self->pushMoveInfo.startOrigin = self->pos1;
        self->pushMoveInfo.startAngles = self->s.angles;
        self->pushMoveInfo.endOrigin = self->pos2;
        self->pushMoveInfo.endAngles = self->s.angles;
    }

    // Link it in.
    gi.linkentity( self );
}