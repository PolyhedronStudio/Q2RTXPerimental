/********************************************************************
*
*
*	ServerGame: Nav2 Benchmark Harness
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"


/**
*
*
*	Nav2 Benchmark Enumerations:
*
*
**/
/**
*	@brief	Stable timing buckets used by the nav2 benchmark harness.
*	@note	These buckets mirror the staged query pipeline so future scheduler integration can
*			accumulate timing data without redesigning the instrumentation surface.
**/
enum class nav2_bench_timing_bucket_t : uint8_t {
    TotalQuery = 0,
    TopologyClassification,
    EndpointSelection,
    CoarseSearch,
    CorridorGeneration,
    FineRefinement,
    MoverLocalRefinement,
    OccupancyQuery,
    WorkerQueueWait,
    WorkerExecutionSlice,
    MainThreadRevalidationCommit,
    SerializationWrite,
    SerializationRead,
    Count
};

/**
*	@brief	Deterministic scenario identifiers tracked by the nav2 benchmark harness.
*	@note	The values are intentionally stable so future compare-mode or serialization tests can
*			refer to scenarios by enum value without relying on fragile strings.
**/
enum class nav2_bench_scenario_t : uint8_t {
    SameFloorOpen = 0,
    SameFloorObstructed,
    HighZGoal,
    Ladder,
    Stair,
    Door,
    FuncPlat,
    FuncRotatingRideTraverse,
    PortalChainedRoute,
    WrongClusterTemptation,
    ManyAgentSimultaneousQuery,
    FatPVSRelevance,
    InlineModelHeadnodeLocalizedQuery,
    SaveLoadRoundTripContinuity,
    Count
};

/**
*	@brief	Failure taxonomy identifiers tracked by the nav2 benchmark harness.
*	@note	These are intended to remain stable so later regression suites can assert specific
*			failure modes without parsing human-readable log strings.
**/
enum class nav2_bench_failure_t : uint8_t {
    WrongFloorConvergence = 0,
    ExcessiveFineAStarExpansion,
    UnreachableFalseNegative,
    CorridorRefinementMismatch,
    WrongClusterOrWrongPortalCommitment,
    MoverBoardingFailure,
    MoverRelativePathDrift,
    StaleSnapshotResult,
    SchedulerStarvation,
    UnfairQueryDelay,
    SerializationVersionMismatch,
    SerializationRoundTripMismatch,
    OwnershipMemoryCleanupMismatch,
    Count
};


/**
*
*
*	Nav2 Benchmark Data Structures:
*
*
**/
/**
*	@brief	Single timing accumulator bucket for nav2 benchmark data.
*	@note	Times are stored in milliseconds so the harness can aggregate both main-thread and
*			worker-slice measurements using the same units.
**/
struct nav2_bench_timing_stat_t {
    //! Total accumulated milliseconds recorded for the bucket.
    double total_ms = 0.0;
    //! Lowest single-sample duration recorded for the bucket.
    double min_ms = 0.0;
    //! Highest single-sample duration recorded for the bucket.
    double max_ms = 0.0;
    //! Number of timing samples added to the bucket.
    uint32_t samples = 0;
};

/**
*	@brief	Aggregate benchmark record for a single deterministic nav2 scenario.
*	@note	This record is intentionally POD-like so it can later be mirrored into save/load or
*			compare-mode snapshots without pointer ownership concerns.
**/
struct nav2_bench_record_t {
    //! Whether the record contains at least one completed measurement.
    bool valid = false;
    //! Scenario identifier the record belongs to.
    nav2_bench_scenario_t scenario = nav2_bench_scenario_t::SameFloorOpen;
    //! Optional human-readable scenario label override used by diagnostics.
    const char *scenario_label = nullptr;
    //! Number of benchmark runs accumulated into this record.
    uint32_t run_count = 0;
    //! Number of runs that produced a successful navigation result.
    uint32_t success_count = 0;
    //! Number of runs that produced a failed navigation result.
    uint32_t failure_count = 0;
    //! Number of failure types recorded for this scenario.
    uint32_t failure_type_counts[ ( size_t )nav2_bench_failure_t::Count ] = {};
    //! Timing accumulators for each pipeline bucket.
    nav2_bench_timing_stat_t timing[ ( size_t )nav2_bench_timing_bucket_t::Count ] = {};
};

/**
*	@brief	Per-query working state used while accumulating nav2 benchmark timings.
*	@note	Callers can keep this on the stack and only commit it if benchmark collection is enabled.
**/
struct nav2_bench_query_t {
    //! Whether the query state has been initialized for the current measurement.
    bool active = false;
    //! Whether benchmark collection was enabled when the query started.
    bool enabled = false;
    //! Scenario identifier currently being measured.
    nav2_bench_scenario_t scenario = nav2_bench_scenario_t::SameFloorOpen;
    //! Optional scenario label override for diagnostics.
    const char *scenario_label = nullptr;
    //! Start time captured when the query began.
    uint64_t query_start_ms = 0;
    //! Bucket start times used while timing individual sections.
    uint64_t bucket_start_ms[ ( size_t )nav2_bench_timing_bucket_t::Count ] = {};
    //! Accumulated milliseconds for the in-flight query.
    double bucket_total_ms[ ( size_t )nav2_bench_timing_bucket_t::Count ] = {};
    //! Whether each bucket currently has an active timing section.
    bool bucket_active[ ( size_t )nav2_bench_timing_bucket_t::Count ] = {};
    //! Whether any failure taxonomy entries were recorded for this query.
    bool had_failure = false;
    //! Failure taxonomy counters captured for this query.
    uint32_t failure_counts[ ( size_t )nav2_bench_failure_t::Count ] = {};
};


