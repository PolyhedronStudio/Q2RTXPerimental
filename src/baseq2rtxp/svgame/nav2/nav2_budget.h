/********************************************************************
*
*
*	ServerGame: Nav2 Budget Model
*
*
********************************************************************/
#pragma once


/**
*
*
*	Nav2 Budget Enumerations:
*
*
**/
/**
*	@brief	Priority classes used by the nav2 scheduler foundation.
*	@note	These values are intentionally compact and stable so they can be stored in future
*			job snapshots, diagnostics, and save-friendly request descriptors.
**/
enum class nav2_query_priority_t : uint8_t {
    Low = 0,
    Normal,
    High,
    Critical,
    Count
};

/**
*	@brief	Individual scheduler stages that may consume bounded work slices.
*	@note	The explicit ordering mirrors the long-term query pipeline from the master plan so
*			later solver stages can be wired into the scheduler without renumbering identifiers.
**/
enum class nav2_query_stage_t : uint8_t {
    Intake = 0,
    SnapshotBind,
    TrivialDirectCheck,
    TopologyClassification,
    MoverClassification,
    CandidateGeneration,
    CandidateRanking,
    CoarseSearch,
    CorridorExtraction,
    FineRefinement,
    MoverLocalRefinement,
    PostProcess,
    Revalidation,
    Commit,
    Completed,
    Count
};


/**
*
*
*	Nav2 Budget Data Structures:
*
*
**/
/**
*	@brief	Per-stage budget policy shared by query jobs and scheduler runtime code.
*	@note	All limits are soft scheduler-facing values; solver stages may still finish earlier
*			or request another slice if they need more time.
**/
struct nav2_stage_budget_t {
    //! Maximum wall-clock milliseconds a single slice may spend in this stage.
    double max_slice_ms = 0.0;
    //! Maximum node or work-unit expansions a single slice may consume in this stage.
    uint32_t max_expansions = 0;
    //! Maximum number of consecutive slices the scheduler should allow before considering fairness rotation.
    uint32_t max_consecutive_slices = 1;
};

/**
*	@brief	Per-query slice description granted by the scheduler.
*	@note	This is designed to be copied into worker-safe job state without raw pointer ownership.
**/
struct nav2_budget_slice_t {
    //! Frame number that produced this slice.
    int64_t frame_number = 0;
    //! Stage this slice applies to.
    nav2_query_stage_t stage = nav2_query_stage_t::Intake;
    //! Priority level that influenced the granted slice.
    nav2_query_priority_t priority = nav2_query_priority_t::Normal;
    //! Maximum wall-clock milliseconds granted to the slice.
    double granted_ms = 0.0;
    //! Maximum node or work-unit expansions granted to the slice.
    uint32_t granted_expansions = 0;
    //! Whether the slice was intentionally trimmed due to overload or fairness pressure.
    bool was_throttled = false;
};

/**
*	@brief	Aggregate frame-level budget policy for the nav2 scheduler foundation.
*	@note	This struct intentionally carries both global limits and per-stage defaults so the
*			scheduler can remain data-driven while the actual solvers are still being built.
**/
struct nav2_budget_policy_t {
    //! Total navigation budget available per frame across all jobs.
    double total_frame_budget_ms = 4.0;
    //! Maximum number of worker-directed slices the scheduler may issue in one frame.
    uint32_t max_worker_slices_per_frame = 4;
    //! Maximum number of main-thread slices the scheduler may issue in one frame.
    uint32_t max_main_thread_slices_per_frame = 4;
    //! Default stage budgets indexed by `nav2_query_stage_t`.
    nav2_stage_budget_t stage_budget[ ( size_t )nav2_query_stage_t::Count ] = {};
};

/**
*	@brief	Mutable budget-accounting state for one scheduler frame.
*	@note	The scheduler owns this state and resets it once per frame.
**/
struct nav2_budget_runtime_t {
    //! Frame number currently being accounted.
    int64_t frame_number = 0;
    //! Policy snapshot used to derive slices for the frame.
    nav2_budget_policy_t policy = {};
    //! Milliseconds already consumed by issued slices this frame.
    double consumed_frame_budget_ms = 0.0;
    //! Number of worker slices already granted this frame.
    uint32_t worker_slices_issued = 0;
    //! Number of main-thread slices already granted this frame.
    uint32_t main_thread_slices_issued = 0;
};


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
void SVG_Nav2_Budget_InitPolicy( nav2_budget_policy_t *policy );

/**
*	@brief	Reset frame-level runtime accounting for a new scheduler frame.
*	@param	runtime		[out] Runtime state to reset.
*	@param	frameNumber	Frame number being entered.
*	@param	policy		Optional policy override copied into the runtime.
**/
void SVG_Nav2_Budget_ResetFrame( nav2_budget_runtime_t *runtime, const int64_t frameNumber, const nav2_budget_policy_t *policy = nullptr );

/**
*	@brief	Return whether the frame-level budget still allows additional work to be issued.
*	@param	runtime	Runtime state to inspect.
*	@return	True when additional slices may still be granted this frame.
**/
const bool SVG_Nav2_Budget_HasFrameCapacity( const nav2_budget_runtime_t *runtime );

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
    const nav2_query_priority_t priority, const bool workerSlice, nav2_budget_slice_t *outSlice );

/**
*	@brief	Commit actual work consumption back into frame-level budget accounting.
*	@param	runtime	Runtime accounting state to update.
*	@param	slice	Granted slice being reported.
*	@param	consumedMs	Actual milliseconds consumed.
**/
void SVG_Nav2_Budget_CommitSliceConsumption( nav2_budget_runtime_t *runtime, const nav2_budget_slice_t &slice, const double consumedMs );

/**
*	@brief	Return a stable display name for a query stage.
*	@param	stage	Stage identifier to convert.
*	@return	Constant string name used by diagnostics and benchmark hooks.
**/
const char *SVG_Nav2_Budget_StageName( const nav2_query_stage_t stage );

/**
*	@brief	Return a stable display name for a scheduler priority class.
*	@param	priority	Priority identifier to convert.
*	@return	Constant string name used by diagnostics and benchmark hooks.
**/
const char *SVG_Nav2_Budget_PriorityName( const nav2_query_priority_t priority );
