/******************************************************************** 
* 
* 
* 	SVGame: Navigation Voxelmesh Generator
* 
* 
********************************************************************/
#pragma once

//! Define to enable default cvar values for navigation debugging features. 
//! This is separate from the main debug define to allow selective enabling of debug features 
//! without incurring the overhead of all debug features (e.g., debug drawing).
#if USE_DEBUG
	#define _DEBUG_NAV_MESH 1
#endif

//! Forward declarations to minimize includes in this header, which is included widely for nav-related code.
struct svg_base_edict_t;
struct nav_mesh_t;
struct nav_occupancy_entry_t;

// Default parameters and constants for navigation mesh generation and traversal, defined as constant expressions 
// or via cvars for tuning. These are declared here for cross-TU usage.
#include "svgame/nav/svg_nav_const_defaults.h"

/**
*	
**/
//! Sentinel used when a tile or leaf has not been assigned to any hierarchy region yet.
static constexpr int32_t NAV_REGION_ID_NONE = -1;
//! Sentinel used when a region does not yet reference a valid portal id.
static constexpr int32_t NAV_PORTAL_ID_NONE = -1;

/**
*
**/
//! Deterministic coarse region budget used to split connected tile space into portal-bearing static regions.
static constexpr int32_t NAV_HIERARCHY_MAX_TILES_PER_REGION = 16;
//! Regions at or above this size are logged as coarse partition saturation points.
static constexpr int32_t NAV_HIERARCHY_OVERSIZED_REGION_TILE_COUNT = NAV_HIERARCHY_MAX_TILES_PER_REGION;



// CVars defined in svg_nav.cpp (declare extern for cross-TU usage)
extern cvar_t *nav_profile_level;
extern cvar_t *nav_astar_step_budget_ms;

extern cvar_t *nav_direct_los_attempt_max_samples;

extern cvar_t *nav_hierarchy_route_enable;
extern cvar_t *nav_hierarchy_route_min_distance;

extern cvar_t *nav_simplify_sync_max_los_tests;
extern cvar_t *nav_simplify_async_max_los_tests;
extern cvar_t *nav_simplify_sync_max_ms;
extern cvar_t *nav_simplify_long_span_min_distance;
extern cvar_t *nav_simplify_clearance_margin;
extern cvar_t *nav_simplify_collinear_angle_degrees;

extern cvar_t *nav_refine_corridor_mid_tiles;
extern cvar_t *nav_refine_corridor_far_tiles;
extern cvar_t *nav_refine_corridor_radius_near;
extern cvar_t *nav_refine_corridor_radius_mid;
extern cvar_t *nav_refine_corridor_radius_far;

extern cvar_t *nav_cell_size_xy;
extern cvar_t *nav_tile_size;
extern cvar_t *nav_z_quant;
extern cvar_t *nav_z_tolerance;

extern cvar_t *nav_max_step;
extern cvar_t *nav_max_drop;
extern cvar_t *nav_max_drop_height_cap;
extern cvar_t *nav_max_slope_normal_z;
extern cvar_t *nav_cost_los_max_vertical_delta;

extern cvar_t *nav_agent_mins_x;
extern cvar_t *nav_agent_mins_y;
extern cvar_t *nav_agent_mins_z;
extern cvar_t *nav_agent_maxs_x;
extern cvar_t *nav_agent_maxs_y;
extern cvar_t *nav_agent_maxs_z;

// Cost tuning CVars
extern cvar_t *nav_cost_w_dist;
extern cvar_t *nav_cost_jump_base;
extern cvar_t *nav_cost_jump_height_weight;
extern cvar_t *nav_cost_los_weight;
extern cvar_t *nav_cost_dynamic_weight;
extern cvar_t *nav_cost_failure_weight;
extern cvar_t *nav_cost_failure_tau_ms;
extern cvar_t *nav_cost_turn_weight;
extern cvar_t *nav_cost_slope_weight;
extern cvar_t *nav_cost_drop_weight;
extern cvar_t *nav_cost_goal_z_blend_factor;
extern cvar_t *nav_cost_min_cost_per_unit;

/**
*	@brief	Convert a world-space distance into nav cells for debugging instrumentation.
*	@param	mesh	Navigation mesh (provides accurate cell/world sizing).
*	@param	worldDistance	Distance in world units to convert.
*	@return	Floating-point cell count that distance spans.
*	@note	Returns 0 when the mesh/cell size cannot be determined.
**/
double SVG_Nav_DebugWorldDistanceToCellCount( const nav_mesh_t *mesh, double worldDistance );

/**
*	@brief	Convert a world-space distance into nav tiles for debugging instrumentation.
*	@param	mesh	Navigation mesh driving the conversion.
*	@param	worldDistance	Distance in world units to convert.
*	@return	Tile count corresponding to the provided distance.
**/
double SVG_Nav_DebugWorldDistanceToTileCount( const nav_mesh_t *mesh, double worldDistance );

/**
*	@brief	Convert a cvar value (with fallback) into nav cells for easier debugging of tuning knobs.
*	@param	mesh	Navigation mesh supplying grid sizing.
*	@param	cvar	Source cvar pointer (may be null).
*	@param	fallbackWorldUnits	Fallback world distance if the cvar is unset or non-positive.
*	@return	Cell count corresponding to the chosen distance.
**/
double SVG_Nav_DebugCvarValueToCellCount( const nav_mesh_t *mesh, const cvar_t *cvar, double fallbackWorldUnits );

/**
*	@brief	Convert a cvar value (with fallback) into nav tiles for easier debugging of tuning knobs.
*	@param	mesh	Navigation mesh supplying tile sizing.
*	@param	cvar	Source cvar pointer (may be null).
*	@param	fallbackWorldUnits	Fallback world distance if the cvar is unset or non-positive.
*	@return	Tile count corresponding to the chosen distance.
**/
double SVG_Nav_DebugCvarValueToTileCount( const nav_mesh_t *mesh, const cvar_t *cvar, double fallbackWorldUnits );

/** 
* 
* 
* 
*    Navigation Voxelmesh Data Structures:
* 
*    The voxelmesh is a sparse per-leaf tiled structure storing walkable
*    surface samples suitable for A*  pathfinding. Generation is bbox-independent
*    with configurable parameters via cvars.
* 
* 
* 
**/
/** 
*    @brief  Content flags for navigation layers.
*         Used to mark walkable surfaces and environmental hazards.
**/
typedef enum {
    NAV_FLAG_WALKABLE   = (1 << 0), //! Surface is walkable.
    NAV_FLAG_WATER      = (1 << 1), //! Surface is in water.
    NAV_FLAG_LAVA       = (1 << 2), //! Surface is in lava.
    NAV_FLAG_SLIME      = (1 << 3), //! Surface is in slime.
    NAV_FLAG_LADDER     = (1 << 4), //! Surface is a ladder.
} nav_layer_flags_t;

/** 
*    @brief  Traversal-oriented feature bits stored separately from legacy/content flags.
**/
enum nav_traversal_feature_bits_t : uint32_t {
	NAV_TRAVERSAL_FEATURE_NONE = 0,
	NAV_TRAVERSAL_FEATURE_LADDER = ( 1u << 0 ),
	NAV_TRAVERSAL_FEATURE_WATER = ( 1u << 1 ),
	NAV_TRAVERSAL_FEATURE_LAVA = ( 1u << 2 ),
	NAV_TRAVERSAL_FEATURE_SLIME = ( 1u << 3 ),
	NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE = ( 1u << 4 ),
	NAV_TRAVERSAL_FEATURE_JUMP_OBSTRUCTION_PRESENCE = ( 1u << 5 ),
	NAV_TRAVERSAL_FEATURE_WALL_ADJACENCY = ( 1u << 6 ),
	NAV_TRAVERSAL_FEATURE_LEDGE_ADJACENCY = ( 1u << 7 ),
	NAV_TRAVERSAL_FEATURE_LADDER_STARTPOINT = ( 1u << 8 ),
	NAV_TRAVERSAL_FEATURE_LADDER_ENDPOINT = ( 1u << 9 )
};

/** 
*    @brief  Edge metadata bits describing traversal behavior between neighboring cells.
**/
enum nav_edge_feature_bits_t : uint32_t {
	NAV_EDGE_FEATURE_NONE = 0,
	NAV_EDGE_FEATURE_PASSABLE = ( 1u << 0 ),
	NAV_EDGE_FEATURE_STAIR_STEP_UP = ( 1u << 1 ),
	NAV_EDGE_FEATURE_STAIR_STEP_DOWN = ( 1u << 2 ),
	NAV_EDGE_FEATURE_LADDER_PASS = ( 1u << 3 ),
	NAV_EDGE_FEATURE_ENTERS_WATER = ( 1u << 4 ),
	NAV_EDGE_FEATURE_ENTERS_LAVA = ( 1u << 5 ),
	NAV_EDGE_FEATURE_ENTERS_SLIME = ( 1u << 6 ),
	NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF = ( 1u << 7 ),
	NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF = ( 1u << 8 ),
	NAV_EDGE_FEATURE_JUMP_OBSTRUCTION = ( 1u << 9 ),
	NAV_EDGE_FEATURE_HARD_WALL_BLOCKED = ( 1u << 10 )
};

