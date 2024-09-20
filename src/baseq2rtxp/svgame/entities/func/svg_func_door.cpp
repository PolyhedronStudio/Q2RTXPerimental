/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"



/*
======================================================================

DOORS

  spawn a trigger surrounding the entire team unless it is
  already targeted by another

======================================================================
*/

/*QUAKED func_door (0 .5 .8) ? START_OPEN x CRUSHER NOMONSTER ANIMATED TOGGLE ANIMATED_FAST
TOGGLE      wait in both the start and end states for a trigger event.
START_OPEN  the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER   monsters will not trigger this door

"message"   is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"     determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"    if set, door must be shot open
"speed"     movement speed (100 default)
"wait"      wait before returning (3 default, -1 = never return)
"lip"       lip remaining at end of move (8 default)
"dmg"       damage to inflict when blocked (2 default)
"sounds"
1)  silent
2)  light
3)  medium
4)  heavy
*/

/**
*	@brief
**/
void door_use_areaportals( edict_t *self, const bool open ) {
    edict_t *t = NULL;

    if ( !self->targetNames.target )
        return;

    while ( ( t = SVG_Find( t, FOFS( targetname ), self->targetNames.target ) ) ) {
        //if (Q_stricmp(t->classname, "func_areaportal") == 0) {
        if ( t->s.entityType == ET_AREA_PORTAL ) {
            gi.SetAreaPortalState( t->style, open );

            //self->s.event = ( open ? EV_AREAPORTAL_OPEN : EV_AREAPORTAL_CLOSE );
            //self->s.event[ 1 ] = t->style;
            //self->s.event[ 2 ] = open;
        }
    }
}

/**
*	@brief
**/
void door_go_down( edict_t *self );

/**
*	@brief
**/
void door_hit_top( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pusherMoveInfo.sound_end )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        self->s.sound = 0;
    }
    self->pusherMoveInfo.state = STATE_TOP;

    // WID: LUA: Call the OnDoorOpened function if it exists.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_OnDoorOpened";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, self, self->activator );
        }
    }

    if ( self->spawnflags & FUNC_DOOR_TOGGLE )
        return;

    if ( self->pusherMoveInfo.wait >= 0 ) {
        self->think = door_go_down;
        self->nextthink = level.time + sg_time_t::from_sec( self->pusherMoveInfo.wait );
    }

}

/**
*	@brief
**/
void door_hit_bottom( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pusherMoveInfo.sound_end )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        self->s.sound = 0;
    }
    self->pusherMoveInfo.state = STATE_BOTTOM;
    door_use_areaportals( self, false );

    // WID: LUA: Call the OnDoorClosed function if it exists.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_OnDoorClosed";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, self, self->activator );
        }
    }
}

/**
*	@brief
**/
void door_go_down( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pusherMoveInfo.sound_start )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        self->s.sound = self->pusherMoveInfo.sound_middle;
    }
    if ( self->max_health ) {
        self->takedamage = DAMAGE_YES;
        self->health = self->max_health;
    }

    self->pusherMoveInfo.state = STATE_MOVING_DOWN;
    if ( strcmp( self->classname, "func_door" ) == 0 )
        Move_Calc( self, self->pusherMoveInfo.start_origin, door_hit_bottom );
    else if ( strcmp( self->classname, "func_door_rotating" ) == 0 )
        AngleMove_Calc( self, door_hit_bottom );
}

/**
*	@brief
**/
void door_go_up( edict_t *self, edict_t *activator ) {
    if ( self->pusherMoveInfo.state == STATE_MOVING_UP )
        return;     // already going up

    if ( self->pusherMoveInfo.state == STATE_TOP ) {
        // reset top wait time
        if ( self->pusherMoveInfo.wait >= 0 )
            self->nextthink = level.time + sg_time_t::from_sec( self->pusherMoveInfo.wait );
        return;
    }

    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pusherMoveInfo.sound_start )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        self->s.sound = self->pusherMoveInfo.sound_middle;
    }
    self->pusherMoveInfo.state = STATE_MOVING_UP;
    if ( strcmp( self->classname, "func_door" ) == 0 )
        Move_Calc( self, self->pusherMoveInfo.end_origin, door_hit_top );
    else if ( strcmp( self->classname, "func_door_rotating" ) == 0 )
        AngleMove_Calc( self, door_hit_top );

    SVG_UseTargets( self, activator );
    door_use_areaportals( self, true );
}

/**
*	@brief
**/
void door_use( edict_t *self, edict_t *other, edict_t *activator ) {
    edict_t *ent;

    if ( self->flags & FL_TEAMSLAVE )
        return;

    if ( self->spawnflags & FUNC_DOOR_TOGGLE ) {
        if ( self->pusherMoveInfo.state == STATE_MOVING_UP || self->pusherMoveInfo.state == STATE_TOP ) {
            // trigger all paired doors
            for ( ent = self; ent; ent = ent->teamchain ) {
                ent->message = NULL;
                ent->touch = NULL;
                door_go_down( ent );
            }
            return;
        }
    }

    // trigger all paired doors
    for ( ent = self; ent; ent = ent->teamchain ) {
        ent->message = NULL;
        ent->touch = NULL;
        door_go_up( ent, activator );
    }
}

