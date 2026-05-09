/********************************************************************
*
*
*    ServerGame: Nav3 Persistence Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav3/nav3_persistence.h"


/**
*
*
*    Nav3 Persistence Public API:
*
*
**/
/**
*	@brief	Build and write a stage-7 generated nav3 asset file.
*	@param	cachePath	Destination cache path.
*	@param	mapName	Map identity source.
*	@param	buildConfig	Build config payload.
*	@param	generatedMesh	Generated sparse mesh payload.
*	@return	Structured persistence result.
**/
nav3_persistence_result_t SVG_Nav3_Persistence_SaveGeneratedAsset( const char *cachePath,
	const char *mapName,
	const nav3_build_config_t &buildConfig,
	const nav3_generated_mesh_t &generatedMesh ) {
	nav3_persistence_result_t result = {};

	// Build a canonical stage-7 generated asset blob before attempting any filesystem writes.
	nav3_serialized_blob_t blob = {};
	const nav3_serialization_result_t buildResult = SVG_Nav3_Serialize_BuildGeneratedAssetBlob( mapName, buildConfig, generatedMesh, &blob );
	if ( !buildResult.success ) {
		result.success = false;
		result.validation = buildResult.validation;
		result.byte_count = 0;
		result.generated_column_count = 0;
		result.generated_span_count = 0;
		result.generated_mesh_checksum = 0;
		return result;
	}

	// Write the built blob through engine filesystem helpers.
	const nav3_serialization_result_t writeResult = SVG_Nav3_Serialize_WriteCacheFile( cachePath, blob );
	result.success = writeResult.success;
	result.validation = writeResult.validation;
	result.byte_count = writeResult.byte_count;
	result.generated_column_count = buildResult.generated_column_count;
	result.generated_span_count = buildResult.generated_span_count;
	result.generated_mesh_checksum = buildResult.generated_mesh_checksum;
	return result;
}

/**
*	@brief	Load and validate a stage-7 generated nav3 asset file.
*	@param	cachePath	Source cache path.
*	@param	expectedMapName	Expected runtime map identity source.
*	@param	expectedBuildConfig	Expected runtime build config.
*	@param	outHeader	[out] Decoded serialized header on success.
*	@param	outGeneratedMesh	[out] Decoded generated sparse mesh payload on success.
*	@return	Structured persistence result.
**/
nav3_persistence_result_t SVG_Nav3_Persistence_LoadGeneratedAsset( const char *cachePath,
	const char *expectedMapName,
	const nav3_build_config_t &expectedBuildConfig,
	nav3_serialized_header_t *outHeader,
	nav3_generated_mesh_t *outGeneratedMesh ) {
	nav3_persistence_result_t result = {};

	// Load and validate header + generated-mesh payload through serializer-owned validation paths.
	const nav3_serialization_result_t loadResult = SVG_Nav3_Serialize_ReadCacheAsset( cachePath,
		expectedMapName,
		expectedBuildConfig,
		outHeader,
		outGeneratedMesh );
	result.success = loadResult.success;
	result.validation = loadResult.validation;
	result.byte_count = loadResult.byte_count;
	result.generated_column_count = loadResult.generated_column_count;
	result.generated_span_count = loadResult.generated_span_count;
	result.generated_mesh_checksum = loadResult.generated_mesh_checksum;
	return result;
}

/**
*	@brief	Return a stable readable name for one persistence error code.
*	@param	error	Serialization error code to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_Persistence_ErrorName( const nav3_serialized_error_t error ) {
	return SVG_Nav3_Serialize_ErrorName( error );
}
