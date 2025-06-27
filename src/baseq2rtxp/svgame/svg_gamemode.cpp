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
const bool SVG_GameModeAllowSaveGames( const bool isDedicatedServer ) {
	// Singleplayer mode always allows saving.
	if ( gamemode->integer == GAMEMODE_TYPE_SINGLEPLAYER ) {
		return true;
	}
	// A dedicated server only allows saving in coop mode.
	if ( dedicated->integer && gamemode->integer == GAMEMODE_TYPE_COOPERATIVE ) {
		return true;
	}
	// } else {
	//		...
	// }
	// Don't allow saving on deathmatch and beyond.
	if ( gamemode->integer >= GAMEMODE_TYPE_DEATHMATCH ) {
		return false;
	}

	// Don't allow.
	return false;
}