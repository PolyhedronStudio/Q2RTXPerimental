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

#include "server/sv_server.h"
#include "server/sv_commands.h"
#include "server/sv_init.h"
#include "server/sv_world.h"



#ifdef WIN32
// Only include what we 'need'.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
    // Make sure to undef.
    #ifdef NEAR
    #undef NEAR
    #endif
    #ifdef FAR
    #undef FAR
    #endif
    #ifdef near
    #undef near
    #endif
    #ifdef far
    #undef far
    #endif
    #ifdef MAX
    #undef MAX
    #endif
    #ifdef MIN
    #undef MIN
    #endif
    #ifdef max
    #undef max
    #endif
    #ifdef min
    #undef min
    #endif
    #ifdef CLAMP
    #undef CLAMP
    #endif
    #ifdef clamp
    #undef clamp
    #endif
#endif

#define SAVE_MAGIC1     MakeLittleLong('S','S','V','2')
#define SAVE_MAGIC2     MakeLittleLong('S','A','V','2')
#define SAVE_VERSION    1

#define SAVE_CURRENT    ".current"
#define SAVE_AUTO       "save0"

cvar_t *sv_savedir = NULL;
///* Don't require GMF_ENHANCED_SAVEGAMES feature for savegame support.
// * Savegame logic may be less "safe", but in practice, this usually works fine.
// * Still, allow it as an option for cautious people. */
//cvar_t *sv_force_enhanced_savegames = NULL;

static int write_server_file(bool autosave)
{
    char        name[MAX_OSPATH];
    cvar_t      *var;
    int         ret;
    //uint64_t    timestamp;

    // write magic
    MSG_WriteInt32(SAVE_MAGIC1);
    MSG_WriteInt32(SAVE_VERSION);

    MSG_WriteInt64( ( int64_t )time( NULL ) );

    // write the comment field
    //MSG_WriteInt32(timestamp & 0xffffffff);
    //MSG_WriteInt32(timestamp >> 32);
    MSG_WriteUint8(autosave);
    MSG_WriteString(sv.configstrings[CS_NAME]);

    // write the mapcmd
    MSG_WriteString(sv.mapcmd);

    // write all CVAR_LATCH cvars
    // these will be things like coop, skill, deathmatch, etc
    for (var = cvar_vars; var; var = var->next) {
        if (!(var->flags & CVAR_LATCH))
            continue;
        if (var->flags & CVAR_PRIVATE)
            continue;
        MSG_WriteString(var->name);
        MSG_WriteString(var->string);
    }
    MSG_WriteString(NULL);

    // write server state
	Q_snprintf(name, MAX_OSPATH, "%s/%s/server.ssv", sv_savedir->string, SAVE_CURRENT);
    ret = FS_WriteFile(name, msg_write.data, msg_write.cursize);

    SZ_Clear(&msg_write);

    if (ret < 0)
        return -1;

    // write game state
    if (Q_snprintf(name, MAX_OSPATH, "%s/%s/%s/game.ssv", fs_gamedir, sv_savedir->string, SAVE_CURRENT) >= MAX_OSPATH)
        return -1;

    ge->WriteGame(name, autosave);
    return 0;
}

static int write_level_file(void)
{
    char        name[MAX_OSPATH];
    int         i, ret;
    char        *s;
    size_t      len;
    static byte portalbits[ MAX_MAP_PORTAL_BYTES ] = {};
    memset( portalbits, 0, MAX_MAP_PORTAL_BYTES );

    // write magic
    MSG_WriteInt32(SAVE_MAGIC2);
    MSG_WriteInt32(SAVE_VERSION);

    // write configstrings
    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        s = sv.configstrings[i];
        if (!s[0])
            continue;

        len = Q_strnlen(s, MAX_QPATH);
        MSG_WriteInt16(i);
        MSG_WriteData(s, len);
        MSG_WriteUint8(0);
    }
    MSG_WriteInt16(MAX_CONFIGSTRINGS);

    len = CM_WritePortalBits(&sv.cm, portalbits);
    MSG_WriteUint8(len);
    MSG_WriteData(portalbits, len);

    if (Q_snprintf(name, MAX_QPATH, "%s/%s/%s.sv2", sv_savedir->string, SAVE_CURRENT, sv.name) >= MAX_QPATH)
        ret = -1;
    else
        ret = FS_WriteFile(name, msg_write.data, msg_write.cursize);

    SZ_Clear(&msg_write);

    if (ret < 0)
        return -1;

    // write game level
    if (Q_snprintf(name, MAX_OSPATH, "%s/%s/%s/%s.sav", fs_gamedir, sv_savedir->string, SAVE_CURRENT, sv.name) >= MAX_OSPATH)
        return -1;

    ge->WriteLevel(name);
    return 0;
}

