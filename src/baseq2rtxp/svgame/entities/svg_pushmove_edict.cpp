/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"

#include "svgame/entities/svg_pushmove_edict.h"
#include "svgame/entities/svg_entities_pushermove.h"




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
*	@brief  Stub.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_pushmove_edict_t, onCloseEndMove )( svg_pushmove_edict_t *self ) -> void { }
/**
*	@brief  Stub.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_pushmove_edict_t, onOpenEndMove )( svg_pushmove_edict_t *self ) -> void { }


/**
*
*
*
*   Non-Rotating Movement:
*
*
*
**/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveBegin )( svg_pushmove_edict_t *ent ) -> void {
    if ( ( ent->pushMoveInfo.speed * gi.frame_time_s ) >= ent->pushMoveInfo.remaining_distance ) {
        ent->onThink_MoveFinal( ent );
        return;
    }
    // Recalculate velocity into current direction.
    ent->velocity = ent->pushMoveInfo.dir * ent->pushMoveInfo.speed;

    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //    ent->pushMoveInfo.remaining_distance -= ent->pushMoveInfo.speed * gi.frame_time_s;
    //    ent->nextthink = level.time + FRAME_TIME_S;
    //    ent->think = SVG_PushMove_MoveBegin;
    //} else {
    const float frames = floor( ( ent->pushMoveInfo.remaining_distance / ent->pushMoveInfo.speed ) / gi.frame_time_s );
    ent->pushMoveInfo.remaining_distance -= frames * ent->pushMoveInfo.speed * gi.frame_time_s;
    ent->nextthink = level.time + ( FRAME_TIME_S * frames );
    ent->SetThinkCallback( &ent->onThink_MoveFinal );
    //}

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveDone )( svg_pushmove_edict_t *ent ) -> void {
    // WID: MoveWith: Clear last velocity also.
    ent->velocity = {};
    // Fire the endMoveCallback.
    if ( ent->pushMoveInfo.endMoveCallback ) {
        ent->pushMoveInfo.endMoveCallback( ent );
    }

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveFinal )( svg_pushmove_edict_t *ent ) -> void {
    if ( ent->pushMoveInfo.remaining_distance == 0 ) {
        ent->onThink_MoveDone( ent );
        return;
    }

    // [Paril-KEX] use exact remaining distance
    ent->velocity = ( ent->pushMoveInfo.dest - ent->s.origin ) * ( 1.f / gi.frame_time_s );
    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //}

    ent->SetThinkCallback( &ent->onThink_MoveDone );
    ent->nextthink = level.time + FRAME_TIME_S;
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}



