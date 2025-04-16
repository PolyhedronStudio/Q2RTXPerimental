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
void SVG_PushMove_MoveDone( svg_base_edict_t *ent ) {
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
void SVG_PushMove_MoveFinal( svg_base_edict_t *ent ) {
    if ( ent->pushMoveInfo.remaining_distance == 0 ) {
        SVG_PushMove_MoveDone( ent );
        return;
    }

    // [Paril-KEX] use exact remaining distance
    ent->velocity = ( ent->pushMoveInfo.dest - ent->s.origin ) * ( 1.f / gi.frame_time_s );
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
void SVG_PushMove_MoveBegin( svg_base_edict_t *ent ) {
    if ( ( ent->pushMoveInfo.speed * gi.frame_time_s ) >= ent->pushMoveInfo.remaining_distance ) {
        SVG_PushMove_MoveFinal( ent );
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
    ent->think = SVG_PushMove_MoveFinal;
    //}

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}

/**
*   @brief
**/
void SVG_PushMove_MoveRegular( svg_base_edict_t *ent, const Vector3 &destination, svg_pushmove_endcallback endMoveCallback ) {
    // If the current level entity that is being processed, happens to be in front of the
    // entity array queue AND is a teamslave, begin moving its team master instead.
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
}
/**
*   @brief
**/
void PushMove_CalculateAcceleratedMove( svg_pushmove_info_t *moveinfo );
void PushMove_Accelerate( svg_pushmove_info_t *moveinfo );
bool Think_AccelMove_MoveInfo( svg_pushmove_info_t *moveinfo ) {
    if ( moveinfo->current_speed == 0 )		// starting or blocked
        PushMove_CalculateAcceleratedMove( moveinfo );

    PushMove_Accelerate( moveinfo );

    // will the entire move complete on next frame?
    return moveinfo->remaining_distance > moveinfo->current_speed;
}
/**
*   @brief
**/
void SVG_PushMove_MoveCalculate( svg_base_edict_t *ent, const Vector3 &destination, svg_pushmove_endcallback endMoveCallback ) {
    // Reset velocity.
    VectorClear( ent->velocity );
    // Assign new destination.
    ent->pushMoveInfo.dest = destination;
    // Subtract destination and origin to acquire move direction.
    VectorSubtract( destination, ent->s.origin, ent->pushMoveInfo.dir );
    // Use the normalized direction vector's length to determine the remaining move idstance.
    ent->pushMoveInfo.remaining_distance = VectorNormalize( &ent->pushMoveInfo.dir.x );
    // Setup end move callback function.
    ent->pushMoveInfo.endMoveCallback = endMoveCallback;

    // Non Accelerating Movement:
    if ( ent->pushMoveInfo.speed == ent->pushMoveInfo.accel && ent->pushMoveInfo.speed == ent->pushMoveInfo.decel ) {
        SVG_PushMove_MoveRegular( ent, destination, endMoveCallback );
    // Accelerative Movement: We're still accelerating up/down:
    } else {
        ent->pushMoveInfo.current_speed = 0;
        // Set think time.
        ent->nextthink = level.time + FRAME_TIME_S;

        // Regular accelerate_think for 10hz.
        if ( gi.tick_rate == 10 ) {
            ent->think = SVG_PushMove_Think_AccelerateMove;
        // Specialized accelerate > 10hz.
        } else {
            // [Paril-KEX] rewritten to work better at higher tickrates
            ent->pushMoveInfo.curve.frame = 0;
            ent->pushMoveInfo.curve.numberSubFrames = ( 0.1f / gi.frame_time_s ) - 1;

            float total_dist = ent->pushMoveInfo.remaining_distance;

            std::vector<float> distances;

            if ( ent->pushMoveInfo.curve.numberSubFrames ) {
                distances.push_back( 0 );
                ent->pushMoveInfo.curve.frame = 1;
            } else
                ent->pushMoveInfo.curve.frame = 0;

            // simulate 10hz movement
            while ( ent->pushMoveInfo.remaining_distance ) {
                if ( !Think_AccelMove_MoveInfo( &ent->pushMoveInfo ) )
                    break;

                ent->pushMoveInfo.remaining_distance -= ent->pushMoveInfo.current_speed;
                distances.push_back( total_dist - ent->pushMoveInfo.remaining_distance );
            }

            if ( ent->pushMoveInfo.curve.numberSubFrames )
                distances.push_back( total_dist );

            ent->pushMoveInfo.curve.subFrame = 0;
            ent->pushMoveInfo.curve.referenceOrigin = ent->s.origin;
            ent->pushMoveInfo.curve.countPositions = distances.size();

            // Q2RE: We dun have this kinda stuff 'yet'.
            // Second time around, we'll be reallocating it instead.
            ent->pushMoveInfo.curve.positions.release();
            allocate_qtag_memory<float, TAG_SVGAME_LEVEL>( &ent->pushMoveInfo.curve.positions, ent->pushMoveInfo.curve.countPositions );
            // Copy in the actual distances.
            std::copy( distances.begin(), distances.end(), ent->pushMoveInfo.curve.positions.ptr );

            ent->pushMoveInfo.curve.numberFramesDone = 0;
            ent->think = SVG_PushMove_Think_AccelerateMoveNew;
        }
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
void SVG_PushMove_AngleMoveDone( svg_base_edict_t *ent ) {
    // Clear angular velocity.
    ent->avelocity = {};
    // Dispatch end move callback.
    ent->pushMoveInfo.endMoveCallback( ent );
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveFinal( svg_base_edict_t *ent ) {
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
    ent->avelocity = QM_Vector3Scale( move, ( 1.0f / FRAMETIME ) ); /** ent->pushMoveInfo.sign*/

    // Set next frame think callback to be that of the end of movement processing.
    ent->think = SVG_PushMove_AngleMoveDone;
    ent->nextthink = level.time + FRAME_TIME_S;
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveBegin( svg_base_edict_t *ent ) {
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
    const float len = QM_Vector3Length( destinationDelta );
    // Calculate its travel time.
    const float travelTime = len / ent->pushMoveInfo.speed;

    // Finish if we're already at the end.
    if ( travelTime < gi.frame_time_s ) {
        SVG_PushMove_AngleMoveFinal( ent );
        return;
    }

    // Calculate number of remaining frames.
    const float numFrames = floor( travelTime / gi.frame_time_s );
    // Scale the destdelta vector by the time spent traveling to get velocity
    ent->avelocity = QM_Vector3Scale( destinationDelta, ( 1.0f / travelTime ) );
     
    // PGM
    // If we're done accelerating, act as a normal rotation.
    if ( ent->pushMoveInfo.speed >= ent->speed ) {
        // set nextthink to trigger a think when dest is reached
        ent->nextthink = level.time + ( FRAME_TIME_S * numFrames );
        ent->think = SVG_PushMove_AngleMoveFinal;
    // Otherwise, keep on accelerating:
    } else {
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = SVG_PushMove_AngleMoveBegin;
    }
    // PGM
}
/**
*   @brief
**/
void SVG_PushMove_AngleMoveCalculate( svg_base_edict_t *ent, svg_pushmove_endcallback endMoveCallback ) {
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
void PushMove_CalculateAcceleratedMove( svg_pushmove_info_t *moveinfo ) {
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
void PushMove_Accelerate( svg_pushmove_info_t *moveinfo ) {
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
*	@brief	Readjust speeds so that teamed movers start/end synchronized.
**/
void SVG_PushMove_Think_CalculateMoveSpeed( svg_base_edict_t *self ) {
    svg_base_edict_t *ent;
    float   minDist;
    float   time;
    float   newspeed;
    float   ratio;
    float   dist;

    if ( self->flags & FL_TEAMSLAVE ) {
        return;     // only the team master does this
    }

    // find the smallest distance any member of the team will be moving
    minDist = fabsf( self->pushMoveInfo.distance );
    for ( ent = self->teamchain; ent; ent = ent->teamchain ) {
        dist = fabsf( ent->pushMoveInfo.distance );
        if ( dist < minDist ) {
            minDist = dist;
        }
    }

    time = minDist / self->pushMoveInfo.speed;

    // adjust speeds so they will all complete at the same time
    for ( ent = self; ent; ent = ent->teamchain ) {
        newspeed = fabsf( ent->pushMoveInfo.distance ) / time;
        ratio = newspeed / ent->pushMoveInfo.speed;
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
**/
void SVG_PushMove_Think_AccelerateMove( svg_base_edict_t *ent ) {
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

void SVG_PushMove_Think_AccelerateMoveNew( svg_base_edict_t *ent ) {
    float t = 0.f;
    float target_dist;

    if ( ent->pushMoveInfo.curve.numberSubFrames ) {
        if ( ent->pushMoveInfo.curve.subFrame == ent->pushMoveInfo.curve.numberSubFrames + 1 ) {
            ent->pushMoveInfo.curve.subFrame = 0;
            ent->pushMoveInfo.curve.frame++;

            if ( ent->pushMoveInfo.curve.frame == ent->pushMoveInfo.curve.countPositions ) {
                SVG_PushMove_MoveFinal( ent );
                return;
            }
        }

        t = ( ent->pushMoveInfo.curve.subFrame + 1 ) / ( (float)ent->pushMoveInfo.curve.numberSubFrames + 1 );

        // Prevent going < 0 for position index.
        int32_t frame_index = ( ent->pushMoveInfo.curve.frame >= 1 ? ent->pushMoveInfo.curve.frame - 1 : 0 );
        target_dist = QM_Lerp<float>( (float)ent->pushMoveInfo.curve.positions[ frame_index ], (float)ent->pushMoveInfo.curve.positions[ ent->pushMoveInfo.curve.frame ], (float)t );
        ent->pushMoveInfo.curve.subFrame++;
    } else {
        if ( ent->pushMoveInfo.curve.frame == ent->pushMoveInfo.curve.countPositions ) {
            //Move_Final( ent );
            SVG_PushMove_MoveFinal( ent );
            return;
        }

        target_dist = ent->pushMoveInfo.curve.positions[ ent->pushMoveInfo.curve.frame++ ];
    }

    ent->pushMoveInfo.curve.numberFramesDone++;
    Vector3 target_pos = ent->pushMoveInfo.curve.referenceOrigin + ( ent->pushMoveInfo.dir * target_dist );
    ent->velocity = ( target_pos - ent->s.origin ) * ( 1.f / gi.frame_time_s );
    ent->nextthink = level.time + FRAME_TIME_S;
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