static int copy_file(const char *src, const char *dst, const char *name)
{
    char    path[MAX_OSPATH];
    byte    buf[0x10000];
    FILE    *ifp, *ofp;
    size_t  len, res;
    int     ret = -1;

    if (Q_snprintf(path, MAX_OSPATH, "%s/%s/%s/%s", fs_gamedir, sv_savedir->string, src, name) >= MAX_OSPATH)
        goto fail0;

    ifp = fopen(path, "rb");
    if (!ifp)
        goto fail0;

    if (Q_snprintf(path, MAX_OSPATH, "%s/%s/%s/%s", fs_gamedir, sv_savedir->string, dst, name) >= MAX_OSPATH)
        goto fail1;

    if (FS_CreatePath(path))
        goto fail1;

    ofp = fopen(path, "wb");
    if (!ofp)
        goto fail1;

    do {
        len = fread(buf, 1, sizeof(buf), ifp);
        res = fwrite(buf, 1, len, ofp);
    } while (len == sizeof(buf) && res == len);

    if (ferror(ifp))
        goto fail2;

    if (ferror(ofp))
        goto fail2;

    ret = 0;
fail2:
    ret |= fclose(ofp);
fail1:
    ret |= fclose(ifp);
fail0:
    return ret;
}

static int remove_file(const char *dir, const char *name)
{
    char path[MAX_OSPATH];

    if (Q_snprintf(path, MAX_OSPATH, "%s/%s/%s/%s", fs_gamedir, sv_savedir->string, dir, name) >= MAX_OSPATH)
        return -1;

    return remove(path);
}

static void **list_save_dir(const char *dir, int *count)
{
    return FS_ListFiles(va("%s/%s", sv_savedir->string, dir), ".ssv;.sav;.sv2",
        FS_TYPE_REAL | FS_PATH_GAME, count);
}

static int wipe_save_dir(const char *dir)
{
    void **list;
    int i, count, ret = 0;

    if ((list = list_save_dir(dir, &count)) == NULL)
        return 0;

    for (i = 0; i < count; i++)
        ret |= remove_file(dir, static_cast<const char*>( list[i] ) ); // WID: C++20: Added cast.

    FS_FreeList(list);
    return ret;
}

static int copy_save_dir(const char *src, const char *dst)
{
    void **list;
    int i, count, ret = 0;

    if ((list = list_save_dir(src, &count)) == NULL)
        return -1;

    for (i = 0; i < count; i++)
        ret |= copy_file(src, dst, static_cast<const char*>( list[i] ) ); // WID: C++20: Added cast.

    FS_FreeList(list);
    return ret;
}

static int read_binary_file(const char *name)
{
    qhandle_t f;
    int64_t len;

    len = FS_OpenFile(name, &f, FS_MODE_READ | FS_TYPE_REAL | FS_PATH_GAME);
    if (!f)
        return -1;

    if (len > MAX_MSGLEN)
        goto fail;

    if (FS_Read(msg_read_buffer, len, f) != len)
        goto fail;

    SZ_Init( &msg_read, msg_read_buffer, sizeof( msg_read_buffer ) );
    msg_read.cursize = len;

    FS_CloseFile(f);
    return 0;

fail:
    FS_CloseFile(f);
    return -1;
}

