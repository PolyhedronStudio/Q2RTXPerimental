/********************************************************************
*
*
*	ServerGame: Memory Management - Zone Allocator Adapter
*
*	Standard C++ compliant stateless allocator backed by engine TagMalloc/TagFree
*	for server-game dynamic standard containers.
*
*
********************************************************************/
#pragma once

#include <cstddef>
#include <cstdint>
#include <climits>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

// Zone memory tags definition.
#include "shared/zone_tags.h"

/**
*	@brief	Statistics structure tracking zone allocations per memory tag.
**/
struct svg_zone_allocator_stats_t {
	//! Currently active live allocations.
	size_t active_allocations;
	//! Currently active allocated payload bytes.
	size_t active_bytes;
	//! High-water peak allocated payload bytes.
	size_t peak_bytes;
	//! Total cumulative allocation operations.
	size_t total_allocations;
	//! Total cumulative bytes allocated across lifetime.
	size_t total_allocated_bytes;
	//! Total cumulative bytes released across lifetime.
	size_t total_freed_bytes;
};

/**
*	@brief	Non-template bridge function to allocate tagged memory through the engine zone allocator.
*	@param	bytes		Number of payload bytes to allocate.
*	@param	alignment	Required alignment for the allocated memory block.
*	@param	tag			Engine zone memory tag.
*	@return	Pointer to the aligned memory block, or nullptr on failure.
**/
void *SVG_ZoneAllocator_Allocate( const size_t bytes, const size_t alignment, const memtag_t tag );

/**
*	@brief	Non-template bridge function to free tagged memory through the engine zone allocator.
*	@param	ptr			Pointer to the memory block previously returned by SVG_ZoneAllocator_Allocate.
*	@param	bytes		Number of payload bytes originally requested.
*	@param	alignment	Alignment originally requested.
*	@param	tag			Engine zone memory tag.
**/
void SVG_ZoneAllocator_Deallocate( void *ptr, const size_t bytes, const size_t alignment, const memtag_t tag );

/**
*	@brief	Query runtime zone allocator metrics for a specific memory tag.
*	@param	tag			Memory tag to inspect.
*	@param	outStats	[out] Pointer to destination statistics structure.
**/
void SVG_ZoneAllocator_GetStats( const memtag_t tag, svg_zone_allocator_stats_t *outStats );

/**
*	@brief	Reset runtime statistics counters for a specific memory tag.
*	@param	tag			Memory tag to reset.
**/
void SVG_ZoneAllocator_ResetStats( const memtag_t tag );

/**
*	@brief	Report zone memory allocation accounting across all server-game tags to console.
**/
void SVG_ZoneAllocator_ReportStats( void );

/**
*	@brief	Configure simulated allocation fault injection for test verification.
*	@param	enabled				True to enable fault injection, false for normal allocation.
*	@param	failAfterCount		Number of successful allocations before injecting failures (-1 to fail immediately).
**/
void SVG_ZoneAllocator_SetFaultInjection( const bool enabled, const int32_t failAfterCount );

/**
*	@brief	Check whether fault injection is currently triggered.
*	@return	True if the current allocation should fail.
**/
bool SVG_ZoneAllocator_ShouldInjectFault( void );

/**
*	@brief	Standard C++ compliant stateless allocator adapter backed by engine TagMalloc/TagFree.
*	@tparam	T		Element value type to allocate.
*	@tparam	Tag		Compile-time engine memory tag (defaults to TAG_SVGAME).
**/
template <typename T, memtag_t Tag = TAG_SVGAME>
class svg_zone_allocator_t {
public:
	//! Element type.
	using value_type = T;
	//! Size type.
	using size_type = size_t;
	//! Pointer difference type.
	using difference_type = ptrdiff_t;
	//! Propagate allocator on container move assignment.
	using propagate_on_container_move_assignment = std::true_type;
	//! Do not propagate allocator on copy assignment (stateless, identical tag).
	using propagate_on_container_copy_assignment = std::false_type;
	//! Do not propagate allocator on swap.
	using propagate_on_container_swap = std::false_type;
	//! All instances of the same allocator type specialization are equal and interchangeable.
	using is_always_equal = std::true_type;

