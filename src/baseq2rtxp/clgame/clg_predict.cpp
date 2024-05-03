/********************************************************************
*
*
*	ClientGame: Player Movement Prediction Simulation Implementation.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_predict.h"

/**
*   @brief  Player Move specific 'Trace' wrapper implementation.
**/
static const trace_t q_gameabi CLG_PM_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const contents_t contentMask ) {
    trace_t t;
    //if (pm_passent->health > 0)
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
    //else
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
    t = clgi.Trace( start, mins, maxs, end, (const centity_t *)passEntity, contentMask );
    return t;
}
/**
*   @brief  Player Move specific 'Clip' wrapper implementation. Clips to world only.
**/
static const trace_t q_gameabi CLG_PM_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const contents_t contentMask ) {
    const trace_t trace = clgi.Clip( start, mins, maxs, end, nullptr, contentMask );
    return trace;
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const contents_t q_gameabi CLG_PM_PointContents( const vec3_t point ) {
    return clgi.PointContents( point );
}

/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void PF_AdjustViewHeight( const int32_t viewHeight ) {
    // Record viewheight changes.
    if ( clgi.client->predictedState.viewheight[ 0 ] != viewHeight ) {
        clgi.client->predictedState.viewheight[ 1 ] = clgi.client->predictedState.viewheight[ 0 ];
        clgi.client->predictedState.viewheight[ 0 ] = viewHeight;
        clgi.client->predictedState.time.height_changed = clgi.client->time;
    }
}

/**
*   @brief  Will determine if we're stepping, and smooth out the step height in case of traversing multiple steps in a row.
**/
void CLG_PredictStepOffset( pmove_t *pm, client_predicted_state_t *predictedState, const float stepHeight ) {
    // Time in miliseconds to lerp the step with.
    static constexpr int32_t PM_STEP_TIME = 100;
    // Maximum -/+ change we allow in step lerps.
    static constexpr int32_t PM_MAX_STEP_CHANGE = 32;

    // Get the absolute step's height value for testing against.
    const float fabsStep = fabsf( stepHeight );

    // Consider a Z change being "stepping" if...
    const bool step_detected = ( fabsStep > PM_MIN_STEP_SIZE && fabsStep < PM_MAX_STEP_SIZE ) // Absolute change is in this limited range.
        && ( ( clgi.client->frame.ps.pmove.pm_flags & PMF_ON_GROUND ) || pm->step_clip ) // And we started off on the ground.
        && ( ( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) && pm->playerState->pmove.pm_type <= PM_GRAPPLE ) // And are still predicted to be on the ground.
        && ( memcmp( &predictedState->ground.plane, &pm->ground.plane, sizeof( cplane_t ) ) != 0 // Plane memory isn't identical, OR..
            || predictedState->ground.entity != pm->ground.entity ); // we stand on another plane or entity.

    if ( step_detected ) {
        // check for stepping up before a previous step is completed
        const uint64_t delta = clgi.GetRealTime() - predictedState->time.step_changed;

        // Default old step to 0.
        double old_step = 0.f;
        // If the delta timefor the previous step, up till the current step frame is smaller than PM_STEP_TIME.
        if ( delta < PM_STEP_TIME ) {
            // Calculate how far we've come.
            //old_step = stepHeight * ( PM_STEP_TIME - delta) / PM_STEP_TIME;
            old_step = predictedState->step * ( PM_STEP_TIME - delta ) / PM_STEP_TIME;
        }

        // Add the stepHeight amount.
        predictedState->step = constclamp( old_step + stepHeight, -PM_MAX_STEP_CHANGE, PM_MAX_STEP_CHANGE );

        // Set the new step_time.
        predictedState->time.step_changed = clgi.GetRealTime();
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
    if ( sv_paused->integer ) {
        clgi.client->predictedState.error = {};
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
void PF_CheckPredictionError( const int64_t frameIndex, const uint64_t commandIndex, const pmove_state_t *in, struct client_movecmd_s *moveCommand, client_predicted_state_t *out ) {
    // Maximum delta allowed before snapping back.
    static constexpr double MAX_DELTA_ORIGIN = ( 2400 * ( 1.0 / BASE_FRAMERATE ) );

    // If it is the first frame, we got nothing to predict yet.
    if ( moveCommand->prediction.time == 0 ) {
        //out->view.origin = in->origin;//in->origin;
        //out->view.offset = clgi.client->frame.ps.viewoffset;
        //out->view.angles = clgi.client->frame.ps.viewangles/* + moveCommand->cmd.angles*/;
        //out->view.velocity = in->velocity;
        //out->view.rdflags = clgi.client->frame.ps.rdflags;
        //out->view.screen_blend = clgi.client->frame.ps.screen_blend;
        out->lastPs = clgi.client->frame.ps;
        out->currentPs = clgi.client->frame.ps;
        //out->currentPs.pmove.origin = in->origin;
        //out->currentPs.pmove.velocity = in->velocity;
        out->currentPs.pmove = *in;
        out->error = {};

        out->viewheight[ 0 ] = out->viewheight[ 1 ] = clgi.client->frame.ps.pmove.viewheight; //out->view_current_height = out->view_previous_height = clgi.client->frame.ps.pmove.viewheight;
        //out->viewheight = clgi.client->frame.ps.pmove.viewheight;
        out->time.height_changed = clgi.client->time;

        out->step = 0;
        out->time.step_changed = clgi.GetRealTime();

        #if USE_DEBUG
        clgi.Print( PRINT_NOTICE, "First frame(%" PRIi64 "). Nothing to predict yet.\n",
            clgi.client->frame.number );
        //clgi.ShowMiss( "First frame(%" PRIi64 ") frame #(%i). Nothing to predict yet.\n",
        //    clgi.client->frame.number );
        #endif
        return;
    }

    // Subtract what the server returned from our predicted origin for that frame.
    out->error = moveCommand->prediction.error = moveCommand->prediction.origin - in->origin;

    // Save the prediction error for interpolation.
    //const float len = fabs( delta[ 0 ] ) + abs( delta[ 1 ] ) + abs( delta[ 2 ] );
    const float len = QM_Vector3Length( out->error );
    //if (len < 1 || len > 640) {
    if ( len > .1f ) {
        // Snap back if the distance was too far off:
        if ( len > MAX_DELTA_ORIGIN ) {
            // Debug misses:
            #if USE_DEBUG
                clgi.ShowMiss( "MAX_DELTA_ORIGIN on frame #(%" PRIi64 "): len(%f) (%f %f %f)\n",
                    clgi.client->frame.number, len, out->error[ 0 ], out->error[ 1 ], out->error[ 2 ] );
            #endif
            //out->view.origin = in->origin; // clgi.client->frame.ps.pmove.origin;
            //out->view.offset = clgi.client->frame.ps.viewoffset;
            //out->view.angles = clgi.client->frame.ps.viewangles;
            //out->view.height[0] = out->view.height[ 1 ] = in->viewheight;//out->view_previous_height = in->viewheight; // clgi.client->frame.ps.pmove.viewheight;
            ////out->view_height_time = clgi.client->time; // clgi.GetRealTime();
            //out->view.velocity = in->velocity; // clgi.client->frame.ps.pmove.velocity;
            //out->error = {};

            //out->view.rdflags = 0;
            //out->view.screen_blend = {};
            out->lastPs = clgi.client->frame.ps;
            out->currentPs = clgi.client->frame.ps;
            //out->currentPs.pmove.origin = in->origin;
            //out->currentPs.pmove.velocity = in->velocity;
            out->currentPs.pmove = *in;
            out->error = {};

            out->viewheight[ 0 ] = out->viewheight[ 1 ] = clgi.client->frame.ps.pmove.viewheight; //out->view_current_height = out->view_previous_height = clgi.client->frame.ps.pmove.viewheight;
            //out->viewheight = clgi.client->frame.ps.pmove.viewheight;
            out->time.height_changed = clgi.client->time;

            out->step = 0;
            out->time.step_changed = clgi.GetRealTime();

            return;
        // In case of a minor distance, when cl_showmiss is enabled, report:
        } else {
            // Debug misses:
            #if USE_DEBUG
                clgi.ShowMiss( "Prediction miss on frame #(%" PRIi64 "): len(%f) (%f %f %f)\n",
                    clgi.client->frame.number, len, out->error[ 0 ], out->error[ 1 ], out->error[ 2 ] );
            #endif
        }
    }
}

/**
*   @brief  Sets the predicted view angles.
**/
void PF_PredictAngles( void ) {
    // Don't predict angles if the pmove asks so specifically.
    if ( ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION )/* || !cl_predict->integer*/ ) {
        VectorCopy( clgi.client->frame.ps.viewangles, clgi.client->predictedState.currentPs.viewangles );
        return;
    }

    // This is done even with cl_predict == 0.
	VectorAdd( clgi.client->viewangles, clgi.client->frame.ps.pmove.delta_angles, clgi.client->predictedState.currentPs.viewangles );
}

/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void PF_PredictMovement( uint64_t acknowledgedCommandNumber, const uint64_t currentCommandNumber ) {
    // Prepare the player move parameters.
    pmoveParams_t pmp;
    SG_ConfigurePlayerMoveParameters( &pmp );

    // Last predicted state.
    client_predicted_state_t *predictedState = &clgi.client->predictedState;

    // Some debug stuff.
    //if ( clgi.client->predictedState.groundEntity ) {
    //    clgi.Print( PRINT_DEVELOPER, "predictedState.groundEntity(#%d)\n", clgi.client->predictedState.groundEntity->current.number );
    //} else {
    //    clgi.Print( PRINT_DEVELOPER, "predictedState.groundEntity(%s)\n", " NONE " );
    //}

    // Shuffle current to last playerState.
    predictedState->lastPs = predictedState->currentPs;

    // Start off with the latest valid frame player state.
    player_state_t pmPlayerState = predictedState->currentPs = clgi.client->frame.ps;

    // Prepare our player move, setup the client side trace function pointers.
    pmove_t pm = {
        .playerState = &pmPlayerState,

        .trace = CLG_PM_Trace,
        .clip = CLG_PM_Clip,
        .pointcontents = CLG_PM_PointContents,
    };


    // Apply client delta_angles.
    pm.playerState->pmove.delta_angles = clgi.client->delta_angles;
    // Ensure viewheight is set properly also.
    pm.playerState->pmove.viewheight = /*predictedState->view_current_height;*/clgi.client->frame.ps.pmove.viewheight;
    // Set view angles.
    pm.playerState->viewangles = clgi.client->viewangles; // This gets recalculated during PMove, based on the 'usercmd' and server 'delta angles'.
    // Set view offset.
    pm.playerState->viewoffset = /*predictedState->pmove.viewOffset*/clgi.client->frame.ps.viewoffset;
    //predictionState.viewoffset = predictedState->view.viewOffset; //clgi.client->frame.ps.viewoffset;

    // Setup ground.
    pm.ground = predictedState->ground;
    // Setup liquid.
    pm.liquid = predictedState->liquid;

    // Run previously stored and acknowledged frames up and including the last one.
    while ( ++acknowledgedCommandNumber <= currentCommandNumber ) {
        // Get the acknowledged move command from our circular buffer.
        client_movecmd_t *moveCommand = &clgi.client->moveCommands[ acknowledgedCommandNumber & CMD_MASK ];

        // Only simulate it if it had movement.
        if ( moveCommand->cmd.msec ) {
            // Timestamp it so the client knows we have valid results.
            moveCommand->prediction.time = clgi.client->time;//clgi.client->time;

            // Simulate the movement.
            pm.cmd = moveCommand->cmd;
            SG_PlayerMove( &pm, &pmp );
        }

        // Save for prediction checking.
        moveCommand->prediction.origin = pm.playerState->pmove.origin;
        moveCommand->prediction.velocity = pm.playerState->pmove.velocity;
    }

    // Now run the pending command number.
    uint64_t frameNumber = currentCommandNumber; //! Default to current frame, expected behavior for if we got msec in predicedState.cmd
    client_movecmd_t *pendingMoveCommand = &clgi.client->moveCommand;
    if ( pendingMoveCommand->cmd.msec ) {
        // Store time of prediction.
        pendingMoveCommand->prediction.time = clgi.client->time;
        
        // Initialize pmove with the proper moveCommand data.
        pm.cmd = pendingMoveCommand->cmd;
        pm.cmd.forwardmove = clgi.client->localmove[ 0 ];
        pm.cmd.sidemove = clgi.client->localmove[ 1 ];
        pm.cmd.upmove = clgi.client->localmove[ 2 ];

        // Perform movement.
        SG_PlayerMove( &pm, &pmp );

        // Save the now not pending anymore move command as the last entry in our circular buffer.
        pendingMoveCommand->prediction.origin = pm.playerState->pmove.origin;
        pendingMoveCommand->prediction.velocity = pm.playerState->pmove.velocity;

        // Save for prediction checking.
        clgi.client->moveCommands[ ( currentCommandNumber + 1 ) & CMD_MASK ] = *pendingMoveCommand;
        // Use previous frame if no command is pending.
    } else {
        frameNumber = currentCommandNumber - 1;
    }


    // Smooth Out Stair Stepping. This is done before updating the ground data so we can test results to the
    // previously predicted ground data.
    CLG_PredictStepOffset( &pm, predictedState, pm.step_height );

    // Copy results out into the current predicted state.
    //predictedState->view.origin = pm.playerState.pmove.origin;
    //predictedState->view.velocity = pm.playerState.pmove.velocity;
    //predictedState->view.angles = pm.playerState.viewangles;
    //predictedState->view.offset = pm.playerState.viewoffset;

    //predictedState->view.screen_blend = pm.playerState.screen_blend; // // To be merged with server screen blend.
    //predictedState->view.rdflags = pm.playerState.rdflags; // To be merged with server rdflags.
    predictedState->ground = pm.ground;
    predictedState->liquid.level = pm.liquid.level;
    predictedState->liquid.type = pm.liquid.type;

    predictedState->mins = pm.mins;
    predictedState->maxs = pm.maxs;


    // Adjust the view height to the new state's viewheight. If it changed, record moment in time.
    PF_AdjustViewHeight( pm.playerState->pmove.viewheight );

    // Swap in the resulting new pmove player state.
    predictedState->currentPs = pmPlayerState;
    //// Start off with the latest valid frame player state.
    //player_state_t pmPlayerState = predictedState->currentPs = clgi.client->frame.ps;
}