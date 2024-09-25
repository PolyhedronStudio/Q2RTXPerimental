/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_plat'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_plat.h"



/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER
speed   default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in the extended position until it is trigger, when it will lower and become a normal plat.

"speed" overrides default 20.
"accel" overrides default 5
"decel" overrides default 5
"lip"   overrides default 8 pixel lip

If the "height" key is set, that will determine the amount the plat moves, instead of being implicitly determoveinfoned by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/

/*
=========================================================

  PLATS

  movement options:

  linear
  smooth start, hard stop
  smooth start, smooth stop

  start
  end
  acceleration
  speed
  deceleration
  begin sound
  end sound
  target fired when reaching end
  wait at end

  object characteristics that use move segments
  ---------------------------------------------
  movetype_push, or movetype_stop
  action when touched
  action when blocked
  action when used
    disabled?
  auto trigger spawning


=========================================================
*/

#define PLAT_LOW_TRIGGER    1

#define DOOR_START_OPEN     1
#define DOOR_REVERSE        2
#define DOOR_CRUSHER        4
#define DOOR_NOMONSTER      8
#define DOOR_TOGGLE         32
#define DOOR_X_AXIS         64
#define DOOR_Y_AXIS         128



void plat_go_down( edict_t *ent );

void plat_hit_top( edict_t *ent ) {
    if ( !( ent->flags & FL_TEAMSLAVE ) ) {
        if ( ent->pushMoveInfo.sound_end )
            gi.sound( ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        ent->s.sound = 0;
    }
    ent->pushMoveInfo.state = PUSHMOVE_STATE_TOP;

    #if 0
    ent->think = plat_go_down;
    ent->nextthink = level.time + 3_sec;
    #endif
    // WID: LUA: Call the HitTop function if it exists.
    if ( ent->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( ent->luaProperties.luaName ) + "_OnPlatformHitTop";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, ent, ent->activator );
        }
    }
}

void plat_hit_bottom( edict_t *ent ) {
    if ( !( ent->flags & FL_TEAMSLAVE ) ) {
        if ( ent->pushMoveInfo.sound_end )
            gi.sound( ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        ent->s.sound = 0;
    }
    ent->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;

    // WID: LUA: Call the HitBottom function if it exists.
    if ( ent->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( ent->luaProperties.luaName ) + "_OnPlatformHitBottom";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, ent, ent->activator );
        }
    }
}

void plat_go_down( edict_t *ent ) {
    if ( !( ent->flags & FL_TEAMSLAVE ) ) {
        if ( ent->pushMoveInfo.sound_start )
            gi.sound( ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        ent->s.sound = ent->pushMoveInfo.sound_middle;
    }
    ent->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_DOWN;
    SVG_PushMove_MoveCalculate( ent, ent->pushMoveInfo.end_origin, plat_hit_bottom );
}

void plat_go_up( edict_t *ent ) {
    if ( !( ent->flags & FL_TEAMSLAVE ) ) {
        if ( ent->pushMoveInfo.sound_start )
            gi.sound( ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        ent->s.sound = ent->pushMoveInfo.sound_middle;
    }
    ent->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_UP;
    SVG_PushMove_MoveCalculate( ent, ent->pushMoveInfo.start_origin, plat_hit_top );
}

void plat_blocked( edict_t *self, edict_t *other ) {
    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) ) {
        const bool knockBack = true;
        // give it a chance to go away on it's own terms (like gibs)
        T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, knockBack, 0, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other ) {
            BecomeExplosion1( other );
        }
        return;
    }

    const bool knockBack = false;
    T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, knockBack, 0, MEANS_OF_DEATH_CRUSHED );

    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP )
        plat_go_down( self );
    else if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN )
        plat_go_up( self );
}


void Use_Plat( edict_t *ent, edict_t *other, edict_t *activator, entity_usetarget_type_t useType, const int32_t useValue ) {
    // WID: <Q2RTXP> For func_button support.
    //if ( ( other && !strcmp( other->classname, "func_button" ) ) ) {
    if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP || ent->pushMoveInfo.state == PUSHMOVE_STATE_TOP ) {
        plat_go_down( ent );
    } else if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN || ent->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
        plat_go_up( ent );
    }
    //    return;     // already down
    //}
    // WID: </Q2RTXP>

    #if 0

    if ( ent->think ) {
        return;
    }
    plat_go_down( ent );
    #endif
}


