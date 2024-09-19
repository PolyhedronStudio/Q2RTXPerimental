/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

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

#define STATE_TOP           0
#define STATE_BOTTOM        1
#define STATE_UP            2
#define STATE_DOWN          3

#define DOOR_START_OPEN     1
#define DOOR_REVERSE        2
#define DOOR_CRUSHER        4
#define DOOR_NOMONSTER      8
#define DOOR_TOGGLE         32
#define DOOR_X_AXIS         64
#define DOOR_Y_AXIS         128



/**
*
*
*
*   Support routines for movement (changes in origin using velocity)
*
* 
*
**/
/**
*   @brief  
**/
void Move_Done( edict_t *ent ) {
    // WID: MoveWith: Clear last velocity also.
    VectorClear( ent->velocity );

    if ( ent->pusherMoveInfo.endfunc ) {
        ent->pusherMoveInfo.endfunc( ent );
    }

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
void Move_Final( edict_t *ent ) {
    if ( ent->pusherMoveInfo.remaining_distance == 0 ) {
        Move_Done( ent );
        return;
    }

    VectorScale( ent->pusherMoveInfo.dir, ent->pusherMoveInfo.remaining_distance / FRAMETIME, ent->velocity );

    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //}

    ent->think = Move_Done;
    ent->nextthink = level.time + FRAME_TIME_S;
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
void Move_Begin( edict_t *ent ) {
    if ( ( ent->pusherMoveInfo.speed * gi.frame_time_s ) >= ent->pusherMoveInfo.remaining_distance ) {
        Move_Final( ent );
        return;
    }
    VectorScale( ent->pusherMoveInfo.dir, ent->pusherMoveInfo.speed, ent->velocity );

    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //    ent->pusherMoveInfo.remaining_distance -= ent->pusherMoveInfo.speed * gi.frame_time_s;
    //    ent->nextthink = level.time + FRAME_TIME_S;
    //    ent->think = Move_Begin;
    //} else {
    const float frames = floor( ( ent->pusherMoveInfo.remaining_distance / ent->pusherMoveInfo.speed ) / gi.frame_time_s );
    ent->pusherMoveInfo.remaining_distance -= frames * ent->pusherMoveInfo.speed * gi.frame_time_s;
    ent->nextthink = level.time + ( FRAME_TIME_S * frames );
    ent->think = Move_Final;
    //}

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
void Think_AccelMove(edict_t *ent);
/**
*   @brief
**/
void Move_Calc( edict_t *ent, const Vector3 &dest, void( *func )( edict_t * ) ) {
    VectorClear( ent->velocity );
    VectorSubtract( dest, ent->s.origin, ent->pusherMoveInfo.dir );
    ent->pusherMoveInfo.remaining_distance = VectorNormalize( &ent->pusherMoveInfo.dir.x );
    ent->pusherMoveInfo.endfunc = func;

    if ( ent->pusherMoveInfo.speed == ent->pusherMoveInfo.accel && ent->pusherMoveInfo.speed == ent->pusherMoveInfo.decel ) {
        // If a Team Master, engage move immediately:
        if ( level.current_entity == ( ( ent->flags & FL_TEAMSLAVE ) ? ent->teammaster : ent ) ) {
            Move_Begin( ent );
            //} else if ( ent->targetEntities.movewith ) {
            //    Move_Begin( ent );
            //} else {
        // Team Slaves start moving next frame:
        } else {
            ent->nextthink = level.time + FRAME_TIME_S;
            ent->think = Move_Begin;
        }
    } else {
        // accelerative
        ent->pusherMoveInfo.current_speed = 0;
        ent->think = Think_AccelMove;
        ent->nextthink = level.time + FRAME_TIME_S;
    }
}



/**
*
*
*
*   Support routines for angular movement (changes in angle using avelocity)
* 
* 
* 
**/
/**
*   @brief
**/
void AngleMove_Done( edict_t *ent ) {
    VectorClear( ent->avelocity );
    ent->pusherMoveInfo.endfunc( ent );
}
/**
*   @brief
**/
void AngleMove_Final( edict_t *ent ) {
    vec3_t  move;

    if ( ent->pusherMoveInfo.state == STATE_UP ) {
        VectorSubtract( ent->pusherMoveInfo.end_angles, ent->s.angles, move );
    } else {
        VectorSubtract( ent->pusherMoveInfo.start_angles, ent->s.angles, move );
    }

    if ( VectorEmpty( move ) ) {
        AngleMove_Done( ent );
        return;
    }

    VectorScale( move, 1.0f / FRAMETIME, ent->avelocity );

    ent->think = AngleMove_Done;
    ent->nextthink = level.time + FRAME_TIME_S;
}
/**
*   @brief
**/
void AngleMove_Begin( edict_t *ent ) {
    vec3_t  destdelta;
    float   len;
    float   traveltime;
    float   frames;

    // set destdelta to the vector needed to move
    if ( ent->pusherMoveInfo.state == STATE_UP ) {
        VectorSubtract( ent->pusherMoveInfo.end_angles, ent->s.angles, destdelta );
    } else {
        VectorSubtract( ent->pusherMoveInfo.start_angles, ent->s.angles, destdelta );
    }

    // calculate length of vector
    len = VectorLength( destdelta );

    // divide by speed to get time to reach dest
    traveltime = len / ent->pusherMoveInfo.speed;

    if ( traveltime < gi.frame_time_s ) {
        AngleMove_Final( ent );
        return;
    }

	frames = floor( traveltime / gi.frame_time_s );

    // scale the destdelta vector by the time spent traveling to get velocity
    VectorScale( destdelta, 1.0f / traveltime, ent->avelocity );

	// PGM
	//  if we're done accelerating, act as a normal rotation
	if ( ent->pusherMoveInfo.speed >= ent->speed ) {
		// set nextthink to trigger a think when dest is reached
		ent->nextthink = level.time + ( FRAME_TIME_S * frames );
		ent->think = AngleMove_Final;
	} else {
		ent->nextthink = level.time + FRAME_TIME_S;
		ent->think = AngleMove_Begin;
	}
	// PGM
}
/**
*   @brief
**/
void AngleMove_Calc( edict_t *ent, void( *func )( edict_t * ) ) {
    VectorClear( ent->avelocity );
    ent->pusherMoveInfo.endfunc = func;
    if ( level.current_entity == ( ( ent->flags & FL_TEAMSLAVE ) ? ent->teammaster : ent ) ) {
        AngleMove_Begin( ent );
    } else {
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = AngleMove_Begin;
    }
}



/**
*
*
*
*   Move Acceleration:
*
*
*
**/
/*
==============
Think_AccelMove

The team has completed a frame of movement, so
change the speed for the next frame
==============
*/
/**
*   @brief
**/
constexpr float AccelerationDistance( float target, float rate ) {
	return ( target * ( ( target / rate ) + 1 ) / 2 );
}
/**
*   @brief
**/
void plat_CalcAcceleratedMove( g_pusher_moveinfo_t *moveinfo ) {
    float   accel_dist;
    float   decel_dist;

    moveinfo->move_speed = moveinfo->speed;

    if ( moveinfo->remaining_distance < moveinfo->accel ) {
        moveinfo->current_speed = moveinfo->remaining_distance;
        return;
    }

    accel_dist = AccelerationDistance( moveinfo->speed, moveinfo->accel );
    decel_dist = AccelerationDistance( moveinfo->speed, moveinfo->decel );

    if ( ( moveinfo->remaining_distance - accel_dist - decel_dist ) < 0 ) {
        float   f;

        f = ( moveinfo->accel + moveinfo->decel ) / ( moveinfo->accel * moveinfo->decel );
        moveinfo->move_speed = ( -2 + sqrtf( 4 - 4 * f * ( -2 * moveinfo->remaining_distance ) ) ) / ( 2 * f );
        decel_dist = AccelerationDistance( moveinfo->move_speed, moveinfo->decel );
    }

    moveinfo->decel_distance = decel_dist;
}
/**
*   @brief
**/
void plat_Accelerate( g_pusher_moveinfo_t *moveinfo ) {
    // are we decelerating?
    if ( moveinfo->remaining_distance <= moveinfo->decel_distance ) {
        if ( moveinfo->remaining_distance < moveinfo->decel_distance ) {
            if ( moveinfo->next_speed ) {
                moveinfo->current_speed = moveinfo->next_speed;
                moveinfo->next_speed = 0;
                return;
            }
            if ( moveinfo->current_speed > moveinfo->decel ) {
                moveinfo->current_speed -= moveinfo->decel;
            }
        }
        return;
    }

    // are we at full speed and need to start decelerating during this move?
    if ( moveinfo->current_speed == moveinfo->move_speed )
        if ( ( moveinfo->remaining_distance - moveinfo->current_speed ) < moveinfo->decel_distance ) {
            float   p1_distance;
            float   p2_distance;
            float   distance;

            p1_distance = moveinfo->remaining_distance - moveinfo->decel_distance;
            p2_distance = moveinfo->move_speed * ( 1.0f - ( p1_distance / moveinfo->move_speed ) );
            distance = p1_distance + p2_distance;
            moveinfo->current_speed = moveinfo->move_speed;
            moveinfo->next_speed = moveinfo->move_speed - moveinfo->decel * ( p2_distance / distance );
            return;
        }

    // are we accelerating?
    if ( moveinfo->current_speed < moveinfo->speed ) {
        float   old_speed;
        float   p1_distance;
        float   p1_speed;
        float   p2_distance;
        float   distance;

        old_speed = moveinfo->current_speed;

        // figure simple acceleration up to move_speed
        moveinfo->current_speed += moveinfo->accel;
        if ( moveinfo->current_speed > moveinfo->speed ) {
            moveinfo->current_speed = moveinfo->speed;
        }

        // are we accelerating throughout this entire move?
        if ( ( moveinfo->remaining_distance - moveinfo->current_speed ) >= moveinfo->decel_distance ) {
            return;
        }

        // during this move we will accelrate from current_speed to move_speed
        // and cross over the decel_distance; figure the average speed for the
        // entire move
        p1_distance = moveinfo->remaining_distance - moveinfo->decel_distance;
        p1_speed = ( old_speed + moveinfo->move_speed ) / 2.0f;
        p2_distance = moveinfo->move_speed * ( 1.0f - ( p1_distance / p1_speed ) );
        distance = p1_distance + p2_distance;
        moveinfo->current_speed = ( p1_speed * ( p1_distance / distance ) ) + ( moveinfo->move_speed * ( p2_distance / distance ) );
        moveinfo->next_speed = moveinfo->move_speed - moveinfo->decel * ( p2_distance / distance );
        return;
    }

    // we are at constant velocity (move_speed)
    return;
}
/**
*   @brief
**/
void Think_AccelMove( edict_t *ent ) {
    ent->pusherMoveInfo.remaining_distance -= ent->pusherMoveInfo.current_speed;

    if ( ent->pusherMoveInfo.current_speed == 0 ) {      // starting or blocked
        plat_CalcAcceleratedMove( &ent->pusherMoveInfo );
    }

    plat_Accelerate(&ent->pusherMoveInfo);

    // will the entire move complete on next frame?
    if (ent->pusherMoveInfo.remaining_distance <= ent->pusherMoveInfo.current_speed) {
        Move_Final(ent);
        return;
    }

    VectorScale(ent->pusherMoveInfo.dir, ent->pusherMoveInfo.current_speed * 10, ent->velocity);

	ent->nextthink = level.time + 10_hz;
    ent->think = Think_AccelMove;

    // Find entities that move along with this entity.
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}


void plat_go_down(edict_t *ent);

void plat_hit_top(edict_t *ent)
{
    if (!(ent->flags & FL_TEAMSLAVE)) {
        if (ent->pusherMoveInfo.sound_end)
            gi.sound(ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pusherMoveInfo.sound_end, 1, ATTN_STATIC, 0);
        ent->s.sound = 0;
    }
    ent->pusherMoveInfo.state = STATE_TOP;

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

void plat_hit_bottom(edict_t *ent)
{
    if (!(ent->flags & FL_TEAMSLAVE)) {
        if (ent->pusherMoveInfo.sound_end)
            gi.sound(ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pusherMoveInfo.sound_end, 1, ATTN_STATIC, 0);
        ent->s.sound = 0;
    }
    ent->pusherMoveInfo.state = STATE_BOTTOM;

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

void plat_go_down(edict_t *ent)
{
    if (!(ent->flags & FL_TEAMSLAVE)) {
        if (ent->pusherMoveInfo.sound_start)
            gi.sound(ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0);
        ent->s.sound = ent->pusherMoveInfo.sound_middle;
    }
    ent->pusherMoveInfo.state = STATE_DOWN;
    Move_Calc(ent, ent->pusherMoveInfo.end_origin, plat_hit_bottom);
}

void plat_go_up(edict_t *ent)
{
    if (!(ent->flags & FL_TEAMSLAVE)) {
        if (ent->pusherMoveInfo.sound_start)
            gi.sound(ent, CHAN_NO_PHS_ADD + CHAN_VOICE, ent->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0);
        ent->s.sound = ent->pusherMoveInfo.sound_middle;
    }
    ent->pusherMoveInfo.state = STATE_UP;
    Move_Calc(ent, ent->pusherMoveInfo.start_origin, plat_hit_top);
}

void plat_blocked(edict_t *self, edict_t *other)
{
    if ( !(other->svflags & SVF_MONSTER) && (!other->client) ) {
        const bool knockBack = true;
        // give it a chance to go away on it's own terms (like gibs)
        T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, knockBack, 0, MEANS_OF_DEATH_CRUSHED);
        // if it's still there, nuke it
        if ( other ) {
            BecomeExplosion1( other );
        }
        return;
    }

    const bool knockBack = false;
    T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, knockBack, 0, MEANS_OF_DEATH_CRUSHED);

    if (self->pusherMoveInfo.state == STATE_UP)
        plat_go_down(self);
    else if (self->pusherMoveInfo.state == STATE_DOWN)
        plat_go_up(self);
}


void Use_Plat(edict_t *ent, edict_t *other, edict_t *activator)
{
    // WID: <Q2RTXP> For func_button support.
    //if ( ( other && !strcmp( other->classname, "func_button" ) ) ) {
        if ( ent->pusherMoveInfo.state == STATE_UP || ent->pusherMoveInfo.state == STATE_TOP ) {
            plat_go_down( ent );
        } else if ( ent->pusherMoveInfo.state == STATE_DOWN || ent->pusherMoveInfo.state == STATE_BOTTOM ) {
            plat_go_up( ent );
        }
    //    return;     // already down
    //}
    // WID: </Q2RTXP>

    #if 0

        if ( ent->think ) {
            return;
        }
        plat_go_down(ent);
    #endif
}


void Touch_Plat_Center(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (!other->client)
        return;

    if (other->health <= 0)
        return;

    ent = ent->enemy;   // now point at the plat, not the trigger
    if ( ent->pusherMoveInfo.state == STATE_BOTTOM ) {
        plat_go_up( ent );
    } else if ( ent->pusherMoveInfo.state == STATE_TOP ) {
        ent->nextthink = level.time + 1_sec; // the player is still on the plat, so delay going down
    }
}

void plat_spawn_inside_trigger(edict_t *ent)
{
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

    tmin[0] = ent->mins[0] + 25;
    tmin[1] = ent->mins[1] + 25;
    tmin[2] = ent->mins[2];

    tmax[0] = ent->maxs[0] - 25;
    tmax[1] = ent->maxs[1] - 25;
    tmax[2] = ent->maxs[2] + 8;

    tmin[2] = tmax[2] - (ent->pos1[2] - ent->pos2[2] + st.lip);

    if (ent->spawnflags & PLAT_LOW_TRIGGER)
        tmax[2] = tmin[2] + 8;

    if (tmax[0] - tmin[0] <= 0) {
        tmin[0] = (ent->mins[0] + ent->maxs[0]) * 0.5f;
        tmax[0] = tmin[0] + 1;
    }
    if (tmax[1] - tmin[1] <= 0) {
        tmin[1] = (ent->mins[1] + ent->maxs[1]) * 0.5f;
        tmax[1] = tmin[1] + 1;
    }

    VectorCopy(tmin, trigger->mins);
    VectorCopy(tmax, trigger->maxs);

    gi.linkentity(trigger);
}


/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER
speed   default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in the extended position until it is trigger, when it will lower and become a normal plat.

"speed" overrides default 200.
"accel" overrides default 500
"lip"   overrides default 8 pixel lip

If the "height" key is set, that will determine the amount the plat moves, instead of being implicitly determoveinfoned by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/
void SP_func_plat(edict_t *ent)
{
    VectorClear(ent->s.angles);
    ent->solid = SOLID_BSP;
    ent->movetype = MOVETYPE_PUSH;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel(ent, ent->model);

    ent->blocked = plat_blocked;

    if (!ent->speed)
        ent->speed = 20;
    else
        ent->speed *= 0.1f;

    if (!ent->accel)
        ent->accel = 5;
    else
        ent->accel *= 0.1f;

    if (!ent->decel)
        ent->decel = 5;
    else
        ent->decel *= 0.1f;

    if (!ent->dmg)
        ent->dmg = 2;

    if (!st.lip)
        st.lip = 8;

    // pos1 is the top position, pos2 is the bottom
    VectorCopy(ent->s.origin, ent->pos1);
    VectorCopy(ent->s.origin, ent->pos2);
    if (st.height)
        ent->pos2[2] -= st.height;
    else
        ent->pos2[2] -= (ent->maxs[2] - ent->mins[2]) - st.lip;

    ent->use = Use_Plat;

    plat_spawn_inside_trigger(ent);     // the "start moving" trigger

    if (ent->targetname) {
        ent->pusherMoveInfo.state = STATE_UP;
    } else {
        VectorCopy(ent->pos2, ent->s.origin);
        gi.linkentity(ent);
        ent->pusherMoveInfo.state = STATE_BOTTOM;
    }

    ent->pusherMoveInfo.speed = ent->speed;
    ent->pusherMoveInfo.accel = ent->accel;
    ent->pusherMoveInfo.decel = ent->decel;
    ent->pusherMoveInfo.wait = ent->wait;
    VectorCopy(ent->pos1, ent->pusherMoveInfo.start_origin);
    VectorCopy(ent->s.angles, ent->pusherMoveInfo.start_angles);
    VectorCopy(ent->pos2, ent->pusherMoveInfo.end_origin);
    VectorCopy(ent->s.angles, ent->pusherMoveInfo.end_angles);

    ent->pusherMoveInfo.sound_start = gi.soundindex("plats/plat_start_01.wav");
    ent->pusherMoveInfo.sound_middle = gi.soundindex("plats/plat_mid_01.wav");
    ent->pusherMoveInfo.sound_end = gi.soundindex("plats/plat_end_01.wav");
}

//====================================================================

/*QUAKED func_rotating (0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP ANIMATED ANIMATED_FAST
You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"speed" determines how fast it moves; default value is 100.
"dmg"   damage to inflict when blocked (2 default)

REVERSE will cause the it to rotate in the opposite direction.
STOP mean it will stop moving instead of pushing entities
*/

void rotating_blocked(edict_t *self, edict_t *other)
{
    T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);
}

void rotating_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (!VectorEmpty(self->avelocity))
        T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);
}

