/********************************************************************
*
*
*	ServerGame: Navigation Performance and Benchmarking Framework
*	File: nav_perf.h
*	Description:
*		High-resolution performance instrumentation, percentile calculation,
*		request telemetry, and deterministic pathfinding benchmark harness.
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"
#include "svgame/nav/nav_types.h"
#include "svgame/nav/nav_perf_metrics.h"

#include <chrono>





/**
*	@brief	Phases of server tick and navigation pipeline tracked by high-resolution performance counters.
**/
enum nav_perf_phase_t : int32_t {
	NAV_PERF_PHASE_SERVER_TICK = 0,
	NAV_PERF_PHASE_NAV_ORCHESTRATION,
	NAV_PERF_PHASE_START_GOAL_LOOKUP,
	NAV_PERF_PHASE_GLOBAL_FALLBACKS,
	NAV_PERF_PHASE_CACHE_BUILD,
	NAV_PERF_PHASE_ASTAR_SEARCH,
	NAV_PERF_PHASE_PORTAL_FUNNEL,
	NAV_PERF_PHASE_CORNER_REFINEMENT,
	NAV_PERF_PHASE_GEOMETRIC_LOS,
	NAV_PERF_PHASE_MOVEMENT_TRACES,
	NAV_PERF_PHASE_COVER_SCORING_TRACES,
	NAV_PERF_PHASE_CROWD_COORDINATION,
	NAV_PERF_PHASE_DEBUG_DRAWING,
	NAV_PERF_PHASE_NAV_COMMIT,
	NAV_PERF_PHASE_CROWD_GUIDE,
	//! Native steering/animation decision work, independent of graph orchestration.
	NAV_PERF_PHASE_ACTOR_STEERING,
	//! Authoritative movement execution, including collision and support updates.
	NAV_PERF_PHASE_ACTOR_MOVEMENT,
	//! Inclusive synchronous crowd order preparation/assignment; guide solve has its own nested phase.
	NAV_PERF_PHASE_CROWD_ASSIGNMENT,
	NAV_PERF_PHASE_COUNT
};

/**
*	@brief	Request reasons for monster path recalculations.
**/
enum nav_perf_request_reason_t : int32_t {
	NAV_PERF_REQ_REASON_NEW_GOAL = 0,
	NAV_PERF_REQ_REASON_MOVED_GOAL,
	NAV_PERF_REQ_REASON_FORCE,
	NAV_PERF_REQ_REASON_BLOCKED,
	NAV_PERF_REQ_REASON_TOPOLOGY,
	NAV_PERF_REQ_REASON_COUNT
};

/**
*	@brief	Outcomes of path recalculation attempts.
**/
enum nav_perf_request_outcome_t : int32_t {
	NAV_PERF_OUTCOME_NEW_PATH = 0,
	NAV_PERF_OUTCOME_REUSED_CACHED,
	NAV_PERF_OUTCOME_FAILED_NO_ROUTE,
	NAV_PERF_OUTCOME_COOLDOWN_THROTTLED,
	NAV_PERF_OUTCOME_COUNT
};

/**
*	@brief	Unsuccessful owner-observed worker completion dispositions.
*	@note	Successful installs are counted by Nav_Perf_RecordWorkerCommit instead.
**/
enum class nav_perf_worker_terminal_t : uint32_t {
	NoRoute = 0,
	Stale,
	Cancelled,
	ResourceLimited,
	GoalSuperseded,
	CorridorDetached,
	Count
};

/**
*	@brief	Summary distribution statistics computed for a measured performance phase.
**/
struct nav_perf_stat_summary_t {
	//! Total samples collected since reset; lifetime totals/min/max cover all of them.
	uint64_t sample_count = 0;
	//! Number of most recent samples used for percentiles, bounded to 4096 per distribution.
	uint64_t percentile_sample_count = 0;
	//! Cumulative wall-clock elapsed time across all samples (microseconds).
	double total_us = 0.0;
	//! Cumulative thread CPU time across all samples (microseconds).
	double total_cpu_us = 0.0;
	//! Arithmetic mean wall-clock time (microseconds).
	double mean_us = 0.0;
	//! Minimum observed wall-clock duration (microseconds).
	double min_us = 0.0;
	//! Maximum observed wall-clock duration (microseconds).
	double max_us = 0.0;
	//! 50th percentile / median duration (microseconds).
	double p50_us = 0.0;
	//! 95th percentile duration (microseconds).
	double p95_us = 0.0;
	//! 99th percentile duration (microseconds).
	double p99_us = 0.0;
};

