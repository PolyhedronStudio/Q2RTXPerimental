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

	// Connected.
	return true;
}
/**
*   @brief  called whenever the player updates a userinfo variable.
*
*           The game can override any of the settings in place
*           (forcing skins or names, etc) before copying it off.
**/
void svg_gamemode_singleplayer_t::ClientUserinfoChanged( svg_player_edict_t *ent, char *userinfo ) {
	// Singleplayer has no spectators, so we don't need to check for that.
	#if 0
	// Set spectator
	char *strSpectator = Info_ValueForKey( userinfo, "spectator" );
	if ( /*deathmatch->value && */*strSpectator && strcmp( strSpectator, "0" ) ) {
		ent->client->pers.spectator = true;
	} else {
		ent->client->pers.spectator = false;
	}
	#endif // #if 0
	// Set character skin.
	const int32_t playernum = g_edict_pool.NumberForEdict( ent ) - 1;
	// Combine name and skin into a configstring.
	char *strSkin = Info_ValueForKey( userinfo, "skin" );
	gi.configstring( CS_PLAYERSKINS + playernum, va( "%s\\%s", ent->client->pers.netname, strSkin ) );

	// Reset the FOV in case it had been changed by any in-use weapon modes.
	SVG_Player_ResetPlayerStateFOV( ent->client );

	// Set weapon handedness
	char *strHand = Info_ValueForKey( userinfo, "hand" );
	if ( strlen( strHand ) ) {
		ent->client->pers.hand = atoi( strHand );
	}
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

/**
*	@brief	Sets the spawn origin and angles to that matching the found spawn point.
**/
svg_base_edict_t *svg_gamemode_singleplayer_t::SelectSpawnPoint( svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles ) {
	svg_base_edict_t *spot = nullptr;

	// Start seeking 'info_player_start' spot since this is singleplayer mode.
	if ( !spot ) {
		spot = svg_gamemode_t::SelectSpawnPoint( ent, origin, angles );
	}

	// If we found a spot, set the origin and angles.
	if ( spot ) {
		origin = spot->s.origin;
		angles = spot->s.angles;
	} else {
		// Debug print if no spawn point was found.
		gi.dprintf( "%s: No spawn point found for player %s, using default origin and angles.\n", __func__, ent->client->pers.netname );
		origin = Vector3( 0, 0, 10 ); // Default origin.
		angles = Vector3( 0, 0, 0 ); // Default angles.
	}

	return spot;
}