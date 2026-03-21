/********************************************************************
*
*
*	ServerGame: Nav2 Budget Model - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_budget.h"

#include <algorithm>


/**
*
*
*	Nav2 Budget Public API:
*
*
**/
/**
*	@brief	Initialize a budget policy with conservative scheduler-foundation defaults.
*	@param	policy	[out] Budget policy to initialize.
*	@note	This helper seeds each stage with a small bounded slice so later stages start from
*			a fairness-aware baseline instead of ad hoc zero values.
**/
void SVG_Nav2_Budget_InitPolicy( nav2_budget_policy_t *policy ) {
    /**
    *    Sanity-check the destination policy before attempting to seed any stage budgets.
    **/
    if ( !policy ) {
        // There is no policy object to initialize.
        return;
    }

    /**
    *    Reset the full policy so callers always receive deterministic baseline values.
    **/
    *policy = nav2_budget_policy_t{};

    /**
    *    Seed conservative frame-level limits for the scheduler foundation. These are intentionally
    *    small because no heavy nav2 stages exist yet and fairness matters more than throughput.
    **/
    policy->total_frame_budget_ms = 4.0;
    policy->max_worker_slices_per_frame = 4;
    policy->max_main_thread_slices_per_frame = 4;

    /**
    *    Seed every stage with a default bounded slice so future jobs have deterministic behavior
    *    even before stage-specific tuning is introduced.
    **/
    for ( size_t stageIndex = 0; stageIndex < ( size_t )nav2_query_stage_t::Count; stageIndex++ ) {
        // Start from a conservative baseline slice duration for the stage.
        policy->stage_budget[ stageIndex ].max_slice_ms = 1.0;
        // Start from a modest work-unit budget until real expansion semantics are wired in.
        policy->stage_budget[ stageIndex ].max_expansions = 128;
        // Keep fairness rotation frequent by default.
        policy->stage_budget[ stageIndex ].max_consecutive_slices = 1;
    }

    /**
    *    Apply lighter defaults to bookkeeping-oriented stages that should stay cheap and deterministic.
    **/
    policy->stage_budget[ ( size_t )nav2_query_stage_t::Intake ].max_slice_ms = 0.25;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::SnapshotBind ].max_slice_ms = 0.25;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::Commit ].max_slice_ms = 0.25;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::Completed ].max_slice_ms = 0.0;

    /**
    *    Allow the search-heavy stages a slightly larger default slice while still remaining bounded.
    **/
    policy->stage_budget[ ( size_t )nav2_query_stage_t::CoarseSearch ].max_slice_ms = 1.5;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::FineRefinement ].max_slice_ms = 1.5;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::MoverLocalRefinement ].max_slice_ms = 1.5;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::CoarseSearch ].max_expansions = 256;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::FineRefinement ].max_expansions = 256;
    policy->stage_budget[ ( size_t )nav2_query_stage_t::MoverLocalRefinement ].max_expansions = 256;
}

/**
*	@brief	Reset frame-level runtime accounting for a new scheduler frame.
*	@param	runtime		[out] Runtime state to reset.
*	@param	frameNumber	Frame number being entered.
*	@param	policy		Optional policy override copied into the runtime.
**/
void SVG_Nav2_Budget_ResetFrame( nav2_budget_runtime_t *runtime, const int64_t frameNumber, const nav2_budget_policy_t *policy ) {
    /**
    *    Sanity-check the runtime state before resetting the frame counters.
    **/
    if ( !runtime ) {
        // There is no runtime accounting state to reset.
        return;
    }

    /**
    *    Reset the runtime first so stale per-frame counters cannot leak into the next frame.
    **/
    *runtime = nav2_budget_runtime_t{};
    // Store the frame number being accounted.
    runtime->frame_number = frameNumber;

    /**
    *    Copy the caller-supplied policy when provided, otherwise seed conservative defaults.
    **/
    if ( policy ) {
        // Copy the external budget policy snapshot into the runtime.
        runtime->policy = *policy;
    } else {
        // Seed a conservative default policy for this runtime frame.
        SVG_Nav2_Budget_InitPolicy( &runtime->policy );
    }
}

/**
*	@brief	Return whether the frame-level budget still allows additional work to be issued.
*	@param	runtime	Runtime state to inspect.
*	@return	True when additional slices may still be granted this frame.
**/
const bool SVG_Nav2_Budget_HasFrameCapacity( const nav2_budget_runtime_t *runtime ) {
    /**
    *    Reject missing runtime state immediately so callers never dereference a nullptr.
    **/
    if ( !runtime ) {
        return false;
    }

    /**
    *    Stop issuing slices once the total frame budget has already been exhausted.
    **/
    if ( runtime->consumed_frame_budget_ms >= runtime->policy.total_frame_budget_ms ) {
        return false;
    }

    /**
    *    Require at least one remaining worker or main-thread slot so the scheduler cannot grant
    *    unbounded slices even if time budget remains.
    **/
    const bool hasWorkerCapacity = runtime->worker_slices_issued < runtime->policy.max_worker_slices_per_frame;
    const bool hasMainThreadCapacity = runtime->main_thread_slices_issued < runtime->policy.max_main_thread_slices_per_frame;
    return hasWorkerCapacity || hasMainThreadCapacity;
}

