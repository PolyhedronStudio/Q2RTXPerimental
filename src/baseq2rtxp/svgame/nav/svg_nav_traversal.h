/** 
* 
* 
* 
*    Navigation System "Traversal Operations":
* 
* 
* 
**/
#pragma once

#include "svgame/nav/svg_nav_traversal_async.h"

#include <vector>

/** 
*    @brief	Structured debug stats captured for the most recent sync navigation query.
*    @note	Phase-1 benchmarking uses this to compare flat-query cost before hierarchy rollout.
**/
struct nav_query_debug_stats_t {
	//! True when this struct contains data from at least one completed query.
	bool valid = false;
	//! True when the measured query produced a path.
	bool path_found = false;
	//! True when a coarse tile-route was discovered and used to constrain fine search.
	bool used_cluster_route = false;
   //! True when a hierarchy region/portal route was discovered and used to constrain fine search.
	bool used_hierarchy_route = false;
   //! True when hierarchy routing was the preferred coarse path for this query.
	bool hierarchy_preferred = false;
	//! True when hierarchy routing was preferred but the query had to fall back to cluster or unrestricted refinement.
	bool hierarchy_fallback_used = false;
	//! True when fine A*  had to run without any coarse corridor restriction.
	bool unrestricted_refine_used = false;
	//! True when the query had to fall back to a secondary endpoint-resolution strategy.
	bool fallback_used = false;
    //! True when fine A*  ran inside an explicit bounded refinement corridor.
	bool used_refine_corridor = false;
	//! Total wall-clock time for the query in milliseconds.
	double total_ms = 0.0;
	//! Time spent in the current coarse-route stage in milliseconds.
	double coarse_ms = 0.0;
	//! Time spent in fine A*  initialization, stepping, and finalize in milliseconds.
	double refine_ms = 0.0;
	//! Fine-search node expansions used as the current leaf/tile/cell expansion proxy.
	int32_t leaf_expansions = 0;
	//! Coarse-route expansion proxy, currently reported as buffered route length.
	int32_t coarse_expansions = 0;
	//! Number of open-list pushes performed by the fine search.
	int32_t open_pushes = 0;
	//! Number of open-list pops performed by the fine search.
	int32_t open_pops = 0;
  //! Number of LOS queries executed by the shared LOS entry point.
	int32_t los_tests = 0;
   //! Number of successful LOS queries executed by the shared LOS entry point.
	int32_t los_successes = 0;
  //! Number of compact regions in the active refinement corridor.
	int32_t corridor_region_count = 0;
	//! Number of compact portals in the active refinement corridor.
	int32_t corridor_portal_count = 0;
	//! Number of exact corridor tiles before buffered fine-search widening.
	int32_t corridor_tile_count = 0;
    //! Buffered refinement corridor radius selected for this query.
	int32_t corridor_buffer_radius = 0;
	//! Number of buffered corridor tiles available to fine refinement after widening.
	int32_t corridor_buffered_tile_count = 0;
  //! Number of greedy simplification LOS spans attempted on the finalized path.
	int32_t simplify_attempts = 0;
	//! Number of successful greedy simplification collapses that skipped intermediate points.
	int32_t simplify_successes = 0;
   //! Waypoint count entering the simplification pass.
	int32_t simplify_input_waypoint_count = 0;
	//! Waypoint count leaving the simplification pass.
	int32_t simplify_output_waypoint_count = 0;
	//! Number of duplicate waypoints pruned before LOS collapsing.
	int32_t simplify_duplicate_prunes = 0;
	//! Number of nearly-collinear waypoints pruned before LOS collapsing.
	int32_t simplify_collinear_prunes = 0;
	//! Number of simplification fallback local replans triggered for validation.
	int32_t simplify_fallback_local_replans = 0;
	//! Total simplification wall-clock milliseconds for the query.
	double simplify_ms = 0.0;
  //! Number of occupancy overlay samples encountered during fine refinement.
	int32_t occupancy_overlay_hits = 0;
	//! Number of occupancy overlay samples that contributed soft-cost pressure during refinement.
	int32_t occupancy_soft_cost_hits = 0;
	//! Number of fine-refinement neighbor expansions rejected due to hard occupancy blocking.
	int32_t occupancy_block_rejects = 0;
	//! Number of LOS shortcut/simplification checks rejected because the dynamic occupancy overlay was present.
	int32_t occupancy_los_rejects = 0;
  //! Number of hierarchy portal transitions skipped by the local portal overlay during coarse routing.
	int32_t portal_overlay_block_count = 0;
	//! Number of hierarchy portal transitions penalized by the local portal overlay during coarse routing.
	int32_t portal_overlay_penalty_count = 0;
	//! Number of waypoints in the finalized path.
	int32_t waypoint_count = 0;
};

