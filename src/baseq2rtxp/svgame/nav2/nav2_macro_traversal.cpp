/********************************************************************
*
*
*	ServerGame: Nav2 Macro Traversal Aids - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_macro_traversal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>


/**
*
*
*	Nav2 Macro Traversal Local Helpers:
*
*
**/
//! Default fallback cell-size used when deriving world-space region centers without a populated span grid.
static constexpr double NAV2_MACRO_FALLBACK_CELL_SIZE_XY = 32.0;

/**
*	@brief	Return whether one connector kind bit is present on a connector record.
*	@param	connector	Connector record to inspect.
*	@param	kind	Kind bit to test.
*	@return	True when the connector contains the requested kind bit.
**/
static inline const bool SVG_Nav2_MacroConnectorHasKind( const nav2_connector_t &connector, const uint32_t kind ) {
    return ( connector.connector_kind & kind ) != 0;
}

/**
*	@brief	Resolve one stable region-layer index from a region-layer graph.
*	@param	graph	Region-layer graph to inspect.
*	@param	region_layer_id	Stable region-layer id.
*	@return	Region-layer index when available, or `-1` when unresolved.
**/
static int32_t SVG_Nav2_MacroResolveRegionLayerIndex( const nav2_region_layer_graph_t &graph, const int32_t region_layer_id ) {
    /**
    *    Resolve the stable reference through the existing region-layer lookup first.
    **/
    nav2_region_layer_ref_t layerRef = {};
    if ( !SVG_Nav2_RegionLayerGraph_TryResolve( graph, region_layer_id, &layerRef ) ) {
        return -1;
    }
    return layerRef.region_layer_index;
}

/**
*	@brief	Derive one world-space representative point for a region-layer node.
*	@param	layer	Region-layer node to localize.
*	@return	World-space representative point.
**/
static Vector3 SVG_Nav2_MacroBuildRegionWorldOrigin( const nav2_region_layer_t &layer ) {
    /**
    *    Prefer tile-local localization when tile metadata is available.
    **/
    const double cellSizeXY = NAV2_MACRO_FALLBACK_CELL_SIZE_XY;
    Vector3 origin = {};
    origin.x = ( float )( ( ( double )layer.tile_ref.tile_x + 0.5 ) * cellSizeXY );
    origin.y = ( float )( ( ( double )layer.tile_ref.tile_y + 0.5 ) * cellSizeXY );
    origin.z = ( float )layer.allowed_z_band.preferred_z;
    return origin;
}

/**
*	@brief	Append one macro node into the output graph with stable id registration.
*	@param	graph	[in,out] Macro graph receiving the node.
*	@param	node	Node payload to append.
*	@return	True when append and id registration succeeded.
**/
static const bool SVG_Nav2_MacroAppendNode( nav2_macro_graph_t *graph, const nav2_macro_node_t &node ) {
    /**
    *    Require output graph storage before appending node data.
    **/
    if ( !graph || node.macro_node_id < 0 ) {
        return false;
    }

    /**
    *    Avoid duplicate stable node ids in the same macro graph.
    **/
    if ( graph->node_index_by_id.find( node.macro_node_id ) != graph->node_index_by_id.end() ) {
        return false;
    }

    /**
    *    Register id lookup and append deterministic node storage.
    **/
    graph->node_index_by_id[ node.macro_node_id ] = ( int32_t )graph->nodes.size();
    graph->nodes.push_back( node );
    return true;
}

