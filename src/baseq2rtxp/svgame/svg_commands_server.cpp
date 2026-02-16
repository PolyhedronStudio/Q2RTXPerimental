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
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"

// Monster types for AI debug introspection.
#include "svgame/entities/monster/svg_monster_testdummy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

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
*	@brief	Generate and immediately save a navigation voxelmesh cache file.
*	@note	Usage: sv nav_bake [filename]
**/
static void ServerCommand_NavBake_f( void ) {
	char defaultFilename[ MAX_QPATH ] = {};
	const char *filename = nullptr;

	if ( gi.argc() >= 3 ) {
		filename = gi.argv( 2 );
	} else if ( SVG_Nav_BuildDefaultCacheFilename( defaultFilename, sizeof( defaultFilename ) ) ) {
		filename = defaultFilename;
	} else {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_bake <filename>\n" );
		return;
	}

	SVG_Nav_GenerateVoxelMesh();
	if ( !g_nav_mesh ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_bake: generation failed (mesh unavailable)\n" );
		return;
	}

	if ( !SVG_Nav_SaveVoxelMesh( filename ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_bake: failed to write '%s'\n", filename );
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "nav_bake: generated + wrote '%s'\n", filename );
}

/**
*	@brief	Print a compact runtime dashboard for nav mesh + async queue profiling.
*	@note	Usage: sv nav_profile_dump
**/
static void ServerCommand_NavProfileDump_f( void ) {
	const nav_mesh_t *mesh = g_nav_mesh.get();
	nav_async_queue_profile_t queueProfile = {};
	nav_backend_profile_t backendProfile = {};
	SVG_Nav_GetAsyncQueueProfile( &queueProfile );
	SVG_Nav_GetBackendProfile( &backendProfile );

	const int32_t queueTimeBudgetMs = SVG_Nav_GetAsyncQueueFrameBudgetMs();
	const double avgQueueWaitMs = ( queueProfile.queue_wait_samples > 0 )
		? ( ( double )queueProfile.total_queue_wait_ms / ( double )queueProfile.queue_wait_samples )
		: 0.0;

	char cacheFilename[ MAX_QPATH ] = {};
	const bool hasDefaultCacheName = SVG_Nav_BuildDefaultCacheFilename( cacheFilename, sizeof( cacheFilename ) );

	gi.cprintf( nullptr, PRINT_HIGH, "=== nav_profile_dump ===\n" );
	gi.cprintf( nullptr, PRINT_HIGH, "backend=%s mesh_loaded=%d map=%s\n",
		SVG_Nav_GetPathBackendName(),
		mesh ? 1 : 0,
		level.mapname[ 0 ] ? level.mapname : "<none>" );
	gi.cprintf( nullptr, PRINT_HIGH, "cache auto_load=%d auto_bake_missing=%d auto_save_after_bake=%d require_hash_match=%d default_cache=%s\n",
		nav_cache_auto_load ? nav_cache_auto_load->integer : 0,
		nav_cache_auto_bake_missing ? nav_cache_auto_bake_missing->integer : 0,
		nav_cache_auto_save_after_bake ? nav_cache_auto_save_after_bake->integer : 0,
		nav_cache_require_hash_match ? nav_cache_require_hash_match->integer : 0,
		hasDefaultCacheName ? cacheFilename : "<unavailable>" );

	if ( mesh ) {
		gi.cprintf( nullptr, PRINT_HIGH, "mesh world_tiles=%d inline_models=%d total_tiles=%d total_cells=%d total_layers=%d cell_size=%.2f z_quant=%.2f tile_size=%d\n",
			( int )mesh->world_tiles.size(),
			mesh->num_inline_models,
			mesh->total_tiles,
			mesh->total_xy_cells,
			mesh->total_layers,
			( float )mesh->cell_size_xy,
			( float )mesh->z_quant,
			mesh->tile_size );
		gi.cprintf( nullptr, PRINT_HIGH, "generation world_ms=%.2f inline_ms=%.2f total_ms=%.2f generated_at_ms=%llu\n",
			mesh->profile_world_gen_ms,
			mesh->profile_inline_gen_ms,
			mesh->profile_total_gen_ms,
			( unsigned long long )mesh->profile_generated_at_ms );
	}

	gi.cprintf( nullptr, PRINT_HIGH, "async queue_depth=%d queue_peak=%d enqueued=%d refreshed=%d processed=%d completed=%d failed=%d cancelled=%d prepare_failures=%d expansions=%lld\n",
		queueProfile.queue_depth,
		queueProfile.queue_peak_depth,
		queueProfile.total_enqueued,
		queueProfile.total_refreshed,
		queueProfile.total_processed,
		queueProfile.total_completed,
		queueProfile.total_failed,
		queueProfile.total_cancelled,
		queueProfile.total_prepare_failures,
		( long long )queueProfile.total_expansions );
	gi.cprintf( nullptr, PRINT_HIGH, "async wait_avg_ms=%.2f wait_max_ms=%llu wait_samples=%d frame_elapsed_ms=%llu frame_processed=%d frame_expansions=%d budget_req=%d budget_expand=%d budget_queue_ms=%d configured_queue_budget_ms=%d over_budget_frames=%d\n",
		avgQueueWaitMs,
		( unsigned long long )queueProfile.max_queue_wait_ms,
		queueProfile.queue_wait_samples,
		( unsigned long long )queueProfile.last_frame_elapsed_ms,
		queueProfile.last_frame_processed,
		queueProfile.last_frame_expansions,
		queueProfile.last_frame_request_budget,
		queueProfile.last_frame_expansion_budget,
		queueProfile.last_frame_queue_budget_ms,
		queueTimeBudgetMs,
		queueProfile.frame_over_budget_count );

	const double detourAvgMs = ( backendProfile.detour_queries > 0 )
		? ( ( double )backendProfile.detour_total_ms / ( double )backendProfile.detour_queries )
		: 0.0;
	gi.cprintf( nullptr, PRINT_HIGH, "backend detour_queries=%d detour_success=%d detour_failed=%d detour_avg_ms=%.2f detour_max_ms=%llu detour_points_before=%lld detour_points_after=%lld\n",
		backendProfile.detour_queries,
		backendProfile.detour_success,
		backendProfile.detour_failed,
		detourAvgMs,
		( unsigned long long )backendProfile.detour_max_ms,
		( long long )backendProfile.detour_points_before,
		( long long )backendProfile.detour_points_after );
}

/**
*	@brief	Reset async queue profiling counters used by nav_profile_dump.
*	@note	Usage: sv nav_profile_reset
**/
static void ServerCommand_NavProfileReset_f( void ) {
	SVG_Nav_ResetAsyncQueueProfile();
	SVG_Nav_ResetBackendProfile();
	gi.cprintf( nullptr, PRINT_HIGH, "nav_profile_reset: counters cleared\n" );
}

/**
*	@brief	Parse a floating-point server-command argument.
**/
static bool Nav_ParseDoubleArg( const int32_t argIndex, double *outValue ) {
	if ( !outValue ) {
		return false;
	}

	const char *text = gi.argv( argIndex );
	if ( !text || !text[ 0 ] ) {
		return false;
	}

	char *endPtr = nullptr;
	const double value = strtod( text, &endPtr );
	if ( endPtr == text || ( endPtr && endPtr[ 0 ] != '\0' ) ) {
		return false;
	}

	*outValue = value;
	return true;
}

/**
*	@brief	Deterministic 32-bit LCG used by nav self-tests.
**/
static inline uint32_t NavSelfTest_NextU32( uint32_t *state ) {
	*state = ( *state * 1664525u ) + 1013904223u;
	return *state;
}

/**
*	@brief	Generate a deterministic random double in [minValue, maxValue].
**/
static inline double NavSelfTest_RandRange( uint32_t *state, const double minValue, const double maxValue ) {
	const double t = ( double )( NavSelfTest_NextU32( state ) & 0x00FFFFFFu ) / ( double )0x01000000u;
	return minValue + ( ( maxValue - minValue ) * t );
}

/**
*	@brief	Run fast deterministic nav-space and transform invariants.
*	@note	Usage: sv nav_selftest [iterations]
**/
static void ServerCommand_NavSelfTest_f( void ) {
	int32_t iterations = 256;
	if ( gi.argc() >= 3 ) {
		iterations = atoi( gi.argv( 2 ) );
	}
	iterations = std::max( 1, std::min( iterations, 10000 ) );

	int32_t checks = 0;
	int32_t failures = 0;
	int32_t skipped = 0;
	double maxError = 0.0;
	uint32_t rngState = 0xC0DEFACEu;

	auto recordError = [&]( const char *label, const double error, const double tolerance ) {
		checks++;
		maxError = std::max( maxError, error );
		if ( error <= tolerance ) {
			return;
		}

		failures++;
		if ( failures <= 8 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "[nav-selftest][FAIL] %s error=%.6f tolerance=%.6f\n", label, error, tolerance );
		}
	};

	for ( int32_t i = 0; i < iterations; i++ ) {
		const Vector3 origin = {
			( float )NavSelfTest_RandRange( &rngState, -2048.0, 2048.0 ),
			( float )NavSelfTest_RandRange( &rngState, -2048.0, 2048.0 ),
			( float )NavSelfTest_RandRange( &rngState, -512.0, 512.0 )
		};
		const Vector3 angles = {
			( float )NavSelfTest_RandRange( &rngState, -180.0, 180.0 ),
			( float )NavSelfTest_RandRange( &rngState, -180.0, 180.0 ),
			( float )NavSelfTest_RandRange( &rngState, -180.0, 180.0 )
		};
		const Vector3 localPoint = {
			( float )NavSelfTest_RandRange( &rngState, -256.0, 256.0 ),
			( float )NavSelfTest_RandRange( &rngState, -256.0, 256.0 ),
			( float )NavSelfTest_RandRange( &rngState, -256.0, 256.0 )
		};

		const nav_rigid_xform_t xform = SVG_Nav_BuildRigidXform( origin, angles );
		const Vector3 worldPoint = SVG_Nav_WorldFromLocal( xform, localPoint );
		const Vector3 localPointRoundTrip = SVG_Nav_LocalFromWorld( xform, worldPoint );
		const double pointError = QM_Vector3LengthDP( QM_Vector3Subtract( localPointRoundTrip, localPoint ) );
		recordError( "xform_point_roundtrip", pointError, 0.02 );

		const double forwardLenError = std::fabs( QM_Vector3LengthDP( xform.forward ) - 1.0 );
		const double sideLenError = std::fabs( QM_Vector3LengthDP( xform.side ) - 1.0 );
		const double upLenError = std::fabs( QM_Vector3LengthDP( xform.up ) - 1.0 );
		recordError( "xform_basis_forward_len", forwardLenError, 0.02 );
		recordError( "xform_basis_side_len", sideLenError, 0.02 );
		recordError( "xform_basis_up_len", upLenError, 0.02 );

		const double fsDotError = std::fabs( QM_Vector3DotProduct( xform.forward, xform.side ) );
		const double fuDotError = std::fabs( QM_Vector3DotProduct( xform.forward, xform.up ) );
		const double suDotError = std::fabs( QM_Vector3DotProduct( xform.side, xform.up ) );
		recordError( "xform_basis_dot_fs", fsDotError, 0.02 );
		recordError( "xform_basis_dot_fu", fuDotError, 0.02 );
		recordError( "xform_basis_dot_su", suDotError, 0.02 );

		const Vector3 localMins = {
			( float )NavSelfTest_RandRange( &rngState, -96.0, -16.0 ),
			( float )NavSelfTest_RandRange( &rngState, -96.0, -16.0 ),
			( float )NavSelfTest_RandRange( &rngState, -64.0, -8.0 )
		};
		const Vector3 localMaxs = {
			( float )NavSelfTest_RandRange( &rngState, 16.0, 96.0 ),
			( float )NavSelfTest_RandRange( &rngState, 16.0, 96.0 ),
			( float )NavSelfTest_RandRange( &rngState, 8.0, 64.0 )
		};
		double maxCornerError = 0.0;
		for ( int32_t cornerMask = 0; cornerMask < 8; cornerMask++ ) {
			const Vector3 localCorner = {
				( cornerMask & 1 ) ? localMaxs.x : localMins.x,
				( cornerMask & 2 ) ? localMaxs.y : localMins.y,
				( cornerMask & 4 ) ? localMaxs.z : localMins.z
			};
			const Vector3 worldCorner = SVG_Nav_WorldFromLocal( xform, localCorner );
			const Vector3 roundTripCorner = SVG_Nav_LocalFromWorld( xform, worldCorner );
			const double cornerError = QM_Vector3LengthDP( QM_Vector3Subtract( roundTripCorner, localCorner ) );
			maxCornerError = std::max( maxCornerError, cornerError );
		}
		recordError( "xform_aabb_corner_roundtrip", maxCornerError, 0.03 );

		Vector3 localDir = {
			( float )NavSelfTest_RandRange( &rngState, -1.0, 1.0 ),
			( float )NavSelfTest_RandRange( &rngState, -1.0, 1.0 ),
			( float )NavSelfTest_RandRange( &rngState, -1.0, 1.0 )
		};
		if ( QM_Vector3LengthDP( localDir ) < 0.0001 ) {
			localDir = { 1.0f, 0.0f, 0.0f };
		}
		localDir = QM_Vector3NormalizeDP( localDir );

		const Vector3 worldDir = SVG_Nav_WorldDirectionFromLocal( xform, localDir );
		const Vector3 localDirRoundTrip = SVG_Nav_LocalDirectionFromWorld( xform, worldDir );
		const double directionError = QM_Vector3LengthDP( QM_Vector3Subtract( localDirRoundTrip, localDir ) );
		recordError( "xform_direction_roundtrip", directionError, 0.02 );
	}

	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( !mesh ) {
		skipped += iterations * 2;
	} else {
		const nav_agent_profile_t resolved = SVG_Nav_ResolveAgentProfile( mesh, nullptr, nullptr );
		if ( SVG_Nav_IsAgentBoundsValid( resolved.mins, resolved.maxs ) ) {
			for ( int32_t i = 0; i < iterations; i++ ) {
				const Vector3 feetPos = {
					( float )NavSelfTest_RandRange( &rngState, -2048.0, 2048.0 ),
					( float )NavSelfTest_RandRange( &rngState, -2048.0, 2048.0 ),
					( float )NavSelfTest_RandRange( &rngState, -256.0, 512.0 )
				};

				const Vector3 centerPos = SVG_Nav_ConvertFeetToCenter( mesh, feetPos, &resolved.mins, &resolved.maxs );
				const Vector3 feetRoundTrip = SVG_Nav_ConvertCenterToFeet( mesh, centerPos, &resolved.mins, &resolved.maxs );
				const double feetError = QM_Vector3LengthDP( QM_Vector3Subtract( feetRoundTrip, feetPos ) );
				recordError( "feet_center_roundtrip", feetError, 0.02 );
			}
		} else {
			skipped += iterations;
		}

		const double tileWorldSize = mesh->cell_size_xy * mesh->tile_size;
		if ( tileWorldSize > 0.0 && mesh->cell_size_xy > 0.0 && mesh->tile_size > 0 ) {
			for ( int32_t i = 0; i < iterations; i++ ) {
				const double worldX = NavSelfTest_RandRange( &rngState, -4096.0, 4096.0 );
				const int32_t tileX = Nav_WorldToTileCoord( worldX, tileWorldSize );
				const double tileOriginX = tileX * tileWorldSize;
				const bool tileMembershipOk = ( worldX >= ( tileOriginX - NAV_TILE_EPSILON ) )
					&& ( worldX <= ( tileOriginX + tileWorldSize + NAV_TILE_EPSILON ) );
				recordError( "tile_membership", tileMembershipOk ? 0.0 : 1.0, 0.5 );

				const double worldInTile = NavSelfTest_RandRange( &rngState, tileOriginX, tileOriginX + tileWorldSize - 0.001 );
				const int32_t cellX = Nav_WorldToCellCoord( worldInTile, tileOriginX, mesh->cell_size_xy );
				const bool cellIndexOk = cellX >= 0 && cellX < mesh->tile_size;
				recordError( "cell_index_bounds", cellIndexOk ? 0.0 : 1.0, 0.5 );
			}
		} else {
			skipped += iterations;
		}
	}

	gi.cprintf( nullptr, PRINT_HIGH, "[nav-selftest] result=%s iterations=%d checks=%d failed=%d skipped=%d max_error=%.6f\n",
		failures == 0 ? "PASS" : "FAIL",
		iterations,
		checks,
		failures,
		skipped,
		maxError );
}

