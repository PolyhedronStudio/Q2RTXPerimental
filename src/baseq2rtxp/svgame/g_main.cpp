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

#include "g_local.h"

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
sg_time_t FRAME_TIME_S;
//! Frame time in Miliseconds.
sg_time_t FRAME_TIME_MS;


/**
*	Random Number Generator.
**/
//! Mersenne Twister random number generator.
std::mt19937 mt_rand;


/**
*	Cached indexes and global meansOfDeath var.
**/
int sm_meat_index;
int snd_fry;
int meansOfDeath;

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

cvar_t *gun_x;
cvar_t *gun_y;
cvar_t *gun_z;

cvar_t *run_pitch;
cvar_t *run_roll;
cvar_t *bob_up;
cvar_t *bob_pitch;
cvar_t *bob_roll;

cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;

cvar_t *g_select_empty;
cvar_t *g_instant_weapon_switch;

//
// Func Declarations:
//
void SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );
void ClientThink(edict_t *ent, usercmd_t *cmd);
qboolean ClientConnect(edict_t *ent, char *userinfo);
void ClientUserinfoChanged(edict_t *ent, char *userinfo);
void ClientDisconnect(edict_t *ent);
void ClientBegin(edict_t *ent);
void ClientCommand(edict_t *ent);
void G_RunEntity(edict_t *ent);
void WriteGame(const char *filename, qboolean autosave);
void ReadGame(const char *filename);
void WriteLevel(const char *filename);
void ReadLevel(const char *filename);
void InitGame(void);
void G_RunFrame(void);
//===================================================================
/**
*
*
*   CVar Changed Callbacks:
* 
*
**/
static void sv_airaccelerate_changed( cvar_t *self ) {
    // Update air acceleration config string.
    if ( COM_IsUint( self->string ) || COM_IsFloat( self->string ) ) {
        gi.configstring( CS_AIRACCEL, self->string );
    }
}

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void ShutdownGame(void)
{
    gi.dprintf("==== Shutdown ServerGame ====\n");

    gi.FreeTags(TAG_SVGAME_LEVEL);
    gi.FreeTags(TAG_SVGAME);
}

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void PreInitGame( void ) {
	// Notify 
	gi.dprintf( "==== PreInit ServerGame ====\n" );

	maxclients = gi.cvar( "maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH );
	maxspectators = gi.cvar( "maxspectators", "4", CVAR_SERVERINFO );

	// 0 = SinglePlayer, 1 = Deathmatch, 2 = Coop.
	gamemode = gi.cvar( "gamemode", 0, CVAR_SERVERINFO | CVAR_LATCH );

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
    // Force set its value so the config string gets updated accordingly.
    //gi.cvar_forceset( "sv_airaccelerate", "0" );
	// Air acceleration defaults to 0 and is only set for DM mode.
	//gi.configstring( CS_AIRACCEL, "0" );

	// Deathmatch:
	if ( gamemode->integer == GAMEMODE_DEATHMATCH ) {
		// Set the cvar for keeping old code in-tact. TODO: Remove in the future.
		gi.cvar_forceset( "deathmatch", "1" );

		// Setup maxclients correctly.
		if ( maxclients->integer <= 1 ) {
			gi.cvar_forceset( "maxclients", "8" ); //Cvar_SetInteger( maxclients, 8, FROM_CODE );
		} else if ( maxclients->integer > CLIENTNUM_RESERVED ) {
			gi.cvar_forceset( "maxclients", std::to_string( CLIENTNUM_RESERVED ).c_str() );
		}
        gi.dprintf( "[GameMode(#%d): Deathmatch][maxclients=%d]\n", gamemode->integer, maxclients->integer );
	// Cooperative:
	} else if ( gamemode->integer == GAMEMODE_COOPERATIVE ) {
		gi.cvar_forceset( "coop", "1" );

		if ( maxclients->integer <= 1 || maxclients->integer > 4 ) {
			gi.cvar_forceset( "maxclients", "4" ); // Cvar_Set( "maxclients", "4" );
		}
		gi.dprintf( "[GameMode(#%d): Cooperative][maxclients=%d]\n", gamemode->integer, maxclients->integer );
	// Default: Singleplayer.
	} else {    // non-deathmatch, non-coop is one player
		//Cvar_FullSet( "maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH, FROM_CODE );
		gi.cvar_forceset( "maxclients", "1" );
		gi.dprintf( "[GameMode(#%d): Singleplayer][maxclients=%d]\n", gamemode->integer, maxclients->integer );
	}
}

/**
*	@brief	Called after PreInitGame when the game has set up gamemode specific cvars.
**/
void InitGame( void )
{
	// Notify 
    gi.dprintf("==== Init ServerGame(Gamemode: \"%s\", maxclients=%d, maxspectators=%d, maxentities=%d) ====\n",
				SG_GetGamemodeName( gamemode->integer ), maxclients->integer, maxspectators->integer, maxentities->integer );

	// C Random time initializing.
    Q_srand(time(NULL));
	
	// seed RNG
	mt_rand.seed( (uint32_t)std::chrono::system_clock::now( ).time_since_epoch( ).count( ) );

    gun_x = gi.cvar("gun_x", "0", 0);
    gun_y = gi.cvar("gun_y", "0", 0);
    gun_z = gi.cvar("gun_z", "0", 0);

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

	g_instant_weapon_switch = gi.cvar( "g_instant_weapon_switch", "0", CVAR_LATCH );
    g_select_empty = gi.cvar("g_select_empty", "0", CVAR_ARCHIVE);

    run_pitch = gi.cvar("run_pitch", "0.002", 0);
    run_roll = gi.cvar("run_roll", "0.005", 0);
    bob_up  = gi.cvar("bob_up", "0.005", 0);
    bob_pitch = gi.cvar("bob_pitch", "0.002", 0);
    bob_roll = gi.cvar("bob_roll", "0.002", 0);

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
    gi.cvar_forceset("g_features", va("%d", G_FEATURES));
    
    // In case we've modified air acceleration, update the config string.
    //gi.configstring( CS_AIRACCEL, std::to_string( sv_airaccelerate->integer ).c_str() );
    // Update air acceleration config string.
    //if ( COM_IsUint( sv_airaccelerate->string ) || COM_IsFloat( sv_airaccelerate->string ) ) {
    //    gi.configstring( CS_AIRACCEL, sv_airaccelerate->string );
    //} else {
    //    gi.configstring( CS_AIRACCEL, "0" );
    //}

    // items
    InitItems();

    game.helpmessage1[0] = 0;
    game.helpmessage2[0] = 0;

    // initialize all entities for this game
    game.maxentities = maxentities->value;
    clamp(game.maxentities, (int)maxclients->value + 1, MAX_EDICTS);
	// WID: C++20: Addec cast.
    g_edicts = (edict_t*)gi.TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_SVGAME);
    globals.edicts = g_edicts;
    globals.max_edicts = game.maxentities;

    // initialize all clients for this game
    game.maxclients = maxclients->value;
	// WID: C++20: Addec cast.
    game.clients = (gclient_t*)gi.TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_SVGAME);
    globals.num_edicts = game.maxclients + 1;
}


