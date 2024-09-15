#pragma once

// Include lua headers.
#include <lua.hpp>
#include <lualib.h>



/**
*
*
*
*	Lua Core Hooks:
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
*	Lua Core Game Callback Hooks:
*
*
*
**/
