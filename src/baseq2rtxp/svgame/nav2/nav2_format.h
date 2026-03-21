/********************************************************************
*
*
*	ServerGame: Nav2 Serialization Format
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"


/**
*
*
*	Nav2 Format Constants:
*
*
**/
/**
*	@brief	Nav2 serialization format constants.
*	@note	These are shared by cache-file and savegame-facing nav2 payloads.
**/
//! Binary magic for standalone nav2 cache files written through engine filesystem helpers.
static constexpr uint32_t NAV_SERIALIZED_FILE_MAGIC = MakeLittleLong( 'N', '2', 'C', 'H' );
//! Binary magic for nav2 savegame-facing runtime chunks written through gz save/load helpers.
static constexpr uint32_t NAV_SERIALIZED_SAVE_MAGIC = MakeLittleLong( 'N', '2', 'S', 'V' );
//! Current nav2 serialized format version.
static constexpr uint32_t NAV_SERIALIZED_FORMAT_VERSION = 1;
//! Current nav2 build-version seed used to invalidate incompatible cached assets.
static constexpr uint32_t NAV_SERIALIZED_BUILD_VERSION = 1;


/**
*
*
*	Nav2 Format Enumerations:
*
*
**/
/**
*	@brief	Top-level categories of nav2 serialized data.
*	@note	These categories separate static cache data, dynamic overlays, and savegame-facing runtime
*			state so load policies can reject or rebuild each class independently.
**/
enum class nav2_serialized_category_t : uint32_t {
	None = 0,
	StaticAsset,
	DynamicOverlay,
	SavegameRuntime,
	Count
};

/**
*	@brief	Explicit nav2 serialized section identifiers.
*	@note	Section ids are stable so future format extensions can add optional sections without
*			changing the meaning of earlier payloads.
**/
enum class nav2_serialized_section_type_t : uint32_t {
	None = 0,
	Header,
	SpanGrid,
	Adjacency,
	Connectors,
	RegionLayers,
	HierarchyGraph,
	MoverLocalModels,
	DynamicOverlay,
	ActorPathState,
	SchedulerState,
	Count
};

/**
*	@brief	Filesystem or save/load transport used by a nav2 serialized payload.
*	@note	This keeps policy explicit so callers know whether a payload belongs to standalone cache IO
*			or gz-backed save/load integration.
**/
enum class nav2_serialized_transport_t : uint32_t {
	None = 0,
	EngineFilesystem,
	SavegameGZip,
	Count
};

/**
*	@brief	High-level load decision produced by format compatibility checks.
*	@note	Later serializer and save/load layers use this to decide whether to accept, reject, or rebuild.
**/
enum class nav2_serialized_compatibility_t : uint32_t {
	Accept = 0,
	Reject,
	RebuildRequired,
	Count
};


/**
*
*
*	Nav2 Format Data Structures:
*
*
**/
/**
*	@brief	Versioned header shared by standalone nav2 cache files and savegame-facing nav2 chunks.
*	@note	The header avoids raw pointers and remains endian-safe by using fixed-width scalar fields only.
**/
struct nav2_serialized_header_t {
	//! Magic value describing the transport family of the serialized payload.
	uint32_t magic = NAV_SERIALIZED_FILE_MAGIC;
	//! Current nav2 serialized format version.
	uint32_t format_version = NAV_SERIALIZED_FORMAT_VERSION;
	//! Nav2 build-version seed used for compatibility rejection.
	uint32_t build_version = NAV_SERIALIZED_BUILD_VERSION;
	//! High-level data category encoded in this payload.
	nav2_serialized_category_t category = nav2_serialized_category_t::None;
	//! Transport family used by the payload.
	nav2_serialized_transport_t transport = nav2_serialized_transport_t::None;
	//! Optional map checksum or compatibility token.
	uint64_t map_identity = 0;
	//! Optional section count written after this header.
	uint32_t section_count = 0;
	//! Reserved flags for future compatibility tuning.
	uint32_t compatibility_flags = 0;
};

/**
*	@brief	Descriptor for one serialized nav2 section.
*	@note	This is intentionally compact and offset-based so section payloads can be streamed or validated
*			without relying on pointer identity.
**/
struct nav2_serialized_section_desc_t {
	//! Stable section identifier.
	nav2_serialized_section_type_t type = nav2_serialized_section_type_t::None;
	//! Byte offset of the section payload relative to the start of the serialized blob.
	uint32_t offset = 0;
	//! Byte size of the section payload.
	uint32_t size = 0;
	//! Optional version for the specific section payload.
	uint32_t version = 0;
};

/**
*	@brief	Structured result of validating a nav2 serialized header against the current runtime.
*	@note	This keeps compatibility policy explicit for both standalone cache loads and savegame restoration.
**/
struct nav2_serialized_validation_t {
	//! Compatibility decision produced by header validation.
	nav2_serialized_compatibility_t compatibility = nav2_serialized_compatibility_t::Reject;
	//! True when the payload magic matched the expected transport family.
	bool magic_ok = false;
	//! True when the serialized format version matched the current runtime expectation.
	bool format_ok = false;
	//! True when the serialized build version matched the current nav2 build seed.
	bool build_ok = false;
	//! True when the encoded category matched the caller's expectation.
	bool category_ok = false;
	//! True when the encoded transport matched the caller's expectation.
	bool transport_ok = false;
};