	/**
	*	@brief	Allocator rebound type for node/bucket allocation in standard associative containers.
	*	@tparam	U	Rebound element type.
	**/
	template <typename U>
	struct rebind {
		//! Rebound allocator type preserving the compile-time memory tag.
		using other = svg_zone_allocator_t<U, Tag>;
	};

	/**
	*	@brief	Default constructor.
	**/
	constexpr svg_zone_allocator_t() noexcept = default;

	/**
	*	@brief	Copy constructor.
	**/
	constexpr svg_zone_allocator_t( const svg_zone_allocator_t & ) noexcept = default;

	/**
	*	@brief	Converting constructor from another svg_zone_allocator_t sharing the same memory tag.
	*	@tparam	U	Source allocator element type.
	**/
	template <typename U>
	constexpr svg_zone_allocator_t( const svg_zone_allocator_t<U, Tag> & ) noexcept {}

	/**
	*	@brief	Destructor.
	**/
	~svg_zone_allocator_t() = default;

	/**
	*	@brief	Calculate maximum theoretically allowable element count for this type without overflow.
	*	@return	Maximum number of elements representable within engine INT_MAX payload limits.
	**/
	[[nodiscard]] constexpr size_type max_size() const noexcept {
		// Guard against zero division or overflow by clamping to engine INT_MAX payload limit.
		constexpr size_type limit = ( sizeof( T ) > 0 ) ? ( static_cast<size_type>( INT_MAX ) / sizeof( T ) ) : 0;
		return limit;
	}

	/**
	*	@brief	Allocate uninitialized storage for n elements of type T using engine TagMalloc.
	*	@param	n	Number of elements to allocate.
	*	@return	Pointer to the beginning of the newly allocated memory block.
	**/
	[[nodiscard]] T *allocate( const size_type n ) {
		/**
		*	Sanity check: zero-count allocations return nullptr as standard empty handling.
		**/
		if ( n == 0 ) {
			return nullptr;
		}

		/**
		*	Guard element-count multiplication against integer overflow and engine INT_MAX limit.
		**/
		if ( n > max_size() ) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
			throw std::bad_alloc();
#else
			return nullptr;
#endif
		}

		// Compute total payload size in bytes.
		const size_type total_bytes = n * sizeof( T );

		/**
		*	Delegate allocation and alignment handling to the non-template engine bridge.
		**/
		void *memory = SVG_ZoneAllocator_Allocate( total_bytes, alignof( T ), Tag );
		if ( !memory ) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
			throw std::bad_alloc();
#else
			return nullptr;
#endif
		}

		return static_cast<T *>( memory );
	}

	/**
	*	@brief	Deallocate storage pointed to by p using engine TagFree.
	*	@param	p	Pointer to memory block to release.
	*	@param	n	Number of elements originally requested during allocation.
	**/
	void deallocate( T *p, const size_type n ) noexcept {
		/**
		*	Sanity check: null pointers require no deallocation.
		**/
		if ( !p ) {
			return;
		}

		// Delegate deallocation to the non-template bridge.
		SVG_ZoneAllocator_Deallocate( static_cast<void *>( p ), n * sizeof( T ), alignof( T ), Tag );
	}
};

/**
*	@brief	Equality operator: two allocators with the same tag share backing domains and are interchangeable.
**/
template <typename T1, typename T2, memtag_t Tag1, memtag_t Tag2>
constexpr bool operator==( const svg_zone_allocator_t<T1, Tag1> &, const svg_zone_allocator_t<T2, Tag2> & ) noexcept {
	return Tag1 == Tag2;
}

/**
*	@brief	Inequality operator: allocators with distinct tags represent separate memory domains.
**/
template <typename T1, typename T2, memtag_t Tag1, memtag_t Tag2>
constexpr bool operator!=( const svg_zone_allocator_t<T1, Tag1> &, const svg_zone_allocator_t<T2, Tag2> & ) noexcept {
	return Tag1 != Tag2;
}
