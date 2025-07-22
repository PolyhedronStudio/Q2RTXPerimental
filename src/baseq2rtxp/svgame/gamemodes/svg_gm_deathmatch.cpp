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
#include "svgame/gamemodes/svg_gm_deathmatch.h"
#include "svgame/entities/target/svg_target_changelevel.h"
#include "svgame/entities/svg_player_edict.h"



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
*	@brief	Returns true if this gamemode allows saving the game.
**/
const bool svg_gamemode_deathmatch_t::AllowSaveGames() const {
	return false;
}

/**
*	@brief	Called once by client during connection to the server. Called once by
*			the server when a new game is started.
**/
void svg_gamemode_deathmatch_t::PrepareCVars() {
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
*	@brief	Called somewhere at the beginning of the game frame. This allows
*			to determine if conditions are met to engage exitting intermission
*			mode and/or exit the level.
**/
void svg_gamemode_deathmatch_t::PreCheckGameRuleConditions() {
	// Exit intermission.
	if ( level.exitintermission ) {
		ExitLevel();
		return;
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
const bool svg_gamemode_deathmatch_t::ClientConnect( svg_player_edict_t *ent, char *userinfo ) {
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

	#if 0
	// make sure we start with known default(s)
	//ent->svflags = SVF_PLAYER;
	SVG_Client_UserinfoChanged( ent, userinfo );

	// Developer connection print.
	if ( game.maxclients > 1 ) {
		gi.dprintf( "%s connected\n", ent->client->pers.netname );
	}

	// Make sure we start with known default(s):
	// We're a player.
	ent->svflags = SVF_PLAYER;
	ent->s.entityType = ET_PLAYER;

	// We're connected.
	ent->client->pers.connected = true;
	#endif
	// Connected.
	return true;
}
/**
*	@brief	Called somewhere at the end of the game frame. This allows
*			to determine if conditions are met to engage into intermission
*			mode and/or exit the level.
**/
void svg_gamemode_deathmatch_t::PostCheckGameRuleConditions() {
	// If we're in an intermission, stop checking for rules.
	if ( level.intermissionFrameNumber ) {
		return;
	}

	if ( timelimit->value ) {
		if ( level.time >= QMTime::FromMinutes( timelimit->value * 60 ) ) {
			gi.bprintf( PRINT_HIGH, "Timelimit hit.\n" );
			EndDeathMatch();
			//EndDMLevel();
			return;
		}
	}

	if ( fraglimit->value ) {
		for ( int32_t i = 0; i < maxclients->value; i++ ) {
			svg_client_t *cl = &game.clients[ i ];
			svg_base_edict_t *cledict = g_edict_pool.EdictForNumber( i + 1 );//g_edicts + 1 + i;
			if ( !cledict || !cledict->inuse ) {
				continue;
			}

			if ( cl->resp.score >= fraglimit->value ) {
				gi.bprintf( PRINT_HIGH, "Fraglimit hit.\n" );
				EndDeathMatch();
				//EndDMLevel();
				return;
			}
		}
	}
};
/**
*	@brief	Ends the DeathMatch, switching to intermission mode, and finding
*			the next level to play. After which it will spawn a TargetChangeLevel entity.
**/
void svg_gamemode_deathmatch_t::EndDeathMatch() {
	svg_base_edict_t *ent;
	char *s, *t, *f;
	static const char *seps = " ,\n\r";

	// stay on same level flag
	if ( (int)dmflags->value & DF_SAME_LEVEL ) {
		SVG_HUD_BeginIntermission( CreateTargetChangeLevel( level.mapname ) );
		return;
	}

	// see if it's in the map list
	if ( *sv_maplist->string ) {
		s = strdup( sv_maplist->string );
		f = NULL;
		t = strtok( s, seps );
		while ( t != NULL ) {
			if ( Q_stricmp( t, level.mapname ) == 0 ) {
				// it's in the list, go to the next one
				t = strtok( NULL, seps );
				if ( t == NULL ) { // end of list, go to first one
					if ( f == NULL ) {// there isn't a first one, same level
						SVG_HUD_BeginIntermission( CreateTargetChangeLevel( level.mapname ) );
					} else {
						SVG_HUD_BeginIntermission( CreateTargetChangeLevel( f ) );
					}
				} else {
					SVG_HUD_BeginIntermission( CreateTargetChangeLevel( t ) );
				}
				free( s );
				return;
			}
			if ( !f ) {
				f = t;
			}
			t = strtok( NULL, seps );
		}
		free( s );
	}

	if ( level.nextmap[ 0 ] ) {// go to a specific map
		SVG_HUD_BeginIntermission( CreateTargetChangeLevel( level.nextmap ) );
	} else {  // search for a changelevel
		ent = SVG_Entities_Find( NULL, q_offsetof( svg_base_edict_t, classname ), "target_changelevel" );
		if ( !ent ) {
			// the map designer didn't include a changelevel,
			// so create a fake ent that goes back to the same level
			SVG_HUD_BeginIntermission( CreateTargetChangeLevel( level.mapname ) );
			return;
		}
		SVG_HUD_BeginIntermission( ent );
	}
}