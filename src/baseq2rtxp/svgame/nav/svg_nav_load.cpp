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

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

static constexpr uint32_t NAV_MESH_SAVE_MAGIC = 0x56414E53; // "VANS"
static constexpr uint32_t NAV_MESH_SAVE_VERSION_V1 = 1;
static constexpr uint32_t NAV_MESH_SAVE_VERSION_V2 = 2;
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

struct nav_cache_param_chunk_t {
	double cell_size_xy = 0.0;
	double z_quant = 0.0;
	int32_t tile_size = 0;
	double max_step = 0.0;
	double max_slope_deg = 0.0;
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};
	int32_t num_leafs = 0;
	int32_t num_inline_models = 0;
	int32_t num_world_tiles = 0;
};

static uint32_t Nav_CalcMapHash( void ) {
	if ( !level.mapname[ 0 ] ) {
		return 0;
	}
	return Nav_Hash32( level.mapname, strlen( level.mapname ) );
}

static uint32_t Nav_CalcParamsHash( const nav_cache_param_chunk_t &params ) {
	const nav_cache_param_chunk_t hashData = params;
	return Nav_Hash32( &hashData, sizeof( hashData ) );
}

static inline bool Nav_NearlyEqual( const double a, const double b, const double eps = 0.0001 ) {
	return std::fabs( a - b ) <= eps;
}

static bool Nav_ParamChunkMatchesActiveCvars( const nav_cache_param_chunk_t &params ) {
	const double runtimeCellSize = nav_cell_size_xy ? nav_cell_size_xy->value : params.cell_size_xy;
	const double runtimeZQuant = nav_z_quant ? nav_z_quant->value : params.z_quant;
	const int32_t runtimeTileSize = nav_tile_size ? nav_tile_size->integer : params.tile_size;
	const double runtimeMaxStep = nav_max_step ? nav_max_step->value : params.max_step;
	const double runtimeMaxSlope = nav_max_slope_deg ? nav_max_slope_deg->value : params.max_slope_deg;
	const Vector3 runtimeMins = {
		nav_agent_mins_x ? nav_agent_mins_x->value : (float)params.agent_mins.x,
		nav_agent_mins_y ? nav_agent_mins_y->value : (float)params.agent_mins.y,
		nav_agent_mins_z ? nav_agent_mins_z->value : (float)params.agent_mins.z
	};
	const Vector3 runtimeMaxs = {
		nav_agent_maxs_x ? nav_agent_maxs_x->value : (float)params.agent_maxs.x,
		nav_agent_maxs_y ? nav_agent_maxs_y->value : (float)params.agent_maxs.y,
		nav_agent_maxs_z ? nav_agent_maxs_z->value : (float)params.agent_maxs.z
	};

	return Nav_NearlyEqual( params.cell_size_xy, runtimeCellSize )
		&& Nav_NearlyEqual( params.z_quant, runtimeZQuant )
		&& params.tile_size == runtimeTileSize
		&& Nav_NearlyEqual( params.max_step, runtimeMaxStep )
		&& Nav_NearlyEqual( params.max_slope_deg, runtimeMaxSlope )
		&& Nav_NearlyEqual( params.agent_mins.x, runtimeMins.x )
		&& Nav_NearlyEqual( params.agent_mins.y, runtimeMins.y )
		&& Nav_NearlyEqual( params.agent_mins.z, runtimeMins.z )
		&& Nav_NearlyEqual( params.agent_maxs.x, runtimeMaxs.x )
		&& Nav_NearlyEqual( params.agent_maxs.y, runtimeMaxs.y )
		&& Nav_NearlyEqual( params.agent_maxs.z, runtimeMaxs.z );
}

/**
*	@brief	Read raw bytes from a (possibly gzipped) file.
*	@return	True if all bytes were read.
**/
static bool Nav_Read( gzFile f, void *data, const size_t size ) {
	if ( !f || !data || size == 0 ) {
		return false;
	}

	return gzread( f, data, (unsigned)size ) == (int)size;
}

template<typename T>
static bool Nav_ReadValue( gzFile f, T &out ) {
	/**
	*	Read a typed POD value.
	**/
	return Nav_Read( f, &out, sizeof( T ) );
}

