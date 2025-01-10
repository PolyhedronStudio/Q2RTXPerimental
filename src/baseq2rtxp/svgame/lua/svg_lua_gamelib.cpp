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
	//
	// EntityEffects:
	//
	solStateView.new_enum( "EntityEffects",
		"NONE", EF_NONE,
		"SPOTLIGHT", EF_SPOTLIGHT,
		"GIB", EF_GIB,
		"ANIM_CYCLE2_2HZ", EF_ANIM_CYCLE2_2HZ,
		"ANIM01", EF_ANIM01,
		"ANIM23", EF_ANIM23,
		"ANIM_ALL", EF_ANIM_ALL,
		"ANIM_ALLFAST", EF_ANIM_ALLFAST,
		"COLOR_SHELL", EF_COLOR_SHELL,
		"QUAD", EF_QUAD,
		"PENT", EF_PENT,
		"DOUBLE", EF_DOUBLE,
		"HALF_DAMAGE", EF_HALF_DAMAGE,
		"TELEPORTER", EF_TELEPORTER
	);
	//
	// RenderFx:
	//
	solStateView.new_enum( "RenderFx",
		"NONE", RF_NONE,
		"MINLIGHT", RF_MINLIGHT,
		"VIEWERMODEL", RF_VIEWERMODEL,
		"WEAPONMODEL", RF_WEAPONMODEL,
		"FULLBRIGHT", RF_FULLBRIGHT,
		"DEPTHHACK", RF_DEPTHHACK,
		"TRANSLUCENT", RF_TRANSLUCENT,
		"FRAMELERP", RF_FRAMELERP,
		"BEAM", RF_BEAM,
		"CUSTOMSKIN", RF_CUSTOMSKIN,
		"GLOW", RF_GLOW,
		"SHELL_RED", RF_SHELL_RED,
		"SHELL_GREEN", RF_SHELL_GREEN,
		"SHELL_BLUE", RF_SHELL_BLUE,
		"NOSHADOW", RF_NOSHADOW,
		"OLD_FRAME_LERP", RF_OLD_FRAME_LERP,
		"STAIR_STEP", RF_STAIR_STEP,

		"IR_VISIBLE", RF_IR_VISIBLE,
		"SHELL_DOUBLE", RF_SHELL_DOUBLE,
		"SHELL_HALF_DAM", RF_SHELL_HALF_DAM,
		"USE_DISGUISE", RF_USE_DISGUISE,

		"BRUSHTEXTURE_SET_FRAME_INDEX", RF_BRUSHTEXTURE_SET_FRAME_INDEX
	);


	//
	// UseTargets:
	//
	// UseTarget Flags:
	solStateView.new_enum( "EntityUseTargetFlags",
		"NONE", ENTITY_USETARGET_FLAG_NONE,
		"PRESS", ENTITY_USETARGET_FLAG_PRESS,
		"TOGGLE", ENTITY_USETARGET_FLAG_TOGGLE,
		"CONTINUOUS", ENTITY_USETARGET_FLAG_CONTINUOUS,
		"DISABLED", ENTITY_USETARGET_FLAG_DISABLED
	);
	// UseTarget State:
	solStateView.new_enum( "EntityUseTargetState",
		"DEFAULT", ENTITY_USETARGET_STATE_DEFAULT,
		"OFF", ENTITY_USETARGET_STATE_OFF,
		"ON", ENTITY_USETARGET_STATE_ON,
		"TOGGLED", ENTITY_USETARGET_STATE_TOGGLED,
		"CONTINUOUS", ENTITY_USETARGET_STATE_CONTINUOUS
	);
	// UseTarget Types:
	solStateView.new_enum( "EntityUseTargetType",
		"OFF", ENTITY_USETARGET_TYPE_OFF,
		"ON", ENTITY_USETARGET_TYPE_ON,
		"SET", ENTITY_USETARGET_TYPE_SET,
		"TOGGLE", ENTITY_USETARGET_TYPE_TOGGLE
	);


	//
	// PushMove(as in a Door) Toggle Types:
	//
	solStateView.new_enum( "PushMoveToggle",
		"CLOSE",	0,	//! Moves it to "Bottom".
		"OPEN",		1	//! Moves it to "Up"
	);
	//
	// PushMoveInfo States:
	//
	solStateView.new_enum( "PushMoveState",
		"TOP",			PUSHMOVE_STATE_TOP,
		"BOTTOM",		PUSHMOVE_STATE_BOTTOM,
		"MOVING_UP",	PUSHMOVE_STATE_MOVING_UP,
		"MOVING_DOWN",	PUSHMOVE_STATE_MOVING_DOWN
	);
	

	//
	// Game Message Print Levels:
	//
	solStateView.new_enum( "ClientPrintLevel",
		"LOW", PRINT_LOW,		//! Pickup messages.
		"MEDIUM", PRINT_MEDIUM,	//! Death messages.
		"HIGH", PRINT_HIGH,		//! Critical messages.
		"CHAT", PRINT_CHAT		//! Chat messages.
	);
	//
	// System Print Levels:
	//
	solStateView.new_enum( "PrintLevel",
		//! Prints using default conchars text and thus color.
		"ALL",			PRINT_ALL,		//! Used for General messages..
		"TALK",			PRINT_TALK,		//! Prints in Green color.
		"DEVELOPER",	PRINT_DEVELOPER,//! Only prints when the cvar "developer" >= 1
		"WARNING",		PRINT_WARNING,	//! Print a Warning in Yellow color.
		"ERROR",		PRINT_ERROR,	//! Print an Error in Red color.
		"NOTICE",		PRINT_NOTICE	//! Print a Notice in bright Cyan color.
	);
}
