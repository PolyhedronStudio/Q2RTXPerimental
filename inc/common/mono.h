/********************************************************************
*
*
*
*	Common Mono Header:
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
*	@brief	Creates the server side 'mono jit domain' environment.
**/
const int QCommon_Mono_Init( const std::string &jitDomainName, const bool enableDebugging );

/**
*	@brief	Destroys/Closes the 'mono jit domain' environment.
**/
const void QCommon_Mono_Shutdown( );
