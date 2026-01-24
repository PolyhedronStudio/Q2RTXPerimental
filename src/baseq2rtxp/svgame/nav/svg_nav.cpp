/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator - Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/entities/svg_player_edict.h"
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
//! Small epsilon to ensure max bounds map to the correct tile.
static constexpr float NAV_TILE_EPSILON = 0.001f;



/**
*
*	Forward Declarations (Debug):
*
**/
/**
*	@return	True if navigation debug drawing is enabled.
**/
static inline bool NavDebug_Enabled( void );
/**
*	@brief	Draw a single sample tick at the given position with the given height.
**/
static void NavDebug_DrawSampleTick( const Vector3 &pos, const float height );
/**
*	@brief  Clear cached debug paths.
**/
static void NavDebug_ClearCachedPaths( void );
/**
*	@brief	Purge cached paths that have expired.
**/
static void NavDebug_PurgeCachedPaths( void );

/**
*
*	Forward Declarations:
*
**/
/**
*   @brief  Generate navigation mesh for world (world-only collision).
*           Creates tiles for each BSP leaf containing walkable surface samples.
*   @param  mesh    Navigation mesh structure to populate.
**/
static void GenerateWorldMesh( nav_mesh_t *mesh );

/**
*   @brief  Generate navigation mesh for inline BSP models (brush entities).
*           Creates tiles for each inline model in local space for later transform application.
*   @param  mesh    Navigation mesh structure to populate.
**/
static void GenerateInlineModelMesh( nav_mesh_t *mesh );
/**
*	@brief	Build runtime inline model data for navigation mesh.
*	@note	This allocates and initializes the runtime array. Per-frame updates should call
*			`SVG_Nav_RefreshInlineModelRuntime()` (no allocations).
**/
static void Nav_BuildInlineModelRuntime( nav_mesh_t *mesh, const std::unordered_map<int32_t, svg_base_edict_t *> &model_to_ent );
/**
*   @brief  Refresh the runtime data for inline models (transforms only).
*           This updates the origin/angles of inline model runtime entries based on current entity state.
**/
void SVG_Nav_RefreshInlineModelRuntime( void );

/**
*	@brief	Performs a simple PMove-like step traversal test (3-trace).
*
*			This is intentionally conservative and is used only to validate edges in A*:
*			1) Try direct horizontal move.
*			2) If blocked, try stepping up <= max step, then horizontal move.
*			3) Trace down to find ground.
*
*	@return	True if the traversal is possible, false otherwise.
**/
static bool Nav_CanTraverseStep( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t *clip_entity );
static bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity );



/**
*
*   CVars fpr Navigation Debug Drawing:
*
**/
//! Master on/off. 0 = off, 1 = on.
static cvar_t *nav_debug_draw = nullptr;
//! Only draw a specific BSP leaf index. -1 = any.
static cvar_t *nav_debug_draw_leaf = nullptr;
//! Only draw a specific tile (grid coords). Requires "x y". "*" disables.
static cvar_t *nav_debug_draw_tile = nullptr;
//! Budget in TE_DEBUG_TRAIL line segments per server frame.
static cvar_t *nav_debug_draw_max_segments = nullptr;
//! Max distance from current view player to draw debug (world units). 0 disables.
static cvar_t *nav_debug_draw_max_dist = nullptr;
//! Draw tile bboxes.
static cvar_t *nav_debug_draw_tile_bounds = nullptr;
//! Draw sample ticks.
static cvar_t *nav_debug_draw_samples = nullptr;
//! Draw final path segments from traversal path query.
static cvar_t *nav_debug_draw_path = nullptr;

// Cached debug paths (last computed traversal paths for the current server frame).
static constexpr QMTime NAV_DEBUG_PATH_RETENTION = QMTime::FromMilliseconds( 1500 );
struct nav_debug_cached_path_t {
	nav_traversal_path_t path;
	QMTime expireTime;
};
static std::vector<nav_debug_cached_path_t> s_nav_debug_cached_paths;



/**
*
*
*
*   Navigation Global Variables and CVars:
*
*
*
**/
/**
*	Navigation Debug CVar Variables:
**/
//cvar_t *nav_debug_draw_voxelmesh = nullptr;
//cvar_t *nav_debug_draw_walkable_sample_points = nullptr;
//cvar_t *nav_debug_draw_pathfinding = nullptr;

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
static inline float Nav_TileWorldSize( const nav_mesh_t *mesh ) {
	return mesh->cell_size_xy * ( float )mesh->tile_size;
}
/**
*   @brief  Convert world coordinate to tile grid coordinate.
**/
static inline int32_t Nav_WorldToTileCoord( float value, float tile_world_size ) {
	return ( int32_t )floorf( value / tile_world_size );
}
/**
*   @brief  Convert world coordinate to cell index within a tile.
**/
static inline int32_t Nav_WorldToCellCoord( float value, float tile_origin, float cell_size_xy ) {
	return ( int32_t )floorf( ( value - tile_origin ) / cell_size_xy );
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
	nav_max_step = gi.cvar( "nav_max_step", "18", 0 );
	nav_max_slope_deg = gi.cvar( "nav_max_slope_deg", "45.57", 0 );

	/**
	*   Register agent bounding box CVars:
	**/
	nav_agent_mins_x = gi.cvar( "nav_agent_mins_x", "-16", 0 );
	nav_agent_mins_y = gi.cvar( "nav_agent_mins_y", "-16", 0 );
	nav_agent_mins_z = gi.cvar( "nav_agent_mins_z", "-36", 0 );
	nav_agent_maxs_x = gi.cvar( "nav_agent_maxs_x", "16", 0 );
	nav_agent_maxs_y = gi.cvar( "nav_agent_maxs_y", "16", 0 );
	nav_agent_maxs_z = gi.cvar( "nav_agent_maxs_z", "36", 0 );

	/**
	*   Debug draw CVars:
	**/
	#if _DEBUG_NAV_MESH
	nav_debug_draw = gi.cvar( "nav_debug_draw", "1", CVAR_CHEAT );
	nav_debug_draw_leaf = gi.cvar( "nav_debug_draw_leaf", "-1", CVAR_CHEAT );
	nav_debug_draw_tile = gi.cvar( "nav_debug_draw_tile", "*", CVAR_CHEAT );
	nav_debug_draw_max_segments = gi.cvar( "nav_debug_draw_max_segments", "16384", CVAR_CHEAT );
	nav_debug_draw_max_dist = gi.cvar( "nav_debug_draw_max_dist", "8192", CVAR_CHEAT );
	nav_debug_draw_tile_bounds = gi.cvar( "nav_debug_draw_tile_bounds", "0", CVAR_CHEAT );
	nav_debug_draw_samples = gi.cvar( "nav_debug_draw_samples", "0", CVAR_CHEAT );
	nav_debug_draw_path = gi.cvar( "nav_debug_draw_path", "1", CVAR_CHEAT );
	#else
	nav_debug_draw = gi.cvar( "nav_debug_draw", "0", 0 );
	nav_debug_draw_leaf = gi.cvar( "nav_debug_draw_leaf", "-1", 0 );
	nav_debug_draw_tile = gi.cvar( "nav_debug_draw_tile", "*", 0 );
	nav_debug_draw_max_segments = gi.cvar( "nav_debug_draw_max_segments", "256", 0 );
	nav_debug_draw_max_dist = gi.cvar( "nav_debug_draw_max_dist", "2048", 0 );
	nav_debug_draw_tile_bounds = gi.cvar( "nav_debug_draw_tile_bounds", "1", 0 );
	nav_debug_draw_samples = gi.cvar( "nav_debug_draw_samples", "1", 0 );
	nav_debug_draw_path = gi.cvar( "nav_debug_draw_path", "0", 0 );
	#endif
	// Initialize global navigation mesh pointer.
	g_nav_mesh = nullptr;
}

/**
*   @brief  Shutdown navigation system and free memory.
*           Called during game shutdown to clean up resources.
**/
void SVG_Nav_Shutdown( void ) {
	NavDebug_ClearCachedPaths();
    SVG_Nav_FreeMesh();
}



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
	g_nav_mesh = ( nav_mesh_t * )gi.TagMallocz( sizeof( nav_mesh_t ), TAG_SVGAME_LEVEL );

	/**
	*   Store generation parameters from CVars:
	**/
	g_nav_mesh->cell_size_xy = nav_cell_size_xy->value;
	g_nav_mesh->z_quant = nav_z_quant->value;
	g_nav_mesh->tile_size = ( int32_t )nav_tile_size->value;
	g_nav_mesh->max_step = nav_max_step->value;
	g_nav_mesh->max_slope_deg = nav_max_slope_deg->value;
	g_nav_mesh->agent_mins = Vector3( nav_agent_mins_x->value, nav_agent_mins_y->value, nav_agent_mins_z->value );
	g_nav_mesh->agent_maxs = Vector3( nav_agent_maxs_x->value, nav_agent_maxs_y->value, nav_agent_maxs_z->value );

	/**
	*   Validate bounding box parameters:
	**/
	if ( g_nav_mesh->agent_mins[ 0 ] >= g_nav_mesh->agent_maxs[ 0 ] ||
		g_nav_mesh->agent_mins[ 1 ] >= g_nav_mesh->agent_maxs[ 1 ] ||
		g_nav_mesh->agent_mins[ 2 ] >= g_nav_mesh->agent_maxs[ 2 ] ) {
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
		g_nav_mesh->agent_mins[ 0 ], g_nav_mesh->agent_mins[ 1 ], g_nav_mesh->agent_mins[ 2 ] );
	gi.cprintf( nullptr, PRINT_HIGH, "  agent_maxs: (%.1f, %.1f, %.1f)\n",
		g_nav_mesh->agent_maxs[ 0 ], g_nav_mesh->agent_maxs[ 1 ], g_nav_mesh->agent_maxs[ 2 ] );

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
		nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[ i ];
		g_nav_mesh->total_tiles += leaf->num_tiles;

		// Count cells and layers in each tile.
		for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
			nav_tile_t *tile = &leaf->tiles[ t ];
			int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;

			for ( int32_t c = 0; c < cells_per_tile; c++ ) {
				if ( tile->cells[ c ].num_layers > 0 ) {
					g_nav_mesh->total_xy_cells++;
					g_nav_mesh->total_layers += tile->cells[ c ].num_layers;
				}
			}
		}
	}

	// Count inline model stats.
	for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
		nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[ i ];
		g_nav_mesh->total_tiles += model->num_tiles;

		// Count cells and layers in each tile.
		for ( int32_t t = 0; t < model->num_tiles; t++ ) {
			nav_tile_t *tile = &model->tiles[ t ];
			int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;

			for ( int32_t c = 0; c < cells_per_tile; c++ ) {
				if ( tile->cells[ c ].num_layers > 0 ) {
					g_nav_mesh->total_xy_cells++;
					g_nav_mesh->total_layers += tile->cells[ c ].num_layers;
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
*   @brief  Free the current navigation mesh.
*           Releases all memory allocated for navigation data using TAG_SVGAME_LEVEL.
**/
void SVG_Nav_FreeMesh( void ) {
	// Early out if no mesh exists.
	if ( !g_nav_mesh ) {
		return;
	}

	// Cached debug paths become invalid once the mesh is destroyed.
	NavDebug_ClearCachedPaths();

	// Free world + inline model tile data...
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

	/**
	*	Free inline model runtime data:
	**/
	if ( g_nav_mesh->inline_model_runtime ) {
		gi.TagFree( g_nav_mesh->inline_model_runtime );
		g_nav_mesh->inline_model_runtime = nullptr;
		g_nav_mesh->num_inline_model_runtime = 0;
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
*   Navigation Tile Generation:
*
*
*
**/
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
        }
    }
    
    return has_layers;
}

/**
*	@brief	Build a navigation tile for an inline model using entity clip collision.
*	@return	True if any walkable data was generated.
**/
static bool Nav_BuildInlineTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &model_mins, const Vector3 &model_maxs,
								 float z_min, float z_max, const edict_ptr_t *clip_entity ) {
	const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
	const int32_t presence_words = ( cells_per_tile + 31 ) / 32;
	const float tile_world_size = Nav_TileWorldSize( mesh );

	// Allocate tile storage.
	tile->presence_bits = (uint32_t *)gi.TagMallocz( sizeof( uint32_t ) * presence_words, TAG_SVGAME_LEVEL );
	tile->cells = (nav_xy_cell_t *)gi.TagMallocz( sizeof( nav_xy_cell_t ) * cells_per_tile, TAG_SVGAME_LEVEL );

	bool has_layers = false;

	// Tile origins are in "model local space". Allow negative coords by using local mins as base.
	const float tile_origin_x = model_mins[ 0 ] + ( tile->tile_x * tile_world_size );
	const float tile_origin_y = model_mins[ 1 ] + ( tile->tile_y * tile_world_size );

	for ( int32_t cell_y = 0; cell_y < mesh->tile_size; cell_y++ ) {
		for ( int32_t cell_x = 0; cell_x < mesh->tile_size; cell_x++ ) {
			const float local_x = tile_origin_x + ( (float)cell_x + 0.5f ) * mesh->cell_size_xy;
			const float local_y = tile_origin_y + ( (float)cell_y + 0.5f ) * mesh->cell_size_xy;

			// Skip cells outside the model AABB.
			if ( local_x < model_mins[ 0 ] || local_x > model_maxs[ 0 ] ||
				 local_y < model_mins[ 1 ] || local_y > model_maxs[ 1 ] ) {
				continue;
			}

			Vector3 xy_pos = { local_x, local_y, 0.0f };
			nav_layer_t *layers = nullptr;
			int32_t num_layers = 0;

			FindWalkableLayers( xy_pos, mesh->agent_mins, mesh->agent_maxs,
								z_min, z_max, mesh->max_step, mesh->max_slope_deg, mesh->z_quant,
								&layers, &num_layers, clip_entity );

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
static void Nav_CollectInlineModelEntities( std::unordered_map<int32_t, svg_base_edict_t *> &out_model_to_ent ) {
	out_model_to_ent.clear();

	for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++ ) {
		// Get the entity for this index.
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );

		/**
		*	Skip nullptr/inactive edicts.
		**/
		if ( !SVG_Entity_IsActive( ent ) ) {
			continue;
		}

		const char *modelStr = ent->model;
		if ( !modelStr || modelStr[ 0 ] != '*' ) {
			continue;
		}

		const int32_t modelIndex = atoi( modelStr + 1 );
		if ( modelIndex <= 0 ) {
			continue;
		}

		// Keep first owner encountered for this model index.
		if ( out_model_to_ent.find( modelIndex ) == out_model_to_ent.end() ) {
			out_model_to_ent.emplace( modelIndex, ent );
		}
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
	const cm_t *world_model = gi.GetCollisionModel();
	if ( !world_model || !world_model->cache ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
		return;
	}
	// Get the pointer to the cached BSP data.    
    bsp_t *bsp = world_model->cache;
	// Get number of leafs in the BSP.
    int32_t num_leafs = bsp->numleafs;
    
    gi.cprintf( nullptr, PRINT_HIGH, "Generating world navmesh for %d leafs...\n", num_leafs );
    
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

		if ( leaf->contents & CONTENTS_SOLID ) {
			continue;
		}

		// NOTE: Debug filters must not skip generation; they apply at draw time.
		// if ( NavDebug_Enabled() && !NavDebug_FilterLeaf( i ) ) {
		// 	continue;
		// }

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

				// NOTE: Debug filters must not skip generation; they apply at draw time.
				// if ( NavDebug_Enabled() && !NavDebug_FilterTile( tile_x, tile_y ) ) {
				// 	continue;
				// }

				nav_tile_t tile = {};
				tile.tile_x = tile_x;
				tile.tile_y = tile_y;

				if ( Nav_BuildTile( mesh, &tile, leaf_mins, leaf_maxs, z_min, z_max ) ) {
					//NavDebug_DrawTileBBox( mesh, &tile );
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

	std::unordered_map<int32_t, svg_base_edict_t *> model_to_ent;
	Nav_CollectInlineModelEntities( model_to_ent );

	// Nothing to do if no brush entities are present/active.
	if ( model_to_ent.empty() ) {
		mesh->num_inline_models = 0;
		mesh->inline_model_data = nullptr;
		mesh->num_inline_model_runtime = 0;
		mesh->inline_model_runtime = nullptr;
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "Generating inline model navmesh' for %d inline models...\n", ( int32_t )model_to_ent.size() );

	// Allocate only for inline models that are actually referenced by entities.
	mesh->num_inline_models = ( int32_t )model_to_ent.size();
	mesh->inline_model_data = ( nav_inline_model_data_t * )gi.TagMallocz(
		sizeof( nav_inline_model_data_t ) * mesh->num_inline_models, TAG_SVGAME_LEVEL );

	// Build runtime mapping (transforms).
	Nav_BuildInlineModelRuntime( mesh, model_to_ent );

	const float tile_world_size = Nav_TileWorldSize( mesh );

	int32_t out_index = 0;
	for ( const auto &it : model_to_ent ) {
		const int32_t model_index = it.first;
		svg_base_edict_t *ent = it.second;

		nav_inline_model_data_t *out_model = &mesh->inline_model_data[ out_index ];
		out_model->model_index = model_index;
		out_model->num_tiles = 0;
		out_model->tiles = nullptr;

		if ( !ent ) {
			out_index++;
			continue;
		}

		const char *model_name = ent->model;
		const mmodel_t *mm = ( model_name ? gi.GetInlineModelDataForName( model_name ) : nullptr );
		if ( !mm ) {
			out_index++;
			continue;
		}

		const Vector3 model_mins = mm->mins;
		const Vector3 model_maxs = mm->maxs;

		const float z_min = model_mins[ 2 ] + mesh->agent_mins[ 2 ];
		const float z_max = model_maxs[ 2 ] + mesh->agent_maxs[ 2 ];

		// Compute tile range in model-local space with mins as the origin.
		const int32_t tile_min_x = 0;
		const int32_t tile_min_y = 0;
		const int32_t tile_max_x = Nav_WorldToTileCoord( ( model_maxs[ 0 ] - model_mins[ 0 ] ) - NAV_TILE_EPSILON, tile_world_size );
		const int32_t tile_max_y = Nav_WorldToTileCoord( ( model_maxs[ 1 ] - model_mins[ 1 ] ) - NAV_TILE_EPSILON, tile_world_size );

		std::vector<nav_tile_t> tiles;
		if ( tile_max_x >= tile_min_x && tile_max_y >= tile_min_y ) {
			const int32_t tile_count = ( tile_max_x - tile_min_x + 1 ) * ( tile_max_y - tile_min_y + 1 );
			tiles.reserve( tile_count );
		}

		for ( int32_t tile_y = tile_min_y; tile_y <= tile_max_y; tile_y++ ) {
			for ( int32_t tile_x = tile_min_x; tile_x <= tile_max_x; tile_x++ ) {
				nav_tile_t tile = {};
				tile.tile_x = tile_x;
				tile.tile_y = tile_y;

				if ( Nav_BuildInlineTile( mesh, &tile, model_mins, model_maxs, z_min, z_max, ent ) ) {
					tiles.push_back( tile );
				}
			}
		}

		if ( !tiles.empty() ) {
			out_model->num_tiles = ( int32_t )tiles.size();
			out_model->tiles = ( nav_tile_t * )gi.TagMallocz( sizeof( nav_tile_t ) * out_model->num_tiles, TAG_SVGAME_LEVEL );
			memcpy( out_model->tiles, tiles.data(), sizeof( nav_tile_t ) * out_model->num_tiles );
		}

		out_index++;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "Inline models' navmesh generation complete\n" );
}

/**
*	@brief	Build runtime inline model data for navigation mesh.
*	@note	This allocates and initializes the runtime array. Per-frame updates should call
*			`SVG_Nav_RefreshInlineModelRuntime()` (no allocations).
**/
static void Nav_BuildInlineModelRuntime( nav_mesh_t *mesh, const std::unordered_map<int32_t, svg_base_edict_t *> &model_to_ent ) {
	if ( !mesh ) {
		return;
	}

	// Free old runtime mapping (if any) to avoid leaks on regen.
	if ( mesh->inline_model_runtime ) {
		gi.TagFree( mesh->inline_model_runtime );
		mesh->inline_model_runtime = nullptr;
		mesh->num_inline_model_runtime = 0;
	}

	if ( model_to_ent.empty() ) {
		return;
	}

	mesh->num_inline_model_runtime = (int32_t)model_to_ent.size();
	mesh->inline_model_runtime = (nav_inline_model_runtime_t *)gi.TagMallocz(
		sizeof( nav_inline_model_runtime_t ) * mesh->num_inline_model_runtime, TAG_SVGAME_LEVEL );

	int32_t out_index = 0;
	for ( const auto &it : model_to_ent ) {
		const int32_t model_index = it.first;
		svg_base_edict_t *ent = it.second;

		nav_inline_model_runtime_t &rt = mesh->inline_model_runtime[ out_index ];
		rt.model_index = model_index;
		rt.owner_ent = ent;
		rt.owner_entnum = ent ? ent->s.number : 0;

		rt.origin = ent ? ent->currentOrigin : Vector3{};
		rt.angles = ent ? ent->currentAngles : Vector3{};
		rt.dirty = false;

		out_index++;
	}
}

/**
*   @brief  Refresh the runtime data for inline models (transforms only).
*           This updates the origin/angles of inline model runtime entries based on current entity state.
**/
void SVG_Nav_RefreshInlineModelRuntime( void ) {
	if ( !g_nav_mesh || !g_nav_mesh->inline_model_runtime || g_nav_mesh->num_inline_model_runtime <= 0 ) {
		return;
	}

	for ( int32_t i = 0; i < g_nav_mesh->num_inline_model_runtime; i++ ) {
		nav_inline_model_runtime_t &rt = g_nav_mesh->inline_model_runtime[ i ];
		svg_base_edict_t *ent = rt.owner_ent;

		// Entity pointer can exist but be inactive/reused; verify.
		if ( !SVG_Entity_IsActive( ent ) ) {
			// Keep last transform; mark dirty so consumers can handle dropout.
			rt.dirty = true;
			continue;
		}

		const Vector3 newOrigin = ent->currentOrigin;
		const Vector3 newAngles = ent->currentAngles;

		// Cheap exact compare (Vector3 here is POD-ish); avoid float epsilon logic unless you need it.
		const bool originChanged = ( newOrigin[ 0 ] != rt.origin[ 0 ] ) || ( newOrigin[ 1 ] != rt.origin[ 1 ] ) || ( newOrigin[ 2 ] != rt.origin[ 2 ] );
		const bool anglesChanged = ( newAngles[ 0 ] != rt.angles[ 0 ] ) || ( newAngles[ 1 ] != rt.angles[ 1 ] ) || ( newAngles[ 2 ] != rt.angles[ 2 ] );

		if ( originChanged || anglesChanged ) {
			rt.origin = newOrigin;
			rt.angles = newAngles;
			rt.dirty = true;
		} else {
			rt.dirty = false;
		}
	}
}



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

const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t *path, const Vector3 &current_origin,
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

	// Advance using 2D distance so Z quantization / stairs don't prevent waypoint completion.
	while ( index < path->num_points ) {
		const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
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

	// Clamp vertical component to avoid large up/down jumps.
	if ( g_nav_mesh ) {
		const float maxDz = g_nav_mesh->max_step + g_nav_mesh->z_quant;
		if ( direction[ 2 ] > maxDz ) {
			direction[ 2 ] = maxDz;
		} else if ( direction[ 2 ] < -maxDz ) {
			direction[ 2 ] = -maxDz;
		}
	}

	const float length = ( float )QM_Vector3Length( direction );
	if ( length <= std::numeric_limits<float>::epsilon() ) {
		return false;
	}

	*out_direction = QM_Vector3Normalize( direction );
	*inout_index = index;
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
				nav_node_ref_t candidate = {};
				candidate.key = {
					.leaf_index = leaf_index,
					.tile_index = t,
					.cell_index = c,
					.layer_index = l
				};
				candidate.position = Nav_NodeWorldPosition( mesh, tile, c, &cell->layers[ l ] );

				const Vector3 node_pos = candidate.position;
                const Vector3 delta = QM_Vector3Subtract( node_pos, position );
                const float dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] );
                
                if ( dist_sqr < best_dist_sqr ) {
                    best_dist_sqr = dist_sqr;
                    best_node = candidate;
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
*	@brief	Select the best layer index for a cell based on a blend of start Z and goal Z.
*			This helps pathfinding prefer the correct floor when chasing a target above/below.
*	@param	start_z	Seeker starting Z.
*	@param	goal_z	Target goal Z.
*	@note	The blend factor is based on XY distance between seeker and goal: close targets bias toward start_z,
*			far targets bias toward goal_z.
**/
static bool Nav_SelectLayerIndex_BlendZ( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, float start_z, float goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const float blend_start_dist, const float blend_full_dist, int32_t *out_layer_index ) {
	if ( !mesh || !cell || cell->num_layers <= 0 || !out_layer_index ) {
		return false;
	}

	const Vector3 d = QM_Vector3Subtract( goal_pos, start_pos );
	const float dist2D = sqrtf( ( d[ 0 ] * d[ 0 ] ) + ( d[ 1 ] * d[ 1 ] ) );

	const float BLEND_START_DIST = std::max( 0.0f, blend_start_dist );
	const float BLEND_FULL_DIST = std::max( BLEND_START_DIST + 1.0f, blend_full_dist );
	float t = 0.0f;
	if ( dist2D > BLEND_START_DIST ) {
		t = ( dist2D - BLEND_START_DIST ) / std::max( 1.0f, ( BLEND_FULL_DIST - BLEND_START_DIST ) );
		t = QM_Clamp( t, 0.0f, 1.0f );
	}

	const float desired_z = ( ( 1.0f - t ) * start_z ) + ( t * goal_z );
	return Nav_SelectLayerIndex( mesh, cell, desired_z, out_layer_index );
}

static bool Nav_FindNodeForPosition_BlendZ( const nav_mesh_t *mesh, const Vector3 &position, float start_z, float goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const float blend_start_dist, const float blend_full_dist,
	nav_node_ref_t *out_node, bool allow_fallback ) {
	if ( !mesh || !out_node ) {
		return false;
	}

	const Vector3 d = QM_Vector3Subtract( goal_pos, start_pos );
	const float dist2D = sqrtf( ( d[ 0 ] * d[ 0 ] ) + ( d[ 1 ] * d[ 1 ] ) );

	const float BLEND_START_DIST = std::max( 0.0f, blend_start_dist );
	const float BLEND_FULL_DIST = std::max( BLEND_START_DIST + 1.0f, blend_full_dist );
	float t = 0.0f;
	if ( dist2D > BLEND_START_DIST ) {
		t = ( dist2D - BLEND_START_DIST ) / std::max( 1.0f, ( BLEND_FULL_DIST - BLEND_START_DIST ) );
		t = QM_Clamp( t, 0.0f, 1.0f );
	}

	const float desired_z = ( ( 1.0f - t ) * start_z ) + ( t * goal_z );

	const cm_t *world_model = gi.GetCollisionModel();
	if ( !world_model || !world_model->cache ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
		return false;
	}
	bsp_t *bsp = world_model->cache;

	mleaf_t *leaf = gi.BSP_PointLeaf( bsp->nodes, &position[ 0 ] );
	if ( !leaf ) {
		return false;
	}

	const ptrdiff_t leaf_index = leaf - bsp->leafs;
	if ( leaf_index < 0 || leaf_index >= mesh->num_leafs ) {
		return false;
	}

	if ( Nav_FindNodeInLeaf( mesh, &mesh->leaf_data[ leaf_index ], ( int32_t )leaf_index, position, desired_z, out_node, allow_fallback ) ) {
		return true;
	}

	if ( !allow_fallback ) {
		return false;
	}

	float best_dist_sqr = std::numeric_limits<float>::max();
	nav_node_ref_t best_node = {};
	bool found = false;

	for ( int32_t i = 0; i < mesh->num_leafs; i++ ) {
		nav_node_ref_t candidate = {};
		if ( Nav_FindNodeInLeaf( mesh, &mesh->leaf_data[ i ], i, position, desired_z, &candidate, true ) ) {
			const Vector3 delta2 = QM_Vector3Subtract( candidate.position, position );
			const float dist_sqr = ( delta2[ 0 ] * delta2[ 0 ] ) + ( delta2[ 1 ] * delta2[ 1 ] ) + ( delta2[ 2 ] * delta2[ 2 ] );
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
	const Vector3 &agent_mins, const Vector3 &agent_maxs, std::vector<Vector3> &out_points ) {
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

			// Replace the old |dz| check with a PMove-like step traversal test.
			// Validate using the agent bbox so corner/doorway traversal matches the mover.
			if ( !Nav_CanTraverseStep_ExplicitBBox( mesh, current.node.position, neighbor_node.position, agent_mins, agent_maxs, nullptr ) ) {
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
*
*
*
*	Navigation Debug Drawing - State and Helpers:
*
*
*
**/
typedef struct nav_debug_draw_state_s {
	int64_t frame = -1;
	int32_t segments_used = 0;
} nav_debug_draw_state_t;
//! Global debug draw state.
static nav_debug_draw_state_t s_nav_debug_draw_state = {};

static void NavDebug_PurgeCachedPaths( void ) {
	const QMTime now = level.time;
	auto it = s_nav_debug_cached_paths.begin();
	while ( it != s_nav_debug_cached_paths.end() ) {
		if ( it->expireTime <= now ) {
			SVG_Nav_FreeTraversalPath( &it->path );
			it = s_nav_debug_cached_paths.erase( it );
		} else {
			++it;
		}
	}
}

static void NavDebug_ClearCachedPaths( void ) {
	for ( nav_debug_cached_path_t &entry : s_nav_debug_cached_paths ) {
		SVG_Nav_FreeTraversalPath( &entry.path );
	}
	s_nav_debug_cached_paths.clear();
}

/**
*
*
*
*	Navigation Debug Drawing - Runtime Draw:
*
*
*
**/
/**
*	@brief	Prepare for a new frame of debug drawing.
**/
static inline void NavDebug_NewFrame( void ) {
	if ( s_nav_debug_draw_state.frame == ( int32_t )level.frameNumber ) {
		return;
	}

	s_nav_debug_draw_state.frame = ( int32_t )level.frameNumber;
	s_nav_debug_draw_state.segments_used = 0;
}
/**
*	@brief	Determine if we can emit the requested number of segments.
**/
static inline bool NavDebug_CanEmitSegments( const int32_t count ) {
	NavDebug_NewFrame();

	const int32_t maxSegments = ( nav_debug_draw_max_segments ? nav_debug_draw_max_segments->integer : 0 );
	if ( maxSegments <= 0 ) {
		return false;
	}

	return ( s_nav_debug_draw_state.segments_used + count ) <= maxSegments;
}
/**
*	@brief	Consume the given number of emitted segments.
**/
static inline void NavDebug_ConsumeSegments( const int32_t count ) {
	s_nav_debug_draw_state.segments_used += count;
}
/**
*	@brief	Check if navigation debug drawing is enabled.
**/
static inline bool NavDebug_Enabled( void ) {
	return nav_debug_draw && nav_debug_draw->integer != 0;
}
/**
*	@brief	Get the current view origin for distance filtering.
**/
static inline Vector3 NavDebug_GetViewOrigin( void ) {
	if ( game.currentViewPlayer ) {
		return game.currentViewPlayer->currentOrigin + Vector3{ 0.f, 0.f, ( float )game.currentViewPlayer->viewheight };;
	}

	// Fallback: try client #1 (singleplayer typical).
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( 1 );
	if ( SVG_Entity_IsClient( ent, false ) ) {
		return ent->currentOrigin + Vector3{ 0.f, 0.f, ( float )ent->viewheight };
	}

	return {};
}
/**
*	@brief	Check if a position passes the distance filter.
* 	@param	pos	Position to check.
**/
static inline bool NavDebug_PassesDistanceFilter( const Vector3 &pos ) {
	const float maxDist = ( nav_debug_draw_max_dist ? nav_debug_draw_max_dist->value : 0.0f );
	if ( maxDist <= 0.0f ) {
		return true;
	}

	const Vector3 viewOrg = NavDebug_GetViewOrigin();
	const Vector3 d = QM_Vector3Subtract( pos, viewOrg );
	const float distSqr = ( d[ 0 ] * d[ 0 ] ) + ( d[ 1 ] * d[ 1 ] ) + ( d[ 2 ] * d[ 2 ] );
	return distSqr <= ( maxDist * maxDist );
}
/**
*	@return	Returns true if a tile filter is specified and outputs the wanted tile coordinates.
**/
static bool NavDebug_ParseTileFilter( int32_t *outTileX, int32_t *outTileY ) {
	if ( !outTileX || !outTileY || !nav_debug_draw_tile ) {
		return false;
	}

	const char *s = nav_debug_draw_tile->string;
	if ( !s || !s[ 0 ] || s[ 0 ] == '*' ) {
		return false;
	}

	int x = 0, y = 0;
	if ( sscanf( s, "%d %d", &x, &y ) != 2 ) {
		return false;
	}

	*outTileX = x;
	*outTileY = y;
	return true;
}
/**
*	@brief	Filter by leaf index.
* 	@return	Returns true if the given leaf index passes the filter.
**/
static inline bool NavDebug_FilterLeaf( const int32_t leafIndex ) {
	if ( !nav_debug_draw_leaf ) {
		return true;
	}

	const int32_t wanted = nav_debug_draw_leaf->integer;
	return ( wanted < 0 ) || ( wanted == leafIndex );
}
/**
*	@brief	Will filter by tile coordinates.
*	@return	Returns true if the given tile coordinates pass the filter.
**/
static inline bool NavDebug_FilterTile( const int32_t tileX, const int32_t tileY ) {
	int32_t wantedX = 0, wantedY = 0;
	if ( !NavDebug_ParseTileFilter( &wantedX, &wantedY ) ) {
		return true;
	}

	return tileX == wantedX && tileY == wantedY;
}
/**
*	@brief	Draw a vertical tick at the given position.
**/
static void NavDebug_DrawSampleTick( const Vector3 &pos, const float height ) {
	// Check if we should draw sample ticks.
	if ( !NavDebug_Enabled() || !nav_debug_draw_samples || nav_debug_draw_samples->integer == 0 ) {
		return;
	}
	// Check segment budget.
	if ( !NavDebug_CanEmitSegments( 1 ) ) {
		return;
	}
	// Check distance filter.
	if ( !NavDebug_PassesDistanceFilter( pos ) ) {
		return;
	}

	// Draw the tick.
	Vector3 end = pos;
	end[ 2 ] += height;
	SVG_DebugDrawLine_TE( pos, end, MULTICAST_PVS, false );
	// Consume segment budget.
	NavDebug_ConsumeSegments( 1 );
}

/**
*	@brief	Draw the bounding box of a tile.
*	@param	mesh	The navigation mesh.
*	@param	tile	The tile to draw.
**/
static void NavDebug_DrawTileBBox( const nav_mesh_t *mesh, const nav_tile_t *tile ) {
	if ( !NavDebug_Enabled() || !nav_debug_draw_tile_bounds || nav_debug_draw_tile_bounds->integer == 0 ) {
		return;
	}
	if ( !NavDebug_CanEmitSegments( 12 ) ) {
		return;
	}
	const float tileWorldSize = Nav_TileWorldSize( mesh );
	const Vector3 mins = {
		tile->tile_x * tileWorldSize,
		tile->tile_y * tileWorldSize,
		-4096.0f
	};
	const Vector3 maxs = {
		mins[ 0 ] + tileWorldSize,
		mins[ 1 ] + tileWorldSize,
		4096.0f
	};
	// Check distance filter against tile center.
	const Vector3 tileCenter = {
		( mins[ 0 ] + maxs[ 0 ] ) * 0.5f,
		( mins[ 1 ] + maxs[ 1 ] ) * 0.5f,
		( mins[ 2 ] + maxs[ 2 ] ) * 0.5f
	};
	if ( !NavDebug_PassesDistanceFilter( tileCenter ) ) {
		return;
	}
	SVG_DebugDrawBBox_TE( mins, maxs, MULTICAST_PVS, false );
	NavDebug_ConsumeSegments( 12 );
}



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
	} else if ( !Nav_AStarSearch( g_nav_mesh, start_node, goal_node, g_nav_mesh->agent_mins, g_nav_mesh->agent_maxs, points ) ) {
        return false;
    }
    
    if ( points.empty() ) {
        return false;
    }
    
    out_path->num_points = (int32_t)points.size();
    out_path->points = (Vector3 *)gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
    memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = (Vector3 *)gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 ) * out_path->num_points );
	cached.expireTime = level.time + NAV_DEBUG_PATH_RETENTION;
	s_nav_debug_cached_paths.push_back( cached );
    
	/**
	*	Debug draw: path segments.
	**/
	if ( NavDebug_Enabled() && nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		const int32_t segmentCount = std::max( 0, out_path->num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			if ( !NavDebug_CanEmitSegments( 1 ) ) {
				break;
			}

			const Vector3 &a = out_path->points[ i ];
			const Vector3 &b = out_path->points[ i + 1 ];

			// Distance filter on segment midpoint.
			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] ) * 0.5f, ( a[ 1 ] + b[ 1 ] ) * 0.5f, ( a[ 2 ] + b[ 2 ] ) * 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

    return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const bool enable_goal_z_layer_blend, const float blend_start_dist, const float blend_full_dist ) {
	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};

	if ( enable_goal_z_layer_blend ) {
		if ( !Nav_FindNodeForPosition_BlendZ( g_nav_mesh, start_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &start_node, true ) ) {
			return false;
		}

		if ( !Nav_FindNodeForPosition_BlendZ( g_nav_mesh, goal_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &goal_node, true ) ) {
			return false;
		}
	} else {
		if ( !Nav_FindNodeForPosition( g_nav_mesh, start_origin, start_origin[ 2 ], &start_node, true ) ) {
			return false;
		}
		if ( !Nav_FindNodeForPosition( g_nav_mesh, goal_origin, goal_origin[ 2 ], &goal_node, true ) ) {
			return false;
		}
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.position );
		points.push_back( goal_node.position );
    } else if ( !Nav_AStarSearch( g_nav_mesh, start_node, goal_node, g_nav_mesh->agent_mins, g_nav_mesh->agent_maxs, points ) ) {
		return false;
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = (int32_t)points.size();
	out_path->points = (Vector3 *)gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = (Vector3 *)gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 ) * out_path->num_points );
	cached.expireTime = level.time + NAV_DEBUG_PATH_RETENTION;
	s_nav_debug_cached_paths.push_back( cached );

	if ( NavDebug_Enabled() && nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		const int32_t segmentCount = std::max( 0, out_path->num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			if ( !NavDebug_CanEmitSegments( 1 ) ) {
				break;
			}

			const Vector3 &a = out_path->points[ i ];
			const Vector3 &b = out_path->points[ i + 1 ];

			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] ) * 0.5f, ( a[ 1 ] + b[ 1 ] ) * 0.5f, ( a[ 2 ] + b[ 2 ] ) * 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs ) {
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
	} else if ( !Nav_AStarSearch( g_nav_mesh, start_node, goal_node, agent_mins, agent_maxs, points ) ) {
		return false;
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );
	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const bool enable_goal_z_layer_blend, const float blend_start_dist, const float blend_full_dist ) {
	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};

	if ( enable_goal_z_layer_blend ) {
		if ( !Nav_FindNodeForPosition_BlendZ( g_nav_mesh, start_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &start_node, true ) ) {
			return false;
		}

		if ( !Nav_FindNodeForPosition_BlendZ( g_nav_mesh, goal_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &goal_node, true ) ) {
			return false;
		}
	} else {
		if ( !Nav_FindNodeForPosition( g_nav_mesh, start_origin, start_origin[ 2 ], &start_node, true ) ) {
			return false;
		}
		if ( !Nav_FindNodeForPosition( g_nav_mesh, goal_origin, goal_origin[ 2 ], &goal_node, true ) ) {
			return false;
		}
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.position );
		points.push_back( goal_node.position );
	} else if ( !Nav_AStarSearch( g_nav_mesh, start_node, goal_node, agent_mins, agent_maxs, points ) ) {
		return false;
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );
	return true;
}

/**
*   @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
*   @param  path    Path structure to free.
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
*           Advances the waypoint index as the caller reaches waypoints.
*   @param  path            Path to follow.
*   @param  current_origin  Current world-space origin.
*   @param  waypoint_radius Radius for waypoint completion.
*   @param  inout_index     Current waypoint index (updated on success).
*   @param  out_direction   Output normalized movement direction.
*   @return True if a valid direction was produced, false if path is complete/invalid.
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

	// Advance waypoints using 3D distance so stairs count toward completion.
	while ( index < path->num_points ) {
		const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
		const float dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] );
		if ( dist_sqr > waypoint_radius_sqr ) {
			break;
		}
		index++;
	}

	if ( index >= path->num_points ) {
		*inout_index = path->num_points;
		return false;
	}

	// Produce a 3D direction. Clamp vertical component to avoid large up/down jumps.
	Vector3 direction = QM_Vector3Subtract( path->points[ index ], current_origin );

	// clamp Z to a reasonable step height if the nav mesh is available
	if ( g_nav_mesh ) {
		const float maxDz = g_nav_mesh->max_step + g_nav_mesh->z_quant;
		if ( direction[ 2 ] > maxDz ) {
			direction[ 2 ] = maxDz;
		} else if ( direction[ 2 ] < -maxDz ) {
			direction[ 2 ] = -maxDz;
		}
	}

	const float length = ( float )QM_Vector3Length( direction );
	if ( length <= std::numeric_limits<float>::epsilon() ) {
		return false;
	}

	*out_direction = QM_Vector3Normalize( direction );
	*inout_index = index;
    
    return true;
}

/**
*
*
*
*	Navigation Movement Tests:
*
*
*
**/
/**
*	@brief	Low-level trace wrapper that supports world or inline-model clip.
**/
static inline cm_trace_t Nav_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end,
	const edict_ptr_t *clip_entity, const cm_contents_t mask ) {
	if ( clip_entity ) {
		return gi.clip( clip_entity, &start, &mins, &maxs, &end, mask );
	}

	return gi.trace( &start, &mins, &maxs, &end, nullptr, mask );
}

/**
*	@brief	Performs a simple PMove-like step traversal test (3-trace).
*
*			This is intentionally conservative and is used only to validate edges in A*:
*			1) Try direct horizontal move.
*			2) If blocked, try stepping up <= max step, then horizontal move.
*			3) Trace down to find ground.
*
*	@return	True if the traversal is possible, false otherwise.
**/
static bool Nav_CanTraverseStep( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t *clip_entity ) {
	if ( !mesh ) {
		return false;
	}

	// Ignore Z from caller; we compute step behavior ourselves.
	Vector3 start = startPos;
	Vector3 goal = endPos;
	goal[ 2 ] = start[ 2 ];

	const Vector3 mins = mesh->agent_mins;
	const Vector3 maxs = mesh->agent_maxs;

	return Nav_CanTraverseStep_ExplicitBBox( mesh, startPos, endPos, mins, maxs, clip_entity );
}

static bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity ) {
	if ( !mesh ) {
		return false;
	}

	// Ignore Z from caller; we compute step behavior ourselves.
	Vector3 start = startPos;
	Vector3 goal = endPos;
	goal[ 2 ] = start[ 2 ];

	// Small cushion to avoid precision issues on floors.
	const float eps = 0.25f;

	// Use nav max_step as the analogue to PM_STEP_MAX_SIZE.
	const float stepSize = mesh->max_step;

	// 1) Direct horizontal move.
	{
		cm_trace_t tr = Nav_Trace( start, mins, maxs, goal, clip_entity, CM_CONTENTMASK_SOLID );
		if ( tr.fraction >= 1.0f && !tr.allsolid && !tr.startsolid ) {
			return true;
		}
	}

	// 2) Step up (vertical trace).
	Vector3 up = start;
	up[ 2 ] += stepSize;

	cm_trace_t upTr = Nav_Trace( start, mins, maxs, up, clip_entity, CM_CONTENTMASK_SOLID );
	if ( upTr.allsolid || upTr.startsolid ) {
		return false; // can't step up
	}

	// Use actual reachable up height (may be less than stepSize).
	const float actualStep = upTr.endpos[ 2 ] - start[ 2 ];
	if ( actualStep <= eps ) {
		return false;
	}

	// 3) Horizontal move from stepped-up position.
	Vector3 steppedStart = upTr.endpos;
	Vector3 steppedGoal = goal;
	steppedGoal[ 2 ] = steppedStart[ 2 ];

	cm_trace_t fwdTr = Nav_Trace( steppedStart, mins, maxs, steppedGoal, clip_entity, CM_CONTENTMASK_SOLID );
	if ( fwdTr.allsolid || fwdTr.startsolid || fwdTr.fraction < 1.0f ) {
		return false;
	}

	// 4) Step down to find supporting ground.
	// Trace down by the step amount *plus* a small safety margin.
	Vector3 down = fwdTr.endpos;
	Vector3 downEnd = down;
	downEnd[ 2 ] -= ( actualStep + eps );

	cm_trace_t downTr = Nav_Trace( down, mins, maxs, downEnd, clip_entity, CM_CONTENTMASK_SOLID );
	if ( downTr.allsolid || downTr.startsolid ) {
		return false;
	}

	// Must land on something (not floating).
	if ( downTr.fraction >= 1.0f ) {
		return false;
	}

	// Walkable-ish contact: match your slope test convention (normal.z must be positive and within slope).
	if ( downTr.plane.normal[ 2 ] <= 0.0f ) {
		return false;
	}
	if ( !IsWalkableSlope( downTr.plane.normal, mesh->max_slope_deg ) ) {
		return false;
	}

	return true;
}



/**
*
*
*
*	Debug Drawing for NavMesh
*
*
*
**/
/**
*	@brief	Check if navigation debug drawing is enabled and draw so if it is.
**/
void SVG_Nav_DebugDraw( void ) {
	if ( !g_nav_mesh ) {
		return;
	}
	if ( !NavDebug_Enabled() ) {
		return;
	}

	// Purge old cached paths.
	NavDebug_PurgeCachedPaths();

	// Draw cached paths from this server frame.
	if ( nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		for ( const nav_debug_cached_path_t &entry : s_nav_debug_cached_paths ) {
			const nav_traversal_path_t &path = entry.path;
			if ( path.num_points <= 1 || !path.points ) {
				continue;
			}

			const int32_t segmentCount = std::max( 0, path.num_points - 1 );
			for ( int32_t i = 0; i < segmentCount; i++ ) {
				if ( !NavDebug_CanEmitSegments( 1 ) ) {
					return;
				}

				const Vector3 &a = path.points[ i ];
				const Vector3 &b = path.points[ i + 1 ];

				const Vector3 mid = { ( a[ 0 ] + b[ 0 ] ) * 0.5f, ( a[ 1 ] + b[ 1 ] ) * 0.5f, ( a[ 2 ] + b[ 2 ] ) * 0.5f };
				if ( !NavDebug_PassesDistanceFilter( mid ) ) {
					continue;
				}

				SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
				NavDebug_ConsumeSegments( 1 );
			}
		}
	}

	const nav_mesh_t *mesh = g_nav_mesh;
	const float tileWorldSize = Nav_TileWorldSize( mesh );
	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;

	/**
	*	World mesh (per leaf):
	**/
	for ( int32_t leafIndex = 0; leafIndex < mesh->num_leafs; leafIndex++ ) {
		if ( !NavDebug_FilterLeaf( leafIndex ) ) {
			continue;
		}

		const nav_leaf_data_t *leaf = &mesh->leaf_data[ leafIndex ];
		if ( !leaf || leaf->num_tiles <= 0 || !leaf->tiles ) {
			continue;
		}

		for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
			const nav_tile_t *tile = &leaf->tiles[ t ];
			if ( !tile ) {
				continue;
			}

			if ( !NavDebug_FilterTile( tile->tile_x, tile->tile_y ) ) {
				continue;
			}

			// Tile bounds.
			NavDebug_DrawTileBBox( mesh, tile );

			// Samples (top-most layer tick per XY cell).
			if ( nav_debug_draw_samples && nav_debug_draw_samples->integer != 0 ) {
				const float tileOriginX = tile->tile_x * tileWorldSize;
				const float tileOriginY = tile->tile_y * tileWorldSize;

				for ( int32_t cellIndex = 0; cellIndex < cellsPerTile; cellIndex++ ) {
					const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
					if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
						continue;
					}

					const int32_t cellX = cellIndex % mesh->tile_size;
					const int32_t cellY = cellIndex / mesh->tile_size;

					const nav_layer_t *layer = &cell->layers[ 0 ];
					Vector3 p = {
						tileOriginX + ( ( float )cellX + 0.5f ) * mesh->cell_size_xy,
						tileOriginY + ( ( float )cellY + 0.5f ) * mesh->cell_size_xy,
						layer->z_quantized * mesh->z_quant
					};

					NavDebug_DrawSampleTick( p, 12.0f );

					// If we ran out of budget, stop early.
					if ( !NavDebug_CanEmitSegments( 1 ) ) {
						return;
					}
				}
			}
		}
	}

	/**
	*	Inline model mesh (optional): draw tile bounds + samples transformed into world space.
	**/
	if ( mesh->num_inline_models <= 0 || !mesh->inline_model_data ||
		mesh->num_inline_model_runtime <= 0 || !mesh->inline_model_runtime ) {
		return;
	}

	// Map: model_index -> runtime entry (linear search; small counts, cheap enough for debug).
	auto find_runtime = [mesh]( const int32_t modelIndex ) -> const nav_inline_model_runtime_t * {
		for ( int32_t i = 0; i < mesh->num_inline_model_runtime; i++ ) {
			if ( mesh->inline_model_runtime[ i ].model_index == modelIndex ) {
				return &mesh->inline_model_runtime[ i ];
			}
		}
		return nullptr;
		};

	for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
		const nav_inline_model_data_t *model = &mesh->inline_model_data[ i ];
		if ( !model || model->num_tiles <= 0 || !model->tiles ) {
			continue;
		}

		const nav_inline_model_runtime_t *rt = find_runtime( model->model_index );
		if ( !rt ) {
			continue;
		}

		// Debug draw for inline models currently ignores rotation (angles) and uses translation only.
		// This is still useful for doors/platforms that primarily translate.
		const Vector3 origin = rt->origin;

		for ( int32_t t = 0; t < model->num_tiles; t++ ) {
			const nav_tile_t *tile = &model->tiles[ t ];
			if ( !tile ) {
				continue;
			}

			// Inline tiles are stored in model-local space where tile_x/y start at 0.
			// Provide filtering by treating these as local tile coords.
			if ( !NavDebug_FilterTile( tile->tile_x, tile->tile_y ) ) {
				continue;
			}

			// Tile bounds (translated).
			if ( NavDebug_Enabled() && nav_debug_draw_tile_bounds && nav_debug_draw_tile_bounds->integer != 0 ) {
				if ( NavDebug_CanEmitSegments( 12 ) ) {
					const Vector3 minsLocal = { tile->tile_x * tileWorldSize, tile->tile_y * tileWorldSize, -4096.0f };
					const Vector3 maxsLocal = { minsLocal[ 0 ] + tileWorldSize, minsLocal[ 1 ] + tileWorldSize, 4096.0f };

					const Vector3 minsWorld = QM_Vector3Add( minsLocal, origin );
					const Vector3 maxsWorld = QM_Vector3Add( maxsLocal, origin );

					const Vector3 center = { ( minsWorld[ 0 ] + maxsWorld[ 0 ] ) * 0.5f, ( minsWorld[ 1 ] + maxsWorld[ 1 ] ) * 0.5f, 0.0f };
					if ( NavDebug_PassesDistanceFilter( center ) ) {
						SVG_DebugDrawBBox_TE( minsWorld, maxsWorld, MULTICAST_PVS, false );
						NavDebug_ConsumeSegments( 12 );
					}
				} else {
					return;
				}
			}

			// Samples (translated).
			if ( nav_debug_draw_samples && nav_debug_draw_samples->integer != 0 ) {
				const float tileOriginXLocal = tile->tile_x * tileWorldSize;
				const float tileOriginYLocal = tile->tile_y * tileWorldSize;

				for ( int32_t cellIndex = 0; cellIndex < cellsPerTile; cellIndex++ ) {
					const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
					if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
						continue;
					}

					const int32_t cellX = cellIndex % mesh->tile_size;
					const int32_t cellY = cellIndex / mesh->tile_size;

					const nav_layer_t *layer = &cell->layers[ 0 ];
					Vector3 pLocal = {
						tileOriginXLocal + ( ( float )cellX + 0.5f ) * mesh->cell_size_xy,
						tileOriginYLocal + ( ( float )cellY + 0.5f ) * mesh->cell_size_xy,
						layer->z_quantized * mesh->z_quant
					};

					Vector3 pWorld = QM_Vector3Add( pLocal, origin );
					NavDebug_DrawSampleTick( pWorld, 12.0f );

					if ( !NavDebug_CanEmitSegments( 1 ) ) {
						return;
					}
				}
			}
		}
	}
}