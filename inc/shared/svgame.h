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

#ifndef SVGAME_H
#define SVGAME_H

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

#include "shared/util_list.h"

// Include needed shared refresh types.
#include "refresh/shared_types.h"

//
// svgame.h -- server game dll information visible to server
//

#define SVGAME_API_VERSION    1337

// edict->svflags

// Old Vanilla Q2 flags.
//#define SVF_NOCLIENT            0x00000001  // Don't send entity to clients, even if it has effects
//#define SVF_DEADMONSTER         0x00000002  // Treat as CONTENTS_DEADMONSTER for collision
//#define SVF_MONSTER             0x00000004  // Treat as CONTENTS_MONSTER for collision
//#define SVF_HULL	0x00000008	// When touching the trigger's bounding box, perform an additional clip to trigger brush. (Used for G_TouchTriggers)

// Some [KEX] flags.
#define SVF_NONE            0           // No serverflags.
#define SVF_NOCLIENT        BIT( 0 )    // Don't send entity to clients, even if it has effects.
#define SVF_DEADMONSTER     BIT( 1 )    // Treat as CONTENTS_DEADMONSTER for collision.
#define SVF_MONSTER         BIT( 2 )    // Treat as CONTENTS_MONSTER for collision.
#define SVF_PLAYER          BIT( 3 )    // [Paril-KEX] Treat as CONTENTS_PLAYER for collision.
//#define SVF_BOT           BIT( 4 )    // Entity is controlled by a bot AI.
//#define SVF_NOBOTS        BIT( 5 )    // Don't allow bots to use/interact with entity.
//#define SVF_RESPAWNING    BIT( 6 )    // Entity will respawn on it's next think.
#define SVF_PROJECTILE      BIT( 7 )    // Treat as CONTENTS_PROJECTILE for collision.
//#define SVF_INSTANCED     BIT( 8 )    // Entity has different visibility per player.
#define SVF_DOOR            BIT( 9 )    // Entity is a door of some kind.
//#define SVF_NOCULL        BIT( 10 )   // Always send, even if we normally wouldn't.
#define SVF_HULL            BIT( 11 )   // Always use hull when appropriate (triggers, etc; for gi.clip).

// extended features

#define GMF_CLIENTNUM               0x00000001
#define GMF_PROPERINUSE             0x00000002
//#define GMF_MVDSPEC                 0x00000004
#define GMF_WANT_ALL_DISCONNECTS    0x00000008

//#define GMF_ENHANCED_SAVEGAMES      0x00000400
//#define GMF_VARIABLE_FPS            0x00000800
#define GMF_EXTRA_USERINFO          0x00001000
#define GMF_IPV6_ADDRESS_AWARE      0x00002000

//===============================================================

typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;

#ifndef SVGAME_INCLUDE

typedef struct gclient_s {
    player_state_t  ps;     // communicated by server to clients
    int             ping;

    // the game dll can add anything it wants after
    // this point in the structure
    int             clientNum;
} gclient_t;


typedef struct edict_s {
    entity_state_t  s;
    struct gclient_s *client;   //! NULL if not a player the server expects the first part
                                //! of gclient_s to be a player_state_t but the rest of it is opaque
    qboolean inuse;
    int32_t linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t area;    //! Linked to a division node or leaf

    int32_t num_clusters;       // If -1, use headnode instead.
    int32_t clusternums[MAX_ENT_CLUSTERS];
    int32_t headnode;           // Unused if num_clusters != -1
    
    int32_t areanum, areanum2;

    //================================

    int32_t     svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    contents_t  clipmask;
    contents_t  hullContents;
    edict_t     *owner;

    // the game dll can add anything it wants after
    // this point in the structure
} edict_t;
#else

#endif      // SVGAME_INCLUDE

//===============================================================

