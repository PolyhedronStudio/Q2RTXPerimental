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



/**
*
*
*
*	[Lua MediaLib] -> Init/Shutdown:
*
*
*
**/
/**
*	@brief	Initializes the MediaLib
**/
void MediaLib_Initialize( sol::state_view &solStateView ) {
	// Namespace name.
	constexpr const char *nameSpaceName = "Media";

	// Create namespace.
	auto solNameSpace = solStateView[ nameSpaceName ].get_or_create< sol::table >();
	solNameSpace.set_function( "PrecacheSound", MediaLib_PrecacheSound );
	solNameSpace.set_function( "Sound", MediaLib_Sound );

	// Developer print.
	gi.dprintf( "[Lua]: %s as -> \"%s\"\n", __func__, nameSpaceName );

	/**
	*	Register all global constants, we don't include these in the namespace.
	*	It makes things easier to write and read. We won't be having duplicates anyway.
	**/
	// Sound Attenuation:
	solStateView.new_enum( "SoundAttenuation",
		"NONE",		ATTN_NONE,
		//"DISTANT",		ATTN_DISTANT,
		"NORMAL",	ATTN_NORM,
		"IDLE",		ATTN_IDLE,
		"STATIC",	ATTN_STATIC
	);
	// Sound Channels:
	solStateView.new_enum( "SoundChannel",
		"AUTO",		CHAN_AUTO,
		"WEAPON",	CHAN_WEAPON,
		"VOICE",	CHAN_VOICE,
		"ITEM",		CHAN_ITEM,
		"BODY",		CHAN_BODY,
		//
		"NO_PHS_ADD",	CHAN_NO_PHS_ADD,
		"RELIABLE",		CHAN_RELIABLE
	);
}
