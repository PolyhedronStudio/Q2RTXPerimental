#pragma once



/**
*
*
*   Client Events/Frames/Messages:
*
* 
**/
/**
*   @brief  Stores
**/
typedef struct client_frame_s {
    //! Sequential identifier, incremented each client game's 'local' frame.
    int64_t number;
} client_frame_t;



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
    //! The frame is valid if all parsing went well.
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

    //! Stores the parsed entity states for this 'snapshot' frame.
    //entity_state_t  entities[ MAX_CLIENT_ENTITIES ];
} server_frame_t;

// locally calculated frame flags for debug display
#define FF_SERVERDROP   (1<<4)
#define FF_BADFRAME     (1<<5)
#define FF_OLDFRAME     (1<<6)
#define FF_OLDENT       (1<<7)
#define FF_NODELTA      (1<<8)



/**
*
*
*   Client State:
*
*
**/
// Variable client FPS
#define CL_FRAMETIME    BASE_FRAMETIME
#define CL_1_FRAMETIME  BASE_1_FRAMETIME

//! These are constants for cl_player_model cvar.
#define CL_PLAYER_MODEL_DISABLED     0
#define CL_PLAYER_MODEL_ONLY_GUN     1
#define CL_PLAYER_MODEL_FIRST_PERSON 2
#define CL_PLAYER_MODEL_THIRD_PERSON 3

// Max client weapon models.
#define MAX_CLIENTVIEWMODELS        64        // PGM -- upped from 16 to fit the chainfist vwep



/**
*   @brief  Stores captured mouse motion data for (later) use in the
*           move command generation process.
**/
typedef struct client_mouse_motion_s {
    //! False if there was no mouse motion for this (client) frame.
    qboolean hasMotion;

    //! X-Axis Delta Movement.
    int32_t deltaX;
    //! Y-Axis Delta Movement.
    int32_t deltaY;

    //! Floating point X-axis move distance.
    float moveX;
    //! Floating point Y-axis move distance.
    float moveY;

    //! Mouse speed.
    float speed;
} client_mouse_motion_t;

/**
*   @brief  Stores a received, minimally needed data of a client residing in the server.
**/
typedef struct clientinfo_s {
    //! Actual client display name.
    char name[ MAX_QPATH ];
	//! Handle to the client's skin.
    qhandle_t skin;
	//! Handle to the client's icon.
    qhandle_t icon;
    //! Player model filename.
    char model_name[ MAX_QPATH ];
	//! Model skin filename.
    char skin_name[ MAX_QPATH ];
    //! Model index.
    qhandle_t model;
    //! Weapon Model index for each distinct weapon viewmodewl.
    qhandle_t weaponmodel[ MAX_CLIENTVIEWMODELS ];
} clientinfo_t;

/**
*   @brief  
**/
typedef struct client_movecmd_s {
    //! The command number.
    //uint64_t commandNumber;

    //! The user input command data.
    usercmd_t cmd;

    //! Simulation time when the command was sent. (cls.realtime)
    //uint64_t timeSent;
    //! System time when the command was received. (cls.realtime)
    //uint64_t timeReceived;

    struct {
        // The simulation time(cl.time) when prediction was run.
        uint64_t time;
        // The predicted origin for this command.
        Vector3 origin;
        //! Predicted velocity for this command.
        Vector3 velocity;
        //! The prediction error for this command.
        Vector3 error;
    } prediction;
} client_movecmd_t;

/**
*	@brief  Data needed for storing a 'client user input' command history. Store its time at which
*	        it was sent as well as when it was received. Used for calculating pings.
**/
typedef struct client_usercmd_history_s {
    //! Command Number indexing into the cl.moveCommands.
    uint64_t commandNumber;
    //! Time sent, for calculating pings.
    uint64_t timeSent;
    //! Time rcvd, for calculating pings.
    uint64_t timeReceived;
} client_usercmd_history_t;

/**
*   @brief  Stores client-side predicted player_state_t information.
**/
typedef struct client_predicted_state_s {
    //! Last processed client move command.
    client_movecmd_t cmd;

    //! Reset each time we receive a new server frame. Keeps track of the local client's player_state_t
    //! until yet receiving another new server frame.
    player_state_t currentPs;
    //! This is always the previous client's frame player_state_t.
    player_state_t lastPs;

    //! Player(-Entity) Bounding Box.
    Vector3 mins, maxs;

    //! Stores the ground information. If there is no actual active, valid, ground, then ground.entity will be nullptr.
    cm_ground_info_t ground;
    //! Stores the 'liquid' information. This can be lava, slime, or water.
    cm_contents_info_t liquid;

    //! Stores data for player origin/view transitions.
    struct {
        struct {
            //! Stores the stepheight.
            double height;
            //! Stores cl.realtime of when the step was last changed.
            uint64_t timeChanged;
        } step;
        struct {
            //! Stores the previous view height[#1] and the current[#0] height.
            double height[ 2 ];
            //! Stores cl.time of when the height was last changed.
            uint64_t timeHeightChanged;
        } view;
    } transition;

    //! Margin of origin error to correct for this frame.
    Vector3 error;
} client_predicted_state_t;

