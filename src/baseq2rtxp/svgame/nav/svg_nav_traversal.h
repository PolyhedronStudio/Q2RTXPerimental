/**
*
*
*
*   Navigation System "Traversal Operations":
*
*
*
**/
#pragma once


/**
*   @brief  Generate a traversal path between two world-space origins.
*           Uses the navigation voxelmesh and A* search to produce waypoints.
*   @param  start_origin    World-space starting origin.
*   @param  goal_origin     World-space destination origin.
*   @param  out_path        Output path result (caller must free).
*   @return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path );
/**
 *    @brief    Convert a caller-provided feet-origin into nav-center space.
 *    @param    mesh          Navigation mesh used to derive default agent hull when agent bbox is omitted.
 *    @param    feet_origin   World-space feet-origin (z at feet).
 *    @param    agent_mins    Optional agent bbox mins to override mesh defaults (pass nullptr to use mesh).
 *    @param    agent_maxs    Optional agent bbox maxs to override mesh defaults (pass nullptr to use mesh).
 *    @return   World-space position translated into nav-center space (Z adjusted by agent center offset).
 *    @note     Public traversal APIs accept feet-origin coordinates; use this helper to obtain the
 *              nav-center position used for node lookups and A*.
 **/
const Vector3 SVG_Nav_ConvertFeetToCenter( const nav_mesh_t *mesh, const Vector3 &feet_origin, const Vector3 *agent_mins = nullptr, const Vector3 *agent_maxs = nullptr );
/**
*	@brief  Generate a traversal path between two origins with optional goal Z-layer blending.
*	@param  start_origin             World-space start origin.
*	@param  goal_origin              World-space goal origin.
*	@param  out_path                 Output path result (caller must free).
*	@param  enable_goal_z_layer_blend  If true, blend start Z toward goal Z based on horizontal distance.
*	@param  blend_start_dist         Distance at which blending begins.
*	@param  blend_full_dist          Distance at which blending fully favors goal Z.
*	@return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOriginEx( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist );

/**
*	@brief  Generate a traversal path between two world-space origins with agent bbox.
* 			Uses the navigation voxelmesh and A* search to produce waypoints.
*	@param  start_origin    World-space starting origin.
*	@param  goal_origin     World-space destination origin.
*	@param  out_path        Output path result (caller must free).
*	@param  agent_mins      Agent bounding box minimums.
*	@param  agent_maxs      Agent bounding box maximums.
*	@param  policy          Navigation pathfinding policy.
*	@return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy );

/**
*	@brief  Generate a traversal path between two origins with optional goal Z-layer blending and agent bbox.
*	@param  start_origin             World-space start origin.
*	@param  goal_origin              World-space goal origin.
*	@param  out_path                 Output path result (caller must free).
*	@param  agent_mins               Agent bounding box minimums.
*	@param  agent_maxs               Agent bounding box maximums.
*	@param  policy                    Navigation pathfinding policy.
*	@param  enable_goal_z_layer_blend  If true, blend start Z toward goal Z based on horizontal distance.
*	@param  blend_start_dist         Distance at which blending begins.
*	@param  blend_full_dist          Distance at which blending fully favors goal Z.
*	@param  pathProcess               Optional path process structure to use (nullptr for default).
*	@return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy, const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
	const struct svg_nav_path_process_t *pathProcess );

/**
*   @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
*   @param  path    Path structure to free.
**/
void SVG_Nav_FreeTraversalPath( nav_traversal_path_t *path );

/**
*   @brief  Query movement direction along a traversal path.
*           Advances the waypoint index as the caller reaches waypoints.
*   @param  path            Path to follow.
*   @param  current_origin  Current world-space origin.
*   @param  waypoint_radius Radius for waypoint completion.
 *   @param  policy          Optional traversal policy for per-agent step/drop limits.
 *   @param  inout_index     Current waypoint index (updated on success).
 *   @param  out_direction   Output normalized movement direction.
*   @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin,
	double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 *out_direction );



/**
*
*
*
*	Navigation Movement Tests:
*
*
*
**/
/**
*	@brief	Low-level trace wrapper that supports world or inline-model clip.
* 	@param	start	World-space start position.
* 	@param	mins	World-space bounding box minimums.
* 	@param	maxs	World-space bounding box maximums.
* 	@param	end		World-space end position.
* 	@param	clip_entity	Optional entity to clip against (inline model), null for world.
* 	@param	mask		Content mask for the trace.
* 	@return	Trace result.
**/
const inline svg_trace_t Nav_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end,
	const edict_ptr_t *clip_entity, const cm_contents_t mask );

/**
*	@brief	Performs a simple PMove-like step traversal test (3-trace).
*
*			This is intentionally conservative and is used only to validate edges in A*:
*			1) Try direct horizontal move.
*			2) If blocked, try stepping up <= max step, then horizontal move.
*			3) Trace down to find ground.
*
*	@param	mesh		Navigation mesh (provides default agent bbox and step/slope limits).
*	@param	startPos	Start world-space position (nav-center space).
*	@param	endPos		End world-space position (nav-center space).
*	@param	clip_entity	Optional entity to clip against (inline model). Pass nullptr for world.
*	@return	True if the traversal is possible, false otherwise.
*	@note	This is a conservative feasibility test intended for validating edges during A*.
**/
const bool Nav_CanTraverseStep( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t *clip_entity, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );
/**
* 	@brief	Performs a simple PMove-like step traversal test (3-trace) with explicit bbox.
* 			This is intentionally conservative and is used only to validate edges in A*:
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
* 	@param	mesh		Navigation mesh.
* 	@param	startPos	Start world-space position (nav-center space).
* 	@param	endPos		End world-space position (nav-center space).
* 	@param	mins		Agent bbox mins in the same space as start/end.
* 	@param	maxs		Agent bbox maxs in the same space as start/end.
* 	@param	clip_entity	Optional entity to clip against (inline model). Pass nullptr for world.
* 	@param	policy		Optional traversal policy (step/drop/jump constraints). Pass nullptr for mesh defaults.
* 	@return	True if the traversal is possible, false otherwise.
* 	@note	Used by A* expansion to ensure candidate neighbor edges are physically feasible.
**/
// Forward-declare reject reason enum from traversal async header so callers can opt-in to receive reasons.
#include "svgame/nav/svg_nav_traversal_async.h"
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy, nav_edge_reject_reason_t *out_reason = nullptr, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );

/**
* 	@brief	Validate a traversal edge using already-resolved canonical node references.
* 	@param	mesh		Navigation mesh.
* 	@param	start_node	Canonical start node already resolved on a walkable surface.
* 	@param	end_node	Canonical end node already resolved on a walkable surface.
* 	@param	mins		Agent bbox mins in nav-center space.
* 	@param	maxs		Agent bbox maxs in nav-center space.
* 	@param	clip_entity	Optional entity clip target (nullptr for world validation).
* 	@param	policy		Optional traversal policy (step/drop/jump constraints).
* 	@param	out_reason	[out] Optional reject reason when validation fails.
* 	@param	stepTraceMask	Reserved trace mask kept aligned with the position-based overload.
* 	@return	True if the canonical edge is traversable, false otherwise.
* 	@note	This avoids re-resolving the same node pair from world positions, which is important for
* 			async neighbor expansion on boundary-adjacent walkable cells.
**/
const bool Nav_CanTraverseStep_ExplicitBBox_NodeRefs( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy,
	nav_edge_reject_reason_t *out_reason = nullptr, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );
