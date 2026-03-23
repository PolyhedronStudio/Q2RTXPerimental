/********************************************************************
*
*
*	ServerGame: Nav2 Corridor Extraction
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_coarse_astar.h"
#include "svgame/nav2/nav2_corridor.h"


/**
*	@brief	Build a nav2 corridor from a solved coarse A* state.
*	@param	coarse_state	Solved or partially solved coarse search state.
*	@param	out_corridor	[out] Corridor receiving the committed topology and policy data.
*	@return	True when a corridor was produced.
*	@note	This adapter translates coarse node and edge commitments into the explicit nav2 corridor model.
**/
const bool SVG_Nav2_BuildCorridorFromCoarseAStar( const nav2_coarse_astar_state_t &coarse_state, nav2_corridor_t *out_corridor );

/**
*	@brief	Emit a bounded debug summary for a corridor extracted from a coarse A* state.
*	@param	coarse_state	Coarse A* state supplying the reconstructed path.
*	@param	max_segments	Maximum number of explicit corridor segments to emit.
*	@return	True when a corridor was extracted and reported.
*	@note	This uses the nav2 corridor extraction output so diagnostics stay aligned with Task 8.2 commitments.
**/
const bool SVG_Nav2_DebugPrintCorridorFromCoarseAStar( const nav2_coarse_astar_state_t &coarse_state, const int32_t max_segments = 8 );

/**
*	@brief	Keep the nav2 corridor-extraction helper module represented in the build.
**/
void SVG_Nav2_CorridorBuild_ModuleAnchor( void );
