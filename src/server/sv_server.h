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
// server.h
#pragma once


#include "shared/shared.h"
#include "shared/util/util_list.h"
#include "shared/server/sv_game.h"

#include "common/bsp.h"
#include "common/cmd.h"
#include "common/collisionmodel.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/error.h"
#include "common/files.h"
#include "common/intreadwrite.h"
#include "common/messaging.h"
#include "common/net/net.h"
#include "common/net/chan.h"
//#include "common/pmove.h"
#include "common/prompt.h"
#include "common/protocol.h"
#include "common/zone.h"

// "Common".
#include "client/client.h"
#include "server/server.h"
#include "system/system.h"

#if USE_ZLIB
#include <zlib.h>
#endif


//! Allow all over access to the servergame exported API.
extern svgame_export_t *ge;


/**
*   
*   Wrapper functions for TAG_SERVER memory allocation.
* 
**/
//#define SV_Malloc(size)         Z_TagMalloc(size, TAG_SERVER)
//#define SV_Mallocz(size)        Z_TagMallocz(size, TAG_SERVER)
//#define SV_CopyString(s)        Z_TagCopyString(s, TAG_SERVER)
//#define SV_LoadFile(path, buf)  FS_LoadFileEx(path, buf, 0, TAG_SERVER)
//#define SV_FreeFile(buf)        Z_Free(buf)
/**
*   @brief  Wrapper functions for TAG_SERVER memory allocation.
*   @note   Does not initialize memory to 0 after allocating.
**/
static inline void *SV_Malloc( const size_t size ) {
    return Z_TagMalloc( size, TAG_SERVER );
}
/**
*   @brief  Wrapper functions for TAG_SERVER memory allocation.
*   @note   Initializes memory to 0 after allocating.
**/
static inline void *SV_Mallocz( const size_t size ) {
    return Z_TagMallocz( size, TAG_SERVER );
}
/**
*   @brief  Wrapper functions for TAG_SERVER allocated string buffer copying.
**/
static inline char *SV_CopyString( const char *str ) {
    return Z_TagCopyString( str, TAG_SERVER );
}
/**
*   @brief  Wrapper functions for TAG_SERVER allocated file into buffer loading.
**/
static const int32_t SV_LoadFile( const char *path, void **buffer ) {
    return FS_LoadFileEx( path, buffer, 0, TAG_SERVER );
}
/**
*   @brief  Wrapper functions for TAG_SERVER file buffer memory freeing(Though it'll free any tag memory.)
**/
static inline void SV_FreeFile( void *ptr ) {
    Z_Free( ptr );
}



/**
*   For developers, to print debug messages. Debug level is set by the cvar sv_debug.
*   In Release builds this will do nothing.
**/
#if USE_DEBUG
#define SV_DPrintf(level,...) \
    if (sv_debug && sv_debug->integer > level) { \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__); \
    }
#else
    #define SV_DPrintf(...)
#endif



/**
*
*
*   Various Server Constants.
*
*
**/
// Server FPS constant.
static constexpr double SV_FRAMERATE = BASE_FRAMERATE;
// Server Frame Time constant in seconds.
static constexpr double SV_FRAMETIME = BASE_FRAMETIME;

//! Maximum number of entities.
static constexpr int32_t SV_BASELINES_SHIFT     = ( 6 );
//! Number of baselines per chunk.
static constexpr int32_t SV_BASELINES_PER_CHUNK = ( 1 << SV_BASELINES_SHIFT ); // 64
//! Mask for baselines.
static constexpr int32_t SV_BASELINES_MASK      = ( SV_BASELINES_PER_CHUNK - 1 ); // 63
//! Number of chunks.
static constexpr int32_t SV_BASELINES_CHUNKS    = ( MAX_EDICTS >> SV_BASELINES_SHIFT ); // 128

/**
*   Game features this server supports.
**/
static constexpr int32_t SV_FEATURES = ( GMF_CLIENTNUM | GMF_PROPERINUSE |
                                        GMF_WANT_ALL_DISCONNECTS |
                                        GMF_EXTRA_USERINFO | GMF_IPV6_ADDRESS_AWARE );


