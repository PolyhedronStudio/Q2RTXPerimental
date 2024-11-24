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
*	NameSpace Library API Binding Initializers:
*
*
*
**/
/**
*	@brief	Initializes the CoreLib
**/
void CoreLib_Initialize( lua_State *L );
/**
*	@brief	Initializes the GameLib
**/
void GameLib_Initialize( lua_State *l );
/**
*	@brief	Initializes the MediaLib
**/
void MediaLib_Initialize( lua_State *L );



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
	lua_State *lState;
	//! map script state
	char *scriptBuffer;
	//! Did we succesfully parse, and load it?
	bool scriptInterpreted;
} luaMapState = {};



/**
*
*
*
*	@brief	TODO: Implement this properly so we use a custom reader.
*
*
*
**/
typedef struct LoadS {
	const char *s;
	size_t size;
} LoadS;
static const char *_LUA_getS( lua_State *L, void *ud, size_t *size ) {
	LoadS *ls = (LoadS *)ud;
	(void)L;  /* not used */
	if ( ls->size == 0 ) return NULL;
	*size = ls->size;
	ls->size = 0;
	return ls->s;
}
static int _LUA_luaL_loadbufferx( lua_State *L, const char *buff, size_t size,
	const char *name, const char *mode ) {
	LoadS ls;
	ls.s = buff;
	ls.size = size;
	return lua_load( L, _LUA_getS, &ls, name, mode );
}
static int _LUA_luaL_loadstring( lua_State *L, const char *s ) {
	return _LUA_luaL_loadbufferx( L, s, strlen( s ), s, NULL );
}
/**
*	@brief	For now a Support routine for LoadMapScript.
**/
static int _LUA_LoadFileString( lua_State *L, const char *fileName, const char *s ) {
	return _LUA_luaL_loadbufferx( L, s, strlen( s ), fileName, "bt" );
}



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
*	@brief	Will 'developer print' the current lua state's stack.
*			(From bottom to top.)
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
	// Open LUA Map State.
	luaMapState.lState = luaL_newstate();

	// Load in several useful defualt LUA libraries.
	luaopen_base( luaMapState.lState );
	luaopen_string( luaMapState.lState );
	luaopen_table( luaMapState.lState );
	luaopen_math( luaMapState.lState );
	luaopen_debug( luaMapState.lState );

	// Initialize Game Libraries.
	CoreLib_Initialize( luaMapState.lState );
	GameLib_Initialize( luaMapState.lState );
	MediaLib_Initialize( luaMapState.lState );
}
/**
*	@brief 
**/
void SVG_Lua_Shutdown() {
	// Close LUA Map State.
	lua_close( luaMapState.lState );
}

/**
*	@brief	Returns a pointe rto the Lua State(Thread) that handles the map logic.
**/
lua_State *SVG_Lua_GetMapLuaState() {
	return luaMapState.lState;
}
/**
*	@brief	Returns true if the map script has been interpreted properly.
**/
inline const bool SVG_Lua_IsMapScriptInterpreted() {
	return luaMapState.scriptBuffer && luaMapState.scriptInterpreted;
}

