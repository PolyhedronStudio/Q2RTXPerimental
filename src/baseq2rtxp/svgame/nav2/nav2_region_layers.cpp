/********************************************************************
*
*
*	ServerGame: Nav2 Region-Layer Decomposition - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_region_layers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>


/**
*
*
*	Nav2 Region-Layer Local Helpers:
*
*
**/
/**
* @brief Allocate the next stable region-layer id in a graph.
* @param graph Graph to inspect.
* @return Stable id chosen for the next appended layer.
**/
static int32_t SVG_Nav2_AllocateRegionLayerId( const nav2_region_layer_graph_t &graph ) {
    /**
    *    Region layers are appended in deterministic order during one build pass, so assigning the
    *    next id from the tail element keeps ids stable while avoiding O(N^2) rescans.
    **/
    if ( graph.layers.empty() ) {
        return 1;
    }

    // Continue from the current tail id when it is monotonic, otherwise fall back to size-based id.
    const int32_t tailId = graph.layers.back().region_layer_id;
    if ( tailId >= 1 ) {
        return tailId + 1;
    }
    return ( int32_t )graph.layers.size() + 1;
}

/**
* @brief Allocate the next stable region-layer edge id in a graph.
* @param graph Graph to inspect.
* @return Stable id chosen for the next appended edge.
**/
static int32_t SVG_Nav2_AllocateRegionLayerEdgeId( const nav2_region_layer_graph_t &graph ) {
    /**
    *    Edges are appended monotonically during graph construction, so deriving ids from the tail
    *    edge avoids quadratic edge-id scans that can stall large maps.
    **/
    if ( graph.edges.empty() ) {
        return 1;
    }

    // Continue from the current tail id when it is monotonic, otherwise fall back to size-based id.
    const int32_t tailId = graph.edges.back().edge_id;
    if ( tailId >= 1 ) {
        return tailId + 1;
    }
    return ( int32_t )graph.edges.size() + 1;
}

