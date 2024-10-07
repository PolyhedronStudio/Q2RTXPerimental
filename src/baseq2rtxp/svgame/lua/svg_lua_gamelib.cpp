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
*	Lua GameLib:
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

	// multiply and store the result inside a type lua_Integer
	lua_Integer luaPusherMoveState = pusherMoveState;

	// Here we prepare the values to be returned.
	// First we push the values we want to return onto the stack in direct order.
	// Second, we must return the number of values pushed onto the stack.

	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, luaPusherMoveState );

	return 1; // The number of returned values
}

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
	if ( !creatorEntity ) {
		return;
	}
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
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
static int GameLib_UseTarget( lua_State *L ) {
	// 
	const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const int32_t otherEntityNumber = luaL_checkinteger( L, 2 );
	const int32_t activatorEntityNumber = luaL_checkinteger( L, 3 );
	const entity_usetarget_type_t useType = static_cast<const entity_usetarget_type_t>( luaL_checkinteger( L, 4 ) );
	const int32_t useValue = static_cast<const int32_t>( luaL_checkinteger( L, 5 ) );

	// Validate entity numbers.
	if ( entityNumber < 0 || entityNumber >= game.maxentities ) {
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
	edict_t *other = ( otherEntityNumber != -1 ? &g_edicts[ otherEntityNumber ] : nullptr );
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
*	@brief	Utility/Support routine for delaying SignalOut when a 'delay' is given to it.
**/
void LUA_Think_SignalOutDelay( edict_t *entity ) {
	edict_t *creatorEntity = entity->luaProperties.delayedSignalCreatorEntity;
	if ( !creatorEntity ) {
		return;
	}
	const char *signalName = entity->luaProperties.delayedSignalName;
	if ( creatorEntity->onsignalin ) {
		creatorEntity->onsignalin(
			creatorEntity,
			entity->other,
			entity->activator,
			signalName );
	}
	SVG_FreeEdict( entity );
}
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
static int GameLib_SignalOut( lua_State *L ) {
	// 
	const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const int32_t otherEntityNumber = luaL_checkinteger( L, 2 );
	const int32_t activatorEntityNumber = luaL_checkinteger( L, 3 );
	const char *signalName = static_cast<const char *>( luaL_checkstring( L, 4 ) );

	// Validate entity numbers.
	if ( entityNumber < 0 || entityNumber >= game.maxentities ) {
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
	edict_t *other = ( otherEntityNumber != -1 ? &g_edicts[ otherEntityNumber ] : nullptr );
	edict_t *activator = ( activatorEntityNumber != -1 ? &g_edicts[ activatorEntityNumber ] : nullptr );
	//_UseTargets( entity, activator );

	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		edict_t *t = SVG_AllocateEdict();
		t->classname = "DelayedUse";
		t->nextthink = level.time + sg_time_t::from_sec( entity->delay );
		t->think = LUA_Think_SignalOutDelay;

		t->activator = activator;
		t->other = other;
		if ( !activator ) {
			gi.dprintf( "Think_Delay with no activator\n" );
		}

		t->luaProperties.luaName = entity->luaProperties.luaName;
		t->luaProperties.delayedSignalCreatorEntity = entity;
		//t->luaProperties.delayedUseType = useType;
		//t->luaProperties.delayedUseValue = useValue;
		// The actual string comes from lua so we need to copy it in instead.
		memset( t->luaProperties.delayedSignalName, 0, sizeof( t->luaProperties.delayedSignalName ) );
		Q_strlcpy( t->luaProperties.delayedSignalName, signalName, strlen( signalName ) );

		t->message = entity->message;
		t->targetNames.target = entity->targetNames.target;
		t->targetNames.kill = entity->targetNames.kill;

		// Return 0, UseTarget has not actually used its target yet.
		lua_pushinteger( L, 0 );
		return 1;
	}

	// Fire the signal if it has a OnSignal method registered.
	if ( entity->onsignalin ) {
		entity->activator = activator;
		entity->other = other;
		//entity->use( entity, other, activator, useType, useValue );
		entity->onsignalin( entity, other, activator, signalName );

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
static const struct luaL_Reg GameLib[] = {

	// GetEntity...:
	{ "GetEntityForLuaName", GameLib_GetEntityForLuaName },
	{ "GetEntityForTargetName", GameLib_GetEntityForTargetName },

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
	LUA_RegisterGlobalConstant( L, "DOOR_TOGGLE_CLOSE<const>", static_cast<const lua_Integer>( 0 ) );
	LUA_RegisterGlobalConstant( L, "DOOR_TOGGLE_OPEN<const>", static_cast<const lua_Integer>( 1 ) );

	// PushMoveInfo States:
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_TOP<const>", static_cast<const lua_Integer>( PUSHMOVE_STATE_TOP ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_BOTTOM<const>", static_cast<const lua_Integer>( PUSHMOVE_STATE_BOTTOM ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_MOVING_UP<const>", static_cast<const lua_Integer>( PUSHMOVE_STATE_MOVING_UP ) );
	LUA_RegisterGlobalConstant( L, "PUSHMOVE_STATE_MOVING_DOWN<const>", static_cast<const lua_Integer>( PUSHMOVE_STATE_MOVING_DOWN ) );

	// UseTarget Types:
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_OFF<const>", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_OFF ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_ON<const>", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_ON ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_SET<const>", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_SET ) );
	LUA_RegisterGlobalConstant( L, "ENTITY_USETARGET_TYPE_TOGGLE<const>", static_cast<const lua_Integer>( ENTITY_USETARGET_TYPE_TOGGLE ) );
}
