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
}

/**
*	@brief	Creates the server side 'mono jit domain' environment.
**/
const int QCommon_Mono_Init( const std::string &jitDomainName, const bool enableDebugging ) {
	// Ensure to set the mono dirs to our local mono/lib and mono/etc.
	mono_set_dirs( "mono/lib", "mono/etc" );
	//mono_set_dirs( INTERNAL_MONO_ASSEMBLY_DIR, INTERNAL_MONO_CONFIG_DIR );

	// Enable debugging if requested.
	if ( enableDebugging ) {
		// clang-format off
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
const void QCommon_Mono_Shutdown( ) {
	if ( qcommonMonoJITDomain ) {
		mono_jit_cleanup( qcommonMonoJITDomain );
	}
	qcommonMonoJITDomain = nullptr;
}
