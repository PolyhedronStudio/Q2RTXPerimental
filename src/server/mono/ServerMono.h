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

// MonoAPI includes reside here.
#include "mono/mono.h"

// Our MonoAPI wrapper API resides here.
#include "common/mono.h"

/**
*	@brief	Stores pointers to Mono Runtime objects for the server.
**/
struct ServerMono {
	// Server Domain.
	MonoDomain *monoServerDomain = nullptr;
	// Server Assembly.
	MonoAssembly *monoServerAssembly = nullptr;
	// Server Image.
	MonoImage *monoServerImage = nullptr;
};
extern ServerMono serverMono;

/********************************************************************
*
*	Initialization/Shutdown Domain:
*
/*******************************************************************/
/**
*	@brief	Creates the server side 'mono jit domain' sandbox environment.
*	@return
**/
const int SV_Mono_Init();

/**
*	@brief	Destroys/Closes the server side 'mono jit domain' sandbox
*			environment.
**/
const void SV_Mono_Shutdown();



/********************************************************************
*
*	Load/Unload Assembly:
*
/*******************************************************************/
/**
*	@brief
**/
MonoAssembly* SV_Mono_LoadAssembly( const std::string& path );
/**
*	@brief
**/
const void SV_Mono_UnloadAssembly( );