void rotating_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (!VectorEmpty(self->avelocity)) {
        self->s.sound = 0;
        VectorClear(self->avelocity);
        self->touch = NULL;
    } else {
        self->s.sound = self->pusherMoveInfo.sound_middle;
        VectorScale(self->movedir, self->speed, self->avelocity);
        if (self->spawnflags & 16)
            self->touch = rotating_touch;
    }
}

void SP_func_rotating(edict_t *ent)
{
    ent->solid = SOLID_BSP;
    if (ent->spawnflags & 32)
        ent->movetype = MOVETYPE_STOP;
    else
        ent->movetype = MOVETYPE_PUSH;

    ent->s.entityType = ET_PUSHER;

    // set the axis of rotation
    VectorClear(ent->movedir);
    if (ent->spawnflags & 4)
        ent->movedir[2] = 1.0f;
    else if (ent->spawnflags & 8)
        ent->movedir[0] = 1.0f;
    else // Z_AXIS
        ent->movedir[1] = 1.0f;

    // check for reverse rotation
    if (ent->spawnflags & 2)
        VectorNegate(ent->movedir, ent->movedir);

    if (!ent->speed)
        ent->speed = 100;
    if (!ent->dmg)
        ent->dmg = 2;

//  ent->pusherMoveInfo.sound_middle = "doors/hydro1.wav";

    ent->use = rotating_use;
    if (ent->dmg)
        ent->blocked = rotating_blocked;

    if (ent->spawnflags & 1)
        ent->use(ent, NULL, NULL);

    if (ent->spawnflags & 64)
        ent->s.effects |= EF_ANIM_ALL;
    if (ent->spawnflags & 128)
        ent->s.effects |= EF_ANIM_ALLFAST;

    gi.setmodel(ent, ent->model);
    gi.linkentity(ent);
}

