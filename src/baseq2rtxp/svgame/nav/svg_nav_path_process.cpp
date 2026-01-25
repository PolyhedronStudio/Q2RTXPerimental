/********************************************************************
*
*
*	SVGame: Navigation Path Process
*
*	Reusable, per-entity path follow state with throttled rebuild + failure backoff.
*
********************************************************************/

// Includes: local and navigation headers.
#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_path_process.h"



/**
*
*
*
*	Core:
*
*
*
**/
/**
*	@brief	Policy for path processing and follow behavior.
**/
/**
*	@brief	Reset path process state to an initial empty state.
**/
void svg_nav_path_process_t::Reset( void ) {
	/**
	*	Reset core state.
	**/
	// Free any existing stored path.
	SVG_Nav_FreeTraversalPath( &path );
	// Reset traversal indices.
	path_index = 0;
	// Reset cached goal/start.
	path_goal = {};
	path_start = {};
	// Reset throttle/backoff timers.
	next_rebuild_time = 0_ms;
	backoff_until = 0_ms;
	// Reset failure tracking.
	consecutive_failures = 0;

}

/**
*	@brief	Retrieve the next stored nav-center path point converted into entity feet-origin space.
*	@param	out_point	[out] Next point in entity feet-origin coordinates.
*	@return	True if a point exists, false if the path is empty/invalid.
**/
const bool svg_nav_path_process_t::GetNextPathPointEntitySpace( Vector3 *out_point ) const {
	/**
	*	Sanity checks.
	**/
	// Validate output pointer.
	if ( !out_point ) {
		return false;
	}
	// Require a valid path buffer.
	if ( path.num_points <= 0 || !path.points ) {
		return false;
	}

	/**
	*	Convert stored path point from nav-center space to feet-origin space.
	**/
	// Fetch the current waypoint in nav-center coordinates.
	const Vector3 navPoint = path.points[ path_index ];
	// Subtract center offset to get an entity feet-origin point.
	const Vector3 entityPoint = QM_Vector3Subtract( navPoint, Vector3{ 0.0f, 0.0f, path_center_offset_z } );
	// Output the converted point.
	*out_point = entityPoint;
	return true;
}



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
const bool svg_nav_path_process_t::RebuildPathToWithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
	const Vector3 &agent_mins, const Vector3 &agent_maxs ) {
/**
*	Debug instrumentation: print inputs and key cvars.
**/
// Debug: print agent bbox, step height, and nav mesh params.
	gi.dprintf( "[DEBUG][NavPath] RebuildPathToWithAgentBBox: start(%.1f %.1f %.1f) goal(%.1f %.1f %.1f) agent_mins(%.1f %.1f %.1f) agent_maxs(%.1f %.1f %.1f)\n",
		start_origin.x, start_origin.y, start_origin.z,
		goal_origin.x, goal_origin.y, goal_origin.z,
		agent_mins.x, agent_mins.y, agent_mins.z,
		agent_maxs.x, agent_maxs.y, agent_maxs.z );

	// NOTE: These are defined in `svg_nav.cpp` and used to warn about likely failure cases.
	extern cvar_t *nav_max_step;
	extern cvar_t *nav_max_drop;
	gi.dprintf( "[DEBUG][NavPath] nav_max_step=%.1f\n", nav_max_step ? nav_max_step->value : -1.0f );
	gi.dprintf( "[DEBUG][NavPath] nav_max_drop=%.1f\n", nav_max_drop ? nav_max_drop->value : 100.0f );

	/**
	*	Pre-check: warn if the vertical gap already exceeds step height.
	**/
	// Compute absolute Z difference between endpoints.
	const float zGap = fabsf( goal_origin.z - start_origin.z );
	// Snapshot current step limit.
	const float stepLimit = nav_max_step ? nav_max_step->value : -1.0f;
	// Emit warning if we are outside typical step traversal constraints.
	if ( stepLimit > 0.0f && zGap > stepLimit ) {
		gi.dprintf( "[WARNING][NavPath] Z gap (%.1f) between start and goal exceeds nav_max_step (%.1f)! Pathfinding will likely fail.\n", zGap, stepLimit );
	}

	/**
	*	Throttle/backoff checks.
	**/
	// Exit early if we are currently in backoff or rebuild throttle window.
	if ( !CanRebuild( policy ) ) {
		gi.dprintf( "[DEBUG][NavPath] CanRebuild() returned false, skipping path rebuild.\n" );
		return false;
	}
	// Exit early if neither goal nor start moved enough to justify rebuild.
	if ( !ShouldRebuildForGoal2D( goal_origin, policy ) && !ShouldRebuildForStart2D( start_origin, policy ) ) {
		gi.dprintf( "[DEBUG][NavPath] ShouldRebuildForGoal2D/Start2D returned false, skipping path rebuild.\n" );
		return false;
	}

	/**
	*	Preserve old state so we can roll back on failure.
	**/
	// Keep a copy of the existing path (and index) in case the new path fails.
	nav_traversal_path_t oldPath = path;
	const int32_t oldIndex = path_index;

	// Clear current path state prior to attempting rebuild.
	path = {};
	path_index = 0;

	/**
	*	Convert entity-space bbox/origins into the nav-center space expected by the generator.
	**/
	// Compute center offset (feet-origin -> center-origin).
	const float centerOffsetZ = ( agent_mins.z + agent_maxs.z ) * 0.5f;
	// Convert mins/maxs into center-origin space.
	const Vector3 agent_center_mins = QM_Vector3Subtract( agent_mins, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
	const Vector3 agent_center_maxs = QM_Vector3Subtract( agent_maxs, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
	// Convert start/goal into center-origin space.
	const Vector3 start_center = QM_Vector3Add( start_origin, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
	const Vector3 goal_center = QM_Vector3Add( goal_origin, Vector3{ 0.0f, 0.0f, centerOffsetZ } );

	/**
	*	Run navigation query to rebuild path using explicit bbox.
	**/
	const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox(
		start_center,
		goal_center,
		&path,
		agent_center_mins,
		agent_center_maxs,
		true,
		256.0f,
		512.0f );
	gi.dprintf( "[DEBUG][NavPath] SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox returned %d\n", ok ? 1 : 0 );

	/**
	*	Enforce drop limit post-process: reject paths that contain large downward steps.
	**/
	bool dropLimitOk = true;
	const float maxDrop = nav_max_drop ? nav_max_drop->value : 100.0f;
	if ( ok && path.num_points > 1 && path.points ) {
		// Scan each segment for excessive vertical drop.
		for ( int32_t i = 1; i < path.num_points; ++i ) {
			// Positive dz here means a drop (previous higher than current).
			const float dz = path.points[ i - 1 ].z - path.points[ i ].z;
			if ( dz > maxDrop ) {
				gi.dprintf( "[WARNING][NavPath] Drop between path points %d->%d is %.1f units (exceeds max allowed %.1f), path rejected.\n", i - 1, i, dz, maxDrop );
				dropLimitOk = false;
				break;
			}
		}
	}

	/**
	*	Commit success or revert on failure.
	**/
	if ( ok && dropLimitOk ) {
		// Free old path now that we have a valid replacement.
		SVG_Nav_FreeTraversalPath( &oldPath );
		// Store feet-origin start/goal (callers interact in entity space).
		path_start = start_origin;
		path_goal = goal_origin;
		// Persist center offset to convert between feet-origin and nav-center.
		path_center_offset_z = centerOffsetZ;
		// Reset failure tracking.
		consecutive_failures = 0;
		backoff_until = 0_ms;
		// Throttle subsequent rebuilds.
		next_rebuild_time = level.time + policy.rebuild_interval;
		return true;
	}

	// New path failed (or rejected). Free it and restore the old one.
	SVG_Nav_FreeTraversalPath( &path );
	path = oldPath;
	path_index = oldIndex;

	/**
	*	Failure backoff: increase delay before next rebuild attempt.
	**/
	consecutive_failures++;
	const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
	const QMTime extra = QMTime::FromMilliseconds( ( int32_t )policy.fail_backoff_base.Milliseconds() * ( 1 << powN ) );
	backoff_until = level.time + extra;
	next_rebuild_time = std::max( next_rebuild_time, backoff_until );
	gi.dprintf( "[DEBUG][NavPath] Pathfinding failed, consecutive_failures=%d, backoff_until=%lld\n", consecutive_failures, ( long long )backoff_until.Milliseconds() );
	return false;
}
/**
*	@brief	Used to determine if a path rebuild can be attempted at this time.
**/
const bool svg_nav_path_process_t::CanRebuild( const svg_nav_path_policy_t &policy ) const {
	/**
	*	Sanity: currently policy is not used, but we keep it for future expansion.
	**/
	( void )policy;
	// Rebuild is permitted only if we are past both throttle and backoff times.
	return level.time >= next_rebuild_time && level.time >= backoff_until;
}
/**
*	@brief	Determine if the path should be rebuilt based on the goal's 2D distance change.
*	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForGoal2D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const {
	/**
	*	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/**
	*	Check goal movement in 2D.
	**/
	// Compute XY delta between current goal and cached goal.
	const Vector3 d = QM_Vector3Subtract( goal_origin, path_goal );
	// Only consider XY for rebuild thresholds.
	const float d2 = ( d.x * d.x ) + ( d.y * d.y );
	// Trigger rebuild when squared distance exceeds threshold squared.
	return d2 > ( policy.rebuild_goal_dist_2d * policy.rebuild_goal_dist_2d );
}
/**
*	@brief	Determine if the path should be rebuilt based on the start's 2D distance change.
* 	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForStart2D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const {
	/**
	*	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/**
	*	Check start movement in 2D.
	**/
	// Use the same XY threshold as goal unless tuned separately.
	const Vector3 d = QM_Vector3Subtract( start_origin, path_start );
	const float d2 = ( d.x * d.x ) + ( d.y * d.y );
	return d2 > ( policy.rebuild_goal_dist_2d * policy.rebuild_goal_dist_2d );
}
/**
*	@brief	Rebuild the path to the given goal from the given start.
* 	@return	True if the path was successfully rebuilt.
**/
const bool svg_nav_path_process_t::RebuildPathTo( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) {
	/**
	*	Throttle/backoff checks.
	**/
	// Do not attempt rebuild if we are still throttled or in backoff.
	if ( !CanRebuild( policy ) ) {
		return false;
	}
	// Do not attempt rebuild if neither goal nor start has moved enough.
	if ( !ShouldRebuildForGoal2D( goal_origin, policy ) && !ShouldRebuildForStart2D( start_origin, policy ) ) {
		return false;
	}

	nav_traversal_path_t oldPath = path;
	const int32_t oldIndex = path_index;

	// Reset current path container prior to rebuild.
	path = {};
	path_index = 0;

	/**
	*	Convert feet-origin start/goal to nav-center space using the active mesh agent bbox.
	**/
	float centerOffsetZ = 0.0f;
	Vector3 start_query = start_origin;
	Vector3 goal_query = goal_origin;
	if ( g_nav_mesh ) {
		// Compute center offset from mesh agent bounds.
		centerOffsetZ = ( g_nav_mesh->agent_mins.z + g_nav_mesh->agent_maxs.z ) * 0.5f;
		// Convert start and goal into center-origin coordinates expected by nav.
		start_query = QM_Vector3Add( start_origin, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
		goal_query = QM_Vector3Add( goal_origin, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
	}

	/**
	*	Generate a new traversal path.
	**/
	const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx( start_query, goal_query, &path, true, 256.0f, 512.0f );
	if ( ok ) {
		// Replace old path with new path.
		SVG_Nav_FreeTraversalPath( &oldPath );
		// Store feet-origin start/goal for rebuild heuristics.
		path_start = start_origin;
		path_goal = goal_origin;
		// Store center offset for later conversion during query.
		path_center_offset_z = centerOffsetZ;
		// Reset backoff and failures.
		consecutive_failures = 0;
		backoff_until = 0_ms;
		// Throttle subsequent rebuild attempts.
		next_rebuild_time = level.time + policy.rebuild_interval;
		return true;
	}
	// Failed: restore old path.
	SVG_Nav_FreeTraversalPath( &path );

	// Restore old path on failure.
	path = oldPath;
	path_index = oldIndex;

	//
	consecutive_failures++;
	const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
	const QMTime extra = QMTime::FromMilliseconds( ( int32_t )policy.fail_backoff_base.Milliseconds() * ( 1 << powN ) );
	backoff_until = level.time + extra;
	next_rebuild_time = std::max( next_rebuild_time, backoff_until );
	return false;
}



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
const bool svg_nav_path_process_t::QueryDirection2D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir2d ) {
	/**
	*	Sanity checks: require a valid output pointer and a valid path buffer.
	**/
	// Validate output pointer.
	if ( !out_dir2d ) {
		return false;
	}
	// Require a path to query.
	if ( path.num_points <= 0 || !path.points ) {
		return false;
	}

	/**
	*	Convert feet-origin to nav-center space for query.
	**/
	const Vector3 query_origin = QM_Vector3Add( current_origin, Vector3{ 0.0f, 0.0f, path_center_offset_z } );

	/**
	*	Query next direction and advance waypoints.
	**/
	Vector3 dir = {};
	int32_t idx = path_index;
	const bool ok = SVG_Nav_QueryMovementDirection( &path, query_origin, policy.waypoint_radius, &idx, &dir );

	/**
	*	Sanity: do not update state if the query failed.
	**/
	if ( !ok ) {
		return false;
	}

	/**
	*	Commit advanced index returned by the query.
	**/
	path_index = idx;

	/**
	*	Convert to 2D direction and validate magnitude before normalizing.
	**/
	// Ignore Z for 2D direction.
	dir.z = 0.0f;
	// Compute squared length (XY only).
	const float len2 = ( dir.x * dir.x ) + ( dir.y * dir.y );
	// Guard against normalizing near-zero vectors.
	if ( len2 <= ( 0.001f * 0.001f ) ) {
		return false;
	}

	/**
	*	Output normalized direction.
	**/
	*out_dir2d = QM_Vector3Normalize( dir );
	return true;
}

/**
*	@brief	Query the next movement direction in 3D from the current origin along the path.
*	@param	current_origin	Current position of the agent.
*	@param	policy			Path policy (for waypoint radius).
*	@param	out_dir3d		[out] Resulting normalized 3D direction.
*	@return	True if a direction was found.
**/
const bool svg_nav_path_process_t::QueryDirection3D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir3d ) {
	/**
	*	Sanity checks: require output pointer and a valid path.
	**/
	// Validate output pointer.
	if ( !out_dir3d ) {
		return false;
	}
	// Require a path to query.
	if ( path.num_points <= 0 || !path.points ) {
		return false;
	}

	/**
	*	Convert feet-origin to nav-center space for query.
	**/
	const Vector3 query_origin = QM_Vector3Add( current_origin, Vector3{ 0.0f, 0.0f, path_center_offset_z } );

	/**
	*	Query direction while advancing waypoints in 2D, but output a 3D direction.
	**/
	Vector3 dir = {};
	int32_t idx = path_index;
	const bool ok = SVG_Nav_QueryMovementDirection_Advance2D_Output3D( &path, query_origin, policy.waypoint_radius, &idx, &dir );

	/**
	*	Sanity: do not update state if the query failed.
	**/
	if ( !ok ) {
		return false;
	}

	/**
	*	Commit advanced index returned by the query.
	**/
	path_index = idx;

	/**
	*	Validate magnitude before normalizing.
	**/
	const float len2 = ( dir.x * dir.x ) + ( dir.y * dir.y ) + ( dir.z * dir.z );
	if ( len2 <= ( 0.001f * 0.001f ) ) {
		return false;
	}

	/**
	*	Output normalized 3D direction.
	**/
	*out_dir3d = QM_Vector3Normalize( dir );
	return true;
}