/**
*   Test for whether server is paued or not, this is only used for client/server combined builds.
**/
#if USE_CLIENT
    #define SV_PAUSED (sv_paused->integer != 0)
#else
    #define SV_PAUSED 0
#endif



/**
*   @brief  Server Info CVar Set:
**/
static inline cvar_t *SV_InfoSet( const char *var, const char *value ) {
    return Cvar_FullSet( var, value, CVAR_SERVERINFO | CVAR_ROM, FROM_CODE );
}

/**
*   @return Returns a pointer to the edict matching the number.
**/
static inline sv_edict_t *EDICT_FOR_NUMBER( const int32_t number ) {
    //#define EDICT_FOR_NUMBER(n) ((sv_edict_t *)((byte *)ge->edicts + ge->edict_size*(n)))
    //return ( (sv_edict_t *)( (byte *)ge->edictPool.edicts + ge->edictPool.edict_size * ( number ) ) );
    return static_cast<sv_edict_t *>( ge->edictPool->EdictForNumber( number ) );
}
/**
*   @return Returns the number of the pointer entity.
**/
static inline const int32_t NUMBER_OF_EDICT( const sv_edict_t *ent ) {
    //#define EDICT_NUM(e) ((int)(((byte *)(e) - (byte *)ge->edicts) / ge->edict_size))
    //return ( (int32_t)( ( (byte *)(ent)-(byte *)ge->edictPool.edicts ) / ge->edictPool.edict_size ) );
	return ge->edictPool->NumberForEdict( ent );
}



/**
*
*
*
*   Server Internal Structures.
*
*
*
**/
/**
*   @brief  Stores a client's frame state for the server. 
*           Including, areabits, sentTime, clientNumber, numEntities, playerState.
**/
typedef struct {
    //!
    int64_t		number;
    //! Number of total entities in this frame.
	int32_t		num_entities;
    //! Entry index into the first entity_state_t for this client frame.
    uint32_t	first_entity;
    //! Packed player_state_t.
    player_packed_t ps;
    //! Client Number.
    int32_t		clientNum;

    //
    //! Portalarea Visibility Bits. 
    //! Area bytes.
	int32_t		areabytes;
    //! Area bytes &bits.
    byte        areabits[ MAX_MAP_AREA_BYTES ];

    //
    //! For ping calculations:
	//! Time the frame was sent.
    uint64_t	sentTime;						
    //! Latency of this frame.
    int64_t		latency;
} sv_client_frame_t;

/**
*   @brief  Server internal entity data.
**/
typedef struct {
	// WID: upgr-solid: Q2RE Approach.
    cm_solid_t	solid32;
} server_entity_t;

/**
*   @brief  Server internal state data.
**/
typedef struct {
    server_state_t  state;      // Precache commands are only valid during load.
    int64_t			spawncount; // Random number generated each server spawn.

    //! Frame number.
    int64_t     framenum;
    //! Residual in miliseconds.
    uint64_t    frameresidual;

    //! Map filename command.
    char        mapcmd[ MAX_QPATH ];          // ie: *intro.cin+base
    //! BSP Map name.
    char        name[ MAX_QPATH ];            // map name, or cinematic name
    //! BSP Collision Model of the world.
    cm_t        cm;

    //! GameState Config Strings.
	configstring_t  configstrings[ MAX_CONFIGSTRINGS ];
    //! GameState server entities.
    server_entity_t entities[ MAX_EDICTS ];
} server_t;

/**
*   @brief  A client's state in regards to the server.
**/
typedef enum {
    cs_free,        //! Can be reused for a new connection.
    cs_zombie,      //! Client has been disconnected, but don't reuse.
                    //! Connection for a couple seconds.
    cs_assigned,    //! Client_t assigned, but no data received from client yet.
    cs_connected,   //! Netchan fully established, but not in game yet.
    cs_primed,      //! Sent serverdata, client is precaching.
    cs_spawned      //! Client is fully in game.
} clstate_t;



//! Maximum message pool size.
static constexpr int32_t MSG_POOLSIZE        = 1024;
//! Ensure message_packet_t is 64 bytes aligned.
static constexpr int32_t MSG_TRESHOLD        = ( 62 - sizeof( list_t ) );   // keep message_packet_t 64 bytes aligned

