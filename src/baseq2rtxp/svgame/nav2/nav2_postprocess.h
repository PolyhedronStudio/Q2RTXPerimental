/********************************************************************
*
*
*	ServerGame: Nav2 Path Postprocess and LOS Shortcuts
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_fine_astar.h"
#include "svgame/nav2/nav2_query_iface.h"


/**
*
*
*	Nav2 Postprocess Data Structures:
*
*
**/
/**
* @brief	Policy controls for bounded nav2 path postprocessing.
* @note	These controls are intentionally compact and pointer-free so caller state remains
* 		serialization-friendly and scheduler-friendly.
**/
struct nav2_postprocess_policy_t {
    //! Maximum number of waypoints considered by this postprocess call.
    int32_t max_waypoints = 256;
    //! If true, attempt line-of-sight waypoint reduction.
    bool enable_los_shortcuts = true;
    //! If true, keep endpoint waypoints even when LOS reduction could remove them.
    bool preserve_endpoints = true;
    //! If true, reject LOS shortcuts that violate conservative corridor Z-bands.
    bool enforce_corridor_z_bands = true;
    //! If true, reject LOS shortcuts crossing hard-invalid edge semantics.
    bool reject_hard_invalid_edges = true;
    //! If true, reject LOS shortcuts that cut through mover-anchor nodes.
    bool preserve_mover_anchor_nodes = true;
    //! If true, reject LOS shortcuts passing through hazardous contents.
    bool reject_hazardous_contents = true;
    //! Additional world-units radius used for conservative LOS collision checks.
    double los_clearance_radius = 12.0;
};

/**
* @brief	Bounded diagnostics emitted by one nav2 postprocess invocation.
**/
struct nav2_postprocess_diagnostics_t {
    //! Input waypoint count before smoothing.
    uint32_t input_waypoints = 0;
    //! Output waypoint count after smoothing.
    uint32_t output_waypoints = 0;
    //! Number of LOS shortcut attempts.
    uint32_t los_attempts = 0;
    //! Number of LOS shortcuts accepted.
    uint32_t los_accepts = 0;
    //! Number of LOS shortcuts rejected by trace collisions.
    uint32_t trace_rejects = 0;
    //! Number of LOS shortcuts rejected by content checks.
    uint32_t contents_rejects = 0;
    //! Number of LOS shortcuts rejected by corridor or mover constraints.
    uint32_t semantics_rejects = 0;
};

/**
* @brief	Stable nav2 postprocess output path and diagnostics.
**/
struct nav2_postprocess_result_t {
    //! True when postprocess executed successfully.
    bool success = false;
    //! Smoothed world-space path points in order.
    std::vector<Vector3> waypoints = {};
    //! Bounded diagnostics from postprocess execution.
    nav2_postprocess_diagnostics_t diagnostics = {};
};


/**
*
*
*	Nav2 Postprocess Public API:
*
*
**/
/**
* @brief	Build world-space waypoint candidates from a fine-search path state.
* @param	fine_state	Fine-search state supplying stable node ids and metadata.
* @param	max_points	Maximum points to emit.
* @param	out_points	[out] World-space point list built from fine-search nodes.
* @return	True when waypoint extraction succeeded.
**/
const bool SVG_Nav2_Postprocess_ExtractFineWaypoints( const nav2_fine_astar_state_t &fine_state, const int32_t max_points, std::vector<Vector3> *out_points );

/**
* @brief	Apply bounded LOS and smoothing postprocess to a waypoint path.
* @param	mesh	Optional nav2 query mesh context for future topology-aware checks.
* @param	policy	Postprocess policy controls.
* @param	input_waypoints	Input world-space waypoints.
* @param	out_result	[out] Smoothed waypoints and diagnostics.
* @return	True when postprocess completed.
**/
const bool SVG_Nav2_Postprocess_Path( const nav2_query_mesh_t *mesh, const nav2_postprocess_policy_t &policy,
    const std::vector<Vector3> &input_waypoints, nav2_postprocess_result_t *out_result );

/**
* @brief	Run postprocess directly on a fine-search state.
* @param	mesh	Optional nav2 query mesh context for future topology-aware checks.
* @param	fine_state	Fine-search state supplying path and node metadata.
* @param	policy	Postprocess policy controls.
* @param	out_result	[out] Smoothed waypoints and diagnostics.
* @return	True when postprocess completed.
**/
const bool SVG_Nav2_Postprocess_FinePath( const nav2_query_mesh_t *mesh, const nav2_fine_astar_state_t &fine_state,
    const nav2_postprocess_policy_t &policy, nav2_postprocess_result_t *out_result );

/**
* @brief	Emit bounded debug diagnostics for postprocess output.
* @param	result	Postprocess result to inspect.
* @param	max_print	Maximum waypoint lines to print.
**/
void SVG_Nav2_DebugPrintPostprocess( const nav2_postprocess_result_t &result, const int32_t max_print = 16 );

/**
* @brief	Keep the nav2 postprocess module represented in the build.
**/
void SVG_Nav2_Postprocess_ModuleAnchor( void );
