/********************************************************************
*
*
*	SharedGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "sharedgame/sg_gamemode.h"

#include "svgame/svg_gamemode.h"



/**
*
*
*
*	SinglePlayer GameMode(Also the default for Unknown ID -1):
*
*
*
**/
/**
*	@brief	Called once by client during connection to the server. Called once by
*			the server when a new game is started.
**/
void svg_singleplayer_gamemode_t::PrepareCVars() {
	// Non-deathmatch, non-coop is one player.
	//Cvar_FullSet( "maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH, FROM_CODE );
	gi.cvar_forceset( "maxclients", "1" );
}



/**
*
*
*
*	Cooperative GameMode:
*
*
*
**/
/**
*	@brief	Called once by client during connection to the server. Called once by
*			the server when a new game is started.
**/
void svg_cooperative_gamemode_t::PrepareCVars() {
	// Set the CVar for keeping old code in-tact. TODO: Remove in the future.
	gi.cvar_forceset( "coop", "1" );

	// Setup maxclients correctly.
	if ( maxclients->integer <= 1 || maxclients->integer > 4 ) {
		gi.cvar_forceset( "maxclients", "4" ); // Cvar_Set( "maxclients", "4" );
	}
}


/**
*
*
*
*	DeathMatch GameMode:
*
*
*
**/
/**
*	@brief	Called once by client during connection to the server. Called once by
*			the server when a new game is started.
**/
void svg_deathmatch_gamemode_t::PrepareCVars() {
	// Set the CVar for keeping old code in-tact. TODO: Remove in the future.
	gi.cvar_forceset( "deathmatch", "1" );

	// Setup maxclients correctly.
	if ( maxclients->integer <= 1 ) {
		gi.cvar_forceset( "maxclients", "8" ); //Cvar_SetInteger( maxclients, 8, FROM_CODE );
	} else if ( maxclients->integer > CLIENTNUM_RESERVED ) {
		gi.cvar_forceset( "maxclients", std::to_string( CLIENTNUM_RESERVED ).c_str() );
	}
}



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
sg_igamemode_t *SVG_AllocateGameModeInstance( const sg_gamemode_type_t gameModeType ) {
	switch ( gameModeType ) {
	case GAMEMODE_TYPE_SINGLEPLAYER:
		return new svg_singleplayer_gamemode_t();
	case GAMEMODE_TYPE_COOPERATIVE:
		return new svg_cooperative_gamemode_t();
	case GAMEMODE_TYPE_DEATHMATCH:
		return new svg_deathmatch_gamemode_t();
	default:
		// Unknown gamemode, return nullptr.
		return nullptr;
	}
}

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