/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "cl_client.h"

/**
*   @brief  Checks for prediction if desired. Will determine the error margin
*           between our predicted state and the server returned state. In case
*           the margin is too high, snap back to server provided player state.
**/
void CL_CheckPredictionError(void) {
    // Maximum delta allowed before snapping back.
    static constexpr double MAX_DELTA_ORIGIN = ( 2400 * ( 1.0 / BASE_FRAMERATE ) );

    // Inquire the client game for whether to use prediction or not.
    if ( !clge->UsePrediction() ) {
        return;
    }

    const pmove_state_t *in = &cl.frame.ps.pmove;
    client_predicted_state_t *out = &cl.predictedState;

	// Calculate the last usercmd_t we sent that the server has processed.
	int64_t frameIndex = cls.netchan.incoming_acknowledged & CMD_MASK;
    // Move command index for this frame in history.
	uint64_t commandIndex = cl.history[ frameIndex ].commandNumber;
    // Get the move command.
    client_movecmd_t *moveCommand = &cl.moveCommands[ commandIndex & CMD_MASK ];

    // If it is the first frame, we got nothing to predict yet.
    if ( moveCommand->prediction.time == 0 ) {
        out->angles = cl.frame.ps.viewangles;
        out->origin = in->origin;
        out->velocity = in->velocity;
        out->error = {};

        out->step_time = 0;
        out->step = 0;
        out->rdflags = 0;
        out->screen_blend = {};
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
            SHOWMISS( "MAX_DELTA_ORIGIN on frame #(%i): len(%f) (%f %f %f)\n",
                cl.frame.number, len, out->error[ 0 ], out->error[ 1 ], out->error[ 2 ] );

            out->angles = cl.frame.ps.viewangles;
            out->origin = in->origin;
            out->velocity = in->velocity;
            out->error = {};

            out->step_time = 0;
            out->step = 0;
            out->rdflags = 0;
            out->screen_blend = {};
        // In case of a minor distance, only report if cl_showmiss is enabled:
        } else {
            // Debug misses:
            SHOWMISS( "prediction miss on frame #(%i): len(%f) (%f %f %f)\n",
            		 cl.frame.number, len, out->error[ 0 ], out->error[ 1 ], out->error[ 2 ] );
        }
	}
}

/**
*   @brief  Sets the predicted view angles.
**/
void CL_PredictAngles(void) {
    clge->PredictAngles( );
}

/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void CL_AdjustViewHeight( const int32_t viewHeight ) {
    clge->AdjustViewHeight( viewHeight );
}