/**
*	@brief	Allocate an empty tile storage layout for load-time population.
**/
static void Nav_InitEmptyTileStorage( const nav_mesh_t *mesh, nav_tile_t *tile ) {
	/**
	*	Allocate sparse tile storage arrays for presence bits and per-cell layer lists.
	**/
	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;
	const int32_t presenceWords = ( cellsPerTile + 31 ) / 32;

	// Allocate presence bitset.
	tile->presence_bits = (uint32_t *)gi.TagMallocz( sizeof( uint32_t ) * presenceWords, TAG_SVGAME_LEVEL );
	// Allocate per-cell array.
	tile->cells = (nav_xy_cell_t *)gi.TagMallocz( sizeof( nav_xy_cell_t ) * cellsPerTile, TAG_SVGAME_LEVEL );
}

static inline void Nav_SetPresenceBit_Load( nav_tile_t *tile, const int32_t cellIndex ) {
	/**
	*	Mark a cell as present in the sparse tile storage.
	**/
	const int32_t wordIndex = cellIndex >> 5;
	const int32_t bitIndex = cellIndex & 31;
	tile->presence_bits[ wordIndex ] |= ( 1u << bitIndex );
}

struct nav_chunk_reader_t {
	const uint8_t *data = nullptr;
	size_t size = 0;
	size_t offset = 0;
};

static bool Nav_ChunkRead( nav_chunk_reader_t *reader, void *out, const size_t bytes ) {
	if ( !reader || !out ) {
		return false;
	}
	if ( bytes == 0 ) {
		return true;
	}
	if ( ( reader->offset + bytes ) > reader->size ) {
		return false;
	}

	memcpy( out, reader->data + reader->offset, bytes );
	reader->offset += bytes;
	return true;
}

template<typename T>
static bool Nav_ChunkReadValue( nav_chunk_reader_t *reader, T &out ) {
	return Nav_ChunkRead( reader, &out, sizeof( T ) );
}

static bool Nav_ReadChunk( gzFile f, uint32_t *outChunkId, std::vector<uint8_t> *outPayload ) {
	if ( !outChunkId || !outPayload ) {
		return false;
	}

	uint32_t chunkId = 0;
	uint32_t chunkSize = 0;
	uint32_t chunkCrc = 0;
	if ( !Nav_ReadValue( f, chunkId ) ||
		!Nav_ReadValue( f, chunkSize ) ||
		!Nav_ReadValue( f, chunkCrc ) ) {
		return false;
	}

	const uint32_t maxChunkBytes = 256u * 1024u * 1024u;
	if ( chunkSize > maxChunkBytes ) {
		return false;
	}

	outPayload->resize( chunkSize );
	if ( chunkSize > 0 && !Nav_Read( f, outPayload->data(), chunkSize ) ) {
		return false;
	}

	const uint32_t calcCrc = chunkSize > 0
		? Nav_Hash32( outPayload->data(), outPayload->size() )
		: 0;
	if ( calcCrc != chunkCrc ) {
		return false;
	}

	*outChunkId = chunkId;
	return true;
}

static bool Nav_ChunkReadTilePayload( nav_chunk_reader_t *reader, const nav_mesh_t *mesh, nav_tile_t *tile ) {
	if ( !reader || !mesh || !tile ) {
		return false;
	}

	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;
	if ( cellsPerTile <= 0 ) {
		return false;
	}

	if ( !Nav_ChunkReadValue( reader, tile->tile_x ) ||
		!Nav_ChunkReadValue( reader, tile->tile_y ) ) {
		return false;
	}

	Nav_InitEmptyTileStorage( mesh, tile );

	int32_t populated = 0;
	if ( !Nav_ChunkReadValue( reader, populated ) ) {
		return false;
	}
	if ( populated < 0 || populated > cellsPerTile ) {
		return false;
	}

	for ( int32_t p = 0; p < populated; p++ ) {
		int32_t cellIndex = 0;
		int32_t numLayers = 0;
		if ( !Nav_ChunkReadValue( reader, cellIndex ) ||
			!Nav_ChunkReadValue( reader, numLayers ) ) {
			return false;
		}

		if ( cellIndex < 0 || cellIndex >= cellsPerTile || numLayers <= 0 || numLayers > 64 ) {
			return false;
		}

		nav_xy_cell_t &cell = tile->cells[ cellIndex ];
		cell.num_layers = numLayers;
		cell.layers = (nav_layer_t *)gi.TagMallocz( sizeof( nav_layer_t ) * (size_t)numLayers, TAG_SVGAME_LEVEL );
		if ( !Nav_ChunkRead( reader, cell.layers, sizeof( nav_layer_t ) * (size_t)numLayers ) ) {
			return false;
		}

		Nav_SetPresenceBit_Load( tile, cellIndex );
	}

	return true;
}

