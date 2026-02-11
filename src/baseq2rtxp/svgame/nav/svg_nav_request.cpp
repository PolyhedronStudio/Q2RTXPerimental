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
static cvar_t *s_nav_nav_async_log_stats = nullptr;

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
	s_nav_expansions_per_request = gi.cvar( "nav_expansions_per_request", "64", 0 );
	nav_nav_async_queue_mode = gi.cvar( "nav_nav_async_queue_mode", "1", 0 );
	s_nav_nav_async_log_stats = gi.cvar( "nav_nav_async_log_stats", "0", 0 );
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

		if ( entry.status == nav_request_status_t::Completed || entry.status == nav_request_status_t::Cancelled ) {
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
	if ( nav_request_entry_t *existing = NavRequest_FindEntryForProcess( pathProcess ) ) {
		existing->start = start_origin;
		existing->goal = goal_origin;
		existing->policy = policy;
		existing->agent_mins = agent_mins;
		existing->agent_maxs = agent_maxs;
		existing->force = existing->force || force;
		existing->status = nav_request_status_t::Queued;
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
	*    Budgeting: constrain work by both request throttles and expansion caps.
	**/
	const int32_t requestBudget = SVG_Nav_GetAsyncRequestBudget();
	const int32_t expansionsPerRequest = SVG_Nav_GetAsyncRequestExpansions();
	const int64_t totalExpansions = ( int64_t )expansionsPerRequest * requestBudget;
	int32_t remainingExpansions = ( int32_t )std::min( totalExpansions, ( int64_t )std::numeric_limits<int32_t>::max() );
	int32_t processed = 0;
	const int32_t queueBefore = ( int32_t )s_nav_request_queue.size();
	const uint64_t queueStartMs = gi.Com_GetSystemMilliseconds();
	const uint64_t queueBudgetMs = 2;

	/**
	*    Iterate queue entries while honoring request, expansion, and time budgets.
	**/
	for ( nav_request_entry_t &entry : s_nav_request_queue ) {
		if ( processed >= requestBudget || remainingExpansions <= 0 ) {
			break;
		}
		const uint64_t queueElapsedMs = gi.Com_GetSystemMilliseconds() - queueStartMs;
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
				continue;
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

	process->rebuild_in_progress = true;
	process->pending_request_handle = entry.handle;

	/**
	*    Align policy constraints with the agent hull profile before running A*.
	**/
   entry.resolved_policy = entry.policy;
	const nav_agent_profile_t agentProfile = SVG_Nav_BuildAgentProfileFromCvars();
	entry.resolved_policy.agent_mins = agentProfile.mins;
	entry.resolved_policy.agent_maxs = agentProfile.maxs;
	entry.resolved_policy.max_step_height = agentProfile.max_step_height;
	entry.resolved_policy.max_drop_height = agentProfile.max_drop_height;
	entry.resolved_policy.drop_cap = agentProfile.drop_cap;
	entry.resolved_policy.max_slope_deg = agentProfile.max_slope_deg;

	const float centerOffsetZ = ( entry.agent_mins.z + entry.agent_maxs.z ) * 0.5f;
	const Vector3 start_center = QM_Vector3Add( entry.start, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
	const Vector3 goal_center = QM_Vector3Add( entry.goal, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
	const Vector3 agent_center_mins = QM_Vector3Subtract( entry.agent_mins, Vector3{ 0.0f, 0.0f, centerOffsetZ } );
	const Vector3 agent_center_maxs = QM_Vector3Subtract( entry.agent_maxs, Vector3{ 0.0f, 0.0f, centerOffsetZ } );

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
		return false;
	}

	/**
	*    Restrict expansions to the coarse cluster route when available.
	**/
	std::vector<nav_tile_cluster_key_t> tileRoute;
	const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( mesh, start_center, goal_center, tileRoute );
	const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

	if ( !Nav_AStar_Init( &entry.a_star, mesh, start_node, goal_node, agent_center_mins, agent_center_maxs,
		&entry.resolved_policy, routeFilter, entry.path_process ) ) {
		return false;
	}

	entry.status = nav_request_status_t::Running;
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
		return false;
	}

	const float centerOffsetZ = ( entry.agent_mins.z + entry.agent_maxs.z ) * 0.5f;
	return process->CommitAsyncPathFromPoints( points, entry.start, entry.goal, centerOffsetZ, entry.resolved_policy );
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
}

/**
*    @brief    Emit diagnostics summarizing the async navigation request queue state.
*    @note     Enabled via the `nav_nav_async_log_stats` cvar when troubleshooting queue behavior.
**/
static void NavRequest_LogQueueDiagnostics( int32_t queueBefore, int32_t queueAfter, int32_t processed, int32_t requestBudget, int32_t remainingExpansions ) {
	if ( !s_nav_nav_async_log_stats || s_nav_nav_async_log_stats->integer == 0 ) {
		return;
	}

	gi.dprintf( "[NavAsync][Stats] queue_before=%d queue_after=%d processed=%d request_budget=%d expansions_remaining=%d queue_mode=%d\n",
		queueBefore,
		queueAfter,
		processed,
		requestBudget,
		remainingExpansions,
		nav_nav_async_queue_mode ? nav_nav_async_queue_mode->integer : 0 );
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
