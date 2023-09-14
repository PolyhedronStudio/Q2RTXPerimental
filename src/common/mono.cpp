/********************************************************************
*
*
*
*	Common Mono Implementation:
*
*
*
/*******************************************************************/
#pragma once

#include "shared/shared.h"

#include "mono/mono.h"
#include "common/mono.h"



/********************************************************************
*
*	Common Mono JIT Domain:
*
/*******************************************************************/
static MonoDomain *qcommonMonoJITDomain = nullptr;

/**
*	@brief	Mono message/warning/error log handler.
**/
static void QCommon_Mono_LogCallback( const char *log_domain, const char *log_level, const char *message,
							mono_bool /*fatal*/, void * /*user_data*/ ) {
	/**
	* Generate a nice log message.
	**/
	// Fetch domain name, set to 'unknown' if somehow it had none.
	const std::string domainName = ( log_domain ? log_domain : "unknown" );
	// Consider no log level, to be a warning.
	const std::string logLevel = ( log_level ? log_level : "warning" );
	// Acquire log message.
	const std::string logMessage = ( message ? message : "unknown message?" );
	// Generate final message.
	std::string outputMessage = "[";
	outputMessage += domainName;
	outputMessage += ":";
	outputMessage += logLevel;
	outputMessage += "] ";
	outputMessage += logMessage;

	/**
	*	Output log message.
	**/
	if ( logLevel == "error" ) {
		Com_EPrintf( "%s\n", outputMessage.c_str() );
	} else if ( logLevel == "warning" ) {
		Com_WPrintf( "%s\n", outputMessage.c_str() );
	} else {
		Com_LPrintf( print_type_t::PRINT_DEVELOPER, "%s\n", outputMessage.c_str() );
	}
}

/**
*	@brief	Creates the server side 'mono jit domain' environment.
**/
const int Com_Mono_Init( const std::string &jitDomainName, const bool enableDebugging ) {
	// Ensure to set the mono dirs to our local mono/lib and mono/etc.
	mono_set_dirs( "mono/lib", "mono/etc" );
	//mono_set_dirs( INTERNAL_MONO_ASSEMBLY_DIR, INTERNAL_MONO_CONFIG_DIR );

	// Enable debugging if requested.
	if ( enableDebugging ) {
		const char *options[] =
		{
			"--soft-breakpoints",
			"--debugger-agent=transport=dt_socket,suspend=n,server=y,address=127.0.0.1:55555,embedding=1",
			"--debug-domain-unload",

			// GC options:
			// check-remset-consistency: Makes sure that write barriers are properly issued in native code,
			// and therefore
			//    all old->new generation references are properly present in the remset. This is easy to mess
			//    up in native code by performing a simple memory copy without a barrier, so it's important to
			//    keep the option on.
			// verify-before-collections: Unusure what exactly it does, but it sounds like it could help track
			// down
			//    things like accessing released/moved objects, or attempting to release handles for an
			//    unloaded domain.
			// xdomain-checks: Makes sure that no references are left when a domain is unloaded.
			"--gc-debug=check-remset-consistency,verify-before-collections,xdomain-checks"
		};
		// clang-format on
		mono_jit_parse_options( sizeof( options ) / sizeof( char * ), const_cast<char **>( options ) );
		mono_debug_init( MONO_DEBUG_FORMAT_MONO );
	}

	// Set trace level to start 'logging' from.
	mono_trace_set_level_string( "warning" );
	// Set the logger callback handler.
	mono_trace_set_log_handler( QCommon_Mono_LogCallback, nullptr );

	// Finally create the root JIT domain.
	qcommonMonoJITDomain = mono_jit_init( jitDomainName.c_str( ) );
	// Attach to current thread.
	mono_thread_set_main( mono_thread_current( ) );

	// Return.
	return qcommonMonoJITDomain != nullptr;
}

/**
*	@brief	Destroys/Closes the 'mono jit domain' environment.
**/
const void Com_Mono_Shutdown( ) {
	if ( qcommonMonoJITDomain ) {
		mono_jit_cleanup( qcommonMonoJITDomain );
	}
	qcommonMonoJITDomain = nullptr;
}



