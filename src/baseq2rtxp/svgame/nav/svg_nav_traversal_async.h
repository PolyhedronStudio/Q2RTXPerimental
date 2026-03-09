/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
*  SVGame: Navigation Traversal Async Helpers
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#pragma once

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>
#include <new>

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"

struct svg_nav_path_process_t;
struct svg_nav_path_policy_t;


/** 
*  @brief    Result status returned by incremental A* .
**/
enum class nav_a_star_status_t {
	Idle,
	Running,
	Completed,
	Failed,
};

/** 
* @brief    Reasons an edge/neighbor can be rejected during expansion.
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
*  @brief    Source of the current bounded refinement corridor.
**/
enum class nav_refine_corridor_source_t : uint8_t {
	None = 0,
	Hierarchy,
	Cluster
};

/** 
*  @brief    Explicit bounded refinement corridor derived from the coarse route stage.
**/
struct nav_refine_corridor_t {
	//! Coarse source that produced this corridor.
	nav_refine_corridor_source_t source = nav_refine_corridor_source_t::None;
	//! Ordered compact region path for hierarchy corridors.
	std::vector<int32_t> region_path;
	//! Ordered compact portal path for hierarchy corridors.
	std::vector<int32_t> portal_path;
	//! Ordered exact tile route used as the seed for buffered fine-search restriction.
	std::vector<nav_tile_cluster_key_t> exact_tile_route;
	//! Number of local portal overlay rejections encountered while building this corridor.
	int32_t overlay_block_count = 0;
	//! Number of local portal overlay penalty applications encountered while building this corridor.
	int32_t overlay_penalty_count = 0;

	/** 
	*  @brief    Return true when the corridor contains any tile guidance for fine refinement.
	**/
	bool HasExactTileRoute() const {
		return !exact_tile_route.empty();
	}
};

/** 
*  @brief    Cache key for a directional edge validation result between two canonical nav nodes.
**/
struct nav_edge_cache_key_t {
	//! Canonical node key at the start of the directed edge.
	nav_node_key_t from = {};
	//! Canonical node key at the end of the directed edge.
	nav_node_key_t to = {};

	bool operator==( const nav_edge_cache_key_t &o ) const {
		return from == o.from && to == o.to;
	}
};

