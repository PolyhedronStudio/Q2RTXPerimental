/********************************************************************
*
*
*	SharedGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/player/svg_player_hud.h"

#include "svgame/svg_lua.h"

#include "sharedgame/sg_gamemode.h"
#include "svgame/svg_gamemode.h"



/**
*
*
*
*	General GameMode Functionality:
*
*
*
**/
/**
*   @brief  Returns the created target changelevel
**/
svg_base_edict_t *svg_gamemode_t::CreateTargetChangeLevel( const char *map ) {
	svg_base_edict_t *ent = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
	ent->classname = "target_changelevel";
	if ( map != level.nextmap ) {
		Q_strlcpy( level.nextmap, map, sizeof( level.nextmap ) );
	}
	ent->map = map;// level.nextmap;
	return ent;
}
/**
*	@brief	Defines the behavior for the game mode when the level has to exit.
**/
void svg_gamemode_t::ExitLevel() {
	// Notify Lua.
	SVG_Lua_CallBack_ExitMap();

	// Generate the command string to change the map.
	char    command[ 256 ];
	Q_snprintf( command, sizeof( command ), "gamemap \"%s\"\n", (const char *)level.changemap );
	// Add it to the command string queue.
	gi.AddCommandString( command );
	// Reset the level changemap, and intermission state.
	level.changemap = NULL;
	level.exitintermission = 0;
	level.intermissionFrameNumber = 0;

	// Clear some things before going to next level.
	for ( int32_t i = 0; i < maxclients->value; i++ ) {
		// Get client entity and ensure it is valid.
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i + 1 );
		if ( !ent || !ent->inuse ) {
			continue;
		}
		// Reset the health of the player to their max health.
		if ( ent->health > ent->client->pers.max_health ) {
			ent->health = ent->client->pers.max_health;
		}
	}
}



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
	return false;
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
*	@brief	Called somewhere at the beginning of the game frame. This allows
*			to determine if conditions are met to engage exitting intermission
*			mode and/or exit the level.
**/
void svg_gamemode_singleplayer_t::PreCheckGameRuleConditions() {
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

};



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
	#if 0
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
	#endif

	// <Q2RTXP>: TODO: Pass argument to the game mode object and let it decide.
	// Right now we use the dedicated cvar to determine if we allow saving in coop mode.
	if ( !game.mode ) {
		// Don't allow saving if we don't have a game mode object.
		return false;
	}
	return game.mode->AllowSaveGames();
}