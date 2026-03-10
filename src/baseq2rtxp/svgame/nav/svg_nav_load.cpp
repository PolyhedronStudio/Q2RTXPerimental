/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
* 	SVGame: Navigation Voxelmesh Load - Implementation
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_hierarchy.h"
#include "svgame/nav/svg_nav_load.h"

#if USE_ZLIB
	#include <zlib.h>
#else
	#include <stdio.h>
	#define gzopen(name, mode)          fopen(name, mode)
	#define gzclose(file)               fclose(file)
	#define gzwrite(file, buf, len)     fwrite(buf, 1, len, file)
	#define gzread(file, buf, len)      fread(buf, 1, len, file)
	#define gzbuffer(file, size)        (void)0
	#define gzFile                      FILE * 
#endif

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

//! File magic used to identify serialized navmesh cache files.
static constexpr uint32_t NAV_MESH_SAVE_MAGIC = 0x56414E53; // "VANS"
//! Serialized navmesh format version. Bump when persisted tile/layer payload layout changes.
static constexpr uint32_t NAV_MESH_SAVE_VERSION = 4;

/** 
* 	@brief	Read raw bytes from a (possibly gzipped) file.
* 	@return	True if all bytes were read.
**/
static bool Nav_Read( gzFile f, void * data, const size_t size ) {
	if ( !f || !data || size == 0 ) {
		return false;
	}

	return gzread( f, data, (unsigned)size ) == (int)size;
}

template<typename T>
static bool Nav_ReadValue( gzFile f, T &out ) {
	/** 
	* 	Read a typed POD value.
	**/
	return Nav_Read( f, &out, sizeof( T ) );
}

/** 
* 	@brief	Allocate an empty tile storage layout for load-time population.
**/
static void Nav_InitEmptyTileStorage( const nav_mesh_t * mesh, nav_tile_t * tile ) {
	/** 
	* 	Allocate sparse tile storage arrays for presence bits and per-cell layer lists.
	**/
	const int32_t cellsPerTile = mesh->tile_size* mesh->tile_size;
	const int32_t presenceWords = ( cellsPerTile + 31 ) / 32;

	// Allocate presence bitset.
	tile->presence_bits = (uint32_t * )gi.TagMallocz( sizeof( uint32_t )* presenceWords, TAG_SVGAME_NAVMESH );
	// Allocate per-cell array.
	tile->cells = (nav_xy_cell_t * )gi.TagMallocz( sizeof( nav_xy_cell_t )* cellsPerTile, TAG_SVGAME_NAVMESH );
}

// <Q2RTXP>: WID: Safer method in svg_nav.cpp
#if 0
static inline void Nav_SetPresenceBit_Load( nav_tile_t * tile, const int32_t cellIndex ) {
	/** 
	* 	Mark a cell as present in the sparse tile storage.
	**/
	const int32_t wordIndex = cellIndex >> 5;
	const int32_t bitIndex = cellIndex & 31;
	tile->presence_bits[ wordIndex ] |= ( 1u << bitIndex );
}
#endif

/** 
* 	@brief	Accumulate populated-cell and layer statistics for one loaded tile.
* 	@param	mesh		Navigation mesh that owns the tile storage.
* 	@param	tile		Tile to inspect.
* 	@param	io_cells	[in,out] Running populated-cell counter.
* 	@param	io_layers	[in,out] Running layer counter.
* 	@return	True when the tile could be inspected, false on invalid storage.
**/
static bool Nav_AccumulateTileStatistics( const nav_mesh_t * mesh, const nav_tile_t * tile, uint64_t * io_cells, uint64_t * io_layers ) {
	/** 
	* 	Sanity: require valid inputs so statistics stay trustworthy.
	**/
	if ( !mesh || !tile || !io_cells || !io_layers ) {
		return false;
	}

	/** 
	* 	Inspect the tile through the shared safe accessor.
	**/
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || cellsCount < 0 ) {
		return false;
	}

	/** 
	* 	Count only populated cells because that matches the save-side statistics.
	**/
	for ( int32_t cellIndex = 0; cellIndex < cellsCount; cellIndex++ ) {
		const nav_xy_cell_t &cell = cellsPtr[ cellIndex ];
		if ( cell.num_layers <= 0 || !cell.layers ) {
			continue;
		}

		(* io_cells)++;
		* io_layers += ( uint64_t )cell.num_layers;
	}

	return true;
}