/** 
*  @brief    Hasher for directional edge validation cache keys.
**/
struct nav_edge_cache_key_hash_t {
	size_t operator()( const nav_edge_cache_key_t &k ) const {
		size_t seed = nav_node_key_hash_t{}( k.from );
		seed ^= nav_node_key_hash_t{}( k.to ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

/** 
*  @brief    Working state for incremental navigation path searches.
**/
struct nav_a_star_state_t {
	//! Current search phase status.
	nav_a_star_status_t status = nav_a_star_status_t::Idle;
	//! Navigation mesh used for node lookups and geometry heuristics.
	const nav_mesh_t * mesh = nullptr;
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
	//! Cached directional edge-validation results for this search.
	std::unordered_map<nav_edge_cache_key_t, nav_edge_reject_reason_t, nav_edge_cache_key_hash_t> edge_validation_cache;
	//! Explicit bounded refinement corridor copied from the coarse route stage.
	nav_refine_corridor_t refine_corridor = {};

	//! Optional tile-route filter storage/copy to extend lifetime.
	std::vector<nav_tile_cluster_key_t> tile_route_storage;
   //! Constant-time membership lookup for the buffered tile-route filter.
	std::unordered_set<nav_tile_cluster_key_t, nav_tile_cluster_key_hash_t> tile_route_lookup;
	//! Pointer to either `tile_route_storage` or nullptr when no filter applies.
	const std::vector<nav_tile_cluster_key_t> * tileRouteFilter = nullptr;

	/** 
	*  @brief    Accessor returning a pointer to the tile-route filter if present.
	*  @note     Prefer using this accessor to avoid directly copying raw internal pointers.
	**/
	const std::vector<nav_tile_cluster_key_t> * GetTileRouteFilterPtr() const {
		return tileRouteFilter;
	}

	/** 
	*  @brief    Return a safe reference to the tile-route filter. If none exists,
	*            an empty static vector is returned so callers can iterate without
	*            checking for nullptr.
	**/
	const std::vector<nav_tile_cluster_key_t> &GetTileRouteFilterView() const {
		static const std::vector<nav_tile_cluster_key_t> s_empty = {};
		return tileRouteFilter ? * tileRouteFilter : s_empty;
	}

	/** 
	*  @brief    Rebind any internal non-owning pointers that reference owned
	*            storage inside this object. Call after moves or when the
	*            internal storage has been changed.
	**/
	void RebindInternalPointers() {
		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
		 // Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
	}

	//! Optional traversal policy tuning applied per expansion.
	const svg_nav_path_policy_t * policy = nullptr;
	//! Optional owner path process used for failure penalties.
	const svg_nav_path_process_t * pathProcess = nullptr;

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
 //! Number of nodes popped from the open list for expansion.
	int32_t node_expand_count = 0;
	//! Number of entries pushed onto the open list.
	int32_t open_push_count = 0;
	//! Number of entries popped from the open list.
	int32_t open_pop_count = 0;
	//! Number of neighbor nodes considered during expansions (including those rejected for having no node, failing filters, etc).
	int32_t neighbor_try_count = 0;
	//! Number of neighbor nodes rejected for having no corresponding nav node (missing tile, cell, or layer).
	int32_t no_node_count = 0;
	//! Number of no-node failures caused by an invalid current tile reference.
	int32_t no_node_invalid_current_tile_count = 0;
	//! Number of no-node failures caused by missing or invalid target world tiles.
	int32_t no_node_target_tile_count = 0;
	//! Number of no-node failures caused by missing sparse-cell presence bits.
	int32_t no_node_presence_count = 0;
	//! Number of no-node failures caused by missing target cell storage.
	int32_t no_node_cell_view_count = 0;
	//! Number of no-node failures caused by failing to choose a compatible target layer.
	int32_t no_node_layer_select_count = 0;
	int32_t tile_filter_reject_count = 0;
	int32_t edge_reject_count = 0;
	//! Number of sparse occupancy overlay entries encountered during refinement.
	int32_t occupancy_overlay_hit_count = 0;
	//! Number of sparse occupancy overlay entries that contributed additional soft cost.
	int32_t occupancy_soft_cost_hit_count = 0;
	//! Number of sparse occupancy overlay entries that caused hard rejection.
	int32_t occupancy_block_reject_count = 0;
	//! Buffered refinement corridor radius applied during fine-search initialization.
	int32_t corridor_buffer_radius = 0;
	//! Number of buffered corridor tiles after widening the exact route for refinement.
	int32_t corridor_buffered_tile_count = 0;
	//! Number of neighbor queries that resolved back onto the currently expanded node.
	int32_t same_node_alias_count = 0;
	//! Number of neighbor queries that resolved to an already-closed node entry.
	int32_t closed_duplicate_count = 0;
	//! Number of long-hop probes skipped because the full one-cell chain already exists.
	int32_t pass_through_prune_count = 0;

	//! Per-reason counters for edge rejection to help diagnose why neighbors are discarded.
	//! Index into this array is defined by `nav_edge_reject_reason_t`.
	std::array<int32_t, 8> edge_reject_reason_counts = {};

	// Default constructor / destructor
	nav_a_star_state_t() = default;
	~nav_a_star_state_t() = default;

	/** 
	*  @brief    Use the engine tag allocator for nav state allocations so
	*            they are tracked with level memory and can be freed by
	*            the engine's tag-freeing facilities.
	**/
	static void * operator new( std::size_t size ) {
		void * p = gi.TagMallocz( ( int )size, TAG_SVGAME_NAVMESH );
		if ( !p ) {
			// On allocation failure, propagate as bad_alloc to match normal new semantics.
			throw std::bad_alloc();
		}
		return p;
	}

	/** 
	*  @brief    Ensure deallocation uses the engine tag-free to return memory to the
	*            level allocator.
	**/
	static void operator delete( void * ptr ) noexcept {
		if ( ptr ) {
			gi.TagFree( ptr );
		}
	}

	// Copy constructor: deep-copy containers and rebind internal view pointer.
	nav_a_star_state_t( const nav_a_star_state_t &o )
		: status( o.status ),
		mesh( o.mesh ),
		agent_mins( o.agent_mins ),
		agent_maxs( o.agent_maxs ),
		start_node( o.start_node ),
		goal_node( o.goal_node ),
		nodes( o.nodes ),
		open_list( o.open_list ),
		node_lookup( o.node_lookup ),
		edge_validation_cache( o.edge_validation_cache ),
     refine_corridor( o.refine_corridor ),
		tile_route_storage( o.tile_route_storage ),
		tile_route_lookup( o.tile_route_lookup ),
		policy( o.policy ),
		pathProcess( o.pathProcess ),
		goal_index( o.goal_index ),
		max_nodes( o.max_nodes ),
		search_budget_ms( o.search_budget_ms ),
		step_start_ms( o.step_start_ms ),
		best_f_cost_seen( o.best_f_cost_seen ),
		stagnation_count( o.stagnation_count ),
		hit_stagnation_limit( o.hit_stagnation_limit ),
		hit_time_budget( o.hit_time_budget ),
		saw_vertical_neighbor( o.saw_vertical_neighbor ),
     node_expand_count( o.node_expand_count ),
		open_push_count( o.open_push_count ),
		open_pop_count( o.open_pop_count ),
		neighbor_try_count( o.neighbor_try_count ),
		no_node_count( o.no_node_count ),
		no_node_invalid_current_tile_count( o.no_node_invalid_current_tile_count ),
		no_node_target_tile_count( o.no_node_target_tile_count ),
		no_node_presence_count( o.no_node_presence_count ),
		no_node_cell_view_count( o.no_node_cell_view_count ),
		no_node_layer_select_count( o.no_node_layer_select_count ),
		tile_filter_reject_count( o.tile_filter_reject_count ),
		edge_reject_count( o.edge_reject_count ),
       occupancy_overlay_hit_count( o.occupancy_overlay_hit_count ),
		occupancy_soft_cost_hit_count( o.occupancy_soft_cost_hit_count ),
		occupancy_block_reject_count( o.occupancy_block_reject_count ),
       corridor_buffer_radius( o.corridor_buffer_radius ),
		corridor_buffered_tile_count( o.corridor_buffered_tile_count ),
		same_node_alias_count( o.same_node_alias_count ),
        closed_duplicate_count( o.closed_duplicate_count ),
		pass_through_prune_count( o.pass_through_prune_count ),
		edge_reject_reason_counts( o.edge_reject_reason_counts ) {
	  // Rebind pointer to our own storage (or nullptr)
		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
		// Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
	}

	// Copy assignment: copy containers and rebind pointer.
	nav_a_star_state_t &operator=( const nav_a_star_state_t &o ) {
		if ( this == &o ) {
			return * this;
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
		edge_validation_cache = o.edge_validation_cache;
      refine_corridor = o.refine_corridor;
		tile_route_storage = o.tile_route_storage;
		tile_route_lookup = o.tile_route_lookup;
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
      node_expand_count = o.node_expand_count;
		open_push_count = o.open_push_count;
		open_pop_count = o.open_pop_count;
		neighbor_try_count = o.neighbor_try_count;
		no_node_count = o.no_node_count;
		no_node_invalid_current_tile_count = o.no_node_invalid_current_tile_count;
		no_node_target_tile_count = o.no_node_target_tile_count;
		no_node_presence_count = o.no_node_presence_count;
		no_node_cell_view_count = o.no_node_cell_view_count;
		no_node_layer_select_count = o.no_node_layer_select_count;
		tile_filter_reject_count = o.tile_filter_reject_count;
		edge_reject_count = o.edge_reject_count;
        occupancy_overlay_hit_count = o.occupancy_overlay_hit_count;
		occupancy_soft_cost_hit_count = o.occupancy_soft_cost_hit_count;
		occupancy_block_reject_count = o.occupancy_block_reject_count;
        corridor_buffer_radius = o.corridor_buffer_radius;
		corridor_buffered_tile_count = o.corridor_buffered_tile_count;
		same_node_alias_count = o.same_node_alias_count;
		closed_duplicate_count = o.closed_duplicate_count;
		pass_through_prune_count = o.pass_through_prune_count;
		edge_reject_reason_counts = o.edge_reject_reason_counts;

		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
	 // Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
		return * this;
	}

	// Move constructor: move containers and rebind pointer into destination.
	nav_a_star_state_t( nav_a_star_state_t &&o ) noexcept
		: status( o.status ),
		mesh( o.mesh ),
		agent_mins( std::move( o.agent_mins ) ),
		agent_maxs( std::move( o.agent_maxs ) ),
		start_node( std::move( o.start_node ) ),
		goal_node( std::move( o.goal_node ) ),
		nodes( std::move( o.nodes ) ),
		open_list( std::move( o.open_list ) ),
		node_lookup( std::move( o.node_lookup ) ),
		edge_validation_cache( std::move( o.edge_validation_cache ) ),
        refine_corridor( std::move( o.refine_corridor ) ),
		tile_route_storage( std::move( o.tile_route_storage ) ),
		tile_route_lookup( std::move( o.tile_route_lookup ) ),
		policy( o.policy ),
		pathProcess( o.pathProcess ),
		goal_index( o.goal_index ),
		max_nodes( o.max_nodes ),
		search_budget_ms( o.search_budget_ms ),
		step_start_ms( o.step_start_ms ),
		best_f_cost_seen( o.best_f_cost_seen ),
		stagnation_count( o.stagnation_count ),
		hit_stagnation_limit( o.hit_stagnation_limit ),
		hit_time_budget( o.hit_time_budget ),
		saw_vertical_neighbor( o.saw_vertical_neighbor ),
     node_expand_count( o.node_expand_count ),
		open_push_count( o.open_push_count ),
		open_pop_count( o.open_pop_count ),
		neighbor_try_count( o.neighbor_try_count ),
		no_node_count( o.no_node_count ),
		no_node_invalid_current_tile_count( o.no_node_invalid_current_tile_count ),
		no_node_target_tile_count( o.no_node_target_tile_count ),
		no_node_presence_count( o.no_node_presence_count ),
		no_node_cell_view_count( o.no_node_cell_view_count ),
		no_node_layer_select_count( o.no_node_layer_select_count ),
		tile_filter_reject_count( o.tile_filter_reject_count ),
		edge_reject_count( o.edge_reject_count ),
       occupancy_overlay_hit_count( o.occupancy_overlay_hit_count ),
		occupancy_soft_cost_hit_count( o.occupancy_soft_cost_hit_count ),
		occupancy_block_reject_count( o.occupancy_block_reject_count ),
       corridor_buffer_radius( o.corridor_buffer_radius ),
		corridor_buffered_tile_count( o.corridor_buffered_tile_count ),
		same_node_alias_count( o.same_node_alias_count ),
        closed_duplicate_count( o.closed_duplicate_count ),
		pass_through_prune_count( o.pass_through_prune_count ),
		edge_reject_reason_counts( o.edge_reject_reason_counts ) {
	  // Rebind pointer to our own storage (or nullptr)
		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;
		// Debug assertion: ensure non-owning pointer either null or points into our storage.
		Q_assert( tileRouteFilter == nullptr || tileRouteFilter == &tile_route_storage );
		// Leave moved-from object in a safe state.
		o.tileRouteFilter = nullptr;
		o.mesh = nullptr;
	}

	// Move assignment: move containers then rebind pointer.
	nav_a_star_state_t &operator=( nav_a_star_state_t &&o ) noexcept {
		if ( this == &o ) {
			return * this;
		}
		status = o.status;
		mesh = o.mesh;
		agent_mins = std::move( o.agent_mins );
		agent_maxs = std::move( o.agent_maxs );
		start_node = std::move( o.start_node );
		goal_node = std::move( o.goal_node );
		nodes = std::move( o.nodes );
		open_list = std::move( o.open_list );
		node_lookup = std::move( o.node_lookup );
		edge_validation_cache = std::move( o.edge_validation_cache );
     refine_corridor = std::move( o.refine_corridor );
		tile_route_storage = std::move( o.tile_route_storage );
		tile_route_lookup = std::move( o.tile_route_lookup );
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
      node_expand_count = o.node_expand_count;
		open_push_count = o.open_push_count;
		open_pop_count = o.open_pop_count;
		neighbor_try_count = o.neighbor_try_count;
		no_node_count = o.no_node_count;
		no_node_invalid_current_tile_count = o.no_node_invalid_current_tile_count;
		no_node_target_tile_count = o.no_node_target_tile_count;
		no_node_presence_count = o.no_node_presence_count;
		no_node_cell_view_count = o.no_node_cell_view_count;
		no_node_layer_select_count = o.no_node_layer_select_count;
		tile_filter_reject_count = o.tile_filter_reject_count;
		edge_reject_count = o.edge_reject_count;
        occupancy_overlay_hit_count = o.occupancy_overlay_hit_count;
		occupancy_soft_cost_hit_count = o.occupancy_soft_cost_hit_count;
		occupancy_block_reject_count = o.occupancy_block_reject_count;
        corridor_buffer_radius = o.corridor_buffer_radius;
		corridor_buffered_tile_count = o.corridor_buffered_tile_count;
		same_node_alias_count = o.same_node_alias_count;
		closed_duplicate_count = o.closed_duplicate_count;
		pass_through_prune_count = o.pass_through_prune_count;
		edge_reject_reason_counts = o.edge_reject_reason_counts;

		tileRouteFilter = tile_route_storage.empty() ? nullptr : &tile_route_storage;

		// Reset moved-from object to safe state.
		o.tileRouteFilter = nullptr;
		o.mesh = nullptr;
		return * this;
	}
};

/** 
*  @brief    Initialize incremental A*  state ready for step-wise expansion.
*  @param    state        [in,out] Working state.
*  @param    mesh         Navigation mesh pointer (may be nullptr until generated).
*  @param    start_node   Starting node reference.
*  @param    goal_node    Goal node reference.
*  @param    agent_mins   Bbox mins used for traversal validation.
*  @param    agent_maxs   Bbox maxs used for traversal validation.
*  @param    policy       Traversal policy tuning.
* @param    refineCorridor  Optional bounded refinement corridor to restrict expansion.
*  @param    pathProcess  Optional owning path process for failure penalties.
*  @return   True when initialization succeeded.
* @note     Copies the provided corridor and configures the per-call budgets so
*            subsequent `Nav_AStar_Step` calls honor the intended limits.
**/
bool Nav_AStar_Init( nav_a_star_state_t * state, const nav_mesh_t * mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs,
  const svg_nav_path_policy_t * policy, const nav_refine_corridor_t * refineCorridor,
	const svg_nav_path_process_t * pathProcess );

/** 
*  @brief    Advance the search by the specified number of node expansions.
*  @param    state            Working incremental state.
*  @param    expansions       Maximum node expansions to perform this call.
*  @return   Current A*  status (running/completed/failed).
*  @note     Enforces the configured per-call expansion budget, node cap, and failure
*            heuristics before returning so long-running searches progress incrementally.
**/
nav_a_star_status_t Nav_AStar_Step( nav_a_star_state_t * state, int32_t expansions );

/** 
*  @brief    Finalize the path if A*  completed and output the resulting point list.
*  @param    state        Finalized state (must be Completed).
*  @param    out_points   [out] Vector to fill with ordered path points.
*  @return   True on success.
*  @note     Walks the parent chain from the solved node to emit ordered waypoints; fails if
*            the state is not completed.
**/
const bool Nav_AStar_Finalize( nav_a_star_state_t * state, std::vector<Vector3> * out_points );

/** 
*  @brief    Try to replace a node with a same-cell layer variant that has better local connectivity.
*  @param    mesh         Navigation mesh.
*  @param    seed_node    Initially resolved node in the target cell.
*  @param    policy       Traversal policy used for neighbor compatibility checks.
*  @param    out_node     [out] Best connected same-cell node variant.
*  @return   True when a same-cell candidate was evaluated successfully.
*  @note     Used to rescue start/goal layer picks that land on a locally isolated layer inside a
*            multi-layer cell even though another layer in the same XY cell has adjacent connectivity.
**/
bool Nav_AStar_TrySelectConnectedSameCellLayer( const nav_mesh_t * mesh, const nav_node_ref_t &seed_node,
	const svg_nav_path_policy_t * policy, nav_node_ref_t * out_node );

/** 
*  @brief    Reset incremental state before reuse or destruction.
*  @note     Clears the node/open containers, diagnostic counters, and returns status to Idle.
**/
void Nav_AStar_Reset( nav_a_star_state_t * state );
