/********************************************************************
*
*
*   Player Movement:
*
*
********************************************************************/
#pragma once


/**
*   pmove_state_t is the information necessary for client side movement prediction.
**/
typedef enum {  // : uint8_t {
    // Types that can accelerate and turn:
    PM_NORMAL,      //! Gravity. Clips to world and its entities.
    PM_GRAPPLE,     //! No gravity. Pull towards velocity.
    PM_NOCLIP,      //! No gravity. Don't clip against entities/world at all. 
    PM_SPECTATOR,   //! No gravity. Only clip against walls.

    // Types with no acceleration or turning support:
    PM_DEAD,
    PM_GIB,         //! Different bounding box for when the player is 'gibbing out'.
    PM_FREEZE       //! Does not respond to any movement inputs.
} pmtype_t;

// pmove->pm_flags
#define PMF_NONE						0   //! No flags.
#define PMF_DUCKED						1   //! Player is ducked.
#define PMF_JUMP_HELD					2   //! Player is keeping jump button pressed.
#define PMF_ON_GROUND					4   //! Player is on-ground.
#define PMF_TIME_WATERJUMP				8   //! pm_time is waterjump.
#define PMF_TIME_LAND					16  //! pm_time is time before rejump.
#define PMF_TIME_TELEPORT				32  //! pm_time is non-moving time.
#define PMF_NO_POSITIONAL_PREDICTION	64  //! Temporarily disables prediction (used for grappling hook).
//#define PMF_TELEPORT_BIT				128 //! used by q2pro
#define PMF_ON_LADDER					128	//! Signal to game that we are on a ladder.
#define PMF_NO_ANGULAR_PREDICTION		256 //! Temporary disables angular prediction.
#define PMF_IGNORE_PLAYER_COLLISION		512	//! Don't collide with other players.
#define PMF_TIME_TRICK_JUMP				1024//! pm_time is the trick jump time.
//#define PMF_GROUNDENTITY_CHANGED        2048//! Set if the ground entity has changed between previous and current pmove state.

#define MAX_PS_EVENTS 2

/**
*   This structure needs to be communicated bit-accurate from the server to the client to guarantee that
*   prediction stays in sync. If any part of the game code modifies this struct, it will result in a
*   prediction error of some degree.
**/
typedef struct {
    //! The player move type.
    pmtype_t    pm_type;
    //! The state's flags describing the move's situation.
    uint16_t    pm_flags;		//! Ducked, jump_held, etc
    //! Timer value for a specific few of state flags.
    uint16_t	pm_time;		//! Each unit = 8 ms

    //! Bob Cycle.
    uint8_t bob_cycle;

    //! Gravity to apply.
    int16_t     gravity;

    //! State origin.
    Vector3		origin;
    //! Add to command angles to get view direction, it is changed by spawns, rotating objects, and teleporters
    Vector3     delta_angles;
    //! State Velocity.
    Vector3		velocity;

    //! State viewheight.
    int8_t		viewheight;		//! View height, added to origin[2] + viewoffset[2], for crouching.

    //! PMove generated state events.
    int32_t			eventSequence;
    int32_t			events[ MAX_PS_EVENTS ];
    int32_t			eventParms[ MAX_PS_EVENTS ];

    // WID: TODO: Just use its entity events instead?
    //int32_t			externalEvent;	//! Events set on player from another source.
    //int32_t			externalEventParm;
    //int32_t			externalEventTime;
} pmove_state_t;

