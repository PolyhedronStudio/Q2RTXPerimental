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

#include "server.h"

server_static_t svs;                // persistant server info
server_t        sv;                 // local server

void SV_ClientReset(client_t *client)
{
    if (client->state < cs_connected) {
        return;
    }

    // any partially connected client will be restarted
    client->state = cs_connected;
    client->framenum = 1; // frame 0 can't be used
    client->lastframe = -1;
    client->frames_nodelta = 0;
    client->send_delta = 0;
    client->suppress_count = 0;
    memset(&client->lastcmd, 0, sizeof(client->lastcmd));
}

static void resolve_masters(void)
{
#if !USE_CLIENT
    time_t now = time(NULL);

    for (int i = 0; i < MAX_MASTERS; i++) {
        master_t *m = &sv_masters[i];
        if (!m->name) {
            break;
        }
        if (now < m->last_resolved) {
            m->last_resolved = now;
            continue;
        }
        // re-resolve valid address after one day,
        // resolve invalid address after three hours
        int hours = m->adr.type ? 24 : 3;
        if (now - m->last_resolved < hours * 3600) {
            continue;
        }
        if (NET_StringToAdr(m->name, &m->adr, PORT_MASTER)) {
            Com_DPrintf("Master server at %s.\n", NET_AdrToString(&m->adr));
        } else {
            Com_WPrintf("Couldn't resolve master: %s\n", m->name);
            memset(&m->adr, 0, sizeof(m->adr));
        }
        m->last_resolved = now = time(NULL);
    }
#endif
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
================
*/
void SV_SpawnServer(mapcmd_t *cmd)
{
    int         i;
    client_t    *client;
    //const char        *entitystring; // WID: C++20: Added const.

    SCR_BeginLoadingPlaque();           // for local system

    Com_Printf("------- Server Initialization -------\n");
    Com_Printf("SpawnServer: %s\n", cmd->server);

	static bool warning_printed = false;
	if (dedicated->integer && !SV_NoSaveGames() && !warning_printed)
	{
		Com_Printf("\nWARNING: Dedicated coop servers save game state into the same place as single player game by default (currently '%s/%s'). "
			"To override that, set the 'sv_savedir' console variable. To host multiple dedicated coop servers on one machine, set that cvar "
			"to different values on different instances of the server.\n\n", fs_gamedir, Cvar_WeakGet("sv_savedir")->string);

		warning_printed = true;
	}

    // everyone needs to reconnect
    FOR_EACH_CLIENT(client) {
        SV_ClientReset(client);
    }

    SV_BroadcastCommand("changing map=%s\n", cmd->server);
    SV_SendClientMessages();
    SV_SendAsyncPackets();

    // free current level
    CM_FreeMap(&sv.cm);

    // wipe the entire per-level structure
    memset(&sv, 0, sizeof(sv));
    sv.spawncount = Q_rand() & 0x7fffffff;

    // set legacy spawncounts
    FOR_EACH_CLIENT(client) {
        client->spawncount = sv.spawncount;
    }

    // reset entity counter
    svs.next_entity = 0;

    // save name for levels that don't set message
    Q_strlcpy(sv.configstrings[CS_NAME], cmd->server, MAX_CS_STRING_LENGTH );
    Q_strlcpy(sv.name, cmd->server, sizeof(sv.name));
    Q_strlcpy(sv.mapcmd, cmd->buffer, sizeof(sv.mapcmd));

    //if (Cvar_VariableInteger("deathmatch")) {
    //    sprintf(sv.configstrings[CS_AIRACCEL], "%d", sv_airaccelerate->integer);
    //} else {
    //    strcpy(sv.configstrings[CS_AIRACCEL], "0");
    //}

    resolve_masters();

    if (cmd->state == ss_game) {
        sv.cm = cmd->cm;
        sprintf(sv.configstrings[CS_MAPCHECKSUM], "%d", sv.cm.checksum);

        // set inline model names
        Q_concat(sv.configstrings[CS_MODELS + 1], MAX_CS_STRING_LENGTH, "maps/", cmd->server, ".bsp");
        for (i = 1; i < sv.cm.cache->nummodels; i++) {
            sprintf(sv.configstrings[CS_MODELS + 1 + i], "*%d", i);
        }
    } else {
        // no real map
        strcpy(sv.configstrings[CS_MAPCHECKSUM], "0");
        //sv.cm.entitystring = "";
		strcpy(sv.cm.entitystring, "" );
    }

    //
    // clear physics interaction links
    //
    SV_ClearWorld();



    //
    // spawn the rest of the entities on the map
    //

    // precache and static commands can be issued during
    // map initialization
    sv.state = ss_loading;

    // load and spawn all other entities
    ge->SpawnEntities(sv.name, sv.cm.entitystring, cmd->spawnpoint);

    // run two frames to allow everything to settle
    ge->RunFrame(); sv.framenum++;
    ge->RunFrame(); sv.framenum++;

    // make sure maxclients string is correct
    sprintf(sv.configstrings[CS_MAXCLIENTS], "%d", sv_maxclients->integer);

    // check for a savegame
    SV_CheckForSavegame(cmd);

    // all precaches are complete
    sv.state = cmd->state;

    // set serverinfo variable
    SV_InfoSet("mapname", sv.name);
    SV_InfoSet("port", net_port->string);

    Cvar_SetInteger(sv_running, sv.state, FROM_CODE);
    Cvar_Set("sv_paused", "0");
    Cvar_Set("timedemo", "0");

    EXEC_TRIGGER(sv_changemapcmd);

#if USE_SYSCON
    SV_SetConsoleTitle();
#endif

    SV_BroadcastCommand("reconnect\n");

    Com_Printf("-------------------------------------\n");
}

static bool check_server(mapcmd_t *cmd, const char *server, bool nextserver)
{
    char        expanded[MAX_QPATH];
    char        *s, *ch;
    int         ret = Q_ERR(ENAMETOOLONG);

    // copy it off to keep original mapcmd intact
    Q_strlcpy(cmd->server, server, sizeof(cmd->server));
    s = cmd->server;

    // if there is a $, use the remainder as a spawnpoint
    ch = strchr(s, '$');
    if (ch) {
        *ch = 0;
        cmd->spawnpoint = ch + 1;
    } else {
        cmd->spawnpoint = s + strlen(s);
    }

    // now expand and try to load the map
    if (!COM_CompareExtension(s, ".pcx")) {
        if (Q_concat(expanded, sizeof(expanded), "pics/", s) < sizeof(expanded)) {
            ret = COM_DEDICATED ? Q_ERR_SUCCESS : FS_LoadFile(expanded, NULL);
        }
        cmd->state = ss_pic;
    } else if (!COM_CompareExtension(s, ".cin")) {
        if (!sv_cinematics->integer && nextserver)
            return false;   // skip it
        if (Q_concat(expanded, sizeof(expanded), "video/", s) < sizeof(expanded)) {
            if (COM_DEDICATED || (ret = FS_LoadFile(expanded, NULL)) == Q_ERR(EFBIG))
                ret = Q_ERR_SUCCESS;
        }
        cmd->state = ss_cinematic;
    }
    else {
        //CM_LoadOverrides(&cmd->cm, cmd->server, sizeof(cmd->server));
        if (Q_concat(expanded, sizeof(expanded), "maps/", s, ".bsp") < sizeof(expanded)) {
            ret = CM_LoadMap(&cmd->cm, expanded);
        }
        cmd->state = ss_game;
    }

    if (ret < 0) {
        Com_Printf("Couldn't load %s: %s\n", expanded, BSP_ErrorString(ret));
        CM_FreeMap(&cmd->cm);   // free entstring if overridden
        return false;
    }

    return true;
}

/*
==============
SV_ParseMapCmd

Parses mapcmd into more C friendly form.
Loads and fully validates the map to make sure server doesn't get killed.
==============
*/
bool SV_ParseMapCmd(mapcmd_t *cmd)
{
    char *s, *ch;
    char copy[MAX_QPATH];
    bool killserver = false;

    // copy it off to keep original mapcmd intact
    Q_strlcpy(copy, cmd->buffer, sizeof(copy));
    s = copy;

    while (1) {
        // hack for nextserver: kill server if map doesn't exist
        if (*s == '!') {
            s++;
            killserver = true;
        }

        // skip the end-of-unit flag if necessary
        if (*s == '*') {
            s++;
            cmd->endofunit = true;
        }

        // if there is a + in the map, set nextserver to the remainder.
        ch = strchr(s, '+');
        if (ch)
            *ch = 0;

        // see if map exists and can be loaded
        if (check_server(cmd, s, ch)) {
            if (ch)
                Cvar_Set("nextserver", va("gamemap \"!%s\"", ch + 1));
            else
                Cvar_Set("nextserver", "");

            // special hack for end game screen in coop mode
            if (Cvar_VariableInteger("coop") && !Q_stricmp(s, "victory.pcx"))
                Cvar_Set("nextserver", "gamemap \"!*base1\"");
            return true;
        }

        // skip to nextserver if cinematic doesn't exist
        if (!ch)
            break;

        s = ch + 1;
    }

    if (killserver)
        Cbuf_AddText(&cmd_buffer, "killserver\n");

    return false;
}

/**
*	@brief
**/
static void SV_InitGame_PreInit( ) {
	ge->PreInit( );
}
/**
*	@brief
**/
static void SV_InitGame_Init( ) {
	// initialize
	ge->Init( );

	// sanitize edict_size
	if ( ge->edict_size < sizeof( edict_t ) || ge->edict_size >( unsigned )INT_MAX / MAX_EDICTS ) {
		Com_Error( ERR_DROP, "ServerGame library returned bad size of edict_t" );
	}

	// sanitize max_edicts
	if ( ge->max_edicts <= sv_maxclients->integer || ge->max_edicts > MAX_EDICTS ) {
		Com_Error( ERR_DROP, "ServerGame library returned bad number of max_edicts" );
	}

	// Set up default pmove parameters
	//ge->ConfigurePlayerMoveParameters( &sv_pmp );
}

/*
==============
SV_InitGame

A brand new game has been started.
If mvd_spawn is non-zero, load the built-in MVD game module.
==============
*/
void SV_InitGame()
{
    int     i, entnum;
    edict_t *ent;
    client_t *client;

    if (svs.initialized) {
        // cause any connected clients to reconnect
        SV_Shutdown("Server restarted\n", static_cast<error_type_t>( ERR_RECONNECT ) ); // WID: C++20: Added cast.
    } else {
        // make sure the client is down
        CL_Disconnect(ERR_RECONNECT);
        SCR_BeginLoadingPlaque();

        CM_FreeMap(&sv.cm);
        memset(&sv, 0, sizeof(sv));
    }

	// Ensure gamemode is properly prepared to be set.
	Cvar_Get( "gamemode", "0", CVAR_SERVERINFO | CVAR_LATCH );

    // get any latched variable changes (maxclients, etc)
    Cvar_GetLatchedVars();

#if !USE_CLIENT
    Cvar_Reset(sv_recycle);
#endif
	// load up game progs.
	SV_InitGameProgs( );

    // Ensure the gamemode is valid.
    if ( !ge->IsGamemodeIDValid( Cvar_VariableInteger( "gamemode" ) ) ) {
        // Warn.
        Com_WPrintf( "Invalid gamemode detected, defaulting to %s\n", ge->GetGamemodeName( 0 ) );

        // Set to singleplayer.
        Cvar_Set( "gamemode", "0" );
    }

    // Dedicated servers can't be singleplayer. 
    // Force it to the game API 'default' multiplayer mode.
    if ( COM_DEDICATED ) {
        if ( !ge->IsMultiplayerGameMode( Cvar_VariableInteger( "gamemode" ) ) ) {
            const int32_t newGameModeID = ge->GetDefaultMultiplayerGamemodeID();
            // Warn.
            Com_WPrintf( "Can't do %s on a dedicated server. Defaulting to %s\n",
                ge->GetGamemodeName( Cvar_VariableInteger( "gamemode" ) ),
                ge->GetGamemodeName( newGameModeID ) );

            std::string gameModeStr = std::to_string( newGameModeID );
            Cvar_Set( "gamemode", gameModeStr.c_str() );
        }
    }

	// Preinitialize the game.
	SV_InitGame_PreInit( );

    //if (Cvar_VariableInteger("coop") &&
    //    Cvar_VariableInteger("deathmatch")) {
    //    Com_Printf("Deathmatch and Coop both set, disabling Coop\n");
    //    Cvar_Set("coop", "0");
    //}


    //// init clients
    //if (Cvar_VariableInteger("deathmatch")) {
    //    if (sv_maxclients->integer <= 1) {
    //        Cvar_SetInteger(sv_maxclients, 8, FROM_CODE);
    //    } else if (sv_maxclients->integer > CLIENTNUM_RESERVED) {
    //        Cvar_SetInteger(sv_maxclients, CLIENTNUM_RESERVED, FROM_CODE);
    //    }
    //} else if (Cvar_VariableInteger("coop")) {
    //    if (sv_maxclients->integer <= 1 || sv_maxclients->integer > 4)
    //        Cvar_Set("maxclients", "4");
    //} else {    // non-deathmatch, non-coop is one player
    //    Cvar_FullSet("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH, FROM_CODE);
    //}

    // enable networking
    if (sv_maxclients->integer > 1) {
        NET_Config(NET_SERVER);
    }

    svs.client_pool = static_cast<client_t*>( SV_Mallocz(sizeof(client_t) * sv_maxclients->integer) ); // WID: C++20: Added cast.

    svs.num_entities = sv_maxclients->integer * UPDATE_BACKUP * MAX_PACKET_ENTITIES;
    svs.entities = static_cast<entity_packed_t*>( SV_Mallocz(sizeof(entity_packed_t) * svs.num_entities) ); // WID: C++20: Added cast.

    Cvar_ClampInteger(sv_reserved_slots, 0, sv_maxclients->integer - 1);

#if USE_ZLIB
    svs.z.zalloc = SV_zalloc;
    svs.z.zfree = SV_zfree;
    Q_assert(deflateInit2(&svs.z, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
             -MAX_WBITS, 9, Z_DEFAULT_STRATEGY) == Z_OK);
	svs.z_buffer_size = ZPACKET_HEADER + deflateBound( &svs.z, MAX_MSGLEN );
	svs.z_buffer = static_cast<byte*>( SV_Malloc( svs.z_buffer_size ) );
#endif

	SV_InitGame_Init();

    // send heartbeat very soon
    svs.last_heartbeat = -(HEARTBEAT_SECONDS - 5) * 1000;
    svs.heartbeat_index = 0;

    for (i = 0; i < sv_maxclients->integer; i++) {
        client = svs.client_pool + i;
        entnum = i + 1;
        ent = EDICT_FOR_NUMBER(entnum);
        ent->s.number = entnum;
        client->edict = ent;
        client->number = i;
    }

    svs.initialized = true;
}