char *SV_GetSaveInfo(const char *dir)
{
    char        name[MAX_QPATH], date[MAX_QPATH];
    size_t      len;
    int64_t    timestamp;
    int         autosave, year;
    time_t      t;
    struct tm   *tm;

    if (Q_snprintf(name, MAX_QPATH, "%s/%s/server.ssv", sv_savedir->string, dir) >= MAX_QPATH)
        return NULL;

    if (read_binary_file(name))
        return NULL;

    if (MSG_ReadInt32() != SAVE_MAGIC1)
        return NULL;

    if (MSG_ReadInt32() != SAVE_VERSION)
        return NULL;

    // read the comment field
    timestamp = MSG_ReadInt64();
    //timestamp = (uint64_t)MSG_ReadInt32();
    //timestamp |= (uint64_t)MSG_ReadInt32() << 32;
    autosave = MSG_ReadUint8();
    MSG_ReadString(name, sizeof(name));

    if (autosave)
        return Z_CopyString(va("ENTERING %s", name));

    // get current year
    t = time(NULL);
    tm = localtime(&t);
    year = tm ? tm->tm_year : -1;

    // format savegame date
    t = timestamp;
    len = 0;
    if ((tm = localtime(&t)) != NULL) {
        if (tm->tm_year == year)
            len = strftime(date, sizeof(date), "%b %d %H:%M", tm);
        else
            len = strftime(date, sizeof(date), "%b %d  %Y", tm);
    } else {
        len = strftime(date, sizeof(date), "%b %d  %Y", tm);
    }
    if (!len)
        strcpy(date, "???");

    return Z_CopyString(va("%s %s", date, name));
}
/**
*   @brief  Frees the pending Collision Model.
**/
static void abort_func(void *arg)
{
    CM_FreeMap( static_cast<cm_t*>( arg ) ); // WID: C++20: Added cast.
}

/**
*   @brief  Read in the 'server save' file.
**/
static int read_server_file(void) {
    char        name[ MAX_OSPATH ] = {}, string[ MAX_STRING_CHARS ] = {};
    mapcmd_t    cmd = {};

    // errors like missing file, bad version, etc are
    // non-fatal and just return to the command handler

	Q_snprintf(name, MAX_OSPATH, "%s/%s/server.ssv", 
        sv_savedir->string, SAVE_CURRENT );
    if ( read_binary_file( name ) ) {
        return -1;
    }

    if ( MSG_ReadInt32() != SAVE_MAGIC1 ) {
        return -1;
    }

    if ( MSG_ReadInt32() != SAVE_VERSION ) {
        return -1;
    }

    cmd = {};//memset( &cmd, 0, sizeof( cmd ) );

    // Read the comment field.
    //MSG_ReadInt32();
    //MSG_ReadInt32();
    MSG_ReadInt64();

    // Check whether it was an autosave or regular savegame.
    if ( MSG_ReadUint8() ) {
        cmd.loadgame = 2;   // autosave
    } else {
        cmd.loadgame = 1;   // regular savegame
    }
    // Read in a null string.
    MSG_ReadString(NULL, 0);

    // Read the map command string.
    if ( MSG_ReadString( cmd.buffer, sizeof( cmd.buffer ) ) >= sizeof( cmd.buffer ) ) {
        return -1;
    }

    // Try and load the map.
    if ( !SV_ParseMapCmd( &cmd ) ) {
        return -1;
    }

    // Save pending CM to be freed later if ERR_DROP is thrown
    Com_AbortFunc( abort_func, &cmd.cm );

    // any error will drop from this point
    SV_Shutdown( "Server restarted\n", ERR_RECONNECT );

    // the rest can't underflow
    msg_read.allowunderflow = false;

    // read all CVAR_LATCH cvars
    // these will be things like coop, skill, deathmatch, etc
    while ( 1 ) {
        if ( MSG_ReadString( name, MAX_QPATH ) >= MAX_QPATH ) {
            Com_Error( ERR_DROP, "Savegame cvar name too long" );
            return 0;
        }
        if ( !name[ 0 ] ) {
            break;
        }

        if ( MSG_ReadString( string, sizeof( string ) ) >= sizeof( string ) ) {
            Com_Error( ERR_DROP, "Savegame cvar value too long" );
        }

        Cvar_UserSet( name, string );
    }

    // Start a new game fresh with new cvars.
    SV_InitGame();

    //// error out immediately if game doesn't support safe savegames
    //if (sv_force_enhanced_savegames->integer && !(g_features->integer & GMF_ENHANCED_SAVEGAMES))
    //    Com_Error(ERR_DROP, "Game does not support enhanced savegames");

    // Read the game state.
    if ( Q_snprintf( name, MAX_OSPATH, "%s/%s/%s/game.ssv",
        fs_gamedir, sv_savedir->string, SAVE_CURRENT ) >= MAX_OSPATH ) {
        Com_Error( ERR_DROP, "Savegame path too long" );
    }
    ge->ReadGame( name );

    // Clear pending CM.
    Com_AbortFunc( NULL, NULL );

    // Go to the map by spawning the server.
    SV_SpawnServer( &cmd );

    // Return.
    return 0;
}

