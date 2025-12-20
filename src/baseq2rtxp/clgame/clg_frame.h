/********************************************************************
*
*
*	ClientGame: Server Frames(SnapShots):
*
*
********************************************************************/
#pragma once




/**
*   @brief  Prepares the client state for 'activation', also setting the predicted values
*           to those of the initially received first valid frame.
**/
void CLG_Frame_SetInitialServerFrame( void );

/**
*   @brief  Called after finished parsing the frame data. All entity states and the
*           player state will be transitioned and/or reset as needed, to make sure
*           the client has a proper view of the current server frame. It does the following:
*               - Rebuilds the solid entity list for this frame.
*               - Updates all entity states for this frame.
*               - Fires any entity events for this frame.
*               - Updates the predicted frame.
*               - Initializes the player's own entity position from the received frame's playerstate.
*               - Lerps or snaps the playerstate between the old and current frame.
*
*				- If demo playback, handles demo frame recording and demo freelook hack.
*
*				- Grabs mouse if player move type changed between frames.
*				- Notifies screen about the delta frame.
*
*               - Checks for prediction errors.
**/
void CLG_Frame_TransitionToNext( void );