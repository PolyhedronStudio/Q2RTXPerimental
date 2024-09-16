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
//! For developer prints:
#ifdef LUA_DEBUG_OUTPUT
#define LUA_DeveloperPrintf(...) gi.dprintf( __VA_ARGS__ );
#else
#define LUA_DeveloperPrintf(...) 
#endif
//! For errors:
#define LUA_ErrorPrintf(...) gi.bprintf( PRINT_WARNING, __VA_ARGS__ );



/**
*
* 
*
*	TODO: Needs structurisation.
* 
* 
* 
**/
//! lua state.
static lua_State *lMapState;
//! map script state
static char *mapScriptBuffer = nullptr;
static bool mapScriptInterpreted = false;


/**
*	@brief	Initializes the EntitiesLib
**/
void EntitiesLib_Initialize( lua_State *l );
/**
*	@brief	Initializes the EntitiesLib
**/
void CoreLib_Initialize( lua_State *L );


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
void SVG_LUA_Initialize() {
	// Open LUA Map State.
	lMapState = luaL_newstate();

	// Load in several useful defualt LUA libraries.
	luaopen_base( lMapState );
	luaopen_string( lMapState );
	luaopen_table( lMapState );
	luaopen_math( lMapState );
	luaopen_debug( lMapState );

	// Initialize Game Libraries.
	CoreLib_Initialize( lMapState );
	EntitiesLib_Initialize( lMapState );
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
		if ( lua_pcall( lMapState, 0, 0, 0 ) == LUA_OK ) {
			// Set interpreted to true.
			mapScriptInterpreted = true;
			// Remove the code buffer, pop it from stack.
			lua_pop( lMapState, lua_gettop( lMapState ) );
			// Debug:
			LUA_DeveloperPrintf( "%s: Succesfully interpreted buffer\n", __func__ );
			// Success:
			return true;
		// Failure:
		} else {
			// Get error.
			const std::string errorStr = lua_tostring( lMapState, lua_gettop( lMapState ) );
			// Remove the function from the stack
			lua_pop( lMapState, lua_gettop( lMapState ) );
			// Output.
			LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
		}
	}

	// TODO: Output display error.
	LUA_DeveloperPrintf( "%s: Failed interpreting buffer, unknown error.\n", __func__ );
	return false;
}

/**
*	@brief	For now a Support routine for LoadMapScript.
*			TODO: The buffer is stored in tag_filesystem so... basically...
**/
static void LUA_UnloadMapScript() {
	// TODO: This is technically not safe, we need to store the buffer elsewhere ... expose FS_LoadFileEx!
	mapScriptBuffer = nullptr; 
	mapScriptInterpreted = false;
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
			LUA_DeveloperPrintf( "%s: Loaded Lua Script File: %s\n", __func__, filePath.c_str() );

			if ( LUA_InterpreteString( mapScriptBuffer ) ) {
				// Debug output:
				LUA_DeveloperPrintf( "%s: Succesfully parsed and interpreted Lua Script File: %s\n", __func__, filePath.c_str() );
			} else {
				// Debug output:
				LUA_DeveloperPrintf( "%s: Succesfully parsed and interpreted Lua Script File: %s\n", __func__, filePath.c_str() );
			}
		// Failure:
		} else {
			// Debug output.
			LUA_DeveloperPrintf( "%s: Failed loading Lua Script File: %s\n", __func__, filePath.c_str() );
		}
	// Failure:
	} else {
		// Debug output:
		LUA_DeveloperPrintf( "%s: Map %s has no existing Lua Script File\n", __func__, scriptName.c_str() );
	}
}



/**
*
*
*
*	Lua CoreLib:
*
*
*
**/
/**
*	@brief	Prints the given string.
**/
int CoreLib_DPrint( lua_State *L ) {
	// Check if the first argument is string.
	const char *printStr = luaL_checkstring( L, 1 );
	if ( !printStr ) {
		LUA_DeveloperPrintf( "%s: invalid string\n", __func__ );
		return 0;
	}

	// Print
	gi.dprintf( "%s", printStr );

	return 0; // The number of returned values
}
/**
*	@brief	Entities Namespace Functions:
**/
const struct luaL_Reg CoreLib[] = {
	{ "DPrint", CoreLib_DPrint },
	{ NULL, NULL }
};
/**
*	@brief	Initializes the EntitiesLib
**/
void CoreLib_Initialize( lua_State *L ) {
	// We create a new table
	lua_newtable( L );

	// Here we set all functions from CoreLib array into
	// the table on the top of the stack
	luaL_setfuncs( L, CoreLib, 0 );

	// We get the table and set as global variable
	lua_setglobal( L, "Core" );

	if ( luaL_dostring( L, "Core.DPrint(\"Initialized Lua CoreLib\n\"" ) == LUA_OK ) {
		lua_pop( L, lua_gettop( L ) );
	}
}



/**
*
*
*
*	Lua EntitiesLib:
*
*
*
**/
/**
*	@return	The number of the entity if it has a matching targetname, -1 otherwise.
**/
int EntitiesLib_GetForTargetName( lua_State *L ) {
	// Check if the first argument is string.
	const char *targetName = luaL_checkstring( L, 1 );
	if ( !targetName ) {
		LUA_DeveloperPrintf( "%s: invalid string\n", __func__ );
		// Pushing the result onto the stack to be returned
		lua_Integer luaEntityNumber = -1;
		lua_pushinteger( L, luaEntityNumber );
		return 1;
	}

	// Find entity and acquire its number.
	int32_t entityNumber = -1; // None by default.

	edict_t *targetNameEntity = G_Find( NULL, FOFS( targetname ), targetName );
	if ( targetNameEntity ) {
		entityNumber = targetNameEntity->s.number;
	}

	// multiply and store the result inside a type lua_Integer
	lua_Integer luaEntityNumber = entityNumber;

	// Here we prepare the values to be returned.
	// First we push the values we want to return onto the stack in direct order.
	// Second, we must return the number of values pushed onto the stack.

	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, luaEntityNumber );

	return 1; // The number of returned values
}
/**
*	@return	The number of the entity if it has a matching targetname, -1 otherwise.
**/
int EntitiesLib_UseTargets( lua_State *L ) {
	//// Check if the first argument is string.
	//const char *targetName = luaL_checkstring( L, 1 );
	//if ( !targetName ) {
	//	LUA_DeveloperPrintf( "%s: invalid string\n", __func__ );
	//	// Pushing the result onto the stack to be returned
	//	lua_Integer luaEntityNumber = -1;
	//	lua_pushinteger( L, luaEntityNumber );
	//	return 1;
	//}

	//// Find entity and acquire its number.
	//int32_t entityNumber = -1; // None by default.

	//edict_t *targetNameEntity = G_Find( NULL, FOFS( targetname ), targetName );
	//if ( targetNameEntity ) {
	//	entityNumber = targetNameEntity->s.number;
	//}

	// multiply and store the result inside a type lua_Integer
	lua_Integer result = 0;

	// Here we prepare the values to be returned.
	// First we push the values we want to return onto the stack in direct order.
	// Second, we must return the number of values pushed onto the stack.

	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, result );

	return 1; // The number of returned values
}
/**
*	@brief	Entities Namespace Functions:
**/
const struct luaL_Reg EntitiesLib[] = {
	{ "GetForTargetName", EntitiesLib_GetForTargetName },
	{ "UseTargets", EntitiesLib_UseTargets },
	{ NULL, NULL }
};
/**
*	@brief	Initializes the EntitiesLib
**/
void EntitiesLib_Initialize( lua_State *L ) {
	// We create a new table
	lua_newtable( L );

	// Here we set all functions from EntitiesLib array into
	// the table on the top of the stack
	luaL_setfuncs( L, EntitiesLib, 0);

	// We get the table and set as global variable
	lua_setglobal( L, "Entities" );

	if ( luaL_dostring( L, "Core.DPrint(\"Initialized Lua EntitiesLib\n\"" ) == LUA_OK ) {
		lua_pop( L, lua_gettop( L ) );
	}
}



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
static const bool LUA_CallFunction( const char *functionName ) {
	// We require the script to be succesfully interpreted.
	if ( !mapScriptInterpreted || !lMapState || !functionName ) {
		return false;
	}

	// Get the global functionname value and push it to stack:
	lua_getglobal( lMapState, functionName );

	// Check if function even exists.
	if ( lua_isfunction( lMapState, -1 ) ) {
		// Protect Call the pushed function name string.
		if ( lua_pcall( lMapState, 0, 1, 0 ) == LUA_OK ) {
			// Pop function name from stack.
			lua_pop( lMapState, lua_gettop( lMapState ) );
			// Success.
			return true;
		// Failure:
		} else {
			// Get error.
			const std::string errorStr = lua_tostring( lMapState, lua_gettop( lMapState ) );
			// Remove the function from the stack
			lua_pop( lMapState, lua_gettop( lMapState ) );
			// Output.
			LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
			// Failure.
			return false;
		}
	// Failure:
	} else {
		// Remove the function from the stack
		lua_pop( lMapState, lua_gettop( lMapState ) );

		LUA_ErrorPrintf( "%s: %s is not a function\n", __func__, functionName );
		return false;
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
	LUA_CallFunction( "OnBeginMap" );
}
/**
*	@brief
**/
void SVG_LUA_CallBack_ExitMap() {
	LUA_CallFunction( "OnExitMap" );
}


//
//	For when a client is connecting thus entering the active map(level) 
//	or disconnecting, thus exiting the map(level).
//
/**
*	@brief
**/
void SVG_LUA_CallBack_ClientEnterLevel() {
	LUA_CallFunction( "OnClientEnterLevel" );
}
/**
*	@brief
**/
void SVG_LUA_CallBack_ClientExitLevel() {
	LUA_CallFunction( "OnClientExitLevel" );
}


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_LUA_CallBack_BeginServerFrame() {
	LUA_CallFunction( "OnBeginServerFrame" );
}
/**
*	@brief
**/
void SVG_LUA_CallBack_RunFrame() {
	LUA_CallFunction( "OnRunFrame" );
}
/**
*	@brief
**/
void SVG_LUA_CallBack_EndServerFrame() {
	LUA_CallFunction( "OnEndServerFrame" );
}