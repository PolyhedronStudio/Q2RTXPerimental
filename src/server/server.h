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

#include "shared/shared.h"
#include "shared/util_list.h"
#include "shared/svgame.h"

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

#include "client/client.h"
#include "server/server.h"
#include "system/system.h"

#if USE_ZLIB
#include <zlib.h>
#endif

//=============================================================================

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

#define SV_Malloc(size)         Z_TagMalloc(size, TAG_SERVER)
#define SV_Mallocz(size)        Z_TagMallocz(size, TAG_SERVER)
#define SV_CopyString(s)        Z_TagCopyString(s, TAG_SERVER)
#define SV_LoadFile(path, buf)  FS_LoadFileEx(path, buf, 0, TAG_SERVER)
#define SV_FreeFile(buf)        Z_Free(buf)

#if USE_DEBUG
#define SV_DPrintf(level,...) \
    if (sv_debug && sv_debug->integer > level) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#else
#define SV_DPrintf(...)
#endif

#define SV_BASELINES_SHIFT      6
#define SV_BASELINES_PER_CHUNK  (1 << SV_BASELINES_SHIFT) // 64
#define SV_BASELINES_MASK       (SV_BASELINES_PER_CHUNK - 1) // 63
#define SV_BASELINES_CHUNKS     (MAX_EDICTS >> SV_BASELINES_SHIFT) // 16

#define SV_InfoSet(var, val) \
    Cvar_FullSet(var, val, CVAR_SERVERINFO|CVAR_ROM, FROM_CODE)

#if USE_CLIENT
#define SV_PAUSED (sv_paused->integer != 0)
#else
#define SV_PAUSED 0
#endif

// game features this server supports
#define SV_FEATURES (GMF_CLIENTNUM | GMF_PROPERINUSE | \
                     GMF_WANT_ALL_DISCONNECTS | \
                     GMF_EXTRA_USERINFO | GMF_IPV6_ADDRESS_AWARE )

typedef struct {
    int64_t		number;
	int32_t		num_entities;
    uint32_t	first_entity;
    player_packed_t ps;
    int32_t		clientNum;
	int32_t		areabytes;
    byte        areabits[MAX_MAP_AREA_BYTES];	// portalarea visibility bits
    uint64_t	sentTime;						// for ping calculations
    int64_t		latency;
} client_frame_t;

typedef struct {
	// WID: upgr-solid: Q2RE Approach.
	uint32_t	solid32;
	//int         solid32;
} server_entity_t;

// Server FPS
#define SV_FRAMERATE	BASE_FRAMERATE
#define SV_FRAMETIME	BASE_FRAMETIME

typedef struct {
    server_state_t  state;      // precache commands are only valid during load
    int64_t			spawncount; // random number generated each server spawn

    int64_t     framenum;
    uint64_t    frameresidual;

    char        mapcmd[MAX_QPATH];          // ie: *intro.cin+base

    char        name[MAX_QPATH];            // map name, or cinematic name
    cm_t        cm;

	configstring_t  configstrings[ MAX_CONFIGSTRINGS ];

    server_entity_t entities[MAX_EDICTS];
} server_t;

//! Access all over.
extern svgame_export_t *ge;

//! Returns a pointer to the edict matching the number.
static inline edict_t *EDICT_FOR_NUMBER( const int32_t number ) {
    //#define EDICT_FOR_NUMBER(n) ((edict_t *)((byte *)ge->edicts + ge->edict_size*(n)))
    return ( (edict_t *)( (byte *)ge->edicts + ge->edict_size * ( number ) ) );
}
//! Returns the number of the pointer entity.
static inline const int32_t NUMBER_OF_EDICT( const edict_t *ent ) {
    //#define EDICT_NUM(e) ((int)(((byte *)(e) - (byte *)ge->edicts) / ge->edict_size))
    return ( (int32_t)( ( (byte *)(ent)-(byte *)ge->edicts ) / ge->edict_size ) );
}

