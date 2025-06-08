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
#include "svgame/lua/svg_lua_signals.hpp"



/**
*
*
*
*	Signaling I/O API:
*
*
*
**/
DECLARE_GLOBAL_CALLBACK_THINK( LUA_Think_SignalOutDelay );
/**
*	@brief	Utility/Support routine for delaying SignalOut when a 'delay' is given to it.
**/
DEFINE_GLOBAL_CALLBACK_THINK( LUA_Think_SignalOutDelay )( svg_base_edict_t *entity ) -> void {
	svg_base_edict_t *creatorEntity = entity->delayed.signalOut.creatorEntity;
	if ( !SVG_Entity_IsActive( creatorEntity ) ) {
		return;
	}
	const char *signalName = entity->delayed.signalOut.name;
	bool propogateToLua = true;
	if ( creatorEntity->HasOnSignalInCallback() ) {
		/*propogateToLua = */creatorEntity->DispatchOnSignalInCallback(
			entity->other, entity->activator,
			signalName, entity->delayed.signalOut.arguments
		);
	}
	// If desired, propogate the signal to Lua '_OnSignalIn' callbacks.
	if ( propogateToLua ) {
		SVG_Lua_SignalOut( SVG_Lua_GetSolState(),
			creatorEntity, entity->other, entity->activator,
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
			// TODO: Hmm?? Sol has no notion, maybe though using sol::object.as however, of an integer??
			#if 0
			} else if ( lua_isinteger( L, -2 ) ) {
				// Get string value copy.
				lua_Integer value = lua_tointeger( L, -2 );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_INTEGER, key, {.integer = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %" PRIi64 "\n", key, static_cast<int64_t>( value ) );
			#endif
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
*	@brief	Support routine for GameLib_SignalOut.
**/
#include <algorithm>
static svg_signal_argument_array_t _GameLib_LuaTable_ToArgumentsArray( sol::this_state s, sol::table &table ) {
	svg_signal_argument_array_t signalArguments = { };
	
	// Iterate the signalArguments table and create our signal arguments "array" with its keys and values.
	// Use std::for_each to keep the lua stack happy :-)
	std::for_each( table.begin(), table.end(),
		[&s, &signalArguments]( std::pair<sol::object, sol::object> kvPair ) -> void {
			// We require string keys here.
			if ( kvPair.first.get_type() == sol::type::string ) {
				// Get value object reference.
				sol::object &valueObject = kvPair.second;
				// Get its type.
				sol::type valueType = valueObject.get_type();

				// We allow at the moment: numeric, integral, and string
				if ( valueType == sol::type::boolean ) {
					signalArguments.push_back( {
						.type = SIGNAL_ARGUMENT_TYPE_BOOLEAN,
						.key = kvPair.first.as<const char *>(),
						.value = { .boolean = kvPair.second.as<bool>() }
					} );
				}
				else if ( valueType == sol::type::number ) {
					signalArguments.push_back( {
						.type = SIGNAL_ARGUMENT_TYPE_NUMBER,
						.key = kvPair.first.as<const char *>(),
						.value = { .number = kvPair.second.as<double>() }
					} );
				}
				else if ( valueType == sol::type::string ) {
					signalArguments.push_back( {
						.type = SIGNAL_ARGUMENT_TYPE_STRING,
						.key = kvPair.first.as<const char *>(),
						.value = {.str = kvPair.second.as<const char *>() }
					} );
				}
				else if ( valueType == sol::type::nil /*|| valueType == sol::type::lua_nil */) {
					signalArguments.push_back( {
						.type = SIGNAL_ARGUMENT_TYPE_NULLPTR,
						.key = kvPair.first.as<const char *>(),
						.value = { 0 }
					} );
				} else {
					gi.dprintf( "%s: uncompatible singalArguments value type(%s) for key(%s)\n",
						__func__,
						sol::type_name( s, valueType).c_str(),
						kvPair.first.as<const char *>() );
				}
			}
		}
	);

	return signalArguments;
}

/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_SignalOut( sol::this_state s, lua_edict_t leEnt, lua_edict_t leSignaller, lua_edict_t leActivator, std::string signalName, sol::table signalArguments ) {
	// Make sure that the entity is at least active and valid to be signalling.
	if ( !SVG_Entity_IsActive( leEnt.handle.edictPtr ) ) {
		return -1; // SIGNALOUT_FAILED
	}

	// Stores the parsed sol::table results.
	svg_signal_argument_array_t signalArgumentsArray = {};// _GameLib_LuaTable_ToArgumentsArray( s, signalArguments );

	// Acquire pointers from lua_edict_t handles.
	svg_base_edict_t *entity = leEnt.handle.edictPtr;
	svg_base_edict_t *signaller = leSignaller.handle.edictPtr;
	svg_base_edict_t *activator = leActivator.handle.edictPtr;

	// Spawn a delayed signal out entity if a delay was requested.
	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		svg_base_edict_t *delayEntity = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
		// In case it failed to allocate of course.
		if ( !SVG_Entity_IsActive( delayEntity ) ) {
			return -1; // SIGNALOUT_FAILED
		}
		delayEntity->classname = "DelayedLuaSignalOut";

		delayEntity->nextthink = level.time + QMTime::FromMilliseconds( entity->delay );
		delayEntity->SetThinkCallback( LUA_Think_SignalOutDelay );
		if ( !activator ) {
			gi.dprintf( "LUA_Think_SignalOutDelay with no activator\n" );
		}
		delayEntity->activator = activator;
		delayEntity->other = signaller;

		delayEntity->message = entity->message;

		delayEntity->targetEntities.target = entity->targetEntities.target;
		delayEntity->targetNames.target = entity->targetNames.target;
		delayEntity->targetNames.kill = entity->targetNames.kill;

		// The luaName of the actual original entity.
		delayEntity->luaProperties.luaName = entity->luaProperties.luaName;
		// The entity which created this temporary delay signal entity.
		delayEntity->delayed.signalOut.creatorEntity = entity;
		// The arguments of said signal.
		delayEntity->delayed.signalOut.arguments = signalArgumentsArray;
		// The actual string comes from lua so we need to copy it in instead.
		memset( delayEntity->delayed.signalOut.name, 0, sizeof( delayEntity->delayed.signalOut.name ) );
		signalName.copy( delayEntity->delayed.signalOut.name, signalName.size(), 0 );

		return 0; // SIGNALOUT_DELAYED
	}

	// Fire the signal if it has a OnSignal method registered.
	bool propogateToLua = true;
	bool sentSignalOut = false;
	if ( entity->HasOnSignalInCallback() ) {
		// Set activator/other.
		entity->activator = activator;
		entity->other = signaller;
		// Fire away.
		/*propogateToLua = */entity->DispatchOnSignalInCallback( signaller, activator, signalName.c_str(), signalArgumentsArray );
		sentSignalOut = true;
	}

	// If desired, propogate the signal to Lua '_OnSignalIn' callbacks.
	if ( propogateToLua ) {
		// Set activator/other.
		entity->activator = activator;
		entity->other = signaller;
		// Get view for state.
		sol::state_view solStateView = sol::state_view( s );
		// Fire away.
		SVG_Lua_SignalOut( solStateView, entity, signaller, activator, signalName.c_str(), signalArgumentsArray, LUA_CALLFUNCTION_VERBOSE_NOT );
		sentSignalOut = true;
	}

	return 1; // SIGNALOUT_SIGNALLED
}