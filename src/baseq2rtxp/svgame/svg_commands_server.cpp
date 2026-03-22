/********************************************************************
*
*
*	SVGame: Server(Game Admin Stuff alike) Console Commands:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_signalio.h"
// Nav2.
#include "svgame/nav2/nav2_bench.h"
#include "svgame/nav2/nav2_debug_draw.h"
#include "svgame/nav2/nav2_scheduler.h"
#include "svgame/nav2/nav2_span_adjacency.h"
#include "svgame/nav2/nav2_span_grid_build.h"
#include "svgame/nav2/nav2_types.h"
#include "svgame/nav2/nav2_save_load.h"

// Monster types for AI debug introspection.
//#include "svgame/entities/monster/svg_monster_testdummy_sfxfollow.h"

/**
*   @brief  
**/
void ServerCommand_Test_f(void)
{
    gi.cprintf(NULL, PRINT_HIGH, "ServerCommand_Test_f()\n");
}

/**
*\t@brief\tRun bounded live scheduler/query debug validation overlays for nav2 jobs.
*\t@note\tUsage: sv nav_query_debug_validate [max_jobs]
*\t\t\tThis command emits bounded job snapshot/stage diagnostics and in-world overlays
*\t\t\tthrough the Task 3.3 debug seam so scheduler/query compatibility markers can be
*\t\t\tverified under active runtime state without persistent log spam.
**/
static void ServerCommand_NavQueryDebugValidate_f( void ) {
    /**
    *\tResolve a bounded job output cap from the optional command argument.
    **/
    int32_t maxJobs = 4;
    if ( gi.argc() >= 3 ) {
        maxJobs = std::max( 1, atoi( gi.argv( 2 ) ) );
        maxJobs = std::min( maxJobs, 32 );
    }

    /**
    *\tRequire an active scheduler runtime before attempting query-stage debug validation.
    **/
    nav2_scheduler_runtime_t *schedulerRuntime = SVG_Nav2_Scheduler_GetRuntime();
    if ( !schedulerRuntime ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_query_debug_validate: scheduler runtime unavailable\n" );
        return;
    }

    /**
    *\tRequire at least one active job so the debug draw seam has live stage and snapshot state to inspect.
    **/
    if ( schedulerRuntime->jobs.empty() ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_query_debug_validate: no active nav2 jobs\n" );
        return;
    }

    /**
    *\tEmit bounded per-job debug diagnostics and overlays through the existing nav2 debug draw seam.
    **/
    const int32_t jobCount = ( int32_t )schedulerRuntime->jobs.size();
    const int32_t debugCount = std::min( maxJobs, jobCount );
    for ( int32_t i = 0; i < debugCount; i++ ) {
        SVG_Nav2_DebugDrawQueryJob( schedulerRuntime->jobs[ i ], schedulerRuntime->snapshot_runtime );
    }

    /**
    *\tPrint one compact summary line so callers can confirm bounded validation coverage.
    **/
    gi.cprintf( nullptr, PRINT_HIGH, "nav_query_debug_validate: reported %d/%d job(s)\n", debugCount, jobCount );
}

