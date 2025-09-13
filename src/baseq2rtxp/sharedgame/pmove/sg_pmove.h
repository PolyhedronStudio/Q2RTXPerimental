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
*
*
*	PMove Constants:
*
*
*
*
**/
/**
*	(Predictable-)Player State Events:
**/
//! Enumerator event numbers, also act as string indices.
typedef enum sg_player_state_event_e {
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
    static constexpr float pm_stop_speed = 100.f;
    //! Server determined maximum speed.
    static constexpr float pm_max_speed = 300.f;
    //! Velocity that is set for jumping. (Determines the height we aim for.)
    static constexpr float pm_jump_height = 270.f;

    //! General up/down movespeed for on a ladder.
    static constexpr float pm_ladder_speed = 200.f;
    //! Maximum 'strafe' side move speed while on a ladder.
    static constexpr float pm_ladder_sidemove_speed = 150.f;
    //! Ladder modulation scalar for when being in-water and climbing a ladder.
    static constexpr float pm_ladder_mod = 0.5f;

    //! Speed for the viewheight and bounding box to ease in/out at when
    //! switching from crouch to stand-up and visa versa. ( 100ms )
    //static constexpr QMTime pm_duck_viewheight_speed = QMTime::FromMilliseconds( 100 );
    //! Movement speed for when ducked and crawling on-ground.
    static constexpr float pm_crouch_move_speed = 100.f;
    //! Movement speed for when moving in water(swimming).
    static constexpr float pm_water_speed = 400.f;
    //! Movement speed for when flying.
    static constexpr float pm_fly_speed = 400.f;

    //! General acceleration.
    static constexpr float pm_accelerate = 10.f;
    //! General acceleration.
    static constexpr float pm_air_accelerate = 1.f;
    //! General water acceleration.
    static constexpr float pm_water_accelerate = 10.f;

    //! General friction.
    static constexpr float pm_friction = 6.f;
    //! General water friction.
    static constexpr float pm_water_friction = 1.f;
} default_pmoveParams_t;

/**
*	Default BoundingBoxes:
**/
//! For when player is standing straight up.
static constexpr Vector3 PM_BBOX_STANDUP_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 PM_BBOX_STANDUP_MAXS = { 16.f, 16.f, 36.f };
static constexpr float   PM_VIEWHEIGHT_STANDUP = 30.f;
//! For when player is crouching.
static constexpr Vector3 PM_BBOX_DUCKED_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 PM_BBOX_DUCKED_MAXS = { 16.f, 16.f, 8.f };
static constexpr float   PM_VIEWHEIGHT_DUCKED = 4.f;
//! For when player is gibbed out.
static constexpr Vector3 PM_BBOX_GIBBED_MINS = { -16.f, -16.f, 0.f };
static constexpr Vector3 PM_BBOX_GIBBED_MAXS = { 16.f, 16.f, 24.f };
static constexpr float   PM_VIEWHEIGHT_GIBBED = 8.f;

/**
*	StepHeight:
**/
//! Minimal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_MIN_STEP_SIZE = 4.f;
//! Maximal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_MAX_STEP_SIZE = 18.f;
//! Offset for distance to account for between step and ground.
static constexpr float PM_STEP_GROUND_DIST = 0.25f;

/**
*	Slide Move Results:
**/
enum pm_slideMoveFlags_t {
    //! None.
    PM_SLIDEMOVEFLAG_NONE = 0,
    //! Successfully performed the move.
    PM_SLIDEMOVEFLAG_MOVED = BIT( 0 ),
    //! Touched at least a single plane along the way.
    PM_SLIDEMOVEFLAG_PLANE_TOUCHED = BIT( 1 ),
    //! It was blocked at some point, doesn't mean it didn't slide along the blocking object.
    PM_SLIDEMOVEFLAG_BLOCKED = BIT( 2 ),
    //! It is trapped.
    PM_SLIDEMOVEFLAG_TRAPPED = BIT( 3 ),
    //! Blocked by a literal wall(Normal UP == 1).
    PM_SLIDEMOVEFLAG_WALL_BLOCKED = BIT( 4 ),
    //! Standing on a sloped surface.
    //PM_SLIDEMOVEFLAG_SLOPE_BLOCKED = BIT( 5 ),
    //! Standing on a sloped surface.
    //PM_SLIDEMOVEFLAG_SLOPE_STEPPED = BIT( 6 ),
};
QENUM_BIT_FLAGS( pm_slideMoveFlags_t );

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

    //! Speed for the viewheight and bounding box to ease in/out at when
    //! switching from crouch to stand-up and visa versa. ( 100ms )
    //QMTime pm_duck_viewheight_speed;
    //! Speed for when ducked and crawling on-ground.
    float pm_crouch_move_speed;
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
*   @brief  Stores the final ground information results.
**/
typedef struct pm_ground_info_s {
    //! Pointer to the actual ground entity we are on.(nullptr if none).
    struct edict_ptr_t *entity;

    //! A copy of the plane data from the ground entity.
    cm_plane_t        plane;
    //! A copy of the ground plane's surface data. (May be none, in which case, it has a 0 name.)
    cm_surface_t      surface;
    //! A copy of the contents data from the ground entity's brush.
    cm_contents_t      contents;
    //! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
    struct cm_material_s *material;
} pm_ground_info_t;

