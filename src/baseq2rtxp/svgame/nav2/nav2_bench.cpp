/********************************************************************
*
*
*	ServerGame: Nav2 Benchmark Harness - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_bench.h"

#include <algorithm>
#include <limits>


/**
*
*
*	Nav2 Benchmark Global State:
*
*
**/
//! Master toggle controlling whether nav2 benchmark collection is active.
static cvar_t *s_nav2_bench_enable = nullptr;
//! Optional verbosity toggle controlling whether completed benchmark queries print a compact summary.
static cvar_t *s_nav2_bench_log_summary = nullptr;
//! Aggregate per-scenario benchmark records.
static nav2_bench_record_t s_nav2_bench_records[ ( size_t )nav2_bench_scenario_t::Count ] = {};
//! Pointer to the most recently updated benchmark record.
static nav2_bench_record_t *s_nav2_bench_last_record = nullptr;


/**
*
*
*	Nav2 Benchmark Internal Helpers:
*
*
**/
/**
*	@brief	Return the current monotonic engine time used for benchmark measurements.
*	@return	Current realtime in milliseconds.
*	@note	Uses the engine realtime import so the harness can measure both main-thread and
*			worker-adjacent stages using a shared clock source.
**/
static uint64_t Nav2_Bench_GetNowMs( void ) {
    /**
    *    Use the engine realtime import when available so diagnostics stay consistent with
    *    other server-side timing systems.
    **/
    if ( gi.GetRealTime ) {
        // Return the engine-provided realtime in milliseconds.
        return gi.GetRealTime();
    }

    /**
    *    Conservative fallback: use zero when realtime is unavailable.
    *    This keeps the harness safe during very early initialization paths.
    **/
    // Return a safe default value.
    return 0;
}

/**
*	@brief	Retrieve a mutable aggregate record for a benchmark scenario.
*	@param	scenario	Scenario identifier to resolve.
*	@return	Pointer to the record, or `nullptr` if the scenario is out of range.
**/
static nav2_bench_record_t *Nav2_Bench_GetMutableRecord( const nav2_bench_scenario_t scenario ) {
    /**
    *    Sanity-check the scenario enum value before indexing the static record array.
    **/
    const size_t scenarioIndex = ( size_t )scenario;
    if ( scenarioIndex >= ( size_t )nav2_bench_scenario_t::Count ) {
        // Return nullptr for invalid scenario identifiers.
        return nullptr;
    }

    // Return the address of the aggregate scenario record.
    return &s_nav2_bench_records[ scenarioIndex ];
}

/**
*	@brief	Accumulate a single duration sample into an aggregate timing bucket.
*	@param	stat		[out] Timing accumulator to update.
*	@param	elapsedMs	Measured duration in milliseconds.
**/
static void Nav2_Bench_AccumulateTimingStat( nav2_bench_timing_stat_t *stat, const double elapsedMs ) {
    /**
    *    Sanity-check the output accumulator before attempting to update it.
    **/
    if ( !stat ) {
        // Early-out when no destination accumulator is available.
        return;
    }

    /**
    *    Clamp negative or invalid durations to zero so bad callers do not corrupt the
    *    aggregate benchmark statistics.
    **/
    const double sanitizedElapsedMs = std::max( 0.0, elapsedMs );

    /**
    *    Initialize min/max bounds on the first sample so future comparisons have valid
    *    starting state.
    **/
    if ( stat->samples == 0 ) {
        // Seed the minimum duration from the first sample.
        stat->min_ms = sanitizedElapsedMs;
        // Seed the maximum duration from the first sample.
        stat->max_ms = sanitizedElapsedMs;
    } else {
        // Tighten the minimum duration if the new sample is smaller.
        stat->min_ms = std::min( stat->min_ms, sanitizedElapsedMs );
        // Expand the maximum duration if the new sample is larger.
        stat->max_ms = std::max( stat->max_ms, sanitizedElapsedMs );
    }

    /**
    *    Accumulate totals after min/max bounds are updated.
    **/
    // Add the duration into the running total.
    stat->total_ms += sanitizedElapsedMs;
    // Increment the sample counter.
    stat->samples++;
}

