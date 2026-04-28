/********************************************************************
*
*
*	ServerGame: Nav2 Scheduler Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_scheduler.h"
#include "svgame/nav2/nav2_query_iface_internal.h"

#include "svgame/nav2/nav2_coarse_astar.h"
#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_bench.h"
#include "svgame/nav2/nav2_corridor_build.h"
#include "svgame/nav2/nav2_fine_astar.h"
#include "svgame/nav2/nav2_goal_candidates.h"
#include "svgame/nav2/nav2_hierarchy_graph.h"
#include "svgame/nav2/nav2_mover_local_nav.h"
#include "svgame/nav2/nav2_postprocess.h"
#include "svgame/nav2/nav2_region_layers.h"
#include "svgame/nav2/nav2_snapshot.h"
#include "svgame/nav2/nav2_span_grid_build.h"
#include "svgame/nav2/nav2_topology.h"
#include "svgame/nav2/nav2_worker_iface.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>


/**
*
*
*	Nav2 Scheduler Global State:
*
*
**/
//! RAII owner for the scheduler foundation runtime.
static nav2_scheduler_runtime_owner_t s_nav2_scheduler_runtime = {};

/**
*\t@brief\tReturn whether verbose scheduler diagnostics are enabled.
*\t@return\tTrue when async-nav stats logging cvar is enabled.
**/
static const bool Nav2_Scheduler_IsVerboseStatsEnabled( void ) {
    static cvar_t *s_nav2_scheduler_log_stats = nullptr;
    if ( !s_nav2_scheduler_log_stats ) {
        s_nav2_scheduler_log_stats = gi.cvar( "nav_nav_async_log_stats", "0", 0 );
    }
    return s_nav2_scheduler_log_stats && s_nav2_scheduler_log_stats->integer != 0;
}

/**
*	@brief	Emit a bounded diagnostic for one scheduler-owned nav2 job.
*	@param	job	Job being reported.
*	@param	eventLabel	Short label describing the scheduler checkpoint.
**/
static void Nav2_Scheduler_DebugLogJobEvent( const nav2_query_job_t *job, const char *eventLabel );


/**
*
*
*	Nav2 Scheduler Stage Runtime Structures:
*
*
**/
/**
*	@brief	Scheduler-local stage artifacts owned per job id for Task 11.1 staged execution.
*	@note	This keeps stage payload ownership out of `nav2_query_job_t` so scheduler wiring can evolve
*			without introducing include cycles across nav2 stage modules.
**/
struct nav2_scheduler_job_stage_runtime_t {
    //! Candidate endpoints built for the query start point.
    nav2_goal_candidate_list_t start_candidates = {};
    //! Candidate endpoints built for the query goal point.
    nav2_goal_candidate_list_t goal_candidates = {};
    //! Selected start candidate used by staged coarse search.
    nav2_goal_candidate_t selected_start_candidate = {};
    //! Selected goal candidate used by staged coarse search.
    nav2_goal_candidate_t selected_goal_candidate = {};
    //! True when the start candidate has been selected.
    bool has_selected_start_candidate = false;
    //! True when the goal candidate has been selected.
    bool has_selected_goal_candidate = false;
    //! Snapshot-scoped topology artifact bound for this job when available.
    nav2_topology_artifact_t topology_artifact = {};
    //! Bounded topology summary bound for this job when available.
    nav2_topology_summary_t topology_summary = {};
    //! True when the job has a valid topology artifact bound.
    bool has_topology_artifact = false;
    //! Span-grid snapshot used to build region and hierarchy stages.
    nav2_span_grid_t span_grid = {};
    //! Extracted connectors used for hierarchy and corridor stages.
    nav2_connector_list_t connectors = {};
    //! Region-layer graph feeding hierarchy build.
    nav2_region_layer_graph_t region_layers = {};
    //! Per-job hierarchy graph used by coarse-search stage execution.
    nav2_hierarchy_graph_t hierarchy_graph = {};
    //! Resumable coarse-search stage state.
    nav2_coarse_astar_state_t coarse_state = {};
    //! Last coarse-search result snapshot.
    nav2_coarse_astar_result_t coarse_result = {};
    //! Extracted corridor used by fine-search stage execution.
    nav2_corridor_t corridor = {};
    //! Resumable fine-search stage state.
    nav2_fine_astar_state_t fine_state = {};
    //! Last fine-search result snapshot.
    nav2_fine_astar_result_t fine_result = {};
    //! Latest mover-local transform used for mover refinement.
    nav2_mover_transform_t mover_transform = {};
    //! Latest mover-local refinement result.
    nav2_mover_local_refine_result_t mover_refine_result = {};
    //! Latest postprocess output for this query.
    nav2_postprocess_result_t postprocess_result = {};
    //! Agent-aware adjacency policy mirrored from the active request when available.
    nav2_span_adjacency_policy_t span_adjacency_policy = {};
    //! Request-local query policy mirrored from the active request when available.
    nav2_query_policy_t query_policy = {};
};

//! Scheduler-local per-job stage runtime cache for Task 11.1 staged execution.
static std::unordered_map<uint64_t, nav2_scheduler_job_stage_runtime_t> s_nav2_scheduler_job_stage_runtime = {};

/**
*	@brief	Invalidate cached topology and hierarchy publications when static-nav state changes.
*	@param	runtime	[in,out] Scheduler runtime owning the cached artifacts.
**/
static void Nav2_Scheduler_ClearCachedTopologyAndHierarchy( nav2_scheduler_runtime_t *runtime ) {
    /**
    *	Require runtime storage before clearing cached publications.
    **/
    if ( !runtime ) {
        return;
    }

    /**
    *	Clear cached topology publication first so later hierarchy stages cannot accidentally mix
    *	new hierarchy dependencies with stale topology ownership.
    **/
    runtime->cached_topology_artifact = {};
    runtime->cached_topology_summary = {};
    runtime->has_cached_topology_artifact = false;

    /**
    *	Clear hierarchy dependency caches that are version-coupled to the same static-nav publication.
    **/
    runtime->cached_hierarchy_span_grid = {};
    runtime->cached_hierarchy_connectors = {};
    runtime->cached_hierarchy_region_layers = {};
    runtime->cached_hierarchy_graph = {};
    runtime->cached_hierarchy_static_nav_version = 0;
    runtime->has_cached_hierarchy_dependencies = false;
    runtime->cached_hierarchy_is_no_connector_fallback = false;
}

/**
*	@brief	Bind the current cached or freshly built topology publication into one job-stage runtime.
*	@param	stageRuntime	[in,out] Job-local stage runtime receiving the bound topology publication.
*	@return	True when a valid topology artifact was bound.
**/
static const bool Nav2_Scheduler_BindTopologyArtifact( nav2_scheduler_job_stage_runtime_t *stageRuntime ) {
    /**
    *	Require job-local stage runtime and scheduler runtime storage before topology publication.
    **/
    if ( !stageRuntime || !s_nav2_scheduler_runtime ) {
        return false;
    }

    nav2_scheduler_runtime_t *runtime = s_nav2_scheduler_runtime.get();
    const uint32_t currentStaticNavVersion = runtime->snapshot_runtime.current.static_nav_version;

    /**
    *	Invalidate cached publications when topology version no longer matches the active snapshot.
    **/
    if ( runtime->has_cached_topology_artifact
        && runtime->cached_topology_artifact.static_nav_version != currentStaticNavVersion )
    {
        Nav2_Scheduler_ClearCachedTopologyAndHierarchy( runtime );
    }

    /**
    *	Reuse cached topology publication when still valid for the active static-nav snapshot.
    **/
    if ( runtime->has_cached_topology_artifact && SVG_Nav2_Topology_ValidateArtifact( runtime->cached_topology_artifact ) ) {
        stageRuntime->topology_artifact = runtime->cached_topology_artifact;
        stageRuntime->topology_summary = runtime->cached_topology_summary;
        stageRuntime->has_topology_artifact = true;
        return true;
    }

    /**
    *	Build and cache a fresh topology publication from the currently published runtime state.
    **/
    nav2_topology_artifact_t builtArtifact = {};
    nav2_topology_summary_t builtSummary = {};
    if ( !SVG_Nav2_Topology_BuildArtifact( &builtArtifact, &builtSummary ) || !SVG_Nav2_Topology_ValidateArtifact( builtArtifact ) ) {
        stageRuntime->topology_artifact = {};
        stageRuntime->topology_summary = {};
        stageRuntime->has_topology_artifact = false;
        return false;
    }

    runtime->cached_topology_artifact = builtArtifact;
    runtime->cached_topology_summary = builtSummary;
    runtime->has_cached_topology_artifact = true;
    stageRuntime->topology_artifact = builtArtifact;
    stageRuntime->topology_summary = builtSummary;
    stageRuntime->has_topology_artifact = true;
    return true;
}

/**
*	@brief	Execute the topology-classification stage for one scheduler-owned job.
*	@param	job		[in,out] Scheduler-owned job receiving stage-local state updates.
*	@param	stageRuntime	[in,out] Job-local stage runtime receiving the bound topology publication.
*	@return	True when topology classification completed successfully.
*	@note	This stage currently binds the scheduler-owned topology artifact publication and records
*			bounded milestone diagnostics without yet classifying per-query endpoint-local location tables.
**/
static const bool Nav2_Scheduler_ExecuteTopologyClassificationStage( nav2_query_job_t *job,
    nav2_scheduler_job_stage_runtime_t *stageRuntime )
{
    /**
    *	Require both scheduler job and job-local stage runtime before topology classification begins.
    **/
    if ( !job || !stageRuntime ) {
        return false;
    }

    /**
    *	Bind the current topology publication as the owned output of this stage.
    **/
    if ( !Nav2_Scheduler_BindTopologyArtifact( stageRuntime ) || !stageRuntime->has_topology_artifact ) {
        return false;
    }

    /**
    *	Record bounded progress from the topology publication so later diagnostics can distinguish
    *	topology-stage completion from downstream candidate generation.
    **/
    job->state.progress.candidate_count = 0;
    job->state.result_status = nav2_query_result_status_t::Partial;
    job->state.has_provisional_result = true;
    Nav2_Scheduler_DebugLogJobEvent( job, "TopologyClassification" );
    return true;
}

