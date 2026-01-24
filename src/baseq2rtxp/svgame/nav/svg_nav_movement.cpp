/********************************************************************
*
*
*	SVGame: Navigation Movement Helpers - Implementation
*
*
********************************************************************/
#include "svgame/nav/svg_nav_movement.h"

#include "svgame/monsters/svg_mmove.h"

#include <algorithm>
#include <cmath>



/**
*	@brief	Computes 3D distance squared.
**/
static inline float NavMov_DistSqr3D( const Vector3 &a, const Vector3 &b ) {
	const Vector3 d = QM_Vector3Subtract( a, b );
	return ( d.x * d.x ) + ( d.y * d.y ) + ( d.z * d.z );
}

/**
*	@brief	Computes 2D distance squared (XY).
**/
static inline float NavMov_DistSqr2D( const Vector3 &a, const Vector3 &b ) {
	const Vector3 d = QM_Vector3Subtract( a, b );
	return ( d.x * d.x ) + ( d.y * d.y );
}

bool SVG_NavMovement_ShouldRebuildPath( const nav_follow_state_t &state, const Vector3 &goal_origin, const nav_follow_policy_t &policy ) {
	if ( level.time < state.next_rebuild_time ) {
		return false;
	}

	if ( state.path.num_points <= 0 || !state.path.points ) {
		return true;
	}

	const float distSqr3D = NavMov_DistSqr3D( goal_origin, state.path_goal );
	return distSqr3D > ( policy.rebuild_goal_dist_3d * policy.rebuild_goal_dist_3d );
}

bool SVG_NavMovement_RebuildPathIfNeeded( nav_follow_state_t *state, const Vector3 &start_origin, const Vector3 &goal_origin, const nav_follow_policy_t &policy ) {
	if ( !state ) {
		return false;
	}

	if ( !SVG_NavMovement_ShouldRebuildPath( *state, goal_origin, policy ) ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( &state->path );
	state->path_index = 0;

	const bool ok = SVG_Nav_GenerateTraversalPathForOriginEx(
		start_origin,
		goal_origin,
		&state->path,
		state->enable_goal_z_layer_blend,
		state->goal_z_blend_start_dist,
		state->goal_z_blend_full_dist
	);
	if ( ok ) {
		state->path_goal = goal_origin;
	}

	state->next_rebuild_time = level.time + policy.rebuild_interval;
	return ok;
}

bool SVG_NavMovement_QueryFollowDirection( const nav_follow_state_t &state, const Vector3 &current_origin, const nav_follow_policy_t &policy, Vector3 *out_dir3d ) {
	if ( !out_dir3d ) {
		return false;
	}

	int32_t idx = state.path_index;
	return SVG_Nav_QueryMovementDirection_Advance2D_Output3D( &state.path, current_origin, policy.waypoint_radius, &idx, out_dir3d );
}

bool SVG_NavMovement_IsDropSafe( const svg_base_edict_t *ent, const Vector3 &next_origin, const nav_follow_policy_t &policy ) {
	if ( !ent ) {
		return false;
	}
	if ( !policy.avoid_large_drops ) {
		return true;
	}

	// Trace down from the candidate position by the configured limit.
	Vector3 start = next_origin;
	Vector3 end = next_origin;
	end.z -= policy.drop_max_height;

	const svg_trace_t tr = SVG_MMove_Trace( start, ent->mins, ent->maxs, end, const_cast<svg_base_edict_t *>( ent ), CM_CONTENTMASK_MONSTERSOLID );
	if ( tr.allsolid || tr.startsolid ) {
		return false;
	}

	// If no ground was hit within the allowed drop height, reject.
	if ( tr.fraction >= 1.0f ) {
		return false;
	}

	return true;
}

bool SVG_NavMovement_TrySmallObstructionJump( const svg_base_edict_t *ent, const Vector3 &move_dir_xy, const nav_follow_policy_t &policy, Vector3 *inout_velocity ) {
	if ( !ent || !inout_velocity ) {
		return false;
	}
	if ( !policy.allow_small_obstruction_jump ) {
		return false;
	}

	// Only attempt when on ground.
	if ( ent->groundInfo.entityNumber == ENTITYNUM_NONE ) {
		return false;
	}

	// Conservative test: trace a short distance forward; if blocked, see if a jump-height up trace would clear.
	// NOTE: This does not perform full arc simulation; StepSlideMove will handle actual collision response.
	const float forwardDist = std::max( 8.0f, ent->maxs.x );

	Vector3 fwd = move_dir_xy;
	fwd.z = 0.0f;
	const float fwdLen = (float)QM_Vector3Length( fwd );
	if ( fwdLen <= FLT_EPSILON ) {
		return false;
	}
	fwd = QM_Vector3Normalize( fwd );

	Vector3 start = ent->currentOrigin;
	Vector3 end = start + ( fwd * forwardDist );

	svg_trace_t tr = SVG_MMove_Trace( start, ent->mins, ent->maxs, end, const_cast<svg_base_edict_t *>( ent ), CM_CONTENTMASK_MONSTERSOLID );
	if ( tr.fraction >= 1.0f || tr.allsolid || tr.startsolid ) {
		return false;
	}

	// Try stepping/jumping up by up to max_obstruction_jump_height at the impact point.
	Vector3 upStart = ent->currentOrigin;
	Vector3 upEnd = upStart;
	upEnd.z += policy.max_obstruction_jump_height;

	const svg_trace_t upTr = SVG_MMove_Trace( upStart, ent->mins, ent->maxs, upEnd, const_cast<svg_base_edict_t *>( ent ), CM_CONTENTMASK_MONSTERSOLID );
	if ( upTr.allsolid || upTr.startsolid ) {
		return false;
	}

	const float actualUp = upTr.endpos.z - upStart.z;
	if ( actualUp <= 1.0f ) {
		return false;
	}

	// Ensure the landing area is navigable-ish by checking for supporting ground directly below the forward point.
	Vector3 landStart = upTr.endpos + ( fwd * forwardDist );
	Vector3 landEnd = landStart;
	landEnd.z -= policy.max_obstruction_jump_height;

	const svg_trace_t downTr = SVG_MMove_Trace( landStart, ent->mins, ent->maxs, landEnd, const_cast<svg_base_edict_t *>( ent ), CM_CONTENTMASK_MONSTERSOLID );
	if ( downTr.fraction >= 1.0f || downTr.allsolid || downTr.startsolid ) {
		return false;
	}

	// Additionally require that there is nav data at/near the landing XY.
	// This prevents jumping out of bounds.
	if ( g_nav_mesh ) {
		nav_traversal_path_t dummy = {};
		const bool hasNode = SVG_Nav_GenerateTraversalPathForOrigin( ent->currentOrigin, downTr.endpos, &dummy );
		SVG_Nav_FreeTraversalPath( &dummy );
		if ( !hasNode ) {
			return false;
		}
	}

	// Apply an upward velocity request proportional to the jump height.
	const float dt = std::max( 0.001f, (float)gi.frame_time_s );
	const float desiredUpVel = actualUp / dt;
	if ( desiredUpVel > inout_velocity->z ) {
		inout_velocity->z = desiredUpVel;
	}

	return true;
}