/**
*	@brief
**/
void door_blocked( edict_t *self, edict_t *other ) {
    edict_t *ent;

    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) ) {
        // give it a chance to go away on it's own terms (like gibs)
        T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other )
            BecomeExplosion1( other );
        return;
    }

    T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED );

    if ( self->spawnflags & FUNC_DOOR_CRUSHER )
        return;


    // if a door has a negative wait, it would never come back if blocked,
    // so let it just squash the object to death real fast
    if ( self->pusherMoveInfo.wait >= 0 ) {
        if ( self->pusherMoveInfo.state == STATE_MOVING_DOWN ) {
            for ( ent = self->teammaster; ent; ent = ent->teamchain )
                door_go_up( ent, ent->activator );
        } else {
            for ( ent = self->teammaster; ent; ent = ent->teamchain )
                door_go_down( ent );
        }
    }
}

/**
*	@brief
**/
void door_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    edict_t *ent;

    for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
        ent->health = ent->max_health;
        ent->takedamage = DAMAGE_NO;
    }
    door_use( self->teammaster, attacker, attacker );
}

/**
*	@brief
**/
void door_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !other->client )
        return;

    if ( level.time < self->touch_debounce_time )
        return;
    self->touch_debounce_time = level.time + 5_sec;

    gi.centerprintf( other, "%s", self->message );
    gi.sound( other, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
}

/**
*	@brief
**/
void door_postspawn( edict_t *self ) {
    //if ( self->spawnflags & DOOR_START_OPEN ) {
    //    //SVG_UseTargets( self, self );
    //    door_use_areaportals( self, true );
    //    //self->pusherMoveInfo.state = STATE_TOP;
    //}
}

/**
*	@brief
**/
void SP_func_door( edict_t *ent ) {
    vec3_t  abs_movedir;

    if ( ent->sounds != 1 ) {
        ent->pusherMoveInfo.sound_start = gi.soundindex( "doors/door_start_01.wav" );
        ent->pusherMoveInfo.sound_middle = gi.soundindex( "doors/door_mid_01.wav" );
        ent->pusherMoveInfo.sound_end = gi.soundindex( "doors/door_end_01.wav" );
    }

    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel( ent, ent->model );

    ent->postspawn = door_postspawn;
    ent->blocked = door_blocked;
    ent->use = door_use;

    if ( !ent->speed )
        ent->speed = 100;
    if ( deathmatch->value )
        ent->speed *= 2;

    if ( !ent->accel )
        ent->accel = ent->speed;
    if ( !ent->decel )
        ent->decel = ent->speed;

    if ( !ent->wait )
        ent->wait = 3;
    if ( !st.lip )
        st.lip = 8;
    if ( !ent->dmg )
        ent->dmg = 2;

    // calculate second position
    VectorCopy( ent->s.origin, ent->pos1 );
    abs_movedir[ 0 ] = fabsf( ent->movedir[ 0 ] );
    abs_movedir[ 1 ] = fabsf( ent->movedir[ 1 ] );
    abs_movedir[ 2 ] = fabsf( ent->movedir[ 2 ] );
    ent->pusherMoveInfo.distance = abs_movedir[ 0 ] * ent->size[ 0 ] + abs_movedir[ 1 ] * ent->size[ 1 ] + abs_movedir[ 2 ] * ent->size[ 2 ] - st.lip;
    VectorMA( ent->pos1, ent->pusherMoveInfo.distance, ent->movedir, ent->pos2 );

    // if it starts open, switch the positions
    if ( ent->spawnflags & FUNC_DOOR_START_OPEN ) {
        VectorCopy( ent->pos2, ent->s.origin );
        VectorCopy( ent->pos1, ent->pos2 );
        VectorCopy( ent->s.origin, ent->pos1 );
    }

    ent->pusherMoveInfo.state = STATE_BOTTOM;

    if ( ent->health ) {
        ent->takedamage = DAMAGE_YES;
        ent->die = door_killed;
        ent->max_health = ent->health;
    } else if ( ent->targetname && ent->message ) {
        gi.soundindex( "hud/chat01.wav" );
        ent->touch = door_touch;
    }

    ent->pusherMoveInfo.speed = ent->speed;
    ent->pusherMoveInfo.accel = ent->accel;
    ent->pusherMoveInfo.decel = ent->decel;
    ent->pusherMoveInfo.wait = ent->wait;
    VectorCopy( ent->pos1, ent->pusherMoveInfo.start_origin );
    VectorCopy( ent->s.angles, ent->pusherMoveInfo.start_angles );
    VectorCopy( ent->pos2, ent->pusherMoveInfo.end_origin );
    VectorCopy( ent->s.angles, ent->pusherMoveInfo.end_angles );

    if ( ent->spawnflags & 16 )
        ent->s.effects |= EF_ANIM_ALL;
    if ( ent->spawnflags & 64 )
        ent->s.effects |= EF_ANIM_ALLFAST;

    // to simplify logic elsewhere, make non-teamed doors into a team of one
    if ( !ent->targetNames.team )
        ent->teammaster = ent;

    gi.linkentity( ent );

    ent->nextthink = level.time + FRAME_TIME_S;

    if ( ent->health || ent->targetname )
        ent->think = Think_CalcMoveSpeed;
    else
        ent->think = Think_SpawnDoorTrigger;
}