/**
*	@brief	Return whether two requests are close enough to be treated as duplicates.
*	@param	existing	Existing scheduler-owned request.
*	@param	incoming	Incoming request being submitted.
*	@return	True when incoming work can reuse existing in-flight or queued work.
**/
static const bool Nav2_Scheduler_IsDuplicateRequest( const nav2_query_request_t &existing, const nav2_query_request_t &incoming ) {
    /**
    *    Require stable same-agent identity before attempting geometric dedupe checks.
    **/
    if ( existing.agent_entnum <= 0 || incoming.agent_entnum <= 0 || existing.agent_entnum != incoming.agent_entnum ) {
        return false;
    }

    /**
    *    Respect explicit goal-owner identity when both requests provide one.
    **/
    if ( existing.goal_entnum > 0 && incoming.goal_entnum > 0 && existing.goal_entnum != incoming.goal_entnum ) {
        return false;
    }

    /**
    *    Use bounded spatial tolerances so tiny sound-origin jitter does not flood the queue.
    **/
    const double startDx = ( double )incoming.start_origin.x - ( double )existing.start_origin.x;
    const double startDy = ( double )incoming.start_origin.y - ( double )existing.start_origin.y;
    const double startDz = std::fabs( ( double )incoming.start_origin.z - ( double )existing.start_origin.z );
    const double goalDx = ( double )incoming.goal_origin.x - ( double )existing.goal_origin.x;
    const double goalDy = ( double )incoming.goal_origin.y - ( double )existing.goal_origin.y;
    const double goalDz = std::fabs( ( double )incoming.goal_origin.z - ( double )existing.goal_origin.z );
    const double startDist2D = std::sqrt( ( startDx * startDx ) + ( startDy * startDy ) );
    const double goalDist2D = std::sqrt( ( goalDx * goalDx ) + ( goalDy * goalDy ) );

    // Keep dedupe conservative to avoid suppressing legitimately new path queries.
    constexpr double dedupeStartDist2D = 48.0;
    constexpr double dedupeStartDistZ = 64.0;
    constexpr double dedupeGoalDist2D = 80.0;
    constexpr double dedupeGoalDistZ = 96.0;
    if ( startDist2D > dedupeStartDist2D || startDz > dedupeStartDistZ ) {
        return false;
    }
    if ( goalDist2D > dedupeGoalDist2D || goalDz > dedupeGoalDistZ ) {
        return false;
    }

    return true;
}

/**
*	@brief	Build a lightweight no-connector hierarchy fallback from selected endpoints.
*	@param	stageRuntime	[in,out] Scheduler-local stage runtime storage.
*	@return	True when the lightweight hierarchy fallback was produced.
*	@note	This intentionally avoids expensive full region-layer decomposition on connector-less maps
*			where we only need a bounded coarse bridge between selected start/goal endpoints.
**/
static const bool Nav2_Scheduler_BuildNoConnectorFallbackHierarchy( nav2_scheduler_job_stage_runtime_t *stageRuntime ) {
    /**
    *    Require stage runtime plus selected endpoint candidates before building fallback hierarchy.
    **/
    if ( !stageRuntime || !stageRuntime->has_selected_start_candidate || !stageRuntime->has_selected_goal_candidate ) {
        return false;
    }

    /**
    *    Reset hierarchy payloads so fallback publication is deterministic.
    **/
    stageRuntime->region_layers = {};
    stageRuntime->hierarchy_graph = {};

    /**
    *    Build one hierarchy node for each selected endpoint.
    **/
    const nav2_goal_candidate_t &startCandidate = stageRuntime->selected_start_candidate;
    const nav2_goal_candidate_t &goalCandidate = stageRuntime->selected_goal_candidate;

    nav2_hierarchy_node_t startNode = {};
    startNode.node_id = 1;
    startNode.kind = nav2_hierarchy_node_kind_t::RegionLayer;
    startNode.region_layer_id = startCandidate.layer_index;
    startNode.connector_id = startCandidate.tile_id;
    startNode.topology.leaf_index = startCandidate.leaf_index;
    startNode.topology.cluster_id = startCandidate.cluster_id;
    startNode.topology.area_id = startCandidate.area_id;
    startNode.topology.region_id = startCandidate.layer_index;
    startNode.topology.portal_id = startCandidate.tile_id;
    startNode.topology.tile_id = startCandidate.tile_id;
    startNode.topology.tile_x = ( int32_t )startCandidate.tile_x;
    startNode.topology.tile_y = ( int32_t )startCandidate.tile_y;
    startNode.topology.cell_index = startCandidate.cell_index;
    startNode.topology.layer_index = startCandidate.layer_index;
    startNode.allowed_z_band.min_z = startCandidate.center_origin.z - 32.0;
    startNode.allowed_z_band.max_z = startCandidate.center_origin.z + 32.0;
    startNode.allowed_z_band.preferred_z = startCandidate.center_origin.z;
    startNode.flags = NAV2_HIERARCHY_NODE_FLAG_HAS_ALLOWED_Z_BAND;
    if ( startCandidate.leaf_index >= 0 ) {
        startNode.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_LEAF;
    }
    if ( startCandidate.cluster_id >= 0 ) {
        startNode.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_CLUSTER;
    }
    if ( startCandidate.area_id >= 0 ) {
        startNode.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_AREA;
    }

    nav2_hierarchy_node_t goalNode = {};
    goalNode.node_id = 2;
    goalNode.kind = nav2_hierarchy_node_kind_t::RegionLayer;
    goalNode.region_layer_id = goalCandidate.layer_index;
    goalNode.connector_id = goalCandidate.tile_id;
    goalNode.topology.leaf_index = goalCandidate.leaf_index;
    goalNode.topology.cluster_id = goalCandidate.cluster_id;
    goalNode.topology.area_id = goalCandidate.area_id;
    goalNode.topology.region_id = goalCandidate.layer_index;
    goalNode.topology.portal_id = goalCandidate.tile_id;
    goalNode.topology.tile_id = goalCandidate.tile_id;
    goalNode.topology.tile_x = ( int32_t )goalCandidate.tile_x;
    goalNode.topology.tile_y = ( int32_t )goalCandidate.tile_y;
    goalNode.topology.cell_index = goalCandidate.cell_index;
    goalNode.topology.layer_index = goalCandidate.layer_index;
    goalNode.allowed_z_band.min_z = goalCandidate.center_origin.z - 32.0;
    goalNode.allowed_z_band.max_z = goalCandidate.center_origin.z + 32.0;
    goalNode.allowed_z_band.preferred_z = goalCandidate.center_origin.z;
    goalNode.flags = NAV2_HIERARCHY_NODE_FLAG_HAS_ALLOWED_Z_BAND;
    if ( goalCandidate.leaf_index >= 0 ) {
        goalNode.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_LEAF;
    }
    if ( goalCandidate.cluster_id >= 0 ) {
        goalNode.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_CLUSTER;
    }
    if ( goalCandidate.area_id >= 0 ) {
        goalNode.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_AREA;
    }

    if ( !SVG_Nav2_HierarchyGraph_AppendNode( &stageRuntime->hierarchy_graph, startNode )
        || !SVG_Nav2_HierarchyGraph_AppendNode( &stageRuntime->hierarchy_graph, goalNode ) ) {
        return false;
    }

    /**
    *    Bridge endpoints with a direct bidirectional region link so coarse search can progress
    *    without full decomposition overhead.
    **/
    const double dx = ( double )goalCandidate.center_origin.x - ( double )startCandidate.center_origin.x;
    const double dy = ( double )goalCandidate.center_origin.y - ( double )startCandidate.center_origin.y;
    const double dz = std::fabs( ( double )goalCandidate.center_origin.z - ( double )startCandidate.center_origin.z );
    const double directCost = std::sqrt( ( dx * dx ) + ( dy * dy ) ) + ( dz * 0.5 );

    nav2_hierarchy_edge_t forwardEdge = {};
    forwardEdge.edge_id = 1;
    forwardEdge.from_node_id = startNode.node_id;
    forwardEdge.to_node_id = goalNode.node_id;
    forwardEdge.kind = nav2_hierarchy_edge_kind_t::RegionLink;
    forwardEdge.flags = NAV2_HIERARCHY_EDGE_FLAG_PASSABLE | NAV2_HIERARCHY_EDGE_FLAG_TOPOLOGY_MATCHED;
    forwardEdge.base_cost = std::max( 1.0, directCost );
    forwardEdge.allowed_z_band.min_z = std::min( startNode.allowed_z_band.min_z, goalNode.allowed_z_band.min_z );
    forwardEdge.allowed_z_band.max_z = std::max( startNode.allowed_z_band.max_z, goalNode.allowed_z_band.max_z );
    forwardEdge.allowed_z_band.preferred_z = goalNode.allowed_z_band.preferred_z;

    nav2_hierarchy_edge_t reverseEdge = forwardEdge;
    reverseEdge.edge_id = 2;
    reverseEdge.from_node_id = goalNode.node_id;
    reverseEdge.to_node_id = startNode.node_id;
    reverseEdge.allowed_z_band.preferred_z = startNode.allowed_z_band.preferred_z;

    if ( !SVG_Nav2_HierarchyGraph_AppendEdge( &stageRuntime->hierarchy_graph, forwardEdge )
        || !SVG_Nav2_HierarchyGraph_AppendEdge( &stageRuntime->hierarchy_graph, reverseEdge ) ) {
        return false;
    }

    return true;
}


/**
*	@brief	Emit a bounded diagnostic for one scheduler-owned nav2 job.
*	@param	job	Job being reported.
*	@param	eventLabel	Short label describing the scheduler checkpoint.
**/
static void Nav2_Scheduler_DebugLogJobEvent( const nav2_query_job_t *job, const char *eventLabel ) {
    /**
    *    Require a valid job pointer and label before printing anything.
    **/
    if ( !job || !eventLabel ) {
        return;
    }

    /**
    *    Emit a compact checkpoint snapshot so live logs can prove whether the staged path reached
    *    PostProcess, Revalidation, and Commit with committed waypoints intact.
    **/
    /**
    *    Keep stage checkpoint spam opt-in through existing async-nav debug statistics cvar.
    **/
    if ( !Nav2_Scheduler_IsVerboseStatsEnabled() ) {
        return;
    }

    gi.dprintf( "[NAV2][Scheduler][%s] job=%llu stage=%d result=%d committed=%d waypoints=%zu pending=%d slice_ms=%.2f\n",
        eventLabel,
        ( unsigned long long )job->job_id,
        ( int32_t )job->state.stage,
        ( int32_t )job->state.result_status,
        job->has_committed_waypoints ? 1 : 0,
        job->committed_waypoints.size(),
        job->worker_in_flight ? 1 : 0,
        job->state.active_slice.granted_ms );
}


/**
*
*
*	Nav2 Scheduler Internal Helpers:
*
*
**/
/**
*	@brief	Resolve a comparable integer weight for a priority class.
*	@param	priority	Priority class to convert.
*	@return	Higher values represent more urgent scheduling priority.
**/
static int32_t Nav2_Scheduler_PriorityWeight( const nav2_query_priority_t priority ) {
    switch ( priority ) {
    case nav2_query_priority_t::Low: return 0;
    case nav2_query_priority_t::Normal: return 1;
    case nav2_query_priority_t::High: return 2;
    case nav2_query_priority_t::Critical: return 3;
    case nav2_query_priority_t::Count: break;
    default: break;
    }
    return 0;
}

/**
*	@brief	Request a stage-boundary restart for one active scheduler job.
*	@param	job	[in,out] Scheduler-owned query job.
*	@param	restartStage	Stage to restart from.
**/
static void Nav2_Scheduler_RequestStageRestart( nav2_query_job_t *job, const nav2_query_stage_t restartStage ) {
    /**
    *    Require a live job before applying restart metadata.
    **/
    if ( !job ) {
        return;
    }

    /**
    *    Increment restart diagnostics and move stage to requested restart boundary.
    **/
    job->restart_count++;
    job->stage_restart_count++;
    job->state.requires_revalidation = true;
    job->state.has_provisional_result = true;
    job->state.result_status = nav2_query_result_status_t::Partial;
    SVG_Nav2_QueryState_SetStage( &job->state, restartStage );
}

/**
*	@brief	Return whether a stage is expensive enough to degrade under overload.
*	@param	stage	Query stage to inspect.
*	@return	True when overload policy may reduce this stage to provisional behavior.
**/
static const bool Nav2_Scheduler_IsOverloadDegradableStage( const nav2_query_stage_t stage ) {
    /**
    *    Limit overload degradation to expensive refinement/postprocess stages where provisional output
    *    is acceptable for low-priority jobs.
    **/
    switch ( stage ) {
        case nav2_query_stage_t::FineRefinement:
        case nav2_query_stage_t::MoverLocalRefinement:
        case nav2_query_stage_t::PostProcess:
            return true;
        case nav2_query_stage_t::Intake:
        case nav2_query_stage_t::SnapshotBind:
        case nav2_query_stage_t::TrivialDirectCheck:
        case nav2_query_stage_t::TopologyClassification:
        case nav2_query_stage_t::MoverClassification:
        case nav2_query_stage_t::CandidateGeneration:
        case nav2_query_stage_t::CandidateRanking:
        case nav2_query_stage_t::CoarseSearch:
        case nav2_query_stage_t::CorridorExtraction:
        case nav2_query_stage_t::Revalidation:
        case nav2_query_stage_t::Commit:
        case nav2_query_stage_t::Completed:
        case nav2_query_stage_t::Count:
        default:
            break;
    }
    return false;
}

/**
*	@brief	Apply overload policy for one job after a slice was granted.
*	@param	runtime	[in,out] Scheduler runtime owning global overload counters.
*	@param	job	[in,out] Job receiving overload bookkeeping updates.
*	@param	slice	Granted slice.
**/
static void Nav2_Scheduler_ApplyOverloadPolicy( nav2_scheduler_runtime_t *runtime, nav2_query_job_t *job, const nav2_budget_slice_t &slice ) {
    /**
    *    Require runtime and job references before mutating overload bookkeeping fields.
    **/
    if ( !runtime || !job ) {
        return;
    }

    /**
    *    Count throttled slices for diagnostics and per-job tracking.
    **/
    if ( slice.was_throttled ) {
        runtime->overload_throttle_count++;
        job->throttled_slice_count++;
    }

    /**
    *    Request provisional fallback when critical overload affects low-priority expensive stages.
    **/
    if ( runtime->budget_runtime.overload_critical
        && ( job->request.priority == nav2_query_priority_t::Low || job->request.priority == nav2_query_priority_t::Normal )
        && Nav2_Scheduler_IsOverloadDegradableStage( job->state.stage ) ) {
        job->state.has_provisional_result = true;
        job->overload_provisional_fallback_count++;
        runtime->overload_provisional_fallback_count++;
    }
}

/**
*	@brief	Return the current scheduler frame number when runtime is initialized.
*	@return	Current scheduler frame number, or 0 when runtime is unavailable.
**/
static int64_t Nav2_Scheduler_CurrentFrameNumber( void ) {
    /**
    *    Return a deterministic fallback frame number when scheduler runtime is unavailable.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return 0;
    }
    return s_nav2_scheduler_runtime->budget_runtime.frame_number;
}

/**
*	@brief	Return the queue age in frames for one job.
*	@param	job	Scheduler-owned job to inspect.
*	@param	currentFrame	Current scheduler frame index.
*	@return	Queue age in frames.
**/
static uint32_t Nav2_Scheduler_JobQueueAgeFrames( const nav2_query_job_t &job, const int64_t currentFrame ) {
    /**
    *    Guard against pre-initialized frame indices and clamp to zero when unknown.
    **/
    if ( job.submitted_frame_number <= 0 || currentFrame <= job.submitted_frame_number ) {
        return 0;
    }
    return ( uint32_t )( currentFrame - job.submitted_frame_number );
}

/**
*	@brief	Return queue wait time in milliseconds for one job.
*	@param	job	Scheduler-owned job to inspect.
*	@param	nowMs	Current realtime milliseconds.
*	@return	Queue wait milliseconds.
**/
static double Nav2_Scheduler_JobQueueWaitMs( const nav2_query_job_t &job, const uint64_t nowMs ) {
    /**
    *    Without a queue-enter timestamp the wait duration cannot be computed.
    **/
    if ( job.queue_enter_timestamp_ms == 0 || nowMs <= job.queue_enter_timestamp_ms ) {
        return 0.0;
    }
    return ( double )( nowMs - job.queue_enter_timestamp_ms );
}

/**
*	@brief	Resolve or create scheduler-local stage runtime for one job id.
*	@param	jobId	Stable scheduler job identifier.
*	@return	Reference to scheduler-local stage runtime storage.
**/
static nav2_scheduler_job_stage_runtime_t &Nav2_Scheduler_StageRuntimeForJob( const uint64_t jobId ) {
    /**
    *    Use unordered-map emplacement so stage runtime is created lazily and deterministically.
    **/
    return s_nav2_scheduler_job_stage_runtime[ jobId ];
}

/**
*	@brief	Drop scheduler-local stage runtime for one finished or canceled job.
*	@param	jobId	Stable scheduler job identifier.
**/
static void Nav2_Scheduler_ClearStageRuntimeForJob( const uint64_t jobId ) {
    /**
    *    Erase scheduler-local per-job stage artifacts once they are no longer needed.
    **/
    s_nav2_scheduler_job_stage_runtime.erase( jobId );
}

/**
*	@brief	Return whether the supplied stage should execute through worker dispatch by default.
*	@param	stage	Query stage to classify.
*	@return	True when the stage is worker-friendly in the Task 11.1 staged pipeline.
**/
static const bool Nav2_Scheduler_StagePrefersWorker( const nav2_query_stage_t stage ) {
    /**
    *    Keep lightweight setup and acceptance stages on main thread while allowing expensive solver
    *    and postprocess stages to run through worker dispatch.
    **/
    switch ( stage ) {
        case nav2_query_stage_t::TopologyClassification:
        case nav2_query_stage_t::CandidateGeneration:
        case nav2_query_stage_t::CandidateRanking:
        case nav2_query_stage_t::Commit:
        case nav2_query_stage_t::Completed:
            return false;
        case nav2_query_stage_t::Intake:
        case nav2_query_stage_t::SnapshotBind:
        case nav2_query_stage_t::TrivialDirectCheck:
        case nav2_query_stage_t::MoverClassification:
        case nav2_query_stage_t::CoarseSearch:
        case nav2_query_stage_t::CorridorExtraction:
        case nav2_query_stage_t::FineRefinement:
        case nav2_query_stage_t::MoverLocalRefinement:
        case nav2_query_stage_t::PostProcess:
        case nav2_query_stage_t::Revalidation:
            return true;
        case nav2_query_stage_t::Count:
        default:
            break;
    }
    return true;
}

/**
*	@brief	Advance one query state to a new stage and apply default dispatch preference.
*	@param	state	[in,out] Query state to mutate.
*	@param	stage	Target stage.
**/
static void Nav2_Scheduler_SetJobStage( nav2_query_state_t *state, const nav2_query_stage_t stage ) {
    /**
    *    Guard against missing state pointers before applying stage transitions.
    **/
    if ( !state ) {
        return;
    }

    /**
    *    Assign next stage and synchronize default worker/main-thread execution preference.
    **/
    SVG_Nav2_QueryState_SetStage( state, stage );
    state->prefer_worker_dispatch = Nav2_Scheduler_StagePrefersWorker( stage );
}

/**
*	@brief	Map one stage to the next deterministic stage in the Task 11.1 pipeline.
*	@param	stage	Current stage.
*	@return	Next stage.
**/
static nav2_query_stage_t Nav2_Scheduler_NextStage( const nav2_query_stage_t stage ) {
    /**
    *    Preserve strict pipeline ordering from intake to final commit/completed transitions.
    **/
    switch ( stage ) {
        case nav2_query_stage_t::Intake:
            return nav2_query_stage_t::SnapshotBind;
        case nav2_query_stage_t::SnapshotBind:
            return nav2_query_stage_t::TrivialDirectCheck;
        case nav2_query_stage_t::TrivialDirectCheck:
            return nav2_query_stage_t::TopologyClassification;
        case nav2_query_stage_t::TopologyClassification:
            return nav2_query_stage_t::MoverClassification;
        case nav2_query_stage_t::MoverClassification:
            return nav2_query_stage_t::CandidateGeneration;
        case nav2_query_stage_t::CandidateGeneration:
            return nav2_query_stage_t::CandidateRanking;
        case nav2_query_stage_t::CandidateRanking:
            return nav2_query_stage_t::CoarseSearch;
        case nav2_query_stage_t::CoarseSearch:
            return nav2_query_stage_t::CorridorExtraction;
        case nav2_query_stage_t::CorridorExtraction:
            return nav2_query_stage_t::FineRefinement;
        case nav2_query_stage_t::FineRefinement:
            return nav2_query_stage_t::MoverLocalRefinement;
        case nav2_query_stage_t::MoverLocalRefinement:
            return nav2_query_stage_t::PostProcess;
        case nav2_query_stage_t::PostProcess:
            return nav2_query_stage_t::Revalidation;
        case nav2_query_stage_t::Revalidation:
            return nav2_query_stage_t::Commit;
        case nav2_query_stage_t::Commit:
            return nav2_query_stage_t::Completed;
        case nav2_query_stage_t::Completed:
        case nav2_query_stage_t::Count:
        default:
            break;
    }
    return nav2_query_stage_t::Completed;
}

/**
*	@brief	Mark a job as terminal and clear active slice bookkeeping.
*	@param	job	[in,out] Scheduler-owned query job.
*	@param	status	Terminal result status.
**/
static void Nav2_Scheduler_SetTerminalStatus( nav2_query_job_t *job, const nav2_query_result_status_t status ) {
    /**
    *    Guard against missing jobs before mutating terminal fields.
    **/
    if ( !job ) {
        return;
    }

    /**
    *    When a job is finalized as non-success, clear any staged committed-waypoint payload so
    *    request consumers do not ingest stale path points from a previous lifecycle.
    **/
    if ( status != nav2_query_result_status_t::Success ) {
        job->committed_waypoints.clear();
        job->has_committed_waypoints = false;
    }

    /**
    *    Assign terminal status and move stage to completed while clearing any active slice.
    **/
    job->state.result_status = status;
    Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::Completed );
    job->state.active_slice = nav2_budget_slice_t{};
}

