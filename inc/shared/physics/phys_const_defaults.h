/********************************************************************
*
*
*   Constant(-Expression)s for default physics values, such as gravity, friction, etc.
*
*
********************************************************************/
#pragma once


/**
* 
* 
*	Default values used for physics and obstruction step handling. These are used as defaults for the player move as well as 
*	for the monster movement and navigation systems. For the later, these defaults can be overridden on a per-move basis via 
*	the svg_nav_path_policy_t struct, which is used to communicate navigation constraints and settings from the 
*	navigation system to the movement system.
*
*
**/
//! The 'beating heart' of the physics system: the default gravity value applied to entities when not overridden by a level or entity-specific setting. 
//! Units are world units per second squared (e.g., 800 units/s^2 is a common Quake gravity).
static double constexpr PHYS_DEFAULT_GRAVITY = 800.0;



/**
* 
*	Stair Step Configuration Defaults:
* 
*
*	These values are used to determine whether a given step up/down, is traversable based 
*	the (agent/player)'s capabilities and the geometry of the step.
* 
*	Brushes that have a step height difference in the Z axis between PM_STEP_MIN_SIZE and PM_STEP_MAX_SIZE,
*	and have a normal of (0, 0, 1) within a certain tolerance, will be considered 'stair steps' and will
*	trigger special handling in the movement code to allow the 'Agent Entity' to step up them smoothly without
*	needing to jump.
*
*	The PM_STEP_GROUND_DIST is used to ensure that the 'Agent Entity' is close enough to the ground after stepping up,
*	which helps prevent issues with small steps or uneven terrain.
* 
**/
//! Minimal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr double PHYS_STEP_MIN_SIZE = 2.f;
//! Maximal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr double PHYS_STEP_MAX_SIZE = 18.f;
//! This defines the maximum step height that will be smoothed out more, by doubling the stair_step_delta, 
//! which results in a faster lerp and thus smoother appearance for smaller steps. 
//! The value of 15.f is chosen based on typical step heights in the game, 
//! but can be adjusted as needed for better visual results.
static constexpr double PHYS_STEP_SMALL_SIZE = 15.;
//! Offset for distance to account for between step and ground.
static constexpr double PHYS_STEP_GROUND_DIST = 0.25f;



/**
*
*
*	Default '(Nav-)Agent Entity' Bounding Boxes:
*
*	Mins and maxs are in nav-center space, meaning the agent's origin is at the center of its bounding box at the feet level.
*	This allows for more intuitive movement and collision handling, as the origin is at a consistent point relative to the ground, 
*	regardless of the agent's stance (standing, crouching, etc.). 
*
*	The view height is then defined as an offset from this origin, which simplifies calculations for things like camera positioning 
*	and line-of-sight checks.
*
**/
/**
*	For when the agent is in its 'regular' stance, standing straight up.
**/
//! The minimal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_STANDUP_MINS = { -16.f, -16.f, -36.f };
//! The maximal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_STANDUP_MAXS = { 16.f, 16.f, 36.f };
//! The view height offset relative from the origin for the agent's viewpoint when standing up.
static constexpr double  PHYS_DEFAULT_VIEWHEIGHT_STANDUP = 30.f;

/**
*	For when the agent is actively crouching.
**/
//! The minimal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_DUCKED_MINS = { -16.f, -16.f, -36.f };
//! The maximal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_DUCKED_MAXS = { 16.f, 16.f, 8.f };
//! The view height offset relative to the origin for the agent's viewpoint when ducked up.
static constexpr double  PHYS_DEFAULT_VIEWHEIGHT_DUCKED = 4.f;

/**
*	For when the agent is spectating / noclipping.
**/
//! The minimal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_FLYING_MINS = { -8.f, -8.f, -8.f };
//! The maximal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_FLYING_MAXS = { 8.f, 8.f, 8.f };
//! The view height offset relative to the origin for the agent's viewpoint when flying.
static constexpr double  PHYS_DEFAULT_VIEWHEIGHT_FLYING = 0.f;

/**
*	For when the agent is "gibbed" out. ( Splattered to pieces, e.g., by a rocket explosion.
**/
//! The minimal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_GIBBED_MINS = { -16.f, -16.f, 0.f };
//! The maximal bounding box for the agent, relative to its origin at the center of its feet.
static constexpr Vector3 PHYS_DEFAULT_BBOX_GIBBED_MAXS = { 16.f, 16.f, 24.f };
//! The view height offset relative to the origin for the agent's viewpoint when 'gibbed out'.
static constexpr double  PHYS_DEFAULT_VIEWHEIGHT_GIBBED = 8.f;