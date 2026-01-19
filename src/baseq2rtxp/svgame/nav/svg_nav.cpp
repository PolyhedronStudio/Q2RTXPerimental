/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator - Implementation
*
*
********************************************************************/
#include "../svg_local.h"
#include "svg_nav.h"
#include <math.h>

// Constants
static constexpr float DEG_TO_RAD = M_PI / 180.0f;
static constexpr double MICROSECONDS_PER_SECOND = 1000000.0;

/**
*   Global navigation mesh instance.
**/
nav_mesh_t *g_nav_mesh = nullptr;

/**
*   Navigation CVars.
**/
cvar_t *nav_cell_size_xy = nullptr;
cvar_t *nav_z_quant = nullptr;
cvar_t *nav_tile_size = nullptr;
cvar_t *nav_max_step = nullptr;
cvar_t *nav_max_slope_deg = nullptr;
cvar_t *nav_agent_mins_x = nullptr;
cvar_t *nav_agent_mins_y = nullptr;
cvar_t *nav_agent_mins_z = nullptr;
cvar_t *nav_agent_maxs_x = nullptr;
cvar_t *nav_agent_maxs_y = nullptr;
cvar_t *nav_agent_maxs_z = nullptr;

/**
*   @brief  Initialize navigation system and register cvars.
**/
void SVG_Nav_Init( void ) {
    // Register CVars with sensible defaults
    nav_cell_size_xy = gi.cvar( "nav_cell_size_xy", "4", 0 );
    nav_z_quant = gi.cvar( "nav_z_quant", "2", 0 );
    nav_tile_size = gi.cvar( "nav_tile_size", "32", 0 );
    nav_max_step = gi.cvar( "nav_max_step", "18", 0 ); // Matches PM_STEP_MAX_SIZE
    nav_max_slope_deg = gi.cvar( "nav_max_slope_deg", "45.57", 0 ); // acos(0.7) ~= 45.57 degrees, matches PM_STEP_MIN_NORMAL
    
    // Default player standing bbox: mins (-16, -16, -36), maxs (16, 16, 36)
    nav_agent_mins_x = gi.cvar( "nav_agent_mins_x", "-16", 0 );
    nav_agent_mins_y = gi.cvar( "nav_agent_mins_y", "-16", 0 );
    nav_agent_mins_z = gi.cvar( "nav_agent_mins_z", "-36", 0 );
    nav_agent_maxs_x = gi.cvar( "nav_agent_maxs_x", "16", 0 );
    nav_agent_maxs_y = gi.cvar( "nav_agent_maxs_y", "16", 0 );
    nav_agent_maxs_z = gi.cvar( "nav_agent_maxs_z", "36", 0 );
    
    g_nav_mesh = nullptr;
}

/**
*   @brief  Shutdown navigation system and free memory.
**/
void SVG_Nav_Shutdown( void ) {
    SVG_Nav_FreeMesh();
}

/**
*   @brief  Free the current navigation mesh.
**/
void SVG_Nav_FreeMesh( void ) {
    if ( !g_nav_mesh ) {
        return;
    }
    
    // Free world mesh leaf data
    if ( g_nav_mesh->leaf_data ) {
        for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
            nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[i];
            if ( leaf->tiles ) {
                for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
                    nav_tile_t *tile = &leaf->tiles[t];
                    if ( tile->presence_bits ) {
                        gi.TagFree( tile->presence_bits );
                    }
                    if ( tile->cells ) {
                        // Free layers in each cell
                        int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
                        for ( int32_t c = 0; c < cells_per_tile; c++ ) {
                            if ( tile->cells[c].layers ) {
                                gi.TagFree( tile->cells[c].layers );
                            }
                        }
                        gi.TagFree( tile->cells );
                    }
                }
                gi.TagFree( leaf->tiles );
            }
        }
        gi.TagFree( g_nav_mesh->leaf_data );
    }
    
    // Free inline model data
    if ( g_nav_mesh->inline_model_data ) {
        for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
            nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[i];
            if ( model->tiles ) {
                for ( int32_t t = 0; t < model->num_tiles; t++ ) {
                    nav_tile_t *tile = &model->tiles[t];
                    if ( tile->presence_bits ) {
                        gi.TagFree( tile->presence_bits );
                    }
                    if ( tile->cells ) {
                        int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
                        for ( int32_t c = 0; c < cells_per_tile; c++ ) {
                            if ( tile->cells[c].layers ) {
                                gi.TagFree( tile->cells[c].layers );
                            }
                        }
                        gi.TagFree( tile->cells );
                    }
                }
                gi.TagFree( model->tiles );
            }
        }
        gi.TagFree( g_nav_mesh->inline_model_data );
    }
    
    gi.TagFree( g_nav_mesh );
    g_nav_mesh = nullptr;
}

