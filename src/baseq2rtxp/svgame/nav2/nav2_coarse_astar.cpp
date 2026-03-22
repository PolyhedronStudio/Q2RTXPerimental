/********************************************************************
*
*
*	ServerGame: Nav2 Coarse A* Solver - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_coarse_astar.h"

#include <algorithm>
#include <cmath>
#include <limits>


/**
*
*
*	Nav2 Coarse A* Local Helpers:
*
*
**/
/**
* @brief Allocate the next stable coarse-node id in a solver state.
* @param state Solver state to inspect.
* @return Stable node id for the next appended frontier node.
**/
static int32_t SVG_Nav2_AllocateCoarseNodeId( const nav2_coarse_astar_state_t &state ) {
    // Keep node ids monotonic so reconstruction remains deterministic.
    int32_t nextId = 1;
    for ( const nav2_coarse_astar_node_t &node : state.nodes ) {
        nextId = std::max( nextId, node.node_id + 1 );
    }
    return nextId;
}

/**
* @brief Allocate the next stable coarse-edge id in a solver state.
* @param state Solver state to inspect.
* @return Stable edge id for the next appended frontier edge.
**/
static int32_t SVG_Nav2_AllocateCoarseEdgeId( const nav2_coarse_astar_state_t &state ) {
    // Keep edge ids monotonic so debug output and path reconstruction remain stable.
    int32_t nextId = 1;
    for ( const nav2_coarse_astar_edge_t &edge : state.edges ) {
        nextId = std::max( nextId, edge.edge_id + 1 );
    }
    return nextId;
}

/**
* @brief Return the hierarchy node id for one candidate when available.
* @param candidate Candidate to inspect.
* @return Hierarchy-node id, or `-1` when no stable id is available.
**/
static int32_t SVG_Nav2_CandidateHierarchyNodeId( const nav2_goal_candidate_t &candidate ) {
    // The candidate type already stores stable topology ids; keep the mapping explicit.
    if ( candidate.layer_index >= 0 ) {
        return candidate.layer_index;
    }
    // Fall back to the canonical tile identifier when the candidate does not expose a dedicated hierarchy id.
    if ( candidate.tile_id >= 0 ) {
        return candidate.tile_id;
    }
    return candidate.candidate_id;
}

/**
* @brief Classify a candidate into a coarse node kind.
* @param candidate Candidate to inspect.
* @return Stable coarse node kind.
**/
static nav2_coarse_astar_node_kind_t SVG_Nav2_ClassifyCandidateNodeKind( const nav2_goal_candidate_t &candidate ) {
    // Preserve vertical intent by mapping semantic candidate types into the matching coarse node kinds.
    switch ( candidate.type ) {
        case nav2_goal_candidate_type_t::PortalEndpoint:
            return nav2_coarse_astar_node_kind_t::Portal;
        case nav2_goal_candidate_type_t::LadderEndpoint:
            return nav2_coarse_astar_node_kind_t::Ladder;
        case nav2_goal_candidate_type_t::StairBand:
            return nav2_coarse_astar_node_kind_t::Stair;
        case nav2_goal_candidate_type_t::MoverBoarding:
            return nav2_coarse_astar_node_kind_t::MoverBoarding;
        case nav2_goal_candidate_type_t::MoverExit:
            return nav2_coarse_astar_node_kind_t::MoverExit;
        case nav2_goal_candidate_type_t::RawGoal:
        case nav2_goal_candidate_type_t::ProjectedSameCell:
        case nav2_goal_candidate_type_t::NearbySupport:
        case nav2_goal_candidate_type_t::SameLeafHint:
        case nav2_goal_candidate_type_t::SameClusterHint:
        case nav2_goal_candidate_type_t::LegacyProjectedFallback:
            return nav2_coarse_astar_node_kind_t::RegionLayer;
        default:
            return nav2_coarse_astar_node_kind_t::Fallback;
    }
}

/**
* @brief Create a coarse node from a candidate.
* @param candidate Candidate to convert.
* @param nodeId Stable node id to assign.
* @return Coarse-node payload.
**/
static nav2_coarse_astar_node_t SVG_Nav2_MakeNodeFromCandidate( const nav2_goal_candidate_t &candidate, const int32_t nodeId ) {
    // Mirror the candidate into a stable coarse-search node.
    nav2_coarse_astar_node_t node = {};
    node.node_id = nodeId;
    node.kind = SVG_Nav2_ClassifyCandidateNodeKind( candidate );
    node.hierarchy_node_id = SVG_Nav2_CandidateHierarchyNodeId( candidate );
    node.connector_id = candidate.tile_id;
    node.region_layer_id = candidate.layer_index;
    node.inline_model_index = candidate.layer_index;
    node.candidate_id = candidate.candidate_id;
    node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_ALLOWED_Z_BAND;
    node.allowed_z_band.min_z = candidate.center_origin.z - 32.0;
    node.allowed_z_band.max_z = candidate.center_origin.z + 32.0;
    node.allowed_z_band.preferred_z = candidate.center_origin.z;
    node.topology.leaf_index = candidate.leaf_index;
    node.topology.cluster_id = candidate.cluster_id;
    node.topology.area_id = candidate.area_id;
    node.topology.region_id = candidate.layer_index;
    node.topology.portal_id = candidate.tile_id;
    if ( candidate.flags & NAV_GOAL_CANDIDATE_FLAG_SAME_LEAF_AS_START ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_PREFERRED;
    }
    if ( candidate.flags & NAV_GOAL_CANDIDATE_FLAG_IN_FAT_PVS ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_PARTIAL;
    }
    if ( candidate.flags & NAV_GOAL_CANDIDATE_FLAG_HAS_LADDER_BITS ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_LADDER;
    }
    if ( candidate.flags & NAV_GOAL_CANDIDATE_FLAG_HAS_STAIR_BITS ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_STAIR;
    }
    if ( candidate.flags & NAV_GOAL_CANDIDATE_FLAG_SAME_CLUSTER_AS_START ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_REGION_LAYER;
    }
    if ( candidate.flags & NAV_GOAL_CANDIDATE_FLAG_SAME_LEAF_AS_START ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_REGION_LAYER;
    }
    return node;
}

/**
* @brief Convert a hierarchy node into a coarse frontier node.
* @param hierarchyNode Hierarchy source node.
* @param nodeId Stable node id to assign.
* @param kind Semantic kind to assign.
* @return Coarse-node payload.
**/
static nav2_coarse_astar_node_t SVG_Nav2_MakeNodeFromHierarchyNode( const nav2_hierarchy_node_t &hierarchyNode, const int32_t nodeId, const nav2_coarse_astar_node_kind_t kind ) {
    // Copy the hierarchy node into the coarse solver so the frontier can reason about the node with stable ids.
    nav2_coarse_astar_node_t node = {};
    node.node_id = nodeId;
    node.kind = kind;
    node.hierarchy_node_id = hierarchyNode.node_id;
    node.region_layer_id = hierarchyNode.region_layer_id;
    node.connector_id = hierarchyNode.connector_id;
    node.mover_entnum = hierarchyNode.mover_entnum;
    node.inline_model_index = hierarchyNode.inline_model_index;
    node.allowed_z_band = hierarchyNode.allowed_z_band;
    node.topology = hierarchyNode.topology;
    node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_ALLOWED_Z_BAND;
    if ( hierarchyNode.flags & NAV2_HIERARCHY_NODE_FLAG_HAS_REGION_LAYER ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_REGION_LAYER;
    }
    if ( hierarchyNode.flags & NAV2_HIERARCHY_NODE_FLAG_HAS_PORTAL ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_PORTAL;
    }
    if ( hierarchyNode.flags & NAV2_HIERARCHY_NODE_FLAG_HAS_LADDER ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_LADDER;
    }
    if ( hierarchyNode.flags & NAV2_HIERARCHY_NODE_FLAG_HAS_STAIR ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_STAIR;
    }
    if ( hierarchyNode.flags & NAV2_HIERARCHY_NODE_FLAG_HAS_MOVER ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_HAS_MOVER;
    }
    if ( hierarchyNode.flags & NAV2_HIERARCHY_NODE_FLAG_PARTIAL ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_PARTIAL;
    }
    if ( hierarchyNode.flags & NAV2_HIERARCHY_NODE_FLAG_FAT_PVS_RELEVANT ) {
        node.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_PREFERRED;
    }
    return node;
}

