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



/**
*
*
*
*   Utilities, common for UseTargets trigger system. (Also used in lua usetargets code.)
*
*
*
**/
/**
*   @brief  Calls the (usually key/value field luaName).."_Use" matching Lua function.
**/
const bool SVG_Trigger_DispatchLuaUseCallback( sol::state_view &stateView, const std::string &luaName, edict_t *entity, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType = entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, const int32_t useValue = 0, const bool verboseIfMissing = true );
/**
*   @brief  Centerprints the trigger message and plays a set sound, or default chat hud sound.
**/
void SVG_Trigger_PrintMessage( edict_t *self, edict_t *activator );
/**
*   @brief  Kills all entities matching the killtarget name.
**/
const int32_t SVG_Trigger_KillTargets( edict_t *self );



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
	// Acquire delayed creator entity, make sure it is still active.
	edict_t *creatorEntity = entity->delayed.useTarget.creatorEntity;
	if ( !SVG_IsActiveEntity( creatorEntity ) ) {
		return;
	}

	// Acquire activator and other.
	edict_t *activator = entity->activator;
	edict_t *other = entity->other;

	// Acquire the delayed UseTarget data.
	const entity_usetarget_type_t useType = entity->delayed.useTarget.useType;
	const int32_t useValue = entity->delayed.useTarget.useValue;

	// Call upon UseTarget, again.
	GameLib_UseTarget( SVG_Lua_GetSolStateView().lua_state(), creatorEntity, other, activator, useType, useValue );

	//
	// print the message
	//
	SVG_Trigger_PrintMessage( entity, activator );

	//
	// kill killtargets
	//
	if ( !SVG_Trigger_KillTargets( entity ) ) {
		return; // -1; // USETARGET_INVALID
	}

	//
	// Fire the use method if it has any.
	//
	int32_t useResult = -1; // Default to USETARGET_INVALID
	if ( entity->use ) {
		// We fired.
		entity->use( entity, other, activator, useType, useValue );
		useResult = 1; // USETARGET_FIRED.
	}

	// Dispatch lua use.
	if ( entity->luaProperties.luaName ) {
		// Dispatch Callback.
		useResult = ( SVG_Trigger_DispatchLuaUseCallback(
			// Sol State.
			SVG_Lua_GetSolStateView(),
			// LuaName of entity/entities, appended with "_Use".
			entity->luaProperties.luaName,
			// Entities.
			entity, other, activator,
			// UseType n Value.
			useType, useValue
		) ? 1 : -1 ); // 1 == USETARGET_FIRED, -1 == USETARGET_INVALID
	}

	// We can now free up the entity since we're done with it.
	SVG_FreeEdict( entity );
}
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTarget( sol::this_state s, lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	// Make sure that the entity is at least active and valid to be signalling.
	if ( !SVG_IsActiveEntity( leEnt.edict ) ) {
		return -1; // USETARGET_INVALID
	}

	// Acquire pointers from lua_edict_t handles.
	edict_t *entity = leEnt.edict;
	edict_t *other = leOther.edict;
	edict_t *activator = leActivator.edict;

	// Spawn a delayed useTargets entity if a delay was requested.
	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		edict_t *delayEntity = SVG_AllocateEdict();
		delayEntity->classname = "DelayedLuaUseTargets";

		delayEntity->nextthink = level.time + sg_time_t::from_sec( entity->delay );
		delayEntity->think = LUA_Think_UseTargetDelay;	

		gi.dprintf( "%s: delayEntity->nextthink=%" PRIx64 "\n", __func__, delayEntity->nextthink.milliseconds() );
		if ( !activator ) {
			gi.dprintf( "%s: LUA_Think_UseTargetDelay with no activator\n", __func__ );
		}
		delayEntity->activator = activator;
		delayEntity->other = other;

		delayEntity->message = entity->message;
		
		delayEntity->targetNames.target = entity->targetNames.target;
		delayEntity->targetNames.kill = entity->targetNames.kill;

		// The luaName of the actual original entity.
		delayEntity->luaProperties.luaName = entity->luaProperties.luaName;

		// The entity which created this temporary delay signal entity.
		delayEntity->delayed.useTarget.creatorEntity = entity;
		// The arguments of said signal.
		delayEntity->delayed.useTarget.useType = (entity_usetarget_type_t)useType;
		// The actual string comes from lua so we need to copy it in instead.
		delayEntity->delayed.useTarget.useValue = useValue;

		return 0; // USETARGET_DELAYED
	}

	//
	// print the message
	//
	SVG_Trigger_PrintMessage( entity, activator );

	//
	// kill killtargets
	//
	if ( !SVG_Trigger_KillTargets( entity ) ) {
		return -1; // USETARGET_INVALID
	}

	//
	// Fire the use method if it has any.
	//
	entity->activator = activator;
	entity->other = other;

	int32_t useResult = 0; // USETARGET_DELAYED

	// Doors fire area portals in a specific way
	if ( !Q_stricmp( entity->classname, "func_areaportal" )
		&& ( !Q_stricmp( entity->classname, "func_door" ) || !Q_stricmp( entity->classname, "func_door_rotating" ) ) ) {
		return 0; // USETARGET_INVALID;
	}

	// Dispatch C use.
	if ( entity->use ) {
		entity->use( entity, other, activator, (entity_usetarget_type_t)useType, useValue );
		useResult = 1; // USETARGET_FIRED
	}

	// Dispatch lua use.
	if ( entity->luaProperties.luaName ) {
		// Dispatch Callback.
		useResult = ( SVG_Trigger_DispatchLuaUseCallback(
			// Sol State.
			SVG_Lua_GetSolStateView(),
			// LuaName of entity/entities, appended with "_Use".
			entity->luaProperties.luaName,
			// Entities.
			entity, other, activator,
			// UseType n Value.
			useType, useValue
		) ? 1 : -1 ); // 1 == USETARGET_FIRED, -1 == USETARGET_INVALID
	}

	if ( !entity->inuse ) {
		gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, entity->classname );
		return -1; // USETARGET_INVALID
	}

	return useResult;
}
/**
*	@brief	Utility/Support routine for delaying UseTarget when the 'delay' key/value is set on an entity.
**/
void LUA_Think_UseTargetsDelay( edict_t *entity ) {
	edict_t *creatorEntity = entity->delayed.useTarget.creatorEntity;
	if ( !SVG_IsActiveEntity( creatorEntity ) ) {
		return;
	}

	edict_t *activator = entity->activator;
	edict_t *other = entity->other;

	// Acquire the delayed UseTarget data.
	const entity_usetarget_type_t useType = entity->delayed.useTarget.useType;
	const int32_t useValue = entity->delayed.useTarget.useValue;

	GameLib_UseTargets( SVG_Lua_GetSolStateView().lua_state(), creatorEntity, other, activator, useType, useValue);
	////
	//// print the message
	////
	//SVG_Trigger_PrintMessage( entity, activator );

	////
	//// kill killtargets
	////
	//if ( !SVG_Trigger_KillTargets( entity ) ) {
	//	return; // -1; // USETARGET_INVALID
	//}

	////
	//// Fire the use method if it has any.
	////
	////
	//// Fire Targets.
	////
	//int32_t useResult = -1; // USETARGET_INVALID

	//if ( entity->targetNames.target ) {
	//	edict_t *fireTargetEntity = nullptr;
	//	while ( ( fireTargetEntity = SVG_Find( fireTargetEntity, FOFS( targetname ), entity->targetNames.target ) ) ) {
	//		// Doors fire area portals in a specific way
	//		if ( !Q_stricmp( fireTargetEntity->classname, "func_areaportal" )
	//			&& ( !Q_stricmp( entity->classname, "func_door" ) || !Q_stricmp( entity->classname, "func_door_rotating" ) ) ) {
	//			continue;
	//		}

	//		if ( fireTargetEntity == entity ) {
	//			gi.dprintf( "%s: entity(#%d, \"%s\") used itself!\n", __func__, entity->s.number, entity->classname );
	//		} else {
	//			// Dispatch C use.
	//			if ( fireTargetEntity->use ) {
	//				fireTargetEntity->use( fireTargetEntity, other, activator, useType, useValue );
	//				useResult = 1; // USETARGET_FIRED
	//			}
	//			// Dispatch lua use.
	//			if ( fireTargetEntity->luaProperties.luaName ) {
	//				// Dispatch Callback.
	//				useResult = ( SVG_Trigger_DispatchLuaUseCallback(
	//					// Sol State.
	//					SVG_Lua_GetSolStateView(),
	//					// LuaName of entity/entities, appended with "_Use".
	//					fireTargetEntity->luaProperties.luaName,
	//					// Entities.
	//					entity, other, activator,
	//					// UseType n Value.
	//					useType, useValue
	//				) ? 1 : -1 ); // 1 == USETARGET_FIRED, -1 == USETARGET_INVALID
	//			}
	//		}
	//		if ( !entity->inuse ) {
	//			gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, entity->classname );
	//			return; // USETARGET_INVALID
	//		}
	//	}
	//}

	// We can now free up the entity since we're done with it.
	SVG_FreeEdict( entity );
}
/**
*	@return < 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTargets( sol::this_state s, lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, const entity_usetarget_type_t useType, int32_t useValue ) {
	// Make sure that the entity is at least active and valid to be signalling.
	if ( !SVG_IsActiveEntity( leEnt.edict ) ) {
		return -1; // USETARGET_INVALID
	}

	// Acquire pointers from lua_edict_t handles.
	edict_t *entity = leEnt.edict;
	edict_t *other = leOther.edict;
	edict_t *activator = leActivator.edict;

	// Spawn a delayed useTargets entity if a delay was requested.
	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		edict_t *delayEntity = SVG_AllocateEdict();
		delayEntity->classname = "DelayedLuaUseTargets";

		delayEntity->nextthink = level.time + sg_time_t::from_sec( entity->delay );
		delayEntity->think = LUA_Think_UseTargetsDelay;

		if ( !activator ) {
			gi.dprintf( "%s: LUA_Think_UseTargetDelay with no activator\n", __func__ );
		}
		delayEntity->activator = activator;
		delayEntity->other = other;

		delayEntity->message = entity->message;

		delayEntity->targetNames.target = entity->targetNames.target;
		delayEntity->targetNames.kill = entity->targetNames.kill;

		// The luaName of the actual original entity.
		delayEntity->luaProperties.luaName = entity->luaProperties.luaName;

		// The entity which created this temporary delay signal entity.
		delayEntity->delayed.useTarget.creatorEntity = entity;
		// The arguments of said signal.
		delayEntity->delayed.useTarget.useType = (entity_usetarget_type_t)useType;
		// The actual string comes from lua so we need to copy it in instead.
		delayEntity->delayed.useTarget.useValue = useValue;

		return 0; // USETARGET_DELAYED
	}

	//
	// print the message
	//
	SVG_Trigger_PrintMessage( entity, activator );

	//
	// kill killtargets
	//
	if ( !SVG_Trigger_KillTargets( entity ) ) {
		return -1; // USETARGET_INVALID
	}

	//
	// Fire Targets.
	//
	int32_t useResult = -1; // USETARGET_INVALID

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
				// Dispatch C use.
				if ( fireTargetEntity->use ) {
					fireTargetEntity->use( fireTargetEntity, entity, activator, (entity_usetarget_type_t)useType, useValue );
					useResult = 1; // USETARGET_FIRED
				}

				// Dispatch lua use.
				if ( fireTargetEntity->luaProperties.luaName ) {
					// Dispatch Callback.
					useResult = ( SVG_Trigger_DispatchLuaUseCallback(
						// Sol State.
						SVG_Lua_GetSolStateView(),
						// LuaName of entity/entities, appended with "_Use".
						fireTargetEntity->luaProperties.luaName,
						// Entities.
						entity, other, activator,
						// UseType n Value.
						useType, useValue
					) ? 1 : -1 ); // 1 == USETARGET_FIRED, -1 == USETARGET_INVALID
				}

				if ( !entity->inuse ) {
					gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, entity->classname );
					return -1; // USETARGET_INVALID
				}
			}
		}
	}

	return useResult;
}