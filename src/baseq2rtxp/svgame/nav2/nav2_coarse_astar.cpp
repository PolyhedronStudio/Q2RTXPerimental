/********************************************************************
*
*
*	ServerGame: Nav2 Coarse A* Solver - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_coarse_astar.h"

// Movement constants used to bound vertical transitions in the coarse solver.
#include "sharedgame/pmove/sg_pmove.h"

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
*	@brief	Validate the coarse A* solver state.
*	@param	state	Solver state to inspect.
*	@return	True when the solver state is internally consistent.
**/
const bool SVG_Nav2_CoarseAStar_ValidateState( const nav2_coarse_astar_state_t *state ) {
    /**
    *	Sanity-check the input pointer before validating state.
    **/
    // Return false when the state pointer is missing.
    if ( !state ) {
        return false;
    }

    /**
    *	Ensure every frontier node has a stable id and valid Z band.
    **/
    // Iterate over all frontier nodes.
    for ( size_t nodeIndex = 0; nodeIndex < state->nodes.size(); ++nodeIndex ) {
        // Cache the node being inspected.
        const nav2_coarse_astar_node_t &node = state->nodes[ nodeIndex ];
        // Reject nodes without valid ids.
        if ( node.node_id < 0 ) {
            return false;
        }
        // Reject nodes without a valid Z band.
        if ( !node.allowed_z_band.IsValid() ) {
            return false;
        }
        // Validate the parent id if present.
        if ( node.parent_node_id >= 0 && node.parent_node_id == node.node_id ) {
            return false;
        }
    }

    /**
    *	Ensure every frontier edge has stable ids and valid Z bands.
    **/
    // Iterate over all frontier edges.
    for ( size_t edgeIndex = 0; edgeIndex < state->edges.size(); ++edgeIndex ) {
        // Cache the edge being inspected.
        const nav2_coarse_astar_edge_t &edge = state->edges[ edgeIndex ];
        // Reject edges without valid ids.
        if ( edge.edge_id < 0 ) {
            return false;
        }
        // Reject edges without a valid Z band.
        if ( !edge.allowed_z_band.IsValid() ) {
            return false;
        }
    }

    // State appears internally consistent.
    return true;
}

/**
* @brief Return the hierarchy node id for one candidate when available.
* @param candidate Candidate to inspect.
* @return Hierarchy-node id, or `-1` when no stable id is available.
**/
static int32_t SVG_Nav2_CandidateHierarchyNodeId( const nav2_goal_candidate_t &candidate ) {
    /**
    *	Prefer a direct hierarchy mapping when the candidate already exposes a layer index.
    **/
    // Use the layer index as the coarse hierarchy hint when available.
    if ( candidate.layer_index >= 0 ) {
        return candidate.layer_index;
    }

    /**
    *	Fallback to tile identifiers only when a hierarchy id is not present.
    **/
    // Use the canonical tile id as a fallback hierarchy hint.
    if ( candidate.tile_id >= 0 ) {
        return candidate.tile_id;
    }

    /**
    *	Last resort: return the local candidate id.
    **/
    // Return the candidate id as a last resort placeholder.
    return candidate.candidate_id;
}

/**
*	@brief	Select the best hierarchy node to represent a coarse endpoint.
*	@param	graph	Hierarchy graph to search.
*	@param	candidate	Endpoint candidate metadata.
*	@return	Stable hierarchy node id or -1 when no suitable node was found.
**/
static int32_t SVG_Nav2_SelectHierarchyNodeForCandidate( const nav2_hierarchy_graph_t &graph, const nav2_goal_candidate_t &candidate ) {
    /**
    *	Require a populated hierarchy graph before searching for candidate matches.
    **/
    // Bail out when no hierarchy nodes are available.
    if ( graph.nodes.empty() ) {
        // Return invalid to signal no match.
        return -1;
    }

    /**
    *	Score hierarchy nodes based on topology alignment with the candidate.
    **/
    // Track the best matching node id and score.
    int32_t bestNodeId = -1;
    int32_t bestScore = -1;
    double bestZDelta = std::numeric_limits<double>::infinity();

    // Evaluate every hierarchy node for a topology match.
    for ( const nav2_hierarchy_node_t &node : graph.nodes ) {
        // Skip nodes that do not carry a valid Z band.
        if ( !node.allowed_z_band.IsValid() ) {
            continue;
        }

        // Score the node based on topology membership and stability.
        int32_t score = 0;
        if ( candidate.leaf_index >= 0 && candidate.leaf_index == node.topology.leaf_index ) {
            score = 3;
        } else if ( candidate.cluster_id >= 0 && candidate.cluster_id == node.topology.cluster_id ) {
            score = 2;
        } else if ( candidate.area_id >= 0 && candidate.area_id == node.topology.area_id ) {
            score = 1;
        }

        // Skip nodes that do not match any topology criteria.
        if ( score <= 0 ) {
            continue;
        }

        // Compute the vertical delta between the candidate and the hierarchy node.
        const double zDelta = std::fabs( candidate.center_origin.z - node.allowed_z_band.preferred_z );

        // Prefer higher scores and tighter Z alignment.
        if ( score > bestScore || ( score == bestScore && zDelta < bestZDelta ) ) {
            bestScore = score;
            bestZDelta = zDelta;
            bestNodeId = node.node_id;
        }
    }

    // Return the best match found, or -1 if no match exists.
    return bestNodeId;
}

