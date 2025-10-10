/********************************************************************
*
*
*   Player State:
*
*
********************************************************************/
#pragma once


/**
*   @brief  Player Movement Type.
**/
typedef enum {  // : uint8_t {
    //
    // Types that:
    //  - Accelerate
    //  - Turn
    //  - (Stair-)Step
	//  - Clip to world and its entities.
    //! Gravity. Accelerates movement to velocity. Clips to world and its entities
    PM_NORMAL,
    //! No gravity. Pull towards velocity. Clips to world and its entities
    PM_GRAPPLE,

    //
	// Excentric types:
    //  
    //! Fly around, no clipping at all.
    PM_NOCLIP,
    //! Fly around, but clips to world. (Can't escape it.).
    PM_SPECTATOR,

    //
    // Types that:
    //  - No control over movement.
    //  - Possibly 'auto move' the player.
    // 
    //! No movement or status HUD.
    PM_INTERMISSION,
    //! No movement or status HUD.
    PM_SPINTERMISSION,

    //
    // Types that are non user input reliant:
    //
    //! Different bounding box for when the player is 'dead'.
    PM_DEAD,
    //! Different bounding box for when the player is 'gibbing out'.
    PM_GIB,
    //! Does not respond to any movement inputs. Makes the player 'idle'/'still', not moving.
    PM_FREEZE       
} pmtype_t;

/**
*   pmove_state->pm_flags
**/
typedef uint16_t pmflags_t;

#define PMF_NONE                        ( 0 )    //! No flags.
#define PMF_DUCKED		                BIT( 0 ) //! Player is ducked.
#define PMF_JUMP_HELD	                BIT( 1 ) //! Player is keeping jump button pressed.
//#define PMF_ON_GROUND	BIT( 2 )  //! Player is on-ground.
#define PMF_ON_LADDER                   BIT( 2 ) //! Player is on a ladder.
#define PMF_TIME_LAND                   BIT( 3 ) //! pm_time is time before rejump.
#define PMF_TIME_KNOCKBACK              BIT( 4 ) //! pm_time is an air-accelerate only time
#define PMF_TIME_WATERJUMP              BIT( 5 ) //! pm_time is waterjump.
#define PMF_TIME_TELEPORT               BIT( 6 ) //! pm_time is non-moving time.
#define PMF_NO_POSITIONAL_PREDICTION    BIT( 7 ) //! Temporarily disables prediction (used for grappling hook).
#define PMF_NO_ANGULAR_PREDICTION       BIT( 8 ) //! Temporary disables angular prediction.
//#define PMF_IGNORE_PLAYER_COLLISION		BIT( 9 )	//! Don't collide with other players.
//#define PMF_TELEPORT_BIT				BIT( 10 )    //! used by q2pro
//#define PMF_TIME_TRICK_JUMP				BIT( 12 )   //! pm_time is the trick jump time.
//#define PMF_GROUNDENTITY_CHANGED      BIT( 13 )   //! Set if the ground entity has changed between previous and current pmove state.
#define PMF_ALL_TIMES   ( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_KNOCKBACK | PMF_TIME_TELEPORT )

//! Maximum number of player state events.
#define MAX_PS_EVENTS   ( 2 )
//static constexpr int32_t MAX_PS_EVENTS = 2;

