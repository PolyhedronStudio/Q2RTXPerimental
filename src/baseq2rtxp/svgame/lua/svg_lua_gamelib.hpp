/********************************************************************
*
*
*	ServerGame: Lua Binding API functions.
*	NameSpace: "Game".
*
*
********************************************************************/
#pragma once



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
int GameLib_GetEntityForLuaName( lua_State *L );
/**
*	@return	The number of the first matching targetname entity in the entities array, -1 if not found.
**/
int GameLib_GetEntityForTargetName( lua_State *L );
/**
*	@return	The number of the team matching entities found in the entity array, -1 if none found.
*	@note	In Lua, it returns a table containing the entity number(s) with a matching targetname.
**/
int GameLib_GetEntitiesForTargetName( lua_State *L );


/**
*
*	(Push-)Movers:
*
**/
/**
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
int GameLib_GetPushMoverState( lua_State *L );


/**
*
*	Signal I/O:
*
**/
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
int GameLib_SignalOut( lua_State *L );


/**
*
*	UseTargets:
*
**/
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
int GameLib_UseTarget( lua_State *L );
/**
*	@return	< 0 if failed, 0 if delayed or not fired at all, 1 if fired.
**/
int GameLib_UseTargets( lua_State * L );