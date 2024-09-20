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



/*QUAKED func_button (0 .5 .8) ?
When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

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

void button_done( edict_t *self ) {

    self->pusherMoveInfo.state = STATE_BOTTOM;
    self->s.effects &= ~EF_ANIM23;
    self->s.effects |= EF_ANIM01;
}

void button_return( edict_t *self ) {
    self->pusherMoveInfo.state = STATE_MOVING_DOWN;

    Move_Calc( self, self->pusherMoveInfo.start_origin, button_done );

    self->s.frame = 0;

    if ( self->health )
        self->takedamage = DAMAGE_YES;
}

void button_wait( edict_t *self ) {
    self->pusherMoveInfo.state = STATE_TOP;
    self->s.effects &= ~EF_ANIM01;
    self->s.effects |= EF_ANIM23;

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

    self->s.frame = 1;
    //if (self->pusherMoveInfo.wait >= 0) {
    //  self->nextthink = level.time + sg_time_t::from_sec( self->pusherMoveInfo.wait );
    //  self->think = button_return;
    //}
}

const bool button_fire( edict_t *self ) {
    if ( self->pusherMoveInfo.state == STATE_MOVING_UP || self->pusherMoveInfo.state == STATE_MOVING_DOWN ) {
        return false;
    }

    // Button sound.
    if ( self->pusherMoveInfo.sound_start && !( self->flags & FL_TEAMSLAVE ) ) {
        gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
    }
    // Engage transitioning.
    if ( self->pusherMoveInfo.state == STATE_BOTTOM ) {
        self->pusherMoveInfo.state = STATE_MOVING_UP;
        Move_Calc( self, self->pusherMoveInfo.end_origin, button_wait );
    } else {
        button_return( self );
    }

    return true;
}

void button_use( edict_t *self, edict_t *other, edict_t *activator ) {
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

// <Q2RTXP>
const bool button_usetarget_toggle( edict_t *self, edict_t *activator ) {
    if ( !activator ) {
        return false;
    }
    if ( !activator->client || activator->health <= 0 ) {
        return false;
    }

    self->activator = activator;
    button_fire( self );
    gi.dprintf( "%s: Dispatched TOGGLE call\n", __func__ );
    return true;
}
const bool button_usetarget_untoggle( edict_t *self, edict_t *activator ) {
    if ( !activator ) {
        return false;
    }
    if ( !activator->client /*|| activator->health <= 0*/ ) {
        return false;
    }

    self->activator = activator;
    button_fire( self );
    gi.dprintf( "%s: Dispatched UNTOGGLE call\n", __func__ );
    return true;
}
const bool button_usetarget_hold( edict_t *self, edict_t *activator ) {
    gi.dprintf( "%s: Dispatched HOLD call\n", __func__ );
    return false;
}

// </Q2RTXP>

void SP_func_button( edict_t *ent ) {
    vec3_t  abs_movedir;
    float   dist;

    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_STOP;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel( ent, ent->model );

    if ( ent->sounds != 1 )
        ent->pusherMoveInfo.sound_start = gi.soundindex( "switches/butn2.wav" );

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
    //if ( !ent->wait ) {
    //    ent->wait = 3;
    //}
    ent->wait = -1;
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
    ent->use = button_use;

    // Makes the button togleable.
    if ( ent->spawnflags & SPAWNFLAG_USETARGET_TOGGLEABLE ) {
        ent->useTargetFlags = ENTITY_USETARGET_FLAG_TOGGLE;
        // Will keep the button in hold state.
    } else if ( ent->spawnflags & SPAWNFLAG_USETARGET_HOLDABLE ) {
        ent->useTargetFlags = ENTITY_USETARGET_FLAG_HOLD;
    }

    ent->usetarget_toggle = button_usetarget_toggle;
    ent->usetarget_untoggle = button_usetarget_untoggle;
    ent->usetarget_hold = button_usetarget_hold;

    // Animation type in case texture is animated.
    ent->s.effects |= EF_ANIM01;
    #if 0
    if ( ent->health ) {
        ent->max_health = ent->health;
        ent->die = button_killed;
        ent->takedamage = DAMAGE_YES;
        // WID: LUA: TODO: This breaks old default behavior.
    } else if ( !ent->targetname )
        ent->touch = button_touch;
}
#endif    

// Default starting state.
ent->pusherMoveInfo.state = STATE_BOTTOM;

ent->pusherMoveInfo.speed = ent->speed;
ent->pusherMoveInfo.accel = ent->accel;
ent->pusherMoveInfo.decel = ent->decel;
ent->pusherMoveInfo.wait = ent->wait;
VectorCopy( ent->pos1, ent->pusherMoveInfo.start_origin );
VectorCopy( ent->s.angles, ent->pusherMoveInfo.start_angles );
VectorCopy( ent->pos2, ent->pusherMoveInfo.end_origin );
VectorCopy( ent->s.angles, ent->pusherMoveInfo.end_angles );

gi.linkentity( ent );
}