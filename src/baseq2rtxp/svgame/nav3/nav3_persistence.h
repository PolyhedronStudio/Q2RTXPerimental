/********************************************************************
*
*
*    ServerGame: Nav3 Persistence Foundation
*
*
********************************************************************/
#pragma once

#include "svgame/nav3/nav3_serialize.h"


/**
*
*
*    Nav3 Persistence Data Structures:
*
*
**/
/**
*	@brief	Structured result of one nav3 persistence operation.
**/
struct nav3_persistence_result_t {
	//! True when the requested persistence operation completed successfully.
	bool success = false;
	//! Validation/failure summary.
	nav3_serialized_validation_t validation = {};
	//! Number of bytes written or loaded.
	uint32_t byte_count = 0;
	//! Number of generated sparse columns written or loaded.
	uint32_t generated_column_count = 0;
	//! Number of generated spans written or loaded.
	uint32_t generated_span_count = 0;
	//! Deterministic generated-mesh checksum written or loaded.
	uint32_t generated_mesh_checksum = 0;
};


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
	const nav3_generated_mesh_t &generatedMesh );

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
	nav3_generated_mesh_t *outGeneratedMesh );

/**
*	@brief	Return a stable readable name for one persistence error code.
*	@param	error	Serialization error code to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_Persistence_ErrorName( const nav3_serialized_error_t error );
