/*
 * Full voxelmesh generation implementation (generation-only TU).
 *
 * This file contains voxelmesh generation helpers and the two primary
 * entry points `GenerateWorldMesh` and `GenerateInlineModelMesh`.
 *
 * Runtime-only globals (CVars, debug drawing, cluster-graph state) are
 * defined in `svg_nav.cpp`; reference them via `extern` where required.
 */

#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_generate.h"
#include "svgame/nav/svg_nav_path_process.h"

#include "svgame/entities/svg_player_edict.h"

// Common BSP access for navigation generation.
#include "common/bsp.h"
#include "system/system.h"

// Use QMTime for timing to integrate with engine time utilities.


// Declare external CVars (defined in svg_nav.cpp)
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

// Cluster CVars (also defined in svg_nav.cpp)
extern cvar_t *nav_cluster_route_weighted;
extern cvar_t *nav_cluster_cost_stair;
extern cvar_t *nav_cluster_cost_water;
extern cvar_t *nav_cluster_cost_lava;
extern cvar_t *nav_cluster_cost_slime;
extern cvar_t *nav_cluster_forbid_stair;
extern cvar_t *nav_cluster_forbid_water;
extern cvar_t *nav_cluster_forbid_lava;
extern cvar_t *nav_cluster_forbid_slime;

// Forward declarations for local helpers (internal to this TU).
static inline double Nav_TileWorldSize( const nav_mesh_t *mesh );
static inline int32_t Nav_WorldToTileCoord( double value, double tile_world_size );
static void Nav_FreeTileCells( nav_tile_t *tile, int32_t cells_per_tile );
static void FindWalkableLayers( const Vector3 &xy_pos, const Vector3 &mins, const Vector3 &maxs,
                                double z_min, double z_max, double max_step, double max_slope_deg, double z_quant,
                                nav_layer_t **out_layers, int32_t *out_num_layers,
                                const edict_ptr_t *clip_entity, const nav_rigid_xform_t *clip_xform,
                                bool sample_in_local_space );
static bool Nav_BuildTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &leaf_mins, const Vector3 &leaf_maxs,
                           double z_min, double z_max );
static bool Nav_BuildInlineTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &model_mins, const Vector3 &model_maxs,
                                 double z_min, double z_max, const edict_ptr_t *clip_entity );
void Nav_CollectInlineModelEntities( std::unordered_map<int32_t, svg_base_edict_t *> &out_model_to_ent );
static void Nav_BuildInlineModelRuntime( nav_mesh_t *mesh, const std::unordered_map<int32_t, svg_base_edict_t *> &model_to_ent );

/**
 *	@brief	Helper to conditionally sample finer-grained timings based on `nav_profile_level` cvar.
 *	@param	level_requested The profiling level required to enable this sample (2 = per-leaf, 3 = per-tile).
 *	@return	True if sampling at this granularity is enabled.
 */
static inline bool SVG_Nav_ProfileEnabled( int level_requested ) {
    return nav_profile_level && nav_profile_level->integer >= level_requested;
}

/**
*	@brief	Free tile cell allocations (per-cell layer buffers + cell array).
*	@param	tile		Tile whose `cells`/`layers` were allocated with `gi.TagMallocz`.
*	@param	cells_per_tile	Number of XY cells in the tile (`tile_size * tile_size`).
*	@note	This does not free `tile->presence_bits`.
*/
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
*	@brief	Set presence bit for a cell within a tile.
*	@param	tile		Tile with an allocated `presence_bits` array.
*	@param	cell_index	Cell index within the tile (0..cells_per_tile-1).
*/
static inline void Nav_SetPresenceBit_Load( nav_tile_t *tile, int32_t cell_index ) {
    const int32_t word_index = cell_index >> 5;
    const int32_t bit_index = cell_index & 31;
    tile->presence_bits[ word_index ] |= ( 1u << bit_index );
}

/**
*	@brief	Detect nav-layer content flags from a collision trace.
*	@param	trace	Collision trace result produced by CM trace/clip.
*	@return	Bitmask of `nav_layer_flags_t` values.
*/
static uint32_t DetectContentFlags( const cm_trace_t &trace ) {
    uint32_t flags = NAV_FLAG_WALKABLE;
    if ( trace.contents & CONTENTS_WATER ) {
        flags |= NAV_FLAG_WATER;
    }
    if ( trace.contents & CONTENTS_LAVA ) {
        flags |= NAV_FLAG_LAVA;
    }
    if ( trace.contents & CONTENTS_SLIME ) {
        flags |= NAV_FLAG_SLIME;
    }
    if ( trace.contents & CONTENTS_LADDER ) {
        flags |= NAV_FLAG_LADDER;
    }
    return flags;
}

// Diagnostic counters (kept local to generation TU).
static int32_t s_precheck_fail_count = 0;
static int32_t s_trace_attempt_count = 0;
static int32_t s_trace_hit_count = 0;
static int32_t s_slope_reject_count = 0;

