/********************************************************************
*
* 
* 
*	Server-Side Mono Implementation:
* 
* 
* 
/*******************************************************************/
#include "server.h"
#include "servermono.h"

// Server mono Domain.
static MonoDomain* monoServerDomain = nullptr;


/********************************************************************
*
*	Initialization/Shutdown Domain:
*
/*******************************************************************/
/**
*	@brief	Creates the server side 'mono jit domain' sandbox environment.
*	@return	
**/
const int SV_Mono_Init() {
	// Server domain name.
	std::string serverDomainName = "MonoServerDomain";

	// Initialize the server domain.
	monoServerDomain = mono_domain_create_appdomain( const_cast<char *>( serverDomainName.c_str( ) ), nullptr );

	// Attempt to set the domain so we can thread attach it.
	auto res = mono_domain_set( monoServerDomain, 0 );
	if ( res ) {
		mono_thread_attach( monoServerDomain );
	}

	// True if non nullptr.
	return monoServerDomain ? true : false;
}

/**
*	@brief	Destroys/Closes the server side 'mono jit domain' sandbox environment.
**/
const void SV_Mono_Shutdown( ) {
	// Shutdown the server domain if we have any.
	if ( monoServerDomain ) {
		// Switch back to root domain.
		auto root_domain = mono_get_root_domain( );
		auto res = mono_domain_set( root_domain, 0 );
		// Unload 'ServerDomain'.
		if ( res ) {
			mono_domain_unload( monoServerDomain );
		}
	}
	// Perform garbage collection.
	mono_gc_collect( mono_gc_max_generation( ) );
}



/********************************************************************
*
*	Load/Unload Assembly:
*
/*******************************************************************/
/**
*	@brief	
**/
const MonoAssembly *SV_Mono_LoadAssembly( const std::string &path ) {
	return nullptr;
}
/**
*	@brief
**/
const void SV_Mono_UnloadAssembly( MonoAssembly *assembly ) {
	// Close mono assembly.
	if ( assembly ) {
		mono_assembly_close(assembly);
	}
}