/** 
* 	@brief	LOS traversal mode used by the reusable hierarchy accelerator.
**/
enum class nav_los_mode_t : uint8_t {
	//! Automatically choose the preferred LOS backend.
	Auto = 0,
	//! Use nav-grid DDA stepping over canonical nodes.
	DDA,
	//! Use conservative canonical leaf tracking plus direct step validation.
	LeafTraversal
};

/** 
* 	@brief	LOS request parameters shared by future shortcutting and simplification callers.
**/
struct nav_los_request_t {
	//! Requested LOS backend.
	nav_los_mode_t mode = nav_los_mode_t::Auto;
	//! Additional scale applied to the default LOS sample spacing.
	double sample_step_scale = 1.0;
	//! If true, consult sparse dynamic occupancy while evaluating LOS samples.
	bool consult_dynamic_occupancy = true;
};

/** 
* 	@brief	LOS result payload returned by the reusable LOS entry point.
**/
struct nav_los_result_t {
	//! True when the LOS query succeeded.
	bool has_line_of_sight = false;
	//! Requested mode supplied by the caller.
	nav_los_mode_t requested_mode = nav_los_mode_t::Auto;
	//! Backend actually used by the LOS query.
	nav_los_mode_t resolved_mode = nav_los_mode_t::Auto;
	//! Number of positional samples evaluated by the LOS backend.
	int32_t sample_count = 0;
	//! Number of BSP leaf visits recorded by leaf-traversal mode.
	int32_t leaf_visit_count = 0;
	//! Number of canonical node resolutions performed by the LOS backend.
	int32_t node_resolve_count = 0;
};

/** 
* 	@brief	Greedy LOS simplification counters for one finalized path pass.
**/
struct nav_path_simplify_stats_t {
   //! Waypoint count before any simplification or cleanup pass.
	int32_t input_waypoint_count = 0;
	//! Waypoint count after the final simplification pass.
	int32_t output_waypoint_count = 0;
	//! Number of candidate LOS spans tested while scanning for collapsible segments.
	int32_t attempts = 0;
	//! Number of successful collapses that skipped at least one intermediate point.
	int32_t successes = 0;
  //! Number of duplicate waypoint removals performed before LOS collapsing.
	int32_t duplicate_prunes = 0;
	//! Number of nearly-collinear waypoint removals performed before LOS collapsing.
	int32_t collinear_prunes = 0;
	//! Number of local replan validations triggered by simplification fallback logic.
	int32_t fallback_local_replan_count = 0;
	//! Total wall-clock milliseconds spent inside the simplification pass.
	double total_ms = 0.0;
};

/** 
* 	@brief	Internal simplification aggressiveness mode shared by sync and async callers.
**/
enum class nav_path_simplify_aggressiveness_t : uint8_t {
	//! Small sync pass: cleanup plus only a tightly budgeted LOS collapse attempt.
	SyncConservative = 0,
	//! Async pass: more aggressive farthest-jump LOS collapsing with bounded validation.
	AsyncAggressive
};

/** 
* 	@brief	Optional simplification controls used by sync and async callers.
**/
struct nav_path_simplify_options_t {
	//! Maximum LOS tests allowed for this simplification pass (0 or less means unlimited).
	int32_t max_los_tests = 0;
	//! Maximum wall-clock milliseconds allowed for this simplification pass (0 means unlimited).
	uint64_t max_elapsed_ms = 0;
	//! Angle threshold below which interior points are treated as collinear enough to prune.
	double collinear_angle_degrees = 4.0;
	//! Whether duplicate removal should run before LOS collapsing.
	bool remove_duplicates = true;
	//! Whether collinearity pruning should run before LOS collapsing.
	bool prune_collinear = true;
	//! Aggressiveness mode controlling how candidate LOS spans are searched.
	nav_path_simplify_aggressiveness_t aggressiveness = nav_path_simplify_aggressiveness_t::SyncConservative;
};