//! Reliable message.
static constexpr int32_t MSG_RELIABLE        = BIT( 0 );
//! Clear message buffer.
static constexpr int32_t MSG_CLEAR           = BIT( 1 );
//! Enforce compression.
static constexpr int32_t MSG_COMPRESS        = BIT( 2 );
//! Let it automatically determine if it needs compression.
static constexpr int32_t MSG_COMPRESS_AUTO   = BIT( 4 );

//! ZPacket Header.
static constexpr int32_t ZPACKET_HEADER     = 5;
//! Maximum sound packets.
static constexpr int32_t MAX_SOUND_PACKET   = 14;

/**
*   @brief  Server message packet.
**/
typedef struct {
    list_t              entry;
    uint16_t            cursize;    // Zero means sound packet
    union {
        uint8_t         data[MSG_TRESHOLD];
        struct {
            uint16_t    index;
            uint16_t    sendchan;
            uint8_t     flags;
            uint8_t     volume;
            uint8_t     attenuation;
            uint8_t     timeofs;
            float		pos[3];     // saved in case entity is freed
        };
    };
} message_packet_t;

// WID: 40hz:
static constexpr int32_t  RATE_MESSAGES = 40;
//#define RATE_MESSAGES   SV_FRAMERATE

/**
*   @brief
**/
#define FOR_EACH_CLIENT(client) \
    LIST_FOR_EACH(client_t, client, &sv_clientlist, entry)

/**
*   @brief
**/
#define CLIENT_ACTIVE(cl) \
    ((cl)->state == cs_spawned && !(cl)->download && !(cl)->nodata)

/**
*   @brief Ping Calculation utilities.
**/
#define PL_S2C(cl) (cl->frames_sent ? \
    (1.0f - (float)cl->frames_acked / cl->frames_sent) * (float)SV_FRAMETIME : 0.0f)
#define PL_C2S(cl) (cl->netchan.total_received ? \
    ((float)cl->netchan.total_dropped / cl->netchan.total_received) * (float)SV_FRAMETIME: 0.0f)
#define AVG_PING(cl) (cl->avg_ping_count ? \
    cl->avg_ping_time / cl->avg_ping_count : cl->ping)

/**
*   @brief  Used for rate, as in determining ping.
**/
typedef struct {
    uint64_t    time;
	uint64_t	credit;
	uint64_t	credit_cap;
	uint64_t	cost;
} ratelimit_t;