/**
*   @brief  Used to store the client's audio 'spatial awareness'.
*           It is always set by CL_CalculateViewValues, which can
*           also be called directly from the main loop if rendering
*           is disabled, yet sound is running.
**/
typedef struct client_listener_spatialization_s {
    //! View origin.
    vec3_t      origin;

    //! View vectors.
    vec3_t      v_forward;
    vec3_t      v_right;
    vec3_t      v_up;

    //! Entity number of which we're spatialized to.
    int32_t     entnum;
} client_listener_spatialization_t;


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
    //! 'Dirty', so said the old q2rtx comment, demo configstrings. Idk why 'Dirty', someone, go clean them plz.
    byte        dcs[ CS_BITMAP_BYTES ];

    /**
    *
    *   Client User Commands:
    *
    **/
    //! The initial client frame sequence.
    int64_t initialSeq;

    //! Immediately send the 'command' packet, or not.
    qboolean    sendPacketNow;
    //! Last time of packet transmission.
    uint64_t	lastTransmitTime;
    //! Last command number which is meant to be transmitted.
    uint64_t	lastTransmitCmdNumber;
    //! Real last command number that actually got transmitted.
    uint64_t	lastTransmitCmdNumberReal;

    //! Current client move command that user input is enacting on.
    client_movecmd_t moveCommand;
    //! Circular client buffer of (predicted-)move commands.
    client_movecmd_t moveCommands[ CMD_BACKUP ];

    //! The current user command its numerical index.
    uint64_t currentUserCommandNumber;
    //! Circular client history buffer that stores the user command index, its time sent, and time received.
    client_usercmd_history_t history[ CMD_BACKUP ];
    
    

    /**
    *
    *   Local Client Movement State:
    *
    **/
    //! Interpolated movement vector used for local prediction,
    //! never sent to server, rebuilt each client frame.
    Vector3     localmove;
    //! Accumulated mouse forward/side movement, added to both
    //! localmove and pending cmd, cleared each time cmd is finalized.
    Vector2     mousemove;

    //! The client maintains its own idea of view angles, which are
    //! sent to the server each frame.  It is cleared to 0 upon entering each level.
    //! the server sends a delta each frame which is added to the locally
    //! tracked view angles to account for standing on rotating objects,
    //! and teleport direction changes.
    Vector3     viewangles;
    //! The delta of the current frames move angles.
    Vector3     delta_angles;



    /**
    *
    *   Local Predicted Frame State:
    *
    **/
    //! This always has its value reset to the latest received frame's data. For all the time in-between the
    //! received frames, it maintains track of the predicted client states.
    //! (Currently though, player_state_t only.)
    client_predicted_state_t predictedState;
    //! Clien't s audio spatialized data.
    client_listener_spatialization_t listener_spatialize;

    //! This is the 'extrapolated moment in time' value of the client's game state.
    //! Always >= cl.serverTime and <= cl.serverTime + FRAMERATE_MS
    int64_t extrapolatedTime;

    //! We always extrapolate only a single frame ahead. Linear extrapolation fraction between cl.oldframe and cl.frame.
    double  xerpFraction;



    /**
    *
    *   Client Side Entities:
    *
    **/
    //! Rebuilt each valid frame:
    centity_t       *solidEntities[ MAX_PACKET_ENTITIES ];
    int32_t         numSolidEntities;
    //! Rebuilt each valid frame.
    
    //! Stores all entity baseline states to use for delta-ing. Sent and received at time of connect.
    entity_state_t	baselines[ MAX_EDICTS ];

    /**
    *   @brief  Each frame maintains an index into this buffer.Entity states are parsed
    *           from the frame into this buffer, and then copied into the relevant entities.
    **/
    entity_state_t	entityStates[ MAX_PARSE_ENTITIES ];
    //! Number of total current entity states.
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
    //! Linear interpolation fraction between cl.oldframe and cl.frame.
    double		lerpfrac;



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
    qboolean    thirdPersonView;
    //! Predicted values, used for smooth player entity movement in thirdperson view.
    vec3_t      playerEntityOrigin;
    vec3_t      playerEntityAngles;
    //! Local PVS
    //byte localPVS[ VIS_MAX_BYTES ];
    //int32_t localLastValidCluster;
    //! Client Possible Visibility Set, used for local entities.
    struct LocalPVS {
        // PVS Set.
        byte pvs[ 8192 ];
        // Last valid cluster.
        int32_t lastValidCluster;// = -1;

        //! Leaf, Area, and Current Cluster.
        mleaf_t *leaf;// = nullptr;
        int32_t leafArea;// = -1;
        int32_t leafCluster;// = -1;

        //// Area Bits.
        //byte areaBits[ 32 ];
        //// Area Btes.
        //int32_t areaBytes;
    } localPVS;


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
    char            layout[ MAX_NET_STRING ];
    //! Parsed sound event parameters.
    snd_params_t    snd;


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
    //! Maximum number of clients that the current connected game accepts.
    int32_t		maxclients;
    /**
    *   @brief      Our local client slot number, acts as our personal private index into `CS_PLAYERSKINS`.
    *               Never changed during gameplay, set by serverdata packet which is sent during connecting phase from SV_New_F.
    **/
    int32_t		clientNumber;
    /**
    *   @brief      The server entity which represents our local client view.
    *               This is a pointer into `clg_entities`, and may point to an entity we are chasing.
    **/
    centity_t   *clientEntity;

    // Received pmove configuration.
    //pmoveParams_t pmp;

    //! Configstrings.
    configstring_t baseconfigstrings[ MAX_CONFIGSTRINGS ];
    configstring_t configstrings[ MAX_CONFIGSTRINGS ];
    //! short format - q2dm1, etc.
    char mapname[ MAX_QPATH ];

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
    //! The current map's BSP (collision-)models data.
    cm_t collisionModel; // Once was: bsp_t *bsp;

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

    // WID: 40hz - For proper frame lerping for 10hz/custom rate models.
    float	sv_frametime_inv;
    int64_t	sv_frametime;
    int64_t	sv_framediv;
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