/**
*  Targeted pathfinding diagnostics:
*      These are intentionally gated behind `nav_debug_draw >= 2` to avoid
*      spamming logs during normal gameplay.
**/
bool Nav_PathDiagEnabled( void );

/** 
*    @brief	Return the last recorded sync navigation query stats.
*    @return	Const reference to the cached phase-1 debug stats snapshot.
**/
const nav_query_debug_stats_t &SVG_Nav_GetLastQueryStats( void );

/** 
* 	@brief	Build a coarse tile-route filter from hierarchy routing with legacy cluster fallback.
* 	@param	mesh	Navigation mesh containing hierarchy and cluster routing data.
* 	@param	start_node	Resolved canonical start node.
* 	@param	goal_node	Resolved canonical goal node.
* 	@param	policy	Optional traversal policy used to gate coarse transitions.
* 	@param	out_tile_route	[out] Materialized tile-key route usable by fine A* .
* 	@param	out_used_hierarchy	[out] True when hierarchy coarse A*  produced the returned route.
* 	@param	out_used_cluster	[out] True when legacy tile-cluster routing produced the returned route.
* 	@param	out_coarse_expansions	[out] Coarse expansion count reported for diagnostics.
* 	@return	True when a coarse route filter was produced, false when callers should fall back to unrestricted fine A* .
**/
bool SVG_Nav_BuildCoarseRouteFilter( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const svg_nav_path_policy_t * policy, std::vector<nav_tile_cluster_key_t> * out_tile_route,
	bool * out_used_hierarchy = nullptr, bool * out_used_cluster = nullptr, int32_t * out_coarse_expansions = nullptr );

/** 
* 	@brief	Build an explicit bounded refinement corridor from hierarchy routing with legacy cluster fallback.
* 	@param	mesh	Navigation mesh containing hierarchy and cluster routing data.
* 	@param	start_node	Resolved canonical start node.
* 	@param	goal_node	Resolved canonical goal node.
* 	@param	policy	Optional traversal policy used to gate coarse transitions.
* 	@param	out_corridor	[out] Explicit corridor describing the bounded local refinement space.
* 	@param	out_used_hierarchy	[out] True when hierarchy coarse A*  produced the corridor.
* 	@param	out_used_cluster	[out] True when legacy tile-cluster routing produced the corridor.
* 	@param	out_coarse_expansions	[out] Coarse expansion count reported for diagnostics.
* 	@return	True when a bounded refinement corridor was produced.
**/
bool SVG_Nav_BuildRefineCorridor( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const svg_nav_path_policy_t * policy, nav_refine_corridor_t * out_corridor,
	bool * out_used_hierarchy = nullptr, bool * out_used_cluster = nullptr, int32_t * out_coarse_expansions = nullptr );

/** 
* 	@brief	Evaluate a reusable LOS accelerator query between two canonical nodes.
* 	@param	mesh	Navigation mesh containing the canonical world tiles and BSP linkage.
* 	@param	start_node	Canonical LOS start node.
* 	@param	goal_node	Canonical LOS goal node.
* 	@param	agent_mins	Agent bbox minimums in nav-center space.
* 	@param	agent_maxs	Agent bbox maximums in nav-center space.
* 	@param	policy	Optional traversal policy controlling hazards and occupancy handling.
* 	@param	request	Optional LOS mode/sample configuration.
* 	@param	out_result	[out] Optional LOS result payload describing the backend work performed.
* 	@return	True when the direct LOS segment is considered traversable.
* 	@note	This is a reusable accelerator for future shortcutting phases, not a replacement pathfinder.
**/
bool SVG_Nav_HasLineOfSight( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy = nullptr,
	const nav_los_request_t * request = nullptr, nav_los_result_t * out_result = nullptr );

