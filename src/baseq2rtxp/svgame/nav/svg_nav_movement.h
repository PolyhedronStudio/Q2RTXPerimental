/********************************************************************
*
*
*	SVGame: Navigation Movement Helpers
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"



/**
*
*
*	Navigation Movement Policy:
*
*
**/
/**
*	@brief	Policy describing how an entity should follow a nav path.
**/
typedef struct nav_follow_policy_s {
	//! Waypoint radius for path advancement.
	float waypoint_radius = 16.0f;
	//! Rebuild when goal moved more than this distance (3D).
	float rebuild_goal_dist_3d = 64.0f;
	//! Rebuild no more often than this.
	QMTime rebuild_interval = 500_ms;

	//! If true, do not allow moving into a drop deeper than drop_max_height.
	bool avoid_large_drops = true;
	//! Max allowed drop height (units).
	float drop_max_height = 128.0f;

	//! If true, try to perform a small obstacle jump when blocked.
	bool allow_small_obstruction_jump = true;
	//! Max obstruction height allowed to jump over.
	float max_obstruction_jump_height = 33.0f;
} nav_follow_policy_t;



/**
*
*
*	Navigation Follow State:
*
*
**/
/**
*	@brief	Reusable follow state that can be embedded in an entity.
**/
typedef struct nav_follow_state_s {
	//! Current traversal path.
	nav_traversal_path_t path = {};
	//! Index into the path waypoints.
	int32_t path_index = 0;
	//! Current goal origin for the path.
	Vector3 path_goal = {};
	//! Next allowed time to rebuild the path.
	QMTime next_rebuild_time = 0_ms;

	// --- Z Layer Blend Configuration (per-entity) ---
	//! If true, layer selection for start/goal nodes will bias toward goal Z when far away.
	bool enable_goal_z_layer_blend = true;
	//! XY distance at which we start blending start_z -> goal_z.
	float goal_z_blend_start_dist = 256.0f;
	//! XY distance at which we fully prefer goal_z.
	float goal_z_blend_full_dist = 512.0f;
} nav_follow_state_t;



/**
*
*
*	Navigation Follow Helpers:
*
*
**/
/**
*	@brief	Checks whether the follow state should rebuild its nav path to the given goal.
**/
const bool SVG_NavMovement_ShouldRebuildPath( const nav_follow_state_t &state, const Vector3 &goal_origin, const nav_follow_policy_t &policy );

/**
*	@brief	Rebuilds a traversal path if the policy allows it (throttled by next_rebuild_time).
*	@return	True if a rebuild occurred and a path was produced.
**/
const bool SVG_NavMovement_RebuildPathIfNeeded( nav_follow_state_t *state, const Vector3 &start_origin, const Vector3 &goal_origin, const nav_follow_policy_t &policy );

/**
*	@brief	Returns a 3D direction for following the current path.
*	@note	Waypoints are advanced in 2D to avoid stairs/Z-quant issues.
**/
const bool SVG_NavMovement_QueryFollowDirection( const nav_follow_state_t &state, const Vector3 &current_origin, const nav_follow_policy_t &policy, Vector3 *out_dir3d );

/**
*	@brief	Performs a conservative drop test ahead of the entity.
*	@return	True if it is safe to proceed.
**/
const bool SVG_NavMovement_IsDropSafe( const svg_base_edict_t *ent, const Vector3 &next_origin, const nav_follow_policy_t &policy );

/**
*	@brief	Attempts a small obstruction jump by applying an upward velocity request.
*	@note	This is intended to be used when blocked, and only for navigation-adjacent obstruction.
**/
bool SVG_NavMovement_TrySmallObstructionJump( const svg_base_edict_t *ent, const Vector3 &move_dir_xy, const nav_follow_policy_t &policy, Vector3 *inout_velocity );