/**
* @brief Create a stable coarse edge between two nodes from a hierarchy edge.
* @param fromNode Source coarse node.
* @param toNode Destination coarse node.
* @param hierarchyEdge Hierarchy edge used as the source semantic cue.
* @param edgeId Stable edge id to assign.
* @return Coarse-edge payload.
**/
static nav2_coarse_astar_edge_t SVG_Nav2_MakeEdgeFromHierarchyEdge( const nav2_coarse_astar_node_t &fromNode, const nav2_coarse_astar_node_t &toNode, const nav2_hierarchy_edge_t &hierarchyEdge, const int32_t edgeId ) {
    // Translate the hierarchy edge into a solver edge while preserving route commitments.
    nav2_coarse_astar_edge_t edge = {};
    edge.edge_id = edgeId;
    edge.from_node_id = fromNode.node_id;
    edge.to_node_id = toNode.node_id;
    edge.connector_id = hierarchyEdge.connector_id;
    edge.region_layer_id = hierarchyEdge.region_layer_id;
    edge.mover_ref = hierarchyEdge.mover_ref;
    edge.allowed_z_band = hierarchyEdge.allowed_z_band;
    edge.base_cost = hierarchyEdge.base_cost;
    edge.topology_penalty = hierarchyEdge.topology_penalty;

    switch ( hierarchyEdge.kind ) {
        case nav2_hierarchy_edge_kind_t::PortalLink:
            edge.kind = nav2_coarse_astar_edge_kind_t::PortalLink;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_PORTAL;
            break;
        case nav2_hierarchy_edge_kind_t::MoverBoarding:
            edge.kind = nav2_coarse_astar_edge_kind_t::MoverBoarding;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_MOVER | NAV2_COARSE_ASTAR_EDGE_FLAG_ALLOWS_BOARDING;
            break;
        case nav2_hierarchy_edge_kind_t::MoverRide:
            edge.kind = nav2_coarse_astar_edge_kind_t::MoverRide;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_MOVER | NAV2_COARSE_ASTAR_EDGE_FLAG_ALLOWS_RIDE;
            break;
        case nav2_hierarchy_edge_kind_t::MoverExit:
            edge.kind = nav2_coarse_astar_edge_kind_t::MoverExit;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_MOVER | NAV2_COARSE_ASTAR_EDGE_FLAG_ALLOWS_EXIT;
            break;
        case nav2_hierarchy_edge_kind_t::LadderLink:
            edge.kind = nav2_coarse_astar_edge_kind_t::LadderLink;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_LADDER;
            break;
        case nav2_hierarchy_edge_kind_t::StairLink:
            edge.kind = nav2_coarse_astar_edge_kind_t::StairLink;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_STAIR;
            break;
        case nav2_hierarchy_edge_kind_t::RegionLink:
            edge.kind = nav2_coarse_astar_edge_kind_t::RegionLink;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_PASSABLE | NAV2_COARSE_ASTAR_EDGE_FLAG_TOPOLOGY_MATCHED;
            break;
        default:
            edge.kind = nav2_coarse_astar_edge_kind_t::FallbackLink;
            edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_PARTIAL;
            break;
    }

    // Carry allowed Z-band information forward so the fine stage can stay corridor-aware.
    if ( edge.allowed_z_band.IsValid() ) {
        edge.flags |= NAV2_COARSE_ASTAR_EDGE_FLAG_PASSABLE;
    }
    return edge;
}

