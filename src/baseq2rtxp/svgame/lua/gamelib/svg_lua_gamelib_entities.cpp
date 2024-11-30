/********************************************************************
*
*
*	ServerGame: Lua GameLib API.
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
*	Entities API:
*
*
*
**/
/**
*	@return	The number of the entity if it has a matching luaName, -1 otherwise.
**/
lua_edict_t GameLib_GetEntityForLuaName( const std::string &luaName ) {
	// Get the first matching entity for the luaName.
	edict_t *luaNameEntity = SVG_Find( NULL, FOFS( luaProperties.luaName ), luaName.c_str() );
	// Return it.
	return luaNameEntity;
}

/**
*	@return	The number of the first matching targetname entity in the entities array, -1 if not found.
**/
lua_edict_t GameLib_GetEntityForTargetName( const std::string &targetName ) {
	// Get the first matching entity for the targetname.
	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName.c_str() );
	// Return it.
	return targetNameEntity;
}
/**
*	@return	The number of the team matching entities found in the entity array, -1 if none found.
*	@note	In Lua, it returns a table containing the entity number(s) with a matching targetname.
**/
sol::table GameLib_GetEntitiesForTargetName( const std::string &targetName ) {
	// Table to push lua_edict_t types onto.
	sol::table targetEntities( SVG_Lua_GetSolStateView(), sol::create );

	// Find our first targetnamed entity, if any at all.
	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName.c_str() );
	if ( !targetNameEntity ) {
		return targetEntities;
	}

	// Iterate over all entities seeking for targetnamed ents.
	while ( 1 ) {
		// Find next entity in line.
		targetNameEntity = SVG_Find( targetNameEntity, FOFS( targetname ), targetName.c_str() );
		// Exit if it's nullptr.
		if ( !targetNameEntity ) {
			break;
		}
		// Set the table. Entities won't be having same number so no need to check I guess..
		targetEntities.set( std::to_string( targetNameEntity->s.number ), lua_edict_t( targetNameEntity ) );
	}
	return targetEntities;
}