static bool Nav_RebuildInlineRuntimeAfterLoad( nav_mesh_t *mesh ) {
	if ( !mesh ) {
		return false;
	}

	mesh->num_inline_model_runtime = 0;
	mesh->inline_model_runtime = nullptr;

	if ( mesh->num_inline_models <= 0 ) {
		return true;
	}

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
	return true;
}

static bool Nav_LoadVoxelMeshV2Payload( gzFile f ) {
	if ( !f ) {
		return false;
	}

	uint32_t endianMarker = 0;
	uint32_t mapHash = 0;
	uint32_t paramsHash = 0;
	uint32_t chunkCount = 0;
	if ( !Nav_ReadValue( f, endianMarker ) ||
		!Nav_ReadValue( f, mapHash ) ||
		!Nav_ReadValue( f, paramsHash ) ||
		!Nav_ReadValue( f, chunkCount ) ) {
		return false;
	}

	if ( endianMarker != NAV_MESH_SAVE_ENDIAN_MARKER ) {
		return false;
	}

	std::unordered_map<uint32_t, std::vector<uint8_t>> chunks = {};
	chunks.reserve( chunkCount );
	for ( uint32_t i = 0; i < chunkCount; i++ ) {
		uint32_t chunkId = 0;
		std::vector<uint8_t> payload = {};
		if ( !Nav_ReadChunk( f, &chunkId, &payload ) ) {
			return false;
		}
		chunks[ chunkId ] = std::move( payload );
	}

	const auto paramIt = chunks.find( NAV_CHUNK_PARAM );
	const auto worldTilesIt = chunks.find( NAV_CHUNK_WORLD_TILES );
	const auto leafMapIt = chunks.find( NAV_CHUNK_LEAF_MAP );
	const auto inlineIt = chunks.find( NAV_CHUNK_INLINE_MODELS );
	if ( paramIt == chunks.end() || worldTilesIt == chunks.end() || leafMapIt == chunks.end() || inlineIt == chunks.end() ) {
		return false;
	}

	SVG_Nav_FreeMesh();
	if ( !g_nav_mesh.create( TAG_SVGAME_LEVEL, []( nav_mesh_t *mesh ) {
		mesh->occupancy_frame = -1;
	} ) ) {
		return false;
	}

	nav_cache_param_chunk_t params = {};
	{
		nav_chunk_reader_t reader = { paramIt->second.data(), paramIt->second.size(), 0 };
		if ( !Nav_ChunkReadValue( &reader, params.cell_size_xy ) ||
			!Nav_ChunkReadValue( &reader, params.z_quant ) ||
			!Nav_ChunkReadValue( &reader, params.tile_size ) ||
			!Nav_ChunkReadValue( &reader, params.max_step ) ||
			!Nav_ChunkReadValue( &reader, params.max_slope_deg ) ||
			!Nav_ChunkReadValue( &reader, params.agent_mins ) ||
			!Nav_ChunkReadValue( &reader, params.agent_maxs ) ||
			!Nav_ChunkReadValue( &reader, params.num_leafs ) ||
			!Nav_ChunkReadValue( &reader, params.num_inline_models ) ||
			!Nav_ChunkReadValue( &reader, params.num_world_tiles ) ) {
			SVG_Nav_FreeMesh();
			return false;
		}

		if ( params.tile_size <= 0 ||
			params.num_leafs < 0 || params.num_leafs > 1'000'000 ||
			params.num_inline_models < 0 || params.num_inline_models > 1'000'000 ||
			params.num_world_tiles < 0 || params.num_world_tiles > 1'000'000 ) {
			SVG_Nav_FreeMesh();
			return false;
		}
		if ( reader.offset != reader.size ) {
			SVG_Nav_FreeMesh();
			return false;
		}

		const uint32_t calculatedParamsHash = Nav_CalcParamsHash( params );
		if ( paramsHash != calculatedParamsHash ) {
			gi.dprintf( "[Nav] cache load failed: PARAM hash mismatch (file=0x%08X calc=0x%08X)\n",
				paramsHash, calculatedParamsHash );
			SVG_Nav_FreeMesh();
			return false;
		}

		const bool requireHashMatch = nav_cache_require_hash_match && nav_cache_require_hash_match->integer != 0;
		if ( requireHashMatch ) {
			const uint32_t runtimeMapHash = Nav_CalcMapHash();
			if ( mapHash != runtimeMapHash ) {
				gi.dprintf( "[Nav] cache load failed: map hash mismatch (file=0x%08X runtime=0x%08X)\n",
					mapHash, runtimeMapHash );
				SVG_Nav_FreeMesh();
				return false;
			}

			if ( !Nav_ParamChunkMatchesActiveCvars( params ) ) {
				gi.dprintf( "[Nav] cache load failed: nav generation cvars changed; rebake required (set nav_cache_require_hash_match 0 to override)\n" );
				SVG_Nav_FreeMesh();
				return false;
			}
		}

		g_nav_mesh->cell_size_xy = params.cell_size_xy;
		g_nav_mesh->z_quant = params.z_quant;
		g_nav_mesh->tile_size = params.tile_size;
		g_nav_mesh->max_step = params.max_step;
		g_nav_mesh->max_slope_deg = params.max_slope_deg;
		g_nav_mesh->agent_mins = params.agent_mins;
		g_nav_mesh->agent_maxs = params.agent_maxs;
		g_nav_mesh->num_leafs = params.num_leafs;
		g_nav_mesh->num_inline_models = params.num_inline_models;
	}

	{
		nav_chunk_reader_t reader = { worldTilesIt->second.data(), worldTilesIt->second.size(), 0 };
		int32_t numWorldTiles = 0;
		if ( !Nav_ChunkReadValue( &reader, numWorldTiles ) ) {
			SVG_Nav_FreeMesh();
			return false;
		}
		if ( numWorldTiles < 0 || numWorldTiles > 1'000'000 ) {
			SVG_Nav_FreeMesh();
			return false;
		}
		if ( numWorldTiles != params.num_world_tiles ) {
			SVG_Nav_FreeMesh();
			return false;
		}

		g_nav_mesh->world_tiles.clear();
		g_nav_mesh->world_tile_id_of.clear();
		g_nav_mesh->world_tiles.reserve( (size_t)numWorldTiles );
		g_nav_mesh->world_tile_id_of.reserve( (size_t)numWorldTiles * 2u );

		for ( int32_t i = 0; i < numWorldTiles; i++ ) {
			nav_tile_t tile = {};
			if ( !Nav_ChunkReadTilePayload( &reader, g_nav_mesh.get(), &tile ) ) {
				SVG_Nav_FreeMesh();
				return false;
			}

			const int32_t tileId = (int32_t)g_nav_mesh->world_tiles.size();
			g_nav_mesh->world_tiles.push_back( tile );
			nav_world_tile_key_t key = { tile.tile_x, tile.tile_y };
			g_nav_mesh->world_tile_id_of[ key ] = tileId;
		}
		if ( reader.offset != reader.size ) {
			SVG_Nav_FreeMesh();
			return false;
		}
	}

	{
		nav_chunk_reader_t reader = { leafMapIt->second.data(), leafMapIt->second.size(), 0 };
		int32_t numLeafsChunk = 0;
		if ( !Nav_ChunkReadValue( &reader, numLeafsChunk ) ) {
			SVG_Nav_FreeMesh();
			return false;
		}
		if ( numLeafsChunk != g_nav_mesh->num_leafs ) {
			SVG_Nav_FreeMesh();
			return false;
		}

		g_nav_mesh->leaf_data = (nav_leaf_data_t *)gi.TagMallocz( sizeof( nav_leaf_data_t ) * (size_t)g_nav_mesh->num_leafs, TAG_SVGAME_LEVEL );
		for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
			nav_leaf_data_t &leaf = g_nav_mesh->leaf_data[ i ];
			if ( !Nav_ChunkReadValue( &reader, leaf.leaf_index ) ||
				!Nav_ChunkReadValue( &reader, leaf.num_tiles ) ) {
				SVG_Nav_FreeMesh();
				return false;
			}
			if ( leaf.num_tiles < 0 || leaf.num_tiles > 1'000'000 ) {
				SVG_Nav_FreeMesh();
				return false;
			}

			leaf.tile_ids = (int32_t *)gi.TagMallocz( sizeof( int32_t ) * (size_t)leaf.num_tiles, TAG_SVGAME_LEVEL );
			for ( int32_t t = 0; t < leaf.num_tiles; t++ ) {
				int32_t tileId = -1;
				if ( !Nav_ChunkReadValue( &reader, tileId ) ) {
					SVG_Nav_FreeMesh();
					return false;
				}
				if ( tileId < 0 || tileId >= (int32_t)g_nav_mesh->world_tiles.size() ) {
					SVG_Nav_FreeMesh();
					return false;
				}
				leaf.tile_ids[ t ] = tileId;
			}
		}
		if ( reader.offset != reader.size ) {
			SVG_Nav_FreeMesh();
			return false;
		}
	}

	{
		nav_chunk_reader_t reader = { inlineIt->second.data(), inlineIt->second.size(), 0 };
		int32_t numInlineChunk = 0;
		if ( !Nav_ChunkReadValue( &reader, numInlineChunk ) ) {
			SVG_Nav_FreeMesh();
			return false;
		}
		if ( numInlineChunk != g_nav_mesh->num_inline_models ) {
			SVG_Nav_FreeMesh();
			return false;
		}

		if ( g_nav_mesh->num_inline_models > 0 ) {
			g_nav_mesh->inline_model_data = (nav_inline_model_data_t *)gi.TagMallocz( sizeof( nav_inline_model_data_t ) * (size_t)g_nav_mesh->num_inline_models, TAG_SVGAME_LEVEL );
			for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
				nav_inline_model_data_t &model = g_nav_mesh->inline_model_data[ i ];
				if ( !Nav_ChunkReadValue( &reader, model.model_index ) ||
					!Nav_ChunkReadValue( &reader, model.num_tiles ) ) {
					SVG_Nav_FreeMesh();
					return false;
				}
				if ( model.num_tiles < 0 || model.num_tiles > 1'000'000 ) {
					SVG_Nav_FreeMesh();
					return false;
				}

				if ( model.num_tiles > 0 ) {
					model.tiles = (nav_tile_t *)gi.TagMallocz( sizeof( nav_tile_t ) * (size_t)model.num_tiles, TAG_SVGAME_LEVEL );
					for ( int32_t t = 0; t < model.num_tiles; t++ ) {
						if ( !Nav_ChunkReadTilePayload( &reader, g_nav_mesh.get(), &model.tiles[ t ] ) ) {
							SVG_Nav_FreeMesh();
							return false;
						}
					}
				}
			}
		}
		if ( reader.offset != reader.size ) {
			SVG_Nav_FreeMesh();
			return false;
		}
	}

	return Nav_RebuildInlineRuntimeAfterLoad( g_nav_mesh.get() );
}

