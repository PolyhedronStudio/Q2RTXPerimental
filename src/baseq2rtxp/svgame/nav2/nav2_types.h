/********************************************************************
*
*
*	ServerGame: Nav2 Foundational Types and Constants
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Core Constants:
*
*
**/
//! Sentinel used when a tile or leaf has not been assigned to any hierarchy region yet.
static constexpr int32_t NAV_REGION_ID_NONE = -1;
//! Sentinel used when a region does not yet reference a valid portal id.
static constexpr int32_t NAV_PORTAL_ID_NONE = -1;
//! Deterministic coarse region budget used to split connected tile space into portal-bearing static regions.
static constexpr int32_t NAV_HIERARCHY_MAX_TILES_PER_REGION = 16;
//! Regions at or above this size are logged as coarse partition saturation points.
static constexpr int32_t NAV_HIERARCHY_OVERSIZED_REGION_TILE_COUNT = NAV_HIERARCHY_MAX_TILES_PER_REGION;
//! Number of persisted per-layer horizontal edge slots.
static constexpr int32_t NAV_LAYER_EDGE_DIR_COUNT = 8;


/**
*	Configurative Default Constants:
**/
#include "svgame/nav2/nav2_default_consts.h"


/**
*
*
*	Foundational Enumerations:
*
*
**/
/**
*	@brief	Layer bitflags describing per-layer traversal / surface properties.
*	@note	These values are simple bitflags (1<<N) and are intended to be combined
*			using bitwise operators (e.g., `flags & NAV_FLAG_WALKABLE`). They describe
*			surface/material characteristics at the layer granularity.
**/
typedef enum {
	//! Walkable surface (agent may attempt to traverse this layer).
	NAV_FLAG_WALKABLE = ( 1 << 0 ),
	//! Water surface. May affect traversal policy, speed, or require swimming.
	NAV_FLAG_WATER = ( 1 << 1 ),
	//! Lava surface. Typically damaging and/or non-traversable for many agents.
	NAV_FLAG_LAVA = ( 1 << 2 ),
	//! Slime surface. May alter movement and damage rules.
	NAV_FLAG_SLIME = ( 1 << 3 ),
	//! Layer contains a ladder surface. Ladder traversal requires endpoint semantics.
	NAV_FLAG_LADDER = ( 1 << 4 ),
} nav2_layer_flags_t;

/**
*	@brief	Traversal feature bits describing localized navigational affordances.
*	@note	Each bit represents a detected traversal feature for a cell/layer.
*			Use these bits when evaluating candidate node expansions, ladder
*			selection, or special-case traversal rules.
**/
enum nav2_traversal_feature_bits_t : uint32_t {
	//! No traversal features present.
	NAV_TRAVERSAL_FEATURE_NONE = 0,
	//! Ladder geometry present at this location (affords climb).
	NAV_TRAVERSAL_FEATURE_LADDER = ( 1u << 0 ),
	//! Water feature present (affects traversal costs/validity).
	NAV_TRAVERSAL_FEATURE_WATER = ( 1u << 1 ),
	//! Lava feature present (usually blocked or high-cost).
	NAV_TRAVERSAL_FEATURE_LAVA = ( 1u << 2 ),
	//! Slime feature present (special traversal handling).
	NAV_TRAVERSAL_FEATURE_SLIME = ( 1u << 3 ),
	//! Presence of a stair-like surface (series of small step-ups).
	NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE = ( 1u << 4 ),
	//! Presence of a jump obstruction that may require a jump or avoidance.
	NAV_TRAVERSAL_FEATURE_JUMP_OBSTRUCTION_PRESENCE = ( 1u << 5 ),
	//! Adjacent to a wall (useful for corner/clearance rules).
	NAV_TRAVERSAL_FEATURE_WALL_ADJACENCY = ( 1u << 6 ),
	//! Adjacent to a ledge (used to detect potential walk-off hazards).
	NAV_TRAVERSAL_FEATURE_LEDGE_ADJACENCY = ( 1u << 7 ),
	//! This location is the bottom/startpoint of a ladder.
	NAV_TRAVERSAL_FEATURE_LADDER_STARTPOINT = ( 1u << 8 ),
	//! This location is the top/endpoint of a ladder.
	NAV_TRAVERSAL_FEATURE_LADDER_ENDPOINT = ( 1u << 9 )
};


/**
 *
 *
 *	Runtime Helper Types:
 *
 *
 **/
