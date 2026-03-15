/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
* 	SVGame: Navigation Path Process
* 
* 	Reusable, per-entity path follow state with throttled rebuild + failure backoff.
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/

// Includes: local and navigation headers.
#include "svgame/svg_local.h"
#include <cstring>
#include <limits>

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_debug.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_traversal.h"

extern cvar_t *nav_max_step;
extern cvar_t *nav_max_drop_height_cap;
extern cvar_t *nav_debug_draw;

/** 
* 	@brief    Build a traversal policy mirroring the provided agent profile.
* 	@param    profile    Agent hull/extents and traversal limits.
* 	@return   Policy whose bounds (bbox, steps, drops, slope) match the profile.
**/
static svg_nav_path_policy_t SVG_Nav_BuildPolicyFromAgentProfile( const nav_agent_profile_t &profile ) {
    /** 
    * 	Copy profile data into the policy so any later modifications can layer
    * 	on top without mutating the original profile.
    **/
    svg_nav_path_policy_t policy = {};
    policy.agent_mins = profile.mins;
    policy.agent_maxs = profile.maxs;
    policy.max_step_height = profile.max_step_height;
    policy.max_drop_height = profile.max_drop_height;
    policy.max_drop_height_cap = profile.max_drop_height_cap;
    policy.max_slope_normal_z = profile.max_slope_normal_z;
	policy.min_step_normal = profile.max_slope_normal_z;
    return policy;
}

/** 
*  @brief    Compute feet->center Z offset from an agent bbox.
*  @param    mins    Feet-origin bbox mins.
*  @param    maxs    Feet-origin bbox maxs.
*  @return   Center offset to add to feet-origin Z for nav-center conversion.
**/
static inline const float SVG_Nav_ComputeFeetToCenterOffsetZ( const Vector3 &mins, const Vector3 &maxs ) {
	return ( mins.z + maxs.z )* 0.5f;
}

/**
*	@brief	Select the best waypoint index when committing a freshly rebuilt async path.
*	@param	path_points		Waypoint buffer in nav-center space.
*	@param	point_count		Number of waypoints in `path_points`.
*	@param	current_center_origin	Current mover origin in nav-center space.
*	@param	waypoint_radius	Completion radius used by path following.
*	@return	Index of the waypoint that best preserves forward progress on the new path.
*	@note	This prevents async recommits from snapping the consumer back to the first
*			start-adjacent waypoint every frame, which otherwise causes yaw jitter and
*			tiny corrective movement on simple direct-shortcut paths.
**/
static int32_t SVG_Nav_SelectCommitPathIndex( const Vector3 * path_points, const int32_t point_count,
	const Vector3 &current_center_origin, const double waypoint_radius ) {
	/**
	*	Sanity checks: fall back to the first waypoint when the incoming path is invalid.
	**/
	if ( !path_points || point_count <= 1 ) {
		return 0;
	}

	/**
	*	Find the waypoint nearest to the mover's current nav-center position.
	*		This preserves progress when the async worker replaces the path with a newer
	*		snapshot while the monster is already moving along the route.
	**/
	int32_t nearest_index = 0;
	double nearest_dist2 = std::numeric_limits<double>::max();
	for ( int32_t i = 0; i < point_count; i++ ) {
		// Measure squared distance in full 3D nav-center space.
		const Vector3 to_waypoint = QM_Vector3Subtract( path_points[ i ], current_center_origin );
		const double dist2 =
			( double )to_waypoint.x * ( double )to_waypoint.x +
			( double )to_waypoint.y * ( double )to_waypoint.y +
			( double )to_waypoint.z * ( double )to_waypoint.z;
		// Keep the nearest waypoint encountered so far.
		if ( dist2 < nearest_dist2 ) {
			nearest_dist2 = dist2;
			nearest_index = i;
		}
	}

	/**
	*	Advance past an already-reached waypoint when we are within the follow radius.
	*		This is the critical fix for 2-point direct paths where waypoint 0 is a fresh
	*		start sample close to the monster's current location.
	**/
	const double waypoint_radius2 = waypoint_radius > 0.0
		? waypoint_radius * waypoint_radius
		: 0.0;
	if ( nearest_index < ( point_count - 1 ) && nearest_dist2 <= waypoint_radius2 ) {
		return nearest_index + 1;
	}

	/**
	*	Otherwise keep following the nearest unresolved waypoint.
	**/
	return nearest_index;
}

