/********************************************************************
*
*
* ServerGame: Nav2 Debug Draw Visualization
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_goal_candidates.h"
#include "svgame/nav2/nav2_query_job.h"
#include "svgame/nav2/nav2_snapshot.h"
#include "svgame/nav2/nav2_span_grid_build.h"


/**
*
*
*	Nav2 Debug Draw Enumerations:
*
*
**/
/**
*	@brief	Compact debug verbosity level for nav2 corridor visualization.
*	@note	This keeps Task 3.3 debug output bounded so callers can request summary-only or segment-level diagnostics without reintroducing log spam.
**/
enum class nav2_debug_corridor_verbosity_t : uint8_t {
    SummaryOnly = 0,
    IncludeSegments,
    Count
};


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
*	@param	verbosity	Requested debug verbosity for corridor output.
*	@param	max_segments	Maximum number of explicit segments to emit when segment output is enabled.
*	@note	This helper is intentionally debug-only and must remain contract-safe with the strict public nav2 path.
**/
void SVG_Nav2_DebugDrawCorridor( const nav2_corridor_t &corridor,
    const nav2_debug_corridor_verbosity_t verbosity = nav2_debug_corridor_verbosity_t::SummaryOnly,
    const int32_t max_segments = 4 );

/**
*    @brief    Render explicit corridor anchor markers for visual debugging.
*    @param    corridor        Corridor whose anchor commitments to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     This overlay uses the explicit segment anchors when they exist, so it stays safe for diagnostics.
**/
void SVG_Nav2_DebugDrawCorridorAnchors( const nav2_corridor_t &corridor, const multicast_t multicast_type = MULTICAST_ALL );

/**
*    @brief    Render corridor segment overlays using debug trails between anchors.
*    @param    corridor        Corridor whose segments to trace.
*    @param    limit           Maximum number of segments to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     The caller controls `limit` through the verbosity policy so the overlay remains bounded.
**/
void SVG_Nav2_DebugDrawCorridorSegmentOverlay( const nav2_corridor_t &corridor, const int32_t limit,
    const multicast_t multicast_type = MULTICAST_ALL );

/**
*	@brief	Emit a bounded debug summary for one nav2 query job and its snapshot state.
*	@param	job Query job to report.
*	@param	latest_snapshot Latest published snapshot state to compare against.
*	@note	This stays allocation-free and is intended for opt-in diagnostics only.
**/
void SVG_Nav2_DebugDrawQueryJob( const nav2_query_job_t &job, const nav2_snapshot_runtime_t &latest_snapshot );

/**
*    @brief    Draw the scheduler request overlay for a nav2 job.
*    @param    job             Job whose request span to render.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     This uses the request start/goal to highlight the desired travel corridor.
**/
void SVG_Nav2_DebugDrawJobSchedulerOverlay( const nav2_query_job_t &job, const multicast_t multicast_type = MULTICAST_ALL );

/**
*    @brief    Visualize snapshot staleness or serialization requirements for a query job.
*    @param    job             Job whose snapshot diagnostics to highlight.
*    @param    staleness       Snapshot comparison result used to determine overlay state.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     Each staleness flag draws a short indicator so developers can see why the job may restart or resubmit.
**/
void SVG_Nav2_DebugDrawQuerySnapshotOverlay( const nav2_query_job_t &job, const nav2_snapshot_staleness_t &staleness,
    const multicast_t multicast_type = MULTICAST_ALL );

/**
*    @brief    Render bounded sparse span-grid overlays and summary diagnostics.
*    @param    grid             Span grid to visualize.
*    @param    limit_columns    Maximum number of sparse columns to draw.
*    @param    multicast_type   Temp-entity multicast channel used for the overlay.
*    @note     This helper is opt-in and bounded so Task 4.2 validation can inspect rasterized outputs without log spam.
**/
void SVG_Nav2_DebugDrawSpanGrid( const nav2_span_grid_t &grid, const int32_t limit_columns = 64,
    const multicast_t multicast_type = MULTICAST_ALL );
