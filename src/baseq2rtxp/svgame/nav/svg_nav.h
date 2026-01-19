/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator
*
*
********************************************************************/
#pragma once



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
    //! Array of tiles in this leaf.
    nav_tile_t *tiles;
} nav_leaf_data_t;

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

/**
*   @brief  Main navigation mesh structure.
*           Contains both world mesh (static geometry) and inline model meshes
*           (dynamic brush entities).
**/
typedef struct nav_mesh_s {
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
    *   Generation parameters:
    **/
    //! XY grid cell size.
    float cell_size_xy;
    //! Z-axis quantization step.
    float z_quant;
    //! Number of cells per tile dimension.
    int32_t tile_size;
    //! Maximum step height (matches PM_STEP_MAX_SIZE).
    float max_step;
    //! Maximum walkable slope in degrees (matches PM_STEP_MIN_NORMAL).
    float max_slope_deg;
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
*   @brief  Path result for navigation traversal queries.
*           Stores world-space waypoints for A* pathfinding results.
**/
typedef struct nav_traversal_path_s {
    //! Number of path points in the traversal path.
    int32_t num_points;
    //! Array of world-space points.
    Vector3 *points;
} nav_traversal_path_t;

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
*   @brief  Global navigation mesh instance.
*           Stores the complete navigation data for the current level.
**/
extern nav_mesh_t *g_nav_mesh;

/**
*   @brief  Navigation CVars for generation parameters.
*           These control the voxelmesh generation behavior and agent properties.
**/
extern cvar_t *nav_cell_size_xy;    //! XY grid cell size in world units.
extern cvar_t *nav_z_quant;         //! Z-axis quantization step.
extern cvar_t *nav_tile_size;       //! Number of cells per tile dimension.
extern cvar_t *nav_max_step;        //! Maximum step height.
extern cvar_t *nav_max_slope_deg;   //! Maximum walkable slope in degrees.
extern cvar_t *nav_agent_mins_x;    //! Agent bounding box minimum X.
extern cvar_t *nav_agent_mins_y;    //! Agent bounding box minimum Y.
extern cvar_t *nav_agent_mins_z;    //! Agent bounding box minimum Z.
extern cvar_t *nav_agent_maxs_x;    //! Agent bounding box maximum X.
extern cvar_t *nav_agent_maxs_y;    //! Agent bounding box maximum Y.
extern cvar_t *nav_agent_maxs_z;    //! Agent bounding box maximum Z.



/**
*
*
*
*   Navigation System Functions:
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
*   @brief  Generate a traversal path between two world-space origins.
*           Uses the navigation voxelmesh and A* search to produce waypoints.
*   @param  start_origin    World-space starting origin.
*   @param  goal_origin     World-space destination origin.
*   @param  out_path        Output path result (caller must free).
*   @return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path );

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
*   @param  inout_index     Current waypoint index (updated on success).
*   @param  out_direction   Output normalized movement direction.
*   @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin, float waypoint_radius, int32_t *inout_index, Vector3 *out_direction );
