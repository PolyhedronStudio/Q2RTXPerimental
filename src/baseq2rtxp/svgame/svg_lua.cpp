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
}
/**
*	@brief 
**/
void SVG_LUA_Shutdown() {
	// Close LUA Map State.
	lua_close( lMapState );
}

/**
*	@brief
**/
void SVG_LUA_LoadMapScript( const std::string &scriptName ) {
	// File path.
	const std::string filePath = "maps/scripts/" + scriptName + ".lua";

	// The buffer is stored in tag_filesystem so... basically...
	mapScriptBuffer = nullptr; // This is technically not safe, we need to store the buffer elsewhere ... expose FS_LoadFileEx!

	// Ensure file exists.
	if ( SG_FS_FileExistsEx( filePath.c_str(), 0 ) ) {
		// Load Lua file.
		size_t scriptFileLength = SG_FS_LoadFile( filePath.c_str(), (void **)&mapScriptBuffer );

		if ( mapScriptBuffer && scriptFileLength != Q_ERR( ENOENT ) ) {
			gi.dprintf( "%s: Loaded Lua Script File: %s\n", __func__, filePath.c_str() );
		} else {
			gi.dprintf( "%s: Failed loading Lua Script File: %s\n", __func__, filePath.c_str() );
		}
	} else {
		gi.dprintf( "%s: Map %s has no existing Lua Script File\n", __func__, scriptName.c_str() );
	}
}