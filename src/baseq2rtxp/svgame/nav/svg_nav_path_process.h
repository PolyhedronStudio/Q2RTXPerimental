/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
* 	SVGame: Navigation Path Process
* 
* 	Reusable, per-entity path follow state with throttled rebuild + failure backoff.
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#pragma once

#include "svgame/nav/svg_nav.h"

/** 
* 	@brief	Policy for path processing and follow behavior.
**/
struct svg_nav_path_policy_t {
	/** 
	* 	Timers and backoff exponent:
	**/
	//! Minimum time interval between path rebuild attempts.
   QMTime rebuild_interval = 0_ms;
	//! Base time for exponential backoff on path rebuild failures.
	QMTime fail_backoff_base = 25_ms;
	//! Maximum exponent for backoff time (2^n).
	int32_t fail_backoff_max_pow = 4;

	/** 
	* 	Ignore/Allow States:
	**/
	//! If true, ignore visibility when deciding whether to rebuild the path. 
	//! This can be useful for testing and debugging, but generally we want the monster to 
	//! try to rebuild more aggressively when it can see the player.
	bool ignore_visibility	= true;
	//! If true, ignore distance in front of the monster when deciding whether to rebuild the path.
	bool ignore_infront		= true;

	/** 
	* 	Goal and Waypoint parameters:
	**/
	//! Radius around waypoints to consider 'reached'.
	double waypoint_radius = NAV_DEFAULT_WAYPOINT_RADIUS;

	//! 2D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_2d = NAV_DEFAULT_GOAL_REBUILD_2D_DISTANCE;
	//! 3D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_3d = NAV_DEFAULT_GOAL_REBUILD_3D_DISTANCE; // 48* 48 = 2304

	/**
	*	Goal Z-layer blending controls:
	*		When start or goal selection lands in an XY location that contains multiple
	*		valid nav layers, the selector can derive a blended "desired Z" instead of
	*		using only the local sample height.
	*
	*		This acts as a soft hint during start/goal node selection so longer horizontal
	*		paths can progressively favor layers whose elevation better matches the final
	*		objective. In practice, this helps the path builder commit earlier to stairs,
	*		ramps, or other vertical connectors when the target is known to be above or
	*		below the current floor.
	*
	*		Behavior is distance-driven:
	*		- At or below `blend_start_dist`, selection remains anchored to the local
	*		  start/query height.
	*		- Between `blend_start_dist` and `blend_full_dist`, the desired Z is blended
	*		  toward the goal height.
	*		- At or beyond `blend_full_dist`, selection is fully biased to the goal Z.
	*
	*		Disable this only for targeted diagnostics or when comparing against purely
	*		local closest-layer selection.
	**/
	//! If true, apply horizontal-distance-based blending from local/query Z toward goal Z during layer selection.
	bool enable_goal_z_layer_blend = true;
	//! Horizontal distance threshold where goal-height influence begins; shorter ranges keep selection fully local.
	double blend_start_dist = NAV_DEFAULT_BLEND_DIST_START;
	//! Horizontal distance threshold where the desired selection Z becomes fully goal-biased.
	double blend_full_dist = NAV_DEFAULT_BLEND_DIST_FULL;

	/**
	*	Cluster-route pre-pass controls:
	*		The async request worker can first derive a coarse long-range tile/cluster
	*		route from the navigation hierarchy and then use that route as an allow-list
	*		for fine-grained A* expansion.
	*
	*		This keeps long-distance searches bounded on large or open maps by reducing
	*		unnecessary node expansion outside the expected corridor, while still leaving
	*		final traversal validity to the authoritative fine traversal search.
	*
	*		Disable this only for targeted diagnostics, benchmarking, or direct comparison
	*		against unrestricted fine refinement behavior.
	**/
	//! If true, apply the hierarchical tile-route filter during async A* prep.
	bool enable_cluster_route_filter = true;

	/** 
	* 	Traversal policy controls derived from persisted edge and feature metadata.
	**/
	//! If true, forbid edges that explicitly enter water.
	bool forbid_water = true;
	//! If true, forbid edges that explicitly enter lava.
	bool forbid_lava = true;
	//! If true, forbid edges that explicitly enter slime.
	bool forbid_slime = true;
	//! If true, allow explicitly optional walk-off edges.
	bool allow_optional_walk_off = false;
	//! If true, allow explicitly forbidden walk-off edges.
	bool allow_forbidden_walk_off = false;
   //! If true, allow async A*  to skip long-hop probes that can already be predicted as passed-through one-cell chains.
	bool enable_pass_through_pruning = true;
	//! If true, prefer ladders when they reach the same or a relatively close goal height more directly.
	bool prefer_ladders = true;
	//! Cost bonus applied to ladder-favoring transitions when ladder preference is active.
	double ladder_preference_bias = 12.0;
	//! Maximum goal-height slack within which ladder endpoints are considered meaningfully close to the target height.
	double ladder_preferred_height_slack = 96.0;
	//! If true, consult sparse dynamic occupancy during async neighbor expansion.
	bool use_dynamic_occupancy = true;
	//! If true, occupancy-marked cells remain hard blockers; otherwise they are converted into additional soft cost.
	bool hard_block_dynamic_occupancy = false;
	//! Additional scale applied to sparse occupancy soft costs when occupancy participation is enabled.
	double dynamic_occupancy_soft_cost_scale = 1.0;

