/********************************************************************
*
*
*	ServerGame: Memory Management - Zone Allocator Implementation
*
*	Provides non-template bridge functions connecting svg_zone_allocator_t
*	to engine TagMalloc/TagFree, tracking per-tag allocation statistics,
*	handling over-alignment shims, supporting pre-initialization CRT
*	fallback for static constructors, and providing test fault injection.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/memory/svg_zone_allocator.h"

#include <cstdint>
#include <cstring>
#include <climits>
#include <cstdlib>
#include <algorithm>

//! Magic signature identifying zone allocator block headers ('Q2ZN' = 0x51325A4E).
static constexpr uint32_t kZoneHeaderMagic = 0x51325A4E;

//! Internal allocation header prepended to every memory block allocated by SVG_ZoneAllocator_Allocate.
struct zone_allocation_header_t {
	//! Magic value for corruption / validity checks.
	uint32_t magic;
	//! Flag: 1 if allocated via standard CRT malloc (fallback before gi.TagMalloc was bound), 0 if via gi.TagMalloc.
	uint32_t is_crt_fallback;
	//! Memory tag for telemetry tracking.
	uint32_t tag;
	//! Padding to preserve 8-byte member alignment.
	uint32_t padding;
	//! Payload size requested in bytes.
	size_t payload_bytes;
	//! Original raw pointer returned by malloc / gi.TagMalloc.
	void *raw_ptr;
};

//! Accounting statistics tracked per zone tag within the server-game module.
static svg_zone_allocator_stats_t s_zone_tag_stats[ TAG_MAX + 1 ] = {};

//! Active allocation count made via pre-initialization CRT fallback.
static size_t s_zone_crt_fallback_active_allocs = 0;

//! Total allocation count made via pre-initialization CRT fallback.
static size_t s_zone_crt_fallback_total_allocs = 0;

//! Flag controlling allocation fault injection testing.
static bool s_zone_fault_injection_enabled = false;

//! Count of successful allocations before fault injection triggers failure.
static int32_t s_zone_fault_injection_countdown = -1;

/**
*	@brief	Configure simulated allocation fault injection for test verification.
*	@param	enabled			True to enable fault injection, false for normal allocation.
*	@param	failAfterCount	Number of successful allocations before injecting failures (-1 to fail immediately).
**/
void SVG_ZoneAllocator_SetFaultInjection( const bool enabled, const int32_t failAfterCount ) {
	/**
	*	Update module fault-injection state for single-threaded server-owner test execution.
	**/
	// Enable or disable fault injection.
	s_zone_fault_injection_enabled = enabled;
	// Record the remaining successful allocation count before failure triggers.
	s_zone_fault_injection_countdown = failAfterCount;
}

/**
*	@brief	Check whether fault injection is currently triggered.
*	@return	True if the current allocation should fail.
**/
bool SVG_ZoneAllocator_ShouldInjectFault( void ) {
	/**
	*	Evaluate fault-injection state and advance countdown when active.
	**/
	// If fault injection is not enabled, proceed normally.
	if ( !s_zone_fault_injection_enabled ) {
		return false;
	}

	// If countdown reaches 0 or was negative, trigger immediate allocation failure.
	if ( s_zone_fault_injection_countdown <= 0 ) {
		return true;
	}

	// Decrement remaining successful allocations.
	s_zone_fault_injection_countdown--;
	return false;
}

