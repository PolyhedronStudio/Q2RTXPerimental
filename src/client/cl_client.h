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

// client.h -- primary header for client
#include "shared/shared.h"
#include "shared/util_list.h"
#include "shared/clgame.h"

#include "common/bsp.h"
#include "common/cmd.h"
#include "common/cmodel.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/field.h"
#include "common/files.h"
#include "common/math.h"
#include "common/messaging.h"
#include "common/net/chan.h"
#include "common/net/net.h"
#include "common/prompt.h"
#include "common/protocol.h"
#include "common/sizebuf.h"
#include "common/zone.h"

#include "refresh/refresh.h"
#include "server/server.h"
#include "system/system.h"

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

#include "client/client.h"
#include "client/input.h"
#include "client/keys.h"
#include "client/sound/sound.h"
#include "client/ui.h"
#include "client/video.h"

#if USE_ZLIB
#include <zlib.h>
#endif

//=============================================================================
#include "client/client_types.h"
//#define MAX_EXPLOSIONS  32
//
//typedef struct {
//	enum {
//		ex_free,
//		//ex_explosion, Somehow unused. lol. TODO: Probably implement some day? xD
//		ex_misc,
//		ex_flash,
//		ex_mflash,
//		ex_poly,
//		ex_poly2,
//		ex_light,
//		ex_blaster,
//		ex_flare
//	} type;
//
//	entity_t    ent;
//	int         frames;
//	float       light;
//	vec3_t      lightcolor;
//	float       start;
//	int         baseframe;
//	int         frametime; /* in milliseconds */
//} explosion_t;
//
//extern explosion_t  cl_explosions[MAX_EXPLOSIONS];
/**
* 
* The client defined centity_t moved to inc/shared/clgame.h
* If CLGAME_INCLUDE is defined it'll declare the entire whole
* game side version.
* 
* If CLGAME_INCLUDE is NOT defined, it'll declare only the first
* shared memory part that it shares with the client game.
* 
**/
//typedef struct centity_s {
//    entity_state_t    current;
//    entity_state_t    prev;
//
//    ...
//
//} centity_t;
// Will point to the actual client game its client entity array.
extern centity_t *cl_entities;

// Extern it so we got access to it anywhere in the client.
extern clgame_export_t *clge;

static inline centity_t *ENTITY_FOR_NUMBER( const int32_t number ) {
    //#define CENTITY_NUM(n) ((centity_t *)((byte *)cl_entities + clge->entity_size*(n)))
    return ( (centity_t *)( (byte *)cl_entities + clge->entity_size * ( number ) ) );
}
static inline const int32_t NUMBER_OF_ENTITY( const centity_t *cent ) {
    //#define NUM_FOR_CENTITY(e) ((int)(((byte *)(e) - (byte *)cl_entities) / clge->entity_size))
    return ( (int32_t)( ( (byte *)(cent)-(byte *)cl_entities ) / clge->entity_size ) );
}

#define MAX_CLIENTWEAPONMODELS        20        // PGM -- upped from 16 to fit the chainfist vwep