/**
* @brief Return a region-layer kind for a span based on its topology and semantics.
* @param span Span to classify.
* @param connectorHint Optional connector hint derived from the extracted connector list.
* @return Region-layer semantic kind for the span.
**/
static nav2_region_layer_kind_t SVG_Nav2_ClassifySpanRegionKind( const nav2_span_t &span, const nav2_connector_list_t &connectorHint ) {
    // Prefer a mover band when the span already carries mover-style traversal metadata.
    if ( span.connector_hint_mask != 0 ) {
        for ( const nav2_connector_t &connector : connectorHint.connectors ) {
            if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_BOARDING ) != 0 ) {
                return nav2_region_layer_kind_t::MoverBand;
            }
        }
    }

    // Ladder and stair semantics should commit early so the hierarchy keeps vertical routing explicit.
    if ( ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 || ( span.surface_flags & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
        return nav2_region_layer_kind_t::LadderBand;
    }
    if ( ( span.topology_flags & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0 || ( span.surface_flags & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
        return nav2_region_layer_kind_t::StairBand;
    }

    // Keep flat connected components as floor bands when they are not already specialized.
    if ( span.cluster_id >= 0 ) {
        return nav2_region_layer_kind_t::ClusterBand;
    }
    if ( span.leaf_id >= 0 ) {
        return nav2_region_layer_kind_t::LeafBand;
    }
    return nav2_region_layer_kind_t::FallbackBand;
}

/**
* @brief Populate a region-layer node from one span.
* @param span Source span.
* @param connectorHint Connector list used to detect portal and mover commitments.
* @param regionLayerId Stable id to assign.
* @return Fully populated region-layer node.
**/
static nav2_region_layer_t SVG_Nav2_MakeRegionLayerFromSpan( const nav2_span_t &span, const nav2_connector_list_t &connectorHint, const int32_t regionLayerId ) {
    // Construct a stable node with the span's BSP anchors.
    nav2_region_layer_t layer = {};
    layer.region_layer_id = regionLayerId;
    layer.kind = SVG_Nav2_ClassifySpanRegionKind( span, connectorHint );
    layer.leaf_id = span.leaf_id;
    layer.cluster_id = span.cluster_id;
    layer.area_id = span.area_id;
    layer.allowed_z_band.min_z = span.floor_z;
    layer.allowed_z_band.max_z = span.ceiling_z;
    layer.allowed_z_band.preferred_z = span.preferred_z;
    layer.topology.leaf_index = span.leaf_id;
    layer.topology.cluster_id = span.cluster_id;
    layer.topology.area_id = span.area_id;
    layer.topology.region_id = span.region_layer_id;
    layer.tile_ref.tile_id = span.occupancy.stamp_frame >= 0 ? span.occupancy.stamp_frame : -1;
    layer.tile_ref.tile_x = 0;
    layer.tile_ref.tile_y = 0;
    layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_LEAF;
    if ( span.cluster_id >= 0 ) {
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_CLUSTER;
    }
    if ( span.area_id >= 0 ) {
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_AREA;
    }
    if ( layer.allowed_z_band.IsValid() ) {
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_ALLOWED_Z_BAND;
    }
    if ( ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 || ( span.surface_flags & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_LADDER;
    }
    if ( ( span.topology_flags & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0 || ( span.surface_flags & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_STAIR;
    }
    return layer;
}

/**
* @brief Return whether two region-layer nodes are close enough to consider adjacency.
* @param fromLayer Source region-layer node.
* @param toLayer Destination region-layer node.
* @return True when layers are topology- or tile-local neighbors.
* @note This prevents quadratic all-to-all edge explosion on large sparse grids.
**/
static const bool SVG_Nav2_AreLayersSpatiallyAdjacent( const nav2_region_layer_t &fromLayer, const nav2_region_layer_t &toLayer ) {
    /**
    *    First apply a hard locality gate when both layers have canonical tile ownership so broad
    *    area/cluster ids do not accidentally create near-complete graphs.
    **/
    if ( fromLayer.tile_ref.tile_id >= 0 && toLayer.tile_ref.tile_id >= 0 ) {
        const int32_t dx = std::abs( fromLayer.tile_ref.tile_x - toLayer.tile_ref.tile_x );
        const int32_t dy = std::abs( fromLayer.tile_ref.tile_y - toLayer.tile_ref.tile_y );
        if ( dx > 2 || dy > 2 ) {
            return false;
        }
    }

    /**
    *    Prefer explicit topology matches first so we keep robust local connectivity even when
    *    tile metadata is unavailable for connector-derived layers.
    **/
    if ( ( fromLayer.leaf_id >= 0 && fromLayer.leaf_id == toLayer.leaf_id )
        || ( fromLayer.cluster_id >= 0 && fromLayer.cluster_id == toLayer.cluster_id )
        || ( fromLayer.area_id >= 0 && fromLayer.area_id == toLayer.area_id ) ) {
        return true;
    }

    /**
    *    If both nodes carry canonical tile coordinates, require local neighborhood proximity to
    *    avoid constructing a global complete graph.
    **/
    if ( fromLayer.tile_ref.tile_id >= 0 && toLayer.tile_ref.tile_id >= 0 ) {
        const int32_t dx = std::abs( fromLayer.tile_ref.tile_x - toLayer.tile_ref.tile_x );
        const int32_t dy = std::abs( fromLayer.tile_ref.tile_y - toLayer.tile_ref.tile_y );
        if ( dx <= 1 && dy <= 1 ) {
            return true;
        }
    }

    /**
    *    Keep connector-derived commitment nodes lightly connected by allowing short-range tile
    *    proximity when either side is a portal/mover-specialized layer.
    **/
    const bool hasSpecializedCommitment = ( fromLayer.kind == nav2_region_layer_kind_t::PortalEndpoint )
        || ( toLayer.kind == nav2_region_layer_kind_t::PortalEndpoint )
        || ( fromLayer.kind == nav2_region_layer_kind_t::MoverBand )
        || ( toLayer.kind == nav2_region_layer_kind_t::MoverBand );
    if ( hasSpecializedCommitment && fromLayer.tile_ref.tile_id >= 0 && toLayer.tile_ref.tile_id >= 0 ) {
        const int32_t dx = std::abs( fromLayer.tile_ref.tile_x - toLayer.tile_ref.tile_x );
        const int32_t dy = std::abs( fromLayer.tile_ref.tile_y - toLayer.tile_ref.tile_y );
        return dx <= 2 && dy <= 2;
    }

    return false;
}

/**
* @brief Attach a connector-derived region-layer node.
* @param connector Connector to encode.
* @param regionLayerId Stable id to assign.
* @return Region-layer node representing the connector.
**/
static nav2_region_layer_t SVG_Nav2_MakeConnectorRegionLayer( const nav2_connector_t &connector, const int32_t regionLayerId ) {
    // Construct a region-layer node that preserves the connector's route commitments.
    nav2_region_layer_t layer = {};
    layer.region_layer_id = regionLayerId;
    layer.kind = nav2_region_layer_kind_t::PortalEndpoint;
    layer.connector_id = connector.connector_id;
    layer.mover_entnum = connector.owner_entnum;
    layer.inline_model_index = connector.inline_model_index;
    layer.endpoint_semantics = connector.start.endpoint_semantics | connector.end.endpoint_semantics;
    layer.transition_semantics = connector.transition_semantics;
    layer.allowed_z_band.min_z = connector.allowed_min_z;
    layer.allowed_z_band.max_z = connector.allowed_max_z;
    layer.allowed_z_band.preferred_z = ( connector.allowed_min_z + connector.allowed_max_z ) * 0.5;
    layer.topology = connector.start.valid ? connector.start.span_ref.IsValid() ? nav2_corridor_topology_ref_t{ connector.start.leaf_id, connector.start.cluster_id, connector.start.area_id, NAV_REGION_ID_NONE, NAV_PORTAL_ID_NONE, connector.start.span_ref.column_index, 0, 0, 0 } : nav2_corridor_topology_ref_t{} : nav2_corridor_topology_ref_t{};
    layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_PORTAL;
    layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_ALLOWED_Z_BAND;
    layer.flags |= NAV2_REGION_LAYER_FLAG_PORTAL_BARRIER;
    if ( connector.owner_entnum >= 0 ) {
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_MOVER;
    }
    if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_LADDER_ENDPOINT ) != 0 ) {
        layer.kind = nav2_region_layer_kind_t::LadderBand;
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_LADDER;
    }
    if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_STAIR_BAND ) != 0 ) {
        layer.kind = nav2_region_layer_kind_t::StairBand;
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_STAIR;
    }
    if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_BOARDING ) != 0 || ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_RIDE ) != 0 || ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_EXIT ) != 0 ) {
        layer.kind = nav2_region_layer_kind_t::MoverBand;
        layer.flags |= NAV2_REGION_LAYER_FLAG_HAS_MOVER;
    }
    return layer;
}