/**
*	@brief	Append one macro edge into the output graph with adjacency linkage.
*	@param	graph	[in,out] Macro graph receiving the edge.
*	@param	edge	Edge payload to append.
*	@return	True when append and adjacency linkage succeeded.
**/
static const bool SVG_Nav2_MacroAppendEdge( nav2_macro_graph_t *graph, const nav2_macro_edge_t &edge ) {
    /**
    *    Require output graph storage and valid edge endpoints before appending.
    **/
    if ( !graph || edge.macro_edge_id < 0 || edge.from_macro_node_id < 0 || edge.to_macro_node_id < 0 ) {
        return false;
    }

    /**
    *    Avoid duplicate stable edge ids.
    **/
    if ( graph->edge_index_by_id.find( edge.macro_edge_id ) != graph->edge_index_by_id.end() ) {
        return false;
    }

    /**
    *    Resolve both endpoint indices so adjacency lists can be updated deterministically.
    **/
    const auto fromIt = graph->node_index_by_id.find( edge.from_macro_node_id );
    const auto toIt = graph->node_index_by_id.find( edge.to_macro_node_id );
    if ( fromIt == graph->node_index_by_id.end() || toIt == graph->node_index_by_id.end() ) {
        return false;
    }
    const int32_t fromIndex = fromIt->second;
    const int32_t toIndex = toIt->second;
    if ( fromIndex < 0 || fromIndex >= ( int32_t )graph->nodes.size() || toIndex < 0 || toIndex >= ( int32_t )graph->nodes.size() ) {
        return false;
    }

    /**
    *    Register edge lookup, append the edge, and wire adjacency ids on both endpoints.
    **/
    graph->edge_index_by_id[ edge.macro_edge_id ] = ( int32_t )graph->edges.size();
    graph->edges.push_back( edge );
    graph->nodes[ ( size_t )fromIndex ].outgoing_edge_ids.push_back( edge.macro_edge_id );
    graph->nodes[ ( size_t )toIndex ].incoming_edge_ids.push_back( edge.macro_edge_id );
    return true;
}

/**
*	@brief	Build one macro-node semantic kind from region-layer metadata.
*	@param	layer	Region-layer node to classify.
*	@return	Macro-node semantic kind for the region-layer payload.
**/
static nav2_macro_node_kind_t SVG_Nav2_MacroClassifyNodeKindFromLayer( const nav2_region_layer_t &layer ) {
    /**
    *    Prefer explicit connector-derived semantic kinds first.
    **/
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_PORTAL ) != 0 ) {
        return nav2_macro_node_kind_t::PortalAnchor;
    }
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_STAIR ) != 0 ) {
        return nav2_macro_node_kind_t::StairAnchor;
    }
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_LADDER ) != 0 ) {
        return nav2_macro_node_kind_t::LadderAnchor;
    }

    /**
    *    Prefer bottleneck-marked layers before general open-area classification.
    **/
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_PARTIAL ) != 0 ) {
        return nav2_macro_node_kind_t::CorridorRun;
    }

    /**
    *    Use open-area region as the default macro-node kind.
    **/
    return nav2_macro_node_kind_t::OpenAreaRegion;
}

/**
*	@brief	Build one macro-edge semantic kind from region-layer edge metadata.
*	@param	edge	Region-layer edge to classify.
*	@return	Macro-edge semantic kind.
**/
static nav2_macro_edge_kind_t SVG_Nav2_MacroClassifyEdgeKindFromRegionEdge( const nav2_region_layer_edge_t &edge ) {
    /**
    *    Prefer connector-derived semantic kinds before fallback run compression.
    **/
    if ( ( edge.flags & NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_PORTAL ) != 0 ) {
        return nav2_macro_edge_kind_t::PortalGuidance;
    }
    if ( ( edge.flags & NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_STAIR ) != 0 ) {
        return nav2_macro_edge_kind_t::StairGuidance;
    }
    if ( ( edge.flags & NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_LADDER ) != 0 ) {
        return nav2_macro_edge_kind_t::LadderGuidance;
    }

    /**
    *    Classify same-band links as region compression and others as fallback guidance links.
    **/
    if ( edge.kind == nav2_region_layer_edge_kind_t::SameBand ) {
        return nav2_macro_edge_kind_t::RegionCompression;
    }
    return nav2_macro_edge_kind_t::FallbackLink;
}

