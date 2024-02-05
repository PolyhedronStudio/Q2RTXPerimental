#include "cl_client.h"

extern "C" {

/**
*
*
*	Prog Functions
*
*
**/
/**
*
*
*	Client Static:
*
*
**/
/**
*	@return	True if playing a demo, false otherwise.
**/
static const bool PF_IsDemoPlayback( ) {
	return cls.demo.playback;
}
static const uint64_t PF_GetRealTime() {
	return cls.realtime;
}
static const int32_t PF_GetConnectionState() {
	return cls.state;
}
static const ref_type_t PF_GetRefreshType() {
	return cls.ref_type;
}



/**
*
*
*	ConfigStrings:
*
*
**/
/**
*	@brief	Returns the given configstring that sits at index.
**/
static configstring_t *PF_GetConfigString( const int32_t configStringIndex ) {
	if ( configStringIndex < 0 || configStringIndex > MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "%s: bad index: %d", __func__, configStringIndex );
		return nullptr;
	}
	return &cl.configstrings[ configStringIndex ];
}



/**
* 
* 
*	CVar:
* 
* 
**/
/**
*	@brief	For creating CVars in the client game. Will append them with CVAR_GAME flag.
**/
static cvar_t *PF_CVar( const char *name, const char *value, const int32_t flags ) {
	int32_t newFlags = flags;
	if ( flags & CVAR_EXTENDED_MASK ) {
		Com_WPrintf( "ClientGame attemped to set extended flags on '%s', masked out.\n", name );
		newFlags &= ~CVAR_EXTENDED_MASK;
	}

	return Cvar_Get( name, value, newFlags | CVAR_GAME );
}
/**
*	@brief	For fetching CVars outside of the client game.
**/
static cvar_t *PF_CVarGet( const char *name, const char *value, const int32_t flags ) {
	return Cvar_Get( name, value, flags );
}
/**
*	@brief	Resets cvar to default value.
**/
static void PF_Cvar_Reset( cvar_t *cvar ) {
	Cvar_Reset( cvar );
}



/**
*
*
*	Rendering:
*
* 
**/
const qhandle_t PF_R_RegisterModel( const char *name ) {
	return R_RegisterModel( name );
}

/**
* 
* 
*	Printing:
* 
* 
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
* 
* 
*	'Tag' Managed memory allocation:
* 
* 
**/
/**
*	@brief	Malloc tag.
**/
static void *PF_TagMalloc( unsigned size, unsigned tag ) {
	Q_assert( tag <= UINT16_MAX - TAG_MAX );
	return Z_TagMallocz( size, static_cast<memtag_t>( tag + TAG_MAX ) );
}

/**
*	@brief	Free tag memory.
**/
static void PF_FreeTags( unsigned tag ) {
	Q_assert( tag <= UINT16_MAX - TAG_MAX );
	Z_FreeTags( static_cast<memtag_t>( tag + TAG_MAX ) );
}



