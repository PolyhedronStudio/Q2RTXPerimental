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
*	Lua GameLib:
*
*
*
**/
/**
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
int GameLib_GetEntityForLuaName( lua_State *L ) {
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

	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( luaProperties.luaName ), luaName );
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
int GameLib_GetEntityForTargetName( lua_State *L ) {
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

	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName );
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
*	@brief	Utility/Support routine for delaying UseTarget when the 'delay' key/value is set on an entity.
**/
void LUA_Think_UseTargetDelay( edict_t *entity ) {
	edict_t *creatorEntity = entity->luaProperties.delayedUseCreatorEntity;
	const entity_usetarget_type_t useType = entity->luaProperties.delayedUseType;
	const int32_t useValue = entity->luaProperties.delayedUseValue;
	if ( creatorEntity->use ) {
		creatorEntity->use( 
			creatorEntity,
			entity->other, 
			entity->activator,
			useType,
			useValue );
	}
	SVG_FreeEdict( entity );
}
/**
*	@return	The number of the entity if it has a matching targetname, -1 otherwise.
**/
int GameLib_UseTarget( lua_State *L ) {
	// 
	const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const int32_t otherEntityNumber = luaL_checkinteger( L, 2 );
	const int32_t activatorEntityNumber = luaL_checkinteger( L, 3 );
	const entity_usetarget_type_t useType = static_cast<const entity_usetarget_type_t>( luaL_checkinteger( L, 4 ) );
	const int32_t useValue = static_cast<const int32_t>( luaL_checkinteger( L, 5 ) );

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

	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		edict_t *t = SVG_AllocateEdict();
		t->classname = "DelayedUse";
		t->nextthink = level.time + sg_time_t::from_sec( entity->delay );
		t->think = LUA_Think_UseTargetDelay;

		t->activator = activator;
		t->other = other;
		if ( !activator ) {
			gi.dprintf( "Think_Delay with no activator\n" );
		}

		t->luaProperties.luaName = entity->luaProperties.luaName;
		t->luaProperties.delayedUseCreatorEntity = entity;
		t->luaProperties.delayedUseType = useType;
		t->luaProperties.delayedUseValue = useValue;

		t->message = entity->message;
		t->targetNames.target = entity->targetNames.target;
		t->targetNames.kill = entity->targetNames.kill;

		// Return 0, UseTarget has not actually used its target yet.
		lua_pushinteger( L, 0 );
		return 1;
	}

	// Fire the use method if it has any.
	if ( entity->use ) {
		entity->activator = activator;
		entity->other = other;
		entity->use( entity, other, activator, useType, useValue );

		// Return 1, we have used our method.
		lua_pushinteger( L, 1 );
		return 1;
	} else {
		// Return 0, UseTarget has not actually used its target yet.
		lua_pushinteger( L, 0 );
		return 1;
	}
}
/**
*	@brief	Game Namespace Functions:
**/
const struct luaL_Reg GameLib[] = {
	{ "GetEntityForLuaName", GameLib_GetEntityForLuaName },
	{ "GetEntityForTargetName", GameLib_GetEntityForTargetName },
	{ "UseTarget", GameLib_UseTarget },
	{ NULL, NULL }
};
/**
*	@brief	Initializes the GameLib
**/
void GameLib_Initialize( lua_State *L ) {
	// We create a new table
	lua_newtable( L );

	// Here we set all functions from GameLib array into
	// the table on the top of the stack
	luaL_setfuncs( L, GameLib, 0 );

	// We get the table and set as global variable
	lua_setglobal( L, "Game" );

	if ( luaL_dostring( L, "Core.DPrint(\"Initialized Lua GameLib\n\"" ) == LUA_OK ) {
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
	lua_pop( lMapState, 1 );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ExitMap() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnExitMap", 1 );
	lua_pop( lMapState, 1 );
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
	lua_pop( lMapState, 1 );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_ClientExitLevel( edict_t *clientEntity ) {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnClientExitLevel", 1, clientEntity );
	lua_pop( lMapState, 1 );
}


//
//	Generic Server Frame stuff.
//
/**
*	@brief
**/
void SVG_Lua_CallBack_BeginServerFrame() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnBeginServerFrame", 1 );
	lua_pop( lMapState, 1 );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_RunFrame() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnRunFrame", 1, level.framenum );
	lua_pop( lMapState, 1 );
}
/**
*	@brief
**/
void SVG_Lua_CallBack_EndServerFrame() {
	const bool calledFunction = LUA_CallFunction( lMapState, "OnEndServerFrame", 1 );
	lua_pop( lMapState, 1 );
}