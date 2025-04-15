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
#include "svgame/lua/svg_lua_gamelib.hpp" // for lua_edict_t
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
const qhandle_t MediaLib_PrecacheSound( const std::string &soundPath ) {
	// Error.
	if ( soundPath.empty() ) {
		Lua_DeveloperPrintf( "%s: no sound filename given \n", __func__ );
		sol::error e( std::string(__func__) + ": no sound filename given\n" );
		
		// Not found.
		return 0;
	}

	// Load the sound.
	qhandle_t soundIndex = gi.soundindex( soundPath.c_str() );
	if ( !soundIndex ) {
		Lua_DeveloperPrintf( "%s: Failed to load audio file: (\"%s\")\n", __func__, soundPath.c_str() );

		//sol::error e( std::string( __func__ ) + ": Failed to load audio file: (\"" + __func__ + "\")\n" );
	}

	gi.dprintf( "PRECACHING LUA SOUND %s MOFUCKAHS! \n ", soundPath.c_str() );

	return soundIndex;
}

/**
*	@return	Binding to gi.sound
**/
const int32_t MediaLib_Sound( lua_edict_t leEnt, const int32_t soundChannel, const qhandle_t soundHandle, const float soundVolume, const int32_t soundAttenuation, const float soundTimeOffset ) {
	svg_edict_t *entity = leEnt.handle.edictPtr;
	if ( !SVG_Entity_IsActive( entity ) ) {
		return 0; // INVALID ENTITY
	}
	// Perform gi.sound.
	gi.sound( entity, soundChannel, soundHandle, soundVolume, soundAttenuation, soundTimeOffset );
	// Success.
	return 1;
}