/**
*	@brief	Compute one macro-edge score from base cost, utility guidance, and policy settings.
*	@param	from_node	Source macro-node.
*	@param	to_node	Destination macro-node.
*	@param	params	Build policy settings.
*	@param	out_cost	[out] Macro-cost output.
*	@param	out_penalty	[out] Macro-penalty output.
**/
static void SVG_Nav2_MacroComputeEdgeScore( const nav2_macro_node_t &from_node, const nav2_macro_node_t &to_node,
    const nav2_macro_traversal_build_params_t &params, double *out_cost, double *out_penalty ) {
    /**
    *    Require output storage before composing score values.
    **/
    if ( !out_cost || !out_penalty ) {
        return;
    }

    /**
    *    Start from conservative Euclidean distance and then blend utility guidance into penalties.
    **/
    const double dx = ( double )to_node.world_origin.x - ( double )from_node.world_origin.x;
    const double dy = ( double )to_node.world_origin.y - ( double )from_node.world_origin.y;
    const double dz = ( double )to_node.world_origin.z - ( double )from_node.world_origin.z;
    *out_cost = std::sqrt( ( dx * dx ) + ( dy * dy ) + ( dz * dz ) );
    *out_penalty = 0.0;

    /**
    *    Apply centerline preference and bottleneck avoidance from distance-field metrics.
    **/
    if ( from_node.centerline_distance >= params.centerline_preference_min_distance ) {
        *out_penalty += from_node.centerline_distance * 0.05;
    }
    if ( from_node.clearance_bottleneck_distance < params.clearance_bottleneck_min_distance ) {
        *out_penalty += ( params.clearance_bottleneck_min_distance - from_node.clearance_bottleneck_distance ) * 0.25;
    }

    /**
    *    Apply tie-break guidance scaling from seeded distance-field scores.
    **/
    *out_penalty += std::max( 0.0, from_node.heuristic_tie_breaker ) * std::max( 0.0, params.distance_field_score_scale ) * 0.02;

    /**
    *    Apply optional fat-PVS relevance bonus as a small negative penalty when configured.
    **/
    if ( params.use_fat_pvs_relevance_bias && ( from_node.flags & NAV2_MACRO_NODE_FLAG_FAT_PVS_RELEVANT ) != 0 ) {
        *out_penalty -= 4.0;
    }
}

/**
*	@brief	Return whether two macro nodes satisfy corridor constraints for edge creation.
*	@param	from_node	Source macro node.
*	@param	to_node	Destination macro node.
*	@param	corridor	Optional corridor constraints.
*	@param	enforce_constraints	True when constraints should be enforced.
*	@return	True when edge creation is allowed.
**/
static const bool SVG_Nav2_MacroAllowEdgeByCorridor( const nav2_macro_node_t &from_node, const nav2_macro_node_t &to_node,
    const nav2_corridor_t *corridor, const bool enforce_constraints ) {
    /**
    *    Allow edges unconditionally when corridor constraints are disabled or unavailable.
    **/
    if ( !enforce_constraints || !corridor ) {
        return true;
    }

    /**
    *    Use coarse region-path membership as the corridor constraint gate when available.
    **/
    if ( corridor->region_path.empty() ) {
        return true;
    }

    bool fromAllowed = false;
    bool toAllowed = false;
    for ( const int32_t regionId : corridor->region_path ) {
        if ( regionId == from_node.region_layer_id ) {
            fromAllowed = true;
        }
        if ( regionId == to_node.region_layer_id ) {
            toAllowed = true;
        }
        if ( fromAllowed && toAllowed ) {
            return true;
        }
    }
    return false;
}

/**
*	@brief	Accumulate one node into macro summary counters.
*	@param	node	Node payload to accumulate.
*	@param	summary	[in,out] Summary output.
**/
static void SVG_Nav2_MacroAccumulateNodeSummary( const nav2_macro_node_t &node, nav2_macro_traversal_summary_t *summary ) {
    if ( !summary ) {
        return;
    }

    summary->node_count++;
    switch ( node.kind ) {
        case nav2_macro_node_kind_t::OpenAreaRegion:
            summary->open_area_node_count++;
            break;
        case nav2_macro_node_kind_t::CorridorRun:
            summary->corridor_run_node_count++;
            break;
        case nav2_macro_node_kind_t::PortalAnchor:
            summary->portal_node_count++;
            break;
        case nav2_macro_node_kind_t::StairAnchor:
            summary->stair_node_count++;
            break;
        case nav2_macro_node_kind_t::LadderAnchor:
            summary->ladder_node_count++;
            break;
        default:
            break;
    }
    if ( ( node.flags & NAV2_MACRO_NODE_FLAG_FAT_PVS_RELEVANT ) != 0 ) {
        summary->fat_pvs_relevant_node_count++;
    }
}

/**
*	@brief	Accumulate one edge into macro summary counters.
*	@param	edge	Edge payload to accumulate.
*	@param	summary	[in,out] Summary output.
*	@param	total_cost	[in,out] Running cost sum.
*	@param	total_penalty	[in,out] Running penalty sum.
**/
static void SVG_Nav2_MacroAccumulateEdgeSummary( const nav2_macro_edge_t &edge, nav2_macro_traversal_summary_t *summary,
    double *total_cost, double *total_penalty ) {
    if ( !summary || !total_cost || !total_penalty ) {
        return;
    }

    summary->edge_count++;
    *total_cost += edge.macro_cost;
    *total_penalty += edge.macro_penalty;
    if ( ( edge.flags & NAV2_MACRO_EDGE_FLAG_CENTERLINE_GUIDED ) != 0 ) {
        summary->centerline_edge_count++;
    }
    if ( ( edge.flags & NAV2_MACRO_EDGE_FLAG_DIRECTIONAL_RUN ) != 0 ) {
        summary->directional_run_edge_count++;
    }
    if ( ( edge.flags & NAV2_MACRO_EDGE_FLAG_CORRIDOR_CONSTRAINED ) != 0 ) {
        summary->corridor_constrained_edge_count++;
    }
}


/**
*
*
*	Nav2 Macro Traversal Public API:
*
*
**/
/**
*	@brief	Keep the nav2 macro traversal module represented in the build.
**/
void SVG_Nav2_MacroTraversal_ModuleAnchor( void ) {
}

/**
*	@brief	Reset one macro graph to deterministic defaults.
*	@param	graph	[in,out] Macro graph to clear.
**/
void SVG_Nav2_MacroTraversal_Clear( nav2_macro_graph_t *graph ) {
    /**
    *    Require mutable storage before clearing macro traversal data.
    **/
    if ( !graph ) {
        return;
    }

    /**
    *    Reset build params and all pointer-free graph containers.
    **/
    graph->params = {};
    graph->nodes.clear();
    graph->edges.clear();
    graph->node_index_by_id.clear();
    graph->edge_index_by_id.clear();
}

/**
*	@brief	Build macro traversal aids from region layers, connectors, corridor constraints, and distance fields.
*	@param	region_layers	Region-layer graph used as a topological substrate.
*	@param	connectors	Connector list used for anchor semantics.
*	@param	corridor	Optional corridor constraints.
*	@param	distance_fields	Distance-field guidance records.
*	@param	params	Build policy parameters.
*	@param	out_graph	[out] Macro graph receiving generated nodes and edges.
*	@param	out_summary	[out] Optional bounded summary diagnostics.
*	@return	True when at least one macro node was produced.
*	@note	Macro traversal aids are optimization-only and must not replace topology legality or corridor commitments.
**/
const bool SVG_Nav2_BuildMacroTraversalGraph( const nav2_region_layer_graph_t &region_layers, const nav2_connector_list_t &connectors,
    const nav2_corridor_t *corridor, const nav2_distance_field_set_t &distance_fields, const nav2_macro_traversal_build_params_t &params,
    nav2_macro_graph_t *out_graph, nav2_macro_traversal_summary_t *out_summary ) {
    /**
    *    Require output graph storage and at least one region-layer node.
    **/
    if ( !out_graph || region_layers.layers.empty() ) {
        return false;
    }

    /**
    *    Reset output graph and initialize build params/summary storage.
    **/
    SVG_Nav2_MacroTraversal_Clear( out_graph );
    out_graph->params = params;
    if ( out_summary ) {
        *out_summary = {};
    }
    double totalEdgeCost = 0.0;
    double totalEdgePenalty = 0.0;

    /**
    *    Build one macro node per region layer and mirror available distance-field guidance.
    **/
    for ( int32_t layerIndex = 0; layerIndex < ( int32_t )region_layers.layers.size(); layerIndex++ ) {
        const nav2_region_layer_t &layer = region_layers.layers[ ( size_t )layerIndex ];

        // Build stable node identity from region-layer identity when available.
        nav2_macro_node_t macroNode = {};
        macroNode.macro_node_id = ( layer.region_layer_id >= 0 ) ? layer.region_layer_id : layerIndex;
        macroNode.kind = SVG_Nav2_MacroClassifyNodeKindFromLayer( layer );
        macroNode.region_layer_id = layer.region_layer_id;
        macroNode.connector_id = layer.connector_id;
        macroNode.topology = layer.topology;
        macroNode.world_origin = SVG_Nav2_MacroBuildRegionWorldOrigin( layer );
        macroNode.flags = NAV2_MACRO_NODE_FLAG_HAS_REGION_LAYER;
        if ( layer.connector_id >= 0 ) {
            macroNode.flags |= NAV2_MACRO_NODE_FLAG_HAS_CONNECTOR;
        }
        if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_PORTAL ) != 0 ) {
            macroNode.flags |= NAV2_MACRO_NODE_FLAG_HAS_PORTAL;
        }
        if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_STAIR ) != 0 ) {
            macroNode.flags |= NAV2_MACRO_NODE_FLAG_HAS_STAIR;
        }
        if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_LADDER ) != 0 ) {
            macroNode.flags |= NAV2_MACRO_NODE_FLAG_HAS_LADDER;
        }

        /**
        *    Seed utility guidance from distance fields by matching region-layer ids first and falling back to first match.
        **/
        const nav2_distance_field_record_t *seedRecord = nullptr;
        for ( const nav2_distance_field_record_t &record : distance_fields.records ) {
            if ( record.region_layer_id == layer.region_layer_id ) {
                seedRecord = &record;
                break;
            }
        }
        if ( !seedRecord && !distance_fields.records.empty() ) {
            seedRecord = &distance_fields.records.front();
        }
        if ( seedRecord ) {
            macroNode.seed_span_id = seedRecord->span_id;
            macroNode.portal_distance = seedRecord->distance_to_portal_endpoint;
            macroNode.stair_distance = seedRecord->distance_to_stair_connector;
            macroNode.ladder_distance = seedRecord->distance_to_ladder_endpoint;
            macroNode.centerline_distance = seedRecord->distance_to_open_space_centerline;
            macroNode.obstacle_distance = seedRecord->distance_to_obstacle_boundary;
            macroNode.clearance_bottleneck_distance = seedRecord->distance_to_clearance_bottleneck;
            macroNode.heuristic_tie_breaker = seedRecord->heuristic_tie_breaker;
            if ( ( seedRecord->flags & NAV2_DISTANCE_FIELD_FLAG_HAS_FAT_PVS_RELEVANCE ) != 0 ) {
                macroNode.flags |= NAV2_MACRO_NODE_FLAG_FAT_PVS_RELEVANT;
            }
            if ( ( seedRecord->flags & NAV2_DISTANCE_FIELD_FLAG_HAS_CENTERLINE_DISTANCE ) != 0 ) {
                macroNode.flags |= NAV2_MACRO_NODE_FLAG_HAS_CENTERLINE;
            }
            if ( ( seedRecord->flags & NAV2_DISTANCE_FIELD_FLAG_HAS_CLEARANCE_BOTTLENECK_DISTANCE ) != 0 ) {
                macroNode.flags |= NAV2_MACRO_NODE_FLAG_HAS_BOTTLENECK;
            }
        }

        /**
        *    Append macro node and update bounded summary metrics.
        **/
        if ( !SVG_Nav2_MacroAppendNode( out_graph, macroNode ) ) {
            continue;
        }
        if ( out_summary ) {
            SVG_Nav2_MacroAccumulateNodeSummary( macroNode, out_summary );
        }
    }

    /**
    *    Build macro edges from region-layer edges while preserving corridor constraints and utility-guided scoring.
    **/
    int32_t nextMacroEdgeId = 0;
    for ( const nav2_region_layer_edge_t &regionEdge : region_layers.edges ) {
        const int32_t fromRegionIndex = SVG_Nav2_MacroResolveRegionLayerIndex( region_layers, regionEdge.from_region_layer_id );
        const int32_t toRegionIndex = SVG_Nav2_MacroResolveRegionLayerIndex( region_layers, regionEdge.to_region_layer_id );
        if ( fromRegionIndex < 0 || toRegionIndex < 0 ) {
            continue;
        }
        if ( fromRegionIndex >= ( int32_t )region_layers.layers.size() || toRegionIndex >= ( int32_t )region_layers.layers.size() ) {
            continue;
        }

        const nav2_region_layer_t &fromLayer = region_layers.layers[ ( size_t )fromRegionIndex ];
        const nav2_region_layer_t &toLayer = region_layers.layers[ ( size_t )toRegionIndex ];
        const nav2_macro_node_t *fromMacroNode = SVG_Nav2_MacroTraversal_TryResolveNode( *out_graph,
            ( fromLayer.region_layer_id >= 0 ) ? fromLayer.region_layer_id : fromRegionIndex );
        const nav2_macro_node_t *toMacroNode = SVG_Nav2_MacroTraversal_TryResolveNode( *out_graph,
            ( toLayer.region_layer_id >= 0 ) ? toLayer.region_layer_id : toRegionIndex );
        if ( !fromMacroNode || !toMacroNode ) {
            continue;
        }

        /**
        *    Enforce optional corridor membership constraints before edge creation.
        **/
        if ( !SVG_Nav2_MacroAllowEdgeByCorridor( *fromMacroNode, *toMacroNode, corridor, params.enforce_corridor_constraints ) ) {
            continue;
        }

        /**
        *    Build macro-edge semantics and utility-guided cost/penalty estimates.
        **/
        nav2_macro_edge_t macroEdge = {};
        macroEdge.macro_edge_id = nextMacroEdgeId++;
        macroEdge.from_macro_node_id = fromMacroNode->macro_node_id;
        macroEdge.to_macro_node_id = toMacroNode->macro_node_id;
        macroEdge.kind = SVG_Nav2_MacroClassifyEdgeKindFromRegionEdge( regionEdge );
        macroEdge.flags = NAV2_MACRO_EDGE_FLAG_PASSABLE;
        if ( params.enforce_corridor_constraints && corridor ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_CORRIDOR_CONSTRAINED;
        }
        if ( macroEdge.kind == nav2_macro_edge_kind_t::RegionCompression ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_REGION_COMPRESSED;
        }
        if ( macroEdge.kind == nav2_macro_edge_kind_t::PortalGuidance ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_PORTAL_GUIDED;
        }
        if ( macroEdge.kind == nav2_macro_edge_kind_t::StairGuidance ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_STAIR_GUIDED;
        }
        if ( macroEdge.kind == nav2_macro_edge_kind_t::LadderGuidance ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_LADDER_GUIDED;
        }
        if ( fromMacroNode->centerline_distance >= params.centerline_preference_min_distance ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_CENTERLINE_GUIDED;
        }
        if ( std::abs( fromLayer.tile_ref.tile_x - toLayer.tile_ref.tile_x ) + std::abs( fromLayer.tile_ref.tile_y - toLayer.tile_ref.tile_y ) >= 2 ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_DIRECTIONAL_RUN;
        }
        if ( ( regionEdge.flags & NAV2_REGION_LAYER_EDGE_FLAG_TOPOLOGY_PENALIZED ) != 0 ) {
            macroEdge.flags |= NAV2_MACRO_EDGE_FLAG_TOPOLOGY_PENALIZED;
        }
        macroEdge.connector_id = regionEdge.connector_id;
     // Mirror the source region-layer id so diagnostics and estimate queries can map this macro edge back to topology ownership.
        macroEdge.region_layer_id = regionEdge.from_region_layer_id;
        macroEdge.mover_ref = regionEdge.mover_ref;
        SVG_Nav2_MacroComputeEdgeScore( *fromMacroNode, *toMacroNode, params, &macroEdge.macro_cost, &macroEdge.macro_penalty );
        macroEdge.macro_cost += regionEdge.base_cost;
        macroEdge.macro_penalty += regionEdge.topology_penalty;

        /**
        *    Append macro edge and update bounded summary metrics.
        **/
        if ( !SVG_Nav2_MacroAppendEdge( out_graph, macroEdge ) ) {
            continue;
        }
        if ( out_summary ) {
            SVG_Nav2_MacroAccumulateEdgeSummary( macroEdge, out_summary, &totalEdgeCost, &totalEdgePenalty );
        }
    }

    /**
    *    Finalize average summary values when summary output was requested.
    **/
    if ( out_summary && out_summary->edge_count > 0 ) {
        out_summary->average_macro_cost = totalEdgeCost / ( double )out_summary->edge_count;
        out_summary->average_macro_penalty = totalEdgePenalty / ( double )out_summary->edge_count;
    }

    /**
    *    Report success only when at least one macro node was produced.
    **/
    return !out_graph->nodes.empty();
}

