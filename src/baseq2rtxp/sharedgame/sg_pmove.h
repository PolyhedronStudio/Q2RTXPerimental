/********************************************************************
*
*
*	SharedGame: Player Movement
*
*
********************************************************************/
#pragma once


/**
*	
*
*	PMove Constants:
* 
* 
**/
/**
*	(Predictable-)Player State Events:
**/
//! Enumerator event numbers, also act as string indices.
typedef enum {
	PS_EV_NONE = 0,

	//
	// External Events:
	//
	//! Reload Weapon.
	PS_EV_WEAPON_RELOAD,
	//! Weapon Primary Fire.
	PS_EV_WEAPON_PRIMARY_FIRE,
	//! Weapon Secondary Fire.
	PS_EV_WEAPON_SECONDARY_FIRE,
	
	//! Weapon Holster/Draw:
	PS_EV_WEAPON_HOLSTER_AND_DRAW,
	// TODO: We really do wanna split weapon draw and holstering, but, alas, lack animation skills.
	//! Draw Weapon.
	PS_EV_WEAPON_DRAW,
	//! Holster Weapon.
	PS_EV_WEAPON_HOLSTER,

	//
	// Non External Events:
	// 
	//! For when jumping up.
	PS_EV_JUMP_UP,
	//! Or Fall, FallShort, FallFar??
	PS_EV_JUMP_LAND,

	//! Maximum.
	PS_EV_MAX,
} sg_player_state_event_t;
//! String representatives.
extern const char *sg_player_state_event_strings[ PS_EV_MAX ];


/**
*	@brief	Default player movement parameter constants.
**/
typedef struct default_pmoveParams_s {
	//! Stop speed.
	static constexpr float pm_stop_speed	= 100.f;
	//! Server determined maximum speed.
	static constexpr float pm_max_speed		= 300.f;
	//! Velocity that is set for jumping. (Determines the height we aim for.)
	static constexpr float pm_jump_height	= 270.f;

	//! General up/down movespeed for on a ladder.
	static constexpr float pm_ladder_speed			= 200.f;
	//! Maximum 'strafe' side move speed while on a ladder.
	static constexpr float pm_ladder_sidemove_speed = 150.f;
	//! Ladder modulation scalar for when being in-water and climbing a ladder.
	static constexpr float pm_ladder_mod			= 0.5f;

	//! Speed for when ducked and crawling on-ground.
	static constexpr float pm_duck_speed	= 100.f;
	//! Speed for when moving in water(swimming).
	static constexpr float pm_water_speed	= 400.f;
	//! Speed for when flying.
	static constexpr float pm_fly_speed		= 400.f;

	//! General acceleration.
	static constexpr float pm_accelerate		= 10.f;
	//! General water acceleration.
	static constexpr float pm_water_accelerate	= 10.f;

	//! General friction.
	static constexpr float pm_friction = 6.f;
	//! General water friction.
	static constexpr float pm_water_friction = 1.f;
} default_pmoveParams_t;

/**
*	Default BoundingBoxes:
**/
//! For when player is standing straight up.
static constexpr Vector3 PM_BBOX_STANDUP_MINS	= { -16.f, -16.f, -36.f };
static constexpr Vector3 PM_BBOX_STANDUP_MAXS	= { 16.f, 16.f, 36.f };
static constexpr float   PM_VIEWHEIGHT_STANDUP	= 30.f;
//! For when player is crouching.
static constexpr Vector3 PM_BBOX_DUCKED_MINS	= { -16.f, -16.f, -36.f };
static constexpr Vector3 PM_BBOX_DUCKED_MAXS	= { 16.f, 16.f, 8.f };
static constexpr float   PM_VIEWHEIGHT_DUCKED	= 4.f;
//! For when player is gibbed out.
static constexpr Vector3 PM_BBOX_GIBBED_MINS	= { -16.f, -16.f, 0.f };
static constexpr Vector3 PM_BBOX_GIBBED_MAXS	= { 16.f, 16.f, 24.f };
static constexpr float   PM_VIEWHEIGHT_GIBBED	= 8.f;

/**
*	StepHeight:
**/
//! Minimal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_MIN_STEP_SIZE = 4.f;
//! Maximal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_MAX_STEP_SIZE = 18.f;

/**
*	Slide Move Results:
**/
//! Succesfully performed the move.
static constexpr int32_t PM_SLIDEMOVEFLAG_MOVED = BIT( 0 );
//! It was blocked at some point, doesn't mean it didn't slide along the blocking object.
static constexpr int32_t PM_SLIDEMOVEFLAG_BLOCKED = BIT( 1 );
//! It is trapped.
static constexpr int32_t PM_SLIDEMOVEFLAG_TRAPPED = BIT( 2 );
//! Blocked by a literal wall.
static constexpr int32_t PM_SLIDEMOVEFLAG_WALL_BLOCKED = BIT( 3 );
//! Touched at least a single plane along the way.
static constexpr int32_t PM_SLIDEMOVEFLAG_PLANE_TOUCHED = BIT( 4 );



/**
*
*
*	PMove Functions:
*
*
**/
/**
*	@brief	Used to configure player movement with, it is set by SG_ConfigurePlayerMoveParameters.
*
*			NOTE: In the future this will change, obviously.
**/
typedef struct pmoveParams_s {
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

///**
//*   @brief  Stores the final ground information results.
//**/
//typedef struct pm_ground_info_s {
//    //! Pointer to the actual ground entity we are on.(nullptr if none).
//    struct edict_s *entity;
//
//    //! A copy of the plane data from the ground entity.
//    cplane_t        plane;
//    //! A copy of the ground plane's surface data. (May be none, in which case, it has a 0 name.)
//    csurface_t      surface;
//    //! A copy of the contents data from the ground entity's brush.
//    contents_t      contents;
//    //! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
//    cm_material_t *material;
//} pm_ground_info_t;
//
///**
//*   @brief  Stores the final 'liquid' information results. This can be lava, slime, or water, or none.
//**/
//typedef struct pm_liquid_info_s {
//    //! The actual BSP liquid 'contents' type we're residing in.
//    contents_t      type;
//    //! The depth of the player in the actual liquid.
//    liquid_level_t	level;
//} pm_liquid_info_t;


/**
*   @brief  Object storing data such as the player's move state, in order to perform another
*           frame of movement on its data.
**/
typedef struct pmove_s {
    /**
    *   (In/Out):
    **/
    player_state_t *playerState; //pmove_state_t s;

    /**
    *   (In):
    **/
    //! Time we're at in the simulation.
    QMTime      simulationTime;
    //! The real time of this move command.
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
    //Vector3 viewangles;
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
    QMEaseState easeDuckHeight;
    // [KEX] variables (in)
    //Vector3 viewoffset; // Predicted last viewoffset (for accurate calculation of blending)

    /**
    *   (Out):
    **/
    // [KEX] results (out)
    //Vector4 screen_blend;
    //! Merged with rdflags from server.
    //int32_t rdflags;

    //! XY Speed:
    //float xySpeed;

    //! Play jump sound.
    qboolean jump_sound;
    //! Impact delta, for falling damage.
    float impact_delta;

    //! We clipped on top of an object from below.
    qboolean step_clip;
    //! Step taken, used for smooth lerping stair transitions.
    float step_height;
} pmove_t;

/**
*	@brief	Shard Game Player Movement code implementation:
**/
void SG_PlayerMove( pmove_s *pmove, pmoveParams_s *params );
/**
*	@brief	Useful to in the future, configure the player move parameters to game specific scenarions.
**/
void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp );
