/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
*
*	SVGame: Navigation Request Queue Implementation
*
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"
#include "svgame/nav/svg_nav_traversal_async.h"

/**
*	@brief	Temporary recovery forward declarations for shared nav lookup helpers used by the async request queue.
*	@note	These mirror the shared nav implementations so the queue can compile while shared header visibility is restored.
**/
const bool Nav_FindNodeForPosition_BlendZ( const nav_mesh_t *mesh, const Vector3 &position, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist,
	nav_node_ref_t *out_node, bool allow_fallback );
const bool Nav_FindNodeForPosition( const nav_mesh_t *mesh, const Vector3 &position, double desired_z,
	nav_node_ref_t *out_node, bool allow_fallback );

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>
#include <cmath>

/**
* NOTE:
* This file's PrepareAStar logic has been refactored so that heavy node
* resolution and `Nav_AStar_Init` vector allocations run on the async
* worker thread. The main thread now performs only lightweight validation
* and enqueues a small payload for the worker. The worker produces an
* initialized `nav_a_star_state_t` and returns it to the main thread via
* the async done-callback where it is moved into the queue entry.
 **/

//! Container holding queued and in-flight navigation requests.
static std::vector<nav_request_entry_t> s_nav_request_queue = {};

//! Completed worker payloads awaiting main-thread queue application.
static std::vector<struct nav_request_prep_payload_t *> s_nav_completed_payloads = {};

//! Constant-time lookup from request handle to the current queue slot.
static std::unordered_map<nav_request_handle_t, size_t> s_nav_request_handle_lookup = {};

//! Value used to issue unique handles per request.
static nav_request_handle_t s_nav_request_next_handle = 1;

//! Round-robin cursor used to prevent early queue entries from monopolizing per-frame service.
static size_t s_nav_request_round_robin_cursor = 0;

//! Mutex protecting the completed-payload producer/consumer handoff list.
static std::mutex s_nav_completed_payload_mutex;

//! Toggle controlling whether path rebuilds should be enqueued via the async request queue.
cvar_t *nav_nav_async_queue_mode = nullptr;

//! Optional debug toggle for emitting async queue statistics.
cvar_t *s_nav_nav_async_log_stats = nullptr;

//! Optional cvar to limit async worker attempts at direct LOS shortcuts based on estimated sample counts.
extern cvar_t *nav_direct_los_attempt_max_samples;

//! Master toggle cvar for async request processing.
static cvar_t *s_nav_nav_async_enable = nullptr;

//! Per-frame request budget cvar.
static cvar_t *s_nav_requests_per_frame = nullptr;

//! Per-frame prepare-request budget cvar.
static cvar_t *s_nav_prepare_requests_per_frame = nullptr;

//! Per-frame running-step request budget cvar.
static cvar_t *s_nav_step_requests_per_frame = nullptr;

//! Per-request expansion budget cvar.
static cvar_t *s_nav_expansions_per_request = nullptr;

//! Queue-level budget for queued-entry prepare work.
static cvar_t *s_nav_prepare_budget_ms = nullptr;

//! Queue-level budget for running-entry stepping work.
static cvar_t *s_nav_step_budget_ms = nullptr;

//! Enables an optional second late-frame queue service pass after entity think completes.
static cvar_t *s_nav_late_frame_tick = nullptr;

//! Maximum consecutive async request failures recorded for a single path process.
static constexpr int32_t NAV_REQUEST_MAX_CONSECUTIVE_FAILURES = 20;

//! Number of edge-reject reason buckets recorded by A* so diagnostics can snapshot them.
//static constexpr int32_t NAV_EDGE_REJECT_REASON_BUCKETS = ( ( int32_t )nav_edge_reject_reason_t::Other - ( int32_t )nav_edge_reject_reason_t::None + 1 );
//static_assert( NAV_EDGE_REJECT_REASON_BUCKETS > 0, "invalid NAV_EDGE_REJECT_REASON_BUCKETS" );

//! Worker-side expansion budget used when running A* to completion on the async worker thread.
//! Controls how many node expansions a worker may perform when attempting to finish a path.
static cvar_t *s_nav_worker_expansions_per_run = nullptr;
//! Worker-side time budget in milliseconds for running A* on the async worker thread (0 = no cap).
//! Limits how long the worker may spend on a single request before yielding.
static cvar_t *s_nav_worker_budget_ms = nullptr;

/**
*	@brief    Determine whether a request status is terminal.
*	@param    status    Status value to inspect.
*	@return   True when the status represents a completed/cancelled/failed request.
**/
static inline bool NavRequest_IsTerminalStatus( const nav_request_status_t status ) {
	/**
	*	Treat only terminal outcomes as finished.
	**/
	return status == nav_request_status_t::Completed
		|| status == nav_request_status_t::Cancelled
		|| status == nav_request_status_t::Failed;
}

/**
 *	@brief    Find the dense-vector index for a queued entry by handle.
 *	@param    handle    Handle returned when the request was enqueued.
 *	@return   std::optional<size_t> index into s_nav_request_queue when found, std::nullopt otherwise.
 *
 *	@note     This helper exists so callers can avoid holding raw pointers into
 *	          `s_nav_request_queue` across operations that may reallocate or
 *	          compact the vector. Callers should resolve an index and then use
 *	          that index to access the vector element as-needed (re-querying
 *	          if the vector may have been mutated in-between).
 **/
static void NavRequest_RebuildHandleLookup( void );

static std::optional<size_t> NavRequest_FindEntryIndexByHandle( nav_request_handle_t handle ) {
	if ( handle <= 0 ) {
		return std::nullopt;
	}

	if ( const auto found = s_nav_request_handle_lookup.find( handle ); found != s_nav_request_handle_lookup.end() ) {
		const size_t index = found->second;
		if ( index < s_nav_request_queue.size() && s_nav_request_queue[ index ].handle == handle ) {
			return index;
		}
	}

	NavRequest_RebuildHandleLookup();
	if ( const auto found = s_nav_request_handle_lookup.find( handle ); found != s_nav_request_handle_lookup.end() ) {
		const size_t index = found->second;
		if ( index < s_nav_request_queue.size() && s_nav_request_queue[ index ].handle == handle ) {
			return index;
		}
	}

	return std::nullopt;
}


/**
*	@brief    Convert an edge rejection enum value into a stable diagnostic name.
*	@param    reason    Rejection reason value.
*	@return   Constant string used in async queue diagnostics.
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
*	@brief    Emit per-reason A*	edge rejection counters.
*	@param    prefix    Prefix tag for each emitted line.
*	@param    state     Search state containing rejection counters.
**/
static void NavRequest_LogEdgeRejectReasonCounters( const char *prefix, const nav_a_star_state_t &state ) {
	/**
	*	Print all configured reason buckets so failure diagnostics include both
	*	dominant and zero-count reasons.
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
static void NavRequest_RebuildHandleLookup( void );
static void NavRequest_DestroyPrepPayload( struct nav_request_prep_payload_t *payload );
static void NavRequest_EnqueueCompletedPayload( struct nav_request_prep_payload_t *payload );
static void NavRequest_DrainCompletedPayloads( std::vector<nav_request_prep_payload_t *> &payloads );
static void NavRequest_ApplyCompletedPayload( struct nav_request_prep_payload_t *payload );
static nav_request_entry_t *NavRequest_FindEntryByHandle( nav_request_handle_t handle );
static nav_request_entry_t *NavRequest_FindEntryForProcess( svg_nav_path_process_t *process );
static bool NavRequest_PrepareAStarForEntry( nav_request_entry_t &entry );
static void NavRequest_ClearProcessMarkers( nav_request_entry_t &entry );
static bool NavRequest_FinalizePathForEntry( nav_request_entry_t &entry );
static void NavRequest_HandleFailure( nav_request_entry_t &entry );
static bool NavRequest_GoalMovedMaterially( const Vector3 &current_goal, const Vector3 &incoming_goal,
	const svg_nav_path_policy_t &policy, const double fallback_refresh_threshold );

/**
*	@brief    Decide whether the async worker should attempt a direct LOS shortcut for this query.
*	@param    mesh         Navigation mesh supplying LOS sample spacing.
*	@param    start_node   Canonical LOS start node.
*	@param    goal_node    Canonical LOS goal node.
*	@return   True when the configured LOS sample budget allows the direct shortcut attempt.
*	@note     Kept local to the async request implementation so does not widen the public traversal API.
**/
static bool NavRequest_ShouldAttemptDirectLosShortcut( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node ) {
	/**
	*	Sanity checks: require mesh data before estimating LOS sample cost.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	*	Treat zero or negative limits as "always attempt" so tuning can disable this gate explicitly.
	**/
	const int32_t maxSamples = nav_direct_los_attempt_max_samples ? nav_direct_los_attempt_max_samples->integer : NAV_DEFAULT_DIRECT_LOS_ATTEMPT_MAX_SAMPLES;
	if ( maxSamples <= 0 ) {
		return true;
	}

	/**
	*	Estimate LOS sampling cost:
	*	 - `sampleStep` is the spacing between LOS samples. Prefer the mesh cell size when positive,
	*	   otherwise fall back to 4.0. Finally enforce a minimum of 1.0 to avoid excessively dense sampling.
	*	 - `distance` is the Euclidean length between canonical world positions used to approximate sample count.
	*	 - `sampleCount` is ceil(distance / sampleStep) clamped to [1, 16384].
	*
	*	These choices deliberately mirror the DDA LOS backend's sample-spacing logic so the async gate
	*	behaves consistently with the real LOS cost.
	**/
	// Compute a robust sample spacing value derived from the navmesh cell size.
	const double sampleStep = std::max( 1.0, mesh->cell_size_xy > 0.0 ? mesh->cell_size_xy : 4.0 );

	// Vector from start to goal in world-space.
	const Vector3 delta = QM_Vector3Subtract( goal_node.worldPosition, start_node.worldPosition );

	// Euclidean distance between endpoints. Use sqrt of squared components for numeric stability.
	const double distance = QM_Vector3LengthDP( delta ); // std::sqrt( ( delta.x * delta.x ) + ( delta.y * delta.y ) + ( delta.z * delta.z ) );

	// Estimate number of DDA samples along the segment.
	// - Very short distances use a single sample.
	// - Otherwise ceil(distance / sampleStep) then clamp to a sane upper bound.
	const int32_t sampleCount = ( distance <= 0.001 ) ? 1 : std::clamp( ( int32_t )std::ceil( distance / sampleStep ), 1, NAV_DEFAULT_DIRECT_LOS_ATTEMPT_MAX_SAMPLES );

	// Allow the direct LOS attempt only when the estimated sample count fits within the configured budget.
	return sampleCount <= maxSamples;
}

/**
*	@brief	Determine whether a refreshed goal meaningfully invalidates the current in-flight request.
*	@param	current_goal	Goal currently stored on the queue entry.
*	@param	incoming_goal	Fresh goal being requested by the caller.
*	@param	policy	Traversal policy supplying rebuild and stair-step thresholds.
*	@param	fallback_refresh_threshold	Fallback distance threshold used when policy thresholds are small.
*	@return	True when the goal changed enough that the current generation should be replaced.
*	@note	This keeps small caller drift from constantly recreating stair searches while still allowing
*			meaningful target movement or floor changes to invalidate stale work.
**/
static bool NavRequest_GoalMovedMaterially( const Vector3 &current_goal, const Vector3 &incoming_goal,
	const svg_nav_path_policy_t &policy, const double fallback_refresh_threshold ) {
	/**
	*	Measure horizontal and vertical goal drift against the most permissive refresh thresholds.
	**/
	const double goalDx = QM_Vector3LengthDP( QM_Vector3Subtract( incoming_goal, current_goal ) );
	const double goalZDelta = std::fabs( incoming_goal.z - current_goal.z );
	const double goalRefreshThreshold = std::max( fallback_refresh_threshold,
		std::max( ( double )policy.rebuild_goal_dist_2d, ( double )policy.waypoint_radius ) );
	const double goalStepThreshold = std::max( policy.max_step_height > 0.0 ? policy.max_step_height : ( double )NAV_DEFAULT_STEP_MAX_SIZE, 1.0 );

	/**
	*	Replace the active generation only when the goal moved far enough to change the expected route or floor.
	**/
	return goalDx > goalRefreshThreshold || goalZDelta > goalStepThreshold;
}

/**
*	@brief    Compute the vertical threshold beyond which LOS collapse should be avoided.
*	@param    policy    Traversal policy driving step height limits and overrides.
*	@return   Step height threshold used to gate LOS simplification.
**/
static inline double NavRequest_GetVerticalStepThreshold( const svg_nav_path_policy_t *policy ) {
	if ( policy && policy->max_step_height > 0.0 ) {
		return policy->max_step_height;
	}
	return NAV_DEFAULT_STEP_MAX_SIZE;
}

/**
*	@brief    Return whether the query behaves like a meaningful multi-level/cross-Z request.
*	@param    start_point    Query start position in the same space as the goal point.
*	@param    goal_point     Query goal position in the same space as the start point.
*	@param    policy         Traversal policy supplying the effective step threshold.
*	@return   True when the absolute Z delta exceeds a step-sized same-level move.
*	@note     Cross-Z requests benefit from more permissive endpoint recovery and faster coarse-route retry
*	          relaxation because direct same-floor shortcuts are less likely to be valid.
**/
static bool NavRequest_IsCrossZQuery( const Vector3 &start_point, const Vector3 &goal_point, const svg_nav_path_policy_t *policy ) {
	/**
	*	Compare the absolute Z delta against the current step-sized threshold.
	**/
	const double threshold = NavRequest_GetVerticalStepThreshold( policy );
	return std::fabs( ( double )goal_point.z - ( double )start_point.z ) > threshold;
}

/**
*	@brief    Prevent LOS direct shortcuts when the vertical delta exceeds the agent's step tolerance.
*	@param    start_node    Canonical start node world position.
*	@param    goal_node     Canonical goal node world position.
*	@param    policy        Traversal policy used to define the step threshold.
*	@return   True when LOS collapse should be skipped due to a large Z difference.
**/
static bool NavRequest_ShouldSkipDirectShortcutDueToVerticalGap( const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const svg_nav_path_policy_t *policy ) {
	/**
	*	Compare the absolute Z delta with the policy-derived threshold.
	**/
	const double deltaZ = std::fabs( start_node.worldPosition.z - goal_node.worldPosition.z );
	const double threshold = NavRequest_GetVerticalStepThreshold( policy );
	return deltaZ > threshold;
}

/**
*	@brief    Determine whether a finalized waypoint list contains stair-like vertical transitions.
*	@param    points     Finalized nav-center waypoint list to inspect.
*	@param    policy     Traversal policy driving the vertical-step tolerance.
*	@return   True when any adjacent waypoint pair exceeds the LOS-collapse Z tolerance.
*	@note     This keeps stair and multi-level waypoint runs intact while still allowing LOS collapse
*	          on flat or gently sloped paths where waypoint removal is safe.
**/
static bool NavRequest_PathContainsLargeVerticalWaypointStep( const std::vector<Vector3> &points, const svg_nav_path_policy_t *policy ) {
	/**
	*	Short paths cannot contain an interior stair run worth protecting.
	**/
	if ( points.size() < 2 ) {
		return false;
	}

	/**
	*	Compare each adjacent waypoint pair against the same vertical-step threshold used by direct shortcuts.
	**/
	const double threshold = NavRequest_GetVerticalStepThreshold( policy );
	for ( size_t pointIndex = 1; pointIndex < points.size(); pointIndex++ ) {
		// Measure the absolute Z delta for this adjacent waypoint pair.
		const double deltaZ = std::fabs( points[ pointIndex ].z - points[ pointIndex - 1 ].z );
		// Keep the full waypoint chain intact when this pair behaves like a stair or larger vertical move.
		if ( deltaZ > threshold ) {
			return true;
		}
	}

	return false;
}

/**
*	@brief    Determine whether a finalized waypoint list contains repeated stair-like vertical anchors.
*	@param    points     Finalized nav-center waypoint list to inspect.
*	@param    policy     Traversal policy driving the vertical-step tolerance.
*	@return   True when multiple adjacent waypoint pairs stay within step-height-sized vertical changes.
*	@note     Stair corridors often consist of several consecutive small rises rather than one large jump, so this
*	          helper protects those interior anchors from LOS collapse even when no single rise exceeds step height.
**/
static bool NavRequest_PathContainsSteppedVerticalCorridor( const std::vector<Vector3> &points, const svg_nav_path_policy_t *policy ) {
	/**
	*	Short paths cannot encode a meaningful stair corridor.
	**/
	if ( points.size() < 3 ) {
		return false;
	}

	/**
	*	Count adjacent pairs that look like repeated step-sized vertical transitions.
	**/
	const double threshold = NavRequest_GetVerticalStepThreshold( policy );
	int32_t steppedPairs = 0;
	for ( size_t pointIndex = 1; pointIndex < points.size(); pointIndex++ ) {
		// Measure the absolute vertical delta for this adjacent pair.
		const double deltaZ = std::fabs( points[ pointIndex ].z - points[ pointIndex - 1 ].z );
		// Count only meaningful rises that still fall within the normal step range.
		if ( deltaZ > 1.0 && deltaZ <= threshold ) {
			steppedPairs++;
			if ( steppedPairs >= 2 ) {
				return true;
			}
		} else {
			// Break the streak when the corridor returns to flat ground or jumps too far.
			steppedPairs = 0;
		}
	}

	return false;
}

/**
*	@brief    Apply the async path simplifier while preserving vertical waypoint runs when required.
*	@param    mesh                         Navigation mesh used for LOS and clearance checks.
*	@param    agent_mins                   Agent bbox minimum extents in nav-center space.
*	@param    agent_maxs                   Agent bbox maximum extents in nav-center space.
*	@param    policy                       Traversal policy used to derive LOS and stair thresholds.
*	@param    preserve_detailed_waypoints  True when the caller already knows LOS collapse must be skipped.
*	@param    inout_points                 [in,out] Finalized nav-center waypoint list to simplify.
*	@note     This helper keeps the worker-complete and main-thread finalize paths aligned so both use the same
*	          vertical-gap guard before applying aggressive LOS collapse.
**/
static void NavRequest_TrySimplifyFinalPathPoints( const nav_mesh_t *mesh, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t &policy, const bool preserve_detailed_waypoints, std::vector<Vector3> *inout_points ) {
	/**
	*	Sanity checks: require mesh storage and a non-trivial waypoint list before attempting simplification.
	**/
	if ( !mesh || !inout_points || inout_points->size() <= 2 ) {
		return;
	}

	/**
	*	Decide whether LOS simplification should be skipped because the request or finalized path contains a stair-like vertical run.
	**/
	const bool avoidLosSimplification = preserve_detailed_waypoints
		|| NavRequest_PathContainsLargeVerticalWaypointStep( *inout_points, &policy )
		|| NavRequest_PathContainsSteppedVerticalCorridor( *inout_points, &policy );

	/**
	*	Configure the shared simplifier so flat routes can still collapse aggressively while stair-like paths keep their anchors.
	**/
	nav_path_simplify_options_t simplifyOptions = {};
	simplifyOptions.max_los_tests = avoidLosSimplification ? 0 : ( nav_simplify_async_max_los_tests ? nav_simplify_async_max_los_tests->integer : 12 );
	simplifyOptions.max_elapsed_ms = 0;
	simplifyOptions.collinear_angle_degrees = nav_simplify_collinear_angle_degrees ? ( double )nav_simplify_collinear_angle_degrees->value : 4.0;
	simplifyOptions.remove_duplicates = true;
	simplifyOptions.prune_collinear = true;
	simplifyOptions.aggressiveness = avoidLosSimplification
		? nav_path_simplify_aggressiveness_t::SyncConservative
		: nav_path_simplify_aggressiveness_t::AsyncAggressive;

	/**
	*	Re-validate the candidate simplification before replacing the caller-owned waypoint list.
	**/
	std::vector<Vector3> candidateSimplifiedPoints = *inout_points;
	if ( SVG_Nav_SimplifyPathPoints( mesh, agent_mins, agent_maxs, &policy, &candidateSimplifiedPoints, &simplifyOptions, nullptr ) ) {
		*inout_points = std::move( candidateSimplifiedPoints );
	}
}

/**
*	@brief    Retrieve the configured per-frame queued-entry prepare budget.
*	@return   At least one queued-entry prepare slot.
**/
static inline int32_t NavRequest_GetPrepareRequestBudget( void ) {
	const int32_t budget = s_nav_prepare_requests_per_frame ? s_nav_prepare_requests_per_frame->integer : SVG_Nav_GetAsyncRequestBudget();
	return std::max( 1, budget );
}

/**
*	@brief    Retrieve the configured per-frame running-entry step budget.
*	@return   At least one running-entry step slot.
**/
static inline int32_t NavRequest_GetStepRequestBudget( void ) {
	const int32_t budget = s_nav_step_requests_per_frame ? s_nav_step_requests_per_frame->integer : SVG_Nav_GetAsyncRequestBudget();
	return std::max( 1, budget );
}

/**
*	@brief    Retrieve the queue-level prepare-time budget in milliseconds.
*	@return   Millisecond budget used for queued-entry preparation.
**/
static inline uint64_t NavRequest_GetPrepareBudgetMs( void ) {
	const int32_t budgetMs = s_nav_prepare_budget_ms ? s_nav_prepare_budget_ms->integer : 4;
	return ( uint64_t )std::max( 0, budgetMs );
}

/**
*	@brief    Retrieve the queue-level step-time budget in milliseconds.
*	@return   Millisecond budget used for running-entry stepping.
**/
static inline uint64_t NavRequest_GetStepBudgetMs( void ) {
	const int32_t budgetMs = s_nav_step_budget_ms ? s_nav_step_budget_ms->integer : 4;
	return ( uint64_t )std::max( 0, budgetMs );
}

/**
*	@brief    Validate that an agent bbox is well-formed.
*	@param    mins    Minimum extents.
*	@param    maxs    Maximum extents.
*	@return   True when mins are strictly lower than maxs on all axes.
**/
static inline const bool NavRequest_AgentBoundsAreValid( const Vector3 &mins, const Vector3 &maxs ) {
	return ( maxs[ 0 ] > mins[ 0 ] ) && ( maxs[ 1 ] > mins[ 1 ] ) && ( maxs[ 2 ] > mins[ 2 ] );
}

// Forward worker callbacks for async queue.
static void NavRequest_Worker_DoInit( void *cb_arg );
static void NavRequest_Worker_Done( void *cb_arg );

/**
*	@brief    Probe a small XY neighborhood to rescue endpoint node resolution on boundary origins.
*	@param    mesh            Navigation mesh.
*	@param    query_center    Endpoint center position that failed direct lookup.
*	@param    start_center    Original request start center used by blend lookup.
*	@param    goal_center     Original request goal center used by blend lookup.
*	@param    query_is_goal   True when rescuing the goal endpoint, false for the start endpoint.
*	@param    policy          Resolved path policy controlling blend behavior.
*	@param    out_node        [out] Best rescued canonical node.
*	@return   True when a nearby endpoint node was recovered.
*	@note     This hardens async endpoint selection against sound origins that land on tile or cell
*	          boundaries where the raw point itself is not directly walkable even though a nearby
*	          canonical walk surface exists.
**/
static bool NavRequest_TryResolveBoundaryOriginNode( const nav_mesh_t *mesh, const Vector3 &query_center,
	const Vector3 &start_center, const Vector3 &goal_center, const bool query_is_goal,
	const svg_nav_path_policy_t &policy, nav_node_ref_t *out_node ) {
	/**
	*	Sanity checks: require mesh storage and an output node reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}
	const bool crossZQuery = NavRequest_IsCrossZQuery( start_center, goal_center, &policy );

	/**
	*	Build a compact probe ring around the failed endpoint.
	*	    Use half-cell and one-cell offsets so boundary-origin sound points can snap onto a nearby
	*	    valid walk surface without skipping across large gaps.
	**/
	const double mesh_cell_size = ( double )mesh->cell_size_xy;
	const double half_cell_raw = mesh_cell_size * 0.5;
	const double half_cell = ( half_cell_raw > 1.0 ) ? half_cell_raw : 1.0;
	const double full_cell = ( half_cell > mesh_cell_size ) ? half_cell : mesh_cell_size;
	const Vector3 probe_offsets[ ] = {
		{ half_cell, 0.0, 0.0 },
		{ -half_cell, 0.0, 0.0 },
		{ 0., half_cell, 0. },
		{ 0., -half_cell, 0. },
		{ half_cell, half_cell, 0. },
		{ half_cell, -half_cell, 0. },
		{ -half_cell, half_cell, 0. },
		{ -half_cell, -half_cell, 0. },
		{ full_cell, 0., 0. },
		{ -full_cell, 0., 0. },
		{ 0., full_cell, 0. },
	  { 0., -full_cell, 0. },
		{ full_cell, full_cell, 0. },
		{ full_cell, -full_cell, 0. },
		{ -full_cell, full_cell, 0. },
		{ -full_cell, -full_cell, 0. }
	};

	/**
	*	Keep the closest rescued node to the original failed endpoint.
	**/
	bool found_node = false;
	double best_distance_sqr = std::numeric_limits<double>::infinity();
	nav_node_ref_t best_node = {};

	// Probe each nearby XY offset and keep the closest successfully resolved canonical node.
	for ( const Vector3 &probe_offset : probe_offsets ) {
		// Shift the failed endpoint by the current local probe offset.
		const Vector3 probe_center = QM_Vector3Add( query_center, probe_offset );
		nav_node_ref_t candidate_node = {};
		bool resolved = false;

		/**
		*	Reuse the caller's configured lookup policy so boundary rescue stays aligned with the
		*	same blend and fallback behavior as the primary endpoint resolution path.
		**/
		if ( policy.enable_goal_z_layer_blend ) {
			const double local_start_z = query_is_goal ? start_center.z : goal_center.z;
			const double local_goal_z = query_is_goal ? goal_center.z : start_center.z;
			const Vector3 &local_start_pos = query_is_goal ? start_center : goal_center;
			const Vector3 &local_goal_pos = query_is_goal ? goal_center : start_center;
			resolved = Nav_FindNodeForPosition_BlendZ( mesh, probe_center, local_start_z, local_goal_z,
				local_start_pos, local_goal_pos, policy.blend_start_dist, policy.blend_full_dist, &candidate_node, true );
		} else {
			const double desired_z = crossZQuery
				? ( query_is_goal ? goal_center.z : start_center.z )
				: query_center.z;
			resolved = Nav_FindNodeForPosition( mesh, probe_center, desired_z, &candidate_node, true );
		}

		// Continue probing until we find at least one nearby canonical node.
		if ( !resolved ) {
			continue;
		}

		// Score the candidate by how closely its recovered world position matches the original endpoint.
		const double candidate_distance_sqr = QM_Vector3DistanceSqr( candidate_node.worldPosition, query_center );
		if ( !found_node || candidate_distance_sqr < best_distance_sqr ) {
			found_node = true;
			best_distance_sqr = candidate_distance_sqr;
			best_node = candidate_node;
		}
	}

	/**
	*	Commit the closest rescued endpoint node when probing succeeded.
	**/
	if ( !found_node ) {
		return false;
	}

	*out_node = best_node;
	return true;
}

/**
* Small payload passed to the worker thread when preparing A* .
* Contains only lightweight, stable inputs gathered on the main thread.
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

  // Worker direct-shortcut results:
	bool direct_path_success = false;
	std::vector<Vector3> direct_path_points = {};

	// Worker search-initialization results:
	bool init_success = false;
	std::optional<nav_a_star_state_t> initialized_state = std::nullopt;

	// Worker finalization outputs (populated when the worker runs A* to completion).
	bool final_path_success = false;
	std::vector<Vector3> final_path_points = {};
  //! True when the worker built an earlier usable provisional path before full completion.
	bool provisional_path_success = false;
	//! Provisional nav-center waypoint chain reaching the best frontier node discovered so far.
	std::vector<Vector3> provisional_path_points = {};
  //! True when the worker should continue owning the incremental search across multiple async slices.
	bool worker_controls_search = false;
	nav_a_star_status_t final_state = nav_a_star_status_t::Failed;
	int32_t worker_expansions = 0;
	uint64_t worker_elapsed_ms = 0;

	//! When true, LOS simplification/direct shortcut collapse should be skipped for the worker result.
	bool preserve_detailed_waypoints = false;

	// Minimal worker-side diagnostics captured for failure handling on the main thread.
	struct worker_diag_t {
		int32_t neighbor_try_count = 0;
		int32_t no_node_count = 0;
		int32_t edge_reject_count = 0;
		int32_t tile_filter_reject_count = 0;
		int32_t same_node_alias_count = 0;
		int32_t closed_duplicate_count = 0;
		int32_t pass_through_prune_count = 0;
		int32_t stagnation_count = 0;
		int32_t edge_reject_reason_counts[ NAV_EDGE_REJECT_REASON_BUCKETS ];
		worker_diag_t() {
			neighbor_try_count = 0;
			no_node_count = 0;
			edge_reject_count = 0;
			tile_filter_reject_count = 0;
			same_node_alias_count = 0;
			closed_duplicate_count = 0;
			pass_through_prune_count = 0;
			stagnation_count = 0;
			for ( int i = 0; i < NAV_EDGE_REJECT_REASON_BUCKETS; ++i ) {
				edge_reject_reason_counts[ i ] = 0;
			}
		}
	} diag;
};

/**
*	@brief    Register cvars that control async request processing and diagnostics.
*	@note     Safe to invoke from SVG_Nav_Init after the navigation subsystem starts.
**/
void SVG_Nav_RequestQueue_RegisterCvars( void ) {
	if ( s_nav_nav_async_enable ) {
		return;
	}


  // Default to near-immediate async nav response so path requests can usually be
	// prepared and finished within the same or next frame, especially for short stair routes.
	s_nav_nav_async_enable = gi.cvar( "nav_nav_async_enable", "1", 0 );
	// Increase the amount of queue work we service each frame so new requests do not sit queued unnecessarily.
	s_nav_requests_per_frame = gi.cvar( "nav_requests_per_frame", "256", 0 );
	s_nav_prepare_requests_per_frame = gi.cvar( "nav_prepare_requests_per_frame", "256", 0 );
	s_nav_step_requests_per_frame = gi.cvar( "nav_step_requests_per_frame", "256", 0 );
	// Raise the per-request expansion budget so in-flight searches can reach a usable path immediately.
	s_nav_expansions_per_request = gi.cvar( "nav_expansions_per_request", "1048576", 0 );
	s_nav_prepare_budget_ms = gi.cvar( "nav_prepare_budget_ms", "100", 0 );
	s_nav_step_budget_ms = gi.cvar( "nav_step_budget_ms", "100", 0 );

	// Worker-side caps: allow the async worker to run A* to completion within these limits.
	// `nav_worker_expansions_per_run` controls a hard expansion cap per worker-run (large default).
	// `nav_worker_budget_ms` controls a soft time cap in milliseconds (0 = no cap).
	s_nav_worker_expansions_per_run = gi.cvar( "nav_worker_expansions_per_run", "4000000", 0 );
	s_nav_worker_budget_ms = gi.cvar( "nav_worker_budget_ms", "0", 0 );
	s_nav_late_frame_tick = gi.cvar( "nav_nav_async_late_frame_tick", "1", 0 );
	nav_nav_async_queue_mode = gi.cvar( "nav_nav_async_queue_mode", "1", 0 );
	s_nav_nav_async_log_stats = gi.cvar( "nav_nav_async_log_stats", "1", 0 );

	// Reserve some reasonable default capacity to avoid repeated reallocations
	// when many entities enqueue navigation requests during startup or stress tests.
	s_nav_request_queue.reserve( 16384 * 4 );
	s_nav_completed_payloads.reserve( 2048 );
	s_nav_request_handle_lookup.reserve( 2048 );
}

/**
*	@brief    Rebuild the handle-to-index lookup after queue compaction.
*	@note     This keeps `NavRequest_FindEntryByHandle` constant-time while
*	          allowing the queue to remain a dense vector.
**/
static void NavRequest_RebuildHandleLookup( void ) {
	/**
	*	Reset the lookup so each surviving queue entry can publish its latest slot.
	**/
	s_nav_request_handle_lookup.clear();

	/**
	*	Mirror the queue order into the handle lookup table.
	**/
	for ( size_t index = 0; index < s_nav_request_queue.size(); index++ ) {
		// Cache the current dense-vector slot for this request handle.
		s_nav_request_handle_lookup[ s_nav_request_queue[ index ].handle ] = index;
	}
}

/**
*	@brief    Destroy a worker payload and release its tag-allocated storage.
*	@param    payload    Payload allocated in `NavRequest_PrepareAStarForEntry`.
**/
static void NavRequest_DestroyPrepPayload( nav_request_prep_payload_t *payload ) {
	/**
	*	Guard against redundant cleanup calls from early-return paths.
	**/
	if ( payload == nullptr ) {
		return;
	}

	/**
	*	Run the payload destructor so in-place state ownership is released before
	*	the engine tag allocator frees the backing memory.
	**/
	payload->~nav_request_prep_payload_t();
	gi.TagFree( payload );
}

/**
*	@brief	Dispatch one navigation worker payload through the shared async work queue.
*	@param	payload	Payload containing either fresh request inputs or an in-flight worker-owned search state.
*	@note	This small wrapper keeps first-dispatch and continuation-dispatch paths aligned.
**/
static void NavRequest_DispatchWorkerPayload( nav_request_prep_payload_t *payload ) {
	/**
	*	Ignore null payloads from defensive early-return paths.
	**/
	if ( payload == nullptr ) {
		return;
	}

	/**
	*	Queue the worker task using the shared async callbacks.
	**/
	asyncwork_t work = {};
	work.work_cb = NavRequest_Worker_DoInit;
	work.done_cb = NavRequest_Worker_Done;
	work.cb_arg = payload;
	gi.Com_QueueAsyncWork( &work );
}

/**
*	@brief	Queue a completed worker payload for later main-thread application.
*	@param	payload	Completed worker payload produced by `NavRequest_Worker_DoInit`.
*	@note	Keeps `NavRequest_Worker_Done` lightweight so async completion does not spend
* 			time mutating the request queue while `Com_CompleteAsyncWork()` holds `work_lock`.
**/
static void NavRequest_EnqueueCompletedPayload( nav_request_prep_payload_t *payload ) {
	/**
	*	Ignore null payloads from defensive early-return paths.
	**/
	if ( payload == nullptr ) {
		return;
	}

	/**
	*	Append the payload into the producer/consumer handoff list.
	**/
	std::scoped_lock lock( s_nav_completed_payload_mutex );
	s_nav_completed_payloads.push_back( payload );
}

/**
*	@brief	Drain completed worker payloads into a caller-owned temporary list.
*	@param	payloads	[out] Receives payloads ready for main-thread application.
*	@note	Swaps the shared completion list into `payloads` so the shared mutex remains
* 			held for only a very short duration.
**/
static void NavRequest_DrainCompletedPayloads( std::vector<nav_request_prep_payload_t *> &payloads ) {
	/**
	*	Keep the handoff list untouched when no payloads are waiting.
	**/
	std::scoped_lock lock( s_nav_completed_payload_mutex );
	if ( s_nav_completed_payloads.empty() ) {
		return;
	}

	/**
	*	Transfer ownership of the pending payload pointers to the caller.
	**/
	payloads.swap( s_nav_completed_payloads );
}

/**
*	@brief    Locate a queued entry by handle.
*	@param    handle    Handle returned when the request was enqueued.
*	@return   Pointer to the entry or nullptr if not found.
**/
static nav_request_entry_t *NavRequest_FindEntryByHandle( nav_request_handle_t handle ) {
	/**
	*	Reject invalid handles before touching the lookup table.
	**/
	if ( handle <= 0 ) {
		return nullptr;
	}

	// Fast-path: cached lookup slot still matches the dense-vector layout.
	if ( const auto found = s_nav_request_handle_lookup.find( handle ); found != s_nav_request_handle_lookup.end() ) {
		const size_t index = found->second;
		if ( index < s_nav_request_queue.size() && s_nav_request_queue[ index ].handle == handle ) {
			return &s_nav_request_queue[ index ];
		}
	}

   // Recover from any stale lookup state by rebuilding once from the queue.
	NavRequest_RebuildHandleLookup();
	if ( const auto found = s_nav_request_handle_lookup.find( handle ); found != s_nav_request_handle_lookup.end() ) {
		const size_t index = found->second;
		if ( index < s_nav_request_queue.size() && s_nav_request_queue[ index ].handle == handle ) {
			return &s_nav_request_queue[ index ];
		}
	}

	return nullptr;
}

/**
*	@brief    Locate an in-flight request owned by a path process.
*	@param    process    Requesting path process.
*	@return   Pointer to the matching entry or nullptr if none exist.
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
* @brief    Enqueue a navigation request.
* @note     Deduplicates requests by path process and reuses the existing handle when needed.
**/
nav_request_handle_t SVG_Nav_RequestPathAsync( svg_nav_path_process_t *pathProcess, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, bool force, double startIgnoreThreshold ) {
	const nav_mesh_t *mesh = g_nav_mesh.get();
	Vector3 normalized_start = start_origin;
	Vector3 normalized_goal = goal_origin;

	/**
	*	Project request endpoints onto the nearest walkable Z before queue bookkeeping.
	*	    This prevents mid-air or inter-floor sound origins from poisoning async endpoint selection.
	**/
	if ( mesh ) {
		SVG_Nav_TryProjectFeetOriginToWalkableZ( mesh, start_origin, agent_mins, agent_maxs, &normalized_start );
		SVG_Nav_TryProjectFeetOriginToWalkableZ( mesh, goal_origin, agent_mins, agent_maxs, &normalized_goal );
	}

	/**
	*	Sanity: require a valid process pointer and an enabled queue.
	**/
	if ( !pathProcess ) {
		return 0;
	}

	if ( !SVG_Nav_IsAsyncNavEnabled() ) {
		return 0;
	}

	/**
	*	Resolve the caller's effective refresh threshold once.
	*	    Reuse the per-request start-ignore threshold when provided so queued,
	*	    preparing, and running requests all share the same notion of what counts
	*	    as insignificant start drift.
	**/
	const double effectiveRefreshThreshold = ( startIgnoreThreshold > 0.0 ) ? startIgnoreThreshold : 32.0;
	// Keep in-flight searches alive across ordinary monster drift so provisional worker results
	// can still land before we decide a materially newer request must replace them.
	const double effectiveInFlightStartIgnore = std::max( effectiveRefreshThreshold, 24.0 );

	/**
	*	Deduplication: refresh existing entry parameters instead of building a new handle.
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
	/**
	*	Rebuild the handle lookup when dense-vector erases changed queue slots.
	**/
	if ( !removedHandles.empty() ) {
		NavRequest_RebuildHandleLookup();
	}

   // Debounce: keep this effectively disabled so fresh async requests can produce
	// a first usable path on the very next queue tick instead of waiting behind an
	// arbitrary time gate.
	const QMTime minPrepInterval = 0_ms;
	if ( !force && pathProcess && pathProcess->last_prep_time > 0_ms ) {
		// Get the current time and compute the delta since the last prep attempt for this process.
		const QMTime now = level.time;
		const QMTime delta = now - pathProcess->last_prep_time;
		// Also compute the movement deltas since the last prep for both start and goal positions.
		const double dx = QM_Vector3LengthDP( QM_Vector3Subtract( normalized_start, pathProcess->last_prep_start ) );
		const double dg = QM_Vector3LengthDP( QM_Vector3Subtract( normalized_goal, pathProcess->last_prep_goal ) );
		const double moveThresh = effectiveRefreshThreshold;
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
		*	In-flight drift guard:
		*	    Preparing and running entries already have useful worker or incremental progress attached.
		*	    Suppress refreshes caused only by small start drift so the current generation can still
		*	    deliver a provisional or final path instead of being invalidated before it returns.
		**/
		if ( !force && ( existing->status == nav_request_status_t::Preparing || existing->status == nav_request_status_t::Running ) ) {
			// Resolve the most stable available start snapshot for drift measurement.
			const Vector3 referenceStart = ( existing->path_process != nullptr && existing->path_process->last_prep_time > 0_ms )
				? existing->path_process->last_prep_start
				: existing->start;
			// Measure how far the live start moved while the in-flight request was still working.
			const double startDx = QM_Vector3LengthDP( QM_Vector3Subtract( normalized_start, referenceStart ) );
		  // Measure whether the goal changed enough that continuing the current generation would be misleading.
			const double goalDx = QM_Vector3LengthDP( QM_Vector3Subtract( normalized_goal, existing->goal ) );
			const double goalZDelta = std::fabs( normalized_goal.z - existing->goal.z );
			const bool goalMovedMaterially = NavRequest_GoalMovedMaterially( existing->goal, normalized_goal, policy, effectiveRefreshThreshold );

			// Keep the current generation alive while only the start drifted within the in-flight tolerance.
			if ( startDx <= effectiveInFlightStartIgnore && !goalMovedMaterially ) {
			   /**
				*	Keep the live request metadata aligned with the latest small-drift origin without invalidating the worker search.
				**/
				existing->start = normalized_start;
				existing->goal = normalized_goal;
				existing->policy = policy;
				existing->agent_mins = agent_mins;
				existing->agent_maxs = agent_maxs;
				existing->force = existing->force || force;
				/**
				*	Refresh the path-process snapshots so later drift checks compare against the newest pursuit origin.
				**/
				if ( existing->path_process ) {
					existing->path_process->last_prep_time = level.time;
					existing->path_process->last_prep_start = normalized_start;
					existing->path_process->last_prep_goal = normalized_goal;
				}
				if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
					gi.dprintf( "[NavAsync][Queue] Suppressed in-flight refresh for handle=%d status=%d startDx=%.2f <= ignore=%.2f goalDx=%.2f goalDz=%.2f\n",
						existing->handle,
						( int )existing->status,
						startDx,
						effectiveInFlightStartIgnore,
						goalDx,
						goalZDelta );
				}
				return existing->handle;
			}
		}

		/**
		* 	When a non-terminal entry already exists for this process, avoid doing
		* 	expensive/mutating refresh work when the caller hasn't moved meaningfully.
		*
		* 	Implementation notes:
		* 	 - If the entry is Queued or Running and both start/goal moved less than
		* 	   'moveThresh' units we skip updating the entry entirely. This avoids
		* 	   repeated "Refreshed existing entry" log spam and copying of resolved
		* 	   policy/hull fields.
		* 	 - We now bump the owning process's `request_generation` only when we
		* 	   actually perform a refresh (or create a new entry). This keeps the
		* 	   generation monotonic semantics local to real changes and avoids
		* 	   needing to revert a bump when we early-skip refreshes.
		**/
		if ( existing->status == nav_request_status_t::Queued || existing->status == nav_request_status_t::Preparing || existing->status == nav_request_status_t::Running ) {
			const double moveThresh = ( double )effectiveRefreshThreshold;
			const double moveThreshSqr = moveThresh * moveThresh;
			const double dx_sqr = QM_Vector3DistanceSqr( existing->start, normalized_start );
			const double dg_sqr = QM_Vector3DistanceSqr( existing->goal, normalized_goal );
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
		existing->start = normalized_start;
		existing->goal = normalized_goal;
		existing->generation = pathProcess->request_generation;
		existing->policy = policy;
		existing->agent_mins = agent_mins;
		existing->agent_maxs = agent_maxs;
		existing->force = existing->force || force;
	  // If the caller drifted only slightly while a main-thread running search is active,
		// keep the current generation alive so we do not throw away usable progress.
		if ( existing->status == nav_request_status_t::Running ) {
			const double effectiveIgnore = effectiveInFlightStartIgnore;
			if ( effectiveIgnore > 0.0 ) {
				const Vector3 refStart = ( existing->path_process != nullptr ? ( existing->path_process && existing->path_process->last_prep_time > 0_ms ? existing->path_process->last_prep_start : existing->path_process->path_start_position ) : normalized_start );
				const double startDx = QM_Vector3LengthDP( QM_Vector3Subtract( normalized_start, refStart ) );
				const bool goalMovedMaterially = NavRequest_GoalMovedMaterially( existing->goal, normalized_goal, policy, effectiveRefreshThreshold );
				if ( startDx <= effectiveIgnore && !goalMovedMaterially && !force ) {
					if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
						gi.dprintf( "[NavAsync][Queue] Suppressing running refresh for handle=%d startDx=%.2f <= ignore=%.2f\n",
							existing->handle, startDx, effectiveIgnore );
					}
					// Don't flip back to Queued; update params only.
					existing->needs_refresh = existing->needs_refresh || force;
					if ( existing->path_process ) {
						existing->path_process->last_prep_time = level.time;
						existing->path_process->last_prep_start = normalized_start;
						existing->path_process->last_prep_goal = normalized_goal;
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
			existing->path_process->last_prep_start = normalized_start;
			existing->path_process->last_prep_goal = normalized_goal;
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
	*	Handle assignment: issue a new handle, ensuring zero is never returned.
	**/
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
	entry.start = normalized_start;
	entry.goal = normalized_goal;
	entry.generation = pathProcess->request_generation;
	entry.policy = policy;
	entry.agent_mins = agent_mins;
	entry.agent_maxs = agent_maxs;
	entry.force = force;
	entry.status = nav_request_status_t::Queued;
	entry.handle = handle;

	/**
	*	Enqueue the new entry for future processing ticks.
	**/
	s_nav_request_queue.push_back( entry );
 // Publish the new handle slot for constant-time completion lookups.
	s_nav_request_handle_lookup[ handle ] = s_nav_request_queue.size() - 1;
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
*	@brief    Cancel a previously queued request.
*	@note     Marks the entry so it can be removed during the next tick.
**/
void SVG_Nav_CancelRequest( nav_request_handle_t handle ) {
	/**
	*	Guard: handle zero is invalid and must be ignored.
	**/
	if ( handle <= 0 ) {
		return;
	}

	/**
	*	Locate the request and update its status to cancelled.
	**/
	nav_request_entry_t *entry = NavRequest_FindEntryByHandle( handle );
	if ( !entry ) {
		return;
	}

	if ( NavRequest_IsTerminalStatus( entry->status ) ) {
		return;
	}

	/**
	*	Cancellation wins immediately:
	*	    - Mark terminal status.
	*	    - Drop refresh intent.
	*	    - Clear owner process markers now so pending checks stop immediately.
	**/
	entry->status = nav_request_status_t::Cancelled;
	entry->needs_refresh = false;
	NavRequest_ClearProcessMarkers( *entry );
}

/**
*	@brief    Check if a path process currently owns a queued request.
**/
bool SVG_Nav_IsRequestPending( const svg_nav_path_process_t *pathProcess ) {
	/**
	*	Guard: invalid process pointers never own requests.
	**/
	if ( !pathProcess ) {
		return false;
	}

	/**
	*	Scan the queue for matching active entries.
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
*	@brief    Process pending requests per frame.
*	@note     Each request now steps the incremental A*	helper using the configured expansion budget.
**/
void SVG_Nav_ProcessRequestQueue( void ) {
	/**
	*	Drain any worker-completed payloads first so async completion stays decoupled
	*	from the async subsystem's internal completion lock.
	**/
	std::vector<nav_request_prep_payload_t *> completedPayloads = {};
	NavRequest_DrainCompletedPayloads( completedPayloads );

	/**
	*	Feature gate: skip work when async processing is disabled.
	**/
	if ( !SVG_Nav_IsAsyncNavEnabled() ) {
		/**
		*	Release drained payloads when async processing is disabled so no worker results leak.
		**/
		for ( nav_request_prep_payload_t *payload : completedPayloads ) {
			NavRequest_DestroyPrepPayload( payload );
		}
		return;
	}

	/**
	*	Apply drained worker results on the main-thread queue before new stepping work.
	**/
	for ( nav_request_prep_payload_t *payload : completedPayloads ) {
		NavRequest_ApplyCompletedPayload( payload );
	}

	const bool diagLogging = s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0;

	/**
	*	Guard: wait for the navigation mesh before touching queued requests.
	**/
	if ( !g_nav_mesh.get() ) {
		/**
		*	No-mesh hardening:
		*	    Do not keep requests pending forever when a mesh is unavailable.
		*	    Mark active entries cancelled and clear owner process markers so
		*	    callers immediately stop reporting pending async state.
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
			NavRequest_RebuildHandleLookup();

			/**
			*	Reset the fairness cursor when the queue is drained while no mesh exists.
			**/
			if ( s_nav_request_queue.empty() ) {
				s_nav_request_round_robin_cursor = 0;
			} else {
				s_nav_request_round_robin_cursor %= s_nav_request_queue.size();
			}

			if ( diagLogging ) {
				gi.dprintf( "[NavAsync][Queue] Cancelled pending requests because navmesh is unavailable.\n" );
			}
		}

		return;
	}

	/**
	*	Budgeting: constrain work by both request throttles and expansion caps.
	**/
	const int32_t prepareRequestBudget = NavRequest_GetPrepareRequestBudget();
	const int32_t stepRequestBudget = NavRequest_GetStepRequestBudget();
	const int32_t expansionsPerRequest = SVG_Nav_GetAsyncRequestExpansions();
	const int64_t totalExpansions = ( int64_t )expansionsPerRequest * stepRequestBudget;
	int32_t remainingExpansions = ( int32_t )std::min( totalExpansions, ( int64_t )std::numeric_limits<int32_t>::max() );
	const int32_t perRequestBudget = std::max( 1, expansionsPerRequest );
	const int32_t expansionBound = std::max( 1, ( remainingExpansions + perRequestBudget - 1 ) / perRequestBudget );
	const int32_t stepBudgetCap = std::max( 1, std::min( stepRequestBudget, expansionBound ) );
	int32_t prepareProcessed = 0;
	int32_t stepProcessed = 0;
	const int32_t queueBefore = ( int32_t )s_nav_request_queue.size();
	const size_t queueCount = s_nav_request_queue.size();
	size_t startIndex = 0;

	/**
	*	Start each frame from the persisted round-robin cursor so later requests are not
	*	perpetually starved behind earlier expensive entries.
	**/
	if ( queueCount > 0 ) {
		startIndex = s_nav_request_round_robin_cursor % queueCount;
	}

	/**
	*	Service queued-entry preparation first using its own frame budget.
	*	 Keep prepares isolated from running-step work so a burst of expensive worker setup cannot
	*	 consume the same frame slice needed by already-running searches.
	**/
	const uint64_t prepareStartMs = gi.GetRealSystemTime();
	const uint64_t prepareBudgetMs = NavRequest_GetPrepareBudgetMs();
	size_t prepareVisited = 0;
	while ( prepareVisited < queueCount ) {
		if ( prepareProcessed >= prepareRequestBudget ) {
			break;
		}
		const uint64_t prepareElapsedMs = gi.GetRealSystemTime() - prepareStartMs;
		if ( prepareBudgetMs > 0 && prepareElapsedMs > prepareBudgetMs ) {
			break;
		}

		/**
	  * Visit the next queue slot relative to the current fairness cursor.
		**/
		const size_t entryIndex = ( startIndex + prepareVisited ) % queueCount;
		nav_request_entry_t &entry = s_nav_request_queue[ entryIndex ];
		prepareVisited++;

		if ( NavRequest_IsTerminalStatus( entry.status ) ) {
			NavRequest_ClearProcessMarkers( entry );
			continue;
		}

	   /**
		*	Skip entries that are not waiting for worker preparation.
		**/
		if ( entry.status != nav_request_status_t::Queued ) {
			continue;
		}

		if ( !NavRequest_PrepareAStarForEntry( entry ) ) {
			entry.status = nav_request_status_t::Failed;
			NavRequest_HandleFailure( entry );
			NavRequest_ClearProcessMarkers( entry );
			Nav_AStar_Reset( &entry.a_star );
			if ( diagLogging ) {
				gi.dprintf( "[NavAsync][Queue] PrepareAStarForEntry failed for handle=%d path_process=%p\n", entry.handle, ( void * )entry.path_process );
			}
			continue;
		}
		if ( diagLogging ) {
			gi.dprintf( "[NavAsync][Queue] Prepared A*	for handle=%d path_process=%p\n", entry.handle, ( void * )entry.path_process );
		}
		prepareProcessed++;
	}

	/**
	*	Advance fairness after the prepare pass so the running-step pass does not restart at the same slot.
	**/
	if ( queueCount > 0 ) {
		startIndex = ( startIndex + prepareVisited ) % queueCount;
	}

	/**
	*	Service running searches using a separate frame budget layered on top of per-request A*	budgets.
	**/
	const uint64_t stepStartMs = gi.GetRealSystemTime();
	const uint64_t stepBudgetMs = NavRequest_GetStepBudgetMs();
	size_t stepVisited = 0;
	while ( stepVisited < queueCount ) {
		if ( stepProcessed >= stepBudgetCap || remainingExpansions <= 0 ) {
			break;
		}
		const uint64_t stepElapsedMs = gi.GetRealSystemTime() - stepStartMs;
		if ( stepBudgetMs > 0 && stepElapsedMs > stepBudgetMs ) {
			break;
		}

		/**
		*	Visit the next queue slot relative to the updated fairness cursor.
		**/
		const size_t entryIndex = ( startIndex + stepVisited ) % queueCount;
		nav_request_entry_t &entry = s_nav_request_queue[ entryIndex ];
		stepVisited++;

		/**
		*	Clear markers for already-terminal entries and skip them.
		**/
		if ( NavRequest_IsTerminalStatus( entry.status ) ) {
			NavRequest_ClearProcessMarkers( entry );
			continue;
		}

		/**
		*	Skip work that is not yet ready to be stepped on the main thread.
		**/
		if ( entry.status == nav_request_status_t::Preparing ) {
			continue;
		}

		if ( entry.status != nav_request_status_t::Running ) {
			continue;
		}

		/**
		*	Advance the running incremental search and finalize when it completes.
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
		*	Track that we serviced one running request this frame.
		**/
		stepProcessed++;

		if ( starStatus == nav_a_star_status_t::Completed ) {
			// If a refresh was requested while this entry was running, defer
			// committing the just-completed result and instead re-queue the
			// entry so it will be re-prepared with the newer parameters.
			if ( entry.needs_refresh ) {
				if ( diagLogging ) {
					gi.dprintf( "[NavAsync][Queue] Deferring commit for handle=%d due to pending refresh; re-queueing.\n", entry.handle );
				}
					// Clear the flag and reset the working A*	state so it can be
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
				if ( diagLogging ) {
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
	*	Advance the fairness cursor to the next unvisited slot so later entries receive service on the next frame.
	**/
	if ( queueCount > 0 ) {
		s_nav_request_round_robin_cursor = ( startIndex + stepVisited ) % queueCount;
	}

	/**
	*	Remove any entries that reached a terminal state so handles can be reused.
	**/
	const auto isTerminalEntry = []( const nav_request_entry_t &entry ) {
		return entry.status == nav_request_status_t::Completed
			|| entry.status == nav_request_status_t::Failed
			|| entry.status == nav_request_status_t::Cancelled;
		};
	const bool hasTerminalEntry = std::any_of( s_nav_request_queue.begin(), s_nav_request_queue.end(), isTerminalEntry );
	int32_t queueAfter = queueBefore;
	bool queueShrank = false;
	if ( hasTerminalEntry ) {
		s_nav_request_queue.erase( std::remove_if( s_nav_request_queue.begin(), s_nav_request_queue.end(), isTerminalEntry ),
			s_nav_request_queue.end() );
		queueAfter = ( int32_t )s_nav_request_queue.size();
		queueShrank = queueAfter != queueBefore;
	}

	/**
	*	Clamp the fairness cursor after compaction so it always points at a valid future slot.
	**/
	if ( s_nav_request_queue.empty() ) {
		s_nav_request_round_robin_cursor = 0;
	} else {
		s_nav_request_round_robin_cursor %= s_nav_request_queue.size();
	}

	if ( queueShrank ) {
		/**
		*	Refresh cached handle slots after terminal entries were compacted away.
		**/
		NavRequest_RebuildHandleLookup();
	}
	NavRequest_LogQueueDiagnostics( queueBefore, queueAfter, prepareProcessed + stepProcessed, prepareRequestBudget + stepBudgetCap, remainingExpansions );
}

/**
*	@brief    Prepare incremental A*	state for a queued request entry.
*	@param    entry    Request entry to initialize before stepping.
*	@return   True when the search state successfully entered the Running phase OR
*	          when worker has been enqueued to initialize the state. False indicates
*	          an immediate failure that should mark the request terminal.
*
*	NOTE: This function was refactored to enqueue heavy work to the async worker:
*	      - Main thread: compute resolved policy/agent hull and perform small sanity checks.
*	      - Worker thread: perform feet->center conversion, node resolution, tile-route lookup,
*	                       and call Nav_AStar_Init (allocations occur on worker).
*	      - Worker done-callback: move-initialize `entry.a_star` and transition entry->Running.
**/
static bool NavRequest_PrepareAStarForEntry( nav_request_entry_t &entry ) {
	/**
	*	Only queued entries should enter worker-prep dispatch.
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
	const bool diagLogging = s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0;

	// Mark the owning path process so callers can inspect and (if necessary)
	// cancel the outstanding request. If another request arrives for the same
	// process it will refresh the existing entry rather than enqueue a duplicate.
	process->rebuild_in_progress = true;
	process->pending_request_handle = entry.handle;

	/**
	*	Align policy constraints with the agent hull profile before running A* .
	*	This is lightweight and deliberately kept on the main thread so the worker
	*	can rely on a stable resolved policy snapshot.
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
	*	Resolve the agent bounds that should drive center offsets and traversal.
	*	    Prefer mesh-authored hull sizes so navigation queries stay aligned.
	*	This selection is cheap and done on the main thread.
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
	*	Update the resolved policy with the chosen agent profile and traversal limits.
	*	Store a snapshot on the entry so worker and finalizer see the same policy.
	**/
	entry.resolved_policy.agent_mins = resolvedAgentMins;
	entry.resolved_policy.agent_maxs = resolvedAgentMaxs;
	entry.resolved_policy.max_step_height = meshAgentValid ? mesh->max_step : agentProfile.max_step_height;
	entry.resolved_policy.max_drop_height = agentProfile.max_drop_height;
	entry.resolved_policy.max_drop_height_cap = agentProfile.max_drop_height_cap;
	entry.resolved_policy.max_slope_normal_z = meshAgentValid ? mesh->max_slope_normal_z : agentProfile.max_slope_normal_z;
	entry.resolved_policy.min_step_normal = entry.resolved_policy.max_slope_normal_z;

	/**
	*	Boundary guard:
	*	    External callers provide feet-origin with a valid bbox. Reject malformed
	*	    hulls before worker prep to avoid undefined conversion behavior.
	**/
	if ( !NavRequest_AgentBoundsAreValid( entry.resolved_policy.agent_mins, entry.resolved_policy.agent_maxs ) ) {
		if ( diagLogging ) {
			gi.dprintf( "[NavAsync][Prep] invalid agent bounds handle=%d mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f)\n",
				entry.handle,
				entry.resolved_policy.agent_mins.x, entry.resolved_policy.agent_mins.y, entry.resolved_policy.agent_mins.z,
				entry.resolved_policy.agent_maxs.x, entry.resolved_policy.agent_maxs.y, entry.resolved_policy.agent_maxs.z );
		}
		return false;
	}

	// Emit diagnostic about chosen agent hull for this request when async logging is enabled.
	if ( diagLogging ) {
		gi.dprintf( "[NavAsync][Prep] handle=%d resolved_agent_mins=(%.1f %.1f %.1f) resolved_agent_maxs=(%.1f %.1f %.1f) use_mesh_agent=%d\n",
			entry.handle,
			resolvedAgentMins.x, resolvedAgentMins.y, resolvedAgentMins.z,
			resolvedAgentMaxs.x, resolvedAgentMaxs.y, resolvedAgentMaxs.z,
			meshAgentValid ? 1 : 0 );
	}

	/**
	*	Sanity: quick world-bounds check on main thread to avoid obvious worker work.
	*	This is intentionally tiny to avoid main-thread node lookups while catching
	*	requests that are clearly outside the nav extents.
	**/
	if ( entry.start.x < mesh->world_bounds.mins.x - 1.0 || entry.start.x > mesh->world_bounds.maxs.x + 1.0 ||
		entry.start.y < mesh->world_bounds.mins.y - 1.0 || entry.start.y > mesh->world_bounds.maxs.y + 1.0 ||
		entry.goal.x  < mesh->world_bounds.mins.x - 1.0 || entry.goal.x  > mesh->world_bounds.maxs.x + 1.0 ||
		entry.goal.y  < mesh->world_bounds.mins.y - 1.0 || entry.goal.y  > mesh->world_bounds.maxs.y + 1.0 ) {
	   // Out-of-bounds: immediate failure, no worker enqueue.
		if ( diagLogging ) {
			gi.dprintf( "[NavAsync][Prep] immediate OOB failure handle=%d start=(%.1f,%.1f) goal=(%.1f,%.1f)\n",
				entry.handle, entry.start.x, entry.start.y, entry.goal.x, entry.goal.y );
		}
		return false;
	}

	/**
	*	Build a small payload for the worker thread. This contains only stable,
	*	copyable inputs so the worker can operate without touching the main-thread
	*	owned queue memory.
	**/
	void *payloadMemory = gi.TagMallocz( sizeof( nav_request_prep_payload_t ), TAG_SVGAME_NAVMESH );
	nav_request_prep_payload_t *payload = new ( payloadMemory ) nav_request_prep_payload_t();
	payload->handle = entry.handle;
	payload->generation = entry.generation;
	payload->start_feet = entry.start;
	payload->goal_feet = entry.goal;
	payload->resolved_policy = entry.resolved_policy;
	payload->agent_mins = resolvedAgentMins;
	payload->agent_maxs = resolvedAgentMaxs;
	payload->path_process = entry.path_process;

	/**
	*	Hand the payload to the async worker so initialization can begin off-thread.
	**/
	NavRequest_DispatchWorkerPayload( payload );

   /**
	*	Mark the request as preparing so queue processing does not dispatch
	*	duplicate worker init tasks while this generation is in-flight.
	**/
	entry.status = nav_request_status_t::Preparing;

	// Worker done-callback will transition to Running on success.
	return true;
}

/**
*	@brief    Worker thread function to perform heavy A*	prep:
*	          - Convert feet->center
*	          - Resolve start/goal nodes
*	          - Optionally compute tile-route
*	          - Call Nav_AStar_Init (allocations happen here)
*
*	This runs on the async worker thread.
**/
static void NavRequest_Worker_DoInit( void *cb_arg ) {
	/**
	*	Sanity checks: require a worker payload before doing any async navigation work.
	**/
	if ( !cb_arg ) {
		return;
	}
	nav_request_prep_payload_t *p = ( nav_request_prep_payload_t * )cb_arg;

	/**
	*	Reset one-run outputs while preserving any worker-owned search state carried across slices.
	**/
	p->direct_path_success = false;
	p->direct_path_points.clear();
	p->final_path_success = false;
	p->final_path_points.clear();
	p->provisional_path_success = false;
	p->provisional_path_points.clear();
	p->worker_controls_search = false;
	p->final_state = nav_a_star_status_t::Failed;
	p->worker_expansions = 0;
	p->worker_elapsed_ms = 0;
	p->diag = {};

	/**
	*	Detect whether this worker invocation is continuing an already initialized incremental search.
	**/
	const bool continuingSearch = p->init_success && p->initialized_state.has_value();
	if ( !continuingSearch ) {
		p->init_success = false;
		p->initialized_state.reset();
	}

 /**
	*	Require a live mesh before touching canonical node or A* state.
	**/
	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( !mesh ) {
		return;
	}
	const bool diagLogging = s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0;

	/**
	*	Continuation fast path: keep the search worker-owned and avoid repeating endpoint/corridor setup on every async slice.
	**/
	if ( continuingSearch ) {
		// Rebind any non-owning views after payload transport so later worker slices keep internal pointers valid.
		p->initialized_state->RebindInternalPointers();
		if ( diagLogging ) {
			gi.dprintf( "[NavAsync][Worker] continue handle=%d prior_state=%d\n",
				p->handle,
				( int )p->initialized_state->status );
		}
	} else {

	// Convert feet-origin to nav-center using the resolved agent hull.
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, p->start_feet, &p->resolved_policy.agent_mins, &p->resolved_policy.agent_maxs );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, p->goal_feet, &p->resolved_policy.agent_mins, &p->resolved_policy.agent_maxs );
		const bool crossZQuery = NavRequest_IsCrossZQuery( start_center, goal_center, &p->resolved_policy );
		Q_assert( NavRequest_AgentBoundsAreValid( p->resolved_policy.agent_mins, p->resolved_policy.agent_maxs ) );
		if ( diagLogging ) {
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
			/**
			*	Keep the first lookup strict so worker prep does not pay for broad neighbor-tile probing up front.
			*	    The dedicated boundary-origin rescue immediately below already handles seam and edge cases with
			*	    tighter context than the generic fallback scan.
			**/
			start_ok = Nav_FindNodeForPosition_BlendZ( mesh, start_center, goal_center.z, start_center.z, goal_center,
				start_center, p->resolved_policy.blend_start_dist, p->resolved_policy.blend_full_dist, &start_node, false );
			goal_ok = Nav_FindNodeForPosition_BlendZ( mesh, goal_center, start_center.z, goal_center.z, start_center,
				goal_center, p->resolved_policy.blend_start_dist, p->resolved_policy.blend_full_dist, &goal_node, false );
		} else {
		  // Start with a strict canonical lookup before escalating to the targeted boundary rescue path.
			start_ok = Nav_FindNodeForPosition( mesh, start_center, crossZQuery ? goal_center.z : start_center.z, &start_node, false );
			goal_ok = Nav_FindNodeForPosition( mesh, goal_center, goal_center.z, &goal_node, false );
		}

		/**
		*	Boundary-origin rescue:
		*	    Sound and event goals can sit exactly on tile or cell boundaries where the raw position
		*	    maps to a less reliable coarse key even though node resolution already recovered the correct
		*	    canonical start/goal surfaces.
		**/
		if ( !start_ok ) {
			start_ok = NavRequest_TryResolveBoundaryOriginNode( mesh, start_center, start_center, goal_center, false,
				p->resolved_policy, &start_node );
		}
		if ( !goal_ok ) {
			goal_ok = NavRequest_TryResolveBoundaryOriginNode( mesh, goal_center, start_center, goal_center, true,
				p->resolved_policy, &goal_node );
		}

		if ( !start_ok || !goal_ok ) {
			// Node resolution failed.
			if ( diagLogging ) {
				gi.dprintf( "[NavAsync][Worker] node resolution failed handle=%d start_ok=%d goal_ok=%d\n",
					p->handle, start_ok ? 1 : 0, goal_ok ? 1 : 0 );
			}
			return;
		}

		/**
		*	Rescue locally isolated start/goal layers by re-evaluating same-cell variants.
		*	    This keeps the resolved XY cell fixed while preferring a layer that actually has
		*	    adjacent connectivity under the current async traversal policy.
		**/
		Nav_AStar_TrySelectConnectedSameCellLayer( mesh, start_node, &p->resolved_policy, &start_node );
		Nav_AStar_TrySelectConnectedSameCellLayer( mesh, goal_node, &p->resolved_policy, &goal_node );

		// Successful node resolution: emit coordinates for diagnostics when enabled.
		if ( diagLogging ) {
			gi.dprintf( "[NavAsync][Worker] node resolution success handle=%d start=(%.1f %.1f %.1f) start_node=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_node=(%.1f %.1f %.1f)\n",
				p->handle,
				p->start_feet.x, p->start_feet.y, p->start_feet.z,
				start_node.worldPosition.x, start_node.worldPosition.y, start_node.worldPosition.z,
				p->goal_feet.x, p->goal_feet.y, p->goal_feet.z,
				goal_node.worldPosition.x, goal_node.worldPosition.y, goal_node.worldPosition.z );
		}

		/**
		*	Attempt the Phase 7 direct goal shortcut before any coarse routing or A*	initialization work.
		**/
		if ( start_node.key == goal_node.key ) {
			p->direct_path_success = true;
			p->direct_path_points.clear();
			p->direct_path_points.push_back( start_node.worldPosition );
			p->direct_path_points.push_back( goal_node.worldPosition );
			if ( diagLogging ) {
				gi.dprintf( "[NavAsync][Worker] direct_shortcut trivial=1 handle=%d\n", p->handle );
			}
			return;
		}

	 /**
		*	Preserve raw corridor points only when the direct shortcut crosses too much vertical change.
		*	    This keeps LOS collapse available on flat/same-level routes while protecting stair and multi-level moves.
		**/
		const bool avoidLosCollapse = NavRequest_ShouldSkipDirectShortcutDueToVerticalGap( start_node, goal_node, &p->resolved_policy );
		p->preserve_detailed_waypoints = avoidLosCollapse;
		const nav_los_request_t losRequest = { .mode = nav_los_mode_t::DDA };
		if ( !avoidLosCollapse
			&& NavRequest_ShouldAttemptDirectLosShortcut( mesh, start_node, goal_node )
			&& SVG_Nav_HasLineOfSight( mesh, start_node, goal_node, p->agent_mins, p->agent_maxs, &p->resolved_policy, &losRequest, nullptr ) ) {
			p->direct_path_success = true;
			p->direct_path_points.clear();
			p->direct_path_points.push_back( start_node.worldPosition );
			p->direct_path_points.push_back( goal_node.worldPosition );
			if ( diagLogging ) {
				gi.dprintf( "[NavAsync][Worker] direct_shortcut los=1 handle=%d\n", p->handle );
			}
			return;
		}

		/**
		*	Optionally compute a coarse hierarchy route filter (with cluster fallback).
		*	    This is policy-gated so debug entities can disable route restriction
		*	    and isolate fine traversal (`StepTest`) behavior.
		**/
		nav_refine_corridor_t refineCorridor = {};

		/**
		*	Re-allow an unrestricted fine search when the same goal keeps failing.
		*	    Forced sound-goal refreshes and repeated same-goal retries should not stay pinned to the
		*	    previous coarse corridor forever because that can repeatedly reject viable local detours.
		**/
		bool relaxRouteFilterForRetry = false;
		if ( p->resolved_policy.enable_cluster_route_filter && p->path_process ) {
			const svg_nav_path_process_t &pathProcess = *p->path_process;
			const double retryGoalRadius = std::max( p->resolved_policy.rebuild_goal_dist_3d > 0.0 ? p->resolved_policy.rebuild_goal_dist_3d : p->resolved_policy.rebuild_goal_dist_2d, NAV_DEFAULT_GOAL_REBUILD_3D_DISTANCE );
			const bool recentSameGoalFailure = pathProcess.last_failure_time > 0_ms
				&& ( level.time - pathProcess.last_failure_time ) <= 1000_ms /*4_sec*/
				&& ( QM_Vector3DistanceSqr( pathProcess.last_failure_pos, p->goal_feet ) <= ( retryGoalRadius * retryGoalRadius ) );
			const int32_t failureThreshold = crossZQuery ? 1 : 2;
			relaxRouteFilterForRetry = recentSameGoalFailure && pathProcess.consecutive_failures >= failureThreshold;
		}

		// Determine whether to apply the hierarchy route filter based on policy and retry state.
		const bool routeFilterEnabled = p->resolved_policy.enable_cluster_route_filter && !relaxRouteFilterForRetry;
		// Track whether the hierarchy or cluster filter was used to inform diagnostics and future tuning.
		bool usedHierarchyRoute = false;
		// Cluster routes are a fallback when hierarchy filtering fails to produce a viable route. Track both cases separately for diagnostics.
		bool usedClusterRoute = false;
		// Track the number of coarse expansions when filtering is applied to inform diagnostics and future tuning.
		int32_t coarseExpansions = 0;
		/**
		*	Boundary-origin hardening:
		*		Compute the coarse route from the already resolved canonical nodes rather than the raw
		*		caller origins. Sound/event goals can land on tile or cell boundaries where the raw position
		*		maps to a less reliable coarse key even though node resolution already recovered the correct
		*		canonical start/goal surfaces.
		**/
		/**
		*  Note: The async worker intentionally builds the explicit refinement corridor
		*  using the shared SVG_Nav_BuildRefineCorridor() helper. That builder internally
		*  runs the hierarchy coarse A* and will opportunistically apply conservative
		*  portal-to-portal LOS shortcuts (Nav_Hierarchy_TryPortalToPortalLosShortcut).
		*  In other words, async path requests inherit hierarchy portal shortcutting
		*  via the common corridor builder and need no separate async-only shortcut
		*  implementation.
		*/
		const bool hasRefineCorridor = routeFilterEnabled && SVG_Nav_BuildRefineCorridor( mesh, start_node, goal_node, &p->resolved_policy, &refineCorridor,
			&usedHierarchyRoute, &usedClusterRoute, &coarseExpansions );
		if ( diagLogging ) {
			if ( relaxRouteFilterForRetry ) {
				gi.dprintf( "[NavAsync][Worker] corridor retry_relaxed=1 handle=%d failures=%d\n",
					p->handle,
					p->path_process ? p->path_process->consecutive_failures : 0 );
			}
			gi.dprintf( "[NavAsync][Worker] corridor enabled=%d has_corridor=%d hierarchy=%d cluster=%d expansions=%d regions=%d portals=%d tiles=%d handle=%d\n",
				routeFilterEnabled ? 1 : 0,
				hasRefineCorridor ? 1 : 0,
				usedHierarchyRoute ? 1 : 0,
				usedClusterRoute ? 1 : 0,
				coarseExpansions,
				( int32_t )refineCorridor.region_path.size(),
				( int32_t )refineCorridor.portal_path.size(),
				( int32_t )refineCorridor.exact_tile_route.size(),
				p->handle );
		}

		// Construct the state directly inside the payload so later worker slices can keep reusing it.
		p->initialized_state.emplace();
		if ( !Nav_AStar_Init( &p->initialized_state.value(), mesh, start_node, goal_node, p->agent_mins, p->agent_maxs,
			&p->resolved_policy, hasRefineCorridor ? &refineCorridor : nullptr, p->path_process ) ) {
			// Drop the in-place state so failed work does not carry heavy containers forward.
			p->initialized_state.reset();
			if ( diagLogging ) {
				gi.dprintf( "[NavAsync][Worker] Nav_AStar_Init failed handle=%d\n", p->handle );
			}
			return;
		}

	 // Publish that the payload now owns a live incremental search state.
		p->init_success = true;
	}

 // Optionally keep the initialized A* on the worker thread subject to configured caps.
	// Zero caps preserve the fallback path where the initialized state returns to the main thread.
	const int32_t workerExpansionsCap = s_nav_worker_expansions_per_run ? s_nav_worker_expansions_per_run->integer : 0;
	const uint64_t workerBudgetMs = s_nav_worker_budget_ms ? ( uint64_t )std::max( 0, s_nav_worker_budget_ms->integer ) : 0;
	if ( ( workerExpansionsCap != 0 ) || ( workerBudgetMs != 0 ) ) {
	 // Mark this payload as worker-owned so the callback requeues it instead of moving it back to a main-thread running entry.
		p->worker_controls_search = true;
		const uint64_t runStartMs = gi.GetRealSystemTime();
		int32_t usedExpansions = 0;
		nav_a_star_status_t runStatus = p->initialized_state->status;
		// Run in moderate-sized chunks so we check the time/expansion caps frequently.
		static constexpr int32_t kWorkerStepChunk = NAV_TILE_WORKER_STEP_CHUNK_SIZE;
		while ( runStatus == nav_a_star_status_t::Running ) {
			int32_t toUse = kWorkerStepChunk;
			if ( workerExpansionsCap > 0 ) {
				toUse = std::min( toUse, std::max( 0, workerExpansionsCap - usedExpansions ) );
				if ( toUse <= 0 ) {
					break;
				}
			}
			// Step the worker-side A* and accumulate usage.
			runStatus = Nav_AStar_Step( &p->initialized_state.value(), toUse );
			usedExpansions += toUse;
			// Check elapsed time cap.
			if ( workerBudgetMs > 0 ) {
				const uint64_t nowMs = gi.GetRealSystemTime();
				if ( nowMs - runStartMs >= workerBudgetMs ) {
					break;
				}
			}
		}
	 /**
		*	Record worker metrics into the payload so the callback can report timing and failure context.
		**/
		p->worker_expansions = usedExpansions;
		p->worker_elapsed_ms = gi.GetRealSystemTime() - runStartMs;
		p->final_state = p->initialized_state->status;

		/**
		*	Copy compact diagnostic counters from the worker state so the main-thread callback can surface meaningful failures.
		**/
		p->diag.neighbor_try_count = p->initialized_state->neighbor_try_count;
		p->diag.no_node_count = p->initialized_state->no_node_count;
		p->diag.edge_reject_count = p->initialized_state->edge_reject_count;
		p->diag.tile_filter_reject_count = p->initialized_state->tile_filter_reject_count;
		p->diag.same_node_alias_count = p->initialized_state->same_node_alias_count;
		p->diag.closed_duplicate_count = p->initialized_state->closed_duplicate_count;
		p->diag.pass_through_prune_count = p->initialized_state->pass_through_prune_count;
		p->diag.stagnation_count = p->initialized_state->stagnation_count;
		// Snapshot per-reason edge reject buckets for richer diagnostics.
		for ( int i = 0; i < NAV_EDGE_REJECT_REASON_BUCKETS; ++i ) {
			p->diag.edge_reject_reason_counts[ i ] = p->initialized_state->edge_reject_reason_counts[ i ];
		}

		/**
		*	Finalize a completed search on the worker so the main thread only commits finished waypoints.
		**/
		if ( p->initialized_state->status == nav_a_star_status_t::Completed ) {
			std::vector<Vector3> workerPoints;
			if ( Nav_AStar_Finalize( &p->initialized_state.value(), &workerPoints ) ) {
				p->final_path_success = true;
				p->final_path_points = std::move( workerPoints );
			} else {
				p->final_path_success = false;
			}
		  /**
			*	Drop the heavy worker-owned state after producing final points so no redundant containers cross back to the main thread.
			**/
			p->initialized_state.reset();
		} else if ( p->initialized_state->status == nav_a_star_status_t::Running ) {
			   /**
			   *	Build the best currently-known provisional route so the caller can keep moving while the worker continues refining in later slices.
			   **/
			std::vector<Vector3> provisionalPoints;
			if ( Nav_AStar_BuildBestPartialPath( &p->initialized_state.value(), &provisionalPoints ) ) {
				p->provisional_path_success = true;
				p->provisional_path_points = std::move( provisionalPoints );
			}
		}
		if ( diagLogging ) {
			gi.dprintf( "[NavAsync][Worker] Done-run handle=%d expansions=%d elapsed_ms=%llu final=%d state=%d\n",
				p->handle, p->worker_expansions, ( unsigned long long )p->worker_elapsed_ms,
				p->final_path_success ? 1 : 0, ( int )p->final_state );
		}
	}
}

/**
*	@brief	Lightweight async done-callback that hands completed worker payloads to the main-thread drain list.
*	@param	cb_arg	Worker payload produced by `NavRequest_Worker_DoInit`.
*	@note	Keeps completion lightweight so the main-thread queue mutation still happens centrally from `SVG_Nav_ProcessRequestQueue()`.
**/
static void NavRequest_Worker_Done( void *cb_arg ) {
	/**
	*	Ignore null callback payloads from defensive early-return paths.
	**/
	if ( cb_arg == nullptr ) {
		return;
	}

	/**
	*	Forward the completed payload into the producer/consumer handoff list.
	**/
	NavRequest_EnqueueCompletedPayload( static_cast< nav_request_prep_payload_t * >( cb_arg ) );
}

/**
*	@brief	Apply a drained worker payload onto the main-thread request queue.
*	@param	p	Completed worker payload waiting in the handoff list.
*	@note	This contains the previous heavy `NavRequest_Worker_Done` logic, but it now runs
* 			from `SVG_Nav_ProcessRequestQueue()` after the handoff list is drained.
**/
static void NavRequest_ApplyCompletedPayload( nav_request_prep_payload_t *p ) {
	/**
	*	Ignore null payloads from defensive early-return paths.
	**/
	if ( !p ) {
		return;
	}

	/**
	*	Read the logging toggle once for the rest of this application path.
	**/
	const bool diagLogging = s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0;

	/**
	*	Locate the queued entry by handle and drop the payload if the request no longer exists.
	**/
	const auto optIndex = NavRequest_FindEntryIndexByHandle( p->handle );
	if ( !optIndex.has_value() ) {
		NavRequest_DestroyPrepPayload( p );
		return;
	}
	nav_request_entry_t &entry = s_nav_request_queue[ optIndex.value() ];

	/**
	*	Reject stale worker generations before any path points are committed back onto the owning process.
	**/
	if ( entry.generation != p->generation ) {
		NavRequest_DestroyPrepPayload( p );
		return;
	}

	/**
	*	Commit worker-finalized path points immediately when the worker already completed the search.
	**/
	if ( p->final_path_success ) {
		entry.resolved_policy = p->resolved_policy;
		entry.agent_mins = p->agent_mins;
		entry.agent_maxs = p->agent_maxs;
		entry.preserve_detailed_waypoints = p->preserve_detailed_waypoints;
		if ( entry.path_process && entry.path_process->CommitAsyncPathFromPoints( p->final_path_points, p->start_feet, p->goal_feet, p->resolved_policy ) ) {
			entry.status = nav_request_status_t::Completed;
			NavRequest_ClearProcessMarkers( entry );
		} else {
			entry.status = nav_request_status_t::Failed;
			NavRequest_HandleFailure( entry );
			NavRequest_ClearProcessMarkers( entry );
		}
		NavRequest_DestroyPrepPayload( p );
		return;
	}

	/**
	*	Ignore worker output once the request already reached a terminal state.
	**/
	if ( NavRequest_IsTerminalStatus( entry.status ) ) {
		NavRequest_ClearProcessMarkers( entry );
		NavRequest_DestroyPrepPayload( p );
		return;
	}

	/**
	*	Accept worker prep only while the request is still awaiting worker results.
	**/
	if ( entry.status != nav_request_status_t::Preparing && entry.status != nav_request_status_t::Queued ) {
		NavRequest_DestroyPrepPayload( p );
		return;
	}

	/**
	*	Commit a worker-prepared direct shortcut without entering the incremental running phase.
	**/
	if ( p->direct_path_success ) {
		entry.resolved_policy = p->resolved_policy;
		entry.agent_mins = p->agent_mins;
		entry.agent_maxs = p->agent_maxs;
		entry.preserve_detailed_waypoints = p->preserve_detailed_waypoints;
		if ( entry.path_process && entry.path_process->CommitAsyncPathFromPoints( p->direct_path_points, p->start_feet, p->goal_feet, p->resolved_policy ) ) {
			entry.status = nav_request_status_t::Completed;
			NavRequest_ClearProcessMarkers( entry );
		} else {
			entry.status = nav_request_status_t::Failed;
			NavRequest_HandleFailure( entry );
			NavRequest_ClearProcessMarkers( entry );
		}
		NavRequest_DestroyPrepPayload( p );
		return;
	}

	/**
	*	Commit a first-usable provisional path before moving the worker state into the running queue entry.
	*	    This keeps obstructed and cross-Z routes from staying visibly idle while the full A*	solve continues.
	**/
	if ( p->provisional_path_success ) {
		entry.resolved_policy = p->resolved_policy;
		entry.agent_mins = p->agent_mins;
		entry.agent_maxs = p->agent_maxs;
		entry.preserve_detailed_waypoints = p->preserve_detailed_waypoints;
		if ( entry.path_process && !entry.path_process->CommitAsyncPathFromPoints( p->provisional_path_points, p->start_feet, p->goal_feet, p->resolved_policy ) ) {
			entry.status = nav_request_status_t::Failed;
			NavRequest_HandleFailure( entry );
			NavRequest_ClearProcessMarkers( entry );
			NavRequest_DestroyPrepPayload( p );
			return;
		}
	}

	/**
	*	Keep worker-owned searches off the main thread by immediately requeueing the payload for another async slice.
	**/
	if ( p->worker_controls_search && p->init_success && p->initialized_state.has_value() && p->initialized_state->status == nav_a_star_status_t::Running ) {
		entry.resolved_policy = p->resolved_policy;
		entry.agent_mins = p->agent_mins;
		entry.agent_maxs = p->agent_maxs;
		entry.preserve_detailed_waypoints = p->preserve_detailed_waypoints;
		entry.status = nav_request_status_t::Preparing;
		if ( diagLogging ) {
			gi.dprintf( "[NavAsync][Done] Requeueing worker-owned search handle=%d expansions=%d elapsed_ms=%llu provisional=%d\n",
				entry.handle,
				p->worker_expansions,
				( unsigned long long )p->worker_elapsed_ms,
				p->provisional_path_success ? 1 : 0 );
		}
		NavRequest_DispatchWorkerPayload( p );
		return;
	}

	/**
	*	Treat any worker-owned search that did not return a final or requeueable running state as a failure.
	**/
	if ( p->worker_controls_search ) {
		entry.status = nav_request_status_t::Failed;
		entry.a_star.neighbor_try_count = p->diag.neighbor_try_count;
		entry.a_star.no_node_count = p->diag.no_node_count;
		entry.a_star.edge_reject_count = p->diag.edge_reject_count;
		entry.a_star.tile_filter_reject_count = p->diag.tile_filter_reject_count;
		entry.a_star.same_node_alias_count = p->diag.same_node_alias_count;
		entry.a_star.closed_duplicate_count = p->diag.closed_duplicate_count;
		entry.a_star.pass_through_prune_count = p->diag.pass_through_prune_count;
		entry.a_star.stagnation_count = p->diag.stagnation_count;
		for ( int i = 0; i < NAV_EDGE_REJECT_REASON_BUCKETS; ++i ) {
			entry.a_star.edge_reject_reason_counts[ i ] = p->diag.edge_reject_reason_counts[ i ];
		}
		NavRequest_HandleFailure( entry );
		NavRequest_ClearProcessMarkers( entry );
		NavRequest_DestroyPrepPayload( p );
		return;
	}

	/**
	*	Surface worker failure diagnostics on the main thread when init never produced a live state.
	**/
	if ( !p->init_success || !p->initialized_state.has_value() ) {
		entry.status = nav_request_status_t::Failed;
		entry.a_star.neighbor_try_count = p->diag.neighbor_try_count;
		entry.a_star.no_node_count = p->diag.no_node_count;
		entry.a_star.edge_reject_count = p->diag.edge_reject_count;
		entry.a_star.tile_filter_reject_count = p->diag.tile_filter_reject_count;
		entry.a_star.same_node_alias_count = p->diag.same_node_alias_count;
		entry.a_star.closed_duplicate_count = p->diag.closed_duplicate_count;
		entry.a_star.pass_through_prune_count = p->diag.pass_through_prune_count;
		entry.a_star.stagnation_count = p->diag.stagnation_count;
		// Snapshot per-reason edge reject buckets for richer diagnostics.
		for ( int i = 0; i < NAV_EDGE_REJECT_REASON_BUCKETS; ++i ) {
			entry.a_star.edge_reject_reason_counts[ i ] = p->diag.edge_reject_reason_counts[ i ];
		}
		NavRequest_HandleFailure( entry );
		NavRequest_ClearProcessMarkers( entry );
		NavRequest_DestroyPrepPayload( p );
		return;
	}

	/**
   *	Fallback mode:
	*	    Only hand the initialized search state back to the main thread when worker-side ownership
	*	    is explicitly disabled. The streaming worker path should avoid this branch.
	**/
	entry.a_star = std::move( p->initialized_state.value() );
	entry.resolved_policy = p->resolved_policy;
	entry.agent_mins = p->agent_mins;
	entry.agent_maxs = p->agent_maxs;
	entry.preserve_detailed_waypoints = p->preserve_detailed_waypoints;
	entry.a_star.policy = &entry.resolved_policy;
	entry.a_star.pathProcess = entry.path_process;
	entry.a_star.RebindInternalPointers();
	entry.status = nav_request_status_t::Running;

	/**
	*	Emit the existing move-to-running diagnostic once the payload was applied.
	**/
	if ( diagLogging ) {
		gi.dprintf( "[NavAsync][Done] Prepared A*	on main thread by moving worker state handle=%d (gen=%u) path_process=%p\n",
			entry.handle, entry.generation, ( void * )entry.path_process );
	}

	/**
	*	Release the drained payload after its ownership was fully transferred into the queue entry.
	**/
	NavRequest_DestroyPrepPayload( p );
}

/**
*	@brief    Reset the owning path process markers after a queued request finishes.
*	@param    entry    Queue entry whose owning process markers should be cleared.
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
*	@brief    Finalize the incremental search output and commit it back to the caller.
*	@param    entry    Request entry owning the completed state.
*	@return   True if the path was validated and stored.
**/
static bool NavRequest_FinalizePathForEntry( nav_request_entry_t &entry ) {
	svg_nav_path_process_t *process = entry.path_process;
	if ( !process ) {
		return false;
	}

	/**
	*	Stale-result guard:
	*	    Async requests can be refreshed/replaced while an older request is still
	*	    running (see `SVG_Nav_RequestPathAsync` replace semantics).
	**/
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
	*	Apply the shared greedy LOS simplifier to the finalized nav-center points before post-processing safety checks.
	**/
	std::vector<Vector3> simplifiedPoints = points;
	NavRequest_TrySimplifyFinalPathPoints( mesh, entry.agent_mins, entry.agent_maxs, entry.resolved_policy,
		entry.preserve_detailed_waypoints, &simplifiedPoints );

	/**
	*	Enforce post-processed drop limits on the simplified waypoint list.
	**/
	const double maxDrop = ( entry.resolved_policy.max_drop_height_cap > 0. )
		? ( double )entry.resolved_policy.max_drop_height_cap
		: ( entry.resolved_policy.enable_max_drop_height_cap ? ( double )entry.resolved_policy.max_drop_height : ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 100. ) );
	bool dropLimitOk = true;
	const int32_t pointCount = ( int32_t )simplifiedPoints.size();
	if ( pointCount > 1 ) {
		// Iterate segments to detect drops that exceed the configured limit.
		for ( int32_t i = 1; i < pointCount; ++i ) {
			const double dz = simplifiedPoints[ i - 1 ].z - simplifiedPoints[ i ].z;
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

	const bool committed = process->CommitAsyncPathFromPoints( simplifiedPoints, entry.start, entry.goal, entry.resolved_policy );
	if ( committed ) {
		gi.dprintf( "[DEBUG][NavAsync] Finalize: commit succeeded (handle=%d points=%d)\n", entry.handle, ( int )simplifiedPoints.size() );
	} else {
		gi.dprintf( "[DEBUG][NavAsync] Finalize: commit failed (handle=%d)\n", entry.handle );
		// Emit reason-level counters here as well so commit-time failures still include rejection breakdown.
		if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
			NavRequest_LogEdgeRejectReasonCounters( "[NavAsync][HandleFailure][CommitFail]", entry.a_star );
		}
	}
	return committed;
}

/**
*	@brief    Apply failure backoff penalties when incremental A* could not find a path.
*	@param    entry    Request entry whose owning path process should receive failure penalties.
**/
static void NavRequest_HandleFailure( nav_request_entry_t &entry ) {
	svg_nav_path_process_t *process = entry.path_process;
	if ( !process ) {
		return;
	}

	// Temporary diagnostic: print A* internal counters when an async request fails.
	if ( s_nav_nav_async_log_stats && s_nav_nav_async_log_stats->integer != 0 ) {
		gi.dprintf( "[NavAsync][HandleFailure] handle=%d status=%d nodes=%d max_nodes=%d neighbor_tries=%d no_node=%d edge_reject=%d tile_filter_reject=%d same_node_alias=%d closed_duplicate=%d stagnation=%d hit_budget=%d hit_stagnation=%d\n",
			entry.handle,
			( int )entry.a_star.status,
			( int )entry.a_star.nodes.size(),
			entry.a_star.max_nodes,
			entry.a_star.neighbor_try_count,
			entry.a_star.no_node_count,
			entry.a_star.edge_reject_count,
			entry.a_star.tile_filter_reject_count,
			entry.a_star.same_node_alias_count,
			entry.a_star.closed_duplicate_count,
			entry.a_star.stagnation_count,
			entry.a_star.hit_time_budget ? 1 : 0,
			entry.a_star.hit_stagnation_limit ? 1 : 0 );

		gi.dprintf( "[NavAsync][HandleFailure][Detail] handle=%d agent_mins=(%.1f %.1f %.1f) agent_maxs=(%.1f %.1f %.1f) policy: max_step=%.1f min_step=%.1f max_drop=%.1f max_drop_height_cap=%.1f cap_drop=%d allow_jump=%d max_jump=%.1f max_slope=%.1f waypoint_radius=%.1f backoff_base_ms=%d backoff_max_pow=%d\n",
			entry.handle,
			entry.agent_mins.x, entry.agent_mins.y, entry.agent_mins.z,
			entry.agent_maxs.x, entry.agent_maxs.y, entry.agent_maxs.z,
			entry.resolved_policy.max_step_height,
			entry.resolved_policy.min_step_height,
			entry.resolved_policy.max_drop_height,
			entry.resolved_policy.max_drop_height_cap,
			entry.resolved_policy.enable_max_drop_height_cap ? 1 : 0,
			entry.resolved_policy.allow_small_obstruction_jump ? 1 : 0,
			entry.resolved_policy.max_obstruction_jump_height,
			entry.resolved_policy.max_slope_normal_z,
			entry.resolved_policy.waypoint_radius,
			( int )entry.resolved_policy.fail_backoff_base.Milliseconds(),
			entry.resolved_policy.fail_backoff_max_pow );

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

			gi.dprintf( "[NavAsync][HandleFailure][Nodes] start_node=(%.1f %.1f %.1f) goal_node=(%.1f %.1f %.1f) nodes_explored=%d\n",
				s.start_node.worldPosition.x, s.start_node.worldPosition.y, s.start_node.worldPosition.z,
				s.goal_node.worldPosition.x, s.goal_node.worldPosition.y, s.goal_node.worldPosition.z,
				( int )s.nodes.size() );

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
				} else if ( s.neighbor_try_count > 0 && s.same_node_alias_count >= s.neighbor_try_count ) {
					gi.dprintf( "[NavAsync][HandleFailure][Reason] All neighbor queries resolved back onto the currently expanded node (same_node_alias=%d neighbor_tries=%d). This points to canonical node aliasing in lookup or cell-coordinate mapping.\n",
						s.same_node_alias_count, s.neighbor_try_count );
				}
			}

			gi.dprintf( "[NavAsync][HandleFailure][Counts] neighbor_tries=%d no_node=%d edge_reject=%d tile_filter_reject=%d same_node_alias=%d closed_duplicate=%d pass_through_prune=%d stagnation=%d\n",
				( int )s.neighbor_try_count, s.no_node_count, s.edge_reject_count, s.tile_filter_reject_count, s.same_node_alias_count, s.closed_duplicate_count, s.pass_through_prune_count, s.stagnation_count );
			gi.dprintf( "[NavAsync][HandleFailure][NoNode] invalid_current_tile=%d target_tile=%d presence=%d cell_view=%d layer_select=%d\n",
				s.no_node_invalid_current_tile_count,
				s.no_node_target_tile_count,
				s.no_node_presence_count,
				s.no_node_cell_view_count,
				s.no_node_layer_select_count );
			NavRequest_LogEdgeRejectReasonCounters( "[NavAsync][HandleFailure]", s );
		}
	}

	process->consecutive_failures = std::min( process->consecutive_failures + 1, NAV_REQUEST_MAX_CONSECUTIVE_FAILURES );
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
*	@brief    Emit diagnostics summarizing the async navigation request queue state.
*	@param    queueBefore            Queue size before this service pass.
*	@param    queueAfter             Queue size after terminal-entry compaction.
*	@param    processed              Number of queue entries serviced this pass.
*	@param    requestBudget          Combined queue service budget for this pass.
*	@param    remainingExpansions    Global expansion budget still unused after service.
*	@note     Enabled via the `nav_nav_async_log_stats` cvar when troubleshooting queue behavior.
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
*	@brief    Determine if async navigation requests are enabled via cvar.
*	@return   True when async request processing is enabled.
**/
bool SVG_Nav_IsAsyncNavEnabled( void ) {
	SVG_Nav_RequestQueue_RegisterCvars();
	return s_nav_nav_async_enable && s_nav_nav_async_enable->integer != 0;
}

/**
*	@brief    Return whether a second late-frame async queue tick is enabled.
*	@return   True when the late-frame follow-up tick should run.
**/
bool SVG_Nav_ShouldRunLateFrameQueueTick( void ) {
	SVG_Nav_RequestQueue_RegisterCvars();
	return s_nav_late_frame_tick && s_nav_late_frame_tick->integer != 0;
}

/**
*	@brief    Retrieve the configured per-frame request budget.
*	@return   At least one request service slot per frame.
**/
int32_t SVG_Nav_GetAsyncRequestBudget( void ) {
	SVG_Nav_RequestQueue_RegisterCvars();
	const int32_t budget = s_nav_requests_per_frame ? s_nav_requests_per_frame->integer : 1;
	return std::max( 1, budget );
}

/**
*	@brief    Retrieve the configured expansion budget for each request tick.
*	@return   At least one expansion unit for each request step.
**/
int32_t SVG_Nav_GetAsyncRequestExpansions( void ) {
	SVG_Nav_RequestQueue_RegisterCvars();
	const int32_t budget = s_nav_expansions_per_request ? s_nav_expansions_per_request->integer : 1;
	return std::max( 1, budget );
}
