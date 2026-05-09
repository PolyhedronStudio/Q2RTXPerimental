/********************************************************************
*
*
*    ServerGame: Nav3 Core Utilities + Stable Types - Implementation
*
*
********************************************************************/
#include "svgame/nav3/nav3_core.h"

#include <algorithm>
#include <cmath>


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
const bool SVG_Nav3_IsRefValid( const nav3_ref_t &ref ) {
	// Reject invalid type and out-of-range type enum values.
	if ( ref.type == nav3_ref_type_t::Invalid || ref.type == nav3_ref_type_t::Count ) {
		return false;
	}

	// Reject sentinel generation/index markers.
	if ( ref.salt == NAV3_REF_INVALID_SALT ) {
		return false;
	}
	if ( ref.tile_index == NAV3_REF_INVALID_TILE_INDEX ) {
		return false;
	}
	if ( ref.local_index == NAV3_REF_INVALID_LOCAL_INDEX ) {
		return false;
	}
	return true;
}

/**
*	@brief	Return true when a stable ref is valid and has the requested type.
*	@param	ref	Reference to validate.
*	@param	type	Required reference type.
*	@return	True when type and validity checks pass.
**/
const bool SVG_Nav3_IsRefType( const nav3_ref_t &ref, const nav3_ref_type_t type ) {
	// Require caller-requested type to be non-invalid and in range.
	if ( type == nav3_ref_type_t::Invalid || type == nav3_ref_type_t::Count ) {
		return false;
	}

	// Ensure stable ref validity and exact type match.
	if ( !SVG_Nav3_IsRefValid( ref ) ) {
		return false;
	}
	return ref.type == type;
}

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
	const uint32_t maxLocalCount ) {
	// Reject refs that fail base validity/type requirements.
	if ( !SVG_Nav3_IsRefType( ref, requiredType ) ) {
		return false;
	}

	// Reject zero upper bounds so callers cannot accidentally pass empty ranges.
	if ( maxTileCount == 0 || maxLocalCount == 0 ) {
		return false;
	}

	// Reject refs with indices outside caller-provided bounds.
	if ( ref.tile_index >= maxTileCount ) {
		return false;
	}
	if ( ref.local_index >= maxLocalCount ) {
		return false;
	}
	return true;
}

/**
*	@brief	Derive quantized walkable cell thresholds from world-unit build settings.
*	@param	inOutBuildConfig	Build settings to validate and populate.
*	@return	True when settings are valid and derived values were produced.
**/
const bool SVG_Nav3_DeriveBuildConfigCells( nav3_build_config_t *inOutBuildConfig ) {
	// Require writable build config storage.
	if ( !inOutBuildConfig ) {
		return false;
	}

	// Validate world-unit dimensions before converting into cell units.
	if ( inOutBuildConfig->cell_size <= 0.0f || inOutBuildConfig->cell_height <= 0.0f ) {
		return false;
	}
	if ( inOutBuildConfig->agent_height_units <= 0.0f ) {
		return false;
	}
	if ( inOutBuildConfig->agent_step_height_units < 0.0f ) {
		return false;
	}
	if ( inOutBuildConfig->agent_radius_units < 0.0f ) {
		return false;
	}

	// Convert world-unit standing height into quantized vertical cell clearance.
	inOutBuildConfig->walkable_height_cells = std::max( 1,
		static_cast<int32_t>( std::ceil( inOutBuildConfig->agent_height_units / inOutBuildConfig->cell_height ) ) );
	// Convert world-unit step/climb allowance into quantized vertical climb cells.
	inOutBuildConfig->walkable_climb_cells = std::max( 0,
		static_cast<int32_t>( std::ceil( inOutBuildConfig->agent_step_height_units / inOutBuildConfig->cell_height ) ) );
	// Convert world-unit horizontal radius into quantized erosion radius cells.
	inOutBuildConfig->walkable_radius_cells = std::max( 1,
		static_cast<int32_t>( std::ceil( inOutBuildConfig->agent_radius_units / inOutBuildConfig->cell_size ) ) );
	return true;
}

