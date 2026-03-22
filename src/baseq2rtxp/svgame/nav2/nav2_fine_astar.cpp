/********************************************************************
*
*
*	ServerGame: Nav2 Corridor-Constrained Fine A* - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_fine_astar.h"

#include <algorithm>
#include <cmath>
#include <limits>


/**
*	@brief	Allocate the next stable fine-node id in a solver state.
*	@param	state	Solver state to inspect.
*	@return	Stable node id for the next appended frontier node.
**/
static int32_t SVG_Nav2_AllocateFineNodeId( const nav2_fine_astar_state_t &state ) {
    // Keep node ids monotonic so reconstruction remains deterministic.
    int32_t nextId = 1;
    for ( const nav2_fine_astar_node_t &node : state.nodes ) {
        nextId = std::max( nextId, node.node_id + 1 );
    }
    return nextId;
}

/**
*	@brief	Allocate the next stable fine-edge id in a solver state.
*	@param	state	Solver state to inspect.
*	@return	Stable edge id for the next appended frontier edge.
**/
static int32_t SVG_Nav2_AllocateFineEdgeId( const nav2_fine_astar_state_t &state ) {
    // Keep edge ids monotonic so debug output and path reconstruction remain stable.
    int32_t nextId = 1;
    for ( const nav2_fine_astar_edge_t &edge : state.edges ) {
        nextId = std::max( nextId, edge.edge_id + 1 );
    }
    return nextId;
}

/**
*	@brief	Create a fine-search node from one corridor segment.
*	@param	segment	Corridor segment to convert.
*	@param	nodeId	Stable node id to assign.
*	@param	kind	Semantic kind to assign.
*	@return	Fine-search node payload.
**/
static nav2_fine_astar_node_t SVG_Nav2_MakeNodeFromSegment( const nav2_corridor_segment_t &segment, const int32_t nodeId, const nav2_fine_astar_node_kind_t kind ) {
    // Mirror the corridor segment into a stable fine-search node.
    nav2_fine_astar_node_t node = {};
    node.node_id = nodeId;
    node.kind = kind;
    node.span_id = segment.topology.cell_index;
    node.column_index = segment.topology.tile_x;
    node.span_index = segment.topology.layer_index;
    node.connector_id = segment.topology.portal_id;
    node.mover_entnum = segment.mover_ref.owner_entnum;
    node.inline_model_index = segment.mover_ref.model_index;
    node.flags |= NAV2_FINE_ASTAR_NODE_FLAG_HAS_ALLOWED_Z_BAND;
    node.allowed_z_band = segment.allowed_z_band;
    node.topology = segment.topology;
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF ) != 0 ) {
        node.flags |= NAV2_FINE_ASTAR_NODE_FLAG_HAS_MOVER;
    }
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID ) != 0 ) {
        node.flags |= NAV2_FINE_ASTAR_NODE_FLAG_HAS_CONNECTOR;
    }
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT ) != 0 ) {
        node.flags |= NAV2_FINE_ASTAR_NODE_FLAG_PREFERRED;
    }
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ) {
        node.flags |= NAV2_FINE_ASTAR_NODE_FLAG_PARTIAL;
    }
    return node;
}

/**
*	@brief	Create a fine-search edge from one span adjacency edge.
*	@param	edge	Span adjacency edge to convert.
*	@param	edgeId	Stable edge id to assign.
*	@return	Fine-search edge payload.
**/
static nav2_fine_astar_edge_t SVG_Nav2_MakeEdgeFromSpanEdge( const nav2_span_edge_t &edge, const int32_t edgeId ) {
    // Translate span adjacency into fine-search edge metadata.
    nav2_fine_astar_edge_t fineEdge = {};
    fineEdge.edge_id = edgeId;
    fineEdge.from_node_id = edge.from_span_id;
    fineEdge.to_node_id = edge.to_span_id;
    fineEdge.kind = nav2_fine_astar_edge_kind_t::SpanLink;
    fineEdge.base_cost = edge.cost;
    fineEdge.topology_penalty = edge.soft_penalty_cost;
    fineEdge.allowed_z_band.min_z = std::min( edge.from_column_index, edge.to_column_index ) >= 0 ? std::min( edge.vertical_delta, 0.0 ) : edge.vertical_delta;
    fineEdge.allowed_z_band.max_z = std::max( edge.vertical_delta, 0.0 );
    fineEdge.allowed_z_band.preferred_z = edge.vertical_delta;
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_PASSABLE ) != 0 ) {
        fineEdge.flags |= NAV2_FINE_ASTAR_EDGE_FLAG_PASSABLE;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_STEP_UP ) != 0 ) {
        fineEdge.flags |= NAV2_FINE_ASTAR_EDGE_FLAG_REQUIRES_STAIR;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_STEP_DOWN ) != 0 ) {
        fineEdge.flags |= NAV2_FINE_ASTAR_EDGE_FLAG_REQUIRES_STAIR;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_CONTROLLED_DROP ) != 0 ) {
        fineEdge.flags |= NAV2_FINE_ASTAR_EDGE_FLAG_PARTIAL;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_HARD_INVALID ) != 0 ) {
        fineEdge.flags |= NAV2_FINE_ASTAR_EDGE_FLAG_HARD_INVALID;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_SOFT_PENALIZED ) != 0 ) {
        fineEdge.flags |= NAV2_FINE_ASTAR_EDGE_FLAG_TOPOLOGY_PENALIZED;
    }
    return fineEdge;
}

