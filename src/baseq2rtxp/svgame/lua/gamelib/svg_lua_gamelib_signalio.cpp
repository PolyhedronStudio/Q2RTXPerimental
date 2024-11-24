/********************************************************************
*
*
*	ServerGame: Lua Binding API functions for 'Signal I/O'.
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
*	Signaling I/O API:
*
*
*
**/
/**
*	@brief	Utility/Support routine for delaying SignalOut when a 'delay' is given to it.
**/
void LUA_Think_SignalOutDelay( edict_t *entity ) {
	edict_t *creatorEntity = entity->delayed.signalOut.creatorEntity;
	if ( !creatorEntity ) {
		return;
	}
	const char *signalName = entity->delayed.signalOut.name;
	bool propogateToLua = true;
	if ( creatorEntity->onsignalin ) {
		/*propogateToLua = */creatorEntity->onsignalin(
			creatorEntity, entity->other, entity->activator,
			signalName, entity->delayed.signalOut.arguments
		);
	}
	// If desired, propogate the signal to Lua '_OnSignalIn' callbacks.
	if ( propogateToLua ) {
		SVG_Lua_SignalOut( SVG_Lua_GetMapLuaState(),
			entity, entity->other, entity->activator,
			signalName, entity->delayed.signalOut.arguments
		);
	}
	SVG_FreeEdict( entity );
}
/**
*	@brief	Supporting routine for GameLib_SignalOut
**/
static const svg_signal_argument_array_t GameLib_SignalOut_ParseArgumentsTable( lua_State *L ) {
	// The final result.
	svg_signal_argument_array_t signalArguments = {};

	// Is there a fifth stack variable, typed as a table?
	if ( lua_gettop( L ) == 5 && lua_istable( L, 5 ) ) {
		// Push another reference to the table on top of the stack (so we know where it is.)
		lua_pushvalue( L, 5 ); // -1 = table
		// Push nill for stack space.
		lua_pushnil( L ); // -1 = nil; -2 = table
		// Debug Print:
		gi.dprintf( "%s: signalArguments = {\n", __func__ );

		// Iterate over table.
		while ( lua_next( L, -2 ) ) {
			// Stack now contains: -1 => value; -2 => key; -3 => table
			// copy the key so that lua_tostring does not modify the original
			lua_pushvalue( L, -2 );
			// Stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
			const char *key = lua_tostring( L, -1 );

			// We only pass through, integers, numbers, and strings.
			// Act accordingly to that.
			if ( lua_isboolean( L, -2 ) ) {
				// Get string value copy.
				bool value = ( lua_tointeger( L, -2 ) ? true : false );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_BOOLEAN, key, {.boolean = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %s\n", key, value ? "true" : "false" );
			} else if ( lua_isinteger( L, -2 ) ) {
				// Get string value copy.
				lua_Integer value = lua_tointeger( L, -2 );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_INTEGER, key, {.integer = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %" PRIi64 "\n", key, static_cast<int64_t>( value ) );
			} else if ( lua_isnumber( L, -2 ) ) {
				// Get string value copy.
				lua_Number value = lua_tonumber( L, -2 );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_NUMBER, key, {.number = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %f\n", key, value );
			} else if ( lua_isstring( L, -2 ) ) {
				// Get string value copy.
				const char *value = lua_tostring( L, -2 );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_STRING, key, {.str = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %s\n", key, value );
			} else {
				// Debug Print:
				gi.dprintf( "%s: signalArguments[%s] => has unacceptable signal argument type: \"%s\"\n", __func__, key, lua_typename( L, -2 ) );
			}

			// pop value + copy of key, leaving original key
			lua_pop( L, 2 );
			// stack now contains: -1 => key; -2 => table
		}
		// stack now contains: -1 => table (when lua_next returns 0 it pops the key
		// but does not push anything.)
		// Pop table
		lua_pop( L, 1 );
		// Stack is now the same as it was on entry to this function
		// Debug Print:
		gi.dprintf( "}\n" );
	}

	return signalArguments;
}

/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
int GameLib_SignalOut( lua_State *L ) {
	// 
	const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const int32_t signallerEntityNumber = luaL_checkinteger( L, 2 );
	const int32_t activatorEntityNumber = luaL_checkinteger( L, 3 );
	const char *signalName = static_cast<const char *>( luaL_checkstring( L, 4 ) );
	svg_signal_argument_array_t signalArguments = GameLib_SignalOut_ParseArgumentsTable( L );

	// Validate entity numbers.
	if ( entityNumber < 0 || entityNumber >= game.maxentities ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	#if 0
	if ( signallerEntityNumber < 0 || signallerEntityNumber >= game.maxentities ) {
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
	if ( !g_edicts[ signallerEntityNumber ].inuse ) {
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
	edict_t *signaller = ( signallerEntityNumber != -1 ? &g_edicts[ signallerEntityNumber ] : nullptr );
	edict_t *activator = ( activatorEntityNumber != -1 ? &g_edicts[ activatorEntityNumber ] : nullptr );

	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		edict_t *delayEntity = SVG_AllocateEdict();
		delayEntity->classname = "DelayedLuaSignalOut";
		delayEntity->nextthink = level.time + sg_time_t::from_sec( entity->delay );
		delayEntity->think = LUA_Think_SignalOutDelay;
		if ( !activator ) {
			gi.dprintf( "LUA_Think_SignalOutDelay with no activator\n" );
		}

		delayEntity->activator = activator;
		delayEntity->other = signaller;
		delayEntity->message = entity->message;

		delayEntity->targetNames.target = entity->targetNames.target;
		delayEntity->targetNames.kill = entity->targetNames.kill;

		// The luaName of the actual original entity.
		delayEntity->luaProperties.luaName = entity->luaProperties.luaName;
		// The entity which created this temporary delay signal entity.
		delayEntity->delayed.signalOut.creatorEntity = entity;
		// The arguments of said signal.
		delayEntity->delayed.signalOut.arguments = signalArguments;
		// The actual string comes from lua so we need to copy it in instead.
		memset( delayEntity->delayed.signalOut.name, 0, sizeof( delayEntity->delayed.signalOut.name ) );
		Q_strlcpy( delayEntity->delayed.signalOut.name, signalName, strlen( signalName ) );

		// Return 0, UseTarget has not actually used its target yet.
		lua_pushinteger( L, 0 );
		return 1;
	}

	// Fire the signal if it has a OnSignal method registered.
	bool propogateToLua = true;
	bool sentSignalOut = false;
	if ( entity->onsignalin ) {
		// Set activator/other.
		entity->activator = activator;
		entity->other = signaller;
		// Fire away.
		/*propogateToLua = */entity->onsignalin( entity, signaller, activator, signalName, signalArguments );
		sentSignalOut = true;
	}

	// If desired, propogate the signal to Lua '_OnSignalIn' callbacks.
	if ( propogateToLua ) {
		SVG_Lua_SignalOut( SVG_Lua_GetMapLuaState(), entity, signaller, activator, signalName, signalArguments );
		sentSignalOut = true;
	}

	// Return 1, we have used our method.
	lua_pushinteger( L, sentSignalOut );
	return 1;
}