/********************************************************************
*
*
*	ServerGame: Nav2 Coarse Hierarchy Graph
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_region_layers.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Hierarchy Enumerations:
*
*
**/
/**
* @brief High-level hierarchy node kinds used by coarse A* and corridor extraction.
**/
enum class nav2_hierarchy_node_kind_t : uint8_t {
    None = 0,
    RegionLayer,
    PortalEndpoint,
    MoverBoarding,
    MoverRide,
    MoverExit,
    LadderEndpoint,
    StairBand,
    ClusterBand,
    LeafBand,
    GoalAnchor,
    Count
};

/**
* @brief High-level hierarchy edge kinds used by coarse A* and corridor extraction.
**/
enum class nav2_hierarchy_edge_kind_t : uint8_t {
    None = 0,
    RegionLink,
    PortalLink,
    MoverBoarding,
    MoverRide,
    MoverExit,
    LadderLink,
    StairLink,
    FallbackLink,
    Count
};

/**
* @brief Stable flags describing one hierarchy node.
**/
enum nav2_hierarchy_node_flag_t : uint32_t {
    NAV2_HIERARCHY_NODE_FLAG_NONE = 0,
    NAV2_HIERARCHY_NODE_FLAG_HAS_REGION_LAYER = ( 1u << 0 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_PORTAL = ( 1u << 1 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_LADDER = ( 1u << 2 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_STAIR = ( 1u << 3 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_MOVER = ( 1u << 4 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_ALLOWED_Z_BAND = ( 1u << 5 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_LEAF = ( 1u << 6 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_CLUSTER = ( 1u << 7 ),
    NAV2_HIERARCHY_NODE_FLAG_HAS_AREA = ( 1u << 8 ),
    NAV2_HIERARCHY_NODE_FLAG_PARTIAL = ( 1u << 9 ),
    NAV2_HIERARCHY_NODE_FLAG_FAT_PVS_RELEVANT = ( 1u << 10 )
};

/**
* @brief Stable flags describing one hierarchy edge.
**/
enum nav2_hierarchy_edge_flag_t : uint32_t {
    NAV2_HIERARCHY_EDGE_FLAG_NONE = 0,
    NAV2_HIERARCHY_EDGE_FLAG_PASSABLE = ( 1u << 0 ),
    NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_PORTAL = ( 1u << 1 ),
    NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_LADDER = ( 1u << 2 ),
    NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_STAIR = ( 1u << 3 ),
    NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_MOVER = ( 1u << 4 ),
    NAV2_HIERARCHY_EDGE_FLAG_ALLOWS_BOARDING = ( 1u << 5 ),
    NAV2_HIERARCHY_EDGE_FLAG_ALLOWS_RIDE = ( 1u << 6 ),
    NAV2_HIERARCHY_EDGE_FLAG_ALLOWS_EXIT = ( 1u << 7 ),
    NAV2_HIERARCHY_EDGE_FLAG_TOPOLOGY_MATCHED = ( 1u << 8 ),
    NAV2_HIERARCHY_EDGE_FLAG_TOPOLOGY_PENALIZED = ( 1u << 9 ),
    NAV2_HIERARCHY_EDGE_FLAG_HARD_INVALID = ( 1u << 10 ),
    NAV2_HIERARCHY_EDGE_FLAG_PARTIAL = ( 1u << 11 )
};


/**
*
*
*	Nav2 Hierarchy Data Structures:
*
*
**/
/**
* @brief Stable reference to one hierarchy node.
**/
struct nav2_hierarchy_node_ref_t {
    //! Stable node id.
    int32_t node_id = -1;
    //! Stable node index.
    int32_t node_index = -1;

    /**
    * @brief Return whether this reference points to a concrete hierarchy node.
    * @return True when the id and index are both valid.
    **/
    const bool IsValid() const {
        return node_id >= 0 && node_index >= 0;
    }
};

/**
* @brief Stable hierarchy node used by coarse search.
**/
struct nav2_hierarchy_node_t {
    //! Stable node id.
    int32_t node_id = -1;
    //! High-level node kind.
    nav2_hierarchy_node_kind_t kind = nav2_hierarchy_node_kind_t::None;
    //! Stable region-layer id when the node mirrors a region-layer entry.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Stable region-layer index when known.
    int32_t region_layer_index = -1;
    //! Stable connector id when this node mirrors a connector endpoint.
    int32_t connector_id = -1;
    //! Aggregated connector endpoint semantics when this node is connector-aware.
    uint32_t endpoint_semantics = NAV2_CONNECTOR_ENDPOINT_NONE;
    //! Aggregated connector transition semantics when this node is connector-aware.
    uint32_t transition_semantics = NAV2_CONNECTOR_TRANSITION_NONE;
    //! Stable mover entity number when known.
    int32_t mover_entnum = -1;
    //! Stable inline model index when known.
    int32_t inline_model_index = -1;
    //! Stable topology reference for this node.
    nav2_corridor_topology_ref_t topology = {};
    //! Committed Z band for this node.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Stable flags describing this node.
    uint32_t flags = NAV2_HIERARCHY_NODE_FLAG_NONE;
    //! Outgoing edge ids in deterministic order.
    std::vector<int32_t> outgoing_edge_ids = {};
    //! Incoming edge ids in deterministic order.
    std::vector<int32_t> incoming_edge_ids = {};
};

/**
* @brief Stable hierarchy edge used by coarse search.
**/
struct nav2_hierarchy_edge_t {
    //! Stable edge id.
    int32_t edge_id = -1;
    //! Source node id.
    int32_t from_node_id = -1;
    //! Destination node id.
    int32_t to_node_id = -1;
    //! Edge kind.
    nav2_hierarchy_edge_kind_t kind = nav2_hierarchy_edge_kind_t::None;
    //! Stable edge flags.
    uint32_t flags = NAV2_HIERARCHY_EDGE_FLAG_NONE;
    //! Base traversal cost for the edge.
    double base_cost = 0.0;
    //! Additional topology penalty for the edge.
    double topology_penalty = 0.0;
    //! Allowed Z band for the edge.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Connector id attached to the edge when known.
    int32_t connector_id = -1;
    //! Connector transition semantics inherited from extraction or topology classification.
    uint32_t transition_semantics = NAV2_CONNECTOR_TRANSITION_NONE;
    //! Region-layer id attached to the edge when known.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Mover reference attached to the edge when known.
    nav2_corridor_mover_ref_t mover_ref = {};
};

/**
* @brief Stable hierarchy graph used by coarse A*.
**/
struct nav2_hierarchy_graph_t {
    //! Hierarchy nodes in deterministic order.
    std::vector<nav2_hierarchy_node_t> nodes = {};
    //! Hierarchy edges in deterministic order.
    std::vector<nav2_hierarchy_edge_t> edges = {};
    //! Stable node id lookup.
    std::unordered_map<int32_t, nav2_hierarchy_node_ref_t> by_id = {};
};

/**
* @brief Compact summary of one hierarchy build.
**/
struct nav2_hierarchy_summary_t {
    //! Number of hierarchy nodes.
    int32_t node_count = 0;
    //! Number of hierarchy edges.
    int32_t edge_count = 0;
    //! Number of mover-aware nodes.
    int32_t mover_node_count = 0;
    //! Number of portal nodes.
    int32_t portal_node_count = 0;
    //! Number of ladder nodes.
    int32_t ladder_node_count = 0;
    //! Number of stair nodes.
    int32_t stair_node_count = 0;
    //! Number of partial nodes.
    int32_t partial_node_count = 0;
};


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
void SVG_Nav2_HierarchyGraph_Clear( nav2_hierarchy_graph_t *graph );

/**
* @brief Resolve a hierarchy node by stable id.
* @param graph Graph to inspect.
* @param node_id Stable node id to resolve.
* @param out_ref [out] Resolved hierarchy reference.
* @return True when the node exists.
**/
const bool SVG_Nav2_HierarchyGraph_TryResolve( const nav2_hierarchy_graph_t &graph, const int32_t node_id, nav2_hierarchy_node_ref_t *out_ref );

/**
* @brief Append one hierarchy node to the graph.
* @param graph Graph to mutate.
* @param node Node payload to append.
* @return True when the node was appended.
**/
const bool SVG_Nav2_HierarchyGraph_AppendNode( nav2_hierarchy_graph_t *graph, const nav2_hierarchy_node_t &node );

/**
* @brief Append one hierarchy edge to the graph and wire adjacency lists.
* @param graph Graph to mutate.
* @param edge Edge payload to append.
* @return True when the edge was appended.
**/
const bool SVG_Nav2_HierarchyGraph_AppendEdge( nav2_hierarchy_graph_t *graph, const nav2_hierarchy_edge_t &edge );

/**
* @brief Validate a hierarchy graph for stable ids and adjacency consistency.
* @param graph Graph to inspect.
* @return True when hierarchical nodes/edges are internally consistent.
**/
const bool SVG_Nav2_ValidateHierarchyGraph( const nav2_hierarchy_graph_t &graph );

/**
 * @brief Build a coarse hierarchy graph from topology publication, connectors, and region layers.
 * @param topologyArtifact Topology publication validating the current static-nav classification state.
 * @param connectors Connector collection providing explicit endpoint and transition semantics.
 * @param regionLayers Region-layer graph providing vertical and topological commitments.
* @param out_graph [out] Hierarchy graph receiving the coarse graph.
* @param out_summary [out] Optional summary.
* @return True when at least one hierarchy node was produced.
**/
const bool SVG_Nav2_BuildHierarchyGraph( const nav2_topology_artifact_t &topologyArtifact, const nav2_connector_list_t &connectors,
    const nav2_region_layer_graph_t &regionLayers, nav2_hierarchy_graph_t *out_graph, nav2_hierarchy_summary_t *out_summary = nullptr );

/**
* @brief Emit a bounded debug summary for a hierarchy graph.
* @param graph Graph to report.
* @param limit Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintHierarchyGraph( const nav2_hierarchy_graph_t &graph, const int32_t limit = 16 );

/**
* @brief Keep the nav2 hierarchy module represented in the build.
**/
void SVG_Nav2_HierarchyGraph_ModuleAnchor( void );