/**
*	@brief	Resolve one macro-node record by stable id.
*	@param	graph	Macro graph to inspect.
*	@param	macro_node_id	Stable macro-node id.
*	@return	Pointer to the macro node when found, or `nullptr` otherwise.
**/
const nav2_macro_node_t *SVG_Nav2_MacroTraversal_TryResolveNode( const nav2_macro_graph_t &graph, const int32_t macro_node_id ) {
    /**
    *    Resolve node index through stable lookup map first.
    **/
    const auto nodeIt = graph.node_index_by_id.find( macro_node_id );
    if ( nodeIt == graph.node_index_by_id.end() ) {
        return nullptr;
    }

    /**
    *    Validate resolved index before returning node storage.
    **/
    const int32_t nodeIndex = nodeIt->second;
    if ( nodeIndex < 0 || nodeIndex >= ( int32_t )graph.nodes.size() ) {
        return nullptr;
    }
    return &graph.nodes[ ( size_t )nodeIndex ];
}

/**
*	@brief	Resolve one macro-edge record by stable id.
*	@param	graph	Macro graph to inspect.
*	@param	macro_edge_id	Stable macro-edge id.
*	@return	Pointer to the macro edge when found, or `nullptr` otherwise.
**/
const nav2_macro_edge_t *SVG_Nav2_MacroTraversal_TryResolveEdge( const nav2_macro_graph_t &graph, const int32_t macro_edge_id ) {
    /**
    *    Resolve edge index through stable lookup map first.
    **/
    const auto edgeIt = graph.edge_index_by_id.find( macro_edge_id );
    if ( edgeIt == graph.edge_index_by_id.end() ) {
        return nullptr;
    }

    /**
    *    Validate resolved index before returning edge storage.
    **/
    const int32_t edgeIndex = edgeIt->second;
    if ( edgeIndex < 0 || edgeIndex >= ( int32_t )graph.edges.size() ) {
        return nullptr;
    }
    return &graph.edges[ ( size_t )edgeIndex ];
}

/**
*	@brief	Build a bounded traversal estimate between two macro nodes.
*	@param	graph	Macro graph to inspect.
*	@param	from_macro_node_id	Source macro-node id.
*	@param	to_macro_node_id	Destination macro-node id.
*	@param	out_estimate	[out] Estimate payload receiving aggregate traversal guidance.
*	@return	True when an estimate could be produced.
**/
const bool SVG_Nav2_MacroTraversal_BuildEstimate( const nav2_macro_graph_t &graph, const int32_t from_macro_node_id,
    const int32_t to_macro_node_id, nav2_macro_traversal_estimate_t *out_estimate ) {
    /**
    *    Require output storage before computing traversal estimates.
    **/
    if ( !out_estimate ) {
        return false;
    }
    *out_estimate = {};

    /**
    *    Resolve source and destination nodes first.
    **/
    const nav2_macro_node_t *fromNode = SVG_Nav2_MacroTraversal_TryResolveNode( graph, from_macro_node_id );
    const nav2_macro_node_t *toNode = SVG_Nav2_MacroTraversal_TryResolveNode( graph, to_macro_node_id );
    if ( !fromNode || !toNode ) {
        return false;
    }

    /**
    *    Run a bounded Dijkstra-style traversal using macro-edge cost + penalty.
    **/
    const int32_t nodeCount = ( int32_t )graph.nodes.size();
    if ( nodeCount <= 0 ) {
        return false;
    }
    const int32_t fromIndex = graph.node_index_by_id.at( from_macro_node_id );
    const int32_t toIndex = graph.node_index_by_id.at( to_macro_node_id );
    if ( fromIndex < 0 || fromIndex >= nodeCount || toIndex < 0 || toIndex >= nodeCount ) {
        return false;
    }

    std::vector<double> bestCost( ( size_t )nodeCount, std::numeric_limits<double>::max() );
    std::vector<double> bestPenalty( ( size_t )nodeCount, std::numeric_limits<double>::max() );
    std::vector<int32_t> hopCount( ( size_t )nodeCount, 0 );
    using queue_entry_t = std::pair<double, int32_t>;
    std::priority_queue<queue_entry_t, std::vector<queue_entry_t>, std::greater<queue_entry_t>> queue;

    bestCost[ ( size_t )fromIndex ] = 0.0;
    bestPenalty[ ( size_t )fromIndex ] = 0.0;
    queue.push( { 0.0, fromIndex } );

    while ( !queue.empty() ) {
        const queue_entry_t current = queue.top();
        queue.pop();
        const double currentScore = current.first;
        const int32_t currentIndex = current.second;
        if ( currentIndex == toIndex ) {
            break;
        }
        if ( currentScore > bestCost[ ( size_t )currentIndex ] ) {
            continue;
        }

        const nav2_macro_node_t &currentNode = graph.nodes[ ( size_t )currentIndex ];
        for ( const int32_t edgeId : currentNode.outgoing_edge_ids ) {
            const nav2_macro_edge_t *edge = SVG_Nav2_MacroTraversal_TryResolveEdge( graph, edgeId );
            if ( !edge ) {
                continue;
            }
            const auto nextIt = graph.node_index_by_id.find( edge->to_macro_node_id );
            if ( nextIt == graph.node_index_by_id.end() ) {
                continue;
            }
            const int32_t nextIndex = nextIt->second;
            if ( nextIndex < 0 || nextIndex >= nodeCount ) {
                continue;
            }

            const double nextCost = bestCost[ ( size_t )currentIndex ] + edge->macro_cost + edge->macro_penalty;
            if ( nextCost < bestCost[ ( size_t )nextIndex ] ) {
                bestCost[ ( size_t )nextIndex ] = nextCost;
                bestPenalty[ ( size_t )nextIndex ] = bestPenalty[ ( size_t )currentIndex ] + edge->macro_penalty;
                hopCount[ ( size_t )nextIndex ] = hopCount[ ( size_t )currentIndex ] + 1;
                queue.push( { nextCost, nextIndex } );
            }
        }
    }

    /**
    *    Publish estimate output when destination became reachable.
    **/
    if ( bestCost[ ( size_t )toIndex ] >= std::numeric_limits<double>::max() ) {
        return false;
    }
    out_estimate->valid = true;
    out_estimate->from_macro_node_id = from_macro_node_id;
    out_estimate->to_macro_node_id = to_macro_node_id;
    out_estimate->estimated_cost = bestCost[ ( size_t )toIndex ];
    out_estimate->estimated_penalty = bestPenalty[ ( size_t )toIndex ];
    out_estimate->edge_hop_count = hopCount[ ( size_t )toIndex ];
    return true;
}

