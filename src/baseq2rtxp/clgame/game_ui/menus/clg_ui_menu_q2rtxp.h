/********************************************************************
*
*
*	ClientGame: Q2RTXP Menu Script MicroUI Drop-In Header:
*
*
********************************************************************/
#pragma once

extern "C" {
	#include "clgame/game_ui/microui-2.02/src/microui.h"
}



/**
*	@brief	Open the root MicroUI menu by script menu name (defaults to `"game"` when in-game or `"main"`).
*	@param	menuName	Optional script menu name from `q2rtxp.menu` (e.g. `"main"`, `"game"`, `"options"`, `"video"`).
**/
void CLG_MUI_OpenRootMenu( const char *menuName = nullptr );

/**
*	@brief	Open the root MicroUI menu (`"game"` when `inGameMenu` is true, `"main"` when false).
*	@param	inGameMenu	True to open `"game"` ("In-Game Menu"), false to open `"main"` ("Q2RTXPerimental").
**/
void CLG_MUI_OpenRootMenu( const bool inGameMenu );

/**
*	@brief	Push a Q2RTXP MicroUI menu onto the navigation stack by its `.menu` script name.
*	@param	menuName	Null-terminated script menu name (e.g. `"options"`, `"video"`, `"quit"`).
*	@return	True if the menu was found and pushed onto the stack.
**/
const bool CLG_MUI_PushMenuByName( const char *menuName );

/**
*	@brief	Pop the top-most Q2RTXP MicroUI menu from the stack, closing the menu system when empty.
**/
void CLG_MUI_PopMenu( void );

/**
*	@brief	Close all open MicroUI script menus and clear the menu navigation stack.
**/
void CLG_MUI_CloseAllMenus( void );

/**
*	@brief	Query whether any Q2RTXP MicroUI script menu is currently open on the stack.
*	@return	True if at least one menu is active on the stack.
**/
const bool CLG_MUI_IsMenuActive( void );

/**
*	@brief	Handle key events for active Q2RTXP MicroUI menus (Escape back/close and `bind` key capture).
*	@param	key		Engine key code (`K_ESCAPE`, etc.).
*	@param	isDown	True when the key is pressed, false when released.
*	@return	True if the key event was consumed by the Q2RTXP menu stack.
**/
const bool CLG_MUI_HandleKeyEvent( const int32_t key, const bool isDown );

/**
*	@brief	Draw the menu background image (`conback.png`) when disconnected / at the boot main menu.
**/
void CLG_MUI_DrawBackground( void );

/**
*	@brief	Process and emit MicroUI commands for the currently active Q2RTXP menu on the stack.
*	@param	ctx	Active MicroUI context (called between `mu_begin( ctx )` and `mu_end( ctx )`).
*	@return	True if a Q2RTXP menu is currently active and was rendered this frame.
**/
const bool CLG_MUI_ProcessQ2RTXPMenus( mu_Context *ctx );