/** 
* 	@brief	Greedily simplify a finalized nav-center waypoint list using the shared LOS API.
* 	@param	mesh	Navigation mesh used to resolve canonical waypoint nodes.
* 	@param	agent_mins	Agent bbox minimums in nav-center space.
* 	@param	agent_maxs	Agent bbox maximums in nav-center space.
* 	@param	policy	Optional traversal policy controlling hazards and occupancy checks.
* 	@param	inout_points	[in,out] Finalized nav-center waypoint list to simplify in place.
* 	@param	out_stats	[out] Optional simplification attempt/success counters.
* 	@return	True when the waypoint list was shortened, false when no safe collapse was found.
* 	@note	This is a post-refinement path simplifier, not a replacement pathfinder.
**/
bool SVG_Nav_SimplifyPathPoints( const nav_mesh_t * mesh, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t * policy, std::vector<Vector3> * inout_points, const nav_path_simplify_options_t * options = nullptr,
	nav_path_simplify_stats_t * out_stats = nullptr );


/** 
*    @brief  Generate a traversal path between two world-space origins.
*         Uses the navigation voxelmesh and A*  search to produce waypoints.
*    @param  start_origin    World-space starting origin.
*    @param  goal_origin     World-space destination origin.
*    @param  out_path        Output path result (caller must free).
*    @return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path );
/** 
* @brief    Convert a caller-provided feet-origin into nav-center space.
* @param    mesh          Navigation mesh used to derive default agent hull when agent bbox is omitted.
* @param    feet_origin   World-space feet-origin (z at feet).
* @param    agent_mins    Optional agent bbox mins to override mesh defaults (pass nullptr to use mesh).
* @param    agent_maxs    Optional agent bbox maxs to override mesh defaults (pass nullptr to use mesh).
* @return   World-space position translated into nav-center space (Z adjusted by agent center offset).
* @note     Public traversal APIs accept feet-origin coordinates; use this helper to obtain the
*           nav-center position used for node lookups and A* .
 **/
const Vector3 SVG_Nav_ConvertFeetToCenter( const nav_mesh_t * mesh, const Vector3 &feet_origin, const Vector3 * agent_mins = nullptr, const Vector3 * agent_maxs = nullptr );
/** 
* 	@brief  Generate a traversal path between two origins with optional goal Z-layer blending.
* 	@param  start_origin             World-space start origin.
* 	@param  goal_origin              World-space goal origin.
* 	@param  out_path                 Output path result (caller must free).
* 	@param  enable_goal_z_layer_blend  If true, blend start Z toward goal Z based on horizontal distance.
* 	@param  blend_start_dist         Distance at which blending begins.
* 	@param  blend_full_dist          Distance at which blending fully favors goal Z.
* 	@return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOriginEx( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path,
	const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist );

/** 
* 	@brief  Generate a traversal path between two world-space origins with agent bbox.
*  			Uses the navigation voxelmesh and A*  search to produce waypoints.
* 	@param  start_origin    World-space starting origin.
* 	@param  goal_origin     World-space destination origin.
* 	@param  out_path        Output path result (caller must free).
* 	@param  agent_mins      Agent bounding box minimums.
* 	@param  agent_maxs      Agent bounding box maximums.
* 	@param  policy          Navigation pathfinding policy.
* 	@return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy );

/** 
* 	@brief  Generate a traversal path between two origins with optional goal Z-layer blending and agent bbox.
* 	@param  start_origin             World-space start origin.
* 	@param  goal_origin              World-space goal origin.
* 	@param  out_path                 Output path result (caller must free).
* 	@param  agent_mins               Agent bounding box minimums.
* 	@param  agent_maxs               Agent bounding box maximums.
* 	@param  policy                    Navigation pathfinding policy.
* 	@param  enable_goal_z_layer_blend  If true, blend start Z toward goal Z based on horizontal distance.
* 	@param  blend_start_dist         Distance at which blending begins.
* 	@param  blend_full_dist          Distance at which blending fully favors goal Z.
* 	@param  pathProcess               Optional path process structure to use (nullptr for default).
* 	@return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy, const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
	const struct svg_nav_path_process_t * pathProcess );

/** 
*    @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
*    @param  path    Path structure to free.
**/
void SVG_Nav_FreeTraversalPath( nav_traversal_path_t * path );