/**
*
*
*	Nav2 Region-Layer Public API:
*
*
**/
/**
* @brief Clear a region-layer graph to an empty state.
* @param graph Graph to reset.
**/
void SVG_Nav2_RegionLayerGraph_Clear( nav2_region_layer_graph_t *graph ) {
    // Guard the output pointer so callers can reuse the helper safely.
    if ( !graph ) {
        return;
    }
    graph->layers.clear();
    graph->edges.clear();
    graph->by_id.clear();
}

/**
* @brief Resolve a region-layer reference by stable id.
* @param graph Graph to inspect.
* @param region_layer_id Stable region-layer id to resolve.
* @param out_ref [out] Resolved region-layer reference.
* @return True when the region-layer exists.
**/
const bool SVG_Nav2_RegionLayerGraph_TryResolve( const nav2_region_layer_graph_t &graph, const int32_t region_layer_id, nav2_region_layer_ref_t *out_ref ) {
    // Require a destination for the resolved reference.
    if ( !out_ref ) {
        return false;
    }
    *out_ref = {};
    const auto it = graph.by_id.find( region_layer_id );
    if ( it == graph.by_id.end() ) {
        return false;
    }
    *out_ref = it->second;
    return out_ref->IsValid();
}

/**
* @brief Append one region-layer node to the graph.
* @param graph Graph to mutate.
* @param layer Region-layer payload to append.
* @return True when the node was appended.
**/
const bool SVG_Nav2_RegionLayerGraph_AppendLayer( nav2_region_layer_graph_t *graph, const nav2_region_layer_t &layer ) {
    // Keep writes deterministic and reject invalid inputs early.
    if ( !graph || layer.region_layer_id < 0 ) {
        return false;
    }

    nav2_region_layer_t stored = layer;
    if ( stored.region_layer_id < 0 ) {
        stored.region_layer_id = SVG_Nav2_AllocateRegionLayerId( *graph );
    }
    const int32_t index = ( int32_t )graph->layers.size();
    graph->layers.push_back( stored );
    graph->by_id[ stored.region_layer_id ] = { stored.region_layer_id, index };
    return true;
}

