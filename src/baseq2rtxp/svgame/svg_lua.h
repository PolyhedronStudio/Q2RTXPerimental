#pragma once

// Include lua headers.
#include <lua.hpp>
#include <lualib.h>
#include <lauxlib.h>


// (Templated-) Support Lua Utilities:
//! For errors:
#define LUA_ErrorPrintf(...) gi.bprintf( PRINT_WARNING, __VA_ARGS__ );



/**
*
*
*
*	Utilities:
*
*
*
**/
/**
*	@brief	 
**/
static inline const bool LUA_HasFunction( lua_State *L, const std::string &functionName ) {
	// Get the global functionname value and push it to stack:
	lua_getglobal( L, functionName.c_str() );
	// Check if function even exists.
	const bool isFunction = ( lua_isfunction( L, -1 ) ? true : false );
	// Pop from stack.
	lua_pop( L, lua_gettop( L ) );
	// Return result.
	return isFunction;
}

//! For calling into LUA functions:
#include "svgame/lua/svg_lua_callfunction.hpp"
//! For signaling.
#include "svgame/lua/svg_lua_signals.hpp"





/**
*
*
*
*	Lua Core:
*
*
*
**/
/**
*	@brief
**/
void SVG_Lua_Initialize();
/**
*	@brief
**/
void SVG_Lua_Shutdown();
/**
*	@brief
**/
lua_State *SVG_Lua_GetMapLuaState();

/**
*	@brief
**/
void SVG_Lua_LoadMapScript( const std::string &scriptName );



/**
*
*
*
*	Lua Script Calling:
*
*
*
**/



/**
*
*
*
*	Lua Game Callback Hooks:
*
*
*
**/
//
//	For when a map is just loaded up, or about to be unloaded.
//
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginMap();
/**
*	@brief
**/
void SVG_Lua_CallBack_ExitMap();


//
//	For when a client is connecting thus entering the active map(level) 
//	or disconnecting, thus exiting the map(level).
//
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientEnterLevel( edict_t *clientEntity );
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientExitLevel( edict_t *clientEntity );


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginServerFrame();
/**
*	@brief
**/
void SVG_Lua_CallBack_RunFrame();
/**
*	@brief
**/
void SVG_Lua_CallBack_EndServerFrame();

