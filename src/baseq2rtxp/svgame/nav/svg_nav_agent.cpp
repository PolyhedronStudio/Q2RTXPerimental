/********************************************************************
*
*
*	SVGame: Navigation Agent Helpers
*
*
********************************************************************/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav_agent.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"

static bool NavAgent_IsBBoxValid( const Vector3 &mins, const Vector3 &maxs ) {
	return maxs.x > mins.x
		&& maxs.y > mins.y
		&& maxs.z > mins.z;
}

void SVG_NavAgent_GetQueryBBox( const svg_base_edict_t *ent, Vector3 *out_mins, Vector3 *out_maxs ) {
	if ( !out_mins || !out_maxs ) {
		return;
	}

	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( mesh && NavAgent_IsBBoxValid( mesh->agent_mins, mesh->agent_maxs ) ) {
		*out_mins = mesh->agent_mins;
		*out_maxs = mesh->agent_maxs;
		return;
	}

	const nav_agent_profile_t agentProfile = SVG_Nav_BuildAgentProfileFromCvars();
	if ( NavAgent_IsBBoxValid( agentProfile.mins, agentProfile.maxs ) ) {
		*out_mins = agentProfile.mins;
		*out_maxs = agentProfile.maxs;
		return;
	}

	if ( ent && NavAgent_IsBBoxValid( ent->mins, ent->maxs ) ) {
		*out_mins = ent->mins;
		*out_maxs = ent->maxs;
		return;
	}

	*out_mins = {};
	*out_maxs = {};
}

void SVG_NavAgent_ResetPathProcess( svg_nav_path_process_t *pathProcess ) {
	if ( !pathProcess ) {
		return;
	}

	if ( pathProcess->pending_request_handle > 0 ) {
		SVG_Nav_CancelRequest( pathProcess->pending_request_handle );
	}

	SVG_Nav_FreeTraversalPath( &pathProcess->path );
	pathProcess->path_index = 0;
	pathProcess->path_goal = {};
	pathProcess->path_start = {};
	pathProcess->path_center_offset_z = 0.0f;
	pathProcess->rebuild_in_progress = false;
	pathProcess->pending_request_handle = 0;
}

bool SVG_NavAgent_TryQueueRebuild( svg_nav_path_process_t *pathProcess, int32_t ownerEntNum,
	const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, bool force, bool emitDebug ) {
	if ( !pathProcess ) {
		return false;
	}

	if ( !nav_nav_async_queue_mode || nav_nav_async_queue_mode->integer == 0 ) {
		if ( emitDebug ) {
			gi.dprintf( "[NavAgent] queue disabled; cannot enqueue ent=%d\n", ownerEntNum );
		}
		return false;
	}

	if ( !SVG_Nav_IsAsyncNavEnabled() ) {
		if ( emitDebug ) {
			gi.dprintf( "[NavAgent] async nav globally disabled ent=%d\n", ownerEntNum );
		}
		return false;
	}

	const bool occupancyWarrantsRebuild = pathProcess->ShouldRebuildForOccupancy( policy );
	const bool effectiveForce = force || occupancyWarrantsRebuild;

	if ( !effectiveForce && !pathProcess->CanRebuild( policy ) ) {
		if ( emitDebug ) {
			gi.dprintf( "[NavAgent] throttled/backoff ent=%d next_rebuild=%lld backoff_until=%lld\n",
				ownerEntNum,
				(long long)pathProcess->next_rebuild_time.Milliseconds(),
				(long long)pathProcess->backoff_until.Milliseconds() );
		}
		return true;
	}

	const bool movementWarrantsRebuild =
		pathProcess->ShouldRebuildForGoal2D( goal_origin, policy )
		|| pathProcess->ShouldRebuildForGoal3D( goal_origin, policy )
		|| pathProcess->ShouldRebuildForStart2D( start_origin, policy )
		|| pathProcess->ShouldRebuildForStart3D( start_origin, policy );

	if ( !effectiveForce && !movementWarrantsRebuild ) {
		if ( emitDebug ) {
			gi.dprintf( "[NavAgent] rebuild not warranted ent=%d\n", ownerEntNum );
		}
		return true;
	}

	if ( SVG_Nav_IsRequestPending( pathProcess ) ) {
		if ( emitDebug ) {
			gi.dprintf( "[NavAgent] request already pending ent=%d\n", ownerEntNum );
		}
		return true;
	}

	const nav_request_handle_t handle = SVG_Nav_RequestPathAsync( pathProcess, start_origin, goal_origin,
		policy, agent_mins, agent_maxs, effectiveForce );
	if ( handle <= 0 ) {
		if ( emitDebug ) {
			gi.dprintf( "[NavAgent] enqueue failed handle=%d ent=%d\n", handle, ownerEntNum );
		}
		return false;
	}

	pathProcess->rebuild_in_progress = true;
	pathProcess->pending_request_handle = handle;
	if ( occupancyWarrantsRebuild ) {
		pathProcess->MarkOccupancyRebuildScheduled( policy );
	}

	if ( emitDebug ) {
		gi.dprintf( "[NavAgent] queued rebuild handle=%d ent=%d force=%d occupancy=%d\n",
			handle, ownerEntNum, effectiveForce ? 1 : 0, occupancyWarrantsRebuild ? 1 : 0 );

		const nav_mesh_t *mesh = g_nav_mesh.get();
		if ( mesh ) {
			const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
			const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
			gi.dprintf( "[NavAgent] start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
				start_origin.x, start_origin.y, start_origin.z,
				start_center.x, start_center.y, start_center.z,
				goal_origin.x, goal_origin.y, goal_origin.z,
				goal_center.x, goal_center.y, goal_center.z );
		}
	}

	return true;
}
