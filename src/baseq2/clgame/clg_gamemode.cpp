/********************************************************************
*
*
*	ClientGame: Gamemode Implementation(s).
*
*
********************************************************************/
#include "clg_local.h"

extern cvar_t *gamemode;

/**
*	@return	The actual ID of the current gamemode.
**/
const int32_t G_GetActiveGamemodeID() {
	if ( gamemode && gamemode->integer >= GAMEMODE_SINGLEPLAYER && gamemode->integer <= GAMEMODE_COOPERATIVE ) {
		return gamemode->integer;
	}

	// Unknown gamemode.
	return -1;
}