/** 
* 
* 
* 
* 	Core:
* 
* 
* 
**/
/** 
* 	@brief	Reset path process state to an initial empty state.
**/
void svg_nav_path_process_t::Reset( void ) {
	/** 
	* 	Reset core state.
	**/
	// Free any existing stored path.
	SVG_Nav_FreeTraversalPath( &path );
	// Reset traversal indices.
	path_index = 0;
	// Reset cached goal/start.
	path_goal_position = {};
	path_start_position = {};
	// Reset throttle/backoff timers.
	next_rebuild_time = 0_ms;
	backoff_until = 0_ms;
	// Reset failure tracking.
	consecutive_failures = 0;
	last_failure_time = 0_ms;
	last_failure_pos = {};
	last_failure_yaw = 0.0f;
   path_center_offset_z = 0.0f;
   // Reset async request generation so stale queue entries cannot commit after a hard reset.
	request_generation = 0;
}

/** 
 * 	@brief	Commit an async traversal path snapshot and reset failure tracking.
 * 	@param	points	Nav-center waypoints produced by either a provisional worker checkpoint or a finalized async rebuild.
 * 	@param	start_origin	Agent start position in entity feet-origin space.
 * 	@param	goal_origin	Agent goal position in entity feet-origin space.
 * 	@param	policy	Path policy informing throttle timing after commit.
 * 	@return	True when the path was stored successfully.
 * 	@note	Worker streaming is allowed to replace the stored path repeatedly with longer valid snapshots while
 * 			the async search continues in the background. This function therefore treats every committed point list
 * 			as the current best authoritative route rather than assuming it is already final.
 **/
const bool svg_nav_path_process_t::CommitAsyncPathFromPoints( const std::vector<Vector3> &points, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) {
	/** 
	 * 	Validate the incoming waypoint list.
	 **/
	if ( points.empty() ) {
		return false;
	}

 /** 
	*  NOTE: Path smoothing temporarily disabled due to a runtime regression
	*  observed in async commit flow. Revert to committing raw A*  points to
	*  ensure path availability remains correct. The funnel+offset approach
	*  will be reintroduced after a safer incremental integration.
	**/

 /** 
	 * 	Copy the waypoints into a traversal path owned by this process.
	 **/
    const int32_t pointCount = ( int32_t )points.size();
	nav_traversal_path_t newPath = {};
	newPath.num_points = pointCount;
	newPath.points = ( Vector3* )gi.TagMallocz( sizeof( Vector3 )* newPath.num_points, TAG_SVGAME_NAVMESH );
	memcpy( newPath.points, points.data(), sizeof( Vector3 )* newPath.num_points );

	/**
	 * 	Translate the current feet-origin start into nav-center space before selecting
	 * 	a preserved waypoint index on the freshly committed path.
	 **/
	const float center_offset_z = SVG_Nav_ComputeFeetToCenterOffsetZ( policy.agent_mins, policy.agent_maxs );
	Vector3 current_center_origin = start_origin;
	current_center_origin.z += center_offset_z;

	/**
	 * 	Pick the most appropriate starting index on the new path so async recommits do
	 * 	not snap the mover back to a start-adjacent waypoint every frame.
	 **/
	const int32_t preserved_path_index = SVG_Nav_SelectCommitPathIndex(
		newPath.points,
		newPath.num_points,
		current_center_origin,
		policy.waypoint_radius );

    /** 
	 * 	Update stored path metadata and reset failure state.
	 * 		Worker streaming replaces the stored route wholesale so the mover always follows the newest valid snapshot.
	 **/
	SVG_Nav_FreeTraversalPath( &path );
	path = newPath;
	path_index = preserved_path_index;
	path_start_position = start_origin;
	path_goal_position = goal_origin;
	path_center_offset_z = center_offset_z;
	consecutive_failures = 0;
	backoff_until = 0_ms;
	next_rebuild_time = level.time + policy.rebuild_interval;
	return true;
}

/** 
* 	@brief	Retrieve the next stored nav-center path point converted into entity feet-origin space.
* 	@param	out_point	[out] Next point in entity feet-origin coordinates.
* 	@return	True if a point exists, false if the path is empty/invalid.
**/
const bool svg_nav_path_process_t::GetNextPathPointEntitySpace( Vector3 * out_point ) const {
	/** 
	* 	Sanity checks.
	**/
	// Validate output pointer.
	if ( !out_point ) {
		return false;
	}
	// Require a valid path buffer.
	if ( path.num_points <= 0 || !path.points ) {
		return false;
	}
	// Require the current path index to still address a valid waypoint.
	if ( path_index < 0 || path_index >= path.num_points ) {
		return false;
	}

	/** 
	* 	Output the stored path point in entity/world origin space.
	* 		The navigation system now operates directly in entity-origin coordinates
	* 		(center-based bounding boxes), so no feet-origin center offset is applied.
	**/
	*out_point = path.points[ path_index ];
	( * out_point )[ 2 ] -= path_center_offset_z;
	return true;
}



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
* 	@param	start_origin	Agent start position in entity feet-origin space.
* 	@param	goal_origin	Agent goal position in entity feet-origin space.
* 	@param	policy		Path policy controlling rebuild cadence and traversal constraints.
* 	@param	agent_mins	Agent bbox mins in entity feet-origin space.
* 	@param	agent_maxs	Agent bbox maxs in entity feet-origin space.
* 	@param	force		If true, bypass rebuild throttles/backoff and movement heuristics.
* 	@return	True if a new path was successfully built and committed.
* 	@note	This function converts feet-origin inputs into the nav-center space expected by
* 			voxelmesh traversal/pathfinding.
* 	@note	Drop safety is enforced both during traversal edge validation and as a post-pass
* 			on the resulting waypoint list.
**/
const bool svg_nav_path_process_t::RebuildPathToWithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, const bool force ) {
	/** 
	* 	Throttle debug spam:
	* 		Callers may invoke this every frame. Only emit verbose debug when we are
	* 		actually allowed to attempt a rebuild (or forced).
	**/
	const bool canAttemptRebuild = force || CanRebuild( policy );
	const bool doVerboseDebug = canAttemptRebuild;
	/** 
	* 	Debug instrumentation:
	* 		Emit input parameters and key cvar-derived constraints.
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
		gi.dprintf( "[DEBUG][NavPath] nav_max_drop_height_cap=%.1f\n", nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 100.0f );
	}

	/** 
	*  Align the working policy with the runtime agent profile.
	**/
	const nav_agent_profile_t agentProfile = SVG_Nav_BuildAgentProfileFromCvars();
	svg_nav_path_policy_t resolvedPolicy = policy;
	const svg_nav_path_policy_t profilePolicy = SVG_Nav_BuildPolicyFromAgentProfile( agentProfile );
	resolvedPolicy.agent_mins = profilePolicy.agent_mins;
	resolvedPolicy.agent_maxs = profilePolicy.agent_maxs;
	resolvedPolicy.max_step_height = profilePolicy.max_step_height;
	resolvedPolicy.max_drop_height = profilePolicy.max_drop_height;
	resolvedPolicy.max_drop_height_cap = profilePolicy.max_drop_height_cap;
	resolvedPolicy.max_slope_normal_z = profilePolicy.max_slope_normal_z;

    /** 
	* 	Pre-check: detect whether the target behaves like a same-level move or a multi-level route.
	* 		Large vertical gaps are expected for stairs, ladders, and floor changes, so they must not be
	* 		treated as impossible purely because the raw endpoint delta exceeds one step height.
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
	*  	Track whether we already have a usable path.
	*  		This lets us treat throttle/backoff skips as "no rebuild needed"
	*  		instead of "pathing failure" so callers can keep following the old path.
	**/
	const bool hasExistingPath = ( path.num_points > 0 && path.points );
	/** 
	*  Guard: avoid expensive A*  attempts when the required climb is outright impossible.
	*  Continually retrying such rebuilds was the root cause of a hitch spam.
	**/
	if ( exceedsStepLimit ) {
		/** 
		*  Throttle impossible vertical gaps:
		*      When no path exists and the endpoint Z gap exceeds our step limit,
		*      schedule a backoff delay so we do not re-attempt every frame.
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
			const QMTime extra = QMTime::FromMilliseconds( ( int32_t )policy.fail_backoff_base.Milliseconds()* ( 1 << powN ) );
			backoff_until = level.time + extra;
			next_rebuild_time = std::max( next_rebuild_time, backoff_until );
		}
		return hasExistingPath;
	}



	/** 
	* 	Throttle/backoff checks:
	* 		Normally we enforce both the per-policy rebuild interval and any failure
	* 		backoff to avoid repeated expensive attempts.
	* 
	* 		We allow bypass in two cases:
	* 			1) No existing path: allow an initial acquisition attempt.
	* 			2) Major goal movement: allow reaction to rapid target relocation.
	**/
	/** 
	* 	Determine whether we should bypass throttle/backoff:
	* 		- If no existing path exists, always allow one acquisition attempt.
	* 		- If the goal moved far enough in Z (e.g. upstairs), allow an attempt even
	* 		  during backoff so we can react to floor transitions.
	**/
	//const bool hasExistingPath = ( path.num_points > 0 && path.points );
	const float goalMovedZ = fabsf( goal_origin.z - path_goal_position.z );
	const float forceZThreshold = ( stepLimit > 0.0f ) ? ( stepLimit* 2.0f ) : NAV_DEFAULT_STEP_MAX_SIZE * 2;
	const bool forceForGoalZ = ( goalMovedZ >= forceZThreshold );
	// Only bypass movement heuristics when the goal moved floors; do not bypass time-based backoff.
	const bool throttleBypass = forceForGoalZ;

	/** 
	* 	Throttle/backoff enforcement:
	* 		Always enforce time-based throttles unless explicitly forced.
	* 		Even without an existing path we must not rebuild every frame, otherwise we
	* 		spam logs and stall the game when a route is impossible.
	**/
	if ( !force && !canAttemptRebuild ) {
		/** 
		*  	Skip rebuilds while throttled/backing off.
		*  		If we already have a path, keep using it instead of reporting failure.
		**/
		return hasExistingPath;
	}
	/** 
	* 	Movement heuristic:
	* 		If neither the goal nor the start moved enough (based on policy thresholds),
	* 		skip the rebuild to avoid wasting CPU.
	**/
	const bool movementWarrantsRebuild =
		ShouldRebuildForGoal2D( goal_origin, policy )
		|| ShouldRebuildForGoal3D( goal_origin, policy )
		|| ShouldRebuildForStart2D( start_origin, policy )
		|| ShouldRebuildForStart3D( start_origin, policy );
	if ( !force && !hasExistingPath && !movementWarrantsRebuild ) {
		/** 
		*  	No path exists and movement doesn't warrant rebuilding.
		*  		Report "no path" to the caller so it can fallback appropriately.
		**/
		return false;
	}
	if ( !force && hasExistingPath && !throttleBypass && !movementWarrantsRebuild ) {
		/** 
		*  	Existing path is still considered valid and the goal hasn't moved enough.
		*  		Return true so callers keep following the current path.
		**/
		return true;
	}

	/** 
	* 	Preserve old state so we can roll back on failure.
	**/
	// Keep a copy of the existing path (and index) in case the new path fails.
	nav_traversal_path_t oldPath = path;
	const int32_t oldIndex = path_index;
	const float oldCenterOffsetZ = path_center_offset_z;

	// Clear current path state prior to attempting rebuild.
	path = {};
	path_index = 0;

 /** 
	* 	Boundary convention:
	* 		External callers provide feet-origin coordinates while traversal APIs
	* 		convert to nav-center exactly once internally.
	**/
    const Vector3 agent_center_mins = resolvedPolicy.agent_mins;
	const Vector3 agent_center_maxs = resolvedPolicy.agent_maxs;
	Q_assert( agent_center_maxs.x > agent_center_mins.x && agent_center_maxs.y > agent_center_mins.y && agent_center_maxs.z > agent_center_mins.z );

	/** 
	* 	Run navigation query to rebuild path using explicit bbox.
	* 		We pass `this` so A*  can apply per-entity failure penalties.
	**/
	const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox(
       start_origin,
		goal_origin,
	    &path,
	    agent_center_mins,
	    agent_center_maxs,
	    &resolvedPolicy,
	    resolvedPolicy.enable_goal_z_layer_blend,
	    resolvedPolicy.blend_start_dist,
	    resolvedPolicy.blend_full_dist,
	    this );
	// Note: pass path-process (this) into the traversal generator to allow
	// A*  to use recent failure/backoff timing for per-entity failure penalties.
    // Only emit this verbose return code when verbose debug is enabled to avoid spamming logs every rebuild.
	if ( doVerboseDebug ) {
		gi.dprintf( "[DEBUG][NavPath] SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox returned %d\n", ok ? 1 : 0 );
	}

	/** 
	* 	Drop-limit post-process:
	* 		Reject paths that contain large downward transitions.
	* 		Even if an edge is technically traversable, large drops can be undesirable for AI.
	**/
	bool dropLimitOk = true;
	// Determine the effective max drop to enforce on paths. Prefer policy settings when enabled,
	// otherwise fall back to the global nav_max_drop_height_cap cvar.
	/** 
	*  Determine drop rejection limit:
	*      Prefer an explicit policy drop cap when positive, otherwise use the
	*      per-policy max drop when dropping is capped. Fall back to the global
	*      drop cap when no policy value is configured.
	**/
	const float maxDrop = ( resolvedPolicy.max_drop_height_cap > 0.0f )
	    ? ( float )resolvedPolicy.max_drop_height_cap
	    : ( resolvedPolicy.enable_max_drop_height_cap ? ( float )resolvedPolicy.max_drop_height : ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 100.0f ) );
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
	* 	Commit success or revert on failure.
	**/
	if ( ok && dropLimitOk ) {
		// Free old path now that we have a valid replacement.
		SVG_Nav_FreeTraversalPath( &oldPath );
		// Store feet-origin start/goal (callers interact in entity space).
		path_start_position = start_origin;
		path_goal_position = goal_origin;
      path_center_offset_z = SVG_Nav_ComputeFeetToCenterOffsetZ( resolvedPolicy.agent_mins, resolvedPolicy.agent_maxs );
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
	path_center_offset_z = oldCenterOffsetZ;

	/** 
	* 	Failure backoff:
	* 		Increase delay before next rebuild attempt and record a failure signature
	* 		for A*  penalties (avoid repeating the same failing approach).
	**/
    consecutive_failures++;
    // Record failure time and signature for per-entity A*  penalties.
    last_failure_time = level.time;
    last_failure_pos = goal_origin;
    // Record a coarse facing yaw from start->goal for signature matching.
    Vector3 toGoal = QM_Vector3Subtract( goal_origin, start_origin );
    last_failure_yaw = QM_Vector3ToYaw( toGoal );
	const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
	const QMTime extra = QMTime::FromMilliseconds( ( int32_t )policy.fail_backoff_base.Milliseconds()* ( 1 << powN ) );
	backoff_until = level.time + extra;
	next_rebuild_time = std::max( next_rebuild_time, backoff_until );
    // Throttle this debug print so repeated failures do not flood the console when not debugging.
	if ( doVerboseDebug ) {
		gi.dprintf( "[DEBUG][NavPath] Pathfinding failed, consecutive_failures=%d, backoff_until=%lld\n", consecutive_failures, ( long long )backoff_until.Milliseconds() );
	}
	/** 
	*  	Failure outcome:
	*  		If we restored a previous path, report success so callers can keep using it.
	*  		If we have no usable path, report failure to trigger fallbacks.
	**/
	return ( path.num_points > 0 && path.points );
}
/** 
* 	@brief	Used to determine if a path rebuild can be attempted at this time.
**/
const bool svg_nav_path_process_t::CanRebuild( const svg_nav_path_policy_t &policy ) const {
	/** 
	* 	Sanity: currently policy is not used, but we keep it for future expansion.
	**/
	( void )policy;
	// Rebuild is permitted only if we are past both throttle and backoff times.
	return level.time >= next_rebuild_time && level.time >= backoff_until;
}
/** 
* 	@brief	Determine if the path should be rebuilt based on the goal's 2D distance change.
* 	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForGoal2D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const {
	/** 
	* 	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/** 
	* 	Check goal movement in 2D.
	**/
	// Compute XY delta between current goal and cached goal.
	const Vector3 d = QM_Vector3Subtract( goal_origin, path_goal_position );
	// Only consider XY for rebuild thresholds.
	const float d2 = ( d.x* d.x ) + ( d.y* d.y );
	// Trigger rebuild when squared distance exceeds threshold squared.
	return d2 > ( policy.rebuild_goal_dist_2d* policy.rebuild_goal_dist_2d );
}