/**
*   @brief  Check if a surface normal is walkable based on slope.
**/
static bool IsWalkableSlope( const Vector3 &normal, float max_slope_deg ) {
    // Convert max_slope_deg to minimum normal Z component
    float min_normal_z = cosf( max_slope_deg * DEG_TO_RAD );
    return normal[2] >= min_normal_z;
}

/**
*   @brief  Detect content flags from trace.
**/
static uint8_t DetectContentFlags( const cm_trace_t &trace ) {
    uint8_t flags = NAV_FLAG_WALKABLE;
    
    // Check for water, lava, slime based on contents
    if ( trace.contents & CM_CONTENTS_WATER ) {
        flags |= NAV_FLAG_WATER;
    }
    if ( trace.contents & CM_CONTENTS_LAVA ) {
        flags |= NAV_FLAG_LAVA;
    }
    if ( trace.contents & CM_CONTENTS_SLIME ) {
        flags |= NAV_FLAG_SLIME;
    }
    
    // TODO: Ladder detection would require additional checks
    
    return flags;
}

/**
*   @brief  Perform downward traces to find walkable layers at an XY position.
*           Returns array of layers and count.
*   @note   This function is not thread-safe due to the static temp_layers array.
*           It should only be called from a single thread (main game thread).
**/
static void FindWalkableLayers( const Vector3 &xy_pos, const Vector3 &mins, const Vector3 &maxs,
                                float z_min, float z_max, float max_step, float max_slope_deg,
                                nav_layer_t **out_layers, int32_t *out_num_layers,
                                bool world_only ) {
    const int32_t MAX_LAYERS = 16; // Reasonable limit per XY column
    // Note: Static array for efficiency, but limits thread-safety
    static nav_layer_t temp_layers[MAX_LAYERS];
    int32_t num_layers = 0;
    
    float current_z = z_max;
    float step_down = max_step * 2.0f; // Search distance for each trace
    
    while ( current_z > z_min && num_layers < MAX_LAYERS ) {
        Vector3 start = xy_pos;
        start[2] = current_z;
        
        Vector3 end = xy_pos;
        end[2] = current_z - step_down;
        
        cm_trace_t trace;
        if ( world_only ) {
            // World-only collision
            trace = gi.trace( &start, &mins, &maxs, &end, nullptr, CM_CONTENTMASK_SOLID );
        } else {
            // Use clip for inline models (handled by caller)
            trace = gi.trace( &start, &mins, &maxs, &end, nullptr, CM_CONTENTMASK_SOLID );
        }
        
        // Check if we hit something and if it's walkable
        if ( trace.fraction < 1.0f && trace.plane.normal[2] > 0.0f ) {
            if ( IsWalkableSlope( trace.plane.normal, max_slope_deg ) ) {
                // Found a walkable surface
                // Note: Assumes nav_z_quant->value is non-zero (validated in cvar registration)
                temp_layers[num_layers].z_quantized = (int16_t)( trace.endpos[2] / nav_z_quant->value );
                temp_layers[num_layers].flags = DetectContentFlags( trace );
                temp_layers[num_layers].clearance = 0; // TODO: Calculate clearance
                num_layers++;
                
                // Continue searching below this surface
                current_z = trace.endpos[2] - 1.0f; // Move slightly below found surface
            } else {
                // Hit a non-walkable surface, continue searching below
                current_z = trace.endpos[2] - 1.0f;
            }
        } else {
            // No hit, move down
            current_z -= step_down;
        }
    }
    
    // Allocate and copy layers
    if ( num_layers > 0 ) {
        *out_layers = (nav_layer_t *)gi.TagMallocz( sizeof(nav_layer_t) * num_layers, TAG_SVGAME_LEVEL );
        memcpy( *out_layers, temp_layers, sizeof(nav_layer_t) * num_layers );
        *out_num_layers = num_layers;
    } else {
        *out_layers = nullptr;
        *out_num_layers = 0;
    }
}

