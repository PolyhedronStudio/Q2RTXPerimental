/********************************************************************
*
*
*	ClientGame: Player Movement Prediction Simulation Implementation.
*
*
********************************************************************/
#include "clg_local.h"

/**
*   @brief  Player Move specific 'Trace' wrapper implementation.
**/
static const trace_t q_gameabi CLG_PM_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const int32_t contentMask ) {
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
static const trace_t q_gameabi CLG_PM_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const int32_t contentmask ) {
    trace_t trace;
    trace = clgi.Clip( start, mins, maxs, end, nullptr, contentmask );
    return trace;
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const int32_t q_gameabi CLG_PM_PointContents( const vec3_t point ) {
    return clgi.PointContents( point );
}

/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void PF_AdjustViewHeight( const int32_t viewHeight ) {
    // Record viewheight changes.
    if ( clgi.client->predictedState.view_height != viewHeight ) {
        clgi.client->predictedState.view_height = viewHeight;
        clgi.client->predictedState.view_height_time = clgi.client->time;
    }
}

/**
*   @brief  Sets the predicted view angles.
**/
void PF_PredictAngles( void ) {
    // Don't predict angles if the pmove asks so specifically.
    if ( ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION )/* || !cl_predict->integer*/ ) {
        VectorCopy( clgi.client->frame.ps.viewangles, clgi.client->predictedState.view.angles );
        return;
    }

    // This is done even with cl_predict == 0.
	VectorAdd( clgi.client->viewangles, clgi.client->frame.ps.pmove.delta_angles, clgi.client->predictedState.view.angles );
}

/**
*	@return	False if prediction is not desired for. True if it is.
**/
const bool PF_UsePrediction( void ) {
	// We're playing a demo, there is nothing to predict.
	if ( clgi.IsDemoPlayback() ) {
		return false;
	}

	// When the server is paused, nothing to predict, make sure to clear any possible predicted state error.
	if ( sv_paused->integer ) {
		clgi.client->predictedState.error = {};
		return false;
	}

	// Prediction is disabled by user, or received frame playerstate.
	if ( !cl_predict->integer || ( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION ) ) {
		return false;
	}

	return true;
}

/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void PF_PredictMovement( uint64_t acknowledgedCommandNumber, const uint64_t currentCommandNumber ) {
    // Time in miliseconds to lerp the step with.
    static constexpr int32_t PM_STEP_TIME = 100;
    // Maximum -/+ change we allow in step lerps.
    static constexpr int32_t PM_MAX_STEP_CHANGE = 32;

    // Last predicted state.
    client_predicted_state_t *predictedState = &clgi.client->predictedState;

    // Prepare our player move, setup the client side trace function pointers.
    pmove_t pm = {};
    pm.trace = CLG_PM_Trace;
    pm.pointcontents = CLG_PM_PointContents;
    pm.clip = CLG_PM_Clip;

    // Copy over the current client state data into pmove.
    pm.s = clgi.client->frame.ps.pmove;
    // Apply client delta_angles.
    pm.s.delta_angles = clgi.client->delta_angles;
    // Ensure viewoffset is set properly also.
    pm.s.viewheight = predictedState->view_height;
    // Set view offset.
    pm.viewoffset = predictedState->view.viewOffset;
    // Set ground entity to last predicted one.
    pm.groundentity = (edict_s*)predictedState->groundEntity;
    // Set ground plane to last predicted one.
    pm.groundplane = predictedState->groundPlane;

    // Run previously stored and acknowledged frames up and including the last one.
    while ( ++acknowledgedCommandNumber <= currentCommandNumber ) {
        // Get the acknowledged move command from our circular buffer.
        client_movecmd_t *moveCommand = &clgi.client->moveCommands[ acknowledgedCommandNumber & CMD_MASK ];

        // Only simulate it if it had movement.
        if ( moveCommand->cmd.msec ) {
            // Timestamp it so the client knows we have valid results.
            moveCommand->prediction.time = clgi.client->time;

            // Simulate the movement.
            pm.cmd = moveCommand->cmd;
            SG_PlayerMove( &pm, &clgi.client->pmp );
        }

        // Save for prediction checking.
        moveCommand->prediction.origin = pm.s.origin;
    }

    // Now run the pending command number.
    uint64_t frameNumber = currentCommandNumber; //! Default to current frame, expected behavior for if we got msec in predicedState.cmd
    client_movecmd_t *moveCommand = &clgi.client->moveCommand;
    if ( moveCommand->cmd.msec ) {
        // Store time of prediction.
        moveCommand->prediction.time = clgi.client->time;

        // Initialize pmove with the proper moveCommand data.
        pm.cmd = clgi.client->moveCommand.cmd;
        pm.cmd.forwardmove = clgi.client->localmove[ 0 ];
        pm.cmd.sidemove = clgi.client->localmove[ 1 ];
        pm.cmd.upmove = clgi.client->localmove[ 2 ];

        // Perform movement.
        SG_PlayerMove( &pm, &clgi.client->pmp );

        // Save for prediction checking.
        //clgi.client->moveCommands[ ( currentCommandNumber + 1 ) & CMD_MASK ].prediction.origin = pm.s.origin;

        // Save the now not pending anymore move command as the last entry in our circular buffer.
        clgi.client->moveCommand.prediction.origin = pm.s.origin;
        clgi.client->moveCommands[ ( currentCommandNumber + 1 ) & CMD_MASK ] = *moveCommand;
    // Use previous frame if no command is pending.
    } else {
        frameNumber = currentCommandNumber - 1;
    }

    // Stair Stepping:
    const float oldZ = clgi.client->moveCommands[ frameNumber & CMD_MASK ].prediction.origin[ 2 ];
    const float step = pm.s.origin[ 2 ] - oldZ;
    const float fabsStep = fabsf( step );

    // Consider a Z change being "stepping" if...
    const bool step_detected = ( fabsStep > PM_MIN_STEP_SIZE && fabsStep < PM_MAX_STEP_SIZE ) // Absolute change is in this limited range
        && ( ( clgi.client->frame.ps.pmove.pm_flags & PMF_ON_GROUND ) || pm.step_clip ) // And we started off on the ground
        && ( ( pm.s.pm_flags & PMF_ON_GROUND ) && pm.s.pm_type <= PM_GRAPPLE ) // And are still predicted to be on the ground
        && ( memcmp( &predictedState->groundPlane, &pm.groundplane, sizeof( cplane_t ) ) != 0 // Plane memory isn't identical, or
            || predictedState->groundEntity != (centity_t *)pm.groundentity ); // we stand on another plane or entity

    // Code below adapted from Q3A. Smoothes out stair step.
    if ( step_detected ) {
        // check for stepping up before a previous step is completed
        const float delta = clgi.GetRealTime() - predictedState->step_time;
        float old_step = 0.f;
        if ( delta < PM_STEP_TIME ) {
            old_step = predictedState->step * ( PM_STEP_TIME - delta ) / PM_STEP_TIME;
        } else {
            old_step = 0;
        }

        // add this amount
        predictedState->step = constclamp( old_step + step, -PM_MAX_STEP_CHANGE, PM_MAX_STEP_CHANGE );
        predictedState->step_time = clgi.GetRealTime();
    }

    if ( pm.s.pm_flags ) {
        int32_t oldGroundEntityNumber = SG_GetEntityNumber( (sgentity_s*)predictedState->groundEntity );
        int32_t groundEntityNumber = SG_GetEntityNumber( (sgentity_s *)pm.groundentity );
        SG_DPrintf( "Ground Entity changed from(%d) to (%d)!!!\n", oldGroundEntityNumber, groundEntityNumber );
    }
    // Copy results out into the current predicted state.
    predictedState->view.origin = pm.s.origin;
    predictedState->view.velocity = pm.s.velocity;
    predictedState->view.angles = pm.viewangles;
    predictedState->view.viewOffset = pm.viewoffset;

    predictedState->view.screen_blend = pm.screen_blend; // // To be merged with server screen blend.
    predictedState->view.rdflags = pm.rdflags; // To be merged with server rdflags.
    
    predictedState->groundEntity = (centity_t *)pm.groundentity;
    predictedState->groundPlane = pm.groundplane;

    // Adjust the view height to the new state's viewheight. If it changed, record moment in time.
    PF_AdjustViewHeight( pm.s.viewheight );
}