/********************************************************************
*
*
*    ServerGame: Nav3 Serialization Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav3/nav3_serialize.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "common/files.h"


/**
*
*
*    Nav3 Serialized Local Constants:
*
*
**/
//! Section version for the stage-4 build-config section payload.
static constexpr uint32_t NAV3_SERIALIZED_SECTION_VERSION_BUILD_CONFIG = 1;
//! Section version for the stage-7 generated-mesh section payload.
static constexpr uint32_t NAV3_SERIALIZED_SECTION_VERSION_GENERATED_MESH = 1;
//! Canonical game-directory prefix occasionally included by user-facing nav3 paths.
static constexpr const char *NAV3_SERIALIZED_GAME_DIR_PREFIX = "baseq2rtxp/";


/**
*
*
*    Nav3 Serialized Local Types:
*
*
**/
/**
*	@brief	Bounded reader cursor for decoding little-endian nav3 payload bytes.
**/
struct nav3_serialized_blob_reader_t {
	const uint8_t *data = nullptr;
	size_t size = 0;
	size_t offset = 0;
	bool failed = false;
};

/**
*	@brief	In-memory section payload used while assembling serialized blobs.
**/
struct nav3_serialized_section_payload_t {
	nav3_serialized_section_type_t type = nav3_serialized_section_type_t::None;
	uint32_t version = 0;
	uint32_t flags = NAV3_SERIALIZED_SECTION_FLAG_NONE;
	std::vector<uint8_t> bytes = {};
};


/**
*
*
*    Nav3 Serialized Local Helpers:
*
*
**/
/**
*	@brief	Build a reject result with one stable error code.
**/
static nav3_serialization_result_t Nav3_Serialize_MakeRejectResult( const nav3_serialized_error_t error ) {
	nav3_serialization_result_t result = {};
	result.success = false;
	result.validation.compatibility = nav3_serialized_compatibility_t::Reject;
	result.validation.error = error;
	return result;
}

/**
*	@brief	Normalize a caller-supplied cache path into a filesystem-write-safe relative path.
*	@param	cachePath	Raw caller cache path.
*	@param	outPath		[out] Normalized relative cache path.
*	@param	outPathCount	Capacity of `outPath`.
*	@return	True when path normalization succeeded.
**/
static const bool Nav3_Serialize_NormalizeCachePathForFs( const char *cachePath, char *outPath, const size_t outPathCount ) {
	/**
	*	Require writable destination storage and a non-empty input path before normalizing.
	**/
	if ( !cachePath || cachePath[ 0 ] == '\0' || !outPath || outPathCount == 0 ) {
		return false;
	}

	/**
	*	Trim optional explicit game-directory prefix so writes land under fs_gamedir exactly once.
	**/
	const char *pathStart = cachePath;
	const size_t gameDirPrefixLength = std::strlen( NAV3_SERIALIZED_GAME_DIR_PREFIX );
	if ( Q_strncasecmp( pathStart, NAV3_SERIALIZED_GAME_DIR_PREFIX, gameDirPrefixLength ) == 0 ) {
		pathStart += gameDirPrefixLength;
	}

	/**
	*	Copy path into fixed storage and normalize separators before filesystem IO.
	**/
	if ( Q_strlcpy( outPath, pathStart, outPathCount ) >= outPathCount || outPath[ 0 ] == '\0' ) {
		if ( outPathCount > 0 ) {
			outPath[ 0 ] = '\0';
		}
		return false;
	}

	for ( size_t i = 0; outPath[ i ] != '\0'; i++ ) {
		if ( outPath[ i ] == '\\' ) {
			outPath[ i ] = '/';
		}
	}

	return true;
}

/**
*	@brief	Append one little-endian `uint32_t` value to a byte vector.
**/
static void Nav3_Serialize_AppendU32LE( std::vector<uint8_t> *bytes, const uint32_t value ) {
	if ( !bytes ) {
		return;
	}
	bytes->push_back( static_cast<uint8_t>( value & 0xFFu ) );
	bytes->push_back( static_cast<uint8_t>( ( value >> 8 ) & 0xFFu ) );
	bytes->push_back( static_cast<uint8_t>( ( value >> 16 ) & 0xFFu ) );
	bytes->push_back( static_cast<uint8_t>( ( value >> 24 ) & 0xFFu ) );
}

/**
*	@brief	Append one little-endian `int32_t` value to a byte vector.
**/
static void Nav3_Serialize_AppendI32LE( std::vector<uint8_t> *bytes, const int32_t value ) {
	Nav3_Serialize_AppendU32LE( bytes, static_cast<uint32_t>( value ) );
}

/**
*	@brief	Append one little-endian `uint64_t` value to a byte vector.
**/
static void Nav3_Serialize_AppendU64LE( std::vector<uint8_t> *bytes, const uint64_t value ) {
	if ( !bytes ) {
		return;
	}
	for ( int32_t shift = 0; shift < 64; shift += 8 ) {
		bytes->push_back( static_cast<uint8_t>( ( value >> shift ) & 0xFFu ) );
	}
}

/**
*	@brief	Append one little-endian float bit-pattern to a byte vector.
**/
static void Nav3_Serialize_AppendF32LE( std::vector<uint8_t> *bytes, const float value ) {
	uint32_t bits = 0;
	std::memcpy( &bits, &value, sizeof( bits ) );
	Nav3_Serialize_AppendU32LE( bytes, bits );
}

/**
*	@brief	Append one vector payload as three little-endian floats.
**/
static void Nav3_Serialize_AppendVector3LE( std::vector<uint8_t> *bytes, const Vector3 &value ) {
	Nav3_Serialize_AppendF32LE( bytes, value.x );
	Nav3_Serialize_AppendF32LE( bytes, value.y );
	Nav3_Serialize_AppendF32LE( bytes, value.z );
}

/**
*	@brief	Read one little-endian `uint32_t` from a bounded blob reader.
**/
static uint32_t Nav3_Serialize_ReadU32LE( nav3_serialized_blob_reader_t *reader ) {
	if ( !reader || !reader->data || ( reader->offset + sizeof( uint32_t ) ) > reader->size ) {
		if ( reader ) {
			reader->failed = true;
		}
		return 0;
	}

	const uint8_t *ptr = reader->data + reader->offset;
	const uint32_t value = static_cast<uint32_t>( ptr[ 0 ] )
		| ( static_cast<uint32_t>( ptr[ 1 ] ) << 8 )
		| ( static_cast<uint32_t>( ptr[ 2 ] ) << 16 )
		| ( static_cast<uint32_t>( ptr[ 3 ] ) << 24 );
	reader->offset += sizeof( uint32_t );
	return value;
}

/**
*	@brief	Read one little-endian `int32_t` from a bounded blob reader.
**/
static int32_t Nav3_Serialize_ReadI32LE( nav3_serialized_blob_reader_t *reader ) {
	return static_cast<int32_t>( Nav3_Serialize_ReadU32LE( reader ) );
}

/**
*	@brief	Read one little-endian `uint64_t` from a bounded blob reader.
**/
static uint64_t Nav3_Serialize_ReadU64LE( nav3_serialized_blob_reader_t *reader ) {
	if ( !reader || !reader->data || ( reader->offset + sizeof( uint64_t ) ) > reader->size ) {
		if ( reader ) {
			reader->failed = true;
		}
		return 0;
	}

	uint64_t value = 0;
	for ( int32_t shift = 0; shift < 64; shift += 8 ) {
		value |= ( static_cast<uint64_t>( reader->data[ reader->offset++ ] ) << shift );
	}
	return value;
}