/**
*   This structure needs to be communicated bit-accurate from the server to the client to guarantee that
*   prediction stays in sync. If any part of the game code modifies this struct, it will result in a
*   prediction error of some degree.
**/
typedef struct pmove_state_s {
    //! The player move type.
    pmtype_t    pm_type;
    //! The state's flags describing the move's situation.
    pmflags_t   pm_flags;		//! Ducked, jump_held, etc
    //! Timer value for a specific few of state flags.
    double	pm_time;		//! Each unit = 8 ms

    //! Gravity to apply.
    int16_t     gravity;
    //! Wish Speed of movement.
    int16_t     speed;
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
typedef enum refdef_flags_e {
    //! No specific screen-space flags.
	RDF_NONE = 0,
    //! Warp the client's screen as appropriate.
	RDF_UNDERWATER = BIT( 0 ),
    //! Used for rendering in the player configuration screen.
	RDF_NOWORLDMODEL = BIT( 1 ),
    //! Render IR Goggles.
	RDF_IRGOGGLES = BIT( 2 ),
    //! Render UV Goggles. ( Not implemented in VKPT. )
	RDF_UVGOGGLES = BIT( 3 )
} refdef_flags_t;
QENUM_BIT_FLAGS( refdef_flags_t );

/**
*   player_state->stats[] static indices that are shared with the engine( required for reasons. )
**/
#define STAT_HEALTH             0   // Client/Server need this to determine death.
#define STAT_LAYOUTS            1   //! Key Input needs this to see whether to close a layout string based display.
#define STAT_FRAGS              2   //! Server status info needs this.
#define STATS_GAME_OFFSET       3   //! The start offset for game specific player stats types.
#define MAX_STATS               64  //! Maximum number of stats we compile with.


//! Used to detect animation changes.
#define GUN_ANIMATION_TOGGLE_BIT    BIT(7)
#define HUD_TARGETHINT_TOGGLE_BIT   BIT(15)

//! Used to determine what player base channel animation to set.
//! "(crouch/walk/run)_forward/backward_left/right_(weapon)"
#define PM_MOVEDIRECTION_FORWARD_LEFT   BIT( 0 )
#define PM_MOVEDIRECTION_FORWARD        BIT( 1 )
#define PM_MOVEDIRECTION_FORWARD_RIGHT  BIT( 2 )
#define PM_MOVEDIRECTION_BACKWARD_LEFT  BIT( 3 )
#define PM_MOVEDIRECTION_BACKWARD       BIT( 4 )
#define PM_MOVEDIRECTION_BACKWARD_RIGHT BIT( 5 )
#define PM_MOVEDIRECTION_LEFT           BIT( 6 )
#define PM_MOVEDIRECTION_RIGHT          BIT( 7 )
#define PM_MOVEDIRECTION_RUN            BIT( 8 )
#define PM_MOVEDIRECTION_WALK           BIT( 9 )

//#define PM_MOVEDIRECTION_BACKWARD       1

// pmove_state_t is the information needed in addition to player_state_t
// to rendered a view. These are sent from client to server in an amount relative
// to the client's (optionally set) frame rate limit.
// 
// player_state_t is sent at a rate of 40hz:
// (So, one time for each frame, meaning 40 times a second.)
// Communicate BIT precise.
typedef struct player_state_s {
    //! Contains the current player movement values.
    pmove_state_t   pmove;

    /**
    *   View State:
    **/
    // These fields do not need to be communicated bit-precise
    Vector3 viewangles;		//! For fixed views.
    Vector3 viewoffset;		//! Add to pmovestate->origin
    Vector3 kick_angles;    //! Add to view direction to get render angles
                            //! set by weapon kicks, pain effects, etc

    /**
    *   (Networked) Gun State:
    **/
    struct {
        //! Weapon(gun model) index.
        uint32_t modelIndex;
        //! Animation ID. We use GUN_ANIMATION_TOGGLE_BIT so we can detect animation
        //! changes even if it is the same ID. Keep in mind that this limits our
        //! animationID range from 0 to 127.
        uint8_t animationID;
    } gun;

    /**
    *   Screen State:
    **/
    //float damage_blend[ 4 ];      // rgba full screen damage blend effect
    //! RGBA full screen general blend effect
    float screen_blend[ 4 ];
    //! horizontal field of view
    float fov;
    //! Refdef flags.
    refdef_flags_t rdflags;

    /**
    *   Misc State:
    **/
    //! The bob cycle, which is wrapped around to uint8_t limit.
    int32_t bobCycle;
    //! Numerical player stats storage. ( fast status bar updates etc. )
    int64_t stats[ MAX_STATS ];

    /**
    *   (Predicted-)Events:
    **/
    //! PMove generated state events.
    //int32_t     eventSequence;
    int32_t events[ MAX_PS_EVENTS ];
    int32_t eventParms[ MAX_PS_EVENTS ];

    // WID: TODO: Just use its entity events instead?
    int32_t externalEvent;	//! Events set on player from another source.
    int32_t externalEventParm;
    int64_t externalEventTime;


    /**
    * 
    *   NOTE:
    * 
    *   These are NOT communicated over the net at all. Some are calculated locally
    *   for either both Client AND/OR Server Game(s).
    * 
    **/
    //! [Client/Server]: Event Sequencing - To differentiate from a same event ID.
    int64_t	eventSequence;

    //! [Client]: Gun Angles on-screen.
    Vector3 gunangles;
    //! [Client]: Gun Pffset on-screen.
    Vector3 gunoffset;

    //! [Client/Server] Calculated bobMove value.
    double bobMove;
    //! [Client/Server] Actual XYSpeed.
    double xySpeed;
    //! [Client/Server] Actual XYZSpeed.
    double xyzSpeed;

    //! [Client/Server] Animation State
    struct {
        //! True if the crouch key is pressed.
        bool isCrouched;
        //! True if the walk modifier key is pressed.
        bool isWalking;
        //! True if our xySpeed/xyzSpeed has slowed down to its epsilon values.
        bool isIdle;

        //! True if we're 'in-air'.
        bool inAir;

        //! Movement Direction, used to determine what crouch/walk/run animation to play
        //! for the main animation channel..
        int32_t moveDirection;
    } animation;

    //! [Server]: Gun frame.
    //int32_t gunframe;
    //! server to game info for scoreboard			
    //int32_t ping; 
    //int64_t pmove_framecount;
    //int32_t	jumppad_frame;
} player_state_t;