/** 
*    @brief  Query movement direction along a traversal path.
*         Advances the waypoint index as the caller reaches waypoints.
*    @param  path            Path to follow.
*    @param  current_origin  Current world-space origin.
*    @param  waypoint_radius Radius for waypoint completion.
*   @param  policy          Optional traversal policy for per-agent step/drop limits.
*   @param  inout_index     Current waypoint index (updated on success).
*   @param  out_direction   Output normalized movement direction.
*    @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t * path, const Vector3 &current_origin,
	double waypoint_radius, const svg_nav_path_policy_t * policy, int32_t * inout_index, Vector3 * out_direction );



/** 
* 
* 
* 
* 	Navigation Movement Tests:
* 
* 
* 
**/
/** 
* 	@brief	Low-level trace wrapper that supports world or inline-model clip.
*  	@param	start	World-space start position.
*  	@param	mins	World-space bounding box minimums.
*  	@param	maxs	World-space bounding box maximums.
*  	@param	end		World-space end position.
*  	@param	clip_entity	Optional entity to clip against (inline model), null for world.
*  	@param	mask		Content mask for the trace.
*  	@return	Trace result.
**/
const inline svg_trace_t Nav_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end,
	const edict_ptr_t * clip_entity, const cm_contents_t mask );

/** 
* 	@brief	Performs a simple PMove-like step traversal test (3-trace).
* 
* 			This is intentionally conservative and is used only to validate edges in A* :
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
* 
* 	@param	mesh		Navigation mesh (provides default agent bbox and step/slope limits).
* 	@param	startPos	Start world-space position (nav-center space).
* 	@param	endPos		End world-space position (nav-center space).
* 	@param	clip_entity	Optional entity to clip against (inline model). Pass nullptr for world.
* 	@return	True if the traversal is possible, false otherwise.
* 	@note	This is a conservative feasibility test intended for validating edges during A* .
**/
const bool Nav_CanTraverseStep( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t * clip_entity, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );
/** 
*  	@brief	Performs a simple PMove-like step traversal test (3-trace) with explicit bbox.
*  			This is intentionally conservative and is used only to validate edges in A* :
*  			1) Try direct horizontal move.
*  			2) If blocked, try stepping up <= max step, then horizontal move.
*  			3) Trace down to find ground.
*  	@param	mesh		Navigation mesh.
*  	@param	startPos	Start world-space position (nav-center space).
*  	@param	endPos		End world-space position (nav-center space).
*  	@param	mins		Agent bbox mins in the same space as start/end.
*  	@param	maxs		Agent bbox maxs in the same space as start/end.
*  	@param	clip_entity	Optional entity to clip against (inline model). Pass nullptr for world.
*  	@param	policy		Optional traversal policy (step/drop/jump constraints). Pass nullptr for mesh defaults.
*  	@return	True if the traversal is possible, false otherwise.
*  	@note	Used by A*  expansion to ensure candidate neighbor edges are physically feasible.
**/
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity, const svg_nav_path_policy_t * policy, nav_edge_reject_reason_t * out_reason = nullptr, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );

/** 
*  	@brief	Validate a traversal edge using already-resolved canonical node references.
*  	@param	mesh		Navigation mesh.
*  	@param	start_node	Canonical start node already resolved on a walkable surface.
*  	@param	end_node	Canonical end node already resolved on a walkable surface.
*  	@param	mins		Agent bbox mins in nav-center space.
*  	@param	maxs		Agent bbox maxs in nav-center space.
*  	@param	clip_entity	Optional entity clip target (nullptr for world validation).
*  	@param	policy		Optional traversal policy (step/drop/jump constraints).
*  	@param	out_reason	[out] Optional reject reason when validation fails.
*  	@param	stepTraceMask	Reserved trace mask kept aligned with the position-based overload.
*  	@return	True if the canonical edge is traversable, false otherwise.
*  	@note	This avoids re-resolving the same node pair from world positions, which is important for
*  			async neighbor expansion on boundary-adjacent walkable cells.
**/
const bool Nav_CanTraverseStep_ExplicitBBox_NodeRefs( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity, const svg_nav_path_policy_t * policy,
	nav_edge_reject_reason_t * out_reason = nullptr, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );
