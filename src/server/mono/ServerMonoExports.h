/********************************************************************
*
*
*
*	Server-Side Mono Header:
*
*
*
/*******************************************************************/
#pragma once

extern "C" {
	#include "shared/shared.h"
	#include "shared/game.h"
	#include "common/common.h"
	#include "common/zone.h"
};

// MonoAPI includes reside here.
#include "mono/mono.h"

// Our MonoAPI wrapper API resides here.
#include "common/mono.h"


/*
*	@brief All of this is enclosed in extern "C" and uses OS specific EXPORT defines.
*/
extern "C" {
	/*************************************************************************
	*
	*
	*	ConfigString and Resource Indexing/Precaching.
	*
	*
	*************************************************************************/
	/**
	*	@brief	Export wrapper for PF_ConfigString
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_ConfigString( int32_t index, const char *value );
	/**
	*	@brief	Export wrapper for PF_ModelIndex
	**/
	__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_ModelIndex( const char *name );
	/**
	*	@brief	Export wrapper for PF_SoundIndex
	**/
	__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_SoundIndex( const char *name );
	/**
	*	@brief	Export wrapper for PF_ModelIndex
	**/
	__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_ImageIndex( const char *name );


	/*************************************************************************
	*
	*
	*	Print Messaging
	*
	*
	*************************************************************************/
	/**
	*	@brief	Export wrapper for BPrintf.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_BPrintf( int32_t gamePrintLevel, const char *str );
	/**
	*	@brief	Export wrapper for DPrintf.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_DPrintf( const char *str );
	/**
	*	@brief	Export wrapper for CPrintf.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_CPrintf( edict_t *ent, int gamePrintLevel, const char *str );
	/**
	*	@brief	Export wrapper for CenterPrintf.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_CenterPrint( edict_t *ent, const char *str );


	/*************************************************************************
	*
	*
	*	(Positioned-) Sound
	*
	*
	*************************************************************************/
	/**
	*	@brief	Export wrapper for Sound().
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_StartSound( edict_t *ent, int channel, int soundIndex, float volume, float attenuation, float timeOffset );

	/**
	*	@brief	Export wrapper for StartPositionedSound.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_StartPositionedSound( vec3_t origin, edict_t *ent, int channel, int soundIndex, float volume, float attenuation, float timeOffset );


	/*************************************************************************
	*
	*
	*	Message Read/Write, Uni, and Multi -casting.
	*
	*
	*************************************************************************/
	/**
	*	@brief	Export wrapper for Unicast.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_Unicast( edict_t *ent, qboolean reliable );
	/**
	*	@brief	Export wrapper for Multicast.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_Multicast( const vec3_t origin, multicast_t to );
	/**
	*	@brief	Export wrapper for WriteFloat
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteFloat( const float fl );
	/**
	*	@brief	Export wrapper for WriteChar
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteChar( int c );
	/**
	*	@brief	Export wrapper for WriteChar
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteByte( int c );
	/**
	*	@brief	Export wrapper for WriteChar
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteShort( int c );
	/**
	*	@brief	Export wrapper for WriteChar
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteLong( int c );
	/**
	*	@brief	Export wrapper for WriteChar
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteString( const char *s );
	/**
	*	@brief	Export wrapper for WriteChar
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WritePos( const vec3_t pos );
	/**
	*	@brief	Export wrapper for WriteChar
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteAngle( float f );

	/*************************************************************************
	*
	*
	*	VIS/PVS/PHS/Area Functions
	*
	*
	*************************************************************************/
	/**
	*	@brief	Export wrapper for LinkEntity
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_LinkEntity( edict_t *ent );
	/**
	*	@brief	Export wrapper for UnlinkEntity
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_UnlinkEntity( edict_t *ent );
	/**
	*	@brief	Export wrapper for inVIS
	**/
	__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_inVIS( const vec3_t p1, const vec3_t p2, int32_t vis );
	/**
	*	@brief	Export wrapper for inPVS
	**/
	__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_inPVS( const vec3_t p1, const vec3_t p2 );
	/**
	*	@brief	Export wrapper for inPHS
	**/
	__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_inPHS( const vec3_t p1, const vec3_t p2 );

	/**
	*	@brief	Export wrapper for SetAreaPortalState
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_SetAreaPortalState( int32_t portalNumber, qboolean open );
	/**
	*	@brief	Export wrapper for AreasConnected.
	**/
	__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_AreasConnected( int32_t area1, int32_t area2 );

	/**
	*	@brief	Export wrapper for SV_Trace function.
	**/
	__declspec( dllexport ) trace_t __cdecl SV_Mono_Export_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passedict, int contentmask );
	/**
	*	@brief	Export wrapper for SV_PointContents function.
	**/
	__declspec( dllexport ) int __cdecl SV_Mono_Export_PointContents( const vec3_t point );


	/**
	*	@brief	Export wrapper for SV_AreaEntities function.
	**/
	__declspec( dllexport ) int __cdecl SV_Mono_Export_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxCount, int areaType );

	/*************************************************************************
	*
	*
	*	ClientCommand and ServerCommand parameter access
	*
	*
	*************************************************************************/
	/**
	*	@brief	Export wrapper for CmdArgc.
	**/
	__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_CmdArgc( );
	/**
	*	@brief	Export wrapper for CmdArgv.
	**/
	__declspec( dllexport ) const char *__cdecl SV_Mono_Export_CmdArgv( int32_t n );
	/**
	*	@brief	Export wrapper for CmdArgs. ( The concatenation of all argv >= 1 .)
	**/
	__declspec( dllexport ) const char *__cdecl SV_Mono_Export_CmdArgs( );


	/*************************************************************************
	*
	*
	*	Random/Other Functions
	*
	*
	*************************************************************************/
	/**
	*	@brief	Export wrapper for Error.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_Error( const char *str );
	/**
	*	@brief	Export wrapper for SetModel.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_SetModel( edict_t *ent, const char *name );
	/**
	*	@brief	Export wrapper for CVar.
	**/
	__declspec( dllexport ) cvar_t *__cdecl SV_Mono_Export_CVar( const char *name, const char *value, int32_t flags );
	/**
	*	@brief	Export wrapper for CVar.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_Pmove( pmove_t *pm );

	/**
	*	@brief	Export wrapper for CVar.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_AddCommandString( const char *str );
	/**
	*	@brief	Export wrapper for CVar.
	**/
	__declspec( dllexport ) void __cdecl SV_Mono_Export_DebugGraph( float value, int32_t color );
}