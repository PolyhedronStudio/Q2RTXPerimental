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
#include "svgame/entities/svg_entities_pushermove.h"

// UserTypes:
#include "svgame/lua/usertypes/svg_lua_usertype_edict_t.hpp"
#include "svgame/lua/usertypes/svg_lua_usertype_edict_state_t.hpp"


/**
*
*
*
*	Lua GameLib:
*
*
*
**/
/**
*	@brief	Prints a message of 'gamePrintLevel' to the ALL clients.
**/
static const int32_t GameLib_Print( const int32_t gamePrintLevel, std::string string ) {
	// Print.
	gi.bprintf( gamePrintLevel, "%s", string.c_str() );
	return 1;
}
/**
*	@brief	Prints a message of 'printLevel' to the given client.
**/
static const int32_t GameLib_ClientPrint( lua_edict_t leClientEntity, const int32_t clientPrintLevel, std::string string ) {
	// Make sure the client is active.
	if ( !SVG_IsClientEntity( leClientEntity.edict ) ) {
		return 0;
	}
	// Print.
	gi.cprintf( leClientEntity.edict, clientPrintLevel, "%s", string.c_str() );
	return 1;
}
/**
*	@brief	Prints a centered screen message to the given client.
**/
static const int32_t GameLib_CenterPrint( lua_edict_t leClientEntity, std::string string ) {
	// Make sure the client is active.
	if ( !SVG_IsClientEntity( leClientEntity.edict ) ) {
		return 0;
	}
	// Print.
	gi.centerprintf( leClientEntity.edict, "%s", string.c_str() );
	return 1;
}



/**
*
*
*
*	[Lua GameLib] -> Init/Shutdown:
*
*
*
**/
/**
*	@brief	Initializes the GameLib
**/
void GameLib_Initialize( sol::state_view &solStateView ) {
	// Namespace name.
	constexpr const char *nameSpaceName = "Game";

	// Create namespace.
	sol::table solNameSpace = solStateView[ nameSpaceName ].get_or_create< sol::table >();
	// Entities:
	solNameSpace.set_function( "GetEntityForLuaName", GameLib_GetEntityForLuaName );
	solNameSpace.set_function( "GetEntityForTargetName", GameLib_GetEntityForTargetName );
	solNameSpace.set_function( "GetEntitiesForTargetName", GameLib_GetEntitiesForTargetName );
	// PushMovers:
	solNameSpace.set_function( "GetPushMoverState", GameLib_GetPushMoverState );
	// UseTarget(s):
	solNameSpace.set_function( "UseTarget", GameLib_UseTarget );
	solNameSpace.set_function( "UseTargets", GameLib_UseTargets );
	// Signal I/O:
	solNameSpace.set_function( "SignalOut", GameLib_SignalOut );
	// Printing:
	solNameSpace.set_function( "Print", GameLib_Print );
	solNameSpace.set_function( "ClientPrint", GameLib_ClientPrint );
	solNameSpace.set_function( "CenterPrint", GameLib_CenterPrint );

	// Developer print.
	gi.dprintf( "[Lua]: %s as -> \"%s\"\n", __func__, nameSpaceName );

	/**
	*	Register all global constants, we don't include these in the namespace.
	*	It makes things easier to write and read. We won't be having duplicates anyway.
	**/
	// Door Toggle Types:
	solStateView.new_enum( "PushMoveToggle",
		"CLOSE",	0,	//! Moves it to "Bottom".
		"OPEN",		1	//! Moves it to "Up"
	);
	// PushMoveInfo States:
	solStateView.new_enum( "PushMoveState",
		"TOP",			PUSHMOVE_STATE_TOP,
		"BOTTOM",		PUSHMOVE_STATE_BOTTOM,
		"MOVING_UP",	PUSHMOVE_STATE_MOVING_UP,
		"MOVING_DOWN",	PUSHMOVE_STATE_MOVING_DOWN
	);

	// UseTarget Types:
	solStateView.new_enum( "EntityUseTarget",
		"OFF",		ENTITY_USETARGET_TYPE_OFF,
		"ON",		ENTITY_USETARGET_TYPE_ON,
		"SET",		ENTITY_USETARGET_TYPE_SET,
		"TOGGLE",	ENTITY_USETARGET_TYPE_TOGGLE
	);

	// Print Levels:
	solStateView.new_enum( "PrintLevel",
		//! Prints using default conchars text and thus color.
		"ALL",			PRINT_ALL,		//! Used for General messages..
		"TALK",			PRINT_TALK,		//! Prints in Green color.
		"DEVELOPER",	PRINT_DEVELOPER,//! Only prints when the cvar "developer" >= 1
		"WARNING",		PRINT_WARNING,	//! Print a Warning in Yellow color.
		"ERROR",		PRINT_ERROR,	//! Print an Error in Red color.
		"NOTICE",		PRINT_NOTICE	//! Print a Notice in bright Cyan color.
	);
	// Game Message Print Levels:
	solStateView.new_enum( "ClientPrintLevel",
		"LOW",		PRINT_LOW,		//! Pickup messages.
		"MEDIUM",	PRINT_MEDIUM,	//! Death messages.
		"HIGH",		PRINT_HIGH,		//! Critical messages.
		"CHAT",		PRINT_CHAT		//! Chat messages.
	);
}
