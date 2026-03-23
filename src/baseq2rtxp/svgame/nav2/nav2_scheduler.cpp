/********************************************************************
*
*
*	ServerGame: Nav2 Scheduler Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_scheduler.h"

#include "svgame/nav2/nav2_coarse_astar.h"
#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_corridor_build.h"
#include "svgame/nav2/nav2_fine_astar.h"
#include "svgame/nav2/nav2_goal_candidates.h"
#include "svgame/nav2/nav2_hierarchy_graph.h"
#include "svgame/nav2/nav2_mover_local_nav.h"
#include "svgame/nav2/nav2_postprocess.h"
#include "svgame/nav2/nav2_region_layers.h"
#include "svgame/nav2/nav2_snapshot.h"
#include "svgame/nav2/nav2_span_grid_build.h"
#include "svgame/nav2/nav2_worker_iface.h"

#include <algorithm>
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
};

//! Scheduler-local per-job stage runtime cache for Task 11.1 staged execution.
static std::unordered_map<uint64_t, nav2_scheduler_job_stage_runtime_t> s_nav2_scheduler_job_stage_runtime = {};


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
    *    Assign terminal status and move stage to completed while clearing any active slice.
    **/
    job->state.result_status = status;
    Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::Completed );
    job->state.active_slice = nav2_budget_slice_t{};
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
    *    Resolve active nav2 mesh and canonical agent bounds used by candidate generation.
    **/
    const nav2_query_mesh_t *queryMesh = SVG_Nav2_GetQueryMesh();
    if ( !queryMesh || !queryMesh->main_mesh ) {
        return false;
    }
    const nav2_query_agent_profile_t agentProfile = SVG_Nav2_BuildAgentProfileFromCvars();

    /**
    *    Build candidate sets for both start and goal endpoints using the same BSP-aware stage helper.
    **/
    const bool haveStartCandidates = SVG_Nav2_BuildGoalCandidates( queryMesh->main_mesh,
        job->request.start_origin,
        job->request.start_origin,
        agentProfile.mins,
        agentProfile.maxs,
        &stageRuntime->start_candidates );
    const bool haveGoalCandidates = SVG_Nav2_BuildGoalCandidates( queryMesh->main_mesh,
        job->request.start_origin,
        job->request.goal_origin,
        agentProfile.mins,
        agentProfile.maxs,
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
    *    Build span-grid snapshot used by connector and region-layer extraction.
    **/
    nav2_span_grid_build_stats_t spanBuildStats = {};
    if ( !SVG_Nav2_BuildSpanGridFromMesh( SVG_Nav2_Runtime_GetMesh(), &stageRuntime->span_grid, &spanBuildStats ) ) {
        return false;
    }

    /**
    *    Extract connectors from span-grid data; use empty entity set for deterministic scheduler baseline.
    **/
    const std::vector<svg_base_edict_t *> noEntities = {};
    if ( !SVG_Nav2_ExtractConnectors( stageRuntime->span_grid, noEntities, &stageRuntime->connectors ) ) {
        return false;
    }

    /**
    *    Build region-layer and hierarchy graph assets required by coarse-stage solver state.
    **/
    if ( !SVG_Nav2_BuildRegionLayers( stageRuntime->span_grid, stageRuntime->connectors, &stageRuntime->region_layers, nullptr ) ) {
        return false;
    }
    if ( !SVG_Nav2_BuildHierarchyGraph( stageRuntime->region_layers, &stageRuntime->hierarchy_graph, nullptr ) ) {
        return false;
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
        job->state.restart_requested = false;
        Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::TopologyClassification );
    }

    /**
    *    Execute stage-specific handler and transition state based on outcomes.
    **/
    switch ( job->state.stage ) {
        case nav2_query_stage_t::TopologyClassification:
        case nav2_query_stage_t::CandidateGeneration:
        case nav2_query_stage_t::CandidateRanking: {
            if ( !Nav2_Scheduler_BuildAndSelectCandidates( job, stageRuntime ) ) {
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                return false;
            }
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::CoarseSearch );
            return true;
        }

        case nav2_query_stage_t::CoarseSearch: {
            if ( !stageRuntime->coarse_state.hierarchy_graph_bound ) {
                if ( !Nav2_Scheduler_BuildHierarchyDependencies( stageRuntime ) ) {
                    Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                    return false;
                }
                if ( !SVG_Nav2_CoarseAStar_Init( &stageRuntime->coarse_state,
                    stageRuntime->hierarchy_graph,
                    stageRuntime->selected_start_candidate,
                    stageRuntime->selected_goal_candidate,
                    job->job_id ) ) {
                    Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                    return false;
                }
                stageRuntime->coarse_state.query_state.snapshot = job->state.snapshot;
            }

            if ( !SVG_Nav2_CoarseAStar_Step( &stageRuntime->coarse_state, job->state.active_slice, &stageRuntime->coarse_result ) ) {
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                return false;
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

            Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
            return false;
        }

        case nav2_query_stage_t::CorridorExtraction: {
            if ( !SVG_Nav2_BuildCorridorFromCoarseAStar( stageRuntime->coarse_state, &stageRuntime->corridor ) ) {
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                return false;
            }
            job->state.has_provisional_result = true;
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::FineRefinement );
            return true;
        }

        case nav2_query_stage_t::FineRefinement: {
            if ( stageRuntime->fine_state.status == nav2_fine_astar_status_t::None ) {
                if ( !SVG_Nav2_FineAStar_Init( &stageRuntime->fine_state, stageRuntime->corridor, job->job_id ) ) {
                    Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                    return false;
                }
                stageRuntime->fine_state.query_state.snapshot = job->state.snapshot;
            }

            if ( !SVG_Nav2_FineAStar_Step( &stageRuntime->fine_state, job->state.active_slice, &stageRuntime->fine_result ) ) {
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                return false;
            }
            job->state.progress.fine_expansions = stageRuntime->fine_state.query_state.progress.fine_expansions;

            if ( stageRuntime->fine_result.status == nav2_fine_astar_status_t::Success ) {
                Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::MoverLocalRefinement );
                return true;
            }
            if ( stageRuntime->fine_result.status == nav2_fine_astar_status_t::Partial
                || stageRuntime->fine_result.status == nav2_fine_astar_status_t::Running ) {
                job->state.result_status = nav2_query_result_status_t::Partial;
                return true;
            }

            Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
            return false;
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
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                return false;
            }
            job->state.progress.mover_expansions = stageRuntime->mover_refine_result.diagnostics.inspected_nodes;
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::PostProcess );
            return true;
        }

        case nav2_query_stage_t::PostProcess: {
            nav2_postprocess_policy_t postPolicy = {};
            if ( !SVG_Nav2_Postprocess_FinePath( SVG_Nav2_GetQueryMesh(), stageRuntime->fine_state, postPolicy, &stageRuntime->postprocess_result ) ) {
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                return false;
            }
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::Revalidation );
            return true;
        }

        case nav2_query_stage_t::Revalidation: {
            if ( !s_nav2_scheduler_runtime ) {
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
                return false;
            }

            const nav2_snapshot_staleness_t staleness = SVG_Nav2_Snapshot_Compare(
                &s_nav2_scheduler_runtime->snapshot_runtime,
                job->state.snapshot );
            if ( staleness.requires_resubmit ) {
                Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Stale );
                return false;
            }

            job->state.requires_revalidation = staleness.requires_revalidation;
            Nav2_Scheduler_SetJobStage( &job->state, nav2_query_stage_t::Commit );
            return true;
        }

        case nav2_query_stage_t::Commit: {
            job->state.has_provisional_result = true;
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

    Nav2_Scheduler_SetTerminalStatus( job, nav2_query_result_status_t::Failed );
    return false;
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
    const bool executed = Nav2_Scheduler_ExecuteStage( job, &stageRuntime );
    job->state.progress.slices_consumed++;
    job->state.active_slice = nav2_budget_slice_t{};

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
static int32_t Nav2_Scheduler_SelectNextJobIndex( const nav2_scheduler_runtime_t &runtime ) {
    int32_t bestIndex = -1;
    int32_t bestPriorityWeight = std::numeric_limits<int32_t>::min();
    uint64_t bestJobId = std::numeric_limits<uint64_t>::max();

    /**
    *    Scan the active job list and select the highest-priority eligible job using a stable tie-break.
    **/
    for ( size_t i = 0; i < runtime.jobs.size(); i++ ) {
        const nav2_query_job_t &job = runtime.jobs[ i ];
        if ( !Nav2_Scheduler_IsSliceEligible( job ) ) {
            continue;
        }

        const int32_t priorityWeight = Nav2_Scheduler_PriorityWeight( job.request.priority );
        if ( priorityWeight > bestPriorityWeight ) {
            bestPriorityWeight = priorityWeight;
            bestJobId = job.job_id;
            bestIndex = ( int32_t )i;
            continue;
        }

        if ( priorityWeight == bestPriorityWeight && job.job_id < bestJobId ) {
            bestJobId = job.job_id;
            bestIndex = ( int32_t )i;
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
    const uint64_t jobId = s_nav2_scheduler_runtime->next_job_id++;

    /**
    *    Append a new default-initialized job slot and then initialize it deterministically.
    **/
    s_nav2_scheduler_runtime->jobs.emplace_back();
    SVG_Nav2_QueryJob_Init( &s_nav2_scheduler_runtime->jobs.back(), materializedRequest, jobId );

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
    *    Record the granted slice on the job and bump generic progress counters so the scheduler
    *    foundation already exposes deterministic slice accounting.
    **/
    job.state.active_slice = grantedSlice;
    job.state.prefer_worker_dispatch = stagePrefersWorker;
    job.state.progress.slices_consumed++;
    job.execution_time_ms += grantedSlice.granted_ms;

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