/**
*	@brief	Used to configure player movement with, it is set by SG_ConfigurePlayerMoveParameters.
*
*			NOTE: In the future this will change, obviously.
**/
typedef struct {
    //! Stop speed.
    float pm_stop_speed;
    //! Server determined maximum speed.
    float pm_max_speed;
    //! Velocity that is set for jumping. (Determines the height we aim for.)
    float pm_jump_height;

    //! General up/down movespeed for on a ladder.
    float pm_ladder_speed;
    //! Maximum 'strafe' side move speed while on a ladder.
    float pm_ladder_sidemove_speed;
    //! Ladder modulation scalar for when being in-water and climbing a ladder.
    float pm_ladder_mod;

    //! Speed for when ducked and crawling on-ground.
    float pm_duck_speed;
    //! Speed for when moving in water(swimming).
    float pm_water_speed;
    //! Speed for when flying.
    float pm_fly_speed;

    //! General acceleration.
    float pm_accelerate;
    //! If set, general 'in-air' acceleration.
    float pm_air_accelerate;
    //! General water acceleration.
    float pm_water_accelerate;

    //! General friction.
    float pm_friction;
    //! General water friction.
    float pm_water_friction;
} pmoveParams_t;

/**
*   @brief  Used for registering entity touching resulting traces.
**/
#define MAX_TOUCH_TRACES    32
typedef struct pm_touch_trace_list_s {
    uint32_t numberOfTraces;
    trace_t traces[ MAX_TOUCH_TRACES ];
} pm_touch_trace_list_t;

/**
*   @brief  Stores the final ground information results.
**/
typedef struct pm_ground_info_s {
    //! Pointer to the actual ground entity we are on.(nullptr if none).
    struct edict_s *entity;

    //! A copy of the plane data from the ground entity.
    cplane_t        plane;
    //! A copy of the ground plane's surface data. (May be none, in which case, it has a 0 name.)
    csurface_t      surface;
    //! A copy of the contents data from the ground entity's brush.
    contents_t      contents;
    //! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
    cm_material_t *material;
} pm_ground_info_t;

/**
*   @brief  Stores the final 'liquid' information results. This can be lava, slime, or water, or none.
**/
typedef struct pm_liquid_info_s {
    //! The actual BSP liquid 'contents' type we're residing in.
    contents_t      type;
    //! The depth of the player in the actual liquid.
    liquid_level_t	level;
} pm_liquid_info_t;

/**
*   @brief  Object storing data such as the player's move state, in order to perform another
*           frame of movement on its data.
**/
typedef struct {
    /**
    *   (In/Out):
    **/
    pmove_state_t s;

    /**
    *   (In):
    **/
    //! The player's move command.
    usercmd_t	cmd;
    //! Set to 'true' if player state 's' has been changed outside of pmove.
    qboolean    snapinitial;
    //! Opaque pointer to the player entity.
    struct edict_s *player;

    /**
    *   (Out):
    **/
    //! Contains the trace results of any entities touched.
    pm_touch_trace_list_t touchTraces;

    /**
    *   (In/Out):
    **/
    //! Actual view angles, clamped to (0 .. 360) and for Pitch(-89 .. 89).
    Vector3 viewangles;
    //! Bounding Box.
    Vector3 mins, maxs;

    //! Stores the ground information. If there is no actual ground, ground.entity will be nullptr.
    pm_ground_info_t ground;
    //! Stores the possible solid liquid type brush we're in(-touch with/inside of)
    pm_liquid_info_t liquid;

    /**
    *   (Out):
    **/
    //! Callbacks to test the world with.
    //! Trace against all entities.
    const trace_t( *q_gameabi trace )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const contents_t contentMask );
    //! Clips to world only.
    const trace_t( *q_gameabi clip )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, /*const void *clipEntity,*/ const contents_t contentMask );
    //! PointContents.
    const contents_t( *q_gameabi pointcontents )( const vec3_t point );

    /**
    *   (In):
    **/
    // [KEX] variables (in)
    Vector3 viewoffset; // Predicted last viewoffset (for accurate calculation of blending)

    /**
    *   (Out):
    **/
    // [KEX] results (out)
    Vector4 screen_blend;
    //! Merged with rdflags from server.
    int32_t rdflags;

    //! Play jump sound.
    qboolean jump_sound;
    //! Impact delta, for falling damage.
    float impact_delta;

    //! We clipped on top of an object from below.
    qboolean step_clip;
    //! Step taken, used for smooth lerping stair transitions.
    float step_height;
} pmove_t;