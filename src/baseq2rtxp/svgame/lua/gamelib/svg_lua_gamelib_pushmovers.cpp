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
#include "svgame/entities/svg_entities_pushermove.h"



/**
*
*
*
*	PushMovers API:
*
*
*
**/
/**
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
int GameLib_GetPushMoverState( lua_State *L ) {
	// Check if the first argument is numeric(the entity number).
	const int32_t entityNumber = luaL_checkinteger( L, 1 );

	// Validate entity numbers.
	if ( entityNumber < 0 || entityNumber >= game.maxentities ) {
		Lua_DeveloperPrintf( "%s: invalid entity number(#%d)\n", __func__, entityNumber );
		lua_pushinteger( L, -1 );
		return 1;
	}

	// Find entity and acquire its number.
	int32_t pusherMoveState = -1; // None by default.

	svg_edict_t *pushMoverEntity = g_edict_pool.EdictForNumber( entityNumber );
	if ( SVG_Entity_IsActive( pushMoverEntity ) ) {
		pusherMoveState = pushMoverEntity->pushMoveInfo.state;
	}

	// Store the result inside a type lua_Integer
	lua_Integer luaPusherMoveState = pusherMoveState;

	// Here we prepare the values to be returned.
	// First we push the values we want to return onto the stack in direct order.
	// Second, we must return the number of values pushed onto the stack.

	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, luaPusherMoveState );

	return 1; // The number of returned values
}
/**
*	@return	The push mover its' current state.
**/
const int32_t GameLib_GetPushMoverState( sol::this_state s, lua_edict_t pushMoverEntity ) {
	const int32_t pushMoverState = ( pushMoverEntity.handle.edictPtr ? pushMoverEntity.handle.edictPtr->pushMoveInfo.state : -1 );
	return pushMoverState;
	//// Get the first matching entity for the targetname.
	//svg_edict_t *targetNameEntity = SVG_Entities_Find( NULL, FOFS_GENTITY( targetname ), targetName.c_str() );
	//// Return it.
	//return targetNameEntity;
}