/**
*	@brief	Fail one scheduler-owned job while emitting a bounded stage-local reason.
*	@param	job	[in,out] Scheduler-owned query job.
*	@param	reason	Short deterministic reason label for the failure site.
*	@return	Always false so callers can return this helper directly from failure branches.
**/
static const bool Nav2_Scheduler_FailJobWithReason( nav2_query_job_t *job, const char *reason ) {
    /**
    *    Require a live job before mutating terminal state; still return false for call-site flow.
    **/
    if ( !job ) {
        return false;
    }

    /**
    *    Emit a compact failure checkpoint so runtime logs reveal exactly which stage gate rejected
    *    the job before postprocess/commit.
    **/
    if ( Nav2_Scheduler_IsVerboseStatsEnabled() ) {
        gi.dprintf( "[NAV2][Scheduler][Fail] job=%llu stage=%d result=%d reason=%s\n",
            ( unsigned long long )job->job_id,
            ( int32_t )job->state.stage,
            ( int32_t )job->state.result_status,
            reason ? reason : "(null)" );
    }

    /**
    *    Publish deterministic terminal failed status after logging the local reason.
    **/
    Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
    return false;
}

/**
*\t@brief\tBuild deterministic start/goal candidate sets and select candidates for one job.
*\t@param\tjob\t[in,out] Job receiving selected candidates.
*\t@param\tstageRuntime\t[in,out] Scheduler-local stage runtime storage.
*\t@return\tTrue when both start and goal candidates were selected.
**/
static const bool Nav2_Scheduler_BuildAndSelectCandidates( nav2_query_job_t *job, nav2_scheduler_job_stage_runtime_t *stageRuntime ) {
    /**
    *    Require both scheduler job and stage-runtime storage before running candidate generation.
    **/
    if ( !job || !stageRuntime ) {
        return false;
    }

    /**
    *    Keep queue-enter timestamps hot while a job remains dispatchable so fairness diagnostics can
    *    evaluate queue-age and delay behavior accurately.
    **/
    if ( job->queue_enter_timestamp_ms == 0 ) {
        job->queue_enter_timestamp_ms = gi.GetRealTime ? gi.GetRealTime() : 0;
    }

    /**
    *    Resolve active nav2 mesh and canonical agent bounds used by candidate generation.
    **/
    const nav2_query_mesh_t *queryMesh = SVG_Nav2_GetQueryMesh();
    if ( !queryMesh || !queryMesh->main_mesh ) {
        return false;
    }

    /**
    *   Prefer the per-request agent hull captured at submission time so scheduler-side
    *   candidate generation stays consistent with the requesting mover's navigation bounds.
    *
    *   Falling back to the cvar-derived profile remains important for older or generic
    *   callers that may not have populated request-local bounds yet, but sound-follow and
    *   other reusable NPC movers should not lose valid candidate generation because the
    *   async scheduler silently swapped back to default profile dimensions.
    **/
    Vector3 candidateAgentMins = job->request.agent_mins;
    Vector3 candidateAgentMaxs = job->request.agent_maxs;
    const bool hasRequestAgentBounds = ( candidateAgentMaxs.x > candidateAgentMins.x )
        && ( candidateAgentMaxs.y > candidateAgentMins.y )
        && ( candidateAgentMaxs.z > candidateAgentMins.z );
    if ( !hasRequestAgentBounds ) {
        const nav2_query_agent_profile_t agentProfile = SVG_Nav2_BuildAgentProfileFromCvars();
        candidateAgentMins = agentProfile.mins;
        candidateAgentMaxs = agentProfile.maxs;
    }

    /**
     *    Mirror the same request-local mover constraints into stage-local adjacency policy so any
     *    later span-adjacency legality work can stay aligned with the requesting monster hull.
     **/
    stageRuntime->query_policy = job->request.policy;
    if ( stageRuntime->query_policy.agent_maxs.x <= stageRuntime->query_policy.agent_mins.x
        || stageRuntime->query_policy.agent_maxs.y <= stageRuntime->query_policy.agent_mins.y
        || stageRuntime->query_policy.agent_maxs.z <= stageRuntime->query_policy.agent_mins.z ) {
        const nav2_query_agent_profile_t agentProfile = SVG_Nav2_BuildAgentProfileFromCvars();
        stageRuntime->query_policy.agent_mins = agentProfile.mins;
        stageRuntime->query_policy.agent_maxs = agentProfile.maxs;
    }
    stageRuntime->span_adjacency_policy = SVG_Nav2_QueryBuildAdjacencyPolicy( stageRuntime->query_policy );
    stageRuntime->span_adjacency_policy.agent_mins = candidateAgentMins;
    stageRuntime->span_adjacency_policy.agent_maxs = candidateAgentMaxs;

    /**
    *    Build candidate sets for both start and goal endpoints using the same BSP-aware stage helper.
    **/
    const bool haveStartCandidates = SVG_Nav2_BuildGoalCandidates( queryMesh->main_mesh,
        job->request.start_origin,
        job->request.start_origin,
        candidateAgentMins,
        candidateAgentMaxs,
        &stageRuntime->start_candidates );
    const bool haveGoalCandidates = SVG_Nav2_BuildGoalCandidates( queryMesh->main_mesh,
        job->request.start_origin,
        job->request.goal_origin,
        candidateAgentMins,
        candidateAgentMaxs,
        &stageRuntime->goal_candidates );
    if ( !haveStartCandidates || !haveGoalCandidates ) {
        return false;
    }

    /**
    *    Select deterministic best candidates for both endpoints.
    **/
    stageRuntime->has_selected_start_candidate = SVG_Nav2_SelectBestGoalCandidate(
        stageRuntime->start_candidates,
        &stageRuntime->selected_start_candidate );
    stageRuntime->has_selected_goal_candidate = SVG_Nav2_SelectBestGoalCandidate(
        stageRuntime->goal_candidates,
        &stageRuntime->selected_goal_candidate );
    if ( !stageRuntime->has_selected_start_candidate || !stageRuntime->has_selected_goal_candidate ) {
        return false;
    }

    /**
    *    Mirror candidate counts into generic query progress counters for scheduler diagnostics.
    **/
    job->state.progress.candidate_count = ( uint32_t )(
        stageRuntime->start_candidates.candidate_count + stageRuntime->goal_candidates.candidate_count );
    return true;
}

