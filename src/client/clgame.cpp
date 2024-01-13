#include "client.h"

extern "C" {

/**
*
*
*	Prog Functions
*
*
**/
/**
*	CVar:
**/
static cvar_t *PF_CVar( const char *name, const char *value, int flags ) {
	if ( flags & CVAR_EXTENDED_MASK ) {
		Com_WPrintf( "ClientGame attemped to set extended flags on '%s', masked out.\n", name );
		flags &= ~CVAR_EXTENDED_MASK;
	}

	return Cvar_Get( name, value, flags | CVAR_GAME );
}


/**
*	Printing:
**/
/**
*	@brief	Client Side Printf
**/
static void PF_Print( print_type_t printType, const char *fmt, ... ) {
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	Com_LPrintf( printType, "%s", msg );
}

/**
*	@brief	Abort the client/(server too in case we're local.) with a game error
**/
static q_noreturn void PF_Error( const char *fmt, ... ) {
	char        msg[ MAXERRORMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	Com_Error( ERR_DROP, "ClientGame Error: %s", msg );
}


/**
*	'Tag' Managed memory allocation:
**/
/**
*	@brief	Malloc tag.
**/
static void *PF_TagMalloc( unsigned size, unsigned tag ) {
	Q_assert( tag + TAG_MAX > tag );
	if ( !size ) {
		return NULL;
	}
	return memset( Z_TagMalloc( size, static_cast<memtag_t>( tag + TAG_MAX ) ), 0, size ); // WID: C++20: Added cast.
}

/**
*	@brief	Free all 'tags'.
**/
static void PF_FreeTags( unsigned tag ) {
	Q_assert( tag + TAG_MAX > tag );
	Z_FreeTags( static_cast<memtag_t>( tag + TAG_MAX ) ); // WID: C++20: Added cast.
}



/**
*
*
*	Library Loading
*
*
**/
// Stores actual game library.
static void *game_library;

clgame_export_t *clge;

// WID: C++20: Typedef this for casting
typedef clgame_export_t *( GameEntryFunctionPointer( clgame_import_t * ) );


/**
*	@brief	Called when the client disconnects/quits, or has changed to a different game directory.
**/
void CL_GM_Shutdown( void ) {
	if ( clge ) {
		clge->Shutdown( );
		clge = NULL;
	}
	if ( game_library ) {
		Sys_FreeLibrary( game_library );
		game_library = NULL;
	}
}

/**
*	@brief	Helper for CL_LoadGameLibrary.
**/
static GameEntryFunctionPointer *_CL_LoadGameLibrary( const char *path ) {
	void *entry;

	entry = Sys_LoadLibrary( path, "GetGameAPI", &game_library );
	if ( !entry )
		Com_EPrintf( "Failed to load ClientGame library: %s\n", Com_GetLastError( ) );
	else
		Com_Printf( "Loaded ClientGame library from %s\n", path );

	return (GameEntryFunctionPointer *)( entry ); // WID: GCC Linux hates static cast here.
}

/**
*	@brief  Handles loading the actual Client Game library.
*/
static GameEntryFunctionPointer *CL_LoadGameLibrary( const char *game, const char *prefix ) {
	char path[ MAX_OSPATH ];

	if ( Q_concat( path, sizeof( path ), sys_libdir->string,
		 PATH_SEP_STRING, game, PATH_SEP_STRING,
		 prefix, "clgame" CPUSTRING LIBSUFFIX ) >= sizeof( path ) ) {
		Com_EPrintf( "ClientGame library path length exceeded\n" );
		return NULL;
	}

	if ( os_access( path, F_OK ) ) {
		if ( !*prefix )
			Com_Printf( "Can't access %s: %s\n", path, strerror( errno ) );
		return NULL;
	}

	return _CL_LoadGameLibrary( path );
}

/*
===============
CL_GM_InitGameProgs

Init the client game subsystem for a new map
===============
*/
void CL_GM_InitProgs( void ) {

	clgame_import_t   imports;
	GameEntryFunctionPointer *entry = NULL;

	// unload anything we have now
	CL_GM_Shutdown( );

	// for debugging or `proxy' mods
	//if ( sys_forceclgamelib->string[ 0 ] )
	//	entry = _CL_LoadGameLibrary( sys_forceclgamelib->string ); // WID: C++20: Added cast.

	// Try game first
	if ( !entry && fs_game->string[ 0 ] ) {
		entry = CL_LoadGameLibrary( fs_game->string, "" ); // WID: C++20: Added cast.
	}

	// Then try baseq2
	if ( !entry ) {
		entry = CL_LoadGameLibrary( BASEGAME, "" ); // WID: C++20: Added cast.
	}

	// all paths failed
	if ( !entry )
		Com_Error( ERR_DROP, "Failed to load game library" );

	// Setup import frametime related values so the GameDLL knows about it.
	imports.tick_rate = BASE_FRAMERATE;
	imports.frame_time_s = BASE_FRAMETIME_1000;
	imports.frame_time_ms = BASE_FRAMETIME;

	imports.CVar = PF_CVar;
	imports.CVar_Set = Cvar_UserSet;
	imports.CVar_ForceSet = Cvar_Set;

	imports.Print = PF_Print;
	imports.Error = PF_Error;

	imports.TagMalloc = PF_TagMalloc;
	imports.TagFree = Z_Free;
	imports.FreeTags = PF_FreeTags;

	clge = entry( &imports );

	if ( !clge ) {
		Com_Error( ERR_DROP, "ClientGame library returned NULL exports" );
	}

	if ( clge->apiversion != CLGAME_API_VERSION ) {
		Com_Error( ERR_DROP, "ClientGame library is version %d, expected %d",
				  clge->apiversion, CLGAME_API_VERSION );
	}

	// initialize
	clge->Init( );
}

}; // extern "C"