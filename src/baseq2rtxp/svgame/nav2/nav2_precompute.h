/********************************************************************
*
*
*	ServerGame: Nav2 Coarse Lower-Bound Precomputation
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_hierarchy_graph.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Precompute Enumerations:
*
*
**/
/**
* @brief	Stable lower-bound source classification for coarse precompute records.
* @note	These values document how each cached bound was derived so diagnostics can distinguish
* 		landmark, connector, and fallback estimates without depending on opaque internals.
**/
enum class nav2_precompute_lb_source_t : uint8_t {
	None = 0,
	LandmarkTriangle,
	ConnectorCrossing,
	RegionLayerAdjacency,
	ClusterPair,
	PortalPair,
	FallbackHeuristic,
	Count
};

/**
* @brief	Feature bits describing what data a published lower-bound cache contains.
**/
enum nav2_precompute_feature_flag_t : uint32_t {
	NAV2_PRECOMPUTE_FEATURE_NONE = 0,
	NAV2_PRECOMPUTE_FEATURE_LANDMARKS = ( 1u << 0 ),
	NAV2_PRECOMPUTE_FEATURE_PORTAL_PAIR_BOUNDS = ( 1u << 1 ),
	NAV2_PRECOMPUTE_FEATURE_CLUSTER_PAIR_BOUNDS = ( 1u << 2 ),
	NAV2_PRECOMPUTE_FEATURE_REGION_LAYER_EDGE_COSTS = ( 1u << 3 ),
	NAV2_PRECOMPUTE_FEATURE_CONNECTOR_BASE_COSTS = ( 1u << 4 ),
	NAV2_PRECOMPUTE_FEATURE_IMMUTABLE_PUBLISHED = ( 1u << 5 ),
	NAV2_PRECOMPUTE_FEATURE_SERIALIZATION_READY = ( 1u << 6 )
};


/**
*
*
*	Nav2 Precompute Data Structures:
*
*
**/
/**
* @brief	Build-policy controls for coarse lower-bound precompute.
* @note	Task 13.1 focuses on practical memory usage; all fields stay scalar/pointer-free so the
* 		policy can be snapshotted or serialized later with no ownership hazards.
**/
struct nav2_precompute_policy_t {
	//! Maximum number of landmark nodes selected from the hierarchy graph.
	int32_t max_landmarks = 8;
	//! Maximum number of portal-pair records retained.
	int32_t max_portal_pairs = 4096;
	//! Maximum number of cluster-pair records retained.
	int32_t max_cluster_pairs = 2048;
	//! True when portal-pair lower bounds should be built.
	bool enable_portal_pair_bounds = true;
	//! True when cluster-pair lower bounds should be built.
	bool enable_cluster_pair_bounds = true;
	//! True when region-layer edge base costs should be cached explicitly.
	bool enable_region_layer_edge_costs = true;
	//! True when connector crossing base costs should be cached explicitly.
	bool enable_connector_crossing_costs = true;
	//! True when this cache should be marked serialization-ready for later persistence tasks.
	bool mark_serialization_ready = false;
};

/**
* @brief	One immutable landmark descriptor used for triangle-inequality lower bounds.
**/
struct nav2_precompute_landmark_t {
	//! Stable hierarchy node id chosen as landmark anchor.
	int32_t hierarchy_node_id = -1;
	//! Stable index in the landmark vector.
	int32_t landmark_index = -1;
	//! Topology metadata mirrored from the anchor hierarchy node.
	nav2_corridor_topology_ref_t topology = {};
	//! Preferred Z used for quick vertical compatibility checks.
	double preferred_z = 0.0;
};

/**
* @brief	One cached lower-bound pair entry keyed by two stable ids.
**/
struct nav2_precompute_pair_bound_t {
	//! Stable key A (ordered key: min id).
	int32_t key_a = -1;
	//! Stable key B (ordered key: max id).
	int32_t key_b = -1;
	//! Cached lower-bound value for the pair.
	double lower_bound = 0.0;
	//! Source classifier describing how this value was derived.
	nav2_precompute_lb_source_t source = nav2_precompute_lb_source_t::None;
};

/**
* @brief	One cached directed edge base-cost entry for region-layer or connector traversal.
**/
struct nav2_precompute_edge_cost_t {
	//! Stable source node id.
	int32_t from_node_id = -1;
	//! Stable destination node id.
	int32_t to_node_id = -1;
	//! Base traversal cost copied from coarse topology.
	double base_cost = 0.0;
	//! Optional penalty component mirrored from topology edge metadata.
	double topology_penalty = 0.0;
};

/**
* @brief	Immutable coarse lower-bound cache published to worker-safe readers.
* @note	This cache stores only stable ids/scalars. No raw pointers are retained.
**/
struct nav2_precompute_cache_t {
	//! Build policy snapshot used to produce this cache.
	nav2_precompute_policy_t policy = {};
	//! Feature flags describing populated cache sections.
	uint32_t feature_flags = NAV2_PRECOMPUTE_FEATURE_NONE;
	//! Static-nav version this cache was built against.
	uint32_t static_nav_version = 0;
	//! Stable hierarchy node count at build time.
	int32_t hierarchy_node_count = 0;
	//! Stable hierarchy edge count at build time.
	int32_t hierarchy_edge_count = 0;
	//! Landmark descriptors used for landmark-triangle lower bounds.
	std::vector<nav2_precompute_landmark_t> landmarks = {};
	//! Distances from each hierarchy node to each landmark (flattened node-major storage).
	std::vector<double> node_to_landmark_distances = {};
	//! Cached portal-pair lower bounds.
	std::vector<nav2_precompute_pair_bound_t> portal_pair_bounds = {};
	//! Cached cluster-pair lower bounds.
	std::vector<nav2_precompute_pair_bound_t> cluster_pair_bounds = {};
	//! Cached region-layer edge costs.
	std::vector<nav2_precompute_edge_cost_t> region_layer_edge_costs = {};
	//! Cached connector crossing base costs.
	std::vector<nav2_precompute_edge_cost_t> connector_crossing_costs = {};
};

