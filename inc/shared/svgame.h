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

#include "shared/list.h"

//
// svgame.h -- server game dll information visible to server
//

#define SVGAME_API_VERSION    1337

// edict->svflags

#define SVF_NOCLIENT            0x00000001  // Don't send entity to clients, even if it has effects
#define SVF_DEADMONSTER         0x00000002  // Treat as CONTENTS_DEADMONSTER for collision
#define SVF_MONSTER             0x00000004  // Treat as CONTENTS_MONSTER for collision
#define SVF_USE_TRIGGER_HULL	0x00000008	// When touching the trigger's bounding box, perform an additional clip to trigger brush. (Used for G_TouchTriggers)
// edict->solid values

typedef enum {
    SOLID_NOT,          // no interaction with other objects
    SOLID_TRIGGER,      // only touch when inside, after moving
    SOLID_BBOX,         // touch on edge
    SOLID_BSP           // bsp clip, touch on edge
} solid_t;

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

#define MAX_ENT_CLUSTERS    16


typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;


#ifndef SVGAME_INCLUDE

struct gclient_s {
    player_state_t  ps;     // communicated by server to clients
    int             ping;

    // the game dll can add anything it wants after
    // this point in the structure
    int             clientNum;
};


struct edict_s {
    entity_state_t  s;
    struct gclient_s    *client;
    qboolean    inuse;
    int         linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t      area;               // linked to a division node or leaf

    int         num_clusters;       // if -1, use headnode instead
    int         clusternums[MAX_ENT_CLUSTERS];
    int         headnode;           // unused if num_clusters != -1
    int         areanum, areanum2;

    //================================

    int         svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    int         clipmask;
    edict_t     *owner;

    // the game dll can add anything it wants after
    // this point in the structure
};

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
	configstring_t *(*GetConfigString)( int32_t index );
    void (*configstring)(int num, const char *string);

    void (* q_noreturn q_printf(1, 2) error)(const char *fmt, ...);

    // the *index functions create configstrings and some internal server state.
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
    trace_t (* q_gameabi trace)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passent, int contentmask);
	trace_t( *q_gameabi clip )( edict_t *entity, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentmask );
    int (*pointcontents)(const vec3_t point);
    qboolean (*inPVS)(const vec3_t p1, const vec3_t p2);
    qboolean (*inPHS)(const vec3_t p1, const vec3_t p2);
    void (*SetAreaPortalState)(int portalnum, qboolean open);
    qboolean (*AreasConnected)(int area1, int area2);

	/**
    *	An entity will never be sent to a client or used for collision
    *	if it is not passed to linkentity.  If the size, position, or
    *	solidity changes, it must be relinked.
	**/
    void (*linkentity)(edict_t *ent);
    void (*unlinkentity)(edict_t *ent);     // call before removing an interactive edict
    int (*BoxEdicts)(const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype);
    //void (*Pmove)(pmove_t *pmove);          // player movement code common with client prediction

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
    void ( *WritePosition )( const vec3_t pos, const bool encodeAsShort );    // some fractional bits
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
    int (*argc)(void);
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

    // Each new level entered will cause a call to SpawnEntities
    void (*SpawnEntities)(const char *mapname, const char *entstring, const char *spawnpoint);

	/**
	*	GameModes:
	**/
	const int32_t (*GetGamemodeID)( );
	const char *(*GetGamemodeName)( const int32_t gameModeID );
	const bool (*GamemodeNoSaveGames)( const bool isDedicated );

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
	void ( *PlayerMove )( pmove_t *pmove, pmoveParams_t *params );
	void ( *ConfigurePlayerMoveParameters )( pmoveParams_t *pmp );

    /**
    *
	*	Frames:
	* 
	* 
    **/
    void (*RunFrame)(void);

	/**
	*
	*	ServerCommand:
	*
	**/
    // ServerCommand will be called when an "sv <command>" command is issued on the
    // server console.
    // The game can issue gi.argc() / gi.argv() commands to get the rest
    // of the parameters
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
    int         edict_size;
    int         num_edicts;     // current number, <= max_edicts
    int         max_edicts;
} svgame_export_t;

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif

#endif // SVGAME_H