///**
//*   @brief  
//**/
//typedef struct clientinfo_s {
//    char name[MAX_QPATH];
//    qhandle_t skin;
//    qhandle_t icon;
//    char model_name[MAX_QPATH];
//    char skin_name[MAX_QPATH];
//    qhandle_t model;
//    qhandle_t weaponmodel[MAX_CLIENTWEAPONMODELS];
//} clientinfo_t;
//
///**
//*   @brief  Client command history, used for calculating pings.
//**/
//typedef struct {
//    uint64_t sent;    // time sent, for calculating pings
//	uint64_t rcvd;    // time rcvd, for calculating pings
//	uint64_t cmdNumber;    // current cmdNumber for this frame
//} client_usercmd_history_t;
//
///**
//*   @brief  Contains, if valid, snapshots of the player state and the range of
//*           entity_state_t entities that were in the current frame.
//**/
//typedef struct {
//    bool            valid;
//
//    int64_t         number;
//    int64_t         delta;
//
//    byte            areabits[MAX_MAP_AREA_BYTES];
//    int             areabytes;
//
//    player_state_t  ps;
//    int             clientNum;
//
//    int             numEntities;
//    int             firstEntity;
//} server_frame_t;
//
//// locally calculated frame flags for debug display
//#define FF_SERVERDROP   (1<<4)
//#define FF_BADFRAME     (1<<5)
//#define FF_OLDFRAME     (1<<6)
//#define FF_OLDENT       (1<<7)
//#define FF_NODELTA      (1<<8)
//
//// Variable client FPS
//#define CL_FRAMETIME    BASE_FRAMETIME
//#define CL_1_FRAMETIME  BASE_1_FRAMETIME
//
//
///**
//*   @brief  Stores client-side predicted player_state_t information. These are generated by // CL_PredictMovement
//**/
//typedef struct {
//	//! User Command Input for this frame.
//	usercmd_t cmd;
//
//    //! Total accumulated screen blend.
//    vec4_t screen_blend;
//    //! Refdef Flags.
//    int32_t rdflags;
//
//	// for stair up smoothing
//	double step;
//	uint64_t step_time;
//
//	//! Origin, angles and velocity of current frame pmove's results.
//	vec3_t origin;    
//	vec3_t angles;
//	vec3_t velocity;
//
//	//! Margin of error for this frame.
//	vec3_t error;
//} client_predicted_state_t;
//
///**
// *  @brief  The client state structure is cleared at each level load, and is exposed to
// *          the client game module to provide access to media and other client state.
// **/
//typedef struct client_state_s {
//    //! Count for possible time-outs.
//    int64_t     timeoutcount;
//
//    //! Last time of packet transmission.
//    uint64_t	lastTransmitTime;
//    //! Last command number that transmitted.
//	uint64_t	lastTransmitCmdNumber;
//    //! Real last command number that transmitted.
//	uint64_t	lastTransmitCmdNumberReal;
//    //! Immediately send the 'command' packet, or not.
//    bool        sendPacketNow;
//
//	//! The current command its numerical index.
//	uint64_t cmdNumber;
//	//! Client history of command user input index, its time sent, as well as time received.
//    client_usercmd_history_t history[CMD_BACKUP];
//	//! The initiali client frame sequence.
//    int64_t initialSeq;
//
//	//! The actual pmove predicted state history results.
//	client_predicted_state_t predictedStates[CMD_BACKUP];
//	//! The current pmove state to be predicted this frame.
//	client_predicted_state_t predictedState;
//    /////////////////////////////
//    // TODO: Move to predictedState?
//    struct {
//        //! Current viewheight from client Pmove().
//        int8_t  current;
//        //! Viewheight before last change.
//        int8_t  previous;
//        //! Time when a viewheight change was detected.
//        int64_t change_time;
//        //! :ast groundentity reported by pmove.
//    } viewheight;
//    struct {
//        centity_t *entity;
//        //! last groundplane reported by pmove.
//        cplane_t plane;
//    } lastGround;
//    /////////////////////////////
//
//    // Rebuilt each valid frame:
//    centity_t		*solidEntities[MAX_PACKET_ENTITIES];
//    int32_t			numSolidEntities;
//    //! Stores all entity baseline states to use for delta-ing.
//    entity_state_t	baselines[MAX_EDICTS];
//    //! Stores the actual received delta decoded entity states.
//    entity_state_t	entityStates[MAX_PARSE_ENTITIES];
//    int32_t			numEntityStates;
//    //! Entity specific message flags.
//    msgEsFlags_t	esFlags;
//    //! Received server frames, might possibly be invalid.
//    server_frame_t	frames[UPDATE_BACKUP];
//    uint32_t		frameflags;
//
//    //! Last/Currently received from server:
//    server_frame_t	frame;                
//    server_frame_t	oldframe;
//    int64_t			servertime;
//    int64_t			serverdelta;
//
//	// WID: Seems to be related to demo configstring bytes.
//    byte            dcs[CS_BITMAP_BYTES];
//
//    //! The client maintains its own idea of view angles, which are
//    //! sent to the server each frame.  It is cleared to 0 upon entering each level.
//    //! the server sends a delta each frame which is added to the locally
//    //! tracked view angles to account for standing on rotating objects,
//    //! and teleport direction changes.
//    vec3_t      viewangles;
//
//    //! Interpolated movement vector used for local prediction,
//    //! never sent to server, rebuilt each client frame.
//    vec3_t      localmove;
//
//    //! Accumulated mouse forward/side movement, added to both
//    //! localmove and pending cmd, cleared each time cmd is finalized.
//    vec2_t      mousemove;
//
//	//! The delta of the current frames move angles.
//    vec3_t      delta_angles;
//
//	//! This is the 'moment-in-time' value that the client is rendering at.
//	//! Always <= cl.servertime
//    int64_t     time;           
//	//! The current "lerp" -fraction between 'oldframe' and 'frame'
//    double      lerpfrac;       
//
//	//! Refresh frame settings.
//    refdef_t    refdef;
//
//	//! Interpolated
//    float       fov_x;      
//    float       fov_y;      // derived from fov_x assuming 4/3 aspect ratio.
//
//	//! Set when refdef.angles is set.
//    vec3_t      v_forward, v_right, v_up;   
//
//	//! Whether in thirdperson view or not.
//    bool        thirdPersonView;
//
//    //! Predicted values, used for smooth player entity movement in thirdperson view.
//    vec3_t      playerEntityOrigin;
//    vec3_t      playerEntityAngles;
//
//    //!
//    //! Transient data from server.
//    //!
//    //! General 2D overlay cmd script.
//    char		layout[MAX_NET_STRING];     
//    int32_t		inventory[MAX_ITEMS];
//
//    //!
//    //! Server state information.
//    //!
//    //! ss_* constants
//    int32_t		serverstate;    
//    //! Server identification for prespawns.
//    int32_t		servercount;
//    //! Directory name of the current game(dir) that is in-use.
//    char		gamedir[MAX_QPATH];
//    //! Never changed during gameplay, set by serverdata packet.
//    int32_t		clientNum;
//    //! Maximum number of clients that the current connected game accepts.
//    int32_t		maxclients;
//
//	// Received pmove configuration.
//    pmoveParams_t pmp;
//
//	configstring_t baseconfigstrings[MAX_CONFIGSTRINGS];
//	configstring_t configstrings[MAX_CONFIGSTRINGS];
//    char		mapname[MAX_QPATH]; // short format - q2dm1, etc
//
//#if USE_AUTOREPLY
//    uint64_t	reply_time;
//    uint64_t	reply_delta;
//#endif
//
//    //
//    // locally derived information from server state
//    //
//    bsp_t *bsp;
//
//    qhandle_t model_draw[MAX_MODELS];
//    mmodel_t *model_clip[MAX_MODELS];
//
//    qhandle_t sound_precache[MAX_SOUNDS];
//    qhandle_t image_precache[MAX_IMAGES];
//
//    clientinfo_t clientinfo[MAX_CLIENTS];
//    clientinfo_t baseclientinfo;
//
//    char	weaponModels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
//    int32_t	numWeaponModels;
//
//	// WID: 40hz - For proper frame lerping for 10hz/custom rate models.
//	float	sv_frametime_inv;
//	int64_t	sv_frametime;
//	int64_t	sv_framediv;
//
//	// Data for view weapon
//	struct {
//		int64_t frame, last_frame;
//		int64_t server_time;
//
//		//qhandle_t muzzle_model;
//		//int32_t muzzle_time;
//		//float muzzle_roll, muzzle_scale;
//		//int32_t muzzle_skin;
//		//vec3_t muzzle_offset;
//	} weapon;
//// WID: 40hz
//} client_state_t;

