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
#include "svgame/svg_utils.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/svg_pushmove_edict.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"



/**
*
*
*
*   Lock/UnLock:
*
*
*
**/
/**
*   @brief  Set button lock state.
*   @param  isLocked
*           If true: Locks the button, thereby disabling it from being able to change state.
*           If false: UnLocks the button, thereby enabling it from being able to change state.
**/
void svg_pushmove_edict_t::SetLockState( const bool lockButton ) {
    if ( lockButton ) {
        // Last but not least, lock its state.
        pushMoveInfo.lockState.isLocked = true;
    } else {
        // Last but not least, unlock its state.
        pushMoveInfo.lockState.isLocked = false;
    }
}



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
void svg_pushmove_edict_t::SVG_PushMove_MoveCalculate( const Vector3 &destination, svg_pushmove_endcallback endMoveCallback ) {
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
const bool svg_pushmove_edict_t::Think_AccelMove_MoveInfo( ) {
    if ( pushMoveInfo.current_speed == 0 )		// starting or blocked
        PushMove_CalculateAcceleratedMove( );

    PushMove_Accelerate( );

    // will the entire move complete on next frame?
    return pushMoveInfo.remaining_distance > pushMoveInfo.current_speed;
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
void SVG_PushMove_AngleMoveDone( svg_pushmove_edict_t *ent ) {
    // Clear angular velocity.
    ent->avelocity = {};
    // Dispatch end move callback.
    ent->pushMoveInfo.endMoveCallback( ent );
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveFinal( svg_pushmove_edict_t *ent ) {
    Vector3  move = {};

    // set destdelta to the vector needed to move
    if ( /*ent->pushMoveInfo.state == PUSHMOVE_STATE_TOP || */ ent->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP ) {
        move = ent->pushMoveInfo.endAngles - ent->s.angles;
    } else {
        move = ent->pushMoveInfo.startAngles - ent->s.angles;
    }

    // Proceed to end of movement process if we got no 'move' left to make.
    if ( VectorEmpty( move ) ) {
        SVG_PushMove_AngleMoveDone( ent );
        return;
    }

    // Set angular velocity to the final move vector, for its last move.
    ent->avelocity = QM_Vector3Scale( move, ( 1.0 / FRAMETIME ) ); /** ent->pushMoveInfo.sign*/

    // Set next frame think callback to be that of the end of movement processing.
    ent->SetThinkCallback( &SVG_PushMove_AngleMoveDone );
    ent->nextthink = level.time + FRAME_TIME_S;
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveBegin( svg_pushmove_edict_t *ent ) {
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
        SVG_PushMove_AngleMoveFinal( ent );
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
        ent->SetThinkCallback( &SVG_PushMove_AngleMoveFinal );
    // Otherwise, keep on accelerating:
    } else {
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->SetThinkCallback( &SVG_PushMove_AngleMoveBegin );
    }
    // PGM
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveCalculate( svg_pushmove_edict_t *ent, svg_pushmove_endcallback endMoveCallback ) {
    // Clear angular velocity.
    ent->avelocity = QM_Vector3Zero();
    // Set function pointer for end position callback.
    ent->pushMoveInfo.endMoveCallback = endMoveCallback;
     
    // PGM
//  if we're supposed to accelerate, this will tell anglemove_begin to do so
    if ( ent->accel != ent->speed ) {
        ent->pushMoveInfo.speed = 0;
    }
    // PGM
    // 
    // If the current level entity that is being processed, happens to be in front of the
    // entity array queue AND is a teamslave, begin moving its team master instead.
    if ( level.current_entity == ( ( ent->flags & FL_TEAMSLAVE ) ? ent->teammaster : ent ) ) {
        SVG_PushMove_AngleMoveBegin( ent );
    // Team Slaves start moving next frame:
    } else {
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->SetThinkCallback( &SVG_PushMove_AngleMoveBegin );
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
constexpr double svg_pushmove_edict_t::AccelerationDistance( const double target, const double rate ) {
    return ( target * ( ( target / rate ) + 1 ) / 2 );
}
/**
*   @brief
**/
void svg_pushmove_edict_t::PushMove_CalculateAcceleratedMove( ) {
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
void svg_pushmove_edict_t::PushMove_Accelerate( ) {
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
        self->PushMove_CalculateAcceleratedMove( );
    }

    self->PushMove_Accelerate( );

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



/**
*
*
*
*   MoveWith Entities:
*
*
**/
/**
*   @brief  Reposition 'moveWith' entities to their parent entity its made movement.
**/
void SVG_PushMove_UpdateMoveWithEntities() {
    //
    // Readjust specific mover entities if necessary.
    //
    for ( int32_t i = 0; i < game.num_movewithEntityStates; i++ ) {
        // Parent mover.
        svg_base_edict_t *parentMover = nullptr;
        if ( game.moveWithEntities[ i ].parentNumber > 0 && game.moveWithEntities[ i ].parentNumber < MAX_EDICTS ) {
            parentMover = g_edict_pool.EdictForNumber( game.moveWithEntities[ i ].parentNumber );//&g_edicts[ game.moveWithEntities[ i ].parentNumber ];
        }

        if ( parentMover && parentMover->inuse && ( parentMover->movetype == MOVETYPE_PUSH || parentMover->movetype == MOVETYPE_STOP ) ) {
            Vector3 parentAnglesDelta = parentMover->s.angles - parentMover->lastAngles;
            Vector3 parentOriginDelta = parentMover->s.origin - parentMover->lastOrigin;

            Vector3 parentVForward, parentVRight, parentVUp;
            QM_AngleVectors( parentAnglesDelta, &parentVForward, &parentVRight, &parentVUp );
            //parentVRight = QM_Vector3Negate( parentVRight );

            //        Vector3 delta_angles, forward, right, up;
            //        VectorSubtract( moveWithEntity->s.angles, ent->moveWith.originAnglesOffset, delta_angles );
            //        AngleVectors( delta_angles, forward, right, up );
            //        VectorNegate( right, right );

            // Iterate to find the child movers to adjust to parent.
            for ( int32_t j = 0; j < game.num_movewithEntityStates; j++ ) {
                // Child mover.
                svg_base_edict_t *childMover = nullptr;
                if ( game.moveWithEntities[ j ].parentNumber == parentMover->s.number &&
                    game.moveWithEntities[ j ].childNumber > 0 && game.moveWithEntities[ j ].childNumber < MAX_EDICTS ) {
                    //childMover = &g_edicts[ game.moveWithEntities[ j ].childNumber ];
                    childMover = g_edict_pool.EdictForNumber( game.moveWithEntities[ j ].childNumber );
                }
                if ( childMover && childMover->inuse 
                    && ( 
                        //// Allow Triggers, 
                        //childMover->solid == SOLID_TRIGGER ||
                        // And Pushers.
                        childMover->movetype == MOVETYPE_PUSH || childMover->movetype == MOVETYPE_STOP ) ) {
                    SVG_MoveWith_AdjustToParent(
                        parentOriginDelta, parentAnglesDelta,
                        parentVUp, parentVRight, parentVForward,
                        parentMover, childMover
                    );
                }
            }
        }
    }
}