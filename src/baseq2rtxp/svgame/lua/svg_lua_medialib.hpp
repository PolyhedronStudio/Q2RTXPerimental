/********************************************************************
*
*
*	ServerGame: Lua Binding API functions.
*	NameSpace: "Media".
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	NameSpace: "Media":
*
*
*
**/
/**
*
*	Sound:
*
**/
/**
*	@return	The numeric handle of the precached sound, -1 on failure.
**/
int MediaLib_PrecacheSound( lua_State *L );
/**
*	@return	Binding to gi.sound
**/
int MediaLib_Sound( lua_State *L );