extern    client_state_t    cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

// resend delay for challenge/connect packets
#define CONNECT_DELAY       3000u

#define CONNECT_INSTANT     CONNECT_DELAY
#define CONNECT_FAST        (CONNECT_DELAY - 1000u)

#define FOR_EACH_DLQ(q) \
    LIST_FOR_EACH(dlqueue_t, q, &cls.download.queue, entry)
#define FOR_EACH_DLQ_SAFE(q, n) \
    LIST_FOR_EACH_SAFE(dlqueue_t, q, n, &cls.download.queue, entry)

typedef enum {
    // generic types
    DL_OTHER,
    DL_MAP,
    DL_MODEL,
#if USE_CURL
    // special types
    DL_LIST,
    DL_PAK
#endif
} dltype_t;

typedef enum {
    DL_FREE,
    DL_PENDING,
    DL_RUNNING,
    DL_DONE
} dlstate_t;

typedef struct {
    list_t      entry;
    dltype_t    type;
    dlstate_t   state;
    char        path[1];
} dlqueue_t;

typedef struct client_static_s {
    connstate_t state;
    keydest_t   key_dest;

    active_t    active;

    bool        ref_initialized;
    ref_type_t  ref_type;
    uint64_t    disable_screen;

    int         userinfo_modified;
    cvar_t      *userinfo_updates[MAX_PACKET_USERINFOS];
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

    int64_t     framecount;
    uint64_t    realtime;           // always increasing, no clamping, etc
    double      frametime;          // seconds since last frame

// preformance measurement
#define C_FPS   cls.measure.fps[0]
#define R_FPS   cls.measure.fps[1]
#define C_MPS   cls.measure.fps[2]
#define C_PPS   cls.measure.fps[3]
#define C_FRAMES    cls.measure.frames[0]
#define R_FRAMES    cls.measure.frames[1]
#define M_FRAMES    cls.measure.frames[2]
#define P_FRAMES    cls.measure.frames[3]
    struct {
        uint64_t time;
        int64_t frames[4];
		int64_t fps[4];
		int64_t ping;
    } measure;

// connection information
    netadr_t    serverAddress;
    char        servername[MAX_OSPATH]; // name of server from original connect
    uint64_t	connect_time;           // for connection retransmits
    int         connect_count;
    bool        passive;

#if USE_ZLIB
    z_stream    z;
#endif

    int         quakePort;          // a 16 bit value that allows quake servers
                                    // to work around address translating routers
    netchan_t   netchan;
    int         serverProtocol;     // in case we are doing some kind of version hack
    int         protocolVersion;    // minor version

    int         challenge;          // from the server to use for connecting

#if USE_ICMP
    bool        errorReceived;      // got an ICMP error from server
#endif

#define RECENT_ADDR 4
#define RECENT_MASK (RECENT_ADDR - 1)

    netadr_t    recent_addr[RECENT_ADDR];
    int         recent_head;

    struct {
        list_t      queue;              // queue of paths we need
        int         pending;            // number of non-finished entries in queue
        dlqueue_t   *current;           // path being downloaded
        int         percent;            // how much downloaded
        int64_t     position;           // how much downloaded (in bytes)
        qhandle_t   file;               // UDP file transfer from server
        char        temp[MAX_QPATH + 4];// account 4 bytes for .tmp suffix
#if USE_ZLIB
        z_stream    z;                  // UDP download zlib stream
#endif
        string_entry_t  *ignores;       // list of ignored paths
    } download;

// demo recording info must be here, so it isn't cleared on level change
    struct {
        qhandle_t   playback;
        qhandle_t   recording;
        uint64_t    time_start;
		uint64_t	time_frames;
		uint64_t	last_server_frame;  // number of server frame the last svc_frame was written
		uint64_t	frames_written;     // number of frames written to demo file
		uint64_t	frames_dropped;     // number of svc_frames that didn't fit
		uint64_t	others_dropped;     // number of misc svc_* messages that didn't fit
		uint64_t	frames_read;        // number of frames read from demo file
		uint64_t	last_snapshot;      // number of demo frame the last snapshot was saved
        int64_t     file_size;
        int64_t     file_offset;
        float       file_progress;
        sizebuf_t   buffer;
        list_t      snapshots;
        bool        paused;
        bool        seeking;
        bool        eof;
    } demo;
    struct {
        // Number of timedemo runs to perform
        int64_t     runs_total;
        // Current run
        int64_t     run_current;
        // Results of timedemo runs
        uint64_t *results;
    } timedemo;
} client_static_t;

