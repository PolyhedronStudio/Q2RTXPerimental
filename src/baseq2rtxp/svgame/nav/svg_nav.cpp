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



/**
*
*
*
*   Navigation Constants:
*
*
*
**/
//! Conversion factor from degrees to radians.
static constexpr float DEG_TO_RAD = M_PI / 180.0f;
//! Conversion factor from microseconds to seconds.
static constexpr double MICROSECONDS_PER_SECOND = 1000000.0;



/**
*
*
*
*   Navigation Global Variables:
*
*
*
**/
/**
*   @brief  Global navigation mesh instance.
*           Stores the complete navigation data for the current level.
**/
nav_mesh_t *g_nav_mesh = nullptr;

/**
*   @brief  Navigation CVars for generation parameters.
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
void SVG_Nav_Init( void ) {
    /**
    *   Register grid and quantization CVars with sensible defaults:
    **/
    nav_cell_size_xy = gi.cvar( "nav_cell_size_xy", "4", 0 );
    nav_z_quant = gi.cvar( "nav_z_quant", "2", 0 );
    nav_tile_size = gi.cvar( "nav_tile_size", "32", 0 );
    
    /**
    *   Register physics constraint CVars matching player movement:
    **/
    // Matches PM_STEP_MAX_SIZE (18 units).
    nav_max_step = gi.cvar( "nav_max_step", "18", 0 );
    // acos(0.7) ~= 45.57 degrees, matches PM_STEP_MIN_NORMAL.
    nav_max_slope_deg = gi.cvar( "nav_max_slope_deg", "45.57", 0 );
    
    /**
    *   Register agent bounding box CVars:
    *   Default player standing bbox: mins (-16, -16, -36), maxs (16, 16, 36).
    **/
    nav_agent_mins_x = gi.cvar( "nav_agent_mins_x", "-16", 0 );
    nav_agent_mins_y = gi.cvar( "nav_agent_mins_y", "-16", 0 );
    nav_agent_mins_z = gi.cvar( "nav_agent_mins_z", "-36", 0 );
    nav_agent_maxs_x = gi.cvar( "nav_agent_maxs_x", "16", 0 );
    nav_agent_maxs_y = gi.cvar( "nav_agent_maxs_y", "16", 0 );
    nav_agent_maxs_z = gi.cvar( "nav_agent_maxs_z", "36", 0 );
    
    // Initialize global navigation mesh pointer.
    g_nav_mesh = nullptr;
}

/**
*   @brief  Shutdown navigation system and free memory.
*           Called during game shutdown to clean up resources.
**/
void SVG_Nav_Shutdown( void ) {
    SVG_Nav_FreeMesh();
}

