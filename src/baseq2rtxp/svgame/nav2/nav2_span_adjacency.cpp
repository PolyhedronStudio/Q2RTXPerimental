/********************************************************************
*
*
*\tServerGame: Nav2 Span Adjacency - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_span_adjacency.h"


/**
*	@brief	Build local span adjacency from a sparse span grid.
*	@param	grid	Sparse span grid to analyze.
*	@param	out_adjacency	[out] Adjacency output.
*	@return	True when adjacency was built successfully.
*	@note	The first Milestone 4 pass is conservative and only establishes the container and entry point.
**/
const bool SVG_Nav2_BuildSpanAdjacency( const nav2_span_grid_t &grid, nav2_span_adjacency_t *out_adjacency ) {
    // Sanity check: require output storage.
    if ( !out_adjacency ) {
        return false;
    }

    // Reset adjacency so future work can append validated edges without stale state.
    *out_adjacency = {};
    (void)grid;
    return true;
}
