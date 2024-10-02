/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_button'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

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
static constexpr int32_t BUTTON_SPAWNFLAG_LOCKED            = BIT( 4 );
//! Button stays pushed until reused.
//static constexpr int32_t BUTTON_SPAWNFLAG_TOGGLE      = BIT( 5 );    // This is a target use flag.
//! Button fires targets when pressed/used.
//static constexpr int32_t BUTTON_SPAWNFLAG_PRESS_ACTIVATES   = BIT( 6 ); // This is also a target use flag.


/**
*   For readability's sake:
**/
static constexpr int32_t BUTTON_STATE_PRESSED   = PUSHMOVE_STATE_TOP;
static constexpr int32_t BUTTON_STATE_UNPRESSED = PUSHMOVE_STATE_BOTTOM;
static constexpr int32_t BUTTON_STATE_MOVING_TO_PRESSED_STATE   = PUSHMOVE_STATE_MOVING_UP;
static constexpr int32_t BUTTON_STATE_MOVING_TO_UNPRESSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;


/**
*   Trigger respond actions determining what the button should do.
**/
typedef enum button_response_e {
    BUTTON_RESPONSE_NOTHING,
    BUTTON_RESPONSE_PRESS,
    BUTTON_RESPONSE_UNPRESS,
} button_response_t;
// Debugging reasons:
#if 0
static inline constexpr std::string GetButtonResponseString( const button_response_t &buttonResponse ) {
    if ( buttonResponse == BUTTON_RESPONSE_NOTHING ) {
        return "BUTTON_RESPONSE_NOTHING";
    } else if ( buttonResponse == BUTTON_RESPONSE_PRESS ) {
        return "BUTTON_RESPONSE_PRESS";
    } else if ( buttonResponse == BUTTON_RESPONSE_UNPRESS ) {
        return "BUTTON_RESPONSE_UNPRESS";
    } else {
        return "<UNKNOWN BUTTON_RESPONSE #" + std::to_string( buttonResponse ) + ">";
    }
}
#endif
static button_response_t button_response_to_trigger( edict_t *self ) {
    // Touch Activator.
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );
    // Remain pressed, or wait a specified amount of time?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // UseTarget button behavior type:
    const bool isDisabledButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_DISABLED );
    const bool isContinuousButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    const bool isPressButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_PRESS );
    const bool isToggleButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );

    // If for whatever reason we are disabled to be useTargetted anymore, do nothing of course.
    if ( isDisabledButton ) {
        return BUTTON_RESPONSE_NOTHING;
    }

    // Do nothing if the button is still moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE
        // Do nothing if it is pressed and waiting to return after time has passed.
        || ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED && !stayPressed && !isToggleButton ) ) {
        // Do nothing:
        return BUTTON_RESPONSE_NOTHING;
    }

    //// Logic for PRESSABLE buttons:
    //if ( self->useTarget.flags & ENTITY_USETARGET_FLAG_PRESS ) {
    //    // If pressed, and not set to never return(wait == -1) then engage unpressing.
    //    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED && self->wait != -1 ) {
    //        return BUTTON_RESPONSE_UNPRESS;
    //    // Otherwise, engage into pressing the button instead.
    //    } else {
    //        return BUTTON_RESPONSE_PRESS;
    //    }
    //}

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
        if ( isToggleButton && !stayPressed ) {
            return BUTTON_RESPONSE_UNPRESS;
        }
    } else {
        return BUTTON_RESPONSE_PRESS;
    }
    //// Logic for TOGGLEABLE buttons:
    //if ( self->useTarget.flags & ENTITY_USETARGET_FLAG_TOGGLE ) {
    //    // If pressed, then engage unpressing.
    //        return BUTTON_RESPONSE_UNPRESS;
    //    // Otherwise, engage into pressing the button instead.
    //    } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
    //        return BUTTON_RESPONSE_PRESS;
    //    }
    //}

    return BUTTON_RESPONSE_NOTHING;
}


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

/**
*   @brief  Will call back upon the specified `luaName_eventName(..)` function, if existent.
**/
void button_lua_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t &useType, const int32_t useValue ) {

    // WID: LUA: Call the event function if it exists.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_Use";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, self, other, activator, useType, useValue );
        }
    }
}

/**
*   @brief  Will notify the entity about the signal being fired.
**/
void button_lua_signal( edict_t *self, const std::string &signalName ) {
    // WID: LUA: Call the event function if it exists.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_OnSignal";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, self, self->activator, signalName );
        }
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
void button_unpress_move_done( edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE );

    // Toggleable button?
    const bool isToggleButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );

    // Engage 'Unpressed' state.
    self->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;

    // Adjust entity animation effects for the client to display.
    self->s.effects &= ~EF_ANIM23;
    self->s.effects |= EF_ANIM01;

    // Respond to the untoggling of the button.
    if ( isToggleButton ) {
        #if 0
        // Play sound.
        if ( self->pushMoveInfo.sound_end && !( self->flags & FL_TEAMSLAVE ) ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        }
        #endif
        // Fire use targets.
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
    }

    // If it is a touch button, reassign the touch callback.
    if ( !isTouchButton ) {
        self->touch = nullptr;
    } else {
        self->touch = button_touch;
    }

    // Dispatch a lua signal.
    button_lua_signal( self, "OnUnPressed" );
}