/**
*	@brief	Functions and variable data that is provided by the main engine.
**/
typedef struct {
	/**
	*	Consts and Variables:
	**/
	uint32_t    tick_rate;
	float       frame_time_s;
	uint32_t    frame_time_ms;

    const int64_t ( *GetServerFrameNumber )( void );

	/**
	*
	*	Special Messages:
	*
	**/
    void (* q_printf(2, 3) bprintf)(int printlevel, const char *fmt, ...);
    void (* q_printf(1, 2) dprintf)(const char *fmt, ...);
    void (* q_printf(3, 4) cprintf)(edict_t *ent, int printlevel, const char *fmt, ...);
    void (* q_printf(2, 3) centerprintf)(edict_t *ent, const char *fmt, ...);
    void (*sound)(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
    void (*positioned_sound)(const vec3_t origin, edict_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs);

	/**
    *	Config strings hold all the index strings, the lightstyles, the models that are in-use,
    *	and also misc data like the sky definition and cdtrack.
	* 
    *	All of the current configstrings are sent to clients when they connect, 
	*	and in case of any changes, which too are sent to all connected clients.
	**/
	configstring_t *(*GetConfigString)( const int32_t index );
    void (*configstring)(int num, const char *string);

    void (* q_noreturn q_printf(1, 2) error)(const char *fmt, ...);

    //! Get access to the actual pointer of a loaded configstring model.
    const model_t *( *GetModelDataForName )( const char *name );
    const model_t *( *GetModelDataForHandle )( const qhandle_t handle );

    // the *index functions create configstrings(precache iqm models) and some internal server state.
	// these are sent over to the client 
    int (*modelindex)(const char *name);
    int (*soundindex)(const char *name);
    int (*imageindex)(const char *name);

    void (*setmodel)(edict_t *ent, const char *name);

	/**
	*
	*	Collision Detection:
	*
	**/
    const trace_t (* q_gameabi trace)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passent, const contents_t contentmask );
	const trace_t( *q_gameabi clip )( edict_t *entity, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const contents_t contentmask );
    const contents_t (*pointcontents)(const vec3_t point);
    const qboolean (*inPVS)(const vec3_t p1, const vec3_t p2);
    const qboolean (*inPHS)(const vec3_t p1, const vec3_t p2);
    void (*SetAreaPortalState)( const int32_t portalnum, const bool open);
    const int32_t ( *GetAreaPortalState )( const int32_t portalnum );
    const qboolean (*AreasConnected)(const int32_t area1, const int32_t area2);
    /**
    *	An entity will never be sent to a client or used for collision if it is not passed to linkentity.  
    *   If the size, position, solidity, clipmask, hullContents, or owner changes, it must be relinked.
	**/
    void (*linkentity)(edict_t *ent);
    void (*unlinkentity)(edict_t *ent);     
    const int32_t(*BoxEdicts)(const vec3_t mins, const vec3_t maxs, edict_t **list, const int32_t maxcount, const int32_t areatype);
    


    /**
    *
    *	(Collision Model-) Entities:
    *
    **/
    /**
    *   @brief  Looks up the key/value cm_entity_t pair in the list for the cm_entity_t entity.
    *   @return If found, a pointer to the key/value pair, otherwise a pointer to the 'cm_null_entity'.
    **/
    const cm_entity_t *( *CM_EntityKeyValue )( const cm_entity_t *edict, const char *key );
    /**
    *   @brief  Used to check whether CM_EntityValue was able/unable to find a matching key in the cm_entity_t.
    *   @return Pointer to the collision model system's 'null' entity key/pair.
    **/
    const cm_entity_t *( *CM_GetNullEntity )( void );



    /**
    *
    * 
    *	FileSystem:
    *
    *
    **/
    /**
    *	@brief	Returns non 0 in case of existance.
    **/
    const int32_t( *FS_FileExistsEx )( const char *path, const uint32_t flags );
    /**
    *	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
    *	@return	length < 0 indicates error.
    **/
    const int32_t( *FS_LoadFile )( const char *path, void **buffer );



	/**
	*
	*	Network Messaging:
	*
	**/
    void ( *multicast )( const vec3_t origin, multicast_t to, bool reliable );
    void ( *unicast )( edict_t *ent, bool reliable );
    void ( *WriteInt8 )( const int32_t c );
    void ( *WriteUint8 )( const int32_t c );
    void ( *WriteInt16 )( const int32_t c );
    void ( *WriteUint16 )( const int32_t c );
    void ( *WriteInt32 )( const int32_t c );
    void ( *WriteInt64 )( const int64_t c );
    void ( *WriteIntBase128 )( int64_t c );
    void ( *WriteUintBase128 )( uint64_t c );
    void ( *WriteFloat )( const float f );
    void ( *WriteString )( const char *s );
    void ( *WritePosition )( const vec3_t pos, const msgPositionEncoding_t encoding );    // some fractional bits
    void ( *WriteDir8 )( const vec3_t pos );         // single byte encoded, very coarse
    void ( *WriteAngle8 )( const float f );
    void ( *WriteAngle16 )( const float f );

	/**
	*
	*	'Tag' Managed Memory Allocation:
	*
	**/
    void *(*TagMalloc)(unsigned size, unsigned tag);
    void (*TagFree)(void *block);
    void (*FreeTags)(unsigned tag);

	/**
	*
	*	Console Variable Interaction:
	*
	**/
    cvar_t *(*cvar)(const char *var_name, const char *value, int flags);
    cvar_t *(*cvar_set)(const char *var_name, const char *value);
    cvar_t *(*cvar_forceset)(const char *var_name, const char *value);

	/**
	*
	*	ClientCommand and ServerCommand parameter access:
	*
	**/
    const int32_t (*argc)(void);
    char *(*argv)(int n);
    char *(*args)(void);     // concatenation of all argv >= 1

	/**
    *	Add commands to the server console as if they were typed in
    *	for map changing, etc
	**/
    void (*AddCommandString)(const char *text);

    void (*DebugGraph)(float value, int color);
} svgame_import_t;

