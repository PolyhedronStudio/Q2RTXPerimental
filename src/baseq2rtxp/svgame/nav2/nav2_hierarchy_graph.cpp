/********************************************************************
*
*
*	ServerGame: Nav2 Coarse Hierarchy Graph - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_hierarchy_graph.h"

#include <algorithm>
#include <cmath>


/**
*
*
*	Nav2 Hierarchy Local Helpers:
*
*
**/
/**
* @brief Allocate the next stable hierarchy node id in a graph.
* @param graph Graph to inspect.
* @return Stable id chosen for the next appended node.
**/
static int32_t SVG_Nav2_AllocateHierarchyNodeId( const nav2_hierarchy_graph_t &graph ) {
    // Keep node ids monotonic so published references stay stable across rebuilds.
    int32_t nextId = 1;
    for ( const nav2_hierarchy_node_t &node : graph.nodes ) {
        nextId = std::max( nextId, node.node_id + 1 );
    }
    return nextId;
}

/**
* @brief Allocate the next stable hierarchy edge id in a graph.
* @param graph Graph to inspect.
* @return Stable id chosen for the next appended edge.
**/
static int32_t SVG_Nav2_AllocateHierarchyEdgeId( const nav2_hierarchy_graph_t &graph ) {
    // Keep edge ids monotonic so debug output and worker snapshots remain deterministic.
    int32_t nextId = 1;
    for ( const nav2_hierarchy_edge_t &edge : graph.edges ) {
        nextId = std::max( nextId, edge.edge_id + 1 );
    }
    return nextId;
}

/**
* @brief Classify one region-layer node into a coarse hierarchy node kind.
* @param layer Region-layer source.
* @return Hierarchy kind for the source node.
**/
static nav2_hierarchy_node_kind_t SVG_Nav2_ClassifyHierarchyNodeKind( const nav2_region_layer_t &layer ) {
    // Route portal, mover, ladder, and stair states explicitly so coarse A* commits vertical intent early.
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_MOVER ) != 0 || layer.kind == nav2_region_layer_kind_t::MoverBand ) {
        return nav2_hierarchy_node_kind_t::MoverRide;
    }
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_PORTAL ) != 0 || layer.kind == nav2_region_layer_kind_t::PortalEndpoint ) {
        return nav2_hierarchy_node_kind_t::PortalEndpoint;
    }
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_LADDER ) != 0 || layer.kind == nav2_region_layer_kind_t::LadderBand ) {
        return nav2_hierarchy_node_kind_t::LadderEndpoint;
    }
    if ( ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_STAIR ) != 0 || layer.kind == nav2_region_layer_kind_t::StairBand ) {
        return nav2_hierarchy_node_kind_t::StairBand;
    }
    if ( layer.kind == nav2_region_layer_kind_t::ClusterBand ) {
        return nav2_hierarchy_node_kind_t::ClusterBand;
    }
    if ( layer.kind == nav2_region_layer_kind_t::LeafBand ) {
        return nav2_hierarchy_node_kind_t::LeafBand;
    }
    if ( layer.kind == nav2_region_layer_kind_t::FloorBand ) {
        return nav2_hierarchy_node_kind_t::RegionLayer;
    }
    return nav2_hierarchy_node_kind_t::RegionLayer;
}