/**
*	@brief	Finish all active bucket timers for a query.
*	@param	query	Active benchmark query state.
*	@note	This protects aggregate timings from callers that forget to explicitly close a bucket.
**/
static void Nav2_Bench_EndAllActiveBuckets( nav2_bench_query_t *query ) {
    /**
    *    Sanity-check the query before attempting to iterate the bucket state arrays.
    **/
    if ( !query || !query->active || !query->enabled ) {
        // There is nothing to finalize for an inactive benchmark query.
        return;
    }

    /**
    *    Iterate every bucket and end any timer that is still active so partial paths still
    *    produce coherent aggregate timings.
    **/
    for ( size_t bucketIndex = 0; bucketIndex < ( size_t )nav2_bench_timing_bucket_t::Count; bucketIndex++ ) {
        // Skip buckets that are not currently timing.
        if ( !query->bucket_active[ bucketIndex ] ) {
            continue;
        }

        // End the active bucket through the public helper so duration handling stays centralized.
        SVG_Nav2_Bench_EndBucket( query, ( nav2_bench_timing_bucket_t )bucketIndex );
    }
}

/**
*	@brief	Emit a compact per-query benchmark summary when verbose logging is enabled.
*	@param	query	Completed benchmark query state.
*	@param	success	True when the query completed successfully.
**/
static void Nav2_Bench_LogCompletedQuery( const nav2_bench_query_t *query, const bool success ) {
    /**
    *    Respect the explicit summary logging toggle so the benchmark harness does not introduce
    *    the log spam that earlier navigation debugging suffered from.
    **/
    if ( !query || !query->enabled || !s_nav2_bench_log_summary || s_nav2_bench_log_summary->integer == 0 ) {
        // Keep the harness quiet unless the developer explicitly requests summary output.
        return;
    }

    /**
    *    Read the most important summary values up front so the single console print remains easy
    *    to scan when developers are checking scenario baselines.
    **/
    const double totalQueryMs = query->bucket_total_ms[ ( size_t )nav2_bench_timing_bucket_t::TotalQuery ];
    const uint32_t failureCount = query->had_failure ? 1u : 0u;
    const char *scenarioName = query->scenario_label ? query->scenario_label : SVG_Nav2_Bench_ScenarioName( query->scenario );

    // Print one compact summary line for the completed benchmark query.
    gi.dprintf( "[Nav2Bench] scenario=%s success=%d total_ms=%.3f failure_flags=%u\n",
        scenarioName ? scenarioName : "Unknown",
        success ? 1 : 0,
        totalQueryMs,
        failureCount );
}


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
void SVG_Nav2_Bench_RegisterCvars( void ) {
    /**
    *    Register the master enable toggle once so the harness can stay near-zero overhead when
    *    benchmarking is disabled.
    **/
    if ( !s_nav2_bench_enable ) {
        // Register the benchmark master enable cvar.
        s_nav2_bench_enable = gi.cvar( "nav2_bench_enable", "0", 0 );
    }

    /**
    *    Register the optional summary logging toggle separately so developers can collect timing
    *    data without automatically spamming the console every query.
    **/
    if ( !s_nav2_bench_log_summary ) {
        // Register the compact per-query summary logging toggle.
        s_nav2_bench_log_summary = gi.cvar( "nav2_bench_log_summary", "0", 0 );
    }
}

/**
*	@brief	Reset all accumulated nav2 benchmark records.
*	@note	This clears only benchmark aggregates and does not affect any runtime nav state.
**/
void SVG_Nav2_Bench_Reset( void ) {
    /**
    *    Clear every aggregate scenario record so future runs start from a clean baseline.
    **/
    std::fill( std::begin( s_nav2_bench_records ), std::end( s_nav2_bench_records ), nav2_bench_record_t{} );

    /**
    *    Clear the last-record pointer so callers do not accidentally inspect stale benchmark data.
    **/
    // Drop the pointer to the previous last-updated record.
    s_nav2_bench_last_record = nullptr;
}