/**
*   @brief  Stores all the data about connected clients, including their number,
*           state, userinfo, etc.
**/
typedef struct client_s {
    list_t          entry;

    // core info
    clstate_t       state;
    sv_edict_t         *edict;     // EDICT_FOR_NUMBER(clientnum+1)
    int             number;     // client slot number

    // client flags
    bool            reconnected: 1;
    bool            nodata: 1;
    bool            has_zlib: 1;
    bool            drop_hack: 1;
#if USE_ICMP
    bool            unreachable: 1;
#endif
    bool            http_download: 1;

    // userinfo
    char            userinfo[MAX_INFO_STRING];  // name, etc
    char            name[MAX_CLIENT_NAME];      // extracted from userinfo, high bits masked
    int             messagelevel;               // for filtering printed messages
	uint64_t		rate;
    ratelimit_t     ratelimit_namechange;       // for suppressing "foo changed name" flood

    // console var probes
    char            *version_string;
    char            reconnect_var[16];
    char            reconnect_val[16];
    int             console_queries;

    // usercmd stuff
	uint64_t		lastmessage;    // svs.realtime when packet was last received
	uint64_t		lastactivity;   // svs.realtime when user activity was last seen
    int64_t			lastframe;      // for delta compression
    usercmd_t       lastcmd;        // for filling in big drops
    int64_t         command_msec;   // every seconds this is reset, if user
                                    // commands exhaust it, assume time cheating
    int64_t         num_moves;      // reset every 10 seconds
    int64_t         moves_per_sec;  // average movement FPS
    int64_t         cmd_msec_used;
    double          timescale;

    int64_t			ping, min_ping, max_ping;
    int64_t			avg_ping_time, avg_ping_count;

    // frame encoding
    sv_client_frame_t  frames[UPDATE_BACKUP];    // updates can be delta'd from here
    uint64_t		frames_sent; //! Number of frames sent. 
    uint64_t		frames_acked; //! Number of frames acknowledged.
    uint64_t        frames_nodelta; //! Number of frames that did not have delta compression.
    int64_t			framenum;
    uint64_t		frameflags;

    // rate dropping
    uint64_t        message_size[RATE_MESSAGES];    // Used to rate drop 'Normal' packets.
    int64_t         suppress_count;                 // Sumber of messages rate suppressed (rate-dropped).
    uint64_t		send_time, send_delta;          // Used to rate drop 'Async' packets.

    // current download
    byte            *download;      // file being downloaded
    int             downloadsize;   // total bytes (can't use EOF because of paks)
    int             downloadcount;  // bytes sent
    char            *downloadname;  // name of the file
    int             downloadcmd;    // svc_(z)download
    bool            downloadpending;

    // protocol stuff
    int             challenge;  // challenge of this user, randomly generated
    int             protocol;   // major version
    int             version;    // minor version

    //pmoveParams_t   pmp;        // spectator speed, etc
    msgEsFlags_t    esFlags;    // entity protocol flags

    // packetized messages
    list_t              msg_free_list;
    list_t              msg_unreliable_list;
    list_t              msg_reliable_list;
    message_packet_t    *msg_pool;
    unsigned            msg_unreliable_bytes;   // total size of unreliable datagram
    unsigned            msg_dynamic_bytes;      // total size of dynamic memory allocated

    // per-client baseline chunks
    entity_packed_t *baselines[SV_BASELINES_CHUNKS];

    // server state pointers (hack for MVD channels implementation)
	configstring_t	*configstrings;
    char            *gamedir, *mapname;
    sv_edict_pool_i *pool;
    cm_t            *cm;
    int             slot;
    int             spawncount;
    int             maxclients;

    // netchan
    netchan_t       netchan;

    // misc
    time_t          connect_time; // time of initial connect
	int             last_valid_cluster;
} client_t;

/**
*   @return Returns the edict for the client entity pool matching the number.
**/
static inline sv_edict_t *EDICT_POOL( client_s *client, const int32_t number ) {
    //#define EDICT_POOL(c, n) ((sv_edict_t *)((byte *)(c)->pool->edicts + (c)->pool->edict_size*(n)))
    //return ( (sv_edict_t *)( (byte *)( client )->pool->edicts + ( client )->pool->edict_size * ( number ) ) );
    return client->pool->EdictForNumber( number );
}

// a client can leave the server in one of four ways:
// dropping properly by quiting or disconnecting
// timing out if no valid messages are received for timeout.value seconds
// getting kicked off by the server operator
// a program error, like an overflowed reliable buffer

//=============================================================================

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
static constexpr int32_t    MAX_CHALLENGES = 1024;

/**
*   @brief
**/
typedef struct {
    netadr_t    adr;
    unsigned    challenge;
    uint64_t    time;
} challenge_t;

/**
*   @brief
**/
typedef struct {
    list_t      entry;
    netadr_t    addr;
    netadr_t    mask;
    unsigned    hits;
    time_t      time;   // time of the last hit
    char        comment[1];
} addrmatch_t;

/**
*   @brief
**/
typedef struct {
    list_t  entry;
    char    string[1];
} stuffcmd_t;

/**
*   @brief
**/
typedef enum {
    FA_IGNORE,
    FA_LOG,
    FA_PRINT,
    FA_STUFF,
    FA_KICK,

    FA_MAX
} filteraction_t;

/**
*   @brief
**/
typedef struct {
    list_t          entry;
    filteraction_t  action;
    char            *comment;
    char            string[1];
} filtercmd_t;

/**
*   @brief
**/
typedef struct {
    list_t          entry;
    filteraction_t  action;
    char            *var;
    char            *match;
    char            *comment;
} cvarban_t;

#define MAX_MASTERS         8       // max recipients for heartbeat packets
#define HEARTBEAT_SECONDS   300

/**
*   @brief
**/
typedef struct {
    netadr_t        adr;
    uint64_t        last_ack;
    time_t          last_resolved;
    char            *name;
} master_t;