/**
*
*
*	Nav2 Benchmark Public API:
*
*
**/
/**
*	@brief	Register nav2 benchmark cvars used to enable or report diagnostics.
*	@note	This should be called during svgame initialization before benchmark helpers are used.
**/
void SVG_Nav2_Bench_RegisterCvars( void );

/**
*	@brief	Reset all accumulated nav2 benchmark records.
*	@note	This clears only benchmark aggregates and does not affect any runtime nav state.
**/
void SVG_Nav2_Bench_Reset( void );

/**
*	@brief	Return whether nav2 benchmark collection is currently enabled.
*	@return	True when the benchmark harness should collect timing and failure data.
**/
const bool SVG_Nav2_Bench_IsEnabled( void );

/**
*	@brief	Begin measuring a deterministic nav2 benchmark query.
*	@param	query		[out] Query working state to initialize.
*	@param	scenario	Scenario identifier being measured.
*	@param	scenarioLabel	Optional scenario label override used in diagnostics.
*	@return	True when benchmark collection is enabled and the query state was initialized.
*	@note	Callers may skip all further benchmark work when this returns false.
**/
const bool SVG_Nav2_Bench_BeginQuery( nav2_bench_query_t *query, const nav2_bench_scenario_t scenario, const char *scenarioLabel = nullptr );

/**
*	@brief	Start timing a specific benchmark bucket for an active query.
*	@param	query	Active benchmark query state.
*	@param	bucket	Timing bucket to start.
*	@note	If the bucket is already active this helper leaves the existing start time intact.
**/
void SVG_Nav2_Bench_BeginBucket( nav2_bench_query_t *query, const nav2_bench_timing_bucket_t bucket );

/**
*	@brief	Finish timing a specific benchmark bucket for an active query.
*	@param	query	Active benchmark query state.
*	@param	bucket	Timing bucket to stop.
*	@note	The measured duration is accumulated into the per-query working totals.
**/
void SVG_Nav2_Bench_EndBucket( nav2_bench_query_t *query, const nav2_bench_timing_bucket_t bucket );

/**
*	@brief	Add an explicit duration to a timing bucket without a begin/end pair.
*	@param	query	Active benchmark query state.
*	@param	bucket	Timing bucket receiving the duration.
*	@param	elapsedMs	Duration in milliseconds.
*	@note	Useful when a stage reports a pre-measured slice duration.
**/
void SVG_Nav2_Bench_AddDuration( nav2_bench_query_t *query, const nav2_bench_timing_bucket_t bucket, const double elapsedMs );

/**
*	@brief	Record a benchmark failure taxonomy entry for an active query.
*	@param	query	Active benchmark query state.
*	@param	failure	Failure taxonomy identifier to increment.
**/
void SVG_Nav2_Bench_RecordFailure( nav2_bench_query_t *query, const nav2_bench_failure_t failure );

/**
*	@brief	Commit a completed benchmark query into the aggregate scenario record.
*	@param	query	Active benchmark query state.
*	@param	success	True when the measured query completed successfully.
*	@note	This automatically finalizes the total-query bucket if it is still active.
**/
void SVG_Nav2_Bench_EndQuery( nav2_bench_query_t *query, const bool success );

/**
*	@brief	Retrieve the aggregate benchmark record for a scenario.
*	@param	scenario	Scenario identifier to retrieve.
*	@return	Const pointer to the record, or `nullptr` if the scenario is out of range.
**/
const nav2_bench_record_t *SVG_Nav2_Bench_GetRecord( const nav2_bench_scenario_t scenario );

/**
*	@brief	Retrieve the most recent benchmark record that received a completed query.
*	@return	Const pointer to the last updated record, or `nullptr` if no record is available.
**/
const nav2_bench_record_t *SVG_Nav2_Bench_GetLastRecord( void );

/**
*	@brief	Return a stable display name for a timing bucket.
*	@param	bucket	Timing bucket to convert.
*	@return	Constant string name used in diagnostics.
**/
const char *SVG_Nav2_Bench_TimingBucketName( const nav2_bench_timing_bucket_t bucket );

/**
*	@brief	Return a stable display name for a benchmark scenario.
*	@param	scenario	Scenario identifier to convert.
*	@return	Constant string name used in diagnostics.
**/
const char *SVG_Nav2_Bench_ScenarioName( const nav2_bench_scenario_t scenario );

/**
*	@brief	Return a stable display name for a benchmark failure taxonomy value.
*	@param	failure	Failure taxonomy identifier to convert.
*	@return	Constant string name used in diagnostics.
**/
const char *SVG_Nav2_Bench_FailureName( const nav2_bench_failure_t failure );

/**
*	@brief	Emit an aggregate benchmark summary to the developer console.
*	@param	scenario	Scenario to dump, or `Count` to dump all valid scenarios.
*	@note	This helper is intended for low-frequency developer diagnostics, not per-frame logging.
**/
void SVG_Nav2_Bench_DumpRecord( const nav2_bench_scenario_t scenario = nav2_bench_scenario_t::Count );
