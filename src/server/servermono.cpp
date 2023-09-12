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



// Server Domain.
static MonoDomain *monoServerDomain = nullptr;
// Server Assembly.
static MonoAssembly *monoServerAssembly = nullptr;
// Server Image.
static MonoImage *monoServerImage = nullptr;



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
	auto res = mono_domain_set( monoServerDomain, false );
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
		auto res = mono_domain_set( root_domain, false );
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
*	Load/Unload Assembly/Image:
*
/*******************************************************************/
/**
*	@brief	
**/
const MonoAssembly *SV_Mono_LoadAssembly( const std::string &path ) {
	// 'Open' the mono server assembly in our server domain.
	monoServerAssembly = mono_domain_assembly_open( monoServerDomain, path.c_str() );

	// If we do have an assembly loaded up, acquire its image.
	if ( monoServerAssembly ) {
		monoServerImage = mono_assembly_get_image( monoServerAssembly );
	}

	// Is a nullptr on failure.
	return monoServerAssembly;
}

/**
*	@brief
**/
const void SV_Mono_UnloadAssembly( ) {
	// Close image if we have any.
	if ( monoServerImage ) {
		mono_image_close( monoServerImage );
		monoServerImage = nullptr;
	}
	// Close mono assembly.
	if ( monoServerAssembly ) {
		mono_assembly_close( monoServerAssembly );
		monoServerAssembly = nullptr;
	}
}