/**
*	@brief	Read one little-endian float from a bounded blob reader.
**/
static float Nav3_Serialize_ReadF32LE( nav3_serialized_blob_reader_t *reader ) {
	const uint32_t bits = Nav3_Serialize_ReadU32LE( reader );
	float value = 0.0f;
	std::memcpy( &value, &bits, sizeof( value ) );
	return value;
}

/**
*	@brief	Read one vector payload encoded as three little-endian floats.
**/
static Vector3 Nav3_Serialize_ReadVector3LE( nav3_serialized_blob_reader_t *reader ) {
	Vector3 value = {};
	value.x = Nav3_Serialize_ReadF32LE( reader );
	value.y = Nav3_Serialize_ReadF32LE( reader );
	value.z = Nav3_Serialize_ReadF32LE( reader );
	return value;
}

/**
*	@brief	Append canonical build-config payload bytes.
**/
static void Nav3_Serialize_AppendBuildConfigBytes( std::vector<uint8_t> *bytes, const nav3_build_config_t &buildConfig ) {
	Nav3_Serialize_AppendF32LE( bytes, buildConfig.cell_size );
	Nav3_Serialize_AppendF32LE( bytes, buildConfig.cell_height );
	Nav3_Serialize_AppendF32LE( bytes, buildConfig.agent_height_units );
	Nav3_Serialize_AppendF32LE( bytes, buildConfig.agent_step_height_units );
	Nav3_Serialize_AppendF32LE( bytes, buildConfig.agent_radius_units );
	Nav3_Serialize_AppendU32LE( bytes, static_cast<uint32_t>( buildConfig.walkable_height_cells ) );
	Nav3_Serialize_AppendU32LE( bytes, static_cast<uint32_t>( buildConfig.walkable_climb_cells ) );
	Nav3_Serialize_AppendU32LE( bytes, static_cast<uint32_t>( buildConfig.walkable_radius_cells ) );
}

/**
*	@brief	Read canonical build-config payload bytes.
**/
static const bool Nav3_Serialize_ReadBuildConfigBytes( nav3_serialized_blob_reader_t *reader, nav3_build_config_t *outBuildConfig ) {
	if ( !reader || !outBuildConfig ) {
		return false;
	}

	nav3_build_config_t buildConfig = {};
	buildConfig.cell_size = Nav3_Serialize_ReadF32LE( reader );
	buildConfig.cell_height = Nav3_Serialize_ReadF32LE( reader );
	buildConfig.agent_height_units = Nav3_Serialize_ReadF32LE( reader );
	buildConfig.agent_step_height_units = Nav3_Serialize_ReadF32LE( reader );
	buildConfig.agent_radius_units = Nav3_Serialize_ReadF32LE( reader );
	buildConfig.walkable_height_cells = static_cast<int32_t>( Nav3_Serialize_ReadU32LE( reader ) );
	buildConfig.walkable_climb_cells = static_cast<int32_t>( Nav3_Serialize_ReadU32LE( reader ) );
	buildConfig.walkable_radius_cells = static_cast<int32_t>( Nav3_Serialize_ReadU32LE( reader ) );
	if ( reader->failed ) {
		return false;
	}

	*outBuildConfig = buildConfig;
	return true;
}

/**
*	@brief	Encode one nav3 serialized header to canonical bytes.
*	@param	header	Header value to encode.
*	@param	zeroChecksum	When true, encodes `header_checksum` as zero.
*	@param	outBytes	[out] Encoded bytes.
**/
static void Nav3_Serialize_EncodeHeaderBytes( const nav3_serialized_header_t &header, const bool zeroChecksum, std::vector<uint8_t> *outBytes ) {
	if ( !outBytes ) {
		return;
	}

	Nav3_Serialize_AppendU32LE( outBytes, header.magic );
	Nav3_Serialize_AppendU32LE( outBytes, header.format_version );
	Nav3_Serialize_AppendU32LE( outBytes, header.type_schema_version );
	Nav3_Serialize_AppendU32LE( outBytes, header.endian_marker );
	Nav3_Serialize_AppendU64LE( outBytes, header.map_identity );
	Nav3_Serialize_AppendU32LE( outBytes, header.build_settings_hash );
	Nav3_Serialize_AppendU32LE( outBytes, header.section_count );
	Nav3_Serialize_AppendU32LE( outBytes, header.section_table_offset );
	Nav3_Serialize_AppendU32LE( outBytes, header.section_table_size );
	Nav3_Serialize_AppendU32LE( outBytes, header.payload_offset );
	Nav3_Serialize_AppendU32LE( outBytes, header.payload_size );
	Nav3_Serialize_AppendU32LE( outBytes, zeroChecksum ? 0u : header.header_checksum );
	Nav3_Serialize_AppendU32LE( outBytes, header.payload_checksum );
}

/**
*	@brief	Decode one nav3 serialized header from canonical bytes.
**/
static const bool Nav3_Serialize_DecodeHeaderBytes( nav3_serialized_blob_reader_t *reader, nav3_serialized_header_t *outHeader ) {
	if ( !reader || !outHeader ) {
		return false;
	}

	nav3_serialized_header_t header = {};
	header.magic = Nav3_Serialize_ReadU32LE( reader );
	header.format_version = Nav3_Serialize_ReadU32LE( reader );
	header.type_schema_version = Nav3_Serialize_ReadU32LE( reader );
	header.endian_marker = Nav3_Serialize_ReadU32LE( reader );
	header.map_identity = Nav3_Serialize_ReadU64LE( reader );
	header.build_settings_hash = Nav3_Serialize_ReadU32LE( reader );
	header.section_count = Nav3_Serialize_ReadU32LE( reader );
	header.section_table_offset = Nav3_Serialize_ReadU32LE( reader );
	header.section_table_size = Nav3_Serialize_ReadU32LE( reader );
	header.payload_offset = Nav3_Serialize_ReadU32LE( reader );
	header.payload_size = Nav3_Serialize_ReadU32LE( reader );
	header.header_checksum = Nav3_Serialize_ReadU32LE( reader );
	header.payload_checksum = Nav3_Serialize_ReadU32LE( reader );
	if ( reader->failed ) {
		return false;
	}

	*outHeader = header;
	return true;
}

/**
*	@brief	Encode one section descriptor to canonical bytes.
**/
static void Nav3_Serialize_EncodeSectionDescBytes( const nav3_serialized_section_desc_t &section, std::vector<uint8_t> *outBytes ) {
	if ( !outBytes ) {
		return;
	}

	Nav3_Serialize_AppendU32LE( outBytes, static_cast<uint32_t>( section.type ) );
	Nav3_Serialize_AppendU32LE( outBytes, section.version );
	Nav3_Serialize_AppendU32LE( outBytes, section.flags );
	Nav3_Serialize_AppendU32LE( outBytes, section.offset );
	Nav3_Serialize_AppendU32LE( outBytes, section.size );
	Nav3_Serialize_AppendU32LE( outBytes, section.checksum );
}

