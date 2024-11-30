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

// UserTypes:
#include "svgame/lua/usertypes/svg_lua_usertype_edict_t.hpp"

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
void CoreLib_Initialize( sol::state_view &solStateView );
/**
*	@brief	Initializes the GameLib
**/
void GameLib_Initialize( sol::state_view &solStateView );
/**
*	@brief	Initializes the MediaLib
**/
void MediaLib_Initialize( sol::state_view &solStateView );



/**
*
* 
*
*	TODO: Needs structurisation.
* 
* 
* 
**/
static struct LuaMapInstance {
	//! lua state.
	lua_State *lState;
	// State view on lState.
	sol::state solState;
	//! map script state
	char *scriptBuffer;
	//! Did we succesfully parse, and load it?
	bool scriptInterpreted;

	// References to map callback hooks.
	struct {
		//! 
		sol::protected_function OnPrecacheMedia;
		//! 
		sol::protected_function OnBeginMap;
		//! 
		sol::protected_function OnExitMap;

		//!
		sol::protected_function OnClientEnterLevel;
		//!
		sol::protected_function OnClientExitLevel;

		//!
		sol::protected_function OnBeginServerFrame;
		//!
		sol::protected_function OnEndServerFrame;
		//!
		sol::protected_function OnRunFrame;
	} callBacks;
} luaMapInstance = { };



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
*	@brief	For now a Support routine for LoadMapScript.
*			TODO: The buffer is stored in tag_filesystem so... basically...
**/
static void LUA_UnloadMapScript() {
	/**
	*	Very important with how libSol operates to, clean up here properly.
	**/
	// No more.
	luaMapInstance.scriptInterpreted = false;
	// Nullptr all callBack references.
	luaMapInstance.callBacks = {};
	// Clear out (luaL_close) the SOL Lua State.
	luaMapInstance.solState = nullptr;

	// TODO: This is technically not safe, we need to store the buffer elsewhere ... expose FS_LoadFileEx!
	SG_FS_FreeFile( luaMapInstance.scriptBuffer );
	luaMapInstance.scriptBuffer = nullptr;
}
/**
*	@brief	Finds all references to neccessary core map hook callbacks.
**/
static void LUA_FindCallBackReferences() {
	/**
	*	OnPrecacheMedia
	**/
	// Get function object.
	sol::protected_function &funcRefOnPrecacheMedia = luaMapInstance.callBacks.OnPrecacheMedia = luaMapInstance.solState[ "OnPrecacheMedia" ];
	// Get type.
	sol::type funcRefType = funcRefOnPrecacheMedia.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnPrecacheMedia.is<std::function<bool()>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnPrecacheMedia.reset();
	}

	/**
	*	OnBeginMap
	**/
	// Get function object.
	sol::protected_function &funcRefOnBeginMap = luaMapInstance.callBacks.OnBeginMap = luaMapInstance.solState[ "OnBeginMap" ];
	// Get type.
	funcRefType = funcRefOnBeginMap.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnBeginMap.is<std::function<bool()>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnBeginMap.reset();
	}
	/**
	*	OnExitMap
	**/
	// Get function object.
	sol::protected_function &funcRefOnExitMap = luaMapInstance.callBacks.OnExitMap = luaMapInstance.solState[ "OnExitMap" ];
	// Get type.
	funcRefType = funcRefOnExitMap.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnExitMap.is<std::function<bool()>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnExitMap.reset();
	}

	/**
	*	OnClientEnterLevel
	**/
	// Get function object.
	sol::protected_function &funcRefOnClientEnterLevel = luaMapInstance.callBacks.OnClientEnterLevel = luaMapInstance.solState[ "OnClientEnterLevel" ];
	// Get type.
	funcRefType = funcRefOnClientEnterLevel.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnClientEnterLevel.is<std::function<bool(lua_edict_t&)>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnClientEnterLevel.reset();
	}
	/**
	*	OnClientExitLevel
	**/
	// Get function object.
	sol::protected_function &funcRefOnClientExitLevel = luaMapInstance.callBacks.OnClientExitLevel = luaMapInstance.solState[ "OnClientExitLevel" ];
	// Get type.
	funcRefType = funcRefOnClientExitLevel.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnClientExitLevel.is<std::function<bool(lua_edict_t&)>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnClientExitLevel.reset();
	}

	/**
	*	OnBeginServerFrame
	**/
	// Get function object.
	sol::protected_function &funcRefOnBeginServerFrame = luaMapInstance.callBacks.OnBeginServerFrame = luaMapInstance.solState[ "OnBeginServerFrame" ];
	// Get type.
	funcRefType = funcRefOnBeginServerFrame.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnBeginServerFrame.is<std::function<bool()>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnBeginServerFrame.reset();
	}
	/**
	*	OnRunFrame
	**/
	// Get function object.
	sol::protected_function &funcRefOnRunFrame = luaMapInstance.callBacks.OnRunFrame = luaMapInstance.solState[ "OnRunFrame" ];
	// Get type.
	funcRefType = funcRefOnRunFrame.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnRunFrame.is<std::function<bool(uint64_t)>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnRunFrame.reset();
	}
	/**
	*	OnEndServerFrame
	**/
	// Get function object.
	sol::protected_function &funcRefOnEndServerFrame = luaMapInstance.callBacks.OnEndServerFrame = luaMapInstance.solState[ "OnEndServerFrame" ];
	// Get type.
	funcRefType = funcRefOnEndServerFrame.get_type();
	// Ensure it matches, and if not, unset it to a nullptr.
	if ( funcRefType != sol::type::function || !funcRefOnEndServerFrame.is<std::function<bool()>>() ) {
		// Reset it so it is LUA_NOREF and luaState == nullptr again.
		funcRefOnEndServerFrame.reset();
	}
}

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
	// Get our sol state view.
	//luaMapInstance.solState = sol::state(  );
	luaMapInstance.solState.open_libraries( sol::lib::base 
		| sol::lib::string 
		| sol::lib::table 
		| sol::lib::math 
		#if USE_DEBUG
		| sol::lib::debug
		#endif
	);

	//
	luaMapInstance.lState = luaMapInstance.solState.lua_state();
	
	// Initialize UserTypes:
	UserType_Register_Edict_t( luaMapInstance.solState );


	// Initialize Game Libraries.
	CoreLib_Initialize( luaMapInstance.solState );
	GameLib_Initialize( luaMapInstance.solState );
	MediaLib_Initialize( luaMapInstance.solState );
}
/**
*	@brief 
**/
void SVG_Lua_Shutdown() {
	// luaMapInstance.solState destructs itself at DLL unloading.
	LUA_UnloadMapScript();
}