/**
* @brief Compute a cheap heuristic between two coarse nodes.
* @param fromNode Source node.
* @param toNode Destination node.
* @return Heuristic cost estimate.
**/
static double SVG_Nav2_Heuristic( const nav2_coarse_astar_node_t &fromNode, const nav2_coarse_astar_node_t &toNode ) {
    // Use the allowed Z-band and topology data to produce a stable lower bound.
    const double dz = std::fabs( fromNode.allowed_z_band.preferred_z - toNode.allowed_z_band.preferred_z );
    const double topologyPenalty = ( fromNode.region_layer_id >= 0 && toNode.region_layer_id >= 0 && fromNode.region_layer_id != toNode.region_layer_id ) ? 16.0 : 0.0;
    const double moverPenalty = ( fromNode.mover_entnum >= 0 || toNode.mover_entnum >= 0 ) ? 4.0 : 0.0;
    return dz + topologyPenalty + moverPenalty;
}

/**
* @brief Find the lowest-f-score node index in the frontier.
* @param state Solver state to inspect.
* @return Frontier index, or `-1` when empty.
**/
static int32_t SVG_Nav2_FindBestFrontierNodeIndex( const nav2_coarse_astar_state_t &state ) {
    // Scan the frontier linearly because the initial solver version prioritizes simplicity and determinism.
    if ( state.nodes.empty() ) {
        return -1;
    }

    double bestScore = std::numeric_limits<double>::infinity();
    int32_t bestIndex = -1;
    for ( size_t i = 0; i < state.nodes.size(); ++i ) {
        const nav2_coarse_astar_node_t &node = state.nodes[ i ];
        if ( node.f_score < bestScore ) {
            bestScore = node.f_score;
            bestIndex = ( int32_t )i;
        }
    }
    return bestIndex;
}

/**
* @brief Reconstruct a stable path from a terminal node.
* @param state Solver state to inspect.
* @param terminalNodeId Terminal node id to unwind.
* @param out_path [out] Path buffer receiving reconstructed node and edge ids.
**/
static void SVG_Nav2_ReconstructPath( const nav2_coarse_astar_state_t &state, const int32_t terminalNodeId, nav2_coarse_astar_path_t *out_path ) {
    // Rebuild the result path from goal to start using the stored parent links.
    if ( !out_path ) {
        return;
    }
    out_path->node_ids.clear();
    out_path->edge_ids.clear();

    int32_t currentNodeId = terminalNodeId;
    while ( currentNodeId >= 0 ) {
        const nav2_coarse_astar_node_t *currentNode = nullptr;
        for ( const nav2_coarse_astar_node_t &node : state.nodes ) {
            if ( node.node_id == currentNodeId ) {
                currentNode = &node;
                break;
            }
        }
        if ( !currentNode ) {
            break;
        }

        // Push the node and its incoming edge before continuing with the parent chain.
        out_path->node_ids.push_back( currentNode->node_id );
        if ( currentNode->parent_edge_id >= 0 ) {
            out_path->edge_ids.push_back( currentNode->parent_edge_id );
        }
        currentNodeId = currentNode->parent_node_id;
    }

    // Reverse the path so callers get start-to-goal order.
    std::reverse( out_path->node_ids.begin(), out_path->node_ids.end() );
    std::reverse( out_path->edge_ids.begin(), out_path->edge_ids.end() );
}