extern client_static_t    cls;

extern cmdbuf_t    cl_cmdbuf;
extern char        cl_cmdbuf_text[MAX_STRING_CHARS];

//=============================================================================

//
// cvars
//
//extern cvar_t    *cl_gunalpha;
extern cvar_t    *cl_gunscale;
//extern cvar_t    *cl_gun_x;
//extern cvar_t    *cl_gun_y;
//extern cvar_t    *cl_gun_z;
extern cvar_t    *cl_predict;
extern cvar_t    *cl_noskins;
//extern cvar_t    *cl_kickangles;
extern cvar_t    *cl_rollhack;
extern cvar_t    *cl_noglow;
extern cvar_t    *cl_nolerp;

#if USE_DEBUG
#define SHOWNET(level, ...) \
    if (cl_shownet->integer > level) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#define SHOWCLAMP(level, ...) \
    if (cl_showclamp->integer > level) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#define SHOWMISS(...) \
    if (cl_showmiss->integer) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
extern cvar_t    *cl_shownet;
extern cvar_t    *cl_showmiss;
extern cvar_t    *cl_showclamp;
#else
#define SHOWNET(...)
#define SHOWCLAMP(...)
#define SHOWMISS(...)
#endif

extern cvar_t    *cl_vwep;

extern cvar_t    *cl_disable_explosions;
extern cvar_t    *cl_explosion_sprites;
extern cvar_t    *cl_explosion_frametime;
extern cvar_t    *cl_dlight_hacks;