/**
*	@brief	Return readable name for a nav3 ref type.
*	@param	refType	Ref type enum to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_RefTypeName( const nav3_ref_type_t refType ) {
	switch ( refType ) {
	case nav3_ref_type_t::Invalid:
		return "invalid";
	case nav3_ref_type_t::Span:
		return "span";
	case nav3_ref_type_t::Edge:
		return "edge";
	case nav3_ref_type_t::Portal:
		return "portal";
	case nav3_ref_type_t::Cover:
		return "cover";
	case nav3_ref_type_t::JumpLink:
		return "jump_link";
	case nav3_ref_type_t::MoverLink:
		return "mover_link";
	case nav3_ref_type_t::OctreeLeaf:
		return "octree_leaf";
	case nav3_ref_type_t::Count:
	default:
		break;
	}
	return "unknown";
}

/**
*	@brief	Return readable name for a nav3 telemetry bucket.
*	@param	bucket	Telemetry bucket enum to convert.
*	@return	Stable label string.
**/
const char *SVG_Nav3_TelemetryBucketName( const nav3_telemetry_bucket_t bucket ) {
	switch ( bucket ) {
	case nav3_telemetry_bucket_t::Generation:
		return "generation";
	case nav3_telemetry_bucket_t::Load:
		return "load";
	case nav3_telemetry_bucket_t::Query:
		return "query";
	case nav3_telemetry_bucket_t::DirtyRebuild:
		return "dirty_rebuild";
	case nav3_telemetry_bucket_t::Avoidance:
		return "avoidance";
	case nav3_telemetry_bucket_t::Count:
	default:
		break;
	}
	return "unknown";
}

/**
*	@brief	Return true when host byte-order is little-endian.
**/
const bool SVG_Nav3_IsLittleEndianHost( void ) {
	const uint16_t probe = 0x0102;
	const uint8_t *probeBytes = reinterpret_cast<const uint8_t *>( &probe );
	return probeBytes[ 0 ] == 0x02;
}

/**
*	@brief	Byte-swap a 16-bit unsigned value.
**/
uint16_t SVG_Nav3_ByteSwapU16( const uint16_t value ) {
	return static_cast<uint16_t>( ( ( value & 0x00FFu ) << 8 ) | ( ( value & 0xFF00u ) >> 8 ) );
}

/**
*	@brief	Byte-swap a 32-bit unsigned value.
**/
uint32_t SVG_Nav3_ByteSwapU32( const uint32_t value ) {
	return ( ( value & 0x000000FFu ) << 24 )
		| ( ( value & 0x0000FF00u ) << 8 )
		| ( ( value & 0x00FF0000u ) >> 8 )
		| ( ( value & 0xFF000000u ) >> 24 );
}

/**
*	@brief	Compute deterministic FNV-1a 32-bit checksum over arbitrary bytes.
*	@param	data	Input bytes.
*	@param	byteCount	Input byte count.
*	@return	FNV-1a 32-bit checksum.
**/
uint32_t SVG_Nav3_ChecksumFNV1a32( const void *data, const size_t byteCount ) {
	// Return canonical empty-buffer FNV-1a offset basis for invalid input pointers.
	if ( !data || byteCount == 0 ) {
		return 2166136261u;
	}

	const uint8_t *bytes = reinterpret_cast<const uint8_t *>( data );
	uint32_t hash = 2166136261u;
	// Iterate each byte once using standard FNV-1a xor/multiply updates.
	for ( size_t i = 0; i < byteCount; i++ ) {
		hash ^= bytes[ i ];
		hash *= 16777619u;
	}
	return hash;
}

/**
*	@brief	Reset fixed scratch allocator usage to zero.
*	@param	scratch	Scratch descriptor to reset.
**/
void SVG_Nav3_FixedScratchReset( nav3_fixed_scratch_span_t *scratch ) {
	if ( !scratch ) {
		return;
	}
	scratch->used = 0;
}

/**
*	@brief	Allocate aligned bytes from fixed scratch storage.
*	@param	scratch	Scratch descriptor.
*	@param	byteCount	Requested byte count.
*	@param	alignment	Power-of-two byte alignment requirement.
*	@return	Pointer to allocated bytes or `nullptr` when capacity/inputs are invalid.
**/
void *SVG_Nav3_FixedScratchAlloc( nav3_fixed_scratch_span_t *scratch, const size_t byteCount, const size_t alignment ) {
	// Require valid allocator state and non-empty allocation request.
	if ( !scratch || !scratch->bytes || byteCount == 0 ) {
		return nullptr;
	}

	// Reject non-power-of-two alignment requirements.
	if ( alignment == 0 || ( alignment & ( alignment - 1 ) ) != 0 ) {
		return nullptr;
	}

	// Align current offset before capacity checks.
	const size_t alignedUsed = ( scratch->used + ( alignment - 1 ) ) & ~( alignment - 1 );
	if ( alignedUsed > scratch->capacity ) {
		return nullptr;
	}
	if ( byteCount > ( scratch->capacity - alignedUsed ) ) {
		return nullptr;
	}

	// Commit allocation and return pointer into caller-owned scratch bytes.
	void *allocation = scratch->bytes + alignedUsed;
	scratch->used = alignedUsed + byteCount;
	return allocation;
}