/**
* @brief Validate a region-layer graph for stable-id, bounds, and adjacency consistency.
* @param graph Graph to inspect.
* @return True when all nodes and edges are internally consistent.
**/
const bool SVG_Nav2_ValidateRegionLayerGraph( const nav2_region_layer_graph_t &graph ) {
    // Every stored node must resolve through the stable lookup table.
    for ( size_t i = 0; i < graph.layers.size(); ++i ) {
        const nav2_region_layer_t &layer = graph.layers[ i ];
        if ( layer.region_layer_id < 0 ) {
            return false;
        }
        nav2_region_layer_ref_t ref = {};
        if ( !SVG_Nav2_RegionLayerGraph_TryResolve( graph, layer.region_layer_id, &ref ) ) {
            return false;
        }
        if ( ref.region_layer_index != ( int32_t )i ) {
            return false;
        }
        if ( !layer.allowed_z_band.IsValid() ) {
            return false;
        }
    }

    // Every edge must point at valid node ids and appear in the owning adjacency lists.
    for ( const nav2_region_layer_edge_t &edge : graph.edges ) {
        if ( edge.edge_id < 0 || edge.from_region_layer_id < 0 || edge.to_region_layer_id < 0 ) {
            return false;
        }
        nav2_region_layer_ref_t fromRef = {};
        nav2_region_layer_ref_t toRef = {};
        if ( !SVG_Nav2_RegionLayerGraph_TryResolve( graph, edge.from_region_layer_id, &fromRef ) ) {
            return false;
        }
        if ( !SVG_Nav2_RegionLayerGraph_TryResolve( graph, edge.to_region_layer_id, &toRef ) ) {
            return false;
        }
        const nav2_region_layer_t &fromLayer = graph.layers[ ( size_t )fromRef.region_layer_index ];
        const nav2_region_layer_t &toLayer = graph.layers[ ( size_t )toRef.region_layer_index ];
        const bool fromHasEdge = std::find( fromLayer.outgoing_edge_ids.begin(), fromLayer.outgoing_edge_ids.end(), edge.edge_id ) != fromLayer.outgoing_edge_ids.end();
        const bool toHasEdge = std::find( toLayer.incoming_edge_ids.begin(), toLayer.incoming_edge_ids.end(), edge.edge_id ) != toLayer.incoming_edge_ids.end();
        if ( !fromHasEdge || !toHasEdge ) {
            return false;
        }
        if ( !edge.allowed_z_band.IsValid() ) {
            return false;
        }
    }

    return true;
}

/**
* @brief Append one region-layer edge to the graph and wire it into the adjacency lists.
* @param graph Graph to mutate.
* @param edge Region-layer edge payload to append.
* @return True when the edge was appended.
**/
const bool SVG_Nav2_RegionLayerGraph_AppendEdge( nav2_region_layer_graph_t *graph, const nav2_region_layer_edge_t &edge ) {
    // Reject null graphs and incomplete edges so the graph stays valid.
    if ( !graph || edge.from_region_layer_id < 0 || edge.to_region_layer_id < 0 ) {
        return false;
    }

    // Look up both ends before storing the edge.
    nav2_region_layer_ref_t fromRef = {};
    nav2_region_layer_ref_t toRef = {};
    if ( !SVG_Nav2_RegionLayerGraph_TryResolve( *graph, edge.from_region_layer_id, &fromRef ) ) {
        return false;
    }
    if ( !SVG_Nav2_RegionLayerGraph_TryResolve( *graph, edge.to_region_layer_id, &toRef ) ) {
        return false;
    }

    // Self-loops are allowed only when they represent a connector-owned commitment node.
    if ( edge.from_region_layer_id == edge.to_region_layer_id && edge.connector_id < 0 ) {
        return false;
    }

    // Store the edge with a stable id and wire the adjacency lists.
    nav2_region_layer_edge_t stored = edge;
    if ( stored.edge_id < 0 ) {
        stored.edge_id = SVG_Nav2_AllocateRegionLayerEdgeId( *graph );
    }
    graph->edges.push_back( stored );
    graph->layers[ ( size_t )fromRef.region_layer_index ].outgoing_edge_ids.push_back( stored.edge_id );
    graph->layers[ ( size_t )toRef.region_layer_index ].incoming_edge_ids.push_back( stored.edge_id );
    return true;
}

