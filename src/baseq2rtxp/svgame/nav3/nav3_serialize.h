/********************************************************************
*
*
*    ServerGame: Nav3 Serialization Foundation
*
*
********************************************************************/
#pragma once

#include "svgame/nav3/nav3_generate.h"

#include <vector>


/**
*
*
*    Nav3 Serialized Format Constants:
*
*
**/
//! Stable nav3 file magic (`NAV3`).
static constexpr uint32_t NAV3_SERIALIZED_FILE_MAGIC = 0x3356414Eu;
//! Stable nav3 serialized format version.
static constexpr uint32_t NAV3_SERIALIZED_FORMAT_VERSION = 1;
//! Endian marker used to reject byte-order mismatches.
static constexpr uint32_t NAV3_SERIALIZED_ENDIAN_MARKER = 0x01020304u;
//! Maximum section count accepted by the nav3 reader.
static constexpr uint32_t NAV3_SERIALIZED_MAX_SECTION_COUNT = 32;


/**
*
*
*    Nav3 Serialized Enums:
*
*
**/
/**
*	@brief	Compatibility verdict produced by nav3 serialization validation.
**/
enum class nav3_serialized_compatibility_t : uint8_t {
	Accept = 0,
	Reject,
};

/**
*	@brief	Stable failure taxonomy for nav3 serialized blob validation.
**/
enum class nav3_serialized_error_t : uint8_t {
	None = 0,
	InvalidInput,
	FileTooSmall,
	BadMagic,
	UnsupportedVersion,
	UnsupportedTypeSchema,
	EndianMismatch,
	MapIdentityMismatch,
	BuildSettingsMismatch,
	BadHeaderChecksum,
	BadSectionTable,
	BadSectionRange,
	BadPayloadChecksum,
	SectionChecksumMismatch,
	MissingBuildConfigSection,
	BadGeneratedMeshPayload,
	GeneratedMeshChecksumMismatch,
	Count
};

/**
*	@brief	Payload section kinds currently supported by nav3 serializer.
**/
enum class nav3_serialized_section_type_t : uint32_t {
	None = 0,
	BuildConfig = 1,
	GeneratedMesh = 2,
	Count
};

/**
*	@brief	Serialized section flags.
*	@note	Compression support is reserved for later stages.
**/
enum nav3_serialized_section_flags_t : uint32_t {
	NAV3_SERIALIZED_SECTION_FLAG_NONE = 0,
	NAV3_SERIALIZED_SECTION_FLAG_COMPRESSED = 1 << 0,
};


/**
*
*
*    Nav3 Serialized Data Structures:
*
*
**/
/**
*	@brief	Header metadata published at the front of every nav3 serialized blob.
**/
struct nav3_serialized_header_t {
	//! File magic (`NAV3`).
	uint32_t magic = NAV3_SERIALIZED_FILE_MAGIC;
	//! Serialized format version.
	uint32_t format_version = NAV3_SERIALIZED_FORMAT_VERSION;
	//! Nav3 type/schema version expected by the runtime.
	uint32_t type_schema_version = NAV3_TYPE_SCHEMA_VERSION;
	//! Endian marker used for byte-order mismatch detection.
	uint32_t endian_marker = NAV3_SERIALIZED_ENDIAN_MARKER;
	//! Map identity hash for the asset.
	uint64_t map_identity = 0;
	//! Build settings hash for compatibility checks.
	uint32_t build_settings_hash = 0;
	//! Number of section descriptors in the section table.
	uint32_t section_count = 0;
	//! Byte offset to section table.
	uint32_t section_table_offset = 0;
	//! Byte size of section table.
	uint32_t section_table_size = 0;
	//! Byte offset to first payload byte.
	uint32_t payload_offset = 0;
	//! Total payload bytes after section table.
	uint32_t payload_size = 0;
	//! Header checksum with this field zeroed during checksum computation.
	uint32_t header_checksum = 0;
	//! Payload checksum over section table + payload bytes.
	uint32_t payload_checksum = 0;
};

