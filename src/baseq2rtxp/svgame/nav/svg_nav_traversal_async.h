/********************************************************************
*
*
*    SVGame: Navigation Traversal Async Helpers
*
*
********************************************************************/
#pragma once

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"

struct svg_nav_path_process_t;
struct svg_nav_path_policy_t;


/**
*    @brief    Result status returned by incremental A*.
**/
enum class nav_a_star_status_t {
	Idle,
	Running,
	Completed,
	Failed,
};

/**
*    @brief    Working state for incremental navigation path searches.
**/
struct nav_a_star_state_t {
	//! Current search phase status.
	nav_a_star_status_t status = nav_a_star_status_t::Idle;
	//! Navigation mesh used for node lookups and geometry heuristics.
	const nav_mesh_t *mesh = nullptr;
	//! Agent bounding box used for edge-validation.
	Vector3 agent_mins = {};
	//! Agent bounding box top extents used for edge-validation.
	Vector3 agent_maxs = {};
	//! Node references for start and goal positions.
	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};

	//! Working node storage and open list for the current search.
	std::vector<nav_search_node_t> nodes;
	std::vector<int32_t> open_list;
	std::unordered_map<nav_node_key_t, int32_t, nav_node_key_hash_t> node_lookup;

	//! Optional tile-route filter storage/copy to extend lifetime.
	std::vector<nav_tile_cluster_key_t> tile_route_storage;
	//! Pointer to either `tile_route_storage` or nullptr when no filter applies.
	const std::vector<nav_tile_cluster_key_t> *tileRouteFilter = nullptr;

	//! Optional traversal policy tuning applied per expansion.
	const svg_nav_path_policy_t *policy = nullptr;
	//! Optional owner path process used for failure penalties.
	const svg_nav_path_process_t *pathProcess = nullptr;

	//! Recorded index of the goal node after completion.
	int32_t goal_index = -1;

	//! Configurable limits and diagnostics counters.
	int32_t max_nodes = 8192;
	uint64_t search_budget_ms = 64;
	uint64_t step_start_ms = 0;
	double best_f_cost_seen = std::numeric_limits<double>::infinity();
	int32_t stagnation_count = 0;
	bool hit_stagnation_limit = false;
	bool hit_time_budget = false;
	bool saw_vertical_neighbor = false;
	int32_t neighbor_try_count = 0;
	int32_t no_node_count = 0;
	int32_t tile_filter_reject_count = 0;
	int32_t edge_reject_count = 0;
};

/**
*    @brief    Initialize incremental A* state ready for step-wise expansion.
*    @param    state        [in,out] Working state.
*    @param    mesh         Navigation mesh pointer (may be nullptr until generated).
*    @param    start_node   Starting node reference.
*    @param    goal_node    Goal node reference.
*    @param    agent_mins   Bbox mins used for traversal validation.
*    @param    agent_maxs   Bbox maxs used for traversal validation.
*    @param    policy       Traversal policy tuning.
*    @param    tileRoute    Optional coarse tile route to restrict expansion.
*    @param    pathProcess  Optional owning path process for failure penalties.
*    @return   True when initialization succeeded.
*    @note     Copies the provided tile route and configures the per-call budgets so
*              subsequent `Nav_AStar_Step` calls honor the intended limits.
**/
bool Nav_AStar_Init( nav_a_star_state_t *state, const nav_mesh_t *mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t *policy, const std::vector<nav_tile_cluster_key_t> *tileRoute,
	const svg_nav_path_process_t *pathProcess );

/**
*    @brief    Advance the search by the specified number of node expansions.
*    @param    state            Working incremental state.
*    @param    expansions       Maximum node expansions to perform this call.
*    @return   Current A* status (running/completed/failed).
 *    @note     Enforces the configured per-call expansion budget, node cap, and failure
 *              heuristics before returning so long-running searches progress incrementally.
**/
nav_a_star_status_t Nav_AStar_Step( nav_a_star_state_t *state, int32_t expansions );

/**
*    @brief    Finalize the path if A* completed and output the resulting point list.
*    @param    state        Finalized state (must be Completed).
*    @param    out_points   [out] Vector to fill with ordered path points.
*    @return   True on success.
 *    @note     Walks the parent chain from the solved node to emit ordered waypoints; fails if
 *              the state is not completed.
**/
bool Nav_AStar_Finalize( nav_a_star_state_t *state, std::vector<Vector3> *out_points );

/**
*    @brief    Reset incremental state before reuse or destruction.
 *    @note     Clears the node/open containers, diagnostic counters, and returns status to Idle.
**/
void Nav_AStar_Reset( nav_a_star_state_t *state );
