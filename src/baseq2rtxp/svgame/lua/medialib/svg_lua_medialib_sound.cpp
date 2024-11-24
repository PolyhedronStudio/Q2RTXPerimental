/********************************************************************
*
*
*	ServerGame: Lua MediaLib API.
*	NameSpace: "Media".
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_medialib.hpp"
#include "svgame/entities/svg_entities_pushermove.h"



/**
*
*
*
*	Sound API:
*
*
*
**/
/**
*	@return	The numeric handle of the precached sound, -1 on failure.
**/
int MediaLib_PrecacheSound( lua_State *L ) {
	// Check if the first argument is numeric(the entity number).
	//const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const std::string soundPath = luaL_checkstring( L, 1 );
	// Error.
	if ( soundPath.empty() ) {
		Lua_DeveloperPrintf( "%s: no sound filename given \n", __func__ );
		lua_pushinteger( L, 0 );
		return 1;
	}

	// Load the sound.
	qhandle_t soundIndex = gi.soundindex( soundPath.c_str() );
	if ( !soundIndex ) {
		Lua_DeveloperPrintf( "%s: Failed to load audio file: (\"%s\")\n", __func__, soundPath.c_str() );
	}

	gi.dprintf( "PRECACHING LUA SOUNDS MOFUCKAHS! \n " );
	// Pushing the result onto the stack to be returned
	lua_pushinteger( L, static_cast<lua_Integer>( soundIndex ) );
	return 1; // The number of returned values
}

/**
*	@return	Binding to gi.sound
**/
int MediaLib_Sound( lua_State *L ) {
	const int32_t entityNumber = luaL_checkinteger( L, 1 );
	const int32_t soundChannel = luaL_checkinteger( L, 2 );
	const qhandle_t soundHandle = luaL_checkinteger( L, 3 );
	const float soundVolume = luaL_checknumber( L, 4 );
	const int32_t soundAttenuation = luaL_checknumber( L, 5 );
	const float soundTimeOffset = luaL_checknumber( L, 6 );

	// Validate entity numbers.
	if ( entityNumber < 0 || entityNumber >= game.maxentities ) {
		lua_pushinteger( L, -1 );
		return 0;
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
	edict_t *entity = &g_edicts[ entityNumber ];
	if ( !SVG_IsActiveEntity( entity ) ) {
		lua_pushinteger( L, -1 );
		return 0;
	}

	// Perform gi.sound.
	gi.sound( entity, soundChannel, soundHandle, soundVolume, soundAttenuation, 0.f );

	// Success.
	lua_pushinteger( L, 0 );
	return 1;
}