/**
*	@brief	Returns a pointe rto the Lua State(Thread) that handles the map logic.
**/
sol::state &SVG_Lua_GetSolState() {
	return luaMapInstance.solState;
}
/**
*	@brief	
**/
sol::state_view &SVG_Lua_GetSolStateView() {
	return luaMapInstance.solState;
}
/**
*	@brief	Returns true if the map script has been interpreted properly.
**/
inline const bool SVG_Lua_IsMapScriptInterpreted() {
	return luaMapInstance.scriptBuffer && luaMapInstance.scriptInterpreted;
}

/**
*	Will load a chunk of LUA and compile it, if given a filename the chunk will
*	be stored with a filename identifier, used to display errors etc with line numbers.
**/
static const bool LUA_InterpreteString( const char *fileName, const char *buffer ) {
	// Get a state_view reference to the solState.
	sol::state_view &solState = luaMapInstance.solState;

	if ( fileName != nullptr ) {
		// Load buffer into fileName chunk.
		sol::load_result fileLoadResult = solState.load( buffer, fileName );
		// Check for load errors.
		sol::load_status loadStatus = fileLoadResult.status();
		if ( !fileLoadResult.valid() || loadStatus != sol::load_status::ok ) {
			sol::error errorStatus = fileLoadResult;
			gi.bprintf( PRINT_ERROR, "%s:[%s]: %s\n", __func__, sol::to_string( loadStatus ).c_str(), errorStatus.what() );
			return false;
		}
		// Execute it.
		sol::protected_function_result resultValue = fileLoadResult();
		// Check for runtime errors.
		if ( resultValue.status() != sol::call_status::ok ) { // We don't support yielding. (Impossible to save/load aka restore state.)
			gi.bprintf( PRINT_ERROR, "%s:CallStatus[%s]\n", __func__, sol::to_string( resultValue.status() ).c_str() );
			return false;
		}
	} else {
		#if 1
		// Execute it.
		sol::protected_function_result resultValue = solState.do_string( buffer );
		// Check for runtime errors.
		if ( resultValue.status() != sol::call_status::ok ) { // We don't support yielding. (Impossible to save/load aka restore state.)
			gi.bprintf( PRINT_ERROR, "%s:CallStatus[%s]\n", __func__, sol::to_string( resultValue.status() ).c_str() );
			return false;
		}
		#else
		auto prtCallResult = solState.safe_script( buffer,
		[]( lua_State *, sol::protected_function_result pfr ) {
				if ( !pfr.valid() ) {
					sol::call_status statusResult = pfr.status();
					gi.bprintf( PRINT_ERROR, "%s:[%s]\n", __func__, sol::to_string( statusResult ).c_str() );
					//luaMapInstance.scriptInterpreted = false;
				}
				return pfr;
		});
		
		sol::call_status callStatus = prtCallResult.status();
		if ( !prtCallResult.valid() || callStatus != sol::call_status::ok ) {
			sol::error errorStatus = prtCallResult;
			gi.bprintf( PRINT_ERROR, "%s:[%s]: %s\n", __func__, sol::to_string( callStatus ).c_str(), errorStatus.what() );
			return false;
		}
		#endif
	}
	// Return ok.
	return true;
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

	// Reinitialize the SOL lua State.
	SVG_Lua_Initialize();

	// Ensure file exists.
	if ( SG_FS_FileExistsEx( filePath.c_str(), 0 ) ) {
		// Load Lua file.
		size_t scriptFileLength = SG_FS_LoadFile( filePath.c_str(), (void **)&luaMapInstance.scriptBuffer );

		if ( luaMapInstance.scriptBuffer && scriptFileLength != Q_ERR( ENOENT ) ) {
			// Debug output.			
			Lua_DeveloperPrintf( "%s: Loaded Lua Script: %s\n", __func__, filePath.c_str() );

			if ( LUA_InterpreteString( fileNameExt.c_str(), luaMapInstance.scriptBuffer ) ) {
				// Debug output:
				Lua_DeveloperPrintf( "%s: Parsed Lua Script: %s\n", __func__, filePath.c_str() );
				// Succeeded.
				luaMapInstance.scriptInterpreted = true;

				// Acquire references to global map callback functions.
				LUA_FindCallBackReferences();
			}
		// Failure:
		} else {
			// Debug output.
			Lua_DeveloperPrintf( "%s: Load Lua Script Failure: %s\n", __func__, filePath.c_str() );
		}
	// Failure:
	} else {
		// Debug output:
		Lua_DeveloperPrintf( "%s: Map %s has no existing Lua script file\n", __func__, scriptName.c_str() );
	}

	// Return result.
	return luaMapInstance.scriptInterpreted;
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
// For checking whether to proceed lua callbacks or not.
inline const bool SVG_Lua_IsMapScriptInterpreted();
#define LUA_CanDispatchCallback( callBackObject ) \
	if ( !SVG_Lua_IsMapScriptInterpreted() || !callBackObject.valid() ) { \
		return; \
}

//
//	For when a map is just loaded up, or about to be unloaded.
//
void SVG_Lua_CallBack_OnPrecacheMedia() {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnPrecacheMedia );

	// Expect true.
	bool retval = true;
	try {
		retval = luaMapInstance.callBacks.OnPrecacheMedia();
	// Print the error in case of failure.
	} catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginMap() {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnBeginMap );

	// Expect true.
	bool retval = true;
	try {
		retval = luaMapInstance.callBacks.OnBeginMap();
	// Print the error in case of failure.
	}
	catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ExitMap() {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnExitMap );

	// Expect true.
	bool retval = true;
	try {
		retval = luaMapInstance.callBacks.OnExitMap();
	// Print the error in case of failure.
	}
	catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}