/**
*	@brief	Compute a cheap heuristic between two corridor-local nodes.
*	@param	fromNode	Source node.
*	@param	toNode	Destination node.
*	@return	Heuristic cost estimate.
**/
static double SVG_Nav2_Heuristic( const nav2_fine_astar_node_t &fromNode, const nav2_fine_astar_node_t &toNode ) {
    // Use the allowed Z-band and topology data to produce a stable lower bound.
    const double dz = std::fabs( fromNode.allowed_z_band.preferred_z - toNode.allowed_z_band.preferred_z );
    const double topologyPenalty = ( fromNode.topology.region_id >= 0 && toNode.topology.region_id >= 0 && fromNode.topology.region_id != toNode.topology.region_id ) ? 8.0 : 0.0;
    const double moverPenalty = ( fromNode.mover_entnum >= 0 || toNode.mover_entnum >= 0 ) ? 4.0 : 0.0;
    return dz + topologyPenalty + moverPenalty;
}

/**
*	@brief	Find the lowest-f-score node index in the frontier.
*	@param	state	Solver state to inspect.
*	@return	Frontier index, or `-1` when empty.
**/
static int32_t SVG_Nav2_FindBestFrontierNodeIndex( const nav2_fine_astar_state_t &state ) {
    // Scan the frontier linearly because the initial solver version prioritizes determinism and clarity.
    if ( state.nodes.empty() ) {
        return -1;
    }

    double bestScore = std::numeric_limits<double>::infinity();
    int32_t bestIndex = -1;
    for ( size_t i = 0; i < state.nodes.size(); ++i ) {
        const nav2_fine_astar_node_t &node = state.nodes[ i ];
        if ( node.f_score < bestScore ) {
            bestScore = node.f_score;
            bestIndex = ( int32_t )i;
        }
    }
    return bestIndex;
}