/**
*	@brief	Build and validate local span adjacency from the active nav2 span-grid snapshot.
*	@note	Usage: sv nav_span_adjacency_validate
*			This command runs a bounded legality summary so Task 4.3 can be validated
*			against live map data without per-edge log spam.
**/
static void ServerCommand_NavSpanAdjacencyValidate_f( void ) {
    /**
    *    Build a fresh sparse span-grid snapshot from the active nav2 runtime mesh first,
    *    because adjacency validation depends on currently published map and collision state.
    **/
    nav2_span_grid_t spanGrid = {};
    nav2_span_grid_build_stats_t spanBuildStats = {};
    if ( !SVG_Nav2_BuildSpanGridFromMesh( SVG_Nav2_Runtime_GetMesh(), &spanGrid, &spanBuildStats ) ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_span_adjacency_validate: span-grid build failed\n" );
        return;
    }

    /**
    *    Build local span adjacency over the generated sparse span-grid so transition legality
    *    probes execute on real runtime geometry.
    **/
    nav2_span_adjacency_t adjacency = {};
    if ( !SVG_Nav2_BuildSpanAdjacency( spanGrid, &adjacency ) ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_span_adjacency_validate: adjacency build failed\n" );
        return;
    }

    /**
    *    Emit a compact legality summary so the caller can inspect passable/soft/hard counts
    *    and explicit trace/contents invalidation totals without per-edge diagnostics.
    **/
    nav2_span_adjacency_summary_t summary = {};
    if ( !SVG_Nav2_BuildSpanAdjacencySummary( adjacency, &summary ) ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_span_adjacency_validate: summary build failed\n" );
        return;
    }

    /**
    *    Print one bounded result line for runtime validation.
    **/
    gi.cprintf( nullptr, PRINT_HIGH,
        "nav_span_adjacency_validate: sampled=%d spans=%d edges=%d passable=%d soft=%d hard=%d trace_blocked=%d contents_blocked=%d drop=%d liquid=%d\n",
        spanBuildStats.sampled_columns,
        spanBuildStats.emitted_spans,
        summary.edge_count,
        summary.passable_count,
        summary.soft_penalized_count,
        summary.hard_invalid_count,
        summary.trace_invalidated_count,
        summary.contents_invalidated_count,
        summary.controlled_drop_count,
        summary.liquid_transition_count );
}

/**
*	@brief	Build and validate the nav2 sparse span-grid on the active runtime mesh publication.
*	@note	Usage: sv nav_span_grid_validate [draw_columns]
*			When draw_columns is provided and greater than zero, the command also emits bounded
*			in-world span markers through the nav2 debug draw seam.
**/
static void ServerCommand_NavSpanGridValidate_f( void ) {
    /**
    *    Parse optional bounded draw-column count so runtime validation stays explicit and low-noise.
    **/
    int32_t drawColumns = 0;
    if ( gi.argc() >= 3 ) {
        drawColumns = std::max( 0, atoi( gi.argv( 2 ) ) );
        drawColumns = std::min( drawColumns, 256 );
    }

    /**
    *    Build a span-grid snapshot from the currently published nav2 runtime mesh so validation uses live map state.
    **/
    nav2_span_grid_t spanGrid = {};
    nav2_span_grid_build_stats_t stats = {};
    if ( !SVG_Nav2_BuildSpanGridFromMesh( SVG_Nav2_Runtime_GetMesh(), &spanGrid, &stats ) ) {
        gi.cprintf( nullptr, PRINT_HIGH, "nav_span_grid_validate: build failed (mesh/imports/bounds unavailable)\n" );
        return;
    }

    /**
    *    Report a bounded one-line summary so Task 4.2 runtime validation can confirm sampled-domain outcomes.
    **/
    gi.cprintf( nullptr, PRINT_HIGH,
        "nav_span_grid_validate: sampled=%d columns=%d spans=%d reject=(solid_point=%d floor_trace=%d floor_miss=%d slope=%d clearance=%d solid_box=%d) hazards=(water=%d slime=%d lava=%d)\n",
        stats.sampled_columns,
        stats.emitted_columns,
        stats.emitted_spans,
        stats.rejected_solid_point,
        stats.rejected_floor_trace,
        stats.rejected_floor_miss,
        stats.rejected_slope,
        stats.rejected_clearance,
        stats.rejected_solid_box,
        stats.spans_with_water,
        stats.spans_with_slime,
        stats.spans_with_lava );

    /**
    *    Optionally draw a bounded subset of sparse columns so operators can inspect real map overlays directly.
    **/
    if ( drawColumns > 0 ) {
        SVG_Nav2_DebugDrawSpanGrid( spanGrid, drawColumns );
        gi.cprintf( nullptr, PRINT_HIGH, "nav_span_grid_validate: drew %d column(s) (bounded)\n", drawColumns );
    }
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
	#if 0
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
					const nav2_tile_t *tile = &g_nav_mesh->world_tiles[ tileId ];
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
                        const nav2_tile_t *tile = &model->tiles[ t ];
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
						const nav2_tile_t *tile = &g_nav_mesh->world_tiles[ tileId ];
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
                                const nav2_tile_t *tile = &model->tiles[ t ];
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
	#endif
}

/**
*   @brief  Generate navigation voxelmesh for the current level.
**/
void ServerCommand_NavGenVoxelMesh_f( void ) {
	gi.cprintf( nullptr, PRINT_HIGH, "nav_gen: legacy voxelmesh generator removed\n" );
}

/**
*\t@brief\tSave the current navigation mesh through the nav2 runtime seam.
*\t@note\tUsage: sv nav_save <filename>
**/
static void ServerCommand_NavSave_f( void ) {
	if ( gi.argc() < 3 ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_save <filename>\n" );
		return;
	}

	const char *filename = gi.argv( 2 );
	if ( !SVG_Nav2_Runtime_SaveMesh( filename ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_save: failed to write '%s'\n", filename );
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "nav_save: wrote '%s'\n", filename );
}

/**
*\t@brief\tLoad a navigation mesh through the nav2 runtime seam.
*\t@note\tUsage: sv nav_load <filename>
**/
static void ServerCommand_NavLoad_f( void ) {
	if ( gi.argc() < 3 ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_load <filename>\n" );
		return;
	}

	const char *filename = gi.argv( 2 );
	const std::tuple<const bool, const std::string> loadResult = SVG_Nav2_Runtime_LoadMesh( filename );
	if ( !std::get<0>( loadResult ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_load: failed to read '%s'\n", filename );
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "nav_load: loaded '%s'\n", filename );
}

/**
*\t@brief\tPrint the most recent nav2 benchmark record snapshot.
*\t@note\tUsage: sv nav_query_stats
**/
static void ServerCommand_NavQueryStats_f( void ) {
	const nav2_bench_record_t *stats = SVG_Nav2_Bench_GetLastRecord();
	if ( !stats ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_query_stats: no benchmark stats recorded yet\n" );
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH,
		"%s runs=%u success=%u failure=%u total_ms=%.3f coarse_ms=%.3f fine_ms=%.3f occupancy_ms=%.3f worker_wait_ms=%.3f worker_slice_ms=%.3f serialize_read_ms=%.3f serialize_write_ms=%.3f\n",
		"nav_query_stats:",
		stats->run_count,
		stats->success_count,
		stats->failure_count,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::TotalQuery ].total_ms,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::CoarseSearch ].total_ms,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::FineRefinement ].total_ms,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::OccupancyQuery ].total_ms,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::WorkerQueueWait ].total_ms,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::WorkerExecutionSlice ].total_ms,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::SerializationRead ].total_ms,
		stats->timing[ ( size_t )nav2_bench_timing_bucket_t::SerializationWrite ].total_ms );
}

/**
*\t@brief\tBenchmark path queries are no longer wired through this server command.
*\t@note\tUsage: sv nav_bench_path <...>
**/
static void ServerCommand_NavBenchPath_f( void ) {
	gi.cprintf( nullptr, PRINT_HIGH, "nav_bench_path: legacy benchmark path removed\n" );
}

/**
*	@brief	Run a minimal static-nav serializer round-trip benchmark through the nav2 benchmark harness.
*	@note	Usage: sv nav_bench_roundtrip
*	@note	This uses a deterministic nav2-owned sample payload so the caller path exists before broader regression scenarios are wired in.
**/
static void ServerCommand_NavBenchRoundTrip_f( void ) {
    /**
    *    Build a deterministic nav2-owned sample span grid so the benchmark helper can be exercised from a concrete developer-facing command without depending on oldnav or unfinished runtime mesh extraction seams.
    **/
    nav2_span_grid_t sampleGrid = {};
    sampleGrid.tile_size = 1;
    sampleGrid.cell_size_xy = 32.0;
    sampleGrid.z_quant = 1.0;

    // Populate one sparse column with one stable traversable span.
    nav2_span_column_t sampleColumn = {};
    sampleColumn.tile_x = 0;
    sampleColumn.tile_y = 0;
    sampleColumn.tile_id = 0;

    // Populate one deterministic span carrying representative topology and movement metadata.
    nav2_span_t sampleSpan = {};
    sampleSpan.span_id = 0;
    sampleSpan.floor_z = 0.0;
    sampleSpan.ceiling_z = 64.0;
    sampleSpan.preferred_z = 16.0;
    sampleSpan.leaf_id = 1;
    sampleSpan.cluster_id = 2;
    sampleSpan.area_id = 3;
    sampleSpan.region_layer_id = 4;
    sampleSpan.movement_flags = NAV_FLAG_WALKABLE;
    sampleSpan.surface_flags = NAV_TILE_SUMMARY_NONE;
    sampleSpan.topology_flags = NAV_TRAVERSAL_FEATURE_NONE;
    sampleSpan.connector_hint_mask = 0;
    sampleColumn.spans.push_back( sampleSpan );
    sampleGrid.columns.push_back( sampleColumn );

    /**
    *    Build a deterministic adjacency payload with a single self-edge so the round-trip covers both span-grid and adjacency section encoding without needing a larger generated nav asset.
    **/
    nav2_span_adjacency_t sampleAdjacency = {};
    nav2_span_edge_t sampleEdge = {};
    sampleEdge.edge_id = 0;
    sampleEdge.from_span_id = 0;
    sampleEdge.to_span_id = 0;
    sampleEdge.from_column_index = 0;
    sampleEdge.to_column_index = 0;
    sampleEdge.from_span_index = 0;
    sampleEdge.to_span_index = 0;
    sampleEdge.delta_x = 0;
    sampleEdge.delta_y = 0;
    sampleEdge.vertical_delta = 0.0;
    sampleEdge.cost = 1.0;
    sampleEdge.soft_penalty_cost = 0.0;
    sampleEdge.movement = nav2_span_edge_movement_t::HorizontalPass;
    sampleEdge.legality = nav2_span_edge_legality_t::Passable;
    sampleEdge.feature_bits = NAV_EDGE_FEATURE_PASSABLE;
    sampleEdge.flags = NAV2_SPAN_EDGE_FLAG_PASSABLE;
    sampleAdjacency.edges.push_back( sampleEdge );

    /**
    *    Configure the round-trip as a standalone static-asset cache payload so serializer validation exercises the same format family used by the current Task 5.1 cache foundation.
    **/
    nav2_serialization_policy_t policy = {};
    policy.category = nav2_serialized_category_t::StaticAsset;
    policy.transport = nav2_serialized_transport_t::EngineFilesystem;
    policy.map_identity = 0x4E41563252544C31ull;

    /**
    *    Run the benchmark-owned round-trip helper and capture both the compact benchmark summary and the detailed serializer result for concise developer output.
    **/
    nav2_bench_roundtrip_summary_t summary = {};
    nav2_serialized_roundtrip_result_t detailedResult = {};
    const bool success = SVG_Nav2_Bench_RunStaticNavRoundTrip( policy, sampleGrid, sampleAdjacency,
        &summary, &detailedResult, "StaticNavRoundTripSample" );

    /**
    *    Optionally exercise the standalone cache workflow as well so the benchmark-owned round-trip helper is validated against a real nav cache file on disk.
    **/
    if ( gi.argc() >= 3 ) {
        // Capture the caller-supplied cache path once so the filesystem round-trip stays deterministic.
        const char *cachePath = gi.argv( 2 );

        // Build a standalone cache blob from the same sample payload used by the in-memory benchmark path.
        nav2_serialized_blob_t cacheBlob = {};
        const nav2_serialization_result_t cacheBuildResult = SVG_Nav2_Serialize_BuildStaticNavBlob( policy, sampleGrid, sampleAdjacency, &cacheBlob );
        if ( !cacheBuildResult.success ) {
            gi.cprintf( nullptr, PRINT_HIGH, "nav_bench_roundtrip: cache build failed for '%s'\n", cachePath );
        } else {
            // Write the blob through the filesystem helper so the command exercises the same cache path used by static nav assets.
            const nav2_serialization_result_t cacheWriteResult = SVG_Nav2_Serialize_WriteCacheFile( cachePath, cacheBlob );
            if ( !cacheWriteResult.success ) {
                gi.cprintf( nullptr, PRINT_HIGH, "nav_bench_roundtrip: cache write failed for '%s'\n", cachePath );
            } else {
                // Read the written file back through the engine filesystem path so the command validates the standalone cache workflow end-to-end.
                nav2_serialized_header_t cacheHeader = {};
                nav2_span_grid_t cacheGrid = {};
                nav2_span_adjacency_t cacheAdjacency = {};
                const nav2_serialization_result_t cacheReadResult = SVG_Nav2_Serialize_ReadStaticNavCacheFile( cachePath, policy,
                    &cacheHeader, &cacheGrid, &cacheAdjacency );
                if ( !cacheReadResult.success ) {
                    gi.cprintf( nullptr, PRINT_HIGH, "nav_bench_roundtrip: cache read failed for '%s' (compat=%u)\n",
                        cachePath, ( uint32_t )cacheReadResult.validation.compatibility );
                } else {
                    // Reuse the benchmark-owned round-trip helper on the decoded file payload so the cache workflow feeds back into the same regression accounting path.
                    nav2_bench_roundtrip_summary_t cacheSummary = {};
                    nav2_serialized_roundtrip_result_t cacheDetailedResult = {};
                    const bool cacheRoundTripOk = SVG_Nav2_Bench_RunStaticNavRoundTrip( policy, cacheGrid, cacheAdjacency,
                        &cacheSummary, &cacheDetailedResult, cachePath );

                    // Emit one compact cache-workflow summary line so operators can tell whether the on-disk path matched the in-memory payload.
                    gi.cprintf( nullptr, PRINT_HIGH,
                        "nav_bench_roundtrip_cache: path='%s' success=%d compat=%u primary=%s build_ms=%.3f read_ms=%.3f bytes=(%u/%u)\n",
                        cachePath,
                        cacheRoundTripOk ? 1 : 0,
                        ( uint32_t )cacheReadResult.validation.compatibility,
                        SVG_Nav2_Bench_RoundTripSummaryName( cacheSummary ),
                        cacheDetailedResult.build_elapsed_ms,
                        cacheDetailedResult.read_elapsed_ms,
                        cacheDetailedResult.build_result.byte_count,
                        cacheDetailedResult.read_result.byte_count );
                }
            }
        }
    }

    /**
    *    Print one compact result line so developers can confirm the caller seam works without introducing the log spam that earlier navigation diagnostics produced.
    **/
    gi.cprintf( nullptr, PRINT_HIGH,
        "nav_bench_roundtrip: success=%d mismatch_count=%u primary=%s build_ms=%.3f read_ms=%.3f build_bytes=%u read_bytes=%u\n",
        success ? 1 : 0,
        summary.mismatch_count,
        SVG_Nav2_Bench_RoundTripSummaryName( summary ),
        detailedResult.build_elapsed_ms,
        detailedResult.read_elapsed_ms,
        detailedResult.build_result.byte_count,
        detailedResult.read_result.byte_count );

    /**
    *    If benchmark collection is enabled, surface the latest aggregate snapshot too so developers can confirm the result flowed through the benchmark record path instead of bypassing it.
    **/
    if ( SVG_Nav2_Bench_IsEnabled() ) {
        // Reuse the existing benchmark summary output path instead of adding new ad hoc logging.
        ServerCommand_NavQueryStats_f();
    }
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
    //else if ( Q_stricmp( cmd, "nav_save" ) == 0 )
    //    ServerCommand_NavSave_f();
    //else if ( Q_stricmp( cmd, "nav_load" ) == 0 )
    //    ServerCommand_NavLoad_f();
    else if ( Q_stricmp( cmd, "nav_cell" ) == 0 )
        ServerCommand_NavCell_f();
    else if ( Q_stricmp( cmd, "nav_bench_roundtrip" ) == 0 )
        ServerCommand_NavBenchRoundTrip_f();
    else if ( Q_stricmp( cmd, "nav_span_grid_validate" ) == 0 )
        ServerCommand_NavSpanGridValidate_f();
    else if ( Q_stricmp( cmd, "nav_span_adjacency_validate" ) == 0 )
        ServerCommand_NavSpanAdjacencyValidate_f();
  else if ( Q_stricmp( cmd, "nav_query_debug_validate" ) == 0 )
        ServerCommand_NavQueryDebugValidate_f();
	//else if ( Q_stricmp( cmd, "nav_query_stats" ) == 0 )
 //       ServerCommand_NavQueryStats_f();
 //   else if ( Q_stricmp( cmd, "nav_bench_path" ) == 0 )
 //       ServerCommand_NavBenchPath_f();
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