/**
 *	@brief	Sparse dynamic occupancy entry used to represent local actor blocking or soft-costs.
 *	@note*	prefers a sparse occupancy map keyed by (tile,cell,layer). This lightweight entry
 *		captures whether a cell is hard-blocked and a soft-cost contribution used by path heuristics.
 **/
struct nav2_occupancy_entry_t {
	//! True when this occupancy entry is a hard blocker and should prevent traversal.
	bool hard_block = false;
	//! Soft cost contribution applied during neighbor expansion for this entry.
	double soft_cost = 0.0;
	//! Optional stamp/frame id when this entry was last updated.
	int64_t stamp_frame = -1;
};


/**
 *	@brief	Local portal overlay entry used for dynamic invalidation without rebuilding the hierarchy.
 *	@note	This mirrors the conceptual overlay used by the legacy runtime but keeps ownership in nav2.
 **/
struct nav2_portal_overlay_t {
	//! Overlay flags (bitmask user-defined by higher-level systems).
	uint32_t flags = 0;
	//! Additional traversal penalty applied when the portal remains usable.
	double additional_traversal_cost = 0.0;
};


/**
 *	@brief	Lightweight runtime descriptor for an inline-model mesh owned by the nav mesh.
 *	@note	This holds the minimal runtime fields necessary for query and membership resolution.
 **/
struct nav2_inline_model_runtime_t {
	//! Runtime index of the inline-model within mesh-owned arrays, or -1 when unused.
	int32_t model_index = -1;
	//! Owning entity number when the inline-model is associated with a game entity.
	int32_t owner_entnum = -1;
	//! World-space origin for the inline-model runtime entry.
	Vector3 origin = { 0., 0., 0. };
	//! Euler angles for the inline-model runtime entry.
	Vector3 angles = { 0., 0., 0. };
	//! Dirty flag indicating the runtime transform or geometry changed and requires rebuild.
	bool dirty = false;
};


/**
*	@brief	Edge feature bits describing properties of connectivity between nodes.
*	@note	Edges are annotated with these bits during nav-generation to guide
*			region traversal, A* expansion rules, and policy decisions (walk-off,
*			jump, ladder transitions, etc.).
**/
enum nav2_edge_feature_bits_t : uint32_t {
	//! No special features on this edge.
	NAV_EDGE_FEATURE_NONE = 0,
	//! Edge is passable in normal traversal.
	NAV_EDGE_FEATURE_PASSABLE = ( 1u << 0 ),
	//! Edge represents an upward stair step (small step up).
	NAV_EDGE_FEATURE_STAIR_STEP_UP = ( 1u << 1 ),
	//! Edge represents a downward stair step (small step down).
	NAV_EDGE_FEATURE_STAIR_STEP_DOWN = ( 1u << 2 ),
	//! Edge crosses or follows ladder geometry (ladder traversal transition).
	NAV_EDGE_FEATURE_LADDER_PASS = ( 1u << 3 ),
	//! Traversal through this edge enters water.
	NAV_EDGE_FEATURE_ENTERS_WATER = ( 1u << 4 ),
	//! Traversal through this edge enters lava.
	NAV_EDGE_FEATURE_ENTERS_LAVA = ( 1u << 5 ),
	//! Traversal through this edge enters slime.
	NAV_EDGE_FEATURE_ENTERS_SLIME = ( 1u << 6 ),
	//! Walk-off on this edge is allowed but optional (preferred to remain on surface).
	NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF = ( 1u << 7 ),
	//! Walk-off on this edge is forbidden unless policy overrides it.
	NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF = ( 1u << 8 ),
	//! Edge is blocked by a jump obstruction (may require special handling).
	NAV_EDGE_FEATURE_JUMP_OBSTRUCTION = ( 1u << 9 ),
	//! Edge is blocked by a hard wall (impassable for non-destructive agents).
	NAV_EDGE_FEATURE_HARD_WALL_BLOCKED = ( 1u << 10 )
};

/**
*	@brief	Coarse tile summary bits describing aggregated properties of a tile.
*	@note	Tile summaries provide a quick, cache-friendly overview used by
*			coarse routing and early rejection in path queries.
**/
enum nav2_tile_summary_bits_t : uint32_t {
	//! No notable summary features.
	NAV_TILE_SUMMARY_NONE = 0,
	//! Tile contains stair geometry.
	NAV_TILE_SUMMARY_STAIR = ( 1u << 0 ),
	//! Tile contains water.
	NAV_TILE_SUMMARY_WATER = ( 1u << 1 ),
	//! Tile contains lava.
	NAV_TILE_SUMMARY_LAVA = ( 1u << 2 ),
	//! Tile contains slime.
	NAV_TILE_SUMMARY_SLIME = ( 1u << 3 ),
	//! Tile includes ladder geometry.
	NAV_TILE_SUMMARY_LADDER = ( 1u << 4 ),
	//! Tile contains potential walk-off locations (risk of dropping).
	NAV_TILE_SUMMARY_WALK_OFF = ( 1u << 5 ),
	//! Tile contains hard wall obstacles (non-traversable surface).
	NAV_TILE_SUMMARY_HARD_WALL = ( 1u << 6 )
};

