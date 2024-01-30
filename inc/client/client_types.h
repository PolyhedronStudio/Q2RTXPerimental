#pragma once

// Include needed shared refresh types.
#include "refresh/shared_types.h"

/**
*
* 
*   Explosions.
* 
* 
**/
#define MAX_EXPLOSIONS  32

#define NOEXP_GRENADE   1
#define NOEXP_ROCKET    2

/**
*   @brief  Explosion struct for varying explosion type effects.
*/
typedef struct {
    //! Explosion Type.
    enum {
        ex_free,
        //ex_explosion, Somehow unused. lol. TODO: Probably implement some day? xD
        ex_misc,
        ex_flash,
        ex_mflash,
        ex_poly,
        ex_poly2,
        ex_light,
        ex_blaster,
        ex_flare
    } type;

    //! Render Entity.
    entity_t    ent;
    //! Amount of sprite frames.
    int         frames;
    //! Light intensity.
    float       light;
    //! Light RGB color.
    vec3_t      lightcolor;
    //! 
    float       start;
    //! Frame offset into the sprite.
    int64_t     baseframe;
    //! Frametime in miliseconds.
    int64_t     frametime;
} explosion_t;


//extern explosion_t  cl_explosions[ MAX_EXPLOSIONS ];

/**
*
*
*   Client State:
*
* 
**/
#define MAX_CLIENTWEAPONMODELS        20        // PGM -- upped from 16 to fit the chainfist vwep

/**
*   @brief
**/
typedef struct clientinfo_s {
    char name[ MAX_QPATH ];
    qhandle_t skin;
    qhandle_t icon;
    char model_name[ MAX_QPATH ];
    char skin_name[ MAX_QPATH ];
    qhandle_t model;
    qhandle_t weaponmodel[ MAX_CLIENTWEAPONMODELS ];
} clientinfo_t;

/**
*	@brief  Data needed for storing a 'client user input' command history. Store its time at which
*	        it was sent as well as when it was received. Used for calculating pings.
**/
typedef struct {
    //! Command Number indexing into the cl.predictedStates.
    uint64_t commandNumber;
    //! Time sent, for calculating pings.
    uint64_t timeSent;
    //! Time rcvd, for calculating pings.
    uint64_t timeReceived;
} client_usercmd_history_t;

/**
*   @brief  
**/
typedef struct {
    //! The user input command data.
    usercmd_t cmd;

    //! Simulation time when the command was sent.
    uint64_t simulationTime;
    //! System time when the command was sent.
    uint64_t systemTime;

    struct {
        uint64_t time; // The simulation time when prediction was run.
        Vector3 origin; // The predicted origin for this command.
        Vector3 error; // The prediction error for this command.
    } prediction;
} client_movecmd_t;

/**
*   @brief  Stores client-side predicted player_state_t information.
**/
typedef struct {
    //! User Command Input for this frame.
    usercmd_t cmd;

    //! Total accumulated screen blend.
    Vector4 screen_blend;
    //! Refdef Flags.
    int32_t rdflags;

    // for stair up smoothing
    double step;
    uint64_t step_time;

    //! Origin, angles and velocity of current frame pmove's results.
    Vector3 origin;
    Vector3 angles;
    Vector3 velocity;

    //! Margin of error for this frame.
    Vector3 error;
} client_predicted_state_t;

/**
*   @brief  Contains, if valid, snapshots of the player state and the range of
*           entity_state_t entities that were in the current frame.
**/
typedef struct {
    bool            valid;

    //! Sequential identifier, used for delta.
    int64_t         number;
    //! Delta frame identifier, negatives indicate no delta.
    int64_t         delta;

    //! Visibility area bits and bytes.
    byte            areabits[ MAX_MAP_AREA_BYTES ];
    int32_t         areabytes;

    //! A snapshot of the player's state during this frame.
    player_state_t  ps;
    //! The client number that this frame belongs to.
    int32_t         clientNum;

    //! The number of entities in the frame.
    int32_t         numEntities;
    //! Non-masked index into cl.entityStates array
    uint32_t        firstEntity;
} server_frame_t;

// locally calculated frame flags for debug display
#define FF_SERVERDROP   (1<<4)
#define FF_BADFRAME     (1<<5)
#define FF_OLDFRAME     (1<<6)
#define FF_OLDENT       (1<<7)
#define FF_NODELTA      (1<<8)

// Variable client FPS
#define CL_FRAMETIME    BASE_FRAMETIME
#define CL_1_FRAMETIME  BASE_1_FRAMETIME