/********************************************************************
*
*	Common Mono 'Class' Functions:
*
/*******************************************************************/
/**
*	@return	A pointer to the className's runtime MonoClass type inside of said image's nameSpace.
**/
MonoClass *Com_Mono_GetClass_FromName_InImage( MonoImage *image, const std::string &nameSpace, const std::string className ) {
	// Lookup the class type object.
	MonoClass *klass = mono_class_from_name( image, nameSpace.c_str(), className.c_str() );

	// Log error in case of an invalid find (nullptr).
	if ( klass == nullptr ) {
		std::string errorStr = "Com_Mono_GetClass_FromName_InImage: Couldn't find: ";
		errorStr += nameSpace;
		errorStr += "::";
		errorStr += className;
		Com_WPrintf( "%s\n", errorStr.c_str() );

		// klass == nullptr, but for ensurance sake, we return nullptr here.
		return nullptr;
	}

	return klass;
}
/**
*	@return A pointer to the className's runtime MonoClass type inside of said assembly's image nameSpace.
**/
MonoClass *Com_Mono_GetClass_FromName_InAssembly( MonoAssembly *assembly, const std::string &nameSpace, const std::string &className ) {
	// Retreive the Assembly's Image.
	MonoImage *image = mono_assembly_get_image( assembly );

	// Look up the class in the assembly's image.
	return Com_Mono_GetClass_FromName_InImage( image, nameSpace, className );
}



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
MonoObject *Com_Mono_NewObject_FromClassType( MonoDomain *monoDomain, MonoClass *klass, const bool instanciate ) {
	MonoObject *klassInstance = mono_object_new( monoDomain, klass );

	if ( klassInstance == nullptr ) {
		std::string errorStr = "Com_Mono_Object_New_FromClass: Got a 'nullptr' result.";
		Com_WPrintf( "%s\n", errorStr.c_str( ) );

		// klassInstance == nullptr, but for ensurance sake, we return nullptr here.
		return nullptr;
	}

	// Instanciate if demanded.
	if ( instanciate ) {
		mono_runtime_object_init( klassInstance );
	}

	return klassInstance;
}



/********************************************************************
*
*	Common Mono 'Method' Functions:
*
/*******************************************************************/
/**
*	@brief	Will call (if existing) methodName on objectInstance using the specified argument count and argument values.
*	@return	nullptr if the function is not found, otherwise a MonoObject containing the results.
**/
MonoObject *Com_Mono_CallMethod_FromName( MonoObject *objectInstance, const std::string &methodName, const int argCount, void **argValues ) {
	// Get the MonoClass pointer from the instance
	MonoClass *instanceClass = mono_object_get_class( objectInstance );

	// Get a reference to the method in the class
	MonoMethod *method = mono_class_get_method_from_name( instanceClass, methodName.c_str(), argCount );

	if ( method == nullptr ) {
		std::string errorStr = "Com_Mono_CallMethod_FromName: No method '";
		errorStr += methodName;
		errorStr += "'";
		errorStr += " found with ";
		errorStr += std::to_string( argCount );
		errorStr += " parameter";
		errorStr += argCount > 1 ? "s" : "";
		Com_WPrintf( "%s\n", errorStr.c_str( ) );

		// Something went wrong, nullptr results.
		return nullptr;
	}

	// Call the C# method on the objectInstance instance, and get any potential exceptions
	MonoObject *exception;// = nullptr;
	//void *param = &argValues[0];
	MonoObject *invokeResult = mono_runtime_invoke( method, objectInstance, argValues, &exception );

	// OR

	//MonoObject *exception = nullptr;
	//void *params[] =
	//{
	//	&argValues
	//};

	//// Invoke the method.
	//MonoObject *invokeResult = mono_runtime_invoke( method, objectInstance, params, &exception );

	// Return the invocation resulting MonoObject pointer.
	return invokeResult;
}


