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
*	TODO: This is DJ Not Nice, code is not neat, needs fixing.
*
*
8
**/
//! For developer prints:
#ifdef Lua_DEBUG_OUTPUT
#define Lua_DeveloperPrintf(...) gi.dprintf( __VA_ARGS__ );
#else
#define Lua_DeveloperPrintf(...) 
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
	EntitiesLib_Initialize( lMapState );
}
/**
*	@brief 
**/
void SVG_Lua_Shutdown() {
	// Close LUA Map State.
	lua_close( lMapState );
}

/**
*	@brief
**/
lua_State *SVG_Lua_GetMapLuaState() {
	return lMapState;
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
			Lua_DeveloperPrintf( "%s: Succesfully interpreted buffer\n", __func__ );
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
	Lua_DeveloperPrintf( "%s: Failed interpreting buffer, unknown error.\n", __func__ );
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
			Lua_DeveloperPrintf( "%s: Loaded Lua Script File: %s\n", __func__, filePath.c_str() );

			if ( LUA_InterpreteString( mapScriptBuffer ) ) {
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
		Lua_DeveloperPrintf( "%s: invalid string\n", __func__ );
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
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
int EntitiesLib_GetForLuaName( lua_State *L ) {
	// Check if the first argument is string.
	const char *luaName = luaL_checkstring( L, 1 );
	if ( !luaName ) {
		Lua_DeveloperPrintf( "%s: invalid string\n", __func__ );
		// Pushing the result onto the stack to be returned
		lua_Integer luaEntityNumber = -1;
		lua_pushinteger( L, luaEntityNumber );
		return 1;
	}

	// Find entity and acquire its number.
	int32_t entityNumber = -1; // None by default.

	edict_t *targetNameEntity = G_Find( NULL, FOFS( luaProperties.luaName ), luaName );
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
int EntitiesLib_GetForTargetName( lua_State *L ) {
	// Check if the first argument is string.
	const char *targetName = luaL_checkstring( L, 1 );
	if ( !targetName ) {
		Lua_DeveloperPrintf( "%s: invalid string\n", __func__ );
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
int EntitiesLib_UseTarget( lua_State *L ) {
	// 
	const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const int32_t otherEntityNumber = luaL_checkinteger( L, 2 );
	const int32_t activatorEntityNumber = luaL_checkinteger( L, 3 );

	// Validate entity numbers.
	if ( entityNumber < 0 || activatorEntityNumber >= game.maxentities ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	#if 0
	if ( otherEntityNumber < 0 || otherEntityNumber >= game.maxentities ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	if ( activatorEntityNumber < 0 || activatorEntityNumber >= game.maxentities ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	#endif

	// See if the targetted entity is inuse.
	if ( !g_edicts[ entityNumber ].inuse ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	#if 0
	if ( !g_edicts[ otherEntityNumber ].inuse ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	// See if the activator is inuse.
	if ( !g_edicts[ activatorEntityNumber ].inuse ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	#endif

	// Perform UseTargets
	edict_t *entity = &g_edicts[ entityNumber ];
	edict_t *other = ( otherEntityNumber != -1 ? &g_edicts[otherEntityNumber] : nullptr );
	edict_t *activator = ( activatorEntityNumber != -1 ? &g_edicts[ activatorEntityNumber ] : nullptr );
	//_UseTargets( entity, activator );

	if ( entity->use ) {
		entity->activator = activator;
		entity->other = other;
		entity->use( entity, other, activator );
	}

	lua_pushinteger( L, 1 );
	return 1;
}
/**
*	@brief	Entities Namespace Functions:
**/
const struct luaL_Reg EntitiesLib[] = {
	{ "GetForLuaName", EntitiesLib_GetForLuaName },
	{ "GetForTargetName", EntitiesLib_GetForTargetName },
	{ "UseTarget", EntitiesLib_UseTarget },
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
//static const bool LUA_CallFunction( const char *functionName ) {
//	// We require the script to be succesfully interpreted.
//	if ( !mapScriptInterpreted || !lMapState || !functionName ) {
//		return false;
//	}
//
//	// Get the global functionname value and push it to stack:
//	lua_getglobal( lMapState, functionName );
//
//	// Check if function even exists.
//	if ( lua_isfunction( lMapState, -1 ) ) {
//		// Protect Call the pushed function name string.
//		if ( lua_pcall( lMapState, 0, 1, 0 ) == LUA_OK ) {
//			// Pop function name from stack.
//			lua_pop( lMapState, lua_gettop( lMapState ) );
//			// Success.
//			return true;
//		// Failure:
//		} else {
//			// Get error.
//			const std::string errorStr = lua_tostring( lMapState, lua_gettop( lMapState ) );
//			// Remove the errorStr from the stack
//			lua_pop( lMapState, lua_gettop( lMapState ) );
//			// Output.
//			LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
//			// Failure.
//			return false;
//		}
//	// Failure:
//	} else {
//		// Remove the function from the stack
//		lua_pop( lMapState, lua_gettop( lMapState ) );
//
//		LUA_ErrorPrintf( "%s: %s is not a function\n", __func__, functionName );
//		return false;
//	}
//}

/**
*	@brief	Calls specified function.
**/
const bool SVG_Lua_DispatchTargetNameUseCallBack( edict_t *self, edict_t *other, edict_t *activator ) {
	// We require the script to be succesfully interpreted.
	if ( !mapScriptInterpreted || !lMapState || !self ) {
		return false;
	}

	// Determine the function name based on self.targetname.
	const std::string functionName = std::string( self->targetname ) + "_onUse";

	// Dispatch.
	int32_t numRetValues = LUA_CallFunction( lMapState, functionName.c_str(), 1, self, other, activator);

	// Pop return value from stack.
	for ( int32_t i = 0; i < numRetValues; i++ ) {
		lua_pop( lMapState, i );
	}

	return true;

	//// Check if function even exists.
	//if ( lua_isfunction( lMapState, -1 ) ) {
	//	// Push the entity state numbers for the onUse callback.
	//	lua_pushinteger( lMapState, self->s.number );
	//	if ( other && other->inuse ) {
	//		lua_pushinteger( lMapState, other->s.number );
	//	} else {
	//		lua_pushinteger( lMapState, -1 );
	//	}
	//	if ( activator && activator->inuse ) {
	//		lua_pushinteger( lMapState, activator->s.number );
	//	} else {
	//		lua_pushinteger( lMapState, -1 );
	//	}


	//	// Protect Call the pushed function name string.
	//	if ( lua_pcall( lMapState, 3, 1, 0 ) == LUA_OK ) {
	//		// Pop function name from stack.
	//		lua_pop( lMapState, lua_gettop( lMapState ) );
	//		// Success.
	//		return true;
	//		// Failure:
	//	} else {
	//		// Get error.
	//		const std::string errorStr = lua_tostring( lMapState, lua_gettop( lMapState ) );
	//		// Remove the errorStr from the stack
	//		lua_pop( lMapState, lua_gettop( lMapState ) );
	//		// Output.
	//		LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
	//		// Failure.
	//		return false;
	//	}
	//	// Failure:
	//} else {
	//	LUA_ErrorPrintf( "%s: %s is not a function\n", __func__, functionName.c_str() );
	//	// Remove the function from the stack
	//	lua_pop( lMapState, lua_gettop( lMapState ) );
	//	return false;
	//}
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
	VPrint( "FUNCTION NAME LOL", "Hello World! VARIADIC!@!!\n" );
	//VPrint( "aids", "dacht je dat?" );
	const bool calledFunction = LUA_CallFunction( lMapState, "OnBeginMap", 1 );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ExitMap() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnExitMap", 1 );
}


//
//	For when a client is connecting thus entering the active map(level) 
//	or disconnecting, thus exiting the map(level).
//
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientEnterLevel( edict_t *clientEntity ) {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnClientEnterLevel", 1, clientEntity );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientExitLevel( edict_t *clientEntity ) {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnClientExitLevel", 1, clientEntity );
}


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginServerFrame() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnBeginServerFrame", 1 );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_RunFrame() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnRunFrame", 1, level.framenum );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_EndServerFrame() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnEndServerFrame", 1 );
}