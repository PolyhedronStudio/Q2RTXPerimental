/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator
*
*
********************************************************************/
#pragma once


#include "svgame/memory/svg_raiiobject.hpp"

struct svg_base_edict_t;

//! Conversion factor from degrees to radians.
static constexpr double DEG_TO_RAD = M_PI / 180.0f;
//! Conversion factor from microseconds to seconds.
static constexpr double MICROSECONDS_PER_SECOND = 1000000.0;
//! Small epsilon to ensure max bounds map to the correct tile.
static constexpr double NAV_TILE_EPSILON = 0.001f;



/**
*
*
*
*   Navigation Voxelmesh Data Structures:
*
*   The voxelmesh is a sparse per-leaf tiled structure storing walkable
*   surface samples suitable for A* pathfinding. Generation is bbox-independent
*   with configurable parameters via cvars.
*
*
*
**/
/**
*   @brief  Content flags for navigation layers.
*           Used to mark walkable surfaces and environmental hazards.
**/
typedef enum {
    NAV_FLAG_WALKABLE   = (1 << 0), //! Surface is walkable.
    NAV_FLAG_WATER      = (1 << 1), //! Surface is in water.
    NAV_FLAG_LAVA       = (1 << 2), //! Surface is in lava.
    NAV_FLAG_SLIME      = (1 << 3), //! Surface is in slime.
    NAV_FLAG_LADDER     = (1 << 4), //! Surface is a ladder.
} nav_layer_flags_t;

/**
*   @brief  A single Z layer at an XY position.
*           Stores quantized Z and flags for multi-layer navigation support.
**/
typedef struct nav_layer_s {
    //! Quantized Z position.
    int16_t z_quantized;
    //! Content flags (nav_layer_flags_t).
    uint8_t flags;
    //! Optional clearance in grid cells.
    uint8_t clearance;
} nav_layer_t;

/**
*   @brief  Profile describing an agent's navigation hull and constraints.
*   @note   Used to centralize step/drop/slope limits when tuning traversal heuristics.
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
    double drop_cap;
    //! Maximum walkable slope steepness in degrees.
    double max_slope_deg;
} nav_agent_profile_t;

/**
*   @brief  XY cell entry containing multiple Z layers.
*           Supports multi-floor environments where multiple walkable surfaces
*           exist at the same XY coordinate.
**/
typedef struct nav_xy_cell_s {
    //! Number of Z layers at this XY position.
    int32_t num_layers;
    //! Array of Z layers at this XY position.
    nav_layer_t *layers;
} nav_xy_cell_t;

/**
*   @brief  A single tile covering a region of XY cells.
*           Uses sparse storage with presence bitsets for memory efficiency.
**/
typedef struct nav_tile_s {
    //! Tile X coordinate in the grid.
    int32_t tile_x;
    //! Tile Y coordinate in the grid.
    int32_t tile_y;
    //! Bitset for which XY cells are present (sparse storage).
    uint32_t *presence_bits;
    //! Array of XY cells (sparse).
    nav_xy_cell_t *cells;
} nav_tile_t;

/**
*   @brief  Per-leaf navigation data.
*           World mesh is organized per BSP leaf for spatial locality.
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
} nav_leaf_data_t;

/**
*	@brief	World tile key used for canonical world tile storage.
*	@note	World tiles are unique by `(tile_x,tile_y)`.
*/
struct nav_world_tile_key_t {
	int32_t tile_x = 0;
	int32_t tile_y = 0;

	bool operator==( const nav_world_tile_key_t &o ) const {
		return tile_x == o.tile_x && tile_y == o.tile_y;
	}
};

