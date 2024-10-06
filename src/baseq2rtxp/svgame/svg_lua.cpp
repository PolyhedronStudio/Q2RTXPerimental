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
#define Lua_DEBUG_OUTPUT 1


///////////////////////////////////////////////////////////////
void VarPrint() {
	gi.dprintf( "\n" );
}
//template <typename T> void VarPrint() {
//	return;
//}
template <typename... Rest> void VarPrint( const int32_t &i, const Rest&... rest ) {
	gi.dprintf( "%i", i );
	VarPrint( rest... );
}
template <typename... Rest> void VarPrint( const float &f, const Rest&... rest ) {
	gi.dprintf( "%f", f );
	//cout << t << endl;
	VarPrint( rest... );
}
template <typename... Rest> void VarPrint( const std::string &s, const Rest&... rest ) {
	gi.dprintf( "%s", s.c_str() );
	VarPrint( rest... );
	//cout << t << endl;
}
//template <typename char*> void VarPrint( char *s ) {
//	gi.dprintf( "%s", s );
//	//cout << t << endl;
//}
template <typename... Rest> void VPrint( const std::string &functionName, const Rest&... rest ) {
	VarPrint( rest... ); // recursive call using pack expansion syntax
}
//////////////////////////////////////////////


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
//! lua state.
static lua_State *lMapState;
//! map script state
static char *mapScriptBuffer = nullptr;
static bool mapScriptInterpreted = false;


/**
*	@brief	Initializes the EntitiesLib
**/
void GameLib_Initialize( lua_State *l );
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
void SVG_Lua_Initialize() {
	//VPrint( "Hello World! VarPrint! ", 10, " ", 0.5f, " ", "Some other str" );

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
	GameLib_Initialize( lMapState );
}
/**
*	@brief 
**/
void SVG_Lua_Shutdown() {
	// Close LUA Map State.
	lua_close( lMapState );
}

/**
*	@brief	Returns a pointe rto the Lua State(Thread) that handles the map logic.
**/
lua_State *SVG_Lua_GetMapLuaState() {
	return lMapState;
}
/**
*	@brief	Returns true if the map script has been interpreted properly.
**/
inline const bool SVG_Lua_IsMapScriptInterpreted() {
	return mapScriptBuffer && mapScriptInterpreted;
}

/**
*	@brief	For now a Support routine for LoadMapScript.
**/
int LUA_LoadFileString( lua_State *L, const char *fileName, const char *s ) {
	return luaL_loadbuffer( L, s, strlen( s ), fileName );
}
static const bool LUA_InterpreteString( const char *fileName, const char *buffer ) {
	// Raw string:
	int loadResult = LUA_OK;
	if ( fileName ) {
		loadResult = LUA_LoadFileString( lMapState, fileName, buffer );
	} else {
		loadResult = luaL_loadstring( lMapState, buffer );
	}
	
	if ( loadResult == LUA_OK ) {
		// Execute the code.
		if ( lua_pcall( lMapState, 0, 0, 0 ) == LUA_OK ) {
			// Set interpreted to true.
			mapScriptInterpreted = true;
			// Debug:
			Lua_DeveloperPrintf( "%s: Succesfully interpreted buffer\n", __func__ );
			// Remove the code buffer, pop it from stack.
			lua_pop( lMapState, lua_gettop( lMapState ) );
			// Success:
			return true;
		// Failure:
		} else {
			// Get error.
			const std::string errorStr = lua_tostring( lMapState, lua_gettop( lMapState ) );
			// Output.
			LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
			// Remove the function from the stack
			lua_pop( lMapState, lua_gettop( lMapState ) );
		}
	} else {
		// Get error.
		const std::string errorStr = lua_tostring( lMapState, lua_gettop( lMapState ) );
		// Output.
		if ( fileName ) {
			LUA_ErrorPrintf( "%s: Error compiling buffer(%s): %s\n", __func__, fileName, errorStr.c_str() );
		} else {
			LUA_ErrorPrintf( "%s: Error compiling buffer(raw): %s\n", __func__, errorStr.c_str() );
		}
		// Remove the function from the stack
		lua_pop( lMapState, lua_gettop( lMapState ) );
	}

	// Failed.
	mapScriptInterpreted = false;

	//// TODO: Output display error.
	//Lua_DeveloperPrintf( "%s: Failed interpreting buffer, unknown error.\n", __func__ );
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
void SVG_Lua_LoadMapScript( const std::string &scriptName ) {
	// File name+ext & path.
	const std::string fileNameExt = scriptName + ".lua";
	const std::string filePath = "maps/scripts/" + fileNameExt;

	// Unload one if we had one loaded up before.
	LUA_UnloadMapScript();

	// Ensure file exists.
	if ( SG_FS_FileExistsEx( filePath.c_str(), 0 ) ) {
		// Load Lua file.
		size_t scriptFileLength = SG_FS_LoadFile( filePath.c_str(), (void **)&mapScriptBuffer );

		if ( mapScriptBuffer && scriptFileLength != Q_ERR( ENOENT ) ) {
			// Debug output.			
			Lua_DeveloperPrintf( "%s: Loaded Lua Script File: %s\n", __func__, filePath.c_str() );

			if ( LUA_InterpreteString( fileNameExt.c_str(), mapScriptBuffer ) ) {
				// Debug output:
				Lua_DeveloperPrintf( "%s: Succesfully parsed and interpreted Lua Script File: %s\n", __func__, filePath.c_str() );
			} else {
				// Debug output:
				Lua_DeveloperPrintf( "%s: Succesfully parsed and interpreted Lua Script File: %s\n", __func__, filePath.c_str() );
			}
		// Failure:
		} else {
			// Debug output.
			Lua_DeveloperPrintf( "%s: Failed loading Lua Script File: %s\n", __func__, filePath.c_str() );
		}
	// Failure:
	} else {
		// Debug output:
		Lua_DeveloperPrintf( "%s: Map %s has no existing Lua Script File\n", __func__, scriptName.c_str() );
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
void SVG_Lua_CallBack_BeginMap() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( lMapState, "OnBeginMap", 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	// Pop the bool from stack.
	lua_pop( lMapState, 1 );
	// Pop remaining stack.
	lua_pop( lMapState, lua_gettop( lMapState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ExitMap() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( lMapState, "OnExitMap", 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/);
	// Pop the bool from stack.
	lua_pop( lMapState, 1 );
	// Pop remaining stack.
	lua_pop( lMapState, lua_gettop( lMapState ) );
}


//
//	For when a client is connecting thus entering the active map(level) 
//	or disconnecting, thus exiting the map(level).
//
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientEnterLevel( edict_t *clientEntity ) {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( lMapState, "OnClientEnterLevel", 1, LUA_CALLFUNCTION_VERBOSE_MISSING, 
		/*[lua args]:*/clientEntity );
	// Pop the bool from stack.
	lua_pop( lMapState, 1 );
	// Pop remaining stack.
	lua_pop( lMapState, lua_gettop( lMapState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientExitLevel( edict_t *clientEntity ) {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( lMapState, "OnClientExitLevel", 1, LUA_CALLFUNCTION_VERBOSE_MISSING, 
		/*[lua args]:*/ clientEntity );
	// Pop the bool from stack.
	lua_pop( lMapState, 1 );
	// Pop remaining stack.
	lua_pop( lMapState, lua_gettop( lMapState ) );
}


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginServerFrame() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( lMapState, "OnBeginServerFrame", 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	// Pop the bool from stack.
	lua_pop( lMapState, 1 );
	// Pop remaining stack.
	lua_pop( lMapState, lua_gettop( lMapState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_RunFrame() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( lMapState, "OnRunFrame", 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
		/*[lua args]:*/ level.framenum );
	// Pop the bool from stack.
	lua_pop( lMapState, 1 );
	// Pop remaining stack.
	lua_pop( lMapState, lua_gettop( lMapState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_EndServerFrame() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( lMapState, "OnEndServerFrame", 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	// Pop the bool from stack.
	lua_pop( lMapState, 1 );
	// Pop remaining stack.
	lua_pop( lMapState, lua_gettop( lMapState ) );
}