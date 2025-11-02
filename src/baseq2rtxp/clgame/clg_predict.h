/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void CLG_AdjustViewHeight( const int32_t viewHeight );
/**
*   @brief  Checks for prediction if desired. Will determine the error margin
*           between our predicted state and the server returned state. In case
*           the margin is too high, snap back to server provided player state.
**/
void CLG_CheckPredictionError( const int64_t frameIndex, const int64_t commandIndex, struct client_movecmd_s *moveCommand );
/**
*   @brief  Sets the predicted view angles.
**/
void CLG_PredictAngles();
/**
*	@return	False if prediction is not desired for. True if it is.
**/
const qboolean CLG_UsePrediction();
/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void CLG_PredictMovement( int64_t acknowledgedCommandNumber, const int64_t currentCommandNumber );
