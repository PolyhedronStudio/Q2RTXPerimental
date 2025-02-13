/********************************************************************
*
*
*	ServerGame: Lua Binding API functions.
*	NameSpace: "Game".
*
*
********************************************************************/
#pragma once


//
// UserTypes:
//
#include "svgame/lua/usertypes/svg_lua_usertype_edict_t.hpp"



/**
*
*
* 
*	NameSpace: "Game":
* 
* 
*
**/
/**
*
*	Entities:
*
**/
/**
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
sol::userdata GameLib_GetEntityForLuaName( sol::this_state s, const std::string &luaName );
/**
*	@return	The number of the targetname matching entities found in the entity array, -1 if none found.
*	@note	In Lua, it returns a table containing the entity number(s) with a matching targetname.
**/
sol::table GameLib_GetEntitiesForLuaName( sol::this_state s, const std::string &luaName );
/**
*	@return	The number of the first matching targetname entity in the entities array, -1 if not found.
**/
sol::userdata GameLib_GetEntityForTargetName( sol::this_state s, const std::string &targetName );
/**
*	@return	The number of the team matching entities found in the entity array, -1 if none found.
*	@note	In Lua, it returns a table containing the entity number(s) with a matching targetname.
**/
sol::table GameLib_GetEntitiesForTargetName( sol::this_state s, const std::string &targetName );


/**
*
*	(Push-)Movers:
*
**/
/**
*	@return	The push mover its' current state.
**/
const int32_t GameLib_GetPushMoverState( sol::this_state s, lua_edict_t pushMoverEntity );


/**
*
*	Signal I/O:
*
**/
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_SignalOut( sol::this_state s, lua_edict_t leEnt, lua_edict_t leSignaller, lua_edict_t leActivator, std::string signalName, sol::table signalArguments );


/**
*
*	UseTargets:
*
**/
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTarget( sol::this_state s, lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, const entity_usetarget_type_t useType, const int32_t useValue );
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
const int32_t GameLib_UseTargets( sol::this_state s, lua_edict_t leEnt, lua_edict_t leOther, lua_edict_t leActivator, const entity_usetarget_type_t useType, const int32_t useValue );