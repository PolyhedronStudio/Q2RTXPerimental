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
#include <array>
#include <new>

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
 *    @brief    Reasons an edge/neighbor can be rejected during expansion.
 **/
enum class nav_edge_reject_reason_t : int32_t {
	None = 0,
	TileRouteFilter = 1,
	NoNode = 2,
	DropCap = 3,
	StepTest = 4,
	ObstructionJump = 5,
	Occupancy = 6,
	Other = 7,
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

	/**
	*    @brief    Accessor returning a pointer to the tile-route filter if present.
	*    @note     Prefer using this accessor to avoid directly copying raw internal pointers.
	**/
	const std::vector<nav_tile_cluster_key_t> *GetTileRouteFilterPtr() const {
		return tileRouteFilter;
	}

	/**
	*    @brief    Return a safe reference to the tile-route filter. If none exists,
	*              an empty static vector is returned so callers can iterate without
	*              checking for nullptr.
	**/
	const std::vector<nav_tile_cluster_key_t> &GetTileRouteFilterView() const {
		static const std::vector<nav_tile_cluster_key_t> s_empty = {};
		return tileRouteFilter ? *tileRouteFilter : s_empty;
	}

	/**
	*    @brief    Rebind any internal non-owning pointers that reference owned
	*              storage inside this object. Call after moves or when the
	*              internal storage has been changed.
	**/
	void RebindInternalPointers() {
       tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
		// Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
	}

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

	//! Per-reason counters for edge rejection to help diagnose why neighbors are discarded.
	//! Index into this array is defined by `nav_edge_reject_reason_t`.
	std::array<int32_t, 8> edge_reject_reason_counts = {};

	// Default constructor / destructor
	nav_a_star_state_t() = default;
	~nav_a_star_state_t() = default;

	/**
	*    @brief    Use the engine tag allocator for nav state allocations so
	*              they are tracked with level memory and can be freed by
	*              the engine's tag-freeing facilities.
	**/
	static void *operator new( std::size_t size ) {
		void *p = gi.TagMallocz( (int)size, TAG_SVGAME_LEVEL );
		if ( !p ) {
			// On allocation failure, propagate as bad_alloc to match normal new semantics.
			throw std::bad_alloc();
		}
		return p;
	}

	/**
	*    @brief    Ensure deallocation uses the engine tag-free to return memory to the
	*              level allocator.
	**/
	static void operator delete( void *ptr ) noexcept {
		if ( ptr ) {
			gi.TagFree( ptr );
		}
	}

	// Copy constructor: deep-copy containers and rebind internal view pointer.
	nav_a_star_state_t(const nav_a_star_state_t &o)
		: status(o.status),
		  mesh(o.mesh),
		  agent_mins(o.agent_mins),
		  agent_maxs(o.agent_maxs),
		  start_node(o.start_node),
		  goal_node(o.goal_node),
		  nodes(o.nodes),
		  open_list(o.open_list),
		  node_lookup(o.node_lookup),
		  tile_route_storage(o.tile_route_storage),
		  policy(o.policy),
		  pathProcess(o.pathProcess),
		  goal_index(o.goal_index),
		  max_nodes(o.max_nodes),
		  search_budget_ms(o.search_budget_ms),
		  step_start_ms(o.step_start_ms),
		  best_f_cost_seen(o.best_f_cost_seen),
		  stagnation_count(o.stagnation_count),
		  hit_stagnation_limit(o.hit_stagnation_limit),
		  hit_time_budget(o.hit_time_budget),
		  saw_vertical_neighbor(o.saw_vertical_neighbor),
		  neighbor_try_count(o.neighbor_try_count),
		  no_node_count(o.no_node_count),
		  tile_filter_reject_count(o.tile_filter_reject_count),
		  edge_reject_count(o.edge_reject_count)
   {
		// Rebind pointer to our own storage (or nullptr)
		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
		// Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
	}

