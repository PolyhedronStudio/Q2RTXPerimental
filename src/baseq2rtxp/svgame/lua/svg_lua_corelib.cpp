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
void CoreLib_Initialize( lua_State *L ) {
	// We create a new table
	lua_newtable( L );

	// Here we set all functions from CoreLib array into
	// the table on the top of the stack
	luaL_setfuncs( L, CoreLib, 0 );

	// We get the table and set as global variable
	lua_setglobal( L, "Core" );

	// Process string.
	if ( luaL_dostring( L, "Core.DPrint(\"Initialized Lua CoreLib\n\"" ) == LUA_OK ) {
		// Pop stack.
		lua_pop( L, lua_gettop( L ) );
	}
}