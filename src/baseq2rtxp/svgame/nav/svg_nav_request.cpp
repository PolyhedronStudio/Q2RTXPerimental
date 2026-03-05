/********************************************************************
*
*
*    SVGame: Navigation Request Queue Implementation
*
*
********************************************************************/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"
#include "svgame/nav/svg_nav_traversal_async.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

/**
 *    NOTE:
 *    This file's PrepareAStar logic has been refactored so that heavy node
 *    resolution and `Nav_AStar_Init` vector allocations run on the async
 *    worker thread. The main thread now performs only lightweight validation
 *    and enqueues a small payload for the worker. The worker produces an
 *    initialized `nav_a_star_state_t` and returns it to the main thread via
 *    the async done-callback where it is moved into the queue entry.
 **/

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

/**
*    @brief    Determine whether a request status is terminal.
*    @param    status    Status value to inspect.
*    @return   True when the status represents a completed/cancelled/failed request.
**/
static inline bool NavRequest_IsTerminalStatus( const nav_request_status_t status ) {
	/**
	*    Treat only terminal outcomes as finished.
	**/
	return status == nav_request_status_t::Completed
		|| status == nav_request_status_t::Cancelled
		|| status == nav_request_status_t::Failed;
}


/**
*    @brief    Convert an edge rejection enum value into a stable diagnostic name.
*    @param    reason    Rejection reason value.
*    @return   Constant string used in async queue diagnostics.
**/
static const char *NavRequest_EdgeRejectReasonToString( const nav_edge_reject_reason_t reason ) {
	switch ( reason ) {
	case nav_edge_reject_reason_t::None: return "None";
	case nav_edge_reject_reason_t::TileRouteFilter: return "TileRouteFilter";
	case nav_edge_reject_reason_t::NoNode: return "NoNode";
	case nav_edge_reject_reason_t::DropCap: return "DropCap";
	case nav_edge_reject_reason_t::StepTest: return "StepTest";
	case nav_edge_reject_reason_t::ObstructionJump: return "ObstructionJump";
	case nav_edge_reject_reason_t::Occupancy: return "Occupancy";
	case nav_edge_reject_reason_t::Other: return "Other";
	default: return "Unknown";
	}
}

/**
*    @brief    Emit per-reason A* edge rejection counters.
*    @param    prefix    Prefix tag for each emitted line.
*    @param    state     Search state containing rejection counters.
**/
static void NavRequest_LogEdgeRejectReasonCounters( const char *prefix, const nav_a_star_state_t &state ) {
	/**
	*    Print all configured reason buckets so failure diagnostics include both
	*    dominant and zero-count reasons.
	**/
	for ( int32_t reasonIndex = ( int32_t )nav_edge_reject_reason_t::None;
		reasonIndex <= ( int32_t )nav_edge_reject_reason_t::Other;
		reasonIndex++ ) {
		const nav_edge_reject_reason_t reason = ( nav_edge_reject_reason_t )reasonIndex;
		gi.dprintf( "%s edge_reject_reason[%d]=%d (%s)\n",
			prefix ? prefix : "[NavAsync][Diag]",
			reasonIndex,
			state.edge_reject_reason_counts[ reasonIndex ],
			NavRequest_EdgeRejectReasonToString( reason ) );
	}
}

static void NavRequest_LogQueueDiagnostics( int32_t queueBefore, int32_t queueAfter, int32_t processed, int32_t requestBudget, int32_t remainingExpansions );
static nav_request_entry_t *NavRequest_FindEntryByHandle( nav_request_handle_t handle );
static nav_request_entry_t *NavRequest_FindEntryForProcess( svg_nav_path_process_t *process );
static bool NavRequest_PrepareAStarForEntry( nav_request_entry_t &entry );
static void NavRequest_ClearProcessMarkers( nav_request_entry_t &entry );
static bool NavRequest_FinalizePathForEntry( nav_request_entry_t &entry );
static void NavRequest_HandleFailure( nav_request_entry_t &entry );

/**
*    @brief    Validate that an agent bbox is well-formed.
*    @param    mins    Minimum extents.
*    @param    maxs    Maximum extents.
*    @return   True when mins are strictly lower than maxs on all axes.
**/
static inline const bool NavRequest_AgentBoundsAreValid( const Vector3 &mins, const Vector3 &maxs ) {
	return ( maxs[ 0 ] > mins[ 0 ] ) && ( maxs[ 1 ] > mins[ 1 ] ) && ( maxs[ 2 ] > mins[ 2 ] );
}

// Forward worker callbacks for async queue.
static void NavRequest_Worker_DoInit( void *cb_arg );
static void NavRequest_Worker_Done( void *cb_arg );

/**
 *    Small payload passed to the worker thread when preparing A*.
 *    Contains only lightweight, stable inputs gathered on the main thread.
 **/
struct nav_request_prep_payload_t {
	nav_request_handle_t handle;
	uint32_t generation;

	// Feet-origin positions (caller-provided).
	Vector3 start_feet;
	Vector3 goal_feet;

	// Resolved policy and agent hull (copied on main thread to avoid mesh reads on worker).
	svg_nav_path_policy_t resolved_policy;
	Vector3 agent_mins;
	Vector3 agent_maxs;

	// Owning process pointer (readonly usage on worker for diagnostics / failure penalties).
	svg_nav_path_process_t *path_process = nullptr;

	// Worker results:
	bool init_success = false;
	nav_a_star_state_t *initialized_state = nullptr;
};

/**
*    @brief    Register cvars that control async request processing and diagnostics.
*    @note     Safe to invoke from SVG_Nav_Init after the navigation subsystem starts.
**/
void SVG_Nav_RequestQueue_RegisterCvars( void ) {
	if ( s_nav_nav_async_enable ) {
		return;
	}

	s_nav_nav_async_enable = gi.cvar( "nav_nav_async_enable", "1", 0 );
	s_nav_requests_per_frame = gi.cvar( "nav_requests_per_frame", "1280", 0 ); // <Q2RTXP>: WID: Increased from 40 times, for 32 monsters with 1 request each, to 40 times that for 1280 requests to allow more async processing per frame and reduce main-thread work when many entities are queuing requests.
	s_nav_expansions_per_request = gi.cvar( "nav_expansions_per_request", "2048", 0 ); // <Q2RTXP>: WID: Increased from 512
	nav_nav_async_queue_mode = gi.cvar( "nav_nav_async_queue_mode", "1", 0 );
	s_nav_nav_async_log_stats = gi.cvar( "nav_nav_async_log_stats", "1", 0 );

	// Reserve some reasonable default capacity to avoid repeated reallocations
	// when many entities enqueue navigation requests during startup or stress tests.
	s_nav_request_queue.reserve( 256 );
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
	const Vector3 &agent_maxs, bool force, double startIgnoreThreshold ) {
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
    // Debounce: avoid refreshing/prepping repeatedly within a very short time window
	// for the same process unless forced. This helps prevent per-frame prep work that
	// can cause single-frame hitches when many entities refresh every frame.
	// Increase debounce to reduce frequent per-frame prepare attempts from callers
	// that may still invoke refreshes too often. This value is intentionally
	// conservative to give the async worker time to initialize and start running.
	const QMTime minPrepInterval = 100_ms; // 100 ms debounce (tunable)
	if ( !force && pathProcess && pathProcess->last_prep_time > 0_ms ) {
		// Get the current time and compute the delta since the last prep attempt for this process.
		const QMTime now = level.time;
		const QMTime delta = now - pathProcess->last_prep_time;
		// Also compute the movement deltas since the last prep for both start and goal positions.
		const double dx = QM_Vector3LengthDP( QM_Vector3Subtract( start_origin, pathProcess->last_prep_start ) );
		const double dg = QM_Vector3LengthDP( QM_Vector3Subtract( goal_origin, pathProcess->last_prep_goal ) );
		const double moveThresh = 4.0; // Small movement threshold in units
		// If the time delta is below the threshold and the positions haven't moved significantly, skip the refresh.
		if ( delta < minPrepInterval && dx <= moveThresh && dg <= moveThresh ) {
			// Skip enqueue/refresh when consecutive calls are too frequent and positions haven't moved.
			if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
				gi.dprintf( "[NavAsync][Prep] Debounced request for ent_process=%p delta_ms=%lld dx=%.2f dg=%.2f\n",
					( void * )pathProcess, ( long long )delta.Milliseconds(), dx, dg );
			}
			return 0;
		}
	}

	if ( nav_request_entry_t *existing = NavRequest_FindEntryForProcess( pathProcess ) ) {
		/**
		*	When a non-terminal entry already exists for this process, avoid doing
		*	expensive/mutating refresh work when the caller hasn't moved meaningfully.
		*
		*	Implementation notes:
		*	 - If the entry is Queued or Running and both start/goal moved less than
		*	   'moveThresh' units we skip updating the entry entirely. This avoids
		*	   repeated "Refreshed existing entry" log spam and copying of resolved
		*	   policy/hull fields.
		*	 - We now bump the owning process's `request_generation` only when we
		*	   actually perform a refresh (or create a new entry). This keeps the
		*	   generation monotonic semantics local to real changes and avoids
		*	   needing to revert a bump when we early-skip refreshes.
		**/
      if ( existing->status == nav_request_status_t::Queued || existing->status == nav_request_status_t::Preparing || existing->status == nav_request_status_t::Running ) {
			const float moveThresh = 4.0f;
			const float moveThreshSqr = moveThresh * moveThresh;
			const float dx_sqr = QM_Vector3DistanceSqr( existing->start, start_origin );
			const float dg_sqr = QM_Vector3DistanceSqr( existing->goal, goal_origin );
			// If both start and goal moved less than threshold and caller did not force,
			// skip the refresh. `force` bypasses this cheap movement-skip so callers can
			// guarantee a refresh even for small movements.
			if ( !force && dx_sqr <= moveThreshSqr && dg_sqr <= moveThreshSqr ) {
				if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
					gi.dprintf( "[NavAsync][Queue] Skipped refresh (movement below threshold) for handle=%d ent_process=%p dx_sqr=%.2f dg_sqr=%.2f\n",
						existing->handle, ( void * )existing->path_process, dx_sqr, dg_sqr );
				}
				// Return the existing handle without mutating the queued entry or bumping generation.
				return existing->handle;
			}
		}

		// We will refresh the existing entry: bump the per-process generation now.
		pathProcess->request_generation++;
		// Never allow generation 0 to be considered valid for an in-flight request.
		if ( pathProcess->request_generation == 0 ) {
			pathProcess->request_generation = 1;
		}

		// Refresh existing entry parameters instead of creating a duplicate.
		// Update the existing entry's parameters in-place so a running search can
		// finish without being repeatedly reinitialized. If the entry is Running
		// and the caller requires an immediate replacement, mark it so the
		// runner can transition it to Queued after finishing the current step.
        existing->start = start_origin;
		existing->goal = goal_origin;
		existing->generation = pathProcess->request_generation;
		existing->policy = policy;
		existing->agent_mins = agent_mins;
		existing->agent_maxs = agent_maxs;
		existing->force = existing->force || force;
		// If a startIgnoreThreshold was provided by the caller, use it to
		// decide whether an in-flight Running entry should be refreshed.
		if ( existing->status == nav_request_status_t::Running ) {
			const double effectiveIgnore = startIgnoreThreshold > 0.0 ? startIgnoreThreshold : 0.0;
			if ( effectiveIgnore > 0.0 ) {
				const Vector3 refStart = ( existing->path_process != nullptr ? ( existing->path_process && existing->path_process->last_prep_time > 0_ms ? existing->path_process->last_prep_start : existing->path_process->path_start_position ) : start_origin );
				const double startDx = QM_Vector3LengthDP( QM_Vector3Subtract( start_origin, refStart ) );
				if ( startDx <= effectiveIgnore && !force ) {
					if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
						gi.dprintf( "[NavAsync][Queue] Suppressing running refresh for handle=%d startDx=%.2f <= ignore=%.2f\n",
							existing->handle, startDx, effectiveIgnore );
					}
					// Don't flip back to Queued; update params only.
					existing->needs_refresh = existing->needs_refresh || force;
					if ( existing->path_process ) {
						existing->path_process->last_prep_time = level.time;
						existing->path_process->last_prep_start = start_origin;
						existing->path_process->last_prep_goal = goal_origin;
					}
					return existing->handle;
				}
			}
			// Otherwise defer re-prep by marking needs_refresh; worker will be re-prepared after current search.
			existing->needs_refresh = existing->needs_refresh || force;
			existing->status = nav_request_status_t::Queued;
		} else {
			// If not running, mark queued so worker will prepare it normally.
			existing->status = nav_request_status_t::Queued;
		}
		// Record last prep time/positions on the owning process to enable debounce logic.
		if ( existing->path_process ) {
			existing->path_process->last_prep_time = level.time;
			existing->path_process->last_prep_start = start_origin;
			existing->path_process->last_prep_goal = goal_origin;
		}
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
	// Bump generation for the process because we're about to create a new queued request.
	pathProcess->request_generation++;
	// Never allow generation 0 to be considered valid for an in-flight request.
	if ( pathProcess->request_generation == 0 ) {
		pathProcess->request_generation = 1;
	}

	nav_request_handle_t handle = s_nav_request_next_handle++;
	if ( s_nav_request_next_handle <= 0 ) {
		s_nav_request_next_handle = 1;
	}

	nav_request_entry_t entry = {};
	entry.path_process = pathProcess;
	entry.start = start_origin;
	entry.goal = goal_origin;
	entry.generation = pathProcess->request_generation;
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

   if ( NavRequest_IsTerminalStatus( entry->status ) ) {
		return;
	}

    /**
	*    Cancellation wins immediately:
	*        - Mark terminal status.
	*        - Drop refresh intent.
	*        - Clear owner process markers now so pending checks stop immediately.
	**/
	entry->status = nav_request_status_t::Cancelled;
	entry->needs_refresh = false;
	NavRequest_ClearProcessMarkers( *entry );
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
     /**
		*    No-mesh hardening:
		*        Do not keep requests pending forever when a mesh is unavailable.
		*        Mark active entries cancelled and clear owner process markers so
		*        callers immediately stop reporting pending async state.
		**/
		if ( !s_nav_request_queue.empty() ) {
			for ( nav_request_entry_t &entry : s_nav_request_queue ) {
				if ( NavRequest_IsTerminalStatus( entry.status ) ) {
					continue;
				}

				// Cancel non-terminal work because it cannot progress without a mesh.
				entry.status = nav_request_status_t::Cancelled;
				entry.needs_refresh = false;
				NavRequest_ClearProcessMarkers( entry );
			}

			// Drop all terminal entries now that they were cancelled.
			s_nav_request_queue.erase( std::remove_if( s_nav_request_queue.begin(), s_nav_request_queue.end(),
				[]( const nav_request_entry_t &entry ) {
					return NavRequest_IsTerminalStatus( entry.status );
				} ),
				s_nav_request_queue.end() );

			if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
				gi.dprintf( "[NavAsync][Queue] Cancelled pending requests because navmesh is unavailable.\n" );
			}
		}

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
	const uint64_t queueBudgetMs = 8;

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

     if ( NavRequest_IsTerminalStatus( entry.status ) ) {
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

		/**
		*    Worker prep is still in-flight for this entry.
		*        Skip until the done-callback transitions it to Running.
		**/
		if ( entry.status == nav_request_status_t::Preparing ) {
			continue;
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
			// If a refresh was requested while this entry was running, defer
			// committing the just-completed result and instead re-queue the
			// entry so it will be re-prepared with the newer parameters.
			if ( entry.needs_refresh ) {
				if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
					gi.dprintf( "[NavAsync][Queue] Deferring commit for handle=%d due to pending refresh; re-queueing.\n", entry.handle );
				}
				// Clear the flag and reset the working A* state so it can be
				// re-initialized by the worker using the updated parameters.
				entry.needs_refresh = false;
				entry.status = nav_request_status_t::Queued;
			} else {
				const bool commitOk = NavRequest_FinalizePathForEntry( entry );
				entry.status = commitOk ? nav_request_status_t::Completed : nav_request_status_t::Failed;
				if ( !commitOk ) {
					NavRequest_HandleFailure( entry );
				}
				NavRequest_ClearProcessMarkers( entry );
			}
		} else if ( starStatus == nav_a_star_status_t::Failed ) {
			// If a refresh was requested while running, prefer re-queueing
			// over applying failure backoff so callers get a chance to retry
			// with updated parameters. Otherwise apply failure handling.
			if ( entry.needs_refresh ) {
				if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
					gi.dprintf( "[NavAsync][Queue] Re-queueing handle=%d after failure due to pending refresh.\n", entry.handle );
				}
				entry.needs_refresh = false;
				entry.status = nav_request_status_t::Queued;
			} else {
				entry.status = nav_request_status_t::Failed;
				NavRequest_HandleFailure( entry );
				NavRequest_ClearProcessMarkers( entry );
			}
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
*    @return   True when the search state successfully entered the Running phase OR
*              when worker has been enqueued to initialize the state. False indicates
*              an immediate failure that should mark the request terminal.
*
*    NOTE: This function was refactored to enqueue heavy work to the async worker:
*          - Main thread: compute resolved policy/agent hull and perform small sanity checks.
*          - Worker thread: perform feet->center conversion, node resolution, tile-route lookup,
*                           and call Nav_AStar_Init (allocations occur on worker).
*          - Worker done-callback: move-initialize `entry.a_star` and transition entry->Running.
**/
static bool NavRequest_PrepareAStarForEntry( nav_request_entry_t &entry ) {
   /**
	*    Only queued entries should enter worker-prep dispatch.
	**/
	if ( entry.status != nav_request_status_t::Queued ) {
		return false;
	}

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
	*    This is lightweight and deliberately kept on the main thread so the worker
	*    can rely on a stable resolved policy snapshot.
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
	*    This selection is cheap and done on the main thread.
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
	*    Store a snapshot on the entry so worker and finalizer see the same policy.
	**/
	entry.resolved_policy.agent_mins = resolvedAgentMins;
	entry.resolved_policy.agent_maxs = resolvedAgentMaxs;
	entry.resolved_policy.max_step_height = meshAgentValid ? mesh->max_step : agentProfile.max_step_height;
	entry.resolved_policy.max_drop_height = agentProfile.max_drop_height;
	entry.resolved_policy.max_drop_height_cap = agentProfile.max_drop_height_cap;
	entry.resolved_policy.max_slope_normal_z = meshAgentValid ? mesh->max_slope_normal_z : agentProfile.max_slope_normal_z;
	entry.resolved_policy.min_step_normal = entry.resolved_policy.max_slope_normal_z;

	/**
	*    Boundary guard:
	*        External callers provide feet-origin with a valid bbox. Reject malformed
	*        hulls before worker prep to avoid undefined conversion behavior.
	**/
	if ( !NavRequest_AgentBoundsAreValid( entry.resolved_policy.agent_mins, entry.resolved_policy.agent_maxs ) ) {
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Prep] invalid agent bounds handle=%d mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f)\n",
				entry.handle,
				entry.resolved_policy.agent_mins.x, entry.resolved_policy.agent_mins.y, entry.resolved_policy.agent_mins.z,
				entry.resolved_policy.agent_maxs.x, entry.resolved_policy.agent_maxs.y, entry.resolved_policy.agent_maxs.z );
		}
		return false;
	}

	// Emit diagnostic about chosen agent hull for this request when async logging is enabled.
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][Prep] handle=%d resolved_agent_mins=(%.1f %.1f %.1f) resolved_agent_maxs=(%.1f %.1f %.1f) use_mesh_agent=%d\n",
			entry.handle,
			resolvedAgentMins.x, resolvedAgentMins.y, resolvedAgentMins.z,
			resolvedAgentMaxs.x, resolvedAgentMaxs.y, resolvedAgentMaxs.z,
			meshAgentValid ? 1 : 0 );
	}

	/**
	*    Sanity: quick world-bounds check on main thread to avoid obvious worker work.
	*    This is intentionally tiny to avoid main-thread node lookups while catching
	*    requests that are clearly outside the nav extents.
	**/
	if ( entry.start.x < mesh->world_bounds.mins.x - 1.0 || entry.start.x > mesh->world_bounds.maxs.x + 1.0 ||
		 entry.start.y < mesh->world_bounds.mins.y - 1.0 || entry.start.y > mesh->world_bounds.maxs.y + 1.0 ||
		 entry.goal.x  < mesh->world_bounds.mins.x - 1.0 || entry.goal.x  > mesh->world_bounds.maxs.x + 1.0 ||
		 entry.goal.y  < mesh->world_bounds.mins.y - 1.0 || entry.goal.y  > mesh->world_bounds.maxs.y + 1.0 ) {
		// Out-of-bounds: immediate failure, no worker enqueue.
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Prep] immediate OOB failure handle=%d start=(%.1f,%.1f) goal=(%.1f,%.1f)\n",
				entry.handle, entry.start.x, entry.start.y, entry.goal.x, entry.goal.y );
		}
		return false;
	}

	/**
	*    Build a small payload for the worker thread. This contains only stable,
	*    copyable inputs so the worker can operate without touching the main-thread
	*    owned queue memory.
	**/
	nav_request_prep_payload_t *payload = ( nav_request_prep_payload_t * )gi.TagMallocz( sizeof( nav_request_prep_payload_t ), TAG_SVGAME_LEVEL );
	payload->handle = entry.handle;
	payload->generation = entry.generation;
	payload->start_feet = entry.start;
	payload->goal_feet = entry.goal;
	payload->resolved_policy = entry.resolved_policy;
	payload->agent_mins = resolvedAgentMins;
	payload->agent_maxs = resolvedAgentMaxs;
	payload->path_process = entry.path_process;

	/**
	*    Enqueue worker task. Use stack-local asyncwork_t which Com_QueueAsyncWork
	*    will copy via Z_CopyStruct internally. The cb_arg points to our heap
	*    payload which the worker and done-callback will manage.
	**/
	asyncwork_t work = {};
	work.work_cb = NavRequest_Worker_DoInit;
	work.done_cb = NavRequest_Worker_Done;
	work.cb_arg = payload;
	gi.Com_QueueAsyncWork( &work );

   /**
	*    Mark the request as preparing so queue processing does not dispatch
	*    duplicate worker init tasks while this generation is in-flight.
	**/
	entry.status = nav_request_status_t::Preparing;

	// Worker done-callback will transition to Running on success.
	return true;
}

/**
*    @brief    Worker thread function to perform heavy A* prep:
*              - Convert feet->center
*              - Resolve start/goal nodes
*              - Optionally compute tile-route
*              - Call Nav_AStar_Init (allocations happen here)
*
*    This runs on the async worker thread.
**/
static void NavRequest_Worker_DoInit( void *cb_arg ) {
	if ( !cb_arg ) {
		return;
	}
	nav_request_prep_payload_t *p = ( nav_request_prep_payload_t * )cb_arg;
	p->init_success = false;
	p->initialized_state = nullptr;

	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( !mesh ) {
		return;
	}

	// Convert feet-origin to nav-center using the resolved agent hull.
	const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, p->start_feet, &p->resolved_policy.agent_mins, &p->resolved_policy.agent_maxs );
	const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, p->goal_feet, &p->resolved_policy.agent_mins, &p->resolved_policy.agent_maxs );
	Q_assert( NavRequest_AgentBoundsAreValid( p->resolved_policy.agent_mins, p->resolved_policy.agent_maxs ) );
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][Boundary] feet->center handle=%d start_feet=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal_feet=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
			p->handle,
			p->start_feet.x, p->start_feet.y, p->start_feet.z,
			start_center.x, start_center.y, start_center.z,
			p->goal_feet.x, p->goal_feet.y, p->goal_feet.z,
			goal_center.x, goal_center.y, goal_center.z );
	}

	// Resolve start/goal nodes on the worker.
	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};
	bool start_ok = false;
	bool goal_ok = false;
	if ( p->resolved_policy.enable_goal_z_layer_blend ) {
		start_ok = Nav_FindNodeForPosition_BlendZ( mesh, start_center, start_center.z, goal_center.z, start_center,
							goal_center, p->resolved_policy.blend_start_dist, p->resolved_policy.blend_full_dist, &start_node, true );
		goal_ok = Nav_FindNodeForPosition_BlendZ( mesh, goal_center, start_center.z, goal_center.z, start_center,
							goal_center, p->resolved_policy.blend_start_dist, p->resolved_policy.blend_full_dist, &goal_node, true );
	} else {
		start_ok = Nav_FindNodeForPosition( mesh, start_center, start_center.z, &start_node, true );
		goal_ok = Nav_FindNodeForPosition( mesh, goal_center, goal_center.z, &goal_node, true );
	}

	if ( !start_ok || !goal_ok ) {
		// Node resolution failed.
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Worker] node resolution failed handle=%d start_ok=%d goal_ok=%d\n",
				p->handle, start_ok ? 1 : 0, goal_ok ? 1 : 0 );
		}
		return;
	}

	// Successful node resolution: emit coordinates for diagnostics when enabled.
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][Worker] node resolution success handle=%d start=(%.1f %.1f %.1f) start_node=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_node=(%.1f %.1f %.1f)\n",
			p->handle,
			p->start_feet.x, p->start_feet.y, p->start_feet.z,
			start_node.worldPosition.x, start_node.worldPosition.y, start_node.worldPosition.z,
			p->goal_feet.x, p->goal_feet.y, p->goal_feet.z,
			goal_node.worldPosition.x, goal_node.worldPosition.y, goal_node.worldPosition.z );
	}

 /**
	*    Optionally compute coarse tile route filter (can be expensive on some maps).
	*        This is policy-gated so debug entities can disable route restriction
	*        and isolate fine traversal (`StepTest`) behavior.
	**/
	std::vector<nav_tile_cluster_key_t> tileRoute;
	const bool routeFilterEnabled = p->resolved_policy.enable_cluster_route_filter;
	const bool hasTileRoute = routeFilterEnabled && SVG_Nav_ClusterGraph_FindRoute( mesh, start_center, goal_center, tileRoute );
	const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][Worker] route_filter enabled=%d has_route=%d handle=%d\n",
			routeFilterEnabled ? 1 : 0,
			hasTileRoute ? 1 : 0,
			p->handle );
	}

	// Allocate state on the worker and initialize it (this performs the heavy vector allocations).
	nav_a_star_state_t *workerState = new nav_a_star_state_t();
	if ( !Nav_AStar_Init( workerState, mesh, start_node, goal_node, p->agent_mins, p->agent_maxs,
		&p->resolved_policy, routeFilter, p->path_process ) ) {
		delete workerState;
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Worker] Nav_AStar_Init failed handle=%d\n", p->handle );
		}
		return;
	}

	// Success: hand state back to main thread via payload.
	p->initialized_state = workerState;
	p->init_success = true;
}

/**
*    @brief    Done-callback invoked on the main thread after worker finished.
*    This moves the worker-initialized `nav_a_star_state_t` into the queue entry
*    without performing heavy allocations on the main thread.
**/
static void NavRequest_Worker_Done( void *cb_arg ) {
	if ( !cb_arg ) {
		return;
	}
	nav_request_prep_payload_t *p = ( nav_request_prep_payload_t * )cb_arg;

	// Find the queued entry by handle and ensure generation still matches.
	nav_request_entry_t *entry = NavRequest_FindEntryByHandle( p->handle );
	if ( !entry ) {
		// No entry to apply to; free worker state if we allocated one.
		if ( p->initialized_state ) {
			delete p->initialized_state;
		}
		gi.TagFree( p );
		return;
	}

	/**
	*    Cancellation/terminal guard:
	*        If this entry already reached a terminal status, drop worker output.
	**/
	if ( NavRequest_IsTerminalStatus( entry->status ) ) {
		if ( p->initialized_state ) {
			delete p->initialized_state;
		}
		NavRequest_ClearProcessMarkers( *entry );
		gi.TagFree( p );
		return;
	}

	// Stale-result guard: ensure the generation hasn't changed since payload creation.
	if ( entry->generation != p->generation ) {
		if ( p->initialized_state ) {
			delete p->initialized_state;
		}
		gi.TagFree( p );
		return;
	}

	/**
	*    Queue-state guard:
	*        Accept worker prep only while this request generation is still in
	*        pre-run states. Any other state indicates a superseded transition.
	**/
	if ( entry->status != nav_request_status_t::Preparing && entry->status != nav_request_status_t::Queued ) {
		if ( p->initialized_state ) {
			delete p->initialized_state;
		}
		gi.TagFree( p );
		return;
	}

	// If worker failed to initialize, mark entry failed and apply backoff.
	if ( !p->init_success || !p->initialized_state ) {
		entry->status = nav_request_status_t::Failed;
		// Keep resolved_policy on entry (it was copied earlier) so HandleFailure has the data it needs.
		NavRequest_HandleFailure( *entry );
        NavRequest_ClearProcessMarkers( *entry );
		gi.TagFree( p );
		return;
	}

    // Move the worker-initialized state into the entry using move semantics
	// so the bulk vector storage transfers without reallocation on this thread.
	entry->a_star = std::move( *p->initialized_state );
	// WorkerState memory no longer needed (moved-from). Delete the heap object.
	delete p->initialized_state;
	p->initialized_state = nullptr;

	// Defensive normalization: rebind internal non-owning pointers to point
	// into the moved-in storage. Prefer the struct accessor when available.
	entry->a_star.RebindInternalPointers();

	// Ensure entry has the same resolved policy snapshot used by worker.
	entry->resolved_policy = p->resolved_policy;
	// Also ensure entry uses the same agent hull snapshot.
	entry->agent_mins = p->agent_mins;
	entry->agent_maxs = p->agent_maxs;

	// Transition to Running so the main thread can call Nav_AStar_Step.
	entry->status = nav_request_status_t::Running;

	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][Done] Prepared A* on main thread by moving worker state handle=%d (gen=%u) path_process=%p\n",
			entry->handle, entry->generation, ( void * )entry->path_process );
	}

	// Free payload memory allocated during prepare.
	gi.TagFree( p );
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

	/**
	*    Stale-result guard:
	*        Async requests can be refreshed/replaced while an older request is still
	*        running (see `SVG_Nav_RequestPathAsync` replace semantics).
	*
	*        If an older request finishes after a newer goal/start has already been
	*        requested, committing the older result will cause visible path
	*        oscillation ("ping-pong"), especially when entities switch between
	*        breadcrumb pursuit and direct pursuit.
	*
	*        To prevent this, discard results when:
	*            - The path process now refers to a different pending handle (newer work).
	*            - The path process's latest desired goal is meaningfully different.
	*
	*        @note    This is intentionally conservative: a discarded commit will be
	*        re-requested naturally by the caller on the next rebuild tick.
	**/
	// If the process has been updated to track a newer request generation, do not commit.
	// This is the primary stale-result guard (deterministic, no distance heuristics).
	if ( process->request_generation != entry.generation ) {
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[DEBUG][NavAsync] Finalize: discarded stale generation result (handle=%d gen=%u current_gen=%u)\n",
				entry.handle, entry.generation, process->request_generation );
		}
		return false;
	}

	// Secondary guard: if the process has been updated to track a newer request handle, do not commit.
	if ( process->pending_request_handle != 0 && process->pending_request_handle != entry.handle ) {
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[DEBUG][NavAsync] Finalize: discarded stale result (handle=%d newer_handle=%d)\n",
				entry.handle, process->pending_request_handle );
		}
		return false;
	}

	std::vector<Vector3> points;
    if ( !Nav_AStar_Finalize( &entry.a_star, &points ) ) {
		// Emit temporary diagnostic describing A* state when finalize fails.
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			gi.dprintf( "[NavAsync][Finalize] Nav_AStar_Finalize failed handle=%d status=%d nodes=%d max_nodes=%d neighbor_tries=%d no_node=%d edge_reject=%d tile_filter_reject=%d stagnation=%d hit_budget=%d hit_stagnation=%d\n",
				entry.handle,
				( int )entry.a_star.status,
				( int )entry.a_star.nodes.size(),
				entry.a_star.max_nodes,
				entry.a_star.neighbor_try_count,
				entry.a_star.no_node_count,
				entry.a_star.edge_reject_count,
				entry.a_star.tile_filter_reject_count,
				entry.a_star.stagnation_count,
				entry.a_star.hit_time_budget ? 1 : 0,
				entry.a_star.hit_stagnation_limit ? 1 : 0 );
           // Include per-reason edge rejection counters to show why expansion failed.
			NavRequest_LogEdgeRejectReasonCounters( "[NavAsync][Finalize]", entry.a_star );
		}
		return false;
	}
	// Diagnostic: compute and print the feet->center conversion used for this completed entry.
	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( mesh && s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, entry.start, &entry.agent_mins, &entry.agent_maxs );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, entry.goal, &entry.agent_mins, &entry.agent_maxs );
		gi.dprintf( "[NavAsync][Finalize] handle=%d start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
			entry.handle,
			entry.start.x, entry.start.y, entry.start.z,
			start_center.x, start_center.y, start_center.z,
			entry.goal.x, entry.goal.y, entry.goal.z,
			goal_center.x, goal_center.y, goal_center.z );
	}


	if ( points.empty() ) {
		return false;
	}

	/**
	*    Enforce post-processed drop limits on the finalized waypoint list.
	**/
	const float maxDrop = ( entry.resolved_policy.max_drop_height_cap > 0.0f )
		? ( float )entry.resolved_policy.max_drop_height_cap
		: ( entry.resolved_policy.enable_max_drop_height_cap ? ( float )entry.resolved_policy.max_drop_height : ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 100.0f ) );
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
       // Emit reason-level counters here as well so commit-time failures still include rejection breakdown.
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			NavRequest_LogEdgeRejectReasonCounters( "[NavAsync][Finalize][CommitFail]", entry.a_star );
		}
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

	// Temporary diagnostic: print A* internal counters when an async request fails.
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][HandleFailure] handle=%d status=%d nodes=%d max_nodes=%d neighbor_tries=%d no_node=%d edge_reject=%d tile_filter_reject=%d stagnation=%d hit_budget=%d hit_stagnation=%d\n",
			entry.handle,
			( int )entry.a_star.status,
			( int )entry.a_star.nodes.size(),
			entry.a_star.max_nodes,
			entry.a_star.neighbor_try_count,
			entry.a_star.no_node_count,
			entry.a_star.edge_reject_count,
			entry.a_star.tile_filter_reject_count,
			entry.a_star.stagnation_count,
			entry.a_star.hit_time_budget ? 1 : 0,
			entry.a_star.hit_stagnation_limit ? 1 : 0 );

		/**
		*    Emit a concise structured dump of the resolved traversal policy and
		*    agent hull used for this failed request. This helps quickly identify
		*    mismatches between policy limits and the navmesh that cause edge
		*    rejection during expansion.
		**/
		gi.dprintf( "[NavAsync][HandleFailure][Detail] handle=%d agent_mins=(%.1f %.1f %.1f) agent_maxs=(%.1f %.1f %.1f) policy: max_step=%.1f min_step=%.1f max_drop=%.1f max_drop_height_cap=%.1f cap_drop=%d allow_jump=%d max_slope=%.1f waypoint_radius=%.1f backoff_base_ms=%d backoff_max_pow=%d\n",
			entry.handle,
			entry.agent_mins.x, entry.agent_mins.y, entry.agent_mins.z,
			entry.agent_maxs.x, entry.agent_maxs.y, entry.agent_maxs.z,
			entry.resolved_policy.max_step_height,
			entry.resolved_policy.min_step_height,
			entry.resolved_policy.max_drop_height,
            entry.resolved_policy.max_drop_height_cap,
			entry.resolved_policy.enable_max_drop_height_cap ? 1 : 0,
			entry.resolved_policy.allow_small_obstruction_jump ? 1 : 0,
			entry.resolved_policy.max_slope_normal_z,
			entry.resolved_policy.waypoint_radius,
			( int )entry.resolved_policy.fail_backoff_base.Milliseconds(),
			entry.resolved_policy.fail_backoff_max_pow );

		// Additional diagnostics: report original feet-origin positions, converted
		// nav-center positions, and summarized node information so callers can
		// correlate failures with agent/profile mismatches or local connectivity.
		{
			const nav_a_star_state_t &s = entry.a_star;
			const nav_mesh_t *mesh = g_nav_mesh.get();
			Vector3 start_center = mesh ? SVG_Nav_ConvertFeetToCenter( mesh, entry.start, &entry.agent_mins, &entry.agent_maxs ) : entry.start;
			Vector3 goal_center = mesh ? SVG_Nav_ConvertFeetToCenter( mesh, entry.goal, &entry.agent_mins, &entry.agent_maxs ) : entry.goal;
			gi.dprintf( "[NavAsync][HandleFailure][Pos] handle=%d start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
				entry.handle,
				entry.start.x, entry.start.y, entry.start.z,
				start_center.x, start_center.y, start_center.z,
				entry.goal.x, entry.goal.y, entry.goal.z,
				goal_center.x, goal_center.y, goal_center.z );

			// If the worker initialized node refs, print their positions for quick inspection.
			gi.dprintf( "[NavAsync][HandleFailure][Nodes] start_node=(%.1f %.1f %.1f) goal_node=(%.1f %.1f %.1f) nodes_explored=%d\n",
				s.start_node.worldPosition.x, s.start_node.worldPosition.y, s.start_node.worldPosition.z,
				s.goal_node.worldPosition.x, s.goal_node.worldPosition.y, s.goal_node.worldPosition.z,
				( int )s.nodes.size() );

			// Provide a concise interpretation when all neighbor attempts were rejected
			// which commonly indicates edge validation (step/slope/drop/clearance) issues.
			if ( ( int )s.nodes.size() <= 1 ) {
				if ( s.neighbor_try_count > 0 && s.edge_reject_count >= s.neighbor_try_count ) {
					gi.dprintf( "[NavAsync][HandleFailure][Reason] All neighbor attempts were rejected by edge validation (edge_reject=%d neighbor_tries=%d). Check agent hull, max_step, max_drop, and max_slope settings.\n",
						s.edge_reject_count, s.neighbor_try_count );
				} else if ( s.neighbor_try_count > 0 && s.tile_filter_reject_count >= s.neighbor_try_count ) {
					gi.dprintf( "[NavAsync][HandleFailure][Reason] All neighbor attempts were rejected by the tile-route filter (tile_filter_reject=%d neighbor_tries=%d).\n",
						s.tile_filter_reject_count, s.neighbor_try_count );
				} else if ( s.neighbor_try_count > 0 && s.no_node_count >= s.neighbor_try_count ) {
					gi.dprintf( "[NavAsync][HandleFailure][Reason] No neighbor nodes were found around the start position (no_node=%d neighbor_tries=%d). This may indicate sparse or missing navmesh coverage.\n",
						s.no_node_count, s.neighbor_try_count );
				}
			}

			// Summarize rejection counters for quick grep/automation.
			gi.dprintf( "[NavAsync][HandleFailure][Counts] neighbor_tries=%d no_node=%d edge_reject=%d tile_filter_reject=%d stagnation=%d\n",
				( int )s.neighbor_try_count, s.no_node_count, s.edge_reject_count, s.tile_filter_reject_count, s.stagnation_count );
           // Emit reason-bucket counters for edge rejection to aid root-cause analysis.
			NavRequest_LogEdgeRejectReasonCounters( "[NavAsync][HandleFailure]", s );

		}
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
