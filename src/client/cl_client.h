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
#include "common/collisionmodel.h"
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
//! Extern client_state_t so we can access it anyhwere.
extern client_state_t cl;

/**
*   @return A pointer to the entity which matches by array index number.
**/
static inline centity_t *ENTITY_FOR_NUMBER( const int32_t number ) {
    //#define CENTITY_NUM(n) ((centity_t *)((byte *)cl_entities + clge->entity_size*(n)))
    return ( (centity_t *)( (byte *)cl_entities + clge->entity_size * ( number ) ) );
}
/**
*   @return The array index number of the entity which is pointed at.
**/
static inline const int32_t NUMBER_OF_ENTITY( const centity_t *cent ) {
    //#define NUM_FOR_CENTITY(e) ((int)(((byte *)(e) - (byte *)cl_entities) / clge->entity_size))
    return ( (int32_t)( ((byte *)(cent) - (byte *)cl_entities) / clge->entity_size ) );
}



/**
*
*
*   Client Static:
*
*
**/

//! Resend delay for challenge/connect packets
#define CONNECT_DELAY       3000u

#define CONNECT_INSTANT     CONNECT_DELAY
#define CONNECT_FAST        (CONNECT_DELAY - 1000u)

/**
*   @brief
**/
#define FOR_EACH_DLQ(q) \
    LIST_FOR_EACH(dlqueue_t, q, &cls.download.queue, entry)
#define FOR_EACH_DLQ_SAFE(q, n) \
    LIST_FOR_EACH_SAFE(dlqueue_t, q, n, &cls.download.queue, entry)

/**
*   @brief
**/
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
/**
*   @brief
**/
typedef enum {
    DL_FREE,
    DL_PENDING,
    DL_RUNNING,
    DL_DONE
} dlstate_t;
/**
*   @brief
**/
typedef struct {
    list_t      entry;
    dltype_t    type;
    dlstate_t   state;
    char        path[1];
} dlqueue_t;

/**
*   @brief  The client_static_t structure is persistent for the execution of the game.
*           It is only cleared when Cl_Init is called. Some parts of it are accessable
*           to the client game module by specific API callbacks.
**/
typedef struct client_static_s {
    /**
    *
    *   Client Application State:
    *
    **/
    //! Actual connective state. (This includes disconnected, being dropped to console.)
    connstate_t state;
    //! Destination 'layer' for where key events end up being handled in.
    keydest_t   key_dest;

    //! State of the application. ( Minimized, restored, activated. )
    active_t    active;

    //! Whether the renderer is initialized.
    qboolean    ref_initialized;
    //! OpenGL or VKPT.
    ref_type_t  ref_type;
    //! If the screen is disabled (loading plaque is up), do nothing at all
    uint64_t    disable_screen;


    /**
    *
    *   User Info
    *
    **/
    //! Whether the userinfo was modified or not.
    //! This is set each time a CVAR_USERINFO variable is changed, so that the client knows to send it to the server
    int         userinfo_modified;
    //! Pointers to userinfo cvars that need updating.
    cvar_t      *userinfo_updates[MAX_PACKET_USERINFOS];


    /**
    *
    *   Client Timing:
    *
    **/
    //! Actual number of frames we've ran so far.
    int64_t     framecount;
    //! Time since application boot, always increasing, no clamping, etc.
    uint64_t    realtime;
    //! Seconds delta since last frame.
    double      realdelta;
    //! Seconds since last frame.
    double      frametime;


    /**
    *
    *   Performance measurement
    *
    **/
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


    /**
    *
    *   Connection Information:
    *
    **/
    netadr_t    serverAddress;
    char        servername[MAX_OSPATH]; // name of server from original connect
    uint64_t	connect_time;           // for connection retransmits
    int         connect_count;
    qboolean    passive;

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
    qboolean    errorReceived;      // got an ICMP error from server
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


    /**
    *
    *   Demo ( -Recording)/Time Demo:
    *
    **/
    //! Demo recording info must be here, so it isn't cleared on level change
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
        qboolean    paused;
        qboolean    seeking;
        qboolean    eof;
    } demo;
    //! Time Demo:
    struct {
        // Number of timedemo runs to perform
        int64_t     runs_total;
        // Current run
        int64_t     run_current;
        // Results of timedemo runs
        uint64_t *results;
    } timedemo;


    /**
    *
    *   Other:
    *
    **/
    //! Opaque handle to the reserved Bone Pose Cache memory manager context.
    qhandle_t clientPoseCache;
} client_static_t;

//! Extern for access anywhere.
extern client_static_t    cls;

//! Extern for access anywhere.
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
//extern cvar_t    *cl_noskins;
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

//extern cvar_t    *cl_vwep;

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
//extern cvar_t    *info_gender;
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
void CL_GM_PreShutdown();



//
// main.cpp
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
qboolean CL_CheckForIgnore(const char *s);
void CL_WriteConfig(void);

void cl_timeout_changed(cvar_t *self);

