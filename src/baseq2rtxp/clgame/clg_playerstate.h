/********************************************************************
*
*
*	ClientGame:	General Entities Header
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	PlayerState Updating:
*
*
*
**/
/**
*   @brief  Handles player state transition ON the given client entity
*           (thus can be predicted player entity, or the frame's playerstate client number matching entitiy)
*           between old and new server frames.
*           Duplicates old state into current state in case of no lerping conditions being met.
*   @param clent Pointer to the client entity on which the playerstates of each frame are to be processed.
*   @note   This is used for demo playerback, cl_nopredict = 0, as well as in the prediction codepath.
**/
void CLG_PlayerState_Transition( centity_t *clent, server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv );