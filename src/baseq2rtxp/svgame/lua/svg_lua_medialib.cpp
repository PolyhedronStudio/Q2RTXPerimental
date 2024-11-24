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
*	@brief	"Media" Namespace Functions:
**/
static const struct luaL_Reg MediaLib[] = {
	// svg_lua_medialib_sound.cpp:
	{ "PrecacheSound", MediaLib_PrecacheSound },
	{ "Sound", MediaLib_Sound },

	// End OF Array.
	{ NULL, NULL }
};
/**
*	@brief	Initializes the MediaLib
**/
void MediaLib_Initialize( lua_State *L ) {
	// We create a new table
	lua_newtable( L );

	// Here we set all functions from GameLib array into
	// the table on the top of the stack
	luaL_setfuncs( L, MediaLib, 0 );

	// We get the table and set as global variable
	lua_setglobal( L, "Media" );

	if ( luaL_dostring( L, "Core.DPrint(\"Initialized Lua MediaLib\n\"" ) == LUA_OK ) {
		lua_pop( L, lua_gettop( L ) );
	}

	/**
	*	Register all global constants.
	**/
	// Sound Attenuation
	LUA_RegisterGlobalConstant( L, "ATTN_NONE", static_cast<const lua_Integer>( ATTN_NONE ) );
	//LUA_RegisterGlobalConstant( L, "ATTN_DISTANT", static_cast<const lua_Integer>( ATTN_DISTANT ) );
	LUA_RegisterGlobalConstant( L, "ATTN_NORM", static_cast<const lua_Integer>( ATTN_NORM ) );
	LUA_RegisterGlobalConstant( L, "ATTN_IDLE", static_cast<const lua_Integer>( ATTN_IDLE ) );
	LUA_RegisterGlobalConstant( L, "ATTN_STATIC", static_cast<const lua_Integer>( ATTN_STATIC ) );

	// Sound Channels:
	LUA_RegisterGlobalConstant( L, "CHAN_AUTO", static_cast<const lua_Integer>( CHAN_AUTO ) );
	LUA_RegisterGlobalConstant( L, "CHAN_WEAPON", static_cast<const lua_Integer>( CHAN_WEAPON ) );
	LUA_RegisterGlobalConstant( L, "CHAN_VOICE", static_cast<const lua_Integer>( CHAN_VOICE ) );
	LUA_RegisterGlobalConstant( L, "CHAN_ITEM", static_cast<const lua_Integer>( CHAN_ITEM ) );
	LUA_RegisterGlobalConstant( L, "CHAN_BODY", static_cast<const lua_Integer>( CHAN_BODY ) );
	// Sound Channel Modifier flags.
	LUA_RegisterGlobalConstant( L, "CHAN_NO_PHS_ADD", static_cast<const lua_Integer>( CHAN_NO_PHS_ADD ) );
	LUA_RegisterGlobalConstant( L, "CHAN_RELIABLE", static_cast<const lua_Integer>( CHAN_RELIABLE ) );
}
