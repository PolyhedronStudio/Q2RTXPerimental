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
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_debug.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_traversal.h"

extern cvar_t *nav_max_step;
extern cvar_t *nav_drop_cap;
extern cvar_t *nav_debug_draw;

/**
*	@brief    Build a traversal policy mirroring the provided agent profile.
*	@param    profile    Agent hull/extents and traversal limits.
*	@return   Policy whose bounds (bbox, steps, drops, slope) match the profile.
**/
static svg_nav_path_policy_t SVG_Nav_BuildPolicyFromAgentProfile( const nav_agent_profile_t &profile ) {
    /**
    *	Copy profile data into the policy so any later modifications can layer
    *	on top without mutating the original profile.
    **/
    svg_nav_path_policy_t policy = {};
    policy.agent_mins = profile.mins;
    policy.agent_maxs = profile.maxs;
    policy.max_step_height = profile.max_step_height;
    policy.max_drop_height = profile.max_drop_height;
    policy.drop_cap = profile.drop_cap;
    policy.max_slope_deg = profile.max_slope_deg;
    return policy;
}

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
	last_failure_time = 0_ms;
	last_failure_pos = {};
	last_failure_yaw = 0.0f;
}

/**
 *	@brief	Commit a finalized async traversal path and reset failure tracking.
 *	@param	points	Finalized nav-center waypoints produced by the async rebuild.
 *	@param	start_origin	Agent start position in entity feet-origin space.
 *	@param	goal_origin	Agent goal position in entity feet-origin space.
 *	@param	center_offset_z	Z offset that converts feet-origin to nav-center coordinates for this path.
 *	@param	policy	Path policy informing throttle timing after commit.
 *	@return	True when the path was stored successfully.
 **/