/**
 *  @brief  The client state structure is cleared at each level load, and is exposed to
 *          the client game module to provide access to media and other client state.
 **/
typedef struct client_state_s {
    //! Used for chats, for possible time-outs.
    int64_t     timeoutcount;
    //! Dirty demo configstrings.
    byte            dcs[ CS_BITMAP_BYTES ];

    /**
    *
    *   Client User Command Related:
    *
    **/
    //! The initiali client frame sequence.
    int64_t initialSeq;

    //! Immediately send the 'command' packet, or not.
    bool        sendPacketNow;
    //! Last time of packet transmission.
    uint64_t	lastTransmitTime;
    //! Last command number which is meant to be transmitted.
    uint64_t	lastTransmitCmdNumber;
    //! Real last command number that actually got transmitted.
    uint64_t	lastTransmitCmdNumberReal;


    //! The current pmove state to be predicted this frame.
    client_predicted_state_t predictedState;


    //! The current command its numerical index.
    uint64_t currentUserCommandNumber;
    //! Current client move command that is being processed.
    client_movecmd_t moveCommand;
    //! The client maintains its own idea of view angles, which are
    //! sent to the server each frame.  It is cleared to 0 upon entering each level.
    //! the server sends a delta each frame which is added to the locally
    //! tracked view angles to account for standing on rotating objects,
    //! and teleport direction changes.
    Vector3      viewangles;
    //! Interpolated movement vector used for local prediction,
    //! never sent to server, rebuilt each client frame.
    Vector3     localmove;
    //! Accumulated mouse forward/side movement, added to both
    //! localmove and pending cmd, cleared each time cmd is finalized.
    Vector2     mousemove;
    //! The delta of the current frames move angles.
    Vector3     delta_angles;

    //! Circular client buffer of (predicted-) move commands.
    client_movecmd_t moveCommands[ CMD_BACKUP ];
    //! Circular client history buffer, of time sent, and received, for user commands.
    client_usercmd_history_t history[ CMD_BACKUP ];

    //! The actual pmove predicted state history results.
    /*client_predicted_state_t predictedStates[ CMD_BACKUP ]*/

    /////////////////////////////
    // TODO: Move to predictedState?
    struct {
        //! Current viewheight from client Pmove().
        int8_t  current;
        //! Viewheight before last change.
        int8_t  previous;
        //! Time when a viewheight change was detected.
        int64_t change_time;
        //! :ast groundentity reported by pmove.
    } viewheight;
    struct {
        centity_t *entity;
        //! last groundplane reported by pmove.
        cplane_t plane;
    } lastGround;
    /////////////////////////////

    /**
    *
    *   Entity States:
    *
    **/
    //! Rebuilt each valid frame:
    centity_t *solidEntities[ MAX_PACKET_ENTITIES ];
    int32_t			numSolidEntities;
    //! Stores all entity baseline states to use for delta-ing. Sent and received at time of connect.
    entity_state_t	baselines[ MAX_EDICTS ];
    //! Stores the actual received delta decoded entity states.
    entity_state_t	entityStates[ MAX_PARSE_ENTITIES ];
    int32_t			numEntityStates;
    //! Client entity specific message flags.
    msgEsFlags_t	esFlags;


    /**
    *
    *   Server Frames:
    *
    **/
    //! Received server frames, might possibly be invalid.
    server_frame_t	frames[ UPDATE_BACKUP ];
    uint32_t		frameflags;

    //! Last/Currently received from server:
    server_frame_t	frame;
    server_frame_t	oldframe;
    int64_t			servertime;
    int64_t			serverdelta;
    
    //! This is the 'moment-in-time' value that the client is rendering at.
    //! Always <= cl.servertime
    int64_t     time;
    //! The current "lerp" -fraction between 'oldframe' and 'frame'
    double      lerpfrac;

    /**
    *
    *   Refresh Related:
    *
    **/
    //! Refresh frame settings.
    refdef_t    refdef;
    //! Interpolated
    float       fov_x;
    float       fov_y;      // derived from fov_x assuming 4/3 aspect ratio.
    //! Set when refdef.angles is set.
    vec3_t      v_forward, v_right, v_up;
    //! Whether in thirdperson view or not.
    bool        thirdPersonView;
    //! Predicted values, used for smooth player entity movement in thirdperson view.
    vec3_t      playerEntityOrigin;
    vec3_t      playerEntityAngles;


    /**
    *
    *   Transient data from server.
    *
    **/
    //! General 2D overlay cmd script.
    char		layout[ MAX_NET_STRING ];
    int32_t		inventory[ MAX_ITEMS ];


    /**
    *
    *   Server State Information:
    *
    **/
    //! ss_* constants
    int32_t		serverstate;
    //! Server identification for prespawns.
    int32_t		servercount;
    //! Directory name of the current game(dir) that is in-use.
    char		gamedir[ MAX_QPATH ];
    //! Never changed during gameplay, set by serverdata packet.
    int32_t		clientNum;
    //! Maximum number of clients that the current connected game accepts.
    int32_t		maxclients;

    // Received pmove configuration.
    pmoveParams_t pmp;

    configstring_t baseconfigstrings[ MAX_CONFIGSTRINGS ];
    configstring_t configstrings[ MAX_CONFIGSTRINGS ];
    char		mapname[ MAX_QPATH ]; // short format - q2dm1, etc

    #if USE_AUTOREPLY
    uint64_t	reply_time;
    uint64_t	reply_delta;
    #endif

    //
    // locally derived information from server state
    //
    // TODO: Somehow move bsp_t and mmodel_t out of common/bsp.h.
    //       Essentially we don't want to include common in these parts of the code.
    #ifndef BSP_H
    struct bsp_t *bsp;

    qhandle_t model_draw[ MAX_MODELS ];
    struct mmodel_t *model_clip[ MAX_MODELS ];

    #else
    bsp_t *bsp;

    qhandle_t model_draw[ MAX_MODELS ];
    mmodel_t *model_clip[ MAX_MODELS ];
    #endif

    qhandle_t sound_precache[ MAX_SOUNDS ];
    qhandle_t image_precache[ MAX_IMAGES ];

    clientinfo_t clientinfo[ MAX_CLIENTS ];
    clientinfo_t baseclientinfo;

    char	weaponModels[ MAX_CLIENTWEAPONMODELS ][ MAX_QPATH ];
    int32_t	numWeaponModels;

    // WID: 40hz - For proper frame lerping for 10hz/custom rate models.
    float	sv_frametime_inv;
    int64_t	sv_frametime;
    int64_t	sv_framediv;

    // Data for view weapon
    struct {
        int64_t frame, last_frame;
        int64_t server_time;

        //qhandle_t muzzle_model;
        //int32_t muzzle_time;
        //float muzzle_roll, muzzle_scale;
        //int32_t muzzle_skin;
        //vec3_t muzzle_offset;
    } weapon;
    // WID: 40hz
} client_state_t;