/**
*   @brief  Reads in the 'level save' file.
**/
static int read_level_file(void)
{
    char    name[MAX_OSPATH];
    size_t  len, maxlen;
    int     index;

    // Generate path string.
    if ( Q_snprintf( name, MAX_OSPATH, "%s/%s/%s.sv2",
        sv_savedir->string, SAVE_CURRENT, sv.name ) >= MAX_OSPATH ) {
        return -1;
    }
    // Read the file.
    if ( read_binary_file( name ) ) {
        return -1;
    }

    if ( MSG_ReadInt32() != SAVE_MAGIC2 ) {
        return -1;
    }

    if ( MSG_ReadInt32() != SAVE_VERSION ) {
        return -1;
    }

    // any error will drop from this point

    // the rest can't underflow
    msg_read.allowunderflow = false;

    // read all configstrings
    while (1) {
        index = MSG_ReadInt16();
        if ( index == MAX_CONFIGSTRINGS ) {
            break;
        }

        if ( index < 0 || index > MAX_CONFIGSTRINGS ) {
            Com_Error( ERR_DROP, "Bad savegame configstring index" );
        }

        maxlen = CS_SIZE(index);
        if ( MSG_ReadString( sv.configstrings[ index ], maxlen ) >= maxlen ) {
            Com_Error( ERR_DROP, "Savegame configstring too long" );
        }
    }

    // Read in portalbits length.
    len = MSG_ReadUint8();
    if ( len > MAX_MAP_PORTAL_BYTES ) {
        Com_Error( ERR_DROP, "Savegame portalbits too long" );
    }

    // Clear world links.
    SV_ClearWorld();

    // Set portalstate data to that which we just loaded.
    CM_SetPortalStates(&sv.cm, MSG_ReadData(len), len);

    // Read game level.
    if ( Q_snprintf( name, MAX_OSPATH, "%s/%s/%s/%s.sav",
        fs_gamedir, sv_savedir->string, SAVE_CURRENT, sv.name ) >= MAX_OSPATH ) {
        Com_Error( ERR_DROP, "Savegame path too long" );
    }
    // Read in the game level data.
    ge->ReadLevel(name);
    return 0;
}

/**
*	@brief
**/
const int32_t SV_NoSaveGames(void)
{
	//if (dedicated->integer && !Cvar_VariableInteger("coop"))
 //       return 1;

    //if (sv_force_enhanced_savegames->integer && !(g_features->integer & GMF_ENHANCED_SAVEGAMES))
    //    return 1;

    //if (Cvar_VariableInteger("deathmatch"))
    //    return 1;

    return !ge->GameModeAllowSaveGames( dedicated->integer );
}

