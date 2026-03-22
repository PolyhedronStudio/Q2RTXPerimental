/********************************************************************
*
*
* ServerGame: Nav2 Sparse Span Grid
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include <vector>
#include <unordered_map>

#include "svgame/nav2/nav2_types.h"


/**
*	@brief	A single traversable vertical span inside a sparse XY column.
*	@note	The span stores compact topology and movement metadata so the precision layer remains grid-like,
*			non-triangulated, and friendly to future serialization.
**/
struct nav2_span_t {
    //! Stable span id within the owning column or grid.
    int32_t span_id = -1;
    //! Minimum traversable Z for the span.
    double floor_z = 0.0;
    //! Maximum traversable Z for the span.
    double ceiling_z = 0.0;
    //! Preferred standing Z inside the span.
    double preferred_z = 0.0;
    //! BSP leaf id backing the span when known.
    int32_t leaf_id = -1;
    //! BSP cluster id backing the span when known.
    int32_t cluster_id = -1;
    //! BSP area id backing the span when known.
    int32_t area_id = -1;
    //! Region-layer id backing the span when known.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Compact movement flags mirrored from analysis or generation.
    uint32_t movement_flags = NAV_FLAG_WALKABLE;
    //! Compact surface or hazard flags mirrored from analysis or generation.
    uint32_t surface_flags = NAV_TILE_SUMMARY_NONE;
    //! Compact topology flags that influence corridor and refinement decisions.
    uint32_t topology_flags = 0;
    //! Stable connector hint mask used by later hierarchy/corridor stages.
    uint32_t connector_hint_mask = 0;
    //! Optional occupancy reference for dynamic overlays.
    nav2_occupancy_entry_t occupancy = {};
};

/**
*	@brief	A sparse XY column containing zero or more traversable spans.
**/
struct nav2_span_column_t {
    //! World grid X coordinate.
    int32_t tile_x = 0;
    //! World grid Y coordinate.
    int32_t tile_y = 0;
    //! Canonical world-tile id when known.
    int32_t tile_id = -1;
    //! Ordered spans for this XY column.
    std::vector<nav2_span_t> spans = {};
};

/**
*	@brief	Stable pointer-free span reference used by reverse indices and diagnostics.
*	@note	This keeps reverse-index lookup persistence-friendly and safe for worker publication.
**/
struct nav2_span_ref_t {
    //! Stable span id mirrored from the owning span record.
    int32_t span_id = -1;
    //! Owning sparse-column index in `nav2_span_grid_t::columns`.
    int32_t column_index = -1;
    //! Span index inside the owning sparse column.
    int32_t span_index = -1;

    /**
    *	@brief	Return whether this span reference points at a concrete span slot.
    *	@return	True when column, span, and id metadata are all non-negative.
    **/
    const bool IsValid() const {
        return span_id >= 0 && column_index >= 0 && span_index >= 0;
    }
};

/**
*	@brief	Reverse-index storage for sparse span-grid topology lookups.
*	@note	Task 4.1 uses compact vectors keyed by leaf/cluster/area where practical and a stable span-id map for direct diagnostics.
**/
struct nav2_span_grid_reverse_indices_t {
    //! Stable span-id lookup table.
    std::unordered_map<int32_t, nav2_span_ref_t> by_span_id = {};
    //! Leaf-indexed span memberships.
    std::vector<std::vector<nav2_span_ref_t>> by_leaf = {};
    //! Cluster-indexed span memberships.
    std::vector<std::vector<nav2_span_ref_t>> by_cluster = {};
    //! Area-indexed span memberships.
    std::vector<std::vector<nav2_span_ref_t>> by_area = {};
    //! Connector-hint indexed span memberships.
    std::vector<std::vector<nav2_span_ref_t>> by_connector = {};
};

/**
*	@brief	The sparse precision-layer span grid used by nav2.
**/
struct nav2_span_grid_t {
    //! World tile size used by the grid.
    int32_t tile_size = 0;
    //! World-space XY size of each cell.
    double cell_size_xy = 0.0;
    //! Quantization used for vertical layer storage.
    double z_quant = 0.0;
    //! Sparse columns keyed in canonical XY order.
    std::vector<nav2_span_column_t> columns = {};
    //! Reverse indices rebuilt from sparse columns for topology-local lookups.
    nav2_span_grid_reverse_indices_t reverse_indices = {};
};


/**
*	@brief	Return whether the span contains a usable height range.
**/
inline const bool SVG_Nav2_SpanIsValid( const nav2_span_t &span ) {
    return span.ceiling_z >= span.floor_z;
}