/**
*
*
*	Debugging Utilities, for USE_DEBUG!=0:
*
*
**/
#if USE_DEBUG
static void PF_ShowNet( const int32_t level, const char *fmt, ... ) {
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	SHOWNET( level, "%s", msg );
}
static void PF_ShowClamp( const int32_t level, const char *fmt, ... ) {
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	SHOWCLAMP( level, "%s", msg );
}
static void PF_ShowMiss( const char *fmt, ... ) {
	
}
#else
// Empty placeholders, in case they are used outside of a forgotten #if USE_DEBUG
// so we can prevent anything from crashing.
void PF_ShowNet( const int32_t level, const char *fmt, ... ) {}
void PF_ShowClamp( const int32_t level, const char *fmt, ... ) {}
void PF_ShowMiss( const int32_t level, const char *fmt, ... ) {}
#endif

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

	Z_LeakTest( TAG_FREE );
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
void CL_GM_LoadProgs( void ) {

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
	if ( !entry ) {
		Com_Error( ERR_DROP, "Failed to load game library" );
	}

	// Setup import frametime related values so the GameDLL knows about it.
	imports.tick_rate = BASE_FRAMERATE;
	imports.frame_time_s = BASE_FRAMETIME_1000;
	imports.frame_time_ms = BASE_FRAMETIME;

	imports.client = &cl;

	imports.IsDemoPlayback = PF_IsDemoPlayback;
	imports.GetRealTime = PF_GetRealTime;
	imports.GetConnectionState = PF_GetConnectionState;
	imports.GetRefreshType = PF_GetRefreshType;

	imports.GetConfigString = PF_GetConfigString;

	imports.SCR_GetColorName = SCR_GetColorName;
	imports.SCR_ParseColor = SCR_ParseColor;

	imports.CVar = PF_CVar;
	imports.CVar_Get = PF_CVarGet;
	imports.CVar_Set = Cvar_UserSet;
	imports.CVar_ForceSet = Cvar_Set;
	imports.CVar_Reset = PF_Cvar_Reset;

	imports.Print = PF_Print;
	imports.Error = PF_Error;

	imports.TagMalloc = PF_TagMalloc;
	imports.TagFree = Z_Free;
	imports.FreeTags = PF_FreeTags;

	imports.MSG_ReadInt8 = MSG_ReadInt8;
	imports.MSG_ReadUint8 = MSG_ReadUint8;
	imports.MSG_ReadInt16 = MSG_ReadInt16;
	imports.MSG_ReadUint16 = MSG_ReadUint16;
	imports.MSG_ReadInt32 = MSG_ReadInt32;
	imports.MSG_ReadInt64 = MSG_ReadInt64;
	imports.MSG_ReadUint64 = MSG_ReadUint64;
	imports.MSG_ReadUintBase128 = MSG_ReadUintBase128;
	imports.MSG_ReadIntBase128 = MSG_ReadIntBase128;
	imports.MSG_ReadFloat = MSG_ReadFloat;
	imports.MSG_ReadHalfFloat = MSG_ReadHalfFloat;
	imports.MSG_ReadString = MSG_ReadString;
	imports.MSG_ReadStringLine = MSG_ReadStringLine;
	imports.MSG_ReadAngle8 = MSG_ReadAngle8;
	imports.MSG_ReadAngle16 = MSG_ReadAngle16;
	imports.MSG_ReadAngleHalfFloat = MSG_ReadAngleHalfFloat;
	imports.MSG_ReadDir8 = MSG_ReadDir8;
	imports.MSG_ReadPos = MSG_ReadPos;

	imports.Trace = CL_Trace;
	imports.Clip = CL_Clip;
	imports.PointContents = CL_PointContents;

	imports.Prompt_AddMatch = Prompt_AddMatch;

	imports.R_RegisterModel = PF_R_RegisterModel;

	imports.S_StartSound = S_StartSound;
	imports.S_StartLocalSound = S_StartLocalSound;
	imports.S_StartLocalSoundOnce = S_StartLocalSoundOnce;
	imports.S_StopAllSounds = S_StopAllSounds;
	imports.S_RegisterSound = S_RegisterSound;

	imports.V_AddEntity = V_AddEntity;
	imports.V_AddParticle = V_AddParticle;
	imports.V_AddSphereLight = V_AddSphereLight;
	imports.V_AddSpotLight = V_AddSpotLight;
	imports.V_AddSpotLightTexEmission = V_AddSpotLightTexEmission;
	imports.V_AddLight = V_AddLight;
	imports.V_AddLightStyle = V_AddLightStyle;

	#if USE_DEBUG
	// ShowNet
	imports.ShowNet = PF_ShowNet;
	// ShowClamp
	imports.ShowClamp = PF_ShowClamp;
	// ShowMiss
	imports.ShowMiss = PF_ShowMiss;
	#endif

	clge = entry( &imports );

	if ( !clge ) {
		Com_Error( ERR_DROP, "ClientGame library returned NULL exports" );
	}

	if ( clge->apiversion != CLGAME_API_VERSION ) {
		Com_Error( ERR_DROP, "ClientGame library is version %d, expected %d",
				  clge->apiversion, CLGAME_API_VERSION );
	}
}

/**
*	@brief	Properly pre-initialize the client game sub systems.
**/
void CL_GM_PreInit( void ) {
	clge->PreInit( );
}

/**
*	@brief	Finalize intializing the client game system.
**/
void CL_GM_Init( void ) {
	// initialize
	clge->Init();

	// Point our cl_entities to the address of the memory supplied by the client game.
	cl_entities = clge->entities;
}

}; // extern "C"