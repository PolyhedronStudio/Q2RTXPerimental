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
void SVG_PushMove_MoveDone( edict_t *ent ) {
    // WID: MoveWith: Clear last velocity also.
    VectorClear( ent->velocity );

    if ( ent->pushMoveInfo.endfunc ) {
        ent->pushMoveInfo.endfunc( ent );
    }

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
void SVG_PushMove_MoveFinal( edict_t *ent ) {
    if ( ent->pushMoveInfo.remaining_distance == 0 ) {
        SVG_PushMove_MoveDone( ent );
        return;
    }

    VectorScale( ent->pushMoveInfo.dir, ent->pushMoveInfo.remaining_distance / FRAMETIME, ent->velocity );

    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //}

    ent->think = SVG_PushMove_MoveDone;
    ent->nextthink = level.time + FRAME_TIME_S;
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
void SVG_PushMove_MoveBegin( edict_t *ent ) {
    if ( ( ent->pushMoveInfo.speed * gi.frame_time_s ) >= ent->pushMoveInfo.remaining_distance ) {
        SVG_PushMove_MoveFinal( ent );
        return;
    }
    VectorScale( ent->pushMoveInfo.dir, ent->pushMoveInfo.speed, ent->velocity );

    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //    ent->pushMoveInfo.remaining_distance -= ent->pushMoveInfo.speed * gi.frame_time_s;
    //    ent->nextthink = level.time + FRAME_TIME_S;
    //    ent->think = SVG_PushMove_MoveBegin;
    //} else {
    const float frames = floor( ( ent->pushMoveInfo.remaining_distance / ent->pushMoveInfo.speed ) / gi.frame_time_s );
    ent->pushMoveInfo.remaining_distance -= frames * ent->pushMoveInfo.speed * gi.frame_time_s;
    ent->nextthink = level.time + ( FRAME_TIME_S * frames );
    ent->think = SVG_PushMove_MoveFinal;
    //}

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
void SVG_PushMove_Think_AccelerateMove( edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_MoveCalculate( edict_t *ent, const Vector3 &destination, svg_pushmove_endcallback endMoveCallback ) {
    // Reset velocity.
    VectorClear( ent->velocity );
    // Subtract destination and origin to acquire move direction.
    VectorSubtract( destination, ent->s.origin, ent->pushMoveInfo.dir );
    // Use the normalized direction vector's length to determine the remaining move idstance.
    ent->pushMoveInfo.remaining_distance = VectorNormalize( &ent->pushMoveInfo.dir.x );
    // Setup end move callback function.
    ent->pushMoveInfo.endfunc = endMoveCallback;

    if ( ent->pushMoveInfo.speed == ent->pushMoveInfo.accel && ent->pushMoveInfo.speed == ent->pushMoveInfo.decel ) {
        // If a Team Master, engage move immediately:
        if ( level.current_entity == ( ( ent->flags & FL_TEAMSLAVE ) ? ent->teammaster : ent ) ) {
            SVG_PushMove_MoveBegin( ent );
            //} else if ( ent->targetEntities.movewith ) {
            //    SVG_PushMove_MoveBegin( ent );
            //} else {
        // Team Slaves start moving next frame:
        } else {
            ent->nextthink = level.time + FRAME_TIME_S;
            ent->think = SVG_PushMove_MoveBegin;
        }
    } else {
        // accelerative
        ent->pushMoveInfo.current_speed = 0;
        ent->think = SVG_PushMove_Think_AccelerateMove;
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
void SVG_PushMove_AngleMoveDone( edict_t *ent ) {
    VectorClear( ent->avelocity );
    ent->pushMoveInfo.endfunc( ent );
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveFinal( edict_t *ent ) {
    vec3_t  move;

    if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP ) {
        VectorSubtract( ent->pushMoveInfo.end_angles, ent->s.angles, move );
    } else {
        VectorSubtract( ent->pushMoveInfo.start_angles, ent->s.angles, move );
    }

    if ( VectorEmpty( move ) ) {
        SVG_PushMove_AngleMoveDone( ent );
        return;
    }

    VectorScale( move, 1.0f / FRAMETIME, ent->avelocity );

    ent->think = SVG_PushMove_AngleMoveDone;
    ent->nextthink = level.time + FRAME_TIME_S;
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveBegin( edict_t *ent ) {
    vec3_t  destdelta;
    float   len;
    float   traveltime;
    float   frames;

    // set destdelta to the vector needed to move
    if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP ) {
        VectorSubtract( ent->pushMoveInfo.end_angles, ent->s.angles, destdelta );
    } else {
        VectorSubtract( ent->pushMoveInfo.start_angles, ent->s.angles, destdelta );
    }

    // calculate length of vector
    len = VectorLength( destdelta );

    // divide by speed to get time to reach dest
    traveltime = len / ent->pushMoveInfo.speed;

    if ( traveltime < gi.frame_time_s ) {
        SVG_PushMove_AngleMoveFinal( ent );
        return;
    }

    frames = floor( traveltime / gi.frame_time_s );

    // scale the destdelta vector by the time spent traveling to get velocity
    VectorScale( destdelta, 1.0f / traveltime, ent->avelocity );

    // PGM
    //  if we're done accelerating, act as a normal rotation
    if ( ent->pushMoveInfo.speed >= ent->speed ) {
        // set nextthink to trigger a think when dest is reached
        ent->nextthink = level.time + ( FRAME_TIME_S * frames );
        ent->think = SVG_PushMove_AngleMoveFinal;
    } else {
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = SVG_PushMove_AngleMoveBegin;
    }
    // PGM
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveCalculate( edict_t *ent, void( *func )( edict_t * ) ) {
    VectorClear( ent->avelocity );
    ent->pushMoveInfo.endfunc = func;
    if ( level.current_entity == ( ( ent->flags & FL_TEAMSLAVE ) ? ent->teammaster : ent ) ) {
        SVG_PushMove_AngleMoveBegin( ent );
    } else {
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = SVG_PushMove_AngleMoveBegin;
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
SVG_PushMove_Think_AccelerateMove

The team has completed a frame of movement, so
change the speed for the next frame
==============
*/
/**
*   @brief
**/
static constexpr float AccelerationDistance( float target, float rate ) {
    return ( target * ( ( target / rate ) + 1 ) / 2 );
}
/**
*   @brief
**/
static void PushMove_CalculateAcceleratedMove( g_pushmove_info_t *moveinfo ) {
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
void PushMove_Accelerate( g_pushmove_info_t *moveinfo ) {
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
*   @brief  The team has completed a frame of movement, so calculate
*			the speed required for a move during the next game frame.
**/
void SVG_PushMove_Think_AccelerateMove( edict_t *ent ) {
    ent->pushMoveInfo.remaining_distance -= ent->pushMoveInfo.current_speed;

    if ( ent->pushMoveInfo.current_speed == 0 ) {      // starting or blocked
        PushMove_CalculateAcceleratedMove( &ent->pushMoveInfo );
    }

    PushMove_Accelerate( &ent->pushMoveInfo );

    // will the entire move complete on next frame?
    if ( ent->pushMoveInfo.remaining_distance <= ent->pushMoveInfo.current_speed ) {
        SVG_PushMove_MoveFinal( ent );
        return;
    }

    VectorScale( ent->pushMoveInfo.dir, ent->pushMoveInfo.current_speed * 10, ent->velocity );

    ent->nextthink = level.time + 10_hz;
    ent->think = SVG_PushMove_Think_AccelerateMove;

    // Find entities that move along with this entity.
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}