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


// Debugging reasons:
static inline constexpr std::string GetUseTargetTypeString( const entity_usetarget_type_t &useTargetType ) {
    if ( useTargetType == ENTITY_USETARGET_TYPE_OFF ) {
        return "ENTITY_USETARGET_TYPE_OFF";
    } else if ( useTargetType == ENTITY_USETARGET_TYPE_ON ) {
        return "ENTITY_USETARGET_TYPE_ON";
    } else if ( useTargetType == ENTITY_USETARGET_TYPE_SET ) {
        return "ENTITY_USETARGET_TYPE_SET";
    } else if ( useTargetType == ENTITY_USETARGET_TYPE_TOGGLE ) {
        return "ENTITY_USETARGET_TYPE_TOGGLE";
    } else {
        return "<UNKNOWN USETYPE #" + std::to_string( useTargetType ) + ">";
    }
}


/**
*   @brief  For readability's sake:
**/
static constexpr int32_t BUTTON_STATE_PRESSED = PUSHMOVE_STATE_TOP;
static constexpr int32_t BUTTON_STATE_MOVING_TO_PRESSED_STATE = PUSHMOVE_STATE_MOVING_UP;
static constexpr int32_t BUTTON_STATE_UNPRESSED = PUSHMOVE_STATE_BOTTOM;
static constexpr int32_t BUTTON_STATE_MOVING_TO_UNPRESSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;
// Debugging reasons:
#if 1
static inline constexpr std::string GetButtonStateString( const int32_t &useTargetType ) {
    if ( useTargetType == BUTTON_STATE_PRESSED ) {
        return "BUTTON_STATE_PRESSED";
    } else if ( useTargetType == BUTTON_STATE_MOVING_TO_PRESSED_STATE ) {
        return "BUTTON_STATE_MOVING_TO_PRESSED_STATE";
    } else if ( useTargetType == BUTTON_STATE_UNPRESSED ) {
        return "BUTTON_STATE_UNPRESSED";
    } else if ( useTargetType == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return "BUTTON_STATE_MOVING_TO_UNPRESSED_STATE";
    } else {
        return "<UNKNOWN BUTTON_STATE #" + std::to_string( useTargetType ) + ">";
    }
}
#endif


/**
*   Trigger respond actions determining what the button should do.
**/
typedef enum button_response_e {
    BUTTON_RESPONSE_NOTHING,
    BUTTON_RESPONSE_ENGAGE_PRESSING,
    BUTTON_RESPONSE_ENGAGE_UNPRESSING,
    BUTTON_RESPONSE_HOLD,
} button_response_t;
// Debugging reasons:
#if 1
static inline constexpr std::string GetButtonResponseString( const button_response_t &buttonResponse ) {
    if ( buttonResponse == BUTTON_RESPONSE_NOTHING ) {
        return "BUTTON_RESPONSE_NOTHING";
    } else if ( buttonResponse == BUTTON_RESPONSE_ENGAGE_PRESSING ) {
        return "BUTTON_RESPONSE_ENGAGE_PRESSING";
    } else if ( buttonResponse == BUTTON_RESPONSE_ENGAGE_UNPRESSING ) {
        return "BUTTON_RESPONSE_ENGAGE_UNPRESSING";
    } else if ( buttonResponse == BUTTON_RESPONSE_HOLD ) {
        return "BUTTON_RESPONSE_HOLD";
    } else {
        return "<UNKNOWN BUTTON_RESPONSE #" + std::to_string( buttonResponse ) + ">";
    }
}
#endif
static button_response_t button_response_to_trigger( edict_t *self ) {
    // Ignore trigger responses if button is moving, or pushed-in and waiting to auto-come-out.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return BUTTON_RESPONSE_NOTHING;
    }
//or pushed - in and waiting to auto - come - out.
//|| ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED && self->wait == -1 && !( self->spawnflags & ENTITY_USETARGET_FLAG_TOGGLE ) ) ) {

    // If for whatever reason we are disabled to be useTargetted anymore, do nothing of course.
    if ( self->useTargetFlags & ENTITY_USETARGET_FLAG_DISABLED ) {
        return BUTTON_RESPONSE_NOTHING;
    }
    // Logic for PRESSABLE buttons:
    if ( self->useTargetFlags & ENTITY_USETARGET_FLAG_PRESS ) {
        // If pressed, and not set to never return(wait == -1) then engage unpressing.
        if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED && self->wait != -1 ) {
            return BUTTON_RESPONSE_ENGAGE_UNPRESSING;
        // Otherwise, engage into pressing the button instead.
        } else {
            return BUTTON_RESPONSE_ENGAGE_PRESSING;
        }
    }
    // Logic for TOGGLEABLE buttons:
    if ( self->useTargetFlags & ENTITY_USETARGET_FLAG_TOGGLE ) {
        // If pressed, then engage unpressing.
        if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
            return BUTTON_RESPONSE_ENGAGE_UNPRESSING;
        // Otherwise, engage into pressing the button instead.
        } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
            return BUTTON_RESPONSE_ENGAGE_PRESSING;
        }
    }
    // Logic for HOLDABLE buttons:
    if ( self->useTargetFlags & ENTITY_USETARGET_FLAG_HOLD ) {

    }

    return BUTTON_RESPONSE_NOTHING;
    #if 0
    // Ignore touches if button is moving, or pushed-in and waiting to auto-come-out.
    if ( m_toggle_state == TS_GOING_UP ||
        m_toggle_state == TS_GOING_DOWN ||
        ( m_toggle_state == TS_AT_TOP && !m_fStayPushed && !FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE ) ) )
        return BUTTON_NOTHING;

    if ( m_toggle_state == TS_AT_TOP ) {
        if ( ( FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE ) ) && !m_fStayPushed ) {
            return BUTTON_RETURN;
        }
    } else
        return BUTTON_ACTIVATE;

    return BUTTON_NOTHING;
    #endif
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
/**
*   @brief
**/
void button_trigger_lua_event( edict_t *self, const std::string &eventName ) {
    // WID: LUA: Call the HitBottom function if it exists.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + eventName;
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, self, self->activator );
        }
    }
}

void button_done( edict_t *self ) {
    self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;
    self->s.effects &= ~EF_ANIM23;
    self->s.effects |= EF_ANIM01;
}

void button_return( edict_t *self ) {
    self->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_DOWN;

    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.start_origin, button_done );

    self->s.frame = 0;

    if ( self->health )
        self->takedamage = DAMAGE_YES;
}

void button_wait( edict_t *self ) {
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    self->s.effects &= ~EF_ANIM01;
    self->s.effects |= EF_ANIM23;

    SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
    button_trigger_lua_event( self, "_OnButtonFire" );

    self->s.frame = 1;
    if ( self->pushMoveInfo.wait >= 0 ) {
        self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
        self->think = button_return;
    }
}

void button_fire( edict_t *self ) {
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP || self->pushMoveInfo.state == PUSHMOVE_STATE_TOP )
        return;

    self->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_UP;
    if ( self->pushMoveInfo.sound_start && !( self->flags & FL_TEAMSLAVE ) )
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, button_wait );
}

void button_use( edict_t *self, edict_t *other, edict_t *activator, entity_usetarget_type_t useType, const int32_t useValue ) {
    self->activator = activator;
    button_fire( self );
}

void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !other->client )
        return;

    if ( other->health <= 0 )
        return;

    self->activator = other;
    button_fire( self );
}

void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    self->activator = attacker;
    self->health = self->max_health;
    self->takedamage = DAMAGE_NO;
    button_fire( self );
}

void SP_func_button( edict_t *ent ) {
    vec3_t  abs_movedir;
    float   dist;

    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    if ( ent->targetNames.movewith ) {
        ent->movetype = MOVETYPE_PUSH;

    } else {
        ent->movetype = MOVETYPE_STOP;
    }
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel( ent, ent->model );

    if ( ent->sounds != 1 )
        ent->pushMoveInfo.sound_start = gi.soundindex( "switches/butn2.wav" );

    if ( !ent->speed )
        ent->speed = 40;
    if ( !ent->accel )
        ent->accel = ent->speed;
    if ( !ent->decel )
        ent->decel = ent->speed;

    if ( !ent->wait )
        ent->wait = 3;
    if ( !st.lip )
        st.lip = 4;

    VectorCopy( ent->s.origin, ent->pos1 );
    abs_movedir[ 0 ] = fabsf( ent->movedir[ 0 ] );
    abs_movedir[ 1 ] = fabsf( ent->movedir[ 1 ] );
    abs_movedir[ 2 ] = fabsf( ent->movedir[ 2 ] );
    dist = abs_movedir[ 0 ] * ent->size[ 0 ] + abs_movedir[ 1 ] * ent->size[ 1 ] + abs_movedir[ 2 ] * ent->size[ 2 ] - st.lip;
    VectorMA( ent->pos1, dist, ent->movedir, ent->pos2 );

    ent->use = button_use;
    ent->s.effects |= EF_ANIM01;

    if ( ent->health ) {
        ent->max_health = ent->health;
        ent->die = button_killed;
        ent->takedamage = DAMAGE_YES;
    } else if ( !ent->targetname )
        ent->touch = button_touch;

    ent->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;

    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    VectorCopy( ent->pos1, ent->pushMoveInfo.start_origin );
    VectorCopy( ent->s.angles, ent->pushMoveInfo.start_angles );
    VectorCopy( ent->pos2, ent->pushMoveInfo.end_origin );
    VectorCopy( ent->s.angles, ent->pushMoveInfo.end_angles );

    gi.linkentity( ent );
}

// OLD... but bugged too lazy to figure it out just gonna rewrite it yay haha for teh lulz.
#if 0
/**
*   @brief
**/
void button_trigger_lua_event( edict_t *self, const std::string &eventName ) {
    // WID: LUA: Call the HitBottom function if it exists.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + eventName;
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, self, self->activator );
        }
    }
}
void button_reached_pressed_state( edict_t *self ) {
    self->pushMoveInfo.state = BUTTON_STATE_PRESSED;
    self->s.effects &= ~EF_ANIM23;
    self->s.effects |= EF_ANIM01;
}
void button_reached_unpressed_state( edict_t *self ) {
    self->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;
    self->s.effects &= ~EF_ANIM23;
    self->s.effects |= EF_ANIM01;
}

void button_return_to_unpressed_state( edict_t *self ) {
    self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_UNPRESSED_STATE;

    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.start_origin, button_reached_unpressed_state );

    self->s.frame = 0;

    if ( self->health ) {
        self->takedamage = DAMAGE_YES;
    }
}

void button_return_to_pressed_state( edict_t *self ) {
    self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_PRESSED_STATE;

    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, button_reached_pressed_state );

    self->s.frame = 1;
}

void button_trigger_and_wait( edict_t *self ) {
    #if 0
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE ) {
        self->pushMoveInfo.state = BUTTON_STATE_PRESSED;
    } else if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        self->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;
    }
    #endif

    self->s.effects &= ~EF_ANIM01;
    self->s.effects |= EF_ANIM23;

    // Logic for PRESSABLE buttons: Only do a UseTarget one time, if we reached pressed state.
    if ( self->useTargetFlags & ENTITY_USETARGET_FLAG_PRESS ) {
        if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
            // Dispatch UseTargets.
            SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 1 );
            button_trigger_lua_event( self, "_OnButtonFire" );

            self->s.frame = 1;
            // If not set to remain pressed, engage moving back to its unpressed state.
            //if ( self->pushMoveInfo.wait >= 0 ) {
            //    self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
            //    self->think = button_return_to_unpressed_state;
            //}
        } else {
            SVG_UseTargets( self, self->activator, ENTITY_USETARGET_TYPE_TOGGLE, 0 );
        }
    }
    // Logic for TOGGABLE buttons: Keep use targetting until we're out of pressed state.
    else if ( self->useTargetFlags & ENTITY_USETARGET_FLAG_TOGGLE ) {
        if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
            // Dispatch UseTargets.
            SVG_UseTargets( self, self->activator );
            button_trigger_lua_event( self, "_OnButtonFire" );
            // If not set to remain pressed, engage moving back to its unpressed state.
            if ( self->pushMoveInfo.wait >= 0 ) {
                self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
                self->think = button_return_to_unpressed_state;
            }
        } else if ( self->pushMoveInfo.state == BUTTON_STATE_UNPRESSED ) {
            // Dispatch UseTargets.
            SVG_UseTargets( self, self->activator );
            button_trigger_lua_event( self, "_OnButtonFire" );
            // If not set to remain pressed, engage moving back to its unpressed state.
            if ( self->pushMoveInfo.wait >= 0 ) {
                self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
                self->think = button_return_to_pressed_state;
            }
            // If not set to remain pressed, engage moving back to its unpressed state.
            //if ( self->pushMoveInfo.wait >= 0 ) {
                //self->nextthink = level.time + FRAME_TIME_MS; //sg_time_t::from_sec( self->pushMoveInfo.wait );
                //self->think = button_return_to_unpressed_state;
            //}
        }
    // Logic for HOLDABLE buttons: Keep use targetting until we're out of pressed state.
    } else if ( self->useTargetFlags & ENTITY_USETARGET_FLAG_HOLD ) {
        //if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE ) {
        //    self->pushMoveInfo.state = BUTTON_STATE_PRESSED;
        //}
        if ( self->pushMoveInfo.state == BUTTON_STATE_PRESSED ) {
            // Dispatch UseTargets.
            SVG_UseTargets( self, self->activator );
            button_trigger_lua_event( self, "_OnButtonFire" );
        } else {

        }
    }

    self->s.frame = 1;
    //if ( self->pushMoveInfo.wait >= 0 ) {
    //    self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
    //    self->think = button_return_to_unpressed_state;
    //}
}