/**
*   @brief  Stores the collision model and other actual data map data that
*           is matching the latest valid map command.
**/
typedef struct {
    //! Original map command.
    char            buffer[MAX_QPATH];  // original mapcmd
    //! Parsed map name.
    char            server[MAX_QPATH];  // parsed map name
	//! Parsed spawnpoint for next map continuation(If any).
    char            *spawnpoint;

	//! Server state.
    server_state_t  state;
	//! Whether it is a loadgame command.
    int32_t         loadgame;
	//! Whether we are at the end of a 'unit'.
    bool            endofunit;

    //! Actual collision model data.
    cm_t            cm;
} mapcmd_t;

/**
*   @brief  "Static" server state data that persists between map changes.
**/
typedef struct server_static_s {
    bool        initialized;        // sv_init has completed
    uint64_t    realtime;           // always increasing, no clamping, etc

    client_t    *client_pool;   // [maxclients]

    unsigned        num_entities;   // maxclients*UPDATE_BACKUP*MAX_PACKET_ENTITIES
    unsigned        next_entity;    // next state to use
    entity_packed_t *entities;      // [num_entities]

#if USE_ZLIB
    z_stream        z;  // for compressing messages at once
	byte			*z_buffer;
	size_t          z_buffer_size;
#endif

    uint64_t        last_heartbeat;
    uint64_t		last_timescale_check;

    uint64_t		heartbeat_index;

    ratelimit_t     ratelimit_status;
    ratelimit_t     ratelimit_auth;
    ratelimit_t     ratelimit_rcon;

    challenge_t     challenges[MAX_CHALLENGES]; // to prevent invalid IPs from connecting

    //! Handle to the skeletal model pose cache for skeletal model animation blending.
    qhandle_t serverPoseCache;
} server_static_t;

//=============================================================================

extern master_t     sv_masters[MAX_MASTERS];    // address of the master server

extern list_t       sv_banlist;
extern list_t       sv_blacklist;
extern list_t       sv_cmdlist_connect;
extern list_t       sv_cmdlist_begin;
extern list_t       sv_lrconlist;
extern list_t       sv_filterlist;
extern list_t       sv_cvarbanlist;
extern list_t       sv_infobanlist;
extern list_t       sv_clientlist;  // linked list of non-free clients

extern server_static_t      svs;        // persistant server info
extern server_t             sv;         // local server

//extern pmoveParams_t    sv_pmp;

extern cvar_t       *sv_hostname;
extern cvar_t       *sv_maxclients;
extern cvar_t       *sv_password;
extern cvar_t       *sv_reserved_slots;
//extern cvar_t       *sv_airaccelerate;        // development tool
//extern cvar_t       *sv_qwmod;                // atu QW Physics modificator
extern cvar_t       *sv_enforcetime;
extern cvar_t       *sv_force_reconnect;
extern cvar_t       *sv_iplimit;

#if USE_DEBUG
extern cvar_t       *sv_debug;
extern cvar_t       *sv_pad_packets;
#endif
extern cvar_t       *sv_novis;
extern cvar_t       *sv_lan_force_rate;
extern cvar_t       *sv_calcpings_method;
extern cvar_t       *sv_changemapcmd;
extern cvar_t       *sv_max_download_size;
extern cvar_t       *sv_max_packet_entities;

extern cvar_t       *sv_allow_map;
extern cvar_t       *sv_cinematics;
#if !USE_CLIENT
extern cvar_t       *sv_recycle;
#endif
extern cvar_t       *sv_enhanced_setplayer;

extern cvar_t       *sv_status_limit;
extern cvar_t       *sv_status_show;
extern cvar_t       *sv_auth_limit;
extern cvar_t       *sv_rcon_limit;
extern cvar_t       *sv_uptime;

extern cvar_t       *sv_allow_unconnected_cmds;

extern cvar_t       *g_features;

extern cvar_t       *sv_timeout;
extern cvar_t       *sv_zombietime;
extern cvar_t       *sv_ghostime;

extern client_t     *sv_client;
extern sv_edict_t      *sv_player;

extern bool     sv_pending_autosave;


//===========================================================

typedef enum {RD_NONE, RD_CLIENT, RD_PACKET} redirect_t;


extern    svgame_export_t    *ge;


//
// sv_save.c
//


//============================================================