/** 
* 	@brief	Rebuild aggregate mesh statistics after loading serialized tile data.
* 	@param	mesh	Navigation mesh to update.
* 	@return	True when the statistics were rebuilt successfully, false otherwise.
* 	@note	This keeps load-time validation independent from generation-only counters.
**/
static bool Nav_RebuildLoadedStatistics( nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a valid mesh before counting loaded storage.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	* 	Accumulate world-tile statistics from the canonical tile store.
	**/
	uint64_t totalTiles = ( uint64_t )mesh->world_tiles.size();
	uint64_t totalXYCells = 0;
	uint64_t totalLayers = 0;
	for ( const nav_tile_t &tile : mesh->world_tiles ) {
		if ( !Nav_AccumulateTileStatistics( mesh, &tile, &totalXYCells, &totalLayers ) ) {
			return false;
		}
	}

	/** 
	* 	Accumulate inline-model tiles because generation statistics include them too.
	**/
	for ( int32_t modelIndex = 0; modelIndex < mesh->num_inline_models; modelIndex++ ) {
		const nav_inline_model_data_t &model = mesh->inline_model_data[ modelIndex ];
		if ( model.num_tiles < 0 || ( model.num_tiles > 0 && !model.tiles ) ) {
			return false;
		}

		for ( int32_t tileIndex = 0; tileIndex < model.num_tiles; tileIndex++ ) {
			totalTiles++;
			if ( !Nav_AccumulateTileStatistics( mesh, &model.tiles[ tileIndex ], &totalXYCells, &totalLayers ) ) {
				return false;
			}
		}
	}

	/** 
	* 	Clamp the rebuilt values into the mesh's persisted counter width.
	**/
	const uint64_t maxCounter = ( uint64_t )std::numeric_limits<int32_t>::max();
	mesh->total_tiles = ( int32_t )std::min( totalTiles, maxCounter );
	mesh->total_xy_cells = ( int32_t )std::min( totalXYCells, maxCounter );
	mesh->total_layers = ( int32_t )std::min( totalLayers, maxCounter );
	return true;
}