/**
*	@brief	Return whether the span grid contains any columns.
**/
inline const bool SVG_Nav2_SpanGridHasColumns( const nav2_span_grid_t &grid ) {
    return !grid.columns.empty();
}

/**
* @brief Return whether the span grid has no sparse columns.
* @param grid Grid to inspect.
* @return True when the grid contains no sparse columns.
**/
inline const bool SVG_Nav2_SpanGridIsEmpty( const nav2_span_grid_t &grid ) {
	return grid.columns.empty();
}

/**
* @brief Find the sparse column index for a tile coordinate pair.
* @param grid Grid to inspect.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @return Zero-based column index when found, or `-1` otherwise.
**/
int32_t SVG_Nav2_SpanGrid_FindColumnIndex( const nav2_span_grid_t &grid, const int32_t tile_x, const int32_t tile_y );

/**
* @brief Resolve a sparse column for a tile coordinate pair.
* @param grid Grid to inspect.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @return Pointer to the sparse column, or `nullptr` when absent.
**/
const nav2_span_column_t *SVG_Nav2_SpanGrid_TryGetColumn( const nav2_span_grid_t &grid, const int32_t tile_x, const int32_t tile_y );

/**
* @brief Resolve a mutable sparse column for a tile coordinate pair.
* @param grid Grid to inspect.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @return Pointer to the sparse column, or `nullptr` when absent.
**/
nav2_span_column_t *SVG_Nav2_SpanGrid_TryGetColumn( nav2_span_grid_t *grid, const int32_t tile_x, const int32_t tile_y );

/**
* @brief Find or create a sparse column for a tile coordinate pair.
* @param grid Grid to mutate.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @param tile_id Canonical tile id to assign when creating a new column.
* @return Pointer to the existing or newly created sparse column, or `nullptr` on invalid input.
**/
nav2_span_column_t *SVG_Nav2_SpanGrid_FindOrAddColumn( nav2_span_grid_t *grid, const int32_t tile_x, const int32_t tile_y, const int32_t tile_id = -1 );

/**
* @brief Append a span to a sparse column.
* @param column Sparse column receiving the span.
* @param span Span payload to append.
* @return Pointer to the appended span slot, or `nullptr` when no column was supplied.
* @note The caller is responsible for assigning a stable span id before rebuilds or serialization.
**/
nav2_span_t *SVG_Nav2_SpanGrid_AppendSpan( nav2_span_column_t *column, const nav2_span_t &span );

/**
* @brief Resolve one span by pointer-free span reference.
* @param grid Grid to inspect.
* @param ref Pointer-free span reference to resolve.
* @return Pointer to the span slot, or `nullptr` when the reference is invalid.
**/
const nav2_span_t *SVG_Nav2_SpanGrid_TryResolveSpan( const nav2_span_grid_t &grid, const nav2_span_ref_t &ref );

/**
* @brief Resolve one mutable span by pointer-free span reference.
* @param grid Grid to inspect.
* @param ref Pointer-free span reference to resolve.
* @return Pointer to the span slot, or `nullptr` when the reference is invalid.
**/
nav2_span_t *SVG_Nav2_SpanGrid_TryResolveSpan( nav2_span_grid_t *grid, const nav2_span_ref_t &ref );

/**
* @brief Sort sparse columns and spans into a canonical deterministic order.
* @param grid Grid to mutate.
* @return True when sorting completed.
* @note This keeps worker-published snapshots and serialization output stable.
**/
const bool SVG_Nav2_SpanGrid_SortCanonical( nav2_span_grid_t *grid );

/**
*	@brief	Reset one span-grid instance to a clean empty state.
*	@param	grid	[in,out] Grid to clear.
**/
void SVG_Nav2_SpanGrid_Clear( nav2_span_grid_t *grid );

/**
*	@brief	Rebuild pointer-free reverse indices from the current sparse-column payload.
*	@param	grid	[in,out] Grid whose reverse indices should be regenerated.
*	@return	True when rebuild completed.
**/
const bool SVG_Nav2_SpanGrid_RebuildReverseIndices( nav2_span_grid_t *grid );

/**
*	@brief	Resolve one span reference by stable span id.
*	@param	grid	Grid to inspect.
*	@param	span_id	Stable span id to resolve.
*	@param	out_ref	[out] Resolved pointer-free span reference.
*	@return	True when the span id exists inside the grid reverse index.
**/
const bool SVG_Nav2_SpanGrid_TryResolveSpanRef( const nav2_span_grid_t &grid, const int32_t span_id, nav2_span_ref_t *out_ref );
