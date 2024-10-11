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



/**
*
* 
*
*	TODO: Needs structurisation.
* 
* 
* 
**/
static struct {
	//! lua state.
	lua_State *luaState;
	//! map script state
	char *scriptBuffer;
	//! Did we succesfully parse, and load it?
	bool scriptInterpreted;
} luaMapState = {};


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
void SVG_Lua_DumpStack( lua_State *L ) {
	// Need a valid loaded script.
	if ( !SVG_Lua_IsMapScriptInterpreted() ) {
		return;
	}

	// Output func name.
	gi.dprintf( "-----------  Lua StackDump  ------------\n" );

	// Get top stack number.
	const int32_t  top = lua_gettop( L );
	// Iterate to top of stack.
	for ( int32_t i = 1; i <= top; i++ ) {  /* repeat for each level */
		// Get type.
		const int32_t t = lua_type( L, i );
		// Act based on type.
		switch ( t ) {

		// strings:
		case LUA_TSTRING:
			gi.dprintf( "idx[#%d] : value[ `%s' ]\n", i, lua_tostring( L, i ) );
			break;
		// booleans:
		case LUA_TBOOLEAN:
			gi.dprintf( "idx[#%d] : value[ %s ]\n", i, lua_toboolean( L, i ) ? "true" : "false" );
			break;
		// numbers:
		case LUA_TNUMBER:
			gi.dprintf( "idx[#%d] : value[ %g ]\n", i, lua_tonumber( L, i ) );
			break;
		
		// other values:
		default:
			gi.dprintf( "idx[#%d] : value[ %s ]\n", i, lua_typename( L, t ) );
			break;
		}
	}

	// End of stack dump.
	gi.dprintf( "----------------------------------------\n" );
}

/**
*	@brief
**/
void SVG_Lua_Initialize() {
	//VPrint( "Hello World! VarPrint! ", 10, " ", 0.5f, " ", "Some other str" );

	// Open LUA Map State.
	luaMapState.luaState = luaL_newstate();

	// Load in several useful defualt LUA libraries.
	luaopen_base( luaMapState.luaState );
	luaopen_string( luaMapState.luaState );
	luaopen_table( luaMapState.luaState );
	luaopen_math( luaMapState.luaState );
	luaopen_debug( luaMapState.luaState );

	// Initialize Game Libraries.
	CoreLib_Initialize( luaMapState.luaState );
	GameLib_Initialize( luaMapState.luaState );
}
/**
*	@brief 
**/
void SVG_Lua_Shutdown() {
	// Close LUA Map State.
	lua_close( luaMapState.luaState );
}

/**
*	@brief	Returns a pointe rto the Lua State(Thread) that handles the map logic.
**/
lua_State *SVG_Lua_GetMapLuaState() {
	return luaMapState.luaState;
}
/**
*	@brief	Returns true if the map script has been interpreted properly.
**/
inline const bool SVG_Lua_IsMapScriptInterpreted() {
	return luaMapState.scriptBuffer && luaMapState.scriptInterpreted;
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
		loadResult = LUA_LoadFileString( luaMapState.luaState, fileName, buffer );
	} else {
		loadResult = luaL_loadstring( luaMapState.luaState, buffer );
	}
	
	if ( loadResult == LUA_OK ) {
		// Execute the code.
		if ( lua_pcall( luaMapState.luaState, 0, 0, 0 ) == LUA_OK ) {
			// Debug:
			Lua_DeveloperPrintf( "%s: Succesfully interpreted buffer\n", __func__ );
			// Remove the code buffer, pop it from stack.
			lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
			// Set interpreted to true.
			luaMapState.scriptInterpreted = true;
			// Success:
			return true;
		// Failure:
		} else {
			// Get error.
			const std::string errorStr = lua_tostring( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
			// Output.
			if ( fileName ) {
				LUA_ErrorPrintf( "%s: %s\n", __func__, fileName, errorStr.c_str() );
			} else {
				LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
			}
			// Remove the function from the stack
			lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
		}
	} else {
		// Get error.
		const std::string errorStr = lua_tostring( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
		// Output.
		if ( fileName ) {
			LUA_ErrorPrintf( "%s: %s\n", __func__, fileName, errorStr.c_str() );
		} else {
			LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
		}
		// Remove the function from the stack
		lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
	}

	// Failed.
	luaMapState.scriptInterpreted = false;

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
	luaMapState.scriptBuffer = nullptr;
	luaMapState.scriptInterpreted = false;
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
		size_t scriptFileLength = SG_FS_LoadFile( filePath.c_str(), (void **)&luaMapState.scriptBuffer );

		if ( luaMapState.scriptBuffer && scriptFileLength != Q_ERR( ENOENT ) ) {
			// Debug output.			
			Lua_DeveloperPrintf( "%s: Loaded Lua Script File: %s\n", __func__, filePath.c_str() );

			if ( LUA_InterpreteString( fileNameExt.c_str(), luaMapState.scriptBuffer ) ) {
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
	const bool calledFunction = LUA_CallFunction( luaMapState.luaState, "OnBeginMap", 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	// Pop the bool from stack.
	lua_pop( luaMapState.luaState, 1 );
	// Pop remaining stack.
	lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ExitMap() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.luaState, "OnExitMap", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/);
	// Pop the bool from stack.
	lua_pop( luaMapState.luaState, 1 );
	// Pop remaining stack.
	lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
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
	const bool calledFunction = LUA_CallFunction( luaMapState.luaState, "OnClientEnterLevel", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
		/*[lua args]:*/clientEntity );
	// Pop the bool from stack.
	lua_pop( luaMapState.luaState, 1 );
	// Pop remaining stack.
	lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientExitLevel( edict_t *clientEntity ) {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.luaState, "OnClientExitLevel", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
		/*[lua args]:*/ clientEntity );
	// Pop the bool from stack.
	lua_pop( luaMapState.luaState, 1 );
	// Pop remaining stack.
	lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
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
	const bool calledFunction = LUA_CallFunction( luaMapState.luaState, "OnBeginServerFrame", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	// Pop the bool from stack.
	lua_pop( luaMapState.luaState, 1 );
	// Pop remaining stack.
	lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_RunFrame() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.luaState, "OnRunFrame", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
		/*[lua args]:*/ level.framenum );
	// Pop the bool from stack.
	lua_pop( luaMapState.luaState, 1 );
	// Pop remaining stack.
	lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_EndServerFrame() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.luaState, "OnEndServerFrame", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	// Pop the bool from stack.
	lua_pop( luaMapState.luaState, 1 );
	// Pop remaining stack.
	lua_pop( luaMapState.luaState, lua_gettop( luaMapState.luaState ) );
}