/** 
* 	@brief	Load a navigation voxelmesh from file into `g_nav_mesh`.
* 	@return	True on success, false on failure.
**/
bool SVG_Nav_LoadVoxelMesh( const char * levelName ) {
	/** 
	* 	Sanity: require filename.
	**/
	if ( !levelName || !levelName[ 0 ] ) {
		return false;
	}

	/** 
	* 	Build full file path for the nav cache.
	**/
	// Actual filename of the .nav file.
	const std::string navMeshFilePath = Nav_GetPathForLevelNav( levelName, true );

	/** 
	* 	Open nav cache file (gzip if available).
	**/
	gzFile f = gzopen( navMeshFilePath.c_str(), "rb");
	if ( !f ) {
		gi.dprintf( "%s: failed to open '%s'\n", __func__, navMeshFilePath.c_str() );
		return false;
	}

	/** 
	* 	Configure a larger read buffer for fewer syscalls.
	**/
	gzbuffer( f, 65536 );

	/** 
	* 	Read and validate file header.
	**/
	uint32_t magic = 0;
	uint32_t version = 0;
	if ( !Nav_ReadValue( f, magic ) || !Nav_ReadValue( f, version ) ) {
		gzclose( f );
		return false;
	}
  // Reject files that do not match the expected magic or current format version.
	if ( magic != NAV_MESH_SAVE_MAGIC || version != NAV_MESH_SAVE_VERSION ) {
		gzclose( f );
		return false;
	}

	/** 
	* 	Replace any existing mesh before allocating a new one.
	**/
	SVG_Nav_FreeMesh();

	/** 
	* 	Allocate and publish the mesh instance using RAII helper.
	* 	Initialize occupancy_frame in the callback.
	**/
	if ( !g_nav_mesh.create( TAG_SVGAME_NAVMESH, []( nav_mesh_t * mesh ) {
		mesh->occupancy_frame = -1;
	} ) ) {
		gzclose( f );
		return false;
	}

	/** 
	* 	Read the expected statistics from the navmesh generation for validation and informational purposes.
	* 	These are not strictly required for loading the mesh but can help detect mismatches between the navmesh and game configuration.
	**/
	// Tiles(All): total count of tiles generated across the entire world mesh.
	uint64_t statistic_total_tiles = 0;
	// Cells X/Y.total count of XY cells with data across all tiles (sum of populated cells in each tile).
	uint64_t statistic_total_xy_cells = 0;
	// Layers.total count of Z layers across all XY cells (sum of num_layers in each cell). This reflects the overall vertical complexity of the navmesh.
	uint64_t statistic_total_layers = 0;
   // Read in the statistics from the file for later validation against the loaded mesh.
    if ( !Nav_ReadValue( f, statistic_total_tiles ) ||
		!Nav_ReadValue( f, statistic_total_xy_cells ) ||
		!Nav_ReadValue( f, statistic_total_layers ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	/** 
	*  Read generation parameters required to interpret stored tile data.
	**/
	if (
		!Nav_ReadValue( f, g_nav_mesh->world_bounds.mins.x ) ||
		!Nav_ReadValue( f, g_nav_mesh->world_bounds.mins.y ) ||
		!Nav_ReadValue( f, g_nav_mesh->world_bounds.mins.z ) ||
		!Nav_ReadValue( f, g_nav_mesh->world_bounds.maxs.x ) ||
		!Nav_ReadValue( f, g_nav_mesh->world_bounds.maxs.y ) ||
		!Nav_ReadValue( f, g_nav_mesh->world_bounds.maxs.z ) ||

		!Nav_ReadValue( f, g_nav_mesh->cell_size_xy ) ||
		!Nav_ReadValue( f, g_nav_mesh->z_quant ) ||
		!Nav_ReadValue( f, g_nav_mesh->tile_size ) ||
		!Nav_ReadValue( f, g_nav_mesh->max_step ) ||
		!Nav_ReadValue( f, g_nav_mesh->max_slope_normal_z ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	/** 
	*  Read agent bounds after slope decoding.
	**/
	if ( !Nav_ReadValue( f, g_nav_mesh->agent_mins ) ||
		!Nav_ReadValue( f, g_nav_mesh->agent_maxs ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	/** 
	* 	Read mesh counts (leafs + inline models).
	**/
	if ( !Nav_ReadValue( f, g_nav_mesh->num_leafs ) || !Nav_ReadValue( f, g_nav_mesh->num_inline_models ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	/** 
	* 	Sanity: validate counts before allocating large arrays.
	**/
	if ( g_nav_mesh->num_leafs < 0 || g_nav_mesh->num_leafs > 1'000'000 ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	// Allocate leaf array.
	g_nav_mesh->leaf_data = (nav_leaf_data_t * )gi.TagMallocz( sizeof( nav_leaf_data_t )* (size_t)g_nav_mesh->num_leafs, TAG_SVGAME_NAVMESH );

	/** 
	* 	Initialize canonical world tile storage used during load.
	**/
	g_nav_mesh->world_tiles.clear();
	g_nav_mesh->world_tile_id_of.clear();
	g_nav_mesh->world_tiles.reserve( 1024 );
	g_nav_mesh->world_tile_id_of.reserve( 2048 );

	const int32_t cellsPerTile = g_nav_mesh->tile_size* g_nav_mesh->tile_size;

	/** 
	* 	Read world mesh tiles (per BSP leaf).
	**/
	for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
		nav_leaf_data_t &leaf = g_nav_mesh->leaf_data[ i ];
		// Read leaf header.
		if ( !Nav_ReadValue( f, leaf.leaf_index ) || !Nav_ReadValue( f, leaf.num_tiles ) ) {
			gzclose( f );
			SVG_Nav_FreeMesh();
			return false;
		}
		leaf.num_regions = 0;
		leaf.region_ids = nullptr;

		// Sanity check: tile count must be non-negative.
		if ( leaf.num_tiles < 0 ) {
			gzclose( f );
			SVG_Nav_FreeMesh();
			return false;
		}

		/** 
		* 	Allocate leaf tile-id array.
		* 	
		* 	Tiles themselves are stored canonically in `mesh->world_tiles`.
		**/
		leaf.tile_ids = (int32_t * )gi.TagMallocz( sizeof( int32_t )* (size_t)leaf.num_tiles, TAG_SVGAME_NAVMESH );

		for ( int32_t t = 0; t < leaf.num_tiles; t++ ) {
			nav_world_tile_key_t key = {};
			// Read tile coordinates.
			if ( !Nav_ReadValue( f, key.tile_x ) || !Nav_ReadValue( f, key.tile_y ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			/** 
			* 	Resolve or create canonical world tile entry.
			**/
			auto it = g_nav_mesh->world_tile_id_of.find( key );
			int32_t tileId = -1;
			if ( it != g_nav_mesh->world_tile_id_of.end() ) {
				tileId = it->second;
			} else {
				tileId = (int32_t)g_nav_mesh->world_tiles.size();
				nav_tile_t newTile = {};
				newTile.tile_x = key.tile_x;
				newTile.tile_y = key.tile_y;
             newTile.region_id = NAV_REGION_ID_NONE;
				Nav_InitEmptyTileStorage( g_nav_mesh.get(), &newTile );
				g_nav_mesh->world_tiles.push_back( newTile );
				g_nav_mesh->world_tile_id_of.emplace( key, tileId );
			}

			leaf.tile_ids[ t ] = tileId;
			nav_tile_t &tile = g_nav_mesh->world_tiles[ tileId ];
			if ( !Nav_ReadValue( f, tile.traversal_summary_bits ) || !Nav_ReadValue( f, tile.edge_summary_bits ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			// Tile storage is allocated when inserted into `world_tiles`.

			int32_t populated = 0;
			// Read the number of populated cells for this tile.
			if ( !Nav_ReadValue( f, populated ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}
			// Sanity: populated must be within bounds.
			if ( populated < 0 || populated > cellsPerTile ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			for ( int32_t p = 0; p < populated; p++ ) {
				int32_t cellIndex = 0;
				int32_t numLayers = 0;
				// Read cell index + layer count.
				if ( !Nav_ReadValue( f, cellIndex ) || !Nav_ReadValue( f, numLayers ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				// Sanity: validate cell and layer bounds.
				if ( cellIndex < 0 || cellIndex >= cellsPerTile || numLayers <= 0 || numLayers > 64 ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

                // Access the cell via the tile cells array. The storage was allocated
				// by Nav_InitEmptyTileStorage above and is guaranteed valid here.
				// Use the safe accessor view to obtain a pointer/count and validate
				// the bounds before writing into the cell to avoid UB on corrupt data.
				auto cellsView = SVG_Nav_Tile_GetCells( g_nav_mesh.get(), &tile );
				nav_xy_cell_t * cellsPtr = cellsView.first;
				const int32_t cellsCount = cellsView.second;
				if ( !cellsPtr || cellIndex < 0 || cellIndex >= cellsCount ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}
				nav_xy_cell_t &cell = cellsPtr[ cellIndex ];
				// Store layer count and allocate layer buffer.
				cell.num_layers = numLayers;
				cell.layers = (nav_layer_t * )gi.TagMallocz( sizeof( nav_layer_t )* (size_t)numLayers, TAG_SVGAME_NAVMESH );
				// Read layer data directly into allocated buffer.
				if ( !Nav_Read( f, cell.layers, sizeof( nav_layer_t )* (size_t)numLayers ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

              // Mark the cell as populated in the tile's sparse bitset.
				Nav_SetPresenceBit( &tile, cellIndex );
			}
		}
	}

	/** 
	* 	Read inline model meshes (model-local space).
	**/
	if ( g_nav_mesh->num_inline_models > 0 ) {
		// Allocate per-inline-model container array.
		g_nav_mesh->inline_model_data = (nav_inline_model_data_t * )gi.TagMallocz( sizeof( nav_inline_model_data_t )* (size_t)g_nav_mesh->num_inline_models, TAG_SVGAME_NAVMESH );

		for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
			nav_inline_model_data_t &model = g_nav_mesh->inline_model_data[ i ];
			// Read model header.
			if ( !Nav_ReadValue( f, model.model_index ) || !Nav_ReadValue( f, model.num_tiles ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			// Sanity: tile count must be non-negative.
			if ( model.num_tiles < 0 ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			// Allocate tile array for this model.
			model.tiles = (nav_tile_t * )gi.TagMallocz( sizeof( nav_tile_t )* (size_t)model.num_tiles, TAG_SVGAME_NAVMESH );

			for ( int32_t t = 0; t < model.num_tiles; t++ ) {
				nav_tile_t &tile = model.tiles[ t ];
               tile.region_id = NAV_REGION_ID_NONE;
				// Read tile coordinates.
				if ( !Nav_ReadValue( f, tile.tile_x ) || !Nav_ReadValue( f, tile.tile_y ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}
				if ( !Nav_ReadValue( f, tile.traversal_summary_bits ) || !Nav_ReadValue( f, tile.edge_summary_bits ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				// Allocate tile storage for per-cell layer population.
				Nav_InitEmptyTileStorage( g_nav_mesh.get(), &tile );

				int32_t populated = 0;
				// Read the number of populated cells for this tile.
				if ( !Nav_ReadValue( f, populated ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}
				// Sanity: populated must be within bounds.
				if ( populated < 0 || populated > cellsPerTile ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				for ( int32_t p = 0; p < populated; p++ ) {
					int32_t cellIndex = 0;
					int32_t numLayers = 0;
					// Read cell index + layer count.
					if ( !Nav_ReadValue( f, cellIndex ) || !Nav_ReadValue( f, numLayers ) ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}

					// Sanity: validate cell and layer bounds.
					if ( cellIndex < 0 || cellIndex >= cellsPerTile || numLayers <= 0 || numLayers > 64 ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}

                    auto cellsView = SVG_Nav_Tile_GetCells( g_nav_mesh.get(), &tile );
					nav_xy_cell_t * cellsPtr = cellsView.first;
					const int32_t cellsCount = cellsView.second;
					if ( !cellsPtr || cellIndex < 0 || cellIndex >= cellsCount ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}
					nav_xy_cell_t &cell = cellsPtr[ cellIndex ];
					// Store layer count and allocate layer buffer.
					cell.num_layers = numLayers;
					cell.layers = (nav_layer_t * )gi.TagMallocz( sizeof( nav_layer_t )* (size_t)numLayers, TAG_SVGAME_NAVMESH );
					// Read layer data into allocated buffer.
					if ( !Nav_Read( f, cell.layers, sizeof( nav_layer_t )* (size_t)numLayers ) ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}

                  // Mark the cell as populated.
					Nav_SetPresenceBit( &tile, cellIndex );
				}
			}
		}
	}

	/** 
	* 	Runtime inline-model mapping is not saved; rebuild it from live entities.
	**/
	g_nav_mesh->num_inline_model_runtime = 0;
	g_nav_mesh->inline_model_runtime = nullptr;

	// Rebuild runtime mapping now that the mesh is loaded so inline-model traversal has live transforms.
	// Refresh alone is not enough because the runtime array is not persisted and may be null.
	if ( g_nav_mesh->num_inline_models > 0 ) {
		/** 
		* 	Build model_index -> entity mapping for all active entities referencing "* N".
		**/
		std::unordered_map<int32_t, svg_base_edict_t * > model_to_ent;
		model_to_ent.reserve( (size_t)g_nav_mesh->num_inline_models );
		for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++ ) {
			// Inspect each edict for inline model usage.
			svg_base_edict_t * ent = g_edict_pool.EdictForNumber( i );
			if ( !ent || !SVG_Entity_IsActive( ent ) ) {
				continue;
			}

			const char * modelStr = ent->model;
			if ( !modelStr || modelStr[ 0 ] != '* ' ) {
				continue;
			}

			const int32_t modelIndex = atoi( modelStr + 1 );
			if ( modelIndex <= 0 ) {
				continue;
			}

			if ( model_to_ent.find( modelIndex ) == model_to_ent.end() ) {
				model_to_ent.emplace( modelIndex, ent );
			}
		}

		// Build runtime mapping for all referenced inline models.
		// This allows traversal/pathfinding to use correct live transforms.
		// Allocate runtime array sized to the number of referenced models.
		g_nav_mesh->num_inline_model_runtime = (int32_t)model_to_ent.size();
		if ( g_nav_mesh->num_inline_model_runtime > 0 ) {
			// Allocate runtime transform array.
			g_nav_mesh->inline_model_runtime = (nav_inline_model_runtime_t * )gi.TagMallocz(
				sizeof( nav_inline_model_runtime_t )* (size_t)g_nav_mesh->num_inline_model_runtime, TAG_SVGAME_NAVMESH );
			int32_t out_index = 0;
			for ( const auto &it : model_to_ent ) {
				const int32_t model_index = it.first;
				svg_base_edict_t * ent = it.second;

				// Populate runtime entry.
				nav_inline_model_runtime_t &rt = g_nav_mesh->inline_model_runtime[ out_index ];
				rt.model_index = model_index;
				rt.owner_ent = ent;
				rt.owner_entnum = ent ? ent->s.number : 0;
				rt.origin = ent ? ent->currentOrigin : Vector3{};
				rt.angles = ent ? ent->currentAngles : Vector3{};
				rt.dirty = false;
				out_index++;
			}
		}

		// Refresh transforms once so runtime entries match current entity state.
		SVG_Nav_RefreshInlineModelRuntime();
	}

	/** 
	* 	Rebuild aggregate counters now that all loaded tile storage is populated.
	**/
	if ( !Nav_RebuildLoadedStatistics( g_nav_mesh.get() ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	/** 
	* 	Do a quick statistics validation test to see if the loaded mesh matches expected generation statistic results. 
	* 	This can help detect mismatches between the navmesh and game configuration:
	* 	(e.g. loading a mesh generated with different parameters or from a different map version).
	**/
  if ( statistic_total_tiles != ( uint64_t )g_nav_mesh->total_tiles
		|| statistic_total_xy_cells != ( uint64_t )g_nav_mesh->total_xy_cells
		|| statistic_total_layers != ( uint64_t )g_nav_mesh->total_layers ) {
		//gzclose( f );
		//SVG_Nav_FreeMesh();
		//return false;
		gi.bprintf( PRINT_WARNING, "%s: warning: loaded navmesh statistics mismatch (expected %llu tiles, %llu cells, %llu layers; got %llu tiles, %llu cells, %llu layers)\n",
          __func__,
			( unsigned long long )statistic_total_tiles,
			( unsigned long long )statistic_total_xy_cells,
			( unsigned long long )statistic_total_layers,
			( unsigned long long )( uint64_t )g_nav_mesh->total_tiles,
			( unsigned long long )( uint64_t )g_nav_mesh->total_xy_cells,
			( unsigned long long )( uint64_t )g_nav_mesh->total_layers );
	}

	/** 
 * 	Rebuild the initial static tile adjacency and region partition for loaded meshes from the persisted traversal metadata.
	**/
    SVG_Nav_Hierarchy_BuildStaticRegions( g_nav_mesh.get() );

	/** 
	* 	Rebuild the coarse world-tile cluster graph for loaded meshes.
	* 		Generation already does this, but loaded navmeshes must rebuild the same runtime graph
	* 		or async requests will always report `has_route=0` and fall back to unrestricted fine A* .
	**/
	SVG_Nav_ClusterGraph_BuildFromMesh_World( g_nav_mesh.get() );

	gzclose( f );
	return true;
}