/**
*	@brief	Run a deterministic path query and emit machine-parseable diagnostics.
*	@note	Usage:
*			- sv nav_query_path <start_x> <start_y> <start_z> <goal_x> <goal_y> <goal_z>
*			- sv nav_query_path <query_id> <start_x> <start_y> <start_z> <goal_x> <goal_y> <goal_z>
**/
static void ServerCommand_NavQueryPath_f( void ) {
	const bool hasQueryId = gi.argc() >= 9;
	const int32_t coordBase = hasQueryId ? 3 : 2;
	const char *queryId = hasQueryId ? gi.argv( 2 ) : "q";

	if ( gi.argc() < ( coordBase + 6 ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_query_path [query_id] <start_x> <start_y> <start_z> <goal_x> <goal_y> <goal_z>\n" );
		return;
	}

	if ( !g_nav_mesh ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav-query] id=%s found=0 points=0 segments_ok=0 invalid_segment=-1 error=no_mesh\n", queryId );
		return;
	}

	double startX = 0.0;
	double startY = 0.0;
	double startZ = 0.0;
	double goalX = 0.0;
	double goalY = 0.0;
	double goalZ = 0.0;
	const bool parsed = Nav_ParseDoubleArg( coordBase + 0, &startX )
		&& Nav_ParseDoubleArg( coordBase + 1, &startY )
		&& Nav_ParseDoubleArg( coordBase + 2, &startZ )
		&& Nav_ParseDoubleArg( coordBase + 3, &goalX )
		&& Nav_ParseDoubleArg( coordBase + 4, &goalY )
		&& Nav_ParseDoubleArg( coordBase + 5, &goalZ );
	if ( !parsed ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav-query] id=%s found=0 points=0 segments_ok=0 invalid_segment=-1 error=parse\n", queryId );
		return;
	}

	const Vector3 startOrigin = { (float)startX, (float)startY, (float)startZ };
	const Vector3 goalOrigin = { (float)goalX, (float)goalY, (float)goalZ };

	svg_nav_path_policy_t policy = {};
	const nav_agent_profile_t resolved = SVG_Nav_ResolveAgentProfile( g_nav_mesh.get(), nullptr, nullptr );
	policy.agent_mins = resolved.mins;
	policy.agent_maxs = resolved.maxs;
	policy.max_step_height = resolved.max_step_height;
	policy.max_drop_height = resolved.max_drop_height;
	policy.drop_cap = resolved.drop_cap;
	policy.max_slope_deg = resolved.max_slope_deg;

	nav_traversal_path_t path = {};
	const bool found = SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox(
		startOrigin,
		goalOrigin,
		&path,
		policy.agent_mins,
		policy.agent_maxs,
		&policy,
		policy.enable_goal_z_layer_blend,
		policy.blend_start_dist,
		policy.blend_full_dist,
		nullptr );

	bool segmentsOk = false;
	int32_t invalidSegment = -1;
	if ( found ) {
		segmentsOk = true;
		const int32_t segmentCount = std::max( 0, path.num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			const bool traversable = Nav_CanTraverseStep_ExplicitBBox(
				g_nav_mesh.get(),
				path.points[ i ],
				path.points[ i + 1 ],
				policy.agent_mins,
				policy.agent_maxs,
				nullptr,
				&policy );
			if ( !traversable ) {
				segmentsOk = false;
				invalidSegment = i;
				break;
			}
		}
	}

	gi.cprintf( nullptr, PRINT_HIGH, "[nav-query] id=%s found=%d points=%d segments_ok=%d invalid_segment=%d\n",
		queryId,
		found ? 1 : 0,
		path.num_points,
		segmentsOk ? 1 : 0,
		invalidSegment );

	SVG_Nav_FreeTraversalPath( &path );
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
    else if ( Q_stricmp( cmd, "nav_bake" ) == 0 )
        ServerCommand_NavBake_f();
    else if ( Q_stricmp( cmd, "nav_profile_dump" ) == 0 )
        ServerCommand_NavProfileDump_f();
    else if ( Q_stricmp( cmd, "nav_profile_reset" ) == 0 )
        ServerCommand_NavProfileReset_f();
    else if ( Q_stricmp( cmd, "nav_selftest" ) == 0 )
        ServerCommand_NavSelfTest_f();
    else if ( Q_stricmp( cmd, "nav_query_path" ) == 0 )
        ServerCommand_NavQueryPath_f();
    else if ( Q_stricmp( cmd, "nav_cell" ) == 0 )
        ServerCommand_NavCell_f();
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