/**
*	@brief	Query high-resolution thread CPU time in nanoseconds where supported.
*	@return	Thread CPU time in nanoseconds, or 0 when unsupported.
**/
uint64_t Nav_Perf_GetThreadCpuTimeNs( void );

/**
*	@brief	Check whether explicit diagnostic collection is enabled.
*	@return	False during ordinary gameplay unless nav_perf_enable or an explicit reset enables collection.
*	@note	This performs no clock reads; collision and query behavior never depend on its result.
**/
bool Nav_Perf_IsEnabled( void );


/**
*	@brief	Initialize navigation performance tracking structures.
**/
void Nav_Perf_Init( void );

/**
*	@brief	Reset all performance sample histories, telemetry counters, and phase accumulators.
**/
void Nav_Perf_Reset( void );

/**
*	@brief	Record an elapsed duration for a specific navigation pipeline phase.
*	@param	phase		The phase to record.
*	@param	elapsed_us	Wall-clock duration in microseconds.
*	@param	cpu_us		Thread CPU duration in microseconds.
**/
void Nav_Perf_RecordPhase( const nav_perf_phase_t phase, const double elapsed_us, const double cpu_us = 0.0 );

/**
*	@brief	Enter a selected owner scope without changing historical per-call phase distributions.
*	@param	phase	Only orchestration and worker-native-completion participate in the tick metric.
*	@note	Nested selected scopes are measured once by their outermost elapsed interval.
**/
void Nav_Perf_BeginOwnerScope( const nav_perf_phase_t phase );

/**
*	@brief	Close a selected owner scope and add its outermost paired cost to the current tick.
*	@param	phase	Phase passed to the matching scope begin.
*	@param	elapsed_us	This scope's measured wall time in microseconds.
*	@param	cpu_us	This scope's measured owner CPU time in microseconds, or zero if unavailable.
**/
void Nav_Perf_EndOwnerScope( const nav_perf_phase_t phase, const double elapsed_us, const double cpu_us );

/**
*	@brief	Begin zero-inclusive owner navigation aggregation for one accepted gameplay tick.
*	@note	Generation-exclusive frames are excluded; call before query delivery and actor work.
**/
void Nav_Perf_BeginTick( void );

/**
*	@brief	Record latency only after a successful current-identity worker route installation.
*	@param	submitted_ns	Latest actual owner submission attempt in steady-clock nanoseconds.
*	@param	start_ns	Copied worker solve-start timestamp in the same clock namespace.
*	@param	finish_ns	Copied worker solve-finish timestamp before reply publication.
*	@param	completed_ns	Successful native owner installation timestamp in the same clock namespace.
*	@note	Failures do not enter latency distributions; invalid timestamp order is counted explicitly.
**/
void Nav_Perf_RecordWorkerCommit( const uint64_t submitted_ns, const uint64_t start_ns,
	const uint64_t finish_ns, const uint64_t completed_ns );

/**
*	@brief	Count an unsuccessful owner-observed worker terminal disposition.
*	@param	terminal	No-route, stale, cancelled or resource-limited outcome observed by the owner.
*	@note	This counts calls, not unseen discarded service replies; service counters remain separate.
**/
void Nav_Perf_CountWorkerTerminal( const nav_perf_worker_terminal_t terminal );

/**
*	@brief	Record total server tick execution time and check for 25 ms deadline violations.
*	@param	tick_time_us	Total tick duration in microseconds.
**/
void Nav_Perf_RecordTick( const double tick_time_us );

/**
*	@brief	Increment path request telemetry by reason and outcome.
*	@param	reason		Reason trigger for the recalculation.
*	@param	outcome		Result of the path evaluation.
**/
void Nav_Perf_CountRequest( const nav_perf_request_reason_t reason, const nav_perf_request_outcome_t outcome );

/**
*	@brief	Record search complexity statistics from an A* invocation.
*	@param	expansions	Number of nodes expanded during the graph search.
*	@param	stalePops	Number of stale/redundant priority queue pops encountered.
*	@param	peakHeap	Peak size reached by the priority queue during search.
*	@param	success		Whether the search reached the goal.
**/
void Nav_Perf_RecordAStarSearch( const int32_t expansions, const int32_t stalePops, const size_t peakHeap, const bool success );

/**
*	@brief	Record spatial KD candidate visitation count.
*	@param	cellCount		Number of spatial cells or leaves visited.
*	@param	candidateCount	Number of candidate faces evaluated.
**/
void Nav_Perf_RecordSpatialCandidate( const int32_t cellCount, const int32_t candidateCount );

