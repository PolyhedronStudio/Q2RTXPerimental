/********************************************************************
*
*
*	ServerGame: Lua GameLib API.
*	NameSpace: "Game".
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"



/**
*
*
*
*	Entities API:
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

	// Store the result inside a type lua_Integer
	lua_Integer luaEntityNumber = entityNumber;

	// Here we prepare the values to be returned.
	// First we push the values we want to return onto the stack in direct order.
	// Second, we must return the number of values pushed onto the stack.

	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, luaEntityNumber );

	return 1; // The number of returned values
}

/**
*	@return	The number of the first matching targetname entity in the entities array, -1 if not found.
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

	// Store the result inside a type lua_Integer
	lua_Integer luaEntityNumber = entityNumber;

	// Here we prepare the values to be returned.
	// First we push the values we want to return onto the stack in direct order.
	// Second, we must return the number of values pushed onto the stack.

	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, luaEntityNumber );

	return 1; // The number of returned values
}
/**
*	@return	The number of the team matching entities found in the entity array, -1 if none found.
*	@note	In Lua, it returns a table containing the entity number(s) with a matching targetname.
**/
int GameLib_GetEntitiesForTargetName( lua_State *L ) {
	// Check if the first argument is string.
	const char *targetName = luaL_checkstring( L, 1 );
	if ( !targetName ) {
		Lua_DeveloperPrintf( "%s: invalid string\n", __func__ );
		// Pushing the result onto the stack to be returned
		lua_Integer luaEntityNumber = -1;
		lua_pushinteger( L, luaEntityNumber );
		return 1;
	}

	// Find our first targetnamed entity, if any at all.
	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName );
	if ( !targetNameEntity ) {
		// Pushing the result onto the stack to be returned
		lua_Integer luaEntityNumber = -1;
		lua_pushinteger( L, luaEntityNumber );
		return 1;
	}

	// Store the result inside a type lua_Integer
	lua_Integer luaEntityNumber = targetNameEntity->s.number;
	// Num of targetnamed matching entities..
	int32_t numMatchingEntities = 0;

	// Push table to insert our entity number sequence.
	lua_createtable( L, 0, 0 );
	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, luaEntityNumber );
	// Assign the pushed integer value to our numMatchinEntities index in the table.
	lua_seti( L, -2, numMatchingEntities );

	// Iterate over all entities seeking for targetnamed ents.
	while ( 1 ) {
		// Find next entity in line.
		targetNameEntity = SVG_Find( targetNameEntity, FOFS( targetname ), targetName );
		// Exit if it's nullptr.
		if ( !targetNameEntity ) {
			break;
		}
		// Increment num entities value count.
		numMatchingEntities += 1;
		// Store the result inside a type lua_Integer
		lua_Integer luaEntityNumber = targetNameEntity->s.number;
		// Pushing the result onto the stack to be returned
		lua_pushinteger( L, luaEntityNumber );
		// Assign the pushed integer value to our numMatchinEntities index in the table.
		lua_rawseti( L, -2, numMatchingEntities );
	}

	return 1; // The number of returned values
}