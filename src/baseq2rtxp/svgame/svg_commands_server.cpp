/********************************************************************
*
*
*	SVGame: Server(Game Admin Stuff alike) Console Commands:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_signalio.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_save.h"
#include "svgame/nav/svg_nav_load.h"
#include "svgame/nav/svg_nav_traversal.h"

// Monster types for AI debug introspection.
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"

/**
*   @brief  
**/
void ServerCommand_Test_f(void)
{
    gi.cprintf(NULL, PRINT_HIGH, "ServerCommand_Test_f()\n");
}

/**
*   @brief  Debug: print the navmesh tile/cell under an entity's origin.
*   @note   Usage: sv nav_cell [entnum]
*           If entnum is omitted, uses edict #1 (typical singleplayer client).
*/
/**
 *  @brief  Print navmesh cell information for an entity and optionally a small
 *          neighborhood of tiles around it.
 *  @note   Usage: sv nav_cell [entnum] [tile_radius]
 *          - entnum: optional entity number (defaults to 1)
 *          - tile_radius: optional tile neighborhood radius (defaults to 0)
 **/
static void ServerCommand_NavCell_f( void ) {
    /**
    *   Sanity: require an active nav mesh.
    **/
    if ( !g_nav_mesh ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_cell: no nav mesh loaded\n" );
        return;
    }

    // Choose entity number: optional argument or default to 1.
    int entnum = 1;
    if ( gi.argc() >= 3 ) {
        entnum = atoi( gi.argv( 2 ) );
    }

    // Optional tile neighborhood radius (in tiles).
    int32_t radius = 0;
    if ( gi.argc() >= 4 ) {
        radius = std::max( 0, atoi( gi.argv( 3 ) ) );
        radius = std::min( radius, 4 ); // clamp to reasonable neighborhood
    }

    svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entnum );
    if ( !ent || !SVG_Entity_IsActive( ent ) ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_cell: entity %d is not active\n", entnum );
        return;
    }

    /**
    *   Compute world-space position and tile/cell metrics.
    **/
    const Vector3 pos = ent->currentOrigin; // entity feet-origin
    const float cell_size_xy = g_nav_mesh->cell_size_xy;
    const float z_quant = g_nav_mesh->z_quant;
    const int32_t tile_size = g_nav_mesh->tile_size;
    const float tile_world_size = cell_size_xy * ( float )tile_size;

    // Compute base tile and cell containing the entity position.
    const int32_t baseTileX = ( int32_t )floorf( pos.x / tile_world_size );
    const int32_t baseTileY = ( int32_t )floorf( pos.y / tile_world_size );
    const float baseTileOriginX = baseTileX * tile_world_size;
    const float baseTileOriginY = baseTileY * tile_world_size;
    const int32_t baseCellX = ( int32_t )floorf( ( pos.x - baseTileOriginX ) / cell_size_xy );
    const int32_t baseCellY = ( int32_t )floorf( ( pos.y - baseTileOriginY ) / cell_size_xy );
    const int32_t baseCellIndex = baseCellY * tile_size + baseCellX;

    gi.cprintf( nullptr, PRINT_HIGH, "nav_cell: ent=%d pos=(%.1f %.1f %.1f) baseTile=(%d,%d) baseCell=(%d,%d) cellIndex=%d z_quant=%.3f cell_size_xy=%.3f radius=%d\n",
        entnum, pos.x, pos.y, pos.z, baseTileX, baseTileY, baseCellX, baseCellY, baseCellIndex, z_quant, cell_size_xy, radius );

    // Validate base cell indices before scanning.
    if ( baseCellX < 0 || baseCellX >= tile_size || baseCellY < 0 || baseCellY >= tile_size ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_cell: base cell coords out of tile bounds (tile_size=%d)\n", tile_size );
        return;
    }

    /**
    *   Inspect tiles in the neighborhood [baseTileX-radius .. baseTileX+radius].
    **/
    for ( int32_t ty = baseTileY - radius; ty <= baseTileY + radius; ty++ ) {
        for ( int32_t tx = baseTileX - radius; tx <= baseTileX + radius; tx++ ) {
            // Per-tile origin and cell coords for this tile.
            const float tileOriginX = tx * tile_world_size;
            const float tileOriginY = ty * tile_world_size;
            const int32_t cellX = ( int32_t )floorf( ( pos.x - tileOriginX ) / cell_size_xy );
            const int32_t cellY = ( int32_t )floorf( ( pos.y - tileOriginY ) / cell_size_xy );

            // Skip tiles where the computed cell is outside the tile bounds.
            if ( cellX < 0 || cellX >= tile_size || cellY < 0 || cellY >= tile_size ) {
                gi.cprintf( nullptr, PRINT_HIGH, "tile(%d,%d): cell out of bounds (cellX=%d cellY=%d)\n", tx, ty, cellX, cellY );
                continue;
            }

            const int32_t cellIndex = cellY * tile_size + cellX;
            bool tileReported = false;

            // Search world mesh leafs for this tile.
            for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
                const nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[ i ];
				if ( !leaf || leaf->num_tiles <= 0 || !leaf->tile_ids ) {
                    continue;
                }

                for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
					const int32_t tileId = leaf->tile_ids[ t ];
					if ( tileId < 0 || tileId >= (int32_t)g_nav_mesh->world_tiles.size() ) {
                        continue;
                    }
					const nav_tile_t *tile = &g_nav_mesh->world_tiles[ tileId ];
                    if ( tile->tile_x != tx || tile->tile_y != ty ) {
                        continue;
                    }

                    // Found matching world tile; report cell contents.
                    const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
                    if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
                        gi.cprintf( nullptr, PRINT_HIGH, "world tile(%d,%d) leaf=%d: cell %d empty\n", tx, ty, i, cellIndex );
                    } else {
                        gi.cprintf( nullptr, PRINT_HIGH, "world tile(%d,%d) leaf=%d: cell %d has %d layer(s)\n", tx, ty, i, cellIndex, cell->num_layers );
                        for ( int32_t li = 0; li < cell->num_layers; li++ ) {
                            const nav_layer_t &layer = cell->layers[ li ];
                            const float layerZ = layer.z_quantized * z_quant;
                            gi.cprintf( nullptr, PRINT_HIGH, "  layer %d: z=%.1f flags=0x%02x clearance=%u\n", li, layerZ, layer.flags, ( unsigned )layer.clearance );
                        }
                    }

                    tileReported = true;
                    break;
                }
                if ( tileReported ) {
                    break;
                }
            }

            // If not reported in world leafs, check inline-model tiles.
            if ( !tileReported && g_nav_mesh->num_inline_models > 0 && g_nav_mesh->inline_model_data ) {
                for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
                    const nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[ i ];
                    if ( !model || model->num_tiles <= 0 || !model->tiles ) {
                        continue;
                    }
                    for ( int32_t t = 0; t < model->num_tiles; t++ ) {
                        const nav_tile_t *tile = &model->tiles[ t ];
                        if ( !tile ) {
                            continue;
                        }
                        if ( tile->tile_x != tx || tile->tile_y != ty ) {
                            continue;
                        }

                        const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
                        if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
                            gi.cprintf( nullptr, PRINT_HIGH, "inline tile(%d,%d) model=%d: cell %d empty\n", tx, ty, model->model_index, cellIndex );
                        } else {
                            gi.cprintf( nullptr, PRINT_HIGH, "inline tile(%d,%d) model=%d: cell %d has %d layer(s)\n", tx, ty, model->model_index, cellIndex, cell->num_layers );
                            for ( int32_t li = 0; li < cell->num_layers; li++ ) {
                                const nav_layer_t &layer = cell->layers[ li ];
                                const float layerZ = layer.z_quantized * z_quant;
                                gi.cprintf( nullptr, PRINT_HIGH, "  layer %d: z=%.1f flags=0x%02x clearance=%u\n", li, layerZ, layer.flags, ( unsigned )layer.clearance );
                            }
                        }

                        tileReported = true;
                        break;
                    }
                    if ( tileReported ) {
                        break;
                    }
                }
            }

            if ( !tileReported ) {
                gi.cprintf( nullptr, PRINT_HIGH, "tile(%d,%d): no tile found\n", tx, ty );
            }
        }
    }

    /**
    *   Print a compact ASCII neighborhood map summarizing walkability for the
    *   inspected tile range. This is useful for a quick visual check of which
    *   nearby tiles/cells contain walkable layers.
    **/
    if ( radius > 0 ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_cell: ASCII neighborhood map (center = C, X = walkable, . = empty, ? = no tile)\n" );
        // Print rows from top (higher Y) to bottom to match world Y axis.
        for ( int32_t ty = baseTileY + radius; ty >= baseTileY - radius; ty-- ) {
            // Row prefix with tile Y coordinate for readability.
            gi.cprintf( nullptr, PRINT_HIGH, "%4d: ", ty );
            for ( int32_t tx = baseTileX - radius; tx <= baseTileX + radius; tx++ ) {
                // Compute the cell within this tile that corresponds to the entity's world XY.
                const float tileOriginX = tx * tile_world_size;
                const float tileOriginY = ty * tile_world_size;
                const int32_t cellX = ( int32_t )floorf( ( pos.x - tileOriginX ) / cell_size_xy );
                const int32_t cellY = ( int32_t )floorf( ( pos.y - tileOriginY ) / cell_size_xy );
                char symbol = '?';

                // If the computed cell lies outside the tile, mark as out-of-bounds.
                if ( cellX < 0 || cellX >= tile_size || cellY < 0 || cellY >= tile_size ) {
                    symbol = '?';
                } else {
                    const int32_t cellIndex = cellY * tile_size + cellX;
                    bool foundTile = false;
                    bool hasLayers = false;

                    // Search world tiles.
                    for ( int32_t i = 0; i < g_nav_mesh->num_leafs && !foundTile; i++ ) {
                        const nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[ i ];
					if ( !leaf || leaf->num_tiles <= 0 || !leaf->tile_ids ) {
                            continue;
                        }
                        for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
						const int32_t tileId = leaf->tile_ids[ t ];
						if ( tileId < 0 || tileId >= (int32_t)g_nav_mesh->world_tiles.size() ) {
                                continue;
                            }
						const nav_tile_t *tile = &g_nav_mesh->world_tiles[ tileId ];
                            if ( tile->tile_x != tx || tile->tile_y != ty ) {
                                continue;
                            }
                            foundTile = true;
                            const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
                            if ( cell && cell->num_layers > 0 && cell->layers ) {
                                hasLayers = true;
                            }
                            break;
                        }
                    }

                    // If not found in world tiles, check inline-model tiles.
                    if ( !foundTile && g_nav_mesh->num_inline_models > 0 && g_nav_mesh->inline_model_data ) {
                        for ( int32_t i = 0; i < g_nav_mesh->num_inline_models && !foundTile; i++ ) {
                            const nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[ i ];
                            if ( !model || model->num_tiles <= 0 || !model->tiles ) {
                                continue;
                            }
                            for ( int32_t t = 0; t < model->num_tiles; t++ ) {
                                const nav_tile_t *tile = &model->tiles[ t ];
                                if ( !tile ) {
                                    continue;
                                }
                                if ( tile->tile_x != tx || tile->tile_y != ty ) {
                                    continue;
                                }
                                foundTile = true;
                                const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
                                if ( cell && cell->num_layers > 0 && cell->layers ) {
                                    hasLayers = true;
                                }
                                break;
                            }
                        }
                    }

                    // Decide symbol based on findings.
                    if ( foundTile ) {
                        symbol = hasLayers ? 'X' : '.';
                    } else {
                        symbol = '?';
                    }
                }

                // Mark center tile with 'C' for easier spotting.
                if ( tx == baseTileX && ty == baseTileY ) {
                    gi.cprintf( nullptr, PRINT_HIGH, "%c ", 'C' );
                } else {
                    gi.cprintf( nullptr, PRINT_HIGH, "%c ", symbol );
                }
            }
            gi.cprintf( nullptr, PRINT_HIGH, "\n" );
        }
    }
}

/**
*   @brief  Generate navigation voxelmesh for the current level.
**/
void ServerCommand_NavGenVoxelMesh_f(void)
{
    SVG_Nav_GenerateVoxelMesh();
}

/**
*	@brief	Save the current navigation voxelmesh to disk.
*	@note	Usage: sv nav_save <filename>
**/
static void ServerCommand_NavSave_f( void ) {
	if ( gi.argc() < 3 ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_save <filename>\n" );
		return;
	}

	const char *filename = gi.argv( 2 );
	if ( !SVG_Nav_SaveVoxelMesh( filename ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_save: failed to write '%s'\n", filename );
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "nav_save: wrote '%s'\n", filename );
}

/**
*	@brief	Load a navigation voxelmesh from disk into memory.
*	@note	Usage: sv nav_load <filename>
**/
static void ServerCommand_NavLoad_f( void ) {
	if ( gi.argc() < 3 ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_load <filename>\n" );
		return;
	}

	const char *filename = gi.argv( 2 );
	if ( !SVG_Nav_LoadVoxelMesh( filename ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_load: failed to read '%s'\n", filename );
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "nav_load: loaded '%s'\n", filename );
}

/**
*   @brief	Print one structured navigation query stats snapshot.
*   @param	prefix	Short prefix describing the caller or output mode.
*   @param	stats	Cached navigation query stats snapshot to print.
**/
static void ServerCommand_NavPrintQueryStats( const char *prefix, const nav_query_debug_stats_t &stats ) {
    /**
    *   Use a stable field order so benchmark output is easy to diff across runs.
    **/
    gi.cprintf( nullptr, PRINT_HIGH,
        "%s valid=%d path_found=%d total_ms=%.3f coarse_ms=%.3f refine_ms=%.3f leaf_expansions=%d coarse_expansions=%d open_pushes=%d open_pops=%d los_tests=%d los_successes=%d fallback_used=%d hierarchy_preferred=%d hierarchy_fallback_used=%d unrestricted_refine_used=%d used_hierarchy_route=%d used_cluster_route=%d used_refine_corridor=%d corridor_regions=%d corridor_portals=%d corridor_tiles=%d corridor_buffer_radius=%d corridor_buffered_tiles=%d simplify_attempts=%d simplify_successes=%d simplify_input_waypoints=%d simplify_output_waypoints=%d simplify_duplicate_prunes=%d simplify_collinear_prunes=%d simplify_fallback_local_replans=%d simplify_ms=%.3f occupancy_overlay_hits=%d occupancy_soft_cost_hits=%d occupancy_block_rejects=%d occupancy_los_rejects=%d portal_overlay_blocks=%d portal_overlay_penalties=%d waypoint_count=%d\n",
        prefix ? prefix : "nav_query_stats:",
        stats.valid ? 1 : 0,
        stats.path_found ? 1 : 0,
        stats.total_ms,
        stats.coarse_ms,
        stats.refine_ms,
        stats.leaf_expansions,
        stats.coarse_expansions,
        stats.open_pushes,
        stats.open_pops,
        stats.los_tests,
        stats.los_successes,
        stats.fallback_used ? 1 : 0,
        stats.hierarchy_preferred ? 1 : 0,
        stats.hierarchy_fallback_used ? 1 : 0,
        stats.unrestricted_refine_used ? 1 : 0,
        stats.used_hierarchy_route ? 1 : 0,
        stats.used_cluster_route ? 1 : 0,
        stats.used_refine_corridor ? 1 : 0,
        stats.corridor_region_count,
        stats.corridor_portal_count,
        stats.corridor_tile_count,
        stats.corridor_buffer_radius,
        stats.corridor_buffered_tile_count,
        stats.simplify_attempts,
        stats.simplify_successes,
        stats.simplify_input_waypoint_count,
        stats.simplify_output_waypoint_count,
        stats.simplify_duplicate_prunes,
        stats.simplify_collinear_prunes,
        stats.simplify_fallback_local_replans,
        stats.simplify_ms,
        stats.occupancy_overlay_hits,
        stats.occupancy_soft_cost_hits,
        stats.occupancy_block_rejects,
        stats.occupancy_los_rejects,
        stats.portal_overlay_block_count,
        stats.portal_overlay_penalty_count,
        stats.waypoint_count );
}

/**
*   @brief	Print the most recent structured sync navigation query stats snapshot.
*   @note	Usage: sv nav_query_stats
**/
static void ServerCommand_NavQueryStats_f( void ) {
    /**
    *   Fetch the cached stats snapshot captured by the sync traversal APIs.
    **/
    const nav_query_debug_stats_t &stats = SVG_Nav_GetLastQueryStats();
    if ( !stats.valid ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_query_stats: no query stats recorded yet\n" );
        return;
    }

    /**
    *   Print the cached snapshot in one structured line for easy copy/paste and comparison.
    **/
    ServerCommand_NavPrintQueryStats( "nav_query_stats:", stats );
}

/**
*   @brief	Run a deterministic sync path benchmark for explicit start/goal coordinates.
*   @note	Usage: sv nav_bench_path <sx> <sy> <sz> <gx> <gy> <gz> [iterations]
**/
static void ServerCommand_NavBenchPath_f( void ) {
    /**
    *   Require a loaded navmesh and the explicit benchmark coordinates.
    **/
    if ( !g_nav_mesh ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_bench_path: no nav mesh loaded\n" );
        return;
    }
    if ( gi.argc() < 8 ) {
        gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_bench_path <sx> <sy> <sz> <gx> <gy> <gz> [iterations]\n" );
        return;
    }

    /**
    *   Parse the caller-provided benchmark endpoints and iteration count.
    **/
    const Vector3 start_origin = {
        ( float )atof( gi.argv( 2 ) ),
        ( float )atof( gi.argv( 3 ) ),
        ( float )atof( gi.argv( 4 ) )
    };
    const Vector3 goal_origin = {
        ( float )atof( gi.argv( 5 ) ),
        ( float )atof( gi.argv( 6 ) ),
        ( float )atof( gi.argv( 7 ) )
    };
    int32_t iterations = 16;
    if ( gi.argc() >= 9 ) {
        iterations = std::max( 1, atoi( gi.argv( 8 ) ) );
        iterations = std::min( iterations, 4096 );
    }

    /**
    *   Resolve the benchmark agent hull from the loaded nav mesh, falling back to the nav-agent profile if needed.
    **/
    Vector3 agent_mins = g_nav_mesh->agent_mins;
    Vector3 agent_maxs = g_nav_mesh->agent_maxs;
    if ( !( agent_maxs.x > agent_mins.x ) || !( agent_maxs.y > agent_mins.y ) || !( agent_maxs.z > agent_mins.z ) ) {
        const nav_agent_profile_t agent_profile = SVG_Nav_BuildAgentProfileFromCvars();
        agent_mins = agent_profile.mins;
        agent_maxs = agent_profile.maxs;
    }

    /**
    *   Aggregate deterministic benchmark metrics across repeated sync query runs.
    **/
    int32_t success_count = 0;
    double total_ms_sum = 0.0;
    double coarse_ms_sum = 0.0;
    double refine_ms_sum = 0.0;
    double min_total_ms = 0.0;
    double max_total_ms = 0.0;
    int32_t leaf_expansion_sum = 0;
    int32_t coarse_expansion_sum = 0;
    int32_t open_push_sum = 0;
    int32_t open_pop_sum = 0;
    int32_t waypoint_sum = 0;
    int32_t fallback_sum = 0;
    int32_t hierarchy_route_sum = 0;
    int32_t cluster_route_sum = 0;
    int32_t refine_corridor_sum = 0;
    int32_t corridor_region_sum = 0;
    int32_t corridor_portal_sum = 0;
    int32_t corridor_tile_sum = 0;
    int32_t simplify_attempt_sum = 0;
    int32_t simplify_success_sum = 0;
    int32_t occupancy_overlay_hit_sum = 0;
    int32_t occupancy_soft_cost_hit_sum = 0;
    int32_t occupancy_block_reject_sum = 0;
    int32_t occupancy_los_reject_sum = 0;
    int32_t portal_overlay_block_sum = 0;
    int32_t portal_overlay_penalty_sum = 0;
    int32_t hierarchy_preferred_sum = 0;
    int32_t hierarchy_fallback_sum = 0;
    int32_t unrestricted_refine_sum = 0;
    int32_t corridor_buffer_radius_sum = 0;
    int32_t corridor_buffered_tile_sum = 0;
    int32_t simplify_input_waypoint_sum = 0;
    int32_t simplify_output_waypoint_sum = 0;
    int32_t simplify_duplicate_prune_sum = 0;
    int32_t simplify_collinear_prune_sum = 0;
    int32_t simplify_fallback_replan_sum = 0;
    double simplify_ms_sum = 0.0;
    bool captured_sample = false;

    /**
    *   Execute the same sync query repeatedly so the output remains reproducible for comparison.
    **/
    for ( int32_t iteration = 0; iteration < iterations; iteration++ ) {
        nav_traversal_path_t path = {};
        const bool path_ok = SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( start_origin, goal_origin, &path, agent_mins, agent_maxs, nullptr );
        if ( path_ok ) {
            success_count++;
        }

        const nav_query_debug_stats_t &stats = SVG_Nav_GetLastQueryStats();
        if ( stats.valid ) {
            total_ms_sum += stats.total_ms;
            coarse_ms_sum += stats.coarse_ms;
            refine_ms_sum += stats.refine_ms;
            leaf_expansion_sum += stats.leaf_expansions;
            coarse_expansion_sum += stats.coarse_expansions;
            open_push_sum += stats.open_pushes;
            open_pop_sum += stats.open_pops;
            waypoint_sum += stats.waypoint_count;
            fallback_sum += stats.fallback_used ? 1 : 0;
            hierarchy_route_sum += stats.used_hierarchy_route ? 1 : 0;
            cluster_route_sum += stats.used_cluster_route ? 1 : 0;
            refine_corridor_sum += stats.used_refine_corridor ? 1 : 0;
            corridor_region_sum += stats.corridor_region_count;
            corridor_portal_sum += stats.corridor_portal_count;
            corridor_tile_sum += stats.corridor_tile_count;
            simplify_attempt_sum += stats.simplify_attempts;
            simplify_success_sum += stats.simplify_successes;
            occupancy_overlay_hit_sum += stats.occupancy_overlay_hits;
            occupancy_soft_cost_hit_sum += stats.occupancy_soft_cost_hits;
            occupancy_block_reject_sum += stats.occupancy_block_rejects;
            occupancy_los_reject_sum += stats.occupancy_los_rejects;
            portal_overlay_block_sum += stats.portal_overlay_block_count;
            portal_overlay_penalty_sum += stats.portal_overlay_penalty_count;
            hierarchy_preferred_sum += stats.hierarchy_preferred ? 1 : 0;
            hierarchy_fallback_sum += stats.hierarchy_fallback_used ? 1 : 0;
            unrestricted_refine_sum += stats.unrestricted_refine_used ? 1 : 0;
            corridor_buffer_radius_sum += stats.corridor_buffer_radius;
            corridor_buffered_tile_sum += stats.corridor_buffered_tile_count;
            simplify_input_waypoint_sum += stats.simplify_input_waypoint_count;
            simplify_output_waypoint_sum += stats.simplify_output_waypoint_count;
            simplify_duplicate_prune_sum += stats.simplify_duplicate_prunes;
            simplify_collinear_prune_sum += stats.simplify_collinear_prunes;
            simplify_fallback_replan_sum += stats.simplify_fallback_local_replans;
            simplify_ms_sum += stats.simplify_ms;
            if ( !captured_sample ) {
                min_total_ms = stats.total_ms;
                max_total_ms = stats.total_ms;
                captured_sample = true;
            } else {
                min_total_ms = std::min( min_total_ms, stats.total_ms );
                max_total_ms = std::max( max_total_ms, stats.total_ms );
            }
        }

        /**
        *   Free any temporary waypoint storage before the next benchmark iteration.
        **/
        SVG_Nav_FreeTraversalPath( &path );
    }

    /**
    *   Print the benchmark summary using a stable field order for easy run-to-run comparison.
    **/
    if ( !captured_sample ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_bench_path: no query stats captured\n" );
        return;
    }

    const double inv_iterations = 1.0 / ( double )iterations;
    gi.cprintf( nullptr, PRINT_HIGH,
        "nav_bench_path: iterations=%d success_count=%d start=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) avg_total_ms=%.3f min_total_ms=%.3f max_total_ms=%.3f avg_coarse_ms=%.3f avg_refine_ms=%.3f avg_leaf_expansions=%.2f avg_coarse_expansions=%.2f avg_open_pushes=%.2f avg_open_pops=%.2f avg_waypoint_count=%.2f fallback_rate=%.3f hierarchy_preferred_rate=%.3f hierarchy_fallback_rate=%.3f unrestricted_refine_rate=%.3f hierarchy_route_rate=%.3f cluster_route_rate=%.3f corridor_rate=%.3f avg_corridor_regions=%.2f avg_corridor_portals=%.2f avg_corridor_tiles=%.2f avg_corridor_buffer_radius=%.2f avg_corridor_buffered_tiles=%.2f avg_simplify_attempts=%.2f avg_simplify_successes=%.2f avg_simplify_input_waypoints=%.2f avg_simplify_output_waypoints=%.2f avg_simplify_duplicate_prunes=%.2f avg_simplify_collinear_prunes=%.2f avg_simplify_fallback_local_replans=%.2f avg_simplify_ms=%.3f avg_occupancy_overlay_hits=%.2f avg_occupancy_soft_cost_hits=%.2f avg_occupancy_block_rejects=%.2f avg_occupancy_los_rejects=%.2f avg_portal_overlay_blocks=%.2f avg_portal_overlay_penalties=%.2f\n",
        iterations,
        success_count,
        start_origin.x, start_origin.y, start_origin.z,
        goal_origin.x, goal_origin.y, goal_origin.z,
        total_ms_sum * inv_iterations,
        min_total_ms,
        max_total_ms,
        coarse_ms_sum * inv_iterations,
        refine_ms_sum * inv_iterations,
        ( double )leaf_expansion_sum * inv_iterations,
        ( double )coarse_expansion_sum * inv_iterations,
        ( double )open_push_sum * inv_iterations,
        ( double )open_pop_sum * inv_iterations,
        ( double )waypoint_sum * inv_iterations,
        ( double )fallback_sum * inv_iterations,
        ( double )hierarchy_preferred_sum * inv_iterations,
        ( double )hierarchy_fallback_sum * inv_iterations,
        ( double )unrestricted_refine_sum * inv_iterations,
        ( double )hierarchy_route_sum * inv_iterations,
        ( double )cluster_route_sum * inv_iterations,
        ( double )refine_corridor_sum * inv_iterations,
        ( double )corridor_region_sum * inv_iterations,
        ( double )corridor_portal_sum * inv_iterations,
        ( double )corridor_tile_sum * inv_iterations,
        ( double )corridor_buffer_radius_sum * inv_iterations,
        ( double )corridor_buffered_tile_sum * inv_iterations,
        ( double )simplify_attempt_sum * inv_iterations,
        ( double )simplify_success_sum * inv_iterations,
        ( double )simplify_input_waypoint_sum * inv_iterations,
        ( double )simplify_output_waypoint_sum * inv_iterations,
        ( double )simplify_duplicate_prune_sum * inv_iterations,
        ( double )simplify_collinear_prune_sum * inv_iterations,
        ( double )simplify_fallback_replan_sum * inv_iterations,
        simplify_ms_sum * inv_iterations,
        ( double )occupancy_overlay_hit_sum * inv_iterations,
        ( double )occupancy_soft_cost_hit_sum * inv_iterations,
        ( double )occupancy_block_reject_sum * inv_iterations,
        ( double )occupancy_los_reject_sum * inv_iterations,
        ( double )portal_overlay_block_sum * inv_iterations,
        ( double )portal_overlay_penalty_sum * inv_iterations );

    /**
    *   Print the final single-run snapshot as well so raw structured fields remain visible.
    **/
    ServerCommand_NavPrintQueryStats( "nav_bench_path:last_query", SVG_Nav_GetLastQueryStats() );
}


/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/

typedef struct {
    unsigned    mask;
    unsigned    compare;
} ipfilter_t;

#define MAX_IPFILTERS   1024

ipfilter_t  ipfilters[MAX_IPFILTERS];
int         numipfilters;

/*
=================
StringToFilter
=================
*/
static bool StringToFilter(char *s, ipfilter_t *f)
{
    char    num[128];
    int     i, j;
    union {
        byte bytes[4];
        unsigned u32;
    } b, m;

    b.u32 = m.u32 = 0;

    for (i = 0 ; i < 4 ; i++) {
        if (*s < '0' || *s > '9') {
            gi.cprintf(NULL, PRINT_HIGH, "Bad filter address: %s\n", s);
            return false;
        }

        j = 0;
        while (*s >= '0' && *s <= '9') {
            num[j++] = *s++;
        }
        num[j] = 0;
        b.bytes[i] = atoi(num);
        if (b.bytes[i] != 0)
            m.bytes[i] = 255;

        if (!*s)
            break;
        s++;
    }

    f->mask = m.u32;
    f->compare = b.u32;

    return true;
}

/*
=================
SVG_FilterPacket
=================
*/
const bool SVG_FilterPacket(char *from)
{
    int     i;
    unsigned    in;
    union {
        byte b[4];
        unsigned u32;
    } m;
    char *p;

    m.u32 = 0;

    i = 0;
    p = from;
    while (*p && i < 4) {
        m.b[i] = 0;
        while (*p >= '0' && *p <= '9') {
            m.b[i] = m.b[i] * 10 + (*p - '0');
            p++;
        }
        if (!*p || *p == ':')
            break;
        i++, p++;
    }

    in = m.u32;

    for (i = 0 ; i < numipfilters ; i++)
        if ((in & ipfilters[i].mask) == ipfilters[i].compare)
            return (int)filterban->value;

    return (int)!filterban->value;
}


/*
=================
SV_AddIP_f
=================
*/
void ServerCommand_AddIP_f(void)
{
    int     i;

    if (gi.argc() < 3) {
        gi.cprintf(NULL, PRINT_HIGH, "Usage:  addip <ip-mask>\n");
        return;
    }

    for (i = 0 ; i < numipfilters ; i++)
        if (ipfilters[i].compare == 0xffffffff)
            break;      // free spot
    if (i == numipfilters) {
        if (numipfilters == MAX_IPFILTERS) {
            gi.cprintf(NULL, PRINT_HIGH, "IP filter list is full\n");
            return;
        }
        numipfilters++;
    }

    if (!StringToFilter(gi.argv(2), &ipfilters[i]))
        ipfilters[i].compare = 0xffffffff;
}

/*
=================
SV_RemoveIP_f
=================
*/
void ServerCommand_RemoveIP_f(void)
{
    ipfilter_t  f;
    int         i, j;

    if (gi.argc() < 3) {
        gi.cprintf(NULL, PRINT_HIGH, "Usage:  sv removeip <ip-mask>\n");
        return;
    }

    if (!StringToFilter(gi.argv(2), &f))
        return;

    for (i = 0 ; i < numipfilters ; i++)
        if (ipfilters[i].mask == f.mask
            && ipfilters[i].compare == f.compare) {
            for (j = i + 1 ; j < numipfilters ; j++)
                ipfilters[j - 1] = ipfilters[j];
            numipfilters--;
            gi.cprintf(NULL, PRINT_HIGH, "Removed.\n");
            return;
        }
    gi.cprintf(NULL, PRINT_HIGH, "Didn't find %s.\n", gi.argv(2));
}

/*
=================
SV_ListIP_f
=================
*/
void ServerCommand_ListIP_f(void)
{
    int     i;
    union {
        byte    b[4];
        unsigned u32;
    } b;

    gi.cprintf(NULL, PRINT_HIGH, "Filter list:\n");
    for (i = 0 ; i < numipfilters ; i++) {
        b.u32 = ipfilters[i].compare;
        gi.cprintf(NULL, PRINT_HIGH, "%3i.%3i.%3i.%3i\n", b.b[0], b.b[1], b.b[2], b.b[3]);
    }
}

/*
=================
SV_WriteIP_f
=================
*/
void ServerCommand_WriteIP_f(void)
{
    FILE    *f;
    char    name[MAX_OSPATH];
    size_t  len;
    union {
        byte    b[4];
        unsigned u32;
    } b;
    int     i;
    cvar_t  *cvar_game;

    cvar_game = gi.cvar("game", "", 0);

    if (!*cvar_game->string)
        len = Q_snprintf(name, sizeof(name), "%s/listip.cfg", GAMEVERSION);
    else
        len = Q_snprintf(name, sizeof(name), "%s/listip.cfg", cvar_game->string);

    if (len >= sizeof(name)) {
        gi.cprintf(NULL, PRINT_HIGH, "File name too long\n");
        return;
    }

    gi.cprintf(NULL, PRINT_HIGH, "Writing %s.\n", name);

    f = fopen(name, "wb");
    if (!f) {
        gi.cprintf(NULL, PRINT_HIGH, "Couldn't open %s\n", name);
        return;
    }

    fprintf(f, "set filterban %d\n", (int)filterban->value);

    for (i = 0 ; i < numipfilters ; i++) {
        b.u32 = ipfilters[i].compare;
        fprintf(f, "sv addip %i.%i.%i.%i\n", b.b[0], b.b[1], b.b[2], b.b[3]);
    }

    fclose(f);
}

/**
*   @brief  SVG_ServerCommand will be called when an "sv" command is issued.
*           The game can issue gi.argc() / gi.argv() commands to get the rest
*           of the parameters
**/
void SVG_ServerCommand(void) {
    char    *cmd;

    cmd = gi.argv(1);
    if ( Q_stricmp( cmd, "test" ) == 0 )
        ServerCommand_Test_f();
    else if ( Q_stricmp( cmd, "nav_gen_voxelmesh" ) == 0 )
        ServerCommand_NavGenVoxelMesh_f();
    else if ( Q_stricmp( cmd, "nav_save" ) == 0 )
        ServerCommand_NavSave_f();
    else if ( Q_stricmp( cmd, "nav_load" ) == 0 )
        ServerCommand_NavLoad_f();
    else if ( Q_stricmp( cmd, "nav_cell" ) == 0 )
        ServerCommand_NavCell_f();
  else if ( Q_stricmp( cmd, "nav_query_stats" ) == 0 )
        ServerCommand_NavQueryStats_f();
    else if ( Q_stricmp( cmd, "nav_bench_path" ) == 0 )
        ServerCommand_NavBenchPath_f();
    else if ( Q_stricmp( cmd, "addip" ) == 0 )
        ServerCommand_AddIP_f();
    else if ( Q_stricmp( cmd, "removeip" ) == 0 )
        ServerCommand_RemoveIP_f();
    else if ( Q_stricmp( cmd, "listip" ) == 0 )
        ServerCommand_ListIP_f();
    else if ( Q_stricmp( cmd, "writeip" ) == 0 )
        ServerCommand_WriteIP_f();
    else
        gi.cprintf( NULL, PRINT_HIGH, "Unknown server command \"%s\"\n", cmd );
}

