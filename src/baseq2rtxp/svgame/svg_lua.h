#pragma once

// Include lua headers.
#include <lua.hpp>
#include <lualib.h>
#include <lauxlib.h>


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
void SVG_LUA_CallBack_ClientEnterLevel();
/**
*	@brief
**/
void SVG_LUA_CallBack_ClientExitLevel();


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

