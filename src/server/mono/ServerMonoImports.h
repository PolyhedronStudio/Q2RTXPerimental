/********************************************************************
*
*
*
*	Server-Side Mono P/Invoke Imports API:
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



/**
*
*	'Extern "C"' Functionality:
*
**/
/*
*	@brief All of this is enclosed in extern "C" so we can call to it from our C code bases.
*/
#ifdef __cplusplus
extern "C" {
	#endif
	extern MonoVMS_t *monoVMs;

	/***
	*
	*
	*	ServerGame 'Import' Managed API Callbacks
	*
	*
	***/
	/**
	*	@brief	Call into 'Managed' Initialize function.
	**/
	edict_t *__cdecl ServerMonoVM_Import_Initialize( int *edictSize, int *numEdicts, int *maxEdicts, int *versionAPI );
	/**
	*	@brief	Call into 'Managed' Shutdown function.
	**/
	const void __cdecl ServerMonoVM_Import_ShutdownGame( );

	/**
	*	@brief	Call into 'Managed' SpawnEntities function.
	**/
	const void __cdecl ServerMonoVM_Import_SpawnEntities( const char *mapName, const char *entities, const char *spawnPoint );

	/**
	*	@brief	Call into 'Managed' WriteGame function.
	**/
	const void __cdecl ServerMonoVM_Import_WriteGame( const char *fileName, int32_t autoSave );
	/**
	*	@brief	Call into 'Managed' ReadGame function.
	**/
	const void __cdecl ServerMonoVM_Import_ReadGame( const char *fileName );

	/**
	*	@brief	Call into 'Managed' WriteLevel function.
	**/
	const void __cdecl ServerMonoVM_Import_WriteLevel( const char *fileName );
	/**
	*	@brief	Call into 'Managed' ReadLevel function.
	**/
	const void __cdecl ServerMonoVM_Import_ReadLevel( const char *fileName );


	/**
	*	@brief	Call into 'Managed' ClientConnect function.
	**/
	const int32_t __cdecl ServerMonoVM_Import_ClientConnect( edict_t *entity, const char *userInfo );
	/**
	*	@brief	Call into 'Managed' ClientUserInfoChanged function.
	**/
	const void __cdecl ServerMonoVM_Import_ClientBegin( edict_t *entity );
	/**
	*	@brief	Call into 'Managed' ClientUserInfoChanged function.
	**/
	const void __cdecl ServerMonoVM_Import_ClientUserInfoChanged( edict_t *entity, const char *userInfo );
	/**
	*	@brief	Call into 'Managed' ClientDisconnect function.
	**/
	const void __cdecl ServerMonoVM_Import_ClientDisconnect( edict_t *entity );
	/**
	*	@brief	Call into 'Managed' ClientCommand function.
	**/
	const void __cdecl ServerMonoVM_Import_ClientCommand( edict_t *entity );
	/**
	*	@brief	Call into 'Managed' ClientThink function.
	**/
	const void __cdecl ServerMonoVM_Import_ClientThink( edict_t *entity, usercmd_t *userCommand );

	/**
	*	@brief	Call into 'Managed' RunFrame function.
	**/
	const void __cdecl ServerMonoVM_Import_RunFrame( );
	/**
	*	@brief	Call into 'Managed' RunFrame function.
	**/
	const void __cdecl ServerMonoVM_Import_ServerCommand( );
	#ifdef __cplusplus
}; // extern "C" {
#endif