#pragma once

// Include lua headers.
#include <lua.hpp>
#include <lualib.h>
#include <lauxlib.h>


// (Templated-) Support Lua Utilities:
//! For errors:
#define LUA_ErrorPrintf(...) gi.bprintf( PRINT_WARNING, __VA_ARGS__ );
//! For calling into LUA functions:
#include "svgame/lua/svg_lua_callfunction.hpp"



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
void SVG_LUA_Initialize();
/**
*	@brief
**/
void SVG_LUA_Shutdown();

/**
*	@brief
**/
void SVG_LUA_LoadMapScript( const std::string &scriptName );



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
*	@brief	Calls specified function.
**/
const bool SVG_LUA_DispatchTargetNameUseCallBack( edict_t *self, edict_t *other, edict_t *activator );



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
void SVG_LUA_CallBack_BeginMap();
/**
*	@brief
**/
void SVG_LUA_CallBack_ExitMap();


//
//	For when a client is connecting thus entering the active map(level) 
//	or disconnecting, thus exiting the map(level).
//
/**
*	@brief
**/
void SVG_LUA_CallBack_ClientEnterLevel( edict_t *clientEntity );
/**
*	@brief
**/
void SVG_LUA_CallBack_ClientExitLevel( edict_t *clientEntity );


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_LUA_CallBack_BeginServerFrame();
/**
*	@brief
**/
void SVG_LUA_CallBack_RunFrame();
/**
*	@brief
**/
void SVG_LUA_CallBack_EndServerFrame();

