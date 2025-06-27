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
*	@brief	Enumeration for time types.
**/
typedef enum timeType_s {
	TIME_MILLISECONDS = 0,
	TIME_SECONDS,
	TIME_MINUTES
} timeType_t;
/**
*	@return	Returns the current time in timeType measurement, for the current (possibly client-) frame.
**/
static const int64_t CoreLib_GetWorldTime( const timeType_t timeType ) {
	if ( timeType == TIME_MILLISECONDS ) {
		return level.time.Milliseconds();
	} else if ( timeType == TIME_SECONDS ) {
		return level.time.Seconds();
	} else if ( timeType == TIME_MINUTES ) {
		return level.time.Minutes();
		// Unknown type, just return milliseconds then.
	} else {
		return level.time.Milliseconds();
	}
}



/**
*	@brief	Initializes the EntitiesLib
**/
void CoreLib_Initialize( sol::state_view &solStateView ) {
	// Namespace name.
	constexpr const char *nameSpaceName = "Core";

	// Create namespace.
	sol::table solNameSpace = solStateView[ nameSpaceName ].get_or_create< sol::table >();
	solNameSpace.set_function( "DPrint", CoreLib_DPrint );

	//
	// Time Type:
	//
	solStateView.new_enum( "TimeType", 
		//! Prints using default conchars text and thus color.
		"MILLISECONDS", PRINT_ALL,
		"SECONDS", PRINT_TALK,
		"MINUTES", PRINT_DEVELOPER
	);
	// Time.
	solNameSpace.set_function( "GetWorldTime", CoreLib_GetWorldTime );

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
	if ( developer && developer->integer > 0 ) {
		solStateView.set( "DEVELOPER", 1 );
	} else {
		solStateView.set( "DEVELOPER", 0 );
	}
	
	// None..
}