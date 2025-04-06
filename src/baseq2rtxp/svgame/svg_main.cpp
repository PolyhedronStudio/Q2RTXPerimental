/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "svgame/svg_local.h"
#include "svgame/svg_gamemode.h"
#include "svgame/svg_commands_server.h"

#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_view.h"

#include "svgame/svg_lua.h"

#include "svgame/entities/svg_entities_pushermove.h"


/**
*	General used Game Objects.
**/
game_locals_t   game;
level_locals_t  level;
svgame_import_t	gi;
svgame_export_t	globals;
spawn_temp_t    st;


/**
*	Times.
**/
//! Frame time in Seconds.
QMTime FRAME_TIME_S;
//! Frame time in Miliseconds.
QMTime FRAME_TIME_MS;



/**
*	Cached indexes and global meansOfDeath var.
**/
int sm_meat_index;
int snd_fry;

edict_t	*g_edicts;

// WID: gamemode support:
cvar_t *dedicated;
cvar_t *password;
cvar_t *spectator_password;
cvar_t *needpass;
cvar_t *filterban;

cvar_t *maxclients;
cvar_t *maxspectators;
cvar_t *maxentities;
cvar_t *nomonsters;
cvar_t *aimfix;

cvar_t *gamemode;
cvar_t *deathmatch;
cvar_t *coop;
cvar_t *dmflags;
cvar_t *skill;
cvar_t *fraglimit;
cvar_t *timelimit;

cvar_t *sv_cheats;
cvar_t *sv_flaregun;
cvar_t *sv_maplist;
cvar_t *sv_features;

cvar_t *sv_airaccelerate;
cvar_t *sv_maxvelocity;
cvar_t *sv_gravity;

cvar_t *sv_rollspeed;
cvar_t *sv_rollangle;

// Moved to CLGame.
//cvar_t *gun_x;
//cvar_t *gun_y;
//cvar_t *gun_z;

// Moved to CLGame.
//cvar_t *run_pitch;
//cvar_t *run_roll;
//cvar_t *bob_up;
//cvar_t *bob_pitch;
//cvar_t *bob_roll;

cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;

cvar_t *g_select_empty;

//
// Func Declarations:
//
void SVG_Client_Begin( edict_t *ent );
void SVG_Client_Command( edict_t *ent );
qboolean SVG_Client_Connect( edict_t *ent, char *userinfo );
void SVG_Client_Disconnect( edict_t *ent );
void SVG_Client_Think( edict_t *ent, usercmd_t *cmd );
void SVG_Client_UserinfoChanged( edict_t *ent, char *userinfo );

void SVG_SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );
void SVG_RunEntity(edict_t *ent);
void SVG_WriteGame(const char *filename, qboolean autosave);
void SVG_ReadGame(const char *filename);
void SVG_WriteLevel(const char *filename);
void SVG_ReadLevel(const char *filename);
void SVG_InitGame(void);
void SVG_RunFrame(void);

//===================================================================
/**
*
*
*   CVar Changed Callbacks:
* 
*
**/
static void cvar_sv_airaccelerate_changed( cvar_t *self ) {
    // Update air acceleration config string.
    if ( COM_IsUint( self->string ) || COM_IsFloat( self->string ) ) {
        gi.configstring( CS_AIRACCEL, self->string );
    }
}
#if 0
static void cvar_sv_gamemode_changed( cvar_t *self ) {
    // Is it valid?
    const bool isValidGamemode = SG_IsValidGameModeType( static_cast<sg_gamemode_type_t>( self->integer ) );

    if ( !isValidGamemode ) {
        // In an invalid situation, resort to single player.
        gi.cvar_forceset( "gamemode", std::to_string( GAMEMODE_TYPE_SINGLEPLAYER ).c_str() );

        // Invalid somehow.
        gi.bprintf( PRINT_WARNING, "%s: tried to assign a non valid gamemode type ID(#%i), resorting to default(#%i, %s)\n",
            __func__, gamemode->integer, gamemode->integer, SG_GetGameModeName( static_cast<sg_gamemode_type_t>( gamemode->integer ) ) );
    }
}
#endif
/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void SVG_ShutdownGame(void)
{
    // Notify of shutdown.
    gi.dprintf("==== Shutdown ServerGame ====\n");
    
    // Shutdown the Lua VM.
    SVG_Lua_Shutdown();

    // Free level, lua AND the game module its allocated ram.
    gi.FreeTags( TAG_SVGAME_LEVEL );
    gi.FreeTags( TAG_SVGAME );
    gi.FreeTags( TAG_SVGAME_LUA );
}

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void SVG_PreInitGame( void ) {
	// Notify 
	gi.dprintf( "==== PreInit ServerGame ====\n" );

	maxclients = gi.cvar( "maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH );
	maxspectators = gi.cvar( "maxspectators", "4", CVAR_SERVERINFO );

	// 0 = SinglePlayer, 1 = Deathmatch, 2 = Coop.
	gamemode = gi.cvar( "gamemode", nullptr, 0 );
    //gamemode->changed = cvar_sv_gamemode_changed;

	// The following is to for now keep code compatability.
	deathmatch = gi.cvar( "deathmatch", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ROM );
	gi.cvar_forceset( "deathmatch", "0" );
	coop = gi.cvar( "coop", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ROM );
	gi.cvar_forceset( "coop", "0" );

	// These are possibily game mode related also.
	nomonsters = gi.cvar( "nomonsters", "0", 0 );
	skill = gi.cvar( "skill", "1", CVAR_LATCH );
    maxentities = gi.cvar( "maxentities", std::to_string( MAX_EDICTS ).c_str(), CVAR_LATCH);
	dmflags = gi.cvar( "dmflags", "0", CVAR_SERVERINFO );
	fraglimit = gi.cvar( "fraglimit", "0", CVAR_SERVERINFO );
	timelimit = gi.cvar( "timelimit", "0", CVAR_SERVERINFO );
    sv_airaccelerate = gi.cvar( "sv_airaccelerate", "0", CVAR_SERVERINFO | CVAR_LATCH );
    sv_airaccelerate->changed = cvar_sv_airaccelerate_changed;
    // Force set its value so the config string gets updated accordingly.
    //gi.cvar_forceset( "sv_airaccelerate", "0" );
	// Air acceleration defaults to 0 and is only set for DM mode.
	//gi.configstring( CS_AIRACCEL, "0" );

    // Get the current gamemode type.
    sg_gamemode_type_t activeGameModeType = SG_GetActiveGameModeType();
    // And its corresponding name.
    const char *gameModeName = SG_GetGameModeName( activeGameModeType );

    // Deathmatch:
    if ( activeGameModeType == GAMEMODE_TYPE_DEATHMATCH ) {
		// Set the cvar for keeping old code in-tact. TODO: Remove in the future.
		gi.cvar_forceset( "deathmatch", "1" );

		// Setup maxclients correctly.
		if ( maxclients->integer <= 1 ) {
			gi.cvar_forceset( "maxclients", "8" ); //Cvar_SetInteger( maxclients, 8, FROM_CODE );
		} else if ( maxclients->integer > CLIENTNUM_RESERVED ) {
			gi.cvar_forceset( "maxclients", std::to_string( CLIENTNUM_RESERVED ).c_str() );
		}
	// Cooperative:
	} else if ( activeGameModeType == GAMEMODE_TYPE_COOPERATIVE ) {
		gi.cvar_forceset( "coop", "1" );

		if ( maxclients->integer <= 1 || maxclients->integer > 4 ) {
			gi.cvar_forceset( "maxclients", "4" ); // Cvar_Set( "maxclients", "4" );
		}
	// Default: Singleplayer.
	} else {    
        // Non-deathmatch, non-coop is one player.
		//Cvar_FullSet( "maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH, FROM_CODE );
		gi.cvar_forceset( "maxclients", "1" );
	}

    // Output the game mode type, and the maximum clients allowed for this session.
    gi.dprintf( "[GameMode(#%d): %s][maxclients=%d]\n", activeGameModeType, gameModeName, maxclients->integer );
}

/**
*	@brief	Called after PreInitGame when the game has set up gamemode specific cvars.
**/
void SVG_InitGame( void )
{
	// Notify 
    gi.dprintf("==== Init ServerGame(Gamemode: \"%s\", maxclients=%d, maxspectators=%d, maxentities=%d) ====\n",
				SG_GetGameModeName( static_cast<const sg_gamemode_type_t>( gamemode->integer ) ), maxclients->integer, maxspectators->integer, maxentities->integer );

    game.gamemode = static_cast<sg_gamemode_type_t>( gamemode->integer );
    game.maxclients = maxclients->integer;
    game.maxentities = maxentities->integer;

	// C Random time initializing.
    Q_srand(time(NULL));
	
	// seed RNG
	mt_rand.seed( (uint32_t)std::chrono::system_clock::now( ).time_since_epoch( ).count( ) );

    //FIXME: sv_ prefix is wrong for these
    sv_rollspeed = gi.cvar("sv_rollspeed", "200", 0);
    sv_rollangle = gi.cvar("sv_rollangle", "2", 0);
    sv_maxvelocity = gi.cvar("sv_maxvelocity", "2000", 0);
    sv_gravity = gi.cvar("sv_gravity", "800", 0);

    // noset vars
    dedicated = gi.cvar("dedicated", "0", CVAR_NOSET);

    aimfix = gi.cvar("aimfix", "0", CVAR_ARCHIVE);

    // latched vars
    sv_cheats = gi.cvar("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH);
    gi.cvar("gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_LATCH);
    gi.cvar("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);

    // change anytime vars
	password = gi.cvar("password", "", CVAR_USERINFO);
    spectator_password = gi.cvar("spectator_password", "", CVAR_USERINFO);
    needpass = gi.cvar("needpass", "0", CVAR_SERVERINFO);
    filterban = gi.cvar("filterban", "1", 0);

    g_select_empty = gi.cvar("g_select_empty", "0", CVAR_ARCHIVE);

    // flood control
    flood_msgs = gi.cvar("flood_msgs", "4", 0);
    flood_persecond = gi.cvar("flood_persecond", "4", 0);
    flood_waitdelay = gi.cvar("flood_waitdelay", "10", 0);

    // dm map list
    sv_maplist = gi.cvar("sv_maplist", "", 0);

    // obtain server features
    sv_features = gi.cvar("sv_features", NULL, 0);

	// flare gun switch: 
	//   0 = no flare gun
	//   1 = spawn with the flare gun
	//   2 = spawn with the flare gun and some grenades
	sv_flaregun = gi.cvar("sv_flaregun", "2", 0);

    // export our own features
    gi.cvar_forceset("g_features", va("%d", SVG_FEATURES));
    
    // In case we've modified air acceleration, update the config string.
    //gi.configstring( CS_AIRACCEL, std::to_string( sv_airaccelerate->integer ).c_str() );
    // Update air acceleration config string.
    //if ( COM_IsUint( sv_airaccelerate->string ) || COM_IsFloat( sv_airaccelerate->string ) ) {
    //    gi.configstring( CS_AIRACCEL, sv_airaccelerate->string );
    //} else {
    //    gi.configstring( CS_AIRACCEL, "0" );
    //}

    // items
    SVG_InitItems();

    // initialize all entities for this game
    game.maxentities = QM_ClampUnsigned<uint32_t>(maxentities->integer, (int)maxclients->integer + 1, MAX_EDICTS);
	// WID: C++20: Addec cast.
    g_edicts = (edict_t*)gi.TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_SVGAME);
    globals.edicts.edicts = g_edicts;
    globals.edicts.max_edicts = game.maxentities;

    // initialize all clients for this game
    game.maxclients = maxclients->value;
	// WID: C++20: Addec cast.
    game.clients = (svg_client_t*)gi.TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_SVGAME);
    globals.edicts.num_edicts = game.maxclients + 1;

    // Initialize the Lua VM.
    //SVG_Lua_Initialize();
}



/**
*
*
*   Exports Game API / Wrapper for functions to Exports Game API:
*
*
**/
/**
*	@return	True if the game mode is a legitimate existing one.
**/
const bool _Exports_SG_IsValidGameModeType( const int32_t gameModeType ) {
    return SG_IsValidGameModeType( static_cast<const sg_gamemode_type_t>( gameModeType ) );
}
/**
*   @return True if the game mode is multiplayer.
**/
const bool _Exports_SG_IsMultiplayerGameMode( const int32_t gameModeType ) {
    return SG_IsMultiplayerGameMode( static_cast<const sg_gamemode_type_t>( gameModeType ) );
}
/**
*	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
**/
const int32_t _Exports_SG_GetDefaultMultiplayerGameModeType() {
    // Default to Deathmatch.
    return static_cast<const int32_t>( SG_GetDefaultMultiplayerGameModeType() );
}
/**
*	@return	The actual Type of the current gamemode.
**/
const int32_t _Exports_SG_GetActiveGameModeType() {
    return static_cast<const int32_t>( SG_GetActiveGameModeType() );
}
/**
*	@return	A string representative of the passed in gameModeType.
**/
const char *_Exports_SG_GetGameModeName( const int32_t gameModeType ) {
    return SG_GetGameModeName( static_cast<const sg_gamemode_type_t>( gameModeType ) );
}

/**
*	@brief	Returns a pointer to the structure with all entry points
*			and global variables
**/
extern "C" { // WID: C++20: extern "C".
	q_exported svgame_export_t *GetGameAPI( svgame_import_t *import ) {
		gi = *import;

		// From Q2RE:
        FRAME_TIME_S = QMTime::FromSeconds( gi.frame_time_s );
        FRAME_TIME_MS = QMTime::FromMilliseconds( gi.frame_time_ms );

		globals.apiversion = SVGAME_API_VERSION;
		globals.PreInit = SVG_PreInitGame;
		globals.Init = SVG_InitGame;
		globals.Shutdown = SVG_ShutdownGame;
		globals.SpawnEntities = SVG_SpawnEntities;

		globals.GetActiveGameModeType = _Exports_SG_GetActiveGameModeType;
        globals.IsValidGameModeType = _Exports_SG_IsValidGameModeType;
        globals.IsMultiplayerGameMode = _Exports_SG_IsMultiplayerGameMode;
        globals.GetDefaultMultiplayerGamemodeType = _Exports_SG_GetDefaultMultiplayerGameModeType;
		globals.GetGamemodeName = _Exports_SG_GetGameModeName;
		globals.GamemodeNoSaveGames = SVG_GetGamemodeNoSaveGames;

		globals.WriteGame = SVG_WriteGame;
		globals.ReadGame = SVG_ReadGame;
		globals.WriteLevel = SVG_WriteLevel;
		globals.ReadLevel = SVG_ReadLevel;

		globals.ClientThink = SVG_Client_Think;
		globals.ClientConnect = SVG_Client_Connect;
        globals.ClientBegin = SVG_Client_Begin;
        globals.ClientDisconnect = SVG_Client_Disconnect;
		globals.ClientUserinfoChanged = SVG_Client_UserinfoChanged;
		globals.ClientCommand = SVG_Client_Command;

		globals.PlayerMove = SG_PlayerMove;
		//globals.ConfigurePlayerMoveParameters = SG_ConfigurePlayerMoveParameters;

		globals.RunFrame = SVG_RunFrame;

		globals.ServerCommand = SVG_ServerCommand;

        globals.edicts = { };
        globals.edicts.edicts = g_edicts;
		globals.edicts.edict_size = sizeof( edict_t );

		return &globals;
	}
}; // WID: C++20: extern "C".



/**
*
*
*	For 'Hard Linking'.
*
*
**/
#ifndef SVGAME_HARD_LINKED
// this is only here so the functions in q_shared.c can link
void Com_LPrintf(print_type_t type, const char *fmt, ...)
{
    va_list     argptr;
    char        text[MAX_STRING_CHARS];

    //if (type == PRINT_DEVELOPER) {
    //    return;
    //}

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    //gi.dprintf("%s", text);
    gi.bprintf( type, "%s", text );
}

void Com_Error(error_type_t type, const char *fmt, ...)
{
    va_list     argptr;
    char        text[MAX_STRING_CHARS];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    gi.error("%s", text);
}
#endif

//======================================================================


/**
*   @brief 
**/
void ClientEndServerFrames(void) {
    // calc the player views now that all pushing
    // and damage has been added
    for ( int32_t i = 0 ; i < maxclients->value ; i++) {
        edict_t *ent = g_edicts + 1 + i;
        if ( !ent->inuse || !ent->client ) {
            continue;
        }
        SVG_Client_EndServerFrame(ent);
    }

}

/**
*   @brief  Returns the created target changelevel
**/
edict_t *CreateTargetChangeLevel( char *map ) {
    edict_t *ent;

    ent = SVG_AllocateEdict();
    ent->classname = "target_changelevel";
    if ( map != level.nextmap ) {
        Q_strlcpy( level.nextmap, map, sizeof( level.nextmap ) );
    }
    ent->map = level.nextmap;
    return ent;
}

/**
*   @brief  The timelimit or fraglimit has been exceeded
**/
void EndDMLevel(void) {
    edict_t     *ent;
    char *s, *t, *f;
    static const char *seps = " ,\n\r";

    // stay on same level flag
    if ((int)dmflags->value & DF_SAME_LEVEL) {
        SVG_HUD_BeginIntermission(CreateTargetChangeLevel(level.mapname));
        return;
    }

    // see if it's in the map list
    if (*sv_maplist->string) {
        s = strdup(sv_maplist->string);
        f = NULL;
        t = strtok(s, seps);
        while (t != NULL) {
            if (Q_stricmp(t, level.mapname) == 0) {
                // it's in the list, go to the next one
                t = strtok(NULL, seps);
                if (t == NULL) { // end of list, go to first one
                    if ( f == NULL ) {// there isn't a first one, same level
                        SVG_HUD_BeginIntermission( CreateTargetChangeLevel( level.mapname ) );
                    } else {
                        SVG_HUD_BeginIntermission( CreateTargetChangeLevel( f ) );
                    }
                } else {
                    SVG_HUD_BeginIntermission( CreateTargetChangeLevel( t ) );
                }
                free(s);
                return;
            }
            if ( !f ) {
                f = t;
            }
            t = strtok(NULL, seps);
        }
        free(s);
    }

    if ( level.nextmap[ 0 ] ) {// go to a specific map
        SVG_HUD_BeginIntermission( CreateTargetChangeLevel( level.nextmap ) );
    } else {  // search for a changelevel
        ent = SVG_Find(NULL, FOFS_GENTITY(classname), "target_changelevel");
        if (!ent) {
            // the map designer didn't include a changelevel,
            // so create a fake ent that goes back to the same level
            SVG_HUD_BeginIntermission(CreateTargetChangeLevel(level.mapname));
            return;
        }
        SVG_HUD_BeginIntermission(ent);
    }
}

/**
*   @brief  
**/
void CheckNeedPass(void) {
    int need;

    // if password or spectator_password has changed, update needpass
    // as needed
    if (password->modified || spectator_password->modified) {
        password->modified = spectator_password->modified = false;

        need = 0;

        if ( *password->string && Q_stricmp( password->string, "none" ) ) {
            need |= 1;
        }
        if ( *spectator_password->string && Q_stricmp( spectator_password->string, "none" ) ) {
            need |= 2;
        }

        gi.cvar_set("needpass", va("%d", need));
    }
}

/**
*   @brief
**/
void CheckDMRules(void) {
    int         i;
    svg_client_t   *cl;

    if ( level.intermissionFrameNumber ) {
        return;
    }

    if ( !deathmatch->value ) {
        return;
    }

    if (timelimit->value) {
        if (level.time >= QMTime::FromMinutes( timelimit->value * 60 ) ) {
            gi.bprintf(PRINT_HIGH, "Timelimit hit.\n");
            EndDMLevel();
            return;
        }
    }

    if (fraglimit->value) {
        for (i = 0 ; i < maxclients->value ; i++) {
            cl = game.clients + i;
            if ( !g_edicts[ i + 1 ].inuse ) {
                continue;
            }

            if (cl->resp.score >= fraglimit->value) {
                gi.bprintf(PRINT_HIGH, "Fraglimit hit.\n");
                EndDMLevel();
                return;
            }
        }
    }
}

/**
*   @brief
**/
void ExitLevel(void) {
    int     i;
    edict_t *ent;
    char    command [256];

    // WID: LUA: CallBack.
    SVG_Lua_CallBack_ExitMap();


    Q_snprintf(command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
    gi.AddCommandString(command);
    level.changemap = NULL;
    level.exitintermission = 0;
    level.intermissionFrameNumber = 0;

    // clear some things before going to next level
    for (i = 0 ; i < maxclients->value ; i++) {
        ent = g_edicts + 1 + i;
        if ( !ent->inuse ) {
            continue;
        }
        if ( ent->health > ent->client->pers.max_health ) {
            ent->health = ent->client->pers.max_health;
        }
    }

}

/**
*   @brief  Advances the world by FRAME_TIME_MS seconds
**/
void SVG_RunFrame(void) {
    // Check for cvars that demand a configstring update if they've changed.
    //G_CheckCVarConfigStrings();

    // Increase the frame number we're in for this level..
    level.frameNumber++;
    // Increase the amount of time that has passed for this level.
    level.time += FRAME_TIME_MS;

    // Reseed the mersennery twister.
    mt_rand.seed( level.frameNumber );

    // Choose a client for monsters to target this frame.
    // WID: TODO: Monster Reimplement.
    //AI_SetSightClient();

    // Exit intermissions.
    if ( level.exitintermission ) {
        ExitLevel();
        return;
    }

    // WID: LUA: CallBack.
    SVG_Lua_CallBack_BeginServerFrame();

    //
    // Treat each object in turn
    // even the world gets a chance to think
    //
    edict_t *ent = &g_edicts[ 0 ];
    for ( int32_t i = 0; i < globals.edicts.num_edicts; i++, ent++ ) {
        if ( !ent->inuse ) {
            // "Defer removing client info so that disconnected, etc works."
            if ( i > 0 && i <= game.maxclients ) {
                if ( ent->timestamp && level.time < ent->timestamp ) {
                    const int32_t playernum = ent - g_edicts - 1;
                    gi.configstring( CS_PLAYERSKINS + playernum, "" );
                    ent->timestamp = 0_sec;
                }
            }
            // Skip since entity is not in use.
            continue;
        }

        // Set the current entity being processed for the current frame.
        level.current_entity = ent;

        // RF Beam Entities update their old_origin by hand.
        if ( ent->s.entityType != ET_BEAM && !( ent->s.renderfx & RF_BEAM ) ) {
            VectorCopy( ent->s.origin, ent->s.old_origin );
        }

        // If the ground entity moved, make sure we are still on it.
        if ( ( ent->groundInfo.entity ) && ( ent->groundInfo.entity->linkcount != ent->groundInfo.entityLinkCount ) ) {
            contents_t mask = SVG_GetClipMask( ent );

            // Monsters that don't SWIM or FLY, got their own unique ground check.
            if ( !( ent->flags & ( FL_SWIM | FL_FLY ) ) && ( ent->svflags & SVF_MONSTER ) ) {
                ent->groundInfo.entity = nullptr;
                M_CheckGround( ent, mask );
            }
            //// All other entities use this route instead:
            //} else {
            //    // If the ground entity is still 1 unit below us, we're good.
            //    Vector3 endPoint = Vector3( ent->s.origin ) - Vector3{ 0.f, 0.f, -1.f } /*ent->gravitiyVector*/;
            //    svg_trace_t tr = SVG_Trace( ent->s.origin, ent->mins, ent->maxs, &endPoint.x, ent, mask );

            //    if ( tr.startsolid || tr.allsolid || tr.ent != ent->groundentity ) {
            //        ent->groundentity = nullptr;
            //    } else {
            //        ent->groundentity_linkcount = ent->groundentity->linkcount;
            //    }
            //}
        }

        if ( i > 0 && i <= maxclients->value ) {
            SVG_Client_BeginServerFrame( ent );
            continue;
        } else {
            if ( ent->s.entityType == ET_PUSHER ) {
                // Make sure to store the last ... origins and angles.
                ent->lastOrigin = ent->s.origin;
                ent->lastAngles = ent->s.angles;
            }
            SVG_RunEntity( ent );
        }
    }

    // Readjust "movewith" Push/Stop entities.
    SVG_PushMove_UpdateMoveWithEntities();

    // WID: LUA: CallBack.
    SVG_Lua_CallBack_RunFrame();

    // see if it is time to end a deathmatch
    CheckDMRules();

    // see if needpass needs updated
    CheckNeedPass();

    // build the playerstate_t structures for all players
    ClientEndServerFrames();

    // WID: LUA: CallBack.
    SVG_Lua_CallBack_EndServerFrame();
}