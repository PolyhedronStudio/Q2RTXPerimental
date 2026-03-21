/********************************************************************
*
*
*\tServerGame: Nav2 Span Grid Build
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_runtime.h"
#include "svgame/nav2/nav2_span_grid.h"


/**
*	@brief	Build a sparse span-grid placeholder from the currently published nav2 mesh.
*	@param	out_grid	[out] Grid receiving the generated span data.
*	@return	True when a grid could be produced.
*	@note	This is the Milestone 4 build seam; the initial implementation is deliberately conservative and
*			does not reintroduce any public fallback-style behavior.
**/
const bool SVG_Nav2_BuildSpanGrid( nav2_span_grid_t *out_grid );