/**
*	@brief	One serialized section descriptor entry.
**/
struct nav3_serialized_section_desc_t {
	//! Section payload kind.
	nav3_serialized_section_type_t type = nav3_serialized_section_type_t::None;
	//! Section schema version.
	uint32_t version = 0;
	//! Section flags (compression reserved for later stages).
	uint32_t flags = NAV3_SERIALIZED_SECTION_FLAG_NONE;
	//! Byte offset to section payload.
	uint32_t offset = 0;
	//! Section payload byte count.
	uint32_t size = 0;
	//! Section payload checksum.
	uint32_t checksum = 0;
};

/**
*	@brief	Validation summary for nav3 serialized blobs.
**/
struct nav3_serialized_validation_t {
	//! Compatibility verdict.
	nav3_serialized_compatibility_t compatibility = nav3_serialized_compatibility_t::Reject;
	//! Detailed failure category.
	nav3_serialized_error_t error = nav3_serialized_error_t::InvalidInput;
};

/**
*	@brief	Tag-owned binary blob used by nav3 cache serialization.
**/
struct nav3_serialized_blob_t {
	//! Byte buffer containing the serialized nav3 payload.
	std::vector<uint8_t> bytes = {};
};

/**
*	@brief	Structured result of one nav3 serialization operation.
**/
struct nav3_serialization_result_t {
	//! True when the operation completed successfully.
	bool success = false;
	//! Validation summary for the operation.
	nav3_serialized_validation_t validation = {};
	//! Number of bytes written or loaded.
	uint32_t byte_count = 0;
	//! Number of generated sparse columns encoded/decoded for this operation.
	uint32_t generated_column_count = 0;
	//! Number of generated spans encoded/decoded for this operation.
	uint32_t generated_span_count = 0;
	//! Deterministic generated-mesh checksum for this operation.
	uint32_t generated_mesh_checksum = 0;
};


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
uint64_t SVG_Nav3_Serialize_BuildMapIdentity( const char *mapName );

/**
*	@brief	Build a stable build-settings hash from nav3 build config values.
*	@param	buildConfig	Build config to hash.
*	@return	Stable build settings hash.
**/
uint32_t SVG_Nav3_Serialize_BuildConfigHash( const nav3_build_config_t &buildConfig );

/**
*	@brief	Build a deterministic checksum from canonicalized generated mesh payload fields.
*	@param	generatedMesh	Generated mesh to hash.
*	@return	Deterministic generated mesh checksum.
**/
uint32_t SVG_Nav3_Serialize_BuildGeneratedMeshChecksum( const nav3_generated_mesh_t &generatedMesh );

/**
*	@brief	Build a stage-4 empty nav3 asset blob containing header, section table, and build-config section.
*	@param	mapName	Map identity source string.
*	@param	buildConfig	Build config payload to persist.
*	@param	outBlob	[out] Destination blob.
*	@return	Structured serialization result.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_BuildEmptyAssetBlob( const char *mapName,
	const nav3_build_config_t &buildConfig,
	nav3_serialized_blob_t *outBlob );

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
	nav3_serialized_blob_t *outBlob );

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
	nav3_serialized_header_t *outHeader );

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
	nav3_generated_mesh_t *outGeneratedMesh );

/**
*	@brief	Write a serialized nav3 blob through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative destination path.
*	@param	blob	Serialized blob bytes.
*	@return	Structured serialization result.
**/
nav3_serialization_result_t SVG_Nav3_Serialize_WriteCacheFile( const char *cachePath, const nav3_serialized_blob_t &blob );

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
	nav3_serialized_header_t *outHeader );

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
	nav3_generated_mesh_t *outGeneratedMesh );

/**
*	@brief	Return a stable readable name for a serialization error code.
*	@param	error	Error enum to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_Serialize_ErrorName( const nav3_serialized_error_t error );