/**
*	@brief	Load a navigation voxelmesh from file into `g_nav_mesh`.
*	@return	True on success, false on failure.
**/
bool SVG_Nav_LoadVoxelMesh( const char *filename ) {
	/**
	*	Sanity: require filename.
	**/
	if ( !filename || !filename[ 0 ] ) {
		return false;
	}

	//if ( baseGame.empty() ) {
	//	baseGame = BASEGAME;
	//}
	//// Build full file path.
	//const std::string filePath = baseGame + "/maps/navmeshes/" + std::string( filename );
	/**
	*	Build full file path.
	**/
	const std::string filePath = BASEGAME "/maps/nav/" + std::string( filename );

	/**
	*	Open nav cache file (gzip if available).
	**/
	gzFile f = gzopen( filePath.c_str(), "rb" );
	if ( !f ) {
		gi.dprintf( "%s: failed to open '%s'\n", __func__, filePath.c_str() );
		return false;
	}

	/**
	*	Configure a larger read buffer for fewer syscalls.
	**/
	gzbuffer( f, 65536 );

	/**
	*	Read and validate file header.
	**/
	uint32_t magic = 0;
	uint32_t version = 0;
	if ( !Nav_ReadValue( f, magic ) || !Nav_ReadValue( f, version ) ) {
		gzclose( f );
		return false;
	}
	if ( magic != NAV_MESH_SAVE_MAGIC ) {
		gzclose( f );
		return false;
	}

	if ( version == NAV_MESH_SAVE_VERSION_V2 ) {
		const bool loaded = Nav_LoadVoxelMeshV2Payload( f );
		gzclose( f );
		if ( !loaded ) {
			SVG_Nav_FreeMesh();
		}
		return loaded;
	}

	if ( version != NAV_MESH_SAVE_VERSION_V1 ) {
		gzclose( f );
		return false;
	}

	/**
	*	Replace any existing mesh before allocating a new one.
	**/
	SVG_Nav_FreeMesh();

	/**
	*	Allocate and publish the mesh instance using RAII helper.
	*	Initialize occupancy_frame in the callback.
	**/
	if ( !g_nav_mesh.create( TAG_SVGAME_LEVEL, []( nav_mesh_t *mesh ) {
		mesh->occupancy_frame = -1;
	} ) ) {
		gzclose( f );
		return false;
	}

	/**
	*	Read generation parameters required to interpret stored tile data.
	**/
	if ( !Nav_ReadValue( f, g_nav_mesh->cell_size_xy ) ||
		!Nav_ReadValue( f, g_nav_mesh->z_quant ) ||
		!Nav_ReadValue( f, g_nav_mesh->tile_size ) ||
		!Nav_ReadValue( f, g_nav_mesh->max_step ) ||
		!Nav_ReadValue( f, g_nav_mesh->max_slope_deg ) ||
		!Nav_ReadValue( f, g_nav_mesh->agent_mins ) ||
		!Nav_ReadValue( f, g_nav_mesh->agent_maxs ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	/**
	*	Read mesh counts (leafs + inline models).
	**/
	if ( !Nav_ReadValue( f, g_nav_mesh->num_leafs ) || !Nav_ReadValue( f, g_nav_mesh->num_inline_models ) ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	/**
	*	Sanity: validate counts before allocating large arrays.
	**/
	if ( g_nav_mesh->num_leafs < 0 || g_nav_mesh->num_leafs > 1'000'000 ) {
		gzclose( f );
		SVG_Nav_FreeMesh();
		return false;
	}

	// Allocate leaf array.
	g_nav_mesh->leaf_data = (nav_leaf_data_t *)gi.TagMallocz( sizeof( nav_leaf_data_t ) * (size_t)g_nav_mesh->num_leafs, TAG_SVGAME_LEVEL );

	/**
	*	Initialize canonical world tile storage used during load.
	**/
	g_nav_mesh->world_tiles.clear();
	g_nav_mesh->world_tile_id_of.clear();
	g_nav_mesh->world_tiles.reserve( 1024 );
	g_nav_mesh->world_tile_id_of.reserve( 2048 );

	const int32_t cellsPerTile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;

	/**
	*	Read world mesh tiles (per BSP leaf).
	**/
	for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
		nav_leaf_data_t &leaf = g_nav_mesh->leaf_data[ i ];
		// Read leaf header.
		if ( !Nav_ReadValue( f, leaf.leaf_index ) || !Nav_ReadValue( f, leaf.num_tiles ) ) {
			gzclose( f );
			SVG_Nav_FreeMesh();
			return false;
		}

		// Sanity check: tile count must be non-negative.
		if ( leaf.num_tiles < 0 ) {
			gzclose( f );
			SVG_Nav_FreeMesh();
			return false;
		}

		/**
		*	Allocate leaf tile-id array.
		*	
		*	Tiles themselves are stored canonically in `mesh->world_tiles`.
		**/
		leaf.tile_ids = (int32_t *)gi.TagMallocz( sizeof( int32_t ) * (size_t)leaf.num_tiles, TAG_SVGAME_LEVEL );

		for ( int32_t t = 0; t < leaf.num_tiles; t++ ) {
			nav_world_tile_key_t key = {};
			// Read tile coordinates.
			if ( !Nav_ReadValue( f, key.tile_x ) || !Nav_ReadValue( f, key.tile_y ) ) {
				gzclose( f );
				SVG_Nav_FreeMesh();
				return false;
			}

			/**
			*	Resolve or create canonical world tile entry.
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
				Nav_InitEmptyTileStorage( g_nav_mesh.get(), &newTile );
				g_nav_mesh->world_tiles.push_back( newTile );
				g_nav_mesh->world_tile_id_of.emplace( key, tileId );
			}

			leaf.tile_ids[ t ] = tileId;
			nav_tile_t &tile = g_nav_mesh->world_tiles[ tileId ];

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

				nav_xy_cell_t &cell = tile.cells[ cellIndex ];
				// Store layer count and allocate layer buffer.
				cell.num_layers = numLayers;
				cell.layers = (nav_layer_t *)gi.TagMallocz( sizeof( nav_layer_t ) * (size_t)numLayers, TAG_SVGAME_LEVEL );
				// Read layer data directly into allocated buffer.
				if ( !Nav_Read( f, cell.layers, sizeof( nav_layer_t ) * (size_t)numLayers ) ) {
					gzclose( f );
					SVG_Nav_FreeMesh();
					return false;
				}

				// Mark the cell as populated in the tile's sparse bitset.
				Nav_SetPresenceBit_Load( &tile, cellIndex );
			}
		}
	}

	/**
	*	Read inline model meshes (model-local space).
	**/
	if ( g_nav_mesh->num_inline_models > 0 ) {
		// Allocate per-inline-model container array.
		g_nav_mesh->inline_model_data = (nav_inline_model_data_t *)gi.TagMallocz( sizeof( nav_inline_model_data_t ) * (size_t)g_nav_mesh->num_inline_models, TAG_SVGAME_LEVEL );

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
			model.tiles = (nav_tile_t *)gi.TagMallocz( sizeof( nav_tile_t ) * (size_t)model.num_tiles, TAG_SVGAME_LEVEL );

			for ( int32_t t = 0; t < model.num_tiles; t++ ) {
				nav_tile_t &tile = model.tiles[ t ];
				// Read tile coordinates.
				if ( !Nav_ReadValue( f, tile.tile_x ) || !Nav_ReadValue( f, tile.tile_y ) ) {
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

					nav_xy_cell_t &cell = tile.cells[ cellIndex ];
					// Store layer count and allocate layer buffer.
					cell.num_layers = numLayers;
					cell.layers = (nav_layer_t *)gi.TagMallocz( sizeof( nav_layer_t ) * (size_t)numLayers, TAG_SVGAME_LEVEL );
					// Read layer data into allocated buffer.
					if ( !Nav_Read( f, cell.layers, sizeof( nav_layer_t ) * (size_t)numLayers ) ) {
						gzclose( f );
						SVG_Nav_FreeMesh();
						return false;
					}

					// Mark the cell as populated.
					Nav_SetPresenceBit_Load( &tile, cellIndex );
				}
			}
		}
	}

	/**
	*	Runtime inline-model mapping is not saved; rebuild it from live entities.
	**/
	g_nav_mesh->num_inline_model_runtime = 0;
	g_nav_mesh->inline_model_runtime = nullptr;

	// Rebuild runtime mapping now that the mesh is loaded so inline-model traversal has live transforms.
	// Refresh alone is not enough because the runtime array is not persisted and may be null.
	if ( g_nav_mesh->num_inline_models > 0 ) {
		/**
		*	Build model_index -> entity mapping for all active entities referencing "*N".
		**/
		std::unordered_map<int32_t, svg_base_edict_t *> model_to_ent;
		model_to_ent.reserve( (size_t)g_nav_mesh->num_inline_models );
		for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++ ) {
			// Inspect each edict for inline model usage.
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
		// Allocate runtime array sized to the number of referenced models.
		g_nav_mesh->num_inline_model_runtime = (int32_t)model_to_ent.size();
		if ( g_nav_mesh->num_inline_model_runtime > 0 ) {
			// Allocate runtime transform array.
			g_nav_mesh->inline_model_runtime = (nav_inline_model_runtime_t *)gi.TagMallocz(
				sizeof( nav_inline_model_runtime_t ) * (size_t)g_nav_mesh->num_inline_model_runtime, TAG_SVGAME_LEVEL );
			int32_t out_index = 0;
			for ( const auto &it : model_to_ent ) {
				const int32_t model_index = it.first;
				svg_base_edict_t *ent = it.second;

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

	gzclose( f );
	return true;
}
