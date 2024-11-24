/********************************************************************
*
*
*	ServerGame: Lua Scripting API.
*
*
********************************************************************/
#pragma once

// Include lua headers.
#include <lua.hpp>
#include <lualib.h>
#include <lauxlib.h>

#if _DEBUG
//! Enable LUA debug output:
#define LUA_DEBUG_OUTPUT 1
#else
//! Disable LUA debug output:
#define LUA_DEBUG_OUTPUT 0
#endif

// (Templated-) Support Lua Utilities:
//! For errors:
#define LUA_ErrorPrintf(...) gi.bprintf( PRINT_ERROR, __VA_ARGS__ );


//! For developer prints:
#if LUA_DEBUG_OUTPUT == 1
	#define Lua_DeveloperPrintf(...) gi.dprintf( __VA_ARGS__ );
#else
	#define Lua_DeveloperPrintf(...) 
#endif

// For checking whether to proceed lua callbacks or not.
inline const bool SVG_Lua_IsMapScriptInterpreted();
#define SVG_Lua_ReturnIfNotInterpretedOK if ( !SVG_Lua_IsMapScriptInterpreted() ) { return; } 


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

/**
*	@brief	Returns true if the global is already defined.
**/
static inline const bool LUA_IsGlobalDefined( lua_State *L, const char *name ) {
	lua_getglobal( L, name );
	const bool result = ( lua_isnone( L, -1 ) ? true : false );
	lua_pop( L, -1 );
	return result;
}
/**
*	@brief	Declares if not already existing, a global variable as 'name = integer'.
**/
static inline void LUA_RegisterGlobalConstant( lua_State *L, const char *name, const lua_Integer integer ) {
	if ( LUA_IsGlobalDefined( L, name ) ) {
		Lua_DeveloperPrintf( "%s: Global '%s' already defined!\n", __func__, name );
		return;
	}

	lua_pushinteger( L, integer );
	lua_setglobal( L, name );
}
/**
*	@brief	Declares if not already existing, a global variable as 'name = number'.
**/
static inline void LUA_RegisterGlobalConstant( lua_State *L, const char *name, const lua_Number number ) {
	if ( LUA_IsGlobalDefined( L, name ) ) {
		Lua_DeveloperPrintf( "%s: Global '%s' already defined!\n", __func__, name );
		return;
	}

	lua_pushnumber( L, number );
	lua_setglobal( L, name );
}
/**
*	@brief	Declares if not already existing, a global variable as 'name = str'.
**/
static inline void LUA_RegisterGlobalConstant( lua_State *L, const char *name, const char *str ) {
	if ( LUA_IsGlobalDefined( L, name ) ) {
		Lua_DeveloperPrintf( "%s: Global '%s' already defined!\n", __func__, name );
		return;
	}

	lua_pushlstring( L, str, strlen( str ) );
	lua_setglobal( L, name );
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
*	@brief	Will 'developer print' the current lua state's stack.
*			(From bottom to top.)
**/
void SVG_Lua_DumpStack( lua_State *L );

/**
*	@brief
**/
void SVG_Lua_Initialize();
/**
*	@brief
**/
void SVG_Lua_Shutdown();

/**
*	@brief	Returns a pointer to the Lua State(Thread) that handles the map logic.
**/
lua_State *SVG_Lua_GetMapLuaState();
/**
*	@brief	Returns true if the map script has been interpreted properly.
**/
inline const bool SVG_Lua_IsMapScriptInterpreted();
/**
*	@brief
**/
const bool SVG_Lua_LoadMapScript( const std::string &scriptName );





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
*	@brief	Gives Lua a chance to precache media(audio, models, ..)
**/
void SVG_Lua_CallBack_OnPrecacheMedia();

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

