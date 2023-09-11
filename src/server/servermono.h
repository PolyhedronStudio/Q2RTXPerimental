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

#include "mono/mono.h"


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
const MonoAssembly* SV_Mono_LoadAssembly(const std::string& path);
/**
*	@brief
**/
const void SV_Mono_UnloadAssembly(MonoAssembly* assembly);