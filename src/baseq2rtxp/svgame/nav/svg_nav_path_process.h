/********************************************************************
*
*
*	SVGame: Navigation Path Process
*
*	Reusable, per-entity path follow state with throttled rebuild + failure backoff.
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include <vector>

/**
*	@brief	Policy for path processing and follow behavior.
**/
struct svg_nav_path_policy_t {
	/**
	*	Timers and backoff exponent:
	**/
	//! Minimum time interval between path rebuild attempts.
	QMTime rebuild_interval = 250_ms;
	//! Base time for exponential backoff on path rebuild failures.
	QMTime fail_backoff_base = 100_ms;
	//! Maximum exponent for backoff time (2^n).
	int32_t fail_backoff_max_pow = 4;

	/**
	*	Ignore/Allow States:
	**/
	bool ignore_visibility = true;
	bool ignore_infront = true;

	/**
	*	Goal and Waypoint parameters:
	**/
	//! Radius around waypoints to consider 'reached'.
	float waypoint_radius = 8.0;

	//! 2D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_2d = 48.0;
	//! 3D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_3d = 128.0; // 48 * 48 = 2304

	/**
	*    Goal Z-layer blending controls used to bias layer selection toward the goal's
	*    Z when finding start/goal nodes. This helps prefer climbing stairs or reaching
	*    an objective located on a different floor when appropriate.
	**/
	//! If true, blend the desired layer Z between start and goal when selecting layers.
	bool enable_goal_z_layer_blend = true;
	//! Horizontal distance at which blending begins (units).
	double blend_start_dist = 16.0;
	//! Horizontal distance at which blending is fully biased to the goal Z (units).
	double blend_full_dist = 128.0;

	/**
	*	For obstalce step handling.
	**/
	//! Minimal nnormal angle (degrees) that will be stepped over.
	double min_step_normal = 0.7;
	//! Minimum step height that is considered as and can be stepped over.
	double min_step_height = 1.;
	//! Maximum step height that can be stepped over (matches `nav_max_step`).
	double max_step_height = 18.0;

	/**
	*	For obstacle Jump handling:
	**/
	//! If true, try to perform a small obstacle jump when blocked.
	bool allow_small_obstruction_jump = true;
	//! Max obstruction height allowed to jump over.
	double max_obstruction_jump_height = 33.0;

	/**
	*	For fall-safety.
	**/
	//! If true, do not allow moving into a drop deeper than drop_max_height.
	bool cap_drop_height = true;
	//! Max allowed drop height (units, matches `nav_max_drop`).
	double max_drop_height = 128.0;
	//! Drop cap applied when rejecting large downward transitions (matches `nav_drop_cap`).
	double drop_cap = 64.0;

	/**
	*	Agent navigation constraints derived from nav CVars.
	**/
	//! Agent bounding box minimum extents in feet-origin space (matches `nav_agent_mins_*`).
	Vector3 agent_mins = { -16.0f, -16.0f, -36.0f };
	//! Agent bounding box maximum extents in feet-origin space (matches `nav_agent_maxs_*`).
	Vector3 agent_maxs = { 16.0f, 16.0f, 36.0f };
	//! Maximum walkable slope in degrees (matches `nav_max_slope_deg`).
	double max_slope_deg = 70.0;

	/**
	*	Layer selection tuning:
	**/
	//! If true, prefer the highest layer in a multi-layer XY cell when the desired Z is "close".
	//!
	//! @note	This is intentionally a soft bias, not an absolute rule.
	//!		It only influences selection when multiple layers are considered acceptable
	//!		by the Z-tolerance gate (see `nav_z_tolerance` / derived default tolerance).
	//!		When the desired Z is far away from all layers, selection falls back to
	//!		closest-by-Z.
	bool layer_select_prefer_top = true;
	//! Z distance in world units under which we apply the top/bottom preference bias.
	//!
	//! @note	Larger values make selection "stick" to the preferred layer more often in
	//!		multi-layer cells (stairs / overlapping floors). Smaller values allow selection
	//!		to follow the numeric closest-by-Z more aggressively.
	//! @note	Default is tuned to be slightly above a typical stair riser so the selector
	//!		remains stable on stairs while still allowing large Z deltas to select the
	//!		correct floor.
	double layer_select_prefer_z_threshold = 16.0;
};

