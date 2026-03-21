/********************************************************************
*
*
*\tServerGame: Nav2 Span Adjacency
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_span_grid.h"

#include <vector>


/**
*	@brief	One adjacency edge between two span-grid spans.
**/
struct nav2_span_edge_t {
    //! Stable edge id within the owning builder output.
    int32_t edge_id = -1;
    //! Source span id.
    int32_t from_span_id = -1;
    //! Destination span id.
    int32_t to_span_id = -1;
    //! Edge cost used by later fine search.
    double cost = 0.0;
    //! Hard/soft legality flags.
    uint32_t flags = 0;
};

/**
*	@brief	Adjacency result for one span grid.
**/
struct nav2_span_adjacency_t {
    //! Generated edges in deterministic order.
    std::vector<nav2_span_edge_t> edges = {};
};

/**
*	@brief	Build local span adjacency from a sparse span grid.
*	@param	grid	Sparse span grid to analyze.
*	@param	out_adjacency	[out] Adjacency output.
*	@return	True when adjacency was built successfully.
**/
const bool SVG_Nav2_BuildSpanAdjacency( const nav2_span_grid_t &grid, nav2_span_adjacency_t *out_adjacency );