/**
*\t@brief\tBuild per-job hierarchy dependencies used by coarse search.
*\t@param\tstageRuntime\t[in,out] Scheduler-local stage runtime storage.
*\t@return\tTrue when connectors, region layers, and hierarchy graph were produced.
**/
static const bool Nav2_Scheduler_BuildHierarchyDependencies( nav2_scheduler_job_stage_runtime_t *stageRuntime ) {
    /**
    *    Require stage runtime storage before building hierarchy dependencies.
    **/
    if ( !stageRuntime ) {
        return false;
    }

    /**
    *    Bind validated topology publication before hierarchy dependencies are built so region and
    *    hierarchy stages share one scheduler-owned topology source of truth.
    **/
    if ( !Nav2_Scheduler_BindTopologyArtifact( stageRuntime ) || !stageRuntime->has_topology_artifact ) {
        return false;
    }

    /**
    *    Reuse snapshot-scoped hierarchy dependencies when the scheduler runtime already published
    *    a cache for the current static-nav version.
    **/
    nav2_scheduler_runtime_t *runtime = s_nav2_scheduler_runtime.get();
    uint32_t currentStaticNavVersion = 0;
    if ( runtime ) {
        currentStaticNavVersion = runtime->snapshot_runtime.current.static_nav_version;

        // Invalidate cache when the static-nav publication version changed.
        if ( runtime->has_cached_hierarchy_dependencies
            && runtime->cached_hierarchy_static_nav_version != currentStaticNavVersion ) {
            Nav2_Scheduler_ClearCachedTopologyAndHierarchy( runtime );
        }

        /**
        *    Enforce connector-less fallback as the default policy. If an older cache for this
        *    snapshot was built through full decomposition without connectors, invalidate it once
        *    so the lightweight policy becomes canonical for connector-less maps.
        **/
        if ( runtime->has_cached_hierarchy_dependencies
            && !runtime->cached_hierarchy_is_no_connector_fallback
            && runtime->cached_hierarchy_connectors.connectors.empty() ) {
            Nav2_Scheduler_ClearCachedTopologyAndHierarchy( runtime );
        }

        // Reuse cached dependencies when they are valid for the current snapshot version.
        if ( runtime->has_cached_hierarchy_dependencies ) {
            stageRuntime->span_grid = runtime->cached_hierarchy_span_grid;
            stageRuntime->connectors = runtime->cached_hierarchy_connectors;
            stageRuntime->region_layers = runtime->cached_hierarchy_region_layers;
            stageRuntime->hierarchy_graph = runtime->cached_hierarchy_graph;
            gi.dprintf( "[NAV2][Scheduler][HierarchyDeps] cache=hit static=%u cols=%zu connectors=%zu layers=%zu nodes=%zu\n",
                runtime->cached_hierarchy_static_nav_version,
                stageRuntime->span_grid.columns.size(),
                stageRuntime->connectors.connectors.size(),
                stageRuntime->region_layers.layers.size(),
                stageRuntime->hierarchy_graph.nodes.size() );
            return true;
        }
    }

    /**
    *    Build span-grid snapshot used by connector and region-layer extraction.
    **/
    nav2_span_grid_build_stats_t spanBuildStats = {};
    if ( !SVG_Nav2_BuildSpanGridFromMesh( SVG_Nav2_Runtime_GetMesh(), &stageRuntime->span_grid, &spanBuildStats ) ) {
        /**
        *    Emit bounded diagnostics so logs reveal whether hierarchy build failed before any
        *    connector/layer/graph work started.
        **/
        gi.dprintf( "[NAV2][Scheduler][HierarchyDeps] fail=BuildSpanGridFromMesh sampled=%d cols=%d spans=%d\n",
            spanBuildStats.sampled_columns,
            spanBuildStats.emitted_columns,
            spanBuildStats.emitted_spans );
        return false;
    }

    /**
    *    Extract connectors from span-grid data; use empty entity set for deterministic scheduler baseline.
    **/
    const std::vector<svg_base_edict_t *> noEntities = {};
    const bool hasAnyConnectors = SVG_Nav2_ExtractConnectors( stageRuntime->span_grid, noEntities, &stageRuntime->connectors );

    /**
    *    Empty connector sets are valid on flat/simple maps where span extraction does not emit
    *    vertical or entity-derived transitions. Keep building region/hierarchy data from spans.
    **/
    if ( !hasAnyConnectors ) {
        gi.dprintf( "[NAV2][Scheduler][HierarchyDeps] warn=NoConnectors cols=%zu spans=%d\n",
            stageRuntime->span_grid.columns.size(),
            spanBuildStats.emitted_spans );

        /**
        *    Fast-path connector-less maps with a lightweight endpoint-only hierarchy fallback to
        *    avoid long decomposition stalls on dense single-layer span grids.
        **/
        if ( Nav2_Scheduler_BuildNoConnectorFallbackHierarchy( stageRuntime ) ) {
            gi.dprintf( "[NAV2][Scheduler][HierarchyDeps] ok=NoConnectorFallback nodes=%zu edges=%zu\n",
                stageRuntime->hierarchy_graph.nodes.size(),
                stageRuntime->hierarchy_graph.edges.size() );

            /**
            *    Publish connector-less fallback as the canonical cached hierarchy policy for this
            *    snapshot so later jobs avoid full decomposition on no-connector maps.
            **/
            if ( runtime ) {
                runtime->cached_hierarchy_span_grid = stageRuntime->span_grid;
                runtime->cached_hierarchy_connectors = stageRuntime->connectors;
                runtime->cached_hierarchy_region_layers = stageRuntime->region_layers;
                runtime->cached_hierarchy_graph = stageRuntime->hierarchy_graph;
                runtime->cached_hierarchy_static_nav_version = currentStaticNavVersion;
                runtime->has_cached_hierarchy_dependencies = true;
                runtime->cached_hierarchy_is_no_connector_fallback = true;
            }
            return true;
        }
    }

    /**
    *    Build region-layer and hierarchy graph assets required by coarse-stage solver state.
    **/
    nav2_region_layer_summary_t regionSummary = {};
    if ( !SVG_Nav2_BuildRegionLayersFromTopology( stageRuntime->topology_artifact, stageRuntime->span_grid, stageRuntime->connectors,
        &stageRuntime->region_layers, &regionSummary ) ) {
        /**
        *    Emit bounded diagnostics to show layer-build state when hierarchy dependency
        *    construction fails after connector extraction.
        **/
        gi.dprintf( "[NAV2][Scheduler][HierarchyDeps] fail=BuildRegionLayers cols=%zu spans=%d connectors=%zu\n",
            stageRuntime->span_grid.columns.size(),
            spanBuildStats.emitted_spans,
            stageRuntime->connectors.connectors.size() );
        return false;
    }

    nav2_hierarchy_summary_t hierarchySummary = {};
    if ( !SVG_Nav2_BuildHierarchyGraph( stageRuntime->topology_artifact, stageRuntime->connectors,
        stageRuntime->region_layers, &stageRuntime->hierarchy_graph, &hierarchySummary ) ) {
        /**
        *    Emit bounded diagnostics to show hierarchy-graph state when coarse-search dependency
        *    build fails after region-layer decomposition.
        **/
        gi.dprintf( "[NAV2][Scheduler][HierarchyDeps] fail=BuildHierarchyGraph layers=%d edges=%d connectors=%zu\n",
            regionSummary.layer_count,
            regionSummary.edge_count,
            stageRuntime->connectors.connectors.size() );
        return false;
    }

    /**
    *    Emit one compact success checkpoint with counts so runtime logs can prove dependency
    *    construction reached a usable hierarchy graph.
    **/
    gi.dprintf( "[NAV2][Scheduler][HierarchyDeps] ok cols=%zu spans=%d connectors=%zu layers=%d layer_edges=%d nodes=%d node_edges=%d\n",
        stageRuntime->span_grid.columns.size(),
        spanBuildStats.emitted_spans,
        stageRuntime->connectors.connectors.size(),
        regionSummary.layer_count,
        regionSummary.edge_count,
        hierarchySummary.node_count,
        hierarchySummary.edge_count );

    /**
    *    Publish snapshot-scoped hierarchy dependencies so later jobs can skip expensive rebuilds
    *    until static-nav publication changes.
    **/
    if ( runtime ) {
        runtime->cached_hierarchy_span_grid = stageRuntime->span_grid;
        runtime->cached_hierarchy_connectors = stageRuntime->connectors;
        runtime->cached_hierarchy_region_layers = stageRuntime->region_layers;
        runtime->cached_hierarchy_graph = stageRuntime->hierarchy_graph;
        runtime->cached_hierarchy_static_nav_version = currentStaticNavVersion;
        runtime->has_cached_hierarchy_dependencies = true;
        runtime->cached_hierarchy_is_no_connector_fallback = false;
    }

    return true;
}

