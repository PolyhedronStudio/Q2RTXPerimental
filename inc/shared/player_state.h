/********************************************************************
*
*
*   Player State:
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
    PM_INTERMISSION,//! No movement or status bar.
    PM_SPINTERMISSION,//! No movement or status bar.

    // Types with no acceleration or turning support:
    PM_DEAD,
    PM_GIB,         //! Different bounding box for when the player is 'gibbing out'.
    PM_FREEZE       //! Does not respond to any movement inputs.
} pmtype_t;

/**
*   pmove_state->pm_flags
**/
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
#define	PMF_ALL_TIMES                   ( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_TIME_TRICK_JUMP )


//! Maximum number of player state events.
#define MAX_PS_EVENTS 2

/**
*   This structure needs to be communicated bit-accurate from the server to the client to guarantee that
*   prediction stays in sync. If any part of the game code modifies this struct, it will result in a
*   prediction error of some degree.
**/
typedef struct pmove_state_s {
    //! The player move type.
    pmtype_t    pm_type;
    //! The state's flags describing the move's situation.
    uint16_t    pm_flags;		//! Ducked, jump_held, etc
    //! Timer value for a specific few of state flags.
    uint16_t	pm_time;		//! Each unit = 8 ms

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
} pmove_state_t;



/**
*   player_state_t->refdef flags
**/
#define RDF_NONE            0       // No specific screen-space flags.
#define RDF_UNDERWATER      1       // Warp the client's screen as apropriate.
#define RDF_NOWORLDMODEL    2       // Used for rendering in the player configuration screen
//ROGUE
#define RDF_IRGOGGLES       4       //! Render IR Goggles.
#define RDF_UVGOGGLES       8       //! Render UV Goggles. ( Not implemented in VKPT. )

/**
*   player_state->stats[] static indices that are shared with the engine( required for reasons. )
**/
#define STAT_HEALTH             0   // Client/Server need this to determine death.
#define STAT_LAYOUTS            1   //! Key Input needs this to see whether to close a layout string based display.
#define STAT_FRAGS              2   //! Server status info needs this.
#define STATS_GAME_OFFSET       3   //! The start offset for game specific player stats types.
#define MAX_STATS               64  //! Maximum number of stats we compile with.


// pmove_state_t is the information needed in addition to player_state_t
// to rendered a view. These are sent from client to server in an amount relative
// to the client's (optionally set) frame rate limit.
// 
// player_state_t is sent at a rate of 40hz:
// (So, one time for each frame, meaning 40 times a second.)
typedef struct {
    // Communicate BIT precise.
    pmove_state_t   pmove;      // for prediction

    /**
    *   View State:
    **/
    // These fields do not need to be communicated bit-precise
    Vector3 viewangles;		//! For fixed views.
    Vector3 viewoffset;		//! Add to pmovestate->origin
    Vector3 kick_angles;    //! Add to view direction to get render angles
                            //! set by weapon kicks, pain effects, etc

    /**
    *   Gun State:
    **/
    //! Gun Angles on-screen.
    Vector3 gunangles;
    //! Gun offset on-screen.
    Vector3 gunoffset;
    //! Weapon(gun model) index.
    uint32_t gunindex;
    //! Current weapon model's frame.
    uint32_t gunframe;


    /**
    *   Screen State:
    **/
    //float damage_blend[ 4 ];      // rgba full screen damage blend effect
    //! RGBA full screen general blend effect
    float screen_blend[ 4 ];
    //! horizontal field of view
    float fov;
    //! Refdef flags.
    int32_t rdflags;

    /**
    *   Misc State:
    **/
    //! The bob cycle, which is wrapped around to uint8_t limit.
    int32_t bobCycle;
    //! Numerical player stats storage. ( fast status bar updates etc. )
    int32_t stats[ MAX_STATS ];

    /**
    *   (Predicted-)Events:
    **/
    //! PMove generated state events.
    //int32_t     eventSequence;
    //int32_t     events[ MAX_PS_EVENTS ];
    //int32_t     eventParms[ MAX_PS_EVENTS ];

    // WID: TODO: Just use its entity events instead?
    //int32_t   externalEvent;	//! Events set on player from another source.
    //int32_t   externalEventParm;
    //int32_t   externalEventTime;

    /**
    *   Not communicated over the net at all, and are calculated locally
    *   for both Client AND Server Game(s).
    **/
    //! Calculated bobMove value.
    double bobMove;
    //! XYSpeed.
    double xySpeed;
    //! XYSpeed.
    double xyzSpeed;
    //! server to game info for scoreboard			
    //int32_t ping; 
    //int64_t pmove_framecount;
    //int32_t	jumppad_frame;
    //int64_t	entityEventSequence;
} player_state_t;