/**
* @brief Build a region-layer decomposition from a sparse span grid plus connector list.
* @param grid Sparse span grid providing the precision substrate.
* @param connectors Connector collection used to attach higher-level route commitments.
* @param out_graph [out] Region-layer graph receiving the decomposition.
* @param out_summary [out] Optional build summary.
* @return True when at least one region-layer node was produced.
**/
const bool SVG_Nav2_BuildRegionLayers( const nav2_span_grid_t &grid, const nav2_connector_list_t &connectors, nav2_region_layer_graph_t *out_graph, nav2_region_layer_summary_t *out_summary ) {
    /**
    *	Route the legacy compatibility signature through the topology-first entry point using a
    *	lightweight synthetic topology artifact derived from the currently supplied grid.
    **/
    nav2_topology_artifact_t topologyArtifact = {};
    topologyArtifact.status = nav2_topology_status_t::Ready;
    topologyArtifact.static_nav_version = 1;
    topologyArtifact.world_tile_count = ( int32_t )grid.columns.size();
    topologyArtifact.span_column_count = ( int32_t )grid.columns.size();
    topologyArtifact.has_tile_sizing = grid.tile_size > 0 && grid.cell_size_xy > 0.0;
    topologyArtifact.ready = topologyArtifact.has_tile_sizing;
    for ( const nav2_span_column_t &column : grid.columns ) {
        topologyArtifact.span_count += ( int32_t )column.spans.size();
    }
    return SVG_Nav2_BuildRegionLayersFromTopology( topologyArtifact, grid, connectors, out_graph, out_summary );
}

/**
 * @brief Build a region-layer decomposition from a topology-owned publication plus connector list.
 * @param topologyArtifact Topology publication owning the current classification stage output.
 * @param grid Sparse span grid providing the precision substrate.
 * @param connectors Connector collection used to attach higher-level route commitments.
 * @param out_graph [out] Region-layer graph receiving the decomposition.
 * @param out_summary [out] Optional build summary.
 * @return True when at least one region-layer node was produced.
 **/
const bool SVG_Nav2_BuildRegionLayersFromTopology( const nav2_topology_artifact_t &topologyArtifact,
    const nav2_span_grid_t &grid, const nav2_connector_list_t &connectors, nav2_region_layer_graph_t *out_graph,
    nav2_region_layer_summary_t *out_summary )
{
    // Reset the output graph so a rebuild always starts from a clean slate.
    SVG_Nav2_RegionLayerGraph_Clear( out_graph );
    if ( !out_graph ) {
        return false;
    }

    /**
    *	Require a valid topology publication before region-layer decomposition begins so this stage
    *	can be explicitly topology-owned even while compatibility callers still route through the old signature.
    **/
    if ( !SVG_Nav2_Topology_ValidateArtifact( topologyArtifact ) ) {
        return false;
    }

    // Begin with connector-derived nodes so portal and mover commitments become explicit coarse states.
    for ( const nav2_connector_t &connector : connectors.connectors ) {
        nav2_region_layer_t connectorLayer = SVG_Nav2_MakeConnectorRegionLayer( connector, SVG_Nav2_AllocateRegionLayerId( *out_graph ) );
        SVG_Nav2_RegionLayerGraph_AppendLayer( out_graph, connectorLayer );
        if ( connector.start.valid && connector.end.valid ) {
            nav2_region_layer_edge_t edge = {};
            edge.from_region_layer_id = connectorLayer.region_layer_id;
            edge.to_region_layer_id = connectorLayer.region_layer_id;
            edge.kind = nav2_region_layer_edge_kind_t::PortalTransition;
            edge.flags |= NAV2_REGION_LAYER_EDGE_FLAG_PASSABLE | NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_PORTAL | NAV2_REGION_LAYER_EDGE_FLAG_Z_BAND_CONSTRAINED;
            edge.base_cost = connector.base_cost;
            edge.topology_penalty = connector.policy_penalty;
            edge.allowed_z_band.min_z = connector.allowed_min_z;
            edge.allowed_z_band.max_z = connector.allowed_max_z;
            edge.allowed_z_band.preferred_z = ( connector.allowed_min_z + connector.allowed_max_z ) * 0.5;
            edge.connector_id = connector.connector_id;
            edge.transition_semantics = connector.transition_semantics;
            edge.mover_ref.owner_entnum = connector.owner_entnum;
            edge.mover_ref.model_index = connector.inline_model_index;
            SVG_Nav2_RegionLayerGraph_AppendEdge( out_graph, edge );
        }
    }

    // Add span-derived nodes for each sparse column and span so the hierarchy can reason about vertical structure.
    for ( const nav2_span_column_t &column : grid.columns ) {
        for ( const nav2_span_t &span : column.spans ) {
            if ( !SVG_Nav2_SpanIsValid( span ) ) {
                continue;
            }
            nav2_region_layer_t layer = SVG_Nav2_MakeRegionLayerFromSpan( span, connectors, SVG_Nav2_AllocateRegionLayerId( *out_graph ) );
            layer.tile_ref.tile_x = column.tile_x;
            layer.tile_ref.tile_y = column.tile_y;
            layer.tile_ref.tile_id = column.tile_id;
            layer.topology.tile_id = column.tile_id;
            layer.topology.tile_x = column.tile_x;
            layer.topology.tile_y = column.tile_y;
            if ( ( span.connector_hint_mask != 0 ) || ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 ) {
                layer.flags |= NAV2_REGION_LAYER_FLAG_FAT_PVS_RELEVANT;
            }
            if ( span.region_layer_id < 0 ) {
                layer.flags |= NAV2_REGION_LAYER_FLAG_PARTIAL;
            }
            SVG_Nav2_RegionLayerGraph_AppendLayer( out_graph, layer );
        }
    }

    // Derive bounded local adjacency between compatible layers to keep coarse A* usable without
    // constructing an O(N^2) complete graph on large sparse maps.
    constexpr int32_t maxNeighborEdgesPerLayer = 24;
    for ( size_t i = 0; i < out_graph->layers.size(); ++i ) {
        const nav2_region_layer_t &fromLayer = out_graph->layers[ i ];
        int32_t neighborEdgeCount = 0;

        // Limit per-layer fan-out to keep build cost bounded and deterministic on dense maps.
        for ( size_t j = i + 1; j < out_graph->layers.size(); ++j ) {
            const nav2_region_layer_t &toLayer = out_graph->layers[ j ];

            // Stop adding more neighbors once this layer reached the bounded fan-out budget.
            if ( neighborEdgeCount >= maxNeighborEdgesPerLayer ) {
                break;
            }

            // Skip non-local pairs so we do not generate global all-to-all edges.
            if ( !SVG_Nav2_AreLayersSpatiallyAdjacent( fromLayer, toLayer ) ) {
                continue;
            }

            // Skip pairs with invalid Z-band commitments before creating transition edges.
            if ( fromLayer.allowed_z_band.IsValid() && toLayer.allowed_z_band.IsValid() && std::fabs( fromLayer.allowed_z_band.preferred_z - toLayer.allowed_z_band.preferred_z ) > 256.0 ) {
                continue;
            }

            nav2_region_layer_edge_t edge = {};
            edge.from_region_layer_id = fromLayer.region_layer_id;
            edge.to_region_layer_id = toLayer.region_layer_id;
            edge.base_cost = 1.0;
            edge.topology_penalty = 0.0;
            edge.allowed_z_band.min_z = std::min( fromLayer.allowed_z_band.min_z, toLayer.allowed_z_band.min_z );
            edge.allowed_z_band.max_z = std::max( fromLayer.allowed_z_band.max_z, toLayer.allowed_z_band.max_z );
            edge.allowed_z_band.preferred_z = ( fromLayer.allowed_z_band.preferred_z + toLayer.allowed_z_band.preferred_z ) * 0.5;
            edge.transition_semantics = fromLayer.transition_semantics | toLayer.transition_semantics;
            if ( fromLayer.kind == nav2_region_layer_kind_t::LadderBand || toLayer.kind == nav2_region_layer_kind_t::LadderBand ) {
                edge.kind = nav2_region_layer_edge_kind_t::LadderTransition;
                edge.flags |= NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_LADDER;
            }
            else if ( fromLayer.kind == nav2_region_layer_kind_t::StairBand || toLayer.kind == nav2_region_layer_kind_t::StairBand ) {
                edge.kind = nav2_region_layer_edge_kind_t::StairTransition;
                edge.flags |= NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_STAIR;
            }
            else if ( fromLayer.kind == nav2_region_layer_kind_t::MoverBand || toLayer.kind == nav2_region_layer_kind_t::MoverBand ) {
                edge.kind = nav2_region_layer_edge_kind_t::MoverRide;
                edge.flags |= NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_MOVER | NAV2_REGION_LAYER_EDGE_FLAG_ALLOWS_MOVER_RIDE;
                edge.mover_ref.owner_entnum = fromLayer.mover_entnum >= 0 ? fromLayer.mover_entnum : toLayer.mover_entnum;
                edge.mover_ref.model_index = fromLayer.inline_model_index >= 0 ? fromLayer.inline_model_index : toLayer.inline_model_index;
            }
            else if ( fromLayer.kind == nav2_region_layer_kind_t::PortalEndpoint || toLayer.kind == nav2_region_layer_kind_t::PortalEndpoint ) {
                edge.kind = nav2_region_layer_edge_kind_t::PortalTransition;
                edge.flags |= NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_PORTAL;
            }
            else {
                edge.kind = nav2_region_layer_edge_kind_t::SameBand;
                edge.flags |= NAV2_REGION_LAYER_EDGE_FLAG_PASSABLE | NAV2_REGION_LAYER_EDGE_FLAG_TOPOLOGY_MATCHED;
            }

            // Append forward and reverse edges so coarse routing remains bidirectional.
            if ( SVG_Nav2_RegionLayerGraph_AppendEdge( out_graph, edge ) ) {
                neighborEdgeCount++;
            }

            nav2_region_layer_edge_t reverseEdge = edge;
            reverseEdge.from_region_layer_id = edge.to_region_layer_id;
            reverseEdge.to_region_layer_id = edge.from_region_layer_id;
            (void)SVG_Nav2_RegionLayerGraph_AppendEdge( out_graph, reverseEdge );
        }
    }

    // Fill out the caller-visible summary if requested.
    if ( out_summary ) {
        *out_summary = {};
        out_summary->layer_count = ( int32_t )out_graph->layers.size();
        out_summary->edge_count = ( int32_t )out_graph->edges.size();
        for ( const nav2_region_layer_t &layer : out_graph->layers ) {
            if ( layer.flags & NAV2_REGION_LAYER_FLAG_PORTAL_BARRIER ) {
                out_summary->portal_layer_count++;
            }
            if ( layer.kind == nav2_region_layer_kind_t::StairBand ) {
                out_summary->stair_layer_count++;
            }
            if ( layer.kind == nav2_region_layer_kind_t::LadderBand ) {
                out_summary->ladder_layer_count++;
            }
            if ( layer.kind == nav2_region_layer_kind_t::MoverBand ) {
                out_summary->mover_layer_count++;
            }
            if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_PARTIAL ) != 0 ) {
                out_summary->partial_layer_count++;
            }
        }
    }

    return !out_graph->layers.empty() && SVG_Nav2_ValidateRegionLayerGraph( *out_graph );
}

