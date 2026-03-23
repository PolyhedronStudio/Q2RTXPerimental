/********************************************************************
*
*
*	ServerGame: Nav2 Sparse Occupancy and Dynamic Overlay Pruning
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_fine_astar.h"
#include "svgame/nav2/nav2_span_grid.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Occupancy Enumerations:
*
*
**/
/**
* @brief Sparse occupancy decision for local pruning.
**/
enum class nav2_occupancy_decision_t : uint8_t {
    None = 0,
    Free,
    SoftPenalty,
    RequiresInteraction,
    TemporarilyUnavailable,
    HardBlock,
    Blocked = HardBlock,
    Revalidate,
    Count
};

/**
* @brief Stable occupancy flags describing the state of one localized record.
**/
enum nav2_occupancy_flag_t : uint32_t {
    NAV2_OCCUPANCY_FLAG_NONE = 0,
    NAV2_OCCUPANCY_FLAG_HAS_SPAN = ( 1u << 0 ),
    NAV2_OCCUPANCY_FLAG_HAS_EDGE = ( 1u << 1 ),
    NAV2_OCCUPANCY_FLAG_HAS_MOVER = ( 1u << 2 ),
    NAV2_OCCUPANCY_FLAG_HAS_PORTAL = ( 1u << 3 ),
    NAV2_OCCUPANCY_FLAG_HAS_LEAF = ( 1u << 4 ),
    NAV2_OCCUPANCY_FLAG_HAS_CLUSTER = ( 1u << 5 ),
    NAV2_OCCUPANCY_FLAG_HAS_AREA = ( 1u << 6 ),
    NAV2_OCCUPANCY_FLAG_SOFT_PENALIZED = ( 1u << 7 ),
    NAV2_OCCUPANCY_FLAG_HARD_BLOCKED = ( 1u << 8 ),
    NAV2_OCCUPANCY_FLAG_REQUIRES_REVALIDATION = ( 1u << 9 ),
    NAV2_OCCUPANCY_FLAG_PORTAL_OVERLAY = ( 1u << 10 ),
    NAV2_OCCUPANCY_FLAG_DYNAMIC = ( 1u << 11 ),
    NAV2_OCCUPANCY_FLAG_REQUIRES_INTERACTION = ( 1u << 12 ),
    NAV2_OCCUPANCY_FLAG_TEMPORARILY_UNAVAILABLE = ( 1u << 13 ),
    NAV2_OCCUPANCY_FLAG_NPC_BLOCKER = ( 1u << 14 ),
    NAV2_OCCUPANCY_FLAG_DOOR = ( 1u << 15 ),
    NAV2_OCCUPANCY_FLAG_PLAT = ( 1u << 16 ),
    NAV2_OCCUPANCY_FLAG_HAZARD = ( 1u << 17 ),
    NAV2_OCCUPANCY_FLAG_MOVER_ANCHOR = ( 1u << 18 )
};

/**
* @brief Stable occupancy kind describing what a record represents.
**/
enum class nav2_occupancy_kind_t : uint8_t {
    None = 0,
    Span,
    Edge,
    Portal,
    Mover,
    Entity,
    Hazard,
    Area,
    Leaf,
    Cluster,
    Count
};