/**
*   @brief  Stores the final 'liquid' information results. This can be lava, slime, or water, or none.
**/
typedef struct pm_contents_info_s {
    //! The actual BSP liquid 'contents' type we're residing in.
    cm_contents_t      type;
    //! The depth of the player in the actual liquid.
    cm_liquid_level_t	level;
} pm_contents_info_t;


/**
*   @brief  Used for registering entity touching resulting traces.
**/
static constexpr int32_t MAX_TOUCH_TRACES = 32;
typedef struct pm_touch_trace_list_s {
    //! Current count of stored traces. Always < MAX_TOUCH_TRACES.
    uint32_t count;
    //! Actual traces stored.
    cm_trace_t traces[ MAX_TOUCH_TRACES ];
} pm_touch_trace_list_t;

/**
*   @brief  Object storing data such as the player's move state, in order to perform another
*           frame of movement on its data.
**/
typedef struct pmove_s {
    /**
    *   Callbacks to test the world with:
    **/
    //! Trace against all entities.
    const cm_trace_t( *q_gameabi trace )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const cm_contents_t contentMask );
    //! Clips to world only.
    const cm_trace_t( *q_gameabi clip )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, /*const void *clipEntity,*/ const cm_contents_t contentMask );
    //! PointContents.
    const cm_contents_t( *q_gameabi pointcontents )( const vec3_t point );

    /**
    *   (In):
    **/
    //! Time we're at in the simulation.
    QMTime simulationTime;
    //! The player's move command.
    usercmd_t cmd;
    //! Set to 'true' if player state 's' has been changed outside of pmove.
    qboolean snapInitialPosition;
    //! Opaque pointer to the player entity.
    struct edict_ptr_t *playerEdict;

    /**
    *   (In/Out):
    **/
    player_state_t *state; //pmove_state_t s;
    //! Actual view angles, clamped to (0 .. 360) and for Pitch(-89 .. 89).
    //Vector3 viewangles;
    //! Bounding Box.
    Vector3 mins, maxs;

    //! Stores the ground information. If there is no actual ground, ground.entity will be nullptr.
    pm_ground_info_t ground;
    //! Stores the possible solid liquid type brush we're in(-touch with/inside of)
    pm_contents_info_t liquid;

    /**
    *   (In):
    **/
    QMEaseState easeDuckHeight;
    // [KEX] variables (in)
    //Vector3 viewoffset; // Predicted last viewoffset (for accurate calculation of blending)

    /**
    *   (Out):
    **/
    //! Contains the trace results of any entities touched.
    pm_touch_trace_list_t touchTraces;
    // [KEX] results (out)
    //Vector4 screen_blend;
    //! Merged with rdflags from server.
    //int32_t rdflags;

    //! XY Speed:
    //float xySpeed;

    //! We clipped on top of an object from below.
    qboolean step_clip;
    //! Step taken, used for smooth lerping stair transitions.
    double step_height;

    //! Play jump sound.
    qboolean jump_sound;
    //! Impact delta, for falling damage.
    double impact_delta;
} pmove_t;







/**
*
*
*
*
*	PMove Functions:
*
*
*
*
**/
/**
*	@brief	Actual in-moment local move variables.
*
*			All of the locals will be zeroed before each pmove, just to make damn sure we don't have
*			any differences when running on client or server
**/
struct pml_t {
    //! Forward, right and up vectors.
    Vector3	forward = {}, right = {}, up = {};

    //! Move msec.
    double msec = 0.;
    //! Move frameTime.
    double frameTime = 0.;

    //! Aerial?
    bool isAerial = false;
    //! Walking?
    bool isWalking = false;
    //! Are we on an actual ground plane?
    bool hasGroundPlane = false;
    //! A copy of the result from the last ground trace.
    cm_trace_t groundTrace = {};

    //! Speed at which we (impacted) the ground/other-surface.
    double impactSpeed = 0.;

    //! Origin at the start of move.
    Vector3		previousOrigin = {};
    //! Velocity at the start of the move.
    Vector3		previousVelocity = {};
    //! Stores the possible solid liquid type brush we're in(-touch with/inside of)
    pm_contents_info_t previousLiquid = {};
};
//! The local player move state for the entity that we're moving.
extern pml_t pml;

/**
*	@brief	Shard Game Player Movement code implementation:
**/
void SG_PlayerMove( pmove_s *pmove, pmoveParams_s *params );
/**
*	@brief	Useful to in the future, configure the player move parameters to game specific scenarions.
**/
void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp );
