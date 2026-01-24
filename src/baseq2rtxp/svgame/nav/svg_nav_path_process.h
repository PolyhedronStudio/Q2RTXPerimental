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
	float waypoint_radius = 32.0f;
	float rebuild_goal_dist_2d = 48.0f;
	QMTime rebuild_interval = 500_ms;

	QMTime fail_backoff_base = 250_ms;
	int32_t fail_backoff_max_pow = 6;
};

/**
*	@brief	Per-entity navigation path processing state.
*	@note	Embed this in an entity to add path following behavior.
**/
struct svg_nav_path_process_t {
	nav_traversal_path_t path = {};
	int32_t path_index = 0;
	Vector3 path_goal = {};
	Vector3 path_start = {};
	QMTime next_rebuild_time = 0_ms;
	QMTime backoff_until = 0_ms;
	int32_t consecutive_failures = 0;

	void Reset( void ) {
		SVG_Nav_FreeTraversalPath( &path );
		path_index = 0;
		path_goal = {};
		path_start = {};
		next_rebuild_time = 0_ms;
		backoff_until = 0_ms;
		consecutive_failures = 0;
	}
	bool RebuildPathToWithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
		const Vector3 &agent_mins, const Vector3 &agent_maxs ) {
		if ( !CanRebuild( policy ) ) {
			return false;
		}
		if ( !ShouldRebuildForGoal2D( goal_origin, policy ) && !ShouldRebuildForStart2D( start_origin, policy ) ) {
			return false;
		}

		nav_traversal_path_t oldPath = path;
		const int32_t oldIndex = path_index;

		path = {};
		path_index = 0;

		const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( start_origin, goal_origin, &path, agent_mins, agent_maxs, true, 256.0f, 512.0f );
		if ( ok ) {
			SVG_Nav_FreeTraversalPath( &oldPath );
			path_start = start_origin;
			path_goal = goal_origin;
			consecutive_failures = 0;
			backoff_until = 0_ms;
			next_rebuild_time = level.time + policy.rebuild_interval;
			return true;
		}

		SVG_Nav_FreeTraversalPath( &path );
		path = oldPath;
		path_index = oldIndex;

		consecutive_failures++;
		const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
		const QMTime extra = QMTime::FromMilliseconds( (int32_t)policy.fail_backoff_base.Milliseconds() * ( 1 << powN ) );
		backoff_until = level.time + extra;
		next_rebuild_time = std::max( next_rebuild_time, backoff_until );
		return false;
	}
	bool CanRebuild( const svg_nav_path_policy_t &policy ) const {
		(void)policy;
		return level.time >= next_rebuild_time && level.time >= backoff_until;
	}
	bool ShouldRebuildForGoal2D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const {
		if ( path.num_points <= 0 || !path.points ) {
			return true;
		}
		const Vector3 d = QM_Vector3Subtract( goal_origin, path_goal );
		const float d2 = ( d.x * d.x ) + ( d.y * d.y );
		return d2 > ( policy.rebuild_goal_dist_2d * policy.rebuild_goal_dist_2d );
	}
	bool ShouldRebuildForStart2D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const {
		if ( path.num_points <= 0 || !path.points ) {
			return true;
		}
		// Use the same distance threshold as goal, unless tuned separately.
		const Vector3 d = QM_Vector3Subtract( start_origin, path_start );
		const float d2 = ( d.x * d.x ) + ( d.y * d.y );
		return d2 > ( policy.rebuild_goal_dist_2d * policy.rebuild_goal_dist_2d );
	}
	bool RebuildPathTo( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) {
		if ( !CanRebuild( policy ) ) {
			return false;
		}
		if ( !ShouldRebuildForGoal2D( goal_origin, policy ) && !ShouldRebuildForStart2D( start_origin, policy ) ) {
			return false;
		}

		nav_traversal_path_t oldPath = path;
		const int32_t oldIndex = path_index;

		path = {};
		path_index = 0;

		const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx( start_origin, goal_origin, &path, true, 256.0f, 512.0f );
		if ( ok ) {
			SVG_Nav_FreeTraversalPath( &oldPath );
			path_start = start_origin;
			path_goal = goal_origin;
			consecutive_failures = 0;
			backoff_until = 0_ms;
			next_rebuild_time = level.time + policy.rebuild_interval;
			return true;
		}

		SVG_Nav_FreeTraversalPath( &path );
		path = oldPath;
		path_index = oldIndex;

		consecutive_failures++;
		const int32_t powN = std::min( std::max( 0, consecutive_failures ), policy.fail_backoff_max_pow );
		const QMTime extra = QMTime::FromMilliseconds( (int32_t)policy.fail_backoff_base.Milliseconds() * ( 1 << powN ) );
		backoff_until = level.time + extra;
		next_rebuild_time = std::max( next_rebuild_time, backoff_until );
		return false;
	}
	bool QueryDirection2D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir2d ) {
		if ( !out_dir2d ) {
			return false;
		}
		if ( path.num_points <= 0 || !path.points ) {
			return false;
		}

		Vector3 dir = {};
		int32_t idx = path_index;
		const bool ok = SVG_Nav_QueryMovementDirection( &path, current_origin, policy.waypoint_radius, &idx, &dir );
		if ( !ok ) {
			return false;
		}
		path_index = idx;

		dir.z = 0.0f;
		const float len2 = ( dir.x * dir.x ) + ( dir.y * dir.y );
		if ( len2 <= ( 0.001f * 0.001f ) ) {
			return false;
		}
		*out_dir2d = QM_Vector3Normalize( dir );
		return true;
	}
};