/*
======================================================================

BUTTONS

======================================================================
*/

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

void button_done(edict_t *self)
{
    self->pusherMoveInfo.state = STATE_BOTTOM;
    self->s.effects &= ~EF_ANIM23;
    self->s.effects |= EF_ANIM01;
}

void button_return(edict_t *self)
{
    self->pusherMoveInfo.state = STATE_DOWN;

    Move_Calc(self, self->pusherMoveInfo.start_origin, button_done);

    self->s.frame = 0;

    if (self->health)
        self->takedamage = DAMAGE_YES;
}

void button_wait(edict_t *self)
{
    self->pusherMoveInfo.state = STATE_TOP;
    self->s.effects &= ~EF_ANIM01;
    self->s.effects |= EF_ANIM23;

    SVG_UseTargets(self, self->activator);
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
    if (self->pusherMoveInfo.wait >= 0) {
		self->nextthink = level.time + sg_time_t::from_sec( self->pusherMoveInfo.wait );
        self->think = button_return;
    }
}

void button_fire(edict_t *self)
{
    if (self->pusherMoveInfo.state == STATE_UP || self->pusherMoveInfo.state == STATE_TOP)
        return;

    self->pusherMoveInfo.state = STATE_UP;
    if (self->pusherMoveInfo.sound_start && !(self->flags & FL_TEAMSLAVE))
        gi.sound(self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0);
    Move_Calc(self, self->pusherMoveInfo.end_origin, button_wait);
}

