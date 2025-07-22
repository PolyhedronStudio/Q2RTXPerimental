/*********************************************************************
*
*
*	SVGame: GameMode(s):
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	Game Mode API:
*
*
*
**/
/**
*	@brief	Factory function to create a game mode object based on the gameModeType.
**/
svg_gamemode_t *SVG_AllocateGameModeInstance( const sg_gamemode_type_t gameModeType );

/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool SVG_GameModeAllowSaveGames( const bool isDedicatedServer );