/**
*
*
*	Nav2 Occupancy Data Structures:
*
*
**/
/**
* @brief Stable pointer-free occupancy record used for local pruning.
**/
struct nav2_occupancy_record_t {
    //! Stable record id.
    int32_t occupancy_id = -1;
    //! Occupancy kind.
    nav2_occupancy_kind_t kind = nav2_occupancy_kind_t::None;
    //! Stable span id when the record is span-backed.
    int32_t span_id = -1;
    //! Stable edge id when the record is edge-backed.
    int32_t edge_id = -1;
    //! Stable connector id when the record is connector-backed.
    int32_t connector_id = -1;
    //! Stable mover entity number when known.
    int32_t mover_entnum = -1;
    //! Stable mover-local anchor id when known.
    int32_t mover_anchor_id = -1;
    //! Stable mover-local region id when known.
    int32_t mover_region_id = NAV_REGION_ID_NONE;
    //! Stable inline model index when known.
    int32_t inline_model_index = -1;
    //! Stable entity type when known.
    int32_t entity_type = 0;
    //! Stable solid classification when known.
    int32_t solid_type = 0;
    //! Stable movetype classification when known.
    int32_t move_type = MOVETYPE_NONE;
    //! Stable role flags when known.
    uint32_t role_flags = 0;
    //! Stable leaf id when known.
    int32_t leaf_id = -1;
    //! Stable cluster id when known.
    int32_t cluster_id = -1;
    //! Stable area id when known.
    int32_t area_id = -1;
    //! Occupancy decision.
    nav2_occupancy_decision_t decision = nav2_occupancy_decision_t::None;
    //! Base soft penalty contributed by this occupancy record.
    double soft_penalty = 0.0;
    //! Allowed Z band for this occupancy record.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Stable flags.
    uint32_t flags = NAV2_OCCUPANCY_FLAG_NONE;
    //! Update stamp.
    int64_t stamp_frame = -1;
};

/**
* @brief Stable sparse occupancy overlay entry keyed by topology-local ids.
**/
struct nav2_occupancy_overlay_entry_t {
    //! Stable overlay id.
    int32_t overlay_id = -1;
    //! Stable topology reference attached to the overlay entry.
    nav2_corridor_topology_ref_t topology = {};
    //! Stable mover reference attached to the overlay entry.
    nav2_corridor_mover_ref_t mover_ref = {};
    //! Occupancy decision published by the overlay.
    nav2_occupancy_decision_t decision = nav2_occupancy_decision_t::None;
    //! Soft penalty published by the overlay.
    double soft_penalty = 0.0;
    //! Stable flags.
    uint32_t flags = NAV2_OCCUPANCY_FLAG_NONE;
    //! Update stamp.
    int64_t stamp_frame = -1;
};

/**
* @brief Sparse occupancy collection used by nav2.
**/
struct nav2_occupancy_grid_t {
    //! Stable records in deterministic order.
    std::vector<nav2_occupancy_record_t> records = {};
    //! Lookup by span id.
    std::unordered_map<int32_t, int32_t> by_span_id = {};
    //! Lookup by edge id.
    std::unordered_map<int32_t, int32_t> by_edge_id = {};
    //! Lookup by connector id.
    std::unordered_map<int32_t, int32_t> by_connector_id = {};
    //! Lookup by mover entnum.
    std::unordered_map<int32_t, int32_t> by_mover_entnum = {};
    //! Lookup by mover anchor id.
    std::unordered_map<int32_t, int32_t> by_mover_anchor_id = {};
    //! Reverse memberships by leaf id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_leaf_id = {};
    //! Reverse memberships by cluster id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_cluster_id = {};
    //! Reverse memberships by area id.
    std::unordered_map<int32_t, std::vector<int32_t>> by_area_id = {};
};

/**
* @brief Sparse dynamic overlay collection used by nav2.
**/
struct nav2_dynamic_overlay_t {
    //! Stable overlay entries in deterministic order.
    std::vector<nav2_occupancy_overlay_entry_t> entries = {};
    //! Lookup by overlay id.
    std::unordered_map<int32_t, int32_t> by_overlay_id = {};
};