/**
* @brief Apply budget accounting and prune stale or invalid nodes before expanding.
* @param state Solver state to mutate.
* @param node Node selected for expansion.
* @return True when the node should be expanded.
**/
static const bool SVG_Nav2_ShouldExpandNode( nav2_coarse_astar_state_t *state, const nav2_coarse_astar_node_t &node ) {
    // Reject nodes that have already been rendered stale or cancelled by the scheduler.
    if ( state->query_state.cancel_requested || state->query_state.restart_requested ) {
        state->status = nav2_coarse_astar_status_t::Cancelled;
        return false;
    }
    if ( state->query_state.snapshot.static_nav_version == 0 ) {
        state->diagnostics.stale_prunes++;
        state->status = nav2_coarse_astar_status_t::Stale;
        return false;
    }
    if ( ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_HAS_MOVER ) != 0 && state->query_state.snapshot.mover_version == 0 ) {
        state->diagnostics.mover_prunes++;
        return false;
    }
    if ( ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_PARTIAL ) != 0 ) {
        state->diagnostics.policy_prunes++;
        return false;
    }
    return true;
}


/**
*
*
*	Nav2 Coarse A* Public API:
*
*
**/
/**
* @brief Reset a coarse A* solver state.
* @param state [out] Solver state to reset.
**/
void SVG_Nav2_CoarseAStar_Reset( nav2_coarse_astar_state_t *state ) {
    // Guard null callers so the helper remains easy to use.
    if ( !state ) {
        return;
    }
    *state = {};
    state->status = nav2_coarse_astar_status_t::None;
    SVG_Nav2_QueryState_Reset( &state->query_state );
}

/**
* @brief Initialize a coarse A* solver from a hierarchy graph and goal candidates.
* @param state [out] Solver state to initialize.
* @param hierarchyGraph Hierarchy graph to search.
* @param startCandidate Selected start candidate.
* @param goalCandidate Selected goal candidate.
* @param solverId Stable solver identifier.
* @return True when initialization succeeded.
**/
const bool SVG_Nav2_CoarseAStar_Init( nav2_coarse_astar_state_t *state, const nav2_hierarchy_graph_t &hierarchyGraph,
    const nav2_goal_candidate_t &startCandidate, const nav2_goal_candidate_t &goalCandidate, const uint64_t solverId ) {
    // Validate output storage before building frontier state.
    if ( !state || hierarchyGraph.nodes.empty() ) {
        return false;
    }

    // Reset the solver and bind the query context.
    SVG_Nav2_CoarseAStar_Reset( state );
    state->solver_id = solverId;
    state->status = nav2_coarse_astar_status_t::Running;
    state->start_candidate = startCandidate;
    state->goal_candidate = goalCandidate;
    state->query_state.bench_scenario = startCandidate.flags & NAV_GOAL_CANDIDATE_FLAG_NEIGHBOR_SAMPLE ? nav2_bench_scenario_t::SameFloorOpen : nav2_bench_scenario_t::SameFloorOpen;
    state->query_state.stage = nav2_query_stage_t::CoarseSearch;
    state->query_state.result_status = nav2_query_result_status_t::Pending;

    // Seed the frontier with the start and goal candidates so later slices can grow outward.
    const int32_t startNodeId = SVG_Nav2_AllocateCoarseNodeId( *state );
    const int32_t goalNodeId = startNodeId + 1;
    nav2_coarse_astar_node_t startNode = SVG_Nav2_MakeNodeFromCandidate( startCandidate, startNodeId );
    startNode.kind = nav2_coarse_astar_node_kind_t::Start;
    startNode.g_score = 0.0;
    startNode.h_score = SVG_Nav2_Heuristic( startNode, SVG_Nav2_MakeNodeFromCandidate( goalCandidate, goalNodeId ) );
    startNode.f_score = startNode.h_score;
    state->nodes.push_back( startNode );

    nav2_coarse_astar_node_t goalNode = SVG_Nav2_MakeNodeFromCandidate( goalCandidate, goalNodeId );
    goalNode.kind = nav2_coarse_astar_node_kind_t::Goal;
    goalNode.g_score = 0.0;
    goalNode.h_score = 0.0;
    goalNode.f_score = 0.0;
    state->nodes.push_back( goalNode );

    // Mark the solver as owning a frontier slice so later service calls can resume deterministically.
    state->has_frontier_slice = true;
    return true;
}

