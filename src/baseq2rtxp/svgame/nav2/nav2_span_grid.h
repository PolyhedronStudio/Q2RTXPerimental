/********************************************************************
*
*
*\tServerGame: Nav2 Sparse Span Grid
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include <vector>

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