/**
*   @brief  Generate navigation mesh for world (world-only collision).
**/
static void GenerateWorldMesh( nav_mesh_t *mesh ) {
    // Get BSP data
    const model_t *world_model = gi.GetModelDataForHandle( 1 ); // Model 1 is always the world
    if ( !world_model || !world_model->bsp ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
        return;
    }
    
    bsp_t *bsp = world_model->bsp;
    int32_t num_leafs = bsp->numleafs;
    
    gi.cprintf( nullptr, PRINT_HIGH, "Generating world mesh for %d leafs...\n", num_leafs );
    
    // Allocate leaf data array
    mesh->num_leafs = num_leafs;
    mesh->leaf_data = (nav_leaf_data_t *)gi.TagMallocz( sizeof(nav_leaf_data_t) * num_leafs, TAG_SVGAME_LEVEL );
    
    // For now, generate a simple grid in each leaf's bounds
    // TODO: This is a simplified implementation; a full implementation would:
    // 1. Iterate through all leafs
    // 2. For each leaf, create tiles based on leaf bounds
    // 3. For each tile, sample XY grid positions
    // 4. For each XY position, find walkable layers using downward traces
    
    // Placeholder: Just initialize leaf data
    for ( int32_t i = 0; i < num_leafs; i++ ) {
        mesh->leaf_data[i].leaf_index = i;
        mesh->leaf_data[i].num_tiles = 0;
        mesh->leaf_data[i].tiles = nullptr;
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "World mesh generation placeholder complete\n" );
}

/**
*   @brief  Generate navigation mesh for inline BSP models.
**/
static void GenerateInlineModelMesh( nav_mesh_t *mesh ) {
    // Get number of inline models from world model
    const model_t *world_model = gi.GetModelDataForHandle( 1 );
    if ( !world_model || !world_model->bsp ) {
        return;
    }
    
    bsp_t *bsp = world_model->bsp;
    int32_t num_models = bsp->nummodels;
    
    if ( num_models <= 1 ) {
        // No inline models (model 0 is world)
        mesh->num_inline_models = 0;
        mesh->inline_model_data = nullptr;
        return;
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "Generating inline model mesh for %d models...\n", num_models - 1 );
    
    // Allocate inline model data (skip model 0 which is world)
    mesh->num_inline_models = num_models - 1;
    mesh->inline_model_data = (nav_inline_model_data_t *)gi.TagMallocz( 
        sizeof(nav_inline_model_data_t) * mesh->num_inline_models, TAG_SVGAME_LEVEL );
    
    // For each inline model, generate mesh in local space
    for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
        mesh->inline_model_data[i].model_index = i + 1; // Model indices start at 1
        mesh->inline_model_data[i].num_tiles = 0;
        mesh->inline_model_data[i].tiles = nullptr;
        
        // TODO: Generate tiles for this model using gi.clip traces
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "Inline model mesh generation placeholder complete\n" );
}

