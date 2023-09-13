/********************************************************************
*
* 
* 
*	Server-Side Mono Implementation:
* 
* 
* 
/*******************************************************************/
#include "../server.h"
#include "ServerMono.h"


/**
*	@brief	Stores pointers to Mono Runtime objects for the server.
**/
//struct ServerMono {
//	// Server Domain.
//	MonoDomain *monoServerDomain = nullptr;
//	// Server Assembly.
//	MonoAssembly *monoServerAssembly = nullptr;
//	// Server Image.
//	MonoImage *monoServerImage = nullptr;
//};
ServerMono serverMono;


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
	serverMono.monoServerDomain = mono_domain_create_appdomain( const_cast<char *>( serverDomainName.c_str( ) ), nullptr );

	// Attempt to set the domain so we can thread attach it.
	auto res = mono_domain_set( serverMono.monoServerDomain, false );
	if ( res ) {
		mono_thread_attach( serverMono.monoServerDomain );
	}

	// True if non nullptr.
	return serverMono.monoServerDomain ? true : false;
}

/**
*	@brief	Destroys/Closes the server side 'mono jit domain' sandbox environment.
**/
const void SV_Mono_Shutdown( ) {
	// Shutdown the server domain if we have any.
	if ( serverMono.monoServerDomain ) {
		// Switch back to root domain.
		auto root_domain = mono_get_root_domain( );
		auto res = mono_domain_set( root_domain, false );
		// Unload 'ServerDomain'.
		if ( res ) {
			mono_domain_unload( serverMono.monoServerDomain );
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
*	@brief	Will load the 'Server Game' Mono assembly, if succesfull, return its pointer, otherwise: nullptr.
**/
MonoAssembly *SV_Mono_LoadAssembly( const std::string &path ) {
	// 'Open' the mono server assembly in our server domain.
	serverMono.monoServerAssembly = mono_domain_assembly_open( serverMono.monoServerDomain, path.c_str() );

	// If we do have an assembly loaded up, acquire its image.
	if ( serverMono.monoServerAssembly ) {
		serverMono.monoServerImage = mono_assembly_get_image( serverMono.monoServerAssembly );
	}

	// Is a nullptr on failure.
	return serverMono.monoServerAssembly;
}

/**
*	@brief	Unloads the 'Server Game' Mono assembly.
**/
const void SV_Mono_UnloadAssembly( ) {
	// Close image if we have any.
	//if ( serverMono.monoServerImage ) {
	//	mono_image_close( serverMono.monoServerImage );
	//	serverMono.monoServerImage = nullptr;
	//}
	// Close mono assembly.
	//if ( serverMono.monoServerAssembly ) {
	//	mono_assembly_close( serverMono.monoServerAssembly );
	//	serverMono.monoServerAssembly = nullptr;
	//}
}