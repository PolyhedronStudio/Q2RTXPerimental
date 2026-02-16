/********************************************************************
*
*
*    SVGame: Navigation Request Queue Implementation
*
*
********************************************************************/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

//! Container holding queued and in-flight navigation requests.
static std::vector<nav_request_entry_t> s_nav_request_queue = {};

//! Value used to issue unique handles per request.
static nav_request_handle_t s_nav_request_next_handle = 1;

//! Toggle controlling whether path rebuilds should be enqueued via the async request queue.
cvar_t *nav_nav_async_queue_mode = nullptr;

//! Optional debug toggle for emitting async queue statistics.
cvar_t *s_nav_nav_async_log_stats = nullptr;

//! Master toggle cvar for async request processing.
static cvar_t *s_nav_nav_async_enable = nullptr;

//! Per-frame request budget cvar.
static cvar_t *s_nav_requests_per_frame = nullptr;

//! Per-request expansion budget cvar.
static cvar_t *s_nav_expansions_per_request = nullptr;

static void NavRequest_LogQueueDiagnostics( int32_t queueBefore, int32_t queueAfter, int32_t processed, int32_t requestBudget, int32_t remainingExpansions );
static nav_request_entry_t *NavRequest_FindEntryByHandle( nav_request_handle_t handle );
static nav_request_entry_t *NavRequest_FindEntryForProcess( svg_nav_path_process_t *process );
static bool NavRequest_PrepareAStarForEntry( nav_request_entry_t &entry );
static void NavRequest_ClearProcessMarkers( nav_request_entry_t &entry );
static bool NavRequest_FinalizePathForEntry( nav_request_entry_t &entry );
static void NavRequest_HandleFailure( nav_request_entry_t &entry );

/**
*    @brief    Register cvars that control async request processing and diagnostics.
*    @note     Safe to invoke from SVG_Nav_Init after the navigation subsystem starts.
**/
void SVG_Nav_RequestQueue_RegisterCvars( void ) {
	if ( s_nav_nav_async_enable ) {
		return;
	}

	s_nav_nav_async_enable = gi.cvar( "nav_nav_async_enable", "1", 0 );
	s_nav_requests_per_frame = gi.cvar( "nav_requests_per_frame", "4", 0 );
	s_nav_expansions_per_request = gi.cvar( "nav_expansions_per_request", "512", 0 );
	nav_nav_async_queue_mode = gi.cvar( "nav_nav_async_queue_mode", "1", 0 );
	s_nav_nav_async_log_stats = gi.cvar( "nav_nav_async_log_stats", "1", 0 );
}

/**
*    @brief    Locate a queued entry by handle.
*    @param    handle    Handle returned when the request was enqueued.
*    @return   Pointer to the entry or nullptr if not found.
**/
static nav_request_entry_t *NavRequest_FindEntryByHandle( nav_request_handle_t handle ) {
	if ( handle <= 0 ) {
		return nullptr;
	}

	for ( nav_request_entry_t &entry : s_nav_request_queue ) {
		if ( entry.handle == handle ) {
			return &entry;
		}
	}

	return nullptr;
}

/**
*    @brief    Locate an in-flight request owned by a path process.
*    @param    process    Requesting path process.
*    @return   Pointer to the matching entry or nullptr if none exist.
**/
static nav_request_entry_t *NavRequest_FindEntryForProcess( svg_nav_path_process_t *process ) {
	if ( !process ) {
		return nullptr;
	}

	for ( nav_request_entry_t &entry : s_nav_request_queue ) {
		if ( entry.path_process != process ) {
			continue;
		}

		// Treat any terminal state as non-active so callers do not consider
		// the entry "pending" when it already reached a terminal outcome.
		// Previously this check omitted the Failed state which could cause
		// callers to believe a request was still pending even after failure.
        if ( entry.status == nav_request_status_t::Completed
			|| entry.status == nav_request_status_t::Cancelled
			|| entry.status == nav_request_status_t::Failed ) {
			// Optional debug: when skipping failed entries, emit a small
			// diagnostic to aid debugging. Only print when async logging is enabled
			// to avoid spamming release logs.
			if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
				gi.dprintf( "[NavAsync][Queue] Skipping terminal entry (handle=%d status=%d) for ent_process=%p\n",
					entry.handle, ( int )entry.status, ( void * )entry.path_process );
			}
			continue;
		}

		return &entry;
	}

	return nullptr;
}