/**
* @brief Bounded summary for one occupancy query or prune pass.
**/
struct nav2_occupancy_summary_t {
    //! Records accepted as free.
    int32_t free_count = 0;
    //! Records soft-penalized.
    int32_t soft_penalty_count = 0;
    //! Records that require interaction.
    int32_t requires_interaction_count = 0;
    //! Records that are temporarily unavailable.
    int32_t temporarily_unavailable_count = 0;
    //! Records hard-blocked.
    int32_t hard_block_count = 0;
    //! Records requiring revalidation.
    int32_t revalidate_count = 0;
    //! Overlay entries processed.
    int32_t overlay_count = 0;
    //! Span records processed.
    int32_t span_count = 0;
    //! Edge records processed.
    int32_t edge_count = 0;
    //! Mover records processed.
    int32_t mover_count = 0;
    //! Entity records processed.
    int32_t entity_count = 0;
    //! Hazard records processed.
    int32_t hazard_count = 0;
    //! Records gathered through localized BoxEdicts sampling.
    int32_t localized_entity_samples = 0;
    //! Dynamic occupancy version published after rebuild.
    uint32_t published_occupancy_version = 0;
};


struct nav2_inline_bsp_entity_list_t;
struct nav2_snapshot_runtime_t;


/**
*
*
*	Nav2 Occupancy Public API:
*
*
**/
/**
* @brief Clear a sparse occupancy grid to an empty state.
* @param grid Occupancy grid to reset.
**/
void SVG_Nav2_OccupancyGrid_Clear( nav2_occupancy_grid_t *grid );

/**
* @brief Clear a dynamic overlay collection to an empty state.
* @param overlay Overlay collection to reset.
**/
void SVG_Nav2_DynamicOverlay_Clear( nav2_dynamic_overlay_t *overlay );

/**
* @brief Append or update a sparse occupancy record.
* @param grid Occupancy grid to mutate.
* @param record Record payload to append.
* @return True when the record was stored.
**/
const bool SVG_Nav2_OccupancyGrid_Upsert( nav2_occupancy_grid_t *grid, const nav2_occupancy_record_t &record );

/**
* @brief Append or update a dynamic overlay entry.
* @param overlay Overlay collection to mutate.
* @param entry Overlay payload to append.
* @return True when the entry was stored.
**/
const bool SVG_Nav2_DynamicOverlay_Upsert( nav2_dynamic_overlay_t *overlay, const nav2_occupancy_overlay_entry_t &entry );

/**
* @brief Resolve a sparse occupancy record by span id.
* @param grid Occupancy grid to inspect.
* @param span_id	Stable span id to resolve.
* @param out_record	[out] Resolved occupancy record.
* @return True when a matching record exists.
**/
const bool SVG_Nav2_OccupancyGrid_TryResolveSpan( const nav2_occupancy_grid_t &grid, const int32_t span_id, nav2_occupancy_record_t *out_record );

/**
* @brief Resolve a sparse occupancy record by edge id.
* @param grid Occupancy grid to inspect.
* @param edge_id Stable edge id to resolve.
* @param out_record	[out] Resolved occupancy record.
* @return True when a matching record exists.
**/
const bool SVG_Nav2_OccupancyGrid_TryResolveEdge( const nav2_occupancy_grid_t &grid, const int32_t edge_id, nav2_occupancy_record_t *out_record );

/**
 * @brief Resolve a sparse occupancy record by connector id.
 * @param grid Occupancy grid to inspect.
 * @param connector_id Stable connector id to resolve.
 * @param out_record [out] Resolved occupancy record.
 * @return True when a matching record exists.
 **/
const bool SVG_Nav2_OccupancyGrid_TryResolveConnector( const nav2_occupancy_grid_t &grid, const int32_t connector_id, nav2_occupancy_record_t *out_record );

/**
 * @brief Resolve a sparse occupancy record by mover anchor id.
 * @param grid Occupancy grid to inspect.
 * @param mover_anchor_id Stable mover anchor id to resolve.
 * @param out_record [out] Resolved occupancy record.
 * @return True when a matching record exists.
 **/
const bool SVG_Nav2_OccupancyGrid_TryResolveMoverAnchor( const nav2_occupancy_grid_t &grid, const int32_t mover_anchor_id, nav2_occupancy_record_t *out_record );

