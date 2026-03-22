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
*\t@brief\tSparse occupancy decision for local pruning.
**/
enum class nav2_occupancy_decision_t : uint8_t {
    None = 0,
    Free,
    SoftPenalty,
    HardBlock,
    Revalidate,
    Count
};

/**
*\t@brief\tStable occupancy flags describing the state of one localized record.
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
    NAV2_OCCUPANCY_FLAG_DYNAMIC = ( 1u << 11 )
};

/**
*\t@brief\tStable occupancy kind describing what a record represents.
**/
enum class nav2_occupancy_kind_t : uint8_t {
    None = 0,
    Span,
    Edge,
    Portal,
    Mover,
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
*\t@brief\tStable pointer-free occupancy record used for local pruning.
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
    //! Stable inline model index when known.
    int32_t inline_model_index = -1;
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
*\t@brief\tStable sparse occupancy overlay entry keyed by topology-local ids.
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
*\t@brief\tSparse occupancy collection used by nav2.
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
};

/**
*\t@brief\tSparse dynamic overlay collection used by nav2.
**/
struct nav2_dynamic_overlay_t {
    //! Stable overlay entries in deterministic order.
    std::vector<nav2_occupancy_overlay_entry_t> entries = {};
    //! Lookup by overlay id.
    std::unordered_map<int32_t, int32_t> by_overlay_id = {};
};

/**
*\t@brief\tBounded summary for one occupancy query or prune pass.
**/
struct nav2_occupancy_summary_t {
    //! Records accepted as free.
    int32_t free_count = 0;
    //! Records soft-penalized.
    int32_t soft_penalty_count = 0;
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
};


/**
*
*
*	Nav2 Occupancy Public API:
*
*
**/
/**
*\t@brief\tClear a sparse occupancy grid to an empty state.
*\t@param\tgrid\tOccupancy grid to reset.
**/
void SVG_Nav2_OccupancyGrid_Clear( nav2_occupancy_grid_t *grid );

/**
*\t@brief\tClear a dynamic overlay collection to an empty state.
*\t@param\toverlay\tOverlay collection to reset.
**/
void SVG_Nav2_DynamicOverlay_Clear( nav2_dynamic_overlay_t *overlay );

/**
*\t@brief\tAppend or update a sparse occupancy record.
*\t@param\tgrid\tOccupancy grid to mutate.
*\t@param\trecord\tRecord payload to append.
*\t@return\tTrue when the record was stored.
**/
const bool SVG_Nav2_OccupancyGrid_Upsert( nav2_occupancy_grid_t *grid, const nav2_occupancy_record_t &record );

/**
*\t@brief\tAppend or update a dynamic overlay entry.
*\t@param\toverlay\tOverlay collection to mutate.
*\t@param\tentry\tOverlay payload to append.
*\t@return\tTrue when the entry was stored.
**/
const bool SVG_Nav2_DynamicOverlay_Upsert( nav2_dynamic_overlay_t *overlay, const nav2_occupancy_overlay_entry_t &entry );

/**
*\t@brief\tResolve a sparse occupancy record by span id.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\tspan_id	Stable span id to resolve.
*\t@param\tout_record	[out] Resolved occupancy record.
*\t@return\tTrue when a matching record exists.
**/
const bool SVG_Nav2_OccupancyGrid_TryResolveSpan( const nav2_occupancy_grid_t &grid, const int32_t span_id, nav2_occupancy_record_t *out_record );

/**
*\t@brief\tResolve a sparse occupancy record by edge id.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\tedge_id\tStable edge id to resolve.
*\t@param\tout_record	[out] Resolved occupancy record.
*\t@return\tTrue when a matching record exists.
**/
const bool SVG_Nav2_OccupancyGrid_TryResolveEdge( const nav2_occupancy_grid_t &grid, const int32_t edge_id, nav2_occupancy_record_t *out_record );

/**
*\t@brief\tEvaluate a span against the sparse occupancy grid and dynamic overlay.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\toverlay\tOverlay collection to inspect.
*\t@param\tspan\tSpan being considered for pruning.
*\t@param\tmover_ref	Optional mover reference for mover-local pruning.
*\t@param\tout_summary	[out] Optional summary receiving query results.
*\t@return\tTrue when the span remains traversable after local pruning.
**/
const bool SVG_Nav2_EvaluateSpanOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_span_t &span,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary = nullptr );

/**
*\t@brief\tEvaluate a corridor segment against the sparse occupancy grid and dynamic overlay.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\toverlay\tOverlay collection to inspect.
*\t@param\tsegment\tCorridor segment being considered.
*\t@param\tmover_ref	Optional mover reference for mover-local pruning.
*\t@param\tout_summary	[out] Optional summary receiving query results.
*\t@return\tTrue when the segment remains compatible after local pruning.
**/
const bool SVG_Nav2_EvaluateCorridorSegmentOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_corridor_segment_t &segment,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary = nullptr );

/**
*\t@brief\tEvaluate a fine-search node against occupancy and dynamic overlay state.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\toverlay\tOverlay collection to inspect.
*\t@param\tnode\tFine-search node being considered.
*\t@param\tmover_ref	Optional mover reference for mover-local pruning.
*\t@param\tout_summary	[out] Optional summary receiving query results.
*\t@return\tTrue when the node remains compatible after local pruning.
**/
const bool SVG_Nav2_EvaluateFineNodeOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_fine_astar_node_t &node,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary = nullptr );

/**
*\t@brief\tMerge a span-grid occupancy source into the sparse occupancy grid.
*\t@param\tgrid\tOccupancy grid to mutate.
*\t@param\tspan_grid	Span grid source to translate.
*\t@param\tstamp_frame	Frame stamp to record on imported records.
*\t@return\tTrue when at least one record was imported.
**/
const bool SVG_Nav2_ImportSpanGridOccupancy( nav2_occupancy_grid_t *grid, const nav2_span_grid_t &span_grid, const int64_t stamp_frame );

/**
*\t@brief\tMerge a corridor into sparse occupancy and overlay summaries.
*\t@param\tgrid\tOccupancy grid to mutate.
*\t@param\tcoridor	Corridor to import.
*\t@param\tstamp_frame	Frame stamp to record on imported records.
*\t@return\tTrue when at least one record was imported.
**/
const bool SVG_Nav2_ImportCorridorOccupancy( nav2_occupancy_grid_t *grid, const nav2_corridor_t &corridor, const int64_t stamp_frame );

/**
*\t@brief\tEmit a bounded debug summary for occupancy and overlay data.
*\t@param\tgrid\tOccupancy grid to report.
*\t@param\toverlay\tDynamic overlay to report.
*\t@param\tlimit	Maximum number of entries to print.
**/
void SVG_Nav2_DebugPrintOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const int32_t limit = 16 );

/**
*\t@brief\tKeep the nav2 occupancy module represented in the build.
**/
void SVG_Nav2_Occupancy_ModuleAnchor( void );