//
// precache.cpp
//
void CL_ParsePlayerSkin(char *name, char *model, char *skin, const char *s);
void CL_LoadClientinfo(clientinfo_t *ci, const char *s);
void CL_LoadState(load_state_t state);
void CL_RegisterSounds(void);
void CL_RegisterBspModels(void);
void CL_PrecacheViewModels(void);
/**
*   @brief  Called before entering a new level, or after changing dlls
**/
void CL_PrepRefresh(void);
/**
*   @brief  
**/
void CL_UpdateConfigstring( const int32_t index);



//
// download.cpp
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
// input.cpp
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

/**
*   @brief  Create all move related cvars, and registers all input commands. 
*           (Also gives the client game a chance.)
**/
void CL_RegisterInput(void);
/**
*   @brief  Updates msec, angles and builds the interpolated movement vector for local movement prediction.
*           Doesn't touch command forward/side/upmove, these are filled by CL_FinalizeCommand.
**/
void CL_UpdateCommand(int64_t msec);
/**
*   @brief  Builds the actual movement vector for sending to server. Assumes that msec
*           and angles are already set for this frame by CL_UpdateCommand.
**/
void CL_FinalizeCommand(void);
/**
*   @brief  Sends the current pending command to server.
**/
void CL_SendCommand(void);



//
// parse.cpp
//
void CL_CheckForVersion( const char *s );
// attempt to scan out an IP address in dotted-quad notation and
// add it into circular array of recent addresses
void CL_CheckForIP( const char *s );
void CL_ParseServerMessage(void);
void CL_SeekDemoMessage(void);



//
// entities.cpp
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
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void CL_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t origin );
/**
*   @brief  Used by the sound code in order to determine the reverb effect to apply for the entity's origin.
**/
qhandle_t CL_GetEAXBySoundOrigin( const int32_t entityNumber, vec3_t org );
/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CL_CheckEntityPresent( const int32_t entityNumber, const char *what );
#endif


//
// view.cpp
//
void V_Init(void);
void V_Shutdown(void);
void V_RenderView(void);
/**
*   @brief  Calculate the client's PVS which is a necessity for culling out
*           local client entities.
**/
void V_CalculateLocalPVS( const vec3_t viewOrigin );
void V_AddEntity(entity_t *ent);
void V_AddParticle(particle_t *p);
void V_AddLight(const vec3_t org, float intensity, float r, float g, float b);
void V_AddSphereLight(const vec3_t org, float intensity, float r, float g, float b, float radius);
void V_AddSpotLight(const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle);
void V_AddSpotLightTexEmission( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex );
void V_AddLightStyle(int style, float value);



//
// predict.cpp
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
// demo.cpp
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
// locs.cpp
//
// Location stuff is disabled since if I do want it, I want it implemented differently. Move to CLGame.
#if 0
void LOC_Init(void);
void LOC_LoadLocations(void);
void LOC_FreeLocations(void);
void LOC_UpdateCvars(void);
void LOC_AddLocationsToScene(void);
#endif


//
// console.cpp
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
void Con_SkipNotify( const qboolean skip );
void Con_RegisterMedia(void);
void Con_CheckResize(void);

void Key_Console(int key);
void Key_Message(int key);
void Char_Console(int key);
void Char_Message(int key);



//
// refresh.cpp
//
void    CL_InitRefresh(void);
void    CL_ShutdownRefresh(void);
void    CL_RunRefresh(void);



//
// screen.cpp
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
void    SCR_DeltaFrame(void);
const qhandle_t SCR_GetFont(void);
void    SCR_SetHudAlpha( const float alpha);
/**
*   @brief Fades alpha in and out, keeping the alpha visible for 'visTime' amount.
*   @return 'Alpha' value of the current moment in time. from(startTime) to( startTime + visTime ).
**/
const float SCR_FadeAlpha( const uint64_t startTime, const uint64_t visTime, const uint64_t fadeTime );
int     SCR_DrawStringEx(int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font);
void    SCR_DrawStringMulti(int x, int y, int flags, size_t maxlen, const char *s, qhandle_t font);

/**
*   @brief  Clear the chat HUD.
**/
void SCR_ClearChatHUD_f( void );
/**
*   @brief  Append text to chat HUD.
**/
void SCR_AddToChatHUD( const char *text );

/**
*   @return A lowercase string matching the textual name of the color for colorIndex.
**/
const char *SCR_GetColorName( const color_index_t colorIndex );



//
// cin.cpp
//
void    SCR_StopCinematic(void);
void    SCR_FinishCinematic(void);
void    SCR_RunCinematic(void);
void    SCR_DrawCinematic(void);
void    SCR_ReloadCinematic(void);
void    SCR_PlayCinematic(const char *name);



//
// ascii.cpp
//
void CL_InitAscii(void);



//
// http.cpp
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
// crc.cpp
//
byte COM_BlockSequenceCRCByte(byte *base, size_t length, int sequence);



//
// effects.cpp
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