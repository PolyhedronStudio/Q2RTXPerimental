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


//
// SOL and Settings:
//
// Enable all safeties using protected function calling as a default.
#define SOL_ALL_SAFETIES_ON 1
// Enable Interop.
//#define SOL_ENABLE_INTEROP 1

// Include custom in-game assert functionality macros.
#include "svgame/lua/svg_lua_sol_assert.hpp"
// Include SOL itself.
//// Custom in-engine-asserts:
//#define SOL_ASSERT 
//#define SOL_ASSERT_MSG  
#include <sol/sol.hpp>



/**
*
*
*
*	Utilities:
*
*
*
**/
//! Determines 'How' to be verbose, if at all.
typedef enum {
	//! Don't be verbose at all.
	LUA_GETFUNCTION_VERBOSE_NOT = 0,
	//! Only be verbose if the function is actually missing.
	LUA_GETFUNCTION_VERBOSE_IF_MISSING = BIT( 0 ),
} svg_lua_getfunction_verbosity_t;
/**
*	@brief	Sets the reference to the resulting returned value of trying to get to the function.
*	@return	True if the reference is an actual function.
**/
static inline const bool LUA_GetFunction( sol::state_view &stateView, const std::string &functionName, sol::protected_function &functionReference, const svg_lua_getfunction_verbosity_t &verbosity ) {
	// Get function object.
	functionReference = stateView[ functionName ];
	// Get type.
	sol::type functionReferenceType = functionReference.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( functionReferenceType != sol::type::function /*|| !functionReference.is<std::function<bool()>>()*/ ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		functionReference.reset();
	}
}
/**
*	@brief	 
**/
static inline const bool LUA_HasFunction( sol::state_view &stateView, const std::string &functionName ) {
	// Get function object.
	sol::protected_function functionReference = stateView[ functionName ];
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( functionReference.valid() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		return true;
	}

	// Found.
	return false;
}


//! For calling into LUA functions:
#include "svgame/lua/svg_lua_callfunction.hpp"
//! For signaling.
//#include "svgame/lua/svg_lua_signals.hpp"





/**
*
*
* 
*
*	Lua Core:
*
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
sol::state &SVG_Lua_GetSolState();
/**
*	@brief	
**/
sol::state_view &SVG_Lua_GetSolStateView();

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
* 
*	Lua Game Callback Hooks:
*
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

