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
*	Common Mono 'Initialization/Shutdown' Domain Functions:
*
/*******************************************************************/
/**
*	@brief	Creates the server side 'mono jit domain' environment.
**/
const int Com_Mono_Init( const std::string &jitDomainName, const bool enableDebugging );

/**
*	@brief	Destroys/Closes the 'mono jit domain' environment.
**/
const void Com_Mono_Shutdown( );



/********************************************************************
*
*	Common Mono 'Class' Functions:
*
/*******************************************************************/
/**
*	@return	A pointer to the className's runtime MonoClass type inside of said image's nameSpace.
**/
MonoClass *Com_Mono_GetClass_FromName_InImage( MonoImage *image, const std::string &nameSpace, const std::string className );
/**
*	@return A pointer to the className's runtime MonoClass type inside of said assembly's image nameSpace.
**/
MonoClass *Com_Mono_GetClass_FromName_InAssembly( MonoAssembly *assembly, const std::string &nameSpace, const std::string &className );



/********************************************************************
*
*	Common Mono 'Object' Functions:
*
/*******************************************************************/
/**
*	@brief	Allocate a new 'klass' type 'Object' for the domain.
*			Does not instantiate it (call its constructor.)
*	@param	Instantiate defaults to false, when set to true it expects a zero argument constructor.
**/
MonoObject *Com_Mono_NewObject_FromClassType( MonoDomain *monoDomain, MonoClass *klass, const bool instanciate = false );



/********************************************************************
*
*	Common Mono 'Method' Functions:
*
/*******************************************************************/
/**
*	@brief	Will call (if existing) methodName on objectInstance using the specified argument count and argument values.
*	@return	nullptr if the function is not found, otherwise a MonoObject containing the results.
**/
MonoObject *Com_Mono_CallMethod_FromName( MonoObject *objectInstance, const std::string &methodName, const int argCount, void **argValues );