void button_fire( edict_t *self, edict_t *activator, entity_usetarget_type_t useType, const int32_t useValue ) {
    gi.dprintf( "%s(#0): self->s.number(%d), %s, (%d), %s\n", __func__, self->s.number, GetUseTargetTypeString( useType ).c_str(), useValue, GetButtonStateString( self->pushMoveInfo.state ).c_str() );

    // It won't be any use firing if we're still switching state of course.
    if ( self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_PRESSED_STATE || self->pushMoveInfo.state == BUTTON_STATE_MOVING_TO_UNPRESSED_STATE ) {
        return;
    }

    // Apply as activator.
    self->activator = activator;

    // Determine what action to engage into.
    button_response_t buttonResponse = button_response_to_trigger( self );

    gi.dprintf( "%s(#0): self->s.number(%d), %s, (%d), %s, %s\n", __func__, self->s.number, GetUseTargetTypeString( useType ).c_str(), useValue, GetButtonStateString( self->pushMoveInfo.state ).c_str(), GetButtonResponseString(buttonResponse).c_str() );

    //if ( buttonResponse == BUTTON_RESPONSE_ENGAGE_PRESSING ) {
    if ( useType == ENTITY_USETARGET_TYPE_ON || useValue == 1 ) {
        // Play specified audio.
        if ( self->pushMoveInfo.sound_start && !( self->flags & FL_TEAMSLAVE ) ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        }
        //// Switch state.
        //self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_PRESSED_STATE;
        //// Engage movement.
        //SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, button_trigger_and_wait );
        button_return_to_pressed_state( self );
        gi.dprintf( "%s: BUTTON_RESPONSE_ENGAGE_PRESSING\n", __func__ );
    }
    //if ( buttonResponse == BUTTON_RESPONSE_ENGAGE_UNPRESSING ) {
    if ( useType == ENTITY_USETARGET_TYPE_OFF || useValue == 0 ) {
        // Play specified audio.
        // TODO: We need some audio for this shit.
        #if 0
        if ( self->pushMoveInfo.sound_end && !( self->flags & FL_TEAMSLAVE ) ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        }
        #endif
        // Switch state.
        //self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_UNPRESSED_STATE;
        // Engage movement.
        //SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.start_origin, button_trigger_and_wait );
        button_return_to_unpressed_state( self );
        gi.dprintf( "%s: BUTTON_RESPONSE_ENGAGE_UNPRESSING\n", __func__ );
    }

    //self->pushMoveInfo.state = BUTTON_STATE_MOVING_TO_UNPRESSED_STATE;
    //if ( self->pushMoveInfo.sound_start && !( self->flags & FL_TEAMSLAVE ) ) {
    //    gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
    //}

    //SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, button_trigger_and_wait );
}

void button_use( edict_t *self, edict_t *other, edict_t *activator, entity_usetarget_type_t useType, const int32_t useValue ) {
    button_fire( self, activator, useType, useValue );

    gi.dprintf( "%s: self->s.number(%d), %s\n", __func__, self->s.number, GetUseTargetTypeString( useType ).c_str() );
}

void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !other->client ) {
        return;
    }
    if ( other->health <= 0 ) {
        return;
    }
    button_fire( self, other, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 0 );

    gi.dprintf( "%s: self->s.number(%d), %s\n", __func__, self->s.number, "USETARGET_TYPE_TOGGLE" );
}

