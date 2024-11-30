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
static int CoreLib_DPrint( lua_State *L ) {
	// Check if the first argument is string.
	const char *printStr = luaL_checkstring( L, 1 );
	if ( !printStr ) {
		Lua_DeveloperPrintf( "%s: invalid string\n", __func__ );
		return 0;
	}

	// Print
	gi.dprintf( "%s", printStr );

	return 0; // The number of returned values
}
/**
*	@brief	Entities Namespace Functions:
**/
static const struct luaL_Reg CoreLib[] = {
	{ "DPrint", CoreLib_DPrint },
	{ NULL, NULL }
};
/**
*	@brief	Initializes the EntitiesLib
**/
void CoreLib_Initialize( sol::state_view &solStateView ) {
	// Namespace name.
	constexpr const char *nameSpaceName = "Core";

	// Create namespace.
	auto solNameSpace = solStateView[ nameSpaceName ].get_or_create< sol::table >();
	solNameSpace[ "DPrint" ]  = CoreLib_DPrint;

	// Developer print.
	gi.dprintf( "[Lua]: %s as -> \"%s\"\n", __func__, nameSpaceName );

	/**
	*	Register all global constants.
	**/
	// Door Toggle Types:
	// WID: TODO: We don't use these yet, do we even need them?
	solStateView.set( "TEST_CONST", 1 );
}