	// Copy assignment: copy containers and rebind pointer.
	nav_a_star_state_t &operator=(const nav_a_star_state_t &o) {
		if ( this == &o ) {
			return *this;
		}
		status = o.status;
		mesh = o.mesh;
		agent_mins = o.agent_mins;
		agent_maxs = o.agent_maxs;
		start_node = o.start_node;
		goal_node = o.goal_node;
		nodes = o.nodes;
		open_list = o.open_list;
		node_lookup = o.node_lookup;
		tile_route_storage = o.tile_route_storage;
		policy = o.policy;
		pathProcess = o.pathProcess;
		goal_index = o.goal_index;
		max_nodes = o.max_nodes;
		search_budget_ms = o.search_budget_ms;
		step_start_ms = o.step_start_ms;
		best_f_cost_seen = o.best_f_cost_seen;
		stagnation_count = o.stagnation_count;
		hit_stagnation_limit = o.hit_stagnation_limit;
		hit_time_budget = o.hit_time_budget;
		saw_vertical_neighbor = o.saw_vertical_neighbor;
		neighbor_try_count = o.neighbor_try_count;
		no_node_count = o.no_node_count;
		tile_filter_reject_count = o.tile_filter_reject_count;
		edge_reject_count = o.edge_reject_count;

           tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
		// Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
		return *this;
	}

	// Move constructor: move containers and rebind pointer into destination.
	nav_a_star_state_t(nav_a_star_state_t &&o) noexcept
		: status(o.status),
		  mesh(o.mesh),
		  agent_mins(std::move(o.agent_mins)),
		  agent_maxs(std::move(o.agent_maxs)),
		  start_node(std::move(o.start_node)),
		  goal_node(std::move(o.goal_node)),
		  nodes(std::move(o.nodes)),
		  open_list(std::move(o.open_list)),
		  node_lookup(std::move(o.node_lookup)),
		  tile_route_storage(std::move(o.tile_route_storage)),
		  policy(o.policy),
		  pathProcess(o.pathProcess),
		  goal_index(o.goal_index),
		  max_nodes(o.max_nodes),
		  search_budget_ms(o.search_budget_ms),
		  step_start_ms(o.step_start_ms),
		  best_f_cost_seen(o.best_f_cost_seen),
		  stagnation_count(o.stagnation_count),
		  hit_stagnation_limit(o.hit_stagnation_limit),
		  hit_time_budget(o.hit_time_budget),
		  saw_vertical_neighbor(o.saw_vertical_neighbor),
		  neighbor_try_count(o.neighbor_try_count),
		  no_node_count(o.no_node_count),
		  tile_filter_reject_count(o.tile_filter_reject_count),
		  edge_reject_count(o.edge_reject_count)
   {
		// Rebind pointer to our own storage (or nullptr)
		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
		// Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
		// Leave moved-from object in a safe state.
		o.tileRouteFilter = nullptr;
		o.mesh = nullptr;
	}

	// Move assignment: move containers then rebind pointer.
	nav_a_star_state_t &operator=(nav_a_star_state_t &&o) noexcept {
		if ( this == &o ) {
			return *this;
		}
		status = o.status;
		mesh = o.mesh;
		agent_mins = std::move(o.agent_mins);
		agent_maxs = std::move(o.agent_maxs);
		start_node = std::move(o.start_node);
		goal_node = std::move(o.goal_node);
		nodes = std::move(o.nodes);
		open_list = std::move(o.open_list);
		node_lookup = std::move(o.node_lookup);
		tile_route_storage = std::move(o.tile_route_storage);
		policy = o.policy;
		pathProcess = o.pathProcess;
		goal_index = o.goal_index;
		max_nodes = o.max_nodes;
		search_budget_ms = o.search_budget_ms;
		step_start_ms = o.step_start_ms;
		best_f_cost_seen = o.best_f_cost_seen;
		stagnation_count = o.stagnation_count;
		hit_stagnation_limit = o.hit_stagnation_limit;
		hit_time_budget = o.hit_time_budget;
		saw_vertical_neighbor = o.saw_vertical_neighbor;
		neighbor_try_count = o.neighbor_try_count;
		no_node_count = o.no_node_count;
		tile_filter_reject_count = o.tile_filter_reject_count;
		edge_reject_count = o.edge_reject_count;

		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;

		// Reset moved-from object to safe state.
		o.tileRouteFilter = nullptr;
		o.mesh = nullptr;
		return *this;
	}
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
