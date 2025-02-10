/********************************************************************
*
*
*	SharedGame: Game Mode
*
*
********************************************************************/
#include "shared/shared.h"
#include "sharedgame/sg_shared.h"

/**
*	@return	True if the game mode is a legitimate existing one.
**/
const bool SG_IsValidGameModeType( const sg_gamemode_type_t gameModeType ) {
	if ( gameModeType >= GAMEMODE_TYPE_SINGLEPLAYER && gameModeType < GAMEMODE_TYPE_MAX ) {
		return true;
	}

	return false;
}
/**
*   @return True if the game mode is multiplayer.
**/
const bool SG_IsMultiplayerGameMode( const sg_gamemode_type_t gameModeType ) {
	// All indices above GAMEMODE_SINGLEPLAYER are multiplayer modes.
	if ( gameModeType > GAMEMODE_TYPE_SINGLEPLAYER && gameModeType < GAMEMODE_TYPE_MAX ) {
		// Multiplayer mode.
		return true;
	}
	// Singleplayer mode.
	return false;
}
/**
*	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
**/
const sg_gamemode_type_t SG_GetDefaultMultiplayerGameModeType() {
	// Default to Deathmatch.
	return GAMEMODE_TYPE_DEATHMATCH;
}

/**
*	@return	The actual Type of the current gamemode.
**/
const sg_gamemode_type_t SG_GetActiveGameModeType() {
	// Acquire gamemode cvar.
	cvar_t *gamemode = SG_CVar_Get( "gamemode", nullptr, 0 );

	if ( gamemode->integer >= GAMEMODE_TYPE_SINGLEPLAYER && gamemode->integer < GAMEMODE_TYPE_MAX ) {
		return static_cast<sg_gamemode_type_t>( gamemode->integer );
	}

	// Unknown gamemode.
	return GAMEMODE_TYPE_UNKNOWN;
}

/**
*	@return	A string representative of the passed in gameModeType.
**/
const char *SG_GetGameModeName( sg_gamemode_type_t gameModeType ) {
	// CooperativeL
	if ( gameModeType == GAMEMODE_TYPE_COOPERATIVE ) {
		return "Cooperative";
	// Deathmatch:
	} else if ( gameModeType == GAMEMODE_TYPE_DEATHMATCH ) {
		return "Deathmatch";
	// Unknown: If this ever happens you got things to be concerned about.
	} else if ( gameModeType == GAMEMODE_TYPE_UNKNOWN ) {
		return "(Warning: Unknown)";
	// Default, Singleplayer
	} else {
		return "SinglePlayer";
	}
}