/**
*	@brief	Per-entity navigation path processing state.
*	@note	Embed this in an entity to add path following behavior.
**/
struct svg_nav_path_process_t {
	/**
	*
	*
	*
	*	Core:
	*
	*
	*
	**/
	//! Current traversal path.
	nav_traversal_path_t path = {};
	//! Current index along the path.
	int32_t path_index = 0;
	//! Last path goal and start positions.
	Vector3 path_goal = {};
	//! Start position used for the current path.
	Vector3 path_start = {};
    //! Z offset from entity origin to nav-center used for the current stored path (path points are stored in entity origin space).
    float path_center_offset_z = 0.0f;
	//! Next time at which a path rebuild can be attempted.
	QMTime next_rebuild_time = 0_ms;
	//! Time until which rebuild attempts are backed off due to failures.
	QMTime backoff_until = 0_ms;
	//! Number of consecutive path rebuild failures.
	int32_t consecutive_failures = 0;
	//! Time of the last rebuild failure (used to bias A* away from recent failing paths).
	QMTime last_failure_time = 0_ms;
	//! Last failing goal position (world-space) recorded when a rebuild failed.
	Vector3 last_failure_pos = {};
	//! Last failing direction/yaw recorded when a rebuild failed.
	float last_failure_yaw = 0.0f;
	//! Indicates whether an asynchronous rebuild request is currently outstanding.
	bool rebuild_in_progress = false;
	//! Stores the handle of the pending async rebuild request for cancellation/logging.
	int32_t pending_request_handle = 0;

	/**
	*	@brief	Policy for path processing and follow behavior.
	**/
	void Reset( void );
	/**
	*	@brief	Commit a finalized async traversal path and reset failure tracking.
	*	@param	points	Finalized nav-center waypoints produced by the async rebuild.
	*	@param	start_origin	Agent start position in entity feet-origin space.
	*	@param	goal_origin	Agent goal position in entity feet-origin space.
	*	@param	center_offset_z	Z offset that converts feet-origin to nav-center coordinates for this path.
	*	@param	policy	Path policy used to enforce rebuild throttles after commit.
	*	@return	True when the path was stored successfully.
	**/
	const bool CommitAsyncPathFromPoints( const std::vector<Vector3> &points, const Vector3 &start_origin, const Vector3 &goal_origin, float center_offset_z, const svg_nav_path_policy_t &policy );



	/**
	*
	*
	*
	*	Path (Re-)Building:
	*
	*
	*
	**/
	/**
	*	@brief	Rebuild the path to the given goal from the given start, using the given agent
	*			bounding box for traversal.
	**/
	const bool RebuildPathToWithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
		const Vector3 &agent_mins, const Vector3 &agent_maxs, const bool force = false );
	/**
	*	@brief	Used to determine if a path rebuild can be attempted at this time.
	**/
	const bool CanRebuild( const svg_nav_path_policy_t &policy ) const;
	/**
	*	@brief	Determine if the path should be rebuilt based on the goal's 2D distance change.
	*	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForGoal2D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	*	@brief	Determine if the path should be rebuilt based on the goal's 3D distance change.
	*	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForGoal3D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	*	@brief	Determine if the path should be rebuilt based on the start's 2D distance change.
	*	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForStart2D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	*	@brief	Determine if the path should be rebuilt based on the start's 3D distance change.
	*	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForStart3D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	*	@brief	Rebuild the path to the given goal from the given start.
	*	@return	True if the path was successfully rebuilt.
	**/
	const bool RebuildPathTo( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy );


	/**
	*
	*
	*
	*	Path direction/movement querying:
	*
	*
	*
	**/
	/**
	*	@brief	Query the next movement direction in 2D from the current origin along the path.
	**/
	const bool QueryDirection2D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir2d );
	/**
	*	@brief	Query the next movement direction in 3D from the current origin along the path.
	*	@param	current_origin	Current position of the agent.
	*	@param	policy			Path policy (for waypoint radius).
	*	@param	out_dir3d		[out] Resulting normalized 3D direction.
	*	@return	True if a direction was found.
	**/
	const bool QueryDirection3D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir3d );

	/**
	*   @brief  Get the next path point converted into the entity's origin space (opposite of nav-center).
	*   @param  out_point   [out] Next path point in entity origin coordinates (feet-origin).
	*   @return True if a next point exists.
	**/
	const bool GetNextPathPointEntitySpace( Vector3 *out_point ) const;
};