/**
*	@brief	Return whether nav2 benchmark collection is currently enabled.
*	@return	True when the benchmark harness should collect timing and failure data.
**/
const bool SVG_Nav2_Bench_IsEnabled( void ) {
    /**
    *    Require explicit enablement so the harness remains dormant during ordinary gameplay.
    **/
    return s_nav2_bench_enable != nullptr && s_nav2_bench_enable->integer != 0;
}

/**
*	@brief	Begin measuring a deterministic nav2 benchmark query.
*	@param	query		[out] Query working state to initialize.
*	@param	scenario	Scenario identifier being measured.
*	@param	scenarioLabel	Optional scenario label override used in diagnostics.
*	@return	True when benchmark collection is enabled and the query state was initialized.
*	@note	Callers may skip all further benchmark work when this returns false.
**/
const bool SVG_Nav2_Bench_BeginQuery( nav2_bench_query_t *query, const nav2_bench_scenario_t scenario, const char *scenarioLabel ) {
    /**
    *    Validate the caller-provided query storage before attempting to initialize it.
    **/
    if ( !query ) {
        // Return false when the caller did not provide benchmark query storage.
        return false;
    }

    /**
    *    Reset the full working state up front so reused query objects do not retain stale bucket
    *    timings or failure flags from earlier runs.
    **/
    *query = nav2_bench_query_t{};

    /**
    *    Record whether collection is enabled for this query so later helpers can early-out without
    *    re-checking global cvars on every call site.
    **/
    query->enabled = SVG_Nav2_Bench_IsEnabled();
    if ( !query->enabled ) {
        // Leave the query inactive when benchmarking is disabled.
        return false;
    }

    /**
    *    Capture the scenario metadata and initialize the total-query timer immediately so the
    *    aggregate record always includes end-to-end wall-clock timing.
    **/
    query->active = true;
    query->scenario = scenario;
    query->scenario_label = scenarioLabel;
    query->query_start_ms = Nav2_Bench_GetNowMs();
    query->bucket_start_ms[ ( size_t )nav2_bench_timing_bucket_t::TotalQuery ] = query->query_start_ms;
    query->bucket_active[ ( size_t )nav2_bench_timing_bucket_t::TotalQuery ] = true;

    // Return true to tell callers that benchmark collection is active for this query.
    return true;
}

/**
*	@brief	Start timing a specific benchmark bucket for an active query.
*	@param	query	Active benchmark query state.
*	@param	bucket	Timing bucket to start.
*	@note	If the bucket is already active this helper leaves the existing start time intact.
**/
void SVG_Nav2_Bench_BeginBucket( nav2_bench_query_t *query, const nav2_bench_timing_bucket_t bucket ) {
    /**
    *    Ignore calls for inactive or disabled queries so normal gameplay paths can safely invoke
    *    this helper behind minimal guard logic.
    **/
    if ( !query || !query->active || !query->enabled ) {
        // There is no active benchmark query to time.
        return;
    }

    /**
    *    Validate the bucket enum before indexing into the per-query timer arrays.
    **/
    const size_t bucketIndex = ( size_t )bucket;
    if ( bucketIndex >= ( size_t )nav2_bench_timing_bucket_t::Count ) {
        // Ignore invalid benchmark bucket identifiers.
        return;
    }

    /**
    *    Preserve the first start time for already-active buckets so nested callers do not
    *    accidentally truncate a timing section.
    **/
    if ( query->bucket_active[ bucketIndex ] ) {
        // Keep the existing timing section intact.
        return;
    }

    // Capture the current realtime for the bucket start.
    query->bucket_start_ms[ bucketIndex ] = Nav2_Bench_GetNowMs();
    // Mark the bucket as actively timing.
    query->bucket_active[ bucketIndex ] = true;
}

