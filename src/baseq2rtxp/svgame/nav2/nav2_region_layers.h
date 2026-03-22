/********************************************************************
*
*
*	ServerGame: Nav2 Region-Layer Decomposition
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_span_grid.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Region-Layer Enumerations:
*
*
**/
/**
* @brief Stable region-layer semantic kinds used by the coarse hierarchy.
* @note These values intentionally commit vertical intent early so coarse A* does not
*  fall back to blended-Z search behavior.
**/
enum class nav2_region_layer_kind_t : uint8_t {
    //! No semantic kind has been assigned yet.
    None = 0,
    //! A cluster-scale band derived from BSP connectivity.
    ClusterBand,
    //! A leaf-scale band derived from BSP connectivity.
    LeafBand,
    //! A same-floor connected component.
    FloorBand,
    //! A stair-connected band.
    StairBand,
    //! A portal-separated endpoint band.
    PortalEndpoint,
    //! A ladder-linked band.
    LadderBand,
    //! A mover-aware band.
    MoverBand,
    //! A fallback band used while the hierarchy is being refined.
    FallbackBand,
    //! Count sentinel.
    Count
};

/**
* @brief Explicit edge kind used by region-layer graph connections.
* @note These edges carry enough semantic detail for corridor extraction and coarse A*.
**/
enum class nav2_region_layer_edge_kind_t : uint8_t {
    //! No edge kind has been assigned yet.
    None = 0,
    //! Same-band connection inside a region layer.
    SameBand,
    //! Portal transition between region layers.
    PortalTransition,
    //! Stair transition between region layers.
    StairTransition,
    //! Ladder transition between region layers.
    LadderTransition,
    //! Mover boarding transition.
    MoverBoarding,
    //! Mover ride transition.
    MoverRide,
    //! Mover exit transition.
    MoverExit,
    //! Fallback transition retained for partial results.
    FallbackTransition,
    //! Count sentinel.
    Count
};

/**
* @brief Flags describing stable coarse properties of one region-layer node.
**/
enum nav2_region_layer_flag_t : uint32_t {
    NAV2_REGION_LAYER_FLAG_NONE = 0,
    NAV2_REGION_LAYER_FLAG_HAS_CLUSTER = ( 1u << 0 ),
    NAV2_REGION_LAYER_FLAG_HAS_LEAF = ( 1u << 1 ),
    NAV2_REGION_LAYER_FLAG_HAS_AREA = ( 1u << 2 ),
    NAV2_REGION_LAYER_FLAG_HAS_PORTAL = ( 1u << 3 ),
    NAV2_REGION_LAYER_FLAG_HAS_ALLOWED_Z_BAND = ( 1u << 4 ),
    NAV2_REGION_LAYER_FLAG_HAS_LADDER = ( 1u << 5 ),
    NAV2_REGION_LAYER_FLAG_HAS_STAIR = ( 1u << 6 ),
    NAV2_REGION_LAYER_FLAG_HAS_MOVER = ( 1u << 7 ),
    NAV2_REGION_LAYER_FLAG_PORTAL_BARRIER = ( 1u << 8 ),
    NAV2_REGION_LAYER_FLAG_FAT_PVS_RELEVANT = ( 1u << 9 ),
    NAV2_REGION_LAYER_FLAG_PARTIAL = ( 1u << 10 )
};

/**
* @brief Flags describing stable properties of one region-layer edge.
**/
enum nav2_region_layer_edge_flag_t : uint32_t {
    NAV2_REGION_LAYER_EDGE_FLAG_NONE = 0,
    NAV2_REGION_LAYER_EDGE_FLAG_PASSABLE = ( 1u << 0 ),
    NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_PORTAL = ( 1u << 1 ),
    NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_STAIR = ( 1u << 2 ),
    NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_LADDER = ( 1u << 3 ),
    NAV2_REGION_LAYER_EDGE_FLAG_REQUIRES_MOVER = ( 1u << 4 ),
    NAV2_REGION_LAYER_EDGE_FLAG_ALLOWS_MOVER_BOARDING = ( 1u << 5 ),
    NAV2_REGION_LAYER_EDGE_FLAG_ALLOWS_MOVER_RIDE = ( 1u << 6 ),
    NAV2_REGION_LAYER_EDGE_FLAG_ALLOWS_MOVER_EXIT = ( 1u << 7 ),
    NAV2_REGION_LAYER_EDGE_FLAG_Z_BAND_CONSTRAINED = ( 1u << 8 ),
    NAV2_REGION_LAYER_EDGE_FLAG_TOPOLOGY_MATCHED = ( 1u << 9 ),
    NAV2_REGION_LAYER_EDGE_FLAG_TOPOLOGY_PENALIZED = ( 1u << 10 ),
    NAV2_REGION_LAYER_EDGE_FLAG_HARD_INVALID = ( 1u << 11 )
};


/**
*
*
*	Nav2 Region-Layer Data Structures:
*
*
**/
/**
* @brief Stable reference to one region-layer node.
* @note Scalar ids keep the hierarchy publishable to worker threads and serialization-friendly.
**/
struct nav2_region_layer_ref_t {
    //! Stable region-layer id.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Stable index inside the owning graph vector.
    int32_t region_layer_index = -1;

    /**
    * @brief Return whether this reference points to a concrete region-layer slot.
    * @return True when the id and index are both valid.
    **/
    const bool IsValid() const {
        return region_layer_id >= 0 && region_layer_index >= 0;
    }
};

