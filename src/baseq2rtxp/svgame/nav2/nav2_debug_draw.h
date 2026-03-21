/********************************************************************
*
*
*\tServerGame: Nav2 Debug Draw Visualization
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_goal_candidates.h"


/**
*	@brief	Render or print a concise debug summary for the currently selected nav2 goal candidates.
*	@param	candidates	Candidate set to report.
*	@param	selected_candidate	Optional selected candidate to highlight.
*	@note	This helper is intentionally debug-only and must remain contract-safe with the strict public nav2 path.
**/
void SVG_Nav2_DebugDrawGoalCandidates( const nav2_goal_candidate_list_t &candidates, const nav2_goal_candidate_t *selected_candidate = nullptr );

/**
*	@brief	Render or print a concise debug summary for the currently published nav2 corridor commitments.
*	@param	corridor	Corridor to report.
*	@note	This helper is intentionally debug-only and must remain contract-safe with the strict public nav2 path.
**/
void SVG_Nav2_DebugDrawCorridor( const nav2_corridor_t &corridor );
