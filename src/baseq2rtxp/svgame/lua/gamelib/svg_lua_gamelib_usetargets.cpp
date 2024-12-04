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

	edict_t *activator = entity->activator;
	edict_t *signaller = entity->other;

	// Acquire the delayed UseTarget data.
	const entity_usetarget_type_t useType = entity->delayed.useTarget.useType;
	const int32_t useValue = entity->delayed.useTarget.useValue;

	//
	// print the message
	//
	if ( ( entity->message ) && !( activator && activator->svflags & SVF_MONSTER ) ) {
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
				// Error.
				return; // USETARGET_INVALID
			}
		}
	}

	//
	// Fire the use method if it has any.
	//
	int useResult = -1; // Default to USETARGET_INVALID
	if ( entity->use ) {
		// We fired.
		entity->use( entity, signaller, activator, useType, useValue );
		useResult = 1; // USETARGET_FIRED.
	}

	// Dispatch lua use.
	if ( entity->luaProperties.luaName ) {
		// Generate function 'callback' name.
		const std::string luaFunctionName = std::string( entity->luaProperties.luaName ) + "_Use";
		// Call if it exists.
		if ( LUA_HasFunction( SVG_Lua_GetSolStateView(), luaFunctionName ) ) {
			LUA_CallFunction( SVG_Lua_GetSolStateView(), luaFunctionName, 1, 5, LUA_CALLFUNCTION_VERBOSE_MISSING,
				/*[lua args]:*/ entity, signaller, activator, useType, useValue );
			useResult = 1; // USETARGET_FIRED.
		}
	}

	// We can now free up the entity since we're done with it.
	SVG_FreeEdict( entity );
}
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTarget( lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, int32_t useType, int32_t useValue ) {
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
		delayEntity->classname = "DelayedLuaUseTarget";

		delayEntity->nextthink = level.time + sg_time_t::from_sec( entity->delay );
		delayEntity->think = LUA_Think_UseTargetDelay;	

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
	if ( ( entity->message ) && !( activator && activator->svflags & SVF_MONSTER ) ) {
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
				// Error.
				return -1; // USETARGET_INVALID
			}
		}
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
		// Generate function 'callback' name.
		const std::string luaFunctionName = std::string( entity->luaProperties.luaName ) + "_Use";
		// Call if it exists.
		if ( LUA_HasFunction( SVG_Lua_GetSolStateView(), luaFunctionName ) ) {
			LUA_CallFunction( SVG_Lua_GetSolStateView(), luaFunctionName, 1, 5, LUA_CALLFUNCTION_VERBOSE_MISSING,
				/*[lua args]:*/ entity, other, activator, useType, useValue );

			useResult = 1; // USETARGET_FIRED
		}
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
	if ( !SVG_IsValidLuaEntity( creatorEntity ) ) {
		return;
	}

	edict_t *activator = entity->activator;
	edict_t *signaller = entity->other;

	// Acquire the delayed UseTarget data.
	const entity_usetarget_type_t useType = entity->delayed.useTarget.useType;
	const int32_t useValue = entity->delayed.useTarget.useValue;

	//
	// print the message
	//
	if ( ( entity->message ) && !( activator && activator->svflags & SVF_MONSTER ) ) {
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
				// Error.
				return; // USETARGET_INVALID
			}
		}
	}

	//
	// Fire the use method if it has any.
	//
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
					fireTargetEntity->use( fireTargetEntity, entity, activator, useType, useValue );
					useResult = 1; // USETARGET_FIRED
				}

				// Dispatch lua use.
				if ( fireTargetEntity->luaProperties.luaName ) {
					// Generate function 'callback' name.
					const std::string luaFunctionName = std::string( fireTargetEntity->luaProperties.luaName ) + "_Use";
					// Call if it exists.
					if ( LUA_HasFunction( SVG_Lua_GetSolStateView(), luaFunctionName ) ) {
						LUA_CallFunction( SVG_Lua_GetSolStateView(), luaFunctionName, 1, 5, LUA_CALLFUNCTION_VERBOSE_MISSING,
							/*[lua args]:*/ fireTargetEntity, entity, activator, useType, useValue );

						useResult = 1; // USETARGET_FIRED
					}
				}
			}
			if ( !entity->inuse ) {
				gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, entity->classname );
				return; // USETARGET_INVALID
			}
		}
	}

	// We can now free up the entity since we're done with it.
	SVG_FreeEdict( entity );
}
/**
*	@return < 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTargets( lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, int32_t useType, int32_t useValue ) {
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
		delayEntity->classname = "DelayedLuaUseTarget";

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
	if ( ( entity->message ) && !( activator && activator->svflags & SVF_MONSTER ) ) {
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
				return -1; // USETARGET_INVALID
			}
		}
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
					// Generate function 'callback' name.
					const std::string luaFunctionName = std::string( fireTargetEntity->luaProperties.luaName ) + "_Use";
					// Call if it exists.
					if ( LUA_HasFunction( SVG_Lua_GetSolStateView(), luaFunctionName ) ) {
						LUA_CallFunction( SVG_Lua_GetSolStateView(), luaFunctionName, 1, 5, LUA_CALLFUNCTION_VERBOSE_MISSING,
							/*[lua args]:*/ fireTargetEntity, entity, activator, useType, useValue );

						useResult = 1; // USETARGET_FIRED
					}
				}
			}
			if ( !entity->inuse ) {
				gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, entity->s.number, entity->classname );
				return -1; // USETARGET_INVALID
			}
		}
	}

	return useResult;
}
