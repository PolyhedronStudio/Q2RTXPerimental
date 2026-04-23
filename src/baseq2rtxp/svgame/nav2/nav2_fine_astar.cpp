/********************************************************************
*
*
*	ServerGame: Nav2 Corridor-Constrained Fine A* - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_fine_astar.h"

#include "svgame/nav2/nav2_span_grid_build.h"

#include <algorithm>
#include <array>
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
*	@brief	Resolve one fine-search node by stable id.
*	@param	state	Solver state to inspect.
*	@param	nodeId	Stable node id to resolve.
*	@return	Pointer to the resolved node, or `nullptr` when missing.
**/
static nav2_fine_astar_node_t *SVG_Nav2_FindFineNodeById( nav2_fine_astar_state_t *state, const int32_t nodeId ) {
    /**
    *    Require solver storage before resolving a node id.
    **/
    if ( !state ) {
        return nullptr;
    }

    /**
    *    Resolve through the stable id-to-index map first for deterministic lookup.
    **/
    const auto it = state->node_id_to_index.find( nodeId );
    if ( it == state->node_id_to_index.end() ) {
        return nullptr;
    }

    /**
    *    Reject stale indices so the lookup stays robust during resumable mutation.
    **/
    const int32_t nodeIndex = it->second;
    if ( nodeIndex < 0 || nodeIndex >= ( int32_t )state->nodes.size() ) {
        return nullptr;
    }
    return &state->nodes[ ( size_t )nodeIndex ];
}

/**
*	@brief	Resolve one fine-search node by stable id (const overload).
*	@param	state	Solver state to inspect.
*	@param	nodeId	Stable node id to resolve.
*	@return	Pointer to the resolved node, or `nullptr` when missing.
**/
static const nav2_fine_astar_node_t *SVG_Nav2_FindFineNodeById( const nav2_fine_astar_state_t &state, const int32_t nodeId ) {
    /**
    *    Resolve through the stable id-to-index map first for deterministic lookup.
    **/
    const auto it = state.node_id_to_index.find( nodeId );
    if ( it == state.node_id_to_index.end() ) {
        return nullptr;
    }

    /**
    *    Reject stale indices so the lookup stays robust during resumable mutation.
    **/
    const int32_t nodeIndex = it->second;
    if ( nodeIndex < 0 || nodeIndex >= ( int32_t )state.nodes.size() ) {
        return nullptr;
    }
    return &state.nodes[ ( size_t )nodeIndex ];
}

/**
*	@brief	Register one frontier node into the solver's stable lookup structures.
*	@param	state	[in,out] Solver state receiving the node.
*	@param	node	Node payload to append.
*	@return	True when the node was appended.
**/
static const bool SVG_Nav2_AppendFineNode( nav2_fine_astar_state_t *state, const nav2_fine_astar_node_t &node ) {
    /**
    *    Require solver storage and a valid node id before appending.
    **/
    if ( !state || node.node_id < 0 ) {
        return false;
    }

    /**
    *    Avoid duplicate node ids because resumable solver state depends on stable id identity.
    **/
    if ( state->node_id_to_index.find( node.node_id ) != state->node_id_to_index.end() ) {
        return false;
    }

    /**
    *    Append the node and wire stable id lookup bookkeeping in one place.
    **/
    const int32_t nodeIndex = ( int32_t )state->nodes.size();
    state->nodes.push_back( node );
    state->node_id_to_index[ node.node_id ] = nodeIndex;
    return true;
}

