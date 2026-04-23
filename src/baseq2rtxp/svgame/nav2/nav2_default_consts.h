/********************************************************************
*
*
*	ServerGame: Nav2 Constant(-Expression)s for default navigation mesh and agent values,
* 	such as waypointRadius, rebuildGoal distances, etc.
*
*
********************************************************************/
#pragma once



/**
*
*
* 		Core Navigation Constants:
*
*
**/
//! Default path for the engine to find nav files at.
static constexpr const char *NAV_PATH_DIR = "/maps/nav/";

//! Chunk size for incremental path processing. This is the number of nodes that will be expanded per incremental step.
static constexpr int32_t NAV_TILE_WORKER_STEP_CHUNK_SIZE = 1024;

/**
*
*
* 	Tile parameters:
*
*
**/
//! Small epsilon to ensure max bounds map to the correct tile.
static constexpr const double NAV_TILE_EPSILON = 0.001f;
//! Default tile size in world units, used when no metadata is available to the runtime.
static constexpr const int32_t NAV_TILE_DEFAULT_SIZE = 128;
//! Default XY cell size in world units, used when no metadata is available to the runtime.
static constexpr const double NAV_TILE_DEFAULT_CELL_SIZE_XY = 4.;
//! Default Z quantization for tile generation and runtime queries when no metadata is available.
static constexpr const double NAV_TILE_DEFAULT_Z_QUANT = 16.;

/**
*
*
* 	Goal and Waypoint parameters:
*
*
**/
//! Default Radius around waypoints to consider 'reached':
//! We take the (half-width / waypoint-resolution) of a typical NPC entity (16 units) as a reasonable default for the waypoint radius, which provides a good balance between precision and leniency in path following. 
//! This allows the agent to consider a waypoint reached when it is close enough, without requiring an exact position match, which can help smooth out movement and reduce jitter.
static constexpr const double NAV_DEFAULT_WAYPOINT_RADIUS = 16.;// PM_MAX_STEP_SIZE( 16 ) / ;

//! 2D distance change in goal position that triggers a path rebuild.
static constexpr const double NAV_DEFAULT_GOAL_REBUILD_2D_DISTANCE = 16.;
//! Maximum 2D distance change in goal position that neglects a path rebuild.
static constexpr const double NAV_DEFAULT_GOAL_REBUILD_2D_DISTANCE_MAX = 4096.;
//! 3D distance change in goal position that triggers a path rebuild.
static constexpr const double NAV_DEFAULT_GOAL_REBUILD_3D_DISTANCE = 36.;
//! Maximum 3D distance change in goal position that neglects a path rebuild.
static constexpr const double NAV_DEFAULT_GOAL_REBUILD_3D_DISTANCE_MAX = 960.;


/**
*
*
*	Jumping upwards:
*
*
**/
//! Default mimimum step height that the navigation system considers sa a stair.
static constexpr const double NAV_DEFAULT_STEP_MIN_SIZE = PHYS_STEP_MIN_SIZE;
//! Default maximum step height that the navigation system considers traversable without a jump.
static constexpr const double NAV_DEFAULT_STEP_MAX_SIZE = PHYS_STEP_MAX_SIZE;

//! The default value used for designating a plane as being an actual obstruction, or a slope and traversable, or flat and traversable.
static constexpr const double NAV_DEFAULT_MAX_SLOPE_NORMAL = PHYS_MAX_SLOPE_NORMAL;


/**
*
*
*	Jumping upwards:
*
*
**/
//! Max obstruction height allowed to jump over.
static constexpr const double NAV_DEFAULT_MAX_OBSTRUCTION_JUMP_SIZE = 48.;


/**
*
*
*	Dropping/Falling Downwards:
*
*
**/
//! Max allowed drop height (units, matches `nav_max_drop`).
static constexpr const double NAV_DEFAULT_MAX_DROP_HEIGHT = 128.;
//! Drop cap applied when rejecting large downward transitions (matches `nav_max_drop_height_cap`).
static constexpr const double NAV_DEFAULT_MAX_DROP_HEIGHT_CAP = 192.;


/**
*
*
*	DDA Line of Sight Parameters:
*
*
**/
//! Max vertical delta allowed for a line of sight check to succeed (units, matches `nav_los_max_vertical_delta`).
static constexpr const double NAV_DEFAULT_LOS_MAX_VERTICAL_DELTA = 8.;// ( NAV_DEFAULT_STEP_MAX_SIZE * 3 ) - ( NAV_DEFAULT_STEP_MIN_SIZE * 3 );
//! Default maximum sample count for LOS checks before skipping direct-shortcut LOS attempts for performance reasons.
static constexpr const int32_t NAV_DEFAULT_DIRECT_LOS_ATTEMPT_MAX_SAMPLES = 32768.;


/**
*
*
* 		Z Layer Selection Parameters:
*
*
**/
//! Horizontal distance at which blending begins (units).
static constexpr const double NAV_DEFAULT_BLEND_DIST_START = 2.;
//! Horizontal distance at which blending is fully biased to the goal Z (units).
static constexpr const double NAV_DEFAULT_BLEND_DIST_FULL = 18.;