/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void CL_PredictMovement(void) {
    static constexpr float STEP_MIN_HEIGHT = 4.f;
    static constexpr float STEP_MAX_HEIGHT = 18.0f;

    static constexpr int32_t STEP_TIME = 100;
    static constexpr int32_t MAX_STEP_CHANGE = 32;

    // Can't predict anything without being in 'active client state' first.
    if ( cls.state != ca_active ) {
        return;
    }

    if ( !clge->UsePrediction() ) {
        // just set angles
        CL_PredictAngles();
        return;
    }

	uint64_t acknowledgedCommandNumber = cl.history[ cls.netchan.incoming_acknowledged & CMD_MASK ].commandNumber;
	const uint64_t currentCommandNumber = cl.currentUserCommandNumber;

    // if we are too far out of date, just freeze
    if ( currentCommandNumber - acknowledgedCommandNumber > CMD_BACKUP - 1 ) {
        SHOWMISS("%i: exceeded CMD_BACKUP\n", cl.frame.number);
        return;
    }

    if ( !cl.moveCommand.cmd.msec && currentCommandNumber == acknowledgedCommandNumber ) {
        SHOWMISS("%i: not moved\n", cl.frame.number);
        return;
    }

    clge->PredictMovement( acknowledgedCommandNumber, currentCommandNumber );
 //   // Prepare our player move, setup the client side trace function pointers.
	//pmove_t pm = {};
 //   pm.trace = CL_PM_Trace;
 //   pm.pointcontents = CL_PM_PointContents;
 //   pm.clip = CL_PM_Clip;

 //   // Copy over the current client state data into pmove.
 //   pm.s = cl.frame.ps.pmove;
 //   pm.s.delta_angles = cl.delta_angles;
 //   pm.viewoffset = cl.frame.ps.viewoffset;
 //   //pm.s.viewheight = cl.frame.ps.pmove.viewheight;

 //   // Run previously stored and acknowledged frames up and including the last one.
 //   while ( ++acknowledgedCommandNumber <= currentCommandNumber ) {
 //       // Get the acknowledged move command from our circular buffer.
 //       client_movecmd_t *moveCommand = &cl.moveCommands[ acknowledgedCommandNumber & CMD_MASK ];

 //       // Only simulate it if it had movement.
 //       if ( moveCommand->cmd.msec ) {
 //           // Timestamp it so the client knows we have valid results.
 //           moveCommand->prediction.time = cl.time;

 //           // Simulate the movement.
 //           pm.cmd = moveCommand->cmd;
 //           clge->PlayerMove( &pm, &cl.pmp );
 //       }
 //       
 //       // Save for prediction checking.
 //       moveCommand->prediction.origin = pm.s.origin;
 //   }

 //   // Now run the pending command number.
 //   uint64_t frameNumber = currentCommandNumber; //! Default to current frame, expected behavior for if we got msec in predicedState.cmd
 //   if ( cl.moveCommand.cmd.msec ) {
 //       // Store time of prediction.
 //       cl.moveCommand.prediction.time = cl.time;

 //       // Initialize pmove with the proper moveCommand data.
 //       pm.cmd = cl.moveCommand.cmd;
 //       pm.cmd.forwardmove = cl.localmove[ 0 ];
 //       pm.cmd.sidemove = cl.localmove[ 1 ];
 //       pm.cmd.upmove = cl.localmove[ 2 ];

 //       // Perform movement.
	//	clge->PlayerMove( &pm, &cl.pmp );

 //       // Save for prediction checking.
 //       cl.moveCommands[ ( currentCommandNumber + 1 ) & CMD_MASK ].prediction.origin = pm.s.origin;
 //       //cl.moveCommand.prediction.origin = pm.s.origin; //

 //       // Save the now not pending anymore move command as the last entry in our circular buffer.
 //       //cl.moveCommands[ ( currentCommandNumber + 1 ) & CMD_MASK ] = cl.moveCommand;
	//// Use previous frame if no command is pending.
 //   } else {
	//	frameNumber = currentCommandNumber - 1;
 //   }

	//// Stair Stepping:
 //   const float oldZ = cl.moveCommands[ frameNumber & CMD_MASK ].prediction.origin[ 2 ];
 //   const float step = pm.s.origin[ 2 ] - oldZ;
 //   const float fabsStep = fabsf( step );
 //   
 //   // Consider a Z change being "stepping" if...
 //   const bool step_detected = ( fabsStep > STEP_MIN_HEIGHT && fabsStep < STEP_MAX_HEIGHT ) // Absolute change is in this limited range
 //               && ( ( cl.frame.ps.pmove.pm_flags & PMF_ON_GROUND ) || pm.step_clip ) // And we started off on the ground
 //               && ( ( pm.s.pm_flags & PMF_ON_GROUND ) && pm.s.pm_type <= PM_GRAPPLE ) // And are still predicted to be on the ground
 //               && ( memcmp( &cl.lastGround.plane, &pm.groundplane, sizeof( cplane_t ) ) != 0 // Plane memory isn't identical, or
 //               || cl.lastGround.entity != (centity_t*)pm.groundentity ); // we stand on another plane or entity

 //   // Code below adapted from Q3A. Smoothes out stair step.
 //   if ( step_detected ) {
 //       // check for stepping up before a previous step is completed
 //       const float delta = cls.realtime - cl.predictedState.step_time;
 //       float old_step = 0.f;
 //       if ( delta < STEP_TIME ) {
 //           old_step = cl.predictedState.step * ( STEP_TIME - delta ) / STEP_TIME;
 //       } else {
 //           old_step = 0;
 //       }

 //       // add this amount
 //       cl.predictedState.step = constclamp( old_step + step, -MAX_STEP_CHANGE, MAX_STEP_CHANGE );
 //       cl.predictedState.step_time = cls.realtime;
 //   }

 //   // Copy results out into the current predicted state.
 //   cl.predictedState.origin = pm.s.origin;
 //   cl.predictedState.angles = pm.viewangles;
 //   cl.predictedState.velocity = pm.s.velocity;
 //   cl.predictedState.screen_blend = pm.screen_blend; // // To be merged with server screen blend.
 //   cl.predictedState.rdflags = pm.rdflags; // To be merged with server rdflags.

 //   // Adjust the view height to the new state's height, if changing, record moment in time.
 //   CL_AdjustViewHeight( pm.s.viewheight );

 //   // Store resulting ground data.
 //   cl.lastGround.plane = pm.groundplane;
 //   cl.lastGround.entity = (centity_t *)pm.groundentity;
}

