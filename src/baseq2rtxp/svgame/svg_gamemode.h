/*********************************************************************
*
*
*	SVGame: GameMode(s):
*
*
********************************************************************/
#pragma once



/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool SVG_GetGamemodeNoSaveGames( const bool isDedicatedServer );