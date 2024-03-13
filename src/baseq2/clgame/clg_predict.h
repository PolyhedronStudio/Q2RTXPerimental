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
void PF_AdjustViewHeight( const int32_t viewHeight );
/**
*   @brief  Checks for prediction if desired. Will determine the error margin
*           between our predicted state and the server returned state. In case
*           the margin is too high, snap back to server provided player state.
**/
void PF_CheckPredictionError( const int64_t frameIndex, const uint64_t commandIndex, const pmove_state_t *in, struct client_movecmd_s *moveCommand, client_predicted_state_t *out );
/**
*   @brief  Sets the predicted view angles.
**/
void PF_PredictAngles();
/**
*	@return	False if prediction is not desired for. True if it is.
**/
const qboolean PF_UsePrediction();
/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void PF_PredictMovement( uint64_t acknowledgedCommandNumber, const uint64_t currentCommandNumber );