extern cvar_t    *cl_chat_notify;
extern cvar_t    *cl_chat_sound;
extern cvar_t    *cl_chat_filter;

extern cvar_t    *cl_disconnectcmd;
extern cvar_t    *cl_changemapcmd;
extern cvar_t    *cl_beginmapcmd;

// Moved to CLGame:
//extern cvar_t    *cl_gibs;

extern cvar_t    *cl_player_model;
// Moved to CLGame:
//extern cvar_t    *cl_thirdperson_angle;
//extern cvar_t    *cl_thirdperson_range;

extern cvar_t    *cl_async;

//
// userinfo
//
extern cvar_t    *info_password;
extern cvar_t    *info_spectator;
extern cvar_t    *info_name;
extern cvar_t    *info_skin;
extern cvar_t	 *info_rate; // WID: C++20: Linkage
extern cvar_t    *info_fov;
extern cvar_t    *info_msg;
extern cvar_t    *info_hand;
extern cvar_t    *info_gender;
extern cvar_t    *info_uf;

//
// models.c
//
extern cvar_t    *cl_testmodel;
extern cvar_t    *cl_testfps;
extern cvar_t    *cl_testalpha;
extern qhandle_t  cl_testmodel_handle;
extern vec3_t     cl_testmodel_position;

//=============================================================================

//
// clgame.cpp
// 
extern clgame_export_t *clge;

void CL_GM_LoadProgs( );
void CL_GM_Init();
void CL_GM_PreInit();
void CL_GM_Shutdown( );

//
// main.c
//

void CL_Init(void);
void CL_Quit_f(void);
void CL_Disconnect(error_type_t type);
void CL_Begin(void);
void CL_CheckForResend(void);
void CL_ClearState(void);
void CL_RestartFilesystem(bool total);
void CL_RestartRefresh(bool total);
void CL_ClientCommand(const char *string);
void CL_SendRcon(const netadr_t *adr, const char *pass, const char *cmd);
const char *CL_Server_g(const char *partial, int argnum, int state);
void CL_CheckForPause(void);
void CL_UpdateFrameTimes(void);
bool CL_CheckForIgnore(const char *s);
void CL_WriteConfig(void);

void cl_timeout_changed(cvar_t *self);