/**
*	@brief
**/
void SV_AutoSaveBegin(mapcmd_t *cmd) {
    byte        bitmap[MAX_CLIENTS / CHAR_BIT];
    sv_edict_t     *ent;
    int         i;

    // check for clearing the current savegame
    if (cmd->endofunit) {
        wipe_save_dir(SAVE_CURRENT);
        return;
    }

    if (sv.state != ss_game)
        return;

    if (SV_NoSaveGames())
        return;

    memset(bitmap, 0, sizeof(bitmap));

    // Clear all the client inUse flags before saving so that
    // when the level is re-entered, the clients will spawn
    // at spawn points instead of occupying body shells
    for (i = 0; i < sv_maxclients->integer; i++) {
        // Get client edict.
        ent = EDICT_FOR_NUMBER(i + 1);
        // If in use.
        if (ent->inUse) {
            // Store client ID in the bitmap.
            Q_SetBit(bitmap, i);
            // 'Disable' temporarily the client entity by inUse = false.
            ent->inUse = false;
        }
    }

    // Save the map just exited.
    if ( write_level_file() ) {
        Com_EPrintf( "Couldn't write level file.\n" );
    }
    // We must restore these for clients to transfer over correctly.
    for ( i = 0; i < sv_maxclients->integer; i++ ) {
        ent = EDICT_FOR_NUMBER( i + 1 );
        ent->inUse = Q_IsBitSet( bitmap, i );
    }
}

/**
*	@brief
**/
void SV_AutoSaveEnd(void)
{
    if ( sv.state != ss_game ) {
        return;
    }

    if ( SV_NoSaveGames() ) {
        return;
    }

	// Save the map just entered to include the player position (client edict shell)
    if ( write_level_file() ) {
        Com_EPrintf( "Couldn't write level file.\n" );
        return;
    }

    // Save server state.
    if ( write_server_file( true /* autosave == true */) ) {
        Com_EPrintf( "Couldn't write server file.\n" );
        return;
    }

    // Clear whatever savegames are there.
    if ( wipe_save_dir( SAVE_AUTO ) ) {
        Com_EPrintf( "Couldn't wipe '%s' directory.\n", SAVE_AUTO );
        return;
    }

    // Copy off the level to the autosave slot.
    if ( copy_save_dir( SAVE_CURRENT, SAVE_AUTO ) ) {
        Com_EPrintf( "Couldn't write '%s' directory.\n", SAVE_AUTO );
        return;
    }
}

void SV_CheckForSavegame(mapcmd_t *cmd)
{
    if (SV_NoSaveGames())
        return;

    if ( read_level_file() ) {
        // only warn when loading a regular savegame. autosave without level
        // file is ok and simply starts the map from the beginning.
        if ( cmd->loadgame == 1 )
            Com_EPrintf( "Couldn't read level file.\n" );
        return;
    }

    if ( cmd->loadgame ) {
        // called from SV_Loadgame_f
        ge->RunFrame();
        ge->RunFrame();
    } else {
        int i;

        // coming back to a level after being in a different
        // level, so run it for ten seconds
		//for (i = 0; i < 100; i++)
		// WID: 40hz:
		for( i = 0; i < 400; i++ )
            ge->RunFrame();
    }
}

/**
*   @brief  Lists all possible save files for save cmd.
**/
static void SV_Savegame_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        FS_File_g("save", NULL, FS_SEARCH_DIRSONLY | FS_TYPE_REAL | FS_PATH_GAME, ctx);
    }
}