/** 
*    @brief  Coarse tile summary bits derived from fine node and edge metadata.
**/
enum nav_tile_summary_bits_t : uint32_t {
	NAV_TILE_SUMMARY_NONE = 0,
	NAV_TILE_SUMMARY_STAIR = ( 1u << 0 ),
	NAV_TILE_SUMMARY_WATER = ( 1u << 1 ),
	NAV_TILE_SUMMARY_LAVA = ( 1u << 2 ),
	NAV_TILE_SUMMARY_SLIME = ( 1u << 3 ),
	NAV_TILE_SUMMARY_LADDER = ( 1u << 4 ),
	NAV_TILE_SUMMARY_WALK_OFF = ( 1u << 5 ),
	NAV_TILE_SUMMARY_HARD_WALL = ( 1u << 6 )
};

/** 
*    @brief  Ladder endpoint flags stored on ladder-capable layers.
**/
enum nav_ladder_endpoint_bits_t : uint8_t {
	NAV_LADDER_ENDPOINT_NONE = 0,
	NAV_LADDER_ENDPOINT_STARTPOINT = ( 1u << 0 ),
	NAV_LADDER_ENDPOINT_ENDPOINT = ( 1u << 1 )
};

//! Number of persisted per-layer horizontal edge slots.
static constexpr int32_t NAV_LAYER_EDGE_DIR_COUNT = 8;

/** 
*    @brief  A single Z layer at an XY position.
*         Stores quantized Z and flags for multi-layer navigation support.
**/
typedef struct nav_layer_s {
    //! Quantized Z position.
    int16_t z_quantized;
    //! Content flags (nav_layer_flags_t).
    uint32_t flags;
   //! Traversal-oriented node features separate from legacy/content flags.
	uint32_t traversal_feature_bits;
	//! Persisted per-edge traversal metadata for the 8 neighboring XY directions.
	uint32_t edge_feature_bits[ NAV_LAYER_EDGE_DIR_COUNT ];
	//! Bitmask of edge slots containing explicit metadata.
	uint8_t edge_valid_mask;
	//! Explicit ladder endpoint flags for this layer.
	uint8_t ladder_endpoint_flags;
	//! Quantized ladder startpoint (bottom) for this ladder span.
	int16_t ladder_start_z_quantized;
	//! Quantized ladder endpoint (top) for this ladder span.
	int16_t ladder_end_z_quantized;
	//! Optional clearance in grid cells 
	//! quantized, but stored with wider range for tall ceilings. e.g:
	//! (Skyboxes, a church cathedral of some sorts, elevator shafts, "tunnels").
	uint32_t clearance;
} nav_layer_t;

/** 
*    @brief  Profile describing an agent's navigation hull and constraints.
*    @note   Used to centralize step/drop/slope limits when tuning traversal heuristics.
**/
typedef struct nav_agent_profile_s {
    //! Agent hull minimum offsets in nav-center space.
    Vector3 mins;
    //! Agent hull maximum offsets in nav-center space.
    Vector3 maxs;
    //! Maximum step height this agent can traverse (world units).
    double max_step_height;
    //! Maximum vertical drop permitted during traversal (world units).
    double max_drop_height;
    //! Drop cap applied during path post-processing (world units).
    double max_drop_height_cap;
    //! Minimum walkable surface normal Z threshold.
	double max_slope_normal_z;
} nav_agent_profile_t;

/** 
*    @brief  XY cell entry containing multiple Z layers.
*         Supports multi-floor environments where multiple walkable surfaces
*         exist at the same XY coordinate.
**/
typedef struct nav_xy_cell_s {
    //! Number of Z layers at this XY position.
    int32_t num_layers;
    //! Array of Z layers at this XY position.
    nav_layer_t *layers;
} nav_xy_cell_t;

/** 
*    @brief  A single tile covering a region of XY cells.
*         Uses sparse storage with presence bitsets for memory efficiency.
**/
typedef struct nav_tile_s {
    //! Tile X coordinate in the grid.
    int32_t tile_x;
    //! Tile Y coordinate in the grid.
    int32_t tile_y;
   //! Region membership assigned by the future hierarchy build. `NAV_REGION_ID_NONE` while unassigned.
	int32_t region_id;
	//! Coarse traversal summary bits derived from the tile's fine metadata.
	uint32_t traversal_summary_bits;
	//! Coarse edge-summary bits derived from the tile's fine edge metadata.
	uint32_t edge_summary_bits;
    //! Bitset for which XY cells are present (sparse storage).
    uint32_t *presence_bits;
    //! Array of XY cells (sparse).
    nav_xy_cell_t *cells;
} nav_tile_t;

/** 
*    @brief  Per-leaf navigation data.
*         World mesh is organized per BSP leaf for spatial locality.
**/
typedef struct nav_leaf_data_s {
    //! BSP leaf index.
    int32_t leaf_index;
    //! Number of tiles in this leaf.
    int32_t num_tiles;
	//! Array of canonical world-tile ids (indices into `nav_mesh_t::world_tiles`).
	//!
	//! This is a non-owning view used for:
	//!	- debug drawing by leaf
	//!	- optional leaf-local iteration
	//!	- compatibility while migrating older leaf-centric code
	int32_t *tile_ids;
	//! Number of hierarchy regions touching this leaf.
	int32_t num_regions;
	//! Array of hierarchy-region ids overlapping this leaf.
	int32_t *region_ids;
} nav_leaf_data_t;

/** 
* 	@brief	World tile key used for canonical world tile storage.
* 	@note	World tiles are unique by `(tile_x,tile_y)`.
**/
struct nav_world_tile_key_t {
	int32_t tile_x = 0;
	int32_t tile_y = 0;

	bool operator==( const nav_world_tile_key_t &o ) const {
		return tile_x == o.tile_x && tile_y == o.tile_y;
    }
};

/** 
*  @brief    Set a presence bit for a cell index within a tile (safe helper).
*  @param    tile        Tile whose presence bitset to modify.
*  @param    cell_index  Cell index inside the tile (0..cells_per_tile-1).
*  @note     Defensive: returns when tile or presence_bits are null.
**/
static inline void Nav_SetPresenceBit( nav_tile_t *tile, int32_t cell_index ) {
	/**
	*  Sanity: require tile pointer and a valid cell index.
	**/
	if ( !tile || cell_index < 0 ) {
		return;
	}

	// Presence bitset must exist; validate via the public accessor pattern.
	if ( !tile->presence_bits ) {
		return;
	}

	const int32_t word_index = cell_index >> 5;
	const int32_t bit_index = cell_index & 31;
	tile->presence_bits[ word_index ] |= ( 1u << bit_index );
}