/**
*	@brief	Non-template bridge function to allocate tagged memory through the engine zone allocator.
*	@param	bytes		Number of payload bytes to allocate.
*	@param	alignment	Required alignment for the allocated memory block.
*	@param	tag			Engine zone memory tag.
*	@return	Pointer to the aligned memory block, or nullptr on failure.
*	@note	If engine imports (gi.TagMalloc) are not yet bound (such as during static C++ constructor
*			execution upon DLL loading), gracefully falls back to CRT malloc with header tracking.
**/
void *SVG_ZoneAllocator_Allocate( const size_t bytes, const size_t alignment, const memtag_t tag ) {
	/**
	*	Sanity checks: validate zero size, fault injection, and INT_MAX limits.
	**/
	// Return null immediately if zero bytes requested.
	if ( bytes == 0 ) {
		return nullptr;
	}

	// Check for simulated fault injection during testing.
	if ( SVG_ZoneAllocator_ShouldInjectFault() ) {
		return nullptr;
	}

	// Guard against allocations exceeding the engine INT_MAX zone payload limit.
	if ( bytes > static_cast<size_t>( INT_MAX ) ) {
		// Log diagnostic error if engine developer printing is bound.
		if ( gi.dprintf != nullptr ) {
			gi.dprintf( "%s: allocation request for %zu bytes exceeds INT_MAX payload limit!\n", __func__, bytes );
		}
		return nullptr;
	}

	/**
	*	Calculate total required size including the block header and alignment padding.
	**/
	// Minimum alignment must satisfy void* pointer alignment for the header and raw pointer.
	const size_t eff_align = std::max( alignment, alignof( void * ) );
	constexpr size_t header_size = sizeof( zone_allocation_header_t );

	// Total allocation must cover payload, header, and maximum possible alignment offset.
	const size_t total_alloc_bytes = bytes + header_size + ( eff_align - 1 );
	if ( total_alloc_bytes > static_cast<size_t>( INT_MAX ) ) {
		return nullptr;
	}

	/**
	*	Allocate raw backing memory: route through gi.TagMalloc if engine imports are bound;
	*	otherwise fall back to CRT std::malloc during pre-initialization static construction.
	**/
	void *raw = nullptr;
	bool is_crt = false;

	// Route allocation to engine zone allocator when available, or CRT malloc before initialization.
	if ( gi.TagMalloc != nullptr ) {
		raw = gi.TagMalloc( static_cast<uint32_t>( total_alloc_bytes ), tag );
	} else {
		raw = std::malloc( total_alloc_bytes );
		is_crt = true;
	}

	// Bail out if allocation failed.
	if ( raw == nullptr ) {
		return nullptr;
	}

	/**
	*	Compute aligned payload address and store header immediately preceding it.
	**/
	const uintptr_t raw_addr = reinterpret_cast<uintptr_t>( raw );
	const uintptr_t payload_addr = ( raw_addr + header_size + ( eff_align - 1 ) ) & ~( static_cast<uintptr_t>( eff_align - 1 ) );

	// Store header in the reserved space directly preceding payload_addr.
	zone_allocation_header_t *hdr = reinterpret_cast<zone_allocation_header_t *>( payload_addr - header_size );
	hdr->magic = kZoneHeaderMagic;
	hdr->is_crt_fallback = is_crt ? 1 : 0;
	hdr->tag = static_cast<uint32_t>( tag );
	hdr->padding = 0;
	hdr->payload_bytes = bytes;
	hdr->raw_ptr = raw;

	/**
	*	Update runtime telemetry metrics for the designated tag.
	**/
	const int32_t tagIndex = static_cast<int32_t>( tag );
	// Record allocation counts and byte high-water marks when the tag index is in range.
	if ( tagIndex >= 0 && tagIndex <= TAG_MAX ) {
		s_zone_tag_stats[ tagIndex ].active_allocations++;
		s_zone_tag_stats[ tagIndex ].active_bytes += bytes;
		// Update peak active byte watermark if exceeded.
		if ( s_zone_tag_stats[ tagIndex ].active_bytes > s_zone_tag_stats[ tagIndex ].peak_bytes ) {
			s_zone_tag_stats[ tagIndex ].peak_bytes = s_zone_tag_stats[ tagIndex ].active_bytes;
		}
		s_zone_tag_stats[ tagIndex ].total_allocations++;
		s_zone_tag_stats[ tagIndex ].total_allocated_bytes += bytes;
	}

	// Track pre-initialization CRT fallback allocations separately.
	if ( is_crt ) {
		s_zone_crt_fallback_active_allocs++;
		s_zone_crt_fallback_total_allocs++;
	}

	return reinterpret_cast<void *>( payload_addr );
}

/**
*	@brief	Non-template bridge function to free tagged memory through the engine zone allocator.
*	@param	ptr			Pointer to the memory block previously returned by SVG_ZoneAllocator_Allocate.
*	@param	bytes		Number of payload bytes originally requested.
*	@param	alignment	Alignment originally requested.
*	@param	tag			Engine zone memory tag.
**/
void SVG_ZoneAllocator_Deallocate( void *ptr, const size_t bytes, const size_t alignment, const memtag_t tag ) {
	( void )bytes;
	( void )alignment;
	( void )tag;
	/**
	*	Sanity check: null pointer requires no deallocation.
	**/
	if ( ptr == nullptr ) {
		return;
	}

	/**
	*	Locate and validate block header immediately preceding the payload.
	**/
	constexpr size_t header_size = sizeof( zone_allocation_header_t );
	zone_allocation_header_t *hdr = reinterpret_cast<zone_allocation_header_t *>(
		reinterpret_cast<char *>( ptr ) - header_size
	);

	// Verify header magic signature to guard against corruption or mismatched deallocation.
	if ( hdr->magic != kZoneHeaderMagic ) {
		return;
	}

	const bool is_crt = ( hdr->is_crt_fallback != 0 );
	void *raw = hdr->raw_ptr;
	const size_t alloc_bytes = hdr->payload_bytes;
	const int32_t tagIndex = static_cast<int32_t>( hdr->tag );

	// Invalidate magic signature to detect and prevent double-free bugs.
	hdr->magic = 0;

	/**
	*	Update runtime accounting metrics.
	**/
	// Decrement active allocation and byte counters for valid tag indices.
	if ( tagIndex >= 0 && tagIndex <= TAG_MAX ) {
		if ( s_zone_tag_stats[ tagIndex ].active_allocations > 0 ) {
			s_zone_tag_stats[ tagIndex ].active_allocations--;
		}
		if ( s_zone_tag_stats[ tagIndex ].active_bytes >= alloc_bytes ) {
			s_zone_tag_stats[ tagIndex ].active_bytes -= alloc_bytes;
		} else {
			s_zone_tag_stats[ tagIndex ].active_bytes = 0;
		}
		s_zone_tag_stats[ tagIndex ].total_freed_bytes += alloc_bytes;
	}

	// Decrement active CRT fallback count when releasing a pre-initialization block.
	if ( is_crt ) {
		if ( s_zone_crt_fallback_active_allocs > 0 ) {
			s_zone_crt_fallback_active_allocs--;
		}
	}

	/**
	*	Free raw block: route CRT fallback blocks to std::free;
	*	route engine blocks to gi.TagFree if bound.
	**/
	if ( is_crt ) {
		std::free( raw );
	} else if ( gi.TagFree != nullptr ) {
		gi.TagFree( raw );
	}
}

