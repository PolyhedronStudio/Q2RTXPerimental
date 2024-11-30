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
	solNameSpace[ "PrecacheSound" ] = MediaLib_PrecacheSound;
	solNameSpace[ "Sound" ] = MediaLib_Sound;

	// Developer print.
	gi.dprintf( "[Lua]: %s as -> \"%s\"\n", __func__, nameSpaceName );

	/**
	*	Register all global constants.
	**/
	// Sound Attenuation
	solStateView.set( "ATTN_NONE", ATTN_NONE );
	//solStateView.set( "ATTN_DISTANT", ATTN_DISTANT );
	solStateView.set( "ATTN_NORM", ATTN_NORM );
	solStateView.set( "ATTN_IDLE", ATTN_IDLE );
	solStateView.set( "ATTN_STATIC", ATTN_STATIC );

	// Sound Channels:
	solStateView.set( "CHAN_AUTO", CHAN_AUTO );
	solStateView.set( "CHAN_WEAPON", CHAN_WEAPON );
	solStateView.set( "CHAN_VOICE", CHAN_VOICE );
	solStateView.set( "CHAN_ITEM", CHAN_ITEM );
	solStateView.set( "CHAN_BODY", CHAN_BODY );
	solStateView.set( "CHAN_NO_PHS_ADD", CHAN_NO_PHS_ADD );
	solStateView.set( "CHAN_RELIABLE", CHAN_RELIABLE );
}