/**
*	Functions and Variables Exported by the ServerGame subsystem:
**/
typedef struct {
    int         apiversion;
	
	/**
	*	Init/Shutdown:
	* 
    *	The preinit and init functions will only be called when a game starts,
    *	not each time a level is loaded.  Persistant data for clients
    *	and the server can be allocated in init
	**/
	void (*PreInit)( void );
	void (*Init)(void);
    void (*Shutdown)(void);

    //! Each new level entered will cause a call to SpawnEntities
    void (*SpawnEntities)( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );

	/**
	*	GameModes:
	**/
    /**
    *	@return	True if the game mode is a legitimate existing one.
    **/
    const bool ( *IsValidGameModeType )( const int32_t gameModeID );
    /**
    *   @return True if the game mode is multiplayer.
    **/
    const bool ( *IsMultiplayerGameMode ) ( const int32_t gameModeID );
    /**
    *	@return	The current active game mode ID.
    **/
    const int32_t( *GetActiveGameModeType )( void );
    /**
    *	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
    **/
    const int32_t( *GetDefaultMultiplayerGamemodeType )( void );
    /**
    *	@return	The actual ID of the current gamemode.
    **/
    const char *( *GetGamemodeName )( const int32_t gameModeID );
    /**
    *	@return	True in case the current gamemode allows for saving the game.
    *			(This should only be true for single and cooperative play modes.)
    **/
    const bool ( *GamemodeNoSaveGames )( const bool isDedicated );

	/**
	*	Read/Write Game: 
	*	Is for storing persistant cross level information about the world state and the clients.
	* 
    *	WriteGame is called every time a level is exited.
    *	ReadGame is called on a loadgame.
	**/
    void (*WriteGame)(const char *filename, qboolean autosave);
    void (*ReadGame)(const char *filename);

	/**
	*	Read/Write Level:
	* 
	*	ReadLevel is called after the default map information has been
    *	loaded with SpawnEntities
	**/
    void (*WriteLevel)(const char *filename);
    void (*ReadLevel)(const char *filename);

    /**
    *
	*	Client(s):
	* 
    **/
    qboolean (*ClientConnect)(edict_t *ent, char *userinfo);
    void (*ClientBegin)(edict_t *ent);
    void (*ClientUserinfoChanged)(edict_t *ent, char *userinfo);
    void (*ClientDisconnect)(edict_t *ent);
    void (*ClientCommand)(edict_t *ent);
    void (*ClientThink)(edict_t *ent, usercmd_t *cmd);

	/**
	*
	*	Player Movement:
	*
	**/
    //! Perform a frame's worth of player movement using specified pmoveParams configuration.
	void ( *PlayerMove )( pmove_t *pmove, pmoveParams_t *params );
    //! Setup the basic player move configuration parameters. (Used by server for new clients.)
	void ( *ConfigurePlayerMoveParameters )( pmoveParams_t *pmp );

    /**
    *
	*	Frames:
	* 
	* 
    **/
    //! Runs a single frame of the server game.
    void (*RunFrame)(void);

	/**
	*
	*	ServerCommand:
	*
	**/
    //! ServerCommand will be called when an "sv <command>" command is issued on the
    //! server console.
    //! The game can issue gi.argc() / gi.argv() commands to get the rest
    //! of the parameters
    void (*ServerCommand)(void);

    /**
    *	Global variables shared between game and server
    *
    *	The edict array is allocated in the game dll so it
    *	can vary in size from one game to another.
    *
    *	The size will be fixed when ge->Init() is called
	**/
    struct edict_s  *edicts;
    int32_t edict_size;
    int32_t num_edicts;     // current number, <= max_edicts
    int32_t max_edicts;
} svgame_export_t;

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif

#endif // SVGAME_H