//
//	For when a client is connecting thus entering the active map(level) 
//	or disconnecting, thus exiting the map(level).
//
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientEnterLevel( edict_t *clientEntity ) {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnClientEnterLevel );

	// Make sure it is a valid client entity.
	if ( !SVG_IsClientEntity( clientEntity ) ) {
		gi.dprintf( "%s: invalid clientEntity passed.\n", __func__ );
		return;
	}

	// Expect true.
	bool retval = true;
	try {
		// Create a lua edict handle structure to work with.
		lua_edict_t luaEdict = lua_edict_t( clientEntity );
		// Call upon method.
		retval = luaMapInstance.callBacks.OnClientEnterLevel( luaEdict );
	// Print the error in case of failure.
	}
	catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientExitLevel( edict_t *clientEntity ) {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnClientExitLevel );

	// Make sure it is a valid client entity.
	if ( !SVG_IsClientEntity( clientEntity ) ) {
		gi.dprintf( "%s: invalid clientEntity passed.\n", __func__ );
		return;
	}

	// Expect true.
	bool retval = true;
	try {
		// Create a lua edict handle structure to work with.
		lua_edict_t luaEdict = lua_edict_t( clientEntity );
		// Call upon method.
		retval = luaMapInstance.callBacks.OnClientExitLevel( luaEdict );
	// Print the error in case of failure.
	}
	catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginServerFrame() {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnBeginServerFrame );

	// Expect true.
	bool retval = true;
	try {
		retval = luaMapInstance.callBacks.OnBeginServerFrame( );
	// Print the error in case of failure.
	}
	catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}
/**
*	@brief
**/
void SVG_Lua_CallBack_RunFrame() {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnRunFrame );

	// Expect true.
	bool retval = true;
	try {
		retval = luaMapInstance.callBacks.OnRunFrame( level.framenum );
	// Print the error in case of failure.
	}
	catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}
/**
*	@brief
**/
void SVG_Lua_CallBack_EndServerFrame() {
	// Ensure map script was interpreted, and the callback was found to be in the script.
	LUA_CanDispatchCallback( luaMapInstance.callBacks.OnEndServerFrame );

	// Expect true.
	bool retval = true;
	try {
		retval = luaMapInstance.callBacks.OnEndServerFrame();
	// Print the error in case of failure.
	}
	catch ( const std::exception &e ) {
		gi.dprintf( "%s: %s\n ", __func__, e.what() );
	}
}