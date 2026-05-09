/********************************************************************
*
*
*    ServerGame: Nav3 Core Utilities + Stable Types
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include <cstddef>
#include <cstdint>


/**
*
*
*    Nav3 Stable Versioning:
*
*
**/
//! Stable nav3 type/schema version used by diagnostics and future persistence gates.
static constexpr uint32_t NAV3_TYPE_SCHEMA_VERSION = 1;


/**
*
*
*    Nav3 Stable Reference Types:
*
*
**/
//! Sentinel values for invalid stable refs.
static constexpr uint16_t NAV3_REF_INVALID_SALT = 0;
//! Sentinel tile index for invalid stable refs.
static constexpr uint16_t NAV3_REF_INVALID_TILE_INDEX = UINT16_MAX;
//! Sentinel local index for invalid stable refs.
static constexpr uint32_t NAV3_REF_INVALID_LOCAL_INDEX = UINT32_MAX;

/**
*	@brief	Stable nav3 reference kind used by runtime/persistence seams.
**/
enum class nav3_ref_type_t : uint8_t {
	Invalid = 0,
	Span,
	Edge,
	Portal,
	Cover,
	JumpLink,
	MoverLink,
	OctreeLeaf,
	Count
};

/**
*	@brief	Pointer-free stable nav3 reference payload.
*	@note	This is intentionally compact and does not store raw addresses.
**/
struct nav3_ref_t {
	//! Stable reference kind.
	nav3_ref_type_t type = nav3_ref_type_t::Invalid;
	//! Generation/salt discriminator.
	uint16_t salt = NAV3_REF_INVALID_SALT;
	//! Tile or brick slot index.
	uint16_t tile_index = NAV3_REF_INVALID_TILE_INDEX;
	//! Local index inside tile/brick-owned arrays.
	uint32_t local_index = NAV3_REF_INVALID_LOCAL_INDEX;
};


/**
*
*
*    Nav3 Build Configuration:
*
*
**/
/**
*	@brief	Quantized build configuration used by nav3 generation/runtime diagnostics.
*	@note	Cell-derived fields are computed by `SVG_Nav3_DeriveBuildConfigCells`.
**/
struct nav3_build_config_t {
	//! Horizontal cell size in world units.
	float cell_size = 16.0f;
	//! Vertical cell size in world units.
	float cell_height = 8.0f;
	//! Baseline standing agent height in world units.
	float agent_height_units = 56.0f;
	//! Baseline step/climb height in world units.
	float agent_step_height_units = 18.0f;
	//! Baseline agent radius in world units.
	float agent_radius_units = 16.0f;
	//! Derived walkable-height threshold in cells.
	int32_t walkable_height_cells = 0;
	//! Derived step/climb threshold in cells.
	int32_t walkable_climb_cells = 0;
	//! Derived erosion radius in cells.
	int32_t walkable_radius_cells = 0;
};


/**
*
*
*    Nav3 Telemetry Buckets:
*
*
**/
/**
*	@brief	Named telemetry buckets used by staged nav3 runtime/generation timing.
**/
enum class nav3_telemetry_bucket_t : uint8_t {
	Generation = 0,
	Load,
	Query,
	DirtyRebuild,
	Avoidance,
	Count
};


/**
*
*
*    Nav3 Fixed Scratch Span:
*
*
**/
/**
*	@brief	Small fixed-capacity scratch allocator descriptor.
*	@note	Caller owns the memory backing `bytes`.
**/
struct nav3_fixed_scratch_span_t {
	//! Backing byte storage pointer.
	uint8_t *bytes = nullptr;
	//! Total capacity in bytes.
	size_t capacity = 0;
	//! Current used byte count.
	size_t used = 0;
};


/**
*
*
*    Nav3 Core Helpers:
*
*
**/
/**
*	@brief	Return true when a stable ref has non-sentinel values.
*	@param	ref	Reference to validate.
*	@return	True when the ref has valid type/salt/index fields.
**/
const bool SVG_Nav3_IsRefValid( const nav3_ref_t &ref );

/**
*	@brief	Return true when a stable ref is valid and has the requested type.
*	@param	ref	Reference to validate.
*	@param	type	Required reference type.
*	@return	True when type and validity checks pass.
**/
const bool SVG_Nav3_IsRefType( const nav3_ref_t &ref, const nav3_ref_type_t type );

/**
*	@brief	Return true when a stable ref passes validity checks and index bounds.
*	@param	ref	Reference to validate.
*	@param	requiredType	Expected ref type.
*	@param	maxTileCount	Exclusive tile index upper bound.
*	@param	maxLocalCount	Exclusive local index upper bound.
*	@return	True when type, validity, and bounds checks pass.
**/
const bool SVG_Nav3_IsRefValidInRange( const nav3_ref_t &ref, const nav3_ref_type_t requiredType,
	const uint16_t maxTileCount,
	const uint32_t maxLocalCount );

/**
*	@brief	Derive quantized walkable cell thresholds from world-unit build settings.
*	@param	inOutBuildConfig	Build settings to validate and populate.
*	@return	True when settings are valid and derived values were produced.
**/
const bool SVG_Nav3_DeriveBuildConfigCells( nav3_build_config_t *inOutBuildConfig );

/**
*	@brief	Return readable name for a nav3 ref type.
*	@param	refType	Ref type enum to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_RefTypeName( const nav3_ref_type_t refType );

/**
*	@brief	Return readable name for a nav3 telemetry bucket.
*	@param	bucket	Telemetry bucket enum to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_TelemetryBucketName( const nav3_telemetry_bucket_t bucket );

/**
*	@brief	Return true when host byte-order is little-endian.
**/
const bool SVG_Nav3_IsLittleEndianHost( void );

/**
*	@brief	Byte-swap a 16-bit unsigned value.
**/
uint16_t SVG_Nav3_ByteSwapU16( const uint16_t value );

/**
*	@brief	Byte-swap a 32-bit unsigned value.
**/
uint32_t SVG_Nav3_ByteSwapU32( const uint32_t value );

/**
*	@brief	Compute deterministic FNV-1a 32-bit checksum over arbitrary bytes.
*	@param	data	Input bytes.
*	@param	byteCount	Input byte count.
*	@return	FNV-1a 32-bit checksum.
**/
uint32_t SVG_Nav3_ChecksumFNV1a32( const void *data, const size_t byteCount );

/**
*	@brief	Reset fixed scratch allocator usage to zero.
*	@param	scratch	Scratch descriptor to reset.
**/
void SVG_Nav3_FixedScratchReset( nav3_fixed_scratch_span_t *scratch );

/**
*	@brief	Allocate aligned bytes from fixed scratch storage.
*	@param	scratch	Scratch descriptor.
*	@param	byteCount	Requested byte count.
*	@param	alignment	Power-of-two byte alignment requirement.
*	@return	Pointer to allocated bytes or `nullptr` when capacity/inputs are invalid.
**/
void *SVG_Nav3_FixedScratchAlloc( nav3_fixed_scratch_span_t *scratch, const size_t byteCount, const size_t alignment = alignof( std::max_align_t ) );