/**
* @brief Populate a hierarchy node from one region-layer source.
* @param layer Region-layer source.
* @param regionLayerIndex Stable region-layer index for the hierarchy node.
* @param nodeId Stable hierarchy node id to assign.
* @return Fully populated hierarchy node.
**/
static nav2_hierarchy_node_t SVG_Nav2_MakeHierarchyNodeFromLayer( const nav2_region_layer_t &layer, const int32_t regionLayerIndex, const int32_t nodeId ) {
    // Mirror the region-layer into a hierarchy node that preserves vertical commitments.
    nav2_hierarchy_node_t node = {};
    node.node_id = nodeId;
    node.kind = SVG_Nav2_ClassifyHierarchyNodeKind( layer );
    node.region_layer_id = layer.region_layer_id;
    node.region_layer_index = regionLayerIndex;
    node.connector_id = layer.connector_id;
    node.mover_entnum = layer.mover_entnum;
    node.inline_model_index = layer.inline_model_index;
    node.topology = layer.topology;
    node.allowed_z_band = layer.allowed_z_band;
    node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_REGION_LAYER;
    if ( layer.leaf_id >= 0 ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_LEAF;
    }
    if ( layer.cluster_id >= 0 ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_CLUSTER;
    }
    if ( layer.area_id >= 0 ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_AREA;
    }
    if ( layer.connector_id >= 0 ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_PORTAL;
    }
    if ( layer.mover_entnum >= 0 ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_MOVER;
    }
    if ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_LADDER ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_LADDER;
    }
    if ( layer.flags & NAV2_REGION_LAYER_FLAG_HAS_STAIR ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_HAS_STAIR;
    }
    if ( layer.flags & NAV2_REGION_LAYER_FLAG_PARTIAL ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_PARTIAL;
    }
    if ( layer.flags & NAV2_REGION_LAYER_FLAG_FAT_PVS_RELEVANT ) {
        node.flags |= NAV2_HIERARCHY_NODE_FLAG_FAT_PVS_RELEVANT;
    }
    return node;
}

/**
* @brief Populate a hierarchy edge from two region-layer nodes.
* @param fromNode Source hierarchy node.
* @param toNode Destination hierarchy node.
* @param regionLayerEdge Source region-layer edge used as semantic guidance.
* @param edgeId Stable hierarchy edge id to assign.
* @return Fully populated hierarchy edge.
**/
static nav2_hierarchy_edge_t SVG_Nav2_MakeHierarchyEdgeFromRegionEdge( const nav2_hierarchy_node_t &fromNode, const nav2_hierarchy_node_t &toNode, const nav2_region_layer_edge_t &regionLayerEdge, const int32_t edgeId ) {
    // Translate the region-layer edge into a coarse graph edge while preserving route commitments.
    nav2_hierarchy_edge_t edge = {};
    edge.edge_id = edgeId;
    edge.from_node_id = fromNode.node_id;
    edge.to_node_id = toNode.node_id;
    edge.connector_id = regionLayerEdge.connector_id;
    // Preserve the source region-layer identity through the hierarchy edge without depending on a non-existent field.
    edge.region_layer_id = fromNode.region_layer_id >= 0 ? fromNode.region_layer_id : toNode.region_layer_id;
    edge.mover_ref = regionLayerEdge.mover_ref;
    edge.allowed_z_band = regionLayerEdge.allowed_z_band;
    edge.base_cost = regionLayerEdge.base_cost;
    edge.topology_penalty = regionLayerEdge.topology_penalty;

    // Convert region-layer semantics into hierarchy edge semantics.
    switch ( regionLayerEdge.kind ) {
        case nav2_region_layer_edge_kind_t::PortalTransition:
            edge.kind = nav2_hierarchy_edge_kind_t::PortalLink;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_PORTAL;
            break;
        case nav2_region_layer_edge_kind_t::StairTransition:
            edge.kind = nav2_hierarchy_edge_kind_t::StairLink;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_STAIR;
            break;
        case nav2_region_layer_edge_kind_t::LadderTransition:
            edge.kind = nav2_hierarchy_edge_kind_t::LadderLink;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_LADDER;
            break;
        case nav2_region_layer_edge_kind_t::MoverBoarding:
            edge.kind = nav2_hierarchy_edge_kind_t::MoverBoarding;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_MOVER | NAV2_HIERARCHY_EDGE_FLAG_ALLOWS_BOARDING;
            break;
        case nav2_region_layer_edge_kind_t::MoverRide:
            edge.kind = nav2_hierarchy_edge_kind_t::MoverRide;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_MOVER | NAV2_HIERARCHY_EDGE_FLAG_ALLOWS_RIDE;
            break;
        case nav2_region_layer_edge_kind_t::MoverExit:
            edge.kind = nav2_hierarchy_edge_kind_t::MoverExit;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_MOVER | NAV2_HIERARCHY_EDGE_FLAG_ALLOWS_EXIT;
            break;
        case nav2_region_layer_edge_kind_t::SameBand:
            edge.kind = nav2_hierarchy_edge_kind_t::RegionLink;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_PASSABLE | NAV2_HIERARCHY_EDGE_FLAG_TOPOLOGY_MATCHED;
            break;
        default:
            edge.kind = nav2_hierarchy_edge_kind_t::FallbackLink;
            edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_PARTIAL;
            break;
    }

    // Carry mover and vertical commitment details forward so later corridor extraction does not need to rediscover them.
    if ( edge.allowed_z_band.IsValid() ) {
        edge.flags |= NAV2_HIERARCHY_EDGE_FLAG_PASSABLE;
    }
    return edge;
}