/*
//----------------------------------------------------------------------------------------//
// Bake Invoke.
//
// bakeInvoke is used to bake together a void array containing all the method's arguments.
// If needed it will apply the desired conversion functions that Mono requires.
//----------------------------------------------------------------------------------------//
template<typename T>
void *bakeInvoke( T arg );

// For integers.
template<>
inline void *bakeInvoke( int *x ) {
	return (void *)x;
}

// For booleans.
template<>
inline void *bakeInvoke( bool *x ) {
	return (void *)x;
}

// For char strings.
template<>
inline void *bakeInvoke( char **str ) {
	return mono_string_new( _domain, *str );
}

// For std::strings.
template<>
inline void *bakeInvoke( std::string *str ) {
	return mono_string_new( _domain, str->c_str( ) );
}

inline bool _invoke( const std::string &name, void *arguments ) {
	// Find the method.
	if ( _class && _object && _domain ) {
		MonoMethod *method = mono_class_get_method_from_name_flags( _class, name.c_str( ), -1, MONO_METHOD_ATTR_SPECIAL_NAME );
		if ( method ) {
			mono_runtime_invoke( method, _object, (void **)arguments, NULL );
		}
	}

	return true;
}

//-----------------------------------------
// Maybe I should let Invoke work like this.
// invoke("introduceMyself").add<string>("Mike").add<int>(22).exec();
inline bool invoke( const std::string &name ) {
	void *data = NULL;
	return _invoke( name, data );
}
template<typename T1>
inline bool invoke( const std::string &name, T1 &one ) {
	void *data[ 2 ] = { bakeInvoke( &one ) };
	return _invoke( name, data );
}
template<typename T1, typename T2>
inline bool invoke( const std::string &name, T1 &one, T2 &two ) {
	void *data[ 2 ] = { bakeInvoke( &one ), bakeInvoke( &two ) };
	return _invoke( name, data );
}
template<typename T1, typename T2, typename T3>
inline bool invoke( const std::string &name, typename T1 &one, typename T2 &two, typename T3 &three ) {
	void *data[ 3 ] = { bakeInvoke( &one ), bakeInvoke( &two ), bakeInvoke( &three ) };
	return _invoke( name, data );
}
template<typename T1, typename T2, typename T3, typename T4>
inline bool invoke( const std::string &name, T1 &one, T2 &two, T3 &three, T4 &four ) {
	void *data[ 4 ] = { bakeInvoke( &one ), bakeInvoke( &two ), bakeInvoke( &three ), bakeInvoke( &four ) };
	return _invoke( name, data );
}
template<typename T1, typename T2, typename T3, typename T4, typename T5>
inline bool invoke( const std::string &name, T1 &one, T2 &two, T3 &three, T4 &four, T5 &five ) {
	void *data[ 5 ] = { bakeInvoke( &one ), bakeInvoke( &two ), bakeInvoke( &three ), bakeInvoke( &four ), bakeInvoke( &five ) };
	return _invoke( name, data );
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
inline bool invoke( const std::string &name, T1 &one, T2 &two, T3 &three, T4 &four, T5 &five, T6 &six ) {
	void *data[ 6 ] = { bakeInvoke( &one ), bakeInvoke( &two ), bakeInvoke( &three ), bakeInvoke( &four ), bakeInvoke( &five ), bakeInvoke( &six ) };
	return _invoke( name, data );
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
inline bool invoke( const std::string &name, T1 &one, T2 &two, T3 &three, T4 &four, T5 &five, T6 &six, T7 &seven ) {
	void *data[ 7 ] = { bakeInvoke( &one ), bakeInvoke( &two ), bakeInvoke( &three ), bakeInvoke( &four ), bakeInvoke( &five ), bakeInvoke( &six ), bakeInvoke( &seven ) };
	return _invoke( name, data );
}
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
inline bool invoke( const std::string &name, T1 &one, T2 &two, T3 &three, T4 &four, T5 &five, T6 &six, T7 &seven, T8 &eight ) {
	void *data[ 8 ] = { bakeInvoke( &one ), bakeInvoke( &two ) bakeInvoke( &three ), bakeInvoke( &four ), bakeInvoke( &five ), bakeInvoke( &six ), bakeInvoke( &seven ), bakeInvoke( &eight ) }
	return _invoke( name, data );
}*/