/**
*	@brief	Returns a pointer to the structure with all entry points
*			and global variables
**/
extern "C" { // WID: C++20: extern "C".
	q_exported svgame_export_t *GetGameAPI( svgame_import_t *import ) {
		gi = *import;

		// From Q2RE:
		FRAME_TIME_S = FRAME_TIME_MS = sg_time_t::from_ms( gi.frame_time_ms );

		globals.apiversion = SVGAME_API_VERSION;
		globals.PreInit = PreInitGame;
		globals.Init = InitGame;
		globals.Shutdown = ShutdownGame;
		globals.SpawnEntities = SpawnEntities;

		globals.GetActiveGamemodeID = G_GetActiveGamemodeID;
        globals.IsGamemodeIDValid = G_IsGamemodeIDValid;
        globals.IsMultiplayerGameMode = G_IsMultiplayerGameMode;
        globals.GetDefaultMultiplayerGamemodeID = G_GetDefaultMultiplayerGamemodeID;
		globals.GetGamemodeName = SG_GetGamemodeName;
		globals.GamemodeNoSaveGames = G_GetGamemodeNoSaveGames;

		globals.WriteGame = WriteGame;
		globals.ReadGame = ReadGame;
		globals.WriteLevel = WriteLevel;
		globals.ReadLevel = ReadLevel;

		globals.ClientThink = ClientThink;
		globals.ClientConnect = ClientConnect;
		globals.ClientUserinfoChanged = ClientUserinfoChanged;
		globals.ClientDisconnect = ClientDisconnect;
		globals.ClientBegin = ClientBegin;
		globals.ClientCommand = ClientCommand;

		globals.PlayerMove = SG_PlayerMove;
		globals.ConfigurePlayerMoveParameters = SG_ConfigurePlayerMoveParameters;

		globals.RunFrame = G_RunFrame;

		globals.ServerCommand = ServerCommand;

		globals.edict_size = sizeof( edict_t );

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


/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames(void)
{
    int     i;
    edict_t *ent;

    // calc the player views now that all pushing
    // and damage has been added
    for (i = 0 ; i < maxclients->value ; i++) {
        ent = g_edicts + 1 + i;
        if (!ent->inuse || !ent->client)
            continue;
        ClientEndServerFrame(ent);
    }

}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
    edict_t *ent;

    ent = G_AllocateEdict();
    ent->classname = "target_changelevel";
    if (map != level.nextmap)
        Q_strlcpy(level.nextmap, map, sizeof(level.nextmap));
    ent->map = level.nextmap;
    return ent;
}

/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void EndDMLevel(void)
{
    edict_t     *ent;
    char *s, *t, *f;
    static const char *seps = " ,\n\r";

    // stay on same level flag
    if ((int)dmflags->value & DF_SAME_LEVEL) {
        BeginIntermission(CreateTargetChangeLevel(level.mapname));
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
                    if (f == NULL) // there isn't a first one, same level
                        BeginIntermission(CreateTargetChangeLevel(level.mapname));
                    else
                        BeginIntermission(CreateTargetChangeLevel(f));
                } else
                    BeginIntermission(CreateTargetChangeLevel(t));
                free(s);
                return;
            }
            if (!f)
                f = t;
            t = strtok(NULL, seps);
        }
        free(s);
    }

    if (level.nextmap[0]) // go to a specific map
        BeginIntermission(CreateTargetChangeLevel(level.nextmap));
    else {  // search for a changelevel
        ent = G_Find(NULL, FOFS(classname), "target_changelevel");
        if (!ent) {
            // the map designer didn't include a changelevel,
            // so create a fake ent that goes back to the same level
            BeginIntermission(CreateTargetChangeLevel(level.mapname));
            return;
        }
        BeginIntermission(ent);
    }
}