/**
*	@brief	Endpoint bitflags used to label ladder endpoints.
*	@note	Stored in a small-width type since only a few flags are needed.
**/
enum nav2_ladder_endpoint_bits_t : uint8_t {
	//! No ladder endpoint flags set.
	NAV_LADDER_ENDPOINT_NONE = 0,
	//! Marks the ladder bottom / startpoint.
	NAV_LADDER_ENDPOINT_STARTPOINT = ( 1u << 0 ),
	//! Marks the ladder top / endpoint.
	NAV_LADDER_ENDPOINT_ENDPOINT = ( 1u << 1 )
};


/**
*
*
*	Foundational Data Structures:
*
*
**/
/**
*	@brief	Describes the collision bounds and traversal limits for a nav2-capable agent.
*	@note	The bounds are stored as local-space AABB extents, while the remaining fields
*			describe the maximum vertical and slope constraints the agent can safely traverse.
**/
struct nav2_agent_profile_t {
	//! Local-space minimum collision bounds relative to the agent origin.
	Vector3 mins = {};
	//! Local-space maximum collision bounds relative to the agent origin.
	Vector3 maxs = {};
	//! Maximum step-up height the agent can traverse without jumping.
	double max_step_height = 0.0;
	//! Maximum downward drop height the agent is allowed to traverse.
	double max_drop_height = 0.0;
	//! Additional cap used when rejecting overly large downward transitions.
	double max_drop_height_cap = 0.0;
	//! Minimum acceptable surface normal `z` component for walkable slope classification.
	double max_slope_normal_z = 0.0;
};



/**
*
*
*	Tile Cluster Data Structures:
*
*
**/
/**
*	@brief	Identifies a tile-cluster bucket by its 2D cluster-grid coordinates.
*	@note	This key represents a coarser grouping than an individual tile and is suitable
*			for chunked storage, caching, or hierarchy-oriented indexing.
**/
struct nav2_tile_cluster_key_t {
	//! Cluster coordinate on the X axis.
	int32_t tile_x = 0;
	//! Cluster coordinate on the Y axis.
	int32_t tile_y = 0;

	/**
	*	@brief	Compare two tile-cluster keys for exact coordinate equality.
	*	@param	o	Other tile-cluster key to compare against.
	*	@return	True when both cluster coordinates match.
	**/
	bool operator==( const nav2_tile_cluster_key_t &o ) const {
		return tile_x == o.tile_x && tile_y == o.tile_y;
	}
};

