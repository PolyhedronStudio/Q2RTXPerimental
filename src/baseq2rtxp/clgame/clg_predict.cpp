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


/**
*   @brief  Player Move specific 'Trace' wrapper implementation.
**/
static const cm_trace_t q_gameabi CLG_PM_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const cm_contents_t contentMask ) {
    cm_trace_t t;
    //if (pm_passent->health > 0)
    //    return gi.trace(start, mins, maxs, end, pm_passent, CM_CONTENTMASK_PLAYERSOLID);
    //else
    //    return gi.trace(start, mins, maxs, end, pm_passent, CM_CONTENTMASK_DEADSOLID);
    t = clgi.Trace( start, mins, maxs, end, (const centity_t *)passEntity, contentMask );
    return t;
}
/**
*   @brief  Player Move specific 'Clip' wrapper implementation. Clips to world only.
**/
static const cm_trace_t q_gameabi CLG_PM_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const cm_contents_t contentMask ) {
    const cm_trace_t trace = clgi.Clip( start, mins, maxs, end, nullptr, contentMask );
    return trace;
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const cm_contents_t q_gameabi CLG_PM_PointContents( const vec3_t point ) {
    return clgi.PointContents( point );
}

/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void PF_AdjustViewHeight( const int32_t viewHeight ) {
    // Record viewheight changes.
    if ( game.predictedState.transition.view.height[ 0 ] != viewHeight ) {
        game.predictedState.transition.view.height[ 1 ] = game.predictedState.transition.view.height[ 0 ];
        game.predictedState.transition.view.height[ 0 ] = viewHeight;
        game.predictedState.transition.view.timeHeightChanged = clgi.client->time;
        // Debug.
        clgi.Print( PRINT_DEVELOPER, "%s: %f\n", __func__, game.predictedState.transition.view.height[ 0 ] );
    }

    //if ( game.predictedState.transition.view.height[ 0 ] != viewHeight ) {
    //    // Store current as previous.
    //    game.predictedState.transition.view.height[ 1 ] = game.predictedState.transition.view.height[ 0 ];
    //    // Store new as current.
    //    game.predictedState.transition.view.height[ 0 ] = viewHeight;
    //    // Debug.
    //    clgi.Print( PRINT_DEVELOPER, "%s: %f\n", __func__, game.predictedState.transition.view.height[ 0 ] );
    //}

    // Record viewheight changes.
    //if ( game.predictedState.transition.view.height[ 0 ] != viewHeight && game.predictedState.transition.view.height[ 1 ] == viewHeight ) {
    //    game.predictedState.transition.view.height[ 1 ] = game.predictedState.transition.view.height[ 0 ];
    //    game.predictedState.transition.view.height[ 0 ] = viewHeight;
    //} else if ( game.predictedState.transition.view.height[ 1 ] != viewHeight && game.predictedState.transition.view.height[ 0 ] == viewHeight ) {
    //    game.predictedState.transition.view.height[ 0 ] = game.predictedState.transition.view.height[ 1 ];
    //    game.predictedState.transition.view.height[ 1 ] = viewHeight;
    //}
}

/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
void CLG_CheckPlayerstateEvents( player_state_t *ops, player_state_t *ps ) {
    //if ( !clgi.client->clientEntity ) {
    //    return;
    //}
    //centity_t *cent = clgi.client->clientEntity;
    // Get client entity.
    centity_t *clientEntity = &clg_entities[ clgi.client->frame.clientNum + 1 ];

    // Make sure it is valid for this frame.
    if ( clientEntity->serverframe != clgi.client->frame.number ) {
        return;
    }

    // WID: We don't have support for external events yet. In fact, they would in Q3 style rely on
    // 'temp entities', which are alike a normal entity. Point being is this requires too much refactoring
    // right now.
    #if 0
    if ( ps->externalEvent && ps->externalEvent != ops->externalEvent ) {
        centity_t *clientEntity = clgi.client->clientEntity;//cent = &cg_entities[ ps->clientNum + 1 ];
        clientEntity->currentState.event = ps->externalEvent;
        clientEntity->currentState.eventParm = ps->externalEventParm;
        CG_EntityEvent( clientEntity, clientEntity->lerpOrigin );
    }
    #endif

    // 
    //centity_t *clientEntity= &clg_entities[ clgi.client->frame.clientNum + 1 ];//clgi.client->clientEntity; // cg_entities[ ps->clientNum + 1 ];
    // go through the predictable events buffer
    for ( int64_t i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++ ) {
        // If we have a new predictable event:
        if ( i >= ops->eventSequence
            // OR the server told us to play another event instead of a predicted event we already issued
            // OR something the server told us changed our prediction causing a different event
            || ( i > ops->eventSequence - MAX_PS_EVENTS && ps->events[ i & ( MAX_PS_EVENTS - 1 ) ] != ops->events[ i & ( MAX_PS_EVENTS - 1 ) ] ) ) {

            //clientEntity->current.event = event;
            //clientEntity->current.eventParm = ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
            //CLG_EntityEvent( cent, cent->lerp_origin );

            // Get the event number.
            const int32_t playerStateEvent = ps->events[ i & ( MAX_PS_EVENTS - 1 ) ];
            // Proceed to firing the predicted/received event.
            CLG_FirePlayerStateEvent( ops, ps, playerStateEvent, clientEntity->lerp_origin );

            //// Add to the list of predictable events.
            //game.predictableEvents[ i & ( MAX_PREDICTED_EVENTS - 1 ) ] = event;
            //// Increment Event Sequence.
            //game.eventSequence++;
        }
    }
}

/**
*   @brief  Will predict the bobcycle to be for the next server frame.
**/
void CLG_PredictNextBobCycle( pmove_t *pm ) {
    // Predict next bobcycle.
    const float bobCycleFraction = clgi.client->xerpFraction;
    uint8_t bobCycle = clgi.client->frame.ps.bobCycle;//pm->playerState->bobCycle;// nextframe->ps.bobCycle;
    // Handle wraparound:
    if ( bobCycle < pm->playerState->bobCycle ) {
        bobCycle += 256;
    }
    pm->playerState->bobCycle = pm->playerState->bobCycle + bobCycleFraction * ( bobCycle - clgi.client->frame.ps.bobCycle );

    //clgi.Print( PRINT_DEVELOPER, "%s: bobCycle(%i), bobCycleFraction(%f)\n", __func__, pm->playerState->bobCycle, bobCycleFraction );
}

/**
*   @brief  Will determine if we're stepping, and smooth out the step height in case of traversing multiple steps in a row.
**/
void CLG_PredictStepOffset( pmove_t *pm, client_predicted_state_t *predictedState, const float stepHeight ) {
    // Time in miliseconds to lerp the step with.
    static constexpr double PM_STEP_TIME = 100.;
    // Maximum -/+ change we allow in step lerps.
    static constexpr double PM_MAX_STEP_CHANGE = 32.;

    // Get the absolute step's height value for testing against.
    const double fabsStep = std::fabs( stepHeight );

    // Consider a Z change being "stepping" if...
    const bool step_detected = ( fabsStep > PM_MIN_STEP_SIZE && fabsStep < PM_MAX_STEP_SIZE ) // Absolute change is in this limited range.
        && ( ( clgi.client->frame.ps.pmove.pm_flags & PMF_ON_GROUND ) || pm->step_clip ) // And we started off on the ground.
        && ( ( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) && pm->playerState->pmove.pm_type <= PM_GRAPPLE ) // And are still predicted to be on the ground.
        && ( memcmp( &predictedState->ground.plane, &pm->ground.plane, sizeof( cm_plane_t ) ) != 0 // Plane memory isn't identical, OR..
            || predictedState->ground.entity != pm->ground.entity ); // we stand on another plane or entity.

    if ( step_detected ) {
        // check for stepping up before a previous step is completed
        const uint64_t delta = ( clgi.GetRealTime() - predictedState->transition.step.timeChanged );

        // Default old step to 0.
        double old_step = 0.;
        // If the delta timefor the previous step, up till the current step frame is smaller than PM_STEP_TIME.
        if ( delta < PM_STEP_TIME ) {
            // Calculate how far we've come.
            //old_step = stepHeight * ( PM_STEP_TIME - delta) / PM_STEP_TIME;
            old_step = predictedState->transition.step.height * ( PM_STEP_TIME - delta ) / PM_STEP_TIME;
        }

        // Add the stepHeight amount.
        predictedState->transition.step.height = std::clamp( old_step + stepHeight, -PM_MAX_STEP_CHANGE, PM_MAX_STEP_CHANGE );

        // Set the new step_time.
        predictedState->transition.step.timeChanged = clgi.GetRealTime();
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
    static constexpr double MAX_DELTA_ORIGIN = ( 3200 * ( 1.0 / BASE_FRAMERATE ) );

    // If it is the first frame, we got nothing to predict yet.
    if ( moveCommand->prediction.time == 0 ) {
        game.predictedState.lastPs = clgi.client->frame.ps;
        game.predictedState.currentPs = clgi.client->frame.ps;
        //game.predictedState.currentPs.pmove = game.predictedState.currentPs.pmove;
        game.predictedState.error = {};

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
                .timeHeightChanged = (uint64_t)clgi.client->time
            }
        };

        clgi.ShowMiss( "First frame(%" PRIi64 "). Nothing to predict yet.\n",
            clgi.client->frame.number );
        return;
    }

    // Subtract what the server returned from our predicted origin for that frame.
    game.predictedState.error = moveCommand->prediction.error = moveCommand->prediction.origin - clgi.client->frame.ps.pmove.origin;

    // Save the prediction error for interpolation.
    //const float len = fabs( delta[ 0 ] ) + abs( delta[ 1 ] ) + abs( delta[ 2 ] );
    const float len = fabs( QM_Vector3Length( game.predictedState.error ) );
    //if (len < 1 || len > 640) {
    if ( len > .1f ) {
        // Snap back if the distance was too far off:
        if ( len > MAX_DELTA_ORIGIN ) {
            // Debug misses:
            clgi.ShowMiss( "MAX_DELTA_ORIGIN on frame #(%" PRIi64 "): len(%f) (%f %f %f)\n",
                clgi.client->frame.number, len, game.predictedState.error[ 0 ], game.predictedState.error[ 1 ], game.predictedState.error[ 2 ] );

            game.predictedState.lastPs = clgi.client->frame.ps;
            game.predictedState.currentPs = clgi.client->frame.ps;
            //game.predictedState.currentPs.pmove = clgi.client->predictedFrame.ps.pmove;
            game.predictedState.error = {};

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
                    .timeHeightChanged = (uint64_t)clgi.client->time
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
}

/**
*   @brief  Sets the predicted view angles.
**/
void PF_PredictAngles( void ) {
    // Don't predict angles if the pmove asks so specifically.
    if ( ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) || !cl_predict->integer ) {
        VectorCopy( clgi.client->frame.ps.viewangles, game.predictedState.currentPs.viewangles );
        return;
    }

    // This is done even with cl_predict == 0.
    //game.predictedState.currentPs.viewangles = QM_Vector3AngleMod( clgi.client->viewangles + clgi.client->frame.ps.pmove.delta_angles );
    game.predictedState.currentPs.viewangles = clgi.client->viewangles + QM_Vector3AngleMod( clgi.client->frame.ps.pmove.delta_angles );

	//VectorAdd( , clgi.client->frame.ps.pmove.delta_angles, game.predictedState.currentPs.viewangles );
}

/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void PF_PredictMovement( int64_t acknowledgedCommandNumber, const int64_t currentCommandNumber ) {
    // Prepare the player move parameters.
    pmoveParams_t pmp;
    SG_ConfigurePlayerMoveParameters( &pmp );

    // Last predicted state.
    client_predicted_state_t *predictedState = &game.predictedState;

    // Shuffle current to last playerState.
    predictedState->lastPs = predictedState->currentPs;

    // Start off with the latest valid frame player state.
    //player_state_t pmPlayerState = predictedState->currentPs = clgi.client->frame.ps;
    // Copy in player state.
    predictedState->currentPs = clgi.client->frame.ps;
    // Get pointer to it for player move.
    player_state_t *pmPlayerState = &predictedState->currentPs;

    // Prepare our player move, pointer to the player state, and setup the 
    // client side trace function pointers.
    pmove_t pm = {
        .playerState = pmPlayerState,

        .trace = CLG_PM_Trace,
        .clip = CLG_PM_Clip,
        .pointcontents = CLG_PM_PointContents,
    };

    // Setup client predicted ground data.
    pm.ground = predictedState->ground;
    // Setup client predicted liquid data.
    pm.liquid = predictedState->liquid;

    // Apply client delta_angles.
    pm.playerState->pmove.delta_angles = clgi.client->delta_angles;
    // Set view angles.
    // [NO-NEED]: This gets recalculated during PMove, based on the 'usercmd' and server 'delta angles'.
    //pm.playerState->viewangles = clgi.client->viewangles; 
  //  clgi.Print( PRINT_DEVELOPER, "----------------------------------- \n" );
  //  clgi.Print( PRINT_DEVELOPER, "frame(%" PRIx64 ") stime(%" PRIX64 ") etime(%" PRIX64 ") time(% " PRIx64 "rtime(% " PRIu64 ")\n",
		//clgi.client->frame.number, clgi.client->servertime, clgi.client->extrapolatedTime, clgi.client->time, clgi.GetRealTime() );


    // Run previously stored and acknowledged frames up and including the last one.
    while ( ++acknowledgedCommandNumber <= currentCommandNumber ) {
        // Get the acknowledged move command from our circular buffer.
        client_movecmd_t *moveCommand = &clgi.client->moveCommands[ acknowledgedCommandNumber & CMD_MASK ];

        // Only simulate it if it had movement.
        if ( moveCommand->cmd.msec ) {
            // Timestamp it so the client knows we have valid results.
            //moveCommand->prediction.time = clgi.client->time;//clgi.client->time;

            // Simulate the movement.
            pm.cmd = moveCommand->cmd;
            pm.simulationTime = QMTime::FromMilliseconds( moveCommand->prediction.time );
            SG_PlayerMove( (pmove_s *)&pm, (pmoveParams_s *)&pmp );
            // Predict the next bobCycle for the frame.
            //CLG_PredictNextBobCycle( &pm );

            //clgi.Print( PRINT_DEVELOPER, "move: (%f, %f, %f) time(%" PRIu64 ")\n",
            //    pm.playerState->pmove.origin[ 0 ], pm.playerState->pmove.origin[ 1 ], pm.playerState->pmove.origin[ 2 ],
            //    moveCommand->prediction.time );

            // Save for prediction checking.
        }

        moveCommand->prediction.origin = pm.playerState->pmove.origin;
        moveCommand->prediction.velocity = pm.playerState->pmove.velocity;
    }

    // Now run the pending command number.
    #if 1
    client_movecmd_t *pendingMoveCommand = &clgi.client->moveCommand;
    if ( pendingMoveCommand->cmd.msec ) {
        // Store time of prediction.
        pendingMoveCommand->prediction.time = clgi.client->time;
        
        // Initialize pmove with the proper moveCommand data.
        #if 0
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
        pm.simulationTime = QMTime::FromMilliseconds( pendingMoveCommand->prediction.time );
        SG_PlayerMove( (pmove_s *)&pm, (pmoveParams_s *)&pmp );
        // Predict the next bobCycle for the frame.
        //CLG_PredictNextBobCycle( &pm );
        // Save for prediction checking.

        // Save the now not pending anymore move command as the last entry in our circular buffer.
        pendingMoveCommand->prediction.origin = pm.playerState->pmove.origin;
        pendingMoveCommand->prediction.velocity = pm.playerState->pmove.velocity;
        clgi.client->moveCommands[ ( currentCommandNumber + 1 ) & CMD_MASK ] = *pendingMoveCommand;

        //clgi.Print( PRINT_DEVELOPER, "pending:(%f, %f, %f) time(%" PRIu64 ") \n",
        //    pm.playerState->pmove.origin[ 0 ], pm.playerState->pmove.origin[ 1 ], pm.playerState->pmove.origin[ 2 ],
        //    pendingMoveCommand->prediction.time );

        predictedState->cmd = *pendingMoveCommand;
    }

    #endif

    // Smooth Out Stair Stepping. This is done before updating the ground data so we can test results to the
    // previously predicted ground data.
    CLG_PredictStepOffset( &pm, predictedState, pm.step_height );

    // Copy ground and liquid results out into the current predicted state.
    predictedState->ground = pm.ground;
    predictedState->liquid = pm.liquid;
    // Same for mins/maxs.
    predictedState->mins = pm.mins;
    predictedState->maxs = pm.maxs;

    // Swap in the resulting new pmove player state.
    predictedState->currentPs = *pmPlayerState;

    // Adjust the view height to the new state's viewheight. If it changed, record moment in time.
    PF_AdjustViewHeight( predictedState->currentPs.pmove.viewheight );

    // Check and execute any player state related events.
    CLG_CheckPlayerstateEvents( &predictedState->lastPs, &predictedState->currentPs );
}