/**
*   @brief  PushMoveInfo callback for when the button reaches its 'Pressed' state.
**/
void button_press_move_done( edict_t *self ) {
    // Assert.
    Q_DevAssert( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE );

    // Continous button?
    const bool isContinuousButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );
    
    // Toggleable button?
    const bool isToggleButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    // Change texture animation from.
    self->s.frame = 1;

    // Adjust entity animation effects for the client to display.
    self->s.effects &= ~EF_ANIM01;
    self->s.effects |= EF_ANIM23;

    // Apply "Pressed" state.
    self->pushMoveInfo.state = BUTTON_STATE_PRESSED;

    // If button has to remain pushed, or is a 'Toggleable':
    if ( stayPressed || isToggleButton ) {
        // If it is a touch button, reassign the touch callback.
        if ( !isTouchButton ) {
            self->touch = nullptr;
        } else {
            self->touch = button_touch;
        }
    // If a wait time has been set, use it for when to trigger the button's return(unpressing).
    } else {
        if ( self->pushMoveInfo.wait >= 0 ) {
            self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
            self->think = button_think_return;
        }
    }


    // Trigger UseTargets
    SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
    // Dispatch a lua signal.
    button_lua_signal( self, "OnPressed" );
}



/**
*
*
*
*   Press/UnPress Utilities:
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
    if ( self->pushMoveInfo.sound_start && !( self->flags & FL_TEAMSLAVE ) ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
    }
    // Calculate and begin moving to the button's 'Pressed' state end origin.
    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, button_press_move_done );
    // Dispatch a lua signal.
    button_lua_signal( self, "OnPress" );
}
/**
*   @brief  Engages moving into the 'Unpressed' state, after which at arrival, it calls upon 'button_unpress_move_done'.
**/
void button_unpress_move( edict_t *self ) {
    // Change the frame(texture index).
    self->s.frame = 0;
    // Adjust movement state info.
    self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_UNPRESSED_STATE;
    #ifdef FUNC_BUTTON_ENABLE_END_SOUND
    // Play sound.
    if ( self->pushMoveInfo.sound_end && !( self->flags & FL_TEAMSLAVE ) ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
    }
    #endif
    // Calculate and begin moving back to the button's 'Unpressed' state start origin.
    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.start_origin, button_unpress_move_done );
    // Dispatch a lua signal.
    button_lua_signal( self, "OnUnPress" );
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
    const bool isContinuousButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );

    // Try again next frame.
    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED && isContinuousButton && isContinuousState ) {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->think = button_think_return;
        // Trigger UseTargets
        SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_SET, 1 );
        return;
    }

    // Move back to the button's 'Unpressed' state.
    if ( !SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS ) ) {
        // Unpress itself.
        button_unpress_move( self );

        // For damage based buttons.
        if ( SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES ) && self->health ) {
            self->takedamage = DAMAGE_YES;
        }
    } else {
        self->nextthink = level.time + FRAME_TIME_MS;
        self->think = button_think_return;
    }

}

/**
*   @brief  Actual implementation for 'triggering'/'firing' the button.
**/
void button_trigger( edict_t *self, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    // Ignore triggers calling into fire when the button is still actively moving.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE
        || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }

    // Continuous button?
    const bool isContinuousButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_CONTINUOUS );
    // Continuous active?
    const bool isContinuousState = SVG_UseTarget_HasUseTargetState( self, ENTITY_USETARGET_STATE_CONTINUOUS );


    // Toggleable button?
    const bool isToggleButton = SVG_UseTarget_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE );
    // Touch button?
    const bool isTouchButton = SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES );
    // Is it a wait button? Or meant to remain pressed?
    const bool stayPressed = ( self->wait == -1 ? true : false );

    if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
        // Try again next frame:
        if ( isContinuousButton && isContinuousState ) {
            // Reinitiate this function.
            self->nextthink = level.time + FRAME_TIME_MS;
            self->think = button_think_return;
            // Trigger UseTargets
            SVG_UseTargets( self, self->activator, useType, useValue );
            // Dispatch a lua signal.
            button_lua_signal( self, "OnContinuousPress" );
            return;
        }
        // Unpress if demanded:
        if ( !stayPressed && isToggleButton ) {
            // Engage in unpress movement.
            button_unpress_move( self );
        }
    } else {
        // Engage in press movement.
        button_press_move( self );
    }
}