/**
*	@brief	Decode one section descriptor from canonical bytes.
**/
static const bool Nav3_Serialize_DecodeSectionDescBytes( nav3_serialized_blob_reader_t *reader, nav3_serialized_section_desc_t *outSection ) {
	if ( !reader || !outSection ) {
		return false;
	}

	nav3_serialized_section_desc_t section = {};
	section.type = static_cast<nav3_serialized_section_type_t>( Nav3_Serialize_ReadU32LE( reader ) );
	section.version = Nav3_Serialize_ReadU32LE( reader );
	section.flags = Nav3_Serialize_ReadU32LE( reader );
	section.offset = Nav3_Serialize_ReadU32LE( reader );
	section.size = Nav3_Serialize_ReadU32LE( reader );
	section.checksum = Nav3_Serialize_ReadU32LE( reader );
	if ( reader->failed ) {
		return false;
	}

	*outSection = section;
	return true;
}

/**
*	@brief	Return encoded byte size of one header payload.
**/
static constexpr uint32_t Nav3_Serialize_HeaderByteCount( void ) {
	return sizeof( uint32_t ) * 12 + sizeof( uint64_t );
}

/**
*	@brief	Return encoded byte size of one section descriptor payload.
**/
static constexpr uint32_t Nav3_Serialize_SectionDescByteCount( void ) {
	return sizeof( uint32_t ) * 6;
}

/**
*	@brief	Count spans across all sparse generated columns.
**/
static uint32_t Nav3_Serialize_CountSpans( const nav3_generated_mesh_t &generatedMesh ) {
	uint64_t total = 0;
	for ( const nav3_generated_column_t &column : generatedMesh.columns ) {
		total += static_cast<uint64_t>( column.spans.size() );
	}
	if ( total > std::numeric_limits<uint32_t>::max() ) {
		return 0;
	}
	return static_cast<uint32_t>( total );
}

/**
*	@brief	Return true when lhs column should sort before rhs column.
**/
static const bool Nav3_Serialize_ColumnSortLess( const nav3_generated_column_t &lhs, const nav3_generated_column_t &rhs ) {
	if ( lhs.cell_x != rhs.cell_x ) {
		return lhs.cell_x < rhs.cell_x;
	}
	return lhs.cell_y < rhs.cell_y;
}

/**
*	@brief	Return true when lhs span should sort before rhs span.
**/
static const bool Nav3_Serialize_SpanSortLess( const nav3_generated_span_t &lhs, const nav3_generated_span_t &rhs ) {
	if ( lhs.floor_z != rhs.floor_z ) {
		return lhs.floor_z < rhs.floor_z;
	}
	if ( lhs.ceiling_z != rhs.ceiling_z ) {
		return lhs.ceiling_z < rhs.ceiling_z;
	}
	if ( lhs.span_id != rhs.span_id ) {
		return lhs.span_id < rhs.span_id;
	}
	if ( lhs.source_sample_coord.x != rhs.source_sample_coord.x ) {
		return lhs.source_sample_coord.x < rhs.source_sample_coord.x;
	}
	if ( lhs.source_sample_coord.y != rhs.source_sample_coord.y ) {
		return lhs.source_sample_coord.y < rhs.source_sample_coord.y;
	}
	if ( lhs.source_sample_coord.z != rhs.source_sample_coord.z ) {
		return lhs.source_sample_coord.z < rhs.source_sample_coord.z;
	}
	return lhs.classification_bits < rhs.classification_bits;
}

/**
*	@brief	Build a canonicalized generated mesh copy for deterministic serialization.
**/
static nav3_generated_mesh_t Nav3_Serialize_CanonicalizeGeneratedMesh( const nav3_generated_mesh_t &generatedMesh ) {
	nav3_generated_mesh_t canonical = generatedMesh;

	for ( nav3_generated_column_t &column : canonical.columns ) {
		std::sort( column.spans.begin(), column.spans.end(), Nav3_Serialize_SpanSortLess );
	}
	std::sort( canonical.columns.begin(), canonical.columns.end(), Nav3_Serialize_ColumnSortLess );

	return canonical;
}

/**
*	@brief	Append canonical generated-mesh section payload bytes.
**/
static void Nav3_Serialize_AppendGeneratedMeshSectionBytes( const nav3_generated_mesh_t &generatedMesh, std::vector<uint8_t> *outBytes ) {
	if ( !outBytes ) {
		return;
	}

	const uint32_t columnCount = static_cast<uint32_t>( generatedMesh.columns.size() );
	const uint32_t spanCount = Nav3_Serialize_CountSpans( generatedMesh );

	Nav3_Serialize_AppendVector3LE( outBytes, generatedMesh.world_bounds.mins );
	Nav3_Serialize_AppendVector3LE( outBytes, generatedMesh.world_bounds.maxs );
	Nav3_Serialize_AppendF32LE( outBytes, generatedMesh.cell_size );
	Nav3_Serialize_AppendF32LE( outBytes, generatedMesh.cell_height );
	Nav3_Serialize_AppendI32LE( outBytes, generatedMesh.walkable_height_cells );
	Nav3_Serialize_AppendI32LE( outBytes, generatedMesh.walkable_climb_cells );
	Nav3_Serialize_AppendI32LE( outBytes, generatedMesh.walkable_radius_cells );
	Nav3_Serialize_AppendU32LE( outBytes, columnCount );
	Nav3_Serialize_AppendU32LE( outBytes, spanCount );

	for ( const nav3_generated_column_t &column : generatedMesh.columns ) {
		Nav3_Serialize_AppendI32LE( outBytes, column.cell_x );
		Nav3_Serialize_AppendI32LE( outBytes, column.cell_y );
		Nav3_Serialize_AppendU32LE( outBytes, static_cast<uint32_t>( column.spans.size() ) );
	}

	for ( const nav3_generated_column_t &column : generatedMesh.columns ) {
		for ( const nav3_generated_span_t &span : column.spans ) {
			Nav3_Serialize_AppendI32LE( outBytes, span.span_id );
			Nav3_Serialize_AppendF32LE( outBytes, span.floor_z );
			Nav3_Serialize_AppendF32LE( outBytes, span.ceiling_z );
			Nav3_Serialize_AppendF32LE( outBytes, span.floor_normal_z );
			Nav3_Serialize_AppendF32LE( outBytes, span.clearance );
			Nav3_Serialize_AppendI32LE( outBytes, span.leaf_id );
			Nav3_Serialize_AppendI32LE( outBytes, span.cluster_id );
			Nav3_Serialize_AppendI32LE( outBytes, span.area_id );
			Nav3_Serialize_AppendU32LE( outBytes, span.contents_flags );
			Nav3_Serialize_AppendU32LE( outBytes, span.classification_bits );
			Nav3_Serialize_AppendU32LE( outBytes, span.filtered_out ? 1u : 0u );
			Nav3_Serialize_AppendU32LE( outBytes, static_cast<uint32_t>( span.filtered_pass ) );
			Nav3_Serialize_AppendVector3LE( outBytes, span.source_sample_coord );
			Nav3_Serialize_AppendVector3LE( outBytes, span.source_standing_coord );
		}
	}
}