/** 
* 	@brief	Determine if the path should be rebuilt based on the goal's 3D distance change.
* 	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForGoal3D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const {
	/** 
	* 	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/** 
	* 	Guard: treat non-positive thresholds as disabled.
	**/
	if ( policy.rebuild_goal_dist_3d <= 0.0 ) {
		return false;
	}

	/** 
	* 	Check goal movement in 3D.
	**/
	// Compute XYZ delta between current goal and cached goal.
	const Vector3 d = QM_Vector3Subtract( goal_origin, path_goal_position );
	// Compare squared 3D distance against squared threshold.
	const double d2 = (double)d.x* (double)d.x + (double)d.y* (double)d.y + (double)d.z* (double)d.z;
	const double thresh2 = policy.rebuild_goal_dist_3d* policy.rebuild_goal_dist_3d;
	return d2 > thresh2;
}
/** 
* 	@brief	Determine if the path should be rebuilt based on the start's 2D distance change.
*  	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForStart2D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const {
	/** 
	* 	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/** 
	* 	Check start movement in 2D.
	**/
	// Use the same XY threshold as goal unless tuned separately.
	const Vector3 d = QM_Vector3Subtract( start_origin, path_start_position );
	const float d2 = ( d.x* d.x ) + ( d.y* d.y );
	return d2 > ( policy.rebuild_goal_dist_2d* policy.rebuild_goal_dist_2d );
}

/** 
* 	@brief	Determine if the path should be rebuilt based on the start's 3D distance change.
* 	@return	True if the path should be rebuilt.
**/
const bool svg_nav_path_process_t::ShouldRebuildForStart3D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const {
	/** 
	* 	Rebuild if no path exists.
	**/
	if ( path.num_points <= 0 || !path.points ) {
		return true;
	}

	/** 
	* 	Guard: treat non-positive thresholds as disabled.
	**/
	if ( policy.rebuild_goal_dist_3d <= 0.0 ) {
		return false;
	}

	/** 
	* 	Check start movement in 3D.
	**/
	// Compute XYZ delta between current start and cached start.
	const Vector3 d = QM_Vector3Subtract( start_origin, path_start_position );
	// Compare squared 3D distance against squared threshold.
	const double d2 = (double)d.x* (double)d.x + (double)d.y* (double)d.y + (double)d.z* (double)d.z;
	const double thresh2 = policy.rebuild_goal_dist_3d* policy.rebuild_goal_dist_3d;
	return d2 > thresh2;
}
/** 
* 	@brief	Rebuild the path to the given goal from the given start.
*  	@return	True if the path was successfully rebuilt.
**/
const bool svg_nav_path_process_t::RebuildPathTo( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) {
	/** 
	* 	Throttle/backoff checks.
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
	const float oldCenterOffsetZ = path_center_offset_z;

	// Reset current path container prior to rebuild.
	path = {};
	path_index = 0;

    /** 
	* 	Boundary convention:
	* 		External callers provide feet-origin coordinates while traversal APIs
	* 		convert to nav-center exactly once internally.
	**/
	const Vector3 start_query = start_origin;
	const Vector3 goal_query = goal_origin;

	/** 
	* 	Generate a new traversal path.
	**/
    const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx( start_query, goal_query, &path, policy.enable_goal_z_layer_blend, policy.blend_start_dist, policy.blend_full_dist );
	if ( ok ) {
		// Replace old path with new path.
		SVG_Nav_FreeTraversalPath( &oldPath );
		// Store feet-origin start/goal for rebuild heuristics.
		path_start_position = start_origin;
		path_goal_position = goal_origin;
      const nav_agent_profile_t profile = SVG_Nav_BuildAgentProfileFromCvars();
		path_center_offset_z = SVG_Nav_ComputeFeetToCenterOffsetZ( profile.mins, profile.maxs );
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
	path_center_offset_z = oldCenterOffsetZ;

	//
	consecutive_failures++;
	const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
	const QMTime extra = QMTime::FromMilliseconds( ( int32_t )policy.fail_backoff_base.Milliseconds()* ( 1 << powN ) );
	backoff_until = level.time + extra;
	next_rebuild_time = std::max( next_rebuild_time, backoff_until );
	return false;
}



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
*	@brief	Return whether one adjacent path segment behaves like a step-sized vertical transition.
*	@param	path		Traversal path whose stored nav-center waypoints will be inspected.
*	@param	from_index	Index of the first waypoint in the segment.
*	@param	to_index	Index of the second waypoint in the segment.
*	@param	step_threshold	Maximum Z delta still considered a normal step-sized rise/drop.
*	@return	True when the indexed segment encodes a meaningful but still step-sized vertical change.
*	@note	This helper is shared by follow-state classification so stair runs keep their intermediate
*			anchors longer before callers blend toward later waypoints.
**/
static bool SVG_Nav_PathSegmentIsStepSizedVerticalTransition( const nav_traversal_path_t &path, const int32_t from_index, const int32_t to_index, const double step_threshold ) {
	/**
	* 	Sanity checks: require valid waypoint indices and a positive threshold.
	**/
	if ( !path.points || path.num_points <= 0 || from_index < 0 || to_index < 0 || from_index >= path.num_points || to_index >= path.num_points || step_threshold <= 0.0 ) {
		return false;
	}

	/**
	* 	Measure the vertical delta and keep only meaningful step-sized transitions.
	**/
	const double delta_z = std::fabs( ( double )path.points[ to_index ].z - ( double )path.points[ from_index ].z );
	return delta_z > 1.0 && delta_z <= step_threshold;
}

/**
*	@brief	Return whether one waypoint behaves like a stair/corner anchor.
*	@param	path		Traversal path whose stored nav-center waypoints will be inspected.
*	@param	waypoint_index	Current active waypoint index.
*	@param	step_threshold	Maximum Z delta still considered a normal step-sized rise/drop.
*	@return	True when either adjacent segment around the active waypoint behaves like a step-sized rise/drop.
**/
static bool SVG_Nav_PathWaypointNeedsPreciseCentering( const nav_traversal_path_t &path, const int32_t waypoint_index, const double step_threshold ) {
	/**
	* 	Treat the active waypoint as a precision anchor when either neighboring segment behaves like a stair step.
	**/
	return SVG_Nav_PathSegmentIsStepSizedVerticalTransition( path, waypoint_index - 1, waypoint_index, step_threshold )
		|| SVG_Nav_PathSegmentIsStepSizedVerticalTransition( path, waypoint_index, waypoint_index + 1, step_threshold );
}

/**
*	@brief	Return whether the current waypoint sits inside a repeated stepped vertical corridor.
*	@param	path		Traversal path whose stored nav-center waypoints will be inspected.
*	@param	waypoint_index	Current active waypoint index.
*	@param	step_threshold	Maximum Z delta still considered a normal step-sized rise/drop.
*	@return	True when two adjacent step-sized segments appear back-to-back around the active anchor.
*	@note	This lets callers keep stronger centering through short stair bands instead of steering away
*			as soon as the first intermediate anchor changes.
**/
static bool SVG_Nav_PathWaypointHasSteppedCorridorAhead( const nav_traversal_path_t &path, const int32_t waypoint_index, const double step_threshold ) {
	/**
	* 	Classify corridor-like stair runs by looking for repeated adjacent step-sized transitions.
	**/
	const bool previous_to_current = SVG_Nav_PathSegmentIsStepSizedVerticalTransition( path, waypoint_index - 1, waypoint_index, step_threshold );
	const bool current_to_next = SVG_Nav_PathSegmentIsStepSizedVerticalTransition( path, waypoint_index, waypoint_index + 1, step_threshold );
	const bool next_to_next_next = SVG_Nav_PathSegmentIsStepSizedVerticalTransition( path, waypoint_index + 1, waypoint_index + 2, step_threshold );
	return ( previous_to_current && current_to_next ) || ( current_to_next && next_to_next_next );
}

/**
*	@brief	Query a reusable shared follow-state bundle for the current path.
*	@param	current_origin	Current feet-origin position of the mover.
*	@param	policy		Path-follow policy controlling waypoint radius and stair limits.
*	@param	out_state	[out] Shared follow-state bundle for steering/velocity code.
*	@return	True when the path yielded a valid direction to follow.
*	@note	This helper commits waypoint advancement first, then reports the active waypoint and
*			whether it behaves like a stair/corner anchor that needs tighter centering.
**/
const bool svg_nav_path_process_t::QueryFollowState( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, follow_state_t * out_state ) {
	/**
	* 	Sanity checks: require output storage and a valid path buffer.
	**/
	if ( !out_state ) {
		return false;
	}
	*out_state = {};
	if ( path.num_points <= 0 || !path.points ) {
		return false;
	}

	/**
	* 	Query the next path direction using the shared 3D advancement logic.
	**/
	Vector3 moveDirection3D = {};
	if ( !QueryDirection3D( current_origin, policy, &moveDirection3D ) ) {
		return false;
	}

	/**
	* 	Resolve the active waypoint after `QueryDirection3D` committed any waypoint advancement.
	**/
	Vector3 activeWaypointOrigin = {};
	if ( !GetNextPathPointEntitySpace( &activeWaypointOrigin ) ) {
		return false;
	}

	/**
	* 	Measure the active waypoint in feet-origin space so callers can shape centering and slowdown.
	**/
	Vector3 toActiveWaypoint = QM_Vector3Subtract( activeWaypointOrigin, current_origin );
	const double activeWaypointDeltaZ = std::fabs( ( double )toActiveWaypoint.z );
	toActiveWaypoint.z = 0.0f;
	const double activeWaypointDist2D = std::sqrt( ( toActiveWaypoint.x * toActiveWaypoint.x ) + ( toActiveWaypoint.y * toActiveWaypoint.y ) );

	/**
	* 	Treat any meaningful vertical waypoint change as a precision anchor.
	* 		This keeps stair runs and small multi-level transitions centered longer before callers steer away.
	**/
	double stepThreshold = NAV_DEFAULT_STEP_MAX_SIZE;
	if ( policy.max_step_height > 0.0 ) {
		stepThreshold = policy.max_step_height;
	}
 const double minStepThreshold = policy.min_step_height > 0.0 ? policy.min_step_height : ( double )NAV_DEFAULT_STEP_MIN_SIZE;
	const double stepThresholdWithSlack = stepThreshold + std::max( ( double )PHYS_STEP_GROUND_DIST, 1.0 );
	const bool waypointNeedsPreciseCentering = SVG_Nav_PathWaypointNeedsPreciseCentering( path, path_index, stepThresholdWithSlack )
        || ( activeWaypointDeltaZ >= minStepThreshold && activeWaypointDeltaZ <= stepThresholdWithSlack );
	const bool steppedVerticalCorridorAhead = SVG_Nav_PathWaypointHasSteppedCorridorAhead( path, path_index, stepThresholdWithSlack );

	/**
	* 	Publish the shared follow-state result to the caller.
	**/
	out_state->has_direction = true;
	out_state->move_direction3d = moveDirection3D;
	out_state->active_waypoint_origin = activeWaypointOrigin;
	out_state->active_waypoint_dist2d = activeWaypointDist2D;
	out_state->active_waypoint_delta_z = activeWaypointDeltaZ;
	out_state->waypoint_needs_precise_centering = waypointNeedsPreciseCentering;
   out_state->stepped_vertical_corridor_ahead = steppedVerticalCorridorAhead;
	out_state->approaching_final_goal = path_index >= std::max( 0, path.num_points - 2 );
	return true;
}

/** 
* 	@brief	Query the next movement direction in 2D from the current origin along the path.
**/
const bool svg_nav_path_process_t::QueryDirection2D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 * out_dir2d ) {
	/** 
	* 	Sanity checks: require a valid output pointer and a valid path buffer.
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
	* 	Boundary conversion:
	* 		Path points are stored in nav-center coordinates, so convert the
	* 		feet-origin query into center-origin before querying traversal.
	**/
    const Vector3 query_origin = QM_Vector3Add( current_origin, Vector3{ 0.0f, 0.0f, path_center_offset_z } );

	/** 
	* 	Query next direction and advance waypoints.
	**/
	Vector3 dir = {};
	int32_t idx = path_index;
	const bool ok = SVG_Nav_QueryMovementDirection( &path, query_origin, policy.waypoint_radius, &policy, &idx, &dir );

	/** 
	* 	Sanity: do not update state if the query failed.
	**/
	if ( !ok ) {
		return false;
	}

	/** 
	* 	Commit advanced index returned by the query.
	**/
	path_index = idx;

	/** 
	* 	Convert to 2D direction and validate magnitude before normalizing.
	**/
	// Ignore Z for 2D direction.
	dir.z = 0.0f;
	// Compute squared length (XY only).
	const float len2 = ( dir.x* dir.x ) + ( dir.y* dir.y );
	// Guard against normalizing near-zero vectors.
	if ( len2 <= ( 0.001f* 0.001f ) ) {
		return false;
	}

	/** 
	* 	Output normalized direction.
	**/
	* out_dir2d = QM_Vector3Normalize( dir );
	return true;
}

/** 
* 	@brief	Query the next movement direction in 3D from the current origin along the path.
* 	@param	current_origin	Current position of the agent.
* 	@param	policy			Path policy (for waypoint radius).
* 	@param	out_dir3d		[out] Resulting normalized 3D direction.
* 	@return	True if a direction was found.
**/
const bool svg_nav_path_process_t::QueryDirection3D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 * out_dir3d ) {
	/** 
	* 	Sanity checks: require output pointer and a valid path.
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
	* 	Boundary conversion:
	* 		Path points are stored in nav-center coordinates, so convert the
	* 		feet-origin query into center-origin before querying traversal.
	**/
    const Vector3 query_origin = QM_Vector3Add( current_origin, Vector3{ 0.0f, 0.0f, path_center_offset_z } );

	/** 
	* 	Query direction while advancing waypoints in 2D, but output a 3D direction.
	**/
	Vector3 dir = {};
	int32_t idx = path_index;
	const bool ok = SVG_Nav_QueryMovementDirection_Advance2D_Output3D( &path, query_origin, policy.waypoint_radius, &policy, &idx, &dir );

	/** 
	* 	Sanity: do not update state if the query failed.
	**/
	if ( !ok ) {
		return false;
	}

	/** 
	* 	Commit advanced index returned by the query.
	**/
	path_index = idx;

	/** 
	* 	Validate magnitude before normalizing.
	**/
	const float len2 = ( dir.x* dir.x ) + ( dir.y* dir.y ) + ( dir.z* dir.z );
	if ( len2 <= ( 0.001f* 0.001f ) ) {
		return false;
	}

	/** 
	* 	Output normalized 3D direction.
	**/
	* out_dir3d = QM_Vector3Normalize( dir );
	return true;
}