typedef enum {
    ca_uninitialized,
    ca_disconnected,    // not talking to a server
    ca_challenging,     // sending getchallenge packets to the server
    ca_connecting,      // sending connect packets to the server
    ca_connected,       // netchan_t established, waiting for svc_serverdata
    ca_loading,         // loading level data
    ca_precached,       // loaded level data, waiting for svc_frame
    ca_active,          // game views should be displayed
    ca_cinematic        // running a cinematic
} connstate_t;

#define NOPART_GRENADE_EXPLOSION    1
#define NOPART_GRENADE_TRAIL        2
#define NOPART_ROCKET_EXPLOSION     4
#define NOPART_ROCKET_TRAIL         8
#define NOPART_BLOOD                16

#define DLHACK_ROCKET_COLOR         1
#define DLHACK_SMALLER_EXPLOSION    2
#define DLHACK_NO_MUZZLEFLASH       4

#define CL_PLAYER_MODEL_DISABLED     0
#define CL_PLAYER_MODEL_ONLY_GUN     1
#define CL_PLAYER_MODEL_FIRST_PERSON 2
#define CL_PLAYER_MODEL_THIRD_PERSON 3

//
// precache.c
//
typedef enum {
    LOAD_NONE,
    LOAD_MAP,
    LOAD_MODELS,
    LOAD_IMAGES,
    LOAD_CLIENTS,
    LOAD_SOUNDS
} load_state_t;

// parse.c
//
typedef struct {
    int type;
    vec3_t pos1;
    vec3_t pos2;
    vec3_t offset;
    vec3_t dir;
    int count;
    int color;
    int entity1;
    int entity2;
    int time;
} tent_params_t;

typedef struct {
    int entity;
    int weapon;
    bool silenced;
} mz_params_t;

typedef struct {
    int     flags;
    int     index;
    int     entity;
    int     channel;
    vec3_t  pos;
    float   volume;
    float   attenuation;
    float   timeofs;
} snd_params_t;