/**
*	@brief	Finish timing a specific benchmark bucket for an active query.
*	@param	query	Active benchmark query state.
*	@param	bucket	Timing bucket to stop.
*	@note	The measured duration is accumulated into the per-query working totals.
**/
void SVG_Nav2_Bench_EndBucket( nav2_bench_query_t *query, const nav2_bench_timing_bucket_t bucket ) {
    /**
    *    Ignore calls when there is no active benchmark query or when the bucket was never started.
    **/
    if ( !query || !query->active || !query->enabled ) {
        // There is no active benchmark query to finalize.
        return;
    }

    /**
    *    Validate the bucket enum before reading any per-bucket timer state.
    **/
    const size_t bucketIndex = ( size_t )bucket;
    if ( bucketIndex >= ( size_t )nav2_bench_timing_bucket_t::Count ) {
        // Ignore invalid benchmark bucket identifiers.
        return;
    }
    if ( !query->bucket_active[ bucketIndex ] ) {
        // Ignore end requests for buckets that are not currently timing.
        return;
    }

    /**
    *    Compute the elapsed duration from the stored bucket start time and accumulate it into the
    *    query-local totals before clearing the active flag.
    **/
    const uint64_t nowMs = Nav2_Bench_GetNowMs();
    const uint64_t startMs = query->bucket_start_ms[ bucketIndex ];
    const double elapsedMs = ( nowMs >= startMs ) ? ( double )( nowMs - startMs ) : 0.0;
    query->bucket_total_ms[ bucketIndex ] += elapsedMs;
    query->bucket_active[ bucketIndex ] = false;
    query->bucket_start_ms[ bucketIndex ] = 0;
}

/**
*	@brief	Add an explicit duration to a timing bucket without a begin/end pair.
*	@param	query	Active benchmark query state.
*	@param	bucket	Timing bucket receiving the duration.
*	@param	elapsedMs	Duration in milliseconds.
*	@note	Useful when a stage reports a pre-measured slice duration.
**/
void SVG_Nav2_Bench_AddDuration( nav2_bench_query_t *query, const nav2_bench_timing_bucket_t bucket, const double elapsedMs ) {
    /**
    *    Ignore direct-duration updates for inactive or disabled queries.
    **/
    if ( !query || !query->active || !query->enabled ) {
        // There is no active benchmark query to update.
        return;
    }

    /**
    *    Validate the bucket enum before touching the per-query accumulation array.
    **/
    const size_t bucketIndex = ( size_t )bucket;
    if ( bucketIndex >= ( size_t )nav2_bench_timing_bucket_t::Count ) {
        // Ignore invalid benchmark bucket identifiers.
        return;
    }

    // Accumulate the sanitized explicit duration into the bucket total.
    query->bucket_total_ms[ bucketIndex ] += std::max( 0.0, elapsedMs );
}

/**
*	@brief	Record a benchmark failure taxonomy entry for an active query.
*	@param	query	Active benchmark query state.
*	@param	failure	Failure taxonomy identifier to increment.
**/
void SVG_Nav2_Bench_RecordFailure( nav2_bench_query_t *query, const nav2_bench_failure_t failure ) {
    /**
    *    Ignore failure recording when there is no active benchmark query.
    **/
    if ( !query || !query->active || !query->enabled ) {
        // There is no active benchmark query to annotate.
        return;
    }

    /**
    *    Validate the failure enum before incrementing the taxonomy counter array.
    **/
    const size_t failureIndex = ( size_t )failure;
    if ( failureIndex >= ( size_t )nav2_bench_failure_t::Count ) {
        // Ignore invalid failure taxonomy identifiers.
        return;
    }

    // Mark that the query experienced at least one benchmark failure condition.
    query->had_failure = true;
    // Increment the per-query failure taxonomy counter.
    query->failure_counts[ failureIndex ]++;
}