/**
*\t@brief\tExecute one scheduler stage for a job and apply deterministic stage transitions.
*\t@param\tjob\t[in,out] Scheduler-owned job receiving stage execution.
*\t@param\tstageRuntime\t[in,out] Scheduler-local stage runtime storage.
*\t@return\tTrue when stage execution progressed successfully.
**/
static const bool Nav2_Scheduler_ExecuteStage( nav2_query_job_t *job, nav2_scheduler_job_stage_runtime_t *stageRuntime ) {
    /**
    *    Require both job and stage-runtime storage before running stage handlers.
    **/
    if ( !job || !stageRuntime ) {
        return false;
    }

    /**
    *    Handle cancellation and restart requests before running normal stage logic.
    **/
    if ( job->state.cancel_requested ) {
        Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Cancelled );
        return true;
    }
    if ( job->state.restart_requested ) {
       /**
        *    Record restart churn before clearing the restart request flag.
        **/
        if ( s_nav2_scheduler_runtime ) {
            s_nav2_scheduler_runtime->stage_restart_count++;
        }
        job->stage_restart_count++;
        job->state.restart_requested = false;
        Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::TopologyClassification );
    }

    /**
    *    Execute stage-specific handler and transition state based on outcomes.
    **/
    switch ( job->state.stage ) {
        case nav2_query_stage_t::TopologyClassification:
        {
            if ( !Nav2_Scheduler_ExecuteTopologyClassificationStage( job, stageRuntime ) ) {
                return Nav2_Scheduler_FailJobWithReason( job, "TopologyClassification" );
            }
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::CandidateGeneration );
            return true;
        }

        case nav2_query_stage_t::CandidateGeneration:
        case nav2_query_stage_t::CandidateRanking: {
            if ( !Nav2_Scheduler_BuildAndSelectCandidates( job, stageRuntime ) ) {
                return Nav2_Scheduler_FailJobWithReason( job, "BuildAndSelectCandidates" );
            }
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::CoarseSearch );
            return true;
        }

        case nav2_query_stage_t::CoarseSearch: {
            if ( !stageRuntime->coarse_state.hierarchy_graph_bound ) {
                if ( !Nav2_Scheduler_BuildHierarchyDependencies( stageRuntime ) ) {
                    return Nav2_Scheduler_FailJobWithReason( job, "BuildHierarchyDependencies" );
                }
                if ( !SVG_Nav2_CoarseAStar_Init( &stageRuntime->coarse_state,
                    stageRuntime->hierarchy_graph,
                    stageRuntime->selected_start_candidate,
                    stageRuntime->selected_goal_candidate,
                    job->job_id ) ) {
                    return Nav2_Scheduler_FailJobWithReason( job, "CoarseAStar_Init" );
                }
                stageRuntime->coarse_state.query_state.snapshot = job->state.snapshot;
            }

            if ( !SVG_Nav2_CoarseAStar_Step( &stageRuntime->coarse_state, job->state.active_slice, &stageRuntime->coarse_result ) ) {
                return Nav2_Scheduler_FailJobWithReason( job, "CoarseAStar_Step" );
            }
            job->state.progress.coarse_expansions = stageRuntime->coarse_state.query_state.progress.coarse_expansions;

            if ( stageRuntime->coarse_result.status == nav2_coarse_astar_status_t::Success ) {
                Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::CorridorExtraction );
                return true;
            }
            if ( stageRuntime->coarse_result.status == nav2_coarse_astar_status_t::Partial
                || stageRuntime->coarse_result.status == nav2_coarse_astar_status_t::Running ) {
                job->state.result_status = nav2_query_result_status_t::Partial;
                return true;
            }

            return Nav2_Scheduler_FailJobWithReason( job, "CoarseAStar_Status" );
        }

        case nav2_query_stage_t::CorridorExtraction: {
            if ( !SVG_Nav2_BuildCorridorFromCoarseAStar( stageRuntime->coarse_state, &stageRuntime->corridor ) ) {
                return Nav2_Scheduler_FailJobWithReason( job, "BuildCorridorFromCoarseAStar" );
            }
            job->state.has_provisional_result = true;
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::FineRefinement );
            return true;
        }

        case nav2_query_stage_t::FineRefinement: {
            if ( stageRuntime->fine_state.status == nav2_fine_astar_status_t::None ) {
                if ( !SVG_Nav2_FineAStar_Init( &stageRuntime->fine_state, stageRuntime->corridor, job->job_id,
                    &stageRuntime->span_adjacency_policy ) ) {
                    return Nav2_Scheduler_FailJobWithReason( job, "FineAStar_Init" );
                }
                stageRuntime->fine_state.query_state.snapshot = job->state.snapshot;
            }

            if ( !SVG_Nav2_FineAStar_Step( &stageRuntime->fine_state, job->state.active_slice, &stageRuntime->fine_result ) ) {
                return Nav2_Scheduler_FailJobWithReason( job, "FineAStar_Step" );
            }
            job->state.progress.fine_expansions = stageRuntime->fine_state.query_state.progress.fine_expansions;

            /**
            *    Emit one bounded per-job fine fallback diagnostic so runtime logs can confirm the
            *    loop-break fallback path is activating and quantify residual technical debt.
            **/
            if ( Nav2_Scheduler_IsVerboseStatsEnabled() ) {
                gi.dprintf( "[NAV2][Scheduler][FineFallback] job=%llu activations=%u status=%d path_nodes=%zu\n",
                    ( unsigned long long )job->job_id,
                    stageRuntime->fine_state.diagnostics.fallback_path_activations,
                    ( int32_t )stageRuntime->fine_result.status,
                    stageRuntime->fine_state.path.node_ids.size() );
            }

            if ( stageRuntime->fine_result.status == nav2_fine_astar_status_t::Success ) {
                Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::MoverLocalRefinement );
                return true;
            }
            if ( stageRuntime->fine_result.status == nav2_fine_astar_status_t::Partial
                || stageRuntime->fine_result.status == nav2_fine_astar_status_t::Running ) {
                job->state.result_status = nav2_query_result_status_t::Partial;
                return true;
            }

            return Nav2_Scheduler_FailJobWithReason( job, "FineAStar_Status" );
        }

        case nav2_query_stage_t::MoverLocalRefinement: {
            stageRuntime->mover_transform.world_origin = job->request.start_origin;
            stageRuntime->mover_transform.local_origin = job->request.start_origin;
            stageRuntime->mover_transform.is_valid = true;
            stageRuntime->mover_transform.owner_entnum = job->request.agent_entnum;
            if ( !SVG_Nav2_RunMoverLocalRefinement( stageRuntime->mover_transform,
                job->state.snapshot,
                stageRuntime->fine_state,
                64,
                &stageRuntime->mover_refine_result ) ) {
                return Nav2_Scheduler_FailJobWithReason( job, "RunMoverLocalRefinement" );
            }
            job->state.progress.mover_expansions = stageRuntime->mover_refine_result.diagnostics.inspected_nodes;
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::PostProcess );
            return true;
        }

        case nav2_query_stage_t::PostProcess: {
            nav2_postprocess_policy_t postPolicy = {};
            if ( !SVG_Nav2_Postprocess_FinePath( SVG_Nav2_GetQueryMesh(), stageRuntime->fine_state, postPolicy, &stageRuntime->postprocess_result ) ) {
                return Nav2_Scheduler_FailJobWithReason( job, "Postprocess_FinePath" );
            }

            /**
            *    Publish the postprocessed multi-waypoint payload onto the scheduler job so the
            *    async request seam can commit the real path once this lifecycle reaches Commit.
            **/
            job->committed_waypoints = stageRuntime->postprocess_result.waypoints;
            job->has_committed_waypoints = stageRuntime->postprocess_result.success
                && job->committed_waypoints.size() >= 2;

            /**
            *    Log the waypoint publication checkpoint so live diagnostics can confirm whether the
            *    scheduler actually materialized a real path payload before commit.
            **/
            Nav2_Scheduler_DebugLogJobEvent( job, "PostProcess" );

            /**
            *    Require a real waypoint payload before advancing to revalidation/commit so the
            *    request seam never reports success with a missing path.
            **/
            if ( !job->has_committed_waypoints ) {
                return Nav2_Scheduler_FailJobWithReason( job, "Postprocess_EmptyWaypoints" );
            }

            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::Revalidation );
            return true;
        }

        case nav2_query_stage_t::Revalidation: {
            if ( !s_nav2_scheduler_runtime ) {
                return Nav2_Scheduler_FailJobWithReason( job, "Revalidation_MissingRuntime" );
            }

            const nav2_snapshot_staleness_t staleness = SVG_Nav2_Snapshot_Compare(
                &s_nav2_scheduler_runtime->snapshot_runtime,
                job->state.snapshot );

            /**
            *    Route snapshot decisions through explicit action semantics so scheduler behavior
            *    remains deterministic and debuggable across accept/revalidate/restart/resubmit paths.
            **/
            switch ( staleness.action ) {
                case nav2_snapshot_stale_action_t::Accept:
                    job->state.requires_revalidation = false;
                    Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::Commit );
                    return true;
                case nav2_snapshot_stale_action_t::RevalidateOnly:
                    job->state.requires_revalidation = staleness.requires_revalidation;
                    Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::Commit );
                    return true;
                case nav2_snapshot_stale_action_t::RestartStage:
                    Nav2_Scheduler_RequestStageRestart( job, staleness.restart_stage );
                    return true;
                case nav2_snapshot_stale_action_t::Resubmit:
                    Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Stale );
                    return false;
                default:
                    break;
            }

            return Nav2_Scheduler_FailJobWithReason( job, "Revalidation_UnknownAction" );
        }

        case nav2_query_stage_t::Commit: {
            job->state.has_provisional_result = true;
            job->state.requires_revalidation = false;

            /**
            *    Record the terminal success checkpoint right before the result flips to Success so
            *    callers can see the final stage and waypoint count in the live logs.
            **/
            Nav2_Scheduler_DebugLogJobEvent( job, "Commit" );
            Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Success );
            return true;
        }

        case nav2_query_stage_t::Intake:
        case nav2_query_stage_t::SnapshotBind:
        case nav2_query_stage_t::TrivialDirectCheck:
        case nav2_query_stage_t::MoverClassification: {
            const nav2_query_stage_t nextStage = Nav2_Scheduler_NextStage( job->state.stage );
            Nav2_Scheduler_SetJobStage( &job->state, nextStage );
            return true;
        }

        case nav2_query_stage_t::Completed:
            return true;
        case nav2_query_stage_t::Count:
        default:
            break;
    }

    return Nav2_Scheduler_FailJobWithReason( job, "ExecuteStage_UnexpectedStage" );
}

/**
*\t@brief\tExecute one granted slice for a scheduler-owned job.
*\t@param\tjob\t[in,out] Scheduler-owned job receiving stage execution.
*\t@return\tTrue when stage execution ran.
**/
const bool SVG_Nav2_Scheduler_ExecuteGrantedSlice( nav2_query_job_t *job ) {
    /**
    *    Require a scheduler job and a valid granted slice before running stage handlers.
    **/
    if ( !job ) {
        return false;
    }
    if ( job->state.active_slice.granted_ms <= 0.0 ) {
        return false;
    }

    /**
    *    Bind scheduler snapshot versions on first execution so worker stages stay version-safe.
    **/
    if ( s_nav2_scheduler_runtime && job->state.snapshot.static_nav_version == 0 ) {
        job->state.snapshot = SVG_Nav2_Snapshot_GetCurrentHandle( &s_nav2_scheduler_runtime->snapshot_runtime );
    }

    /**
    *    Execute the current stage and then clear the active slice bookkeeping.
    **/
    nav2_scheduler_job_stage_runtime_t &stageRuntime = Nav2_Scheduler_StageRuntimeForJob( job->job_id );
    const nav2_query_stage_t stageBeforeExecute = job->state.stage;
    const bool executed = Nav2_Scheduler_ExecuteStage( job, &stageRuntime );
    job->state.progress.slices_consumed++;
    job->state.active_slice = nav2_budget_slice_t{};

    /**
    *    Record stage-transition and restart-churn diagnostics after each executed slice.
    **/
    if ( s_nav2_scheduler_runtime && stageBeforeExecute != job->state.stage ) {
        s_nav2_scheduler_runtime->stage_transition_count++;
        job->stage_transition_count++;
    }
   // Restart churn is recorded when restart requests are consumed in stage execution.

    /**
    *    Release scheduler-local stage artifacts once the job reaches terminal state.
    **/
    if ( SVG_Nav2_QueryJob_IsTerminal( job ) ) {
        Nav2_Scheduler_ClearStageRuntimeForJob( job->job_id );
    }
    return executed;
}

