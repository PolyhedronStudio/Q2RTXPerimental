/********************************************************************
*
*
*	ClientGame: Player Movement Prediction Simulation Implementation.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_events.h"
#include "clgame/clg_playerstate.h"
#include "clgame/clg_predict.h"
#include "clgame/clg_world.h"


#include "sharedgame/sg_entities.h"
// For PM_STEP_MIN_NORMAL
#include "sharedgame/pmove/sg_pmove_slidemove.h"



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
static const cm_trace_t CLG_PM_Trace( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const void *passEntity, const cm_contents_t contentMask ) {
    return CLG_Trace( start, mins, maxs, end, (const centity_t *)passEntity, contentMask );
}
/**
*   @brief  Player Move specific 'Clip' wrapper implementation. Clips to world only.
**/
static const cm_trace_t CLG_PM_Clip( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const cm_contents_t contentMask ) {
    return CLG_Clip( start, mins, maxs, end, nullptr, contentMask );
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const cm_contents_t  CLG_PM_PointContents( const Vector3 &point ) {
    return CLG_PointContents( point );
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
void CLG_AdjustViewHeight( const int32_t viewHeight ) {
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
void CLG_CheckPlayerstateEvents( player_state_t *ops, player_state_t *ps ) {
    //if ( !clgi.client->clientEntity ) {
    //    return;
    //}
    
    //centity_t *clientEntity = clgi.client->clientEntity;
    // Get client entity.
    //centity_t *clientEntity = &clg_entities[ clgi.client->frame.clientNum + 1 ];

    //// Make sure it is valid for this frame.
    //if ( clientEntity->serverframe != clgi.client->frame.number ) {
    //    return;
    //}

    // WID: We don't have support for external events yet. In fact, they would in Q3 style rely on
    // 'temp entities', which are alike a normal entity. Point being is this requires too much refactoring
    // right now.
    #if 1
    /**
    *   Check for any external entity events that have been applied to the current frame's
	*   client entity.
    * 
    *   That entity is coming straight from the entity snapshot the server sent us for this frame.
	*   It is either the entity of our own local client, or that of the viewbound entity.
    **/
    if ( ps->externalEvent && ps->externalEvent != ops->externalEvent ) {
		// Get the client entity for the current frame.
        centity_t *clientEntity = &clg_entities[ ps->clientNumber + 1 ]; // CLG_GetLocalClientEntity();

		// Assign the external event to the client entity.
        clientEntity->current.event = ps->externalEvent;
        clientEntity->current.eventParm0 = ps->externalEventParm0;
        clientEntity->current.eventParm1 = ps->externalEventParm1;

        // Check and fire the external entity event.
        CLG_Events_CheckForEntity( clientEntity );
    }
    #endif

	// Get the game's predicted client entity, which is used to (not limited to) 
    // play PMove predicted events.
    centity_t *predictedEntity = &game.predictedEntity;//&clg_entities[ ps->clientNumber + 1 ];

    // Go through the predictable events buffer
    for ( int64_t i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++ ) {
        // If we have a new predictable event:
        if ( i >= ops->eventSequence
            // OR the server told us to play another event instead of a predicted event we already issued
            
            // OR something the server told us changed our prediction causing a different event
            || ( i > ops->eventSequence - MAX_PS_EVENTS && ps->events[ i & ( MAX_PS_EVENTS - 1 ) ] != ops->events[ i & ( MAX_PS_EVENTS - 1 ) ] ) 
        ) {
			// Get the event number.
            const int32_t playerStateEvent = ps->events[ i & ( MAX_PS_EVENTS - 1 ) ];
            // Get the parameter.
			const int32_t playerStateEventParm0 = ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
            // Assign to the client entity.
            predictedEntity->current.event = playerStateEvent;
            predictedEntity->current.eventParm0 = playerStateEventParm0;
            predictedEntity->current.eventParm1 = 0;// ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
            
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
            * 
            * 
            *    ---> see below
			*   The problem here seems that for playing the sound we need to know the entity's origin.
			*   So, we need to know which entity to use for that.
            *
            **/
            // Proceed to firing the predicted/received event.
            #if 0
                const bool processedPlayerStateViewBoundEffect = CLG_Events_CheckForPlayerState( ops, ps, playerStateEvent, clgi.client->predictedFrame.ps.pmove.origin, clgi.client->frame.clientNum );
                if ( !processedPlayerStateViewBoundEffect ) {
                        CLG_Events_CheckForEntity( clientEntity );
                }
            #else
                int32_t firedEvent = playerStateEvent;
				// First try to process via local player state events.
                const bool firedLocalPlayerStateEvent = CLG_Events_CheckForPlayerState( ops, ps, playerStateEvent, playerStateEventParm0, game.predictedState.origin/*, clgi.client->frame.ps.clientNumber */);

				// It was never dealt with by local client events, so assume that the event is
                // for and attached to the entity.
                if ( !firedLocalPlayerStateEvent ) {
                    firedEvent = CLG_Events_CheckForEntity( predictedEntity );
                }
            #endif

            // Add to the list of predictable events.
            game.predictedState.events[ i & ( client_predicted_state_t::MAX_PREDICTED_EVENTS - 1 ) ] = firedEvent;
            // Increment Event Sequence.
            game.predictedState.eventSequence++;
        }
    }
}

/**
*   @brief  Checks for changes in predicted events, and fires them if they differ.
**/
static void CLG_CheckChangedPredictableEvents( const player_state_t *ops, const player_state_t *ps, const QMTime &simulationTime ) {
	// Get local client entity pointer:
    centity_t *cent = &game.predictedEntity;//clg_entities[ ps->clientNumber + 1 ];
	// Sanity check.
    if ( !cent ) {
        return;
	}

    // Get the predicted player entity.
	//centity_t *predictedPlayerEntity = CLG_GetPredictedPlayerEntity();
	// cent = game.predictedPlayerEntity;

	// Go through the predictable events buffer
    for ( int64_t i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++ ) {
		// If we have no more events to check:
        if ( i >= game.predictedState.eventSequence ) {
            continue;
        }
        // If this event is not further back in than the maximum predictable events we remember..
        if ( i > game.predictedState.eventSequence - client_predicted_state_t::MAX_PREDICTED_EVENTS ) {
            // If the new playerstate event is different from a previously predicted one.
            if ( ps->events[ i & ( MAX_PS_EVENTS - 1 ) ] != game.predictedState.events[ i & ( client_predicted_state_t::MAX_PREDICTED_EVENTS - 1 ) ] ) {
				// We have a changed predicted event, so we need to re-fire it.
                const int32_t event = ps->events[ i & ( MAX_PS_EVENTS - 1 ) ];
				// Assign to the client entity.
                cent->current.event = event;
                cent->current.eventParm0 = ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
				cent->current.eventParm1 = 0;// ps->eventParms1[ i & ( MAX_PS_EVENTS - 1 ) ];

				// Fire the changed predicted event.
                CLG_Events_CheckForEntity( cent );

				// Update the predicted event.
                game.predictedState.events[ i & ( client_predicted_state_t::MAX_PREDICTED_EVENTS - 1 ) ] = event;

                if ( cl_showmiss->integer ) {
                    clgi.ShowMiss( "WARNING: changed predicted event\n" );
                }

                if ( clg_debug_pmove_changed_events->integer ) {
                    clgi.Print( PRINT_DEVELOPER, "-----\n" );
                    clgi.Print( PRINT_DEVELOPER, "  WARNING: changed predicted event (Time: %lld, Server Frame: %lld, Client Frame: %lld)\n", simulationTime.Milliseconds(), clgi.client->frame.number, game.predictedState.frame.number );
                    clgi.Print( PRINT_DEVELOPER, "  Old: %d, New: %d\n", game.predictedState.events[ i & ( client_predicted_state_t::MAX_PREDICTED_EVENTS - 1 ) ], ps->events[ i & ( MAX_PS_EVENTS - 1 ) ] );
                    clgi.Print( PRINT_DEVELOPER, "-----\n" );
                }
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
    uint8_t bobCycle = clgi.client->frame.ps.bobCycle;
    // Handle wraparound:
    if ( bobCycle < pm->state->bobCycle ) {
        bobCycle += 256;
    }
	// Lerp to next bobcycle.
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
        && ( predictedState->lastGround.entityNumber != ENTITYNUM_NONE || pm->step_clip )
        // And are still predicted to be on the ground.
        && ( ( predictedState->ground.entityNumber != ENTITYNUM_NONE ) && pm->state->pmove.pm_type <= PM_GRAPPLE )
        // Or we stand on another different ground entity. 
        && ( predictedState->ground.entityNumber != predictedState->lastGround.entityNumber
            // Plane memory isn't identical thus we stand on a different plane. OR.. 
            || std::memcmp( &predictedState->lastGround.plane, &predictedState->ground.plane, sizeof( cm_plane_t ) ) != 0 )
    );
    // Determine the transition step height.
    if ( step_detected ) {
        #if 1
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
        #endif
        // Store time of change.
        predictedState->transition.step.timeChanged = clgi.GetRealTime();
        // Store the size of the stepheight.
        predictedState->transition.step.size = stepSize;
    }

}

/**
*	@return	False if prediction is not desired for. True if it is.
* 
*   @todo   Have it return flags for what to predict, and rename it to CLG_CurrentPredictionFlags or something similar.
**/
const qboolean CLG_UsePrediction( void ) {
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
    
    // We want predicting.
    return true;
}

/**
*   @brief  Checks for prediction if desired. Will determine the error margin
*           between our predicted state and the server returned state. In case
*           the margin is too high, snap back to server provided player state.
**/
void CLG_CheckPredictionError( ) {
    // Determine the last usercmd_t we sent that the server has processed.
    const int64_t frameIndex = clgi.Netchan_GetIncomingAcknowledged()  & CMD_MASK;
    // Get the move command history index for this frame index.
    const uint64_t commandIndex = clgi.client->outPacketHistory[ frameIndex ].commandNumber;
    // Finally get the pointer to the actual move command residing inside the list.
    client_movecmd_t *moveCommand = &clgi.client->moveCommands[ commandIndex & CMD_MASK ];

    // Maximum delta allowed before snapping back. (80 units at 40hz)
    static constexpr double MAX_DELTA_ORIGIN = ( 3200. * ( 1.0 / BASE_FRAMERATE ) );

    // If it is the first frame, we got nothing to predict yet.
    if ( moveCommand->prediction.simulationTime == 0 && clgi.client->time == 0 ) {
        // Snap back to server state.
        game.predictedState.lastPs = clgi.client->frame.ps;
        game.predictedState.currentPs = clgi.client->frame.ps;
        //game.predictedState.currentPs.pmove = game.predictedState.currentPs.pmove;
        game.predictedState.error = {};
        game.predictedState.origin = clgi.client->frame.ps.pmove.origin; // Store the server returned origin for this command index.

        // Record transition state.
        game.predictedState.transition = {
            .step = {
                .height = 0,
                .timeChanged = clgi.GetRealTime()
            },
            .view = {
                .height = {
                    (double)clgi.client->frame.ps.pmove.viewheight,
                    (double)clgi.client->frame.ps.pmove.viewheight
                },
                .timeHeightChanged = (uint64_t)clgi.client->accumulatedRealTime
            }
        };

        if ( cl_showmiss->integer ) {
            clgi.ShowMiss( "First frame(%" PRId64 "). Nothing to predict yet.\n",
                clgi.client->frame.number );
        }

        return;
    }

    // Get the predicted move result for this command index.
	client_movecmd_prediction_t *predictedMoveResult = &clgi.client->predictedMoveResults[ commandIndex & CMD_MASK ]; // Get the predicted move results for this command index.

    // Subtract what the server returned from our predicted origin for that frame.
    //game.predictedState.error = moveCommand->prediction.error = moveCommand->prediction.origin - clgi.client->frame.ps.pmove.origin;
    const Vector3 errorMargin = clgi.client->frame.ps.pmove.origin - predictedMoveResult->origin;
    game.predictedState.error = predictedMoveResult->error = moveCommand->prediction.error = errorMargin;

    // Save the prediction error for interpolation.
    const double len = std::abs( QM_Vector3LengthDP( game.predictedState.error ) );
    //if (len < 1 || len > 640) {
    if ( len > .1
        && game.predictedState.cmd.prediction.simulationTime == moveCommand->prediction.simulationTime ) {
        // Snap back if the distance was too far off:
        if ( len > MAX_DELTA_ORIGIN ) {
            // Debug misses:
            if ( cl_showmiss->integer ) {
                clgi.ShowMiss( "MAX_DELTA_ORIGIN on frame #(%" PRId64 "): len(%f) (%f %f %f)\n",
                    clgi.client->frame.number, len, game.predictedState.error[ 0 ], game.predictedState.error[ 1 ], game.predictedState.error[ 2 ] );
            }

            // Snap back to server state.
            game.predictedState.lastPs = clgi.client->frame.ps;
            game.predictedState.currentPs = clgi.client->frame.ps;
            //game.predictedState.currentPs.pmove = clgi.client->predictedFrame.ps.pmove;
            game.predictedState.error = {};
            game.predictedState.origin = clgi.client->frame.ps.pmove.origin; // Store the server returned origin for this command index.

            // Reset transition state.
            game.predictedState.transition = {
                .step = {
                    .height = 0,
                    .timeChanged = clgi.GetRealTime()
                },
                .view = {
                    .height = {
                        (double)clgi.client->frame.ps.pmove.viewheight,
                        (double)clgi.client->frame.ps.pmove.viewheight
                    },
                    .timeHeightChanged = (uint64_t)clgi.client->accumulatedRealTime
                }
            };

            return;
            // In case of a minor distance, when cl_showmiss is enabled, report:
        } else {
            // Debug misses:
            if ( cl_showmiss->integer ) {
                clgi.ShowMiss( "Prediction miss on frame #(%" PRId64 "): len(%f) (%f %f %f)\n",
                    clgi.client->frame.number, len, game.predictedState.error[ 0 ], game.predictedState.error[ 1 ], game.predictedState.error[ 2 ] );
            }
        }
    }

    // Store the server returned origin for this command index.
    game.predictedState.origin = predictedMoveResult->origin = clgi.client->frame.ps.pmove.origin;
}

/**
*   @brief  Sets the predicted view angles.
**/
void CLG_PredictAngles( void ) {
    // Don't predict angles if the pmove asks so specifically.
    if ( !CLG_UsePrediction() || ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_ANGLES_PREDICTION ) ) {
        // Just use the server provided viewangles.
        game.predictedState.currentPs.viewangles = clgi.client->frame.ps.viewangles;
        // End here.
        return;
    }

    // Otherwise use the client viewangles + delta angles for the predicted player state its view angles.
    game.predictedState.currentPs.viewangles = clgi.client->viewangles + QM_Vector3AngleMod( clgi.client->delta_angles );
}

/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void CLG_PredictMovement( int64_t acknowledgedCommandNumber, const int64_t currentCommandNumber ) {
    // Get the current local client's player entity.
    game.clientEntity       = CLG_GetLocalClientEntity();
    // Get the current frames' chasing view player entity. (Can be nullptr )
	//game.chaseEntity        = CLG_GetChaseBoundEntity();
	// Get the view bound entity. (Can be one we're chasing, or local entity.)
	game.viewBoundEntity    = CLG_GetViewBoundEntity();

	// The client entity.
    centity_t *clientEntity = CLG_GetPredictedClientEntity();// clgi.client->clientEntity;
	if ( !clientEntity ) {
        return;
    }

	// Set serverframe so it'll be 'valid for current frame'. 
	// (Despite practically living outside of its constraint.)
	clientEntity->serverframe = clgi.client->frame.number;

	// Player move configuration parameters.
	pmoveParams_t pmp;
	// Prepare the player move configuration parameters.
	SG_ConfigurePlayerMoveParameters( &pmp );

    // Last predicted state.
    client_predicted_state_t *predictedState = &game.predictedState;

	// For transitioning purposes, store the old predicted frame.
	predictedState->lastFrame = predictedState->frame;

    // Shuffle the still "CURRENT" playerState into our "LAST" playerState.
    predictedState->lastPs = predictedState->currentPs;
    // Start off with the latest valid frame player state.
    predictedState->currentPs = clgi.client->frame.ps;

	// Shuffle the still "CURRENT" ground info into our "LAST" groundinfo.
	predictedState->lastGround = predictedState->ground;

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
            #if 1
                // Check and execute any player state related events.
                CLG_CheckPlayerstateEvents( &predictedState->lastPs, &predictedState->currentPs );
                // Check for predictable events that changed from previous predictions.
                CLG_CheckChangedPredictableEvents( &predictedState->lastPs, &predictedState->currentPs, pm.simulationTime );
                // Convert certain playerstate properties into entity state properties.
                SG_PlayerStateToEntityState( clgi.client->clientNumber, &predictedState->currentPs, &clientEntity->current, false, false );
                // Predict the next bobCycle for the frame.
                CLG_PredictNextBobCycle( &pm );
            #endif

            moveCommand->prediction.origin  = pm.state->pmove.origin;
            moveCommand->prediction.velocity= pm.state->pmove.velocity;
        //}

        // Store the resulting outcome(if no msec was found, it'll be the origin and velocity which were left behind from the previous player move iteration)/
        game.predictedState.origin      = pm.state->pmove.origin;
        game.predictedState.velocity    = pm.state->pmove.velocity;

        // Backup into our circular buffer.
        clgi.client->predictedMoveResults[ ( acknowledgedCommandNumber ) & CMD_MASK ] = moveCommand->prediction;
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
        // Perform movement. // ?? clgi.client->servertime
        pm.simulationTime = QMTime::FromMilliseconds( pendingMoveCommand->prediction.simulationTime );
        SG_PlayerMove( (pmove_s *)&pm, (pmoveParams_s *)&pmp );
        #if 1
        // Check and execute any player state related events.
        CLG_CheckPlayerstateEvents( &predictedState->lastPs, &predictedState->currentPs );
        // Check for predictable events that changed from previous predictions.
        CLG_CheckChangedPredictableEvents( &predictedState->lastPs, &predictedState->currentPs, pm.simulationTime );
        // Convert certain playerstate properties into entity state properties.
        SG_PlayerStateToEntityState( clgi.client->clientNumber, &predictedState->currentPs, &clientEntity->current, true, false );
        // Predict the next bobCycle for the frame.
        CLG_PredictNextBobCycle( &pm );
        #endif

        // Store the predicted outcome results 
        pendingMoveCommand->prediction.origin = pm.state->pmove.origin;
        pendingMoveCommand->prediction.velocity = pm.state->pmove.velocity;

        // Save the pending move command as the last entry in our circular buffer.
        clgi.client->predictedMoveResults[ ( currentCommandNumber + 1 ) & CMD_MASK ] = pendingMoveCommand->prediction;

        // And store it in the predictedState as the last command.
        game.predictedState.cmd = *pendingMoveCommand;
    }

    // Store the resulting outcome(if no msec was found, it'll be the origin and velocity which were left behind from the previous player move iteration)/
    game.predictedState.origin = pm.state->pmove.origin;
    game.predictedState.velocity = pm.state->pmove.velocity;
    // Copy ground and liquid results out into the current predicted state.
    game.predictedState.ground = pm.ground;
    game.predictedState.liquid = pm.liquid;
    // Same for mins/maxs.
    game.predictedState.mins = pm.mins;
    game.predictedState.maxs = pm.maxs;

	// Debug: Check for dropped events.
    if ( cl_showmiss->integer ) {
		// Check for dropped events.
        if ( predictedState->currentPs.eventSequence > predictedState->lastPs.eventSequence + MAX_PS_EVENTS ) {
			clgi.Print( PRINT_DEVELOPER, "WARNING: dropped event\n" );
        }
    }

    // Save the predicted frame for later use.
    //clgi.client->predictedFrame = clgi.client->frame;
    //clgi.client->predictedFrame.ps.pmove = predictedState->currentPs.pmove; // Save the predicted player state.
    predictedState->frame.ps = predictedState->currentPs;

    // Fires events and other transition triggered things. And determine whether the player state has to
    // lerp transition between the current and old frame, or 'snap' to by duplication.    
    //CLG_PlayerState_Transition( &game.predictedEntity &clgi.client->predictedFrame, &oldPredictedFrame, clgi.client->serverdelta );

    // TODO: Use game.predictedEntity for this.
    //CLG_PlayerState_Transition( &clg_entities[ clgi.client->frame.ps.clientNumber + 1 ], &predictedState->frame, &predictedState->lastFrame, clgi.client->serverdelta);
    CLG_PlayerState_Transition( clientEntity, &predictedState->frame, &predictedState->lastFrame, clgi.client->serverdelta );

	// Debug: Check for double events.
    if ( cl_showmiss->integer ) {
		// Check for being out of sync.
        if ( game.predictedState.eventSequence > predictedState->currentPs.eventSequence ) {
			// Sync event sequence.
            game.predictedState.eventSequence = predictedState->currentPs.eventSequence;
            // Debug print.
            clgi.Print( PRINT_DEVELOPER, "WARNING: double event\n" );
        }
    }

    // Smooth Out Stair Stepping. We test results to the previously predicted ground data.
    CLG_PredictStepOffset( &pm, predictedState, pm.step_height );
    // Adjust the view height to the new state's viewheight. If it changed, record moment in time.
    CLG_AdjustViewHeight( predictedState->currentPs.pmove.viewheight );
}