#include "g_local.h"

/**
*	@return	The actual ID of the current gamemode.
**/
const int32_t G_GetGamemodeID( ) {
	if ( gamemode->integer >= GAMEMODE_SINGLEPLAYER && gamemode->integer <= GAMEMODE_COOPERATIVE ) {
		return gamemode->integer;
	}

	// Unknown gamemode.
	return -1;
}

/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool G_GetGamemodeNoSaveGames( const bool isDedicatedServer ) {
	// Dedicated server mode only allows coop saving.
	if ( dedicated->integer && !coop->integer ) {
		return true;
	}

	// Don't allow saving on deathmatch either.
	if ( gamemode->integer == GAMEMODE_DEATHMATCH ) {
		return true;
	}

	// Default is to allow savegames. (Singleplayer mode.)
	return false;
}