/**
*
*
*
*   Navigation Memory Management:
*
*
*
**/
/**
*   @brief  Free the current navigation mesh.
*           Releases all memory allocated for navigation data using TAG_SVGAME_LEVEL.
**/
void SVG_Nav_FreeMesh( void ) {
    // Early out if no mesh exists.
    if ( !g_nav_mesh ) {
        return;
    }
    
    /**
    *   Free world mesh leaf data:
    **/
    if ( g_nav_mesh->leaf_data ) {
        // Iterate through all leafs.
        for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
            nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[i];
            
            // Free tiles in this leaf.
            if ( leaf->tiles ) {
                for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
                    nav_tile_t *tile = &leaf->tiles[t];
                    
                    // Free presence bitset.
                    if ( tile->presence_bits ) {
                        gi.TagFree( tile->presence_bits );
                    }
                    
                    // Free XY cells and their layers.
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
                gi.TagFree( leaf->tiles );
            }
        }
        gi.TagFree( g_nav_mesh->leaf_data );
    }
    
    /**
    *   Free inline model data:
    **/
    if ( g_nav_mesh->inline_model_data ) {
        // Iterate through all inline models.
        for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
            nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[i];
            
            // Free tiles in this model.
            if ( model->tiles ) {
                for ( int32_t t = 0; t < model->num_tiles; t++ ) {
                    nav_tile_t *tile = &model->tiles[t];
                    
                    // Free presence bitset.
                    if ( tile->presence_bits ) {
                        gi.TagFree( tile->presence_bits );
                    }
                    
                    // Free XY cells and their layers.
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
    
    // Free the mesh structure itself.
    gi.TagFree( g_nav_mesh );
    g_nav_mesh = nullptr;
}



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
static bool IsWalkableSlope( const Vector3 &normal, float max_slope_deg ) {
    // Convert max_slope_deg to minimum normal Z component.
    float min_normal_z = cosf( max_slope_deg * DEG_TO_RAD );
    // Surface is walkable if normal Z is above the threshold.
    return normal[2] >= min_normal_z;
}

/**
*   @brief  Detect content flags from trace results.
*   @param  trace   Trace result containing content information.
*   @return Content flags (nav_layer_flags_t) for the traced surface.
**/
static uint8_t DetectContentFlags( const cm_trace_t &trace ) {
    // Start with walkable flag.
    uint8_t flags = NAV_FLAG_WALKABLE;
    
    // Check for water based on trace contents.
    if ( trace.contents & CM_CONTENTS_WATER ) {
        flags |= NAV_FLAG_WATER;
    }
    // Check for lava based on trace contents.
    if ( trace.contents & CM_CONTENTS_LAVA ) {
        flags |= NAV_FLAG_LAVA;
    }
    // Check for slime based on trace contents.
    if ( trace.contents & CM_CONTENTS_SLIME ) {
        flags |= NAV_FLAG_SLIME;
    }
    
    // TODO: Ladder detection would require additional checks.
    
    return flags;
}

/**
*   @brief  Perform downward traces to find walkable layers at an XY position.
*           Supports multi-floor environments by detecting all walkable surfaces
*           at the same XY coordinate.
*   @param  xy_pos          XY position to trace at.
*   @param  mins            Agent bounding box minimum.
*   @param  maxs            Agent bounding box maximum.
*   @param  z_min           Minimum Z height to search.
*   @param  z_max           Maximum Z height to search.
*   @param  max_step        Maximum step height for downward traces.
*   @param  max_slope_deg   Maximum walkable slope in degrees.
*   @param  out_layers      Output array of detected layers.
*   @param  out_num_layers  Output number of layers found.
*   @param  world_only      If true, use world-only collision traces.
*   @note   This function is not thread-safe due to the static temp_layers array.
*           It should only be called from a single thread (main game thread).
**/
static void FindWalkableLayers( const Vector3 &xy_pos, const Vector3 &mins, const Vector3 &maxs,
                                float z_min, float z_max, float max_step, float max_slope_deg,
                                nav_layer_t **out_layers, int32_t *out_num_layers,
                                bool world_only ) {
    // Maximum number of layers that can be found at a single XY position.
    const int32_t MAX_LAYERS = 16;
    // Note: Static array for efficiency, but limits thread-safety.
    static nav_layer_t temp_layers[MAX_LAYERS];
    int32_t num_layers = 0;
    
    // Start at maximum Z and work downward.
    float current_z = z_max;
    // Search distance for each downward trace (2x max step).
    float step_down = max_step * 2.0f;
    
    /**
    *   Perform repeated downward traces to find all walkable layers:
    **/
    while ( current_z > z_min && num_layers < MAX_LAYERS ) {
        // Setup trace start position at current Z height.
        Vector3 start = xy_pos;
        start[2] = current_z;
        
        // Setup trace end position stepping down.
        Vector3 end = xy_pos;
        end[2] = current_z - step_down;
        
        // Perform the trace (world-only or full collision).
        cm_trace_t trace;
        if ( world_only ) {
            // World-only collision for static world geometry.
            trace = gi.trace( &start, &mins, &maxs, &end, nullptr, CM_CONTENTMASK_SOLID );
        } else {
            // Full collision including inline models (for brush entities).
            trace = gi.trace( &start, &mins, &maxs, &end, nullptr, CM_CONTENTMASK_SOLID );
        }
        
        /**
        *   Process trace results:
        **/
        // Check if we hit something and if the normal points upward.
        if ( trace.fraction < 1.0f && trace.plane.normal[2] > 0.0f ) {
            // Check if the slope is walkable.
            if ( IsWalkableSlope( trace.plane.normal, max_slope_deg ) ) {
                // Found a walkable surface - record it.
                // Note: Assumes nav_z_quant->value is non-zero (validated during generation).
                temp_layers[num_layers].z_quantized = (int16_t)( trace.endpos[2] / nav_z_quant->value );
                temp_layers[num_layers].flags = DetectContentFlags( trace );
                temp_layers[num_layers].clearance = 0; // TODO: Calculate clearance.
                num_layers++;
                
                // Continue searching below this surface.
                current_z = trace.endpos[2] - 1.0f;
            } else {
                // Hit a non-walkable surface (too steep), continue searching below.
                current_z = trace.endpos[2] - 1.0f;
            }
        } else {
            // No hit in this trace, move down by the step distance.
            current_z -= step_down;
        }
    }
    
    /**
    *   Allocate and return layer data:
    **/
    if ( num_layers > 0 ) {
        // Allocate permanent storage for the layers.
        *out_layers = (nav_layer_t *)gi.TagMallocz( sizeof(nav_layer_t) * num_layers, TAG_SVGAME_LEVEL );
        memcpy( *out_layers, temp_layers, sizeof(nav_layer_t) * num_layers );
        *out_num_layers = num_layers;
    } else {
        // No walkable layers found at this XY position.
        *out_layers = nullptr;
        *out_num_layers = 0;
    }
}



/**
*
*
*
*   Navigation Mesh Generation:
*
*
*
**/
/**
*   @brief  Generate navigation mesh for world (world-only collision).
*           Creates tiles for each BSP leaf containing walkable surface samples.
*   @param  mesh    Navigation mesh structure to populate.
**/
static void GenerateWorldMesh( nav_mesh_t *mesh ) {
    /**
    *   Get BSP data from the world model:
    **/
    // Model handle 1 is always the world.
    const model_t *world_model = gi.GetModelDataForHandle( 1 );
    if ( !world_model || !world_model->bsp ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
        return;
    }
    
    bsp_t *bsp = world_model->bsp;
    int32_t num_leafs = bsp->numleafs;
    
    gi.cprintf( nullptr, PRINT_HIGH, "Generating world mesh for %d leafs...\n", num_leafs );
    
    /**
    *   Allocate leaf data array:
    **/
    mesh->num_leafs = num_leafs;
    mesh->leaf_data = (nav_leaf_data_t *)gi.TagMallocz( sizeof(nav_leaf_data_t) * num_leafs, TAG_SVGAME_LEVEL );
    
    /**
    *   Generate tiles for each leaf:
    *   TODO: This is a simplified placeholder implementation. A full implementation would:
    *   1. Iterate through all leafs
    *   2. For each leaf, create tiles based on leaf bounds
    *   3. For each tile, sample XY grid positions
    *   4. For each XY position, find walkable layers using downward traces
    **/
    for ( int32_t i = 0; i < num_leafs; i++ ) {
        mesh->leaf_data[i].leaf_index = i;
        mesh->leaf_data[i].num_tiles = 0;
        mesh->leaf_data[i].tiles = nullptr;
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "World mesh generation placeholder complete\n" );
}

/**
*   @brief  Generate navigation mesh for inline BSP models (brush entities).
*           Creates tiles for each inline model in local space for later transform application.
*   @param  mesh    Navigation mesh structure to populate.
**/
static void GenerateInlineModelMesh( nav_mesh_t *mesh ) {
    /**
    *   Get number of inline models from world model:
    **/
    const model_t *world_model = gi.GetModelDataForHandle( 1 );
    if ( !world_model || !world_model->bsp ) {
        return;
    }
    
    bsp_t *bsp = world_model->bsp;
    int32_t num_models = bsp->nummodels;
    
    // Check if we have any inline models (model 0 is world).
    if ( num_models <= 1 ) {
        mesh->num_inline_models = 0;
        mesh->inline_model_data = nullptr;
        return;
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "Generating inline model mesh for %d models...\n", num_models - 1 );
    
    /**
    *   Allocate inline model data (skip model 0 which is world):
    **/
    mesh->num_inline_models = num_models - 1;
    mesh->inline_model_data = (nav_inline_model_data_t *)gi.TagMallocz( 
        sizeof(nav_inline_model_data_t) * mesh->num_inline_models, TAG_SVGAME_LEVEL );
    
    /**
    *   Generate mesh for each inline model in local space:
    *   TODO: This is a simplified placeholder implementation. A full implementation would:
    *   1. Iterate through inline models
    *   2. For each model, extract bounds from mmodel_t
    *   3. Create tiles in model local space
    *   4. Use gi.clip() with model entity for collision tests
    *   5. Store results keyed by model index
    **/
    for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
        // Model indices start at 1 (0 is world).
        mesh->inline_model_data[i].model_index = i + 1;
        mesh->inline_model_data[i].num_tiles = 0;
        mesh->inline_model_data[i].tiles = nullptr;
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "Inline model mesh generation placeholder complete\n" );
}

/**
*   @brief  Generate navigation voxelmesh for the current level.
*           This is the main entry point called by the nav_gen_voxelmesh server command.
*           Generates both world mesh and inline model meshes with statistics reporting.
**/
void SVG_Nav_GenerateVoxelMesh( void ) {
    // Record start time for statistics.
    uint64_t start_time = gi.GetRealTime();
    
    gi.cprintf( nullptr, PRINT_HIGH, "=== Navigation Voxelmesh Generation ===\n" );
    
    /**
    *   Free existing mesh if any:
    **/
    SVG_Nav_FreeMesh();
    
    /**
    *   Allocate new mesh structure:
    **/
    g_nav_mesh = (nav_mesh_t *)gi.TagMallocz( sizeof(nav_mesh_t), TAG_SVGAME_LEVEL );
    
    /**
    *   Store generation parameters from CVars:
    **/
    g_nav_mesh->cell_size_xy = nav_cell_size_xy->value;
    g_nav_mesh->z_quant = nav_z_quant->value;
    g_nav_mesh->tile_size = (int32_t)nav_tile_size->value;
    g_nav_mesh->max_step = nav_max_step->value;
    g_nav_mesh->max_slope_deg = nav_max_slope_deg->value;
    g_nav_mesh->agent_mins = Vector3( nav_agent_mins_x->value, nav_agent_mins_y->value, nav_agent_mins_z->value );
    g_nav_mesh->agent_maxs = Vector3( nav_agent_maxs_x->value, nav_agent_maxs_y->value, nav_agent_maxs_z->value );
    
    /**
    *   Validate bounding box parameters:
    **/
    if ( g_nav_mesh->agent_mins[0] >= g_nav_mesh->agent_maxs[0] ||
         g_nav_mesh->agent_mins[1] >= g_nav_mesh->agent_maxs[1] ||
         g_nav_mesh->agent_mins[2] >= g_nav_mesh->agent_maxs[2] ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Error: Invalid agent bounding box (mins must be < maxs on all axes)\n" );
        SVG_Nav_FreeMesh();
        return;
    }
    
    /**
    *   Validate Z quantization parameter:
    **/
    if ( g_nav_mesh->z_quant <= 0.0f ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Error: nav_z_quant must be > 0\n" );
        SVG_Nav_FreeMesh();
        return;
    }
    
    /**
    *   Print generation parameters:
    **/
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
    
    /**
    *   Generate world mesh (static geometry):
    **/
    GenerateWorldMesh( g_nav_mesh );
    
    /**
    *   Generate inline model mesh (brush entities):
    **/
    GenerateInlineModelMesh( g_nav_mesh );
    
    /**
    *   Calculate statistics from generated data:
    **/
    g_nav_mesh->total_tiles = 0;
    g_nav_mesh->total_xy_cells = 0;
    g_nav_mesh->total_layers = 0;
    
    // Count world mesh stats.
    for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
        nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[i];
        g_nav_mesh->total_tiles += leaf->num_tiles;
        
        // Count cells and layers in each tile.
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
    
    // Count inline model stats.
    for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
        nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[i];
        g_nav_mesh->total_tiles += model->num_tiles;
        
        // Count cells and layers in each tile.
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
    
    /**
    *   Calculate and print generation statistics:
    **/
    uint64_t end_time = gi.GetRealTime();
    double build_time_sec = ( end_time - start_time ) / MICROSECONDS_PER_SECOND;
    
    gi.cprintf( nullptr, PRINT_HIGH, "\n=== Generation Statistics ===\n" );
    gi.cprintf( nullptr, PRINT_HIGH, "  Leafs processed: %d\n", g_nav_mesh->num_leafs );
    gi.cprintf( nullptr, PRINT_HIGH, "  Inline models: %d\n", g_nav_mesh->num_inline_models );
    gi.cprintf( nullptr, PRINT_HIGH, "  Total tiles: %d\n", g_nav_mesh->total_tiles );
    gi.cprintf( nullptr, PRINT_HIGH, "  Total XY cells: %d\n", g_nav_mesh->total_xy_cells );
    gi.cprintf( nullptr, PRINT_HIGH, "  Total layers: %d\n", g_nav_mesh->total_layers );
    gi.cprintf( nullptr, PRINT_HIGH, "  Build time: %.3f seconds\n", build_time_sec );
    gi.cprintf( nullptr, PRINT_HIGH, "===================================\n" );
}