/**
*	@brief	Return whether a job is eligible to receive another scheduler slice.
*	@param	job	Job to inspect.
*	@return	True when the job is active, non-terminal, and not already cancelled.
**/
static const bool Nav2_Scheduler_IsSliceEligible( const nav2_query_job_t &job ) {
    /**
    *    Exclude jobs that already reached a terminal result state.
    **/
    if ( SVG_Nav2_QueryJob_IsTerminal( &job ) ) {
        return false;
    }

    /**
    *    Exclude jobs that have an outstanding explicit cancel request.
    **/
    if ( job.state.cancel_requested ) {
        return false;
    }

    /**
    *    Exclude jobs that already reached the completed pseudo-stage even if their terminal result
    *    has not yet been published by a future integration layer.
    **/
    if ( job.state.stage == nav2_query_stage_t::Completed ) {
        return false;
    }

    return true;
}

/**
*	@brief	Choose the next job index that should receive a scheduler slice.
*	@param	runtime	Scheduler runtime containing queued jobs.
*	@return	Index of the best eligible job, or -1 when no job can receive work.
*	@note	The current tie-break is deterministic: higher priority first, then older lower job id.
**/
static int32_t Nav2_Scheduler_SelectNextJobIndex( nav2_scheduler_runtime_t &runtime ) {
    int32_t bestIndex = -1;
    double bestScore = -std::numeric_limits<double>::infinity();
    const int64_t currentFrame = Nav2_Scheduler_CurrentFrameNumber();
    const uint64_t nowMs = gi.GetRealTime ? gi.GetRealTime() : 0;
    const uint32_t starvationThresholdFrames = runtime.budget_runtime.policy.starvation_frame_threshold;
    const double unfairDelayThresholdMs = runtime.budget_runtime.policy.unfair_delay_threshold_ms;
    const size_t jobCount = runtime.jobs.size();
    int32_t bestJobId = std::numeric_limits<int32_t>::max();

    /**
    *    Scan active jobs and score them by priority plus queue age so old low-priority requests
    *    eventually receive slices and do not starve behind younger high-priority work.
    **/
    for ( size_t i = 0; i < jobCount; i++ ) {
        nav2_query_job_t &job = runtime.jobs[ i ];
        if ( !Nav2_Scheduler_IsSliceEligible( job ) ) {
            continue;
        }

        /**
        *    Ensure queue-enter timestamp is initialized so queue wait diagnostics are meaningful.
        **/
        if ( job.queue_enter_timestamp_ms == 0 ) {
            job.queue_enter_timestamp_ms = nowMs;
        }

        /**
        *    Build fairness score from stable priority weight plus age-derived starvation boost.
        **/
        const int32_t priorityWeight = Nav2_Scheduler_PriorityWeight( job.request.priority );
        const uint32_t queueAgeFrames = Nav2_Scheduler_JobQueueAgeFrames( job, currentFrame );
        const double queueAgeMs = Nav2_Scheduler_JobQueueWaitMs( job, nowMs );
        double score = ( double )priorityWeight * 1000.0;
        score += ( double )std::min<uint32_t>( queueAgeFrames, 120 ) * 15.0;
        score += std::min( queueAgeMs, 500.0 ) * 0.05;

        /**
        *    Apply a hard starvation bump when queue age exceeds the configured threshold.
        **/
        if ( starvationThresholdFrames > 0 && queueAgeFrames >= starvationThresholdFrames ) {
            score += 5000.0;
        }

        /**
        *    Emit unfair-delay diagnostics when queue wait exceeds configured thresholds.
        **/
        if ( unfairDelayThresholdMs > 0.0 && queueAgeMs >= unfairDelayThresholdMs ) {
            runtime.unfair_delay_event_count++;
            SVG_Nav2_Bench_RecordFailure( &job.bench_query, nav2_bench_failure_t::UnfairQueryDelay );
        }

        /**
        *    Select highest score and preserve deterministic tie-break by lower stable job id.
        **/
        if ( bestIndex < 0 || score > bestScore ) {
            bestScore = score;
            bestIndex = ( int32_t )i;
            bestJobId = job.job_id;
            continue;
        }

        if ( score == bestScore && job.job_id < bestJobId ) {
            bestIndex = ( int32_t )i;
            bestJobId = job.job_id;
        }
    }

    /**
    *    Record starvation-prevention diagnostics when selected job exceeded starvation threshold.
    **/
    if ( bestIndex >= 0 && starvationThresholdFrames > 0 ) {
        nav2_query_job_t &selectedJob = runtime.jobs[ ( size_t )bestIndex ];
        const uint32_t selectedAgeFrames = Nav2_Scheduler_JobQueueAgeFrames( selectedJob, currentFrame );
        if ( selectedAgeFrames >= starvationThresholdFrames ) {
            selectedJob.starvation_boost_count++;
            runtime.starvation_prevention_boost_count++;
            SVG_Nav2_Bench_RecordFailure( &selectedJob.bench_query, nav2_bench_failure_t::SchedulerStarvation );
        }
    }

    return bestIndex;
}


/**
*
*
*	Nav2 Scheduler Public API:
*
*
**/
/**
*	@brief	Initialize the nav2 scheduler foundation runtime.
*	@return	True when the scheduler runtime was created successfully.
*	@note	This allocates the scheduler runtime using svgame tagged ownership conventions.
**/
const bool SVG_Nav2_Scheduler_Init( void ) {
    /**
    *    Reuse the existing runtime if the scheduler foundation was already initialized.
    **/
    if ( s_nav2_scheduler_runtime ) {
        return true;
    }

    /**
    *    Allocate the runtime using svgame-level tagged ownership so the scheduler foundation fits
    *    the existing memory model from the beginning.
    **/
    if ( !s_nav2_scheduler_runtime.create( TAG_SVGAME ) ) {
        return false;
    }

    /**
    *    Seed deterministic budget and snapshot defaults inside the newly allocated runtime.
    **/
    SVG_Nav2_Snapshot_ResetRuntime( &s_nav2_scheduler_runtime->snapshot_runtime );
    SVG_Nav2_Budget_ResetFrame( &s_nav2_scheduler_runtime->budget_runtime, 0 );
    return true;
}

/**
*	@brief	Shutdown the nav2 scheduler foundation runtime.
*	@note	This releases all scheduler-owned jobs and resets the runtime RAII owner.
**/
void SVG_Nav2_Scheduler_Shutdown( void ) {
    /**
    *    Reset the runtime owner so all scheduler-owned state is destroyed through the established
    *    svgame RAII path.
    **/
    s_nav2_scheduler_runtime.reset();

    /**
    *    Clear scheduler-local per-job stage artifacts during shutdown so no staged runtime cache
    *    survives across map transitions or subsystem teardown.
    **/
    s_nav2_scheduler_job_stage_runtime.clear();
}

/**
*	@brief	Return whether the scheduler foundation runtime is currently initialized.
*	@return	True when the scheduler runtime exists.
**/
const bool SVG_Nav2_Scheduler_IsInitialized( void ) {
    return ( bool )s_nav2_scheduler_runtime;
}

/**
*	@brief	Return a mutable pointer to the scheduler runtime.
*	@return	Scheduler runtime pointer, or `nullptr` when uninitialized.
**/
nav2_scheduler_runtime_t *SVG_Nav2_Scheduler_GetRuntime( void ) {
    return s_nav2_scheduler_runtime.get();
}

/**
*	@brief	Begin a new scheduler frame and reset budget accounting.
*	@param	frameNumber	Frame number being entered.
**/
void SVG_Nav2_Scheduler_BeginFrame( const int64_t frameNumber ) {
    /**
    *    Skip frame reset work when the scheduler foundation has not been initialized yet.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return;
    }

    /**
    *    Reset the frame-level budget accounting while preserving the current policy defaults.
    **/
    SVG_Nav2_Budget_ResetFrame( &s_nav2_scheduler_runtime->budget_runtime, frameNumber, &s_nav2_scheduler_runtime->budget_runtime.policy );

    /**
    *    Lazily stamp submitted frame indices for jobs that predate fairness-aware metadata fields.
    **/
    for ( nav2_query_job_t &job : s_nav2_scheduler_runtime->jobs ) {
        if ( job.submitted_frame_number == 0 ) {
            job.submitted_frame_number = frameNumber;
        }
    }
   /**
    *    Keep the worker interface aligned with scheduler frame boundaries so cvar-driven mode changes
    *    are observed deterministically.
    **/
    SVG_Nav2_Worker_BeginFrame( frameNumber );
}

/**
*	@brief	Service the scheduler foundation from existing svgame frame hooks.
*	@note	This applies worker completions first and then attempts one early or late bounded slice.
**/
void SVG_Nav2_Scheduler_Service( void ) {
    /**
    *    Skip service work when the scheduler runtime has not been initialized yet.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return;
    }

    /**
    *    Apply completed worker payloads first so jobs become dispatchable again before selecting the
    *    next bounded slice.
    **/
    SVG_Nav2_Worker_ServiceCompletions();

    /**
    *    Issue and dispatch one worker-preferred stage slice when available.
    **/
    const bool issuedWorkerSlice = SVG_Nav2_Scheduler_IssueSlice( true );
    if ( issuedWorkerSlice ) {
        for ( nav2_query_job_t &job : s_nav2_scheduler_runtime->jobs ) {
            if ( job.worker_in_flight ) {
                continue;
            }
            if ( job.state.active_slice.granted_ms <= 0.0 ) {
                continue;
            }
            if ( !job.state.prefer_worker_dispatch ) {
                continue;
            }

            /**
            *    Record queue-entry timestamp once a stage becomes dispatchable.
            **/
            if ( job.queue_enter_timestamp_ms == 0 ) {
                job.queue_enter_timestamp_ms = gi.GetRealTime ? gi.GetRealTime() : 0;
            }

            // Dispatch exactly one worker-preferred stage slice.
            SVG_Nav2_Worker_DispatchJobSlice( &job );
            return;
        }
    }

    /**
    *    Issue and execute one main-thread stage slice when no worker stage dispatched this frame.
    **/
    if ( !SVG_Nav2_Scheduler_IssueSlice( false ) ) {
        return;
    }

    for ( nav2_query_job_t &job : s_nav2_scheduler_runtime->jobs ) {
        if ( job.worker_in_flight ) {
            continue;
        }
        if ( job.state.active_slice.granted_ms <= 0.0 ) {
            continue;
        }
        if ( job.state.prefer_worker_dispatch ) {
            continue;
        }

        // Execute one non-worker stage slice directly on main thread.
        SVG_Nav2_Scheduler_ExecuteGrantedSlice( &job );
        return;
    }
}

