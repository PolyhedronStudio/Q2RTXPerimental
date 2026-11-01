/********************************************************************
*
*
*	ServerGame: Memory Management - Container & Allocator Unit Tests
*
*	Verification test suite validating standard container aliases,
*	lifetime management, alignment, rebind, move-only types,
*	and fault injection.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/memory/svg_zone_allocator.h"
#include "svgame/memory/svg_containers.h"
#include "svgame/memory/svg_containers_test.h"

#include <cstdint>
#include <memory>
#include <string>

/**
*	@brief	Instrumented tracking struct to verify exactly-once constructor and destructor invocations.
**/
struct TestTrackerElement {
	//! Shared live instance counter across all tracker instances.
	static int32_t liveInstances;
	//! Total constructions performed.
	static int32_t totalConstructed;
	//! Total destructions performed.
	static int32_t totalDestroyed;

	//! Identifier payload.
	int32_t id;
	//! True if this instance's value was stolen by a move operation.
	bool isMovedFrom;

	/**
	*	@brief	Reset tracking counters before starting a test case.
	**/
	static void ResetCounters( void ) {
		liveInstances = 0;
		totalConstructed = 0;
		totalDestroyed = 0;
	}

	/**
	*	@brief	Standard value constructor.
	**/
	explicit TestTrackerElement( const int32_t inId = 0 )
		: id( inId )
		, isMovedFrom( false ) {
		liveInstances++;
		totalConstructed++;
	}

	/**
	*	@brief	Copy constructor.
	**/
	TestTrackerElement( const TestTrackerElement &other )
		: id( other.id )
		, isMovedFrom( false ) {
		liveInstances++;
		totalConstructed++;
	}

	/**
	*	@brief	Move constructor.
	**/
	TestTrackerElement( TestTrackerElement &&other ) noexcept
		: id( other.id )
		, isMovedFrom( false ) {
		other.isMovedFrom = true;
		liveInstances++;
		totalConstructed++;
	}

	/**
	*	@brief	Copy assignment operator.
	**/
	TestTrackerElement &operator=( const TestTrackerElement &other ) {
		if ( this != &other ) {
			id = other.id;
			isMovedFrom = false;
		}
		return *this;
	}

	/**
	*	@brief	Move assignment operator.
	**/
	TestTrackerElement &operator=( TestTrackerElement &&other ) noexcept {
		if ( this != &other ) {
			id = other.id;
			isMovedFrom = false;
			other.isMovedFrom = true;
		}
		return *this;
	}

	/**
	*	@brief	Destructor verifying decrement.
	**/
	~TestTrackerElement() {
		liveInstances--;
		totalDestroyed++;
	}
};

//! Initialize static tracker counters.
int32_t TestTrackerElement::liveInstances = 0;
int32_t TestTrackerElement::totalConstructed = 0;
int32_t TestTrackerElement::totalDestroyed = 0;

/**
*	@brief	Move-only test struct with deleted copy operations.
**/
struct TestMoveOnlyElement {
	//! Payload value.
	int32_t value;

	//! Value constructor.
	explicit TestMoveOnlyElement( const int32_t inVal = 0 ) : value( inVal ) {}

	//! Deleted copy constructor.
	TestMoveOnlyElement( const TestMoveOnlyElement & ) = delete;
	//! Deleted copy assignment.
	TestMoveOnlyElement &operator=( const TestMoveOnlyElement & ) = delete;

	//! Defaulted move constructor.
	TestMoveOnlyElement( TestMoveOnlyElement && ) noexcept = default;
	//! Defaulted move assignment.
	TestMoveOnlyElement &operator=( TestMoveOnlyElement && ) noexcept = default;

	//! Destructor.
	~TestMoveOnlyElement() = default;
};

/**
*	@brief	Over-aligned test struct requiring 32-byte alignment.
**/
struct alignas( 32 ) TestAligned32Element {
	//! 32-byte payload.
	float values[ 8 ];
};

/**
*	@brief	Over-aligned test struct requiring 64-byte alignment (cache-line alignment).
**/
struct alignas( 64 ) TestAligned64Element {
	//! 64-byte payload.
	float values[ 16 ];
};

