/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator - Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "common/bsp.h"
#include "svg_nav.h"
#include <math.h>
#include <limits>
#include <unordered_map>
#include <vector>



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
    if ( trace.contents & CONTENTS_WATER ) {
        flags |= NAV_FLAG_WATER;
    }
    // Check for lava based on trace contents.
    if ( trace.contents & CONTENTS_LAVA ) {
        flags |= NAV_FLAG_LAVA;
    }
    // Check for slime based on trace contents.
    if ( trace.contents & CONTENTS_SLIME ) {
        flags |= NAV_FLAG_SLIME;
    }
    // Check for ladder based on trace contents.
    if ( trace.contents & CONTENTS_LADDER ) {
        flags |= NAV_FLAG_LADDER;
    }
    
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
 *   @param  z_quant         Z-axis quantization step.
 *   @param  out_layers      Output array of detected layers.
 *   @param  out_num_layers  Output number of layers found.
 *   @param  clip_entity     Optional entity to clip against (inline model), null for world.
*   @note   This function is not thread-safe due to the static temp_layers array.
*           It should only be called from a single thread (main game thread).
**/
static void FindWalkableLayers( const Vector3 &xy_pos, const Vector3 &mins, const Vector3 &maxs,
                                float z_min, float z_max, float max_step, float max_slope_deg, float z_quant,
                                nav_layer_t **out_layers, int32_t *out_num_layers,
                                const edict_ptr_t *clip_entity ) {
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
        if ( clip_entity ) {
            // Inline model collision using the entity-specific clip.
            trace = gi.clip( clip_entity, &start, &mins, &maxs, &end, CM_CONTENTMASK_SOLID );
        } else {
            // World-only collision for static world geometry.
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
                // Note: Assumes z_quant is non-zero (validated during generation).
                temp_layers[num_layers].z_quantized = (int16_t)( trace.endpos[2] / z_quant );
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
*   Navigation Tile Helpers:
*
*
*
**/
//! Small epsilon to ensure max bounds map to the correct tile.
static constexpr float NAV_TILE_EPSILON = 0.001f;

/**
*   @brief  Calculate the world-space size of a tile.
**/
static inline float Nav_TileWorldSize( const nav_mesh_t *mesh ) {
    return mesh->cell_size_xy * (float)mesh->tile_size;
}

/**
*   @brief  Convert world coordinate to tile grid coordinate.
**/
static inline int32_t Nav_WorldToTileCoord( float value, float tile_world_size ) {
    return (int32_t)floorf( value / tile_world_size );
}

/**
*   @brief  Convert world coordinate to cell index within a tile.
**/
static inline int32_t Nav_WorldToCellCoord( float value, float tile_origin, float cell_size_xy ) {
    return (int32_t)floorf( ( value - tile_origin ) / cell_size_xy );
}

/**
*   @brief  Set a presence bit for a cell index within a tile.
**/
static inline void Nav_SetPresenceBit( nav_tile_t *tile, int32_t cell_index ) {
    const int32_t word_index = cell_index >> 5;
    const int32_t bit_index = cell_index & 31;
    tile->presence_bits[ word_index ] |= ( 1u << bit_index );
}

/**
*   @brief  Free tile cell allocations (layers + arrays).
**/
static void Nav_FreeTileCells( nav_tile_t *tile, int32_t cells_per_tile ) {
    if ( !tile->cells ) {
        return;
    }
    
    for ( int32_t c = 0; c < cells_per_tile; c++ ) {
        if ( tile->cells[ c ].layers ) {
            gi.TagFree( tile->cells[ c ].layers );
        }
    }
    
    gi.TagFree( tile->cells );
    tile->cells = nullptr;
}

/**
*   @brief  Build a navigation tile with sampled walkable layers.
*   @return True if any walkable data was generated.
**/
static bool Nav_BuildTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &leaf_mins, const Vector3 &leaf_maxs,
                           float z_min, float z_max ) {
    const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
    const int32_t presence_words = ( cells_per_tile + 31 ) / 32;
    const float tile_world_size = Nav_TileWorldSize( mesh );
    
    // Allocate tile storage.
    tile->presence_bits = (uint32_t *)gi.TagMallocz( sizeof( uint32_t ) * presence_words, TAG_SVGAME_LEVEL );
    tile->cells = (nav_xy_cell_t *)gi.TagMallocz( sizeof( nav_xy_cell_t ) * cells_per_tile, TAG_SVGAME_LEVEL );
    
    bool has_layers = false;
    
    const float tile_origin_x = tile->tile_x * tile_world_size;
    const float tile_origin_y = tile->tile_y * tile_world_size;
    
    for ( int32_t cell_y = 0; cell_y < mesh->tile_size; cell_y++ ) {
        for ( int32_t cell_x = 0; cell_x < mesh->tile_size; cell_x++ ) {
            const float world_x = tile_origin_x + ( (float)cell_x + 0.5f ) * mesh->cell_size_xy;
            const float world_y = tile_origin_y + ( (float)cell_y + 0.5f ) * mesh->cell_size_xy;
            
            // Skip cells outside the leaf bounds.
            if ( world_x < leaf_mins[ 0 ] || world_x > leaf_maxs[ 0 ] ||
                 world_y < leaf_mins[ 1 ] || world_y > leaf_maxs[ 1 ] ) {
                continue;
            }
            
            Vector3 xy_pos = { world_x, world_y, 0.0f };
            nav_layer_t *layers = nullptr;
            int32_t num_layers = 0;
            
            FindWalkableLayers( xy_pos, mesh->agent_mins, mesh->agent_maxs,
                                z_min, z_max, mesh->max_step, mesh->max_slope_deg, mesh->z_quant,
                                &layers, &num_layers, nullptr );
            
            if ( num_layers > 0 ) {
                const int32_t cell_index = cell_y * mesh->tile_size + cell_x;
                tile->cells[ cell_index ].num_layers = num_layers;
                tile->cells[ cell_index ].layers = layers;
                Nav_SetPresenceBit( tile, cell_index );
                has_layers = true;
            }
        }
    }
    
    if ( !has_layers ) {
        Nav_FreeTileCells( tile, cells_per_tile );
        if ( tile->presence_bits ) {
            gi.TagFree( tile->presence_bits );
            tile->presence_bits = nullptr;
        }
    }
    
    return has_layers;
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
	#if 0
    // Model handle 1 is always the world.
    const model_t *world_model = gi.GetModelDataForHandle( 1 );
    if ( !world_model || !world_model->bsp ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
        return;
    }
	#else
	const cm_t *world_model = gi.GetCollisionModel();
	if ( !world_model || !world_model->cache ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
		return;
	}
	#endif
    
    bsp_t *bsp = world_model->cache;
    int32_t num_leafs = bsp->numleafs;
    
    gi.cprintf( nullptr, PRINT_HIGH, "Generating world mesh for %d leafs...\n", num_leafs );
    
    /**
    *   Allocate leaf data array:
    **/
    mesh->num_leafs = num_leafs;
    mesh->leaf_data = (nav_leaf_data_t *)gi.TagMallocz( sizeof(nav_leaf_data_t) * num_leafs, TAG_SVGAME_LEVEL );
    
    const float tile_world_size = Nav_TileWorldSize( mesh );
    
    /**
    *   Generate tiles for each leaf using BSP bounds:
    **/
    for ( int32_t i = 0; i < num_leafs; i++ ) {
        nav_leaf_data_t *leaf_data = &mesh->leaf_data[ i ];
        mleaf_t *leaf = &bsp->leafs[ i ];
        
        leaf_data->leaf_index = i;
        leaf_data->num_tiles = 0;
        leaf_data->tiles = nullptr;
        
        // Skip solid leafs (non-navigable).
        if ( leaf->contents & CONTENTS_SOLID ) {
            continue;
        }
        
        const Vector3 leaf_mins = leaf->mins;
        const Vector3 leaf_maxs = leaf->maxs;
        const float z_min = leaf_mins[ 2 ] + mesh->agent_mins[ 2 ];
        const float z_max = leaf_maxs[ 2 ] + mesh->agent_maxs[ 2 ];
        
        const int32_t tile_min_x = Nav_WorldToTileCoord( leaf_mins[ 0 ], tile_world_size );
        const int32_t tile_max_x = Nav_WorldToTileCoord( leaf_maxs[ 0 ] - NAV_TILE_EPSILON, tile_world_size );
        const int32_t tile_min_y = Nav_WorldToTileCoord( leaf_mins[ 1 ], tile_world_size );
        const int32_t tile_max_y = Nav_WorldToTileCoord( leaf_maxs[ 1 ] - NAV_TILE_EPSILON, tile_world_size );
        
        std::vector<nav_tile_t> leaf_tiles;
        if ( tile_max_x >= tile_min_x && tile_max_y >= tile_min_y ) {
            const int32_t tile_count = ( tile_max_x - tile_min_x + 1 ) * ( tile_max_y - tile_min_y + 1 );
            leaf_tiles.reserve( tile_count );
        }
        
        for ( int32_t tile_y = tile_min_y; tile_y <= tile_max_y; tile_y++ ) {
            for ( int32_t tile_x = tile_min_x; tile_x <= tile_max_x; tile_x++ ) {
                nav_tile_t tile = {};
                tile.tile_x = tile_x;
                tile.tile_y = tile_y;
                
                if ( Nav_BuildTile( mesh, &tile, leaf_mins, leaf_maxs, z_min, z_max ) ) {
                    leaf_tiles.push_back( tile );
                }
            }
        }
        
        if ( !leaf_tiles.empty() ) {
            leaf_data->num_tiles = (int32_t)leaf_tiles.size();
            leaf_data->tiles = (nav_tile_t *)gi.TagMallocz( sizeof( nav_tile_t ) * leaf_data->num_tiles, TAG_SVGAME_LEVEL );
            memcpy( leaf_data->tiles, leaf_tiles.data(), sizeof( nav_tile_t ) * leaf_data->num_tiles );
        }
    }
    
    gi.cprintf( nullptr, PRINT_HIGH, "World mesh generation complete\n" );
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
	const cm_t *world_model = gi.GetCollisionModel();
	if ( !world_model || !world_model->cache ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
		return;
	}
    
    bsp_t *bsp = world_model->cache;
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
*
*
*
*   Navigation Pathfinding Helpers:
*
*
*
**/
/**
*   @brief  Node key identifying a unique navigation layer.
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
    float g_cost;
    float f_cost;
    int32_t parent_index;
    bool closed;
} nav_search_node_t;

/**
*   @brief  Select the best layer index for a cell based on desired height.
**/
static bool Nav_SelectLayerIndex( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, float desired_z,
                                  int32_t *out_layer_index ) {
    if ( !cell || cell->num_layers <= 0 ) {
        return false;
    }
    
    const float max_step = mesh->max_step + mesh->z_quant;
    int32_t best_index = -1;
    float best_delta = std::numeric_limits<float>::max();
    float best_fallback = std::numeric_limits<float>::max();
    int32_t fallback_index = -1;
    
    for ( int32_t i = 0; i < cell->num_layers; i++ ) {
        const float layer_z = cell->layers[ i ].z_quantized * mesh->z_quant;
        const float delta = fabsf( layer_z - desired_z );
        if ( delta < best_fallback ) {
            best_fallback = delta;
            fallback_index = i;
        }
        if ( delta <= max_step && delta < best_delta ) {
            best_delta = delta;
            best_index = i;
        }
    }
    
    if ( best_index < 0 ) {
        best_index = fallback_index;
    }
    
    if ( best_index < 0 ) {
        return false;
    }
    
    *out_layer_index = best_index;
    return true;
}

/**
*   @brief  Compute the world-space position for a node.
**/
static Vector3 Nav_NodeWorldPosition( const nav_mesh_t *mesh, const nav_tile_t *tile, int32_t cell_index, const nav_layer_t *layer ) {
    const float tile_world_size = Nav_TileWorldSize( mesh );
    const float tile_origin_x = tile->tile_x * tile_world_size;
    const float tile_origin_y = tile->tile_y * tile_world_size;
    const int32_t cell_x = cell_index % mesh->tile_size;
    const int32_t cell_y = cell_index / mesh->tile_size;
    
    Vector3 position = {
        tile_origin_x + ( (float)cell_x + 0.5f ) * mesh->cell_size_xy,
        tile_origin_y + ( (float)cell_y + 0.5f ) * mesh->cell_size_xy,
        layer->z_quantized * mesh->z_quant
    };
    return position;
}

/**
*   @brief  Find a navigation node in a leaf at the given position.
**/
static bool Nav_FindNodeInLeaf( const nav_mesh_t *mesh, const nav_leaf_data_t *leaf_data, int32_t leaf_index,
                                const Vector3 &position, float desired_z, nav_node_ref_t *out_node,
                                bool allow_fallback ) {
    if ( !leaf_data || leaf_data->num_tiles <= 0 ) {
        return false;
    }
    
    const float tile_world_size = Nav_TileWorldSize( mesh );
    const int32_t tile_x = Nav_WorldToTileCoord( position[ 0 ], tile_world_size );
    const int32_t tile_y = Nav_WorldToTileCoord( position[ 1 ], tile_world_size );
    
    for ( int32_t t = 0; t < leaf_data->num_tiles; t++ ) {
        nav_tile_t *tile = &leaf_data->tiles[ t ];
        if ( tile->tile_x != tile_x || tile->tile_y != tile_y ) {
            continue;
        }
        
        const float tile_origin_x = tile->tile_x * tile_world_size;
        const float tile_origin_y = tile->tile_y * tile_world_size;
        const int32_t cell_x = Nav_WorldToCellCoord( position[ 0 ], tile_origin_x, mesh->cell_size_xy );
        const int32_t cell_y = Nav_WorldToCellCoord( position[ 1 ], tile_origin_y, mesh->cell_size_xy );
        
        if ( cell_x < 0 || cell_x >= mesh->tile_size || cell_y < 0 || cell_y >= mesh->tile_size ) {
            break;
        }
        
        const int32_t cell_index = cell_y * mesh->tile_size + cell_x;
        const nav_xy_cell_t *cell = &tile->cells[ cell_index ];
        int32_t layer_index = -1;
        
        if ( Nav_SelectLayerIndex( mesh, cell, desired_z, &layer_index ) ) {
            out_node->key = {
                .leaf_index = leaf_index,
                .tile_index = t,
                .cell_index = cell_index,
                .layer_index = layer_index
            };
            out_node->position = Nav_NodeWorldPosition( mesh, tile, cell_index, &cell->layers[ layer_index ] );
            return true;
        }
        
        break;
    }
    
    if ( !allow_fallback ) {
        return false;
    }
    
    float best_dist_sqr = std::numeric_limits<float>::max();
    nav_node_ref_t best_node = {};
    bool found = false;
    
    for ( int32_t t = 0; t < leaf_data->num_tiles; t++ ) {
        nav_tile_t *tile = &leaf_data->tiles[ t ];
        const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
        
        for ( int32_t c = 0; c < cells_per_tile; c++ ) {
            const nav_xy_cell_t *cell = &tile->cells[ c ];
            if ( cell->num_layers <= 0 ) {
                continue;
            }
            
            for ( int32_t l = 0; l < cell->num_layers; l++ ) {
                const Vector3 node_pos = Nav_NodeWorldPosition( mesh, tile, c, &cell->layers[ l ] );
                const Vector3 delta = QM_Vector3Subtract( node_pos, position );
                const float dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] );
                
                if ( dist_sqr < best_dist_sqr ) {
                    best_dist_sqr = dist_sqr;
                    best_node.key = {
                        .leaf_index = leaf_index,
                        .tile_index = t,
                        .cell_index = c,
                        .layer_index = l
                    };
                    best_node.position = node_pos;
                    found = true;
                }
            }
        }
    }
    
    if ( found ) {
        *out_node = best_node;
    }
    
    return found;
}

