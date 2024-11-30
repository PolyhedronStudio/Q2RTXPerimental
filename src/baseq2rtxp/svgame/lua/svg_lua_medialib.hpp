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
const qhandle_t MediaLib_PrecacheSound( const std::string &soundPath );
/**
*	@return	Binding to gi.sound
**/
const int32_t MediaLib_Sound( lua_edict_t leEnt, const int32_t soundChannel, const qhandle_t soundHandle, const float soundVolume, const int32_t soundAttenuation, const float soundTimeOffset );