/**
*	@brief	Commit a completed benchmark query into the aggregate scenario record.
*	@param	query	Active benchmark query state.
*	@param	success	True when the measured query completed successfully.
*	@note	This automatically finalizes the total-query bucket if it is still active.
**/
void SVG_Nav2_Bench_EndQuery( nav2_bench_query_t *query, const bool success ) {
    /**
    *    Ignore completion requests for inactive or disabled queries so callers can end benchmark
    *    scopes unconditionally without extra branching.
    **/
    if ( !query || !query->active || !query->enabled ) {
        // There is no active benchmark query to commit.
        return;
    }

    /**
    *    Finalize any still-open bucket timers before committing aggregate statistics. This includes
    *    the total-query bucket that was started automatically at query begin.
    **/
    Nav2_Bench_EndAllActiveBuckets( query );

    /**
    *    Resolve the destination scenario record before attempting to merge any query-local data.
    **/
     nav2_bench_record_t *record = Nav2_Bench_GetMutableRecord( query->scenario );
    if ( !record ) {
        // Clear the query state even when the scenario id is invalid.
        query->active = false;
        return;
    }

    /**
    *    Initialize record metadata on first use so future aggregate dumps can print stable scenario
    *    names even if later queries do not pass an explicit label override.
    **/
    if ( !record->valid ) {
        // Mark the scenario record as containing valid benchmark data.
        record->valid = true;
        // Store the scenario enum for later diagnostic dumps.
        record->scenario = query->scenario;
    }
    if ( query->scenario_label ) {
        // Keep the most recent explicit scenario label override for diagnostics.
        record->scenario_label = query->scenario_label;
    }

    /**
    *    Update the aggregate success/failure counters before merging timing and taxonomy data.
    **/
    // Increment the total run counter for the scenario.
    record->run_count++;
    if ( success ) {
        // Count the completed query as a successful run.
        record->success_count++;
    } else {
        // Count the completed query as a failed run.
        record->failure_count++;
    }

    /**
    *    Merge timing totals from the completed query into each aggregate scenario bucket.
    **/
    for ( size_t bucketIndex = 0; bucketIndex < ( size_t )nav2_bench_timing_bucket_t::Count; bucketIndex++ ) {
        // Skip buckets that recorded no time for this query.
        if ( query->bucket_total_ms[ bucketIndex ] <= 0.0 ) {
            continue;
        }

        // Accumulate the query-local timing sample into the aggregate scenario bucket.
        Nav2_Bench_AccumulateTimingStat( &record->timing[ bucketIndex ], query->bucket_total_ms[ bucketIndex ] );
    }

    /**
    *    Merge per-query failure taxonomy counters into the aggregate scenario record.
    **/
    for ( size_t failureIndex = 0; failureIndex < ( size_t )nav2_bench_failure_t::Count; failureIndex++ ) {
        // Skip failure taxonomy values that were not observed in this query.
        if ( query->failure_counts[ failureIndex ] == 0 ) {
            continue;
        }

        // Accumulate the observed failure taxonomy count into the scenario aggregate.
        record->failure_type_counts[ failureIndex ] += query->failure_counts[ failureIndex ];
    }

    /**
    *    Publish the last-updated record pointer so diagnostics and compare-mode helpers can easily
    *    retrieve the most recent aggregate result.
    **/
    s_nav2_bench_last_record = record;

    /**
    *    Optionally emit a compact summary now that the aggregate state has been updated.
    **/
    Nav2_Bench_LogCompletedQuery( query, success );

    /**
    *    Clear the query state so accidental reuse after commit cannot mutate the completed run.
    **/
    *query = nav2_bench_query_t{};
}

/**
*	@brief	Retrieve the aggregate benchmark record for a scenario.
*	@param	scenario	Scenario identifier to retrieve.
*	@return	Const pointer to the record, or `nullptr` if the scenario is out of range.
**/
const nav2_bench_record_t *SVG_Nav2_Bench_GetRecord( const nav2_bench_scenario_t scenario ) {
    return Nav2_Bench_GetMutableRecord( scenario );
}

/**
*	@brief	Retrieve the most recent benchmark record that received a completed query.
*	@return	Const pointer to the last updated record, or `nullptr` if no record is available.
**/
const nav2_bench_record_t *SVG_Nav2_Bench_GetLastRecord( void ) {
    return s_nav2_bench_last_record;
}

/**
*	@brief	Return a stable display name for a timing bucket.
*	@param	bucket	Timing bucket to convert.
*	@return	Constant string name used in diagnostics.
**/
const char *SVG_Nav2_Bench_TimingBucketName( const nav2_bench_timing_bucket_t bucket ) {
    switch ( bucket ) {
    case nav2_bench_timing_bucket_t::TotalQuery: return "TotalQuery";
    case nav2_bench_timing_bucket_t::TopologyClassification: return "TopologyClassification";
    case nav2_bench_timing_bucket_t::EndpointSelection: return "EndpointSelection";
    case nav2_bench_timing_bucket_t::CoarseSearch: return "CoarseSearch";
    case nav2_bench_timing_bucket_t::CorridorGeneration: return "CorridorGeneration";
    case nav2_bench_timing_bucket_t::FineRefinement: return "FineRefinement";
    case nav2_bench_timing_bucket_t::MoverLocalRefinement: return "MoverLocalRefinement";
    case nav2_bench_timing_bucket_t::OccupancyQuery: return "OccupancyQuery";
    case nav2_bench_timing_bucket_t::WorkerQueueWait: return "WorkerQueueWait";
    case nav2_bench_timing_bucket_t::WorkerExecutionSlice: return "WorkerExecutionSlice";
    case nav2_bench_timing_bucket_t::MainThreadRevalidationCommit: return "MainThreadRevalidationCommit";
    case nav2_bench_timing_bucket_t::SerializationWrite: return "SerializationWrite";
    case nav2_bench_timing_bucket_t::SerializationRead: return "SerializationRead";
    case nav2_bench_timing_bucket_t::Count: break;
    default: break;
    }
    return "Unknown";
}

/**
*	@brief	Return a stable display name for a benchmark scenario.
*	@param	scenario	Scenario identifier to convert.
*	@return	Constant string name used in diagnostics.
**/
const char *SVG_Nav2_Bench_ScenarioName( const nav2_bench_scenario_t scenario ) {
    switch ( scenario ) {
    case nav2_bench_scenario_t::SameFloorOpen: return "SameFloorOpen";
    case nav2_bench_scenario_t::SameFloorObstructed: return "SameFloorObstructed";
    case nav2_bench_scenario_t::HighZGoal: return "HighZGoal";
    case nav2_bench_scenario_t::Ladder: return "Ladder";
    case nav2_bench_scenario_t::Stair: return "Stair";
    case nav2_bench_scenario_t::Door: return "Door";
    case nav2_bench_scenario_t::FuncPlat: return "FuncPlat";
    case nav2_bench_scenario_t::FuncRotatingRideTraverse: return "FuncRotatingRideTraverse";
    case nav2_bench_scenario_t::PortalChainedRoute: return "PortalChainedRoute";
    case nav2_bench_scenario_t::WrongClusterTemptation: return "WrongClusterTemptation";
    case nav2_bench_scenario_t::ManyAgentSimultaneousQuery: return "ManyAgentSimultaneousQuery";
    case nav2_bench_scenario_t::FatPVSRelevance: return "FatPVSRelevance";
    case nav2_bench_scenario_t::InlineModelHeadnodeLocalizedQuery: return "InlineModelHeadnodeLocalizedQuery";
    case nav2_bench_scenario_t::SaveLoadRoundTripContinuity: return "SaveLoadRoundTripContinuity";
    case nav2_bench_scenario_t::Count: break;
    default: break;
    }
    return "Unknown";
}

/**
*	@brief	Return a stable display name for a benchmark failure taxonomy value.
*	@param	failure	Failure taxonomy identifier to convert.
*	@return	Constant string name used in diagnostics.
**/
const char *SVG_Nav2_Bench_FailureName( const nav2_bench_failure_t failure ) {
    switch ( failure ) {
    case nav2_bench_failure_t::WrongFloorConvergence: return "WrongFloorConvergence";
    case nav2_bench_failure_t::ExcessiveFineAStarExpansion: return "ExcessiveFineAStarExpansion";
    case nav2_bench_failure_t::UnreachableFalseNegative: return "UnreachableFalseNegative";
    case nav2_bench_failure_t::CorridorRefinementMismatch: return "CorridorRefinementMismatch";
    case nav2_bench_failure_t::WrongClusterOrWrongPortalCommitment: return "WrongClusterOrWrongPortalCommitment";
    case nav2_bench_failure_t::MoverBoardingFailure: return "MoverBoardingFailure";
    case nav2_bench_failure_t::MoverRelativePathDrift: return "MoverRelativePathDrift";
    case nav2_bench_failure_t::StaleSnapshotResult: return "StaleSnapshotResult";
    case nav2_bench_failure_t::SchedulerStarvation: return "SchedulerStarvation";
    case nav2_bench_failure_t::UnfairQueryDelay: return "UnfairQueryDelay";
    case nav2_bench_failure_t::SerializationVersionMismatch: return "SerializationVersionMismatch";
    case nav2_bench_failure_t::SerializationRoundTripMismatch: return "SerializationRoundTripMismatch";
    case nav2_bench_failure_t::OwnershipMemoryCleanupMismatch: return "OwnershipMemoryCleanupMismatch";
    case nav2_bench_failure_t::Count: break;
    default: break;
    }
    return "Unknown";
}

/**
*	@brief	Emit an aggregate benchmark summary to the developer console.
*	@param	scenario	Scenario to dump, or `Count` to dump all valid scenarios.
*	@note	This helper is intended for low-frequency developer diagnostics, not per-frame logging.
**/
void SVG_Nav2_Bench_DumpRecord( const nav2_bench_scenario_t scenario ) {
    /**
    *    Local helper that prints one aggregate record in a stable readable format.
    **/
    auto dumpSingleRecord = []( const nav2_bench_record_t &record ) -> void {
        /**
        *    Skip empty records so aggregate dumps stay focused on scenarios that have actually been
        *    exercised by the harness.
        **/
        if ( !record.valid ) {
            return;
        }

        /**
        *    Resolve the display name once so the summary line and bucket dumps stay consistent.
        **/
        const char *scenarioName = record.scenario_label ? record.scenario_label : SVG_Nav2_Bench_ScenarioName( record.scenario );

        // Print the aggregate scenario summary line.
        gi.dprintf( "[Nav2Bench] scenario=%s runs=%u success=%u failure=%u\n",
            scenarioName ? scenarioName : "Unknown",
            record.run_count,
            record.success_count,
            record.failure_count );

        /**
        *    Emit timing buckets that have at least one recorded sample so the summary remains concise.
        **/
        for ( size_t bucketIndex = 0; bucketIndex < ( size_t )nav2_bench_timing_bucket_t::Count; bucketIndex++ ) {
            const nav2_bench_timing_stat_t &stat = record.timing[ bucketIndex ];
            if ( stat.samples == 0 ) {
                continue;
            }

            const double averageMs = stat.samples > 0 ? ( stat.total_ms / ( double )stat.samples ) : 0.0;
            gi.dprintf( "[Nav2Bench]   bucket=%s samples=%u avg_ms=%.3f min_ms=%.3f max_ms=%.3f total_ms=%.3f\n",
                SVG_Nav2_Bench_TimingBucketName( ( nav2_bench_timing_bucket_t )bucketIndex ),
                stat.samples,
                averageMs,
                stat.min_ms,
                stat.max_ms,
                stat.total_ms );
        }

        /**
        *    Emit only observed failure taxonomy counters so developers can quickly identify the
        *    classes of regression that occurred during the recorded runs.
        **/
        for ( size_t failureIndex = 0; failureIndex < ( size_t )nav2_bench_failure_t::Count; failureIndex++ ) {
            const uint32_t failureCount = record.failure_type_counts[ failureIndex ];
            if ( failureCount == 0 ) {
                continue;
            }

            gi.dprintf( "[Nav2Bench]   failure=%s count=%u\n",
                SVG_Nav2_Bench_FailureName( ( nav2_bench_failure_t )failureIndex ),
                failureCount );
        }
    };

    /**
    *    Support aggregate dumps across all scenarios so developers can review a whole benchmark
    *    session without requesting each scenario one by one.
    **/
    if ( scenario == nav2_bench_scenario_t::Count ) {
        for ( const nav2_bench_record_t &record : s_nav2_bench_records ) {
            dumpSingleRecord( record );
        }
        return;
    }

    /**
    *    Dump just the requested scenario when a specific identifier was supplied.
    **/
    const nav2_bench_record_t *record = SVG_Nav2_Bench_GetRecord( scenario );
    if ( !record ) {
        gi.dprintf( "[Nav2Bench] invalid scenario=%d\n", ( int32_t )scenario );
        return;
    }
    dumpSingleRecord( *record );
}
