/********************************************************************
*
*
*	ClientGame:	Input Header
*
*
********************************************************************/
#pragma once

/**
*	@brief	Called upon when mouse movement is detected.
**/
void PF_MouseMove( const float deltaX, const float deltaY, const float moveX, const float moveY, const float speed );
/**
*	@brief	Called upon to register movement commands.
**/
void PF_RegisterUserInput( void );
/**
*   @brief  Updates msec, angles and builds the interpolated movement vector for local movement prediction.
*           Doesn't touch command forward/side/upmove, these are filled by CL_FinalizeCommand.
**/
void PF_UpdateMoveCommand( const int64_t msec, client_movecmd_t *moveCommand, client_mouse_motion_t *mouseMotion );
/**
*   @brief  Builds the actual movement vector for sending to server. Assumes that msec
*           and angles are already set for this frame by CL_UpdateCommand.
**/
void PF_FinalizeMoveCommand( client_movecmd_t *moveCommand );
/**
*   @brief
**/
void PF_ClearMoveCommand( client_movecmd_t *moveCommand );