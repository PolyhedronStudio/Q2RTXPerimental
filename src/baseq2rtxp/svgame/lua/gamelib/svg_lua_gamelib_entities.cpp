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
sol::userdata GameLib_GetEntityForLuaName( sol::this_state s, const std::string &luaName ) {
	// Get the first matching entity for the luaName.
	edict_t *luaNameEntity = SVG_Find( NULL, FOFS( luaProperties.luaName ), luaName.c_str() );
	// Return it.
	return sol::make_object<lua_edict_t>( s, lua_edict_t( luaNameEntity ) );
}

/**
*	@return	The number of the first matching targetname entity in the entities array, -1 if not found.
**/
sol::userdata GameLib_GetEntityForTargetName( sol::this_state s, const std::string &targetName ) {
	// Get the first matching entity for the targetname.
	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName.c_str() );
	// Return it.
	return sol::make_object<lua_edict_t>( s, lua_edict_t( targetNameEntity ) );
}
/**
*	@return	The number of the team matching entities found in the entity array, -1 if none found.
*	@note	In Lua, it returns a table containing the entity number(s) with a matching targetname.
**/
sol::table GameLib_GetEntitiesForTargetName( sol::this_state s, const std::string &targetName ) {
	// Table to push lua_edict_t types onto.
	sol::table targetEntities = sol::state_view(s).create_table();

	// Find our first targetnamed entity, if any at all.
	edict_t *targetNameEntity = SVG_Find( NULL, FOFS( targetname ), targetName.c_str() );
	if ( !targetNameEntity ) {
		return targetEntities;
	}

	// Add to the table.
	targetEntities.add( sol::make_object<lua_edict_t>( s, lua_edict_t( targetNameEntity ) ) );

	// Iterate over all entities seeking for targetnamed ents.
	while ( 1 ) {
		// Find next entity in line.
		targetNameEntity = SVG_Find( targetNameEntity, FOFS( targetname ), targetName.c_str() );
		// Exit if it's nullptr.
		if ( !targetNameEntity ) {
			break;
		}

		// Add to the table.
		targetEntities.add( sol::make_object<lua_edict_t>( s, lua_edict_t( targetNameEntity ) ) );
	}

	return targetEntities;
}