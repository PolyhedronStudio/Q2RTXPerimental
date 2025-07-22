/********************************************************************
*
*
*	SharedGame: GameMode Related Stuff.
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
#include "svgame/gamemodes/svg_gm_singleplayer.h"

#include "svgame/entities/target/svg_target_changelevel.h"
#include "svgame/entities/svg_player_edict.h"



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
*	@brief	Returns true if this gamemode allows saving the game.
**/
const bool svg_gamemode_singleplayer_t::AllowSaveGames() const {
	return true;
}

/**
*	@brief	Called once by client during connection to the server. Called once by
*			the server when a new game is started.
**/
void svg_gamemode_singleplayer_t::PrepareCVars() {
	// Non-deathmatch, non-coop is one player.
	//Cvar_FullSet( "maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH, FROM_CODE );
	gi.cvar_forceset( "maxclients", "1" );
}


/**
*   @details    Called when a player begins connecting to the server.
*
*               The game can refuse entrance to a client by returning false.
*
*               If the client is allowed, the connection process will continue
*               and eventually get to ClientBegin()
*
*               Changing levels will NOT cause this to be called again, but
*               loadgames WILL.
**/
const bool svg_gamemode_singleplayer_t::ClientConnect( svg_player_edict_t *ent, char *userinfo ) {
	/**
	*	No password check in singleplayer.
	**/
	// They can connect
	ent->client = &game.clients[ g_edict_pool.NumberForEdict( ent ) - 1 ];

	// If there is already a body waiting for us (That means, this is a loadgame) use it.
	// Otherwise spawn a new one from scratch
	if ( ent->inuse == false ) {
		// Clear the respawning variables
		SVG_Player_InitRespawnData( ent->client );
		// Get persistent data for the player from scratch.
		if ( !game.autosaved || !ent->client->pers.weapon ) {
			SVG_Player_InitPersistantData( ent, ent->client );
		}
	}

	// Make sure we start with known default(s)
	SVG_Client_UserinfoChanged( ent, userinfo );

	// Make sure we start with known default(s):
	// We're a player.
	ent->svflags = SVF_PLAYER;
	ent->s.entityType = ET_PLAYER;

	// We're connected.
	ent->client->pers.connected = true;

	// Connected.
	return true;
}
/**
*	@brief	Called somewhere at the beginning of the game frame. This allows
*			to determine if conditions are met to engage exitting intermission
*			mode and/or exit the level.
**/
void svg_gamemode_singleplayer_t::PreCheckGameRuleConditions() {
	// Perform the base gamemode checks first.
	//svg_gamemode_t::PreCheckGameRuleConditions();
	// Now the game mode specific checks.
	
	// Exit intermission.
	if ( level.exitintermission ) {
		ExitLevel();
		return;
	}
}
/**
*	@brief	Called somewhere at the end of the game frame. This allows
*			to determine if conditions are met to engage into intermission
*			mode and/or exit the level.
**/
void svg_gamemode_singleplayer_t::PostCheckGameRuleConditions() {
	// Perform the base gamemode checks first.
	//svg_gamemode_t::PostCheckGameRuleConditions();
};