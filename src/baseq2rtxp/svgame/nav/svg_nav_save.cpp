/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Save - Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_save.h"

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

static constexpr uint32_t NAV_MESH_SAVE_MAGIC = 0x56414E53; // "VANS"
static constexpr uint32_t NAV_MESH_SAVE_VERSION = 1;

/**
*	@brief	Write raw bytes to a (possibly gzipped) file.
*	@return	True if all bytes were written.
**/
static bool Nav_Write( gzFile f, const void *data, const size_t size ) {
	if ( !f || !data || size == 0 ) {
		return false;
	}

	return gzwrite( f, data, (unsigned)size ) == (int)size;
}

template<typename T>
static bool Nav_WriteValue( gzFile f, const T &v ) {
	/**
	*	Write a typed POD value.
	**/
	return Nav_Write( f, &v, sizeof( T ) );
}

/**
*	@brief	Serialize the current navigation voxelmesh to a file.
*	@return	True on success, false on failure.
**/
bool SVG_Nav_SaveVoxelMesh( const char *filename ) {
	/**
	*	Sanity checks: require filename and an active nav mesh.
	**/
	if ( !filename || !filename[ 0 ] ) {
		return false;
	}
	if ( !g_nav_mesh ) {
		return false;
	}

	/**
	*	Build full file path for the nav cache.
	**/
	const std::string filePath = BASEGAME "/maps/nav/" + std::string( filename );

	/**
	*	Open output file (gzip if zlib enabled).
	**/
	gzFile f = gzopen( filePath.c_str(), "wb" );
	if ( !f ) {
		gi.dprintf( "%s: failed to open '%s'\n", __func__, filePath.c_str() );
		return false;
	}

	gzbuffer( f, 65536 );

	// Snapshot the mesh pointer for readability.
	const nav_mesh_t *mesh = g_nav_mesh;

	/**
	*	Write file header (magic + version).
	**/
	if ( !Nav_WriteValue( f, NAV_MESH_SAVE_MAGIC ) ||
		!Nav_WriteValue( f, NAV_MESH_SAVE_VERSION ) ) {
		gzclose( f );
		return false;
	}

	/**
	*	Write generation parameters needed to interpret mesh data.
	**/
	Nav_WriteValue( f, mesh->cell_size_xy );
	Nav_WriteValue( f, mesh->z_quant );
	Nav_WriteValue( f, mesh->tile_size );
	Nav_WriteValue( f, mesh->max_step );
	Nav_WriteValue( f, mesh->max_slope_deg );
	Nav_WriteValue( f, mesh->agent_mins );
	Nav_WriteValue( f, mesh->agent_maxs );

	/**
	*	Write mesh counts.
	**/
	Nav_WriteValue( f, mesh->num_leafs );
	Nav_WriteValue( f, mesh->num_inline_models );

	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;

	/**
	*	Write world mesh (per BSP leaf).
	**/
	for ( int32_t i = 0; i < mesh->num_leafs; i++ ) {
		const nav_leaf_data_t *leaf = &mesh->leaf_data[ i ];
		// Write leaf header.
		Nav_WriteValue( f, leaf->leaf_index );
		Nav_WriteValue( f, leaf->num_tiles );

		for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
			const nav_tile_t *tile = &leaf->tiles[ t ];
			// Write tile coordinates.
			Nav_WriteValue( f, tile->tile_x );
			Nav_WriteValue( f, tile->tile_y );

			/**
			*	Count populated cells so we only serialize active cells.
			**/
			int32_t populated = 0;
			for ( int32_t c = 0; c < cellsPerTile; c++ ) {
				if ( tile->cells[ c ].num_layers > 0 && tile->cells[ c ].layers ) {
					populated++;
				}
			}
			// Write populated cell count.
			Nav_WriteValue( f, populated );

			for ( int32_t c = 0; c < cellsPerTile; c++ ) {
				const nav_xy_cell_t &cell = tile->cells[ c ];
				if ( cell.num_layers <= 0 || !cell.layers ) {
					continue;
				}

				// Write cell index + layer count.
				Nav_WriteValue( f, c );
				Nav_WriteValue( f, cell.num_layers );
				// Write raw layer array.
				Nav_Write( f, cell.layers, sizeof( nav_layer_t ) * (size_t)cell.num_layers );
			}
		}
	}

	/**
	*	Write inline model mesh (model-local space).
	**/
	for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
		const nav_inline_model_data_t *model = &mesh->inline_model_data[ i ];
		// Write model header.
		Nav_WriteValue( f, model->model_index );
		Nav_WriteValue( f, model->num_tiles );

		for ( int32_t t = 0; t < model->num_tiles; t++ ) {
			const nav_tile_t *tile = &model->tiles[ t ];
			// Write tile coordinates.
			Nav_WriteValue( f, tile->tile_x );
			Nav_WriteValue( f, tile->tile_y );

			/**
			*	Count populated cells so we only serialize active cells.
			**/
			int32_t populated = 0;
			for ( int32_t c = 0; c < cellsPerTile; c++ ) {
				if ( tile->cells[ c ].num_layers > 0 && tile->cells[ c ].layers ) {
					populated++;
				}
			}
			// Write populated cell count.
			Nav_WriteValue( f, populated );

			for ( int32_t c = 0; c < cellsPerTile; c++ ) {
				const nav_xy_cell_t &cell = tile->cells[ c ];
				if ( cell.num_layers <= 0 || !cell.layers ) {
					continue;
				}

				// Write cell index + layer count.
				Nav_WriteValue( f, c );
				Nav_WriteValue( f, cell.num_layers );
				// Write raw layer array.
				Nav_Write( f, cell.layers, sizeof( nav_layer_t ) * (size_t)cell.num_layers );
			}
		}
	}

	gzclose( f );
	return true;
}
