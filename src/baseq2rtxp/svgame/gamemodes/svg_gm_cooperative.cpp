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
#include "svgame/entities/target/svg_target_changelevel.h"
#include "svgame/entities/svg_player_edict.h"



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
*	@brief	Returns true if this gamemode allows saving the game.
**/
const bool svg_gamemode_cooperative_t::AllowSaveGames() const {
	// We only allow saving in coop mode if we're a dedicated server.
	return ( dedicated != nullptr && dedicated->value != 0 ? true : false );
}

/**
*	@brief	Called once by client during connection to the server. Called once by
*			the server when a new game is started.
**/
void svg_gamemode_cooperative_t::PrepareCVars() {
	// Set the CVar for keeping old code in-tact. TODO: Remove in the future.
	gi.cvar_forceset( "coop", "1" );

	// Setup maxclients correctly.
	if ( maxclients->integer <= 1 || maxclients->integer > 4 ) {
		gi.cvar_forceset( "maxclients", "4" ); // Cvar_Set( "maxclients", "4" );
	}
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
const bool svg_gamemode_cooperative_t::ClientConnect( svg_player_edict_t *ent, char *userinfo ) {
	/**
	*	@brief	Password and spectator checks.
	**/
	// Check for a spectator
	const char *value = Info_ValueForKey( userinfo, "spectator" );
	if ( /*deathmatch->value && */*value && strcmp( value, "0" ) ) {
		//
		// Check for a spectator password.
		if ( *spectator_password->string &&
			strcmp( spectator_password->string, "none" ) &&
			strcmp( spectator_password->string, value ) ) {
			Info_SetValueForKey( userinfo, "rejmsg", "Spectator password required or incorrect." );
			// Refused.
			return false;
		}

		// Count total spectators.
		int32_t numspec = 0;
		for ( int32_t i = 0; i < maxclients->value; i++ ) {
			// Get edict.
			svg_base_edict_t *ed = g_edict_pool.EdictForNumber( i + 1 );
			// Entity is a spectator.
			if ( ed != nullptr && ed->inuse && ed->client->pers.spectator ) {
				numspec++;
			}
		}

		if ( numspec >= maxspectators->value ) {
			Info_SetValueForKey( userinfo, "rejmsg", "Server spectator limit is full." );
			// Refused.
			return false;
		}
	} else {
		// Check for a regular password if any is set.
		value = Info_ValueForKey( userinfo, "password" );
		if ( *password->string && strcmp( password->string, "none" ) &&
			strcmp( password->string, value ) ) {
			Info_SetValueForKey( userinfo, "rejmsg", "Password required or incorrect." );
			// Refused.
			return false;
		}
	}

	/**
	*	(Re-)Initialize client and entity body.
	**/
	// They can connect.
	ent->client = &game.clients[ g_edict_pool.NumberForEdict( ent ) - 1 ];

	// If there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn a new one from scratch.
	if ( ent->inuse == false ) {
		// clear the respawning variables
		SVG_Player_InitRespawnData( ent->client );
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
void svg_gamemode_cooperative_t::ClientUserinfoChanged( svg_player_edict_t *ent, char *userinfo ) {
	// Set spectator
	char *strSpectator = Info_ValueForKey( userinfo, "spectator" );
	if ( /*deathmatch->value && */*strSpectator && strcmp( strSpectator, "0" ) ) {
		ent->client->pers.spectator = true;
	} else {
		ent->client->pers.spectator = false;
	}

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
void svg_gamemode_cooperative_t::PreCheckGameRuleConditions() {
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
void svg_gamemode_cooperative_t::PostCheckGameRuleConditions() {

};

/**
*	@brief	Sets the spawn origin and angles to that matching the found spawn point.
**/
svg_base_edict_t *svg_gamemode_cooperative_t::SelectSpawnPoint( svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles ) {
	svg_base_edict_t *spot = nullptr;

	// <Q2RTXP>: WID: Implement dmflags as gamemodeflags?
	//if ( (int)( dmflags->value ) & DF_SPAWN_FARTHEST ) {
	//	spot = SelectFarthestDeathmatchSpawnPoint();
	//} else {
		spot = SelectCoopSpawnPoint( ent );
	//}

	// Resort to seeking 'info_player_start' spot since the game modes found none.
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

/**
*	@brief  Will select a coop spawn point for the player.
**/
svg_base_edict_t *svg_gamemode_cooperative_t::SelectCoopSpawnPoint( svg_player_edict_t *ent ) {
	int     index;
	svg_base_edict_t *spot = nullptr;
	// WID: C++20: Added const.
	const char *target;

	index = g_edict_pool.NumberForEdict( ent ) - 1; // ent->client - game.clients;

	// player 0 starts in normal player spawn point
	if ( !index ) {
		return NULL;
	}
	spot = NULL;

	// assume there are four coop spots at each spawnpoint
	while ( 1 ) {
		spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_coop" );
		if ( !spot ) {
			return NULL;    // we didn't have enough...
		}
		target = (const char *)spot->targetname;
		if ( !target ) {
			target = "";
		}
		if ( Q_stricmp( game.spawnpoint, target ) == 0 ) {
			// this is a coop spawn point for one of the clients here
			index--;
			if ( !index ) {
				return spot;        // this is it
			}
		}
	}

	return spot;
}