/**
*	@brief	Reconstruct a stable path from a terminal node.
*	@param	state	Solver state to inspect.
*	@param	terminalNodeId	Terminal node id to unwind.
*	@param	out_path	[out] Path buffer receiving reconstructed node and edge ids.
**/
static void SVG_Nav2_ReconstructPath( const nav2_fine_astar_state_t &state, const int32_t terminalNodeId, nav2_fine_astar_path_t *out_path ) {
    // Rebuild the result path from goal to start using the stored parent links.
    if ( !out_path ) {
        return;
    }
    out_path->node_ids.clear();
    out_path->edge_ids.clear();

    int32_t currentNodeId = terminalNodeId;
    while ( currentNodeId >= 0 ) {
        const nav2_fine_astar_node_t *currentNode = nullptr;
        for ( const nav2_fine_astar_node_t &node : state.nodes ) {
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
*	@brief	Apply corridor-policy pruning before expanding a node.
*	@param	state	Solver state to mutate.
*	@param	node	Node selected for expansion.
*	@return	True when the node should be expanded.
**/
static const bool SVG_Nav2_ShouldExpandNode( nav2_fine_astar_state_t *state, const nav2_fine_astar_node_t &node ) {
    // Reject nodes that have already been rendered stale or cancelled by the scheduler.
    if ( state->query_state.cancel_requested || state->query_state.restart_requested ) {
        state->status = nav2_fine_astar_status_t::Cancelled;
        return false;
    }
    if ( state->query_state.snapshot.static_nav_version == 0 ) {
        state->diagnostics.z_prunes++;
        state->status = nav2_fine_astar_status_t::Stale;
        return false;
    }
    if ( ( node.flags & NAV2_FINE_ASTAR_NODE_FLAG_HAS_MOVER ) != 0 && state->query_state.snapshot.mover_version == 0 ) {
        state->diagnostics.mover_prunes++;
        return false;
    }
    if ( ( node.flags & NAV2_FINE_ASTAR_NODE_FLAG_PARTIAL ) != 0 ) {
        state->diagnostics.topology_prunes++;
        return false;
    }
    return true;
}

/**
*	@brief	Create a compact fallback corridor node when the corridor has only minimal route metadata.
*	@param	corridor	Corridor to inspect.
*	@param	nodeId	Stable node id to assign.
*	@return	Fallback node payload.
**/
static nav2_fine_astar_node_t SVG_Nav2_MakeFallbackNode( const nav2_corridor_t &corridor, const int32_t nodeId ) {
    // Build a fallback node from the corridor start endpoint so the solver can remain resumable.
    nav2_fine_astar_node_t node = {};
    node.node_id = nodeId;
    node.kind = nav2_fine_astar_node_kind_t::Fallback;
    node.allowed_z_band = corridor.global_z_band;
    node.topology = corridor.start_topology;
    node.flags |= NAV2_FINE_ASTAR_NODE_FLAG_PARTIAL | NAV2_FINE_ASTAR_NODE_FLAG_HAS_ALLOWED_Z_BAND;
    return node;
}


/**
*	@brief	Reset a fine A* solver state.
*	@param	state	[out] Solver state to reset.
**/
void SVG_Nav2_FineAStar_Reset( nav2_fine_astar_state_t *state ) {
    // Guard null callers so the helper remains easy to use.
    if ( !state ) {
        return;
    }
    *state = {};
    state->status = nav2_fine_astar_status_t::None;
    SVG_Nav2_QueryState_Reset( &state->query_state );
}

/**
*	@brief	Initialize a fine A* solver from a corridor and its coarse state.
*	@param	state	[out] Solver state to initialize.
*	@param	corridor	Corridor to refine.
*	@param	solverId	Stable solver identifier.
*	@return	True when initialization succeeded.
**/
const bool SVG_Nav2_FineAStar_Init( nav2_fine_astar_state_t *state, const nav2_corridor_t &corridor, const uint64_t solverId ) {
    // Validate output storage before building frontier state.
    if ( !state || corridor.segments.empty() ) {
        return false;
    }

    // Reset the solver and bind the query context.
    SVG_Nav2_FineAStar_Reset( state );
    state->solver_id = solverId;
    state->status = nav2_fine_astar_status_t::Running;
    state->corridor = corridor;
    state->query_state.stage = nav2_query_stage_t::FineRefinement;
    state->query_state.result_status = nav2_query_result_status_t::Pending;
    state->query_state.has_provisional_result = ( corridor.flags & NAV_CORRIDOR_FLAG_PARTIAL_RESULT ) != 0;
    state->query_state.requires_revalidation = true;

    // Seed the frontier with the corridor start and goal so later slices can refine the chosen route locally.
    const int32_t startNodeId = SVG_Nav2_AllocateFineNodeId( *state );
    const int32_t goalNodeId = startNodeId + 1;
    nav2_fine_astar_node_t startNode = SVG_Nav2_MakeNodeFromSegment( corridor.segments.front(), startNodeId, nav2_fine_astar_node_kind_t::Start );
    startNode.g_score = 0.0;
    startNode.h_score = SVG_Nav2_Heuristic( startNode, SVG_Nav2_MakeNodeFromSegment( corridor.segments.back(), goalNodeId, nav2_fine_astar_node_kind_t::Goal ) );
    startNode.f_score = startNode.h_score;
    state->nodes.push_back( startNode );

    nav2_fine_astar_node_t goalNode = SVG_Nav2_MakeNodeFromSegment( corridor.segments.back(), goalNodeId, nav2_fine_astar_node_kind_t::Goal );
    goalNode.g_score = 0.0;
    goalNode.h_score = 0.0;
    goalNode.f_score = 0.0;
    state->nodes.push_back( goalNode );

    // Mark the solver as owning a frontier slice so later service calls can resume deterministically.
    state->has_frontier_slice = true;
    return true;
}

/**
*	@brief	Advance a fine A* solver by one budgeted slice.
*	@param	state	Solver state to advance.
*	@param	budgetSlice	Budget slice controlling this step.
*	@param	out_result	[out] Optional result snapshot.
*	@return	True when the solver made progress or completed.
**/
const bool SVG_Nav2_FineAStar_Step( nav2_fine_astar_state_t *state, const nav2_budget_slice_t &budgetSlice, nav2_fine_astar_result_t *out_result ) {
    // Require a live solver with at least one frontier node.
    if ( !state || state->status != nav2_fine_astar_status_t::Running || state->nodes.empty() ) {
        return false;
    }

    // Record the budget slice on the query state so the scheduler can resume deterministically.
    state->query_state.active_slice = budgetSlice;
    state->query_state.progress.slices_consumed++;
    state->diagnostics.slices_consumed++;
    state->diagnostics.resumes++;
    state->query_state.progress.fine_expansions = state->diagnostics.expansions;

    // Respect cancellation and restart requests before doing any new work.
    if ( state->query_state.cancel_requested ) {
        state->status = nav2_fine_astar_status_t::Cancelled;
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
        state->status = nav2_fine_astar_status_t::Partial;
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
        state->status = nav2_fine_astar_status_t::Failed;
        state->query_state.result_status = nav2_query_result_status_t::Failed;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    const nav2_fine_astar_node_t currentNode = state->nodes[ ( size_t )bestIndex ];
    if ( !SVG_Nav2_ShouldExpandNode( state, currentNode ) ) {
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    // If the best node already corresponds to the goal, reconstruct and stop.
    if ( currentNode.kind == nav2_fine_astar_node_kind_t::Goal ) {
        SVG_Nav2_ReconstructPath( *state, currentNode.node_id, &state->path );
        state->status = nav2_fine_astar_status_t::Success;
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

    // Expand the current node to a synthetic next node so the solver remains resumable before the full span walker is wired in.
    nav2_fine_astar_node_t nextNode = SVG_Nav2_MakeFallbackNode( state->corridor, SVG_Nav2_AllocateFineNodeId( *state ) );
    nextNode.parent_node_id = currentNode.node_id;
    nextNode.parent_edge_id = SVG_Nav2_AllocateFineEdgeId( *state );
    nextNode.g_score = currentNode.g_score + 1.0;
    nextNode.h_score = SVG_Nav2_Heuristic( nextNode, state->nodes.back() );
    nextNode.f_score = nextNode.g_score + nextNode.h_score;
    state->nodes.push_back( nextNode );
    state->edges.push_back( nav2_fine_astar_edge_t{} );
    state->edges.back().edge_id = nextNode.parent_edge_id;
    state->edges.back().from_node_id = currentNode.node_id;
    state->edges.back().to_node_id = nextNode.node_id;
    state->edges.back().kind = nav2_fine_astar_edge_kind_t::FallbackLink;
    state->edges.back().flags |= NAV2_FINE_ASTAR_EDGE_FLAG_PARTIAL;
    state->diagnostics.expansions++;
    state->status = nav2_fine_astar_status_t::Partial;
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
*	@brief	Return whether a fine A* solver has reached a terminal state.
*	@param	state	Solver state to inspect.
*	@return	True when the solver is done, cancelled, failed, or stale.
**/
const bool SVG_Nav2_FineAStar_IsTerminal( const nav2_fine_astar_state_t *state ) {
    // Terminal states are represented directly in the solver status.
    if ( !state ) {
        return true;
    }
    switch ( state->status ) {
        case nav2_fine_astar_status_t::Success:
        case nav2_fine_astar_status_t::Failed:
        case nav2_fine_astar_status_t::Cancelled:
        case nav2_fine_astar_status_t::Stale:
            return true;
        default:
            return false;
    }
}

/**
*	@brief	Emit a bounded debug summary for the fine A* state.
*	@param	state	Solver state to report.
*	@param	limit	Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintFineAStar( const nav2_fine_astar_state_t &state, const int32_t limit ) {
    // Skip diagnostics when nothing useful could be printed.
    if ( limit <= 0 ) {
        return;
    }

    const int32_t reportNodeCount = std::min( ( int32_t )state.nodes.size(), limit );
    gi.dprintf( "[NAV2][FineAStar] status=%d solver=%llu nodes=%d edges=%d report=%d exp=%u slices=%u pauses=%u resumes=%u z=%u topology=%u mover=%u hard=%u pathNodes=%d pathEdges=%d\n",
        ( int32_t )state.status,
        ( unsigned long long )state.solver_id,
        ( int32_t )state.nodes.size(),
        ( int32_t )state.edges.size(),
        reportNodeCount,
        state.diagnostics.expansions,
        state.diagnostics.slices_consumed,
        state.diagnostics.pauses,
        state.diagnostics.resumes,
        state.diagnostics.z_prunes,
        state.diagnostics.topology_prunes,
        state.diagnostics.mover_prunes,
        state.diagnostics.hard_invalid_prunes,
        ( int32_t )state.path.node_ids.size(),
        ( int32_t )state.path.edge_ids.size() );
    for ( int32_t i = 0; i < reportNodeCount; ++i ) {
        const nav2_fine_astar_node_t &node = state.nodes[ ( size_t )i ];
        gi.dprintf( "[NAV2][FineAStar] node=%d kind=%d span=%d col=%d idx=%d conn=%d mover=%d model=%d flags=0x%08x z=(%.1f..%.1f) g=%.1f h=%.1f f=%.1f parent=%d edge=%d\n",
            node.node_id,
            ( int32_t )node.kind,
            node.span_id,
            node.column_index,
            node.span_index,
            node.connector_id,
            node.mover_entnum,
            node.inline_model_index,
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
*	@brief	Keep the nav2 fine A* module represented in the build.
**/
void SVG_Nav2_FineAStar_ModuleAnchor( void ) {
}