/**
*	@brief	Rebuild bounded generated-mesh stats after decoding from serialized bytes.
**/
static void Nav3_Serialize_RebuildGeneratedMeshStats( nav3_generated_mesh_t *inOutGeneratedMesh ) {
	if ( !inOutGeneratedMesh ) {
		return;
	}

	nav3_generation_stats_t stats = {};
	stats.generation_succeeded = !inOutGeneratedMesh->columns.empty();
	stats.sampled_columns = static_cast<int32_t>( inOutGeneratedMesh->columns.size() );
	stats.emitted_columns = static_cast<int32_t>( inOutGeneratedMesh->columns.size() );
	stats.emitted_spans = static_cast<int32_t>( Nav3_Serialize_CountSpans( *inOutGeneratedMesh ) );
	stats.filter_input_spans = stats.emitted_spans;
	stats.filter_output_spans = stats.emitted_spans;

	for ( const nav3_generated_column_t &column : inOutGeneratedMesh->columns ) {
		stats.max_spans_in_column = std::max( stats.max_spans_in_column, static_cast<int32_t>( column.spans.size() ) );
		for ( const nav3_generated_span_t &span : column.spans ) {
			if ( ( span.classification_bits & NAV3_SPAN_CLASS_WATER ) != 0 ) {
				stats.spans_with_water++;
			}
			if ( ( span.classification_bits & NAV3_SPAN_CLASS_SLIME ) != 0 ) {
				stats.spans_with_slime++;
			}
			if ( ( span.classification_bits & NAV3_SPAN_CLASS_LAVA ) != 0 ) {
				stats.spans_with_lava++;
			}
			if ( ( span.classification_bits & NAV3_SPAN_CLASS_LADDER ) != 0 ) {
				stats.spans_with_ladder++;
			}
			if ( ( span.classification_bits & NAV3_SPAN_CLASS_STAIRS ) != 0 ) {
				stats.spans_with_stairs++;
			}
			if ( ( span.classification_bits & NAV3_SPAN_CLASS_LEDGE ) != 0 ) {
				stats.spans_with_ledge++;
			}
		}
	}

	inOutGeneratedMesh->stats = stats;
}

/**
*	@brief	Decode one generated-mesh section payload into runtime sparse mesh storage.
**/
static const bool Nav3_Serialize_ReadGeneratedMeshSectionBytes( const uint8_t *sectionData,
	const size_t sectionSize,
	nav3_generated_mesh_t *outGeneratedMesh ) {
	if ( !sectionData || sectionSize == 0 || !outGeneratedMesh ) {
		return false;
	}

	nav3_serialized_blob_reader_t reader = {};
	reader.data = sectionData;
	reader.size = sectionSize;

	nav3_generated_mesh_t generatedMesh = {};
	generatedMesh.world_bounds.mins = Nav3_Serialize_ReadVector3LE( &reader );
	generatedMesh.world_bounds.maxs = Nav3_Serialize_ReadVector3LE( &reader );
	generatedMesh.cell_size = Nav3_Serialize_ReadF32LE( &reader );
	generatedMesh.cell_height = Nav3_Serialize_ReadF32LE( &reader );
	generatedMesh.walkable_height_cells = Nav3_Serialize_ReadI32LE( &reader );
	generatedMesh.walkable_climb_cells = Nav3_Serialize_ReadI32LE( &reader );
	generatedMesh.walkable_radius_cells = Nav3_Serialize_ReadI32LE( &reader );
	const uint32_t columnCount = Nav3_Serialize_ReadU32LE( &reader );
	const uint32_t spanCount = Nav3_Serialize_ReadU32LE( &reader );

	if ( reader.failed ) {
		return false;
	}

	generatedMesh.columns.clear();
	generatedMesh.columns.reserve( columnCount );

	uint64_t expectedSpanCount = 0;
	for ( uint32_t i = 0; i < columnCount; i++ ) {
		nav3_generated_column_t column = {};
		column.cell_x = Nav3_Serialize_ReadI32LE( &reader );
		column.cell_y = Nav3_Serialize_ReadI32LE( &reader );
		const uint32_t columnSpanCount = Nav3_Serialize_ReadU32LE( &reader );
		if ( reader.failed ) {
			return false;
		}

		expectedSpanCount += static_cast<uint64_t>( columnSpanCount );
		if ( expectedSpanCount > static_cast<uint64_t>( spanCount ) ) {
			return false;
		}

		column.spans.reserve( columnSpanCount );
		generatedMesh.columns.push_back( std::move( column ) );
	}

	if ( expectedSpanCount != static_cast<uint64_t>( spanCount ) ) {
		return false;
	}

	for ( nav3_generated_column_t &column : generatedMesh.columns ) {
		const size_t columnSpanCount = column.spans.capacity();
		for ( size_t spanIndex = 0; spanIndex < columnSpanCount; spanIndex++ ) {
			nav3_generated_span_t span = {};
			span.span_id = Nav3_Serialize_ReadI32LE( &reader );
			span.floor_z = Nav3_Serialize_ReadF32LE( &reader );
			span.ceiling_z = Nav3_Serialize_ReadF32LE( &reader );
			span.floor_normal_z = Nav3_Serialize_ReadF32LE( &reader );
			span.clearance = Nav3_Serialize_ReadF32LE( &reader );
			span.leaf_id = Nav3_Serialize_ReadI32LE( &reader );
			span.cluster_id = Nav3_Serialize_ReadI32LE( &reader );
			span.area_id = Nav3_Serialize_ReadI32LE( &reader );
			span.contents_flags = Nav3_Serialize_ReadU32LE( &reader );
			span.classification_bits = Nav3_Serialize_ReadU32LE( &reader );
			span.filtered_out = ( Nav3_Serialize_ReadU32LE( &reader ) != 0u );
			span.filtered_pass = static_cast<nav3_generation_filter_pass_t>( Nav3_Serialize_ReadU32LE( &reader ) );
			if ( static_cast<uint32_t>( span.filtered_pass ) >= static_cast<uint32_t>( nav3_generation_filter_pass_t::Count ) ) {
				span.filtered_pass = nav3_generation_filter_pass_t::None;
			}
			span.source_sample_coord = Nav3_Serialize_ReadVector3LE( &reader );
			span.source_standing_coord = Nav3_Serialize_ReadVector3LE( &reader );
			if ( reader.failed ) {
				return false;
			}

			column.spans.push_back( span );
		}
	}

	if ( reader.offset != reader.size ) {
		return false;
	}

	Nav3_Serialize_RebuildGeneratedMeshStats( &generatedMesh );
	*outGeneratedMesh = std::move( generatedMesh );
	return true;
}

/**
*	@brief	Return section descriptor for a specific section type.
*	@param	sectionTable	Parsed section table.
*	@param	type	Section type to locate.
*	@return	Pointer to section descriptor when found, otherwise `nullptr`.
**/
static const nav3_serialized_section_desc_t *Nav3_Serialize_FindSectionDesc( const std::vector<nav3_serialized_section_desc_t> &sectionTable,
	const nav3_serialized_section_type_t type ) {
	for ( const nav3_serialized_section_desc_t &section : sectionTable ) {
		if ( section.type == type ) {
			return &section;
		}
	}
	return nullptr;
}


/**
*
*
*    Nav3 Serialization Public API:
*
*
**/
/**
*	@brief	Build a stable map identity hash from a map name string.
*	@param	mapName	Map name to hash.
*	@return	Stable map identity token.
**/
uint64_t SVG_Nav3_Serialize_BuildMapIdentity( const char *mapName ) {
	const char *normalizedMapName = ( mapName && mapName[ 0 ] != '\0' ) ? mapName : "unknown";
	const size_t mapNameLength = std::strlen( normalizedMapName );
	const uint32_t low = SVG_Nav3_ChecksumFNV1a32( normalizedMapName, mapNameLength );

	std::vector<uint8_t> salted = {};
	salted.reserve( mapNameLength + 3 );
	for ( size_t i = 0; i < mapNameLength; i++ ) {
		salted.push_back( static_cast<uint8_t>( normalizedMapName[ i ] ) );
	}
	salted.push_back( 'n' );
	salted.push_back( 'a' );
	salted.push_back( 'v' );
	const uint32_t high = SVG_Nav3_ChecksumFNV1a32( salted.data(), salted.size() );
	return ( static_cast<uint64_t>( high ) << 32 ) | static_cast<uint64_t>( low );
}