/**
*	Will load a chunk of LUA and compile it, if given a filename the chunk will
*	be stored with a filename identifier, used to display errors etc with line numbers.
**/
static const bool LUA_InterpreteString( const char *fileName, const char *buffer ) {
	// Raw string:
	int loadResult = LUA_OK;
	if ( fileName ) {
		loadResult = _LUA_LoadFileString( luaMapState.lState, fileName, buffer );
	} else {
		loadResult = _LUA_luaL_loadstring( luaMapState.lState, buffer );
	}
	
	// Output.
	#define InterpretError(...) \
		if ( fileName ) {\
			LUA_ErrorPrintf( "%s:\n%s\n", __func__, errorStr.c_str() );\
		} else {\
			LUA_ErrorPrintf( "%s:\n%s\n", __func__, errorStr.c_str() );\
		}

	if ( loadResult == LUA_OK ) {
		// Execute the code.
		int pCallReturnValue = lua_pcall( luaMapState.lState, 0, LUA_MULTRET, 0 );
		if ( pCallReturnValue == LUA_OK ) {
			// Debug:
			Lua_DeveloperPrintf( "%s: Succesfully interpreted buffer\n", __func__ );
			// Remove the code buffer, pop it from stack.
			lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
			// Set interpreted to true.
			luaMapState.scriptInterpreted = true;
			// Success:
			return true;
		// Yielded -> TODO: Can we ever make yielding work by doing something here to
		// keeping save/load games in mind?
		} else if ( pCallReturnValue == LUA_YIELD ) {
			// Get error.
			const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
			// Remove the errorStr from the stack
			lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Yielding Not Supported!]:\n%s\n", __func__, errorStr.c_str() );
		// Runtime Error:
		} else if ( pCallReturnValue == LUA_ERRRUN ) {
			// Get error.
			const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
			// Remove the errorStr from the stack
			lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Runtime Error]:\n%s\n", __func__, errorStr.c_str() );
		// Syntax Error:
		} else if ( pCallReturnValue == LUA_ERRSYNTAX ) {
			// Get error.
			const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
			// Remove the errorStr from the stack
			lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Syntax Error]:\n%s\n", __func__, errorStr.c_str() );
		// Memory Error:
		} else if ( pCallReturnValue == LUA_ERRMEM ) {
			// Get error.
			const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
			// Remove the errorStr from the stack
			lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Memory Error]:\n%s\n", __func__, errorStr.c_str() );
		// Error Error? Lol: TODO: WTF?
		} else if ( pCallReturnValue == LUA_ERRERR ) {
			// Get error.
			const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
			// Remove the errorStr from the stack
			lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Error]:\n%s\n", __func__, errorStr.c_str() );
		} else {
			// Get error.
			const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
			// Remove the errorStr from the stack
			lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "%s:\n%s\n", __func__, errorStr.c_str() );
		}
	// Syntax Error:
	} else if ( loadResult == LUA_ERRSYNTAX ) {
		// Get error.
		const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
		// Remove the errorStr from the stack
		lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
		// Print Error Notification.
		LUA_ErrorPrintf( "[%s]:[Syntax Error:]\n%s\n", __func__, errorStr.c_str() );
	// Memory Error:
	} else if ( loadResult == LUA_ERRMEM ) {
		// Get error.
		const std::string errorStr = lua_tostring( luaMapState.lState, -1 );
		// Remove the errorStr from the stack
		lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
		// Print Error Notification.
		LUA_ErrorPrintf( "%s:\nMemory Error: %s\n", __func__, errorStr.c_str() );
		// Error Error? Lol: TODO: WTF?
	} else {
		// Get error.
		const std::string errorStr = lua_tostring( luaMapState.lState, lua_gettop( luaMapState.lState ) );
		// Output.
		if ( fileName ) {
			LUA_ErrorPrintf( "%s:\n%s\n", __func__, errorStr.c_str() );
		} else {
			LUA_ErrorPrintf( "%s:\n%s\n", __func__, errorStr.c_str() );
		}
		// Remove the function from the stack
		lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
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
const bool SVG_Lua_LoadMapScript( const std::string &scriptName ) {
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
			Lua_DeveloperPrintf( "%s: Loaded Lua script file: %s\n", __func__, filePath.c_str() );

			if ( LUA_InterpreteString( fileNameExt.c_str(), luaMapState.scriptBuffer ) ) {
				// Debug output:
				Lua_DeveloperPrintf( "%s: Succesfully parsed and interpreted Lua script file: %s\n", __func__, filePath.c_str() );
				return true;
			} else {
				// Debug output:
				Lua_DeveloperPrintf( "%s: Failed parsing/interpreting Lua script file: %s\n", __func__, filePath.c_str() );
			}
		// Failure:
		} else {
			// Debug output.
			Lua_DeveloperPrintf( "%s: Failed loading Lua script file: %s\n", __func__, filePath.c_str() );
		}
	// Failure:
	} else {
		// Debug output:
		Lua_DeveloperPrintf( "%s: Map %s has no existing Lua script file\n", __func__, scriptName.c_str() );
	}
	return false;
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
void SVG_Lua_CallBack_OnPrecacheMedia() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Proceed to calling the global map precache function.
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnPrecacheMedia", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginMap() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnBeginMap", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ExitMap() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnExitMap", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/);
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
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
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnClientEnterLevel", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
		/*[lua args]:*/clientEntity );
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientExitLevel( edict_t *clientEntity ) {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnClientExitLevel", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
		/*[lua args]:*/ clientEntity );
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
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
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnBeginServerFrame", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_RunFrame() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnRunFrame", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
		/*[lua args]:*/ level.framenum );
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_EndServerFrame() {
	// Ensure it is interpreted succesfully.
	SVG_Lua_ReturnIfNotInterpretedOK
	// Call function.
	const bool calledFunction = LUA_CallFunction( luaMapState.lState, "OnEndServerFrame", 1, 1, LUA_CALLFUNCTION_VERBOSE_MISSING /*, [lua args]:*/ );
	if ( calledFunction ) {
		// Pop the bool from stack.
		lua_pop( luaMapState.lState, 1 );
	}
	// Pop remaining stack.
	lua_pop( luaMapState.lState, lua_gettop( luaMapState.lState ) );
}