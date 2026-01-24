/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Load - Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
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
#include <unordered_map>
#include <vector>

static constexpr uint32_t NAV_MESH_SAVE_MAGIC = 0x56414E53; // "VANS"
static constexpr uint32_t NAV_MESH_SAVE_VERSION = 1;

static bool Nav_Read( gzFile f, void *data, const size_t size ) {
	if ( !f || !data || size == 0 ) {
		return false;
	}

	return gzread( f, data, (unsigned)size ) == (int)size;
}

template<typename T>
static bool Nav_ReadValue( gzFile f, T &out ) {
	return Nav_Read( f, &out, sizeof( T ) );
}

static void Nav_InitEmptyTileStorage( const nav_mesh_t *mesh, nav_tile_t *tile ) {
	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;
	const int32_t presenceWords = ( cellsPerTile + 31 ) / 32;

	tile->presence_bits = (uint32_t *)gi.TagMallocz( sizeof( uint32_t ) * presenceWords, TAG_SVGAME_LEVEL );
	tile->cells = (nav_xy_cell_t *)gi.TagMallocz( sizeof( nav_xy_cell_t ) * cellsPerTile, TAG_SVGAME_LEVEL );
}

static inline void Nav_SetPresenceBit_Load( nav_tile_t *tile, const int32_t cellIndex ) {
	const int32_t wordIndex = cellIndex >> 5;
	const int32_t bitIndex = cellIndex & 31;
	tile->presence_bits[ wordIndex ] |= ( 1u << bitIndex );
}