void button_use(edict_t *self, edict_t *other, edict_t *activator)
{
    self->activator = activator;
    button_fire(self);
}

void button_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (!other->client)
        return;

    if (other->health <= 0)
        return;

    self->activator = other;
    button_fire(self);
}

void button_killed(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    self->activator = attacker;
    self->health = self->max_health;
    self->takedamage = DAMAGE_NO;
    button_fire(self);
}

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
        // WID: LUA: TODO: This breaks old default behavior.
    #if 0
    } else if ( !ent->targetname )
    #else
    }
    #endif
        //ent->touch = button_touch;
    ent->targetUseFlags = ENTITY_TARGET_USE_FLAG_TOGGLE;

    ent->pusherMoveInfo.state = STATE_BOTTOM;

    ent->pusherMoveInfo.speed = ent->speed;
    ent->pusherMoveInfo.accel = ent->accel;
    ent->pusherMoveInfo.decel = ent->decel;
    ent->pusherMoveInfo.wait = ent->wait;
    VectorCopy(ent->pos1, ent->pusherMoveInfo.start_origin);
    VectorCopy(ent->s.angles, ent->pusherMoveInfo.start_angles);
    VectorCopy(ent->pos2, ent->pusherMoveInfo.end_origin);
    VectorCopy(ent->s.angles, ent->pusherMoveInfo.end_angles);

    gi.linkentity(ent);
}

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

void door_use_areaportals(edict_t *self, bool open)
{
    edict_t *t = NULL;

    if (!self->targetNames.target)
        return;

    while ((t = SVG_Find(t, FOFS(targetname), self->targetNames.target))) {
        //if (Q_stricmp(t->classname, "func_areaportal") == 0) {
        if ( t->s.entityType == ET_AREA_PORTAL ) {
            gi.SetAreaPortalState(t->style, open);

            //self->s.event = ( open ? EV_AREAPORTAL_OPEN : EV_AREAPORTAL_CLOSE );
            //self->s.event[ 1 ] = t->style;
            //self->s.event[ 2 ] = open;
        }
    }
}

void door_go_down(edict_t *self);

void door_hit_top(edict_t *self)
{
    if (!(self->flags & FL_TEAMSLAVE)) {
        if (self->pusherMoveInfo.sound_end)
            gi.sound(self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_end, 1, ATTN_STATIC, 0);
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

    if (self->spawnflags & DOOR_TOGGLE)
        return;

    if (self->pusherMoveInfo.wait >= 0) {
        self->think = door_go_down;
		self->nextthink = level.time + sg_time_t::from_sec( self->pusherMoveInfo.wait );
    }

}

void door_hit_bottom(edict_t *self)
{
    if (!(self->flags & FL_TEAMSLAVE)) {
        if (self->pusherMoveInfo.sound_end)
            gi.sound(self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_end, 1, ATTN_STATIC, 0);
        self->s.sound = 0;
    }
    self->pusherMoveInfo.state = STATE_BOTTOM;
    door_use_areaportals(self, false);

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

void door_go_down(edict_t *self)
{
    if (!(self->flags & FL_TEAMSLAVE)) {
        if (self->pusherMoveInfo.sound_start)
            gi.sound(self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0);
        self->s.sound = self->pusherMoveInfo.sound_middle;
    }
    if (self->max_health) {
        self->takedamage = DAMAGE_YES;
        self->health = self->max_health;
    }

    self->pusherMoveInfo.state = STATE_DOWN;
    if (strcmp(self->classname, "func_door") == 0)
        Move_Calc(self, self->pusherMoveInfo.start_origin, door_hit_bottom);
    else if (strcmp(self->classname, "func_door_rotating") == 0)
        AngleMove_Calc(self, door_hit_bottom);
}

void door_go_up(edict_t *self, edict_t *activator)
{
    if (self->pusherMoveInfo.state == STATE_UP)
        return;     // already going up

    if (self->pusherMoveInfo.state == STATE_TOP) {
        // reset top wait time
        if (self->pusherMoveInfo.wait >= 0)
			self->nextthink = level.time + sg_time_t::from_sec( self->pusherMoveInfo.wait );
        return;
    }

    if (!(self->flags & FL_TEAMSLAVE)) {
        if (self->pusherMoveInfo.sound_start)
            gi.sound(self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0);
        self->s.sound = self->pusherMoveInfo.sound_middle;
    }
    self->pusherMoveInfo.state = STATE_UP;
    if (strcmp(self->classname, "func_door") == 0)
        Move_Calc(self, self->pusherMoveInfo.end_origin, door_hit_top);
    else if (strcmp(self->classname, "func_door_rotating") == 0)
        AngleMove_Calc(self, door_hit_top);

    SVG_UseTargets(self, activator);
    door_use_areaportals(self, true);
}

void door_use(edict_t *self, edict_t *other, edict_t *activator)
{
    edict_t *ent;

    if (self->flags & FL_TEAMSLAVE)
        return;

    if (self->spawnflags & DOOR_TOGGLE) {
        if (self->pusherMoveInfo.state == STATE_UP || self->pusherMoveInfo.state == STATE_TOP) {
            // trigger all paired doors
            for (ent = self ; ent ; ent = ent->teamchain) {
                ent->message = NULL;
                ent->touch = NULL;
                door_go_down(ent);
            }
            return;
        }
    }

    // trigger all paired doors
    for (ent = self ; ent ; ent = ent->teamchain) {
        ent->message = NULL;
        ent->touch = NULL;
        door_go_up(ent, activator);
    }
}

void Touch_DoorTrigger(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other->health <= 0)
        return;

    if (!(other->svflags & SVF_MONSTER) && (!other->client))
        return;

    if ((self->owner->spawnflags & DOOR_NOMONSTER) && (other->svflags & SVF_MONSTER))
        return;

    if (level.time < self->touch_debounce_time)
        return;
	self->touch_debounce_time = level.time + 1_sec;

    door_use(self->owner, other, other);
}

void Think_CalcMoveSpeed(edict_t *self)
{
    edict_t *ent;
    float   minDist;
    float   time;
    float   newspeed;
    float   ratio;
    float   dist;

    if (self->flags & FL_TEAMSLAVE)
        return;     // only the team master does this

    // find the smallest distance any member of the team will be moving
	minDist = fabsf(self->pusherMoveInfo.distance);
    for (ent = self->teamchain; ent; ent = ent->teamchain) {
        dist = fabsf(ent->pusherMoveInfo.distance);
        if (dist < minDist )
			minDist = dist;
    }

    time = minDist / self->pusherMoveInfo.speed;

    // adjust speeds so they will all complete at the same time
    for (ent = self; ent; ent = ent->teamchain) {
        newspeed = fabsf(ent->pusherMoveInfo.distance) / time;
        ratio = newspeed / ent->pusherMoveInfo.speed;
        if (ent->pusherMoveInfo.accel == ent->pusherMoveInfo.speed)
            ent->pusherMoveInfo.accel = newspeed;
        else
            ent->pusherMoveInfo.accel *= ratio;
        if (ent->pusherMoveInfo.decel == ent->pusherMoveInfo.speed)
            ent->pusherMoveInfo.decel = newspeed;
        else
            ent->pusherMoveInfo.decel *= ratio;
        ent->pusherMoveInfo.speed = newspeed;
    }
}

void Think_SpawnDoorTrigger(edict_t *ent)
{
    edict_t     *other;
    vec3_t      mins, maxs;

    if (ent->flags & FL_TEAMSLAVE)
        return;     // only the team leader spawns a trigger

    VectorCopy(ent->absmin, mins);
    VectorCopy(ent->absmax, maxs);

    for (other = ent->teamchain ; other ; other = other->teamchain) {
        AddPointToBounds(other->absmin, mins, maxs);
        AddPointToBounds(other->absmax, mins, maxs);
    }

    // expand
    mins[0] -= 60;
    mins[1] -= 60;
    maxs[0] += 60;
    maxs[1] += 60;

    other = SVG_AllocateEdict();
    VectorCopy(mins, other->mins);
    VectorCopy(maxs, other->maxs);
    other->owner = ent;
    other->solid = SOLID_TRIGGER;
    other->movetype = MOVETYPE_NONE;
    other->s.entityType = ET_PUSH_TRIGGER;
    other->touch = Touch_DoorTrigger;
    gi.linkentity(other);

    if (ent->spawnflags & DOOR_START_OPEN)
        door_use_areaportals(ent, true);

    Think_CalcMoveSpeed(ent);
}

void door_blocked(edict_t *self, edict_t *other)
{
    edict_t *ent;

    if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
        // give it a chance to go away on it's own terms (like gibs)
        T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MEANS_OF_DEATH_CRUSHED);
        // if it's still there, nuke it
        if (other)
            BecomeExplosion1(other);
        return;
    }

    T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);

    if (self->spawnflags & DOOR_CRUSHER)
        return;