/**
*   @brief  For when 'Use' triggered.
**/
void button_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {   
    // Set activator.
    self->activator = activator;
    button_trigger( self, activator, useType, useValue );

    // Call upon Lua OnFire.
    button_lua_use( self, other, activator, useType, useValue );
}
/**
*   @brief  For when 'Touch' triggered.
**/
void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    // Only react if it is a client entity which is touch triggering this button.
    if ( !other->client ) {
        return;
    }
    // If it is a dead button, ignore triggering it.
    if ( other->health <= 0 ) {
        return;
    }

    // What is the desired response for the current button state?
    button_response_t buttonResponse = button_response_to_trigger( self );
    // Nothing:
    if ( buttonResponse == BUTTON_RESPONSE_NOTHING ) {
        return;
    }

    //// Are we locked?
    //if ( self.entityclass.button.isLocked ) {
    //    // TODO: Play some locked sound.
    //    return;
    //}

    // Unset touch function until the button is unpressed again.
    self->touch = nullptr;

    if ( buttonResponse == BUTTON_RESPONSE_UNPRESS ) {
        // Untoggle.
        self->activator = other;
        // UseTargets a Toggle.
        SVG_UseTargets( self, other, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
        // Unpress,
        button_unpress_move( self );
    } else if ( buttonResponse == BUTTON_RESPONSE_PRESS ) {
        // Press,
        button_press_move( self );
    }
}
/**
*   @brief  For when 'Death' triggered.
**/
void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    // Of course, only process if we're dealing with a damage trigger button.
    if ( SVG_HasSpawnFlags( self, BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES ) ) {
        // Reset its health.
        self->health = self->max_health;
        // But do not allow it to take damage when 'Pressed'.
        self->takedamage = DAMAGE_NO;
        // Fire the triggering action.
        self->activator = attacker;
        button_trigger( self, attacker, ENTITY_USETARGET_TYPE_TOGGLE, 1 );
    }
}


/**
*   @brief  Spawn function.
**/
void SP_func_button( edict_t *ent ) {
    vec3_t  abs_movedir;
    float   dist;

    // PushMove Entity Basics:
    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    if ( ent->targetNames.movewith ) {
        ent->movetype = MOVETYPE_PUSH;

    } else {
        ent->movetype = MOVETYPE_STOP;
    }
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( ent, ent->model );

    // Default sounds:
    if ( ent->sounds != 1 ) {
        // 'Mechanic':
        ent->pushMoveInfo.sound_start   = gi.soundindex( "buttons/button_mechanic_press.wav" );
        ent->pushMoveInfo.sound_end     = gi.soundindex( "buttons/button_mechanic_unpress.wav" );
    }

    // PushMove defaults:
    if ( !ent->accel ) {
        ent->accel = ent->speed;
    }
    if ( !ent->decel ) {
        ent->decel = ent->speed;
    }
    if ( !ent->speed ) {
        ent->speed = 40;
    }
    if ( !st.lip ) {
        st.lip = 4;
    }
    // Trigger defaults:
    if ( !ent->wait ) {
        ent->wait = 3;
    }

    // Calculate absolute move distance to get from pos1 to pos2.
    ent->pos1 = ent->s.origin;
    abs_movedir[ 0 ] = fabsf( ent->movedir[ 0 ] );
    abs_movedir[ 1 ] = fabsf( ent->movedir[ 1 ] );
    abs_movedir[ 2 ] = fabsf( ent->movedir[ 2 ] );
    dist = abs_movedir[ 0 ] * ent->size[ 0 ] + abs_movedir[ 1 ] * ent->size[ 1 ] + abs_movedir[ 2 ] * ent->size[ 2 ] - st.lip;
    // Translate the determined move distance into the move direction to get pos2, our move end origin.
    ent->pos2 = QM_Vector3MultiplyAdd( ent->movedir, dist, ent->pos1 );

    // Default trigger callback.
    ent->use = button_use;
    // Default animation effects.
    ent->s.effects |= EF_ANIM01;

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
    // Touch based button:
    } else if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_TOUCH_ACTIVATES ) ) {
        // Trigger use callback.
        ent->touch = button_touch;
    // Otherwise check for +usetarget features of this button:
    } else if ( !damageActivates ) {
        // This button is only toggled, never untoggled, by each (+usetarget) interaction.
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            // Remove touch button functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = button_use;
        // This button is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            // Remove touch button functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = button_use;
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            // Remove touch button functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = button_use;
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_DISABLED ) ) {
            ent->useTarget.flags = (entity_usetarget_flags_t)( ent->useTarget.flags | ENTITY_USETARGET_FLAG_DISABLED );
        }
    }

    // Buttons are unpressed by default:
    ent->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;

    // Copy the calculated info into the pushMoveInfo state struct.
    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( ent, BUTTON_SPAWNFLAG_START_PRESSED ) ) {
        ent->pushMoveInfo.state = BUTTON_STATE_PRESSED;
        ent->pushMoveInfo.start_origin = ent->pos2;
        ent->pushMoveInfo.start_angles = ent->s.angles;
        ent->pushMoveInfo.end_origin = ent->pos1;
        ent->pushMoveInfo.end_angles = ent->s.angles;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        ent->pushMoveInfo.start_origin = ent->pos1;
        ent->pushMoveInfo.start_angles = ent->s.angles;
        ent->pushMoveInfo.end_origin = ent->pos2;
        ent->pushMoveInfo.end_angles = ent->s.angles;
    }

    // Link it in.
    gi.linkentity( ent );
}