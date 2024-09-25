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



/**
*   Trigger respond actions determining what the button should do.
**/
typedef enum button_response_action_e {
    BUTTON_RESPONSE_ACTION_NOTHING,
    BUTTON_RESPONSE_ACTION_ACTIVATE,
    BUTTON_RESPONSE_ACTION_RETURN
} button_response_action_t;

static button_response_action_t Button_TriggerResponseAction( edict_t *self ) {
    // Ignore trigger responses if button is moving, or pushed-in and waiting to auto-come-out.
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP
        || self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN 
        || ( self->pushMoveInfo.state == PUSHMOVE_STATE_TOP && self->wait == -1 && !( self->spawnflags & ENTITY_USETARGET_FLAG_TOGGLE ) ) ) {
        return BUTTON_RESPONSE_ACTION_NOTHING;
    }

    // 
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_TOP ) {
        if ( ( self->spawnflags & ENTITY_USETARGET_FLAG_TOGGLE ) && !( self->wait == -1 ) ) {
            return BUTTON_RESPONSE_ACTION_RETURN;
        }
    } else {
        return BUTTON_RESPONSE_ACTION_ACTIVATE;
    }

    return BUTTON_RESPONSE_ACTION_NOTHING;
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
void Button_BackHome( edict_t *self ) {
    // We're at "bottom" state, idling, awaiting another use/usetarget.
    self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;

    if ( self->useTargetFlags == ENTITY_USETARGET_FLAG_TOGGLE ) {
        SVG_UseTargets( self, self->activator );
    }

    // Change brush texture animation state.
    self->s.effects &= ~EF_ANIM23;
    self->s.effects |= EF_ANIM01;
}
/**
*   @brief
**/
void Button_Return( edict_t *self ) {
    self->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_DOWN;

    SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.start_origin, Button_BackHome );

    self->s.frame = 0;

    if ( self->health )
        self->takedamage = DAMAGE_YES;
}
/**
*   @brief  The button has arrived at its 'top'(ending) destination.
*           Call UseTargets and optionally set a time for it to return to 'bottom'(start/default) state.
**/
void Button_TriggerAndWait( edict_t *self ) {
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    self->s.effects &= ~EF_ANIM01;
    self->s.effects |= EF_ANIM23;

    // Call upon usetargets.
    SVG_UseTargets( self, self->activator );

    // WID: LUA: Call the HitBottom function if it exists.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_OnButtonFire";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, self, self->activator );
        }
    }

    // Change texture animation frame.
    self->s.frame = 1;

    // 
    if ( self->wait == -1 || SVG_UseTarget_HasToggleFeature( self ) ) {

    } else {
        if ( !SVG_UseTarget_HasToggleHoldFeatures( self ) ) {
            if ( self->pushMoveInfo.wait >= 0 ) {
                self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
                self->think = Button_Return;
            }
        }
    }
}
/**
*   @brief
**/
const bool button_fire( edict_t *self ) {
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP || self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN ) {
        return false;
    }

    // Engage transitioning.
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
        self->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_UP;

        // Button sound.
        if ( self->pushMoveInfo.sound_start && !( self->flags & FL_TEAMSLAVE ) ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        }

        SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, Button_TriggerAndWait );
    } else {
        if ( self->wait != -1 && self->spawnflags & ENTITY_USETARGET_FLAG_TOGGLE ) {

            // Button sound.
            if ( self->pushMoveInfo.sound_start && !( self->flags & FL_TEAMSLAVE ) ) {
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
            }

            Button_Return( self );
        }
    }

    return true;
}
/**
*   @brief
**/
void button_use( edict_t *self, edict_t *other, edict_t *activator ) {
    self->activator = activator;
    button_fire( self );
}
#if 1
/**
*   @brief
**/
void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !other->client )
        return;

    if ( other->health <= 0 )
        return;

    self->activator = other;
    button_fire( self );
}
#endif

/**
*   @brief
**/
const bool button_usetarget_toggle( edict_t *self, edict_t *activator ) {
    if ( !activator ) {
        return false;
    }
    if ( !activator->client || activator->health <= 0 ) {
        return false;
    }

    button_use( self, nullptr, activator );
    gi.dprintf( "%s: Dispatched TOGGLE call\n", __func__ );
    return true;
}
/**
*   @brief
**/
const bool button_usetarget_untoggle( edict_t *self, edict_t *activator ) {
    if ( !activator ) {
        return false;
    }
    if ( !activator->client /*|| activator->health <= 0*/ ) {
        return false;
    }

    self->activator = activator;
    button_fire( self );
    
    return false;
    gi.dprintf( "%s: Dispatched UNTOGGLE call\n", __func__ );
    return true;
}
/**
*   @brief
**/
const bool button_usetarget_hold( edict_t *self, edict_t *activator ) {
    gi.dprintf( "%s: Dispatched HOLD call\n", __func__ );
    return false;
}
// </Q2RTXP>

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
    if ( !ent->touch ) {
        ent->usetarget_toggle = button_usetarget_toggle;
        ent->usetarget_untoggle = button_usetarget_untoggle;
        ent->usetarget_hold = button_usetarget_hold;
    }

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