/**
*	@brief	Submit a new nav2 query request into the scheduler queue.
*	@param	request	Request descriptor to enqueue.
*	@return	Assigned job identifier, or 0 on failure.
**/
uint64_t SVG_Nav2_Scheduler_SubmitRequest( const nav2_query_request_t &request ) {
    /**
    *    Ensure the scheduler runtime exists before accepting new work.
    **/
    if ( !SVG_Nav2_Scheduler_Init() ) {
        return 0;
    }

    /**
    *    Copy the request, assign a stable request identifier when the caller did not provide one,
    *    and then create the scheduler-owned job wrapper.
    **/
    nav2_query_request_t materializedRequest = request;
    if ( materializedRequest.request_id == 0 ) {
        materializedRequest.request_id = s_nav2_scheduler_runtime->next_request_id++;
    }

    /**
    *    Apply per-agent dedupe and backpressure before allocating a new job id.
    **/
    uint32_t activeJobsForAgent = 0;
    nav2_query_job_t *dedupeJob = nullptr;
    nav2_query_job_t *latestActiveJob = nullptr;
    for ( nav2_query_job_t &existingJob : s_nav2_scheduler_runtime->jobs ) {
        // Skip terminal jobs because only active lifecycle work participates in dedupe/backpressure.
        if ( SVG_Nav2_QueryJob_IsTerminal( &existingJob ) || existingJob.state.cancel_requested ) {
            continue;
        }

        // Track active requests owned by the same agent for bounded submission pressure.
        if ( existingJob.request.agent_entnum > 0 && existingJob.request.agent_entnum == materializedRequest.agent_entnum ) {
            activeJobsForAgent++;
            if ( !latestActiveJob || existingJob.job_id > latestActiveJob->job_id ) {
                latestActiveJob = &existingJob;
            }

            // Resolve the first eligible duplicate so caller can reuse existing in-flight work.
            if ( !dedupeJob && Nav2_Scheduler_IsDuplicateRequest( existingJob.request, materializedRequest ) ) {
                dedupeJob = &existingJob;
            }
        }
    }

    /**
    *    Reuse existing active work when the incoming request is effectively identical.
    **/
    if ( dedupeJob ) {
        gi.dprintf( "[NAV2][Scheduler][Submit] dedupe reuse job=%llu agent=%d req=%llu\n",
            ( unsigned long long )dedupeJob->job_id,
            materializedRequest.agent_entnum,
            ( unsigned long long )materializedRequest.request_id );
        return dedupeJob->job_id;
    }

    /**
    *    Cap per-agent queued/in-flight work to prevent bursty sound events from flooding scheduler scans.
    **/
    constexpr uint32_t maxActiveJobsPerAgent = 2;
    if ( materializedRequest.agent_entnum > 0 && activeJobsForAgent >= maxActiveJobsPerAgent ) {
        if ( latestActiveJob ) {
            gi.dprintf( "[NAV2][Scheduler][Submit] backpressure reuse job=%llu agent=%d active=%u req=%llu\n",
                ( unsigned long long )latestActiveJob->job_id,
                materializedRequest.agent_entnum,
                activeJobsForAgent,
                ( unsigned long long )materializedRequest.request_id );
            return latestActiveJob->job_id;
        }
        return 0;
    }

    const uint64_t jobId = s_nav2_scheduler_runtime->next_job_id++;

    /**
    *    Append a new default-initialized job slot and then initialize it deterministically.
    **/
    s_nav2_scheduler_runtime->jobs.emplace_back();
    SVG_Nav2_QueryJob_Init( &s_nav2_scheduler_runtime->jobs.back(), materializedRequest, jobId );
    s_nav2_scheduler_runtime->jobs.back().submitted_frame_number = Nav2_Scheduler_CurrentFrameNumber();

    /**
    *    Prime scheduler-local stage runtime cache for the new job id.
    **/
    ( void )Nav2_Scheduler_StageRuntimeForJob( jobId );
    return jobId;
}

/**
*	@brief	Issue one deterministic scheduler slice to the best currently eligible job.
*	@param	workerSlice	True when selecting against worker-slice capacity, false for main-thread capacity.
*	@return	True when a job received a slice.
*	@note	The current foundation only grants and records slices; future tasks will execute real solver work.
**/
const bool SVG_Nav2_Scheduler_IssueSlice( const bool workerSlice ) {
    /**
    *    Ensure the scheduler runtime exists before attempting to grant slices.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return false;
    }

    /**
    *    Choose the best currently eligible job using the deterministic priority and age ordering.
    **/
    const int32_t jobIndex = Nav2_Scheduler_SelectNextJobIndex( *s_nav2_scheduler_runtime );
    if ( jobIndex < 0 ) {
        return false;
    }

    /**
    *    Attempt to grant a bounded slice from the current frame budget.
    **/
    nav2_query_job_t &job = s_nav2_scheduler_runtime->jobs[ ( size_t )jobIndex ];

    /**
    *    Respect per-stage worker preference so main-thread-only stages are not dispatched through
    *    worker-oriented slice grants.
    **/
    const bool stagePrefersWorker = Nav2_Scheduler_StagePrefersWorker( job.state.stage );
    if ( workerSlice && !stagePrefersWorker ) {
        return false;
    }
    if ( !workerSlice && stagePrefersWorker ) {
        return false;
    }

    nav2_budget_slice_t grantedSlice = {};
    if ( !SVG_Nav2_Budget_GrantSlice( &s_nav2_scheduler_runtime->budget_runtime, job.state.stage, job.request.priority, workerSlice, &grantedSlice ) ) {
        return false;
    }

    /**
    *    Apply overload policy to update fairness/throttle diagnostics and optional provisional fallback
    *    behavior before this slice is executed.
    **/
    Nav2_Scheduler_ApplyOverloadPolicy( s_nav2_scheduler_runtime.get(), &job, grantedSlice );

    /**
    *    Record the granted slice on the job and bump generic progress counters so the scheduler
    *    foundation already exposes deterministic slice accounting.
    **/
    job.state.active_slice = grantedSlice;
    job.state.prefer_worker_dispatch = stagePrefersWorker;
    job.state.progress.slices_consumed++;
    job.execution_time_ms += grantedSlice.granted_ms;
    job.last_granted_frame_number = s_nav2_scheduler_runtime->budget_runtime.frame_number;

    /**
    *    Commit the granted slice directly into frame consumption for the foundation layer. Future
    *    worker integration can replace this with measured actual consumption.
    **/
    SVG_Nav2_Budget_CommitSliceConsumption( &s_nav2_scheduler_runtime->budget_runtime, grantedSlice, grantedSlice.granted_ms );
    return true;
}

/**
*	@brief	Mark a job for cancellation by identifier.
*	@param	jobId	Stable job identifier to cancel.
*	@return	True when a matching job was found and updated.
**/
const bool SVG_Nav2_Scheduler_CancelJob( const uint64_t jobId ) {
    /**
    *    Resolve the job first so cancellation remains a simple point update.
    **/
    nav2_query_job_t *job = SVG_Nav2_Scheduler_FindJob( jobId );
    if ( !job ) {
        return false;
    }

    // Forward the cancellation request into the shared job helper.
    SVG_Nav2_QueryJob_RequestCancel( job );

    /**
    *    Drop scheduler-local stage artifacts immediately because canceled jobs should not retain
    *    coarse/fine/corridor stage state.
    **/
    Nav2_Scheduler_ClearStageRuntimeForJob( jobId );
    return true;
}

/**
*	@brief	Retrieve a job by identifier.
*	@param	jobId	Stable job identifier to resolve.
*	@return	Mutable pointer to the job, or `nullptr` when not found.
**/
nav2_query_job_t *SVG_Nav2_Scheduler_FindJob( const uint64_t jobId ) {
    /**
    *    Skip lookup work when the scheduler runtime does not exist yet.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return nullptr;
    }

    /**
    *    Scan the active job vector for the requested stable identifier.
    **/
    for ( nav2_query_job_t &job : s_nav2_scheduler_runtime->jobs ) {
        if ( job.job_id == jobId ) {
            return &job;
        }
    }
    return nullptr;
}

/**
*	@brief	Erase one terminal or otherwise unneeded job from the scheduler runtime.
*	@param	jobId	Stable job identifier to erase.
*	@return	True when a matching job was found and removed.
*	@note	This keeps the scheduler's active-job scan bounded after request consumers have already
*			consumed terminal status, preventing long-lived completed failures from accumulating and
*			degrading frame time over repeated async rebuild attempts.
**/
const bool SVG_Nav2_Scheduler_RemoveJob( const uint64_t jobId ) {
    /**
    *	Require the scheduler runtime before attempting any removal work.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return false;
    }

    /**
    *	Find the concrete scheduler-owned job slot so we can erase it deterministically.
    **/
    auto &jobs = s_nav2_scheduler_runtime->jobs;
    const auto eraseIt = std::find_if( jobs.begin(), jobs.end(), [jobId]( const nav2_query_job_t &job ) {
        return job.job_id == jobId;
    } );
    if ( eraseIt == jobs.end() ) {
        return false;
    }

    /**
    *	Drop any staged per-job runtime payload first so no orphaned solver artifacts survive the erase.
    **/
    Nav2_Scheduler_ClearStageRuntimeForJob( jobId );

    /**
    *	Erase the completed/cancelled job from the active scheduler vector so future fairness scans and
    *	lookup work do not keep paying for stale terminal jobs.
    **/
    jobs.erase( eraseIt );
    return true;
}
