/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "svg_local.h"
#include "svg_lua.h"

//! Enable LUA debug output:
#define LUA_DEBUG_OUTPUT 1

/**
*
*
*
*	Share this to game modules, now resides in common/error.h....
*
*
*
**/
#include <errno.h>

#define ERRNO_MAX       0x5000

#if EINVAL > 0
#define Q_ERR(e)        (e < 1 || e > ERRNO_MAX ? -ERRNO_MAX : -e)
#else
#define Q_ERR(e)        (e > -1 || e < -ERRNO_MAX ? -ERRNO_MAX : e)
#endif

#define Q_ERR_(e)       (-ERRNO_MAX - e)



/**
*
*
*
*	TODO: This is DJ Not Nice, code is not neat, needs fixing.
*
*
8
**/
#ifdef LUA_DEBUG_OUTPUT
#define LUA_DPrintf(...) gi.dprintf( __VA_ARGS__ );
#else
#define LUA_DPrintf(...) 
#endif

/**
*
* 
*
*	TODO: Needs structurisation.
* 
* 
* 
**/
static lua_State *lMapState;
static char *mapScriptBuffer = nullptr;

/**
*	@brief
**/
void SVG_LUA_Initialize() {
	// Open LUA Map State.
	lMapState = luaL_newstate();

	// Load in several useful defualt LUA libraries.
	luaopen_base( lMapState );
	luaopen_string( lMapState );
	luaopen_table( lMapState );
	luaopen_math( lMapState );
	luaopen_debug( lMapState );
}
/**
*	@brief 
**/
void SVG_LUA_Shutdown() {
	// Close LUA Map State.
	lua_close( lMapState );
}

/**
*	@brief	For now a Support routine for LoadMapScript.
**/
static const bool LUA_InterpreteString( const char *buffer ) {
	if ( luaL_loadstring( lMapState, buffer ) == LUA_OK ) {
		LUA_DPrintf( "%s: Succesfully interpreted buffer\n", __func__ );
		return true;
	} else {
		// TODO: Output display error.
		LUA_DPrintf( "%s: Failed interpreting buffer\n", __func__ );

		return false;
	}
}

/**
*	@brief	For now a Support routine for LoadMapScript.
*			TODO: The buffer is stored in tag_filesystem so... basically...
**/
static void LUA_UnloadMapScript() {
	// TODO: This is technically not safe, we need to store the buffer elsewhere ... expose FS_LoadFileEx!
	mapScriptBuffer = nullptr; 
}

/**
*	@brief
**/
void SVG_LUA_LoadMapScript( const std::string &scriptName ) {
	// File path.
	const std::string filePath = "maps/scripts/" + scriptName + ".lua";

	// Unload one if we had one loaded up before.
	LUA_UnloadMapScript();

	// Ensure file exists.
	if ( SG_FS_FileExistsEx( filePath.c_str(), 0 ) ) {
		// Load Lua file.
		size_t scriptFileLength = SG_FS_LoadFile( filePath.c_str(), (void **)&mapScriptBuffer );

		if ( mapScriptBuffer && scriptFileLength != Q_ERR( ENOENT ) ) {
			// Debug output.			
			LUA_DPrintf( "%s: Loaded Lua Script File: %s\n", __func__, filePath.c_str() );

			if ( LUA_InterpreteString( mapScriptBuffer ) ) {
				// Debug output:
				LUA_DPrintf( "%s: Succesfully parsed and interpreted Lua Script File: %s\n", __func__, filePath.c_str() );
			} else {
				// Debug output:
				LUA_DPrintf( "%s: Succesfully parsed and interpreted Lua Script File: %s\n", __func__, filePath.c_str() );
			}
		// Failure:
		} else {
			// Debug output.
			LUA_DPrintf( "%s: Failed loading Lua Script File: %s\n", __func__, filePath.c_str() );
		}
	// Failure:
	} else {
		// Debug output:
		LUA_DPrintf( "%s: Map %s has no existing Lua Script File\n", __func__, scriptName.c_str() );
	}
}



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
void SVG_LUA_CallBack_BeginMap() {

}
/**
*	@brief
**/
void SVG_LUA_CallBack_ExitMap() {

}


//
//	For when a client is connecting thus entering the active map(level) 
//	or disconnecting, thus exiting the map(level).
//
/**
*	@brief
**/
void SVG_LUA_CallBack_ClientEnterLevel() {

}
/**
*	@brief
**/
void SVG_LUA_CallBack_ClientExitLevel() {

}


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_LUA_CallBack_BeginServerFrame() {

}
/**
*	@brief
**/
void SVG_LUA_CallBack_RunFrame() {

}
/**
*	@brief
**/
void SVG_LUA_CallBack_EndServerFrame() {

}