// if a door has a negative wait, it would never come back if blocked,
// so let it just squash the object to death real fast
    if (self->pusherMoveInfo.wait >= 0) {
        if (self->pusherMoveInfo.state == STATE_DOWN) {
            for (ent = self->teammaster ; ent ; ent = ent->teamchain)
                door_go_up(ent, ent->activator);
        } else {
            for (ent = self->teammaster ; ent ; ent = ent->teamchain)
                door_go_down(ent);
        }
    }
}

void door_killed(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    edict_t *ent;

    for (ent = self->teammaster ; ent ; ent = ent->teamchain) {
        ent->health = ent->max_health;
        ent->takedamage = DAMAGE_NO;
    }
    door_use(self->teammaster, attacker, attacker);
}

void door_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (!other->client)
        return;

    if (level.time < self->touch_debounce_time)
        return;
    self->touch_debounce_time = level.time + 5_sec;

    gi.centerprintf(other, "%s", self->message);
    gi.sound(other, CHAN_AUTO, gi.soundindex("hud/chat01.wav"), 1, ATTN_NORM, 0);
}

void door_postspawn( edict_t *self ) {
    //if ( self->spawnflags & DOOR_START_OPEN ) {
    //    //SVG_UseTargets( self, self );
    //    door_use_areaportals( self, true );
    //    //self->pusherMoveInfo.state = STATE_TOP;
    //}
}