/**
*
*
*
*   Angular Rotational Movement:
*
*
*
**/
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_AngleMoveDone )( svg_pushmove_edict_t *ent ) -> void {
    // Clear angular velocity.
    ent->avelocity = {};
    // Dispatch end move callback.
    ent->pushMoveInfo.endMoveCallback( ent );
}
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_AngleMoveFinal )( svg_pushmove_edict_t *ent ) -> void {
    Vector3  move = {};

    // set destdelta to the vector needed to move
    if ( /*ent->pushMoveInfo.state == PUSHMOVE_STATE_TOP || */ ent->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP ) {
        move = ent->pushMoveInfo.endAngles - ent->s.angles;
    } else {
        move = ent->pushMoveInfo.startAngles - ent->s.angles;
    }

    // Proceed to end of movement process if we got no 'move' left to make.
    if ( VectorEmpty( move ) ) {
        ent->onThink_AngleMoveDone( ent );
        return;
    }

    // Set angular velocity to the final move vector, for its last move.
    ent->avelocity = QM_Vector3Scale( move, ( 1.0 / FRAMETIME ) ); /** ent->pushMoveInfo.sign*/

    // Set next frame think callback to be that of the end of movement processing.
    ent->SetThinkCallback( &ent->onThink_AngleMoveDone );
    ent->nextthink = level.time + FRAME_TIME_S;
}
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_AngleMoveBegin )( svg_pushmove_edict_t *ent ) -> void {
    Vector3 destinationDelta = {};

    // PGM
    // accelerate as needed
    if ( ent->pushMoveInfo.speed < ent->speed ) {
        ent->pushMoveInfo.speed += ent->accel;
        if ( ent->pushMoveInfo.speed > ent->speed ) {
            ent->pushMoveInfo.speed = ent->speed;
        }
    }
    // PGM

    // set destdelta to the vector needed to move
    if (/* ent->pushMoveInfo.state == PUSHMOVE_STATE_TOP || */ ent->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP ) {
        destinationDelta = ent->pushMoveInfo.endAngles - ent->s.angles;
    } else {
        destinationDelta = ent->pushMoveInfo.startAngles - ent->s.angles;
    }


    // Calculate length of vector
    const double len = QM_Vector3Length( destinationDelta );
    // Calculate its travel time.
    const double travelTime = len / ent->pushMoveInfo.speed;

    // Finish if we're already at the end.
    if ( travelTime < gi.frame_time_s ) {
        ent->onThink_AngleMoveFinal( ent );
        return;
    }

    // Calculate number of remaining frames.
    const double numFrames = floor( travelTime / gi.frame_time_s );
    // Scale the destdelta vector by the time spent traveling to get velocity
    ent->avelocity = QM_Vector3Scale( destinationDelta, ( 1.0 / travelTime ) );

    // PGM
    // If we're done accelerating, act as a normal rotation.
    if ( ent->pushMoveInfo.speed >= ent->speed ) {
        // set nextthink to trigger a think when dest is reached
        ent->nextthink = level.time + ( FRAME_TIME_S * numFrames );
        ent->SetThinkCallback( &ent->onThink_AngleMoveFinal );
        // Otherwise, keep on accelerating:
    } else {
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->SetThinkCallback( &ent->onThink_AngleMoveBegin );
    }
    // PGM
}
/**
*   @brief
**/
void svg_pushmove_edict_t::CalculateAngularMove( svg_pushmove_endcallback endMoveCallback ) {
    // Clear angular velocity.
    avelocity = QM_Vector3Zero();
    // Set function pointer for end position callback.
    pushMoveInfo.endMoveCallback = endMoveCallback;

    // PGM
//  if we're supposed to accelerate, this will tell anglemove_begin to do so
    if ( accel != speed ) {
        pushMoveInfo.speed = 0;
    }
    // PGM
    // 
    // If the current level entity that is being processed, happens to be in front of the
    // entity array queue AND is a teamslave, begin moving its team master instead.
    if ( level.current_entity == ( ( flags & FL_TEAMSLAVE ) ? teammaster : this ) ) {
        this->onThink_AngleMoveBegin( this );
        // Team Slaves start moving next frame:
    } else {
        nextthink = level.time + FRAME_TIME_S;
        SetThinkCallback( &this->onThink_AngleMoveBegin );
    }
}