//
// precache.c
//
void CL_ParsePlayerSkin(char *name, char *model, char *skin, const char *s);
void CL_LoadClientinfo(clientinfo_t *ci, const char *s);
void CL_LoadState(load_state_t state);
void CL_RegisterSounds(void);
void CL_RegisterBspModels(void);
void CL_RegisterVWepModels(void);
/**
*   @brief  Called before entering a new level, or after changing dlls
**/
void CL_PrepRefresh(void);
/**
*   @brief  
**/
void CL_UpdateConfigstring( const int32_t index);


//
// download.c
//
int CL_QueueDownload(const char *path, dltype_t type);
bool CL_IgnoreDownload(const char *path);
void CL_FinishDownload(dlqueue_t *q);
void CL_CleanupDownloads(void);
void CL_LoadDownloadIgnores(void);
void CL_HandleDownload(byte *data, int size, int percent, int decompressed_size);
bool CL_CheckDownloadExtension(const char *ext);
void CL_StartNextDownload(void);
void CL_RequestNextDownload(void);
void CL_ResetPrecacheCheck(void);
void CL_InitDownloads(void);


//
// input.c
//
void IN_Init(void);
void IN_Shutdown(void);
void IN_Frame(void);
void IN_Activate(void);
/**
*   @brief  Register a button as being 'held down'.
**/
void CL_KeyDown( keybutton_t *b );
/**
*   @brief  Register a button as being 'released'.
**/
void CL_KeyUp( keybutton_t *b );
/**
*   @brief  Clear out a key's down state and msec, but maintain track of whether it is 'held'.
**/
void CL_KeyClear( keybutton_t *b );
/**
*   @brief  Returns the fraction of the command frame's interval for which the key was 'down'.
**/
const double CL_KeyState( keybutton_t *key );

void CL_RegisterInput(void);
void CL_UpdateCmd(int msec);
void CL_FinalizeCmd(void);
void CL_SendCmd(void);


//
// parse.c
//
void CL_ParseServerMessage(void);
void CL_SeekDemoMessage(void);


//
// entities.c
//
/**
*   @brief  Called after finished parsing the frame data. All entity states and the
*           player state will be updated, and checked for 'snapping to' their new state,
*           or to smoothly lerp into it. It'll check for any prediction errors afterwards
*           also and calculate its correction value.
*
*           Will switch the clientstatic state to 'ca_active' if it is the first
*           parsed valid frame and the client is done precaching all data.
**/
void CL_DeltaFrame( void );
/**
*   @brief  Prepares the current frame's view scene for the refdef by
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void CL_PrepareViewEntities( void );
/**
*   @brief  Sets cl.refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void CL_CalculateViewValues( void );
/**
*   @brief  Sets up a firstperson or thirdperson view. Depending on what is desired.
**/
//void CL_FinishViewValues( void );


#if USE_DEBUG
void CL_CheckEntityPresent( const int32_t entnum, const char *what);
#endif

// the sound code makes callbacks to the client for entitiy position
// information, so entities can be dynamically re-spatialized
void CL_GetEntitySoundOrigin( const int32_t ent, vec3_t org);


//
// view.c
//
void V_Init(void);
void V_Shutdown(void);
void V_RenderView(void);
void V_AddEntity(entity_t *ent);
void V_AddParticle(particle_t *p);
void V_AddLight(const vec3_t org, float intensity, float r, float g, float b);
void V_AddSphereLight(const vec3_t org, float intensity, float r, float g, float b, float radius);
void V_AddSpotLight(const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle);
void V_AddSpotLightTexEmission( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex );
void V_AddLightStyle(int style, float value);


//
// tent.c
//
//void CL_PrecacheClientSounds(void);
//void CL_PrecacheClientModels(void);
//void CL_ParseTEnt(void);
//void CL_AddTEnts(void);
//void CL_ClearTEnts(void);
//void CL_InitTEnts(void);


//
// predict.c
//
/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void CL_AdjustViewHeight( const int32_t viewHeight );
/**
*   @brief  Sets the predicted view angles.
**/
void CL_PredictAngles( void );
/**
*   @brief  Performs player movement over the registered 'move command frames' and stores the final outcome
*           into the cl.predictedState struct.
**/
void CL_PredictMovement(void);
/**
*   @brief  
**/
void CL_CheckPredictionError(void);