void SP_func_door(edict_t *ent)
{
    vec3_t  abs_movedir;

    if (ent->sounds != 1) {
        ent->pusherMoveInfo.sound_start = gi.soundindex("doors/door_start_01.wav");
        ent->pusherMoveInfo.sound_middle = gi.soundindex("doors/door_mid_01.wav");
        ent->pusherMoveInfo.sound_end = gi.soundindex("doors/door_end_01.wav");
    }

    SVG_SetMoveDir(ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel(ent, ent->model);

    ent->postspawn = door_postspawn;
    ent->blocked = door_blocked;
    ent->use = door_use;

    if (!ent->speed)
        ent->speed = 100;
    if (deathmatch->value)
        ent->speed *= 2;

    if (!ent->accel)
        ent->accel = ent->speed;
    if (!ent->decel)
        ent->decel = ent->speed;

    if (!ent->wait)
        ent->wait = 3;
    if (!st.lip)
        st.lip = 8;
    if (!ent->dmg)
        ent->dmg = 2;

    // calculate second position
    VectorCopy(ent->s.origin, ent->pos1);
    abs_movedir[0] = fabsf(ent->movedir[0]);
    abs_movedir[1] = fabsf(ent->movedir[1]);
    abs_movedir[2] = fabsf(ent->movedir[2]);
    ent->pusherMoveInfo.distance = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - st.lip;
    VectorMA(ent->pos1, ent->pusherMoveInfo.distance, ent->movedir, ent->pos2);

    // if it starts open, switch the positions
    if (ent->spawnflags & DOOR_START_OPEN) {
        VectorCopy(ent->pos2, ent->s.origin);
        VectorCopy(ent->pos1, ent->pos2);
        VectorCopy(ent->s.origin, ent->pos1);
    }

    ent->pusherMoveInfo.state = STATE_BOTTOM;

    if (ent->health) {
        ent->takedamage = DAMAGE_YES;
        ent->die = door_killed;
        ent->max_health = ent->health;
    } else if (ent->targetname && ent->message) {
        gi.soundindex("hud/chat01.wav");
        ent->touch = door_touch;
    }

    ent->pusherMoveInfo.speed = ent->speed;
    ent->pusherMoveInfo.accel = ent->accel;
    ent->pusherMoveInfo.decel = ent->decel;
    ent->pusherMoveInfo.wait = ent->wait;
    VectorCopy(ent->pos1, ent->pusherMoveInfo.start_origin);
    VectorCopy(ent->s.angles, ent->pusherMoveInfo.start_angles);
    VectorCopy(ent->pos2, ent->pusherMoveInfo.end_origin);
    VectorCopy(ent->s.angles, ent->pusherMoveInfo.end_angles);

    if (ent->spawnflags & 16)
        ent->s.effects |= EF_ANIM_ALL;
    if (ent->spawnflags & 64)
        ent->s.effects |= EF_ANIM_ALLFAST;

    // to simplify logic elsewhere, make non-teamed doors into a team of one
    if (!ent->targetNames.team)
        ent->teammaster = ent;

    gi.linkentity(ent);

	ent->nextthink = level.time + FRAME_TIME_S;

    if (ent->health || ent->targetname)
        ent->think = Think_CalcMoveSpeed;
    else
        ent->think = Think_SpawnDoorTrigger;
}


/*QUAKED func_door_rotating (0 .5 .8) ? START_OPEN REVERSE CRUSHER NOMONSTER ANIMATED TOGGLE X_AXIS Y_AXIS
TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN  the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER   monsters will not trigger this door

You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"message"   is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"     determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"    if set, door must be shot open
"speed"     movement speed (100 default)
"wait"      wait before returning (3 default, -1 = never return)
"dmg"       damage to inflict when blocked (2 default)
"sounds"
1)  silent
2)  light
3)  medium
4)  heavy
*/

void SP_func_door_rotating(edict_t *ent)
{
    VectorClear(ent->s.angles);

    // set the axis of rotation
    VectorClear(ent->movedir);
    if (ent->spawnflags & DOOR_X_AXIS)
        ent->movedir[2] = 1.0f;
    else if (ent->spawnflags & DOOR_Y_AXIS)
        ent->movedir[0] = 1.0f;
    else // Z_AXIS
        ent->movedir[1] = 1.0f;

    // check for reverse rotation
    if (ent->spawnflags & DOOR_REVERSE)
        VectorNegate(ent->movedir, ent->movedir);

    if (!st.distance) {
        gi.dprintf("%s at %s with no distance set\n", ent->classname, vtos(ent->s.origin));
        st.distance = 90;
    }

    VectorCopy(ent->s.angles, ent->pos1);
    VectorMA(ent->s.angles, st.distance, ent->movedir, ent->pos2);
    ent->pusherMoveInfo.distance = st.distance;

    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel(ent, ent->model);

    ent->blocked = door_blocked;
    ent->use = door_use;

    if (!ent->speed)
        ent->speed = 100;
    if (!ent->accel)
        ent->accel = ent->speed;
    if (!ent->decel)
        ent->decel = ent->speed;

    if (!ent->wait)
        ent->wait = 3;
    if (!ent->dmg)
        ent->dmg = 2;

    if (ent->sounds != 1) {
        ent->pusherMoveInfo.sound_start = gi.soundindex("doors/door_start_01.wav");
        ent->pusherMoveInfo.sound_middle = gi.soundindex("doors/door_mid_01.wav");
        ent->pusherMoveInfo.sound_end = gi.soundindex("doors/door_end_01.wav");
    }

    // if it starts open, switch the positions
    if (ent->spawnflags & DOOR_START_OPEN) {
        VectorCopy(ent->pos2, ent->s.angles);
        VectorCopy(ent->pos1, ent->pos2);
        VectorCopy(ent->s.angles, ent->pos1);
        VectorNegate(ent->movedir, ent->movedir);
    }

    if (ent->health) {
        ent->takedamage = DAMAGE_YES;
        ent->die = door_killed;
        ent->max_health = ent->health;
    }

    if (ent->targetname && ent->message) {
        gi.soundindex("hud/chat01.wav");
        ent->touch = door_touch;
    }

    ent->pusherMoveInfo.state = STATE_BOTTOM;
    ent->pusherMoveInfo.speed = ent->speed;
    ent->pusherMoveInfo.accel = ent->accel;
    ent->pusherMoveInfo.decel = ent->decel;
    ent->pusherMoveInfo.wait = ent->wait;
    VectorCopy(ent->s.origin, ent->pusherMoveInfo.start_origin);
    VectorCopy(ent->pos1, ent->pusherMoveInfo.start_angles);
    VectorCopy(ent->s.origin, ent->pusherMoveInfo.end_origin);
    VectorCopy(ent->pos2, ent->pusherMoveInfo.end_angles);

    if (ent->spawnflags & 16)
        ent->s.effects |= EF_ANIM_ALL;

    // to simplify logic elsewhere, make non-teamed doors into a team of one
    if (!ent->targetNames.team)
        ent->teammaster = ent;

    gi.linkentity(ent);

	ent->nextthink = level.time + FRAME_TIME_S;
    if (ent->health || ent->targetname)
        ent->think = Think_CalcMoveSpeed;
    else
        ent->think = Think_SpawnDoorTrigger;
}


/*QUAKED func_water (0 .5 .8) ? START_OPEN
func_water is a moveable water brush.  It must be targeted to operate.  Use a non-water texture at your own risk.

START_OPEN causes the water to move to its destination when spawned and operate in reverse.

"angle"     determines the opening direction (up or down only)
"speed"     movement speed (25 default)
"wait"      wait before returning (-1 default, -1 = TOGGLE)
"lip"       lip remaining at end of move (0 default)
"sounds"    (yes, these need to be changed)
0)  no sound
1)  water
2)  lava
*/

void SP_func_water(edict_t *self)
{
    vec3_t  abs_movedir;

    SVG_SetMoveDir(self->s.angles, self->movedir );
    self->movetype = MOVETYPE_PUSH;
    self->solid = SOLID_BSP;
    self->s.entityType = ET_PUSHER;
    gi.setmodel(self, self->model);

    switch (self->sounds) {
    default:
        break;

    case 1: // water
        self->pusherMoveInfo.sound_start = gi.soundindex("world/mov_watr.wav");
        self->pusherMoveInfo.sound_end = gi.soundindex("world/stp_watr.wav");
        break;

    case 2: // lava
        self->pusherMoveInfo.sound_start = gi.soundindex("world/mov_watr.wav");
        self->pusherMoveInfo.sound_end = gi.soundindex("world/stp_watr.wav");
        break;
    }

    // calculate second position
    VectorCopy(self->s.origin, self->pos1);
    abs_movedir[0] = fabsf(self->movedir[0]);
    abs_movedir[1] = fabsf(self->movedir[1]);
    abs_movedir[2] = fabsf(self->movedir[2]);
    self->pusherMoveInfo.distance = abs_movedir[0] * self->size[0] + abs_movedir[1] * self->size[1] + abs_movedir[2] * self->size[2] - st.lip;
    VectorMA(self->pos1, self->pusherMoveInfo.distance, self->movedir, self->pos2);

    // if it starts open, switch the positions
    if (self->spawnflags & DOOR_START_OPEN) {
        VectorCopy(self->pos2, self->s.origin);
        VectorCopy(self->pos1, self->pos2);
        VectorCopy(self->s.origin, self->pos1);
    }

    VectorCopy(self->pos1, self->pusherMoveInfo.start_origin);
    VectorCopy(self->s.angles, self->pusherMoveInfo.start_angles);
    VectorCopy(self->pos2, self->pusherMoveInfo.end_origin);
    VectorCopy(self->s.angles, self->pusherMoveInfo.end_angles);

    self->pusherMoveInfo.state = STATE_BOTTOM;

    if (!self->speed)
        self->speed = 25;
    self->pusherMoveInfo.accel = self->pusherMoveInfo.decel = self->pusherMoveInfo.speed = self->speed;

    if (!self->wait)
        self->wait = -1;
    self->pusherMoveInfo.wait = self->wait;

    self->use = door_use;

    if (self->wait == -1)
        self->spawnflags |= DOOR_TOGGLE;

    self->classname = "func_door";

    gi.linkentity(self);
}


#define TRAIN_START_ON      1
#define TRAIN_TOGGLE        2
#define TRAIN_BLOCK_STOPS   4

/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed   default 100
dmg     default 2
noise   looping sound to play when the train is in motion

*/
void train_next(edict_t *self);

void train_blocked(edict_t *self, edict_t *other)
{
    if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
        // give it a chance to go away on it's own terms (like gibs)
        T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MEANS_OF_DEATH_CRUSHED);
        // if it's still there, nuke it
        if (other)
            BecomeExplosion1(other);
        return;
    }

    if (level.time < self->touch_debounce_time)
        return;

    if (!self->dmg)
        return;
    self->touch_debounce_time = level.time + 0.5_sec;
    T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);
}

