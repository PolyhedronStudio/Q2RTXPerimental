/********************************************************************
*
*
*	ServerGame: Nav2 Serialization Foundation
*
*
********************************************************************/
#pragma once

#include <cstdint>
#include <vector>

#include "svgame/memory/svg_raiiobject.hpp"
#include "svgame/nav2/nav2_format.h"


/**
*	@brief	Tag-owned binary blob used by nav2 standalone cache serialization.
*	@note	The serializer uses this as a staging buffer before writing through engine filesystem helpers.
**/
struct nav2_serialized_blob_t {
	//! Byte buffer containing the serialized nav2 payload.
	std::vector<uint8_t> bytes = {};
};

//! RAII owner type for helper objects used by nav2 serialization code.
using nav2_serialized_blob_owner_t = SVG_RAIIObject<nav2_serialized_blob_t>;

/**
*	@brief	Policy descriptor controlling how a nav2 payload is serialized or validated.
*	@note	This keeps transport, category, and compatibility expectations explicit for future extensions.
**/
struct nav2_serialization_policy_t {
	//! Top-level data category expected for the payload.
	nav2_serialized_category_t category = nav2_serialized_category_t::None;
	//! Transport family expected for the payload.
	nav2_serialized_transport_t transport = nav2_serialized_transport_t::None;
	//! Optional map identity token to embed in written headers and validate on read.
	uint64_t map_identity = 0;
};

/**
*	@brief	Summary result of attempting to write or read a nav2 serialized payload.
*	@note	This keeps error handling structured without throwing exceptions or relying on raw strings alone.
**/
struct nav2_serialization_result_t {
	//! True when the requested serialization operation completed successfully.
	bool success = false;
	//! Validation summary for read-side operations.
	nav2_serialized_validation_t validation = {};
	//! Number of bytes written to or loaded from the serialized payload.
	uint32_t byte_count = 0;
};


/**
*
*
*	Nav2 Serialization Public API:
*
*
**/
/**
*	@brief	Initialize a nav2 serialized header from a policy descriptor.
*	@param	header	[out] Header to initialize.
*	@param	policy	Serialization policy driving the header contents.
*	@param	magic	Magic value to embed in the header.
**/
void SVG_Nav2_Serialize_InitHeader( nav2_serialized_header_t *header, const nav2_serialization_policy_t &policy, const uint32_t magic );

/**
*	@brief	Validate a nav2 serialized header against an expected policy and transport magic.
*	@param	header	Header to validate.
*	@param	policy	Expected serialization policy.
*	@param	expectedMagic	Expected transport magic.
*	@return	Structured validation result used by later cache and save/load code.
**/
nav2_serialized_validation_t SVG_Nav2_Serialize_ValidateHeader( const nav2_serialized_header_t &header,
	const nav2_serialization_policy_t &policy, const uint32_t expectedMagic );

/**
*	@brief	Build a placeholder standalone nav2 cache blob for the requested policy.
*	@param	policy	Serialization policy describing the payload.
*	@param	outBlob	[out] Blob receiving the serialized bytes.
*	@return	Structured serialization result including byte count.
*	@note	The current foundation writes only the versioned header and section container.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_BuildCacheBlob( const nav2_serialization_policy_t &policy,
	nav2_serialized_blob_t *outBlob );

/**
*	@brief	Write a nav2 standalone cache blob through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative path to write.
*	@param	blob	Serialized blob to write.
*	@return	Structured serialization result including byte count.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_WriteCacheFile( const char *cachePath, const nav2_serialized_blob_t &blob );

/**
*	@brief	Load and validate a nav2 standalone cache blob through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative path to load.
*	@param	policy	Expected serialization policy.
*	@param	outHeader	[out] Decoded header on success.
*	@return	Structured load result including validation state.
*	@note	Loaded FS buffers remain owned by the engine and are released internally via `gi.FS_FreeFile`.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_ReadCacheHeader( const char *cachePath,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader );
