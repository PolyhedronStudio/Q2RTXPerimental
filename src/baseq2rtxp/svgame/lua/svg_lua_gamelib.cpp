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
	auto solNameSpace = solStateView[ nameSpaceName ].get_or_create< sol::table >();
	solNameSpace.set_function( "GetEntityForLuaName", GameLib_GetEntityForLuaName );
	solNameSpace.set_function( "GetEntityForTargetName", GameLib_GetEntityForTargetName );
	solNameSpace.set_function( "GetEntitiesForTargetName", GameLib_GetEntitiesForTargetName );
	solNameSpace.set_function( "GetPushMoverState", GameLib_GetPushMoverState );
	solNameSpace.set_function( "UseTarget", GameLib_UseTarget );
	solNameSpace.set_function( "UseTargets", GameLib_UseTarget );
	solNameSpace.set_function( "SignalOut", GameLib_SignalOut );

	// Developer print.
	gi.dprintf( "[Lua]: %s as -> \"%s\"\n", __func__, nameSpaceName );

	/**
	*	Register all global constants.
	**/
	// Door Toggle Types:
	// WID: TODO: We don't use these yet, do we even need them?
	solStateView.set( "DOOR_TOGGLE_CLOSE",	0 );
	solStateView.set( "DOOR_TOGGLE_OPEN",	1 );

	// PushMoveInfo States:
	solStateView.set( "PUSHMOVE_STATE_TOP",			PUSHMOVE_STATE_TOP );
	solStateView.set( "PUSHMOVE_STATE_BOTTOM",		PUSHMOVE_STATE_BOTTOM );
	solStateView.set( "PUSHMOVE_STATE_MOVING_UP",	PUSHMOVE_STATE_MOVING_UP );
	solStateView.set( "PUSHMOVE_STATE_MOVING_DOWN",	PUSHMOVE_STATE_MOVING_DOWN );

	// UseTarget Types:
	solStateView.set( "ENTITY_USETARGET_TYPE_OFF",		ENTITY_USETARGET_TYPE_OFF );
	solStateView.set( "ENTITY_USETARGET_TYPE_ON",		ENTITY_USETARGET_TYPE_ON );
	solStateView.set( "ENTITY_USETARGET_TYPE_SET",		ENTITY_USETARGET_TYPE_SET );
	solStateView.set( "ENTITY_USETARGET_TYPE_TOGGLE",	ENTITY_USETARGET_TYPE_TOGGLE );
}
