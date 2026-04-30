/********************************************************************
*
*
*	ServerGame: Lua Runtime API (lightweight declarations).
*
*
********************************************************************/
#pragma once

#include <string>

struct lua_State;
struct svg_base_edict_t;

/**
*	@brief	Will 'developer print' the current lua state's stack.
*			(From bottom to top.)
**/
void SVG_Lua_DumpStack( lua_State *L );

/**
*	@brief	Initialize the Lua runtime.
**/
void SVG_Lua_Initialize();
/**
*	@brief	Shutdown the Lua runtime.
**/
void SVG_Lua_Shutdown();

/**
*	@brief	Returns the raw Lua C API state pointer.
**/
lua_State *SVG_Lua_GetState();

/**
*	@brief	Returns true if the map script has been interpreted properly.
**/
inline const bool SVG_Lua_IsMapScriptInterpreted();
/**
*	@brief	Load a map script by script name.
**/
const bool SVG_Lua_LoadMapScript( const std::string &scriptName );

/**
*	@brief	Gives Lua a chance to precache media(audio, models, ..)
**/
void SVG_Lua_CallBack_OnPrecacheMedia();
/**
*	@brief	Map-begin callback.
**/
void SVG_Lua_CallBack_BeginMap();
/**
*	@brief	Map-exit callback.
**/
void SVG_Lua_CallBack_ExitMap();

/**
*	@brief	Client entered the current level callback.
**/
void SVG_Lua_CallBack_ClientEnterLevel( svg_base_edict_t *clientEntity );
/**
*	@brief	Client exited the current level callback.
**/
void SVG_Lua_CallBack_ClientExitLevel( svg_base_edict_t *clientEntity );

/**
*	@brief	Begin server-frame callback.
**/
void SVG_Lua_CallBack_BeginServerFrame();
/**
*	@brief	Run-frame callback.
**/
void SVG_Lua_CallBack_RunFrame();
/**
*	@brief	End server-frame callback.
**/
void SVG_Lua_CallBack_EndServerFrame();