//! Maximum total entity leafs.
//#define MAX_TOTAL_ENT_LEAFS        128

// hack for smooth BSP model rotation
//#define Q2PRO_SHORTANGLES(c, e) \
//    ((c)->protocol == PROTOCOL_VERSION_Q2PRO && \
//     (c)->version >= PROTOCOL_VERSION_Q2PRO_SHORT_ANGLES && \
//     sv.state == ss_game && \
//     EDICT_POOL(c, e)->solid == SOLID_BSP)
// WID: Just use proper angles everywhere instead:
//#define Q2PRO_SHORTANGLES(c, e) true

typedef enum {
    cs_free,        // can be reused for a new connection
    cs_zombie,      // client has been disconnected, but don't reuse
                    // connection for a couple seconds
    cs_assigned,    // client_t assigned, but no data received from client yet
    cs_connected,   // netchan fully established, but not in game yet
    cs_primed,      // sent serverdata, client is precaching
    cs_spawned      // client is fully in game
} clstate_t;

#define MSG_POOLSIZE        1024
#define MSG_TRESHOLD        (62 - sizeof(list_t))   // keep message_packet_t 64 bytes aligned

#define MSG_RELIABLE        1
#define MSG_CLEAR           2
#define MSG_COMPRESS        4
#define MSG_COMPRESS_AUTO   8

#define ZPACKET_HEADER      5

#define MAX_SOUND_PACKET   14

typedef struct {
    list_t              entry;
    uint16_t            cursize;    // zero means sound packet
    union {
        uint8_t         data[MSG_TRESHOLD];
        struct {
            uint8_t     flags;
            uint8_t     index;
            uint16_t    sendchan;
            uint8_t     volume;
            uint8_t     attenuation;
            uint8_t     timeofs;
            float		pos[3];     // saved in case entity is freed
        };
    };
} message_packet_t;

// WID: 40hz:
#define RATE_MESSAGES   10
//#define RATE_MESSAGES   SV_FRAMERATE

#define FOR_EACH_CLIENT(client) \
    LIST_FOR_EACH(client_t, client, &sv_clientlist, entry)

#define CLIENT_ACTIVE(cl) \
    ((cl)->state == cs_spawned && !(cl)->download && !(cl)->nodata)

#define PL_S2C(cl) (cl->frames_sent ? \
    (1.0f - (float)cl->frames_acked / cl->frames_sent) * (float)SV_FRAMETIME : 0.0f)
#define PL_C2S(cl) (cl->netchan.total_received ? \
    ((float)cl->netchan.total_dropped / cl->netchan.total_received) * (float)SV_FRAMETIME: 0.0f)
#define AVG_PING(cl) (cl->avg_ping_count ? \
    cl->avg_ping_time / cl->avg_ping_count : cl->ping)

typedef struct {
    uint64_t    time;
	uint64_t	credit;
	uint64_t	credit_cap;
	uint64_t	cost;
} ratelimit_t;

typedef struct {
    struct edict_s  *edicts;
    int         edict_size;
    int         num_edicts;     // current number, <= max_edicts
    int         max_edicts;
} edict_pool_t;