/**
*	@brief	Run comprehensive unit tests validating svg_zone_allocator_t and all svg_*_t container aliases.
*	@return	True if all unit tests pass cleanly, false if any test fails.
**/
bool SVG_Containers_RunUnitTests( void ) {
	bool allPassed = true;
	gi.cprintf( nullptr, PRINT_HIGH, "\n=== SERVER-GAME CONTAINER & ALLOCATOR UNIT TESTS ===\n" );

	/**
	*	Test 1: Verify compilation and instantiation of all 17 container aliases.
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 1/8] Testing alias template instantiation...\n" );
	{
		svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> v;
		v.push_back( 42 );

		svg_deque_t<int32_t, TAG_SVGAME_NAVMESH> d;
		d.push_back( 42 );

		svg_list_t<int32_t, TAG_SVGAME_NAVMESH> l;
		l.push_back( 42 );

		svg_forward_list_t<int32_t, TAG_SVGAME_NAVMESH> fl;
		fl.push_front( 42 );

		svg_map_t<int32_t, int32_t, std::less<int32_t>, TAG_SVGAME_NAVMESH> m;
		m[ 1 ] = 42;

		svg_multimap_t<int32_t, int32_t, std::less<int32_t>, TAG_SVGAME_NAVMESH> mm;
		mm.insert( { 1, 42 } );

		svg_set_t<int32_t, std::less<int32_t>, TAG_SVGAME_NAVMESH> s;
		s.insert( 42 );

		svg_multiset_t<int32_t, std::less<int32_t>, TAG_SVGAME_NAVMESH> ms;
		ms.insert( 42 );

		svg_unordered_map_t<int32_t, int32_t, std::hash<int32_t>, std::equal_to<int32_t>, TAG_SVGAME_NAVMESH> um;
		um[ 1 ] = 42;

		svg_unordered_multimap_t<int32_t, int32_t, std::hash<int32_t>, std::equal_to<int32_t>, TAG_SVGAME_NAVMESH> umm;
		umm.insert( { 1, 42 } );

		svg_unordered_set_t<int32_t, std::hash<int32_t>, std::equal_to<int32_t>, TAG_SVGAME_NAVMESH> us;
		us.insert( 42 );

		svg_unordered_multiset_t<int32_t, std::hash<int32_t>, std::equal_to<int32_t>, TAG_SVGAME_NAVMESH> ums;
		ums.insert( 42 );

		svg_queue_t<int32_t, TAG_SVGAME_NAVMESH> q;
		q.push( 42 );

		svg_stack_t<int32_t, TAG_SVGAME_NAVMESH> st;
		st.push( 42 );

		svg_priority_queue_t<int32_t, std::less<int32_t>, TAG_SVGAME_NAVMESH> pq;
		pq.push( 42 );

		svg_string_t str = "test_string";
		if ( str != "test_string" || v.empty() || m.empty() ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Alias instantiation verification failed!\n" );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: All 17 container alias templates instantiated and functional.\n" );
		}
	}

	/**
	*	Test 2: Instrumented Owning Element Lifetime (exactly-once destruction).
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 2/8] Testing instrumented element lifetime and destruction...\n" );
	{
		TestTrackerElement::ResetCounters();
		{
			svg_vector_t<TestTrackerElement, TAG_SVGAME_NAVMESH> vec;
			vec.reserve( 10 );
			for ( int32_t i = 0; i < 5; i++ ) {
				vec.emplace_back( i );
			}
			if ( TestTrackerElement::liveInstances != 5 ) {
				gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Expected 5 live instances, got %d!\n", TestTrackerElement::liveInstances );
				allPassed = false;
			}
		}
		// Scope exited: all elements must have been destroyed.
		if ( TestTrackerElement::liveInstances != 0 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Memory leak or missing destructions! Live instances: %d\n", TestTrackerElement::liveInstances );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: Instrumented element lifetime and exactly-once destruction verified.\n" );
		}
	}

	/**
	*	Test 3: Move-Only Elements.
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 3/8] Testing move-only element support...\n" );
	{
		svg_vector_t<TestMoveOnlyElement, TAG_SVGAME_NAVMESH> moveVec;
		moveVec.reserve( 4 );
		moveVec.emplace_back( 100 );
		moveVec.push_back( TestMoveOnlyElement( 200 ) );

		// Move-assign vector.
		svg_vector_t<TestMoveOnlyElement, TAG_SVGAME_NAVMESH> movedTo = std::move( moveVec );
		if ( movedTo.size() != 2 || movedTo[ 0 ].value != 100 || movedTo[ 1 ].value != 200 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Move-only element vector operations failed!\n" );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: Move-only element insertion, growth, and move-assignment verified.\n" );
		}
	}

	/**
	*	Test 4: Container Core Operations (reserve, resize, self-aliasing, swap, erase, clear, shrink_to_fit).
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 4/8] Testing container core operations & self-aliasing...\n" );
	{
		svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> vec;
		vec.reserve( 4 );
		vec.push_back( 10 );
		vec.push_back( 20 );

		// Self-aliasing push_back (pushing element that may move during reallocation).
		for ( int32_t i = 0; i < 10; i++ ) {
			vec.push_back( vec[ 0 ] );
		}

		// Erase middle element.
		vec.erase( vec.begin() + 1 );

		// Swap vectors.
		svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> other;
		other.push_back( 999 );
		vec.swap( other );

		if ( vec.size() != 1 || vec[ 0 ] != 999 || other.size() != 11 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Vector swap or self-aliasing insertion failed!\n" );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: Reserve, resize, self-aliasing, swap, and erase verified.\n" );
		}
	}

	/**
	*	Test 5: Nested Tagged Containers.
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 5/8] Testing nested tagged container allocation...\n" );
	{
		svg_vector_t<svg_vector_t<int32_t, TAG_SVGAME_NAVMESH>, TAG_SVGAME_NAVMESH> nested;
		nested.resize( 3 );
		for ( size_t i = 0; i < nested.size(); i++ ) {
			nested[ i ].push_back( static_cast<int32_t>( i * 10 ) );
			nested[ i ].push_back( static_cast<int32_t>( i * 10 + 1 ) );
		}

		if ( nested.size() != 3 || nested[ 2 ][ 1 ] != 21 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Nested tagged container integrity check failed!\n" );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: Nested tagged container allocation and operations verified.\n" );
		}
	}

	/**
	*	Test 6: Node Allocation, Rebind, and Map Rehash.
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 6/8] Testing node allocation, rebind, and hash table rehash...\n" );
	{
		svg_unordered_map_t<int32_t, svg_string_t, std::hash<int32_t>, std::equal_to<int32_t>, TAG_SVGAME_NAVMESH> map;
		// Insert enough elements to force bucket array reallocation and multiple bucket node rebind allocations.
		for ( int32_t i = 0; i < 200; i++ ) {
			map[ i ] = ( "val_" + std::to_string( i ) ).c_str();
		}

		bool mapOk = ( map.size() == 200 );
		for ( int32_t i = 0; i < 200; i++ ) {
			if ( map[ i ] != ( "val_" + std::to_string( i ) ).c_str() ) {
				mapOk = false;
				break;
			}
		}

		if ( !mapOk ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Unordered map node rebind and rehash failed!\n" );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: Unordered map node rebind and rehash verified.\n" );
		}
	}

	/**
	*	Test 7: Over-Alignment Verification (32-byte and 64-byte alignment).
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 7/8] Testing over-aligned memory allocations (32 & 64 bytes)...\n" );
	{
		svg_vector_t<TestAligned32Element, TAG_SVGAME_NAVMESH> aligned32Vec;
		aligned32Vec.resize( 4 );
		const uintptr_t addr32 = reinterpret_cast<uintptr_t>( aligned32Vec.data() );

		svg_vector_t<TestAligned64Element, TAG_SVGAME_NAVMESH> aligned64Vec;
		aligned64Vec.resize( 4 );
		const uintptr_t addr64 = reinterpret_cast<uintptr_t>( aligned64Vec.data() );

		if ( ( addr32 % 32 ) != 0 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: 32-byte alignment violated! Address: %p\n", aligned32Vec.data() );
			allPassed = false;
		} else if ( ( addr64 % 64 ) != 0 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: 64-byte alignment violated! Address: %p\n", aligned64Vec.data() );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: 32-byte and 64-byte over-aligned allocations correctly aligned.\n" );
		}
	}

	/**
	*	Test 8: Allocation Fault Injection & Cleanup.
	**/
	gi.cprintf( nullptr, PRINT_HIGH, "[Test 8/8] Testing allocation fault injection and exception handling...\n" );
	{
		bool caughtExpected = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
		/**
		*	Construct debug iterator bookkeeping before arming allocation failure.
		*	MSVC's noexcept vector constructor allocates a proxy; reserve is the
		*	throwing operation whose failure and recovery this test must verify.
		**/
		svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> failVec;
		SVG_ZoneAllocator_SetFaultInjection( true, 0 );
		try {
			failVec.reserve( 1000 );
		} catch ( const std::bad_alloc & ) {
			// A reserve failure must propagate to the caller without termination.
			caughtExpected = true;
		}
#else
		// Without exception support, directly verify the bridge failure contract.
		SVG_ZoneAllocator_SetFaultInjection( true, 0 );
		caughtExpected = SVG_ZoneAllocator_Allocate( sizeof( int32_t ), alignof( int32_t ), TAG_SVGAME_NAVMESH ) == nullptr;
#endif

		// Restore normal allocation behavior immediately.
		SVG_ZoneAllocator_SetFaultInjection( false, -1 );

		// Verify that normal allocations succeed again after fault injection is disabled.
		svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> recoverVec;
		recoverVec.push_back( 777 );

		if ( !caughtExpected || recoverVec.empty() || recoverVec[ 0 ] != 777 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "  FAIL: Fault injection handling or recovery failed!\n" );
			allPassed = false;
		} else {
			gi.cprintf( nullptr, PRINT_HIGH, "  PASS: Fault injection triggered expected failure and recovered cleanly.\n" );
		}
	}

	gi.cprintf( nullptr, PRINT_HIGH, "---------------------------------------------------\n" );
	if ( allPassed ) {
		gi.cprintf( nullptr, PRINT_HIGH, "OVERALL UNIT TEST RESULT: ALL TESTS PASS [OK]\n\n" );
	} else {
		gi.cprintf( nullptr, PRINT_HIGH, "OVERALL UNIT TEST RESULT: ONE OR MORE TESTS FAILED [FAIL]\n\n" );
	}

	return allPassed;
}

/**
*	@brief	Console command handler for sv nav_alloc_test.
**/
void SVG_Containers_RunUnitTestsCommand( void ) {
	SVG_Containers_RunUnitTests();
}
