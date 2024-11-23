/********************************************************************
*
*
*	ServerGame: Lua GameLib API.
*	NameSpace: "Game".
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"
#include "svgame/entities/svg_entities_pushermove.h"



/**
*
*
*
*	[Lua GameLib] -> Init/Shutdown:
*
*
*
**/
/**
*	@brief	Game Namespace Functions:
**/
static const struct luaL_Reg GameLib[] = {
	// svg_lua_gamelib_entities.cpp:
	{ "GetEntityForLuaName", GameLib_GetEntityForLuaName },
	{ "GetEntityForTargetName", GameLib_GetEntityForTargetName },
	{ "GetEntitiesForTargetName", GameLib_GetEntitiesForTargetName },

	// svg_lua_gamelib_pushmovers.cpp:
	{ "GetPushMoverState", GameLib_GetPushMoverState },

	// svg_lua_gamelib_usetargets.cpp:
	{ "UseTarget", GameLib_UseTarget },
	{ "UseTargets", GameLib_UseTargets },

	// svg_lua_gamelib_signalio.cpp:
	{ "SignalOut", GameLib_SignalOut },

	// End OF Array.
	{ NULL, NULL }
};
/**
*	@brief	Initializes the GameLib
**/
void GameLib_Initialize( lua_State *L ) {
	// We create a new table
	lua_newtable( L );

	// Here we set all functions from GameLib array into
	// the table on the top of the stack
	luaL_setfuncs( L, GameLib, 0 );

	// We get the table and set as global variable
	lua_setglobal( L, "Game" );

	if ( luaL_dostring( L, "Core.DPrint(\"Initialized Lua GameLib\n\"" ) == LUA_OK ) {
		lua_pop( L, lua_gettop( L ) );
	}

	/**
	*	Register all global constants.
	**/
	// WID: TODO: We don't use these yet, do we even need them?
	// Door Toggle Types:
	LUA_RegisterGlobalConstant( L, "DOOR_TOGGLE_CLOSE", static_cast<const lua_Integer>( 0 ) );
	LUA_RegisterGlobalConstant( L, "DOOR_TOGGLE_OPEN", static_cast<const lua_Integer>( 1 ) );

	// PushMoveInfo States:
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_TOP", static_cast<const lua_Integer>( PUSHMOVE_STATE_TOP ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_BOTTOM", static_cast<const lua_Integer>( PUSHMOVE_STATE_BOTTOM ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_MOVING_UP", static_cast<const lua_Integer>( PUSHMOVE_STATE_MOVING_UP ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_MOVING_DOWN", static_cast<const lua_Integer>( PUSHMOVE_STATE_MOVING_DOWN ) );

	// UseTarget Types:
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_OFF", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_OFF ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_ON", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_ON ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_SET", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_SET ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_TOGGLE", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_TOGGLE ) );
}