/**
*
*
*
*   Support Routines:
*
*
*
**/
/**
*   @brief  Processes movement when at top speed, so there is no acceleration present anymore.
**/
void svg_pushmove_edict_t::SVG_PushMove_MoveRegular( const Vector3 &destination, svg_pushmove_endcallback endMoveCallback ) {
    // If the current level entity that is being processed, happens to be in front of the
    // entity array queue AND is a teamslave, begin moving its team master instead.
    if ( level.current_entity == ( ( flags & FL_TEAMSLAVE ) ? teammaster : this ) ) {
        this->onThink_MoveBegin( this );
        //} else if ( ent->targetEntities.movewith ) {
        //    SVG_PushMove_MoveBegin( ent );
        //} else {
    // Team Slaves start moving next frame:
    } else {
        nextthink = level.time + FRAME_TIME_S;
        SetThinkCallback( &this->onThink_MoveBegin );
    }
}
/**
*   @brief
**/
void svg_pushmove_edict_t::CalculateDirectionalMove( const Vector3 &destination, svg_pushmove_endcallback endMoveCallback ) {
    // Reset velocity.
    VectorClear( velocity );
    // Assign new destination.
    pushMoveInfo.dest = destination;
    // Subtract destination and origin to acquire move direction.
    VectorSubtract( destination, s.origin, pushMoveInfo.dir );
    // Use the normalized direction vector's length to determine the remaining move idstance.
    pushMoveInfo.remaining_distance = VectorNormalize( &pushMoveInfo.dir.x );
    // Setup end move callback function.
    pushMoveInfo.endMoveCallback = endMoveCallback;

    // Non Accelerating Movement:
    if ( pushMoveInfo.speed == pushMoveInfo.accel && pushMoveInfo.speed == pushMoveInfo.decel ) {
        this->SVG_PushMove_MoveRegular( destination, endMoveCallback );
        // Accelerative Movement: We're still accelerating up/down:
    } else {
        pushMoveInfo.current_speed = 0;
        // Set think time.
        nextthink = level.time + FRAME_TIME_S;

        // Regular accelerate_think for 10hz.
        if ( gi.tick_rate == 10 ) {
            SetThinkCallback( &this->SVG_PushMove_Think_AccelerateMove );
            // Specialized accelerate > 10hz.
        } else {
            // [Paril-KEX] rewritten to work better at higher tickrates
            pushMoveInfo.curve.frame = 0;
            pushMoveInfo.curve.numberSubFrames = ( 0.1 / gi.frame_time_s ) - 1;

            float total_dist = pushMoveInfo.remaining_distance;

            std::vector<float> distances;

            if ( pushMoveInfo.curve.numberSubFrames ) {
                distances.push_back( 0 );
                pushMoveInfo.curve.frame = 1;
            } else {
                pushMoveInfo.curve.frame = 0;
            }

            // simulate 10hz movement
            while ( pushMoveInfo.remaining_distance ) {
                if ( !Think_AccelMove_MoveInfo() ) {
                    break;
                }

                pushMoveInfo.remaining_distance -= pushMoveInfo.current_speed;
                distances.push_back( total_dist - pushMoveInfo.remaining_distance );
            }

            if ( pushMoveInfo.curve.numberSubFrames ) {
                distances.push_back( total_dist );
            }

            pushMoveInfo.curve.subFrame = 0;
            pushMoveInfo.curve.referenceOrigin = s.origin;
            pushMoveInfo.curve.countPositions = distances.size();

            // Q2RE: We dun have this kinda stuff 'yet'.
            // Second time around, we'll be reallocating it instead.
            pushMoveInfo.curve.positions.release();
            allocate_qtag_memory<float, TAG_SVGAME_LEVEL>( &pushMoveInfo.curve.positions, pushMoveInfo.curve.countPositions );
            // Copy in the actual distances.
            std::copy( distances.begin(), distances.end(), pushMoveInfo.curve.positions.ptr );

            pushMoveInfo.curve.numberFramesDone = 0;
            SetThinkCallback( &this->SVG_PushMove_Think_AccelerateMoveNew );
        }
    }
}

/**
*   @brief
**/
//void PushMove_CalculateAcceleratedMove( svg_pushmove_info_t *moveinfo );
//void PushMove_Accelerate( svg_pushmove_info_t *moveinfo );
const bool svg_pushmove_edict_t::Think_AccelMove_MoveInfo() {
    if ( pushMoveInfo.current_speed == 0 )		// starting or blocked
        PushMove_CalculateAcceleratedMove();

    PushMove_Accelerate();

    // will the entire move complete on next frame?
    return pushMoveInfo.remaining_distance > pushMoveInfo.current_speed;
}



