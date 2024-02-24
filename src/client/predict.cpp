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
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void CL_AdjustViewHeight( const int32_t viewHeight ) {
    clge->AdjustViewHeight( viewHeight );
}

/**
*   @brief  Checks for prediction if desired. Will determine the error margin
*           between our predicted state and the server returned state. In case
*           the margin is too high, snap back to server provided player state.
**/
void CL_CheckPredictionError(void) {
    // Inquire the client game for whether to use prediction or not.
    if ( !clge->UsePrediction() ) {
        return;
    }

    // Get a pointer to the received 'in' frame player move state.
    const pmove_state_t *in = &cl.frame.ps.pmove;
    // Get a pointer to the predicted state.
    client_predicted_state_t *out = &cl.predictedState;

	// Calculate the last usercmd_t we sent that the server has processed.
	int64_t frameIndex = cls.netchan.incoming_acknowledged & CMD_MASK;
    // Get the move command index for this frame in history.
	uint64_t commandIndex = cl.history[ frameIndex ].commandNumber;
    // Finally get the pointer to the actual move command.
    client_movecmd_t *moveCommand = &cl.moveCommands[ commandIndex & CMD_MASK ];

    // Pass control to client game to implement its own error corrections.
    clge->CheckPredictionError( frameIndex, commandIndex, in, moveCommand, out );
}

/**
*   @brief  Sets the predicted view angles.
**/
void CL_PredictAngles(void) {
    clge->PredictAngles( );
}

/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void CL_PredictMovement(void) {
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
}

