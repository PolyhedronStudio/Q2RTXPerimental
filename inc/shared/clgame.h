/********************************************************************
*
*
*	ClientGame Header.
*
*
********************************************************************/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//
// clgame.h -- client game dll information visible to client
//
#define CLGAME_API_VERSION    3

// edict->svflags

#define SVF_NOCLIENT            0x00000001  // don't send entity to clients, even if it has effects
#define SVF_DEADMONSTER         0x00000002  // treat as CONTENTS_DEADMONSTER for collision
#define SVF_MONSTER             0x00000004  // treat as CONTENTS_MONSTER for collision

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
#define GMF_MVDSPEC                 0x00000004
#define GMF_WANT_ALL_DISCONNECTS    0x00000008

#define GMF_ENHANCED_SAVEGAMES      0x00000400
#define GMF_VARIABLE_FPS            0x00000800
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

//
// functions provided by the main engine
//
typedef struct {
	// Similar to Q2RE now.
	uint32_t    tick_rate;
	float       frame_time_s;
	uint32_t    frame_time_ms;

    // special messages
    void (* q_printf(2, 3) bprintf)(int printlevel, const char *fmt, ...);
    void (* q_printf(1, 2) dprintf)(const char *fmt, ...);
    void (* q_printf(3, 4) cprintf)(edict_t *ent, int printlevel, const char *fmt, ...);
    void (* q_printf(2, 3) centerprintf)(edict_t *ent, const char *fmt, ...);
    //void (*sound)(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
    //void (*positioned_sound)(const vec3_t origin, edict_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs);

    // config strings hold all the index strings, the lightstyles,
    // and misc data like the sky definition and cdtrack.
    // All of the current configstrings are sent to clients when
    // they connect, and changes are sent to all connected clients.
    //void (*configstring)(int num, const char *string);

    void (* q_noreturn q_printf(1, 2) error)(const char *fmt, ...);

    //// the *index functions create configstrings and some internal server state
    //int (*modelindex)(const char *name);
    //int (*soundindex)(const char *name);
    //int (*imageindex)(const char *name);

    //void (*setmodel)(edict_t *ent, const char *name);

    //// collision detection
    //trace_t (* q_gameabi trace)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passent, int contentmask);
    //int (*pointcontents)(const vec3_t point);
    //qboolean (*inPVS)(const vec3_t p1, const vec3_t p2);
    //qboolean (*inPHS)(const vec3_t p1, const vec3_t p2);
    //void (*SetAreaPortalState)(int portalnum, qboolean open);
    //qboolean (*AreasConnected)(int area1, int area2);

    //// an entity will never be sent to a client or used for collision
    //// if it is not passed to linkentity.  If the size, position, or
    //// solidity changes, it must be relinked.
    //void (*linkentity)(edict_t *ent);
    //void (*unlinkentity)(edict_t *ent);     // call before removing an interactive edict
    //int (*BoxEdicts)(const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype);
    //void (*Pmove)(pmove_t *pmove);          // player movement code common with client prediction

    // network messaging
    //void (*multicast)(const vec3_t origin, multicast_t to);
    //void (*unicast)(edict_t *ent, qboolean reliable);
    void (*ReadChar)(int c);
    void (*ReadByte)(int c);
    void (*ReadShort)(int c);
    void (*ReadLong)(int c);
    void (*ReadFloat)(float f);
    void (*ReadString)(const char *s);
    void (*ReadPosition)(const vec3_t pos);    // some fractional bits
    void (*ReadDir)(const vec3_t pos);         // single byte encoded, very coarse
    void (*ReadAngle)(float f);

    // managed memory allocation
    void *(*TagMalloc)(unsigned size, unsigned tag);
    void (*TagFree)(void *block);
    void (*FreeTags)(unsigned tag);

    // console variable interaction
    cvar_t *(*cvar)(const char *var_name, const char *value, int flags);
    cvar_t *(*cvar_set)(const char *var_name, const char *value);
    cvar_t *(*cvar_forceset)(const char *var_name, const char *value);

    //// ClientCommand and ServerCommand parameter access
    //int (*argc)(void);
    //char *(*argv)(int n);
    //char *(*args)(void);     // concatenation of all argv >= 1

    //// add commands to the server console as if they were typed in
    //// for map changing, etc
    //void (*AddCommandString)(const char *text);

    //void (*DebugGraph)(float value, int color);
} clgame_import_t;

//
// functions exported by the game subsystem
//
typedef struct {
    int         apiversion;

    // the init function will only be called when a game starts,
    // not each time a level is loaded.  Persistant data for clients
    // and the server can be allocated in init
    void (*Init)(void);
    void (*Shutdown)(void);

    //// each new level entered will cause a call to SpawnEntities
    //void (*SpawnEntities)(const char *mapname, const char *entstring, const char *spawnpoint);

    //// Read/Write Game is for storing persistant cross level information
    //// about the world state and the clients.
    //// WriteGame is called every time a level is exited.
    //// ReadGame is called on a loadgame.
    //void (*WriteGame)(const char *filename, qboolean autosave);
    //void (*ReadGame)(const char *filename);

    //// ReadLevel is called after the default map information has been
    //// loaded with SpawnEntities
    //void (*WriteLevel)(const char *filename);
    //void (*ReadLevel)(const char *filename);

    //qboolean (*ClientConnect)(edict_t *ent, char *userinfo);
    //void (*ClientBegin)(edict_t *ent);
    //void (*ClientUserinfoChanged)(edict_t *ent, char *userinfo);
    //void (*ClientDisconnect)(edict_t *ent);
    //void (*ClientCommand)(edict_t *ent);
    //void (*ClientThink)(edict_t *ent, usercmd_t *cmd);

    //void (*RunFrame)(void);

    //// ServerCommand will be called when an "sv <command>" command is issued on the
    //// server console.
    //// The game can issue gi.argc() / gi.argv() commands to get the rest
    //// of the parameters
    //void (*ServerCommand)(void);

    //
    // global variables shared between game and server
    //

    // The edict array is allocated in the game dll so it
    // can vary in size from one game to another.
    //
    // The size will be fixed when ge->Init() is called
    //struct edict_s  *edicts;
    //int         edict_size;
    //int         num_edicts;     // current number, <= max_edicts
    //int         max_edicts;
} clgame_export_t;

#ifdef __cplusplus
}
#endif