/**
*	@brief	Record global face scan invocation and number of faces inspected.
*	@param	faceCount	Total faces scanned during global fallback.
**/
void Nav_Perf_RecordGlobalFaceScan( const int32_t faceCount );

/**
*	@brief	Record temporary buffer copy size for memory traffic monitoring.
*	@param	bytesCopied	Number of bytes copied.
**/
void Nav_Perf_RecordBufferCopy( const size_t bytesCopied );

/**
*	@brief	Record dynamic memory allocation or reallocation event.
*	@param	bytesAllocated	Size of allocation in bytes.
**/
void Nav_Perf_RecordAllocEvent( const size_t bytesAllocated );

/**
*	@brief	Compute summary statistics for a given phase over its collected sample reservoir.
*	@param	phase		The phase to inspect.
*	@param	outSummary	[out] Populated summary distribution struct.
**/
void Nav_Perf_GetPhaseSummary( const nav_perf_phase_t phase, nav_perf_stat_summary_t *outSummary );

/**
*	@brief	Print formatted performance summary to developer console.
*	@param	json_format	When true, formats output as machine-readable JSON.
**/
void Nav_Perf_Report( const bool json_format = false );

/**
*	@brief	Run deterministic path benchmark across N iterations and report latency distributions.
*	@param	runs		Number of benchmark iterations.
*	@param	start		World-space feet start position in double precision.
*	@param	goal		World-space feet goal position in double precision.
*	@param	json_format	Emit machine-readable JSON output.
**/
void Nav_Perf_BenchPath( const int32_t runs, const Vector3DP &start, const Vector3DP &goal, const bool json_format = false );

/**
*	@brief	Run automated verification of M1 gate invariants:
*			1. Clean rejection of intentionally invalid endpoints (out-of-bounds, in-solid, unreachable)
*			2. Detection and rejection of duplicate slot assignments in crowd formations
*			3. Detection and rejection of stalled/incomplete crowd member arrivals
*	@return	True if all gate invariant checks pass, false if any test fails.
**/
bool Nav_Perf_CheckGates( void );

/**
*	Server console command dispatchers:
**/
void Nav_Perf_ResetCommand( void );
void Nav_Perf_ReportCommand( void );
void Nav_Perf_BenchPathCommand( void );
void Nav_Perf_CheckGatesCommand( void );

/**
*	@brief	RAII scope timer that records execution time for a given performance phase.
**/
class nav_perf_scope_t {
public:
	/**
	*	@brief	Construct and start clock for the specified phase.
	*	@param	phase	Pipeline phase to monitor.
	**/
	inline explicit nav_perf_scope_t( const nav_perf_phase_t phase )
		: m_phase( phase ), m_active( Nav_Perf_IsEnabled() ) {
		/**
		*	Ordinary gameplay does not pay diagnostic wall/CPU clock or sample-ring costs.
		**/
		if ( !m_active ) {
			return;
		}
		m_start_wall = std::chrono::steady_clock::now();
		m_start_cpu_ns = Nav_Perf_GetThreadCpuTimeNs();
		// Selected nested callbacks keep their per-call samples while owner ticks count the outer interval once.
		Nav_Perf_BeginOwnerScope( m_phase );
	}

	/**
	*	@brief	Destruct and record elapsed wall and CPU durations.
	**/
	inline ~nav_perf_scope_t() {
		// Match the constructor's decision even when a diagnostic setting changes during the scope.
		if ( !m_active ) {
			return;
		}
		const auto end_wall = std::chrono::steady_clock::now();
		const double elapsed_us = std::chrono::duration<double, std::micro>( end_wall - m_start_wall ).count();
		const uint64_t end_cpu_ns = Nav_Perf_GetThreadCpuTimeNs();
		const double cpu_us = ( end_cpu_ns >= m_start_cpu_ns && m_start_cpu_ns > 0 )
			? static_cast<double>( end_cpu_ns - m_start_cpu_ns ) / 1000.0
			: 0.0;
		Nav_Perf_RecordPhase( m_phase, elapsed_us, cpu_us );
		// End the paired outer interval after retaining the historical phase call sample.
		Nav_Perf_EndOwnerScope( m_phase, elapsed_us, cpu_us );
	}

private:
	//! Phase measured by this scope.
	nav_perf_phase_t m_phase;
	//! Whether this scope opened a diagnostic interval.
	bool m_active = false;
	//! Wall-clock start time point.
	std::chrono::steady_clock::time_point m_start_wall = {};
	//! Thread CPU start time in nanoseconds.
	uint64_t m_start_cpu_ns = 0;
};
