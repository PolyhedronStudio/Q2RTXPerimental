/********************************************************************
*
*
*	ServerGame: Lua Binding API functions for 'UseTargets'.
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
*	UseTarget(s) API:
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
int GameLib_UseTarget( lua_State *L ) {
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
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
int GameLib_UseTargets( lua_State *L ) {
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
	#if 1
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
	#if 1
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

	//
	// print the message
	//
	if ( ( entity->message ) && !( activator->svflags & SVF_MONSTER ) ) {
		gi.centerprintf( activator, "%s", entity->message );
		if ( entity->noise_index ) {
			gi.sound( activator, CHAN_AUTO, entity->noise_index, 1, ATTN_NORM, 0 );
		} else {
			gi.sound( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
		}
	}

	//
	// kill killtargets
	//
	if ( entity->targetNames.kill ) {
		edict_t *killTargetEntity = nullptr;
		while ( ( killTargetEntity = SVG_Find( killTargetEntity, FOFS( targetname ), entity->targetNames.kill ) ) ) {
			SVG_FreeEdict( killTargetEntity );
			if ( !entity->inuse ) {
				gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, entity->classname );
				// Return 0, UseTarget has not actually used its target yet.
				lua_pushinteger( L, -1 );
				return 1;
			}
		}
	}

	//
	// fire targets
	//
	if ( entity->targetNames.target ) {
		edict_t *fireTargetEntity = nullptr;
		while ( ( fireTargetEntity = SVG_Find( fireTargetEntity, FOFS( targetname ), entity->targetNames.target ) ) ) {
			// Doors fire area portals in a specific way
			if ( !Q_stricmp( fireTargetEntity->classname, "func_areaportal" )
				&& ( !Q_stricmp( entity->classname, "func_door" ) || !Q_stricmp( entity->classname, "func_door_rotating" ) ) ) {
				continue;
			}

			if ( fireTargetEntity == entity ) {
				gi.dprintf( "%s: entity(#%d, \"%s\") used itself!\n", __func__, entity->s.number, entity->classname );
			} else {
				if ( fireTargetEntity->use ) {
					fireTargetEntity->use( fireTargetEntity, entity, activator, useType, useValue );
				}

				if ( fireTargetEntity->luaProperties.luaName ) {
					// Generate function 'callback' name.
					const std::string luaFunctionName = std::string( fireTargetEntity->luaProperties.luaName ) + "_Use";
					// Call if it exists.
					if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
						LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, 5, LUA_CALLFUNCTION_VERBOSE_MISSING,
							/*[lua args]:*/ fireTargetEntity, entity, activator, useType, useValue );
					}
				}
			}
			if ( !entity->inuse ) {
				gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, entity->classname );
				// Return 0, UseTarget has not actually used its target yet.
				lua_pushinteger( L, -1 );
				return 1;
			}
		}
	}

	// Return 0, UseTarget has not actually used its target yet.
	lua_pushinteger( L, 1 );
	return 1;
}