void train_wait(edict_t *self)
{
    if (self->targetEntities.target->targetNames.path) {
        char    *savetarget;
        edict_t *ent;

        ent = self->targetEntities.target;
        savetarget = ent->targetNames.target;
        ent->targetNames.target = ent->targetNames.path;
        SVG_UseTargets(ent, self->activator);
        ent->targetNames.target = savetarget;

        // make sure we didn't get killed by a targetNames.kill
        if (!self->inuse)
            return;
    }

    if (self->pusherMoveInfo.wait) {
        if (self->pusherMoveInfo.wait > 0) {
            self->nextthink = level.time + sg_time_t::from_sec( self->pusherMoveInfo.wait );
            self->think = train_next;
        } else if (self->spawnflags & TRAIN_TOGGLE) { // && wait < 0
            train_next(self);
            self->spawnflags &= ~TRAIN_START_ON;
            VectorClear(self->velocity);
            self->nextthink = 0_ms;
        }

        if (!(self->flags & FL_TEAMSLAVE)) {
            if (self->pusherMoveInfo.sound_end)
                gi.sound(self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_end, 1, ATTN_STATIC, 0);
            self->s.sound = 0;
        }
    } else {
        train_next(self);
    }

}

void train_next(edict_t *self)
{
    edict_t     *ent;
    vec3_t      dest;
    bool        first;

    first = true;
again:
    if (!self->targetNames.target) {
//      gi.dprintf ("train_next: no next target\n");
        return;
    }

    ent = SVG_PickTarget(self->targetNames.target);
    if (!ent) {
        gi.dprintf("train_next: bad target %s\n", self->targetNames.target);
        return;
    }

    self->targetNames.target = ent->targetNames.target;

    // check for a teleport path_corner
    if (ent->spawnflags & 1) {
        if (!first) {
            gi.dprintf("connected teleport path_corners, see %s at %s\n", ent->classname, vtos(ent->s.origin));
            return;
        }
        first = false;
        VectorSubtract(ent->s.origin, self->mins, self->s.origin);
        VectorCopy(self->s.origin, self->s.old_origin);
        self->s.event = EV_OTHER_TELEPORT;
        gi.linkentity(self);
        goto again;
    }

    self->pusherMoveInfo.wait = ent->wait;
    self->targetEntities.target = ent;

    if (!(self->flags & FL_TEAMSLAVE)) {
        if (self->pusherMoveInfo.sound_start)
            gi.sound(self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pusherMoveInfo.sound_start, 1, ATTN_STATIC, 0);
        self->s.sound = self->pusherMoveInfo.sound_middle;
    }

    VectorSubtract(ent->s.origin, self->mins, dest);
    self->pusherMoveInfo.state = STATE_TOP;
    VectorCopy(self->s.origin, self->pusherMoveInfo.start_origin);
    VectorCopy(dest, self->pusherMoveInfo.end_origin);
    Move_Calc(self, dest, train_wait);
    self->spawnflags |= TRAIN_START_ON;
}

void train_resume(edict_t *self)
{
    edict_t *ent;
    vec3_t  dest;

    ent = self->targetEntities.target;

    VectorSubtract(ent->s.origin, self->mins, dest);
    self->pusherMoveInfo.state = STATE_TOP;
    VectorCopy(self->s.origin, self->pusherMoveInfo.start_origin);
    VectorCopy(dest, self->pusherMoveInfo.end_origin);
    Move_Calc(self, dest, train_wait);
    self->spawnflags |= TRAIN_START_ON;
}

void func_train_find(edict_t *self)
{
    edict_t *ent;

    if (!self->targetNames.target) {
        gi.dprintf("train_find: no target\n");
        return;
    }
    ent = SVG_PickTarget(self->targetNames.target);
    if (!ent) {
        gi.dprintf("train_find: target %s not found\n", self->targetNames.target);
        return;
    }
    self->targetNames.target = ent->targetNames.target;

    VectorSubtract(ent->s.origin, self->mins, self->s.origin);
    gi.linkentity(self);

    // if not triggered, start immediately
    if (!self->targetname)
        self->spawnflags |= TRAIN_START_ON;

    if (self->spawnflags & TRAIN_START_ON) {
		self->nextthink = level.time + FRAME_TIME_S;
        self->think = train_next;
        self->activator = self;
    }
}

void train_use(edict_t *self, edict_t *other, edict_t *activator)
{
    self->activator = activator;

    if (self->spawnflags & TRAIN_START_ON) {
        if (!(self->spawnflags & TRAIN_TOGGLE))
            return;
        self->spawnflags &= ~TRAIN_START_ON;
        VectorClear(self->velocity);
        self->nextthink = 0_ms;
    } else {
        if (self->targetEntities.target)
            train_resume(self);
        else
            train_next(self);
    }
}

void SP_func_train(edict_t *self)
{
    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;

    VectorClear(self->s.angles);
    self->blocked = train_blocked;
    if (self->spawnflags & TRAIN_BLOCK_STOPS)
        self->dmg = 0;
    else {
        if (!self->dmg)
            self->dmg = 100;
    }
    self->solid = SOLID_BSP;
    gi.setmodel(self, self->model);

    if (st.noise)
        self->pusherMoveInfo.sound_middle = gi.soundindex(st.noise);

    if (!self->speed)
        self->speed = 100;

    self->pusherMoveInfo.speed = self->speed;
    self->pusherMoveInfo.accel = self->pusherMoveInfo.decel = self->pusherMoveInfo.speed;

    self->use = train_use;

    gi.linkentity(self);

    if (self->targetNames.target) {
        // start trains on the second frame, to make sure their targets have had
        // a chance to spawn
		self->nextthink = level.time + FRAME_TIME_S;
        self->think = func_train_find;
    } else {
        gi.dprintf("func_train without a target at %s\n", vtos(self->absmin));
    }
}


/*QUAKED trigger_elevator (0.3 0.1 0.6) (-8 -8 -8) (8 8 8)
*/
void trigger_elevator_use(edict_t *self, edict_t *other, edict_t *activator)
{
    edict_t *target;

    if (self->movetarget->nextthink) {
//      gi.dprintf("elevator busy\n");
        return;
    }

    if (!other->targetNames.path) {
        gi.dprintf("elevator used with no targetNames.path\n");
        return;
    }

    target = SVG_PickTarget(other->targetNames.path);
    if (!target) {
        gi.dprintf("elevator used with bad targetNames.path: %s\n", other->targetNames.path);
        return;
    }

    self->movetarget->targetEntities.target = target;
    train_resume(self->movetarget);
}

void trigger_elevator_init(edict_t *self)
{
    if (!self->targetNames.target) {
        gi.dprintf("trigger_elevator has no target\n");
        return;
    }
    self->movetarget = SVG_PickTarget(self->targetNames.target);
    if (!self->movetarget) {
        gi.dprintf("trigger_elevator unable to find target %s\n", self->targetNames.target);
        return;
    }
    if (strcmp(self->movetarget->classname, "func_train") != 0) {
        gi.dprintf("trigger_elevator target %s is not a train\n", self->targetNames.target);
        return;
    }

    self->use = trigger_elevator_use;
    self->svflags = SVF_NOCLIENT;
}

void SP_trigger_elevator(edict_t *self)
{
    self->s.entityType = ET_PUSH_TRIGGER;
    self->think = trigger_elevator_init;
	self->nextthink = level.time + FRAME_TIME_S;
}


/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
"wait"          base time between triggering all targets, default is 1
"random"        wait variance, default is 0

so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"delay"         delay before first firing when turned on, default is 0

"pausetime"     additional delay used only the very first time
                and only if spawned with START_ON

These can used but not touched.
*/
void func_timer_think(edict_t *self)
{
    SVG_UseTargets(self, self->activator);
	self->nextthink = level.time + sg_time_t::from_sec( self->wait + crandom( ) * self->random );
}