/**
*	@brief	Select the best span candidate from one sparse column using topology and Z-band hints.
*	@param	grid	Span-grid snapshot being queried.
*	@param	columnIndex	Owning sparse-column index.
*	@param	topology	Corridor topology hint used for membership scoring.
*	@param	zBand	Corridor Z-band hint used for vertical tie-break scoring.
*	@param	outSpanId	[out] Resolved stable span id.
*	@return	True when one span id was selected.
**/
static const bool SVG_Nav2_SelectBestSpanIdFromColumn( const nav2_span_grid_t &grid, const int32_t columnIndex,
    const nav2_corridor_topology_ref_t &topology, const nav2_corridor_z_band_t &zBand, int32_t *outSpanId ) {
    /**
    *    Require a writable output and a valid sparse-column index before scoring spans.
    **/
    if ( !outSpanId || columnIndex < 0 || columnIndex >= ( int32_t )grid.columns.size() ) {
        return false;
    }

    /**
    *    Reject empty columns because no span candidates can be selected.
    **/
    const nav2_span_column_t &column = grid.columns[ ( size_t )columnIndex ];
    if ( column.spans.empty() ) {
        return false;
    }

    /**
    *    First prefer deterministic direct indices when corridor topology provides explicit ids.
    **/
    if ( topology.cell_index >= 0 ) {
        nav2_span_ref_t spanRef = {};
        if ( SVG_Nav2_SpanGrid_TryResolveSpanRef( grid, topology.cell_index, &spanRef ) ) {
            *outSpanId = spanRef.span_id;
            return true;
        }
    }
    if ( topology.layer_index >= 0 && topology.layer_index < ( int32_t )column.spans.size() ) {
        *outSpanId = column.spans[ ( size_t )topology.layer_index ].span_id;
        return true;
    }

    /**
    *    Score all spans in the column and pick the best topology-and-height compatible candidate.
    **/
    const double preferredZ = zBand.IsValid() ? zBand.preferred_z : 0.0;
    int32_t bestSpanId = -1;
    int32_t bestScore = std::numeric_limits<int32_t>::max();
    double bestZDelta = std::numeric_limits<double>::infinity();
    for ( const nav2_span_t &span : column.spans ) {
        int32_t score = 0;

        // Prefer matching coarse region commitments when present.
        if ( topology.region_id >= 0 && span.region_layer_id >= 0 && topology.region_id != span.region_layer_id ) {
            score += 8;
        }

        // Prefer matching BSP area/cluster/leaf commitments for local continuity.
        if ( topology.area_id >= 0 && span.area_id >= 0 && topology.area_id != span.area_id ) {
            score += 4;
        }
        if ( topology.cluster_id >= 0 && span.cluster_id >= 0 && topology.cluster_id != span.cluster_id ) {
            score += 2;
        }
        if ( topology.leaf_index >= 0 && span.leaf_id >= 0 && topology.leaf_index != span.leaf_id ) {
            score += 1;
        }

        // Use preferred Z as a deterministic tie-break so vertical continuity remains stable.
        const double zDelta = std::fabs( span.preferred_z - preferredZ );
        if ( !zBand.IsValid() ) {
            // When no explicit Z band exists, keep penalties neutral and rely on topology score.
            if ( score < bestScore ) {
                bestScore = score;
                bestZDelta = zDelta;
                bestSpanId = span.span_id;
            } else if ( score == bestScore && zDelta < bestZDelta ) {
                bestZDelta = zDelta;
                bestSpanId = span.span_id;
            }
            continue;
        }

        if ( score < bestScore || ( score == bestScore && zDelta < bestZDelta ) ) {
            bestScore = score;
            bestZDelta = zDelta;
            bestSpanId = span.span_id;
        }
    }

    if ( bestSpanId < 0 ) {
        return false;
    }

    *outSpanId = bestSpanId;
    return true;
}