	/** 
	* 	For obstalce step handling.
	**/
	//! Minimal nnormal angle that will be stepped over.
	double min_step_normal = PHYS_MAX_SLOPE_NORMAL;
	//! Minimum step height that is considered as and can be stepped over.
	double min_step_height = PHYS_STEP_MIN_SIZE;
	//! Maximum step height that can be stepped over (matches `nav_max_step`).
	double max_step_height = PHYS_STEP_MAX_SIZE;

	/** 
	* 	For obstacle Jump handling:
	**/
	//! If true, try to perform a small obstacle jump when blocked.
	bool allow_small_obstruction_jump	= true;
	//! Max obstruction height allowed to jump over.
	double max_obstruction_jump_height	= NAV_DEFAULT_MAX_OBSTRUCTION_JUMP_SIZE;

	/** 
	* 	For fall-safety:
	*  
	* 	max_drop_height should be smaller than (or equal to) the cap if the cap is an absolute extremity limiter. 
	* 	Two practical choices depending on your intent:
	* 	- If you want navigation to avoid any damaging falls: set cap < ~187.6 (e.g. 180) so the nav system will never accept a drop that can hurt.
	* 	- If you want to allow risky/damaging drops but still bound them: leave cap ≈ 192 (or slightly above) and keep max_drop_height as a conservative preferred threshold (e.g. 48..96).
	**/
	//! If true, do not allow moving into a drop deeper than max_drop_height.
	bool enable_max_drop_height_cap = true;
	//! Max allowed drop height (units, matches `nav_max_drop`).
	double max_drop_height		= NAV_DEFAULT_MAX_DROP_HEIGHT;
	//! Drop cap applied when rejecting large downward transitions (matches `nav_max_drop_height_cap`).
	double max_drop_height_cap	= NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;

	/** 
	* 	Agent navigation constraints derived from nav CVars.
	**/
	//! Agent bounding box minimum extents in feet-origin space (matches `nav_agent_mins_* `).
	Vector3 agent_mins = { -16.0f, -16.0f, -36.0f };
	//! Agent bounding box maximum extents in feet-origin space (matches `nav_agent_maxs_* `).
	Vector3 agent_maxs = { 16.0f, 16.0f, 36.0f };
    //! Minimum walkable surface normal Z threshold (matches `nav_max_slope_normal_z`).
	double max_slope_normal_z = PHYS_MAX_SLOPE_NORMAL;

	/** 
	* 	Layer selection tuning:
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
	//!			multi-layer cells (stairs / overlapping floors). Smaller values allow selection
	//!			to follow the numeric closest-by-Z more aggressively.
	//! @note	Default is tuned to be slightly above a typical stair riser so the selector
	//!			remains stable on stairs while still allowing large Z deltas to select the
	//!			correct floor.
	double layer_select_prefer_z_threshold = NAV_DEFAULT_STEP_MAX_SIZE + NAV_DEFAULT_STEP_MIN_SIZE;
};

/** 
* 	@brief	Per-entity navigation path processing state.
* 	@note	Embed this in an entity to add path following behavior.
**/
struct svg_nav_path_process_t {
	/** 
	* 
	* 
	* 
	* 	Core:
	* 
	* 
	* 
	**/
	//! Current traversal path.
	nav_traversal_path_t path = {};
	//! Current index along the path.
	int32_t path_index = 0;
	//! Last path goal and start positions.
	Vector3 path_goal_position = {};
	//! Start position used for the current path.
	Vector3 path_start_position = {};
	//! Next time at which a path rebuild can be attempted.
	QMTime next_rebuild_time = 0_ms;
	//! Time until which rebuild attempts are backed off due to failures.
	QMTime backoff_until = 0_ms;
	//! Number of consecutive path rebuild failures.
	int32_t consecutive_failures = 0;
	//! Time of the last rebuild failure (used to bias A*  away from recent failing paths).
	QMTime last_failure_time = 0_ms;
	//! Last failing goal position (world-space) recorded when a rebuild failed.
	Vector3 last_failure_pos = {};
	//! Last failing direction/yaw recorded when a rebuild failed.
	float last_failure_yaw = 0.0f;
	//! Indicates whether an asynchronous rebuild request is currently outstanding.
	bool rebuild_in_progress = false;
	//! Stores the handle of the pending async rebuild request for cancellation/logging.
	int32_t pending_request_handle = 0;
	//! Monotonic request generation used to discard stale async results when requests are replaced.
	uint32_t request_generation = 0;
	//! Time of the last async prep/enqueue attempt for this process (used to debounce repeated per-frame prep work).
	QMTime last_prep_time = 0_ms;
	//! Last prep/start position used when the last async prep was performed.
	Vector3 last_prep_start = {};
	//! Last prep/goal position used when the last async prep was performed.
	Vector3 last_prep_goal = {};
	//! Z offset used to convert external feet-origin queries into internal nav-center space.
	float path_center_offset_z = 0.0f;

