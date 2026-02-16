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
#include <vector>

static constexpr uint32_t NAV_MESH_SAVE_MAGIC = 0x56414E53; // "VANS"
static constexpr uint32_t NAV_MESH_SAVE_VERSION = 2;
static constexpr uint32_t NAV_MESH_SAVE_ENDIAN_MARKER = 0x12345678;

static constexpr uint32_t Nav_FourCC( const char a, const char b, const char c, const char d ) {
	return (uint32_t)(uint8_t)a |
		( (uint32_t)(uint8_t)b << 8 ) |
		( (uint32_t)(uint8_t)c << 16 ) |
		( (uint32_t)(uint8_t)d << 24 );
}

static constexpr uint32_t NAV_CHUNK_PARAM = Nav_FourCC( 'P', 'A', 'R', 'M' );
static constexpr uint32_t NAV_CHUNK_WORLD_TILES = Nav_FourCC( 'W', 'T', 'I', 'L' );
static constexpr uint32_t NAV_CHUNK_LEAF_MAP = Nav_FourCC( 'L', 'E', 'A', 'F' );
static constexpr uint32_t NAV_CHUNK_INLINE_MODELS = Nav_FourCC( 'I', 'M', 'O', 'D' );

static uint32_t Nav_Hash32( const void *data, const size_t size ) {
	const uint8_t *bytes = (const uint8_t *)data;
	uint32_t hash = 2166136261u;
	for ( size_t i = 0; i < size; i++ ) {
		hash ^= bytes[ i ];
		hash *= 16777619u;
	}
	return hash;
}

/**
*	@brief	Write raw bytes to a (possibly gzipped) file.
*	@return	True if all bytes were written.
**/
static bool Nav_Write( gzFile f, const void *data, const size_t size ) {
	if ( !f || ( !data && size > 0 ) ) {
		return false;
	}
	if ( size == 0 ) {
		return true;
	}
	return gzwrite( f, data, (unsigned)size ) == (int)size;
}

template<typename T>
static bool Nav_WriteValue( gzFile f, const T &v ) {
	return Nav_Write( f, &v, sizeof( T ) );
}

static void Nav_BufferWrite( std::vector<uint8_t> &out, const void *data, const size_t size ) {
	if ( !data || size == 0 ) {
		return;
	}

	const size_t oldSize = out.size();
	out.resize( oldSize + size );
	memcpy( out.data() + oldSize, data, size );
}

template<typename T>
static void Nav_BufferWriteValue( std::vector<uint8_t> &out, const T &v ) {
	Nav_BufferWrite( out, &v, sizeof( T ) );
}

static void Nav_BufferWriteTilePayload( std::vector<uint8_t> &out, const nav_tile_t *tile, const int32_t cellsPerTile ) {
	if ( !tile ) {
		const int32_t tileX = 0;
		const int32_t tileY = 0;
		const int32_t populated = 0;
		Nav_BufferWriteValue( out, tileX );
		Nav_BufferWriteValue( out, tileY );
		Nav_BufferWriteValue( out, populated );
		return;
	}

	Nav_BufferWriteValue( out, tile->tile_x );
	Nav_BufferWriteValue( out, tile->tile_y );

	int32_t populated = 0;
	for ( int32_t c = 0; c < cellsPerTile; c++ ) {
		if ( tile->cells && tile->cells[ c ].num_layers > 0 && tile->cells[ c ].layers ) {
			populated++;
		}
	}
	Nav_BufferWriteValue( out, populated );

	if ( populated <= 0 || !tile->cells ) {
		return;
	}

	for ( int32_t c = 0; c < cellsPerTile; c++ ) {
		const nav_xy_cell_t &cell = tile->cells[ c ];
		if ( cell.num_layers <= 0 || !cell.layers ) {
			continue;
		}

		Nav_BufferWriteValue( out, c );
		Nav_BufferWriteValue( out, cell.num_layers );
		Nav_BufferWrite( out, cell.layers, sizeof( nav_layer_t ) * (size_t)cell.num_layers );
	}
}

static bool Nav_WriteChunk( gzFile f, const uint32_t chunkId, const std::vector<uint8_t> &payload ) {
	const uint32_t payloadSize = (uint32_t)payload.size();
	const uint32_t payloadCrc = payloadSize > 0
		? Nav_Hash32( payload.data(), payload.size() )
		: 0;

	if ( !Nav_WriteValue( f, chunkId ) ||
		!Nav_WriteValue( f, payloadSize ) ||
		!Nav_WriteValue( f, payloadCrc ) ) {
		return false;
	}

	return Nav_Write( f, payload.data(), payloadSize );
}

static uint32_t Nav_CalcMapHash( void ) {
	if ( !level.mapname[ 0 ] ) {
		return 0;
	}
	return Nav_Hash32( level.mapname, strlen( level.mapname ) );
}

static uint32_t Nav_CalcParamsHash( const nav_mesh_t *mesh ) {
	if ( !mesh ) {
		return 0;
	}

	struct nav_params_hash_data_t {
		double cell_size_xy;
		double z_quant;
		int32_t tile_size;
		double max_step;
		double max_slope_deg;
		Vector3 agent_mins;
		Vector3 agent_maxs;
		int32_t num_leafs;
		int32_t num_inline_models;
		int32_t num_world_tiles;
	};

	const nav_params_hash_data_t data = {
		mesh->cell_size_xy,
		mesh->z_quant,
		mesh->tile_size,
		mesh->max_step,
		mesh->max_slope_deg,
		mesh->agent_mins,
		mesh->agent_maxs,
		mesh->num_leafs,
		mesh->num_inline_models,
		(int32_t)mesh->world_tiles.size()
	};

	return Nav_Hash32( &data, sizeof( data ) );
}