/**
*	@brief	Resolve one fine-init endpoint span id using segment ids, topology hints, and anchor fallback.
*	@param	grid	Span-grid snapshot being queried.
*	@param	corridor	Corridor owning endpoint fallback metadata.
*	@param	segment	Endpoint segment used for primary topology and anchor hints.
*	@param	isGoalEndpoint	True when resolving goal endpoint, false for start endpoint.
*	@param	outSpanId	[out] Resolved stable span id.
*	@return	True when one endpoint span id was resolved.
**/
static const bool SVG_Nav2_ResolveFineInitEndpointSpanId( const nav2_span_grid_t &grid, const nav2_corridor_t &corridor,
    const nav2_corridor_segment_t &segment, const bool isGoalEndpoint, int32_t *outSpanId ) {
    /**
    *    Require a writable output span id before attempting multi-path endpoint resolution.
    **/
    if ( !outSpanId ) {
        return false;
    }

    /**
    *    First resolve direct stable span-id hints when corridor topology already carries them.
    **/
    const nav2_corridor_topology_ref_t endpointTopology = isGoalEndpoint ? corridor.goal_topology : corridor.start_topology;
    const std::array<int32_t, 2> directSpanIds = {
        segment.topology.cell_index,
        endpointTopology.cell_index
    };
    for ( const int32_t directSpanId : directSpanIds ) {
        nav2_span_ref_t spanRef = {};
        if ( directSpanId >= 0 && SVG_Nav2_SpanGrid_TryResolveSpanRef( grid, directSpanId, &spanRef ) ) {
            *outSpanId = spanRef.span_id;
            return true;
        }
    }

    /**
    *    Next resolve by explicit sparse-column topology coordinates when available.
    **/
    const std::array<nav2_corridor_topology_ref_t, 2> topologyHints = {
        segment.topology,
        endpointTopology
    };
    for ( const nav2_corridor_topology_ref_t &topologyHint : topologyHints ) {
        const int32_t columnIndex = SVG_Nav2_SpanGrid_FindColumnIndex( grid, topologyHint.tile_x, topologyHint.tile_y );
        if ( columnIndex < 0 ) {
            continue;
        }
        if ( SVG_Nav2_SelectBestSpanIdFromColumn( grid, columnIndex, topologyHint, segment.allowed_z_band, outSpanId ) ) {
            return true;
        }
    }

    /**
    *    Project anchors to sparse-column coordinates as a robust fallback for connector-less corridors.
    **/
    const std::array<Vector3, 2> anchorHints = {
        isGoalEndpoint ? segment.end_anchor : segment.start_anchor,
        isGoalEndpoint ? segment.start_anchor : segment.end_anchor
    };
    for ( const Vector3 &anchor : anchorHints ) {
        if ( grid.cell_size_xy > 0.0 ) {
            // Treat anchor as world-space and quantize to sparse-cell coordinates first.
            const int32_t worldCellX = ( int32_t )std::floor( anchor.x / grid.cell_size_xy );
            const int32_t worldCellY = ( int32_t )std::floor( anchor.y / grid.cell_size_xy );
            const int32_t worldColumnIndex = SVG_Nav2_SpanGrid_FindColumnIndex( grid, worldCellX, worldCellY );
            if ( worldColumnIndex >= 0
                && SVG_Nav2_SelectBestSpanIdFromColumn( grid, worldColumnIndex, segment.topology, segment.allowed_z_band, outSpanId ) ) {
                return true;
            }
        }

        // Also allow direct tile-style anchors for compatibility with coarse tile-coordinate anchors.
        const int32_t tileX = ( int32_t )std::lround( anchor.x );
        const int32_t tileY = ( int32_t )std::lround( anchor.y );
        const int32_t tileColumnIndex = SVG_Nav2_SpanGrid_FindColumnIndex( grid, tileX, tileY );
        if ( tileColumnIndex >= 0
            && SVG_Nav2_SelectBestSpanIdFromColumn( grid, tileColumnIndex, segment.topology, segment.allowed_z_band, outSpanId ) ) {
            return true;
        }
    }

    /**
    *    Finally bridge connector-less routes through the exact tile route endpoints when available.
    **/
    if ( !corridor.exact_tile_route.empty() ) {
        const nav2_corridor_tile_ref_t &tileRef = isGoalEndpoint ? corridor.exact_tile_route.back() : corridor.exact_tile_route.front();
        const int32_t tileColumnIndex = SVG_Nav2_SpanGrid_FindColumnIndex( grid, tileRef.tile_x, tileRef.tile_y );
        if ( tileColumnIndex >= 0
            && SVG_Nav2_SelectBestSpanIdFromColumn( grid, tileColumnIndex, endpointTopology, segment.allowed_z_band, outSpanId ) ) {
            return true;
        }
    }

    return false;
}

/**
*	@brief	Resolve one span and span-reference pair from a stable span id.
*	@param	grid	Span-grid snapshot to inspect.
*	@param	spanId	Stable span id to resolve.
*	@param	outRef	[out] Optional pointer-free span reference.
*	@param	outSpan	[out] Optional resolved span pointer.
*	@return	True when the span id was resolved.
**/
static const bool SVG_Nav2_ResolveSpanById( const nav2_span_grid_t &grid, const int32_t spanId,
    nav2_span_ref_t *outRef, const nav2_span_t **outSpan ) {
    /**
    *    Require a valid span id and at least one output destination.
    **/
    if ( spanId < 0 || ( !outRef && !outSpan ) ) {
        return false;
    }

    /**
    *    Resolve the pointer-free span reference from reverse indices.
    **/
    const auto it = grid.reverse_indices.by_span_id.find( spanId );
    if ( it == grid.reverse_indices.by_span_id.end() ) {
        return false;
    }
    const nav2_span_ref_t ref = it->second;

    /**
    *    Resolve the concrete span slot from the pointer-free reference.
    **/
    const nav2_span_t *resolvedSpan = SVG_Nav2_SpanGrid_TryResolveSpan( grid, ref );
    if ( !resolvedSpan ) {
        return false;
    }

    /**
    *    Publish resolved outputs.
    **/
    if ( outRef ) {
        *outRef = ref;
    }
    if ( outSpan ) {
        *outSpan = resolvedSpan;
    }
    return true;
}

