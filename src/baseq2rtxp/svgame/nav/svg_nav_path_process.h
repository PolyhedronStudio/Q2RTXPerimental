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

/**
*	@brief	Policy for path processing and follow behavior.
**/
struct svg_nav_path_policy_t {
	//! Radius around waypoints to consider 'reached'.
	float waypoint_radius = 32.0f;
	//! 2D distance change in goal position that triggers a path rebuild.
	float rebuild_goal_dist_2d = 48.0f;
	//! 3D distance change in goal position that triggers a path rebuild.
	float rebuild_goal_dist_3d = 2304.0f; // 48 * 48 = 2304
	//! Minimum time interval between path rebuild attempts.
	QMTime rebuild_interval = 500_ms;
	//! Base time for exponential backoff on path rebuild failures.
	QMTime fail_backoff_base = 250_ms;
	//! Maximum exponent for backoff time (2^n).
	int32_t fail_backoff_max_pow = 6;
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

	/**
	*	@brief	Policy for path processing and follow behavior.
	**/
	void Reset( void );



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
		const Vector3 &agent_mins, const Vector3 &agent_maxs );
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
	*	@brief	Determine if the path should be rebuilt based on the start's 2D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForStart2D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const;	/**
	*	@brief	Rebuild the path to the given goal from the given start.
	* 	@return	True if the path was successfully rebuilt.
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
