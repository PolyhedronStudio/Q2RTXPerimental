/********************************************************************
*
*
*
*	Server-Side Mono P/Invoke Exports API:
*
*
*
/*******************************************************************/
// Server header. (Does shared etc.)
#include "../server.h"
// SV Mono
#include "ServerMono.h"
// SV Mono Exports
#include "ServerMonoExports.h"



/*************************************************************************
* 
*	Game Progs we want to 'wrap'.
* 
*************************************************************************/
extern "C" {
	void PF_configstring( int index, const char *val ); //
	int PF_ModelIndex( const char *name ); //
	int PF_SoundIndex( const char *name ); //
	int PF_ImageIndex( const char *name ); //

	void PF_StartSound( edict_t *entity, int channel,
		int soundindex, float volume,
		float attenuation, float timeofs ); //
	void PF_StartPositionedSound( vec3_t origin, edict_t *entity, int channel,
		int soundindex, float volume,
		float attenuation, float timeofs ); //


	void PF_Multicast( const vec3_t origin, multicast_t to ); //
	void PF_Unicast( edict_t *ent, qboolean reliable ); //
	void PF_WriteFloat( float f ); //

	void PF_bprintf( int level, const char *fmt, ... ); //
	void PF_dprintf( const char *fmt, ... ); //
	void PF_cprintf( edict_t *ent, int level, const char *fmt, ... ); //
	void PF_centerprintf( edict_t *ent, const char *fmt, ... ); //

	void PF_LinkEdict( edict_t *ent );
	void PF_UnlinkEdict( edict_t *ent );
	qboolean PF_inVIS( const vec3_t p1, const vec3_t p2, int vis );
	qboolean PF_inPVS( const vec3_t p1, const vec3_t p2 );
	qboolean PF_inPHS( const vec3_t p1, const vec3_t p2 );
	void PF_SetAreaPortalState( int portalnum, qboolean open );
	qboolean PF_AreasConnected( int area1, int area2 );

	q_noreturn void PF_error( const char *fmt, ... );

	void PF_setmodel( edict_t *ent, const char *name );

	cvar_t *PF_cvar( const char *name, const char *value, int flags );

	void PF_Pmove( pmove_t *pm );

	void *PF_TagMalloc( unsigned size, unsigned tag );
	void PF_FreeTags( unsigned tag );

	void PF_AddCommandString( const char *string );
	void PF_DebugGraph( float value, int color );
};


/*************************************************************************
*
*	Print Messaging
*
*************************************************************************/
/**
*	@brief	Export wrapper for BPrintf.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_BPrintf( int32_t gamePrintLevel, const char *str ) {
	PF_bprintf( gamePrintLevel, "%s", str );
}

/**
*	@brief	Export wrapper for DPrintf.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_DPrintf( const char *str ) {
	PF_dprintf( "%s", str );
}
/**
*	@brief	Export wrapper for CPrintf.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_CPrintf( edict_t *ent, int gamePrintLevel, const char *str ) {
	PF_cprintf( ent, gamePrintLevel, "%s", str );
}
/**
*	@brief	Export wrapper for CenterPrintf.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_CenterPrint( edict_t *ent, const char *str ) {
	PF_centerprintf( ent, "%s", str );
}


/*************************************************************************
*
*	ConfigString and Resource Indexing/Precaching.
*
*************************************************************************/
/**
*	@brief	Export wrapper for PF_ConfigString
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_ConfigString( int32_t index, const char *value ) {
	PF_configstring( index, value );
}
/**
*	@brief	Export wrapper for PF_ModelIndex
**/
__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_ModelIndex( const char *name ) {
	return PF_ModelIndex( name );
}
/**
*	@brief	Export wrapper for PF_SoundIndex
**/
__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_SoundIndex( const char *name ) {
	return PF_SoundIndex( name );
}
/**
*	@brief	Export wrapper for PF_ModelIndex
**/
__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_ImageIndex( const char *name ) {
	return PF_ImageIndex( name );
}


/*************************************************************************
*
*	(Positioned-) Sound
*
*************************************************************************/
/**
*	@brief	Export wrapper for Sound().
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_StartSound( edict_t *ent, int channel, int soundIndex, float volume, float attenuation, float timeOffset ) {
	PF_StartSound( ent, channel, soundIndex, volume, attenuation, timeOffset );
}
/**
*	@brief	Export wrapper for StartPositionedSound.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_StartPositionedSound( vec3_t origin, edict_t *ent, int channel, int soundIndex, float volume, float attenuation, float timeOffset ) {
	PF_StartPositionedSound( origin, ent, channel, soundIndex, volume, attenuation, timeOffset );
}


/*************************************************************************
*
*	Message Read/Write, Uni, and Multi -casting.
*
*************************************************************************/
/**
*	@brief	Export wrapper for Unicast.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_Unicast( edict_t *ent, qboolean reliable ) {
	PF_Unicast( ent, reliable );
}
/**
*	@brief	Export wrapper for Multicast.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_Multicast( const vec3_t origin, multicast_t to ) {
	PF_Multicast( origin, to );
}
/**
*	@brief	Export wrapper for WriteFloat
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteFloat( const float fl ) {
	PF_WriteFloat( fl );
}
/**
*	@brief	Export wrapper for WriteChar
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteChar( int c ) {
	MSG_WriteChar( c );
}
/**
*	@brief	Export wrapper for WriteChar
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteByte( int c ) {
	MSG_WriteByte( c );
}
/**
*	@brief	Export wrapper for WriteChar
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteShort( int c ) {
	MSG_WriteShort( c );
}
/**
*	@brief	Export wrapper for WriteChar
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteLong( int c ) {
	MSG_WriteLong( c );
}
/**
*	@brief	Export wrapper for WriteChar
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteString( const char *s ) {
	MSG_WriteString( s );
}
/**
*	@brief	Export wrapper for WriteChar
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WritePos( const vec3_t pos ) {
	MSG_WritePos( pos );
}
/**
*	@brief	Export wrapper for WriteChar
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_MSG_WriteAngle( float f ) {
	MSG_WriteAngle( f );
}

/*************************************************************************
*
*	VIS/PVS/PHS/Area Functions
*
*************************************************************************/
/**
*	@brief	Export wrapper for LinkEntity
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_LinkEntity( edict_t *ent ) {
	PF_LinkEdict( ent );
}
/**
*	@brief	Export wrapper for UnlinkEntity
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_UnlinkEntity( edict_t *ent ) {
	PF_UnlinkEdict( ent );
}
/**
*	@brief	Export wrapper for inVIS
**/
__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_inVIS( const vec3_t p1, const vec3_t p2, int32_t vis ) {
	return PF_inVIS( p1, p2, vis );
}
/**
*	@brief	Export wrapper for inPVS
**/
__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_inPVS( const vec3_t p1, const vec3_t p2 ) {
	return PF_inPVS( p1, p2 );
}
/**
*	@brief	Export wrapper for inPHS
**/
__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_inPHS( const vec3_t p1, const vec3_t p2 ) {
	return PF_inPHS( p1, p2 );
}
/**
*	@brief	Export wrapper for SetAreaPortalState
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_SetAreaPortalState( int32_t portalNumber, qboolean open ) {
	return PF_SetAreaPortalState( portalNumber, open );
}
/**
*	@brief	Export wrapper for AreasConnected.
**/
__declspec( dllexport ) qboolean __cdecl SV_Mono_Export_AreasConnected( int32_t area1, int32_t area2 ) {
	return PF_AreasConnected( area1, area2 );
}

/**
*	@brief	Export wrapper for SV_Trace function.
**/
trace_t q_gameabi __cdecl SV_Trace( const vec3_t start, const vec3_t mins,
	const vec3_t maxs, const vec3_t end,
	edict_t *passedict, int contentmask );

__declspec( dllexport ) trace_t __cdecl SV_Mono_Export_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passedict, int contentmask ) {
	return SV_Trace( start, mins, maxs, end, passedict, contentmask );
}
/**
*	@brief	Export wrapper for SV_PointContents function.
**/
int SV_PointContents( const vec3_t p );
__declspec( dllexport ) int __cdecl SV_Mono_Export_PointContents( const vec3_t point ) {
	return SV_PointContents( point );
}

/**
*	@brief	Export wrapper for SV_AreaEntities function.
**/
int SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype );
// fills in a table of edict pointers with edicts that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// ??? does this always return the world?
__declspec( dllexport ) int __cdecl SV_Mono_Export_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxCount, int areaType ) {
	return SV_AreaEdicts( mins, maxs, list, maxCount, areaType );
}

/*************************************************************************
*
*	CVar Functions
*
*************************************************************************/
/**
*	@brief	Export wrapper for CVar.
**/
__declspec( dllexport ) cvar_t *__cdecl SV_Mono_Export_CVar( const char *name, const char *value, int32_t flags ) {
	return PF_cvar( name, value, flags );
}
/**
*	@brief	Export wrapper for CVar_Set. (uses Cvar_UserSet.)
**/
__declspec( dllexport ) cvar_t *__cdecl SV_Mono_Export_CVar_Set( const char *cvarName, const char *value ) {
	return Cvar_UserSet( cvarName, value );
}
/**
*	@brief	Export wrapper for CVar_ForceSet. (uses Cvar_Set.)
**/
__declspec( dllexport ) cvar_t *__cdecl SV_Mono_Export_CVar_ForceSet( const char *cvarName, const char *value ) {
	return Cvar_Set( cvarName, value );
}


/*************************************************************************
*
*	ClientCommand and ServerCommand parameter access
*
*************************************************************************/
/**
*	@brief	Export wrapper for CmdArgc.
**/
__declspec( dllexport ) int32_t __cdecl SV_Mono_Export_CmdArgc( ) {
	return Cmd_Argc( );
}
/**
*	@brief	Export wrapper for CmdArgv.
**/
__declspec( dllexport ) const char *__cdecl SV_Mono_Export_CmdArgv( int32_t n ) {
	return Cmd_Argv( n );
}
/**
*	@brief	Export wrapper for CmdArgs. ( The concatenation of all argv >= 1 .)
**/
__declspec( dllexport ) const char *__cdecl SV_Mono_Export_CmdArgs( ) {
	return Cmd_RawArgs( );
}
/**
*	@brief	Export wrapper for AddCommandString.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_AddCommandString( const char *str ) {
	PF_AddCommandString( str );
}


/*************************************************************************
*
*	Random/Other Functions
*
*************************************************************************/
/**
*	@brief	Export wrapper for Error.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_Error( const char *str ) {
	PF_error( "%s", str );
}
/**
*	@brief	Export wrapper for SetModel.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_SetModel( edict_t *ent, const char *name ) {
	PF_setmodel( ent, name );
}

/**
*	@brief	Export wrapper for CVar.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_Pmove( pmove_t *pm ) {
	PF_Pmove( pm );
}


/**
*	@brief	Export wrapper for CVar.
**/
__declspec( dllexport ) void __cdecl SV_Mono_Export_DebugGraph( float value, int32_t color ) {
	PF_DebugGraph( value, color );
}