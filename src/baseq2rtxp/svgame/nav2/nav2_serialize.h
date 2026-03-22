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
#include "svgame/nav2/nav2_span_adjacency.h"
#include "svgame/nav2/nav2_span_grid.h"


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
*	@brief	Stable mismatch taxonomy for static-nav round-trip validation.
*	@note	The validation helpers use these ids instead of raw strings so callers can summarize serializer regressions deterministically.
**/
enum class nav2_serialized_roundtrip_mismatch_t : uint8_t {
	//! No mismatch was recorded.
	None = 0,
	//! Top-level serialized header fields did not round-trip cleanly.
	Header,
	//! Span-grid metadata such as sizes or counts changed across the round-trip.
	SpanGridMeta,
	//! Sparse column structure or ordering changed across the round-trip.
	SpanGridColumn,
	//! Individual span payload fields changed across the round-trip.
	SpanGridSpan,
	//! Adjacency edge structure or edge count changed across the round-trip.
	AdjacencyEdge,
	//! The deserialize step failed before payload comparison could complete.
	DeserializeFailure,
	Count
};

/**
*	@brief	Structured result of validating a static-nav serialization round-trip.
*	@note	This keeps mismatch counts explicit so callers can report concise diagnostics without depending on noisy ad hoc logging.
**/
struct nav2_serialized_roundtrip_result_t {
	//! True when the entire round-trip succeeded and the decoded payload matched the source payload exactly.
	bool success = false;
	//! Structured serialization result from the in-memory build phase.
	nav2_serialization_result_t build_result = {};
	//! Structured serialization result from the in-memory readback phase.
	nav2_serialization_result_t read_result = {};
  //! In-memory static-nav blob build duration in milliseconds.
	double build_elapsed_ms = 0.0;
	//! In-memory static-nav blob readback duration in milliseconds.
	double read_elapsed_ms = 0.0;
	//! Total mismatch count accumulated across all validation categories.
	uint32_t mismatch_count = 0;
	//! Stable mismatch counters keyed by `nav2_serialized_roundtrip_mismatch_t`.
	uint32_t mismatch_counts[ ( size_t )nav2_serialized_roundtrip_mismatch_t::Count ] = {};
};

/**
*	@brief	Read cursor used while decoding nav2 binary sections from an in-memory blob.
*	@note	The cursor tracks byte bounds explicitly so deserialization can reject truncated or malformed payloads safely.
**/
struct nav2_serialized_blob_reader_t {
	//! Pointer to the first byte of the loaded blob.
	const uint8_t *data = nullptr;
	//! Total byte size of the loaded blob.
	uint32_t size = 0;
	//! Current byte offset of the read cursor.
	uint32_t offset = 0;
	//! True once the reader has detected an out-of-bounds or malformed read.
	bool failed = false;
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

/**
*	@brief	Build a standalone nav2 cache blob containing the current span-grid and adjacency payloads.
*	@param	policy	Serialization policy describing the payload.
*	@param	spanGrid	Sparse span-grid payload to serialize.
*	@param	adjacency	Local adjacency payload to serialize.
*	@param	outBlob	[out] Blob receiving the serialized bytes.
*	@return	Structured serialization result including byte count and validation state.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_BuildStaticNavBlob( const nav2_serialization_policy_t &policy,
	const nav2_span_grid_t &spanGrid, const nav2_span_adjacency_t &adjacency, nav2_serialized_blob_t *outBlob );

/**
*	@brief	Decode span-grid and adjacency payloads from a standalone nav2 cache blob already loaded in memory.
*	@param	blob	Serialized nav2 blob to decode.
*	@param	policy	Expected serialization policy.
*	@param	outHeader	[out] Decoded serialized header on success.
*	@param	outSpanGrid	[out] Decoded span-grid payload on success.
*	@param	outAdjacency	[out] Decoded adjacency payload on success.
*	@return	Structured deserialization result including byte count and validation state.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_ReadStaticNavBlob( const nav2_serialized_blob_t &blob,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader,
	nav2_span_grid_t *outSpanGrid, nav2_span_adjacency_t *outAdjacency );

/**
*	@brief	Load and decode a standalone nav2 cache file containing span-grid and adjacency payloads.
*	@param	cachePath	Absolute or game-relative path to load.
*	@param	policy	Expected serialization policy.
*	@param	outHeader	[out] Decoded serialized header on success.
*	@param	outSpanGrid	[out] Decoded span-grid payload on success.
*	@param	outAdjacency	[out] Decoded adjacency payload on success.
*	@return	Structured load result including byte count and validation state.
*	@note	Loaded FS buffers remain owned by the engine and are released internally via `gi.FS_FreeFile`.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_ReadStaticNavCacheFile( const char *cachePath,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader,
	nav2_span_grid_t *outSpanGrid, nav2_span_adjacency_t *outAdjacency );

/**
*	@brief	Validate that static-nav payloads survive an in-memory serialize/read round-trip unchanged.
*	@param	policy	Serialization policy describing the payload.
*	@param	spanGrid	Source span-grid payload to round-trip.
*	@param	adjacency	Source adjacency payload to round-trip.
*	@param	outResult	[out] Structured round-trip validation result.
*	@return	True when the round-trip completed and the decoded payload matched exactly.
**/
const bool SVG_Nav2_Serialize_ValidateStaticNavRoundTrip( const nav2_serialization_policy_t &policy,
	const nav2_span_grid_t &spanGrid, const nav2_span_adjacency_t &adjacency, nav2_serialized_roundtrip_result_t *outResult );

/**
*	@brief	Return a stable display name for a static-nav round-trip mismatch category.
*	@param	mismatch	Mismatch category to convert.
*	@return	Constant string used by higher-level diagnostics or benchmarks.
**/
const char *SVG_Nav2_Serialize_RoundTripMismatchName( const nav2_serialized_roundtrip_mismatch_t mismatch );
