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
#pragma once

/**
*	@brief	Run comprehensive unit tests validating svg_zone_allocator_t and all svg_*_t container aliases.
*	@return	True if all unit tests pass cleanly, false if any test fails.
**/
bool SVG_Containers_RunUnitTests( void );

/**
*	@brief	Console command handler for sv nav_alloc_test.
**/
void SVG_Containers_RunUnitTestsCommand( void );
