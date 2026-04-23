/********************************************************************
*
*
*	ServerGame: Nav2 Macro Traversal Aids
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_distance_fields.h"
#include "svgame/nav2/nav2_region_layers.h"
#include "svgame/nav2/nav2_span_grid.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Macro Traversal Enumerations:
*
*
**/
/**
*	@brief	Stable macro-node semantic kinds used for optimization-layer traversal guidance.
*	@note	Task 12.2 keeps macro semantics explicit so optimization stays separate from correctness-critical
*			topology and corridor commitments.
**/
enum class nav2_macro_node_kind_t : uint8_t {
    None = 0,
    OpenAreaRegion,
    CorridorRun,
    PortalAnchor,
    StairAnchor,
    LadderAnchor,
    RegionBoundary,
    ClearanceBottleneck,
    GoalAnchor,
    Count
};

/**
*	@brief	Stable macro-edge semantic kinds used by the optimization layer.
**/
enum class nav2_macro_edge_kind_t : uint8_t {
    None = 0,
    RegionCompression,
    PortalGuidance,
    StairGuidance,
    LadderGuidance,
    DirectionalRun,
    CenterlineRun,
    FallbackLink,
    Count
};

/**
*	@brief	Node-level flags describing available macro traversal semantics.
**/
enum nav2_macro_node_flag_t : uint32_t {
    NAV2_MACRO_NODE_FLAG_NONE = 0,
    NAV2_MACRO_NODE_FLAG_HAS_REGION_LAYER = ( 1u << 0 ),
    NAV2_MACRO_NODE_FLAG_HAS_CONNECTOR = ( 1u << 1 ),
    NAV2_MACRO_NODE_FLAG_HAS_PORTAL = ( 1u << 2 ),
    NAV2_MACRO_NODE_FLAG_HAS_STAIR = ( 1u << 3 ),
    NAV2_MACRO_NODE_FLAG_HAS_LADDER = ( 1u << 4 ),
    NAV2_MACRO_NODE_FLAG_HAS_CENTERLINE = ( 1u << 5 ),
    NAV2_MACRO_NODE_FLAG_HAS_BOTTLENECK = ( 1u << 6 ),
    NAV2_MACRO_NODE_FLAG_FAT_PVS_RELEVANT = ( 1u << 7 )
};

/**
*	@brief	Edge-level flags describing available macro traversal semantics.
**/
enum nav2_macro_edge_flag_t : uint32_t {
    NAV2_MACRO_EDGE_FLAG_NONE = 0,
    NAV2_MACRO_EDGE_FLAG_PASSABLE = ( 1u << 0 ),
    NAV2_MACRO_EDGE_FLAG_REGION_COMPRESSED = ( 1u << 1 ),
    NAV2_MACRO_EDGE_FLAG_PORTAL_GUIDED = ( 1u << 2 ),
    NAV2_MACRO_EDGE_FLAG_STAIR_GUIDED = ( 1u << 3 ),
    NAV2_MACRO_EDGE_FLAG_LADDER_GUIDED = ( 1u << 4 ),
    NAV2_MACRO_EDGE_FLAG_CENTERLINE_GUIDED = ( 1u << 5 ),
    NAV2_MACRO_EDGE_FLAG_DIRECTIONAL_RUN = ( 1u << 6 ),
    NAV2_MACRO_EDGE_FLAG_CORRIDOR_CONSTRAINED = ( 1u << 7 ),
    NAV2_MACRO_EDGE_FLAG_TOPOLOGY_PENALIZED = ( 1u << 8 )
};


/**
*
*
*	Nav2 Macro Traversal Data Structures:
*
*
**/
/**
*	@brief	Build-policy settings for macro traversal graph extraction.
**/
struct nav2_macro_traversal_build_params_t {
    //! Maximum sparse-XY compression radius for open-area region aggregation.
    int32_t open_area_compression_radius_cells = 4;
    //! Maximum sparse-XY run length for directional-run compression.
    int32_t directional_run_max_cells = 12;
    //! Minimum centerline distance required before centerline-preference flags are emitted.
    double centerline_preference_min_distance = 24.0;
    //! Minimum region-boundary distance required for open-area region compression.
    double open_area_min_boundary_distance = 48.0;
    //! Minimum clearance-bottleneck distance before bottleneck markers are emitted.
    double clearance_bottleneck_min_distance = 8.0;
    //! Scale applied to distance-field tie-breaker guidance in macro score composition.
    double distance_field_score_scale = 1.0;
    //! True when corridor constraints should explicitly gate macro-edge eligibility.
    bool enforce_corridor_constraints = true;
    //! True when optional fat-PVS relevance hints may bias macro score composition.
    bool use_fat_pvs_relevance_bias = true;
};

