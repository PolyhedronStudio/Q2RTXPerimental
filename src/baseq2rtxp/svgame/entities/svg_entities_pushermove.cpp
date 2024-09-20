/********************************************************************
*
*
*	ServerGame: Support Routines for implementing brush entity
*				movement.
*
*				(Changes in origin using velocity.)
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/entities/svg_entities_pushermove.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"



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
void Think_AccelMove( edict_t *ent );
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

    if ( ent->pusherMoveInfo.state == STATE_MOVING_UP ) {
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
    if ( ent->pusherMoveInfo.state == STATE_MOVING_UP ) {
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
*   @brief  The team has completed a frame of movement, so
*           change the speed for the next frame.
**/
void Think_AccelMove( edict_t *ent ) {
    ent->pusherMoveInfo.remaining_distance -= ent->pusherMoveInfo.current_speed;

    if ( ent->pusherMoveInfo.current_speed == 0 ) {      // starting or blocked
        plat_CalcAcceleratedMove( &ent->pusherMoveInfo );
    }

    plat_Accelerate( &ent->pusherMoveInfo );

    // will the entire move complete on next frame?
    if ( ent->pusherMoveInfo.remaining_distance <= ent->pusherMoveInfo.current_speed ) {
        Move_Final( ent );
        return;
    }

    VectorScale( ent->pusherMoveInfo.dir, ent->pusherMoveInfo.current_speed * 10, ent->velocity );

    ent->nextthink = level.time + 10_hz;
    ent->think = Think_AccelMove;

    // Find entities that move along with this entity.
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}