/**
* @brief Emit a bounded debug summary for a region-layer graph.
* @param graph Graph to report.
* @param limit Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintRegionLayers( const nav2_region_layer_graph_t &graph, const int32_t limit ) {
    // Avoid noisy logging when no useful output was requested.
    if ( limit <= 0 ) {
        return;
    }

    const int32_t reportCount = std::min( ( int32_t )graph.layers.size(), limit );
    gi.dprintf( "[NAV2][RegionLayers] layers=%d edges=%d report=%d\n", ( int32_t )graph.layers.size(), ( int32_t )graph.edges.size(), reportCount );
    for ( int32_t i = 0; i < reportCount; ++i ) {
        const nav2_region_layer_t &layer = graph.layers[ ( size_t )i ];
        gi.dprintf( "[NAV2][RegionLayers] id=%d kind=%d leaf=%d cluster=%d area=%d portal=%d mover=%d model=%d flags=0x%08x z=(%.1f..%.1f) pref=%.1f out=%d in=%d\n",
            layer.region_layer_id,
            ( int32_t )layer.kind,
            layer.leaf_id,
            layer.cluster_id,
            layer.area_id,
            layer.topology.portal_id,
            layer.mover_entnum,
            layer.inline_model_index,
            layer.flags,
            layer.allowed_z_band.min_z,
            layer.allowed_z_band.max_z,
            layer.allowed_z_band.preferred_z,
            ( int32_t )layer.outgoing_edge_ids.size(),
            ( int32_t )layer.incoming_edge_ids.size() );
    }
}

/**
* @brief Keep the nav2 region-layer module represented in the build.
**/
void SVG_Nav2_RegionLayer_ModuleAnchor( void ) {
}
