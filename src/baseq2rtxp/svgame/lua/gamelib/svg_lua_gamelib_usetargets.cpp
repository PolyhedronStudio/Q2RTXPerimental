/********************************************************************
*
*
*	ServerGame: Lua Binding API functions for 'UseTargets'.
*	NameSpace: "Game".
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"

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
const bool SVG_Trigger_DispatchLuaUseCallback( sol::state_view &stateView, const std::string &luaName, bool &functionReturnValue, svg_base_edict_t *entity, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType = entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, const int32_t useValue = 0, const bool verboseIfMissing = true );
/**
*   @brief  Centerprints the trigger message and plays a set sound, or default chat hud sound.
**/
void SVG_Trigger_PrintMessage( svg_base_edict_t *self, svg_base_edict_t *activator );
/**
*   @brief  Kills all entities matching the killtarget name.
**/
const int32_t SVG_Trigger_KillTargets( svg_base_edict_t *self );



/**
*
*
*
*	UseTarget(s) API:
*
*
*
**/
DECLARE_GLOBAL_CALLBACK_THINK( LUA_Think_UseTargetDelay );
/**
*	@brief	Utility/Support routine for delaying UseTarget when the 'delay' key/value is set on an entity.
**/
DEFINE_GLOBAL_CALLBACK_THINK( LUA_Think_UseTargetDelay)( svg_base_edict_t *entity ) -> void {
	// Ensure it is still active.
	if ( !SVG_Entity_IsActive( entity ) ) {
		return;
	}

	// Acquire activator and other.
	svg_base_edict_t *activator = entity->activator;
	svg_base_edict_t *other = entity->other;

	// Acquire the delayed UseTarget data.
	const entity_usetarget_type_t useType = entity->delayed.useTarget.useType;
	const int32_t useValue = entity->delayed.useTarget.useValue;

	// Call UseTargets using delay entity data.
	GameLib_UseTarget( SVG_Lua_GetSolStateView().lua_state(), entity, other, activator, useType, useValue );

	// We can now free up the entity since we're done with it.
	SVG_FreeEdict( entity );

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
	//int32_t useResult = -1; // Default to USETARGET_INVALID
	//if ( entity->use ) {
	//	// We fired.
	//	entity->use( entity, other, activator, useType, useValue );
	//	useResult = 1; // USETARGET_FIRED.
	//}

	//// Dispatch lua use.
	//if ( entity->luaProperties.luaName ) {
	//	// Dispatch Callback.
	//	useResult = ( SVG_Trigger_DispatchLuaUseCallback(
	//		// Sol State.
	//		SVG_Lua_GetSolStateView(),
	//		// LuaName of entity/entities, appended with "_Use".
	//		entity->luaProperties.luaName,
	//		// Entities.
	//		entity, other, activator,
	//		// UseType n Value.
	//		useType, useValue
	//	) ? 1 : -1 ); // 1 == USETARGET_FIRED, -1 == USETARGET_INVALID
	//}

}
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTarget( sol::this_state s, lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	// Make sure that the entity is at least active and valid to be signalling.
	if ( !SVG_Entity_IsActive( leEnt.handle.edictPtr ) ) {
		return -1; // USETARGET_INVALID
	}

	// Acquire pointers from lua_edict_t handles.
	svg_base_edict_t *entity = leEnt.handle.edictPtr;
	svg_base_edict_t *other = leOther.handle.edictPtr;
	svg_base_edict_t *activator = leActivator.handle.edictPtr;

	// Spawn a delayed useTargets entity if a delay was requested.
	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		svg_base_edict_t *delayEntity = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
		// In case it failed to allocate of course.
		if ( !SVG_Entity_IsActive( delayEntity ) ) {
			return -1; // USETARGET_INVALID
		}
		delayEntity->classname = "DelayedLuaUseTarget";

		delayEntity->nextthink = level.time + QMTime::FromMilliseconds( entity->delay );
		delayEntity->SetThinkCallback( LUA_Think_UseTargetDelay );

		if ( !activator ) {
			gi.dprintf( "%s: LUA_Think_UseTargetDelay with no activator\n", __func__ );
		}
		delayEntity->activator = activator;
		delayEntity->other = other;

		delayEntity->message = entity->message;

		delayEntity->targetEntities.target = entity->targetEntities.target;
		delayEntity->targetNames.target = entity->targetNames.target;
		delayEntity->targetNames.kill = entity->targetNames.kill;

		// The luaName of the actual original entity.
		delayEntity->luaProperties.luaName = entity->luaProperties.luaName;

		// The entity which created this temporary delay signal entity.
		delayEntity->delayed.useTarget.creatorEntity = entity;
		// The arguments of said signal.
		delayEntity->delayed.useTarget.useType = useType;
		// The actual string comes from lua so we need to copy it in instead.
		delayEntity->delayed.useTarget.useValue = useValue;

		return 0; // USETARGET_DELAYED
	}

	// If a creator entity has been set in the delay data, use it instead.
	// This way we effectively bypass recursive delay if statement looping.
	if ( entity->delayed.useTarget.creatorEntity ) {
		// Set entity to creator entity.
		entity = entity->delayed.useTarget.creatorEntity;
		// Make sure that the entity is at least active.
		if ( !SVG_Entity_IsActive( entity ) ) {
			return -1; // USETARGET_INVALID
		}
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
	if ( !Q_stricmp( (const char *)entity->classname, "func_areaportal" )
		&& ( !Q_stricmp( (const char *)entity->classname, "func_door" ) || !Q_stricmp( (const char *)entity->classname, "func_door_rotating" ) ) ) {
		return 0; // USETARGET_INVALID;
	}

	// Dispatch C use.
	if ( entity->HasUseCallback() ) {
		entity->DispatchUseCallback( other, activator, (entity_usetarget_type_t)useType, useValue );
		useResult = 1; // USETARGET_FIRED
	}

	// Dispatch lua use.
	if ( entity->luaProperties.luaName ) {
		// Get view for state.
		sol::state_view solStateView = sol::state_view( s );
		// Return value.
		bool returnValue = false;
		// Dispatch Callback.
		useResult = ( SVG_Trigger_DispatchLuaUseCallback(
			// Sol State.
			solStateView,
			// LuaName of entity/entities, appended with "_Use".
			entity->luaProperties.luaName.ptr,
			// Return value.
			returnValue,
			// Entities.
			entity, other, activator,
			// UseType n Value.
			useType, useValue
		) ? 1/*USETARGET_FIRED*/ : -1/*USETARGET_INVALID*/ );
	}

	if ( !entity->inuse ) {
		gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, (const char *)entity->classname );
		return -1; // USETARGET_INVALID
	}

	return useResult;
}
/**
*	@brief	Utility/Support routine for delaying UseTarget when the 'delay' key/value is set on an entity.
**/
DECLARE_GLOBAL_CALLBACK_THINK( LUA_Think_UseTargetsDelay );
DEFINE_GLOBAL_CALLBACK_THINK( LUA_Think_UseTargetsDelay)( svg_base_edict_t *entity ) -> void {
	// Ensure it is still active.
	if ( !SVG_Entity_IsActive( entity ) ) {
		return;
	}

	svg_base_edict_t *activator = entity->activator;
	svg_base_edict_t *other = entity->other;

	// Acquire the delayed UseTarget data.
	const entity_usetarget_type_t useType = entity->delayed.useTarget.useType;
	const int32_t useValue = entity->delayed.useTarget.useValue;

	// Call UseTargets using delay entity data.
	GameLib_UseTargets( SVG_Lua_GetSolStateView().lua_state(), entity, other, activator, useType, useValue);
	
	// We can now free up the entity since we're done with it.
	SVG_FreeEdict( entity );
}
/**
*	@return < 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTargets( sol::this_state s, lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, const entity_usetarget_type_t useType, int32_t useValue ) {
	// Make sure that the entity is at least active.
	if ( !SVG_Entity_IsActive( leEnt.handle.edictPtr ) ) {
		return -1; // USETARGET_INVALID
	}

	// Acquire pointers from lua_edict_t handles.
	svg_base_edict_t *entity = leEnt.handle.edictPtr;
	svg_base_edict_t *other = leOther.handle.edictPtr;
	svg_base_edict_t *activator = leActivator.handle.edictPtr;

	// Spawn a delayed useTargets entity if a delay was requested.
	if ( entity->delay ) {
		// create a temp object to UseTarget at a later time.
		svg_base_edict_t *delayEntity = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
		// In case it failed to allocate of course.
		if ( !SVG_Entity_IsActive( delayEntity ) ) {
			return -1; // USETARGET_INVALID
		}
		delayEntity->classname = "DelayedLuaUseTargets";

		delayEntity->nextthink = level.time + QMTime::FromMilliseconds( entity->delay );
		delayEntity->SetThinkCallback( LUA_Think_UseTargetsDelay );

		if ( !activator ) {
			gi.dprintf( "%s: LUA_Think_UseTargetsDelay with no activator\n", __func__ );
		}
		delayEntity->activator = activator;
		delayEntity->other = other;

		delayEntity->message = entity->message;

		delayEntity->targetEntities.target = entity->targetEntities.target;
		delayEntity->targetNames.target = entity->targetNames.target;
		delayEntity->targetNames.kill = entity->targetNames.kill;

		// The luaName of the actual original entity.
		delayEntity->luaProperties.luaName = entity->luaProperties.luaName;

		// The entity which created this temporary delay signal entity.
		delayEntity->delayed.useTarget.creatorEntity = entity;
		// The arguments of said signal.
		delayEntity->delayed.useTarget.useType = useType;
		// The actual string comes from lua so we need to copy it in instead.
		delayEntity->delayed.useTarget.useValue = useValue;

		return 0; // USETARGET_DELAYED
	}

	// If a creator entity has been set in the delay data, use it instead.
	// This way we effectively bypass recursive delay if statement looping.
	if ( entity->delayed.useTarget.creatorEntity ) {
		// Set entity to creator entity.
		entity = entity->delayed.useTarget.creatorEntity;
		// Make sure that the entity is at least active.
		if ( !SVG_Entity_IsActive( entity ) ) {
			return -1; // USETARGET_INVALID
		}
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
		svg_base_edict_t *fireTargetEntity = nullptr;
		while ( ( fireTargetEntity = SVG_Entities_Find( fireTargetEntity, q_offsetof( svg_base_edict_t, targetname ), (const char *)entity->targetNames.target ) ) ) {
			// Doors fire area portals in a specific way
			if ( !Q_stricmp( (const char *)fireTargetEntity->classname, "func_areaportal" )
				&& ( !Q_stricmp( (const char *)entity->classname, "func_door" ) || !Q_stricmp( (const char *)entity->classname, "func_door_rotating" ) ) ) {
				continue;
			}

			if ( fireTargetEntity == entity ) {
				gi.dprintf( "%s: entity(#%d, \"%s\") used itself!\n", __func__, entity->s.number, (const char *)entity->classname );
			} else {
				// Dispatch C use.
				if ( fireTargetEntity->HasUseCallback() ) {
					fireTargetEntity->DispatchUseCallback( entity, activator, useType, useValue );
					useResult = 1; // USETARGET_FIRED
				}

				// Dispatch lua use.
				if ( fireTargetEntity->luaProperties.luaName ) {
					// Get view for state.
					sol::state_view solStateView = sol::state_view( s );
					// Return value.
					bool returnValue = false;
					// Dispatch Callback.
					useResult = ( SVG_Trigger_DispatchLuaUseCallback(
						// Sol State.
						solStateView,
						// LuaName of entity/entities, appended with "_Use".
						fireTargetEntity->luaProperties.luaName.ptr,
						// Return value.
						returnValue,
						// Entities.
						fireTargetEntity, other, activator,
						// UseType n Value.
						useType, useValue
					) ? 1/*USETARGET_FIRED*/ : -1/*USETARGET_INVALID*/ );
				}

				if ( !entity->inuse ) {
					gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, (const char *)entity->classname );
					return -1; // USETARGET_INVALID
				}
			}
		}
	}

	return useResult;
}