/**
*    @brief    Enqueue a navigation request.
*    @note     Deduplicates requests by path process and reuses the existing handle when needed.
**/
nav_request_handle_t SVG_Nav_RequestPathAsync( svg_nav_path_process_t *pathProcess, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, bool force ) {
	/**
	*    Sanity: require a valid process pointer and an enabled queue.
	**/
	if ( !pathProcess ) {
		return 0;
	}

	if ( !SVG_Nav_IsAsyncNavEnabled() ) {
		return 0;
	}

	/**
	*    Deduplication: refresh existing entry parameters instead of building a new handle.
	**/
 // Before reusing an existing entry, remove any stale "Failed" entries for
	// the same process so callers can re-enqueue immediately. Removing them
	// avoids leaving failed handles lying around and makes retries deterministic.
	std::vector<nav_request_handle_t> removedHandles;
	for ( auto it = s_nav_request_queue.begin(); it != s_nav_request_queue.end(); ) {
		if ( it->path_process == pathProcess && it->status == nav_request_status_t::Failed ) {
			removedHandles.push_back( it->handle );
			it = s_nav_request_queue.erase( it );
			continue;
		}
		++it;
	}
	if ( !removedHandles.empty() && s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		for ( nav_request_handle_t h : removedHandles ) {
			gi.dprintf( "[NavAsync][Queue] Removed previously failed entry handle=%d for ent_process=%p before enqueue\n", h, ( void * )pathProcess );
		}
	}
    if ( nav_request_entry_t *existing = NavRequest_FindEntryForProcess( pathProcess ) ) {
		// Refresh existing entry parameters instead of creating a duplicate.
		existing->start = start_origin;
		existing->goal = goal_origin;
		existing->policy = policy;
		existing->agent_mins = agent_mins;
		existing->agent_maxs = agent_maxs;
		existing->force = existing->force || force;
		// If the existing entry was Running allow it to be marked queued so it
		// will be re-prepared with the new params. This implements a simple
		// replace semantics: callers get their updated goal applied without
		// growing the queue or losing the handle.
		existing->status = nav_request_status_t::Queued;
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Queue] Refreshed existing entry handle=%d for ent_process=%p (queued)\n",
				existing->handle, ( void * )existing->path_process );
		}
		return existing->handle;
	}

	/**
	*    Handle assignment: issue a new handle, ensuring zero is never returned.
	**/
	nav_request_handle_t handle = s_nav_request_next_handle++;
	if ( s_nav_request_next_handle <= 0 ) {
		s_nav_request_next_handle = 1;
	}

	nav_request_entry_t entry = {};
	entry.path_process = pathProcess;
	entry.start = start_origin;
	entry.goal = goal_origin;
	entry.policy = policy;
	entry.agent_mins = agent_mins;
	entry.agent_maxs = agent_maxs;
	entry.force = force;
	entry.status = nav_request_status_t::Queued;
	entry.handle = handle;

	/**
	*    Enqueue the new entry for future processing ticks.
	**/
	s_nav_request_queue.push_back( entry );
    // Lightweight diagnostic: log new enqueue when async logging is desired.
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][Queue] Enqueued handle=%d for ent_process=%p start=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) force=%d\n",
			entry.handle, ( void * )entry.path_process,
			entry.start.x, entry.start.y, entry.start.z,
			entry.goal.x, entry.goal.y, entry.goal.z,
			entry.force ? 1 : 0 );
	}
	return handle;
}