/**
*   @brief  Generate navigation voxelmesh for the current level.
**/
void SVG_Nav_GenerateVoxelMesh( void ) {
    uint64_t start_time = gi.GetRealTime();
    
    gi.cprintf( nullptr, PRINT_HIGH, "=== Navigation Voxelmesh Generation ===\n" );
    
    // Free existing mesh if any
    SVG_Nav_FreeMesh();
    
    // Allocate new mesh
    g_nav_mesh = (nav_mesh_t *)gi.TagMallocz( sizeof(nav_mesh_t), TAG_SVGAME_LEVEL );
    
    // Store generation parameters
    g_nav_mesh->cell_size_xy = nav_cell_size_xy->value;
    g_nav_mesh->z_quant = nav_z_quant->value;
    g_nav_mesh->tile_size = (int32_t)nav_tile_size->value;
    g_nav_mesh->max_step = nav_max_step->value;
    g_nav_mesh->max_slope_deg = nav_max_slope_deg->value;
    g_nav_mesh->agent_mins = Vector3( nav_agent_mins_x->value, nav_agent_mins_y->value, nav_agent_mins_z->value );
    g_nav_mesh->agent_maxs = Vector3( nav_agent_maxs_x->value, nav_agent_maxs_y->value, nav_agent_maxs_z->value );
    
    // Validate bounding box
    if ( g_nav_mesh->agent_mins[0] >= g_nav_mesh->agent_maxs[0] ||
         g_nav_mesh->agent_mins[1] >= g_nav_mesh->agent_maxs[1] ||
         g_nav_mesh->agent_mins[2] >= g_nav_mesh->agent_maxs[2] ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Error: Invalid agent bounding box (mins must be < maxs on all axes)\n" );
        SVG_Nav_FreeMesh();
        return;
    }
    
    // Validate z_quant is non-zero
    if ( g_nav_mesh->z_quant <= 0.0f ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Error: nav_z_quant must be > 0\n" );
        SVG_Nav_FreeMesh();
        return;
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "Parameters:\n" );
    gi.cprintf( nullptr, PRINT_HIGH, "  cell_size_xy: %.1f\n", g_nav_mesh->cell_size_xy );
    gi.cprintf( nullptr, PRINT_HIGH, "  z_quant: %.1f\n", g_nav_mesh->z_quant );
    gi.cprintf( nullptr, PRINT_HIGH, "  tile_size: %d\n", g_nav_mesh->tile_size );
    gi.cprintf( nullptr, PRINT_HIGH, "  max_step: %.1f\n", g_nav_mesh->max_step );
    gi.cprintf( nullptr, PRINT_HIGH, "  max_slope_deg: %.1f\n", g_nav_mesh->max_slope_deg );
    gi.cprintf( nullptr, PRINT_HIGH, "  agent_mins: (%.1f, %.1f, %.1f)\n", 
                g_nav_mesh->agent_mins[0], g_nav_mesh->agent_mins[1], g_nav_mesh->agent_mins[2] );
    gi.cprintf( nullptr, PRINT_HIGH, "  agent_maxs: (%.1f, %.1f, %.1f)\n", 
                g_nav_mesh->agent_maxs[0], g_nav_mesh->agent_maxs[1], g_nav_mesh->agent_maxs[2] );
    
    // Generate world mesh
    GenerateWorldMesh( g_nav_mesh );
    
    // Generate inline model mesh
    GenerateInlineModelMesh( g_nav_mesh );
    
    // Calculate statistics
    g_nav_mesh->total_tiles = 0;
    g_nav_mesh->total_xy_cells = 0;
    g_nav_mesh->total_layers = 0;
    
    // Count world mesh stats
    for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
        nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[i];
        g_nav_mesh->total_tiles += leaf->num_tiles;
        for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
            nav_tile_t *tile = &leaf->tiles[t];
            int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
            for ( int32_t c = 0; c < cells_per_tile; c++ ) {
                if ( tile->cells[c].num_layers > 0 ) {
                    g_nav_mesh->total_xy_cells++;
                    g_nav_mesh->total_layers += tile->cells[c].num_layers;
                }
            }
        }
    }
    
    // Count inline model stats
    for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
        nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[i];
        g_nav_mesh->total_tiles += model->num_tiles;
        for ( int32_t t = 0; t < model->num_tiles; t++ ) {
            nav_tile_t *tile = &model->tiles[t];
            int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
            for ( int32_t c = 0; c < cells_per_tile; c++ ) {
                if ( tile->cells[c].num_layers > 0 ) {
                    g_nav_mesh->total_xy_cells++;
                    g_nav_mesh->total_layers += tile->cells[c].num_layers;
                }
            }
        }
    }
    
    uint64_t end_time = gi.GetRealTime();
    double build_time_sec = ( end_time - start_time ) / MICROSECONDS_PER_SECOND;
    
    // Print statistics
    gi.cprintf( nullptr, PRINT_HIGH, "\n=== Generation Statistics ===\n" );
    gi.cprintf( nullptr, PRINT_HIGH, "  Leafs processed: %d\n", g_nav_mesh->num_leafs );
    gi.cprintf( nullptr, PRINT_HIGH, "  Inline models: %d\n", g_nav_mesh->num_inline_models );
    gi.cprintf( nullptr, PRINT_HIGH, "  Total tiles: %d\n", g_nav_mesh->total_tiles );
    gi.cprintf( nullptr, PRINT_HIGH, "  Total XY cells: %d\n", g_nav_mesh->total_xy_cells );
    gi.cprintf( nullptr, PRINT_HIGH, "  Total layers: %d\n", g_nav_mesh->total_layers );
    gi.cprintf( nullptr, PRINT_HIGH, "  Build time: %.3f seconds\n", build_time_sec );
    gi.cprintf( nullptr, PRINT_HIGH, "===================================\n" );
}