/**
*   @brief  Find a navigation node for a position using BSP leaf lookup.
**/
static bool Nav_FindNodeForPosition( const nav_mesh_t *mesh, const Vector3 &position, float desired_z,
                                     nav_node_ref_t *out_node, bool allow_fallback ) {
	#if 0
	const model_t *world_model = gi.GetModelDataForHandle( 1 );
    if ( !world_model || !world_model->bsp || !world_model->bsp->nodes ) {
        return false;
    }
	bsp_t *bsp = world_model->bsp;
	#else
	const cm_t *world_model = gi.GetCollisionModel();
	if ( !world_model || !world_model->cache ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
		return false;
	}
	bsp_t *bsp = world_model->cache;
	#endif
    mleaf_t *leaf = gi.BSP_PointLeaf( bsp->nodes, &position[ 0 ] );
    if ( !leaf ) {
        return false;
    }
    
    const ptrdiff_t leaf_index = leaf - bsp->leafs;
    if ( leaf_index < 0 || leaf_index >= mesh->num_leafs ) {
        return false;
    }
    
    if ( Nav_FindNodeInLeaf( mesh, &mesh->leaf_data[ leaf_index ], (int32_t)leaf_index, position, desired_z, out_node, allow_fallback ) ) {
        return true;
    }
    
    if ( !allow_fallback ) {
        return false;
    }
    
    // Fallback: search all leafs for the nearest node.
    float best_dist_sqr = std::numeric_limits<float>::max();
    nav_node_ref_t best_node = {};
    bool found = false;
    
    for ( int32_t i = 0; i < mesh->num_leafs; i++ ) {
        nav_node_ref_t candidate = {};
        if ( Nav_FindNodeInLeaf( mesh, &mesh->leaf_data[ i ], i, position, desired_z, &candidate, true ) ) {
            const Vector3 delta = QM_Vector3Subtract( candidate.position, position );
            const float dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] );
            if ( dist_sqr < best_dist_sqr ) {
                best_dist_sqr = dist_sqr;
                best_node = candidate;
                found = true;
            }
        }
    }
    
    if ( found ) {
        *out_node = best_node;
    }
    
    return found;
}

/**
*   @brief  Generate navigation voxelmesh for the current level.
*           This is the main entry point called by the nav_gen_voxelmesh server command.
*           Generates both world mesh and inline model meshes with statistics reporting.
**/
void SVG_Nav_GenerateVoxelMesh( void ) {
    // Record start time for statistics.
    uint64_t start_time = gi.GetRealSystemTime();
    
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
    uint64_t end_time = gi.GetRealSystemTime();
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

/**
*
*
*
*   Navigation Pathfinding:
*
*
*
**/
/**
*   @brief  Perform A* search between two navigation nodes.
**/
static bool Nav_AStarSearch( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
                             std::vector<Vector3> &out_points ) {
    constexpr int32_t MAX_SEARCH_NODES = 4096;
    static constexpr Vector3 neighbor_offsets[] = {
        { 1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f },
        { 1.0f, -1.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f }
    };
    
    auto heuristic = []( const Vector3 &a, const Vector3 &b ) -> float {
        const Vector3 delta = QM_Vector3Subtract( b, a );
        return sqrtf( ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] ) );
    };
    
    std::vector<nav_search_node_t> nodes;
    nodes.reserve( 256 );
    
    std::vector<int32_t> open_list;
    open_list.reserve( 256 );
    
    std::unordered_map<nav_node_key_t, int32_t, nav_node_key_hash_t> node_lookup;
    node_lookup.reserve( 256 );
    
    nav_search_node_t start_search = {
        .node = start_node,
        .g_cost = 0.0f,
        .f_cost = heuristic( start_node.position, goal_node.position ),
        .parent_index = -1,
        .closed = false
    };
    
    nodes.push_back( start_search );
    open_list.push_back( 0 );
    node_lookup.emplace( start_node.key, 0 );
    
    while ( !open_list.empty() && (int32_t)nodes.size() < MAX_SEARCH_NODES ) {
        int32_t best_open_index = 0;
        float best_f_cost = nodes[ open_list[ 0 ] ].f_cost;
        
        for ( int32_t i = 1; i < (int32_t)open_list.size(); i++ ) {
            const float f_cost = nodes[ open_list[ i ] ].f_cost;
            if ( f_cost < best_f_cost ) {
                best_f_cost = f_cost;
                best_open_index = i;
            }
        }
        
        const int32_t current_index = open_list[ best_open_index ];
        open_list[ best_open_index ] = open_list.back();
        open_list.pop_back();
        
        nav_search_node_t &current = nodes[ current_index ];
        current.closed = true;
        
        if ( current.node.key == goal_node.key ) {
            std::vector<Vector3> reversed_points;
            reversed_points.reserve( 64 );
            
            int32_t trace_index = current_index;
            while ( trace_index >= 0 ) {
                reversed_points.push_back( nodes[ trace_index ].node.position );
                trace_index = nodes[ trace_index ].parent_index;
            }
            
            out_points.assign( reversed_points.rbegin(), reversed_points.rend() );
            return true;
        }
        
        for ( const Vector3 &offset_dir : neighbor_offsets ) {
            const Vector3 neighbor_origin = QM_Vector3Add( current.node.position, QM_Vector3Scale( offset_dir, mesh->cell_size_xy ) );
            nav_node_ref_t neighbor_node = {};
            
            if ( !Nav_FindNodeForPosition( mesh, neighbor_origin, current.node.position[ 2 ], &neighbor_node, false ) ) {
                continue;
            }
            
            const float z_delta = fabsf( neighbor_node.position[ 2 ] - current.node.position[ 2 ] );
            if ( z_delta > ( mesh->max_step + mesh->z_quant ) ) {
                continue;
            }
            
            const float step_cost = heuristic( current.node.position, neighbor_node.position );
            const float tentative_g = current.g_cost + step_cost;
            
            auto lookup_it = node_lookup.find( neighbor_node.key );
            if ( lookup_it == node_lookup.end() ) {
                nav_search_node_t neighbor_search = {
                    .node = neighbor_node,
                    .g_cost = tentative_g,
                    .f_cost = tentative_g + heuristic( neighbor_node.position, goal_node.position ),
                    .parent_index = current_index,
                    .closed = false
                };
                
                nodes.push_back( neighbor_search );
                const int32_t neighbor_index = (int32_t)nodes.size() - 1;
                open_list.push_back( neighbor_index );
                node_lookup.emplace( neighbor_node.key, neighbor_index );
                continue;
            }
            
            const int32_t neighbor_index = lookup_it->second;
            nav_search_node_t &neighbor_search = nodes[ neighbor_index ];
            if ( neighbor_search.closed ) {
                continue;
            }
            
            if ( tentative_g < neighbor_search.g_cost ) {
                neighbor_search.g_cost = tentative_g;
                neighbor_search.f_cost = tentative_g + heuristic( neighbor_node.position, goal_node.position );
                neighbor_search.parent_index = current_index;
            }
        }
    }
    
    return false;
}

