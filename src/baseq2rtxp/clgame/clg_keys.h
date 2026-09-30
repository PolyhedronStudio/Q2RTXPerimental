/********************************************************************
*
*
*	ClientGame: Key and Command Handling for HUD Weapon Selector.
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"

/**
*	@brief	Initialize ClientGame weapon selection console commands (slot1..slot7, cancelselect, etc.).
**/
void CLG_Keys_Init( void );

/**
*	@brief	Shutdown and remove ClientGame console commands.
**/
void CLG_Keys_Shutdown( void );

/**
*	@brief	Key event handler called from engine Key_Event during KEY_GAME.
*	@param	key		Key code (K_ESCAPE, '1'..'7', K_UPARROW, K_DOWNARROW, K_ENTER, 'e', etc.).
*	@param	down	True if key was pressed, false if released.
*	@return	True if consumed by ClientGame, false if passed through.
**/
const bool CLG_Keys_KeyEvent( const int32_t key, const bool down );