void Touch_Plat_Center( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !other->client )
        return;

    if ( other->health <= 0 )
        return;

    ent = ent->enemy;   // now point at the plat, not the trigger
    if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
        plat_go_up( ent );
    } else if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_TOP ) {
        ent->nextthink = level.time + 1_sec; // the player is still on the plat, so delay going down
    }
}

void plat_spawn_inside_trigger( edict_t *ent ) {
    edict_t *trigger;
    vec3_t  tmin, tmax;

    //
    // middle trigger
    //
    trigger = SVG_AllocateEdict();
    trigger->touch = Touch_Plat_Center;
    trigger->movetype = MOVETYPE_NONE;
    trigger->solid = SOLID_TRIGGER;
    trigger->s.entityType = ET_PUSH_TRIGGER;
    trigger->enemy = ent;
    //G_MoveWith_SetTargetParentEntity( ent->targetname, ent, trigger );

    tmin[ 0 ] = ent->mins[ 0 ] + 25;
    tmin[ 1 ] = ent->mins[ 1 ] + 25;
    tmin[ 2 ] = ent->mins[ 2 ];

    tmax[ 0 ] = ent->maxs[ 0 ] - 25;
    tmax[ 1 ] = ent->maxs[ 1 ] - 25;
    tmax[ 2 ] = ent->maxs[ 2 ] + 8;

    tmin[ 2 ] = tmax[ 2 ] - ( ent->pos1[ 2 ] - ent->pos2[ 2 ] + st.lip );

    if ( ent->spawnflags & PLAT_LOW_TRIGGER )
        tmax[ 2 ] = tmin[ 2 ] + 8;

    if ( tmax[ 0 ] - tmin[ 0 ] <= 0 ) {
        tmin[ 0 ] = ( ent->mins[ 0 ] + ent->maxs[ 0 ] ) * 0.5f;
        tmax[ 0 ] = tmin[ 0 ] + 1;
    }
    if ( tmax[ 1 ] - tmin[ 1 ] <= 0 ) {
        tmin[ 1 ] = ( ent->mins[ 1 ] + ent->maxs[ 1 ] ) * 0.5f;
        tmax[ 1 ] = tmin[ 1 ] + 1;
    }

    VectorCopy( tmin, trigger->mins );
    VectorCopy( tmax, trigger->maxs );

    gi.linkentity( trigger );
}

void SP_func_plat( edict_t *ent ) {
    VectorClear( ent->s.angles );
    ent->solid = SOLID_BSP;
    ent->movetype = MOVETYPE_PUSH;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel( ent, ent->model );

    ent->blocked = plat_blocked;

    if ( !ent->speed )
        ent->speed = 20;
    else
        ent->speed *= 0.1f;

    if ( !ent->accel )
        ent->accel = 5;
    else
        ent->accel *= 0.1f;

    if ( !ent->decel )
        ent->decel = 5;
    else
        ent->decel *= 0.1f;

    if ( !ent->dmg )
        ent->dmg = 2;

    if ( !st.lip )
        st.lip = 8;

    // pos1 is the top position, pos2 is the bottom
    VectorCopy( ent->s.origin, ent->pos1 );
    VectorCopy( ent->s.origin, ent->pos2 );
    if ( st.height )
        ent->pos2[ 2 ] -= st.height;
    else
        ent->pos2[ 2 ] -= ( ent->maxs[ 2 ] - ent->mins[ 2 ] ) - st.lip;

    ent->use = Use_Plat;

    plat_spawn_inside_trigger( ent );     // the "start moving" trigger

    if ( ent->targetname ) {
        ent->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_UP;
    } else {
        VectorCopy( ent->pos2, ent->s.origin );
        gi.linkentity( ent );
        ent->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;
    }

    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    VectorCopy( ent->pos1, ent->pushMoveInfo.start_origin );
    VectorCopy( ent->s.angles, ent->pushMoveInfo.start_angles );
    VectorCopy( ent->pos2, ent->pushMoveInfo.end_origin );
    VectorCopy( ent->s.angles, ent->pushMoveInfo.end_angles );

    ent->pushMoveInfo.sound_start = gi.soundindex( "plats/plat_start_01.wav" );
    ent->pushMoveInfo.sound_middle = gi.soundindex( "plats/plat_mid_01.wav" );
    ent->pushMoveInfo.sound_end = gi.soundindex( "plats/plat_end_01.wav" );
}