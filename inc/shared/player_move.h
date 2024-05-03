/********************************************************************
*
*
*   Player Movement:
*
*
********************************************************************/
#pragma once



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
    player_state_t *playerState; //pmove_state_t s;

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
    // [KEX] variables (in)
    Vector3 viewoffset; // Predicted last viewoffset (for accurate calculation of blending)

    /**
    *   (Out):
    **/
    // [KEX] results (out)
    Vector4 screen_blend;
    //! Merged with rdflags from server.
    int32_t rdflags;

    //! XY Speed:
    float xySpeed;

    //! Play jump sound.
    qboolean jump_sound;
    //! Impact delta, for falling damage.
    float impact_delta;

    //! We clipped on top of an object from below.
    qboolean step_clip;
    //! Step taken, used for smooth lerping stair transitions.
    float step_height;
} pmove_t;