const bool svg_nav_path_process_t::CommitAsyncPathFromPoints( const std::vector<Vector3> &points, const Vector3 &start_origin,
	const Vector3 &goal_origin, float center_offset_z, const svg_nav_path_policy_t &policy ) {
	/**
	 *	Validate the incoming waypoint list.
	 **/
	if ( points.empty() ) {
		return false;
	}

	/**
	 *	Copy the waypoints into a traversal path owned by this process.
	 **/
	const int32_t pointCount = ( int32_t )points.size();
	nav_traversal_path_t newPath = {};
	newPath.num_points = pointCount;
	newPath.points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * newPath.num_points, TAG_SVGAME_LEVEL );
	memcpy( newPath.points, points.data(), sizeof( Vector3 ) * newPath.num_points );

	/**
	 *	Update stored path metadata and reset failure state.
	 **/
	SVG_Nav_FreeTraversalPath( &path );
	path = newPath;
	path_index = 0;
	path_start = start_origin;
	path_goal = goal_origin;
	path_center_offset_z = center_offset_z;
	consecutive_failures = 0;
	backoff_until = 0_ms;
	next_rebuild_time = level.time + policy.rebuild_interval;
	return true;
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
*	@param	start_origin	Agent start position in entity feet-origin space.
*	@param	goal_origin	Agent goal position in entity feet-origin space.
*	@param	policy		Path policy controlling rebuild cadence and traversal constraints.
*	@param	agent_mins	Agent bbox mins in entity feet-origin space.
*	@param	agent_maxs	Agent bbox maxs in entity feet-origin space.
*	@param	force		If true, bypass rebuild throttles/backoff and movement heuristics.
*	@return	True if a new path was successfully built and committed.
*	@note	This function converts feet-origin inputs into the nav-center space expected by
*			voxelmesh traversal/pathfinding.
*	@note	Drop safety is enforced both during traversal edge validation and as a post-pass
*			on the resulting waypoint list.
**/
const bool svg_nav_path_process_t::RebuildPathToWithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, const bool force ) {
	/**
	*	Throttle debug spam:
	*		Callers may invoke this every frame. Only emit verbose debug when we are
	*		actually allowed to attempt a rebuild (or forced).
	**/
	const bool canAttemptRebuild = force || CanRebuild( policy );
	const bool doVerboseDebug = canAttemptRebuild;
	/**
	*	Debug instrumentation:
	*		Emit input parameters and key cvar-derived constraints.
	**/
	if ( doVerboseDebug ) {
		gi.dprintf( "[DEBUG][NavPath] RebuildPathToWithAgentBBox: start(%.1f %.1f %.1f) goal(%.1f %.1f %.1f) agent_mins(%.1f %.1f %.1f) agent_maxs(%.1f %.1f %.1f)\n",
			start_origin.x, start_origin.y, start_origin.z,
			goal_origin.x, goal_origin.y, goal_origin.z,
			agent_mins.x, agent_mins.y, agent_mins.z,
			agent_maxs.x, agent_maxs.y, agent_maxs.z );
	}

	// NOTE: These are defined in `svg_nav.cpp` and used to warn about likely failure cases.

	if ( doVerboseDebug ) {
		gi.dprintf( "[DEBUG][NavPath] nav_max_step=%.1f\n", nav_max_step ? nav_max_step->value : -1.0f );
		gi.dprintf( "[DEBUG][NavPath] nav_drop_cap=%.1f\n", nav_drop_cap ? nav_drop_cap->value : 100.0f );
	}

	/**
	*    Align the working policy with the runtime agent profile.
	**/
	const nav_agent_profile_t agentProfile = SVG_Nav_BuildAgentProfileFromCvars();
	svg_nav_path_policy_t resolvedPolicy = policy;
	const svg_nav_path_policy_t profilePolicy = SVG_Nav_BuildPolicyFromAgentProfile( agentProfile );
	resolvedPolicy.agent_mins = profilePolicy.agent_mins;
	resolvedPolicy.agent_maxs = profilePolicy.agent_maxs;
	resolvedPolicy.max_step_height = profilePolicy.max_step_height;
	resolvedPolicy.max_drop_height = profilePolicy.max_drop_height;
	resolvedPolicy.drop_cap = profilePolicy.drop_cap;
	resolvedPolicy.max_slope_deg = profilePolicy.max_slope_deg;

	/**
	*	Pre-check: warn if the vertical gap already exceeds the effective step height.
	*		This is diagnostic only; A* can still find a route via ramps/stairs.
	**/
	// Compute absolute Z difference between endpoints.
	const float zGap = fabsf( goal_origin.z - start_origin.z );
	// Determine effective step limit to use for the pre-check. Prefer the
	// per-profile max_step_height when it is positive, otherwise fall back
	// to the global `nav_max_step` cvar so agent tuning is not lost.
	const float stepLimit = ( resolvedPolicy.max_step_height > 0.0 )
		? ( float )resolvedPolicy.max_step_height
		: ( nav_max_step ? nav_max_step->value : -1.0f );
   // Emit warning if we are outside typical step traversal constraints.
	const bool exceedsStepLimit = ( stepLimit > 0.0f && zGap > stepLimit );
	if ( doVerboseDebug && exceedsStepLimit ) {
		gi.dprintf( "[WARNING][NavPath] Z gap (%.1f) between start and goal exceeds nav_max_step (%.1f)! Pathfinding will likely fail.\n", zGap, stepLimit );
	}	
	/**
	* 	Track whether we already have a usable path.
	* 		This lets us treat throttle/backoff skips as "no rebuild needed"
	* 		instead of "pathing failure" so callers can keep following the old path.
	**/
	const bool hasExistingPath = ( path.num_points > 0 && path.points );
 /**
	*    Guard: avoid expensive A* attempts when the required climb is outright impossible.
	*    Continually retrying such rebuilds was the root cause of the hitch spam.
	**/
	if ( exceedsStepLimit ) {
		/**
		*    Throttle impossible vertical gaps:
		*        When no path exists and the endpoint Z gap exceeds our step limit,
		*        schedule a backoff delay so we do not re-attempt every frame.
		**/
		if ( !hasExistingPath ) {
			// Record failure timing and signature for per-entity penalties.
			last_failure_time = level.time;
			last_failure_pos = goal_origin;
			last_failure_yaw = QM_Vector3ToYaw( QM_Vector3Subtract( goal_origin, start_origin ) );
			// Increment failure count to scale backoff when repeated.
			consecutive_failures = std::min( consecutive_failures + 1, policy.fail_backoff_max_pow );
			// Apply exponential backoff based on policy settings.
			const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
			const QMTime extra = QMTime::FromMilliseconds( ( int32_t )policy.fail_backoff_base.Milliseconds() * ( 1 << powN ) );
			backoff_until = level.time + extra;
			next_rebuild_time = std::max( next_rebuild_time, backoff_until );
		}
		return hasExistingPath;
	}



	/**
	*	Throttle/backoff checks:
	*		Normally we enforce both the per-policy rebuild interval and any failure
	*		backoff to avoid repeated expensive attempts.
	*
	*		We allow bypass in two cases:
	*			1) No existing path: allow an initial acquisition attempt.
	*			2) Major goal movement: allow reaction to rapid target relocation.
	**/
	/**
	*	Determine whether we should bypass throttle/backoff:
	*		- If no existing path exists, always allow one acquisition attempt.
	*		- If the goal moved far enough in Z (e.g. upstairs), allow an attempt even
	*		  during backoff so we can react to floor transitions.
	**/
	//const bool hasExistingPath = ( path.num_points > 0 && path.points );
	const float goalMovedZ = fabsf( goal_origin.z - path_goal.z );
	const float forceZThreshold = ( stepLimit > 0.0f ) ? ( stepLimit * 2.0f ) : 36.0f;
	const bool forceForGoalZ = ( goalMovedZ >= forceZThreshold );
	// Only bypass movement heuristics when the goal moved floors; do not bypass time-based backoff.
	const bool throttleBypass = forceForGoalZ;

	/**
	*	Throttle/backoff enforcement:
	*		Always enforce time-based throttles unless explicitly forced.
	*		Even without an existing path we must not rebuild every frame, otherwise we
	*		spam logs and stall the game when a route is impossible.
	**/
	if ( !force && !canAttemptRebuild ) {
		/**
		* 	Skip rebuilds while throttled/backing off.
		* 		If we already have a path, keep using it instead of reporting failure.
		**/
		return hasExistingPath;
	}
	/**
	*	Movement heuristic:
	*		If neither the goal nor the start moved enough (based on policy thresholds),
	*		skip the rebuild to avoid wasting CPU.
	**/
	const bool movementWarrantsRebuild =
		ShouldRebuildForGoal2D( goal_origin, policy )
		|| ShouldRebuildForGoal3D( goal_origin, policy )
		|| ShouldRebuildForStart2D( start_origin, policy )
		|| ShouldRebuildForStart3D( start_origin, policy );
	if ( !force && !hasExistingPath && !movementWarrantsRebuild ) {
		/**
		* 	No path exists and movement doesn't warrant rebuilding.
		* 		Report "no path" to the caller so it can fallback appropriately.
		**/
		return false;
	}
	if ( !force && hasExistingPath && !throttleBypass && !movementWarrantsRebuild ) {
		/**
		* 	Existing path is still considered valid and the goal hasn't moved enough.
		* 		Return true so callers keep following the current path.
		**/
		return true;
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
	*	Convert entity-space bbox/origins into the nav-center space expected by the traversal.
	*		The nav system stores/queries points in a hull-center coordinate frame.
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
	*		We pass `this` so A* can apply per-entity failure penalties.
	**/
	const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox(
	    start_center,
	    goal_center,
	    &path,
	    agent_center_mins,
	    agent_center_maxs,
	    &resolvedPolicy,
	    resolvedPolicy.enable_goal_z_layer_blend,
	    resolvedPolicy.blend_start_dist,
	    resolvedPolicy.blend_full_dist,
	    this );
	// Note: pass path-process (this) into the traversal generator to allow
	// A* to use recent failure/backoff timing for per-entity failure penalties.
	gi.dprintf( "[DEBUG][NavPath] SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox returned %d\n", ok ? 1 : 0 );

	/**
	*	Drop-limit post-process:
	*		Reject paths that contain large downward transitions.
	*		Even if an edge is technically traversable, large drops can be undesirable for AI.
	**/
	bool dropLimitOk = true;
	// Determine the effective max drop to enforce on paths. Prefer policy settings when enabled,
	// otherwise fall back to the global nav_drop_cap cvar.
	/**
	*    Determine drop rejection limit:
	*        Prefer an explicit policy drop cap when positive, otherwise use the
	*        per-policy max drop when dropping is capped. Fall back to the global
	*        drop cap when no policy value is configured.
	**/
	const float maxDrop = ( resolvedPolicy.drop_cap > 0.0f )
	    ? ( float )resolvedPolicy.drop_cap
	    : ( resolvedPolicy.cap_drop_height ? ( float )resolvedPolicy.max_drop_height : ( nav_drop_cap ? nav_drop_cap->value : 100.0f ) );
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
	*	Failure backoff:
	*		Increase delay before next rebuild attempt and record a failure signature
	*		for A* penalties (avoid repeating the same failing approach).
	**/
    consecutive_failures++;
    // Record failure time and signature for per-entity A* penalties.
    last_failure_time = level.time;
    last_failure_pos = goal_origin;
    // Record a coarse facing yaw from start->goal for signature matching.
    Vector3 toGoal = QM_Vector3Subtract( goal_origin, start_origin );
    last_failure_yaw = QM_Vector3ToYaw( toGoal );
	const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
	const QMTime extra = QMTime::FromMilliseconds( ( int32_t )policy.fail_backoff_base.Milliseconds() * ( 1 << powN ) );
	backoff_until = level.time + extra;
	next_rebuild_time = std::max( next_rebuild_time, backoff_until );
	gi.dprintf( "[DEBUG][NavPath] Pathfinding failed, consecutive_failures=%d, backoff_until=%lld\n", consecutive_failures, ( long long )backoff_until.Milliseconds() );
	/**
	* 	Failure outcome:
	* 		If we restored a previous path, report success so callers can keep using it.
	* 		If we have no usable path, report failure to trigger fallbacks.
	**/
	return ( path.num_points > 0 && path.points );
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
*	@brief	Determine if the path should be rebuilt based on the goal's 3D distance change.
*	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForGoal3D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const {
	/**
	*	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/**
	*	Guard: treat non-positive thresholds as disabled.
	**/
	if ( policy.rebuild_goal_dist_3d <= 0.0 ) {
		return false;
	}

	/**
	*	Check goal movement in 3D.
	**/
	// Compute XYZ delta between current goal and cached goal.
	const Vector3 d = QM_Vector3Subtract( goal_origin, path_goal );
	// Compare squared 3D distance against squared threshold.
	const double d2 = (double)d.x * (double)d.x + (double)d.y * (double)d.y + (double)d.z * (double)d.z;
	const double thresh2 = policy.rebuild_goal_dist_3d * policy.rebuild_goal_dist_3d;
	return d2 > thresh2;
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
*	@brief	Determine if the path should be rebuilt based on the start's 3D distance change.
*	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForStart3D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const {
	/**
	*	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/**
	*	Guard: treat non-positive thresholds as disabled.
	**/
	if ( policy.rebuild_goal_dist_3d <= 0.0 ) {
		return false;
	}

	/**
	*	Check start movement in 3D.
	**/
	// Compute XYZ delta between current start and cached start.
	const Vector3 d = QM_Vector3Subtract( start_origin, path_start );
	// Compare squared 3D distance against squared threshold.
	const double d2 = (double)d.x * (double)d.x + (double)d.y * (double)d.y + (double)d.z * (double)d.z;
	const double thresh2 = policy.rebuild_goal_dist_3d * policy.rebuild_goal_dist_3d;
	return d2 > thresh2;
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
	if (	!ShouldRebuildForGoal2D( goal_origin, policy )
		&& !ShouldRebuildForGoal3D( goal_origin, policy )
		&& !ShouldRebuildForStart2D( start_origin, policy )
		&& !ShouldRebuildForStart3D( start_origin, policy ) ) {
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
    const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx( start_query, goal_query, &path, policy.enable_goal_z_layer_blend, policy.blend_start_dist, policy.blend_full_dist );
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
	const bool ok = SVG_Nav_QueryMovementDirection( &path, query_origin, policy.waypoint_radius, &policy, &idx, &dir );

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
	const bool ok = SVG_Nav_QueryMovementDirection_Advance2D_Output3D( &path, query_origin, policy.waypoint_radius, &policy, &idx, &dir );

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