/**
*	@brief	Hash functor for `nav2_tile_cluster_key_t`.
*	@note	Uses the same coordinate-mixing strategy as the world-tile hash so cluster keys
*			can be used directly in hash-based containers.
**/
struct nav2_tile_cluster_key_hash_t {
	/**
	*	@brief	Compute the hash value for a tile-cluster key.
	*	@param	k	Key to hash.
	*	@return	Combined hash of the X and Y cluster coordinates.
	**/
	size_t operator()( const nav2_tile_cluster_key_t &k ) const {
		size_t seed = std::hash<int32_t>{}( k.tile_x );
		seed ^= std::hash<int32_t>{}( k.tile_y ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};



/**
*
*
*	Tile Data Structures:
*
*
**/
/**
*	@brief	Canonical nav2-owned tile descriptor used by nav2_mesh_t and runtime subsystems.
*	@note	This structure is intentionally compact: it documents tile identity, coarse summaries,
*		and minimal bounding information needed by higher-level routing/hierarchy code. More
*		detailed per-cell/layer storage remains in separate mesh-owned buffers.
**/
struct nav2_tile_t {
//! World tile grid X coordinate.
	int32_t tile_x = 0;
	//! World tile grid Y coordinate.
	int32_t tile_y = 0;
	//! Stable world-tile id when the tile is stored in a canonical container; -1 when unset.
	int32_t tile_id = -1;
	//! Coarse hierarchy region id this tile belongs to, or NAV_REGION_ID_NONE when unassigned.
	int32_t region_id = NAV_REGION_ID_NONE;
	//! Bitmask of nav2_tile_summary_bits_t describing coarse per-tile features.
	uint32_t traversal_summary_bits = NAV_TILE_SUMMARY_NONE;
	//! Bitmask summarizing common edge properties present in the tile.
	uint32_t edge_summary_bits = 0;
	//! World-space axis-aligned bounding box for the tile. Helpful for debug, culling, and coarse queries.
	BBox3 world_bounds = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	//! Coarse cluster key for chunked or tiled storage. Mirrors nav2_tile_cluster_key_t semantics.
	nav2_tile_cluster_key_t cluster_key = {};

	/**
	*	@brief	Return whether this tile references a canonical stored tile id.
	*	@return	True when tile_id is non-negative.
	**/
	const bool IsValid() const {
		return tile_id >= 0;
	}
};


/**
*	@brief	Identifies a world-space nav tile by its 2D tile-grid coordinates.
*	@note	This key is intended for associative containers that index globally addressed
*			tiles independent of any finer cell or layer selection.
**/
struct nav2_world_tile_key_t {
	//! World tile coordinate on the X axis.
	int32_t tile_x = 0;
	//! World tile coordinate on the Y axis.
	int32_t tile_y = 0;

	/**
	*	@brief	Compare two world-tile keys for exact coordinate equality.
	*	@param	o	Other world-tile key to compare against.
	*	@return	True when both tile coordinates match.
	**/
	bool operator==( const nav2_world_tile_key_t &o ) const {
		return tile_x == o.tile_x && tile_y == o.tile_y;
	}
};

/**
*	@brief	Hash functor for `nav2_world_tile_key_t`.
*	@note	Combines both coordinates with a standard golden-ratio style mix so the key can
*			be used efficiently in `std::unordered_map` and similar hash containers.
**/
struct nav2_world_tile_key_hash_t {
	/**
	*	@brief	Compute the hash value for a world-tile key.
	*	@param	k	Key to hash.
	*	@return	Combined hash of the X and Y tile coordinates.
	**/
	size_t operator()( const nav2_world_tile_key_t &k ) const {
		size_t seed = std::hash<int32_t>{}( k.tile_x );
		seed ^= std::hash<int32_t>{}( k.tile_y ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};



/**
*
*
*	Node Structures:
*
*
**/
/**
*	@brief	Composite identifier for a concrete nav node within the generated nav data.
*	@note	The key spans the BSP leaf, tile, cell, and layer indices so callers can
*			address a unique traversal node without storing a raw pointer.
**/
struct nav2_node_key_t {
	//! Owning BSP leaf index for the node, or `-1` when unresolved.
	int32_t leaf_index = -1;
	//! Owning tile index within the leaf, or `-1` when unresolved.
	int32_t tile_index = -1;
	//! Owning cell index within the tile, or `-1` when unresolved.
	int32_t cell_index = -1;
	//! Owning layer index within the cell, or `-1` when unresolved.
	int32_t layer_index = -1;

	/**
	*	@brief	Compare two node keys for exact identity.
	*	@param	other	Other node key to compare against.
	*	@return	True when all hierarchy indices refer to the same node.
	**/
	bool operator==( const nav2_node_key_t &other ) const {
		return leaf_index == other.leaf_index
			&& tile_index == other.tile_index
			&& cell_index == other.cell_index
			&& layer_index == other.layer_index;
	}
};

/**
*	@brief	Hash functor for `nav2_node_key_t`.
*	@note	Mixes every hierarchy index into one seed so fully qualified node keys can be
*			stored in hash tables without flattening them into a separate integer id.
**/
struct nav2_node_key_hash_t {
	/**
	*	@brief	Compute the hash value for a composite node key.
	*	@param	key	Node key to hash.
	*	@return	Combined hash across leaf, tile, cell, and layer indices.
	**/
	size_t operator()( const nav2_node_key_t &key ) const {
		size_t seed = std::hash<int32_t>{}( key.leaf_index );
		seed ^= std::hash<int32_t>{}( key.tile_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		seed ^= std::hash<int32_t>{}( key.cell_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		seed ^= std::hash<int32_t>{}( key.layer_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

/**
*	@brief	References a nav node together with a representative world-space position.
*	@note	This pairs the stable structural identity of a node with a concrete point that
*			callers can use for query results, debugging, or path reconstruction.
**/
struct nav2_node_ref_t {
	//! Composite structural key that identifies the referenced nav node.
	nav2_node_key_t key = {};
	//! Representative world-space position associated with the node reference.
	Vector3 worldPosition = { 0., 0., 0. };
};



/**
*
*
*	Pathing Structures:
* 
* 
**/
struct nav2_query_process_t;
struct nav2_query_policy_t;

/**
* 	@brief	Policy for path processing and follow behavior.
**/
struct nav2_path_policy_t {
	/**
	* 	Timers and backoff exponent:
	**/
	//! Minimum time interval between path rebuild attempts.
	QMTime rebuild_interval = 0_ms;
	 //! Base time for exponential backoff on path rebuild failures.
	QMTime fail_backoff_base = 25_ms;
	//! Maximum exponent for backoff time (2^n).
	int32_t fail_backoff_max_pow = 4;

	/**
	* 	Ignore/Allow States:
	**/
	//! If true, ignore visibility when deciding whether to rebuild the path. 
	//! This can be useful for testing and debugging, but generally we want the monster to 
	//! try to rebuild more aggressively when it can see the player.
	bool ignore_visibility = true;
	//! If true, ignore distance in front of the monster when deciding whether to rebuild the path.
	bool ignore_infront = true;

	/**
	* 	Goal and Waypoint parameters:
	**/
	//! Radius around waypoints to consider 'reached'.
	double waypoint_radius = NAV_DEFAULT_WAYPOINT_RADIUS;

	//! 2D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_2d = NAV_DEFAULT_GOAL_REBUILD_2D_DISTANCE;
	//! 3D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_3d = NAV_DEFAULT_GOAL_REBUILD_3D_DISTANCE; // 48* 48 = 2304

	/**
	*	Goal Z-layer blending controls:
	*		When start or goal selection lands in an XY location that contains multiple
	*		valid nav layers, the selector can derive a blended "desired Z" instead of
	*		using only the local sample height.
	*
	*		This acts as a soft hint during start/goal node selection so longer horizontal
	*		paths can progressively favor layers whose elevation better matches the final
	*		objective. In practice, this helps the path builder commit earlier to stairs,
	*		ramps, or other vertical connectors when the target is known to be above or
	*		below the current floor.
	*
	*		Behavior is distance-driven:
	*		- At or below `blend_start_dist`, selection remains anchored to the local
	*		  start/query height.
	*		- Between `blend_start_dist` and `blend_full_dist`, the desired Z is blended
	*		  toward the goal height.
	*		- At or beyond `blend_full_dist`, selection is fully biased to the goal Z.
	*
	*		Disable this only for targeted diagnostics or when comparing against purely
	*		local closest-layer selection.
	**/
	//! If true, apply horizontal-distance-based blending from local/query Z toward goal Z during layer selection.
	bool enable_goal_z_layer_blend = true;
	//! Horizontal distance threshold where goal-height influence begins; shorter ranges keep selection fully local.
	double blend_start_dist = NAV_DEFAULT_BLEND_DIST_START;
	//! Horizontal distance threshold where the desired selection Z becomes fully goal-biased.
	double blend_full_dist = NAV_DEFAULT_BLEND_DIST_FULL;

	/**
	*	Cluster-route pre-pass controls:
	*		The async request worker can first derive a coarse long-range tile/cluster
	*		route from the navigation hierarchy and then use that route as an allow-list
	*		for fine-grained A* expansion.
	*
	*		This keeps long-distance searches bounded on large or open maps by reducing
	*		unnecessary node expansion outside the expected corridor, while still leaving
	*		final traversal validity to the authoritative fine traversal search.
	*
	*		Disable this only for targeted diagnostics, benchmarking, or direct comparison
	*		against unrestricted fine refinement behavior.
	**/
	//! If true, apply the hierarchical tile-route filter during async A* prep.
	bool enable_cluster_route_filter = true;

	/**
	* 	Traversal policy controls derived from persisted edge and feature metadata.
	**/
	//! If true, forbid edges that explicitly enter water.
	bool forbid_water = true;
	//! If true, forbid edges that explicitly enter lava.
	bool forbid_lava = true;
	//! If true, forbid edges that explicitly enter slime.
	bool forbid_slime = true;
	//! If true, allow explicitly optional walk-off edges.
	bool allow_optional_walk_off = false;
	//! If true, allow explicitly forbidden walk-off edges.
	bool allow_forbidden_walk_off = false;
   //! If true, allow async A*  to skip long-hop probes that can already be predicted as passed-through one-cell chains.
	bool enable_pass_through_pruning = true;
	//! If true, prefer ladders when they reach the same or a relatively close goal height more directly.
	bool prefer_ladders = true;
	//! Cost bonus applied to ladder-favoring transitions when ladder preference is active.
	double ladder_preference_bias = 12.0;
	//! Maximum goal-height slack within which ladder endpoints are considered meaningfully close to the target height.
	double ladder_preferred_height_slack = 96.0;
	//! If true, consult sparse dynamic occupancy during async neighbor expansion.
	bool use_dynamic_occupancy = true;
	//! If true, occupancy-marked cells remain hard blockers; otherwise they are converted into additional soft cost.
	bool hard_block_dynamic_occupancy = false;
	//! Additional scale applied to sparse occupancy soft costs when occupancy participation is enabled.
	double dynamic_occupancy_soft_cost_scale = 1.0;

	/**
	* 	For obstalce step handling.
	**/
	//! Minimal nnormal angle that will be stepped over.
	double min_step_normal = PHYS_MAX_SLOPE_NORMAL;
	//! Minimum step height that is considered as and can be stepped over.
	double min_step_height = PHYS_STEP_MIN_SIZE;
	//! Maximum step height that can be stepped over (matches `nav_max_step`).
	double max_step_height = PHYS_STEP_MAX_SIZE;

	/**
	* 	For obstacle Jump handling:
	**/
	//! If true, try to perform a small obstacle jump when blocked.
	bool allow_small_obstruction_jump = true;
	//! Max obstruction height allowed to jump over.
	double max_obstruction_jump_height = NAV_DEFAULT_MAX_OBSTRUCTION_JUMP_SIZE;

	/**
	* 	For fall-safety:
	*
	* 	max_drop_height should be smaller than (or equal to) the cap if the cap is an absolute extremity limiter.
	* 	Two practical choices depending on your intent:
	* 	- If you want navigation to avoid any damaging falls: set cap < ~187.6 (e.g. 180) so the nav system will never accept a drop that can hurt.
	* 	- If you want to allow risky/damaging drops but still bound them: leave cap ≈ 192 (or slightly above) and keep max_drop_height as a conservative preferred threshold (e.g. 48..96).
	**/
	//! If true, do not allow moving into a drop deeper than max_drop_height.
	bool enable_max_drop_height_cap = true;
	//! Max allowed drop height (units, matches `nav_max_drop`).
	double max_drop_height = NAV_DEFAULT_MAX_DROP_HEIGHT;
	//! Drop cap applied when rejecting large downward transitions (matches `nav_max_drop_height_cap`).
	double max_drop_height_cap = NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;

	/**
	* 	Agent navigation constraints derived from nav CVars.
	**/
	//! Agent bounding box minimum extents in feet-origin space (matches `nav_agent_mins_* `).
	Vector3 agent_mins = { -16.0f, -16.0f, -36.0f };
	//! Agent bounding box maximum extents in feet-origin space (matches `nav_agent_maxs_* `).
	Vector3 agent_maxs = { 16.0f, 16.0f, 36.0f };
	//! Minimum walkable surface normal Z threshold (matches `nav_max_slope_normal_z`).
	double max_slope_normal_z = PHYS_MAX_SLOPE_NORMAL;

	/**
	* 	Layer selection tuning:
	**/
	//! If true, prefer the highest layer in a multi-layer XY cell when the desired Z is "close".
	//!
	//! @note	This is intentionally a soft bias, not an absolute rule.
	//!		It only influences selection when multiple layers are considered acceptable
	//!		by the Z-tolerance gate (see `nav_z_tolerance` / derived default tolerance).
	//!		When the desired Z is far away from all layers, selection falls back to
	//!		closest-by-Z.
	bool layer_select_prefer_top = true;
	//! Z distance in world units under which we apply the top/bottom preference bias.
	//!
	//! @note	Larger values make selection "stick" to the preferred layer more often in
	//!			multi-layer cells (stairs / overlapping floors). Smaller values allow selection
	//!			to follow the numeric closest-by-Z more aggressively.
	//! @note	Default is tuned to be slightly above a typical stair riser so the selector
	//!			remains stable on stairs while still allowing large Z deltas to select the
	//!			correct floor.
	double layer_select_prefer_z_threshold = NAV_DEFAULT_STEP_MAX_SIZE + NAV_DEFAULT_STEP_MIN_SIZE;
};

struct nav_traversal_path_t {

};
/**
* 	@brief	Per-entity navigation path processing state.
* 	@note	Embed this in an entity to add path following behavior.
**/
struct svg_nav_path_process_t {
	/**
	*
	*
	*
	* 	Core:
	*
	*
	*
	**/
	#if 0
	//! Current traversal path.
	nav_traversal_path_t path = {};

	//! Current index along the path.
	int32_t path_index = 0;
	//! Last path goal and start positions.
	Vector3 path_goal_position = {};
	//! Start position used for the current path.
	Vector3 path_start_position = {};
	//! Next time at which a path rebuild can be attempted.
	QMTime next_rebuild_time = 0_ms;
	//! Time until which rebuild attempts are backed off due to failures.
	QMTime backoff_until = 0_ms;
	//! Number of consecutive path rebuild failures.
	int32_t consecutive_failures = 0;
	//! Time of the last rebuild failure (used to bias A*  away from recent failing paths).
	QMTime last_failure_time = 0_ms;
	//! Last failing goal position (world-space) recorded when a rebuild failed.
	Vector3 last_failure_pos = {};
	//! Last failing direction/yaw recorded when a rebuild failed.
	float last_failure_yaw = 0.0f;
	//! Indicates whether an asynchronous rebuild request is currently outstanding.
	bool rebuild_in_progress = false;
	//! Stores the handle of the pending async rebuild request for cancellation/logging.
	int32_t pending_request_handle = 0;
	//! Monotonic request generation used to discard stale async results when requests are replaced.
	uint32_t request_generation = 0;
	//! Time of the last async prep/enqueue attempt for this process (used to debounce repeated per-frame prep work).
	QMTime last_prep_time = 0_ms;
	//! Last prep/start position used when the last async prep was performed.
	Vector3 last_prep_start = {};
	//! Last prep/goal position used when the last async prep was performed.
	Vector3 last_prep_goal = {};
	//! Z offset used to convert external feet-origin queries into internal nav-center space.
	float path_center_offset_z = 0.0f;

	/**
	* 	@brief	Policy for path processing and follow behavior.
	**/
	void Reset( void );
	/**
	* 	@brief	Commit a finalized async traversal path and reset failure tracking.
	* 	@param	points	Finalized nav-center waypoints produced by the async rebuild.
	* 	@param	start_origin	Agent start position in entity feet-origin space.
	* 	@param	goal_origin	Agent goal position in entity feet-origin space.
	* 	@param	center_offset_z	Z offset that converts feet-origin to nav-center coordinates for this path.
	* 	@param	policy	Path policy used to enforce rebuild throttles after commit.
	* 	@return	True when the path was stored successfully.
	**/
	const bool CommitAsyncPathFromPoints( const std::vector<Vector3> &points, const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy );



	/**
	*
	*
	*
	* 	Path (Re-)Building:
	*
	*
	*
	**/
	/**
	* 	@brief	Rebuild the path to the given goal from the given start, using the given agent
	* 			bounding box for traversal.
	**/
	const bool RebuildPathToWithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
		const Vector3 &agent_mins, const Vector3 &agent_maxs, const bool force = false );
	/**
	* 	@brief	Used to determine if a path rebuild can be attempted at this time.
	**/
	const bool CanRebuild( const svg_nav_path_policy_t &policy ) const;
	/**
	* 	@brief	Determine if the path should be rebuilt based on the goal's 2D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForGoal2D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	* 	@brief	Determine if the path should be rebuilt based on the goal's 3D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForGoal3D( const Vector3 &goal_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	* 	@brief	Determine if the path should be rebuilt based on the start's 2D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForStart2D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	* 	@brief	Determine if the path should be rebuilt based on the start's 3D distance change.
	* 	@return	True if the path should be rebuilt.
	**/
	const bool ShouldRebuildForStart3D( const Vector3 &start_origin, const svg_nav_path_policy_t &policy ) const;
	/**
	* 	@brief	Rebuild the path to the given goal from the given start.
	* 	@return	True if the path was successfully rebuilt.
	**/
	const bool RebuildPathTo( const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy );


	/**
	*
	*
	*
	* 	Path direction/movement querying:
	*
	*
	*
	**/
	/**
	* 	@brief	Shared follow-state bundle returned by `QueryFollowState`.
	* 	@note	Keeps waypoint-selection and stair-centering semantics inside navigation code so
	* 			monster callers can consume one reusable follow description.
	**/
	struct follow_state_t {
		//! True when `move_direction3d` contains a usable direction for this frame.
		bool has_direction = false;
		//! Normalized 3D direction returned by the shared path query.
		Vector3 move_direction3d = {};
		//! Active waypoint converted into feet-origin/entity space.
		Vector3 active_waypoint_origin = {};
		//! Horizontal distance from current origin to the active waypoint.
		double active_waypoint_dist2d = 0.0;
		//! Absolute vertical delta between current origin and active waypoint.
		double active_waypoint_delta_z = 0.0;
		//! True when the active waypoint should keep tighter stair/corner centering.
		bool waypoint_needs_precise_centering = false;
	  //! True when the current anchor sits inside a repeated step-sized vertical corridor.
		bool stepped_vertical_corridor_ahead = false;
		//! True when the current active waypoint is the final path point.
		bool approaching_final_goal = false;
	};
	/**
	* 	@brief	Query reusable follow state for the current path.
	* 	@param	current_origin	Current feet-origin position of the mover.
	* 	@param	policy		Path-follow policy controlling waypoint radius and stair limits.
	* 	@param	out_state	[out] Shared follow-state bundle for steering/velocity code.
	* 	@return	True when a valid follow direction was produced.
	* 	@note	This centralizes waypoint advancement, active-waypoint selection, and stair-anchor
	* 			classification so monster code does not duplicate those navigation decisions.
	**/
	const bool QueryFollowState( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, follow_state_t *out_state );
	/**
	* 	@brief	Query the next movement direction in 2D from the current origin along the path.
	**/
	const bool QueryDirection2D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir2d );
	/**
	* 	@brief	Query the next movement direction in 3D from the current origin along the path.
	* 	@param	current_origin	Current position of the agent.
	* 	@param	policy			Path policy (for waypoint radius).
	* 	@param	out_dir3d		[out] Resulting normalized 3D direction.
	* 	@return	True if a direction was found.
	**/
	const bool QueryDirection3D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir3d );

	/**
	*    @brief  Get the next path point converted into the entity's origin space (opposite of nav-center).
	*    @param  out_point   [out] Next path point in entity origin coordinates (feet-origin).
	*    @return True if a next point exists.
	**/
	const bool GetNextPathPointEntitySpace( Vector3 *out_point ) const;
	#endif
};



/**
*
*
*	Mesh Structure (stand-alone skeleton):
*
*
**/
/**
*	@brief	Stand-alone nav2 mesh container owned by the nav2 runtime.
*	@note	This is a conservative, minimal skeleton exposing only what has been implemented so far,
*			and will gradually expand as more of the nav2 data model and runtime functionality is implemented. 
*			The mesh owns the buffers for all runtime-relevant data, while query subsystems can hold non-owning
*			views or references to the data as needed. This mesh intentionally does not depend on any legacy types/structures.
**/
struct nav2_mesh_t {
	//! World-space AABB bounds of the mesh.
	BBox3 world_bounds = { { -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE }, { CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE } };

	//! Canonical world tiles owned by this mesh.
	std::vector<nav2_tile_t> world_tiles;

	//! Mapping from world tile key -> canonical tile id (index into world_tiles).
	std::unordered_map<nav2_world_tile_key_t, int32_t, nav2_world_tile_key_hash_t> world_tile_id_of;

	//! Sparse dynamic occupancy map keyed by a flattened tile/cell/layer id.
	std::unordered_map<uint64_t, nav2_occupancy_entry_t> occupancy;
	//! Frame id for which occupancy is currently stamped.
	int64_t occupancy_frame = -1;

	//! Inline-model runtime entries owned by this mesh.
	std::vector<nav2_inline_model_runtime_t> inline_model_runtime;

	//! Mesh sizing metadata mirrored for query consumers.
	int32_t tile_size = 0;        //!< Number of XY cells per tile edge.
	double cell_size_xy = 0.0;    //!< World-space XY size of one nav cell.
	double z_quant = 0.0;         //!< Quantization multiplier used for layer heights.

	//! Agent profile mirrored from the mesh generation step when known.
	nav2_agent_profile_t agent_profile = {};

	//! Load/version state for basic compatibility checks.
	bool loaded = false;
	//! Version of the serialized format used to load or save this mesh, for compatibility validation.
	uint32_t serialized_format_version = 0;

	/**
	*	@brief	Reset the mesh to an empty state, freeing owned buffers.
	**/
    void ResetMesh( void ) {
		world_tiles.clear();
		world_tile_id_of.clear();
		occupancy.clear();
		inline_model_runtime.clear();
		occupancy_frame = -1;
		tile_size = 0;
		cell_size_xy = 0.0;
		z_quant = 0.0;
		loaded = false;
		serialized_format_version = 0;
	}
};


//! RAII owner type for the nav2-owned runtime mesh container.
using nav2_mesh_raii_t = SVG_RAIIObject<nav2_mesh_t>;

//! Nav2-owned runtime mesh published through the runtime seam.
extern nav2_mesh_raii_t g_nav_mesh;