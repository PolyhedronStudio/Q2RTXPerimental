/********************************************************************
*
*
*	SVGame: Navigation Debug Drawing
*
*	Debug visualization for navigation voxelmesh and pathfinding.
*	This file contains ONLY debug draw code (no globals, no runtime logic).
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/svg_player_edict.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_debug.h"



//! Global debug draw state.
nav_debug_draw_state_t s_nav_debug_draw_state = {};
//! Cached debug paths.
std::vector<nav_debug_cached_path_t> s_nav_debug_cached_paths = {};



/**
*
*   CVars for Navigation Debug Drawing:
*
**/
//! Master on/off. 0 = off, 1 = on.
cvar_t *nav_debug_draw = nullptr;
//! Only draw a specific BSP leaf index. -1 = any.
cvar_t *nav_debug_draw_leaf = nullptr;
//! Only draw a specific tile (grid coords). Requires "x y". "*" disables.
cvar_t *nav_debug_draw_tile = nullptr;
//! Budget in TE_DEBUG_TRAIL line segments per server frame.
cvar_t *nav_debug_draw_max_segments = nullptr;
//! Max distance from current view player to draw debug (world units). 0 disables.
cvar_t *nav_debug_draw_max_dist = nullptr;
//! Draw tile bboxes.
cvar_t *nav_debug_draw_tile_bounds = nullptr;
//! Draw sample ticks.
cvar_t *nav_debug_draw_samples = nullptr;
//! Draw final path segments from traversal path query.
cvar_t *nav_debug_draw_path = nullptr;
//! Temporary cvar to enable printing of failed nav lookup diagnostics (tile/cell indices).
cvar_t *nav_debug_show_failed_lookups = nullptr;
//! Optional toggle to draw/log edges rejected for clearance or drop cap violations.
cvar_t *nav_debug_draw_rejects = nullptr;

/**
*	@brief	Describes a rejected edge for optional debug visualization.
**/
struct nav_debug_reject_t {
	Vector3 start;
	Vector3 end;
	nav_debug_reject_reason_t reason;
};
//! Stored rejection events collected during the current server frame.
static std::vector<nav_debug_reject_t> s_nav_debug_rejects = {};
//! Maximum number of rejection events to retain before dropping the oldest entry.
static constexpr size_t NAV_DEBUG_MAX_REJECTS = 512;