/*
=================
CheckNeedPass
=================
*/
void CheckNeedPass(void)
{
    int need;

    // if password or spectator_password has changed, update needpass
    // as needed
    if (password->modified || spectator_password->modified) {
        password->modified = spectator_password->modified = false;

        need = 0;

        if (*password->string && Q_stricmp(password->string, "none"))
            need |= 1;
        if (*spectator_password->string && Q_stricmp(spectator_password->string, "none"))
            need |= 2;

        gi.cvar_set("needpass", va("%d", need));
    }
}

/*
=================
CheckDMRules
=================
*/
void CheckDMRules(void)
{
    int         i;
    gclient_t   *cl;

    if (level.intermission_framenum)
        return;

    if (!deathmatch->value)
        return;

    if (timelimit->value) {
        if (level.time >= sg_time_t::from_min( timelimit->value * 60 ) ) {
            gi.bprintf(PRINT_HIGH, "Timelimit hit.\n");
            EndDMLevel();
            return;
        }
    }

    if (fraglimit->value) {
        for (i = 0 ; i < maxclients->value ; i++) {
            cl = game.clients + i;
            if (!g_edicts[i + 1].inuse)
                continue;

            if (cl->resp.score >= fraglimit->value) {
                gi.bprintf(PRINT_HIGH, "Fraglimit hit.\n");
                EndDMLevel();
                return;
            }
        }
    }
}


