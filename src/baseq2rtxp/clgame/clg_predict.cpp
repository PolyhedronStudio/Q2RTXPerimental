/********************************************************************
*
*
*	ClientGame: Player Movement Prediction Simulation Implementation.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_events.h"
#include "clgame/clg_predict.h"

#include "sharedgame/sg_entities.h"
// For PM_STEP_MIN_NORMAL
#include "sharedgame/pmove/sg_pmove_slidemove.h"



/**
*
*
*
*	Trace Utility Functions:
*
*
*
**/
/**
*	@brief	Wrapper for gi.trace that accepts Vector3 args.
**/
//static inline const cm_trace_t CLG_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, centity_t *passEdict, const cm_contents_t contentMask ) {
//    const Vector3 *_mins = ( mins == qm_vector3_null || &mins == &qm_vector3_null ) ? nullptr : &mins;
//    const Vector3 *_maxs = ( maxs == qm_vector3_null || &maxs == &qm_vector3_null ) ? nullptr : &maxs;
//    return clgi.Trace( &start, _mins, _maxs, &end, passEdict, contentMask );
//}
///**
//*	@brief	Wrapper for gi.clipthat accepts Vector3 args.
//**/
//static inline const cm_trace_t CLG_Clip( centity_t *clipEdict, const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const cm_contents_t contentMask ) {
//    const Vector3 *_mins = ( mins == qm_vector3_null || &mins == &qm_vector3_null ) ? nullptr : &mins;
//    const Vector3 *_maxs = ( maxs == qm_vector3_null || &maxs == &qm_vector3_null ) ? nullptr : &maxs;
//    return clgi.Clip( &start, _mins, _maxs, &end, clipEdict, contentMask );
//}



/**
*
*
*
*   PMove Trace Callbacks:
* 
* 
* 
**/
/**
*   @brief  Player Move specific 'Trace' wrapper implementation.
**/
static const cm_trace_t q_gameabi CLG_PM_Trace( const Vector3 *start, const Vector3 *mins, const Vector3 *maxs, const Vector3 *end, const void *passEntity, const cm_contents_t contentMask ) {
    cm_trace_t t;
    //if (pm_passent->health > 0)
    //    return gi.trace(start, mins, maxs, end, pm_passent, CM_CONTENTMASK_PLAYERSOLID);
    //else
    //    return gi.trace(start, mins, maxs, end, pm_passent, CM_CONTENTMASK_DEADSOLID);
	vec_t *_start = ( *start == qm_vector3_null || !start ) ? nullptr : (vec_t*)start;
	vec_t *_mins = ( *mins == qm_vector3_null || !mins ) ? nullptr : (vec_t *)mins;
	vec_t *_maxs = ( *maxs == qm_vector3_null || !maxs ) ? nullptr : (vec_t *)maxs;
	vec_t *_end = ( *end == qm_vector3_null || !end ) ? nullptr : (vec_t *)end;
    t = clgi.Trace( _start, _mins, _maxs, _end, (const centity_t *)passEntity, contentMask );
    return t;
}
/**
*   @brief  Player Move specific 'Clip' wrapper implementation. Clips to world only.
**/
static const cm_trace_t q_gameabi CLG_PM_Clip( const Vector3 *start, const Vector3 *mins, const Vector3 *maxs, const Vector3 *end, const cm_contents_t contentMask ) {
    vec_t *_start = ( *start == qm_vector3_null || !start ) ? nullptr : (vec_t *)start;
    vec_t *_mins = ( *mins == qm_vector3_null || !mins ) ? nullptr : (vec_t *)mins;
    vec_t *_maxs = ( *maxs == qm_vector3_null || !maxs ) ? nullptr : (vec_t *)maxs;
    vec_t *_end = ( *end == qm_vector3_null || !end ) ? nullptr : (vec_t *)end;
    const cm_trace_t trace = clgi.Clip( _start, _mins, _maxs, _end, nullptr, contentMask );
    return trace;
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const cm_contents_t q_gameabi CLG_PM_PointContents( const Vector3 *point ) {
    return clgi.PointContents( point );
}



/**
*
*
*
*   PMove Prediction:
*
*
*
**/
/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void PF_AdjustViewHeight( const int32_t viewHeight ) {
    // Record viewheight changes.
    if ( game.predictedState.transition.view.height[ 0 ] != viewHeight ) {
        game.predictedState.transition.view.height[ 1 ] = game.predictedState.transition.view.height[ 0 ];
        game.predictedState.transition.view.height[ 0 ] = viewHeight;
        game.predictedState.transition.view.timeHeightChanged = clgi.client->accumulatedRealTime;
    }
}

/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
static void CLG_CheckPlayerstateEvents( player_state_t *ops, player_state_t *ps ) {
    // WID: We don't have support for external events yet. In fact, they would in Q3 style rely on
    // 'temp entities', which are alike a normal entity. Point being is this requires too much refactoring
    // right now.
    #if 1
    if ( ps->externalEvent && ps->externalEvent != ops->externalEvent ) {
        //cent = &cg_entities[ ps->clientNum + 1 ];
        centity_t *clgent = &clg_entities[ clgi.client->frame.clientNum + 1 ];
        clgent->current.event = ps->externalEvent;
        clgent->current.eventParm0 = ps->externalEventParm0;
        clgent->current.eventParm1 = 0;// ps->externalEventParm1;
        CLG_CheckEntityEvents( clgent );
    }
    #endif

	// Get the  client entity.
    centity_t *clientEntity = clgi.client->clientEntity;

    // go through the predictable events buffer
    for ( int64_t i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++ ) {
        // If we have a new predictable event:
        if ( i >= ops->eventSequence
            // OR the server told us to play another event instead of a predicted event we already issued
            
            // OR something the server told us changed our prediction causing a different event
            || ( i > ops->eventSequence - MAX_PS_EVENTS && ps->events[ i & ( MAX_PS_EVENTS - 1 ) ] != ops->events[ i & ( MAX_PS_EVENTS - 1 ) ] ) 
        ) {
			// Get the event number.
            const int32_t playerStateEvent = ps->events[ i & ( MAX_PS_EVENTS - 1 ) ];
            // Assign to the client entity.
            clientEntity->current.event = playerStateEvent;
            clientEntity->current.eventParm0 = ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
            clientEntity->current.eventParm1 = 0;/*ps->eventParms2[ i & ( MAX_PS_EVENTS - 1 ) ];*/

            /**
            *
            *   Either join the two together...
            *   Or keep them separated?
            * 
            *   Q3 only thinks "all clients are basically an entity"....
            * 
            *   But, we got monsters as well as players. How to deal with this?
            * 
            *   Let players deal with the same event if the entity number matches to
			*   that of the view bound entity. Otherwise, let the entity events deal with it?
            *    ---> see below
			*   The problem here seems that for playing the sound we need to know the entity's origin.
			*   So, we need to know which entity to use for that.
            *
            **/
            
            // Proceed to firing the predicted/received event.
            const bool processedPlayerStateViewBoundEffect = CLG_CheckPlayerStateEvent( ops, ps, playerStateEvent, clgi.client->predictedFrame.ps.pmove.origin );
            if ( !processedPlayerStateViewBoundEffect ) {
                CLG_CheckEntityEvents( clientEntity );
            }
            // Add to the list of predictable events.
            game.predictableEvents[ i & ( game_locals_t::MAX_PREDICTED_EVENTS - 1 ) ] = playerStateEvent;
            // Increment Event Sequence.
            game.eventSequence++;
        }
    }
}

void CLG_CheckChangedPredictableEvents( player_state_t *ps ) {
    int i;
    int event;
    centity_t *cent;

    cent = clgi.client->clientEntity;// g.predictedPlayerEntity;
    for ( i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++ ) {
        //
        if ( i >= game.eventSequence ) {
            continue;
        }
        // if this event is not further back in than the maximum predictable events we remember
        if ( i > game.eventSequence - game_locals_t::MAX_PREDICTED_EVENTS ) {
            // if the new playerstate event is different from a previously predicted one
            if ( ps->events[ i & ( MAX_PS_EVENTS - 1 ) ] != game.predictableEvents[ i & ( game_locals_t::MAX_PREDICTED_EVENTS - 1 ) ] ) {

                event = ps->events[ i & ( MAX_PS_EVENTS - 1 ) ];
                cent->current.event = event;
                cent->current.eventParm0 = ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
                cent->current.eventParm1 = 0; // ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
                CLG_CheckEntityEvents( cent );

                game.predictableEvents[ i & ( game_locals_t::MAX_PREDICTED_EVENTS - 1 ) ] = event;

                //if ( cg_showmiss.integer ) {
                //    CG_Printf( "WARNING: changed predicted event\n" );
                //}
				clgi.Print( PRINT_DEVELOPER, "WARNING: changed predicted event\n" );
            }
        }
    }
}

/**
*   @brief  Will predict the bobcycle to be for the next server frame.
**/
static void CLG_PredictNextBobCycle( pmove_t *pm ) {
    // Predict next bobcycle.
    const double bobCycleFraction = clgi.client->xerpFraction;
	// Get the bobcycle from the last received server frame.
    uint8_t bobCycle = clgi.client->frame.ps.bobCycle;//pm->state->bobCycle;// nextframe->ps.bobCycle;
    // Handle wraparound:
    if ( bobCycle < pm->state->bobCycle ) {
        bobCycle += 256;
    }
    pm->state->bobCycle = pm->state->bobCycle + bobCycleFraction * ( bobCycle - clgi.client->frame.ps.bobCycle );

    //clgi.Print( PRINT_DEVELOPER, "%s: bobCycle(%i), bobCycleFraction(%f)\n", __func__, pm->state->bobCycle, bobCycleFraction );
}

/**
*   @brief  Will determine if we're stepping, and smooth out the step height in case of traversing multiple steps in a row.
**/
static void CLG_PredictStepOffset( pmove_t *pm, client_predicted_state_t *predictedState, const double stepSize ) {
    // Time in miliseconds to lerp the step with.
    static constexpr double PM_STEP_TIME = 100.;
    // Maximum -/+ change we allow in step lerps.
    static constexpr double PM_MAX_STEP_CHANGE = ( PM_STEP_MAX_SIZE * 2 ) + ( PM_STEP_MAX_SIZE );
    // Absolute change is in this limited range.
    static constexpr double STEP_MAX_FABS_HEIGHT = ( PM_STEP_MAX_SIZE + PM_STEP_GROUND_DIST);
    static constexpr double STEP_MIN_FABS_HEIGHT = ( PM_STEP_MIN_SIZE );

    // Get the absolute step's height value for testing against.
    const double fabsStep = std::fabs( stepSize );

    // Check for step.
    const bool step_detected = (
        // Consider a Z change being "stepping" if...
        ( fabsStep >= STEP_MIN_FABS_HEIGHT && fabsStep <= STEP_MAX_FABS_HEIGHT )
        // And we started off on the ground in our previously predicted.
        && ( predictedState->lastGround.entity || pm->step_clip )
        // And are still predicted to be on the ground.
        && ( ( predictedState->ground.entity ) && pm->state->pmove.pm_type <= PM_GRAPPLE )
        // Or we stand on another different ground entity. 
        && ( predictedState->ground.entity != predictedState->lastGround.entity
            // Plane memory isn't identical thus we stand on a different plane. OR.. 
            || memcmp( &predictedState->lastGround.plane, &predictedState->ground.plane, sizeof( cm_plane_t ) ) != 0 )
    );
    // Determine the transition step height.
    if ( step_detected ) {
        // Get delta time interval.
        int64_t deltaTime = ( clgi.GetRealTime() - predictedState->transition.step.timeChanged );
        // Default old step to 0.
        double oldStep = 0.;
        // We use < since we want to know if we were already stepping up. Keep things smooth that way.
        if ( deltaTime < PM_STEP_TIME ) {
            // Calculate how far we've come.
            oldStep = predictedState->transition.step.height * ( PM_STEP_TIME - deltaTime ) / PM_STEP_TIME;
        }
        // Add the stepHeight amount.
        predictedState->transition.step.height = std::clamp( oldStep + stepSize, -PM_MAX_STEP_CHANGE, PM_MAX_STEP_CHANGE );
        // Store time of change.
        predictedState->transition.step.timeChanged = clgi.GetRealTime();
        // Store the size of the stepheight.
        predictedState->transition.step.size = stepSize;
    }

}

/**
*	@return	False if prediction is not desired for. True if it is.
* 
*   @todo   Have it return flags for what to predict, and rename it to PF_CurrentPredictionFlags or something similar.
**/
const qboolean PF_UsePrediction( void ) {
    // We're playing a demo, there is nothing to predict.
    if ( clgi.IsDemoPlayback() ) {
        return false;
    }

    // When the server is paused, nothing to predict, make sure to clear any possible predicted state error.
    if ( clgi.GetConnectionState() != ca_active || sv_paused->integer ) {
        game.predictedState.error = {};
        return false;
    }

    // Prediction is disabled by user.
    if ( !cl_predict->integer ) {
        return false;
    }
    // Player state demands no positional('origin') prediction.
    if ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION ) {
        return false;
    }
    // Player state demands no angular('viewangles') prediction.
    if ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) {
        // TODO: 
        return false;
    }
    
    // We want predicting.
    return true;
}

/**
*   @brief  Checks for prediction if desired. Will determine the error margin
*           between our predicted state and the server returned state. In case
*           the margin is too high, snap back to server provided player state.
**/
void PF_CheckPredictionError( const int64_t frameIndex, const int64_t commandIndex, struct client_movecmd_s *moveCommand ) {
    // Maximum delta allowed before snapping back. (80 units at 40hz)
    static constexpr double MAX_DELTA_ORIGIN = ( 3200. * ( 1.0 / BASE_FRAMERATE ) );

    // If it is the first frame, we got nothing to predict yet.
    if ( moveCommand->prediction.simulationTime == 0 ) {
        game.predictedState.lastPs = clgi.client->frame.ps;
        game.predictedState.currentPs = clgi.client->frame.ps;
        //game.predictedState.currentPs.pmove = game.predictedState.currentPs.pmove;
        game.predictedState.error = {};
        game.predictedState.origin = clgi.client->frame.ps.pmove.origin; // Store the server returned origin for this command index.

        game.predictedState.transition = {
            .step = {
                .height = 0,
                .timeChanged = clgi.GetRealTime()
            },
            .view = {
                .height = {
                    (float)clgi.client->frame.ps.pmove.viewheight,
                    (float)clgi.client->frame.ps.pmove.viewheight
                },
                .timeHeightChanged = (uint64_t)clgi.client->accumulatedRealTime
            }
        };

        clgi.ShowMiss( "First frame(%" PRIi64 "). Nothing to predict yet.\n",
            clgi.client->frame.number );
        return;
    }

	client_movecmd_prediction_t *predictedMoveResult = &clgi.client->predictedMoveResults[ (commandIndex)&CMD_MASK ]; // Get the predicted move results for this command index.
    // Subtract what the server returned from our predicted origin for that frame.
    //game.predictedState.error = moveCommand->prediction.error = moveCommand->prediction.origin - clgi.client->frame.ps.pmove.origin;
    game.predictedState.error = predictedMoveResult->error = moveCommand->prediction.error = clgi.client->frame.ps.pmove.origin - predictedMoveResult->origin;

    // Save the prediction error for interpolation.
    //const float len = fabs( delta[ 0 ] ) + abs( delta[ 1 ] ) + abs( delta[ 2 ] );
    const float len = fabs( QM_Vector3Length( game.predictedState.error ) );
    //if (len < 1 || len > 640) {
    if ( len > .1f 
        && game.predictedState.cmd.prediction.simulationTime == moveCommand->prediction.simulationTime ) {
        // Snap back if the distance was too far off:
        if ( len > MAX_DELTA_ORIGIN ) {
            // Debug misses:
            clgi.ShowMiss( "MAX_DELTA_ORIGIN on frame #(%" PRIi64 "): len(%f) (%f %f %f)\n",
                clgi.client->frame.number, len, game.predictedState.error[ 0 ], game.predictedState.error[ 1 ], game.predictedState.error[ 2 ] );

            game.predictedState.lastPs = clgi.client->frame.ps;
            game.predictedState.currentPs = clgi.client->frame.ps;
            //game.predictedState.currentPs.pmove = clgi.client->predictedFrame.ps.pmove;
            game.predictedState.error = {};
			game.predictedState.origin = clgi.client->frame.ps.pmove.origin; // Store the server returned origin for this command index.

            game.predictedState.transition = {
                .step = {
                    .height = 0,
                    .timeChanged = clgi.GetRealTime()
                },
                .view = {
                    .height = {
                        (float)clgi.client->frame.ps.pmove.viewheight,
                        (float)clgi.client->frame.ps.pmove.viewheight
                    },
                    .timeHeightChanged = (uint64_t)clgi.client->accumulatedRealTime
                }
            };

            return;
        // In case of a minor distance, when cl_showmiss is enabled, report:
        } else {
            // Debug misses:
            clgi.ShowMiss( "Prediction miss on frame #(%" PRIi64 "): len(%f) (%f %f %f)\n",
                clgi.client->frame.number, len, game.predictedState.error[ 0 ], game.predictedState.error[ 1 ], game.predictedState.error[ 2 ] );
        }
    }

	game.predictedState.origin = predictedMoveResult->origin = clgi.client->frame.ps.pmove.origin; // Store the server returned origin for this command index.
}