bool SVG_Nav_LoadVoxelMesh( const char *filename ) {
	if ( !filename || !filename[ 0 ] ) {
		return false;
	}

	//if ( baseGame.empty() ) {
	//	baseGame = BASEGAME;
	//}
	//// Build full file path.
	//const std::string filePath = baseGame + "/maps/navmeshes/" + std::string( filename );
	const std::string filePath = BASEGAME "/maps/nav/" + std::string( filename );

	gzFile f = gzopen( filePath.c_str(), "rb");
	if ( !f ) {
		gi.dprintf( "%s: failed to open '%s'\n", __func__, filePath.c_str() );
		return false;
	}

	gzbuffer( f, 65536 );

	uint32_t magic = 0;
	uint32_t version = 0;
	if ( !Nav_ReadValue( f, magic ) || !Nav_ReadValue( f, version ) ) {
		gzclose( f );
		return false;
	}
	if ( magic != NAV_MESH_SAVE_MAGIC || version != NAV_MESH_SAVE_VERSION ) {
		gzclose( f );
		return false;
	}

	// Replace any existing mesh.
	SVG_Nav_FreeMesh();

	nav_mesh_t *mesh = (nav_mesh_t *)gi.TagMallocz( sizeof( nav_mesh_t ), TAG_SVGAME_LEVEL );
	g_nav_mesh = mesh;

	// Generation params.
	if ( !Nav_ReadValue( f, mesh->cell_size_xy ) ||
		!Nav_ReadValue( f, mesh->z_quant ) ||
		!Nav_ReadValue( f, mesh->tile_size ) ||
		!Nav_ReadValue( f, mesh->max_step ) ||
		!Nav_ReadValue( f, mesh->max_slope_deg ) ||
		!Nav_ReadValue( f, mesh->agent_mins ) ||
		!Nav_ReadValue( f, mesh->agent_maxs ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	if ( !Nav_ReadValue( f, mesh->num_leafs ) || !Nav_ReadValue( f, mesh->num_inline_models ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	if ( mesh->num_leafs < 0 || mesh->num_leafs > 1'000'000 ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	mesh->leaf_data = (nav_leaf_data_t *)gi.TagMallocz( sizeof( nav_leaf_data_t ) * (size_t)mesh->num_leafs, TAG_SVGAME_LEVEL );

	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;

	// World mesh.
	for ( int32_t i = 0; i < mesh->num_leafs; i++ ) {
		nav_leaf_data_t &leaf = mesh->leaf_data[ i ];
		if ( !Nav_ReadValue( f, leaf.leaf_index ) || !Nav_ReadValue( f, leaf.num_tiles ) ) {
			gzclose( f );
			SVG_Nav_FreeMesh();
			return false;
		}

		if ( leaf.num_tiles < 0 ) {
			gzclose( f );
			SVG_Nav_FreeMesh();
			return false;
		}

		leaf.tiles = (nav_tile_t *)gi.TagMallocz( sizeof( nav_tile_t ) * (size_t)leaf.num_tiles, TAG_SVGAME_LEVEL );

		for ( int32_t t = 0; t < leaf.num_tiles; t++ ) {
			nav_tile_t &tile = leaf.tiles[ t ];
			if ( !Nav_ReadValue( f, tile.tile_x ) || !Nav_ReadValue( f, tile.tile_y ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			Nav_InitEmptyTileStorage( mesh, &tile );

			int32_t populated = 0;
			if ( !Nav_ReadValue( f, populated ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}
			if ( populated < 0 || populated > cellsPerTile ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			for ( int32_t p = 0; p < populated; p++ ) {
				int32_t cellIndex = 0;
				int32_t numLayers = 0;
				if ( !Nav_ReadValue( f, cellIndex ) || !Nav_ReadValue( f, numLayers ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				if ( cellIndex < 0 || cellIndex >= cellsPerTile || numLayers <= 0 || numLayers > 64 ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				nav_xy_cell_t &cell = tile.cells[ cellIndex ];
				cell.num_layers = numLayers;
				cell.layers = (nav_layer_t *)gi.TagMallocz( sizeof( nav_layer_t ) * (size_t)numLayers, TAG_SVGAME_LEVEL );
				if ( !Nav_Read( f, cell.layers, sizeof( nav_layer_t ) * (size_t)numLayers ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				Nav_SetPresenceBit_Load( &tile, cellIndex );
			}
		}
	}

	// Inline models.
	if ( mesh->num_inline_models > 0 ) {
		mesh->inline_model_data = (nav_inline_model_data_t *)gi.TagMallocz( sizeof( nav_inline_model_data_t ) * (size_t)mesh->num_inline_models, TAG_SVGAME_LEVEL );

		for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
			nav_inline_model_data_t &model = mesh->inline_model_data[ i ];
			if ( !Nav_ReadValue( f, model.model_index ) || !Nav_ReadValue( f, model.num_tiles ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			if ( model.num_tiles < 0 ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			model.tiles = (nav_tile_t *)gi.TagMallocz( sizeof( nav_tile_t ) * (size_t)model.num_tiles, TAG_SVGAME_LEVEL );

			for ( int32_t t = 0; t < model.num_tiles; t++ ) {
				nav_tile_t &tile = model.tiles[ t ];
				if ( !Nav_ReadValue( f, tile.tile_x ) || !Nav_ReadValue( f, tile.tile_y ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				Nav_InitEmptyTileStorage( mesh, &tile );

				int32_t populated = 0;
				if ( !Nav_ReadValue( f, populated ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}
				if ( populated < 0 || populated > cellsPerTile ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				for ( int32_t p = 0; p < populated; p++ ) {
					int32_t cellIndex = 0;
					int32_t numLayers = 0;
					if ( !Nav_ReadValue( f, cellIndex ) || !Nav_ReadValue( f, numLayers ) ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}

					if ( cellIndex < 0 || cellIndex >= cellsPerTile || numLayers <= 0 || numLayers > 64 ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}

					nav_xy_cell_t &cell = tile.cells[ cellIndex ];
					cell.num_layers = numLayers;
					cell.layers = (nav_layer_t *)gi.TagMallocz( sizeof( nav_layer_t ) * (size_t)numLayers, TAG_SVGAME_LEVEL );
					if ( !Nav_Read( f, cell.layers, sizeof( nav_layer_t ) * (size_t)numLayers ) ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}

					Nav_SetPresenceBit_Load( &tile, cellIndex );
				}
			}
		}
	}

	// Runtime mapping is not persisted (depends on live entities).
	mesh->num_inline_model_runtime = 0;
	mesh->inline_model_runtime = nullptr;

	// Rebuild runtime mapping now that the mesh is loaded so inline-model traversal has live transforms.
	// Refresh alone is not enough because the runtime array is not persisted and may be null.
	if ( mesh->num_inline_models > 0 ) {
		std::unordered_map<int32_t, svg_base_edict_t *> model_to_ent;
		model_to_ent.reserve( (size_t)mesh->num_inline_models );
		for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++ ) {
			svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
			if ( !ent || !SVG_Entity_IsActive( ent ) ) {
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

			if ( model_to_ent.find( modelIndex ) == model_to_ent.end() ) {
				model_to_ent.emplace( modelIndex, ent );
			}
		}

		// Build runtime mapping for all referenced inline models.
		// This allows traversal/pathfinding to use correct live transforms.
		mesh->num_inline_model_runtime = (int32_t)model_to_ent.size();
		if ( mesh->num_inline_model_runtime > 0 ) {
			mesh->inline_model_runtime = (nav_inline_model_runtime_t *)gi.TagMallocz(
				sizeof( nav_inline_model_runtime_t ) * (size_t)mesh->num_inline_model_runtime, TAG_SVGAME_LEVEL );
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

		SVG_Nav_RefreshInlineModelRuntime();
	}

	gzclose( f );
	return true;
}
