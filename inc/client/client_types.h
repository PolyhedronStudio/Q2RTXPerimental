#pragma once


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
typedef struct explosion_s {
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
    } explosion_type;
    int32_t     type;

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

#define NOPART_GRENADE_EXPLOSION    1
#define NOPART_GRENADE_TRAIL        2
#define NOPART_ROCKET_EXPLOSION     4
#define NOPART_ROCKET_TRAIL         8
#define NOPART_BLOOD                16



/**
*
*
*   Particles:
*
*
**/
#define PARTICLE_GRAVITY        120
#define BLASTER_PARTICLE_COLOR  0xe0
#define INSTANT_PARTICLE    -10000.0f

typedef struct cparticle_s {
    struct cparticle_s *next;

    double   time;

    vec3_t  org;
    vec3_t  vel;
    vec3_t  accel;
    int     color;      // -1 => use rgba
    float   alpha;
    float   alphavel;
    color_t rgba;
    float   brightness;
} cparticle_t;



/**
*
*
*   Dynamic Lights:
*
*
**/
typedef struct cdlight_s {
    int32_t key;        // so entities can reuse same entry
    vec3_t  color;
    vec3_t  origin;
    float   radius;
    float   die;        // stop lighting after this time
    float   decay;      // drop this each second
    vec3_t  velosity;     // move this far each second
} cdlight_t;

#define DLHACK_ROCKET_COLOR         1
#define DLHACK_SMALLER_EXPLOSION    2
#define DLHACK_NO_MUZZLEFLASH       4
//extern explosion_t  cl_explosions[ MAX_EXPLOSIONS ];



/**
*
*
*   Sustains:
*
*
**/
typedef struct cl_sustain_s {
    int     id;
    int     type;
    int64_t endtime;
    int64_t nextthink;
    vec3_t  org;
    vec3_t  dir;
    int     color;
    int     count;
    int     magnitude;
    void    ( *think )( struct cl_sustain_s *self );
} cl_sustain_t;



/**
*
*
*   Server Events/Frames/Messages:
*
* 
**/
/**
*   @brief  Contains, if valid, snapshots of the player state and the range of
*           entity_state_t entities that were in the current frame.
**/
typedef struct server_frame_s {
    qboolean        valid;

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
*
*
*   Client State:
*
*
**/
//! These are constants for cl_player_model cvar.
#define CL_PLAYER_MODEL_DISABLED     0
#define CL_PLAYER_MODEL_ONLY_GUN     1
#define CL_PLAYER_MODEL_FIRST_PERSON 2
#define CL_PLAYER_MODEL_THIRD_PERSON 3

// Max client weapon models.
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
typedef struct client_usercmd_history_s {
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
typedef struct client_movecmd_s {
    //! The user input command data.
    usercmd_t cmd;

    //! Simulation time when the command was sent.
    uint64_t simulationTime;
    //! System time when the command was sent.
    uint64_t systemTime;

    struct {
        uint64_t time;  // The simulation time when prediction was run.
        Vector3 origin; // The predicted origin for this command.
        Vector3 velocity;// Velocity.
        Vector3 error;  // The prediction error for this command.
    } prediction;
} client_movecmd_t;

/**
*   @brief  Stores client-side predicted player_state_t information.
**/
typedef struct client_predicted_state_s {
    //! User Command Input for this frame.
    //usercmd_t cmd;
    // for stair up smoothing
    uint64_t step_time;
    double step;

    // Viewheight.
    float view_height;
    uint64_t view_height_time;

    //! Origin, angles and velocity of current frame pmove's results.
    struct {
        //! Total accumulated screen blend.
        Vector4 screen_blend;
        //! Refdef Flags.
        int32_t rdflags;

        // Predicted origin, velocity and angles.
        Vector3 origin;
        Vector3 velocity;
        Vector3 angles;

        // Predicted view offset.
        Vector3 viewOffset;
    } view;

    //! Ground entity we're predicted to hit.
    centity_t *groundEntity;
    //! Ground plane we predicted to hit.
    cplane_t groundPlane;

    //! Margin of error for this frame.
    Vector3 error;
} client_predicted_state_t;

/**
*   @brief  Stores captured mouse motion data for (later) use in the
*           move command generation process.
**/
typedef struct client_mouse_motion_s {
    qboolean hasMotion;

    int32_t deltaX;
    int32_t deltaY;

    float moveX;
    float moveY;

    float speed;
} client_mouse_motion_t;

/**
*   @brief  Used to store the client's audio 'spatial awareness'.
*           It is always set by CL_CalculateViewValues, which can
*           also be called directly from the main loop if rendering
*           is disabled, yet sound is running.
**/
typedef struct client_listener_spatialization_s {
    vec3_t      origin;
    vec3_t      v_forward;
    vec3_t      v_right;
    vec3_t      v_up;
    int32_t     entnum;
} client_listener_spatialization_t;

/**
*   @brief  Stores muzzleflash data from the last parsed svc_muzzleflash message.
**/
typedef struct {
    int32_t entity;
    int32_t weapon;
    qboolean silenced;
} mz_params_t;

/**
*   @brief  Stores sound data from the last parsed svc_sound message.
**/
typedef struct {
    int32_t flags;
    int32_t index;
    int32_t entity;
    int32_t channel;
    vec3_t  pos;
    float   volume;
    float   attenuation;
    float   timeofs;
} snd_params_t;

/**
 *  @brief  The client state structure is cleared at each level load, and is exposed to
 *          the client game module to provide access to media and other client state.
 **/
typedef struct client_state_s {
    //! Used for chats, for possible time-outs.
    int64_t     timeoutcount;
    //! Dirty demo configstrings.
    byte        dcs[ CS_BITMAP_BYTES ];

    /**
    *
    *   Client User Command Related:
    *
    **/
    //! The initiali client frame sequence.
    int64_t initialSeq;

    //! Immediately send the 'command' packet, or not.
    qboolean    sendPacketNow;
    //! Last time of packet transmission.
    uint64_t	lastTransmitTime;
    //! Last command number which is meant to be transmitted.
    uint64_t	lastTransmitCmdNumber;
    //! Real last command number that actually got transmitted.
    uint64_t	lastTransmitCmdNumberReal;

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

    //! The current command its numerical index.
    uint64_t currentUserCommandNumber;
    //! Current client move command that is being processed.
    client_movecmd_t moveCommand;
    //! Circular client buffer of (predicted-) move commands.
    client_movecmd_t moveCommands[ CMD_BACKUP ];

    //! The current pmove state to be predicted this frame.
    client_predicted_state_t predictedState;
    //! Circular client history buffer, of time sent, and received, for user commands.
    client_usercmd_history_t history[ CMD_BACKUP ];

    //! Clien't s audio spatialized data.
    client_listener_spatialization_t listener_spatialize;

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

    //! Last(old)/Currently(frame) frames received from the server:
    server_frame_t	frame;
    server_frame_t	oldframe;
    //! The server game time of the last received valid frame.
    int64_t			servertime;
    //! 
    int64_t			serverdelta;
    
    //! This is the 'moment-in-time' value that the client is interpolating at.
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
    qboolean        thirdPersonView;
    //! Predicted values, used for smooth player entity movement in thirdperson view.
    vec3_t      playerEntityOrigin;
    vec3_t      playerEntityAngles;
    /**
    *   @brief  Gets properly configured by the client game, when V_RenderView is called upon,
    *           and then passes on the data to the refresh render module.
    **/
    struct {
        //! Refresh dynamic lights.
        int32_t     r_numdlights;
        dlight_t    r_dlights[ MAX_DLIGHTS ];
        //! Refresh entities.
        int32_t     r_numentities;
        entity_t    r_entities[ MAX_ENTITIES ];
        //! Refresh particles.
        int32_t     r_numparticles;
        particle_t  r_particles[ MAX_PARTICLES ];
        //! Refresh lightstyles.
        lightstyle_t    r_lightstyles[ MAX_LIGHTSTYLES ];
    } viewScene;

    /**
    *
    *   Transient data from server.
    *
    **/
    //! Parsed layout event data, this is a general 2D overlay cmd script.
    char		layout[ MAX_NET_STRING ];
    //! Parsed inventory event data.
    int32_t		inventory[ MAX_ITEMS ];
    //! Parsed sound event parameters.
    snd_params_t     snd;

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
    //! Always points to the client entity itself.
    centity_t   *clientEntity;
    //! Maximum number of clients that the current connected game accepts.
    int32_t		maxclients;

    // Received pmove configuration.
    //pmoveParams_t pmp;

    // Configstrings.
    configstring_t baseconfigstrings[ MAX_CONFIGSTRINGS ];
    configstring_t configstrings[ MAX_CONFIGSTRINGS ];
    char		mapname[ MAX_QPATH ]; // short format - q2dm1, etc

    //#if USE_AUTOREPLY // Removed ifdef for memory alignment consistency sake.
    uint64_t	reply_time;
    uint64_t	reply_delta;
    //#endif

    //
    // Most are locally derived information from server state, with the exception of the following
    // which also store locally client-side specific data:
    // - model_clip
    // - model_draw
    // - sound_precache
    // - image_precache
    //
    //! Pointer to the current map's BSP data.
    bsp_t *bsp;
    //! Collision brush models.
    mmodel_t *model_clip[ MAX_MODELS ];
    //! Refresh handle buffer for all precached models.
    qhandle_t model_draw[ MAX_MODELS ];

    //! Buffer of unique indexed precached sounds.
    qhandle_t sound_precache[ MAX_SOUNDS ];
    //! Buffer of unique indexed precached images.
    qhandle_t image_precache[ MAX_IMAGES ];

    //! Client info for each connected client in the game.
    clientinfo_t clientinfo[ MAX_CLIENTS ];
    //! Baseline client info.
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

/**
*   @brief  Defines the actual client's current 'operating state'.
**/
typedef enum connstate_s {
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

/**
*   @brief  Defines the actual client's current 'loading phase'.
**/
typedef enum load_state_s {
    LOAD_NONE,
    LOAD_MAP,
    LOAD_MODELS,
    LOAD_IMAGES,
    LOAD_CLIENTS,
    LOAD_SOUNDS
} load_state_t;