/**
*	@brief	Stable macro-node record used by optimization-layer traversal guidance.
**/
struct nav2_macro_node_t {
    //! Stable macro-node id.
    int32_t macro_node_id = -1;
    //! Semantic kind of this macro node.
    nav2_macro_node_kind_t kind = nav2_macro_node_kind_t::None;
    //! Stable region-layer id when known.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Stable connector id when known.
    int32_t connector_id = -1;
    //! Stable span id that seeded this macro node.
    int32_t seed_span_id = -1;
    //! Stable topology metadata for this node.
    nav2_corridor_topology_ref_t topology = {};
    //! Representative world-space node origin.
    Vector3 world_origin = {};
    //! Node-level semantic flags.
    uint32_t flags = NAV2_MACRO_NODE_FLAG_NONE;
    //! Outgoing macro-edge ids in deterministic order.
    std::vector<int32_t> outgoing_edge_ids = {};
    //! Incoming macro-edge ids in deterministic order.
    std::vector<int32_t> incoming_edge_ids = {};
    //! Nearest-portal utility distance mirrored from distance fields when available.
    double portal_distance = 0.0;
    //! Nearest-stair utility distance mirrored from distance fields when available.
    double stair_distance = 0.0;
    //! Nearest-ladder utility distance mirrored from distance fields when available.
    double ladder_distance = 0.0;
    //! Open-space centerline utility distance mirrored from distance fields when available.
    double centerline_distance = 0.0;
    //! Obstacle-boundary utility distance mirrored from distance fields when available.
    double obstacle_distance = 0.0;
    //! Clearance-bottleneck utility distance mirrored from distance fields when available.
    double clearance_bottleneck_distance = 0.0;
    //! Optional distance-field tie-break score mirrored from the seed record.
    double heuristic_tie_breaker = 0.0;
};

/**
*	@brief	Stable macro-edge record used by optimization-layer traversal guidance.
**/
struct nav2_macro_edge_t {
    //! Stable macro-edge id.
    int32_t macro_edge_id = -1;
    //! Source macro-node id.
    int32_t from_macro_node_id = -1;
    //! Destination macro-node id.
    int32_t to_macro_node_id = -1;
    //! Semantic kind for this macro edge.
    nav2_macro_edge_kind_t kind = nav2_macro_edge_kind_t::None;
    //! Edge-level semantic flags.
    uint32_t flags = NAV2_MACRO_EDGE_FLAG_NONE;
    //! Base macro traversal estimate.
    double macro_cost = 0.0;
    //! Additional score penalty applied by optimization heuristics.
    double macro_penalty = 0.0;
    //! Optional connector id used by this edge.
    int32_t connector_id = -1;
    //! Optional region-layer id used by this edge.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Optional mover reference inherited from topology/corridor constraints.
    nav2_corridor_mover_ref_t mover_ref = {};
};

/**
*	@brief	Stable macro graph used as an optimization-layer guidance structure.
**/
struct nav2_macro_graph_t {
    //! Build params used to generate this graph.
    nav2_macro_traversal_build_params_t params = {};
    //! Macro nodes in deterministic order.
    std::vector<nav2_macro_node_t> nodes = {};
    //! Macro edges in deterministic order.
    std::vector<nav2_macro_edge_t> edges = {};
    //! Stable id lookup for macro nodes.
    std::unordered_map<int32_t, int32_t> node_index_by_id = {};
    //! Stable id lookup for macro edges.
    std::unordered_map<int32_t, int32_t> edge_index_by_id = {};
};