/**
*	@brief	Build a stable build-settings hash from nav3 build config values.
*	@param	buildConfig	Build config to hash.
*	@return	Stable build settings hash.
**/
uint32_t SVG_Nav3_Serialize_BuildConfigHash( const nav3_build_config_t &buildConfig ) {
	std::vector<uint8_t> buildConfigBytes = {};
	buildConfigBytes.reserve( sizeof( uint32_t ) * 8 );
	Nav3_Serialize_AppendBuildConfigBytes( &buildConfigBytes, buildConfig );
	return SVG_Nav3_ChecksumFNV1a32( buildConfigBytes.data(), buildConfigBytes.size() );
}

/**
*	@brief	Build a deterministic checksum from canonicalized generated mesh payload fields.
*	@param	generatedMesh	Generated mesh to hash.
*	@return	Deterministic generated mesh checksum.
**/
uint32_t SVG_Nav3_Serialize_BuildGeneratedMeshChecksum( const nav3_generated_mesh_t &generatedMesh ) {
	const nav3_generated_mesh_t canonical = Nav3_Serialize_CanonicalizeGeneratedMesh( generatedMesh );
	std::vector<uint8_t> generatedPayloadBytes = {};
	generatedPayloadBytes.reserve( canonical.columns.size() * 64 );
	Nav3_Serialize_AppendGeneratedMeshSectionBytes( canonical, &generatedPayloadBytes );
	return SVG_Nav3_ChecksumFNV1a32( generatedPayloadBytes.data(), generatedPayloadBytes.size() );
}