/**
*	@brief	Serialize the current navigation voxelmesh to a file.
*	@return	True on success, false on failure.
**/
bool SVG_Nav_SaveVoxelMesh( const char *filename ) {
	if ( !filename || !filename[ 0 ] ) {
		return false;
	}
	if ( !g_nav_mesh ) {
		return false;
	}

	const std::string filePath = BASEGAME "/maps/nav/" + std::string( filename );
	gzFile f = gzopen( filePath.c_str(), "wb" );
	if ( !f ) {
		gi.dprintf( "%s: failed to open '%s'\n", __func__, filePath.c_str() );
		return false;
	}

	gzbuffer( f, 65536 );
	const nav_mesh_t *mesh = g_nav_mesh.get();
	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;

	const uint32_t chunkCount = 4;
	if ( !Nav_WriteValue( f, NAV_MESH_SAVE_MAGIC ) ||
		!Nav_WriteValue( f, NAV_MESH_SAVE_VERSION ) ||
		!Nav_WriteValue( f, NAV_MESH_SAVE_ENDIAN_MARKER ) ||
		!Nav_WriteValue( f, Nav_CalcMapHash() ) ||
		!Nav_WriteValue( f, Nav_CalcParamsHash( mesh ) ) ||
		!Nav_WriteValue( f, chunkCount ) ) {
		gzclose( f );
		return false;
	}

	std::vector<uint8_t> paramChunk = {};
	Nav_BufferWriteValue( paramChunk, mesh->cell_size_xy );
	Nav_BufferWriteValue( paramChunk, mesh->z_quant );
	Nav_BufferWriteValue( paramChunk, mesh->tile_size );
	Nav_BufferWriteValue( paramChunk, mesh->max_step );
	Nav_BufferWriteValue( paramChunk, mesh->max_slope_deg );
	Nav_BufferWriteValue( paramChunk, mesh->agent_mins );
	Nav_BufferWriteValue( paramChunk, mesh->agent_maxs );
	Nav_BufferWriteValue( paramChunk, mesh->num_leafs );
	Nav_BufferWriteValue( paramChunk, mesh->num_inline_models );
	const int32_t numWorldTiles = (int32_t)mesh->world_tiles.size();
	Nav_BufferWriteValue( paramChunk, numWorldTiles );
	if ( !Nav_WriteChunk( f, NAV_CHUNK_PARAM, paramChunk ) ) {
		gzclose( f );
		return false;
	}

	std::vector<uint8_t> worldTilesChunk = {};
	Nav_BufferWriteValue( worldTilesChunk, numWorldTiles );
	for ( int32_t i = 0; i < numWorldTiles; i++ ) {
		const nav_tile_t *tile = &mesh->world_tiles[ i ];
		Nav_BufferWriteTilePayload( worldTilesChunk, tile, cellsPerTile );
	}
	if ( !Nav_WriteChunk( f, NAV_CHUNK_WORLD_TILES, worldTilesChunk ) ) {
		gzclose( f );
		return false;
	}

	std::vector<uint8_t> leafMapChunk = {};
	Nav_BufferWriteValue( leafMapChunk, mesh->num_leafs );
	for ( int32_t i = 0; i < mesh->num_leafs; i++ ) {
		const nav_leaf_data_t *leaf = &mesh->leaf_data[ i ];
		Nav_BufferWriteValue( leafMapChunk, leaf->leaf_index );
		Nav_BufferWriteValue( leafMapChunk, leaf->num_tiles );
		for ( int32_t t = 0; t < leaf->num_tiles; t++ ) {
			int32_t tileId = -1;
			if ( leaf->tile_ids ) {
				tileId = leaf->tile_ids[ t ];
			}
			Nav_BufferWriteValue( leafMapChunk, tileId );
		}
	}
	if ( !Nav_WriteChunk( f, NAV_CHUNK_LEAF_MAP, leafMapChunk ) ) {
		gzclose( f );
		return false;
	}

	std::vector<uint8_t> inlineChunk = {};
	Nav_BufferWriteValue( inlineChunk, mesh->num_inline_models );
	for ( int32_t i = 0; i < mesh->num_inline_models; i++ ) {
		const nav_inline_model_data_t *model = &mesh->inline_model_data[ i ];
		Nav_BufferWriteValue( inlineChunk, model->model_index );
		Nav_BufferWriteValue( inlineChunk, model->num_tiles );

		for ( int32_t t = 0; t < model->num_tiles; t++ ) {
			const nav_tile_t *tile = model->tiles ? &model->tiles[ t ] : nullptr;
			Nav_BufferWriteTilePayload( inlineChunk, tile, cellsPerTile );
		}
	}
	if ( !Nav_WriteChunk( f, NAV_CHUNK_INLINE_MODELS, inlineChunk ) ) {
		gzclose( f );
		return false;
	}

	gzclose( f );
	return true;
}
