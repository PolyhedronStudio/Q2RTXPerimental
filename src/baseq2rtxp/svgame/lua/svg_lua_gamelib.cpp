/********************************************************************
*
*
* 
*	ServerGmae: Lua Library: Game
*
* 
* 
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/entities/svg_entities_pushermove.h"


/**
*
*
*
*	Share this to game modules, now resides in common/error.h....
*
*
*
**/
//// WID: TODO: move to shared?
//#include <errno.h>
//
//#define ERRNO_MAX       0x5000
//
//#if EINVAL > 0
//#define Q_ERR(e)        (e < 1 || e > ERRNO_MAX ? -ERRNO_MAX : -e)
//#else
//#define Q_ERR(e)        (e > -1 || e < -ERRNO_MAX ? -ERRNO_MAX : e)
//#endif
//
//#define Q_ERR_(e)       (-ERRNO_MAX - e)



/**
*
*
*
*	Lua GameLib		:	  PushMovers:
*
*
*
**/
/**
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
static int GameLib_GetPushMoverState( lua_State *L ) {
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

	edict_t *pushMoverEntity = &g_edicts[ entityNumber ];
	if ( SVG_IsActiveEntity( pushMoverEntity ) ) {
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
*
*
*
*	Lua GameLib		:	  (Get-)Entities:
*
*
*
**/
/**
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
static int GameLib_GetEntityForLuaName( lua_State *L ) {
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
static int GameLib_GetEntityForTargetName( lua_State *L ) {
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
*	@note	
**/
static int GameLib_GetEntitiesForTargetName( lua_State *L ) {
	// Check if the first argument is string.
	const char *targetName = luaL_checkstring( L, 1 );
	if ( !targetName ) {
		Lua_DeveloperPrintf( "%s: invalid string\n", __func__ );
		// Pushing the result onto the stack to be returned
		lua_Integer luaEntityNumber = -1;
		lua_pushinteger( L, luaEntityNumber );
		return 1;
	}

	// Get the first matching targetname entity.
	//edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName );
	// If not found, return a -1.
	//if ( !targetNameEntity ) {
	//	//Lua_DeveloperPrintf( "%s: no entities found with matching targetname('%s')\n", __func__, targetName );
	//	// Pushing the result onto the stack to be returned
	//	lua_Integer luaEntityNumber = -1;
	//	lua_pushinteger( L, luaEntityNumber );
	//	return 1;
	//}

	// Num of return values.
	int32_t numReturnValues = 1;

	//// Push the found entity onto the stack to be returned first in line.
	//// Store the result inside a type lua_Integer
	//lua_Integer luaEntityNumber = targetNameEntity->s.number;
	//// Pushing the result onto the stack to be returned
	//lua_pushinteger( L, luaEntityNumber );

	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName );
	if ( !targetNameEntity ) {
		// Pushing the result onto the stack to be returned
		lua_Integer luaEntityNumber = -1;
		lua_pushinteger( L, luaEntityNumber );
		return 1;
	}

	// Push table to insert our entity number sequence.
	lua_createtable( L, 0, 0 );
	// Store the result inside a type lua_Integer
	lua_Integer luaEntityNumber = targetNameEntity->s.number;
	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, luaEntityNumber );

	// Iterate over all entities seeking for targetnamed ents.
	//while ( ( targetNameEntity = SVG_Find( targetNameEntity, FOFS( targetname ), targetName ) ) ) {
	while ( 1 ) {
		targetNameEntity = SVG_Find( targetNameEntity, FOFS( targetname ), targetName );
		if ( !targetNameEntity ) {
			break;
		}
		// Increment return value count.
		numReturnValues += 1;
		// Store the result inside a type lua_Integer
		lua_Integer luaEntityNumber = targetNameEntity->s.number;
		// Pushing the result onto the stack to be returned
		lua_pushinteger( L, luaEntityNumber );
	}

	return 1; // The number of returned values
}