/**
*    @brief    Cancel a previously queued request.
*    @note     Marks the entry so it can be removed during the next tick.
**/
void SVG_Nav_CancelRequest( nav_request_handle_t handle ) {
	/**
	*    Guard: handle zero is invalid and must be ignored.
	**/
	if ( handle <= 0 ) {
		return;
	}

	/**
	*    Locate the request and update its status to cancelled.
	**/
	nav_request_entry_t *entry = NavRequest_FindEntryByHandle( handle );
	if ( !entry ) {
		return;
	}

	if ( entry->status == nav_request_status_t::Cancelled || entry->status == nav_request_status_t::Completed ) {
		return;
	}

	entry->status = nav_request_status_t::Cancelled;
}

/**
*    @brief    Check if a path process currently owns a queued request.
**/
bool SVG_Nav_IsRequestPending( const svg_nav_path_process_t *pathProcess ) {
	/**
	*    Guard: invalid process pointers never own requests.
	**/
	if ( !pathProcess ) {
		return false;
	}

	/**
	*    Scan the queue for matching active entries.
	**/
	for ( const nav_request_entry_t &entry : s_nav_request_queue ) {
		if ( entry.path_process != pathProcess ) {
			continue;
		}

		if ( entry.status == nav_request_status_t::Completed || entry.status == nav_request_status_t::Cancelled || entry.status == nav_request_status_t::Failed ) {
			continue;
		}

		return true;
	}

	return false;
}

/**
*    @brief    Process pending requests per frame.
*    @note     Each request now steps the incremental A* helper using the configured expansion budget.
**/
void SVG_Nav_ProcessRequestQueue( void ) {
	/**
	*    Feature gate: skip work when async processing is disabled.
	**/
	if ( !SVG_Nav_IsAsyncNavEnabled() ) {
		return;
	}

	/**
	*    Guard: wait for the navigation mesh before touching queued requests.
	**/
	if ( !g_nav_mesh.get() ) {
		return;
	}

	/**
	*    Budgeting: constrain work by both request throttles and expansion caps.
	**/
	const int32_t requestBudget = SVG_Nav_GetAsyncRequestBudget();
	const int32_t expansionsPerRequest = SVG_Nav_GetAsyncRequestExpansions();
	const int64_t totalExpansions = ( int64_t )expansionsPerRequest * requestBudget;
	int32_t remainingExpansions = ( int32_t )std::min( totalExpansions, ( int64_t )std::numeric_limits<int32_t>::max() );
	int32_t processed = 0;
	const int32_t queueBefore = ( int32_t )s_nav_request_queue.size();
	const uint64_t queueStartMs = gi.GetRealSystemTime();
	const uint64_t queueBudgetMs = 2;

	/**
	*    Iterate queue entries while honoring request, expansion, and time budgets.
	**/
	for ( nav_request_entry_t &entry : s_nav_request_queue ) {
		if ( processed >= requestBudget || remainingExpansions <= 0 ) {
			break;
		}
		const uint64_t queueElapsedMs = gi.GetRealSystemTime() - queueStartMs;
		if ( queueBudgetMs > 0 && queueElapsedMs > queueBudgetMs ) {
			break;
		}

		if ( entry.status == nav_request_status_t::Completed || entry.status == nav_request_status_t::Failed || entry.status == nav_request_status_t::Cancelled ) {
			NavRequest_ClearProcessMarkers( entry );
			continue;
		}

		if ( entry.status == nav_request_status_t::Queued ) {
          if ( !NavRequest_PrepareAStarForEntry( entry ) ) {
				entry.status = nav_request_status_t::Failed;
				NavRequest_HandleFailure( entry );
				NavRequest_ClearProcessMarkers( entry );
				Nav_AStar_Reset( &entry.a_star );
				if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
					gi.dprintf( "[NavAsync][Queue] PrepareAStarForEntry failed for handle=%d path_process=%p\n", entry.handle, ( void * )entry.path_process );
				}
				continue;
			}
			if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
				gi.dprintf( "[NavAsync][Queue] Prepared A* for handle=%d path_process=%p\n", entry.handle, ( void * )entry.path_process );
			}
		}

		if ( entry.status != nav_request_status_t::Running ) {
			continue;
		}

		/**
		*    Advance the running incremental search and finalize when it completes.
		**/
		nav_a_star_status_t starStatus = entry.a_star.status;
		if ( starStatus == nav_a_star_status_t::Running ) {
			const int32_t expansionsToUse = std::min( expansionsPerRequest, remainingExpansions );
			if ( expansionsToUse <= 0 ) {
				break;
			}
			remainingExpansions -= expansionsToUse;
			starStatus = Nav_AStar_Step( &entry.a_star, expansionsToUse );
		}

		/**
		*    Track that we touched this request this frame.
		**/
		processed++;

		if ( starStatus == nav_a_star_status_t::Completed ) {
			const bool commitOk = NavRequest_FinalizePathForEntry( entry );
			entry.status = commitOk ? nav_request_status_t::Completed : nav_request_status_t::Failed;
			if ( !commitOk ) {
				NavRequest_HandleFailure( entry );
			}
			NavRequest_ClearProcessMarkers( entry );
		} else if ( starStatus == nav_a_star_status_t::Failed ) {
			entry.status = nav_request_status_t::Failed;
			NavRequest_HandleFailure( entry );
			NavRequest_ClearProcessMarkers( entry );
		}

		if ( entry.status != nav_request_status_t::Running ) {
			Nav_AStar_Reset( &entry.a_star );
		}
	}

	/**
 *    Remove any entries that reached a terminal state so handles can be reused.
	**/
	s_nav_request_queue.erase( std::remove_if( s_nav_request_queue.begin(), s_nav_request_queue.end(),
		[]( const nav_request_entry_t &entry ) {
			return entry.status == nav_request_status_t::Completed
				|| entry.status == nav_request_status_t::Failed
				|| entry.status == nav_request_status_t::Cancelled;
		} ),
		s_nav_request_queue.end() );
	const int32_t queueAfter = ( int32_t )s_nav_request_queue.size();
	NavRequest_LogQueueDiagnostics( queueBefore, queueAfter, processed, requestBudget, remainingExpansions );
}