/**
*	@brief	Perform downward traces to find walkable layers at an XY position.
*	@param	xy_pos		XY position in the sampling space (world for world mesh, model-local for inline mesh).
*	@param	mins		Agent bbox mins used for trace hull.
*	@param	maxs		Agent bbox maxs used for trace hull.
*	@param	z_min		Lower bound of the vertical search range (units).
*	@param	z_max		Upper bound of the vertical search range (units).
*	@param	max_step	Maximum step height used to choose trace stepping interval.
*	@param	max_slope_deg	Maximum walkable slope (degrees).
*	@param	z_quant		Quantization step used when storing layer Z samples.
*	@param	out_layers	[out] Heap-allocated layer buffer (TAG_SVGAME_LEVEL) or nullptr.
*	@param	out_num_layers	[out] Number of layers returned in `out_layers`.
*	@param	clip_entity	Optional inline-model clip entity for model-local sampling (nullptr for world sampling).
*	@param	clip_xform	Rigid transform for inline-model local/world conversion.
*	@param	sample_in_local_space	If true and `clip_entity` is set, traces are generated from local XY/Z and transformed to world.
*	@note	Not thread-safe due to internal static scratch storage.
*/
static void FindWalkableLayers( const Vector3 &xy_pos, const Vector3 &mins, const Vector3 &maxs,
                                double z_min, double z_max, double max_step, double max_slope_deg, double z_quant,
                                nav_layer_t **out_layers, int32_t *out_num_layers,
                                const edict_ptr_t *clip_entity, const nav_rigid_xform_t *clip_xform,
                                bool sample_in_local_space ) {
    const int32_t MAX_LAYERS = 16;
    static nav_layer_t temp_layers[ MAX_LAYERS ] = {};
    int32_t num_layers = 0;

    /**
    *    Fast precheck:
    *        Use a conservative contents query to skip expensive traces when the entire
    *        vertical column is empty of solid geometry.
    **/
    {
        // For inline-model sampling we avoid the conservative box-contents precheck
        // because headnode/contents queries and entity clip prechecks can be
        // unreliable across different engine exports. In that case always perform
        // downward traces instead of early-outing here.
        if ( clip_entity ) {
            // Skip the fast precheck when tracing against an entity's inline model.
        } else {
        Vector3 colMins = xy_pos;
        Vector3 colMaxs = xy_pos;
        colMins[ 2 ] = z_min;
        colMaxs[ 2 ] = z_max;

        colMins[ 0 ] += mins[ 0 ];
        colMins[ 1 ] += mins[ 1 ];
        colMaxs[ 0 ] += maxs[ 0 ];
        colMaxs[ 1 ] += maxs[ 1 ];

        bool hasSolid = true;
        if ( clip_entity ) {
            const char *modelStr = clip_entity->model;
            int32_t inlineIndex = -1;
            if ( modelStr && modelStr[ 0 ] == '*' ) {
                inlineIndex = atoi( modelStr + 1 );
            }

            mnode_t *headnode = nullptr;
            if ( inlineIndex >= 0 && gi.CM_InlineModelHeadnode ) {
                headnode = gi.CM_InlineModelHeadnode( inlineIndex );
            }

            if ( headnode ) {
                // Fast-path: query the inline-model headnode using model-local coordinates.
                // `colMins`/`colMaxs` are in world-space when called from inline sampling.
                // Subtract the entity origin to get model-local bounds for CM_BoxContents_headnode.
                Vector3 headnodeMins = colMins;
                Vector3 headnodeMaxs = colMaxs;
                if ( clip_entity ) {
                    headnodeMins[ 0 ] -= clip_entity->currentOrigin[ 0 ];
                    headnodeMins[ 1 ] -= clip_entity->currentOrigin[ 1 ];
                    headnodeMins[ 2 ] -= clip_entity->currentOrigin[ 2 ];
                    headnodeMaxs[ 0 ] -= clip_entity->currentOrigin[ 0 ];
                    headnodeMaxs[ 1 ] -= clip_entity->currentOrigin[ 1 ];
                    headnodeMaxs[ 2 ] -= clip_entity->currentOrigin[ 2 ];
                }

                cm_contents_t contents = CONTENTS_NONE;
                gi.CM_BoxContents_headnode( &headnodeMins[ 0 ], &headnodeMaxs[ 0 ], &contents, nullptr, 0, headnode, nullptr );
                hasSolid = ( contents & CM_CONTENTMASK_SOLID ) != 0;

                // If the headnode fast-path reported no solids, fall back to an
                // entity-aware clip trace (world-space) to ensure we don't miss
                // geometry that might be missed by the simple headnode check.
                if ( !hasSolid ) {
                    Vector3 start = xy_pos; start[ 2 ] = z_max;
                    Vector3 end = xy_pos; end[ 2 ] = z_min;
                    const cm_trace_t tr = gi.clip( clip_entity, &start, &mins, &maxs, &end, CM_CONTENTMASK_SOLID );
                    hasSolid = ( tr.fraction < 1.0f ) || tr.startsolid || tr.allsolid;

                    static bool s_reported_inline_precheck = false;
                    if ( !s_reported_inline_precheck ) {
                        s_reported_inline_precheck = true;
                        SVG_Nav_Log( "[NavDebug] Inline precheck: model_index=%d headnode=%p contents=0x%X trace.fraction=%.3f startsolid=%d allsolid=%d\n",
                            inlineIndex, ( void * )headnode, ( unsigned )contents, tr.fraction, tr.startsolid ? 1 : 0, tr.allsolid ? 1 : 0 );
                    }
                }
            } else {
                Vector3 start = xy_pos; start[ 2 ] = z_max;
                Vector3 end = xy_pos; end[ 2 ] = z_min;
                // No headnode: use entity clip trace in world-space.
                const cm_trace_t tr = gi.clip( clip_entity, &start, &mins, &maxs, &end, CM_CONTENTMASK_SOLID );
                hasSolid = ( tr.fraction < 1.0f ) || tr.startsolid || tr.allsolid;
                static bool s_reported_inline_precheck = false;
                if ( !s_reported_inline_precheck ) {
                    s_reported_inline_precheck = true;
                    SVG_Nav_Log( "[NavDebug] Inline precheck: headnode=null trace.fraction=%.3f startsolid=%d allsolid=%d\n",
                        tr.fraction, tr.startsolid ? 1 : 0, tr.allsolid ? 1 : 0 );
                }
            }
        } else {
            cm_contents_t contents = CONTENTS_NONE;
            gi.CM_BoxContents( &colMins[ 0 ], &colMaxs[ 0 ], &contents, nullptr, 0, nullptr );
            hasSolid = ( contents & CM_CONTENTMASK_SOLID ) != 0;
        }

            if ( !hasSolid ) {
                s_precheck_fail_count++;
                // Early out: no solids in the column implies no walkable layers.
                *out_layers = nullptr;
                *out_num_layers = 0;
                return;
            }
        }
    }

    double current_z = z_max;
    double step_down = max_step * 2.0;

    /**
    *    Downward sampling loop:
    *        Repeatedly trace downward to discover multiple floors at the same XY.
    **/
    while ( current_z > z_min && num_layers < MAX_LAYERS ) {
        Vector3 sample_start = xy_pos;
        sample_start[ 2 ] = current_z;
        Vector3 sample_end = xy_pos;
        sample_end[ 2 ] = current_z - step_down;

        const bool use_local_inline = ( clip_entity && clip_xform && sample_in_local_space );
        Vector3 trace_start = sample_start;
        Vector3 trace_end = sample_end;
        if ( use_local_inline ) {
            trace_start = SVG_Nav_WorldFromLocal( *clip_xform, sample_start );
            trace_end = SVG_Nav_WorldFromLocal( *clip_xform, sample_end );
        }

        cm_trace_t trace;
        if ( clip_entity ) {
            trace = gi.clip( clip_entity, &trace_start, &mins, &maxs, &trace_end, CM_CONTENTMASK_SOLID );
        } else {
            trace = gi.trace( &trace_start, &mins, &maxs, &trace_end, nullptr, CM_CONTENTMASK_SOLID );
        }

        s_trace_attempt_count++;

        double trace_end_sample_z = trace.endpos[ 2 ];
        Vector3 slope_normal = trace.plane.normal;
        if ( use_local_inline ) {
            const Vector3 local_end = SVG_Nav_LocalFromWorld( *clip_xform, trace.endpos );
            trace_end_sample_z = local_end[ 2 ];
            slope_normal = SVG_Nav_LocalDirectionFromWorld( *clip_xform, trace.plane.normal );
        }

        // Record valid upward-facing hit surfaces.
			if ( trace.fraction < 1.0f && slope_normal[ 2 ] > 0.0f ) {
				s_trace_hit_count++;
				// Enforce slope threshold in terms of normal.z.
				if ( ( slope_normal[ 2 ] >= cosf( max_slope_deg * DEG_TO_RAD ) ) ) {
				/**
				*    Quantize the sampled Z using rounding so nearby layers don't collapse
				*    downwards when the trace endpos is close to the next quant step.
				*    If we are sampling an inline model, transform the world-space hit
				*    back into model-local space before storing.
				**/
				const double localZ = trace_end_sample_z;

				const double quantizedZ = std::lround( localZ / z_quant );
				constexpr double quantMin = ( double )std::numeric_limits<int32_t>::min();
				constexpr double quantMax = ( double )std::numeric_limits<int32_t>::max();
				const double clampedZ = std::min( std::max( quantizedZ, quantMin ), quantMax );
				temp_layers[num_layers].z_quantized = ( int32_t )clampedZ;
                    temp_layers[num_layers].flags = DetectContentFlags( trace );

                    /**
                    *    Trace upward from the detected floor to measure the next obstruction
                    *    and derive vertical clearance in quantized units.
                    **/
                    const double clearance_probe_start_z = trace_end_sample_z + 1.0;
                    double ceiling_z = z_max;
                    if ( clearance_probe_start_z < z_max ) {
                        Vector3 ceiling_start = xy_pos;
                        ceiling_start[2] = clearance_probe_start_z;
                        Vector3 ceiling_end = xy_pos;
                        ceiling_end[2] = z_max;

						Vector3 ceiling_trace_start = ceiling_start;
						Vector3 ceiling_trace_end = ceiling_end;
						if ( use_local_inline ) {
							ceiling_trace_start = SVG_Nav_WorldFromLocal( *clip_xform, ceiling_start );
							ceiling_trace_end = SVG_Nav_WorldFromLocal( *clip_xform, ceiling_end );
						}

                        cm_trace_t ceiling_trace;
                        if ( clip_entity ) {
                            ceiling_trace = gi.clip( clip_entity, &ceiling_trace_start, &mins, &maxs, &ceiling_trace_end, CM_CONTENTMASK_SOLID );
                        } else {
                            ceiling_trace = gi.trace( &ceiling_trace_start, &mins, &maxs, &ceiling_trace_end, nullptr, CM_CONTENTMASK_SOLID );
                        }

                        if ( ceiling_trace.fraction < 1.0f ) {
							if ( use_local_inline ) {
								const Vector3 local_ceiling = SVG_Nav_LocalFromWorld( *clip_xform, ceiling_trace.endpos );
								ceiling_z = local_ceiling[ 2 ];
							} else {
								ceiling_z = ceiling_trace.endpos[ 2 ];
							}
                        }
                    }

                    double clearance_world = ceiling_z - trace_end_sample_z;
                    if ( clearance_world < 0.0 ) {
                        clearance_world = 0.0;
                    }

		int64_t clearance_quants = (int64_t)std::floor( clearance_world / z_quant );
		if ( clearance_quants < 0 ) {
			clearance_quants = 0;
		}

		/**
		*    Clamp the computed clearance to the new 32-bit storage range.
		**/
		const uint32_t clearance_store = ( clearance_quants > ( int64_t )UINT32_MAX )
			? UINT32_MAX
			: ( uint32_t )clearance_quants;
		temp_layers[num_layers].clearance = clearance_store;

                    num_layers++;
                    current_z = trace_end_sample_z - 1.0f;
                } else {
                    s_slope_reject_count++;
                    if ( SVG_Nav_ProfileEnabled( 3 ) ) {
                        SVG_Nav_Log( "[NavProfile] FindWalkableLayers: trace hit but slope rejected normal.z=%.3f max_slope_deg=%.1f at z=%.2f\n",
                            slope_normal[ 2 ], max_slope_deg, trace_end_sample_z );
                    }
                    current_z = trace_end_sample_z - 1.0f;
                }
            } else {
                if ( SVG_Nav_ProfileEnabled( 3 ) ) {
                    SVG_Nav_Log( "[NavProfile] FindWalkableLayers: trace miss fraction=%.3f startsolid=%d allsolid=%d\n",
                        trace.fraction, trace.startsolid ? 1 : 0, trace.allsolid ? 1 : 0 );
                }
                current_z -= step_down;
            }
    }

    /**
    *    Commit results:
    *        Allocate a persistent layer buffer for the caller.
    **/
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
*	@brief	Build a navigation tile with sampled walkable layers against the world.
*	@param	mesh	Navigation mesh being generated.
*	@param	tile	Tile to populate (allocates `presence_bits` and `cells`).
*	@param	leaf_mins	World-space mins for the owning BSP leaf (for XY culling).
*	@param	leaf_maxs	World-space maxs for the owning BSP leaf (for XY culling).
*	@param	z_min	Minimum Z for sampling.
*	@param	z_max	Maximum Z for sampling.
*	@return	True if any walkable layers were sampled for this tile.
*/
static bool Nav_BuildTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &leaf_mins, const Vector3 &leaf_maxs,
                           double z_min, double z_max ) {
    const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
    const int32_t presence_words = ( cells_per_tile + 31 ) / 32;
    const double tile_world_size = Nav_TileWorldSize( mesh );

    tile->presence_bits = (uint32_t *)gi.TagMallocz( sizeof( uint32_t ) * presence_words, TAG_SVGAME_LEVEL );
    tile->cells = (nav_xy_cell_t *)gi.TagMallocz( sizeof( nav_xy_cell_t ) * cells_per_tile, TAG_SVGAME_LEVEL );

    bool has_layers = false;

    const double tile_origin_x = tile->tile_x * tile_world_size;
    const double tile_origin_y = tile->tile_y * tile_world_size;

    for ( int32_t cell_y = 0; cell_y < mesh->tile_size; cell_y++ ) {
        uint64_t tileCellStartMs = 0;
        if ( SVG_Nav_ProfileEnabled( 3 ) ) {
            tileCellStartMs = gi.GetRealSystemTime();
        }
        for ( int32_t cell_x = 0; cell_x < mesh->tile_size; cell_x++ ) {
            const double world_x = tile_origin_x + ( (double)cell_x + 0.5f ) * mesh->cell_size_xy;
            const double world_y = tile_origin_y + ( (double)cell_y + 0.5f ) * mesh->cell_size_xy;

            if ( world_x < leaf_mins[ 0 ] || world_x > leaf_maxs[ 0 ] ||
                 world_y < leaf_mins[ 1 ] || world_y > leaf_maxs[ 1 ] ) {
                continue;
            }

            Vector3 xy_pos = { (float)world_x, (float)world_y, 0.0f };
            nav_layer_t *layers = nullptr;
            int32_t num_layers = 0;

            FindWalkableLayers( xy_pos, mesh->agent_mins, mesh->agent_maxs,
                                z_min, z_max, mesh->max_step, mesh->max_slope_deg, mesh->z_quant,
                                &layers, &num_layers, nullptr, nullptr, false );

            if ( num_layers > 0 ) {
                const int32_t cell_index = cell_y * mesh->tile_size + cell_x;
                tile->cells[ cell_index ].num_layers = num_layers;
                tile->cells[ cell_index ].layers = layers;
                Nav_SetPresenceBit_Load( tile, cell_index );
                has_layers = true;
            }
        }

        if ( SVG_Nav_ProfileEnabled( 3 ) ) {
            const uint64_t tileCellEndMs = gi.GetRealSystemTime();
            const double tileMs = ( double )( tileCellEndMs - tileCellStartMs );
            // Note: this reports the per-row cell loop time; per-tile timing is available via caller.
            SVG_Nav_Log( "[NavProfile] Tile (%d,%d) row %d cell loop time: %.2f ms\n", tile->tile_x, tile->tile_y, cell_y, tileMs );
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
*	@brief	Build a navigation tile for an inline model using entity clip collision.
*	@param	mesh	Navigation mesh being generated.
*	@param	tile	Tile to populate (allocates `presence_bits` and `cells`).
*	@param	model_mins	Inline model local-space mins.
*	@param	model_maxs	Inline model local-space maxs.
*	@param	z_min	Minimum Z for sampling (model-local).
*	@param	z_max	Maximum Z for sampling (model-local).
*	@param	clip_entity	Inline-model entity used for collision clipping.
*	@return	True if any walkable layers were sampled for this tile.
*/
static bool Nav_BuildInlineTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &model_mins, const Vector3 &model_maxs,
                                 double z_min, double z_max, const edict_ptr_t *clip_entity ) {
    const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
    const int32_t presence_words = ( cells_per_tile + 31 ) / 32;
    const double tile_world_size = Nav_TileWorldSize( mesh );

    tile->presence_bits = (uint32_t *)gi.TagMallocz( sizeof( uint32_t ) * presence_words, TAG_SVGAME_LEVEL );
    tile->cells = (nav_xy_cell_t *)gi.TagMallocz( sizeof( nav_xy_cell_t ) * cells_per_tile, TAG_SVGAME_LEVEL );

    bool has_layers = false;

    const double tile_origin_x = model_mins[ 0 ] + ( tile->tile_x * tile_world_size );
    const double tile_origin_y = model_mins[ 1 ] + ( tile->tile_y * tile_world_size );
	const nav_rigid_xform_t clip_xform = clip_entity
		? SVG_Nav_BuildRigidXform( clip_entity->currentOrigin, clip_entity->currentAngles )
		: nav_rigid_xform_t{};

    for ( int32_t cell_y = 0; cell_y < mesh->tile_size; cell_y++ ) {
        for ( int32_t cell_x = 0; cell_x < mesh->tile_size; cell_x++ ) {
            const double local_x = tile_origin_x + ( (double)cell_x + 0.5 ) * mesh->cell_size_xy;
            const double local_y = tile_origin_y + ( (double)cell_y + 0.5 ) * mesh->cell_size_xy;

            if ( local_x < model_mins[ 0 ] || local_x > model_maxs[ 0 ] ||
                 local_y < model_mins[ 1 ] || local_y > model_maxs[ 1 ] ) {
                continue;
            }

            Vector3 xy_pos = { (float)local_x, (float)local_y, 0.0f };
            nav_layer_t *layers = nullptr;
            int32_t num_layers = 0;

            FindWalkableLayers( xy_pos, mesh->agent_mins, mesh->agent_maxs,
                                z_min, z_max, mesh->max_step, mesh->max_slope_deg, mesh->z_quant,
                                &layers, &num_layers, clip_entity, clip_entity ? &clip_xform : nullptr, true );

            if ( num_layers > 0 ) {
                const int32_t cell_index = cell_y * mesh->tile_size + cell_x;
                tile->cells[ cell_index ].num_layers = num_layers;
                tile->cells[ cell_index ].layers = layers;
                Nav_SetPresenceBit_Load( tile, cell_index );
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
*	@brief	Collect all active entities referencing an inline BSP model ("*N").
*	@param	out_model_to_ent	[out] Mapping from inline-model index (N) to entity pointer.
*	@note	If multiple entities reference the same "*N", the first encountered is kept.
*/
void Nav_CollectInlineModelEntities( std::unordered_map<int32_t, svg_base_edict_t *> &out_model_to_ent ) {
    out_model_to_ent.clear();

    for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++ ) {
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
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

        if ( out_model_to_ent.find( modelIndex ) == out_model_to_ent.end() ) {
            out_model_to_ent.emplace( modelIndex, ent );
        }
    }
}

/**
*	@brief	Build runtime inline model transform data for navigation.
*	@param	mesh	Navigation mesh to populate runtime inline-model data for.
*	@param	model_to_ent	Mapping of inline model indices to owning entities.
*	@note	This function (re)allocates `mesh->inline_model_runtime` and resets `num_inline_model_runtime`.
*/
static void Nav_BuildInlineModelRuntime( nav_mesh_t *mesh, const std::unordered_map<int32_t, svg_base_edict_t *> &model_to_ent ) {
    if ( !mesh ) {
        return;
    }

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
*	@brief	Generate navigation mesh for world (world-only collision).
*	@param	mesh	Navigation mesh structure to populate.
*	@note	Uses a tile-first approach with `CM_BoxContents` for efficient empty-tile skipping.
*/
void GenerateWorldMesh( nav_mesh_t *mesh ) {
    if ( !mesh ) {
        return;
    }
    mesh->profile_world_gen_ms = 0.0;

    const cm_t *world_model = gi.GetCollisionModel();
    if ( !world_model || !world_model->cache ) {
        SVG_Nav_Log( "Failed to get world BSP data\n" );
        return;
    }
    bsp_t *bsp = world_model->cache;
    int32_t num_leafs = bsp->numleafs;

    /**
    *    Log start of world mesh generation and stamp start time for profiling.
    **/
    SVG_Nav_Log( "Generating world navmesh (tile-first approach)...\n" );
    const uint64_t worldStartMs = gi.GetRealSystemTime();

    mesh->num_leafs = num_leafs;
    mesh->leaf_data = (nav_leaf_data_t *)gi.TagMallocz( sizeof( nav_leaf_data_t ) * num_leafs, TAG_SVGAME_LEVEL );

    for ( int32_t i = 0; i < num_leafs; i++ ) {
        uint64_t leafStartMs = 0;
        if ( SVG_Nav_ProfileEnabled( 2 ) ) {
            leafStartMs = gi.GetRealSystemTime();
        }
        mesh->leaf_data[ i ].leaf_index = i;
        mesh->leaf_data[ i ].num_tiles = 0;
        mesh->leaf_data[ i ].tile_ids = nullptr;
    }

    mesh->world_tiles.clear();
    mesh->world_tile_id_of.clear();
    mesh->world_tiles.reserve( 1024 );
    mesh->world_tile_id_of.reserve( 2048 );

    const double tile_world_size = Nav_TileWorldSize( mesh );

    const Vector3 world_mins = bsp->models[ 0 ].mins;
    const Vector3 world_maxs = bsp->models[ 0 ].maxs;
    const double world_z_min = (double)world_mins[ 2 ] + (double)mesh->agent_mins[ 2 ];
    const double world_z_max = (double)world_maxs[ 2 ] + (double)mesh->agent_maxs[ 2 ];

    const int32_t global_tile_min_x = Nav_WorldToTileCoord( world_mins[ 0 ], tile_world_size );
    const int32_t global_tile_max_x = Nav_WorldToTileCoord( world_maxs[ 0 ] - NAV_TILE_EPSILON, tile_world_size );
    const int32_t global_tile_min_y = Nav_WorldToTileCoord( world_mins[ 1 ], tile_world_size );
    const int32_t global_tile_max_y = Nav_WorldToTileCoord( world_maxs[ 1 ] - NAV_TILE_EPSILON, tile_world_size );

    SVG_Nav_Log( "  World bounds: (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f)\n",
        world_mins[ 0 ], world_mins[ 1 ], world_mins[ 2 ], world_maxs[ 0 ], world_maxs[ 1 ], world_maxs[ 2 ] );
    SVG_Nav_Log( "  Tile range: (%d, %d) to (%d, %d)\n",
        global_tile_min_x, global_tile_min_y, global_tile_max_x, global_tile_max_y );

    int32_t total_tile_attempts = 0;
    int32_t total_tile_success = 0;
    int32_t total_tile_skipped_empty = 0;

    for ( int32_t tile_y = global_tile_min_y; tile_y <= global_tile_max_y; tile_y++ ) {
        for ( int32_t tile_x = global_tile_min_x; tile_x <= global_tile_max_x; tile_x++ ) {
            total_tile_attempts++;

            const double tile_origin_x = (double)tile_x * tile_world_size;
            const double tile_origin_y = (double)tile_y * tile_world_size;
            Vector3 tileMins = {
                ( float )( tile_origin_x + mesh->agent_mins[ 0 ] ),
                ( float )( tile_origin_y + mesh->agent_mins[ 1 ] ),
                ( float )world_z_min
            };
            Vector3 tileMaxs = {
                ( float )( tile_origin_x + tile_world_size + ( mesh->agent_maxs[ 0 ] - mesh->agent_mins[ 0 ] ) ),
                ( float )( tile_origin_y + tile_world_size + ( mesh->agent_maxs[ 1 ] - mesh->agent_mins[ 1 ] ) ),
                ( float )world_z_max
            };

            cm_contents_t contents = CONTENTS_NONE;
            gi.CM_BoxContents( &tileMins[ 0 ], &tileMaxs[ 0 ], &contents, nullptr, 0, nullptr );

            if ( ( contents & CM_CONTENTMASK_SOLID ) == 0 ) {
                total_tile_skipped_empty++;
                continue;
            }

            nav_tile_t tile = {};
            tile.tile_x = tile_x;
            tile.tile_y = tile_y;

            if ( Nav_BuildTile( mesh, &tile, world_mins, world_maxs, world_z_min, world_z_max ) ) {
                const nav_world_tile_key_t key = { .tile_x = tile_x, .tile_y = tile_y };
                const int32_t tile_id = (int32_t)mesh->world_tiles.size();
                mesh->world_tiles.push_back( tile );
                mesh->world_tile_id_of.emplace( key, tile_id );
                total_tile_success++;
            }
        }
    }

    // Build leaf->tile mapping
    std::vector<int32_t> temp_tile_ids;
    temp_tile_ids.reserve( 64 );

    for ( int32_t i = 0; i < num_leafs; i++ ) {
        mleaf_t *leaf = &bsp->leafs[ i ];
        if ( leaf->contents & CONTENTS_SOLID ) {
            continue;
        }

        uint64_t leafStartMs = 0;
        if ( SVG_Nav_ProfileEnabled( 2 ) ) {
            leafStartMs = gi.GetRealSystemTime();
        }

        temp_tile_ids.clear();

        const Vector3 leaf_mins = leaf->mins;
        const Vector3 leaf_maxs = leaf->maxs;

        const int32_t leaf_tile_min_x = Nav_WorldToTileCoord( leaf_mins[ 0 ], tile_world_size );
        const int32_t leaf_tile_max_x = Nav_WorldToTileCoord( leaf_maxs[ 0 ] - NAV_TILE_EPSILON, tile_world_size );
        const int32_t leaf_tile_min_y = Nav_WorldToTileCoord( leaf_mins[ 1 ], tile_world_size );
        const int32_t leaf_tile_max_y = Nav_WorldToTileCoord( leaf_maxs[ 1 ] - NAV_TILE_EPSILON, tile_world_size );

        for ( int32_t ty = leaf_tile_min_y; ty <= leaf_tile_max_y; ty++ ) {
            for ( int32_t tx = leaf_tile_min_x; tx <= leaf_tile_max_x; tx++ ) {
                const nav_world_tile_key_t key = { .tile_x = tx, .tile_y = ty };
                auto it = mesh->world_tile_id_of.find( key );
                if ( it != mesh->world_tile_id_of.end() ) {
                    temp_tile_ids.push_back( it->second );
                }
            }
        }

        if ( SVG_Nav_ProfileEnabled( 2 ) ) {
            const uint64_t leafEndMs = gi.GetRealSystemTime();
            const double leafMs = ( double )( leafEndMs - leafStartMs );
            SVG_Nav_Log( "[NavProfile] Leaf %d sampling time: %.2f ms\n", i, leafMs );
        }

        if ( !temp_tile_ids.empty() ) {
            nav_leaf_data_t *leaf_data = &mesh->leaf_data[ i ];
            leaf_data->num_tiles = (int32_t)temp_tile_ids.size();
            leaf_data->tile_ids = (int32_t *)gi.TagMallocz( sizeof( int32_t ) * leaf_data->num_tiles, TAG_SVGAME_LEVEL );
            memcpy( leaf_data->tile_ids, temp_tile_ids.data(), sizeof( int32_t ) * leaf_data->num_tiles );
        }
    }

    /**
    *    Phase complete: record elapsed time and log summary statistics.
    **/
    const double elapsedMs = ( double )( gi.GetRealSystemTime() - worldStartMs );
    mesh->profile_world_gen_ms = elapsedMs;
    SVG_Nav_Log( "World mesh generation complete (tile-first) - %.2f ms\n", elapsedMs );
    SVG_Nav_Log( "  Tiles attempted: %d\n", total_tile_attempts );
    SVG_Nav_Log( "  Tiles skipped (empty): %d (%.1f%%)\n",
        total_tile_skipped_empty,
        total_tile_attempts > 0 ? ( 100.0f * total_tile_skipped_empty / total_tile_attempts ) : 0.0f );
    SVG_Nav_Log( "  Tiles generated: %d (%.1f%% of attempted)\n",
        total_tile_success,
        total_tile_attempts > 0 ? ( 100.0f * total_tile_success / total_tile_attempts ) : 0.0f );

    if ( total_tile_success == 0 && total_tile_attempts > 0 ) {
        SVG_Nav_Log( "[ERROR][NavGen] NO TILES GENERATED! Possible causes:\n" );
        SVG_Nav_Log( "[ERROR][NavGen]   1. All surfaces too steep (check nav_max_slope_deg=%.1f)\n", mesh->max_slope_deg );
    }
}

/**
*	@brief	Generate navigation tiles for inline BSP models (brush entities).
*	@param	mesh	Navigation mesh structure to populate.
*/
void GenerateInlineModelMesh( nav_mesh_t *mesh ) {
    if ( !mesh ) {
        return;
    }
    mesh->profile_inline_gen_ms = 0.0;

    const cm_t *world_model = gi.GetCollisionModel();
    if ( !world_model || !world_model->cache ) {
        SVG_Nav_Log( "Failed to get world BSP data\n" );
        return;
    }

    std::unordered_map<int32_t, svg_base_edict_t *> model_to_ent;
    Nav_CollectInlineModelEntities( model_to_ent );

    if ( model_to_ent.empty() ) {
        mesh->num_inline_models = 0;
        mesh->inline_model_data = nullptr;
        mesh->num_inline_model_runtime = 0;
        mesh->inline_model_runtime = nullptr;
        return;
    }

    // Log start of inline model generation and measure phase time.
    SVG_Nav_Log( "Generating inline model navmesh for %d inline models...\n", ( int32_t )model_to_ent.size() );
    const uint64_t inlineStartMs = gi.GetRealSystemTime();

    mesh->num_inline_models = ( int32_t )model_to_ent.size();
    mesh->inline_model_data = ( nav_inline_model_data_t * )gi.TagMallocz(
        sizeof( nav_inline_model_data_t ) * mesh->num_inline_models, TAG_SVGAME_LEVEL );

    Nav_BuildInlineModelRuntime( mesh, model_to_ent );

    const double tile_world_size = Nav_TileWorldSize( mesh );

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
        // Sanity: inline model data must exist for the referenced model name.
        if ( !mm ) {
            // Diagnostic: Log model-name -> inline model data lookup failure to help debug missing inline meshes.
            SVG_Nav_Log( "[NavDebug] GenerateInlineModelMesh: GetInlineModelDataForName returned NULL for model='%s' ent=%d model_index=%d\n",
                model_name ? model_name : "(null)", ent ? ent->s.number : -1, model_index );
            out_index++;
            continue;
        }

        const Vector3 model_mins = mm->mins;
        const Vector3 model_maxs = mm->maxs;

        const double z_min = model_mins[ 2 ] + mesh->agent_mins[ 2 ];
        const double z_max = model_maxs[ 2 ] + mesh->agent_maxs[ 2 ];

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

                // Compute tile origins in model-local space and always attempt sampling.
                // The headnode-based CM_BoxContents check proved unreliable across some
                // engine exports, so we sample directly using the entity clip path which
                // correctly accounts for inline-model transforms.
                tile.tile_x = tile_x;
                tile.tile_y = tile_y;
                if ( Nav_BuildInlineTile( mesh, &tile, model_mins, model_maxs, z_min, z_max, ent ) ) {
                    tiles.push_back( tile );
                } else {
                    // Tile had no walkable layers; this is expected for non-walkable areas.
                    SVG_Nav_Log( "[NavDebug] GenerateInlineModelMesh: inline tile had no walkable layers model_index=%d tile=(%d,%d)\n",
                        model_index, tile_x, tile_y );
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

    const double elapsedMs = ( double )( gi.GetRealSystemTime() - inlineStartMs );
    mesh->profile_inline_gen_ms = elapsedMs;
    SVG_Nav_Log( "Inline models' navmesh generation complete - %.2f ms\n", elapsedMs );

    // Diagnostic: print a histogram of layer counts across inline model tiles (if any) to detect abnormal distributions.
    if ( mesh && mesh->num_inline_models > 0 ) {
        std::unordered_map<int32_t, int32_t> layerCountHistogram;
        int32_t totalCells = 0;
        int32_t totalLayers = 0;
        for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
            nav_inline_model_data_t *model = &mesh->inline_model_data[ i ];
            for ( int32_t t = 0; t < model->num_tiles; t++ ) {
                nav_tile_t *tile = &model->tiles[ t ];
                const int32_t cells_per_tile_local = mesh->tile_size * mesh->tile_size;
                for ( int32_t c = 0; c < cells_per_tile_local; c++ ) {
                    const int32_t num_layers = tile->cells[ c ].num_layers;
                    if ( num_layers > 0 ) {
                        layerCountHistogram[ num_layers ]++;
                        totalCells++;
                        totalLayers += num_layers;
                    }
                }
            }
        }

        SVG_Nav_Log( "[NavDebug] Inline models layer histogram: total_populated_cells=%d total_layers=%d\n", totalCells, totalLayers );
        for ( const auto &kv : layerCountHistogram ) {
            SVG_Nav_Log( "[NavDebug]   cells_with_%d_layers = %d\n", kv.first, kv.second );
        }
    }
}

/**
 *  @brief  Diagnostic helper: print per-inline-model generation summary when present.
 **/
static void Nav_LogInlineModelSummary( nav_mesh_t *mesh ) {
    if ( !mesh ) {
        return;
    }

    if ( mesh->num_inline_models <= 0 ) {
        SVG_Nav_Log( "Inline model summary: none present\n" );
        return;
    }

    int32_t total_inline_tiles = 0;
    for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
        nav_inline_model_data_t *model = &mesh->inline_model_data[ i ];
        total_inline_tiles += model->num_tiles;
        SVG_Nav_Log( "  Inline model %d: model_index=%d num_tiles=%d\n", i, model->model_index, model->num_tiles );
    }
    SVG_Nav_Log( "Inline model summary: %d models, %d total inline tiles\n", mesh->num_inline_models, total_inline_tiles );
}

/**
*	@brief	Main wrapper called by server command to generate voxelmesh.
*	@note	Allocates `g_nav_mesh` if needed, reads generation parameters from CVars, then builds
*			world + inline-model tiles and finally builds the cluster graph.
*/
void SVG_Nav_GenerateVoxelMesh( void ) {
	if ( !g_nav_mesh ) {
		/**
		*	Allocate and publish the mesh instance using RAII helper.
		*	Initialize occupancy_frame in the callback.
		**/
		if ( !g_nav_mesh.create( TAG_SVGAME_LEVEL, []( nav_mesh_t *mesh ) {
			mesh->occupancy_frame = -1;
		} ) ) {
			SVG_Nav_Log( "SVG_Nav_GenerateVoxelMesh: failed to allocate nav mesh instance.\n" );
			return;
		}

		SVG_Nav_Log( "SVG_Nav_GenerateVoxelMesh: no nav mesh instance available, allocated new one.\n" );
	//    return;
	}

	// Read generation parameters from CVars into mesh.
	g_nav_mesh->cell_size_xy = nav_cell_size_xy->value;
	g_nav_mesh->z_quant = nav_z_quant->value;
	g_nav_mesh->tile_size = ( int32_t )nav_tile_size->value;
	g_nav_mesh->max_step = nav_max_step->value;
	g_nav_mesh->max_slope_deg = nav_max_slope_deg->value;
	g_nav_mesh->agent_mins = Vector3( nav_agent_mins_x->value, nav_agent_mins_y->value, nav_agent_mins_z->value );
	g_nav_mesh->agent_maxs = Vector3( nav_agent_maxs_x->value, nav_agent_maxs_y->value, nav_agent_maxs_z->value );

	// Basic validation.
	if ( g_nav_mesh->agent_mins[ 0 ] >= g_nav_mesh->agent_maxs[ 0 ] ||
		 g_nav_mesh->agent_mins[ 1 ] >= g_nav_mesh->agent_maxs[ 1 ] ||
		 g_nav_mesh->agent_mins[ 2 ] >= g_nav_mesh->agent_maxs[ 2 ] ) {
		SVG_Nav_Log( "Error: Invalid agent bounding box (mins must be < maxs on all axes)\n" );
		SVG_Nav_FreeMesh();
		return;
	}

	if ( g_nav_mesh->z_quant <= 0.0f ) {
		SVG_Nav_Log( "Error: nav_z_quant must be > 0\n" );
		SVG_Nav_FreeMesh();
		return;
	}

	SVG_Nav_Log( "Parameters:\n" );
	SVG_Nav_Log( "  cell_size_xy: %.1f\n", g_nav_mesh->cell_size_xy );
	SVG_Nav_Log( "  z_quant: %.1f\n", g_nav_mesh->z_quant );
	SVG_Nav_Log( "  tile_size: %d\n", g_nav_mesh->tile_size );

	// Generate world and inline meshes with timing.
	const QMTime genStart = QMTime::FromMilliseconds( gi.GetRealSystemTime() );
	g_nav_mesh->profile_world_gen_ms = 0.0;
	g_nav_mesh->profile_inline_gen_ms = 0.0;
	g_nav_mesh->profile_total_gen_ms = 0.0;
	g_nav_mesh->profile_generated_at_ms = 0;
	GenerateWorldMesh( g_nav_mesh.get() );
    GenerateInlineModelMesh( g_nav_mesh.get() );
    // Emit a short inline-model generation summary to help debug missing inline tiles.
    Nav_LogInlineModelSummary( g_nav_mesh.get() );
	const QMTime genEnd = QMTime::FromMilliseconds( gi.GetRealSystemTime() );
	const double genElapsedMs = ( double )( genEnd - genStart ).Milliseconds();
	g_nav_mesh->profile_total_gen_ms = genElapsedMs;
	g_nav_mesh->profile_generated_at_ms = gi.GetRealSystemTime();
	SVG_Nav_Log( "Overall generation time: %.2f ms\n", genElapsedMs );

	// Calculate stats
	g_nav_mesh->total_tiles = 0;
	g_nav_mesh->total_xy_cells = 0;
	g_nav_mesh->total_layers = 0;

	g_nav_mesh->total_tiles += (int32_t)g_nav_mesh->world_tiles.size();
	const int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
	for ( nav_tile_t &tile : g_nav_mesh->world_tiles ) {
		for ( int32_t c = 0; c < cells_per_tile; c++ ) {
			if ( tile.cells && tile.cells[ c ].num_layers > 0 ) {
				g_nav_mesh->total_xy_cells++;
				g_nav_mesh->total_layers += tile.cells[ c ].num_layers;
			}
		}
	}

	for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
		nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[ i ];
		g_nav_mesh->total_tiles += model->num_tiles;
		for ( int32_t t = 0; t < model->num_tiles; t++ ) {
			nav_tile_t *tile = &model->tiles[ t ];
			int32_t cells_per_tile_local = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
			for ( int32_t c = 0; c < cells_per_tile_local; c++ ) {
				if ( tile->cells[ c ].num_layers > 0 ) {
					g_nav_mesh->total_xy_cells++;
					g_nav_mesh->total_layers += tile->cells[ c ].num_layers;
				}
			}
		}
	}

	// Build cluster graph from generated world tiles (use public wrapper in svg_nav.cpp)
	SVG_Nav_ClusterGraph_BuildFromMesh_World( g_nav_mesh.get() );

	SVG_Nav_Log( "\n=== Generation Statistics ===\n" );
	SVG_Nav_Log( "  Total tiles: %d\n", g_nav_mesh->total_tiles );
	SVG_Nav_Log( "  Total XY cells: %d\n", g_nav_mesh->total_xy_cells );
	SVG_Nav_Log( "  Total layers: %d\n", g_nav_mesh->total_layers );
	SVG_Nav_Log( "===================================\n" );
}