	/** 
	* 	@brief	Policy for path processing and follow behavior.
	**/
	void Reset( void );
	/** 
	* 	@brief	Commit a finalized async traversal path and reset failure tracking.
	* 	@param	points	Finalized nav-center waypoints produced by the async rebuild.
	* 	@param	start_origin	Agent start position in entity feet-origin space.
	* 	@param	goal_origin	Agent goal position in entity feet-origin space.
	* 	@param	center_offset_z	Z offset that converts feet-origin to nav-center coordinates for this path.
	* 	@param	policy	Path policy used to enforce rebuild throttles after commit.
	* 	@return	True when the path was stored successfully.
	**/
    const bool CommitAsyncPathFromPoints( const std::vector<Vector3> &points, const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy );



	/** 
	* 
	* 
	* 
	* 	Path (Re-)Building:
	* 
	* 
	* 
	**/
	/** 
	* 	@brief	Rebuild the path to the given goal from the given start, using the given agent
	* 			bounding box for traversal.
	**/
	const bool RebuildPathToWithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
		const Vector3 &agent_mins, const Vector3 &agent_maxs, const bool force = false );
	/** 
	* 	@brief	Used to determine if a path rebuild can be attempted at this time.
	**/
	const bool CanRebuild( const svg_nav_path_policy_t &policy ) const;
	/** 
	* 	@brief	Determine if the path should be rebuilt based on the goal's 2D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForGoal2D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const;
	/** 
	* 	@brief	Determine if the path should be rebuilt based on the goal's 3D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForGoal3D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const;
	/** 
	* 	@brief	Determine if the path should be rebuilt based on the start's 2D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForStart2D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const;
	/** 
	* 	@brief	Determine if the path should be rebuilt based on the start's 3D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForStart3D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const;
	/** 
	* 	@brief	Rebuild the path to the given goal from the given start.
	* 	@return	True if the path was successfully rebuilt.
	**/
	const bool RebuildPathTo( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy );


	/** 
	* 
	* 
	* 
	* 	Path direction/movement querying:
	* 
	* 
	* 
	**/
    /**
	* 	@brief	Shared follow-state bundle returned by `QueryFollowState`.
	* 	@note	Keeps waypoint-selection and stair-centering semantics inside navigation code so
	* 			monster callers can consume one reusable follow description.
	**/
	struct follow_state_t {
		//! True when `move_direction3d` contains a usable direction for this frame.
		bool has_direction = false;
		//! Normalized 3D direction returned by the shared path query.
		Vector3 move_direction3d = {};
		//! Active waypoint converted into feet-origin/entity space.
		Vector3 active_waypoint_origin = {};
		//! Horizontal distance from current origin to the active waypoint.
		double active_waypoint_dist2d = 0.0;
		//! Absolute vertical delta between current origin and active waypoint.
		double active_waypoint_delta_z = 0.0;
		//! True when the active waypoint should keep tighter stair/corner centering.
		bool waypoint_needs_precise_centering = false;
      //! True when the current anchor sits inside a repeated step-sized vertical corridor.
		bool stepped_vertical_corridor_ahead = false;
		//! True when the current active waypoint is the final path point.
		bool approaching_final_goal = false;
	};
	/**
	* 	@brief	Query reusable follow state for the current path.
	* 	@param	current_origin	Current feet-origin position of the mover.
	* 	@param	policy		Path-follow policy controlling waypoint radius and stair limits.
	* 	@param	out_state	[out] Shared follow-state bundle for steering/velocity code.
	* 	@return	True when a valid follow direction was produced.
	* 	@note	This centralizes waypoint advancement, active-waypoint selection, and stair-anchor
	* 			classification so monster code does not duplicate those navigation decisions.
	**/
	const bool QueryFollowState( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, follow_state_t * out_state );
	/** 
	* 	@brief	Query the next movement direction in 2D from the current origin along the path.
	**/
	const bool QueryDirection2D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 * out_dir2d );
	/** 
	* 	@brief	Query the next movement direction in 3D from the current origin along the path.
	* 	@param	current_origin	Current position of the agent.
	* 	@param	policy			Path policy (for waypoint radius).
	* 	@param	out_dir3d		[out] Resulting normalized 3D direction.
	* 	@return	True if a direction was found.
	**/
	const bool QueryDirection3D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 * out_dir3d );

	/** 
	*    @brief  Get the next path point converted into the entity's origin space (opposite of nav-center).
	*    @param  out_point   [out] Next path point in entity origin coordinates (feet-origin).
	*    @return True if a next point exists.
	**/
	const bool GetNextPathPointEntitySpace( Vector3 * out_point ) const;
};