/**
*	@brief	Build a stage-4 empty nav3 asset blob containing header, section table, and build-config section.
*	@param	mapName	Map identity source string.
*	@param	buildConfig	Build config payload to persist.
*	@param	outBlob	[out] Destination blob.
*	@return	Structured serialization result.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_BuildEmptyAssetBlob( const char *mapName,
	const nav3_build_config_t &buildConfig,
	nav3_serialized_blob_t *outBlob ) {
	if ( !outBlob ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	outBlob->bytes.clear();

	// Build the canonical build-config section payload first so offsets and checksums are deterministic.
	std::vector<uint8_t> buildConfigPayload = {};
	buildConfigPayload.reserve( sizeof( uint32_t ) * 8 );
	Nav3_Serialize_AppendBuildConfigBytes( &buildConfigPayload, buildConfig );

	const uint32_t sectionCount = 1;
	const uint32_t headerByteCount = Nav3_Serialize_HeaderByteCount();
	const uint32_t sectionTableByteCount = sectionCount * Nav3_Serialize_SectionDescByteCount();
	const uint32_t payloadOffset = headerByteCount + sectionTableByteCount;
	const uint32_t payloadByteCount = static_cast<uint32_t>( buildConfigPayload.size() );

	nav3_serialized_section_desc_t buildConfigSection = {};
	buildConfigSection.type = nav3_serialized_section_type_t::BuildConfig;
	buildConfigSection.version = NAV3_SERIALIZED_SECTION_VERSION_BUILD_CONFIG;
	buildConfigSection.flags = NAV3_SERIALIZED_SECTION_FLAG_NONE;
	buildConfigSection.offset = payloadOffset;
	buildConfigSection.size = payloadByteCount;
	buildConfigSection.checksum = SVG_Nav3_ChecksumFNV1a32( buildConfigPayload.data(), buildConfigPayload.size() );

	nav3_serialized_header_t header = {};
	header.magic = NAV3_SERIALIZED_FILE_MAGIC;
	header.format_version = NAV3_SERIALIZED_FORMAT_VERSION;
	header.type_schema_version = NAV3_TYPE_SCHEMA_VERSION;
	header.endian_marker = NAV3_SERIALIZED_ENDIAN_MARKER;
	header.map_identity = SVG_Nav3_Serialize_BuildMapIdentity( mapName );
	header.build_settings_hash = SVG_Nav3_Serialize_BuildConfigHash( buildConfig );
	header.section_count = sectionCount;
	header.section_table_offset = headerByteCount;
	header.section_table_size = sectionTableByteCount;
	header.payload_offset = payloadOffset;
	header.payload_size = payloadByteCount;
	header.payload_checksum = 0;
	header.header_checksum = 0;

	// Compute payload checksum across section table + payload bytes.
	std::vector<uint8_t> sectionTableBytes = {};
	sectionTableBytes.reserve( sectionTableByteCount );
	Nav3_Serialize_EncodeSectionDescBytes( buildConfigSection, &sectionTableBytes );
	std::vector<uint8_t> payloadChecksumBytes = {};
	payloadChecksumBytes.reserve( sectionTableBytes.size() + buildConfigPayload.size() );
	payloadChecksumBytes.insert( payloadChecksumBytes.end(), sectionTableBytes.begin(), sectionTableBytes.end() );
	payloadChecksumBytes.insert( payloadChecksumBytes.end(), buildConfigPayload.begin(), buildConfigPayload.end() );
	header.payload_checksum = SVG_Nav3_ChecksumFNV1a32( payloadChecksumBytes.data(), payloadChecksumBytes.size() );

	// Compute header checksum with checksum field forced to zero.
	std::vector<uint8_t> headerChecksumBytes = {};
	headerChecksumBytes.reserve( headerByteCount );
	Nav3_Serialize_EncodeHeaderBytes( header, true, &headerChecksumBytes );
	header.header_checksum = SVG_Nav3_ChecksumFNV1a32( headerChecksumBytes.data(), headerChecksumBytes.size() );

	// Assemble final file bytes in one deterministic layout.
	outBlob->bytes.reserve( headerByteCount + sectionTableByteCount + payloadByteCount );
	Nav3_Serialize_EncodeHeaderBytes( header, false, &outBlob->bytes );
	outBlob->bytes.insert( outBlob->bytes.end(), sectionTableBytes.begin(), sectionTableBytes.end() );
	outBlob->bytes.insert( outBlob->bytes.end(), buildConfigPayload.begin(), buildConfigPayload.end() );

	nav3_serialization_result_t result = {};
	result.success = true;
	result.validation.compatibility = nav3_serialized_compatibility_t::Accept;
	result.validation.error = nav3_serialized_error_t::None;
	result.byte_count = static_cast<uint32_t>( outBlob->bytes.size() );
	return result;
}

/**
*	@brief	Build a stage-7 generated nav3 asset blob containing build-config and generated-mesh sections.
*	@param	mapName	Map identity source string.
*	@param	buildConfig	Build config payload to persist.
*	@param	generatedMesh	Generated sparse mesh payload to persist.
*	@param	outBlob	[out] Destination blob.
*	@return	Structured serialization result.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_BuildGeneratedAssetBlob( const char *mapName,
	const nav3_build_config_t &buildConfig,
	const nav3_generated_mesh_t &generatedMesh,
	nav3_serialized_blob_t *outBlob ) {
	if ( !outBlob ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	outBlob->bytes.clear();

	const nav3_generated_mesh_t canonicalMesh = Nav3_Serialize_CanonicalizeGeneratedMesh( generatedMesh );

	nav3_serialized_section_payload_t buildConfigSection = {};
	buildConfigSection.type = nav3_serialized_section_type_t::BuildConfig;
	buildConfigSection.version = NAV3_SERIALIZED_SECTION_VERSION_BUILD_CONFIG;
	buildConfigSection.flags = NAV3_SERIALIZED_SECTION_FLAG_NONE;
	buildConfigSection.bytes.reserve( sizeof( uint32_t ) * 8 );
	Nav3_Serialize_AppendBuildConfigBytes( &buildConfigSection.bytes, buildConfig );

	nav3_serialized_section_payload_t generatedMeshSection = {};
	generatedMeshSection.type = nav3_serialized_section_type_t::GeneratedMesh;
	generatedMeshSection.version = NAV3_SERIALIZED_SECTION_VERSION_GENERATED_MESH;
	generatedMeshSection.flags = NAV3_SERIALIZED_SECTION_FLAG_NONE;
	generatedMeshSection.bytes.reserve( canonicalMesh.columns.size() * 64 );
	Nav3_Serialize_AppendGeneratedMeshSectionBytes( canonicalMesh, &generatedMeshSection.bytes );

	std::vector<nav3_serialized_section_payload_t> sections = {};
	sections.reserve( 2 );
	sections.push_back( std::move( buildConfigSection ) );
	sections.push_back( std::move( generatedMeshSection ) );

	std::sort( sections.begin(), sections.end(), []( const nav3_serialized_section_payload_t &lhs,
		const nav3_serialized_section_payload_t &rhs ) {
		return static_cast<uint32_t>( lhs.type ) < static_cast<uint32_t>( rhs.type );
	} );

	const uint32_t sectionCount = static_cast<uint32_t>( sections.size() );
	const uint32_t headerByteCount = Nav3_Serialize_HeaderByteCount();
	const uint32_t sectionTableByteCount = sectionCount * Nav3_Serialize_SectionDescByteCount();
	const uint32_t payloadOffset = headerByteCount + sectionTableByteCount;

	std::vector<nav3_serialized_section_desc_t> sectionTable = {};
	sectionTable.reserve( sectionCount );
	uint32_t sectionPayloadOffset = payloadOffset;
	for ( const nav3_serialized_section_payload_t &section : sections ) {
		nav3_serialized_section_desc_t sectionDesc = {};
		sectionDesc.type = section.type;
		sectionDesc.version = section.version;
		sectionDesc.flags = section.flags;
		sectionDesc.offset = sectionPayloadOffset;
		sectionDesc.size = static_cast<uint32_t>( section.bytes.size() );
		sectionDesc.checksum = SVG_Nav3_ChecksumFNV1a32( section.bytes.data(), section.bytes.size() );
		sectionTable.push_back( sectionDesc );
		sectionPayloadOffset += sectionDesc.size;
	}

	nav3_serialized_header_t header = {};
	header.magic = NAV3_SERIALIZED_FILE_MAGIC;
	header.format_version = NAV3_SERIALIZED_FORMAT_VERSION;
	header.type_schema_version = NAV3_TYPE_SCHEMA_VERSION;
	header.endian_marker = NAV3_SERIALIZED_ENDIAN_MARKER;
	header.map_identity = SVG_Nav3_Serialize_BuildMapIdentity( mapName );
	header.build_settings_hash = SVG_Nav3_Serialize_BuildConfigHash( buildConfig );
	header.section_count = sectionCount;
	header.section_table_offset = headerByteCount;
	header.section_table_size = sectionTableByteCount;
	header.payload_offset = payloadOffset;
	header.payload_size = sectionPayloadOffset - payloadOffset;
	header.payload_checksum = 0;
	header.header_checksum = 0;

	std::vector<uint8_t> sectionTableBytes = {};
	sectionTableBytes.reserve( sectionTableByteCount );
	for ( const nav3_serialized_section_desc_t &sectionDesc : sectionTable ) {
		Nav3_Serialize_EncodeSectionDescBytes( sectionDesc, &sectionTableBytes );
	}

	std::vector<uint8_t> payloadChecksumBytes = {};
	payloadChecksumBytes.reserve( sectionTableBytes.size() + header.payload_size );
	payloadChecksumBytes.insert( payloadChecksumBytes.end(), sectionTableBytes.begin(), sectionTableBytes.end() );
	for ( const nav3_serialized_section_payload_t &section : sections ) {
		payloadChecksumBytes.insert( payloadChecksumBytes.end(), section.bytes.begin(), section.bytes.end() );
	}
	header.payload_checksum = SVG_Nav3_ChecksumFNV1a32( payloadChecksumBytes.data(), payloadChecksumBytes.size() );

	std::vector<uint8_t> headerChecksumBytes = {};
	headerChecksumBytes.reserve( headerByteCount );
	Nav3_Serialize_EncodeHeaderBytes( header, true, &headerChecksumBytes );
	header.header_checksum = SVG_Nav3_ChecksumFNV1a32( headerChecksumBytes.data(), headerChecksumBytes.size() );

	outBlob->bytes.reserve( headerByteCount + sectionTableByteCount + header.payload_size );
	Nav3_Serialize_EncodeHeaderBytes( header, false, &outBlob->bytes );
	outBlob->bytes.insert( outBlob->bytes.end(), sectionTableBytes.begin(), sectionTableBytes.end() );
	for ( const nav3_serialized_section_payload_t &section : sections ) {
		outBlob->bytes.insert( outBlob->bytes.end(), section.bytes.begin(), section.bytes.end() );
	}

	nav3_serialization_result_t result = {};
	result.success = true;
	result.validation.compatibility = nav3_serialized_compatibility_t::Accept;
	result.validation.error = nav3_serialized_error_t::None;
	result.byte_count = static_cast<uint32_t>( outBlob->bytes.size() );
	result.generated_column_count = static_cast<uint32_t>( canonicalMesh.columns.size() );
	result.generated_span_count = Nav3_Serialize_CountSpans( canonicalMesh );
	result.generated_mesh_checksum = SVG_Nav3_Serialize_BuildGeneratedMeshChecksum( canonicalMesh );
	return result;
}

/**
*	@brief	Validate and decode a stage-4 empty nav3 asset blob.
*	@param	blob	Input blob bytes.
*	@param	expectedMapName	Expected runtime map name.
*	@param	expectedBuildConfig	Expected runtime build config.
*	@param	outHeader	[out] Decoded header on success.
*	@return	Structured serialization result.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_ReadEmptyAssetBlob( const nav3_serialized_blob_t &blob,
	const char *expectedMapName,
	const nav3_build_config_t &expectedBuildConfig,
	nav3_serialized_header_t *outHeader ) {
	nav3_generated_mesh_t ignoredGeneratedMesh = {};
	return SVG_Nav3_Serialize_ReadGeneratedAssetBlob( blob, expectedMapName, expectedBuildConfig, outHeader, &ignoredGeneratedMesh );
}

/**
*	@brief	Validate and decode a stage-7 generated nav3 asset blob.
*	@param	blob	Input blob bytes.
*	@param	expectedMapName	Expected runtime map name.
*	@param	expectedBuildConfig	Expected runtime build config.
*	@param	outHeader	[out] Decoded header on success.
*	@param	outGeneratedMesh	[out] Decoded generated sparse mesh payload on success.
*	@return	Structured serialization result.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_ReadGeneratedAssetBlob( const nav3_serialized_blob_t &blob,
	const char *expectedMapName,
	const nav3_build_config_t &expectedBuildConfig,
	nav3_serialized_header_t *outHeader,
	nav3_generated_mesh_t *outGeneratedMesh ) {
	if ( !outHeader || !outGeneratedMesh || blob.bytes.empty() ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	const uint32_t headerByteCount = Nav3_Serialize_HeaderByteCount();
	if ( blob.bytes.size() < headerByteCount ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::FileTooSmall );
	}

	nav3_serialized_blob_reader_t reader = {};
	reader.data = blob.bytes.data();
	reader.size = blob.bytes.size();
	reader.offset = 0;
	nav3_serialized_header_t header = {};
	if ( !Nav3_Serialize_DecodeHeaderBytes( &reader, &header ) ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::FileTooSmall );
	}

	// Validate stable header identity fields.
	if ( header.magic != NAV3_SERIALIZED_FILE_MAGIC ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadMagic );
	}
	if ( header.format_version != NAV3_SERIALIZED_FORMAT_VERSION ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::UnsupportedVersion );
	}
	if ( header.type_schema_version != NAV3_TYPE_SCHEMA_VERSION ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::UnsupportedTypeSchema );
	}
	if ( header.endian_marker != NAV3_SERIALIZED_ENDIAN_MARKER ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::EndianMismatch );
	}

	const uint64_t expectedMapIdentity = SVG_Nav3_Serialize_BuildMapIdentity( expectedMapName );
	if ( header.map_identity != expectedMapIdentity ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::MapIdentityMismatch );
	}
	const uint32_t expectedBuildHash = SVG_Nav3_Serialize_BuildConfigHash( expectedBuildConfig );
	if ( header.build_settings_hash != expectedBuildHash ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BuildSettingsMismatch );
	}

	// Validate header checksum with checksum field zeroed.
	nav3_serialized_header_t headerForChecksum = header;
	headerForChecksum.header_checksum = 0;
	std::vector<uint8_t> headerChecksumBytes = {};
	headerChecksumBytes.reserve( headerByteCount );
	Nav3_Serialize_EncodeHeaderBytes( headerForChecksum, true, &headerChecksumBytes );
	const uint32_t expectedHeaderChecksum = SVG_Nav3_ChecksumFNV1a32( headerChecksumBytes.data(), headerChecksumBytes.size() );
	if ( expectedHeaderChecksum != header.header_checksum ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadHeaderChecksum );
	}

	// Validate section table and payload bounds.
	if ( header.section_count == 0 || header.section_count > NAV3_SERIALIZED_MAX_SECTION_COUNT ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
	}
	const uint32_t sectionDescByteCount = Nav3_Serialize_SectionDescByteCount();
	const uint64_t expectedSectionTableSize = static_cast<uint64_t>( header.section_count ) * static_cast<uint64_t>( sectionDescByteCount );
	if ( expectedSectionTableSize != header.section_table_size ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
	}
	if ( header.section_table_offset < headerByteCount ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
	}
	if ( header.payload_offset != ( header.section_table_offset + header.section_table_size ) ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
	}

	const uint64_t tablePlusPayloadEnd = static_cast<uint64_t>( header.section_table_offset )
		+ static_cast<uint64_t>( header.section_table_size )
		+ static_cast<uint64_t>( header.payload_size );
	if ( tablePlusPayloadEnd > blob.bytes.size() ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionRange );
	}

	// Validate payload checksum over section table + payload.
	const uint8_t *payloadChecksumStart = blob.bytes.data() + header.section_table_offset;
	const size_t payloadChecksumByteCount = static_cast<size_t>( header.section_table_size ) + static_cast<size_t>( header.payload_size );
	const uint32_t expectedPayloadChecksum = SVG_Nav3_ChecksumFNV1a32( payloadChecksumStart, payloadChecksumByteCount );
	if ( expectedPayloadChecksum != header.payload_checksum ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadPayloadChecksum );
	}

	// Parse section descriptors and validate section ranges/checksums.
	reader.offset = header.section_table_offset;
	std::vector<nav3_serialized_section_desc_t> sectionTable = {};
	sectionTable.reserve( header.section_count );

	bool hasBuildConfigSection = false;
	bool hasGeneratedMeshSection = false;
	for ( uint32_t i = 0; i < header.section_count; i++ ) {
		nav3_serialized_section_desc_t section = {};
		if ( !Nav3_Serialize_DecodeSectionDescBytes( &reader, &section ) ) {
			return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
		}

		if ( static_cast<uint32_t>( section.type ) >= static_cast<uint32_t>( nav3_serialized_section_type_t::Count ) ) {
			return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
		}

		if ( section.type == nav3_serialized_section_type_t::BuildConfig ) {
			if ( hasBuildConfigSection ) {
				return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
			}
			hasBuildConfigSection = true;
		}
		if ( section.type == nav3_serialized_section_type_t::GeneratedMesh ) {
			if ( hasGeneratedMeshSection ) {
				return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
			}
			hasGeneratedMeshSection = true;
		}

		const uint64_t sectionStart = section.offset;
		const uint64_t sectionEnd = static_cast<uint64_t>( section.offset ) + static_cast<uint64_t>( section.size );
		const uint64_t payloadStart = header.payload_offset;
		const uint64_t payloadEnd = static_cast<uint64_t>( header.payload_offset ) + static_cast<uint64_t>( header.payload_size );
		if ( sectionStart < payloadStart || sectionEnd > payloadEnd || sectionEnd < sectionStart ) {
			return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionRange );
		}

		const uint8_t *sectionData = blob.bytes.data() + section.offset;
		const uint32_t sectionChecksum = SVG_Nav3_ChecksumFNV1a32( sectionData, section.size );
		if ( sectionChecksum != section.checksum ) {
			return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::SectionChecksumMismatch );
		}

		sectionTable.push_back( section );
	}

	if ( !hasBuildConfigSection ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::MissingBuildConfigSection );
	}

	const nav3_serialized_section_desc_t *buildConfigSection = Nav3_Serialize_FindSectionDesc( sectionTable, nav3_serialized_section_type_t::BuildConfig );
	if ( !buildConfigSection || buildConfigSection->version != NAV3_SERIALIZED_SECTION_VERSION_BUILD_CONFIG ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
	}

	// Decode build-config payload and verify expected build hash.
	nav3_serialized_blob_reader_t buildConfigReader = {};
	buildConfigReader.data = blob.bytes.data() + buildConfigSection->offset;
	buildConfigReader.size = buildConfigSection->size;
	buildConfigReader.offset = 0;
	nav3_build_config_t decodedBuildConfig = {};
	if ( !Nav3_Serialize_ReadBuildConfigBytes( &buildConfigReader, &decodedBuildConfig ) ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
	}
	if ( buildConfigReader.offset != buildConfigReader.size ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
	}
	if ( SVG_Nav3_Serialize_BuildConfigHash( decodedBuildConfig ) != expectedBuildHash ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BuildSettingsMismatch );
	}

	nav3_generated_mesh_t decodedGeneratedMesh = {};
	if ( hasGeneratedMeshSection ) {
		const nav3_serialized_section_desc_t *generatedMeshSection = Nav3_Serialize_FindSectionDesc( sectionTable, nav3_serialized_section_type_t::GeneratedMesh );
		if ( !generatedMeshSection || generatedMeshSection->version != NAV3_SERIALIZED_SECTION_VERSION_GENERATED_MESH ) {
			return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadSectionTable );
		}

		const uint8_t *generatedSectionData = blob.bytes.data() + generatedMeshSection->offset;
		if ( !Nav3_Serialize_ReadGeneratedMeshSectionBytes( generatedSectionData, generatedMeshSection->size, &decodedGeneratedMesh ) ) {
			return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::BadGeneratedMeshPayload );
		}
	}

	*outHeader = header;
	*outGeneratedMesh = std::move( decodedGeneratedMesh );

	nav3_serialization_result_t result = {};
	result.success = true;
	result.validation.compatibility = nav3_serialized_compatibility_t::Accept;
	result.validation.error = nav3_serialized_error_t::None;
	result.byte_count = static_cast<uint32_t>( blob.bytes.size() );
	result.generated_column_count = static_cast<uint32_t>( outGeneratedMesh->columns.size() );
	result.generated_span_count = Nav3_Serialize_CountSpans( *outGeneratedMesh );
	result.generated_mesh_checksum = SVG_Nav3_Serialize_BuildGeneratedMeshChecksum( *outGeneratedMesh );
	return result;
}

/**
*	@brief	Write a serialized nav3 blob through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative destination path.
*	@param	blob	Serialized blob bytes.
*	@return	Structured serialization result.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_WriteCacheFile( const char *cachePath, const nav3_serialized_blob_t &blob ) {
	if ( !cachePath || cachePath[ 0 ] == '\0' || blob.bytes.empty() ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	/**
	*	Normalize caller path so save/load callers can use either `baseq2rtxp/...` or game-relative paths.
	**/
	char normalizedCachePath[ MAX_OSPATH ] = {};
	if ( !Nav3_Serialize_NormalizeCachePathForFs( cachePath, normalizedCachePath, sizeof( normalizedCachePath ) ) ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	char pathBuffer[ MAX_OSPATH ] = {};
	const bool writeOk = ( gi.FS_EasyWriteFile )( pathBuffer, sizeof( pathBuffer ), FS_MODE_WRITE, "", normalizedCachePath, "",
		blob.bytes.data(), blob.bytes.size() );
	if ( !writeOk ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	nav3_serialization_result_t result = {};
	result.success = true;
	result.validation.compatibility = nav3_serialized_compatibility_t::Accept;
	result.validation.error = nav3_serialized_error_t::None;
	result.byte_count = static_cast<uint32_t>( blob.bytes.size() );
	return result;
}

/**
*	@brief	Load and validate a serialized nav3 cache header through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative source path.
*	@param	expectedMapName	Expected runtime map name.
*	@param	expectedBuildConfig	Expected runtime build config.
*	@param	outHeader	[out] Decoded header on success.
*	@return	Structured serialization result.
*	@note	Filesystem-owned load buffers are released via `gi.FS_FreeFile`.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_ReadCacheHeader( const char *cachePath,
	const char *expectedMapName,
	const nav3_build_config_t &expectedBuildConfig,
	nav3_serialized_header_t *outHeader ) {
	nav3_generated_mesh_t ignoredGeneratedMesh = {};
	return SVG_Nav3_Serialize_ReadCacheAsset( cachePath, expectedMapName, expectedBuildConfig, outHeader, &ignoredGeneratedMesh );
}

/**
*	@brief	Load and validate a serialized nav3 cache asset through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative source path.
*	@param	expectedMapName	Expected runtime map name.
*	@param	expectedBuildConfig	Expected runtime build config.
*	@param	outHeader	[out] Decoded header on success.
*	@param	outGeneratedMesh	[out] Decoded generated sparse mesh payload on success.
*	@return	Structured serialization result.
*	@note	Filesystem-owned load buffers are released via `gi.FS_FreeFile`.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_ReadCacheAsset( const char *cachePath,
	const char *expectedMapName,
	const nav3_build_config_t &expectedBuildConfig,
	nav3_serialized_header_t *outHeader,
	nav3_generated_mesh_t *outGeneratedMesh ) {
	if ( !cachePath || cachePath[ 0 ] == '\0' || !outHeader || !outGeneratedMesh ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	/**
	*	Normalize caller path so read and write paths follow identical filesystem resolution rules.
	**/
	char normalizedCachePath[ MAX_OSPATH ] = {};
	if ( !Nav3_Serialize_NormalizeCachePathForFs( cachePath, normalizedCachePath, sizeof( normalizedCachePath ) ) ) {
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::InvalidInput );
	}

	void *fileBuffer = nullptr;
	const int32_t fileLength = ( gi.FS_LoadFile )( normalizedCachePath, &fileBuffer );
	if ( fileLength <= 0 || !fileBuffer ) {
		if ( fileBuffer ) {
			( gi.FS_FreeFile )( fileBuffer );
		}
		return Nav3_Serialize_MakeRejectResult( nav3_serialized_error_t::FileTooSmall );
	}

	nav3_serialized_blob_t blob = {};
	blob.bytes.resize( static_cast<size_t>( fileLength ) );
	std::memcpy( blob.bytes.data(), fileBuffer, static_cast<size_t>( fileLength ) );
	( gi.FS_FreeFile )( fileBuffer );

	nav3_serialization_result_t result = SVG_Nav3_Serialize_ReadGeneratedAssetBlob( blob, expectedMapName, expectedBuildConfig, outHeader, outGeneratedMesh );
	if ( result.success ) {
		result.byte_count = static_cast<uint32_t>( fileLength );
	}
	return result;
}