/**
*	@brief	Resolve a hierarchy node id for a coarse frontier node.
*	@param	graph	Hierarchy graph to search.
*	@param	node	Frontier node to resolve.
*	@return	Stable hierarchy node id or -1 when no suitable node was found.
**/
static int32_t SVG_Nav2_ResolveHierarchyNodeForFrontier( const nav2_hierarchy_graph_t &graph, const nav2_coarse_astar_node_t &node ) {
    /**
    *	If the node already carries a hierarchy id, return it immediately.
    **/
    // Keep the explicit hierarchy id when it is already present.
    if ( node.hierarchy_node_id >= 0 ) {
        return node.hierarchy_node_id;
    }

    /**
    *	Fallback to matching by topology metadata.
    **/
    // Construct a pseudo-candidate to reuse the topology matching logic.
    nav2_goal_candidate_t pseudoCandidate = {};
    pseudoCandidate.leaf_index = node.topology.leaf_index;
    pseudoCandidate.cluster_id = node.topology.cluster_id;
    pseudoCandidate.area_id = node.topology.area_id;
    pseudoCandidate.center_origin = Vector3{ 0.0f, 0.0f, ( float )node.allowed_z_band.preferred_z };

    // Reuse the candidate selector to find a matching hierarchy node.
    return SVG_Nav2_SelectHierarchyNodeForCandidate( graph, pseudoCandidate );
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
  /**
    *	Sanity-check the output state and the hierarchy graph before building frontier state.
    **/
    // Require a valid solver state and a populated hierarchy graph.
    if ( !state || hierarchyGraph.nodes.empty() ) {
        // Return false when initialization prerequisites are missing.
        return false;
    }

    /**
    *	Reset the solver and bind the query context for a fresh coarse search.
    **/
    // Reset the state to deterministic defaults.
    SVG_Nav2_CoarseAStar_Reset( state );
    // Assign the stable solver identifier.
    state->solver_id = solverId;
    // Mark the solver as actively running.
    state->status = nav2_coarse_astar_status_t::Running;
    // Store the selected candidates for later diagnostics.
    state->start_candidate = startCandidate;
    state->goal_candidate = goalCandidate;
    // Bind the hierarchy graph used for expansion.
    state->hierarchy_graph = &hierarchyGraph;
    state->hierarchy_graph_bound = true;
    // Assign the benchmark scenario for the query state.
    state->query_state.bench_scenario = startCandidate.flags & NAV_GOAL_CANDIDATE_FLAG_NEIGHBOR_SAMPLE ? nav2_bench_scenario_t::SameFloorOpen : nav2_bench_scenario_t::SameFloorOpen;
    // Set the active stage to coarse search.
    state->query_state.stage = nav2_query_stage_t::CoarseSearch;
    // Mark the result as pending until the search completes.
    state->query_state.result_status = nav2_query_result_status_t::Pending;

    /**
    *	Seed the frontier with start and goal nodes derived from the candidate endpoints.
    **/
    // Allocate stable ids for the start and goal nodes.
    const int32_t startNodeId = SVG_Nav2_AllocateCoarseNodeId( *state );
    const int32_t goalNodeId = startNodeId + 1;
    // Build the start node from the candidate and attach a hierarchy match.
    nav2_coarse_astar_node_t startNode = SVG_Nav2_MakeNodeFromCandidate( startCandidate, startNodeId );
    startNode.kind = nav2_coarse_astar_node_kind_t::Start;
    startNode.g_score = 0.0;
    startNode.h_score = SVG_Nav2_Heuristic( startNode, SVG_Nav2_MakeNodeFromCandidate( goalCandidate, goalNodeId ) );
    startNode.f_score = startNode.h_score;
    startNode.hierarchy_node_id = SVG_Nav2_SelectHierarchyNodeForCandidate( hierarchyGraph, startCandidate );
    state->nodes.push_back( startNode );

    // Build the goal node from the candidate and attach a hierarchy match.
    nav2_coarse_astar_node_t goalNode = SVG_Nav2_MakeNodeFromCandidate( goalCandidate, goalNodeId );
    goalNode.kind = nav2_coarse_astar_node_kind_t::Goal;
    goalNode.g_score = 0.0;
    goalNode.h_score = 0.0;
    goalNode.f_score = 0.0;
    goalNode.hierarchy_node_id = SVG_Nav2_SelectHierarchyNodeForCandidate( hierarchyGraph, goalCandidate );
    state->nodes.push_back( goalNode );

    /**
    *	Mark the solver as owning a frontier slice so later slices can resume deterministically.
    **/
    // Flag that the solver has an active frontier to resume later.
    state->has_frontier_slice = true;
    // Return true to signal successful initialization.
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
    /**
    *	Sanity-check the solver state before spending a slice.
    **/
    // Require a live solver with at least one frontier node.
    if ( !state || state->status != nav2_coarse_astar_status_t::Running || state->nodes.empty() ) {
        // Return false so the scheduler can decide whether to reschedule.
        return false;
    }

    /**
    *	Record the budget slice on the query state so the scheduler can resume deterministically.
    **/
    // Assign the current slice to the query state.
    state->query_state.active_slice = budgetSlice;
    // Record the slice consumed by the query.
    state->query_state.progress.slices_consumed++;
    state->diagnostics.slices_consumed++;
    state->diagnostics.resumes++;
    state->query_state.progress.coarse_expansions = state->diagnostics.expansions;

    /**
    *	Respect cancellation and restart requests before doing any new work.
    **/
    // Cancel the solver when the query requested cancellation.
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
    // Pause the solver when a restart is requested.
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

    /**
    *	Select the next frontier node and ensure it is still valid to expand.
    **/
    // Pick the best frontier node based on f-score.
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

    // Capture the node we are expanding for this slice.
    const nav2_coarse_astar_node_t currentNode = state->nodes[ ( size_t )bestIndex ];
    if ( !SVG_Nav2_ShouldExpandNode( state, currentNode ) ) {
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    /**
    *	Check for goal completion before expanding neighbors.
    **/
    // Stop when the current node corresponds to the goal candidate.
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

    /**
    *	Resolve the hierarchy graph reference for expansion.
    **/
    // Require a bound hierarchy graph to perform expansion.
    if ( !state->hierarchy_graph_bound || state->hierarchy_graph == nullptr ) {
        state->status = nav2_coarse_astar_status_t::Partial;
        state->query_state.result_status = nav2_query_result_status_t::Partial;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }
    const nav2_hierarchy_graph_t &hierarchyGraph = *state->hierarchy_graph;

    /**
    *	Resolve the hierarchy node for the current frontier node.
    **/
    // Find the hierarchy node associated with the frontier node.
    const int32_t hierarchyNodeId = SVG_Nav2_ResolveHierarchyNodeForFrontier( hierarchyGraph, currentNode );
    nav2_hierarchy_node_ref_t hierarchyRef = {};
    if ( hierarchyNodeId < 0 || !SVG_Nav2_HierarchyGraph_TryResolve( hierarchyGraph, hierarchyNodeId, &hierarchyRef ) ) {
        state->diagnostics.policy_prunes++;
        state->status = nav2_coarse_astar_status_t::Partial;
        state->query_state.result_status = nav2_query_result_status_t::Partial;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    /**
    *	Iterate hierarchy edges and expand neighbors within the granted budget.
    **/
    // Track remaining expansion budget for this slice.
    uint32_t remainingExpansions = budgetSlice.granted_expansions;
    // Use a minimum expansion count when the slice does not specify a budget.
    if ( remainingExpansions == 0 ) {
        remainingExpansions = 1;
    }

    // Grab the hierarchy node for adjacency traversal.
    const nav2_hierarchy_node_t &hierarchyNode = hierarchyGraph.nodes[ ( size_t )hierarchyRef.node_index ];

    // Iterate over outgoing hierarchy edges to expand neighbors.
    for ( size_t edgeIdx = 0; edgeIdx < hierarchyNode.outgoing_edge_ids.size() && remainingExpansions > 0; ++edgeIdx ) {
        // Resolve the hierarchy edge id into a concrete edge.
        const int32_t hierarchyEdgeId = hierarchyNode.outgoing_edge_ids[ edgeIdx ];
        const nav2_hierarchy_edge_t *hierarchyEdge = nullptr;
        for ( const nav2_hierarchy_edge_t &edgeCandidate : hierarchyGraph.edges ) {
            if ( edgeCandidate.edge_id == hierarchyEdgeId ) {
                hierarchyEdge = &edgeCandidate;
                break;
            }
        }

        /**
        *	Skip edges that could not be resolved.
        **/
        // Continue when the edge id is invalid.
        if ( !hierarchyEdge ) {
            continue;
        }

        /**
        *	Reject edges that are explicitly marked hard-invalid.
        **/
        // Reject edges that violate hard constraints.
        if ( ( hierarchyEdge->flags & NAV2_HIERARCHY_EDGE_FLAG_HARD_INVALID ) != 0 ) {
            state->diagnostics.hard_invalid_prunes++;
            continue;
        }

        /**
        *	Reject edges that require mover or portal commitments when not present.
        **/
        // Skip portal edges when no connector id is attached.
        if ( ( hierarchyEdge->flags & NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_PORTAL ) != 0 && hierarchyEdge->connector_id < 0 ) {
            state->diagnostics.policy_prunes++;
            continue;
        }
        // Skip mover edges when no mover reference exists.
        if ( ( hierarchyEdge->flags & NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_MOVER ) != 0 && !hierarchyEdge->mover_ref.IsValid() ) {
            state->diagnostics.mover_prunes++;
            continue;
        }

        /**
        *	Apply a conservative drop-limit check using step constants.
        **/
        // Compute vertical delta between the current node and the edge target.
        const double targetZ = hierarchyEdge->allowed_z_band.preferred_z;
        const double zDelta = std::fabs( currentNode.allowed_z_band.preferred_z - targetZ );
        // Skip edges that exceed step limits to avoid unsafe drops.
        if ( zDelta > PM_STEP_MAX_SIZE ) {
            state->diagnostics.policy_prunes++;
            continue;
        }

        /**
        *	Resolve the destination hierarchy node.
        **/
        // Resolve the hierarchy node on the other end of the edge.
        nav2_hierarchy_node_ref_t destHierarchyRef = {};
        if ( !SVG_Nav2_HierarchyGraph_TryResolve( hierarchyGraph, hierarchyEdge->to_node_id, &destHierarchyRef ) ) {
            state->diagnostics.policy_prunes++;
            continue;
        }

        /**
        *	Create or reuse a frontier node for the destination hierarchy node.
        **/
        // Build a new coarse node for the destination.
        nav2_coarse_astar_node_t neighborNode = SVG_Nav2_MakeNodeFromHierarchyNode( hierarchyGraph.nodes[ ( size_t )destHierarchyRef.node_index ], SVG_Nav2_AllocateCoarseNodeId( *state ), nav2_coarse_astar_node_kind_t::RegionLayer );
        neighborNode.parent_node_id = currentNode.node_id;
        neighborNode.parent_edge_id = SVG_Nav2_AllocateCoarseEdgeId( *state );
        neighborNode.g_score = currentNode.g_score + hierarchyEdge->base_cost + hierarchyEdge->topology_penalty;
        neighborNode.h_score = SVG_Nav2_Heuristic( neighborNode, state->nodes.back() );
        neighborNode.f_score = neighborNode.g_score + neighborNode.h_score;

        // Push the neighbor node into the frontier.
        state->nodes.push_back( neighborNode );
        // Translate the hierarchy edge into a coarse edge.
        state->edges.push_back( SVG_Nav2_MakeEdgeFromHierarchyEdge( currentNode, neighborNode, *hierarchyEdge, neighborNode.parent_edge_id ) );

        // Record one expansion for diagnostics and slicing.
        state->diagnostics.expansions++;
        state->query_state.progress.coarse_expansions = state->diagnostics.expansions;
        // Decrement the remaining expansion budget.
        remainingExpansions--;
    }

    /**
    *	Publish a partial result when the slice budget is exhausted.
    **/
    // Mark the solver as partially complete to request another slice.
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
