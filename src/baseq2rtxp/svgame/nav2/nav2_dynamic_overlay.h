/********************************************************************
*
*
*	ServerGame: Nav2 Dynamic Edge Cost and Availability Overlay
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_fine_astar.h"
#include "svgame/nav2/nav2_occupancy.h"
#include "svgame/nav2/nav2_snapshot.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Dynamic Overlay Enumerations:
*
*
**/
/**
*	@brief	Stable overlay-local modulation decision used to adjust edge availability and traversal cost.
**/
enum class nav2_overlay_modulation_t : uint8_t {
    None = 0,
    Pass,
    Penalize,
    Wait,
    Block,
    Count
};

/**
*	@brief	Stable reason taxonomy for one dynamic edge overlay update.
**/
enum class nav2_overlay_reason_t : uint8_t {
    None = 0,
    DoorClosed,
    DoorOpen,
    MoverUnavailable,
    MoverBoardingUnavailable,
    MoverExitUnavailable,
    NpcCongestion,
    Hazard,
    AreaDisconnected,
    OccupancyConflict,
    Count
};

/**
*	@brief	Stable dynamic-overlay flags describing source semantics and update characteristics.
**/
enum nav2_dynamic_overlay_flag_t : uint32_t {
    NAV2_DYNAMIC_OVERLAY_FLAG_NONE = 0,
    NAV2_DYNAMIC_OVERLAY_FLAG_HAS_CONNECTOR = ( 1u << 0 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_HAS_EDGE = ( 1u << 1 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_HAS_MOVER_REF = ( 1u << 2 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_HAS_TOPOLOGY = ( 1u << 3 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_DOOR = ( 1u << 4 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_MOVER = ( 1u << 5 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_NPC_CONGESTION = ( 1u << 6 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_HAZARD = ( 1u << 7 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_LOCALIZED = ( 1u << 8 ),
    NAV2_DYNAMIC_OVERLAY_FLAG_REQUIRES_REVALIDATION = ( 1u << 9 )
};


/**
*
*
*	Nav2 Dynamic Overlay Data Structures:
*
*
**/
/**
*	@brief	Stable dynamic edge overlay record used to modulate traversal cost and availability.
*	@note	This record is pointer-free so worker snapshots can safely consume published overlay data.
**/
struct nav2_dynamic_edge_overlay_entry_t {
    //! Stable overlay entry id.
    int32_t overlay_id = -1;
    //! Stable connector id when this overlay is connector-backed.
    int32_t connector_id = -1;
    //! Stable span-edge id when known.
    int32_t edge_id = -1;
    //! Stable from-span id when known.
    int32_t from_span_id = -1;
    //! Stable to-span id when known.
    int32_t to_span_id = -1;
    //! Stable mover-local region id when known.
    int32_t mover_region_id = NAV_REGION_ID_NONE;
    //! Stable mover anchor id when known.
    int32_t mover_anchor_id = -1;
    //! Stable mover reference when known.
    nav2_corridor_mover_ref_t mover_ref = {};
    //! Stable topology reference used for localized invalidation.
    nav2_corridor_topology_ref_t topology = {};
    //! Modulation decision applied by this overlay entry.
    nav2_overlay_modulation_t modulation = nav2_overlay_modulation_t::None;
    //! Reason for the modulation decision.
    nav2_overlay_reason_t reason = nav2_overlay_reason_t::None;
    //! Additive traversal penalty for this edge/connector.
    double additive_cost = 0.0;
    //! Estimated wait cost in milliseconds for temporarily unavailable traversal.
    double wait_cost_ms = 0.0;
    //! Stable flags.
    uint32_t flags = NAV2_DYNAMIC_OVERLAY_FLAG_NONE;
    //! Stable frame stamp when this entry was updated.
    int64_t stamp_frame = -1;
    //! Stable version of the dynamic overlay publication this entry belongs to.
    uint32_t overlay_version = 0;
};

/**
*	@brief	Stable dynamic overlay cache and reverse-index channels for localized updates.
**/
struct nav2_dynamic_edge_overlay_cache_t {
    //! Stable overlay entries in deterministic order.
    std::vector<nav2_dynamic_edge_overlay_entry_t> entries = {};
    //! Lookup by stable overlay id.
    std::unordered_map<int32_t, int32_t> by_overlay_id = {};
    //! Lookup by stable connector id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_connector_id = {};
    //! Lookup by stable edge id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_edge_id = {};
    //! Lookup by leaf id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_leaf_id = {};
    //! Lookup by cluster id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_cluster_id = {};
    //! Lookup by area id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_area_id = {};
    //! Lookup by mover-local region id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_mover_region_id = {};
    //! Lookup by mover anchor id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_mover_anchor_id = {};
};

/**
*	@brief	Compact result returned when evaluating one edge/connector against the dynamic overlay cache.
**/
struct nav2_dynamic_overlay_eval_t {
    //! Final modulation decision for the evaluated candidate.
    nav2_overlay_modulation_t modulation = nav2_overlay_modulation_t::Pass;
    //! Aggregated additive cost.
    double additive_cost = 0.0;
    //! Aggregated wait cost in milliseconds.
    double wait_cost_ms = 0.0;
    //! Number of overlay entries that contributed to the decision.
    int32_t contributing_entries = 0;
    //! True when any matching entry requested revalidation.
    bool requires_revalidation = false;
};

/**
*	@brief	Bounded dynamic-overlay rebuild and invalidation diagnostics.
**/
struct nav2_dynamic_overlay_summary_t {
    //! Number of entries published.
    int32_t entry_count = 0;
    //! Number of entries marked as pass-through.
    int32_t pass_count = 0;
    //! Number of entries adding soft penalties.
    int32_t penalize_count = 0;
    //! Number of entries adding wait costs.
    int32_t wait_count = 0;
    //! Number of hard-blocked entries.
    int32_t block_count = 0;
    //! Number of entries derived from doors.
    int32_t door_count = 0;
    //! Number of entries derived from mover availability state.
    int32_t mover_count = 0;
    //! Number of entries derived from NPC congestion.
    int32_t congestion_count = 0;
    //! Number of entries derived from hazards.
    int32_t hazard_count = 0;
    //! Number of localized connector/leaf/cluster/mover-region invalidation touches.
    int32_t localized_invalidation_count = 0;
    //! Number of localized BoxEdicts calls used while gathering dynamic state.
    int32_t localized_entity_query_count = 0;
    //! Number of localized BoxLeafs calls used while gathering topology scope.
    int32_t localized_leaf_query_count = 0;
    //! Number of localized CM_BoxContents calls used while gathering hazard scope.
    int32_t localized_contents_query_count = 0;
    //! Published connector-version after overlay rebuild.
    uint32_t published_connector_version = 0;
};


/**
*
*
*	Nav2 Dynamic Overlay Public API:
*
*
**/
/**
*	@brief	Clear a dynamic edge overlay cache to an empty state.
*	@param	cache	Overlay cache to clear.
**/
void SVG_Nav2_DynamicOverlay_ClearCache( nav2_dynamic_edge_overlay_cache_t *cache );

/**
*	@brief	Append or update one dynamic overlay entry while preserving stable overlay identity.
*	@param	cache	Overlay cache receiving the entry.
*	@param	entry	Overlay entry payload.
*	@return	True when the entry was stored.
**/
const bool SVG_Nav2_DynamicOverlay_UpsertEntry( nav2_dynamic_edge_overlay_cache_t *cache, const nav2_dynamic_edge_overlay_entry_t &entry );

/**
*	@brief	Evaluate one span adjacency edge against the dynamic overlay cache.
*	@param	cache	Overlay cache to inspect.
*	@param	edge	Span adjacency edge to evaluate.
*	@param	mover_ref	Optional mover reference for mover-local filtering.
*	@param	out_eval	[out] Aggregated modulation result.
*	@return	True when evaluation succeeded.
**/
const bool SVG_Nav2_DynamicOverlay_EvaluateSpanEdge( const nav2_dynamic_edge_overlay_cache_t &cache, const nav2_span_edge_t &edge,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_dynamic_overlay_eval_t *out_eval );

/**
*	@brief	Evaluate one corridor segment against the dynamic overlay cache.
*	@param	cache	Overlay cache to inspect.
*	@param	segment	Corridor segment to evaluate.
*	@param	out_eval	[out] Aggregated modulation result.
*	@return	True when evaluation succeeded.
**/
const bool SVG_Nav2_DynamicOverlay_EvaluateCorridorSegment( const nav2_dynamic_edge_overlay_cache_t &cache, const nav2_corridor_segment_t &segment,
    nav2_dynamic_overlay_eval_t *out_eval );

/**
*	@brief	Evaluate one fine-search edge against the dynamic overlay cache.
*	@param	cache	Overlay cache to inspect.
*	@param	edge	Fine-search edge to evaluate.
*	@param	mover_ref	Optional mover reference for mover-local filtering.
*	@param	out_eval	[out] Aggregated modulation result.
*	@return	True when evaluation succeeded.
**/
const bool SVG_Nav2_DynamicOverlay_EvaluateFineEdge( const nav2_dynamic_edge_overlay_cache_t &cache, const nav2_fine_astar_edge_t &edge,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_dynamic_overlay_eval_t *out_eval );

/**
*	@brief	Rebuild dynamic edge overlay entries from occupancy and connector state.
*	@param	cache	Overlay cache to rebuild.
*	@param	connectors	Connector registry used for connector-local overlay identities.
*	@param	occupancy	Sparse occupancy grid used for localized dynamic state.
*	@param	occupancy_overlay	Occupancy overlay entries used for topology-local overlay projection.
*	@param	snapshot_runtime	Optional snapshot runtime used to publish connector overlay version.
*	@param	stamp_frame	Frame stamp recorded on rebuilt entries.
*	@param	out_summary	[out] Optional bounded rebuild summary.
*	@return	True when at least one overlay entry was published.
**/
const bool SVG_Nav2_DynamicOverlay_Rebuild( nav2_dynamic_edge_overlay_cache_t *cache, const nav2_connector_list_t &connectors,
    const nav2_occupancy_grid_t &occupancy, const nav2_dynamic_overlay_t &occupancy_overlay, nav2_snapshot_runtime_t *snapshot_runtime,
    const int64_t stamp_frame, nav2_dynamic_overlay_summary_t *out_summary = nullptr );

/**
*	@brief	Invalidate localized overlay entries by topology scope and update their publication version.
*	@param	cache	Overlay cache to mutate.
*	@param	leaf_id	Optional leaf id scope.
*	@param	cluster_id	Optional cluster id scope.
*	@param	connector_id	Optional connector id scope.
*	@param	mover_region_id	Optional mover-local region id scope.
*	@param	overlay_version	Published overlay version to stamp onto invalidated entries.
*	@param	out_invalidated_count	[out] Optional count of entries touched by invalidation.
*	@return	True when one or more entries were invalidated.
**/
const bool SVG_Nav2_DynamicOverlay_InvalidateLocalized( nav2_dynamic_edge_overlay_cache_t *cache,
    const int32_t leaf_id, const int32_t cluster_id, const int32_t connector_id, const int32_t mover_region_id,
    const uint32_t overlay_version, int32_t *out_invalidated_count = nullptr );

/**
*	@brief	Emit bounded debug diagnostics for dynamic edge overlay state.
*	@param	cache	Overlay cache to report.
*	@param	limit	Maximum number of entries to print.
**/
void SVG_Nav2_DebugPrintDynamicOverlay( const nav2_dynamic_edge_overlay_cache_t &cache, const int32_t limit = 16 );

/**
*	@brief	Keep the nav2 dynamic-overlay module represented in the build.
**/
void SVG_Nav2_DynamicOverlay_ModuleAnchor( void );