/**
* @brief Advance a coarse A* solver by one budgeted slice.
* @param state Solver state to advance.
* @param budgetSlice Budget slice controlling this step.
* @param out_result [out] Optional result snapshot.
* @return True when the solver made progress or completed.
**/
const bool SVG_Nav2_CoarseAStar_Step( nav2_coarse_astar_state_t *state, const nav2_budget_slice_t &budgetSlice, nav2_coarse_astar_result_t *out_result ) {
    // Require a live solver with at least one frontier node.
    if ( !state || state->status != nav2_coarse_astar_status_t::Running || state->nodes.empty() ) {
        return false;
    }

    // Record the budget slice on the query state so the scheduler can resume deterministically.
    state->query_state.active_slice = budgetSlice;
    state->query_state.progress.slices_consumed++;
    state->diagnostics.slices_consumed++;
    state->diagnostics.resumes++;
    state->query_state.progress.coarse_expansions = state->diagnostics.expansions;

    // Respect cancellation and restart requests before doing any new work.
    if ( state->query_state.cancel_requested ) {
        state->status = nav2_coarse_astar_status_t::Cancelled;
        state->query_state.result_status = nav2_query_result_status_t::Cancelled;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }
    if ( state->query_state.restart_requested ) {
        state->diagnostics.pauses++;
        state->status = nav2_coarse_astar_status_t::Partial;
        state->query_state.result_status = nav2_query_result_status_t::Partial;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    // Pull the best frontier node and evaluate whether it is still worth expanding.
    const int32_t bestIndex = SVG_Nav2_FindBestFrontierNodeIndex( *state );
    if ( bestIndex < 0 ) {
        state->status = nav2_coarse_astar_status_t::Failed;
        state->query_state.result_status = nav2_query_result_status_t::Failed;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    const nav2_coarse_astar_node_t currentNode = state->nodes[ ( size_t )bestIndex ];
    if ( !SVG_Nav2_ShouldExpandNode( state, currentNode ) ) {
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    // If the best node already corresponds to the goal, reconstruct and stop.
    if ( currentNode.kind == nav2_coarse_astar_node_kind_t::Goal || currentNode.hierarchy_node_id == state->goal_candidate.candidate_id ) {
        SVG_Nav2_ReconstructPath( *state, currentNode.node_id, &state->path );
        state->status = nav2_coarse_astar_status_t::Success;
        state->query_state.result_status = nav2_query_result_status_t::Success;
        state->query_state.has_provisional_result = true;
        state->query_state.requires_revalidation = true;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->has_path = true;
            out_result->path = state->path;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    // Expand the current node to its hierarchy neighbors using the coarse graph edges.
    const nav2_hierarchy_graph_t *hierarchyGraph = nullptr;
    // The solver stores node ids only, so the caller is expected to rebind the graph through the query state in later tasks.
    // For now, the search keeps a synthetic fallback expansion so the resumable frontier is functional.
    if ( hierarchyGraph == nullptr ) {
        // Create a direct fallback move toward the goal so the solver can still make bounded progress.
        nav2_coarse_astar_node_t nextNode = state->nodes.back();
        nextNode.node_id = SVG_Nav2_AllocateCoarseNodeId( *state );
        nextNode.parent_node_id = currentNode.node_id;
        nextNode.parent_edge_id = SVG_Nav2_AllocateCoarseEdgeId( *state );
        nextNode.g_score = currentNode.g_score + 1.0;
        nextNode.h_score = SVG_Nav2_Heuristic( nextNode, state->nodes.back() );
        nextNode.f_score = nextNode.g_score + nextNode.h_score;
        nextNode.kind = nav2_coarse_astar_node_kind_t::Fallback;
        nextNode.flags |= NAV2_COARSE_ASTAR_NODE_FLAG_PARTIAL;
        state->nodes.push_back( nextNode );
        state->edges.push_back( SVG_Nav2_MakeEdgeFromHierarchyEdge( currentNode, nextNode, nav2_hierarchy_edge_t{}, nextNode.parent_edge_id ) );
        state->diagnostics.expansions++;
        state->status = nav2_coarse_astar_status_t::Partial;
        state->query_state.result_status = nav2_query_result_status_t::Partial;
        state->query_state.has_provisional_result = true;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->has_path = !state->path.node_ids.empty();
            out_result->path = state->path;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    // Future hierarchy-backed expansion path: retained as a simple fallback until the scheduler wires a live graph pointer into the query state.
    state->diagnostics.expansions++;
    state->status = nav2_coarse_astar_status_t::Partial;
    state->query_state.result_status = nav2_query_result_status_t::Partial;
    state->query_state.has_provisional_result = true;
    if ( out_result ) {
        *out_result = {};
        out_result->status = state->status;
        out_result->diagnostics = state->diagnostics;
    }
    return true;
}

/**
* @brief Return whether a coarse A* solver has reached a terminal state.
* @param state Solver state to inspect.
* @return True when the solver is done, cancelled, failed, or stale.
**/
const bool SVG_Nav2_CoarseAStar_IsTerminal( const nav2_coarse_astar_state_t *state ) {
    // Terminal states are represented directly in the solver status.
    if ( !state ) {
        return true;
    }
    switch ( state->status ) {
        case nav2_coarse_astar_status_t::Success:
        case nav2_coarse_astar_status_t::Failed:
        case nav2_coarse_astar_status_t::Cancelled:
        case nav2_coarse_astar_status_t::Stale:
            return true;
        default:
            return false;
    }
}

/**
* @brief Emit a bounded debug summary for the coarse A* state.
* @param state Solver state to report.
* @param limit Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintCoarseAStar( const nav2_coarse_astar_state_t &state, const int32_t limit ) {
    // Skip diagnostics when nothing useful could be printed.
    if ( limit <= 0 ) {
        return;
    }

    const int32_t reportNodeCount = std::min( ( int32_t )state.nodes.size(), limit );
    gi.dprintf( "[NAV2][CoarseAStar] status=%d solver=%llu nodes=%d edges=%d report=%d exp=%u slices=%u pauses=%u resumes=%u stale=%u policy=%u mover=%u hard=%u pathNodes=%d pathEdges=%d\n",
        ( int32_t )state.status,
        ( unsigned long long )state.solver_id,
        ( int32_t )state.nodes.size(),
        ( int32_t )state.edges.size(),
        reportNodeCount,
        state.diagnostics.expansions,
        state.diagnostics.slices_consumed,
        state.diagnostics.pauses,
        state.diagnostics.resumes,
        state.diagnostics.stale_prunes,
        state.diagnostics.policy_prunes,
        state.diagnostics.mover_prunes,
        state.diagnostics.hard_invalid_prunes,
        ( int32_t )state.path.node_ids.size(),
        ( int32_t )state.path.edge_ids.size() );
    for ( int32_t i = 0; i < reportNodeCount; ++i ) {
        const nav2_coarse_astar_node_t &node = state.nodes[ ( size_t )i ];
        gi.dprintf( "[NAV2][CoarseAStar] node=%d kind=%d hier=%d region=%d conn=%d mover=%d model=%d cand=%d flags=0x%08x z=(%.1f..%.1f) g=%.1f h=%.1f f=%.1f parent=%d edge=%d\n",
            node.node_id,
            ( int32_t )node.kind,
            node.hierarchy_node_id,
            node.region_layer_id,
            node.connector_id,
            node.mover_entnum,
            node.inline_model_index,
            node.candidate_id,
            node.flags,
            node.allowed_z_band.min_z,
            node.allowed_z_band.max_z,
            node.g_score,
            node.h_score,
            node.f_score,
            node.parent_node_id,
            node.parent_edge_id );
    }
}

/**
* @brief Keep the nav2 coarse A* module represented in the build.
**/
void SVG_Nav2_CoarseAStar_ModuleAnchor( void ) {
}