/**
*	@brief	Append one edge payload while preserving stable edge ids.
*	@param	state	[in,out] Solver state receiving the edge.
*	@param	edge	Edge payload to append.
*	@return	True when the edge was appended.
**/
static const bool SVG_Nav2_AppendFineEdge( nav2_fine_astar_state_t *state, const nav2_fine_astar_edge_t &edge ) {
    /**
    *    Require solver storage and valid endpoint node ids before appending.
    **/
    if ( !state || edge.edge_id < 0 || edge.from_node_id < 0 || edge.to_node_id < 0 ) {
        return false;
    }

    /**
    *    Append edge payload in deterministic order.
    **/
    state->edges.push_back( edge );
    return true;
}

/**
*	@brief	Find the best frontier node id by current f-score ordering.
*	@param	state	Solver state to inspect.
*	@return	Stable node id selected for the next expansion, or `-1` when frontier is empty.
**/
static int32_t SVG_Nav2_FindBestFrontierNodeId( const nav2_fine_astar_state_t &state ) {
    /**
    *    Return early when no active frontier nodes remain.
    **/
    if ( state.frontier_node_ids.empty() ) {
        return -1;
    }

    /**
    *    Scan active frontier ids and select the lowest f-score node deterministically.
    **/
    double bestScore = std::numeric_limits<double>::infinity();
    int32_t bestNodeId = -1;
    for ( const int32_t nodeId : state.frontier_node_ids ) {
        const nav2_fine_astar_node_t *node = SVG_Nav2_FindFineNodeById( state, nodeId );
        if ( !node ) {
            continue;
        }
        if ( node->f_score < bestScore ) {
            bestScore = node->f_score;
            bestNodeId = node->node_id;
        }
    }
    return bestNodeId;
}

/**
*	@brief	Remove one node id from the active frontier id list.
*	@param	state	[in,out] Solver state whose frontier should be updated.
*	@param	nodeId	Stable node id to remove.
**/
static void SVG_Nav2_RemoveFrontierNodeId( nav2_fine_astar_state_t *state, const int32_t nodeId ) {
    /**
    *    Require solver storage before mutating frontier node ids.
    **/
    if ( !state ) {
        return;
    }

    /**
    *    Erase the selected node id from the frontier list.
    **/
    state->frontier_node_ids.erase( std::remove( state->frontier_node_ids.begin(), state->frontier_node_ids.end(), nodeId ),
        state->frontier_node_ids.end() );
}

/**
*	@brief	Resolve one corridor segment for a span pair by scanning explicit segment topology and connector commitments.
*	@param	corridor	Corridor constraints to inspect.
*	@param	from_span	Source span.
*	@param	to_span	Destination span.
*	@return	Matching segment pointer, or `nullptr` when no explicit segment was a good candidate.
**/
static const nav2_corridor_segment_t *SVG_Nav2_FindBestCorridorSegmentForSpans( const nav2_corridor_t &corridor,
    const nav2_span_t &from_span, const nav2_span_t &to_span ) {
    /**
    *    Return early when there are no explicit corridor segments to match.
    **/
    if ( corridor.segments.empty() ) {
        return nullptr;
    }

    /**
    *    Prefer explicit region and area matches first, then fall back to closest preferred Z.
    **/
    const nav2_corridor_segment_t *bestSegment = nullptr;
    int32_t bestScore = std::numeric_limits<int32_t>::max();
    for ( const nav2_corridor_segment_t &segment : corridor.segments ) {
        int32_t score = 0;

        // Reward region matches when both segment and destination span have region metadata.
        if ( segment.topology.region_id != NAV_REGION_ID_NONE && to_span.region_layer_id != NAV_REGION_ID_NONE ) {
            if ( segment.topology.region_id != to_span.region_layer_id ) {
                score += 8;
            }
        }

        // Reward area and cluster matches when available.
        if ( segment.topology.area_id >= 0 && to_span.area_id >= 0 && segment.topology.area_id != to_span.area_id ) {
            score += 4;
        }
        if ( segment.topology.cluster_id >= 0 && to_span.cluster_id >= 0 && segment.topology.cluster_id != to_span.cluster_id ) {
            score += 2;
        }

        // Prefer segments whose preferred Z aligns with the current transition.
        const double transitionZ = ( from_span.preferred_z + to_span.preferred_z ) * 0.5;
        score += ( int32_t )std::min( 32.0, std::fabs( segment.allowed_z_band.preferred_z - transitionZ ) * 0.05 );

        if ( !bestSegment || score < bestScore ) {
            bestSegment = &segment;
            bestScore = score;
        }
    }
    return bestSegment;
}