/**
*
*
*	Nav2 Hierarchy Public API:
*
*
**/
/**
* @brief Clear a hierarchy graph to an empty state.
* @param graph Graph to reset.
**/
void SVG_Nav2_HierarchyGraph_Clear( nav2_hierarchy_graph_t *graph ) {
    // Guard null output so the helper remains safe for reuse.
    if ( !graph ) {
        return;
    }
    graph->nodes.clear();
    graph->edges.clear();
    graph->by_id.clear();
}

/**
* @brief Resolve a hierarchy node by stable id.
* @param graph Graph to inspect.
* @param node_id Stable node id to resolve.
* @param out_ref [out] Resolved hierarchy reference.
* @return True when the node exists.
**/
const bool SVG_Nav2_HierarchyGraph_TryResolve( const nav2_hierarchy_graph_t &graph, const int32_t node_id, nav2_hierarchy_node_ref_t *out_ref ) {
    // Require a destination for the resolved reference.
    if ( !out_ref ) {
        return false;
    }
    *out_ref = {};
    const auto it = graph.by_id.find( node_id );
    if ( it == graph.by_id.end() ) {
        return false;
    }
    *out_ref = it->second;
    return out_ref->IsValid();
}

/**
* @brief Append one hierarchy node to the graph.
* @param graph Graph to mutate.
* @param node Node payload to append.
* @return True when the node was appended.
**/
const bool SVG_Nav2_HierarchyGraph_AppendNode( nav2_hierarchy_graph_t *graph, const nav2_hierarchy_node_t &node ) {
    // Reject invalid inputs early so graph state remains deterministic.
    if ( !graph || node.node_id < 0 ) {
        return false;
    }

    nav2_hierarchy_node_t stored = node;
    if ( stored.node_id < 0 ) {
        stored.node_id = SVG_Nav2_AllocateHierarchyNodeId( *graph );
    }
    const int32_t index = ( int32_t )graph->nodes.size();
    graph->nodes.push_back( stored );
    graph->by_id[ stored.node_id ] = { stored.node_id, index };
    return true;
}

/**
* @brief Validate that the hierarchy graph maintains stable ids and adjacency.
* @param graph Graph to inspect.
* @return True when the graph is internally consistent.
**/
const bool SVG_Nav2_ValidateHierarchyGraph( const nav2_hierarchy_graph_t &graph ) {
    // Every node must resolve through the stable lookup table.
    for ( size_t i = 0; i < graph.nodes.size(); ++i ) {
        const nav2_hierarchy_node_t &node = graph.nodes[ i ];
        if ( node.node_id < 0 ) {
            return false;
        }
        nav2_hierarchy_node_ref_t ref = {};
        if ( !SVG_Nav2_HierarchyGraph_TryResolve( graph, node.node_id, &ref ) ) {
            return false;
        }
        if ( ref.node_index != ( int32_t )i ) {
            return false;
        }
        if ( !node.allowed_z_band.IsValid() ) {
            return false;
        }
    }

    // Every edge must point to valid node ids and appear in adjacency lists.
    for ( const nav2_hierarchy_edge_t &edge : graph.edges ) {
        if ( edge.edge_id < 0 ) {
            return false;
        }
        nav2_hierarchy_node_ref_t fromRef = {};
        nav2_hierarchy_node_ref_t toRef = {};
        if ( !SVG_Nav2_HierarchyGraph_TryResolve( graph, edge.from_node_id, &fromRef ) ) {
            return false;
        }
        if ( !SVG_Nav2_HierarchyGraph_TryResolve( graph, edge.to_node_id, &toRef ) ) {
            return false;
        }
        const nav2_hierarchy_node_t &fromNode = graph.nodes[ ( size_t )fromRef.node_index ];
        const nav2_hierarchy_node_t &toNode = graph.nodes[ ( size_t )toRef.node_index ];
        const bool fromHasEdge = std::find( fromNode.outgoing_edge_ids.begin(), fromNode.outgoing_edge_ids.end(), edge.edge_id ) != fromNode.outgoing_edge_ids.end();
        const bool toHasEdge = std::find( toNode.incoming_edge_ids.begin(), toNode.incoming_edge_ids.end(), edge.edge_id ) != toNode.incoming_edge_ids.end();
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
* @brief Append one hierarchy edge to the graph and wire adjacency lists.
* @param graph Graph to mutate.
* @param edge Edge payload to append.
* @return True when the edge was appended.
**/
const bool SVG_Nav2_HierarchyGraph_AppendEdge( nav2_hierarchy_graph_t *graph, const nav2_hierarchy_edge_t &edge ) {
    // Reject null graphs and incomplete edges so the graph stays valid.
    if ( !graph || edge.from_node_id < 0 || edge.to_node_id < 0 ) {
        return false;
    }

    // Look up both ends before storing the edge.
    nav2_hierarchy_node_ref_t fromRef = {};
    nav2_hierarchy_node_ref_t toRef = {};
    if ( !SVG_Nav2_HierarchyGraph_TryResolve( *graph, edge.from_node_id, &fromRef ) ) {
        return false;
    }
    if ( !SVG_Nav2_HierarchyGraph_TryResolve( *graph, edge.to_node_id, &toRef ) ) {
        return false;
    }

    // Store the edge with a stable id and wire the adjacency lists.
    nav2_hierarchy_edge_t stored = edge;
    if ( stored.edge_id < 0 ) {
        stored.edge_id = SVG_Nav2_AllocateHierarchyEdgeId( *graph );
    }
    graph->edges.push_back( stored );
    graph->nodes[ ( size_t )fromRef.node_index ].outgoing_edge_ids.push_back( stored.edge_id );
    graph->nodes[ ( size_t )toRef.node_index ].incoming_edge_ids.push_back( stored.edge_id );
    return true;
}

/**
* @brief Build a coarse hierarchy graph from region layers.
* @param regionLayers Region-layer graph providing vertical and topological commitments.
* @param out_graph [out] Hierarchy graph receiving the coarse graph.
* @param out_summary [out] Optional summary.
* @return True when at least one hierarchy node was produced.
**/
const bool SVG_Nav2_BuildHierarchyGraph( const nav2_region_layer_graph_t &regionLayers, nav2_hierarchy_graph_t *out_graph, nav2_hierarchy_summary_t *out_summary ) {
    // Reset the output graph first so the build is deterministic.
    SVG_Nav2_HierarchyGraph_Clear( out_graph );
    if ( !out_graph ) {
        return false;
    }

    // Build one hierarchy node per region-layer node, preserving the vertical commitments from the region graph.
    for ( size_t i = 0; i < regionLayers.layers.size(); ++i ) {
        const nav2_region_layer_t &layer = regionLayers.layers[ i ];
        nav2_hierarchy_node_t node = {};
        node = SVG_Nav2_MakeHierarchyNodeFromLayer( layer, ( int32_t )i, SVG_Nav2_AllocateHierarchyNodeId( *out_graph ) );
        SVG_Nav2_HierarchyGraph_AppendNode( out_graph, node );
    }

    // Add edges by translating the region-layer graph edges into hierarchy edges.
    for ( const nav2_region_layer_edge_t &regionEdge : regionLayers.edges ) {
        nav2_region_layer_ref_t fromLayer = {};
        nav2_region_layer_ref_t toLayer = {};
        if ( !regionLayers.by_id.empty() ) {
            const auto fromIt = regionLayers.by_id.find( regionEdge.from_region_layer_id );
            const auto toIt = regionLayers.by_id.find( regionEdge.to_region_layer_id );
            if ( fromIt != regionLayers.by_id.end() ) {
                fromLayer = fromIt->second;
            }
            if ( toIt != regionLayers.by_id.end() ) {
                toLayer = toIt->second;
            }
        }
        if ( !fromLayer.IsValid() || !toLayer.IsValid() ) {
            continue;
        }

        const nav2_hierarchy_node_t &fromNode = out_graph->nodes[ ( size_t )fromLayer.region_layer_index ];
        const nav2_hierarchy_node_t &toNode = out_graph->nodes[ ( size_t )toLayer.region_layer_index ];
        nav2_hierarchy_edge_t edge = SVG_Nav2_MakeHierarchyEdgeFromRegionEdge( fromNode, toNode, regionEdge, SVG_Nav2_AllocateHierarchyEdgeId( *out_graph ) );
        SVG_Nav2_HierarchyGraph_AppendEdge( out_graph, edge );
    }

    // Summarize the build for debug and validation callers.
    if ( out_summary ) {
        *out_summary = {};
        out_summary->node_count = ( int32_t )out_graph->nodes.size();
        out_summary->edge_count = ( int32_t )out_graph->edges.size();
        for ( const nav2_hierarchy_node_t &node : out_graph->nodes ) {
            if ( ( node.flags & NAV2_HIERARCHY_NODE_FLAG_HAS_MOVER ) != 0 ) {
                out_summary->mover_node_count++;
            }
            if ( node.kind == nav2_hierarchy_node_kind_t::PortalEndpoint ) {
                out_summary->portal_node_count++;
            }
            if ( node.kind == nav2_hierarchy_node_kind_t::LadderEndpoint ) {
                out_summary->ladder_node_count++;
            }
            if ( node.kind == nav2_hierarchy_node_kind_t::StairBand ) {
                out_summary->stair_node_count++;
            }
            if ( ( node.flags & NAV2_HIERARCHY_NODE_FLAG_PARTIAL ) != 0 ) {
                out_summary->partial_node_count++;
            }
        }
    }

    if ( !SVG_Nav2_ValidateHierarchyGraph( *out_graph ) ) {
        return false;
    }
    return !out_graph->nodes.empty();
}

/**
* @brief Emit a bounded debug summary for a hierarchy graph.
* @param graph Graph to report.
* @param limit Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintHierarchyGraph( const nav2_hierarchy_graph_t &graph, const int32_t limit ) {
    // Avoid log spam when no report was requested.
    if ( limit <= 0 ) {
        return;
    }

    const int32_t reportCount = std::min( ( int32_t )graph.nodes.size(), limit );
    gi.dprintf( "[NAV2][Hierarchy] nodes=%d edges=%d report=%d\n", ( int32_t )graph.nodes.size(), ( int32_t )graph.edges.size(), reportCount );
    for ( int32_t i = 0; i < reportCount; ++i ) {
        const nav2_hierarchy_node_t &node = graph.nodes[ ( size_t )i ];
        gi.dprintf( "[NAV2][Hierarchy] id=%d kind=%d regionLayer=%d leaf=%d cluster=%d area=%d portal=%d mover=%d model=%d flags=0x%08x z=(%.1f..%.1f) pref=%.1f out=%d in=%d\n",
            node.node_id,
            ( int32_t )node.kind,
            node.region_layer_id,
            node.topology.leaf_index,
            node.topology.cluster_id,
            node.topology.area_id,
            node.topology.portal_id,
            node.mover_entnum,
            node.inline_model_index,
            node.flags,
            node.allowed_z_band.min_z,
            node.allowed_z_band.max_z,
            node.allowed_z_band.preferred_z,
            ( int32_t )node.outgoing_edge_ids.size(),
            ( int32_t )node.incoming_edge_ids.size() );
    }
}

/**
* @brief Keep the nav2 hierarchy module represented in the build.
**/
void SVG_Nav2_HierarchyGraph_ModuleAnchor( void ) {
}