static void SV_Loadgame_f(void)
{
    char *dir;

    if ( Cmd_Argc() != 2 ) {
        Com_Printf( "Usage: %s <directory>\n", Cmd_Argv( 0 ) );
        return;
    }

    if ( dedicated->integer ) {
        Com_Printf( "Savegames are for listen servers only.\n" );
        return;
    }

    dir = Cmd_Argv( 1 );
    if ( !COM_IsPath( dir ) ) {
        Com_Printf( "Bad savedir.\n" );
        return;
    }

    // Make sure the server files exist.
    if ( !FS_FileExistsEx( va( "%s/%s/server.ssv", sv_savedir->string, dir ), FS_TYPE_REAL | FS_PATH_GAME )
        || !FS_FileExistsEx( va( "%s/%s/game.ssv", sv_savedir->string, dir ), FS_TYPE_REAL | FS_PATH_GAME ) ) {
        Com_Printf( "No such savegame: %s\n", dir );
        return;
    }

    // Clear whatever savegames are there.
    if (wipe_save_dir(SAVE_CURRENT)) {
        Com_Printf("Couldn't wipe '%s' directory.\n", SAVE_CURRENT);
        return;
    }

    // Copy it off.
    if ( copy_save_dir( dir, SAVE_CURRENT ) ) {
        Com_Printf( "Couldn't read '%s' directory.\n", dir );
        return;
    }

    // Read server state.
    if ( read_server_file() ) {
        Com_Printf( "Couldn't read server file.\n" );
        return;
    }
}

static void SV_Savegame_f(void)
{
    char *dir;

    if ( sv.state != ss_game ) {
        Com_Printf( "You must be in a game to save.\n" );
        return;
    }

    if ( dedicated->integer ) {
        Com_Printf( "Savegames are for listen servers only.\n" );
        return;
    }

    //// don't bother saving if we can't read them back!
    //if (sv_force_enhanced_savegames->integer && !(g_features->integer & GMF_ENHANCED_SAVEGAMES)) {
    //    Com_Printf("Game does not support enhanced savegames.\n");
    //    return;
    //}

    //if (Cvar_VariableInteger("deathmatch")) {
    //    Com_Printf("Can't savegame in a deathmatch.\n");
    //    return;
    //}
    if ( ge->GameModeAllowSaveGames( dedicated->integer ) == false ) {
        Com_Printf( "The gamemode \"%s\" doesn't support savegames!\n", ge->GetGameModeName( ge->GetRequestedGameModeType() ) );
        return;
    }

    //if ( sv_maxclients->integer == 1 && svs.client_pool[ 0 ].edict->client->ps.stats[ STAT_HEALTH ] <= 0 ) {
    sv_edict_t *clent = EDICT_FOR_NUMBER( 1 );//sv_client->edict;
	// <Q2RTXPP>: WID: TODO: Test for a singleplayer game actually being played.
    if ( sv_maxclients->integer == 1 && ( /*clent &&*/ clent->client->ps.stats[ STAT_HEALTH ] <= 0 ) ) {
        Com_Printf( "Can't savegame while dead!\n" );
        return;
    }

    if ( Cmd_Argc() != 2 ) {
        Com_Printf( "Usage: %s <directory>\n", Cmd_Argv( 0 ) );
        return;
    }

    dir = Cmd_Argv( 1 );
    if ( !COM_IsPath( dir ) ) {
        Com_Printf( "Bad savedir.\n" );
        return;
    }

    // Archive current level, including all client edicts.
    // when the level is reloaded, they will be shells awaiting
    // a connecting client.
    if ( write_level_file() ) {
        Com_Printf( "Couldn't write level file.\n" );
        return;
    }

    // Save server state.
    if ( write_server_file( false ) ) {
        Com_Printf( "Couldn't write server file.\n" );
        return;
    }

    // Clear whatever savegames are there.
    if ( wipe_save_dir( dir ) ) {
        Com_Printf( "Couldn't wipe '%s' directory.\n", dir );
        return;
    }

    // Copy it off.
    if ( copy_save_dir( SAVE_CURRENT, dir ) ) {
        Com_Printf( "Couldn't write '%s' directory.\n", dir );
        return;
    }

    Com_Printf( "Game saved.\n" );
}

static const cmdreg_t c_savegames[] = {
    { "save", SV_Savegame_f, SV_Savegame_c },
    { "load", SV_Loadgame_f, SV_Savegame_c },
    { NULL }
};

void SV_RegisterSavegames(void)
{
    Cmd_Register(c_savegames);
	sv_savedir = Cvar_Get("sv_savedir", "save", 0);
	//sv_force_enhanced_savegames = Cvar_Get("sv_force_enhanced_savegames", "0", 0);
}