/**
*	@brief	Return a stable readable name for a serialization error code.
*	@param	error	Error enum to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_Serialize_ErrorName( const nav3_serialized_error_t error ) {
	switch ( error ) {
	case nav3_serialized_error_t::None:
		return "none";
	case nav3_serialized_error_t::InvalidInput:
		return "invalid_input";
	case nav3_serialized_error_t::FileTooSmall:
		return "file_too_small";
	case nav3_serialized_error_t::BadMagic:
		return "bad_magic";
	case nav3_serialized_error_t::UnsupportedVersion:
		return "unsupported_version";
	case nav3_serialized_error_t::UnsupportedTypeSchema:
		return "unsupported_type_schema";
	case nav3_serialized_error_t::EndianMismatch:
		return "endian_mismatch";
	case nav3_serialized_error_t::MapIdentityMismatch:
		return "map_identity_mismatch";
	case nav3_serialized_error_t::BuildSettingsMismatch:
		return "build_settings_mismatch";
	case nav3_serialized_error_t::BadHeaderChecksum:
		return "bad_header_checksum";
	case nav3_serialized_error_t::BadSectionTable:
		return "bad_section_table";
	case nav3_serialized_error_t::BadSectionRange:
		return "bad_section_range";
	case nav3_serialized_error_t::BadPayloadChecksum:
		return "bad_payload_checksum";
	case nav3_serialized_error_t::SectionChecksumMismatch:
		return "section_checksum_mismatch";
	case nav3_serialized_error_t::MissingBuildConfigSection:
		return "missing_build_config_section";
	case nav3_serialized_error_t::BadGeneratedMeshPayload:
		return "bad_generated_mesh_payload";
	case nav3_serialized_error_t::GeneratedMeshChecksumMismatch:
		return "generated_mesh_checksum_mismatch";
	case nav3_serialized_error_t::Count:
	default:
		break;
	}
	return "unknown";
}