/**
*	@brief	Emit bounded macro traversal diagnostics.
*	@param	graph	Macro graph to report.
*	@param	summary	Optional precomputed summary.
*	@param	limit	Maximum number of node/edge records to print.
**/
void SVG_Nav2_DebugPrintMacroTraversal( const nav2_macro_graph_t &graph, const nav2_macro_traversal_summary_t *summary, const int32_t limit ) {
    /**
    *    Emit one compact summary line first.
    **/
    if ( summary ) {
        gi.dprintf( "[nav2][macro] nodes=%d edges=%d open=%d run=%d portal=%d stair=%d ladder=%d centerline_edges=%d run_edges=%d corridor_edges=%d fatpvs_nodes=%d cost_avg=%.2f penalty_avg=%.2f\n",
            summary->node_count,
            summary->edge_count,
            summary->open_area_node_count,
            summary->corridor_run_node_count,
            summary->portal_node_count,
            summary->stair_node_count,
            summary->ladder_node_count,
            summary->centerline_edge_count,
            summary->directional_run_edge_count,
            summary->corridor_constrained_edge_count,
            summary->fat_pvs_relevant_node_count,
            summary->average_macro_cost,
            summary->average_macro_penalty );
    } else {
        gi.dprintf( "[nav2][macro] nodes=%d edges=%d\n", ( int32_t )graph.nodes.size(), ( int32_t )graph.edges.size() );
    }

    /**
    *    Keep per-record output bounded.
    **/
    const int32_t boundedLimit = std::max( 0, limit );
    for ( int32_t nodeIndex = 0; nodeIndex < ( int32_t )graph.nodes.size() && nodeIndex < boundedLimit; nodeIndex++ ) {
        const nav2_macro_node_t &node = graph.nodes[ ( size_t )nodeIndex ];
        gi.dprintf( "  [nav2][macro][node:%d] id=%d kind=%d region=%d connector=%d tie=%.2f portal=%.2f stair=%.2f ladder=%.2f center=%.2f obstacle=%.2f bottleneck=%.2f flags=0x%08x\n",
            nodeIndex,
            node.macro_node_id,
            ( int32_t )node.kind,
            node.region_layer_id,
            node.connector_id,
            node.heuristic_tie_breaker,
            node.portal_distance,
            node.stair_distance,
            node.ladder_distance,
            node.centerline_distance,
            node.obstacle_distance,
            node.clearance_bottleneck_distance,
            node.flags );
    }
    for ( int32_t edgeIndex = 0; edgeIndex < ( int32_t )graph.edges.size() && edgeIndex < boundedLimit; edgeIndex++ ) {
        const nav2_macro_edge_t &edge = graph.edges[ ( size_t )edgeIndex ];
        gi.dprintf( "  [nav2][macro][edge:%d] id=%d kind=%d from=%d to=%d cost=%.2f penalty=%.2f connector=%d region=%d flags=0x%08x\n",
            edgeIndex,
            edge.macro_edge_id,
            ( int32_t )edge.kind,
            edge.from_macro_node_id,
            edge.to_macro_node_id,
            edge.macro_cost,
            edge.macro_penalty,
            edge.connector_id,
            edge.region_layer_id,
            edge.flags );
    }
}
