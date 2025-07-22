/********************************************************************
*
*
*	ServerGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"

#include "svgame/svg_lua.h"

#include "sharedgame/sg_gamemode.h"
#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"
#include "svgame/gamemodes/svg_gm_cooperative.h"
#include "svgame/gamemodes/svg_gm_deathmatch.h"
#include "svgame/gamemodes/svg_gm_singleplayer.h"

#include "svgame/entities/target/svg_target_changelevel.h"
#include "svgame/entities/svg_player_edict.h"



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
svg_gamemode_t *SVG_AllocateGameModeInstance( const sg_gamemode_type_t gameModeType ) {
	switch ( gameModeType ) {
	case GAMEMODE_TYPE_SINGLEPLAYER:
		return new svg_gamemode_singleplayer_t();
	case GAMEMODE_TYPE_COOPERATIVE:
		return new svg_gamemode_cooperative_t();
	case GAMEMODE_TYPE_DEATHMATCH:
		return new svg_gamemode_deathmatch_t();
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
	// <Q2RTXP>: TODO: Pass argument to the game mode object and let it decide.
	// Right now we use the dedicated cvar to determine if we allow saving in coop mode.
	if ( !game.mode ) {
		// Don't allow saving if we don't have a game mode object.
		return false;
	}
	return game.mode->AllowSaveGames();
}