//
// effects.c
//
void CL_BigTeleportParticles(const vec3_t org);
void CL_RocketTrail(const vec3_t start, const vec3_t end, centity_t *old);
void CL_DiminishingTrail(const vec3_t start, const vec3_t end, centity_t *old, int flags);
void CL_FlyEffect(centity_t *ent, const vec3_t origin);
void CL_BfgParticles(entity_t *ent);
void CL_ItemRespawnParticles(const vec3_t org);
//void CL_InitEffects(void);
//void CL_ClearEffects(void);
void CL_BlasterParticles(const vec3_t org, const vec3_t dir);
void CL_ExplosionParticles(const vec3_t org);
void CL_BFGExplosionParticles(const vec3_t org);
void CL_BlasterTrail(const vec3_t start, const vec3_t end);
void CL_OldRailTrail(void);
void CL_BubbleTrail(const vec3_t start, const vec3_t end);
void CL_FlagTrail(const vec3_t start, const vec3_t end, int color);
//void CL_MuzzleFlash(void);
//void CL_MuzzleFlash2(void);
void CL_TeleporterParticles(const vec3_t org);
void CL_TeleportParticles(const vec3_t org);
void CL_ParticleEffect(const vec3_t org, const vec3_t dir, int color, int count);
void CL_ParticleEffectWaterSplash(const vec3_t org, const vec3_t dir, int color, int count);
void CL_BloodParticleEffect(const vec3_t org, const vec3_t dir, int color, int count);
void CL_ParticleEffect2(const vec3_t org, const vec3_t dir, int color, int count);
cparticle_t *CL_AllocParticle(void);
void CL_RunParticles(void);
void CL_AddParticles(void);
//cdlight_t *CL_AllocDlight(int key);
//void CL_AddDLights(void);
//void CL_SetLightStyle(int index, const char *s);
//void CL_AddLightStyles(void);

//
// newfx.c
//

void CL_BlasterParticles2(const vec3_t org, const vec3_t dir, unsigned int color);
void CL_BlasterTrail2(const vec3_t start, const vec3_t end);
void CL_DebugTrail(const vec3_t start, const vec3_t end);
void CL_Flashlight(int ent, const vec3_t pos);
void CL_ForceWall(const vec3_t start, const vec3_t end, int color);
void CL_BubbleTrail2(const vec3_t start, const vec3_t end, int dist);
void CL_Heatbeam(const vec3_t start, const vec3_t end);
void CL_ParticleSteamEffect(const vec3_t org, const vec3_t dir, int color, int count, int magnitude);
void CL_TrackerTrail(const vec3_t start, const vec3_t end, int particleColor);
void CL_TagTrail(const vec3_t start, const vec3_t end, int color);
void CL_ColorFlash(const vec3_t pos, int ent, int intensity, float r, float g, float b);
void CL_Tracker_Shell(const vec3_t origin);
void CL_MonsterPlasma_Shell(const vec3_t origin);
void CL_ColorExplosionParticles(const vec3_t org, int color, int run);
void CL_ParticleSmokeEffect(const vec3_t org, const vec3_t dir, int color, int count, int magnitude);
void CL_Widowbeamout(cl_sustain_t *self);
void CL_Nukeblast(cl_sustain_t *self);
void CL_WidowSplash(void);
void CL_IonripperTrail(const vec3_t start, const vec3_t end);
void CL_TrapParticles(centity_t *ent, const vec3_t origin);
void CL_ParticleEffect3(const vec3_t org, const vec3_t dir, int color, int count);
void CL_ParticleSteamEffect2(cl_sustain_t *self);


//
// demo.c
//
void CL_InitDemos(void);
void CL_CleanupDemos(void);
void CL_DemoFrame(int msec);
bool CL_WriteDemoMessage(sizebuf_t *buf);
void CL_EmitDemoFrame(void);
void CL_EmitDemoSnapshot(void);
void CL_FirstDemoFrame(void);
void CL_Stop_f(void);
demoInfo_t *CL_GetDemoInfo(const char *path, demoInfo_t *info);