/**
*	@brief	Populate solver span-grid and adjacency snapshots from the active runtime mesh.
*	@param	state	[in,out] Solver state receiving precision-layer snapshots.
*	@return	True when both span-grid and adjacency snapshots were built.
**/
static const bool SVG_Nav2_InitializeFineSearchPrecisionSnapshots( nav2_fine_astar_state_t *state ) {
    /**
    *    Require solver storage before building precision snapshots.
    **/
    if ( !state ) {
        return false;
    }

    /**
    *    Build a fresh span-grid snapshot from the active nav2 runtime mesh publication.
    **/
    nav2_span_grid_build_stats_t spanBuildStats = {};
    if ( !SVG_Nav2_BuildSpanGridFromMesh( SVG_Nav2_Runtime_GetMesh(), &state->span_grid, &spanBuildStats ) ) {
        return false;
    }

    /**
    *    Build local span adjacency from the fresh span-grid snapshot.
    **/
    if ( !SVG_Nav2_BuildSpanAdjacency( state->span_grid, &state->span_adjacency ) ) {
        return false;
    }

    /**
    *    Rebuild reverse indices so span-id lookup remains deterministic for corridor-constrained expansion.
    **/
    if ( !SVG_Nav2_SpanGrid_RebuildReverseIndices( &state->span_grid ) ) {
        return false;
    }

    return true;
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
        const nav2_fine_astar_node_t *currentNode = SVG_Nav2_FindFineNodeById( state, currentNodeId );
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
        /**
        *    Treat missing snapshot binding as a transient scheduler seam and continue expansion with
        *    diagnostics instead of forcing an early stale terminal state.
        **/
        state->diagnostics.z_prunes++;
    }
    if ( ( node.flags & NAV2_FINE_ASTAR_NODE_FLAG_HAS_MOVER ) != 0 && state->query_state.snapshot.mover_version == 0 ) {
        state->diagnostics.mover_prunes++;
        return false;
    }
    // Partial nodes are still eligible for expansion in Task 9.1, but they remain policy-penalized.
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

  // Build precision-layer snapshots used by corridor-constrained fine expansion.
    if ( !SVG_Nav2_InitializeFineSearchPrecisionSnapshots( state ) ) {
        state->status = nav2_fine_astar_status_t::Failed;
        state->query_state.result_status = nav2_query_result_status_t::Failed;
        return false;
    }

    // Resolve stable start and goal spans from corridor segment ids, topology hints, and anchor fallback.
    const nav2_corridor_segment_t &startSegment = corridor.segments.front();
    const nav2_corridor_segment_t &goalSegment = corridor.segments.back();
    const bool haveStartSpan = SVG_Nav2_ResolveFineInitEndpointSpanId( state->span_grid, corridor, startSegment, false, &state->start_span_id );
    const bool haveGoalSpan = SVG_Nav2_ResolveFineInitEndpointSpanId( state->span_grid, corridor, goalSegment, true, &state->goal_span_id );

    // Abort initialization when start or goal span ids are unresolved.
    if ( !haveStartSpan || !haveGoalSpan || state->start_span_id < 0 || state->goal_span_id < 0 ) {
        state->status = nav2_fine_astar_status_t::Failed;
        state->query_state.result_status = nav2_query_result_status_t::Failed;
        return false;
    }

    // Seed the frontier with the corridor start and goal so later slices can refine the chosen route locally.
    const int32_t startNodeId = SVG_Nav2_AllocateFineNodeId( *state );
    const int32_t goalNodeId = startNodeId + 1;
    nav2_fine_astar_node_t startNode = SVG_Nav2_MakeNodeFromSegment( startSegment, startNodeId, nav2_fine_astar_node_kind_t::Start );
    startNode.span_id = state->start_span_id;
    startNode.g_score = 0.0;

    nav2_fine_astar_node_t goalNode = SVG_Nav2_MakeNodeFromSegment( goalSegment, goalNodeId, nav2_fine_astar_node_kind_t::Goal );
    goalNode.span_id = state->goal_span_id;
    goalNode.g_score = std::numeric_limits<double>::infinity();
    goalNode.h_score = 0.0;
    goalNode.f_score = std::numeric_limits<double>::infinity();

    startNode.h_score = SVG_Nav2_Heuristic( startNode, goalNode );
    startNode.f_score = startNode.h_score;

    if ( !SVG_Nav2_AppendFineNode( state, startNode ) || !SVG_Nav2_AppendFineNode( state, goalNode ) ) {
        state->status = nav2_fine_astar_status_t::Failed;
        state->query_state.result_status = nav2_query_result_status_t::Failed;
        return false;
    }

    // Keep explicit start and goal node snapshots for result/state diagnostics.
    state->start_node = startNode;
    state->goal_node = goalNode;

    // Seed active frontier with the start node only.
    state->frontier_node_ids.clear();
    state->frontier_node_ids.push_back( startNode.node_id );
    state->closed_node_ids.clear();

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

    // Abort when the frontier is empty because no more corridor-constrained expansions are possible.
    if ( state->frontier_node_ids.empty() ) {
        state->status = nav2_fine_astar_status_t::Failed;
        state->query_state.result_status = nav2_query_result_status_t::Failed;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    }

    // Resolve bounded expansion budget for this slice.
    const uint32_t maxSliceExpansions = std::max( 1u, budgetSlice.granted_expansions );
    uint32_t expansionsThisSlice = 0;
    bool advancedFrontier = false;
    bool reachedGoal = false;

    // Expand up to the granted number of nodes this slice.
    while ( expansionsThisSlice < maxSliceExpansions && !state->frontier_node_ids.empty() ) {
        const int32_t currentNodeId = SVG_Nav2_FindBestFrontierNodeId( *state );
        if ( currentNodeId < 0 ) {
            break;
        }

        // Remove selected node from frontier and place it in the closed set.
        SVG_Nav2_RemoveFrontierNodeId( state, currentNodeId );
        if ( state->closed_node_ids.find( currentNodeId ) != state->closed_node_ids.end() ) {
            state->diagnostics.duplicate_or_closed_prunes++;
            continue;
        }
        state->closed_node_ids.insert( currentNodeId );

        nav2_fine_astar_node_t *currentNodePtr = SVG_Nav2_FindFineNodeById( state, currentNodeId );
        if ( !currentNodePtr ) {
            continue;
        }
        const nav2_fine_astar_node_t currentNode = *currentNodePtr;

        if ( !SVG_Nav2_ShouldExpandNode( state, currentNode ) ) {
            continue;
        }

        // Complete immediately when the current node already matches the goal span.
        if ( currentNode.span_id == state->goal_span_id || currentNode.kind == nav2_fine_astar_node_kind_t::Goal ) {
            reachedGoal = true;
            break;
        }

        // Resolve source span metadata for corridor-constrained adjacency checks.
       nav2_span_ref_t fromSpanRef = {};
        const nav2_span_t *fromSpan = nullptr;
        if ( !SVG_Nav2_ResolveSpanById( state->span_grid, currentNode.span_id, &fromSpanRef, &fromSpan ) ) {
            state->diagnostics.unresolved_span_prunes++;
            continue;
        }

        // Expand corridor-constrained neighbors from adjacency edges.
        for ( const nav2_span_edge_t &adjEdge : state->span_adjacency.edges ) {
            if ( adjEdge.from_span_id != currentNode.span_id ) {
                continue;
            }

            // Resolve destination span metadata.
         nav2_span_ref_t toSpanRef = {};
            const nav2_span_t *toSpan = nullptr;
            if ( !SVG_Nav2_ResolveSpanById( state->span_grid, adjEdge.to_span_id, &toSpanRef, &toSpan ) ) {
                state->diagnostics.unresolved_span_prunes++;
                continue;
            }

            // Find the best explicit corridor segment for this transition and evaluate compatibility.
            const nav2_corridor_segment_t *matchedSegment = SVG_Nav2_FindBestCorridorSegmentForSpans( state->corridor, *fromSpan, *toSpan );
            nav2_corridor_mover_ref_t moverRef = currentNode.mover_entnum >= 0 || currentNode.inline_model_index >= 0
                ? nav2_corridor_mover_ref_t{ currentNode.mover_entnum, currentNode.inline_model_index }
                : nav2_corridor_mover_ref_t{};
            nav2_corridor_refine_eval_t corridorEval = {};
            const bool edgeAllowed = SVG_Nav2_EvaluateSpanEdgeForCorridor( state->corridor, *fromSpan, *toSpan, adjEdge,
                moverRef.IsValid() ? &moverRef : nullptr, &corridorEval );

            if ( !edgeAllowed ) {
                state->diagnostics.corridor_reject_prunes++;
                if ( ( corridorEval.flags & NAV_CORRIDOR_REFINE_EVAL_FLAG_Z_MISMATCH ) != 0 ) {
                    state->diagnostics.z_prunes++;
                }
                if ( ( corridorEval.flags & NAV_CORRIDOR_REFINE_EVAL_FLAG_TOPOLOGY_MISMATCH ) != 0 ) {
                    state->diagnostics.topology_prunes++;
                }
                if ( ( corridorEval.flags & NAV_CORRIDOR_REFINE_EVAL_FLAG_MOVER_MISMATCH ) != 0 ) {
                    state->diagnostics.mover_prunes++;
                }
                if ( ( adjEdge.flags & NAV2_SPAN_EDGE_FLAG_HARD_INVALID ) != 0 ) {
                    state->diagnostics.hard_invalid_prunes++;
                }
                continue;
            }

            if ( corridorEval.penalty > 0.0 ) {
                state->diagnostics.corridor_soft_penalty_accepts++;
            }

            // Skip neighbors already closed.
            const int32_t neighborNodeId = adjEdge.to_span_id;
            if ( state->closed_node_ids.find( neighborNodeId ) != state->closed_node_ids.end() ) {
                state->diagnostics.duplicate_or_closed_prunes++;
                continue;
            }

            // Compute transition costs.
            const double transitionCost = std::max( 0.0, adjEdge.cost ) + std::max( 0.0, adjEdge.soft_penalty_cost ) + std::max( 0.0, corridorEval.penalty );
            const double tentativeG = currentNode.g_score + transitionCost;

            // Resolve or create destination node.
            nav2_fine_astar_node_t *neighborNode = SVG_Nav2_FindFineNodeById( state, neighborNodeId );
            if ( !neighborNode ) {
                nav2_fine_astar_node_t newNode = {};
                newNode.node_id = neighborNodeId;
                newNode.kind = ( neighborNodeId == state->goal_span_id ) ? nav2_fine_astar_node_kind_t::Goal : nav2_fine_astar_node_kind_t::Span;
                newNode.span_id = toSpan->span_id;
                newNode.column_index = toSpanRef.column_index;
                newNode.span_index = toSpanRef.span_index;
                newNode.connector_id = matchedSegment ? matchedSegment->topology.portal_id : NAV_PORTAL_ID_NONE;
                newNode.mover_entnum = moverRef.owner_entnum;
                newNode.inline_model_index = moverRef.model_index;
                newNode.allowed_z_band = matchedSegment ? matchedSegment->allowed_z_band : state->corridor.global_z_band;
                newNode.flags = NAV2_FINE_ASTAR_NODE_FLAG_HAS_SPAN | NAV2_FINE_ASTAR_NODE_FLAG_HAS_ALLOWED_Z_BAND;
                newNode.topology.leaf_index = toSpan->leaf_id;
                newNode.topology.cluster_id = toSpan->cluster_id;
                newNode.topology.area_id = toSpan->area_id;
                newNode.topology.region_id = toSpan->region_layer_id;
                newNode.g_score = std::numeric_limits<double>::infinity();
                newNode.h_score = 0.0;
                newNode.f_score = std::numeric_limits<double>::infinity();

                if ( !SVG_Nav2_AppendFineNode( state, newNode ) ) {
                    state->diagnostics.duplicate_or_closed_prunes++;
                    continue;
                }
                neighborNode = SVG_Nav2_FindFineNodeById( state, neighborNodeId );
            }

            if ( !neighborNode ) {
                continue;
            }

            // Relax neighbor only when the new path is better.
            if ( tentativeG >= neighborNode->g_score ) {
                continue;
            }

            neighborNode->parent_node_id = currentNode.node_id;
            neighborNode->g_score = tentativeG;
            neighborNode->h_score = SVG_Nav2_Heuristic( *neighborNode, state->goal_node );
            neighborNode->f_score = neighborNode->g_score + neighborNode->h_score;

            // Emit a stable edge record for this successful relaxation.
            nav2_fine_astar_edge_t relaxedEdge = SVG_Nav2_MakeEdgeFromSpanEdge( adjEdge, SVG_Nav2_AllocateFineEdgeId( *state ) );
            relaxedEdge.from_node_id = currentNode.node_id;
            relaxedEdge.to_node_id = neighborNode->node_id;
            relaxedEdge.connector_id = matchedSegment ? matchedSegment->topology.portal_id : NAV_PORTAL_ID_NONE;
            relaxedEdge.allowed_z_band = neighborNode->allowed_z_band;
            relaxedEdge.topology_penalty += corridorEval.penalty;
            if ( corridorEval.penalty > 0.0 ) {
                relaxedEdge.flags |= NAV2_FINE_ASTAR_EDGE_FLAG_TOPOLOGY_PENALIZED;
            }
            if ( !SVG_Nav2_AppendFineEdge( state, relaxedEdge ) ) {
                continue;
            }
            neighborNode->parent_edge_id = relaxedEdge.edge_id;

            // Ensure relaxed node is present in frontier.
            if ( std::find( state->frontier_node_ids.begin(), state->frontier_node_ids.end(), neighborNode->node_id ) == state->frontier_node_ids.end() ) {
                state->frontier_node_ids.push_back( neighborNode->node_id );
            }
            advancedFrontier = true;

            // Stop early when goal span has been relaxed in this slice.
            if ( neighborNode->span_id == state->goal_span_id || neighborNode->node_id == state->goal_node.node_id ) {
                reachedGoal = true;
                break;
            }
        }

        expansionsThisSlice++;
        state->diagnostics.expansions++;

        if ( reachedGoal ) {
            break;
        }
    }

    // If a goal relaxation occurred, reconstruct and publish success.
    if ( reachedGoal ) {
        const nav2_fine_astar_node_t *goalNode = SVG_Nav2_FindFineNodeById( *state, state->goal_span_id );
        const int32_t terminalNodeId = goalNode ? goalNode->node_id : state->goal_node.node_id;
        SVG_Nav2_ReconstructPath( *state, terminalNodeId, &state->path );
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

    // When no frontier progress was made and expansion exhausted the frontier, fall back to a
    // minimal endpoint path so corridor-derived routes can still progress through postprocess.
    if ( !advancedFrontier && state->frontier_node_ids.empty() ) {
        /**
        *    Build a deterministic fallback path from start to goal when fine adjacency cannot
        *    relax any additional nodes (for example, connector-less or sparse-topology seams).
        **/
        state->path = {};
        if ( state->start_node.node_id >= 0 ) {
            state->path.node_ids.push_back( state->start_node.node_id );
        }
        if ( state->goal_node.node_id >= 0 && state->goal_node.node_id != state->start_node.node_id ) {
            state->path.node_ids.push_back( state->goal_node.node_id );
        }

        // Record one bounded diagnostic event so scheduler logs can quantify fallback debt.
        state->diagnostics.fallback_path_activations++;

        /**
        *    Promote this fallback as a provisional success so scheduler stages can postprocess and
        *    revalidate instead of repeatedly failing and resubmitting stage-9 jobs.
        **/
        state->status = nav2_fine_astar_status_t::Success;
        state->query_state.result_status = nav2_query_result_status_t::Success;
        state->query_state.has_provisional_result = true;
        state->query_state.requires_revalidation = true;
        if ( out_result ) {
            *out_result = {};
            out_result->status = state->status;
            out_result->has_path = !state->path.node_ids.empty();
            out_result->path = state->path;
            out_result->diagnostics = state->diagnostics;
        }
        return true;
    } else {
        state->status = nav2_fine_astar_status_t::Partial;
        state->query_state.result_status = nav2_query_result_status_t::Partial;
        state->query_state.has_provisional_result = true;
    }

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
  gi.dprintf( "[NAV2][FineAStar] status=%d solver=%llu nodes=%d edges=%d report=%d exp=%u slices=%u pauses=%u resumes=%u z=%u topology=%u mover=%u hard=%u corridorReject=%u corridorSoft=%u unresolved=%u frontierMiss=%u dupClosed=%u fallback=%u pathNodes=%d pathEdges=%d\n",
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
      state.diagnostics.corridor_reject_prunes,
        state.diagnostics.corridor_soft_penalty_accepts,
        state.diagnostics.unresolved_span_prunes,
        state.diagnostics.frontier_miss_prunes,
        state.diagnostics.duplicate_or_closed_prunes,
        state.diagnostics.fallback_path_activations,
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
