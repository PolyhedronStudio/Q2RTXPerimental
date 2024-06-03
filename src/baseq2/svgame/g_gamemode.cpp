#include "g_local.h"

/**
*	@return	True if the game mode is a legitimate existing one.
**/
const bool G_IsValidGameModeType( const int32_t gameModeID ) {
	if ( gameModeID >= GAMEMODE_SINGLEPLAYER && gameModeID < GAMEMODE_MAX ) {
		return true;
	}

	return false;
}
/**
*   @return True if the game mode is multiplayer.
**/
const bool G_IsMultiplayerGameMode( const int32_t gameModeID ) {
	if ( gameModeID > GAMEMODE_SINGLEPLAYER && gameModeID < GAMEMODE_MAX ) {
		return true;
	}

	return false;
}
/**
*	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
**/
const int32_t G_GetDefaultMultiplayerGamemodeType() {
	// Default to Deathmatch.
	return GAMEMODE_DEATHMATCH;
}

/**
*	@return	The actual ID of the current gamemode.
**/
const int32_t G_GetActiveGameModeType( ) {
	if ( gamemode->integer >= GAMEMODE_SINGLEPLAYER && gamemode->integer < GAMEMODE_MAX ) {
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
	// A dedicated server only allows saving in coop mode.
	if ( dedicated->integer && gamemode->integer != GAMEMODE_COOPERATIVE ) {
		return false;
	}

	// Don't allow saving on deathmatch.
	if ( gamemode->integer == GAMEMODE_DEATHMATCH ) {
		return true;
	}

	// Default is to allow savegames. (Singleplayer mode.)
	return false;
}