/**
 *    @brief
 *        Heuristic helper to detect a single outlier floor sample that is
 *        substantially below both endpoints of a tested neighbor segment.
 *
 *    @param from            Segment start position (nav-center space).
 *    @param to              Segment end position (nav-center space).
 *    @param sampledFloorZ   Floor Z sampled under the neighbor point.
 *    @param dropCap         Allowed drop cap used to qualify outliers.
 *    @return True when the sampled floor appears to be an isolated outlier
 *            (both endpoints sit well above the sampled floor by a margin).
 **/
static bool SVG_Nav_IsOutlierDropSample(const Vector3 &from, const Vector3 &to, float sampledFloorZ, float dropCap)
{
	// Compute endpoint extrema without relying on std::min/std::max overloads.
	const float minEndpointZ = (from.z < to.z) ? from.z : to.z;
	const float maxEndpointZ = (from.z > to.z) ? from.z : to.z;

	// Consider the sample an outlier only when both endpoints sit farther above
	// the sampled floor than `dropCap + 64` world units. The extra margin avoids
	// rejecting genuine long drops while still filtering sporadic erroneous samples.
	return (minEndpointZ - sampledFloorZ) > (dropCap + 64.0f)
		&& (maxEndpointZ - sampledFloorZ) > (dropCap + 64.0f);
}
