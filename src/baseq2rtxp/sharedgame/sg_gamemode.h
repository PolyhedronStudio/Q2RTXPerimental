/********************************************************************
*
*
*	SharedGame: Game Mode
*
*
********************************************************************/
#pragma once

//
// g_gamemode.cpp
//
typedef enum sg_gamemode_type_e {
	//! If you ever get this value, something is badly wrong.
	GAMEMODE_TYPE_UNKNOWN = -1,

	//! Start or load an adventure of your own against monsters.
	GAMEMODE_TYPE_SINGLEPLAYER = 0,
	
	//! Player with other clients against monsters.
	GAMEMODE_TYPE_COOPERATIVE = 1,
	
	//! Play versus other clients.
	GAMEMODE_TYPE_DEATHMATCH = 2,

	//! Total limit of gamemodes. All gamemode IDs from 0 up to GAMEMODE_MAX are valid gamemodes for IsGameModeIDValid.
	GAMEMODE_TYPE_MAX = 3,
} sg_gamemode_type_t;

/**
*	@return	True if the game mode is a legitimate existing one.
**/
const bool SG_IsValidGameModeType( const sg_gamemode_type_t gameModeType );
/**
*   @return True if the game mode is multiplayer.
**/
const bool SG_IsMultiplayerGameMode( const sg_gamemode_type_t gameModeType );
/**
*	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
**/
const sg_gamemode_type_t SG_GetDefaultMultiplayerGameModeType();
/**
*	@return	The actual ID of the current gamemode type.
**/
const sg_gamemode_type_t SG_GetActiveGameModeType();

/**
*	@return	A string representative of the passed in gameModeType.
**/
const char *SG_GetGameModeName( const sg_gamemode_type_t gameModeType );