typedef struct client_s {
    list_t          entry;

    // core info
    clstate_t       state;
    edict_t         *edict;     // EDICT_FOR_NUMBER(clientnum+1)
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
    int             command_msec;   // every seconds this is reset, if user
                                    // commands exhaust it, assume time cheating
    int64_t         num_moves;      // reset every 10 seconds
    int64_t         moves_per_sec;  // average movement FPS
    int             cmd_msec_used;
    double          timescale;

    int64_t			ping, min_ping, max_ping;
    int64_t			avg_ping_time, avg_ping_count;

    // frame encoding
    client_frame_t  frames[UPDATE_BACKUP];    // updates can be delta'd from here
    uint64_t		frames_sent, frames_acked, frames_nodelta;
    int64_t			framenum;
    uint64_t		frameflags;

    // rate dropping
    unsigned        message_size[RATE_MESSAGES];    // used to rate drop normal packets
    int64_t         suppress_count;                 // number of messages rate suppressed
    uint64_t		send_time, send_delta;          // used to rate drop async packets

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
    edict_pool_t    *pool;
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

//! Returns the edict for the client entity pool matching the number.
static inline edict_t *EDICT_POOL( client_s *client, const int32_t number ) {
    //#define EDICT_POOL(c, n) ((edict_t *)((byte *)(c)->pool->edicts + (c)->pool->edict_size*(n)))
    return ( (edict_t *)( (byte *)( client )->pool->edicts + ( client )->pool->edict_size * ( number ) ) );
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
#define    MAX_CHALLENGES    1024

typedef struct {
    netadr_t    adr;
    unsigned    challenge;
    unsigned    time;
} challenge_t;

typedef struct {
    list_t      entry;
    netadr_t    addr;
    netadr_t    mask;
    unsigned    hits;
    time_t      time;   // time of the last hit
    char        comment[1];
} addrmatch_t;

typedef struct {
    list_t  entry;
    char    string[1];
} stuffcmd_t;

typedef enum {
    FA_IGNORE,
    FA_LOG,
    FA_PRINT,
    FA_STUFF,
    FA_KICK,

    FA_MAX
} filteraction_t;

typedef struct {
    list_t          entry;
    filteraction_t  action;
    char            *comment;
    char            string[1];
} filtercmd_t;

typedef struct {
    list_t          entry;
    filteraction_t  action;
    char            *var;
    char            *match;
    char            *comment;
} cvarban_t;

#define MAX_MASTERS         8       // max recipients for heartbeat packets
#define HEARTBEAT_SECONDS   300

typedef struct {
    netadr_t        adr;
    uint64_t        last_ack;
    time_t          last_resolved;
    char            *name;
} master_t;

typedef struct {
    char            buffer[MAX_QPATH];  // original mapcmd
    char            server[MAX_QPATH];  // parsed map name
    char            *spawnpoint;
    server_state_t  state;
    int             loadgame;
    bool            endofunit;
    cm_t            cm;
} mapcmd_t;

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

extern cvar_t       *sv_strafejump_hack;

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
extern edict_t      *sv_player;

extern bool     sv_pending_autosave;


//===========================================================

//
// sv_main.c
//
void SV_DropClient(client_t *drop, const char *reason);
void SV_RemoveClient(client_t *client);
void SV_CleanClient(client_t *client);

void SV_InitOperatorCommands(void);

void SV_UserinfoChanged(client_t *cl);

bool SV_RateLimited(ratelimit_t *r);
void SV_RateRecharge(ratelimit_t *r);
void SV_RateInit(ratelimit_t *r, const char *s);

addrmatch_t *SV_MatchAddress(list_t *list, netadr_t *address);

int SV_CountClients(void);

#if USE_ZLIB
voidpf SV_zalloc(voidpf opaque, uInt items, uInt size);
void SV_zfree(voidpf opaque, voidpf address);
#endif

void sv_sec_timeout_changed(cvar_t *self);
void sv_min_timeout_changed(cvar_t *self);

//
// sv_init.c
//
void SV_ClientReset(client_t *client);
void SV_SpawnServer(mapcmd_t *cmd);
bool SV_ParseMapCmd(mapcmd_t *cmd);
//void SV_PreInitGame( void );
void SV_InitGame( void );

//
// sv_send.c
//
typedef enum {RD_NONE, RD_CLIENT, RD_PACKET} redirect_t;
#define SV_OUTPUTBUF_LENGTH     (MAX_PACKETLEN_DEFAULT - 16)

#define SV_ClientRedirect() \
    Com_BeginRedirect(RD_CLIENT, sv_outputbuf, MAX_STRING_CHARS - 1, SV_FlushRedirect)

#define SV_PacketRedirect() \
    Com_BeginRedirect(RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect)

extern char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect(int redirected, char *outputbuf, size_t len);

void SV_SendClientMessages(void);
void SV_SendAsyncPackets(void);

void SV_Multicast(const vec3_t origin, multicast_t to, bool reliable );
void SV_ClientPrintf(client_t *cl, int level, const char *fmt, ...) q_printf(3, 4);
void SV_BroadcastPrintf(int level, const char *fmt, ...) q_printf(2, 3);
void SV_ClientCommand(client_t *cl, const char *fmt, ...) q_printf(2, 3);
void SV_BroadcastCommand(const char *fmt, ...) q_printf(1, 2);
void SV_ClientAddMessage(client_t *client, int flags);
void SV_ShutdownClientSend(client_t *client);
void SV_InitClientSend(client_t *newcl);

//
// sv_user.c
//
void SV_New_f(void);
void SV_Begin_f(void);
void SV_ExecuteClientMessage(client_t *cl);
void SV_CloseDownload(client_t *client);
cvarban_t *SV_CheckInfoBans(const char *info, bool match_only);

//
// sv_ccmds.c
//

void SV_AddMatch_f(list_t *list);
void SV_DelMatch_f(list_t *list);
void SV_ListMatches_f(list_t *list);
client_t *SV_GetPlayer(const char *s, bool partial);
void SV_PrintMiscInfo(void);

//
// sv_ents.c
//

#define ES_INUSE(s) \
    ((s)->modelindex || (s)->effects || (s)->sound || (s)->event)

void SV_BuildClientFrame(client_t *client);
void SV_WriteFrameToClient( client_t *client );

//
// sv_game.c
//
extern    svgame_export_t    *ge;

void SV_InitGameProgs(void);
void SV_ShutdownGameProgs(void);
//void SV_InitEdict(edict_t *e);

void PF_Pmove(pmove_t *pm);

//
// sv_save.c
//
void SV_AutoSaveBegin(mapcmd_t *cmd);
void SV_AutoSaveEnd(void);
void SV_CheckForSavegame(mapcmd_t *cmd);
void SV_CheckForEnhancedSavegames(void);
void SV_RegisterSavegames(void);
int SV_NoSaveGames(void);

//============================================================

//
// high level object sorting to reduce interaction tests
//

void SV_ClearWorld(void);
// called after the world model has been loaded, before linking any entities

void PF_UnlinkEdict(edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void SV_LinkEdict(cm_t *cm, edict_t *ent);
void PF_LinkEdict(edict_t *ent);
// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->v.absmin and ent->v.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid

const int32_t SV_AreaEdicts(const vec3_t mins, const vec3_t maxs, edict_t **list, const int32_t maxcount, const int32_t areatype);
// fills in a table of edict pointers with edicts that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// ??? does this always return the world?

//===================================================================

//
// functions that interact with everything apropriate
//
/**
*	@return	The CONTENTS_* value from the world at the given point.
*			Quake 2 extends this to also check entities, to allow moving liquids
**/
const contents_t SV_PointContents( const vec3_t p );


/**
*	@description	mins and maxs are relative
*					
*					if the entire move stays in a solid volume, trace.allsolid will be set,
*					trace.startsolid will be set, and trace.fraction will be 0
*
*					if the starting point is in a solid, it will be allowed to move out
*					to an open area
*
*					passedict is explicitly excluded from clipping checks (normally NULL)
**/
const trace_t q_gameabi SV_Trace( const vec3_t start, const vec3_t mins,
                           const vec3_t maxs, const vec3_t end,
                           edict_t *passedict, const contents_t contentmask );

/**
*	@brief	Like SV_Trace(), but clip to specified entity only.
*			Can be used to clip to SOLID_TRIGGER by its BSP tree.
**/
const trace_t q_gameabi SV_Clip( edict_t *clip, const vec3_t start, const vec3_t mins,
						  const vec3_t maxs, const vec3_t end, const contents_t contentmask );
#ifdef __cplusplus
// We extern "C"
};
#endif