/**
*	@brief	Grant a new slice for a query stage from the current frame budget.
*	@param	runtime		Runtime accounting state to mutate.
*	@param	stage		Stage requesting work.
*	@param	priority	Priority class of the requesting job.
*	@param	workerSlice	True when the slice will execute on a worker path.
*	@param	outSlice	[out] Granted slice description on success.
*	@return	True when the scheduler successfully granted a slice.
*	@note	The current implementation is intentionally conservative and deterministic; later work
*			can refine the grant policy without changing the public contract.
**/
const bool SVG_Nav2_Budget_GrantSlice( nav2_budget_runtime_t *runtime, const nav2_query_stage_t stage,
    const nav2_query_priority_t priority, const bool workerSlice, nav2_budget_slice_t *outSlice ) {
    /**
    *    Validate inputs before reading frame counters or mutating the destination slice.
    **/
    if ( !runtime || !outSlice ) {
        return false;
    }

    /**
    *    Validate the stage identifier before indexing the per-stage budget table.
    **/
    const size_t stageIndex = ( size_t )stage;
    if ( stageIndex >= ( size_t )nav2_query_stage_t::Count ) {
        return false;
    }

    /**
    *    Require remaining frame capacity before issuing any additional slice.
    **/
    if ( !SVG_Nav2_Budget_HasFrameCapacity( runtime ) ) {
        return false;
    }

    /**
    *    Enforce worker versus main-thread slice caps separately so one execution domain cannot
    *    monopolize the frame.
    **/
    if ( workerSlice ) {
        if ( runtime->worker_slices_issued >= runtime->policy.max_worker_slices_per_frame ) {
            return false;
        }
    } else {
        if ( runtime->main_thread_slices_issued >= runtime->policy.max_main_thread_slices_per_frame ) {
            return false;
        }
    }

    /**
    *    Derive the granted slice from the stage policy and remaining frame budget.
    **/
    const nav2_stage_budget_t &stageBudget = runtime->policy.stage_budget[ stageIndex ];
    const double remainingFrameBudgetMs = std::max( 0.0, runtime->policy.total_frame_budget_ms - runtime->consumed_frame_budget_ms );
    const double grantedMs = std::min( stageBudget.max_slice_ms, remainingFrameBudgetMs );
    if ( grantedMs <= 0.0 ) {
        return false;
    }

    /**
    *    Populate the destination slice after all checks pass so callers only ever observe complete
    *    successful grants.
    **/
    *outSlice = nav2_budget_slice_t{};
    outSlice->frame_number = runtime->frame_number;
    outSlice->stage = stage;
    outSlice->priority = priority;
    outSlice->granted_ms = grantedMs;
    outSlice->granted_expansions = stageBudget.max_expansions;
    outSlice->was_throttled = grantedMs < stageBudget.max_slice_ms;

    /**
    *    Reserve the execution-domain slot immediately so later grant attempts observe the same
    *    deterministic accounting within the frame.
    **/
    if ( workerSlice ) {
        runtime->worker_slices_issued++;
    } else {
        runtime->main_thread_slices_issued++;
    }

    return true;
}

/**
*	@brief	Commit actual work consumption back into frame-level budget accounting.
*	@param	runtime	Runtime accounting state to update.
*	@param	slice	Granted slice being reported.
*	@param	consumedMs	Actual milliseconds consumed.
**/
void SVG_Nav2_Budget_CommitSliceConsumption( nav2_budget_runtime_t *runtime, const nav2_budget_slice_t &slice, const double consumedMs ) {
    /**
    *    Ignore commits without runtime accounting state.
    **/
    if ( !runtime ) {
        return;
    }

    /**
    *    Clamp consumption to a sensible non-negative range and never allow it to exceed the granted
    *    slice time, which keeps frame accounting deterministic and bounded.
    **/
    const double sanitizedConsumedMs = std::max( 0.0, consumedMs );
    const double clampedConsumedMs = std::min( sanitizedConsumedMs, slice.granted_ms );
    // Accumulate the actual consumed milliseconds into the frame total.
    runtime->consumed_frame_budget_ms += clampedConsumedMs;
}

/**
*	@brief	Return a stable display name for a query stage.
*	@param	stage	Stage identifier to convert.
*	@return	Constant string name used by diagnostics and benchmark hooks.
**/
const char *SVG_Nav2_Budget_StageName( const nav2_query_stage_t stage ) {
    switch ( stage ) {
    case nav2_query_stage_t::Intake: return "Intake";
    case nav2_query_stage_t::SnapshotBind: return "SnapshotBind";
    case nav2_query_stage_t::TrivialDirectCheck: return "TrivialDirectCheck";
    case nav2_query_stage_t::TopologyClassification: return "TopologyClassification";
    case nav2_query_stage_t::MoverClassification: return "MoverClassification";
    case nav2_query_stage_t::CandidateGeneration: return "CandidateGeneration";
    case nav2_query_stage_t::CandidateRanking: return "CandidateRanking";
    case nav2_query_stage_t::CoarseSearch: return "CoarseSearch";
    case nav2_query_stage_t::CorridorExtraction: return "CorridorExtraction";
    case nav2_query_stage_t::FineRefinement: return "FineRefinement";
    case nav2_query_stage_t::MoverLocalRefinement: return "MoverLocalRefinement";
    case nav2_query_stage_t::PostProcess: return "PostProcess";
    case nav2_query_stage_t::Revalidation: return "Revalidation";
    case nav2_query_stage_t::Commit: return "Commit";
    case nav2_query_stage_t::Completed: return "Completed";
    case nav2_query_stage_t::Count: break;
    default: break;
    }
    return "Unknown";
}

/**
*	@brief	Return a stable display name for a scheduler priority class.
*	@param	priority	Priority identifier to convert.
*	@return	Constant string name used by diagnostics and benchmark hooks.
**/
const char *SVG_Nav2_Budget_PriorityName( const nav2_query_priority_t priority ) {
    switch ( priority ) {
    case nav2_query_priority_t::Low: return "Low";
    case nav2_query_priority_t::Normal: return "Normal";
    case nav2_query_priority_t::High: return "High";
    case nav2_query_priority_t::Critical: return "Critical";
    case nav2_query_priority_t::Count: break;
    default: break;
    }
    return "Unknown";
}