/**
* @brief Stable region-layer node used by coarse A* and corridor extraction.
**/
struct nav2_region_layer_t {
    //! Stable region-layer id.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Semantic kind of this region-layer node.
    nav2_region_layer_kind_t kind = nav2_region_layer_kind_t::None;
    //! Stable BSP leaf id when known.
    int32_t leaf_id = -1;
    //! Stable BSP cluster id when known.
    int32_t cluster_id = -1;
    //! Stable BSP area id when known.
    int32_t area_id = -1;
    //! Stable connector id when this node represents a connector endpoint band.
    int32_t connector_id = -1;
    //! Stable mover entity number when this node is mover-aware.
    int32_t mover_entnum = -1;
    //! Stable inline-model index when this node is mover-aware.
    int32_t inline_model_index = -1;
    //! Committed vertical band for this node.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Stable topology reference for the node.
    nav2_corridor_topology_ref_t topology = {};
    //! Associated tile reference when known.
    nav2_corridor_tile_ref_t tile_ref = {};
    //! Stable node flags.
    uint32_t flags = NAV2_REGION_LAYER_FLAG_NONE;
    //! Outgoing edge ids in deterministic order.
    std::vector<int32_t> outgoing_edge_ids = {};
    //! Incoming edge ids in deterministic order.
    std::vector<int32_t> incoming_edge_ids = {};
};

/**
* @brief Stable region-layer edge used by coarse A* and corridor extraction.
**/
struct nav2_region_layer_edge_t {
    //! Stable edge id.
    int32_t edge_id = -1;
    //! Source region-layer id.
    int32_t from_region_layer_id = NAV_REGION_ID_NONE;
    //! Destination region-layer id.
    int32_t to_region_layer_id = NAV_REGION_ID_NONE;
    //! Edge semantic kind.
    nav2_region_layer_edge_kind_t kind = nav2_region_layer_edge_kind_t::None;
    //! Stable edge flags.
    uint32_t flags = NAV2_REGION_LAYER_EDGE_FLAG_NONE;
    //! Base traversal cost used by the coarse graph.
    double base_cost = 0.0;
    //! Additional topology penalty applied by the builder.
    double topology_penalty = 0.0;
    //! Committed vertical band for the transition.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Connector reference used by the transition when known.
    int32_t connector_id = -1;
    //! Topology reference inherited from the source.
    nav2_corridor_topology_ref_t topology = {};
    //! Mover reference inherited from the source.
    nav2_corridor_mover_ref_t mover_ref = {};
};

/**
* @brief Stable container for the region-layer graph.
**/
struct nav2_region_layer_graph_t {
    //! Region-layer nodes in deterministic order.
    std::vector<nav2_region_layer_t> layers = {};
    //! Region-layer edges in deterministic order.
    std::vector<nav2_region_layer_edge_t> edges = {};
    //! Stable node id lookup.
    std::unordered_map<int32_t, nav2_region_layer_ref_t> by_id = {};
};

/**
* @brief Compact summary of one region-layer build.
**/
struct nav2_region_layer_summary_t {
    //! Number of region-layer nodes.
    int32_t layer_count = 0;
    //! Number of region-layer edges.
    int32_t edge_count = 0;
    //! Number of portal-derived nodes.
    int32_t portal_layer_count = 0;
    //! Number of stair-derived nodes.
    int32_t stair_layer_count = 0;
    //! Number of ladder-derived nodes.
    int32_t ladder_layer_count = 0;
    //! Number of mover-aware nodes.
    int32_t mover_layer_count = 0;
    //! Number of partial/fallback nodes.
    int32_t partial_layer_count = 0;
};


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
void SVG_Nav2_RegionLayerGraph_Clear( nav2_region_layer_graph_t *graph );

/**
* @brief Resolve a region-layer reference by stable id.
* @param graph Graph to inspect.
* @param region_layer_id Stable region-layer id to resolve.
* @param out_ref [out] Resolved region-layer reference.
* @return True when the region-layer exists.
**/
const bool SVG_Nav2_RegionLayerGraph_TryResolve( const nav2_region_layer_graph_t &graph, const int32_t region_layer_id, nav2_region_layer_ref_t *out_ref );

/**
* @brief Append one region-layer node to the graph.
* @param graph Graph to mutate.
* @param layer Region-layer payload to append.
* @return True when the node was appended.
**/
const bool SVG_Nav2_RegionLayerGraph_AppendLayer( nav2_region_layer_graph_t *graph, const nav2_region_layer_t &layer );

/**
* @brief Append one region-layer edge to the graph and wire it into the adjacency lists.
* @param graph Graph to mutate.
* @param edge Region-layer edge payload to append.
* @return True when the edge was appended.
**/
const bool SVG_Nav2_RegionLayerGraph_AppendEdge( nav2_region_layer_graph_t *graph, const nav2_region_layer_edge_t &edge );

/**
* @brief Build a region-layer decomposition from a sparse span grid plus connector list.
* @param grid Sparse span grid providing the precision substrate.
* @param connectors Connector collection used to attach higher-level route commitments.
* @param out_graph [out] Region-layer graph receiving the decomposition.
* @param out_summary [out] Optional build summary.
* @return True when at least one region-layer node was produced.
**/
const bool SVG_Nav2_BuildRegionLayers( const nav2_span_grid_t &grid, const nav2_connector_list_t &connectors, nav2_region_layer_graph_t *out_graph, nav2_region_layer_summary_t *out_summary = nullptr );

/**
* @brief Emit a bounded debug summary for a region-layer graph.
* @param graph Graph to report.
* @param limit Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintRegionLayers( const nav2_region_layer_graph_t &graph, const int32_t limit = 16 );

/**
* @brief Keep the nav2 region-layer module represented in the build.
**/
void SVG_Nav2_RegionLayer_ModuleAnchor( void );
