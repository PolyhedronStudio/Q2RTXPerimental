#include "g_local.h"

/**
*	@return	A string representative of the current gamemode ID. 
**/
const char *G_GetGamemodeName( ) {
	// CooperativeL
	if ( gamemode->integer == 1 ) {
		return "Cooperative";
	// Deathmatch:
	} else if ( gamemode->integer == 2 ) {
		return "Deathmatch";
	// Default, Singleplayer
	} else {
		return "Singleplayer";
	}
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
	if ( gamemode->integer == 1 ) {
		return true;
	}

	// Default is to allow savegames. (Singleplayer mode.)
	return false;
}