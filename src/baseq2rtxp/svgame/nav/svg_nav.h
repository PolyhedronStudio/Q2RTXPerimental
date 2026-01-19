/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator
*
*
********************************************************************/
#pragma once

/**
*   @brief  Navigation voxelmesh data structures and generation API.
*
*   The voxelmesh is a sparse per-leaf tiled structure storing walkable
*   surface samples suitable for A* pathfinding. Generation is bbox-independent
*   with configurable parameters via cvars.
**/

// Content flags for nav layers
typedef enum {
    NAV_FLAG_WALKABLE   = (1 << 0),
    NAV_FLAG_WATER      = (1 << 1),
    NAV_FLAG_LAVA       = (1 << 2),
    NAV_FLAG_SLIME      = (1 << 3),
    NAV_FLAG_LADDER     = (1 << 4),
} nav_layer_flags_t;

/**
*   @brief  A single Z layer at an XY position.
*           Stores quantized Z and flags.
**/
typedef struct nav_layer_s {
    int16_t z_quantized;        // Quantized Z position
    uint8_t flags;              // nav_layer_flags_t
    uint8_t clearance;          // Optional clearance in grid cells
} nav_layer_t;

/**
*   @brief  XY cell entry containing multiple Z layers.
**/
typedef struct nav_xy_cell_s {
    int32_t num_layers;
    nav_layer_t *layers;        // Array of Z layers at this XY position
} nav_xy_cell_t;

/**
*   @brief  A single tile covering a region of XY cells.
**/
typedef struct nav_tile_s {
    int32_t tile_x;             // Tile X coordinate
    int32_t tile_y;             // Tile Y coordinate
    uint32_t *presence_bits;    // Bitset for which XY cells are present
    nav_xy_cell_t *cells;       // Array of XY cells (sparse)
} nav_tile_t;

/**
*   @brief  Per-leaf navigation data.
**/
typedef struct nav_leaf_data_s {
    int32_t leaf_index;
    int32_t num_tiles;
    nav_tile_t *tiles;          // Array of tiles in this leaf
} nav_leaf_data_t;

/**
*   @brief  Per inline model navigation data (for brush models).
**/
typedef struct nav_inline_model_data_s {
    int32_t model_index;        // Inline model index (e.g., *1, *2, etc.)
    int32_t num_tiles;
    nav_tile_t *tiles;          // Array of tiles for this model
} nav_inline_model_data_t;

/**
*   @brief  Main navigation mesh structure.
**/
typedef struct nav_mesh_s {
    // World mesh data (per-leaf)
    int32_t num_leafs;
    nav_leaf_data_t *leaf_data; // Array of per-leaf data
    
    // Inline model mesh data (per-model)
    int32_t num_inline_models;
    nav_inline_model_data_t *inline_model_data;
    
    // Generation parameters
    float cell_size_xy;
    float z_quant;
    int32_t tile_size;
    float max_step;
    float max_slope_deg;
    Vector3 agent_mins;
    Vector3 agent_maxs;
    
    // Statistics
    int32_t total_tiles;
    int32_t total_xy_cells;
    int32_t total_layers;
} nav_mesh_t;

/**
*   Navigation mesh global instance.
**/
extern nav_mesh_t *g_nav_mesh;

/**
*   Navigation CVars.
**/
extern cvar_t *nav_cell_size_xy;
extern cvar_t *nav_z_quant;
extern cvar_t *nav_tile_size;
extern cvar_t *nav_max_step;
extern cvar_t *nav_max_slope_deg;
extern cvar_t *nav_agent_mins_x;
extern cvar_t *nav_agent_mins_y;
extern cvar_t *nav_agent_mins_z;
extern cvar_t *nav_agent_maxs_x;
extern cvar_t *nav_agent_maxs_y;
extern cvar_t *nav_agent_maxs_z;

/**
*   @brief  Initialize navigation system and register cvars.
**/
void SVG_Nav_Init( void );

/**
*   @brief  Shutdown navigation system and free memory.
**/
void SVG_Nav_Shutdown( void );

/**
*   @brief  Generate navigation voxelmesh for the current level.
*           This is called by the nav_gen_voxelmesh server command.
**/
void SVG_Nav_GenerateVoxelMesh( void );

/**
*   @brief  Free the current navigation mesh.
**/
void SVG_Nav_FreeMesh( void );