/**
* @brief	Bounded summary for one precompute build pass.
**/
struct nav2_precompute_summary_t {
	//! True when cache build succeeded.
	bool success = false;
	//! Number of hierarchy nodes considered.
	int32_t hierarchy_node_count = 0;
	//! Number of hierarchy edges considered.
	int32_t hierarchy_edge_count = 0;
	//! Number of selected landmarks.
	int32_t landmark_count = 0;
	//! Number of portal-pair bound records.
	int32_t portal_pair_bound_count = 0;
	//! Number of cluster-pair bound records.
	int32_t cluster_pair_bound_count = 0;
	//! Number of cached region-layer edge costs.
	int32_t region_layer_edge_cost_count = 0;
	//! Number of cached connector crossing costs.
	int32_t connector_crossing_cost_count = 0;
	//! Approximate memory footprint in bytes.
	uint64_t approx_bytes = 0;
};

/**
* @brief	Result payload for one lower-bound query.
**/
struct nav2_precompute_lb_result_t {
	//! True when a lower bound could be resolved.
	bool valid = false;
	//! Lower-bound value returned by the cache.
	double lower_bound = 0.0;
	//! Source classifier for the returned lower bound.
	nav2_precompute_lb_source_t source = nav2_precompute_lb_source_t::None;
};


/**
*
*
*	Nav2 Precompute Public API:
*
*
**/
/**
* @brief	Reset one coarse lower-bound cache to deterministic defaults.
* @param	cache	[in,out] Cache to clear.
**/
void SVG_Nav2_Precompute_ClearCache( nav2_precompute_cache_t *cache );

/**
* @brief	Build coarse lower-bound precompute data from a hierarchy graph.
* @param	hierarchy	Hierarchy graph providing coarse traversal topology.
* @param	policy	Build policy controlling optional sections and memory caps.
* @param	staticNavVersion	Static-nav version stamp bound to this cache.
* @param	outCache	[out] Cache receiving built lower-bound data.
* @param	outSummary	[out] Optional bounded summary.
* @return	True when build produced a usable cache.
**/
const bool SVG_Nav2_Precompute_BuildFromHierarchy( const nav2_hierarchy_graph_t &hierarchy,
	const nav2_precompute_policy_t &policy, const uint32_t staticNavVersion,
	nav2_precompute_cache_t *outCache, nav2_precompute_summary_t *outSummary = nullptr );

/**
* @brief	Publish a built lower-bound cache for immutable shared queries.
* @param	cache	Cache to copy into published immutable storage.
* @return	True when publication succeeded.
**/
const bool SVG_Nav2_Precompute_PublishCache( const nav2_precompute_cache_t &cache );

/**
* @brief	Reset the published immutable lower-bound cache.
**/
void SVG_Nav2_Precompute_ClearPublishedCache( void );

/**
* @brief	Get the currently published immutable lower-bound cache.
* @return	Pointer to published cache, or `nullptr` when no cache is published.
**/
const nav2_precompute_cache_t *SVG_Nav2_Precompute_GetPublishedCache( void );

/**
* @brief	Query a coarse lower bound between two hierarchy nodes from a specific cache.
* @param	cache	Cache to query.
* @param	fromNodeId	Stable source hierarchy node id.
* @param	toNodeId	Stable destination hierarchy node id.
* @param	outResult	[out] Lower-bound result payload.
* @return	True when the query executed.
**/
const bool SVG_Nav2_Precompute_QueryLowerBound( const nav2_precompute_cache_t &cache,
	const int32_t fromNodeId, const int32_t toNodeId, nav2_precompute_lb_result_t *outResult );

/**
* @brief	Query a coarse lower bound between two hierarchy nodes using the published cache.
* @param	fromNodeId	Stable source hierarchy node id.
* @param	toNodeId	Stable destination hierarchy node id.
* @param	outResult	[out] Lower-bound result payload.
* @return	True when the query executed.
**/
const bool SVG_Nav2_Precompute_QueryPublishedLowerBound( const int32_t fromNodeId,
	const int32_t toNodeId, nav2_precompute_lb_result_t *outResult );

/**
* @brief	Emit bounded precompute summary diagnostics.
* @param	cache	Cache to inspect.
* @param	summary	Optional precomputed summary.
* @param	limit	Maximum number of record lines to print.
**/
void SVG_Nav2_Precompute_DebugPrint( const nav2_precompute_cache_t &cache,
	const nav2_precompute_summary_t *summary, const int32_t limit = 16 );

/**
* @brief	Keep the nav2 precompute module represented in the build.
**/
void SVG_Nav2_Precompute_ModuleAnchor( void );