/**
*   @brief  Sets the predicted view angles.
**/
void PF_PredictAngles( void ) {
    // Don't predict angles if the pmove asks so specifically.
    if ( ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) || !cl_predict->integer ) {
        game.predictedState.currentPs.viewangles = clgi.client->frame.ps.viewangles;
        return;
    }

    // This is done even with cl_predict == 0.
    //game.predictedState.currentPs.viewangles = QM_Vector3AngleMod( clgi.client->viewangles + clgi.client->frame.ps.pmove.delta_angles );
    //game.predictedState.currentPs.viewangles = clgi.client->viewangles + QM_Vector3AngleMod( clgi.client->frame.ps.pmove.delta_angles );
    game.predictedState.currentPs.viewangles = clgi.client->viewangles + QM_Vector3AngleMod( clgi.client->delta_angles );

	//VectorAdd( , clgi.client->frame.ps.pmove.delta_angles, game.predictedState.currentPs.viewangles );
}

/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void PF_PredictMovement( int64_t acknowledgedCommandNumber, const int64_t currentCommandNumber ) {
    // Get the entity for the client.
    centity_t *cent = &clg_entities[ clgi.client->frame.clientNum + 1 ];
    // Last predicted state.
    client_predicted_state_t *predictedState = &game.predictedState;
   
    // Shuffle the still "CURRENT" ground info into our "LAST" groundinfo.
    predictedState->lastGround = predictedState->ground;
    // Shuffle the still "CURRENT" playerState into our "LAST" playerState.
    predictedState->lastPs = predictedState->currentPs;
    // Start off with the latest valid frame player state.
    predictedState->currentPs = clgi.client->frame.ps;

    // Player move configuration parameters.
    pmoveParams_t pmp;
    // Prepare the player move configuration parameters.
    SG_ConfigurePlayerMoveParameters( &pmp );

    // Prepare our player move, pointer to the player state, and setup the 
    // client side trace function pointers.
    pmove_t pm = {
        // Trace callbacks.
        .trace = CLG_PM_Trace,
        .clip = CLG_PM_Clip,
        .pointcontents = CLG_PM_PointContents,
        // The pointer to the player state we're predicting for.
        .state = &predictedState->currentPs,
    };
    // Apply client delta_angles.
    pm.state->pmove.delta_angles = clgi.client->delta_angles;
    // Setup client predicted ground data.
    pm.ground = predictedState->ground;
    // Setup client predicted liquid data.
    pm.liquid = predictedState->liquid;

    // Set view angles.
    // [NO-NEED]: This gets recalculated during PMove, based on the 'usercmd' and server 'delta angles'.
    //pm.state->viewangles = clgi.client->viewangles; 

    // Run previously stored and acknowledged frames up and including the last one.
    while ( ++acknowledgedCommandNumber <= currentCommandNumber ) {
        // Get the acknowledged move command from our circular buffer.
        client_movecmd_t *moveCommand = &clgi.client->moveCommands[ acknowledgedCommandNumber & CMD_MASK ];

        // Only simulate it if it had movement.
        //if ( moveCommand->cmd.msec ) {
            // Simulate the movement.
            pm.cmd = moveCommand->cmd;
            pm.simulationTime = QMTime::FromMilliseconds( moveCommand->prediction.simulationTime );
            
            #if 0
            // Don't do anything if the time is before the snapshot player time
            if ( pm.simulationTime.Milliseconds() <= game.predictedState.cmd.prediction.simulationTime ) {
                continue;
            }

            // Don't do anything if the command was from a previous map_restart
            if ( pm.simulationTime.Milliseconds() > moveCommand->prediction.simulationTime ) {
                continue;
            }
            #endif

			// Move the simulation.
            SG_PlayerMove( (pmove_s *)&pm, (pmoveParams_s *)&pmp );
            // Predict the next bobCycle for the frame.
            #if 0
                // Convert certain playerstate properties into entity state properties.
                centity_t *cent = &clg_entities[ clgi.client->frame.clientNum + 1 ];
                SG_PlayerStateToEntityState( clgi.client->frame.clientNum, pm.state, &cent->current, false );
                // <Q2RTXP>: WID: We'll keep this in case we fuck up viewweapon animations.
                CLG_PredictNextBobCycle( &pm );
            #endif
        //}

        // Store the resulting outcome(if no msec was found, it'll be the origin that
        // was left behind from the previous player move iteration.)
        moveCommand->prediction.origin = pm.state->pmove.origin;
        moveCommand->prediction.velocity = pm.state->pmove.velocity;

        // Check for changed predictable events.
        CLG_CheckChangedPredictableEvents( pm.state );

        // Backup into our circular buffer.
        clgi.client->predictedMoveResults[ ( acknowledgedCommandNumber ) & CMD_MASK ] = moveCommand->prediction;

        // Store it as the current predicted origin state.
        game.predictedState.origin = moveCommand->prediction.origin;
    }

    // Now predict results for the the pending command.
    client_movecmd_t *pendingMoveCommand = &clgi.client->moveCommand;
    // We got msec, let's predict player movement.
    if ( pendingMoveCommand->cmd.msec ) {
        // Store time of prediction.
        //pendingMoveCommand->prediction.time = clgi.client->cl.servertim;
        
        // Initialize pmove with the proper moveCommand data.
        #if 1
            pm.cmd = pendingMoveCommand->cmd;
            pm.cmd.forwardmove = clgi.client->localmove[ 0 ];
            pm.cmd.sidemove = clgi.client->localmove[ 1 ];
            pm.cmd.upmove = clgi.client->localmove[ 2 ];
        #else
            pendingMoveCommand->cmd.forwardmove = clgi.client->localmove[ 0 ];
            pendingMoveCommand->cmd.sidemove = clgi.client->localmove[ 1 ];
            pendingMoveCommand->cmd.upmove = clgi.client->localmove[ 2 ];
            pm.cmd = pendingMoveCommand->cmd;
        #endif
        // Perform movement.
        pm.simulationTime = QMTime::FromMilliseconds( pendingMoveCommand->prediction.simulationTime );
        SG_PlayerMove( (pmove_s *)&pm, (pmoveParams_s *)&pmp );
        // Convert certain playerstate properties into entity state properties.
        SG_PlayerStateToEntityState( clgi.client->frame.clientNum, pm.state, &cent->current, false );

        // Predict the next bobCycle for the frame.
        #if 1            
        // <Q2RTXP>: WID: We'll keep this in case we fuck up viewweapon animations.
        CLG_PredictNextBobCycle( &pm );
        #endif
        
        // Store the predicted outcome results 
        pendingMoveCommand->prediction.origin = pm.state->pmove.origin;
        pendingMoveCommand->prediction.velocity = pm.state->pmove.velocity;
        
        // Store it as the current last-predicted origin state.
        predictedState->origin = pendingMoveCommand->prediction.origin;
        // And store it in the predictedState as the last command.
        predictedState->cmd = *pendingMoveCommand;

        // Save the pending move command as the last entry in our circular buffer.
        clgi.client->predictedMoveResults[ ( currentCommandNumber + 1 ) & CMD_MASK ] = pendingMoveCommand->prediction;
    }

    // Copy ground and liquid results out into the current predicted state.
    predictedState->ground = pm.ground;
    predictedState->liquid = pm.liquid;
    // Same for mins/maxs.
    predictedState->mins = pm.mins;
    predictedState->maxs = pm.maxs;

    // Smooth Out Stair Stepping. We test results to the previously predicted ground data.
    CLG_PredictStepOffset( &pm, predictedState, pm.step_height );
    // Adjust the view height to the new state's viewheight. If it changed, record moment in time.
    PF_AdjustViewHeight( predictedState->currentPs.pmove.viewheight );
    // Check and execute any player state related events.
    CLG_CheckPlayerstateEvents( &predictedState->lastPs, &predictedState->currentPs );

    // Save the predicted frame for later use.
	//clgi.client->predictedFrame = clgi.client->frame;
	//clgi.client->predictedFrame.ps.pmove = predictedState->currentPs.pmove; // Save the predicted player state.
    clgi.client->predictedFrame.ps = predictedState->currentPs;
}