struct nav_world_tile_key_hash_t {
	size_t operator()( const nav_world_tile_key_t &k ) const {
		size_t seed = std::hash<int32_t>{}( k.tile_x );
		seed ^= std::hash<int32_t>{}( k.tile_y ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

/**
*   @brief  Per inline model navigation data (for brush models).
*           Inline models (func_door, func_plat, etc.) are stored separately
*           in their local space for transform application.
**/
typedef struct nav_inline_model_data_s {
    //! Inline model index (e.g., *1, *2, etc.).
    int32_t model_index;
    //! Number of tiles for this model.
    int32_t num_tiles;
    //! Array of tiles for this model.
    nav_tile_t *tiles;
} nav_inline_model_data_t;

typedef struct nav_inline_model_runtime_s {
	//! Inline model index (e.g., *1, *2, etc.).
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

/**
*	Navigation Cluster Graph (Tile-Level):
*
*	This is a coarse adjacency graph over world tiles. It is built after voxelmesh
*	generation and used as a fast hierarchical pre-pass for long routes:
*		1) Find start/goal tiles.
*		2) BFS across tile adjacency to get a tile route.
*		3) Run fine A* restricted to tiles on that route.
*
*	This keeps the fine path quality (still A* on nodes) while dramatically reducing
*	search space on large maps.
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
*	@brief	Refreshes inline model runtime transforms (for movers).
*	@note	Intended to be cheap enough to call once per frame.
**/
void SVG_Nav_RefreshInlineModelRuntime( void );

/**
*   @brief  Build a navigation agent profile using registered nav cvars.
*   @note   Provides default hull/traversal limits when called early in startup.
*   @return Populated nav_agent_profile_t describing hull extents and traversal constraints.
**/
nav_agent_profile_t SVG_Nav_BuildAgentProfileFromCvars( void );

/**
*    @brief    Runtime occupancy entry used for dynamic blocking/penalties.
*/
struct nav_occupancy_entry_t {
	//! Accumulated soft cost (crowd avoidance, biasing).
	int32_t soft_cost = 0;
	//! Hard block flag to prevent traversal entirely.
	bool blocked = false;
};

/**
*   @brief  Main navigation mesh structure.
*           Contains both world mesh (static geometry) and inline model meshes
*           (dynamic brush entities).
**/
typedef struct nav_mesh_s {
	/**
	*	World Boundaries:
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
	*\tDynamic occupancy map keyed by (tile, cell, layer).
	*\tUsed to avoid per-query collision checks against other actors by consulting
	*\tpre-filled occupancy instead. Stores both hard-block and soft-cost data.
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
	*   Inline model runtime (per owning entity):
	*   Not saved; rebuilt during voxelmesh generation.
	**/
	int32_t num_inline_model_runtime;
	nav_inline_model_runtime_t *inline_model_runtime;

	/**
	*	Inline model runtime lookup:
	*		Maps owning entity number -> runtime entry index.
	*		Used to avoid linear scans when resolving runtime transforms for a clip entity.
	*		Rebuilt during voxelmesh generation.
	**/
	std::unordered_map<int32_t, int32_t> inline_model_runtime_index_of;

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
    //! Maximum walkable slope in degrees (matches PM_STEP_MIN_NORMAL).
    double max_slope_deg;
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
} nav_mesh_t;

/**
*	@brief	Lookup an inline-model runtime entry by owning entity number.
*	@param	mesh	Navigation mesh.
*	@param	owner_entnum	Owning entity number (edict->s.number).
*	@return	Pointer to runtime entry if found, otherwise nullptr.
*	@note	This is a fast path used to avoid linear searches over `inline_model_runtime`.
**/
const nav_inline_model_runtime_t *SVG_Nav_GetInlineModelRuntimeForOwnerEntNum( const nav_mesh_t *mesh, const int32_t owner_entnum );

/**
*	@brief	Dump the inline-model runtime index map for debugging.
*	@param	mesh	Navigation mesh.
*	@note	Intended for developer diagnostics; prints to console.
**/
void SVG_Nav_Debug_PrintInlineModelRuntimeIndexMap( const nav_mesh_t *mesh );

/**
*	@brief	Type alias for nav_mesh_t RAII owner using TagMalloc/TagFree.
*	@note	Ensures proper construction/destruction and prevents memory leaks.
**/
using nav_mesh_raii_t = SVG_RAIIObject<nav_mesh_t>;

/**
*	@brief	Clear all dynamic occupancy records on the given mesh.
*/
void SVG_Nav_Occupancy_Clear( nav_mesh_t *mesh );

/**
*	@brief	Add a dynamic occupancy entry for a tile/cell/layer.
*	@param	mesh	Target mesh.
*	@param	tileId	Canonical tile id into `world_tiles`.
*	@param	cellIndex	Cell index inside the tile.
*	@param	layerIndex	Layer index inside the cell.
*	@param	cost	Soft cost to accumulate (default 1).
*	@param	blocked	If true, mark this location as hard blocked.
*/
void SVG_Nav_Occupancy_Add( nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, int32_t cost = 1, bool blocked = false );

/**
*	@brief	Query the dynamic occupancy soft cost for a tile/cell/layer.
*	@return	Accumulated soft cost (0 if none or mesh missing).
*/
int32_t SVG_Nav_Occupancy_SoftCost( const nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex );

/**
*	@brief	Query if a tile/cell/layer is marked as hard blocked.
*	@return	True if blocked.
*/
bool SVG_Nav_Occupancy_Blocked( const nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex );

/**
*   @brief  Path result for navigation traversal queries.
*           Stores world-space waypoints for A* pathfinding results.
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
*
*
*
*   Navigation System Global Variables:
*
*
*
**/
/**
*   @brief  Global navigation mesh instance (RAII owner).
*           Stores the complete navigation data for the current level.
*   @note   Use g_nav_mesh.get() to access the raw pointer when needed.
**/
extern nav_mesh_raii_t g_nav_mesh;

/**
*   @brief  Navigation CVars for generation parameters.
*           These control the voxelmesh generation behavior and agent properties.
**/
extern cvar_t *nav_cell_size_xy;    //! XY grid cell size in world units.
extern cvar_t *nav_z_quant;         //! Z-axis quantization step.
extern cvar_t *nav_tile_size;       //! Number of cells per tile dimension.
extern cvar_t *nav_max_step;        //! Maximum step height.
extern cvar_t *nav_max_drop;        //! Maximum allowed downward traversal drop.
extern cvar_t *nav_drop_cap;        //! Maximum drop height.
extern cvar_t *nav_drop_cap;        //! Cap applied when rejecting excessive drops.
extern cvar_t *nav_max_slope_deg;   //! Maximum walkable slope in degrees.
extern cvar_t *nav_agent_mins_x;    //! Agent bounding box minimum X.
extern cvar_t *nav_agent_mins_y;    //! Agent bounding box minimum Y.
extern cvar_t *nav_agent_mins_z;    //! Agent bounding box minimum Z.
extern cvar_t *nav_agent_maxs_x;    //! Agent bounding box maximum X.
extern cvar_t *nav_agent_maxs_y;    //! Agent bounding box maximum Y.
extern cvar_t *nav_agent_maxs_z;    //! Agent bounding box maximum Z.
// Navigation A* cost tuning CVars (runtime tuning of heuristics)
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
 *  @brief  Profiling and logging control CVars.
 *
 *  @note   `nav_profile_level` controls the verbosity of timing/profiling emitted by
 *          the nav generation routines:
 *              0 = off, 1 = phase-level only, 2 = per-leaf timings, 3 = per-tile timings.
 *
 *  @note   `nav_log_file_enable` and `nav_log_file_name` allow optionally writing
 *          navigation logs to a dedicated file in the `logs/` directory without
 *          changing global console logging behavior.
 */
extern cvar_t *nav_profile_level;      //!< Profiling verbosity level (0..3).
extern cvar_t *nav_log_file_enable;    //!< If non-zero, write nav logs to dedicated file.
extern cvar_t *nav_log_file_name;      //!< Filename (no path/ext) for dedicated nav log (e.g., "nav").

/**
 *  @brief  Public wrapper to build the cluster graph from a mesh.
 *  @note   Implemented in svg_nav.cpp. Generated mesh code may call this
 *          to populate the tile-level cluster graph after generation.
 */
void SVG_Nav_ClusterGraph_BuildFromMesh_World( const nav_mesh_t *mesh );

/**
 *	@brief	Lightweight logging wrapper for navigation subsystem.
 *	@note	Currently forwards to engine console output; allows future redirection to a
 *		centralized logging facility without changing callsites.
 */
void SVG_Nav_Log( const char *fmt, ... );



/**
*
*
*
*   Navigation Helper Functions:
*
*
*
**/
/**
*   @brief  Check if a surface normal is walkable based on slope.
*   @param  normal          Surface normal vector.
*   @param  max_slope_deg   Maximum walkable slope in degrees.
*   @return True if the slope is walkable, false otherwise.
**/
const bool IsWalkableSlope( const Vector3 &normal, double max_slope_deg );



/**
*
*
*
*   Navigation System Initialization/Shutdown:
*
*
*
**/
/**
*   @brief  Initialize navigation system and register cvars.
*           Called after entities have post-spawned to ensure inline models
*           have their proper modelindex set.
**/
void SVG_Nav_Init( void );
/**
*   @brief  Shutdown navigation system and free memory.
*           Called during game shutdown to clean up resources.
**/
void SVG_Nav_Shutdown( void );


/**
*
*
*
*   Inline BSP model Mesh Generation:
*
*
*
**/
/**
*	@brief	Collects all active entities that reference an inline BSP model ("*N").
*			The returned mapping is keyed by inline model index (N).
*	@note	If multiple entities reference the same "*N", the first one encountered is kept.
**/
void Nav_CollectInlineModelEntities( std::unordered_map<int32_t, svg_base_edict_t *> &out_model_to_ent );

/**
*
*
*
*   Navigation System "Voxel Mesh(-es)":
*
*
*
**/
/**
*   @brief  Generate navigation voxelmesh for the current level.
*           This is called by the nav_gen_voxelmesh server command.
*   @note   Can be called multiple times to regenerate the mesh with
*           different parameters.
**/
void SVG_Nav_GenerateVoxelMesh( void );
/**
*   @brief  Free the current navigation mesh.
*           Releases all memory allocated for navigation data.
**/
void SVG_Nav_FreeMesh( void );



/**
*
*
*
*   Navigation Pathfinding Node Methods:
*
*
*
**/
typedef struct nav_node_key_s {
	int32_t leaf_index;
	int32_t tile_index;
	int32_t cell_index;
	int32_t layer_index;

	bool operator==( const nav_node_key_s &other ) const {
		return leaf_index == other.leaf_index &&
			tile_index == other.tile_index &&
			cell_index == other.cell_index &&
			layer_index == other.layer_index;
	}
} nav_node_key_t;

/**
*   @brief  Hash functor for nav_node_key_t.
**/
struct nav_node_key_hash_t {
	size_t operator()( const nav_node_key_t &key ) const {
		size_t seed = std::hash<int32_t>{}( key.leaf_index );
		seed ^= std::hash<int32_t>{}( key.tile_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		seed ^= std::hash<int32_t>{}( key.cell_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		seed ^= std::hash<int32_t>{}( key.layer_index ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

/**
*   @brief  Navigation node reference with world position.
**/
typedef struct nav_node_ref_s {
	nav_node_key_t key;
	Vector3 position;
} nav_node_ref_t;

/**
*   @brief  Search node for A* pathfinding.
**/
typedef struct nav_search_node_s {
	nav_node_ref_t node;
	double g_cost;
	double f_cost;
	int32_t parent_index;
	bool closed;
} nav_search_node_t;


/**
*
*
*
*   Navigation Pathfinding Node Methods:
*
*
*
**/
/**
*   @brief  Select the best layer index for a cell based on desired height.
*	@param  mesh                Navigation mesh (for Z quantization).
*	@param  cell                Navigation XY cell to select layer from.
*	@param  desired_z           Desired Z height to match.
*	@param  out_layer_index     Output selected layer index.
*	@return True if a suitable layer index was found.
**/
const bool Nav_SelectLayerIndex( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, double desired_z,
	int32_t *out_layer_index );
/**
*	@brief  Advance along a navigation path based on current origin and waypoint radius.
*	@param  path            Navigation traversal path.
*	@param  current_origin  Current 3D position of the mover.
*	@param  waypoint_radius  Radius around waypoints to consider them "reached".
*	@param  inout_index     Input/output current waypoint index in the path.
*	@param  out_direction    Output direction vector towards the current waypoint.
*	@return True if there is a valid waypoint to move towards after advancement.
**/
const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t *path, const Vector3 &current_origin,
	double waypoint_radius, int32_t *inout_index, Vector3 *out_direction );

/**
*   @brief  Compute the world-space position for a node.
*	@param  mesh        Navigation mesh.
*	@param  tile        Tile containing the node.
*	@param  cell_index  Index of the cell containing the node.
*	@param  layer       Layer containing the node.
*	@return World-space position of the node.
*	@note   The position is at the center of the cell in X/Y and at the layer height in Z.
**/
static Vector3 Nav_NodeWorldPosition( const nav_mesh_t *mesh, const nav_tile_t *tile, int32_t cell_index, const nav_layer_t *layer );

/**
*   @brief  Find a navigation node in a leaf at the given position.
*	@param  mesh        Navigation mesh.
*	@param  leaf_data   Leaf data containing tile references.
*	@param  leaf_index  Index of the leaf in the mesh.
*	@param  position    World-space position to query.
*	@param  desired_z   Desired Z height for layer selection.
*	@param  out_node    Output node reference if found.
*	@param  allow_fallback  If true, allows returning a node even if traversal checks fail.
*	@return True if a node was found at the position.
**/
static bool Nav_FindNodeInLeaf( const nav_mesh_t *mesh, const nav_leaf_data_t *leaf_data, int32_t leaf_index,
	const Vector3 &position, double desired_z, nav_node_ref_t *out_node,
	bool allow_fallback );

/**
*	@brief	Select the best layer index for a cell based on a blend of start Z and goal Z.
*			This helps pathfinding prefer the correct floor when chasing a target above/below.
*	@param	start_z	Seeker starting Z.
*	@param	goal_z	Target goal Z.
*	@note	The blend factor is based on XY distance between seeker and goal: close targets bias toward start_z,
*			far targets bias toward goal_z.
**/
const bool Nav_SelectLayerIndex_BlendZ( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist, int32_t *out_layer_index );

/**
*	@brief  Find a navigation node for a position using BSP leaf lookup with blended Z.
* 			This helps pathfinding prefer the correct floor when chasing a target above/below.
* 			The blend factor is based on XY distance between seeker and goal: close targets bias toward start_z,
* 			far targets bias toward goal_z.
*	@param  start_z	Seeker starting Z.
*	@param  goal_z	Target goal Z.
*	@param  start_pos	Seeker starting position.
*	@param  goal_pos	Target goal position.
*	@param  blend_start_dist	Distance at which to start blending toward goal_z.
*	@param  blend_full_dist	Distance at which to fully use goal_z.
*	@note	Uses fallback search if direct leaf lookup fails and allow_fallback is true.
**/
const bool Nav_FindNodeForPosition_BlendZ( const nav_mesh_t *mesh, const Vector3 &position, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist,
	nav_node_ref_t *out_node, bool allow_fallback );
/**
*   @brief  Find a navigation node for a position using BSP leaf lookup.
* 		 Uses fallback search if direct leaf lookup fails and allow_fallback is true.
*	@param	position	World-space position to query.
* 	@param	desired_z	Desired Z height for layer selection.
* 	@param	out_node	Output node reference.
* 	@param	allow_fallback	Allow fallback search if direct leaf lookup fails.
*	@return	True if a node was found, false otherwise.
**/
const bool Nav_FindNodeForPosition( const nav_mesh_t *mesh, const Vector3 &position, double desired_z,
	nav_node_ref_t *out_node, bool allow_fallback );



/**
*
*
*
*   Navigation System "Traversal Operations":
*
*
*
**/
/**
*   @brief  Generate a traversal path between two world-space origins.
*           Uses the navigation voxelmesh and A* search to produce waypoints.
*   @param  start_origin    World-space starting origin.
*   @param  goal_origin     World-space destination origin.
*   @param  out_path        Output path result (caller must free).
*   @return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path );
const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, const struct svg_nav_path_policy_t *policy );
/**
 *   @brief  Generate a traversal path between two origins with optional goal Z-layer blending.
 *           Enables per-call control over whether the start/goal node selection prefers
 *           the goal's Z when far away, preventing stuck navigation when targets are upstairs.
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
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin, double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 *out_direction );
/**
 *   @brief  Query movement direction while advancing waypoints in 2D and emitting 3D directions.
 *           Useful for stair traversal so the vertical component can be used separately from waypoint completion.
 *   @param  path            Path to follow.
 *   @param  current_origin  Current world-space origin.
 *   @param  waypoint_radius Radius for waypoint completion (2D).
 *   @param  policy          Optional traversal policy for per-agent step/drop limits.
 *   @param  inout_index     Current waypoint index (updated as 2D waypoints are reached).
 *   @param  out_direction   Output normalized 3D movement direction.
 *   @return True if a valid direction was produced, false if the path is invalid or complete.
 **/
const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t *path, const Vector3 &current_origin, double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 *out_direction );



/**
* 
* 
* 
*	Navigation System Debug Visualization:
* 
* 
* 
**/
/**
*	@brief	Check if navigation debug drawing is enabled and draw so if it is.
**/
void SVG_Nav_DebugDraw( void );


/**
*
*
*
*
*	Utility Functions:
*
*
*
*
**/
/**
*   @brief  Calculate the world-space size of a tile.
**/
static inline double Nav_TileWorldSize( const nav_mesh_t *mesh ) {
	// Compute world-space size of a nav tile.
	return mesh->cell_size_xy * ( double )mesh->tile_size;
}
/**
*   @brief  Convert world coordinate to tile grid coordinate.
**/
static inline int32_t Nav_WorldToTileCoord( double value, double tile_world_size ) {
	// Map a world coordinate into tile grid coordinate using floor.
	// Add a tiny epsilon to avoid floating-point boundary cases where a value
	// lies exactly on a tile edge and can round down to the previous tile
	// due to precision. NAV_TILE_EPSILON is a small positive value.
	return ( int32_t )floorf( ( value + NAV_TILE_EPSILON ) / tile_world_size );
}
/**
*   @brief  Convert world coordinate to cell index within a tile.
**/
static inline int32_t Nav_WorldToCellCoord( double value, double tile_origin, double cell_size_xy ) {
	// Map a world coordinate into a cell index within its tile.
	// Add a tiny epsilon to handle coordinates lying very close to a cell boundary
	// due to floating point imprecision. This prevents missing a valid cell when
	// the world position is exactly on the boundary.
	return ( int32_t )floorf( ( ( value - tile_origin ) + NAV_TILE_EPSILON ) / cell_size_xy );
}