/**
*    @brief    Prepare incremental A* state for a queued request entry.
*    @param    entry    Request entry to initialize before stepping.
*    @return   True when the search state successfully entered the Running phase.
**/
static bool NavRequest_PrepareAStarForEntry( nav_request_entry_t &entry ) {
	svg_nav_path_process_t *process = entry.path_process;
	if ( !process ) {
		return false;
	}

	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( !mesh ) {
		return false;
	}

    // Mark the owning path process so callers can inspect and (if necessary)
	// cancel the outstanding request. If another request arrives for the same
	// process it will refresh the existing entry rather than enqueue a duplicate.
	process->rebuild_in_progress = true;
	process->pending_request_handle = entry.handle;

	/**
	*    Align policy constraints with the agent hull profile before running A*.
	**/
	entry.resolved_policy = entry.policy;
  const nav_agent_profile_t agentProfile = SVG_Nav_BuildAgentProfileFromCvars();
	// Determine whether the mesh provides a valid agent hull to align with.
	const bool meshAgentValid = ( mesh->agent_maxs.z > mesh->agent_mins.z )
		&& ( mesh->agent_maxs.x > mesh->agent_mins.x )
		&& ( mesh->agent_maxs.y > mesh->agent_mins.y );
	// Validate the cvar-driven agent profile before using it as a fallback.
	const bool profileValid = ( agentProfile.maxs.z > agentProfile.mins.z )
		&& ( agentProfile.maxs.x > agentProfile.mins.x )
		&& ( agentProfile.maxs.y > agentProfile.mins.y );

	/**
	*    Resolve the agent bounds that should drive center offsets and traversal.
	*        Prefer mesh-authored hull sizes so navigation queries stay aligned.
	**/
	Vector3 resolvedAgentMins = entry.agent_mins;
	Vector3 resolvedAgentMaxs = entry.agent_maxs;
	// Prefer the mesh agent bounds when they are valid.
	if ( meshAgentValid ) {
		resolvedAgentMins = mesh->agent_mins;
		resolvedAgentMaxs = mesh->agent_maxs;
	} else if ( profileValid ) {
		// Fall back to the cvar profile when the mesh is unavailable.
		resolvedAgentMins = agentProfile.mins;
		resolvedAgentMaxs = agentProfile.maxs;
	}

	/**
	*    Update the resolved policy with the chosen agent profile and traversal limits.
	**/
	entry.resolved_policy.agent_mins = resolvedAgentMins;
	entry.resolved_policy.agent_maxs = resolvedAgentMaxs;
	entry.resolved_policy.max_step_height = meshAgentValid ? mesh->max_step : agentProfile.max_step_height;
	entry.resolved_policy.max_drop_height = agentProfile.max_drop_height;
	entry.resolved_policy.drop_cap = agentProfile.drop_cap;
	entry.resolved_policy.max_slope_deg = meshAgentValid ? mesh->max_slope_deg : agentProfile.max_slope_deg;

    /**
	*    Use entity/world origin coordinates directly.
	*        The navigation system now operates on center-based entity origins, so
	*        no feet-origin to nav-center conversion is required.
	**/
	const Vector3 start_center = entry.start;
	const Vector3 goal_center = entry.goal;
	const Vector3 agent_center_mins = resolvedAgentMins;
	const Vector3 agent_center_maxs = resolvedAgentMaxs;

	/**
	*    Resolve start/goal nodes using optional Z-layer blending.
	**/
	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};
	bool startResolved = false;
	bool goalResolved = false;
	if ( entry.resolved_policy.enable_goal_z_layer_blend ) {
		startResolved = Nav_FindNodeForPosition_BlendZ( mesh, start_center, start_center.z, goal_center.z, start_center,
			goal_center, entry.resolved_policy.blend_start_dist, entry.resolved_policy.blend_full_dist, &start_node, true );
		goalResolved = Nav_FindNodeForPosition_BlendZ( mesh, goal_center, start_center.z, goal_center.z, start_center,
			goal_center, entry.resolved_policy.blend_start_dist, entry.resolved_policy.blend_full_dist, &goal_node, true );
	} else {
		startResolved = Nav_FindNodeForPosition( mesh, start_center, start_center.z, &start_node, true );
		goalResolved = Nav_FindNodeForPosition( mesh, goal_center, goal_center.z, &goal_node, true );
	}
    if ( !startResolved || !goalResolved ) {
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Prep] node resolution failed for handle=%d start_ok=%d goal_ok=%d start_center=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
				entry.handle, startResolved ? 1 : 0, goalResolved ? 1 : 0,
				start_center.x, start_center.y, start_center.z,
				goal_center.x, goal_center.y, goal_center.z );

			// Additional diagnostics: check whether relevant world tiles exist and show mesh parameters.
			if ( mesh ) {
				double tileWorldSize = mesh->cell_size_xy * ( double )mesh->tile_size;
				const int32_t startTileX = ( int32_t )std::floor( start_center.x / tileWorldSize );
				const int32_t startTileY = ( int32_t )std::floor( start_center.y / tileWorldSize );
				const int32_t goalTileX = ( int32_t )std::floor( goal_center.x / tileWorldSize );
				const int32_t goalTileY = ( int32_t )std::floor( goal_center.y / tileWorldSize );
				nav_world_tile_key_t startKey{ startTileX, startTileY };
				nav_world_tile_key_t goalKey{ goalTileX, goalTileY };
				auto itStart = mesh->world_tile_id_of.find( startKey );
				auto itGoal = mesh->world_tile_id_of.find( goalKey );
				gi.dprintf( "[NavAsync][Prep] mesh: tileWorldSize=%.2f cell_size=%.2f tile_size=%d num_world_tiles=%d\n",
					( float )tileWorldSize, ( float )mesh->cell_size_xy, mesh->tile_size, ( int )mesh->world_tiles.size() );
				gi.dprintf( "[NavAsync][Prep] start tile=(%d,%d) found=%d goal tile=(%d,%d) found=%d mesh_agent_mins=(%.1f %.1f %.1f) mesh_agent_maxs=(%.1f %.1f %.1f) z_quant=%.2f\n",
					startTileX, startTileY, itStart != mesh->world_tile_id_of.end() ? 1 : 0,
					goalTileX, goalTileY, itGoal != mesh->world_tile_id_of.end() ? 1 : 0,
					mesh->agent_mins.x, mesh->agent_mins.y, mesh->agent_mins.z,
					mesh->agent_maxs.x, mesh->agent_maxs.y, mesh->agent_maxs.z,
					( float )mesh->z_quant );
			}
		}
		return false;
	}

	/**
	*    Diagnostics: report resolved start/goal nodes when async stats logging is enabled.
	**/
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][Prep] resolved nodes handle=%d start_node=(%.1f %.1f %.1f) goal_node=(%.1f %.1f %.1f) start_key=(leaf=%d tile=%d cell=%d layer=%d) goal_key=(leaf=%d tile=%d cell=%d layer=%d)\n",
			entry.handle,
			start_node.position.x, start_node.position.y, start_node.position.z,
			goal_node.position.x, goal_node.position.y, goal_node.position.z,
			start_node.key.leaf_index, start_node.key.tile_index, start_node.key.cell_index, start_node.key.layer_index,
			goal_node.key.leaf_index, goal_node.key.tile_index, goal_node.key.cell_index, goal_node.key.layer_index );
	}

	/**
	*    Restrict expansions to the coarse cluster route when available.
	**/
	std::vector<nav_tile_cluster_key_t> tileRoute;
	const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( mesh, start_center, goal_center, tileRoute );
	const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

    if ( !Nav_AStar_Init( &entry.a_star, mesh, start_node, goal_node, agent_center_mins, agent_center_maxs,
		&entry.resolved_policy, routeFilter, entry.path_process ) ) {
       if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Prep] Nav_AStar_Init failed for handle=%d start_node=(%.1f %.1f %.1f) goal_node=(%.1f %.1f %.1f) agent_center_mins=(%.1f %.1f %.1f) agent_center_maxs=(%.1f %.1f %.1f)\n",
				entry.handle,
				start_node.position.x, start_node.position.y, start_node.position.z,
				goal_node.position.x, goal_node.position.y, goal_node.position.z,
				agent_center_mins.x, agent_center_mins.y, agent_center_mins.z,
				agent_center_maxs.x, agent_center_maxs.y, agent_center_maxs.z );
			// Also print a short resolved policy summary to aid debugging of why A* init may fail.
			gi.dprintf( "[NavAsync][Prep] Resolved policy: waypoint_radius=%.1f max_step=%.1f max_drop=%.1f drop_cap=%.1f z_blend=%d\n",
				entry.resolved_policy.waypoint_radius,
				entry.resolved_policy.max_step_height,
				entry.resolved_policy.max_drop_height,
				entry.resolved_policy.drop_cap,
				entry.resolved_policy.enable_goal_z_layer_blend ? 1 : 0 );
		}
		return false;
	}

	entry.status = nav_request_status_t::Running;
    if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		// On successful prepare, emit a resolved policy summary so callers can
		// correlate policy/agent parameters with A* behavior.
		gi.dprintf( "[NavAsync][Prep] Prepared A* for handle=%d path_process=%p resolved_waypoint_radius=%.1f max_step=%.1f max_drop=%.1f drop_cap=%.1f z_blend=%d\n",
			entry.handle, ( void * )entry.path_process,
			entry.resolved_policy.waypoint_radius,
			entry.resolved_policy.max_step_height,
			entry.resolved_policy.max_drop_height,
			entry.resolved_policy.drop_cap,
			entry.resolved_policy.enable_goal_z_layer_blend ? 1 : 0 );
	}
	return true;
}