/**
* @brief Evaluate a span against the sparse occupancy grid and dynamic overlay.
* @param grid Occupancy grid to inspect.
* @param overlay Overlay collection to inspect.
* @param span Span being considered for pruning.
* @param mover_ref	Optional mover reference for mover-local pruning.
* @param out_summary	[out] Optional summary receiving query results.
* @return True when the span remains traversable after local pruning.
**/
const bool SVG_Nav2_EvaluateSpanOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_span_t &span,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary = nullptr );

/**
* @brief Evaluate a corridor segment against the sparse occupancy grid and dynamic overlay.
* @param grid Occupancy grid to inspect.
* @param overlay Overlay collection to inspect.
* @param segment Corridor segment being considered.
* @param mover_ref	Optional mover reference for mover-local pruning.
* @param out_summary	[out] Optional summary receiving query results.
* @return True when the segment remains compatible after local pruning.
**/
const bool SVG_Nav2_EvaluateCorridorSegmentOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_corridor_segment_t &segment,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary = nullptr );

/**
* @brief Evaluate a fine-search node against occupancy and dynamic overlay state.
* @param grid Occupancy grid to inspect.
* @param overlay Overlay collection to inspect.
* @param node Fine-search node being considered.
* @param mover_ref	Optional mover reference for mover-local pruning.
* @param out_summary	[out] Optional summary receiving query results.
* @return True when the node remains compatible after local pruning.
**/
const bool SVG_Nav2_EvaluateFineNodeOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_fine_astar_node_t &node,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary = nullptr );

/**
* @brief Merge a span-grid occupancy source into the sparse occupancy grid.
* @param grid Occupancy grid to mutate.
* @param span_grid	Span grid source to translate.
* @param stamp_frame	Frame stamp to record on imported records.
* @return True when at least one record was imported.
**/
const bool SVG_Nav2_ImportSpanGridOccupancy( nav2_occupancy_grid_t *grid, const nav2_span_grid_t &span_grid, const int64_t stamp_frame );

/**
* @brief Merge a corridor into sparse occupancy and overlay summaries.
* @param grid Occupancy grid to mutate.
* @param coridor	Corridor to import.
* @param stamp_frame	Frame stamp to record on imported records.
* @return True when at least one record was imported.
**/
const bool SVG_Nav2_ImportCorridorOccupancy( nav2_occupancy_grid_t *grid, const nav2_corridor_t &corridor, const int64_t stamp_frame );

/**
 * @brief Rebuild sparse occupancy and overlay data from span-grid and inline-entity metadata.
 * @param grid Occupancy grid to rebuild.
 * @param overlay Dynamic overlay to rebuild.
 * @param span_grid Span-grid source used for localized occupancy seeding.
 * @param entities Optional inline BSP entity semantics to import.
 * @param snapshot_runtime Optional snapshot runtime to bump occupancy publication version.
 * @param stamp_frame Frame stamp to record on imported records.
 * @param out_summary [out] Optional bounded summary for diagnostics.
 * @return True when at least one occupancy or overlay record was imported.
 **/
const bool SVG_Nav2_RebuildDynamicOccupancy( nav2_occupancy_grid_t *grid, nav2_dynamic_overlay_t *overlay, const nav2_span_grid_t &span_grid,
    const nav2_inline_bsp_entity_list_t *entities, nav2_snapshot_runtime_t *snapshot_runtime, const int64_t stamp_frame, nav2_occupancy_summary_t *out_summary = nullptr );

/**
* @brief Emit a bounded debug summary for occupancy and overlay data.
* @param grid Occupancy grid to report.
* @param overlay Dynamic overlay to report.
* @param limit	Maximum number of entries to print.
**/
void SVG_Nav2_DebugPrintOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const int32_t limit = 16 );

/**
* @brief Keep the nav2 occupancy module represented in the build.
**/
void SVG_Nav2_Occupancy_ModuleAnchor( void );