struct nav_world_tile_key_hash_t {
	size_t operator()( const nav_world_tile_key_t &k ) const {
		size_t seed = std::hash<int32_t>{}( k.tile_x );
		seed ^= std::hash<int32_t>{}( k.tile_y ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

/** 
*    @brief  Per inline model navigation data (for brush models).
*         Inline models (func_door, func_plat, etc.) are stored separately
*         in their local space for transform application.
**/
typedef struct nav_inline_model_data_s {
    //! Inline model index (e.g., * 1, * 2, etc.).
    int32_t model_index;
    //! Number of tiles for this model.
    int32_t num_tiles;
    //! Array of tiles for this model.
    nav_tile_t *tiles;
} nav_inline_model_data_t;

typedef struct nav_inline_model_runtime_s {
	//! Inline model index (e.g., * 1, * 2, etc.).
	int32_t model_index;
	//! Owning edict number (entity index in edict pool).
	int32_t owner_entnum;

	//! Cached owning entity pointer (for fast refresh).
	svg_base_edict_t *owner_ent;

	//! Current world-space origin for the owning entity.
	Vector3 origin;
	//! Current world-space angles for the owning entity.
	Vector3 angles;

	//! Set when origin/angles changed since last refresh.
	bool dirty;
} nav_inline_model_runtime_t;

// Forward declaration for `nav_mesh_t` lives near the top of this header.

// helper accessors declared after nav_mesh_t definition

/** 
* 	Navigation Cluster Graph (Tile-Level):
* 
* 	This is a coarse adjacency graph over world tiles. It is built after voxelmesh
* 	generation and used as a fast hierarchical pre-pass for long routes:
* 		1) Find start/goal tiles.
* 		2) BFS across tile adjacency to get a tile route.
* 		3) Run fine A*  restricted to tiles on that route.
* 
* 	This keeps the fine path quality (still A*  on nodes) while dramatically reducing
* 	search space on large maps.
**/
struct nav_tile_cluster_key_t {
	int32_t tile_x = 0;
	int32_t tile_y = 0;

	bool operator==( const nav_tile_cluster_key_t &o ) const {
		return tile_x == o.tile_x && tile_y == o.tile_y;
	}


};

struct nav_tile_cluster_key_hash_t {
	size_t operator()( const nav_tile_cluster_key_t &k ) const {
		size_t seed = std::hash<int32_t>{}( k.tile_x );
		seed ^= std::hash<int32_t>{}( k.tile_y ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

enum nav_tile_cluster_node_flag_t : uint8_t {
	NAV_TILE_CLUSTER_FLAG_NONE = 0,
	NAV_TILE_CLUSTER_FLAG_STAIR = ( 1 << 0 ),
	NAV_TILE_CLUSTER_FLAG_WATER = ( 1 << 1 ),
	NAV_TILE_CLUSTER_FLAG_LAVA = ( 1 << 2 ),
	NAV_TILE_CLUSTER_FLAG_SLIME = ( 1 << 3 ),
	NAV_TILE_CLUSTER_FLAG_LADDER = ( 1 << 4 ),
	NAV_TILE_CLUSTER_FLAG_WALKOFF = ( 1 << 5 )
};

struct nav_tile_cluster_node_t {
	nav_tile_cluster_key_t key;
	int32_t neighbors[ 4 ] = { -1, -1, -1, -1 };
	uint8_t flags = NAV_TILE_CLUSTER_FLAG_NONE;
};

struct nav_tile_cluster_graph_t {
	// Map tile key -> compact node index.
	std::unordered_map<nav_tile_cluster_key_t, int32_t, nav_tile_cluster_key_hash_t> idOf;
	// Node list storing directional neighbors.
	std::vector<nav_tile_cluster_node_t> nodes;
};

/** 
*    @brief  Compatibility hooks describing which traversal semantics a hierarchy element supports.
**/
enum nav_hierarchy_compatibility_bits_t : uint32_t {
	NAV_HIERARCHY_COMPAT_NONE = 0,
	NAV_HIERARCHY_COMPAT_GROUND = ( 1u << 0 ),
	NAV_HIERARCHY_COMPAT_STAIR = ( 1u << 1 ),
	NAV_HIERARCHY_COMPAT_LADDER = ( 1u << 2 ),
	NAV_HIERARCHY_COMPAT_WATER = ( 1u << 3 ),
	NAV_HIERARCHY_COMPAT_WALK_OFF = ( 1u << 4 ),
	NAV_HIERARCHY_COMPAT_LAVA = ( 1u << 5 ),
	NAV_HIERARCHY_COMPAT_SLIME = ( 1u << 6 )
};

/** 
*    @brief  Future-facing region summary flags for static hierarchy bookkeeping.
**/
enum nav_region_flags_t : uint32_t {
	NAV_REGION_FLAG_NONE = 0,
	NAV_REGION_FLAG_HAS_STAIRS = ( 1u << 0 ),
	NAV_REGION_FLAG_HAS_LADDERS = ( 1u << 1 ),
	NAV_REGION_FLAG_HAS_WATER = ( 1u << 2 ),
	NAV_REGION_FLAG_ISOLATED = ( 1u << 3 ),
	NAV_REGION_FLAG_HAS_LAVA = ( 1u << 4 ),
	NAV_REGION_FLAG_HAS_SLIME = ( 1u << 5 )
};

/** 
*    @brief  Adjacency record for one canonical world tile.
**/
struct nav_tile_adjacency_t {
	//! Canonical tile id into `nav_mesh_t::world_tiles`.
	int32_t tile_id = -1;
	//! Offset into `nav_hierarchy_storage_t::tile_neighbor_refs`.
	int32_t first_neighbor_ref = 0;
	//! Number of neighbor tile ids referenced by this adjacency record.
	int32_t num_neighbor_refs = 0;
	//! Aggregated compatibility bits derived from the source tile.
	uint32_t compatibility_bits = NAV_HIERARCHY_COMPAT_NONE;
};

/** 
*    @brief  Future-facing portal state flags supporting local invalidation overlays.
**/
enum nav_portal_flags_t : uint32_t {
	NAV_PORTAL_FLAG_NONE = 0,
	NAV_PORTAL_FLAG_SUPPORTS_STAIRS = ( 1u << 0 ),
	NAV_PORTAL_FLAG_SUPPORTS_LADDER = ( 1u << 1 ),
	NAV_PORTAL_FLAG_SUPPORTS_WATER = ( 1u << 2 ),
	NAV_PORTAL_FLAG_INVALIDATED = ( 1u << 16 ),
	NAV_PORTAL_FLAG_BLOCKED = ( 1u << 17 ),
	NAV_PORTAL_FLAG_DIRTY = ( 1u << 18 )
};

/** 
*    @brief  Reference from a region to a canonical world tile.
**/
struct nav_region_tile_ref_t {
	//! Canonical tile id into `nav_mesh_t::world_tiles`.
	int32_t tile_id = -1;
};

/** 
*    @brief  Reference from a region to a BSP leaf touched by that region.
**/
struct nav_region_leaf_ref_t {
	//! BSP leaf index owning or overlapping the referenced region membership.
	int32_t leaf_index = -1;
};

/** 
*    @brief  Static hierarchy region record.
*    @note   This is internal scaffolding only; Phase 3 will populate it deterministically.
**/
struct nav_region_t {
	//! Stable compact id for this region.
	int32_t id = NAV_REGION_ID_NONE;
	//! Aggregated region summary flags.
	uint32_t flags = NAV_REGION_FLAG_NONE;
	//! Traversal compatibility bits for future policy filtering.
	uint32_t compatibility_bits = NAV_HIERARCHY_COMPAT_NONE;
	//! Offset into `nav_hierarchy_storage_t::tile_refs`.
	int32_t first_tile_ref = 0;
	//! Number of tile refs owned by this region.
	int32_t num_tile_refs = 0;
	//! Offset into `nav_hierarchy_storage_t::leaf_refs`.
	int32_t first_leaf_ref = 0;
	//! Number of leaf refs owned by this region.
	int32_t num_leaf_refs = 0;
	//! Offset into `nav_hierarchy_storage_t::region_portal_refs`.
	int32_t first_portal_ref = 0;
	//! Number of portal refs owned by this region.
	int32_t num_portal_refs = 0;
	//! Optional debug anchor tile used for inspection and diagnostics.
	int32_t debug_anchor_tile_id = -1;
};

/** 
*    @brief  Static hierarchy portal record joining two regions.
*    @note   Includes future-facing invalidation metadata for local overlay updates.
**/
struct nav_portal_t {
	//! Stable compact id for this portal.
	int32_t id = NAV_PORTAL_ID_NONE;
	//! Region on one side of the portal.
	int32_t region_a = NAV_REGION_ID_NONE;
	//! Region on the opposite side of the portal.
	int32_t region_b = NAV_REGION_ID_NONE;
	//! Aggregated portal state and invalidation flags.
	uint32_t flags = NAV_PORTAL_FLAG_NONE;
	//! Traversal compatibility bits for future policy filtering.
	uint32_t compatibility_bits = NAV_HIERARCHY_COMPAT_NONE;
	//! Canonical world tile id contributing one side of the portal.
	int32_t tile_id_a = -1;
	//! Canonical world tile id contributing the opposite side of the portal.
	int32_t tile_id_b = -1;
	//! Representative world-space point for debug draw and routing seeds.
	Vector3 representative_point = {};
	//! Estimated traversal cost used by future coarse routing.
	double traversal_cost = 0.0;
	//! Future-facing serial incremented when local invalidation state changes.
	uint32_t invalidation_serial = 0;
};

/** 
*    @brief  Local runtime overlay for a static hierarchy portal.
*    @note   This remains strictly overlay-based and must not mutate the generated static portal graph.
**/
struct nav_portal_overlay_t {
	//! Runtime invalidation, blocking, and dirty flags mirrored from `nav_portal_flags_t` overlay bits.
	uint32_t flags = NAV_PORTAL_FLAG_NONE;
	//! Optional additional traversal cost applied when the portal remains usable but locally penalized.
	double additional_traversal_cost = 0.0;
	//! Runtime serial incremented whenever the local overlay changes.
	uint32_t invalidation_serial = 0;
};

/** 
*    @brief  Internal hierarchy container owned by the navigation mesh.
**/
struct nav_hierarchy_storage_t {
	//! Canonical-world-tile adjacency records for Phase 3 static region construction.
	std::vector<nav_tile_adjacency_t> tile_adjacency;
	//! Flattened neighbor tile-id refs addressed by `tile_adjacency` ranges.
	std::vector<int32_t> tile_neighbor_refs;

	//! Region records built from canonical world tiles.
	std::vector<nav_region_t> regions;
	//! Portal records joining neighboring regions.
	std::vector<nav_portal_t> portals;

	//! Local runtime overlay entries aligned 1:1 with `portals`.
	std::vector<nav_portal_overlay_t> portal_overlays;
	//! Flat tile-ref storage addressed by region ranges.
	std::vector<nav_region_tile_ref_t> tile_refs;
	//! Flat leaf-ref storage addressed by region ranges.
	std::vector<nav_region_leaf_ref_t> leaf_refs;

	//! Flat portal-id storage addressed by region ranges.
	std::vector<int32_t> region_portal_refs;
	//! Debug count of regions produced by the last static partition pass.
	int32_t debug_region_count = 0;
	//! Debug count of adjacency links produced by the last tile-adjacency pass.
	int32_t debug_adjacency_link_count = 0;
	//! Debug count of traversable cross-region boundary samples seen during portal extraction.
	int32_t debug_boundary_link_count = 0;
	//! Debug count of portal records produced by the last portal extraction pass.
	int32_t debug_portal_count = 0;
	//! Debug count of single-tile isolated regions from the last partition pass.
	int32_t debug_isolated_region_count = 0;
	//! Debug count of oversized regions from the last partition pass.
	int32_t debug_oversized_region_count = 0;
	//! Future build serial for deterministic rebuild tracking.
	uint32_t build_serial = 0;
   //! Runtime serial incremented when any local portal overlay entry changes.
	uint32_t portal_overlay_serial = 0;
	//! Indicates the hierarchy needs rebuilding after mesh regeneration or mutation.
	bool dirty = true;
};

/** 
* 	@brief	Refreshes inline model runtime transforms (for movers).
* 	@note	Intended to be cheap enough to call once per frame.
**/
void SVG_Nav_RefreshInlineModelRuntime( void );

/** 
* @brief    Lookup inline-model runtime index for an owning entity number.
* @param    mesh            Navigation mesh.
* @param    owner_entnum    Owning entity number (edict->s.number).
* @return   Index into `mesh->inline_model_runtime` or -1 if not found/invalid.
 **/
int32_t SVG_Nav_GetInlineModelRuntimeIndexForOwnerEntNum( const nav_mesh_t *mesh, const int32_t owner_entnum );

/** 
* @brief    Lookup inline-model runtime entry by compact index with bounds checks.
* @param    mesh    Navigation mesh.
* @param    idx     Index into runtime array.
* @return   Pointer to runtime entry or nullptr on invalid index.
 **/
const nav_inline_model_runtime_t *SVG_Nav_GetInlineModelRuntimeByIndex( const nav_mesh_t *mesh, const int32_t idx );

/** 
*    @brief  Build a navigation agent profile using registered nav cvars.
*    @note   Provides default hull/traversal limits when called early in startup.
*    @return Populated nav_agent_profile_t describing hull extents and traversal constraints.
**/
nav_agent_profile_t SVG_Nav_BuildAgentProfileFromCvars( void );



/**
*
*
*
*    Tile Layer/Cells API:
*
*
*
**/
/** 
* 	@brief	Get layers pointer and count for a nav XY cell (const overload).
* 	@param	cell	Pointer to the XY cell to query.
* 	@return	A std::pair of (const nav_layer_t*  layers, int32_t num_layers). Returns
* 			nullptr and 0 when the cell or its layers are not present.
* 	@note	Helper intended to prevent callers from dereferencing dangling
* 			pointers by explicitly returning a nullptr/count when data is absent.
**/
std::pair<const nav_layer_t *, int32_t> SVG_Nav_Cell_GetLayers( const nav_xy_cell_t *cell );
/** 
* 	@brief	Get layers pointer and count for a nav XY cell (mutable overload).
* 	@param	cell	Pointer to the XY cell to query.
* 	@return	A std::pair of (nav_layer_t*  layers, int32_t num_layers). Returns
* 			nullptr and 0 when the cell or its layers are not present.
* 	@note	Mutable overload for callers that need to modify layer data. Returns
* 			nullptr/count to avoid dangling pointer use when data is missing.
**/
std::pair<nav_layer_t *, int32_t> SVG_Nav_Cell_GetLayers( nav_xy_cell_t *cell );

/** 
* 	@brief	Get cell array and count for a tile (mutable tile pointer).
* 	@param	mesh	Pointer to the nav mesh (used for tile_size).
* 	@param	tile	Pointer to the tile to query.
* 	@return	A std::pair of (nav_xy_cell_t*  cells, int32_t count). Returns
* 			nullptr and 0 when mesh/tile/cells are missing.
* 	@note	Count is computed as mesh->tile_size* mesh->tile_size. Returning
* 			nullptr/count avoids exposing dangling arrays to callers.
**/
std::pair<nav_xy_cell_t *, int32_t> SVG_Nav_Tile_GetCells( const nav_mesh_t *mesh, nav_tile_t *tile );
/** 
* 	@brief	Get cell array and count for a tile (const overload).
* 	@param	mesh	Pointer to the nav mesh (used for tile_size).
* 	@param	tile	Const pointer to the tile to query.
* 	@return	A std::pair of (const nav_xy_cell_t*  cells, int32_t count). Returns
* 			nullptr and 0 when mesh/tile/cells are missing.
* 	@note	Const overload for read-only callers. Returning nullptr/count avoids
* 			exposing dangling arrays when data is absent.
**/
std::pair<const nav_xy_cell_t *, int32_t> SVG_Nav_Tile_GetCells( const nav_mesh_t *mesh, const nav_tile_t *tile );


/**
*
*
*	Leaf Region/Tile ID API:
*	@details
*		This API provides safe, minimal-overhead accessors for per-BSP-leaf
*		views into the navigation mesh's canonical world-tile and hierarchy-region
*		references. The underlying `nav_leaf_data_t` stores non-owning arrays:
*		 - `tile_ids`   : indices into `nav_mesh_t::world_tiles` for tiles touching the leaf.
*		 - `num_tiles`  : length of `tile_ids`.
*		 - `region_ids` : hierarchy region ids overlapping the leaf.
*		 - `num_regions`: length of `region_ids`.
*
*		These helpers exist to:
*		 - Avoid repeating null/dangling-pointer checks at call sites.
*		 - Make intent explicit (mutable vs const access).
*		 - Document that returned pointers are non-owning views and may be null.
*
*	Safety & semantics:
*		 - The returned pair is (pointer, count). If the leaf pointer is null or the
*		   underlying array pointer within `nav_leaf_data_t` is null, the functions
*		   return `(nullptr, 0)`. Callers must test the pointer and/or count before use.
*		 - The arrays referenced are owned by the nav mesh/leaf structures and must not
*		   be freed or mutated by callers unless the mutable overload is used and the
*		   caller has ownership/intent to modify.
*		 - These accessors do not copy or clone data; they only provide a convenient,
*		   defensive view that prevents accidental iteration over dangling pointers.
*		 - Because the arrays are non-owning, callers must ensure the mesh/leaf lifetime
*		   outlives any use of the returned pointers. In practice, perform queries on the
*		   main/game thread or while holding any synchronization that protects mesh mutation.
*
*	Functions:
*		 - `SVG_Nav_Leaf_GetTileIds( nav_leaf_data_t *leaf )` (mutable)
*			   Returns `(int32_t* tile_ids, int32_t num_tiles)`. Use when you intend to
*			   mutate the tile id list (rare).
*		 - `SVG_Nav_Leaf_GetTileIds( const nav_leaf_data_t *leaf )` (const)
*			   Returns `(const int32_t* tile_ids, int32_t num_tiles)` for read-only access.
*		 - `SVG_Nav_Leaf_GetRegionIds( nav_leaf_data_t *leaf )` (mutable)
*			   Returns `(int32_t* region_ids, int32_t num_regions)`.
*		 - `SVG_Nav_Leaf_GetRegionIds( const nav_leaf_data_t *leaf )` (const)
*			   Returns `(const int32_t* region_ids, int32_t num_regions)`.
*
*	Typical usage (step-by-step):
*		1) Acquire a valid `nav_leaf_data_t *leaf` (from mesh->leaf_data or other API).
*		2) Call the appropriate accessor (const or mutable) to get (ptr, count).
*		3) Test for presence: `if ( ptr && count > 0 )` before iterating.
*		4) Iterate using the returned count; indices are compact ints that index into
*		   `nav_mesh_t::world_tiles` (for tile_ids) or into the hierarchy region set
*		   (for region_ids) as documented by the mesh.
*		5) Do not retain the raw pointer across mesh mutations or mesh frees.
*
*	Example (safe iteration):
*		// Read-only example
*		const auto [tileIds, tileCount] = SVG_Nav_Leaf_GetTileIds( leaf );
*		if ( tileIds && tileCount > 0 ) {
*			for ( int32_t i = 0; i < tileCount; ++i ) {
*				int32_t tileId = tileIds[ i ]; // canonical id into mesh->world_tiles
*				// Use tileId (debug draw, diagnostics, or lookups)
*			}
*		}
*
*		// Region ids example
*		const auto [regionIds, regionCount] = SVG_Nav_Leaf_GetRegionIds( leaf );
*		if ( regionIds && regionCount > 0 ) {
*			for ( int32_t r = 0; r < regionCount; ++r ) {
*				int32_t regionId = regionIds[ r ];
*				// Use regionId for hierarchy queries/filters
*			}
*		}
*
*	Integration notes:
*		 - These helpers are intentionally minimal and defensive. Prefer them over
*		   manual field access when writing code that may run against missing or
*		   partially-initialized leaf data (e.g., tooling, debug drawing, editor tools).
*		 - If you require atomic snapshot semantics across multiple arrays (tile_ids +
*		   region_ids), perform your own copy while the mesh is stable; the accessors
*		   only provide per-array views and do not guarantee cross-array consistency
*		   during concurrent mutations.
*
*	@note
*		- The API returns raw pointers for performance and to avoid allocation overhead.
*		- Use the const overload for most callers; use the mutable overload only when
*		  you are intentionally modifying leaf-local arrays and understand ownership.
*
**/
/** 
* 	@brief	Get tile id array and count for a leaf (mutable overload).
* 	@param	leaf	Pointer to the leaf data to query.
* 	@return	A std::pair of (int32_t*  tile_ids, int32_t num_tiles). Returns
* 			nullptr and 0 when leaf or tile_ids are missing.
* 	@note	Helps prevent callers from iterating over a dangling pointer when
* 			leaf data is absent.
**/
std::pair<int32_t *, int32_t> SVG_Nav_Leaf_GetTileIds( nav_leaf_data_t *leaf );
/** 
* 	@brief	Get tile id array and count for a leaf (const overload).
* 	@param	leaf	Const pointer to the leaf data to query.
* 	@return	A std::pair of (const int32_t*  tile_ids, int32_t num_tiles). Returns
* 			nullptr and 0 when leaf or tile_ids are missing.
* 	@note	Const overload for read-only callers; returning nullptr/count avoids
* 			exposing dangling pointers.
**/
std::pair<const int32_t *, int32_t> SVG_Nav_Leaf_GetTileIds( const nav_leaf_data_t *leaf );
/** 
* 	@brief	Get region id array and count for a leaf (mutable overload).
* 	@param	leaf	Pointer to the leaf data to query.
* 	@return	A std::pair of (int32_t*  region_ids, int32_t num_regions). Returns
* 			nullptr and 0 when leaf or region_ids are missing.
**/
std::pair<int32_t *, int32_t> SVG_Nav_Leaf_GetRegionIds( nav_leaf_data_t *leaf );
/** 
* 	@brief	Get region id array and count for a leaf (const overload).
* 	@param	leaf	Const pointer to the leaf data to query.
* 	@return	A std::pair of (const int32_t*  region_ids, int32_t num_regions). Returns
* 			nullptr and 0 when leaf or region_ids are missing.
**/
std::pair<const int32_t *, int32_t> SVG_Nav_Leaf_GetRegionIds( const nav_leaf_data_t *leaf );



/**
*
*
*
*    Inline-Models API:
*
*
*
**/
/** 
* 	@brief		Get inline-model tiles array and count (mutable overload).
* 	@param		model	Pointer to the inline model data to query.
* 	@return		A std::pair of (nav_tile_t*  tiles, int32_t num_tiles).
* 				Returns nullptr and 0 when model or tiles are missing.
* 	@note		Returning nullptr/count prevents clients from using a dangling tiles
* 				pointer when inline-model data is not present.
**/
inline std::pair<nav_tile_t *, int32_t> SVG_Nav_InlineModel_GetTiles( nav_inline_model_data_t *model );
/** 
* 	@brief		Get inline-model tiles array and count (const overload).
* 	@param		model	Const pointer to the inline model data to query.
* 	@return		A std::pair of (const nav_tile_t*  tiles, int32_t num_tiles).
* 				Returns nullptr and 0 when model or tiles are missing.
* 	@note		Const overload for read-only callers; avoids exposing dangling data.
**/
std::pair<const nav_tile_t *, int32_t> SVG_Nav_InlineModel_GetTiles( const nav_inline_model_data_t *model );
/** 
* 	@brief	Get inline-model runtime array and count from mesh (mutable overload).
* 	@param	mesh	Pointer to the navigation mesh.
* 	@return	A std::pair of (nav_inline_model_runtime_t*  entries, int32_t count).
* 			Returns nullptr and 0 when mesh or runtime array is missing.
* 	@note	Runtime entries are non-owning pointers; this helper returns nullptr/count
* 			to avoid callers holding/using dangling pointers when the mesh is absent.
**/
std::pair<nav_inline_model_runtime_t *, int32_t> SVG_Nav_Mesh_GetInlineModelRuntime( nav_mesh_t *mesh );
/** 
* 	@brief		Get inline-model runtime array and count from mesh (const overload).
* 	@param		mesh	Const pointer to the navigation mesh.
* 	@return		A std::pair of (const nav_inline_model_runtime_t*  entries, int32_t count).
* 				Returns nullptr and 0 when mesh or runtime array is missing.
* 	@note		Const overload for read-only callers; avoids exposing dangling pointers.
**/
std::pair<const nav_inline_model_runtime_t *, int32_t> SVG_Nav_Mesh_GetInlineModelRuntime( const nav_mesh_t *mesh );
/**
* 	@brief	Lookup an inline-model runtime entry by owning entity number.
* 	@param	mesh	Navigation mesh.
* 	@param	owner_entnum	Owning entity number (edict->s.number).
* 	@return	Pointer to runtime entry if found, otherwise nullptr.
* 	@note	This is a fast path used to avoid linear searches over `inline_model_runtime`.
**/
const nav_inline_model_runtime_t *SVG_Nav_GetInlineModelRuntimeForOwnerEntNum( const nav_mesh_t *mesh, const int32_t owner_entnum );

/**
* 	@brief	Dump the inline-model runtime index map for debugging.
* 	@param	mesh	Navigation mesh.
* 	@note	Intended for developer diagnostics; prints to console.
**/
void SVG_Nav_Debug_PrintInlineModelRuntimeIndexMap( const nav_mesh_t *mesh );



/** 
* 
* 
* 
*    Navigation System Occupancy API:
*		- This API manages dynamic occupancy data for navigation tiles, 
*		  supporting both soft costs and hard blocks.
*  
*  
*  
**/
/**
*  @brief    Runtime occupancy entry used for dynamic blocking/penalties.
**/
struct nav_occupancy_entry_t {
	//! Accumulated soft cost (crowd avoidance, biasing).
	int32_t soft_cost = 0;
	//! Hard block flag to prevent traversal entirely.
	bool blocked = false;
};
/** 
* 	@brief	Clear all dynamic occupancy records on the given mesh.
**/
void SVG_Nav_Occupancy_Clear( nav_mesh_t *mesh );

/** 
* 	@brief	Add a dynamic occupancy entry for a tile/cell/layer.
* 	@param	mesh	Target mesh.
* 	@param	tileId	Canonical tile id into `world_tiles`.
* 	@param	cellIndex	Cell index inside the tile.
* 	@param	layerIndex	Layer index inside the cell.
* 	@param	cost	Soft cost to accumulate (default 1).
* 	@param	blocked	If true, mark this location as hard blocked.
**/
void SVG_Nav_Occupancy_Add( nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, int32_t cost, bool blocked );

/** 
* 	@brief	Query the dynamic occupancy soft cost for a tile/cell/layer.
* 	@return	Accumulated soft cost (0 if none or mesh missing).
**/
int32_t SVG_Nav_Occupancy_SoftCost( const nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex );

/** 
* 	@brief	Query if a tile/cell/layer is marked as hard blocked.
* 	@return	True if blocked.
**/
bool SVG_Nav_Occupancy_Blocked( const nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex );

/** 
* 	@brief	Query the full sparse occupancy overlay entry for a tile/cell/layer.
* 	@param	mesh	Navigation mesh containing the sparse occupancy overlay.
* 	@param	tileId	Canonical tile id.
* 	@param	cellIndex	Canonical cell index within the tile.
* 	@param	layerIndex	Canonical layer index within the cell.
* 	@param	out_entry	[out] Occupancy entry snapshot when present.
* 	@return	True when the sparse overlay contains an entry for the queried location.
* 	@note	This is the authoritative local dynamic-overlay lookup and must not imply any nav or hierarchy rebuild.
**/
bool SVG_Nav_Occupancy_TryGet( const nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, nav_occupancy_entry_t *out_entry );

/**
*	@brief	Navigation Hierarchy Portal Overlay API — runtime-only local invalidation layer.
*	@details
*		This API provides a thin, local, runtime-only overlay on top of the static
*		hierarchy portal graph generated during navmesh construction. The static
*		portal graph (`nav_hierarchy_storage_t::portals`) is considered authoritative
*		and immutable from the overlay's perspective; overlays are applied per-portal
*		at runtime to represent temporary or local conditions (moving obstacles,
*		inline-models, temporary closures, local cost penalties) without triggering
*		expensive global hierarchy rebuilds.
*
*		Key data relationships:
*		 - `nav_portal_t`         : The static portal record (stable compact id).
*		 - `nav_portal_overlay_t` : Per-portal runtime overlay entry (stored in
*								   `nav_hierarchy_storage_t::portal_overlays`) aligned
*								   1:1 with `portals`.
*		 - `portal_id`            : Compact index into `nav_hierarchy_storage_t::portals`
*								   and the corresponding `portal_overlays` entry.
*		 - `nav_hierarchy_storage_t::portal_overlay_serial` :
*								   Global serial bumped when any overlay entry changes.
*
*		Overlay semantics:
*		 - Overlays MUST NOT mutate the static portal graph. They are strictly local
*		   annotations that influence runtime routing decisions.
*		 - Common overlay fields:
*			 * `flags` : bitflags from `nav_portal_flags_t` mirrored into the overlay
*						 (e.g. `NAV_PORTAL_FLAG_BLOCKED`, `NAV_PORTAL_FLAG_INVALIDATED`,
*						 `NAV_PORTAL_FLAG_DIRTY`). These indicate how routing should
*						 treat the portal.
*			 * `additional_traversal_cost` : A soft penalty to add to the portal's
*							traversal cost when computing coarse routes. Use for local
*							penalties (crowds, hazards) while leaving the portal usable.
*			 * `invalidation_serial` : Per-overlay serial incremented on change; can
*							be used by caches to detect stale overlay snapshots.
*
*		Interpretation guidance (how pathfinding / routing should consult overlays):
*		 - When an overlay exists for `portal_id`, callers should:
*			 1) If overlay `flags` contains `NAV_PORTAL_FLAG_BLOCKED`, treat the portal
*				  as impassable at the hierarchy-routing stage.
*			 2) If overlay `flags` contains `NAV_PORTAL_FLAG_INVALIDATED`, the portal's
*				  static connectivity may be suspect in the local region and routing
*				  should either fall back to finer-grained node-level checks or treat
*				  the portal as temporarily unusable depending on policy.
*			 3) Otherwise, add `additional_traversal_cost` to the portal's base traversal
*				  cost to bias routing away from the portal without fully blocking it.
*			 4) Respect `NAV_PORTAL_FLAG_DIRTY` to trigger any lazy revalidation logic
*				  the caller maintains (e.g., delayed re-sampling of portal boundary).
*
*		Concurrency / lifetime notes:
*		 - Overlays are runtime-only and NOT persisted to disk. They are intended to be
*		   cheap ephemeral state that reflects the current level runtime (movers, doors, crowds).
*		 - Updates to overlays should generally be performed on the main/game thread.
*		   If overlays are updated from other threads, callers must ensure synchronization
*		   and increment `nav_hierarchy_storage_t::portal_overlay_serial` in a threadsafe
*		   manner so any caches observing that serial can react safely.
*
*		Typical usage pattern (step-by-step):
*		 1) Identify the affected portal(s) — portal ids are compact indices into
*			the mesh's `hierarchy.portals` array (for example found during inline-model
*			overlap testing or mover bounding-box checks).
*		 2) Compute local overlay state:
*			 - Decide whether portal should be temporarily blocked, invalidated, or
*			   penalized with an additional cost, and compute the `additional_traversal_cost`.
*		 3) Call `SVG_Nav_Hierarchy_SetPortalOverlay(mesh, portal_id, overlay_flags, additional_traversal_cost)`.
*			 - This updates the 1:1 `portal_overlays[portal_id]` entry and bumps the
*			   runtime overlay serials (`overlay.invalidation_serial` and
*			   `hierarchy.portal_overlay_serial`) to allow caches to detect changes.
*		 4) Pathfinding / coarse routing:
*			 - When evaluating a portal during hierarchy routing, call
*			   `SVG_Nav_Hierarchy_TryGetPortalOverlay(mesh, portal_id, &out_overlay)`.
*			 - Apply overlay handling rules described above (block, add cost, revalidate).
*		 5) When local condition clears (moving obstacle leaves, door reopens), either:
*			 - Call `SVG_Nav_Hierarchy_SetPortalOverlay` to clear flags / costs for the portal,
*			   or
*			 - Call `SVG_Nav_Hierarchy_ClearPortalOverlays(mesh)` to reset all overlays.
*
*		Implementation notes for integrators:
*		 - Use overlays for inline-model-driven local invalidation: when a mover/door
*		   overlaps a portal boundary, set `NAV_PORTAL_FLAG_BLOCKED` (or add cost)
*		   rather than rebuilding the static hierarchy.
*		 - Portal overlays let you express transient, locally-scoped navigation changes
*		   with minimal overhead and without losing the benefits of a static-first hierarchy.
*		 - Keep overlay state minimal: prefer soft `additional_traversal_cost` for
*		   congestion/hazard modeling and reserve hard `BLOCKED` for true impassability.
*
*	@note	API functions in this header:
*		 - `SVG_Nav_Hierarchy_ClearPortalOverlays` : reset all runtime overlays.
*		 - `SVG_Nav_Hierarchy_SetPortalOverlay`   : set/update a single portal overlay.
*		 - `SVG_Nav_Hierarchy_TryGetPortalOverlay` : query the overlay snapshot for a portal.
*
*	@see	`nav_portal_t`, `nav_portal_overlay_t`, `nav_hierarchy_storage_t::portal_overlays`,
*			`nav_hierarchy_storage_t::portal_overlay_serial`, `nav_portal_flags_t`.
**/
/**
*    @brief	Path result for navigation traversal queries.
*			Stores world-space waypoints for A*  pathfinding results.
**/
typedef struct nav_traversal_path_s {
	//! Number of path points in the traversal path.
	int32_t num_points;
	//! Array of world-space points.
	Vector3 *points;
} nav_traversal_path_t;

// Forward-declare policy to avoid circular includes.
struct svg_nav_path_policy_t;

/** 
* 	@brief	Clear all local portal overlay entries while leaving the static hierarchy untouched.
* 	@param	mesh	Navigation mesh containing the hierarchy portal overlay.
* 	@note	This is local runtime state only and must not trigger hierarchy regeneration.
**/
void SVG_Nav_Hierarchy_ClearPortalOverlays( nav_mesh_t *mesh );
/** 
* 	@brief	Update one local portal overlay entry without mutating the static portal graph.
* 	@param	mesh	Navigation mesh containing the hierarchy portal overlay.
* 	@param	portal_id	Stable compact portal id to update.
* 	@param	overlay_flags	Overlay flags such as invalidated, blocked, or dirty.
* 	@param	additional_traversal_cost	Optional local penalty added when the portal remains usable.
* 	@return	True when the overlay entry was updated.
* 	@note	This is intended for future inline-model-driven local invalidation hooks.
**/
bool SVG_Nav_Hierarchy_SetPortalOverlay( nav_mesh_t *mesh, int32_t portal_id, uint32_t overlay_flags, double additional_traversal_cost );
/** 
* 	@brief	Query one local portal overlay entry by compact portal id.
* 	@param	mesh	Navigation mesh containing the hierarchy portal overlay.
* 	@param	portal_id	Stable compact portal id to query.
* 	@param	out_overlay	[out] Current local portal overlay entry.
* 	@return	True when the portal id is valid and an overlay snapshot was returned.
* 	@note	The returned entry may be default-initialized when no local invalidation is active yet.
**/
bool SVG_Nav_Hierarchy_TryGetPortalOverlay( const nav_mesh_t *mesh, int32_t portal_id, nav_portal_overlay_t *out_overlay );

/** 
*    @brief  Build traversal-oriented node feature bits from persisted content flags.
*    @param  layer_flags  Legacy/content flags stored on the layer.
*    @return Traversal feature bits implied directly by the content flags.
**/
uint32_t SVG_Nav_BuildTraversalFeatureBitsFromLayerFlags( const uint32_t layer_flags );




/** 
* 
* 
* 
*    Navigation System Global Variables:
* 
* 
* 
**/
/**
*    @brief  Main navigation mesh structure.
*         Contains both world mesh (static geometry) and inline model meshes
*         (dynamic brush entities).
**/
struct nav_mesh_t {
	/**
	* 	World Boundaries:
	**/
	BBox3 world_bounds = { { -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE, -CM_MAX_WORLD_HALF_SIZE }, { CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE, CM_MAX_WORLD_HALF_SIZE } };

	/**
	*   Canonical world tile storage (unique tiles by tile_x/y):
	*
	*   World tiles are owned here. Leafs reference these by id in `nav_leaf_data_t::tile_ids`.
	*   Inline-model tiles remain per-model because they are stored in model-local space.
	**/
	std::vector<nav_tile_t> world_tiles;
	std::unordered_map<nav_world_tile_key_t, int32_t, nav_world_tile_key_hash_t> world_tile_id_of;

	/**
	* \tDynamic occupancy map keyed by (tile, cell, layer).
	* \tUsed to avoid per-query collision checks against other actors by consulting
	* \tpre-filled occupancy instead. Stores both hard-block and soft-cost data.
	**/
	std::unordered_map<uint64_t, nav_occupancy_entry_t> occupancy;
	//! Frame number for which occupancy is currently stamped.
	int64_t occupancy_frame = -1;

	/**
   *   World mesh data (per-leaf):
	**/
	//! Number of BSP leafs.
	int32_t num_leafs;
	//! Array of per-leaf navigation data.
	nav_leaf_data_t *leaf_data;

	/**
   *   Inline model mesh data (per-model):
	**/
	//! Number of inline models.
	int32_t num_inline_models;
	//! Array of per-inline-model navigation data.
	nav_inline_model_data_t *inline_model_data;

	/**
	*    Inline model runtime (per owning entity):
	*    Not saved; rebuilt during voxelmesh generation.
	**/
	int32_t num_inline_model_runtime;
	nav_inline_model_runtime_t *inline_model_runtime;

	/**
	* 	Inline model runtime lookup:
	* 		Maps owning entity number -> runtime entry index.
	* 		Used to avoid linear scans when resolving runtime transforms for a clip entity.
	* 		Rebuilt during voxelmesh generation.
	**/
	std::unordered_map<int32_t, int32_t> inline_model_runtime_index_of;

	/**
	* 	Internal static-first hierarchy scaffolding:
	* 		- regions
	* 		- portals
	* 		- flattened membership/reference storage
	**/
	nav_hierarchy_storage_t hierarchy;

	/**
	*   Generation parameters:
	**/
	//! XY grid cell size.
	double cell_size_xy;
	//! Z-axis quantization step.
	double z_quant;
	//! Number of cells per tile dimension.
	int32_t tile_size;
	//! Maximum step height (matches PM_STEP_MAX_SIZE).
	double max_step;
	//! Minimum walkable surface normal Z threshold (matches PM_STEP_MIN_NORMAL).
	double max_slope_normal_z;
	//! Agent bounding box minimum.
	Vector3 agent_mins;
	//! Agent bounding box maximum.
	Vector3 agent_maxs;

	/**
	*   Statistics:
	**/
	//! Total number of tiles generated.
	int32_t total_tiles;
	//! Total number of XY cells with data.
	int32_t total_xy_cells;
  //! Total number of Z layers across all cells.
	int32_t total_layers;
};

/**
* 	@brief	Type alias for nav_mesh_t RAII owner using TagMalloc/TagFree.
* 	@note	Ensures proper construction/destruction and prevents memory leaks.
**/
using nav_mesh_raii_t = SVG_RAIIObject<nav_mesh_t>;
/** 
*    @brief  Global navigation mesh instance (RAII owner).
*         Stores the complete navigation data for the current level.
*    @note   Use g_nav_mesh.get() to access the raw pointer when needed.
**/
extern nav_mesh_raii_t g_nav_mesh;

/** 
* 	@brief	Profiling and logging control CVars.
**/
extern cvar_t *nav_profile_level;      //!< Profiling verbosity level (0..3).

/** 
* 	@brief	Lightweight logging wrapper for navigation subsystem.
* 	@note	Currently forwards to engine console output; allows future redirection to a
* 			centralized logging facility without changing callsites.
**/
void SVG_Nav_Log( const char * fmt, ... );



/** 
* 
* 
* 
*    Navigation Helper Functions:
* 
* 
* 
**/
/** 
*    @brief  Check if a surface normal is walkable based on slope.
*    @param  normal          Surface normal vector.
*    @param  max_slope_deg   Maximum walkable slope in degrees.
*    @return True if the slope is walkable, false otherwise.
**/
const bool IsWalkableSlope( const Vector3 &normal, double max_slope_deg );



/** 
* 
* 
* 
*    Navigation System Initialization/Shutdown:
* 
* 
* 
**/
/** 
*    @brief  Initialize navigation system and register cvars.
*         Called after entities have post-spawned to ensure inline models
*         have their proper modelindex set.
**/
void SVG_Nav_Init( void );
/** 
*    @brief  Shutdown navigation system and free memory.
*         Called during game shutdown to clean up resources.
**/
void SVG_Nav_Shutdown( void );
/** 
* 	@brief	Loads up an existing navigation mesh for the current map, if the file is located.
* 	@return True if a mesh was successfully loaded, false otherwise (e.g., file not found).
**/
const std::tuple<const bool, const std::string> SVG_Nav_LoadMesh( const char * levelName );



/** 
* 
* 
* 
*    Inline BSP model Mesh Generation:
* 
* 
* 
**/
/** 
* 	@brief	Collects all active entities that reference an inline BSP model ("* N").
* 			The returned mapping is keyed by inline model index (N).
* 	@note	If multiple entities reference the same "* N", the first one encountered is kept.
**/
void Nav_CollectInlineModelEntities( std::unordered_map<int32_t, svg_base_edict_t *> &out_model_to_ent );



/** 
* 
* 
* 
*    Navigation System "Voxel Mesh(-es)":
* 
* 
* 
**/
/** 
*    @brief  Generate navigation voxelmesh for the current level.
*         This is called by the nav_gen_voxelmesh server command.
*    @note   Can be called multiple times to regenerate the mesh with
*         different parameters.
**/
void SVG_Nav_GenerateVoxelMesh( void );
/** 
*    @brief  Free the current navigation mesh.
*         Releases all memory allocated for navigation data.
**/
void SVG_Nav_FreeMesh( void );



/** 
* 
* 
* 
*    Navigation Pathfinding Node System:
* 
* 
* 
**/
/** 
* 	@details	The nav_node_key_t uniquely identifies a navigation node within the mesh by its
* 				host tile and cell coordinates. 
* 
* 				It is used as a key for lookups in the occupancy map and other data structures 
* 				that need to reference specific nodes without holding direct pointers. 
* 
* 				The equality operator allows for easy comparison of keys, and the hash functor 
* 				enables efficient use in unordered maps.
**/
typedef struct nav_node_key_s {
	//! Index of the BSP leaf containing this node (for spatial locality).
	int32_t leaf_index = -1;
	//! Index of the tile containing this node (index into `nav_mesh_t::world_tiles`).
	int32_t tile_index = -1;
	//! Index of the cell within the tile (0..tile_size* tile_size-1).
	int32_t cell_index = -1;
	//! Index of the layer within the cell (0..num_layers-1).
	int32_t layer_index = -1;

	/** 
	* 	@brief	Equality operator for nav_node_key_t. Two keys are equal if all their indices match.
	* 	@note	Enables direct comparison of keys for lookups and data structure operations.
	* 	@return True if the keys are equal, false otherwise.
	**/
	bool operator==( const nav_node_key_s &other ) const {
		return leaf_index == other.leaf_index &&
			tile_index == other.tile_index &&
			cell_index == other.cell_index &&
			layer_index == other.layer_index;
	}
} nav_node_key_t;

/** 
*    @brief		Hash functor for nav_node_key_t. 
* 	@details	Combines the hash of each index to produce a unique hash value for the key.
* 				This allows nav_node_key_t to be used as a key in unordered maps, such as the occupancy map.
**/
struct nav_node_key_hash_t {
	/** 
	* 	@brief	Hash function for nav_node_key_t. Combines the hashes of all indices.
	* 	@return	Returns a size_t hash value for the given nav_node_key_t.
	**/
	size_t operator()( const nav_node_key_t &key ) const {
		size_t seed = std::hash<int32_t>{}( key.leaf_index );
		seed ^= std::hash<int32_t>{}( key.tile_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		seed ^= std::hash<int32_t>{}( key.cell_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		seed ^= std::hash<int32_t>{}( key.layer_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

/** 
*    @brief  Navigation node reference with absolute world position.
**/
typedef struct nav_node_ref_s {
	//! Unique key identifying the node's location in the mesh.
	nav_node_key_t key = {};
	//! Cached world-space position of the node for quick access during pathfinding.
	Vector3 worldPosition = { 0., 0., 0. };
} nav_node_ref_t;

/** 
*    @brief  Resolve a canonical tile pointer from a node reference.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference to resolve.
*    @return Pointer to the canonical tile or nullptr on failure.
**/
const nav_tile_t *SVG_Nav_GetNodeTileView( const nav_mesh_t *mesh, const nav_node_ref_t &node );

/** 
*    @brief  Resolve a canonical XY cell pointer from a node reference.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference to resolve.
*    @return Pointer to the canonical XY cell or nullptr on failure.
**/
const nav_xy_cell_t *SVG_Nav_GetNodeCellView( const nav_mesh_t *mesh, const nav_node_ref_t &node );

/** 
*    @brief  Resolve a canonical layer pointer from a node reference.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference to resolve.
*    @return Pointer to the canonical layer or nullptr on failure.
**/
const nav_layer_t *SVG_Nav_GetNodeLayerView( const nav_mesh_t *mesh, const nav_node_ref_t &node );

/** 
*    @brief  Query traversal feature bits for a canonical node.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference to inspect.
*    @return Traversal feature bits for the node, or `NAV_TRAVERSAL_FEATURE_NONE` on failure.
**/
uint32_t SVG_Nav_GetNodeTraversalFeatureBits( const nav_mesh_t *mesh, const nav_node_ref_t &node );

/** 
*    @brief  Query explicit edge metadata for a canonical node and XY offset.
*    @param  mesh      Navigation mesh.
*    @param  node      Source node reference.
*    @param  cell_dx   Neighbor cell X offset in `[-1, 1]`.
*    @param  cell_dy   Neighbor cell Y offset in `[-1, 1]`.
*    @return Explicit edge feature bits, or `NAV_EDGE_FEATURE_NONE` when no persisted edge slot applies.
**/
uint32_t SVG_Nav_GetEdgeFeatureBitsForOffset( const nav_mesh_t *mesh, const nav_node_ref_t &node, const int32_t cell_dx, const int32_t cell_dy );

/** 
*    @brief  Query coarse tile summary bits for the tile owning a canonical node.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference whose tile should be inspected.
*    @return Tile summary bits, or `NAV_TILE_SUMMARY_NONE` on failure.
**/
uint32_t SVG_Nav_GetTileSummaryBitsForNode( const nav_mesh_t *mesh, const nav_node_ref_t &node );

/** 
*    @brief  Search node for A*  pathfinding.
**/
typedef struct nav_search_node_s {
	//! A reference to the navigation node this search node represents.
	nav_node_ref_t node = {};
	//! Cost from the start node to this node.
	double g_cost = 0.;
	//! Estimated cost from this node to the goal (heuristic).
	double f_cost = 0.;
	//! Index of the parent node in the search path (used for path reconstruction).
	int32_t parent_index = 0;
	//! Flag indicating whether this node has been fully explored (closed) in the A*  search.
	bool closed = false;
} nav_search_node_t;



/** 
* 
* 
* 
*    Navigation Pathfinding Node Methods:
* 
* 
* 
**/
/** 
*    @brief  Select the best layer index for a cell based on desired height.
* 	@param  mesh                Navigation mesh (for Z quantization).
* 	@param  cell                Navigation XY cell to select layer from.
* 	@param  desired_z           Desired Z height to match.
* 	@param  out_layer_index     Output selected layer index.
* 	@return True if a suitable layer index was found.
**/
const bool Nav_SelectLayerIndex( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, double desired_z,
	int32_t *out_layer_index );
/** 
* 	@brief  Advance along a navigation path based on current origin and waypoint radius.
* 	@param  path            Navigation traversal path.
* 	@param  current_origin  Current 3D position of the mover.
* 	@param  waypoint_radius  Radius around waypoints to consider them "reached".
* 	@param  inout_index     Input/output current waypoint index in the path.
* 	@param  out_direction    Output direction vector towards the current waypoint.
* 	@return True if there is a valid waypoint to move towards after advancement.
**/
const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t *path, const Vector3 &current_origin,
	double waypoint_radius, int32_t *inout_index, Vector3 * out_direction );

/** 
*    @brief  Compute the world-space position for a node.
* 	@param  mesh        Navigation mesh.
* 	@param  tile        Tile containing the node.
* 	@param  cell_index  Index of the cell containing the node.
* 	@param  layer       Layer containing the node.
* 	@return World-space position of the node.
* 	@note   The position is at the center of the cell in X/Y and at the layer height in Z.
**/
Vector3 Nav_NodeWorldPosition( const nav_mesh_t *mesh, const nav_tile_t *tile, int32_t cell_index, const nav_layer_t *layer );

/** 
*    @brief  Find a navigation node in a leaf at the given position.
* 	@param  mesh        Navigation mesh.
* 	@param  leaf_data   Leaf data containing tile references.
* 	@param  leaf_index  Index of the leaf in the mesh.
* 	@param  position    World-space position to query.
* 	@param  desired_z   Desired Z height for layer selection.
* 	@param  out_node    Output node reference if found.
* 	@param  allow_fallback  If true, allows returning a node even if traversal checks fail.
* 	@return True if a node was found at the position.
**/
static bool Nav_FindNodeInLeaf( const nav_mesh_t *mesh, const nav_leaf_data_t *leaf_data, int32_t leaf_index,
	const Vector3 &position, double desired_z, nav_node_ref_t *out_node,
	bool allow_fallback );

/** 
* 	@brief	Select the best layer index for a cell based on a blend of start Z and goal Z.
* 			This helps pathfinding prefer the correct floor when chasing a target above/below.
* 	@param	start_z	Seeker starting Z.
* 	@param	goal_z	Target goal Z.
* 	@note	The blend factor is based on XY distance between seeker and goal: close targets bias toward start_z,
* 			far targets bias toward goal_z.
**/
const bool Nav_SelectLayerIndex_BlendZ( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist, int32_t *out_layer_index );

/** 
* 	@brief  Find a navigation node for a position using BSP leaf lookup with blended Z.
*  			This helps pathfinding prefer the correct floor when chasing a target above/below.
*  			The blend factor is based on XY distance between seeker and goal: close targets bias toward start_z,
*  			far targets bias toward goal_z.
* 	@param  start_z	Seeker starting Z.
* 	@param  goal_z	Target goal Z.
* 	@param  start_pos	Seeker starting position.
* 	@param  goal_pos	Target goal position.
* 	@param  blend_start_dist	Distance at which to start blending toward goal_z.
* 	@param  blend_full_dist	Distance at which to fully use goal_z.
* 	@note	Uses fallback search if direct leaf lookup fails and allow_fallback is true.
**/
const bool Nav_FindNodeForPosition_BlendZ( const nav_mesh_t *mesh, const Vector3 &position, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist,
	nav_node_ref_t *out_node, bool allow_fallback );
/** 
*    @brief  Find a navigation node for a position using BSP leaf lookup.
*  		 Uses fallback search if direct leaf lookup fails and allow_fallback is true.
* 	@param	position	World-space position to query.
*  	@param	desired_z	Desired Z height for layer selection.
*  	@param	out_node	Output node reference.
*  	@param	allow_fallback	Allow fallback search if direct leaf lookup fails.
* 	@return	True if a node was found, false otherwise.
**/
const bool Nav_FindNodeForPosition( const nav_mesh_t *mesh, const Vector3 &position, double desired_z,
	nav_node_ref_t *out_node, bool allow_fallback );



/** 
* 
* 
* 
*    Navigation System "Traversal Operations":
* 
* 
* 
**/
/** 
*    @brief  Generate a traversal path between two world-space origins.
*         Uses the navigation voxelmesh and A*  search to produce waypoints.
*    @param  start_origin    World-space starting origin.
*    @param  goal_origin     World-space destination origin.
*    @param  out_path        Output path result (caller must free).
*    @return True if a path was found, false otherwise.
**/
/** 
* @brief    Generate a traversal path between two world-space origins.
* @param    start_origin    World-space starting origin (feet-origin, i.e. z at feet)
* @param    goal_origin     World-space destination origin (feet-origin)
* @param    out_path        Output path result (caller must free).
* @note     Public traversal APIs accept feet-origin coordinates. Internally
*           these functions convert to nav-center space (apply center Z
*           offset computed from the agent hull) before performing node
*           lookups and A*  so callers do not need to supply a nav-centered
*           position.
 **/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path );
const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, const struct svg_nav_path_policy_t *policy );
/** 
*   @brief  Generate a traversal path between two origins with optional goal Z-layer blending.
*        Enables per-call control over whether the start/goal node selection prefers
*        the goal's Z when far away, preventing stuck navigation when targets are upstairs.
*   @param  start_origin             World-space start origin.
*   @param  goal_origin              World-space goal origin.
*   @param  out_path                 Output path result (caller must free).
*   @param  enable_goal_z_layer_blend  If true, blend start Z toward goal Z based on horizontal distance.
*   @param  blend_start_dist         Distance at which blending begins.
*   @param  blend_full_dist          Distance at which blending fully favors goal Z.
*   @return True if a path was found, false otherwise.
 **/
const bool SVG_Nav_GenerateTraversalPathForOriginEx( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
    const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist );
const bool SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy, const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
    const struct svg_nav_path_process_t *pathProcess = nullptr );
/** 
*    @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
*    @param  path    Path structure to free.
**/
void SVG_Nav_FreeTraversalPath( nav_traversal_path_t *path );

/** 
* 	@brief  Query movement direction along a traversal path.
*         Advances the waypoint index as the caller reaches waypoints.
*    @param  path            Path to follow.
*    @param  current_origin  Current world-space origin.
*    @param  waypoint_radius Radius for waypoint completion.
*    @param  policy          Optional traversal policy for per-agent step/drop limits.
*    @param  inout_index     Current waypoint index (updated on success).
*    @param  out_direction   Output normalized movement direction.
*    @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin, double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 * out_direction );
/** 
*    @brief  Query movement direction while advancing waypoints in 2D and emitting 3D directions.
*         Useful for stair traversal so the vertical component can be used separately from waypoint completion.
*    @param  path            Path to follow.
*    @param  current_origin  Current world-space origin.
*    @param  waypoint_radius Radius for waypoint completion (2D).
*    @param  policy          Optional traversal policy for per-agent step/drop limits.
*    @param  inout_index     Current waypoint index (updated as 2D waypoints are reached).
*    @param  out_direction   Output normalized 3D movement direction.
*    @return True if a valid direction was produced, false if the path is invalid or complete.
**/
const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t *path, const Vector3 &current_origin, double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 * out_direction );



/** 
*  
*  
*  
* 	Navigation System Debug Visualization:
*  
*  
*  
**/
/** 
* 	@brief	Check if navigation debug drawing is enabled and draw so if it is.
**/
void SVG_Nav_DebugDraw( void );



/**
*
*
*
*
* 	Utility Functions:
*
*
*
*
**/
/**
* 	@brief	Will return the path for a level's matching navigation filename.
* 	@return	A string containing the file path + file extension.
**/
const std::string Nav_GetPathForLevelNav( const char *levelName, const bool prependGameFolder = true );
/**
*    @brief  Calculate the world-space size of a tile.
**/
static inline double Nav_TileWorldSize( const nav_mesh_t *mesh ) {
	// Compute world-space size of a nav tile.
	return mesh->cell_size_xy * ( double )mesh->tile_size;
}
/**
*    @brief  Convert world coordinate to tile grid coordinate.
**/
static inline int32_t Nav_WorldToTileCoord( double value, double tile_world_size ) {
	// Map a world coordinate into tile grid coordinate using floor.
	// Add a tiny epsilon to avoid floating-point boundary cases where a value
	// lies exactly on a tile edge and can round down to the previous tile
	// due to precision. NAV_TILE_EPSILON is a small positive value.
	return ( int32_t )floorf( ( value + NAV_TILE_EPSILON ) / tile_world_size );
}
/**
*    @brief  Convert world coordinate to cell index within a tile.
**/
static inline int32_t Nav_WorldToCellCoord( double value, double tile_origin, double cell_size_xy ) {
	// Map a world coordinate into a cell index within its tile.
	// Add a tiny epsilon to handle coordinates lying very close to a cell boundary
	// due to floating point imprecision. This prevents missing a valid cell when
	// the world position is exactly on the boundary.
	return ( int32_t )floorf( ( ( value - tile_origin ) + NAV_TILE_EPSILON ) / cell_size_xy );
}