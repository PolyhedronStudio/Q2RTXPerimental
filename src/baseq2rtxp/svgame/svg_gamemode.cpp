/********************************************************************
*
*
*	SharedGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "sharedgame/sg_gamemode.h"


/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool SVG_GetGamemodeNoSaveGames( const bool isDedicatedServer ) {
	// A dedicated server only allows saving in coop mode.
	if ( dedicated->integer && gamemode->integer != GAMEMODE_TYPE_COOPERATIVE ) {
		return false;
	}
	
	// Don't allow saving on deathmatch.
	if ( gamemode->integer == GAMEMODE_TYPE_DEATHMATCH ) {
		return true;
	}

	// Default is to allow savegames. (Singleplayer mode.)
	return false;
}