/**
*   @brief  Generate a traversal path between two world-space origins.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path ) {
    if ( !out_path ) {
        return false;
    }
    
    SVG_Nav_FreeTraversalPath( out_path );
    
    if ( !g_nav_mesh ) {
        return false;
    }
    
    nav_node_ref_t start_node = {};
    nav_node_ref_t goal_node = {};
    
    if ( !Nav_FindNodeForPosition( g_nav_mesh, start_origin, start_origin[ 2 ], &start_node, true ) ) {
        return false;
    }
    
    if ( !Nav_FindNodeForPosition( g_nav_mesh, goal_origin, goal_origin[ 2 ], &goal_node, true ) ) {
        return false;
    }
    
    std::vector<Vector3> points;
    if ( start_node.key == goal_node.key ) {
        points.push_back( start_node.position );
        points.push_back( goal_node.position );
    } else if ( !Nav_AStarSearch( g_nav_mesh, start_node, goal_node, points ) ) {
        return false;
    }
    
    if ( points.empty() ) {
        return false;
    }
    
    out_path->num_points = (int32_t)points.size();
    out_path->points = (Vector3 *)gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
    memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );
    
    return true;
}

/**
*   @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
**/
void SVG_Nav_FreeTraversalPath( nav_traversal_path_t *path ) {
    if ( !path ) {
        return;
    }
    
    if ( path->points ) {
        gi.TagFree( path->points );
    }
    
    path->points = nullptr;
    path->num_points = 0;
}

/**
*   @brief  Query movement direction along a traversal path.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin,
                                            float waypoint_radius, int32_t *inout_index, Vector3 *out_direction ) {
    if ( !path || !inout_index || !out_direction ) {
        return false;
    }
    
    if ( path->num_points <= 0 || !path->points ) {
        return false;
    }
    
    int32_t index = *inout_index;
    if ( index < 0 ) {
        index = 0;
    }
    
    const float waypoint_radius_sqr = waypoint_radius * waypoint_radius;
 
     while ( index < path->num_points ) {
         const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
         // Use 2D distance so Z quantization / stairs don't prevent waypoint completion.
         const float dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] );
         if ( dist_sqr > waypoint_radius_sqr ) {
             break;
         }
         index++;
     }
    
    if ( index >= path->num_points ) {
        *inout_index = path->num_points;
        return false;
    }
    
    Vector3 direction = QM_Vector3Subtract( path->points[ index ], current_origin );
    direction[ 2 ] = 0.0f;
    const float length = (float)QM_Vector3Length( direction );
    if ( length <= std::numeric_limits<float>::epsilon() ) {
        return false;
    }
    
    *out_direction = QM_Vector3Normalize( direction );
    *inout_index = index;
    
    return true;
}
