/********************************************************************
*
*
*
*	ServerGmae: Lua Library: Core
*
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"



/**
*
*
*
*	Lua CoreLib:
*
*
*
**/
/**
*	@brief	Prints the given string.
**/
static const int32_t CoreLib_DPrint( std::string string ) {
	// Print.
	gi.dprintf( "%s", string.c_str() );
	return 1;
}



/**
*	@brief	Initializes the EntitiesLib
**/
void CoreLib_Initialize( sol::state_view &solStateView ) {
	// Namespace name.
	constexpr const char *nameSpaceName = "Core";

	// Create namespace.
	auto solNameSpace = solStateView[ nameSpaceName ].get_or_create< sol::table >();
	solNameSpace.set_function( "DPrint", CoreLib_DPrint );

	// Developer print.
	gi.dprintf( "[Lua]: %s as -> \"%s\"\n", __func__, nameSpaceName );

	/**
	*	Register all global constants, we don't include these in the namespace.
	*	It makes things easier to write and read. We won't be having duplicates anyway.
	**/
	// Debug Build:
	#ifdef _DEBUG
	solStateView.set( "DEBUGGING", 1 );
	#else
	solStateView.set( "DEBUGGING", 0 );
	#endif
	// Developer mode:
	cvar_t *developer = gi.cvar( "developer", 0, 0 );
	if ( developer && developer->integer != 0 ) {
		solStateView.set( "DEVELOPER", gi.cvar( "developer", 0, 0 )->integer );
	} else {
		solStateView.set( "DEVELOPER", gi.cvar( "developer", 0, 0 ) );
	}
	
	// None..
}