/**
*
*
*
*	Lua GameLib		:	  UseTargets:
*
*
*
**/
/**
*	@brief	Utility/Support routine for delaying UseTarget when the 'delay' key/value is set on an entity.
**/
void LUA_Think_UseTargetDelay( edict_t *entity ) {
	edict_t *creatorEntity = entity->delayed.useTarget.creatorEntity;
	if ( !SVG_IsValidLuaEntity( creatorEntity ) ) {
		return;
	}
	// Acquire the delayed UseTarget data.
	const entity_usetarget_type_t useType = entity->delayed.useTarget.useType;
	const int32_t useValue = entity->delayed.useTarget.useValue;

	// Dispatch its use callback.
	if ( creatorEntity->use ) {
		creatorEntity->use(
			creatorEntity, entity->other, entity->activator,
			useType, useValue 
		);
	}
	// We can now free up the entity since we're done with it.
	SVG_FreeEdict( entity );
}
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
static int GameLib_UseTarget( lua_State *L ) {
	// 
	const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const int32_t signallerEntityNumber = luaL_checkinteger( L, 2 );
	const int32_t activatorEntityNumber = luaL_checkinteger( L, 3 );
	const entity_usetarget_type_t useType = static_cast<const entity_usetarget_type_t>( luaL_checkinteger( L, 4 ) );
	const int32_t useValue = static_cast<const int32_t>( luaL_checkinteger( L, 5 ) );

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
	//_UseTargets( entity, activator );

	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		edict_t *delayEntity = SVG_AllocateEdict();
		delayEntity->classname = "DelayedLuaUseTarget";
		delayEntity->nextthink = level.time + sg_time_t::from_sec( entity->delay );
		delayEntity->think = LUA_Think_UseTargetDelay;		
		if ( !activator ) {
			gi.dprintf( "%s: LUA_Think_UseTargetDelay with no activator\n", __func__ );
		}
		delayEntity->activator = activator;
		delayEntity->other = signaller;
		delayEntity->message = entity->message;
		
		delayEntity->targetNames.target = entity->targetNames.target;
		delayEntity->targetNames.kill = entity->targetNames.kill;

		delayEntity->luaProperties.luaName = entity->luaProperties.luaName;
		delayEntity->delayed.useTarget.creatorEntity = entity;
		delayEntity->delayed.useTarget.useType = useType;
		delayEntity->delayed.useTarget.useValue = useValue;

		// Return 0, UseTarget has not actually used its target yet.
		lua_pushinteger( L, 0 );
		return 1;
	}

	// Fire the use method if it has any.
	if ( entity->use ) {
		entity->activator = activator;
		entity->other = signaller;
		entity->use( entity, signaller, activator, useType, useValue );

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
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_BOOLEAN, key, { .boolean = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %s\n", key, value ? "true" : "false" );
			} else if ( lua_isinteger( L, -2 ) ) {
				// Get string value copy.
				lua_Integer value = lua_tointeger( L, -2 );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_INTEGER, key, { .integer = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %" PRIi64 "\n", key, static_cast<int64_t>(value) );
			} else if ( lua_isnumber( L, -2 ) ) {
				// Get string value copy.
				lua_Number value = lua_tonumber( L, -2 );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_NUMBER, key, { .number = value } } );
				// Debug Print:
				gi.dprintf( "	%s => %f\n", key, value );
			} else if ( lua_isstring( L, -2 ) ) {
				// Get string value copy.
				const char *value = lua_tostring( L, -2 );
				// Push it to our signalArguments vector.
				signalArguments.push_back( { SIGNAL_ARGUMENT_TYPE_STRING, key, { .str = value } } );
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
*
*
*
*	Lua GameLib		:	  Signalling:
*
*
*
**/
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
static int GameLib_SignalOut( lua_State *L ) {
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
	//_UseTargets( entity, activator );

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
		memset( delayEntity->delayed.signalOut.name, 0, sizeof( delayEntity->delayed.signalOut.name ));
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



/**
*
*
*
*	Lua GameLib		:	  Init/Shutdown:
*
*
*
**/
/**
*	@brief	Game Namespace Functions:
**/
static const struct luaL_Reg GameLib[] = {

	// (Get-)Entities...:
	{ "GetEntityForLuaName", GameLib_GetEntityForLuaName },
	{ "GetEntityForTargetName", GameLib_GetEntityForTargetName },
	{ "GetEntitiesForTargetName", GameLib_GetEntitiesForTargetName },

	// PushMovers:
	{ "GetPushMoverState", GameLib_GetPushMoverState },

	// UseTargets:
	{ "UseTarget", GameLib_UseTarget },

	// Signalling:
	{ "SignalOut", GameLib_SignalOut },

	// End OF Array.
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

	/**
	*	Register all global constants.
	**/
	// WID: TODO: We don't use these yet, do we even need them?
	// Door Toggle Types:
	LUA_RegisterGlobalConstant( L, "DOOR_TOGGLE_CLOSE", static_cast<const lua_Integer>( 0 ) );
	LUA_RegisterGlobalConstant( L, "DOOR_TOGGLE_OPEN", static_cast<const lua_Integer>( 1 ) );

	// PushMoveInfo States:
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_TOP", static_cast<const lua_Integer>( PUSHMOVE_STATE_TOP ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_BOTTOM", static_cast<const lua_Integer>( PUSHMOVE_STATE_BOTTOM ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_MOVING_UP", static_cast<const lua_Integer>( PUSHMOVE_STATE_MOVING_UP ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_MOVING_DOWN", static_cast<const lua_Integer>( PUSHMOVE_STATE_MOVING_DOWN ) );

	// UseTarget Types:
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_OFF", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_OFF ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_ON", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_ON ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_SET", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_SET ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_TOGGLE", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_TOGGLE ) );
}