/*
=============
ExitLevel
=============
*/
void ExitLevel(void)
{
    int     i;
    edict_t *ent;
    char    command [256];

    Q_snprintf(command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
    gi.AddCommandString(command);
    level.changemap = NULL;
    level.exitintermission = 0;
    level.intermission_framenum = 0;

    // clear some things before going to next level
    for (i = 0 ; i < maxclients->value ; i++) {
        ent = g_edicts + 1 + i;
        if (!ent->inuse)
            continue;
        if (ent->health > ent->client->pers.max_health)
            ent->health = ent->client->pers.max_health;
    }

}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
void G_RunFrame(void)
{
    int     i;
    edict_t *ent;

    // Check for cvars that demand a configstring update if they've changed.
    //G_CheckCVarConfigStrings();

    // Increase the frame number we're in for this level..
    level.framenum++;
    // Increase the amount of time that has passed for this level.
    level.time += FRAME_TIME_MS;

    // Choose a client for monsters to target this frame.
    AI_SetSightClient();

    // Exit intermissions.
    if ( level.exitintermission ) {
        ExitLevel();
        return;
    }

    //
    // treat each object in turn
    // even the world gets a chance to think
    //
    ent = &g_edicts[ 0 ];
    for ( i = 0; i < globals.num_edicts; i++, ent++ ) {
        if ( !ent->inuse ) {
            // "Defer removing client info so that disconnected, etc works."
            if ( i > 0 && i <= game.maxclients ) {
                if ( ent->timestamp && level.time < ent->timestamp ) {
                    const int32_t playernum = ent - g_edicts - 1;
                    gi.configstring( CS_PLAYERSKINS + playernum, "" );
                    ent->timestamp = 0_sec;
                }
            }
            continue;
        }

        // Set the current entity being processed for the current frame.
        level.current_entity = ent;

        // RF Beam Entities update their old_origin by hand.
        if ( !( ent->s.renderfx & RF_BEAM ) ) {
            VectorCopy( ent->s.origin, ent->s.old_origin );
        }

        // If the ground entity moved, make sure we are still on it.
        if ( ( ent->groundentity ) && ( ent->groundentity->linkcount != ent->groundentity_linkcount ) ) {
            contents_t mask = G_GetClipMask( ent );

            // Monsters that don't SWIM or FLY, got their own unique ground check.
            if ( !( ent->flags & ( FL_SWIM | FL_FLY ) ) && ( ent->svflags & SVF_MONSTER ) ) {
                ent->groundentity = nullptr;
                M_CheckGround( ent, mask );
            }
            //// All other entities use this route instead:
            //} else {
            //    // If the ground entity is still 1 unit below us, we're good.
            //    Vector3 endPoint = Vector3( ent->s.origin ) - Vector3{ 0.f, 0.f, -1.f } /*ent->gravitiyVector*/;
            //    trace_t tr = gi.trace( ent->s.origin, ent->mins, ent->maxs, &endPoint.x, ent, mask );

            //    if ( tr.startsolid || tr.allsolid || tr.ent != ent->groundentity ) {
            //        ent->groundentity = nullptr;
            //    } else {
            //        ent->groundentity_linkcount = ent->groundentity->linkcount;
            //    }
            //}
        }

        if ( i > 0 && i <= maxclients->value ) {
            ClientBeginServerFrame( ent );
            continue;
        } else {
            G_RunEntity( ent );
        }
    }

    // see if it is time to end a deathmatch
    CheckDMRules();

    // see if needpass needs updated
    CheckNeedPass();

    // build the playerstate_t structures for all players
    ClientEndServerFrames();
}

