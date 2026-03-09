/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
*    Constant(-Expression)s for default navigation mesh and agent values, 
* 	such as waypointRadius, rebuildGoal distances, etc.
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#pragma once



/** 
* 
* 
* 		Core Navigation Constants:
*  
*  
**/
//! Default path for the engine to find nav files at.
static constexpr const char * NAV_PATH_DIR = "/maps/nav/";

//! Small epsilon to ensure max bounds map to the correct tile.
static constexpr const double NAV_TILE_EPSILON = 0.001f;



/** 
* 
* 
* 	Goal and Waypoint parameters:
* 
* 
**/
//! Default Radius around waypoints to consider 'reached':
//! We take the half-width of a typical NPC entity (16 units) as a reasonable default for the waypoint radius, which provides a good balance between precision and leniency in path following. 
//! This allows the agent to consider a waypoint reached when it is close enough, without requiring an exact position match, which can help smooth out movement and reduce jitter.
static constexpr const double NAV_DEFAULT_WAYPOINT_RADIUS = 16.0;

//! 2D distance change in goal position that triggers a path rebuild.
static constexpr const double NAV_DEFAULT_GOAL_REBUILD_2D_DISTANCE = 16.0;
//! 3D distance change in goal position that triggers a path rebuild.
static constexpr const double NAV_DEFAULT_GOAL_REBUILD_3D_DISTANCE = 32.0;



/** 
* 
* 
* 		Jump Gap / Ledge Drop / Step Obstruction Parameters:
*  
*  
**/
//!
//! Stepping:
//! 
//! Default mimimum step height that the navigation system considers sa a stair.
static constexpr const double NAV_DEFAULT_STEP_MIN_SIZE = PHYS_STEP_MIN_SIZE;
//! Default maximum step height that the navigation system considers traversable without a jump.
static constexpr const double NAV_DEFAULT_STEP_MAX_SIZE		= PHYS_STEP_MAX_SIZE;
//! The default value used for designating a plane as being an actual obstruction, or a slope and traversable, or flat and traversable.
static constexpr const double NAV_DEFAULT_MAX_SLOPE_NORMAL	= PHYS_MAX_SLOPE_NORMAL;


//!
//! Jumping:
//! 
//! Max obstruction height allowed to jump over.
static constexpr const double NAV_DEFAULT_MAX_OBSTRUCTION_JUMP_SIZE = 48.0;

//!
//! Falling:
//! 
//! Max allowed drop height (units, matches `nav_max_drop`).
static constexpr const double NAV_DEFAULT_MAX_DROP_HEIGHT = 64.0;
//! Drop cap applied when rejecting large downward transitions (matches `nav_max_drop_height_cap`).
static constexpr const double NAV_DEFAULT_MAX_DROP_HEIGHT_CAP = 192.;



/** 
*  
*  
* 		Z Layer Selection Parameters:
*  
*  
**/
//! Horizontal distance at which blending begins (units).
static constexpr const double NAV_DEFAULT_BLEND_DIST_START = NAV_DEFAULT_STEP_MAX_SIZE;
//! Horizontal distance at which blending is fully biased to the goal Z (units).
static constexpr const double NAV_DEFAULT_BLEND_DIST_FULL = 64.0;