/**
*
*
*
*	Navigation Debug Drawing - State and Helpers:
*
*
*
**/
/**
* 	@brief  Purge expired cached debug paths.
**/
void NavDebug_PurgeCachedPaths( void ) {
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

/**
*	@brief  Clear all cached debug paths immediately.
**/
void NavDebug_ClearCachedPaths( void ) {
	for ( nav_debug_cached_path_t &entry : s_nav_debug_cached_paths ) {
		SVG_Nav_FreeTraversalPath( &entry.path );
	}
	s_nav_debug_cached_paths.clear();
	NavDebug_ClearRejects();
}

/**
*	@brief	Record an edge rejection for later debug draw/logging.
**/
void NavDebug_RecordReject( const Vector3 &start, const Vector3 &end, nav_debug_reject_reason_t reason ) {
	if ( !nav_debug_draw_rejects || nav_debug_draw_rejects->integer == 0 ) {
		return;
	}
	if ( s_nav_debug_rejects.size() >= NAV_DEBUG_MAX_REJECTS ) {
		s_nav_debug_rejects.erase( s_nav_debug_rejects.begin() );
	}
	s_nav_debug_rejects.push_back( nav_debug_reject_t{ start, end, reason } );
}

/**
*	@brief	Clear all recorded rejection events.
**/
void NavDebug_ClearRejects( void ) {
	s_nav_debug_rejects.clear();
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
inline void NavDebug_NewFrame( void ) {
	if ( s_nav_debug_draw_state.frame == ( int32_t )level.frameNumber ) {
		return;
	}

	s_nav_debug_draw_state.frame = ( int32_t )level.frameNumber;
	s_nav_debug_draw_state.segments_used = 0;
}
/**
*	@brief	Determine if we can emit the requested number of segments.
**/
inline bool NavDebug_CanEmitSegments( const int32_t count ) {
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
inline void NavDebug_ConsumeSegments( const int32_t count ) {
	s_nav_debug_draw_state.segments_used += count;
}
/**
*	@brief	Check if navigation debug drawing is enabled.
**/
inline const bool NavDebug_Enabled( void ) {
	return nav_debug_draw && nav_debug_draw->integer != 0;
}
/**
*	@brief	Get the current view origin for distance filtering.
**/
inline Vector3 NavDebug_GetViewOrigin( void ) {
	if ( game.currentViewPlayer ) {
		return game.currentViewPlayer->currentOrigin + Vector3{ 0., 0., ( double )game.currentViewPlayer->viewheight };;
	}

	// Fallback: try client #1 (singleplayer typical).
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( 1 );
	if ( SVG_Entity_IsClient( ent, false ) ) {
		return ent->currentOrigin + Vector3{ 0., 0., ( double )ent->viewheight };
	}

	return {};
}
/**
*	@brief	Check if a position passes the distance filter.
* 	@param	pos	Position to check.
**/
inline const bool NavDebug_PassesDistanceFilter( const Vector3 &pos ) {
	const double maxDist = ( nav_debug_draw_max_dist ? nav_debug_draw_max_dist->value : 0.0f );
	if ( maxDist <= 0.0f ) {
		return true;
	}

	const Vector3 viewOrg = NavDebug_GetViewOrigin();
	const Vector3 d = QM_Vector3Subtract( pos, viewOrg );
	const double distSqr = ( d[ 0 ] * d[ 0 ] ) + ( d[ 1 ] * d[ 1 ] ) + ( d[ 2 ] * d[ 2 ] );
	return distSqr <= ( maxDist * maxDist );
}
/**
*	@return	Returns true if a tile filter is specified and outputs the wanted tile coordinates.
**/
const bool NavDebug_ParseTileFilter( int32_t *outTileX, int32_t *outTileY ) {
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
inline const bool NavDebug_FilterLeaf( const int32_t leafIndex ) {
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
inline const bool NavDebug_FilterTile( const int32_t tileX, const int32_t tileY ) {
	int32_t wantedX = 0, wantedY = 0;
	if ( !NavDebug_ParseTileFilter( &wantedX, &wantedY ) ) {
		return true;
	}

	return tileX == wantedX && tileY == wantedY;
}
/**
*	@brief	Draw a vertical tick at the given position.
**/
void NavDebug_DrawSampleTick( const Vector3 &pos, const double height ) {
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
void NavDebug_DrawTileBBox( const nav_mesh_t *mesh, const nav_tile_t *tile ) {
	if ( !NavDebug_Enabled() || !nav_debug_draw_tile_bounds || nav_debug_draw_tile_bounds->integer == 0 ) {
		return;
	}
	if ( !NavDebug_CanEmitSegments( 12 ) ) {
		return;
	}
	const double tileWorldSize = Nav_TileWorldSize( mesh );
	const Vector3 mins = {
		( double )tile->tile_x * tileWorldSize,
		( double )tile->tile_y * tileWorldSize,
		( double )-4096.0 // <Q2RTXP> mesh->world_bounds.mins.z
	};
	const Vector3 maxs = {
		( double )mins[ 0 ] + tileWorldSize,
		( double )mins[ 1 ] + tileWorldSize,
		( double )4096.0 // <Q2RTXP> mesh->world_bounds.maxs.z
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
	/**
	*	Draw/log any edges that were rejected this frame when instrumentation is enabled.
	**/
	if ( nav_debug_draw_rejects && nav_debug_draw_rejects->integer != 0 && !s_nav_debug_rejects.empty() ) {
		for ( const nav_debug_reject_t &reject : s_nav_debug_rejects ) {
			const char *reason = ( reject.reason == NAV_DEBUG_REJECT_REASON_CLEARANCE ) ? "clearance" : ( reject.reason == NAV_DEBUG_REJECT_REASON_SLOPE ? "slope" : "drop cap" );
			gi.dprintf( "[DEBUG][NavPath][Reject] %s edge from (%.1f %.1f %.1f) to (%.1f %.1f %.1f)\n",
				reason,
				reject.start.x, reject.start.y, reject.start.z,
				reject.end.x, reject.end.y, reject.end.z );

			if ( NavDebug_CanEmitSegments( 1 ) ) {
				SVG_DebugDrawLine_TE( reject.start, reject.end, MULTICAST_PVS, false );
				NavDebug_ConsumeSegments( 1 );
			}
		}
		s_nav_debug_rejects.clear();
	}

	const nav_mesh_t *mesh = g_nav_mesh.get();
	const double tileWorldSize = Nav_TileWorldSize( mesh );
	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;

	/**
	*	World mesh (per leaf):
	**/
	for ( int32_t leafIndex = 0; leafIndex < mesh->num_leafs; leafIndex++ ) {
		if ( !NavDebug_FilterLeaf( leafIndex ) ) {
			continue;
		}

		const nav_leaf_data_t *leaf = &mesh->leaf_data[ leafIndex ];
		if ( !leaf || leaf->num_tiles <= 0 || !leaf->tile_ids ) {
			continue;
		}

		for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
			const int32_t tileId = leaf->tile_ids[ t ];
			if ( tileId < 0 || tileId >= ( int32_t )mesh->world_tiles.size() ) {
				continue;
			}
			const nav_tile_t *tile = &mesh->world_tiles[ tileId ];
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
				const double tileOriginX = tile->tile_x * tileWorldSize;
				const double tileOriginY = tile->tile_y * tileWorldSize;

				for ( int32_t cellIndex = 0; cellIndex < cellsPerTile; cellIndex++ ) {
					const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
					if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
						continue;
					}

					const int32_t cellX = cellIndex % mesh->tile_size;
					const int32_t cellY = cellIndex / mesh->tile_size;

					const nav_layer_t *layer = &cell->layers[ 0 ];
					Vector3 p = {
						tileOriginX + ( ( double )cellX + 0.5 ) * ( double )mesh->cell_size_xy,
						tileOriginY + ( ( double )cellY + 0.5 ) * ( double )mesh->cell_size_xy,
						( double )layer->z_quantized * ( double )mesh->z_quant
					};

					NavDebug_DrawSampleTick( p, 12.0 );

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

	/**
	*	Map: owner_entnum -> runtime entry.
	*	We use the cached lookup map instead of a per-frame linear scan.
	**/
	auto find_runtime = [mesh]( const svg_base_edict_t *owner ) -> const nav_inline_model_runtime_t * {
		if ( !owner ) {
			return nullptr;
		}
		return SVG_Nav_GetInlineModelRuntimeForOwnerEntNum( mesh, owner->s.number );
	};

	for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
		const nav_inline_model_data_t *model = &mesh->inline_model_data[ i ];
		if ( !model || model->num_tiles <= 0 || !model->tiles ) {
			continue;
		}

		/**
		*	Resolve the inline-model owner entity using the runtime array slot.
		*	Note: Inline model data and runtime arrays are built from the same model set.
		**/
		const nav_inline_model_runtime_t *rt = nullptr;

		/**
		*	Fast path: inline model data and runtime arrays are generated from the same
		*	model set and share the same iteration order.
		**/
		if ( i >= 0 && i < mesh->num_inline_model_runtime ) {
			rt = &mesh->inline_model_runtime[ i ];
		}

		/**
		*	Safety net: verify the runtime entry resolves via the owner_entnum lookup.
		*	If it doesn't match, fall back to the lookup result.
		**/
		if ( rt && rt->owner_entnum > 0 ) {
			const nav_inline_model_runtime_t *rtMap = SVG_Nav_GetInlineModelRuntimeForOwnerEntNum( mesh, rt->owner_entnum );
			if ( rtMap ) {
				rt = rtMap;
			}
		}
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
					const Vector3 minsLocal = { tile->tile_x * tileWorldSize, tile->tile_y * tileWorldSize, -4096.0 /* <Q2RTXP>: TODO: world_bounds.mins.z */ };
					const Vector3 maxsLocal = { minsLocal[ 0 ] + tileWorldSize, minsLocal[ 1 ] + tileWorldSize, 4096.0 /* <Q2RTXP>: TODO: world_bounds.maxs.z */ };

					const Vector3 minsWorld = QM_Vector3Add( minsLocal, origin );
					const Vector3 maxsWorld = QM_Vector3Add( maxsLocal, origin );

					const Vector3 center = { ( double )( minsWorld[ 0 ] + maxsWorld[ 0 ] ) * 0.5, ( double )( minsWorld[ 1 ] + maxsWorld[ 1 ] ) * 0.5, 0.0 };
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
				const double tileOriginXLocal = tile->tile_x * tileWorldSize;
				const double tileOriginYLocal = tile->tile_y * tileWorldSize;

				for ( int32_t cellIndex = 0; cellIndex < cellsPerTile; cellIndex++ ) {
					const nav_xy_cell_t *cell = &tile->cells[ cellIndex ];
					if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
						continue;
					}

					const int32_t cellX = cellIndex % mesh->tile_size;
					const int32_t cellY = cellIndex / mesh->tile_size;

					const nav_layer_t *layer = &cell->layers[ 0 ];
					Vector3 pLocal = {
						tileOriginXLocal + ( ( double )cellX + 0.5 ) * ( double )mesh->cell_size_xy,
						tileOriginYLocal + ( ( double )cellY + 0.5 ) * ( double )mesh->cell_size_xy,
						( double )layer->z_quantized * ( double )mesh->z_quant
					};

					Vector3 pWorld = QM_Vector3Add( pLocal, origin );
					NavDebug_DrawSampleTick( pWorld, 12.0 );

					if ( !NavDebug_CanEmitSegments( 1 ) ) {
						return;
					}
				}
			}
		}
	}
}