/********************************************************************
*
*
*\tServerGame: Nav2 Debug Draw Visualization - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_debug_draw.h"


/**
*	@brief	Render or print a concise debug summary for the currently selected nav2 goal candidates.
*	@param	candidates	Candidate set to report.
*	@param	selected_candidate	Optional selected candidate to highlight.
*	@note	This helper stays allocation-free and only emits compact diagnostics when called explicitly.
**/
void SVG_Nav2_DebugDrawGoalCandidates( const nav2_goal_candidate_list_t &candidates, const nav2_goal_candidate_t *selected_candidate ) {
    // Delegate to the existing candidate printer so the visualization seam stays low-risk and contract-safe.
    SVG_Nav2_DebugPrintGoalCandidates( candidates, selected_candidate );
}

/**
*	@brief	Render or print a concise debug summary for the currently published nav2 corridor commitments.
*	@param	corridor	Corridor to report.
*	@note	This helper stays allocation-free and only emits compact diagnostics when called explicitly.
**/
void SVG_Nav2_DebugDrawCorridor( const nav2_corridor_t &corridor ) {
    // Delegate to the existing corridor printer so the visualization seam stays low-risk and contract-safe.
    SVG_Nav2_DebugPrintCorridor( corridor );
}