//
// locs.c
//
void LOC_Init(void);
void LOC_LoadLocations(void);
void LOC_FreeLocations(void);
void LOC_UpdateCvars(void);
void LOC_AddLocationsToScene(void);


//
// console.c
//
void Con_Init(void);
void Con_PostInit(void);
void Con_Shutdown(void);
void Con_DrawConsole(void);
void Con_RunConsole(void);
void Con_Print(const char *txt);
void Con_ClearNotify_f(void);
void Con_ToggleConsole_f(void);
void Con_ClearTyping(void);
void Con_Close(bool force);
void Con_Popup(bool force);
void Con_SkipNotify(bool skip);
void Con_RegisterMedia(void);
void Con_CheckResize(void);

void Key_Console(int key);
void Key_Message(int key);
void Char_Console(int key);
void Char_Message(int key);


//
// refresh.c
//
void    CL_InitRefresh(void);
void    CL_ShutdownRefresh(void);
void    CL_RunRefresh(void);


//
// screen.c
//
extern vrect_t      scr_vrect;        // position of render window

void    SCR_Init(void);
void    SCR_Shutdown(void);
void    SCR_UpdateScreen(void);
void    SCR_SizeUp(void);
void    SCR_SizeDown(void);
void    SCR_CenterPrint(const char *str);
void    SCR_BeginLoadingPlaque(void);
void    SCR_EndLoadingPlaque(void);
void    SCR_TouchPics(void);
void    SCR_RegisterMedia(void);
void    SCR_ModeChanged(void);
void    SCR_LagSample(void);
void    SCR_LagClear(void);
void    SCR_SetCrosshairColor(void);
qhandle_t SCR_GetFont(void);
void    SCR_SetHudAlpha(float alpha);

float   SCR_FadeAlpha(unsigned startTime, unsigned visTime, unsigned fadeTime);
int     SCR_DrawStringEx(int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font);
void    SCR_DrawStringMulti(int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font);

void    SCR_ClearChatHUD_f(void);
void    SCR_AddToChatHUD(const char *text);
/**
*   @return A lowercase string matching the textual name of the color for colorIndex.
**/
const char *SCR_GetColorName( const color_index_t colorIndex );

//
// cin.c
//
void    SCR_StopCinematic(void);
void    SCR_FinishCinematic(void);
void    SCR_RunCinematic(void);
void    SCR_DrawCinematic(void);
void    SCR_ReloadCinematic(void);
void    SCR_PlayCinematic(const char *name);

//
// ascii.c
//
void CL_InitAscii(void);


//
// http.c
//
#if USE_CURL
void HTTP_Init(void);
void HTTP_Shutdown(void);
void HTTP_SetServer(const char *url);
int HTTP_QueueDownload(const char *path, dltype_t type);
void HTTP_RunDownloads(void);
void HTTP_CleanupDownloads(void);
#else
#define HTTP_Init()                     (void)0
#define HTTP_Shutdown()                 (void)0
#define HTTP_SetServer(url)             (void)0
#define HTTP_QueueDownload(path, type)  Q_ERR(ENOSYS)
#define HTTP_RunDownloads()             (void)0
#define HTTP_CleanupDownloads()         (void)0
#endif

//
// crc.c
//
byte COM_BlockSequenceCRCByte(byte *base, size_t length, int sequence);

//
// effects.c
//
void FX_Init(void);

//
// world.cpp
// 
/**
*   @brief  Performs a 'Clipping' trace against the world, and all the active in-frame solidEntities.
**/
const trace_t q_gameabi CL_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const centity_t *passEntity, const contents_t contentmask );
/**
*   @brief  Will perform a clipping trace to the specified entity.
*           If clipEntity == nullptr, it'll perform a clipping trace against the World.
**/
const trace_t q_gameabi CL_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const centity_t *clipEntity, const contents_t contentmask );
/**
*   @return The type of 'contents' at the given point.
**/
const contents_t q_gameabi CL_PointContents( const vec3_t point );

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif