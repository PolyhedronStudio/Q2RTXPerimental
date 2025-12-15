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
#include "svgame/svg_combat.h"
#include "svgame/svg_commands_server.h"
#include "svgame/svg_edict_pool.h"
#include "svgame/svg_clients.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_move.h"
#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_view.h"

#include "svgame/svg_lua.h"

#include "svgame/entities/svg_entities_pushermove.h"

#include "sharedgame/sg_gamemode.h"
#include "sharedgame/pmove/sg_pmove.h"

#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"

/**
*	General used Game Objects.
**/
svg_game_locals_t game;
svg_level_locals_t  level;
svgame_import_t	gi;
svgame_export_t	globals;


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

//! THIS!! is the ACTUAL ARRAY for STORING the EDICTS. (Also referred to by the term entities.)
svg_base_edict_t **g_edicts = nullptr;
//! Memory Pool for entities.
svg_edict_pool_t g_edict_pool = {};

// WID: gamemode support:
cvar_t *dedicated = nullptr;
cvar_t *password = nullptr;
cvar_t *spectator_password = nullptr;
cvar_t *needpass = nullptr;
cvar_t *filterban = nullptr;

cvar_t *maxclients = nullptr;
cvar_t *maxspectators = nullptr;
cvar_t *maxentities = nullptr;
cvar_t *nomonsters = nullptr;
cvar_t *aimfix = nullptr;

cvar_t *gamemode = nullptr;
cvar_t *deathmatch = nullptr;
cvar_t *coop = nullptr;
cvar_t *dmflags = nullptr;
cvar_t *skill = nullptr;
cvar_t *fraglimit = nullptr;
cvar_t *timelimit = nullptr;

cvar_t *sv_cheats = nullptr;
cvar_t *sv_flaregun = nullptr;
cvar_t *sv_maplist = nullptr;
cvar_t *sv_features = nullptr;

cvar_t *sv_airaccelerate = nullptr;
cvar_t *sv_maxvelocity = nullptr;
cvar_t *sv_gravity = nullptr;

cvar_t *sv_rollspeed = nullptr;
cvar_t *sv_rollangle = nullptr;

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

cvar_t *flood_msgs = nullptr;
cvar_t *flood_persecond = nullptr;
cvar_t *flood_waitdelay = nullptr;

cvar_t *g_select_empty = nullptr;

//
// Func Declarations:
//

void SVG_Client_Command( svg_base_edict_t *ent );

void SVG_SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );
void SVG_RunEntity(svg_base_edict_t *ent);
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
const int32_t _Exports_SG_GetRequestedGameModeType() {
    return static_cast<const int32_t>( SG_GetRequestedGameModeType() );
}
/**
*	@return	A string representative of the passed in gameModeType.
**/
const char *_Exports_SG_GetGameModeName( const int32_t gameModeType ) {
    return SG_GetGameModeName( static_cast<const sg_gamemode_type_t>( gameModeType ) );
}

void SVG_PreInitGame( void );
void SVG_InitGame( void );
void SVG_ShutdownGame( void );

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

		globals.GetRequestedGameModeType = _Exports_SG_GetRequestedGameModeType;
        globals.IsValidGameModeType = _Exports_SG_IsValidGameModeType;
        globals.IsMultiplayerGameMode = _Exports_SG_IsMultiplayerGameMode;
        globals.GetDefaultMultiplayerGamemodeType = _Exports_SG_GetDefaultMultiplayerGameModeType;
		globals.GetGameModeName = _Exports_SG_GetGameModeName;
		globals.GameModeAllowSaveGames = SVG_GameModeAllowSaveGames;

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

		//globals.PlayerMove = SG_PlayerMove;
		//globals.ConfigurePlayerMoveParameters = SG_ConfigurePlayerMoveParameters;

		globals.RunFrame = SVG_RunFrame;

		globals.ServerCommand = SVG_ServerCommand;

		// Store a pointer to the edicts pool.
        globals.edictPool = &g_edict_pool;

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



/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void SVG_PreInitGame( void ) {
    // Notify 
    gi.dprintf( "==== PreInit ServerGame ====\n" );

    // Setup the Edict Class TypeInfo system.
    EdictTypeInfo::InitializeTypeInfoRegistry();

    // Initialize the game local's movewith array.
    game.moveWithEntities = static_cast<svg_game_locals_t::game_locals_movewith_t *>( gi.TagMalloc( sizeof( svg_game_locals_t::game_locals_movewith_t ) * MAX_EDICTS, TAG_SVGAME ) );
    memset( game.moveWithEntities, 0, sizeof( svg_game_locals_t::game_locals_movewith_t ) * MAX_EDICTS );

    maxclients = gi.cvar( "maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH );
    maxentities = gi.cvar( "maxentities", std::to_string( MAX_EDICTS ).c_str(), CVAR_LATCH );
    maxspectators = gi.cvar( "maxspectators", "4", CVAR_SERVERINFO );

    // 0 = SinglePlayer, 1 = Coop, 2 = DeathMatch.
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
    dmflags = gi.cvar( "dmflags", "0", CVAR_SERVERINFO );
    fraglimit = gi.cvar( "fraglimit", "0", CVAR_SERVERINFO );
    timelimit = gi.cvar( "timelimit", "0", CVAR_SERVERINFO );
    sv_airaccelerate = gi.cvar( "sv_airaccelerate", "0", CVAR_SERVERINFO | CVAR_LATCH );
    sv_airaccelerate->changed = cvar_sv_airaccelerate_changed;
    // Force set its value so the config string gets updated accordingly.
    //gi.cvar_forceset( "sv_airaccelerate", "0" );
    // Air acceleration defaults to 0 and is only set for DM mode.
    //gi.configstring( CS_AIRACCEL, "0" );
}

/**
*	@brief	Called after PreInitGame when the game has set up gamemode specific cvars.
**/
void SVG_InitGame( void ) {
    // Notify 
    gi.dprintf( "==== Init ServerGame(Gamemode: \"%s\", maxclients=%d, maxspectators=%d, maxentities=%d) ====\n",
        SG_GetGameModeName( static_cast<const sg_gamemode_type_t>( gamemode->integer ) ), maxclients->integer, maxspectators->integer, maxentities->integer );

    game.maxclients = maxclients->integer;
    game.maxentities = maxentities->integer;

    // C Random time initializing.
    Q_srand( time( NULL ) );

    // seed RNG
    mt_rand.seed( (uint32_t)std::chrono::system_clock::now().time_since_epoch().count() );

    //FIXME: sv_ prefix is wrong for these
    sv_rollspeed = gi.cvar( "sv_rollspeed", "200", 0 );
    sv_rollangle = gi.cvar( "sv_rollangle", "2", 0 );
    sv_maxvelocity = gi.cvar( "sv_maxvelocity", "2000", 0 );
    sv_gravity = gi.cvar( "sv_gravity", "800", 0 );

    // noset vars
    dedicated = gi.cvar( "dedicated", "0", CVAR_NOSET );

    aimfix = gi.cvar( "aimfix", "0", CVAR_ARCHIVE );

    // latched vars
	// If we are in development mode, we want to allow cheats.
    //sv_cheats = gi.cvar( "cheats", "0", CVAR_SERVERINFO | CVAR_LATCH );
    sv_cheats = gi.cvar( "cheats", nullptr, 0 );
    gi.cvar( "gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_LATCH );
    gi.cvar( "gamedate", __DATE__, CVAR_SERVERINFO | CVAR_LATCH );

    // change anytime vars
    password = gi.cvar( "password", "", CVAR_USERINFO );
    spectator_password = gi.cvar( "spectator_password", "", CVAR_USERINFO );
    needpass = gi.cvar( "needpass", "0", CVAR_SERVERINFO );
    filterban = gi.cvar( "filterban", "1", 0 );

    g_select_empty = gi.cvar( "g_select_empty", "0", CVAR_ARCHIVE );

    // flood control
    flood_msgs = gi.cvar( "flood_msgs", "4", 0 );
    flood_persecond = gi.cvar( "flood_persecond", "4", 0 );
    flood_waitdelay = gi.cvar( "flood_waitdelay", "10", 0 );

    // dm map list
    sv_maplist = gi.cvar( "sv_maplist", "", 0 );

    // obtain server features
    sv_features = gi.cvar( "sv_features", NULL, 0 );

    // flare gun switch: 
    //   0 = no flare gun
    //   1 = spawn with the flare gun
    //   2 = spawn with the flare gun and some grenades
    sv_flaregun = gi.cvar( "sv_flaregun", "2", 0 );

    // export our own features
    gi.cvar_forceset( "g_features", va( "%d", SVG_FEATURES ) );

    // In case we've modified air acceleration, update the config string.
    //gi.configstring( CS_AIRACCEL, std::to_string( sv_airaccelerate->integer ).c_str() );
    // Update air acceleration config string.
    //if ( COM_IsUint( sv_airaccelerate->string ) || COM_IsFloat( sv_airaccelerate->string ) ) {
    //    gi.configstring( CS_AIRACCEL, sv_airaccelerate->string );
    //} else {
    //    gi.configstring( CS_AIRACCEL, "0" );
    //}


    // Clamp maxentities within valid range.
    game.maxentities = QM_ClampUnsigned<uint32_t>( maxentities->integer, (int)maxclients->integer + 1, MAX_EDICTS );
    // initialize all clients for this game
    game.maxclients = QM_ClampUnsigned<uint32_t>( maxclients->integer, 0, MAX_CLIENTS );

    // Clear the edict pool in case any previous data was there.
    //g_edicts = SVG_EdictPool_Release( &g_edict_pool );
    // (Re-)Initialize the edict pool, and store a pointer to its edicts array in g_edicts.
    g_edicts = SVG_EdictPool_Allocate( &g_edict_pool, game.maxentities );
    // Set the number of edicts to the maxclients + 1 (Encounting for the world at slot #0).
    g_edict_pool.num_edicts = game.maxclients + 1;

    // Initialize a fresh clients array.
    game.clients = SVG_Clients_Reallocate( game.maxclients );

    // Set client fields on player entities.
    for ( int32_t i = 0; i < game.maxclients; i++ ) {
        // Assign this entity to the designated client.
        //g_edicts[ i + 1 ]->client = game.clients + i;
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i + 1 );
        ent->client = &game.clients[ i ];

        // Assign client number.
        //ent->client->clientNum = i;

        //// Set their states as disconnected, unspawned, since the level is switching.
        game.clients[ i ].pers.connected = false;
        game.clients[ i ].pers.spawned = false;
    }
}

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void SVG_ShutdownGame( void ) {
    // Notify of shutdown.
    gi.dprintf( "==== Initiating ServerGame Shutdown ====\n" );

    // Shutdown the Lua VM.
    SVG_Lua_Shutdown();

    // Free game mode object.
    // game.mode->Shutdown();
    delete game.mode;
    game.mode = nullptr;

    // Free level, lua AND the game module its allocated ram.
	//gi.FreeTags( TAG_SVGAME_LUA ); // <-- WID: Moved to SVG_Lua_Shutdown().
    gi.FreeTags( TAG_SVGAME_EDICTS );
    gi.FreeTags( TAG_SVGAME_LEVEL );
    gi.FreeTags( TAG_SVGAME );

    // Notify of shutdown.
    gi.dprintf( "==== ServerGame Shutdown ====\n" );
}

//======================================================================

/**
*   @brief 
**/
static void EndClientServerFrames(void) {
    // Calc the player views now that all pushing and damage has been added
    for ( int32_t i = 1 ; i <= maxclients->value ; i++) {
		// Get the entity for this client.
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
        // Sanity check.
        if ( !ent || !ent->inUse || !ent->client ) {
            continue;
        }

		// Call upon end of server frame for client, which in turn notifies the gamemode.
        SVG_Client_EndServerFrame(ent);
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
*   @brief  Defer
**/
static void DeferRemoveClientInfo( svg_base_edict_t *ent ) {
    // "Defer removing client info so that disconnected, etc works."
    if ( ent->timestamp && level.time < ent->timestamp ) {
        const int32_t playernum = g_edict_pool.NumberForEdict( ent ) - 1;//ent - g_edicts - 1;
        gi.configstring( CS_PLAYERSKINS + playernum, "" );
        ent->timestamp = 0_sec;
    }
}
/**
*   @brief  Checks the entity for whether its event should be cleared.
*   @return True if the entity is meant to be skipped for iterating further on afterwards.
**/
static const bool CheckClearEntityEvents( svg_base_edict_t *ent ) {
	// We want the QMTime version of this.
    static constexpr QMTime _EVENT_VALID_MSEC = QMTime::FromMilliseconds( EVENT_VALID_MSEC );
    // clear events that are too old
    if ( level.time - ent->eventTime > _EVENT_VALID_MSEC ) {
        if ( ent->s.event ) {
            ent->s.event = 0;	// &= EV_EVENT_BITS;
            if ( ent->client ) {
                ent->client->ps.externalEvent = 0;
                // predicted events should never be set to zero
                //ent->client->ps.events[0] = 0;
                //ent->client->ps.events[1] = 0;
            }
        }
        // If true, we free it.
        if ( ent->freeAfterEvent ) {
            // tempEntities or dropped items completely go away after their event
            SVG_FreeEdict( ent );
            //continue;
            return true; // Skip the entity for further processing. 
        // Otherwise, if true, we unlink it.
        } else if ( ent->unlinkAfterEvent ) {
            // Unlink it now.
            gi.unlinkentity( ent );
            // Items that will respawn will hide themselves after their pickup event.
            ent->unlinkAfterEvent = false;
        }
    }

    // Don't skip the entity for further processing.
    return false;
}
/**
*   @brief  Check for whether the entity's ground has been moved below his ass.
**/
static void CheckEntityGroundChange( svg_base_edict_t *ent ) {
    if ( ( ent->groundInfo.entityNumber != ENTITYNUM_NONE ) ) {
        svg_base_edict_t *entityGroundEntity = g_edict_pool.EdictForNumber( ent->groundInfo.entityNumber );
        if ( entityGroundEntity != nullptr && ( entityGroundEntity->linkCount != ent->groundInfo.entityLinkCount ) ) {
            cm_contents_t mask = SVG_GetClipMask( ent );

            // Monsters that don't SWIM or FLY, got their own unique ground check.
            if ( !( ent->flags & ( FL_SWIM | FL_FLY ) ) && ( ent->svFlags & SVF_MONSTER ) ) {
                ent->groundInfo.entityNumber = ENTITYNUM_NONE;
                M_CheckGround( ent, mask );
                // All other entities use this route instead:
            } else {
                // If the ground entity is still 1 unit below us, we're good.
                Vector3 endPoint = Vector3( ent->s.origin ) + Vector3{ 0.f, 0.f, -1.f } /*ent->gravitiyVector*/;
                svg_trace_t tr = SVG_Trace( ent->s.origin, ent->mins, ent->maxs, &endPoint.x, ent, mask );

                if ( tr.startsolid || tr.allsolid || tr.entityNumber != ent->groundInfo.entityNumber ) {
                    ent->groundInfo.entityNumber = ENTITYNUM_NONE;
                } else {
                    svg_base_edict_t *traceGroundEntity = g_edict_pool.EdictForNumber( ent->groundInfo.entityNumber );
                    ent->groundInfo.entityLinkCount = traceGroundEntity->linkCount;
                }
            }
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

    #if 0
    // Exit intermissions.
    if ( level.exitintermission ) {
        ExitLevel();
        return;
    }
    #else
    game.mode->PreCheckGameRuleConditions();
    #endif

    // WID: LUA: CallBack.
    SVG_Lua_CallBack_BeginServerFrame();

    //
    // Treat each object in turn
    // even the world gets a chance to think
    //
    svg_base_edict_t *ent = ent = g_edict_pool.EdictForNumber( 0 );
    for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++, ent = g_edict_pool.EdictForNumber( i ) ) {
		// skip nullptr edicts.
        if ( !ent ) {
            continue;
        }
        // Determine whether it is a client entity based on the loop indexer.
        const bool isClientEntity = i > 0 && i <= game.maxclients;
		/**
        *   Defer removing client info for clients that are no longer in use.
        **/
        if ( !ent->inUse ) {
            if ( isClientEntity ) {
                DeferRemoveClientInfo( ent );
            }
            // Skip since entity is not in use.
            continue;
        }

        /**
        *   Set the current entity being processed for the current frame.
        **/
        level.processingEntity = ent;

        /**
		*   Clear events that are too old.
        * 
        *   (Temp entities never think.)
        **/
        if ( CheckClearEntityEvents( ent ) == true ) {
            // Entity has been freed or whatever, but we shall not pass.
			// Skip the entity for further processing.
            continue;
        }

        /**
		*   Not linked in, so skip it.
        **/
        // Not linked in anywhere.
        if ( !ent->area.prev && ent->neverFreeOnlyUnlink ) {
            continue;
        }
        /**
        *   RF Beam Entities update their old_origin by hand.
        **/
        if ( ent->s.entityType != ET_BEAM 
            && !( ent->s.renderfx & RF_BEAM ) ) {
            ent->s.old_origin = ent->s.origin;
        }

        /**
        *   If the ground entity moved, make sure we are still on it.
        **/
        CheckEntityGroundChange( ent );

        /**
		*   Client entities are handled seperately.
        **/
        if ( isClientEntity ) {
            SVG_Client_BeginServerFrame( ent );
            continue;
        /**
		*   Other entities are handled here.
        **/
        } else {
			// For Pushers, store their last origins and angles before running them.
            if ( ent->s.entityType == ET_PUSHER ) {
                // Make sure to store the last ... origins and angles.
                ent->lastOrigin = ent->s.origin;
                ent->lastAngles = ent->s.angles;
            }
            // Run the entity now. (Make it 'think'.)
            SVG_RunEntity( ent );
        }
    }
    // Readjust "movewith" Push/Stop entities.
    SVG_PushMove_UpdateMoveWithEntities();

    // WID: LUA: CallBack.
    SVG_Lua_CallBack_RunFrame();

    // Give the gamemode a chance to run its own logic to determine whether to
	// engage into some game mode specific behavior.
    game.mode->PostCheckGameRuleConditions();

    // see if needpass needs updated
    CheckNeedPass();

    // build the playerstate_t structures for all players
    EndClientServerFrames();
    // WID: LUA: CallBack.
    SVG_Lua_CallBack_EndServerFrame();
}