/**
*	@brief	Query runtime zone allocator metrics for a specific memory tag.
*	@param	tag			Memory tag to inspect.
*	@param	outStats	[out] Pointer to destination statistics structure.
**/
void SVG_ZoneAllocator_GetStats( const memtag_t tag, svg_zone_allocator_stats_t *outStats ) {
	/**
	*	Sanity check output pointer and copy tag accounting snapshot.
	**/
	// Bail out if caller did not provide an output structure.
	if ( !outStats ) {
		return;
	}
	const int32_t tagIndex = static_cast<int32_t>( tag );
	// Copy recorded statistics for valid tags, or zero-initialize on out-of-range tag.
	if ( tagIndex >= 0 && tagIndex <= TAG_MAX ) {
		*outStats = s_zone_tag_stats[ tagIndex ];
	} else {
		*outStats = {};
	}
}

/**
*	@brief	Reset runtime statistics counters for a specific memory tag.
*	@param	tag			Memory tag to reset.
**/
void SVG_ZoneAllocator_ResetStats( const memtag_t tag ) {
	/**
	*	Clear accounting counters for the specified tag if within valid bounds.
	**/
	const int32_t tagIndex = static_cast<int32_t>( tag );
	// Reset statistics entry when the tag index is valid.
	if ( tagIndex >= 0 && tagIndex <= TAG_MAX ) {
		s_zone_tag_stats[ tagIndex ] = {};
	}
}

/**
*	@brief	Report zone memory allocation accounting across all server-game tags to console.
**/
void SVG_ZoneAllocator_ReportStats( void ) {
	/**
	*	Verify console print import availability before formatting report table.
	**/
	// Return early if console printing is unavailable.
	if ( gi.cprintf == nullptr ) {
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "\n=== SERVER-GAME ZONE ALLOCATOR ACCOUNTING ===\n" );
	gi.cprintf( nullptr, PRINT_HIGH, "%-22s %10s %12s %12s %10s\n", "Tag Name", "Active Allocs", "Active Bytes", "Peak Bytes", "Total Allocs" );
	gi.cprintf( nullptr, PRINT_HIGH, "---------------------- ------------ ------------ ------------ ----------\n" );

	// Array of tag names for server-game relevant tags.
	static const struct {
		memtag_t tag;
		const char *name;
	} tagInfo[] = {
		{ TAG_SVGAME,         "TAG_SVGAME" },
		{ TAG_SVGAME_NAVMESH, "TAG_SVGAME_NAVMESH" },
		{ TAG_SVGAME_LEVEL,   "TAG_SVGAME_LEVEL" },
		{ TAG_SVGAME_EDICTS,  "TAG_SVGAME_EDICTS" },
		{ TAG_SVGAME_LUA,     "TAG_SVGAME_LUA" }
	};

	size_t grandActiveAllocs = 0;
	size_t grandActiveBytes = 0;

	for ( size_t i = 0; i < sizeof( tagInfo ) / sizeof( tagInfo[ 0 ] ); i++ ) {
		const memtag_t tag = tagInfo[ i ].tag;
		const int32_t tagIndex = static_cast<int32_t>( tag );
		const svg_zone_allocator_stats_t &stats = s_zone_tag_stats[ tagIndex ];

		gi.cprintf( nullptr, PRINT_HIGH, "%-22s %10zu %12zu %12zu %10zu\n",
			tagInfo[ i ].name,
			stats.active_allocations,
			stats.active_bytes,
			stats.peak_bytes,
			stats.total_allocations
		);

		grandActiveAllocs += stats.active_allocations;
		grandActiveBytes += stats.active_bytes;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "---------------------- ------------ ------------ ------------ ----------\n" );
	gi.cprintf( nullptr, PRINT_HIGH, "%-22s %10zu %12zu\n", "Total Live", grandActiveAllocs, grandActiveBytes );
	gi.cprintf( nullptr, PRINT_HIGH, "Pre-Init CRT Fallback: %zu active allocs, %zu total allocs\n\n",
		s_zone_crt_fallback_active_allocs, s_zone_crt_fallback_total_allocs );
}