/**
*	@brief	Compact summary counters for one macro traversal build pass.
**/
struct nav2_macro_traversal_summary_t {
    //! Number of macro nodes.
    int32_t node_count = 0;
    //! Number of macro edges.
    int32_t edge_count = 0;
    //! Number of open-area region nodes.
    int32_t open_area_node_count = 0;
    //! Number of corridor-run nodes.
    int32_t corridor_run_node_count = 0;
    //! Number of portal-anchor nodes.
    int32_t portal_node_count = 0;
    //! Number of stair-anchor nodes.
    int32_t stair_node_count = 0;
    //! Number of ladder-anchor nodes.
    int32_t ladder_node_count = 0;
    //! Number of centerline-guided edges.
    int32_t centerline_edge_count = 0;
    //! Number of directional-run edges.
    int32_t directional_run_edge_count = 0;
    //! Number of corridor-constrained edges.
    int32_t corridor_constrained_edge_count = 0;
    //! Number of fat-PVS relevance-marked nodes.
    int32_t fat_pvs_relevant_node_count = 0;
    //! Aggregate macro-cost average.
    double average_macro_cost = 0.0;
    //! Aggregate macro-penalty average.
    double average_macro_penalty = 0.0;
};

/**
*	@brief	Stable traversal estimate result resolved between two macro nodes.
**/
struct nav2_macro_traversal_estimate_t {
    //! True when an estimate was produced.
    bool valid = false;
    //! Source macro-node id used for the estimate.
    int32_t from_macro_node_id = -1;
    //! Destination macro-node id used for the estimate.
    int32_t to_macro_node_id = -1;
    //! Estimated macro traversal cost.
    double estimated_cost = 0.0;
    //! Estimated macro traversal penalty.
    double estimated_penalty = 0.0;
    //! Number of edges touched while building the estimate.
    int32_t edge_hop_count = 0;
};


/**
*
*
*	Nav2 Macro Traversal Public API:
*
*
**/
/**
*	@brief	Reset one macro graph to deterministic defaults.
*	@param	graph	[in,out] Macro graph to clear.
**/
void SVG_Nav2_MacroTraversal_Clear( nav2_macro_graph_t *graph );

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
    nav2_macro_graph_t *out_graph, nav2_macro_traversal_summary_t *out_summary = nullptr );

/**
*	@brief	Resolve one macro-node record by stable id.
*	@param	graph	Macro graph to inspect.
*	@param	macro_node_id	Stable macro-node id.
*	@return	Pointer to the macro node when found, or `nullptr` otherwise.
**/
const nav2_macro_node_t *SVG_Nav2_MacroTraversal_TryResolveNode( const nav2_macro_graph_t &graph, const int32_t macro_node_id );

/**
*	@brief	Resolve one macro-edge record by stable id.
*	@param	graph	Macro graph to inspect.
*	@param	macro_edge_id	Stable macro-edge id.
*	@return	Pointer to the macro edge when found, or `nullptr` otherwise.
**/
const nav2_macro_edge_t *SVG_Nav2_MacroTraversal_TryResolveEdge( const nav2_macro_graph_t &graph, const int32_t macro_edge_id );

/**
*	@brief	Build a bounded traversal estimate between two macro nodes.
*	@param	graph	Macro graph to inspect.
*	@param	from_macro_node_id	Source macro-node id.
*	@param	to_macro_node_id	Destination macro-node id.
*	@param	out_estimate	[out] Estimate payload receiving aggregate traversal guidance.
*	@return	True when an estimate could be produced.
**/
const bool SVG_Nav2_MacroTraversal_BuildEstimate( const nav2_macro_graph_t &graph, const int32_t from_macro_node_id,
    const int32_t to_macro_node_id, nav2_macro_traversal_estimate_t *out_estimate );

/**
*	@brief	Emit bounded macro traversal diagnostics.
*	@param	graph	Macro graph to report.
*	@param	summary	Optional precomputed summary.
*	@param	limit	Maximum number of node/edge records to print.
**/
void SVG_Nav2_DebugPrintMacroTraversal( const nav2_macro_graph_t &graph, const nav2_macro_traversal_summary_t *summary, const int32_t limit = 16 );

/**
*	@brief	Keep the nav2 macro traversal module represented in the build.
**/
void SVG_Nav2_MacroTraversal_ModuleAnchor( void );