/**
*    @brief    Reset the owning path process markers after a queued request finishes.
**/
static void NavRequest_ClearProcessMarkers( nav_request_entry_t &entry ) {
	svg_nav_path_process_t *process = entry.path_process;
	if ( !process ) {
		return;
	}

	// Clear the rebuild markers but avoid stomping a newer pending handle.
	// Only clear the handle if it still refers to the entry being finalized.
	process->rebuild_in_progress = false;
	if ( process->pending_request_handle == entry.handle ) {
		process->pending_request_handle = 0;
	}
}

/**
*    @brief    Finalize the incremental search output and commit it back to the caller.
*    @param    entry    Request entry owning the completed state.
*    @return   True if the path was validated and stored.
**/
static bool NavRequest_FinalizePathForEntry( nav_request_entry_t &entry ) {
	svg_nav_path_process_t *process = entry.path_process;
	if ( !process ) {
		return false;
	}

	std::vector<Vector3> points;
	if ( !Nav_AStar_Finalize( &entry.a_star, &points ) ) {
		return false;
	}

	if ( points.empty() ) {
		return false;
	}

 /**
	* 	Enforce post-processed drop limits on the finalized waypoint list.
	**/
	const float maxDrop = ( entry.resolved_policy.drop_cap > 0.0f )
		? ( float )entry.resolved_policy.drop_cap
		: ( entry.resolved_policy.cap_drop_height ? ( float )entry.resolved_policy.max_drop_height : ( nav_drop_cap ? nav_drop_cap->value : 100.0f ) );
	bool dropLimitOk = true;
   const int32_t pointCount = ( int32_t )points.size();
	if ( pointCount > 1 ) {
        // Iterate segments to detect drops that exceed the configured limit.
		for ( int32_t i = 1; i < pointCount; ++i ) {
			const float dz = points[ i - 1 ].z - points[ i ].z;
			if ( dz > maxDrop ) {
				dropLimitOk = false;
				break;
			}
		}
	}
    if ( !dropLimitOk ) {
		gi.dprintf( "[DEBUG][NavAsync] Finalize: path rejected due to drop limits (handle=%d)\n", entry.handle );
		return false;
	}

 std::vector<Vector3> smoothedPoints = points;
	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( mesh && smoothedPoints.size() > 2 ) {
		/**
		*    Apply a greedy corridor string-pull plus a small lateral offset to
		*    reduce corner hugging without modifying the navmesh. This operates on
		*    nav-center waypoints before they are copied into the path process.
		**/
     const Vector3 agent_center_mins = entry.agent_mins;
		const Vector3 agent_center_maxs = entry.agent_maxs;

		std::vector<Vector3> stringPulled;
		stringPulled.reserve( smoothedPoints.size() );
		stringPulled.push_back( smoothedPoints.front() );
		size_t anchor = 0;
		const size_t totalPoints = smoothedPoints.size();
		while ( anchor + 1 < totalPoints ) {
			size_t chosen = anchor + 1;
			for ( size_t candidate = totalPoints - 1; candidate > anchor; --candidate ) {
				if ( Nav_CanTraverseStep_ExplicitBBox( mesh, smoothedPoints[ anchor ], smoothedPoints[ candidate ], agent_center_mins, agent_center_maxs, nullptr, &entry.resolved_policy ) ) {
					chosen = candidate;
					break;
				}
			}
			stringPulled.push_back( smoothedPoints[ chosen ] );
			anchor = chosen;
		}

		const float agent_half_width = std::max( fabsf( entry.agent_mins.x ), fabsf( entry.agent_maxs.x ) );
		const float max_lateral_offset = std::min( agent_half_width, 32.0f );
		for ( size_t idx = 1; idx + 1 < stringPulled.size(); ++idx ) {
			const Vector3 &prev = stringPulled[ idx - 1 ];
			const Vector3 &next = stringPulled[ idx + 1 ];
			Vector3 corridorCenter = QM_Vector3Add( prev, next ) * 0.5f;
			Vector3 deviation = QM_Vector3Subtract( corridorCenter, stringPulled[ idx ] );
			const float deviationLen = QM_Vector3Length( deviation );
			if ( deviationLen > 0.001f ) {
				const float offsetFrac = std::min( max_lateral_offset / deviationLen, 0.5f );
				stringPulled[ idx ] = QM_Vector3Add( stringPulled[ idx ], deviation * offsetFrac );
			}
		}

		if ( stringPulled.size() > 1 ) {
			smoothedPoints.swap( stringPulled );
		}
	}

 const bool committed = process->CommitAsyncPathFromPoints( smoothedPoints, entry.start, entry.goal, entry.resolved_policy );
	if ( committed ) {
		gi.dprintf( "[DEBUG][NavAsync] Finalize: commit succeeded (handle=%d points=%d)\n", entry.handle, ( int )points.size() );
	} else {
		gi.dprintf( "[DEBUG][NavAsync] Finalize: commit failed (handle=%d)\n", entry.handle );
	}
	return committed;
}

