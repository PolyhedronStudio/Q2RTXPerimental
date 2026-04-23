/********************************************************************
*
*
*	ServerGame: Nav2 Snapshot Model
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_budget.h"
#include "svgame/svg_local.h"


/**
*
*
*	Nav2 Snapshot Data Structures:
*
*
**/
/**
*	@brief	Worker-safe version tuple bound to one nav2 query.
*	@note	This is the minimal save-friendly identity a job needs so completion-time revalidation
*			can decide whether the consumed static and dynamic views are still current enough to use.
**/
struct nav2_snapshot_handle_t {
    //! Static nav publication version used by the query.
    uint32_t static_nav_version = 0;
    //! Dynamic occupancy overlay version used by the query.
    uint32_t occupancy_version = 0;
    //! Mover-transform publication version used by the query.
    uint32_t mover_version = 0;
    //! Dynamic connector overlay version used by the query.
    uint32_t connector_version = 0;
    //! Model/headnode publication version used by the query.
    uint32_t model_version = 0;
};

/**
*	@brief	Mutable snapshot publication counters owned by the main thread.
*	@note	Future nav2 publishers can increment only the domains they actually mutate, which keeps
*			revalidation precise and avoids unnecessary query restarts.
**/
struct nav2_snapshot_runtime_t {
    //! Latest published worker-safe snapshot handle.
    nav2_snapshot_handle_t current = {};
    //! Snapshot comparison policy used to classify stale results.
    struct nav2_snapshot_policy_t {
        //! Maximum tolerated occupancy-version drift before escalation from revalidate-only to stage restart.
        uint32_t occupancy_revalidate_version_grace = 1;
        //! Maximum tolerated mover-version drift before escalation from stage restart to full resubmit.
        uint32_t mover_restart_version_grace = 2;
        //! Maximum tolerated connector-version drift before escalation from stage restart to full resubmit.
        uint32_t connector_restart_version_grace = 2;
        //! Stage to restart from when occupancy-only drift exceeds grace.
        nav2_query_stage_t occupancy_restart_stage = nav2_query_stage_t::FineRefinement;
        //! Stage to restart from when mover drift exceeds revalidate-only tolerance.
        nav2_query_stage_t mover_restart_stage = nav2_query_stage_t::MoverLocalRefinement;
        //! Stage to restart from when connector drift exceeds revalidate-only tolerance.
        nav2_query_stage_t connector_restart_stage = nav2_query_stage_t::CoarseSearch;
    } policy = {};
};

/**
*\t@brief\tStable change-domain bit flags emitted by snapshot comparison.
**/
enum nav2_snapshot_change_flag_t : uint32_t {
    NAV2_SNAPSHOT_CHANGE_FLAG_NONE = 0,
    NAV2_SNAPSHOT_CHANGE_FLAG_STATIC_NAV = ( 1u << 0 ),
    NAV2_SNAPSHOT_CHANGE_FLAG_OCCUPANCY = ( 1u << 1 ),
    NAV2_SNAPSHOT_CHANGE_FLAG_MOVER = ( 1u << 2 ),
    NAV2_SNAPSHOT_CHANGE_FLAG_CONNECTOR = ( 1u << 3 ),
    NAV2_SNAPSHOT_CHANGE_FLAG_MODEL = ( 1u << 4 )
};

/**
*\t@brief\tScheduler action selected by snapshot staleness comparison.
**/
enum class nav2_snapshot_stale_action_t : uint8_t {
    Accept = 0,
    RevalidateOnly,
    RestartStage,
    Resubmit
};

/**
*	@brief	Staleness decision returned when comparing a consumed query snapshot against the latest runtime versions.
*	@note	The scheduler uses this decision to choose between accept, revalidate, stage restart, or full resubmit.
**/
struct nav2_snapshot_staleness_t {
    //! True when all compared versions still match exactly.
    bool is_current = true;
    //! True when the result may still be usable but should undergo main-thread revalidation.
    bool requires_revalidation = false;
    //! True when the query should restart from a stage boundary instead of being accepted directly.
    bool requires_restart = false;
    //! True when the query should be canceled and fully resubmitted.
    bool requires_resubmit = false;
    //! Domain-local mismatch flags kept explicit for debugging and future policy tuning.
    bool static_nav_changed = false;
    bool occupancy_changed = false;
    bool mover_changed = false;
    bool connector_changed = false;
    bool model_changed = false;
    //! Stable change-domain bitmask for diagnostics and scheduler policy branching.
    uint32_t change_mask = NAV2_SNAPSHOT_CHANGE_FLAG_NONE;
    //! Chosen scheduler action for this stale snapshot.
    nav2_snapshot_stale_action_t action = nav2_snapshot_stale_action_t::Accept;
    //! Stage-boundary restart target when `action == RestartStage`.
    nav2_query_stage_t restart_stage = nav2_query_stage_t::TopologyClassification;
    //! True when scheduler may continue via partial repair/restart instead of full resubmit.
    bool allows_partial_repair = false;
    //! Version delta between current and consumed occupancy versions.
    uint32_t occupancy_version_delta = 0;
    //! Version delta between current and consumed mover versions.
    uint32_t mover_version_delta = 0;
    //! Version delta between current and consumed connector versions.
    uint32_t connector_version_delta = 0;
};


/**
*
*
*	Nav2 Snapshot Public API:
*
*
**/
/**
*	@brief	Reset the snapshot runtime to an initial zero-version state.
*	@param	runtime	[out] Snapshot runtime to reset.
**/
void SVG_Nav2_Snapshot_ResetRuntime( nav2_snapshot_runtime_t *runtime );

/**
*	@brief	Return the latest published snapshot handle.
*	@param	runtime	Snapshot runtime to inspect.
*	@return	Copy of the current snapshot handle, or a zeroed handle when runtime is unavailable.
**/
nav2_snapshot_handle_t SVG_Nav2_Snapshot_GetCurrentHandle( const nav2_snapshot_runtime_t *runtime );

/**
*	@brief	Publish a new static-nav version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated static-nav version.
**/
uint32_t SVG_Nav2_Snapshot_BumpStaticNavVersion( nav2_snapshot_runtime_t *runtime );

/**
*	@brief	Publish a new occupancy-overlay version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated occupancy version.
**/
uint32_t SVG_Nav2_Snapshot_BumpOccupancyVersion( nav2_snapshot_runtime_t *runtime );

/**
*	@brief	Publish a new mover-transform version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated mover version.
**/
uint32_t SVG_Nav2_Snapshot_BumpMoverVersion( nav2_snapshot_runtime_t *runtime );

/**
*	@brief	Publish a new dynamic-connector version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated connector version.
**/
uint32_t SVG_Nav2_Snapshot_BumpConnectorVersion( nav2_snapshot_runtime_t *runtime );

/**
*	@brief	Publish a new model/headnode metadata version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated model version.
**/
uint32_t SVG_Nav2_Snapshot_BumpModelVersion( nav2_snapshot_runtime_t *runtime );

/**
*	@brief	Compare a consumed query snapshot against the current runtime versions.
*	@param	runtime	Snapshot runtime providing the latest published versions.
*	@param	consumed	Snapshot handle used by the query that is finishing or resuming.
*	@return	Structured staleness decision that later scheduler code can map onto restart policy.
*	@note	The current policy is intentionally conservative: static-nav or model changes force resubmit,
*			while dynamic overlay changes request revalidation and stage-aware restart.
**/
nav2_snapshot_staleness_t SVG_Nav2_Snapshot_Compare( const nav2_snapshot_runtime_t *runtime, const nav2_snapshot_handle_t &consumed );