/**
*   @brief
**/
void SP_func_button( edict_t *ent ) {
    vec3_t  abs_movedir;
    float   dist;

    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_STOP;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel( ent, ent->model );

    if ( ent->sounds != 1 )
        ent->pushMoveInfo.sound_start = gi.soundindex( "switches/butn2.wav" );

    // Default speed.
    if ( !ent->speed ) {
        ent->speed = 40;
    }
    // Use speed for deceleration and acceleration if they weren't ever set.
    if ( !ent->accel ) {
        ent->accel = ent->speed;
    }
    if ( !ent->decel ) {
        ent->decel = ent->speed;
    }
    // Default wait. (Otherwise it'd return instantly.)
    if ( !ent->wait ) {
        ent->wait = 3;
    }
    //ent->wait = -1;
    if ( !st.lip ) {
        st.lip = 4;
    }

    // Determine start and end position
    VectorCopy( ent->s.origin, ent->pos1 );
    abs_movedir[ 0 ] = fabsf( ent->movedir[ 0 ] );
    abs_movedir[ 1 ] = fabsf( ent->movedir[ 1 ] );
    abs_movedir[ 2 ] = fabsf( ent->movedir[ 2 ] );
    dist = abs_movedir[ 0 ] * ent->size[ 0 ] + abs_movedir[ 1 ] * ent->size[ 1 ] + abs_movedir[ 2 ] * ent->size[ 2 ] - st.lip;
    VectorMA( ent->pos1, dist, ent->movedir, ent->pos2 );

    // Trigger use callback.
    ent->touch = button_touch;

    // This button is only toggled, never untoggled, by each (+usetarget) interaction.
    if ( ent->spawnflags & SPAWNFLAG_USETARGET_PRESSABLE ) {
        ent->useTargetFlags = ENTITY_USETARGET_FLAG_PRESS;
        // Remove touch button functionality, instead, reside to usetarget functionality.
        ent->touch = nullptr;
        ent->use = button_use;
    // This button is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
    } else if ( ent->spawnflags & SPAWNFLAG_USETARGET_TOGGLEABLE ) {
        ent->useTargetFlags = ENTITY_USETARGET_FLAG_TOGGLE;
        // Remove touch button functionality, instead, reside to usetarget functionality.
        ent->touch = nullptr;
        ent->use = button_use;
    // This button is pressed for as long as it is focused and +usetargetted.
    } else if ( ent->spawnflags & SPAWNFLAG_USETARGET_HOLDABLE ) {
        ent->useTargetFlags = ENTITY_USETARGET_FLAG_HOLD;
        // Remove touch button functionality, instead, reside to usetarget functionality.
        ent->touch = nullptr;
        ent->use = button_use;
    }
    // Is usetargetting disabled by default?
    if ( ent->spawnflags & SPAWNFLAG_USETARGET_DISABLED ) {
        ent->useTargetFlags = (entity_usetarget_flags_t)( ent->useTargetFlags | ENTITY_USETARGET_FLAG_DISABLED );
    }

    ent->useTargetState = ENTITY_USETARGET_STATE_DEFAULT;

    // If this was (nullptr) we are dealing with a usetargets button.
    #if 0
    if ( !ent->touch ) {
        ent->usetarget_toggle = button_usetarget_toggle;
        ent->usetarget_untoggle = button_usetarget_untoggle;
        ent->usetarget_hold = button_usetarget_hold;
    }
    #endif

    // Animation type in case texture is animated.
    ent->s.effects |= EF_ANIM01;
    #if 0
    if ( ent->health ) {
        ent->max_health = ent->health;
        ent->die = button_killed;
        ent->takedamage = DAMAGE_YES;
        // WID: LUA: TODO: This breaks old default behavior.
    } else if ( !ent->targetname )
    }
    #endif    

    // Default starting state.
    ent->pushMoveInfo.state = BUTTON_STATE_UNPRESSED;

    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    VectorCopy( ent->pos1, ent->pushMoveInfo.start_origin );
    VectorCopy( ent->s.angles, ent->pushMoveInfo.start_angles );
    VectorCopy( ent->pos2, ent->pushMoveInfo.end_origin );
    VectorCopy( ent->s.angles, ent->pushMoveInfo.end_angles );

    gi.linkentity( ent );
}

#endif // #if 0/1