/**
*    @brief    Apply failure backoff penalties when incremental A* could not find a path.
**/
static void NavRequest_HandleFailure( nav_request_entry_t &entry ) {
	svg_nav_path_process_t *process = entry.path_process;
	if ( !process ) {
		return;
	}

	process->consecutive_failures++;
	process->last_failure_time = level.time;
	process->last_failure_pos = entry.goal;
	Vector3 toGoal = QM_Vector3Subtract( entry.goal, entry.start );
	process->last_failure_yaw = QM_Vector3ToYaw( toGoal );
	const int32_t powN = std::min( std::max( 0, process->consecutive_failures ), entry.resolved_policy.fail_backoff_max_pow );
	const QMTime extra = QMTime::FromMilliseconds( ( int32_t )entry.resolved_policy.fail_backoff_base.Milliseconds() * ( 1 << powN ) );
	process->backoff_until = level.time + extra;
	process->next_rebuild_time = std::max( process->next_rebuild_time, process->backoff_until );
    gi.dprintf( "[DEBUG][NavAsync] HandleFailure: handle=%d consecutive_failures=%d backoff_until=%lld\n", entry.handle, process->consecutive_failures, ( long long )process->backoff_until.Milliseconds() );
}

/**
*    @brief    Emit diagnostics summarizing the async navigation request queue state.
*    @note     Enabled via the `nav_nav_async_log_stats` cvar when troubleshooting queue behavior.
**/
static void NavRequest_LogQueueDiagnostics( int32_t queueBefore, int32_t queueAfter, int32_t processed, int32_t requestBudget, int32_t remainingExpansions ) {
	if ( !s_nav_nav_async_log_stats || s_nav_nav_async_log_stats->integer == 0 ) {
		return;
	}

	if ( queueBefore != 0 && queueAfter != 0 ) {
		gi.dprintf( "[NavAsync][Stats] queue_before=%d queue_after=%d processed=%d request_budget=%d expansions_remaining=%d queue_mode=%d\n",
			queueBefore,
			queueAfter,
			processed,
			requestBudget,
			remainingExpansions,
			nav_nav_async_queue_mode ? nav_nav_async_queue_mode->integer : 0 );
	}
}

/**
*    @brief    Determine if async navigation requests are enabled via cvar.
**/
bool SVG_Nav_IsAsyncNavEnabled( void ) {
	SVG_Nav_RequestQueue_RegisterCvars();
	return s_nav_nav_async_enable && s_nav_nav_async_enable->integer != 0;
}

/**
*    @brief    Retrieve the configured per-frame request budget.
**/
int32_t SVG_Nav_GetAsyncRequestBudget( void ) {
	SVG_Nav_RequestQueue_RegisterCvars();
	const int32_t budget = s_nav_requests_per_frame ? s_nav_requests_per_frame->integer : 1;
	return std::max( 1, budget );
}

/**
*    @brief    Retrieve the configured expansion budget for each request tick.
**/
int32_t SVG_Nav_GetAsyncRequestExpansions( void ) {
	SVG_Nav_RequestQueue_RegisterCvars();
	const int32_t budget = s_nav_expansions_per_request ? s_nav_expansions_per_request->integer : 1;
	return std::max( 1, budget );
}