/**
*   @brief
**/
constexpr double svg_pushmove_edict_t::AccelerationDistance( const double target, const double rate ) {
    return ( target * ( ( target / rate ) + 1 ) / 2 );
}
/**
*   @brief
**/
void svg_pushmove_edict_t::PushMove_CalculateAcceleratedMove() {
    double accel_dist;
    double decel_dist;

    pushMoveInfo.move_speed = pushMoveInfo.speed;

    if ( pushMoveInfo.remaining_distance < pushMoveInfo.accel ) {
        pushMoveInfo.current_speed = pushMoveInfo.remaining_distance;
        return;
    }

    accel_dist = AccelerationDistance( pushMoveInfo.speed, pushMoveInfo.accel );
    decel_dist = AccelerationDistance( pushMoveInfo.speed, pushMoveInfo.decel );

    if ( ( pushMoveInfo.remaining_distance - accel_dist - decel_dist ) < 0 ) {
        double f;

        f = ( pushMoveInfo.accel + pushMoveInfo.decel ) / ( pushMoveInfo.accel * pushMoveInfo.decel );
        pushMoveInfo.move_speed = ( -2 + sqrt( 4 - 4 * f * ( -2 * pushMoveInfo.remaining_distance ) ) ) / ( 2 * f );
        decel_dist = AccelerationDistance( pushMoveInfo.move_speed, pushMoveInfo.decel );
    }

    pushMoveInfo.decel_distance = decel_dist;
}
/**
*   @brief
**/
void svg_pushmove_edict_t::PushMove_Accelerate() {
    // are we decelerating?
    if ( pushMoveInfo.remaining_distance <= pushMoveInfo.decel_distance ) {
        if ( pushMoveInfo.remaining_distance < pushMoveInfo.decel_distance ) {
            if ( pushMoveInfo.next_speed ) {
                pushMoveInfo.current_speed = pushMoveInfo.next_speed;
                pushMoveInfo.next_speed = 0;
                return;
            }
            if ( pushMoveInfo.current_speed > pushMoveInfo.decel ) {
                pushMoveInfo.current_speed -= pushMoveInfo.decel;
            }
        }
        return;
    }

    // are we at full speed and need to start decelerating during this move?
    if ( pushMoveInfo.current_speed == pushMoveInfo.move_speed )
        if ( ( pushMoveInfo.remaining_distance - pushMoveInfo.current_speed ) < pushMoveInfo.decel_distance ) {
            float   p1_distance;
            float   p2_distance;
            float   distance;

            p1_distance = pushMoveInfo.remaining_distance - pushMoveInfo.decel_distance;
            p2_distance = pushMoveInfo.move_speed * ( 1.0 - ( p1_distance / pushMoveInfo.move_speed ) );
            distance = p1_distance + p2_distance;
            pushMoveInfo.current_speed = pushMoveInfo.move_speed;
            pushMoveInfo.next_speed = pushMoveInfo.move_speed - pushMoveInfo.decel * ( p2_distance / distance );
            return;
        }

    // are we accelerating?
    if ( pushMoveInfo.current_speed < pushMoveInfo.speed ) {
        float   old_speed;
        float   p1_distance;
        float   p1_speed;
        float   p2_distance;
        float   distance;

        old_speed = pushMoveInfo.current_speed;

        // figure simple acceleration up to move_speed
        pushMoveInfo.current_speed += pushMoveInfo.accel;
        if ( pushMoveInfo.current_speed > pushMoveInfo.speed ) {
            pushMoveInfo.current_speed = pushMoveInfo.speed;
        }

        // are we accelerating throughout this entire move?
        if ( ( pushMoveInfo.remaining_distance - pushMoveInfo.current_speed ) >= pushMoveInfo.decel_distance ) {
            return;
        }

        // during this move we will accelrate from current_speed to move_speed
        // and cross over the decel_distance; figure the average speed for the
        // entire move
        p1_distance = pushMoveInfo.remaining_distance - pushMoveInfo.decel_distance;
        p1_speed = ( old_speed + pushMoveInfo.move_speed ) / 2.0;
        p2_distance = pushMoveInfo.move_speed * ( 1.0 - ( p1_distance / p1_speed ) );
        distance = p1_distance + p2_distance;
        pushMoveInfo.current_speed = ( p1_speed * ( p1_distance / distance ) ) + ( pushMoveInfo.move_speed * ( p2_distance / distance ) );
        pushMoveInfo.next_speed = pushMoveInfo.move_speed - pushMoveInfo.decel * ( p2_distance / distance );
        return;
    }

    // we are at constant velocity (move_speed)
    return;
}
/**
*	@brief	Readjust speeds so that teamed movers start/end synchronized.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_CalculateMoveSpeed )( svg_pushmove_edict_t *self ) -> void {
    if ( self->flags & FL_TEAMSLAVE ) {
        return;     // only the team master does this
    }

    // find the smallest distance any member of the team will be moving
    double dist = 0.;
    double minDist = fabs( self->pushMoveInfo.distance );
    for ( svg_base_edict_t *ent = self->teamchain; ent; ent = ent->teamchain ) {
        dist = fabs( ent->pushMoveInfo.distance );
        if ( dist < minDist ) {
            minDist = dist;
        }
    }

    double time = minDist / self->pushMoveInfo.speed;

    // adjust speeds so they will all complete at the same time
    for ( svg_base_edict_t *ent = self; ent; ent = ent->teamchain ) {
        const double newspeed = fabs( ent->pushMoveInfo.distance ) / time;
        const double ratio = newspeed / ent->pushMoveInfo.speed;
        if ( ent->pushMoveInfo.accel == ent->pushMoveInfo.speed ) {
            ent->pushMoveInfo.accel = newspeed;
        } else {
            ent->pushMoveInfo.accel *= ratio;
        }
        if ( ent->pushMoveInfo.decel == ent->pushMoveInfo.speed ) {
            ent->pushMoveInfo.decel = newspeed;
        } else {
            ent->pushMoveInfo.decel *= ratio;
        }
        ent->pushMoveInfo.speed = newspeed;
    }
}
/**
*   @brief  The team has completed a frame of movement, so calculate
*			the speed required for a move during the next game frame.
*   @note   This is the VANILLA for 10hz movement acceleration procedure.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_AccelerateMove )( svg_pushmove_edict_t *self ) -> void {
    self->pushMoveInfo.remaining_distance -= self->pushMoveInfo.current_speed;

    if ( self->pushMoveInfo.current_speed == 0 ) {      // starting or blocked
        self->PushMove_CalculateAcceleratedMove();
    }

    self->PushMove_Accelerate();

    // will the entire move complete on next frame?
    if ( self->pushMoveInfo.remaining_distance <= self->pushMoveInfo.current_speed ) {
        self->onThink_MoveFinal( self );
        return;
    }

    VectorScale( self->pushMoveInfo.dir, self->pushMoveInfo.current_speed * 10., self->velocity );

    self->nextthink = level.time + 10_hz;
    self->SetThinkCallback( &self->SVG_PushMove_Think_AccelerateMove );

    // Find entities that move along with this entity.
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}

/**
*   @brief  The team has completed a frame of movement, so calculate
*			the speed required for a move during the next game frame.
*   @note   This is the RERELEASE for 40hz and higher movement acceleration procedure.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_AccelerateMoveNew )( svg_pushmove_edict_t *self ) -> void {
    double t = 0.;
    double target_dist;

    if ( self->pushMoveInfo.curve.numberSubFrames ) {
        if ( self->pushMoveInfo.curve.subFrame == self->pushMoveInfo.curve.numberSubFrames + 1 ) {
            self->pushMoveInfo.curve.subFrame = 0;
            self->pushMoveInfo.curve.frame++;

            if ( self->pushMoveInfo.curve.frame == self->pushMoveInfo.curve.countPositions ) {
                self->onThink_MoveFinal( self );
                return;
            }
        }

        t = ( self->pushMoveInfo.curve.subFrame + 1 ) / ( (float)self->pushMoveInfo.curve.numberSubFrames + 1 );

        // Prevent going < 0 for position index.
        int32_t frame_index = ( self->pushMoveInfo.curve.frame >= 1 ? self->pushMoveInfo.curve.frame - 1 : 0 );
        target_dist = QM_Lerp<double>( (double)self->pushMoveInfo.curve.positions[ frame_index ], (double)self->pushMoveInfo.curve.positions[ self->pushMoveInfo.curve.frame ], (double)t );
        self->pushMoveInfo.curve.subFrame++;
    } else {
        if ( self->pushMoveInfo.curve.frame == self->pushMoveInfo.curve.countPositions ) {
            //Move_Final( ent );
            self->onThink_MoveFinal( self );
            return;
        }

        target_dist = self->pushMoveInfo.curve.positions[ self->pushMoveInfo.curve.frame++ ];
    }

    self->pushMoveInfo.curve.numberFramesDone++;
    Vector3 target_pos = self->pushMoveInfo.curve.referenceOrigin + ( self->pushMoveInfo.dir * target_dist );
    self->velocity = ( target_pos - self->s.origin ) * ( 1. / gi.frame_time_s );
    self->nextthink = level.time + FRAME_TIME_S;
}

//! Stub.
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink )( svg_pushmove_edict_t *ent ) -> void { }
//! Stub.
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_OpenMove )( svg_pushmove_edict_t *ent ) -> void { }
//! Stub.
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_CloseMove )( svg_pushmove_edict_t *ent ) -> void { }