void func_timer_use(edict_t *self, edict_t *other, edict_t *activator)
{
    self->activator = activator;

    // if on, turn it off
    if (self->nextthink) {
        self->nextthink = 0_ms;
        return;
    }

    // turn it on
    if (self->delay)
		self->nextthink = level.time + sg_time_t::from_sec( self->delay );
    else
        func_timer_think(self);
}

void SP_func_timer(edict_t *self)
{
    if (!self->wait)
        self->wait = 1.0f;

    self->use = func_timer_use;
    self->think = func_timer_think;

    if (self->random >= self->wait) {
		self->random = self->wait - gi.frame_time_s;
        gi.dprintf("func_timer at %s has random >= wait\n", vtos(self->s.origin));
    }

    if (self->spawnflags & 1) {
		self->nextthink = level.time + 1_sec + sg_time_t::from_sec( st.pausetime + self->delay + self->wait + crandom( ) * self->random );
        self->activator = self;
    }

    self->svflags = SVF_NOCLIENT;
}


/*QUAKED func_conveyor (0 .5 .8) ? START_ON TOGGLE
Conveyors are stationary brushes that move what's on them.
The brush should be have a surface with at least one current content enabled.
speed   default 100
*/

void func_conveyor_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (self->spawnflags & 1) {
        self->speed = 0;
        self->spawnflags &= ~1;
    } else {
        self->speed = self->count;
        self->spawnflags |= 1;
    }

    if (!(self->spawnflags & 2))
        self->count = 0;
}

void SP_func_conveyor(edict_t *self)
{
    if (!self->speed)
        self->speed = 100;

    if (!(self->spawnflags & 1)) {
        self->count = self->speed;
        self->speed = 0;
    }

    self->use = func_conveyor_use;

    gi.setmodel(self, self->model);
    self->solid = SOLID_BSP;
    gi.linkentity(self);
}


/*QUAKED func_door_secret (0 .5 .8) ? always_shoot 1st_left 1st_down
A secret door.  Slide back and then to the side.

open_once       doors never closes
1st_left        1st move is left of arrow
1st_down        1st move is down from arrow
always_shoot    door is shootebale even if targeted

"angle"     determines the direction
"dmg"       damage to inflic when blocked (default 2)
"wait"      how long to hold in the open position (default 5, -1 means hold)
*/

#define SECRET_ALWAYS_SHOOT 1
#define SECRET_1ST_LEFT     2
#define SECRET_1ST_DOWN     4

void door_secret_move1(edict_t *self);
void door_secret_move2(edict_t *self);
void door_secret_move3(edict_t *self);
void door_secret_move4(edict_t *self);
void door_secret_move5(edict_t *self);
void door_secret_move6(edict_t *self);
void door_secret_done(edict_t *self);

void door_secret_use(edict_t *self, edict_t *other, edict_t *activator)
{
    // make sure we're not already moving
    if (!VectorEmpty(self->s.origin))
        return;

    Move_Calc(self, self->pos1, door_secret_move1);
    door_use_areaportals(self, true);
}

void door_secret_move1(edict_t *self)
{
	self->nextthink = level.time + 1_sec;
    self->think = door_secret_move2;
}

void door_secret_move2(edict_t *self)
{
    Move_Calc(self, self->pos2, door_secret_move3);
}

void door_secret_move3(edict_t *self)
{
    if (self->wait == -1)
        return;
	self->nextthink = level.time + sg_time_t::from_sec( self->wait );
    self->think = door_secret_move4;
}

void door_secret_move4(edict_t *self)
{
    Move_Calc(self, self->pos1, door_secret_move5);
}

void door_secret_move5(edict_t *self)
{
	self->nextthink = level.time + 1_sec;
    self->think = door_secret_move6;
}

void door_secret_move6(edict_t *self)
{
    Move_Calc(self, vec3_origin, door_secret_done);
}

void door_secret_done(edict_t *self)
{
    if (!(self->targetname) || (self->spawnflags & SECRET_ALWAYS_SHOOT)) {
        self->health = 0;
        self->takedamage = DAMAGE_YES;
    }
    door_use_areaportals(self, false);
}

void door_secret_blocked(edict_t *self, edict_t *other)
{
    if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
        // give it a chance to go away on it's own terms (like gibs)
        T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MEANS_OF_DEATH_CRUSHED);
        // if it's still there, nuke it
        if (other)
            BecomeExplosion1(other);
        return;
    }

    if (level.time < self->touch_debounce_time)
        return;
    self->touch_debounce_time = level.time + 0.5_sec;

    T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);
}

void door_secret_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    self->takedamage = DAMAGE_NO;
    door_secret_use(self, attacker, attacker);
}

void SP_func_door_secret(edict_t *ent)
{
    vec3_t  forward, right, up;
    float   side;
    float   width;
    float   length;

    ent->pusherMoveInfo.sound_start = gi.soundindex("doors/door_start_01.wav");
    ent->pusherMoveInfo.sound_middle = gi.soundindex("doors/door_mid_01.wav");
    ent->pusherMoveInfo.sound_end = gi.soundindex("doors/door_end_01.wav");

    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    gi.setmodel(ent, ent->model);

    ent->blocked = door_secret_blocked;
    ent->use = door_secret_use;

    if (!(ent->targetname) || (ent->spawnflags & SECRET_ALWAYS_SHOOT)) {
        ent->health = 0;
        ent->takedamage = DAMAGE_YES;
        ent->die = door_secret_die;
    }

    if (!ent->dmg)
        ent->dmg = 2;

    if (!ent->wait)
        ent->wait = 5;

    ent->pusherMoveInfo.accel =
        ent->pusherMoveInfo.decel =
            ent->pusherMoveInfo.speed = 50;

    // calculate positions
    AngleVectors(ent->s.angles, forward, right, up);
    VectorClear(ent->s.angles);
    side = 1.0f - (ent->spawnflags & SECRET_1ST_LEFT);
    if (ent->spawnflags & SECRET_1ST_DOWN)
        width = fabsf(DotProduct(up, ent->size));
    else
        width = fabsf(DotProduct(right, ent->size));
    length = fabsf(DotProduct(forward, ent->size));
    if (ent->spawnflags & SECRET_1ST_DOWN)
        VectorMA(ent->s.origin, -1 * width, up, ent->pos1);
    else
        VectorMA(ent->s.origin, side * width, right, ent->pos1);
    VectorMA(ent->pos1, length, forward, ent->pos2);

    if (ent->health) {
        ent->takedamage = DAMAGE_YES;
        ent->die = door_killed;
        ent->max_health = ent->health;
    } else if (ent->targetname && ent->message) {
        gi.soundindex("hud/chat01.wav");
        ent->touch = door_touch;
    }

    ent->classname = "func_door";

    gi.linkentity(ent);
}


/*QUAKED func_killbox (1 0 0) ?
Kills everything inside when fired, irrespective of protection.
*/
static constexpr int32_t SPAWNFLAG_KILLBOX_TRIGGER_BRUSH_CLIP = 32;
void use_killbox(edict_t *self, edict_t *other, edict_t *activator)
{
    self->solid = SOLID_TRIGGER;
    gi.linkentity( self );

    KillBox(self, self->spawnflags & SPAWNFLAG_KILLBOX_TRIGGER_BRUSH_CLIP );

    self->solid = SOLID_NOT;
    gi.linkentity( self );
}

void SP_func_killbox(edict_t *ent)
{
    gi.setmodel(ent, ent->model);
    ent->use = use_killbox;
    ent->svflags = SVF_NOCLIENT;
}

