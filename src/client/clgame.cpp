#include "cl_client.h"
#include "refresh/models.h"
#include "refresh/images.h"

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
static const qboolean PF_IsDemoPlayback( ) {
	return cls.demo.playback;
}
const uint64_t PF_GetDemoFramesRead( ) {
	return cls.demo.frames_read;
}
const float PF_GetDemoFileProgress() {
	return cls.demo.file_progress;
}
const int64_t PF_GetDemoFileSize() {
	return cls.demo.file_size;
}

/**
*	@return	The real system time since application boot time.
**/
static const uint64_t PF_GetRealTime() {
	return cls.realtime;
}
/**
*	@return	Client's actual current 'connection' state.
**/
static const int32_t PF_GetConnectionState() {
	return cls.state;
}
/**
*	@return	Type of renderer that is in-use.
**/
static const ref_type_t PF_GetRefreshType() {
	return cls.ref_type;
}



/**
*
*
*	Collision (Model):
* 
*
**/
/**
*   @return The number that matched the node's pointer. -1 if node was a nullptr.
**/
static const int32_t PF_CM_NumberForNode( cm_t *cm, mnode_t *node ) {
	return CM_NumberForNode( cm, node );
}
/**
*   @return The number that matched the leaf's pointer. 0 if leaf was a nullptr.
**/
static const int32_t PF_CM_NumberForLeaf( cm_t *cm, mleaf_t *leaf ) {
	return CM_NumberForLeaf( cm, leaf );
}
/**
*   @Return True if any leaf under headnode has a cluster that is potentially visible
**/
const qboolean PF_CM_HeadnodeVisible( mnode_t *headnode, byte *visbits ) {
	return CM_HeadnodeVisible( headnode, visbits );
}


/**
*
*
*	Commands:
*
*
**/
/**
*	@brief	Registers the specified function pointer as the 'name' command.
**/
void PF_Cmd_AddCommand( const char *name, xcommand_t function ) {
	cmdreg_t reg = { .name = name, .function = function };
	Cmd_RegCommand( &reg );
}
/**
*	@brief	Registers the specified function pointer as the 'name' command.
**/
void PF_Cmd_Register( const cmdreg_t *reg ) {
	for ( ; reg->name; reg++ )
		Cmd_RegCommand( reg );
}
/**
*	@brief	Deregisters the specified function pointer as the 'name' command.
**/
void PF_Cmd_Deregister( const cmdreg_t *reg ) {
	for ( ; reg->name; reg++ )
		Cmd_RemoveCommand( reg->name );
}

/**
*	@brief	Removes the specified 'name' command registration.
**/
void PF_Cmd_RemoveCommand( const char *name ) {
	Cmd_RemoveCommand( name );
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
*	Key Event destination:
*
*
**/
/**
*	@brief	Set the 'layer' of where key events are handled by.
**/
static void PF_SetKeyEventDestination( const keydest_t keyEventDestination ) {
	Key_SetDest( keyEventDestination );
}
/** 
*	@return	The 'layer' of where key events are handled by.
**/
static const keydest_t PF_GetKeyEventDestination( void ) {
	return Key_GetDest();
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
*	Refresh:
*
*
**/
/**
*	@brief
**/
const qhandle_t PF_R_RegisterModel( const char *name ) {
	return R_RegisterModel( name );
}
/**
*	@brief
**/
const qhandle_t PF_R_RegisterSkin( const char *name ) {
	return R_RegisterSkin( name );
}
const qhandle_t PF_R_RegisterImage( const char *name, const imagetype_t type, const imageflags_t flags ) {
	return R_RegisterImage( name, type, flags );
}

/**
*	@brief
**/
const qhandle_t PF_R_RegisterFont( const char *name ) {
	return R_RegisterPic( name );
}
/**
*	@brief	
**/
const qhandle_t PF_R_RegisterPic( const char *name ) {
	return R_RegisterPic( name );
}
/**
*	@brief
**/
const qhandle_t PF_R_RegisterPic2( const char *name ) {
	return R_RegisterPic2( name );
}
/**
*	@brief
**/
void PF_R_AddDecal( decal_t *d ) {
	return R_AddDecal( d );
}
/**
*	@brief	Returns a pointer to d_8to24table[256]; And yes, I know this is a cheesy method.
**/
const uint32_t *PF_R_Get8BitTo24BitTable( void ) {
	return d_8to24table;
}

/**
*
*
*	Screen:
*
*
**/
void PF_R_ClearColor( void ) {
	R_ClearColor();
}
void PF_R_SetAlpha( const float alpha ) {
	R_SetAlpha( alpha );
}
void PF_R_SetAlphaScale( const float alphaScale ) {
	R_SetAlphaScale( alphaScale );
}
void PF_R_SetColor( const uint32_t color ) {
	R_SetColor( color );
}
void PF_R_SetClipRect( const clipRect_t *clipRect ) {
	R_SetClipRect( clipRect );
}
const float PF_R_ClampScale( cvar_t *var ) {
	return R_ClampScale( var );
}
void PF_R_SetScale( const float scale ) {
	R_SetScale( scale );
}
void PF_R_DrawChar( const int32_t x, const int32_t y, const int32_t flags, const int32_t ch, const qhandle_t font ) {
	R_DrawChar( x, y, flags, ch, font );
}
const int32_t PF_R_DrawString( const int32_t x, const int32_t y, const int32_t flags, const size_t maxChars, const char *str, const qhandle_t font ) {
	return R_DrawString( x, y, flags, maxChars, str, font );
}
const qboolean PF_R_GetPicSize( int32_t *w, int32_t *h, const qhandle_t pic ) {
	return R_GetPicSize( w, h, pic );
}
void PF_R_DrawPic( const int32_t x, const int32_t y, const qhandle_t pic ) {
	R_DrawPic( x, y, pic );
}
void PF_R_DrawStretchPic( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic ) {
	R_DrawStretchPic( x, y, w, h, pic );
}
void PF_R_DrawKeepAspectPic( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic ) {
	R_DrawKeepAspectPic( x, y, w, h, pic );
}
void PF_R_DrawStretchRaw( const int32_t x, const int32_t y, const int32_t w, const int32_t h ) {
	R_DrawStretchRaw( x, y, w, h );
}
void PF_R_TileClear( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic ) {
	R_TileClear( x, y, w, h, pic );
}
void PF_R_DrawFill8( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const int32_t c ) {
	R_DrawFill8( x, y, w, h, c );
}
void PF_R_DrawFill32( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const uint32_t color ) {
	R_DrawFill32( x, y, w, h, color );
}
void PF_R_UpdateRawPic( const int32_t pic_w, const int32_t pic_h, uint32_t *pic ) {
	R_UpdateRawPic( pic_w, pic_h, pic );
}
void PF_R_DiscardRawPic( void ) {
	R_DiscardRawPic();
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
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	SHOWMISS( "%s", msg );
}
#else
// Empty placeholders, in case they are used outside of a forgotten #if USE_DEBUG
// so we can prevent anything from crashing.
static void PF_ShowNet( const int32_t level, const char *fmt, ... ) {
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	//SHOWNET( level, "%s", msg );
}
static void PF_ShowClamp( const int32_t level, const char *fmt, ... ) {
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	//SHOWNET( level, "%s", msg );
}
static void PF_ShowMiss( const char *fmt, ... ) {
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	SHOWNET( level, "%s", msg );
}
#endif



/**
*
*
*	Other:
*
*
**/
/**
*	@brief	Scans the ignore list for the given nickname.
*	@return	True if the nickname is found in the ignore list.
**/
const qboolean PF_CL_CheckForIgnore( const char *nickname ) {
	return CL_CheckForIgnore( nickname );
}
/**
*	@brief	Attempt to scan out an IP address in dotted-quad notation and
*			add it into circular array of recent addresses.
**/
void PF_CL_CheckForIP( const char *str ) {
	CL_CheckForIP( str );
}
/**
*	@brief	
**/
void PF_CL_CheckForVersion( const char *str ) {
	CL_CheckForVersion( str );
}

/**
*	@brief
**/
void PF_SetSpriteModelVerticality( const qhandle_t spriteHandle ) {
	model_t *model = MOD_ForHandle( spriteHandle );

	if ( model ) {
		model->sprite_vertical = true;
	}
}
/**
*	@return Sprite's framecount, or 0 on failure.
**/
const int32_t PF_GetSpriteModelFrameCount( const qhandle_t spriteHandle ) {
	model_t *model = MOD_ForHandle( spriteHandle );

	if ( model ) {
		return model->numframes;
	}

	return 0;
}
/**
*	@return	True if the handle leads to a valid and loaded sprite model.
**/
const qboolean PF_IsValidSpriteModelHandle( const qhandle_t spriteHandle ) {
	model_t *model = MOD_ForHandle( spriteHandle );
	return ( model ? true : false );
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

	Z_LeakTest( TAG_FREE );
}

/**
*	@brief	Called when the client is about to shutdown, giving us a last minute
*			shot at accessing possible required data.
**/
void CL_GM_PreShutdown( void ) {
	if ( clge ) {
		clge->PreShutdown();
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
void CL_GM_LoadProgs( void ) {

	clgame_import_t   imports;
	GameEntryFunctionPointer *entry = NULL;

	// Pre shutdown also.
	CL_GM_PreShutdown();

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

	// Client Static:
	imports.IsDemoPlayback = PF_IsDemoPlayback;
	imports.GetDemoFramesRead = PF_GetDemoFramesRead;
	imports.GetDemoFileProgress = PF_GetDemoFileProgress;
	imports.GetDemoFileSize = PF_GetDemoFileSize;
	//
	imports.GetRealTime = PF_GetRealTime;
	imports.GetConnectionState = PF_GetConnectionState;
	imports.GetRefreshType = PF_GetRefreshType;
	imports.Sys_Milliseconds = Sys_Milliseconds;
	//
	imports.GetClientFps = CL_GetClientFps;
	imports.GetRefreshFps = CL_GetRefreshFps;
	imports.GetResolutionScale = CL_GetResolutionScale;
	// End of Client Static.

	imports.Trace = CL_Trace;
	imports.Clip = CL_Clip;
	imports.PointContents = CL_PointContents;

	imports.GetConfigString = PF_GetConfigString;

	imports.Cmd_From = Cmd_From;
	imports.Cmd_Argc = Cmd_Argc;
	imports.Cmd_Argv = Cmd_Argv;
	imports.Cmd_Args = Cmd_Args;
	imports.Cmd_RawArgs = Cmd_RawArgs;
	imports.Cmd_ArgsFrom = Cmd_ArgsFrom;
	imports.Cmd_RawArgsFrom = Cmd_RawArgsFrom;
	imports.Cmd_ArgsRange = Cmd_ArgsRange;
	imports.Cmd_ArgsBuffer = Cmd_ArgsBuffer;
	imports.Cmd_ArgvBuffer = Cmd_ArgvBuffer;
	imports.Cmd_ArgOffset = Cmd_ArgOffset;
	imports.Cmd_FindArgForOffset = Cmd_FindArgForOffset;
	imports.Cmd_RawString = Cmd_RawString; // WID: C++20: Added const.
	imports.Cmd_Shift = Cmd_Shift;

	imports.Cmd_ExecTrigger = Cmd_ExecTrigger;

	imports.Cmd_Command_g = Cmd_Command_g;
	imports.Cmd_Alias_g = Cmd_Alias_g;
	imports.Cmd_Macro_g = Cmd_Macro_g;
	imports.Cmd_Config_g = Cmd_Config_g;
	imports.Cmd_FindMacro = Cmd_FindMacro;

	imports.Cmd_AddCommand = PF_Cmd_AddCommand;
	imports.Cmd_RemoveCommand = PF_Cmd_RemoveCommand;
	imports.Cmd_Register = PF_Cmd_Register;
	imports.Cmd_Deregister = PF_Cmd_Deregister;

	imports.CVar = PF_CVar;
	imports.CVar_Get = PF_CVarGet;
	imports.CVar_Set = Cvar_UserSet;
	imports.CVar_ForceSet = Cvar_Set;
	imports.CVar_SetInteger = Cvar_SetInteger;
	imports.CVar_ClampInteger = Cvar_ClampInteger;
	imports.CVar_ClampValue = Cvar_ClampValue;
	imports.CVar_Reset = PF_Cvar_Reset;
	imports.CVar_WeakGet = Cvar_WeakGet;
	imports.CVar_Variable_g = Cvar_Variable_g;
	imports.CVar_Default_g = Cvar_Default_g;

	imports.Con_ClearNotify_f = Con_ClearNotify_f;

	imports.CM_NodeForNumber = CM_NodeForNumber;
	imports.CM_NumberForNode = PF_CM_NumberForNode;
	imports.CM_LeafForNumber = CM_LeafForNumber;
	imports.CM_NumberForLeaf = PF_CM_NumberForLeaf;
	imports.CM_HeadnodeVisible = PF_CM_HeadnodeVisible;

	imports.CM_SetAreaPortalState = CM_SetAreaPortalState;
	imports.CM_GetAreaPortalState = CM_GetAreaPortalState;
	imports.CM_AreasConnected = CM_AreasConnected;

	imports.CM_BoxLeafs = CM_BoxLeafs;
	imports.CM_BoxLeafs_headnode = CM_BoxLeafs_headnode;
	imports.CM_BoxContents = CM_BoxContents;

	imports.CM_EntityKeyValue = CM_EntityKeyValue;
	imports.CM_GetNullEntity = CM_GetNullEntity;

	imports.KeyDown = CL_KeyDown;
	imports.KeyUp = CL_KeyUp;
	imports.KeyClear = CL_KeyClear;
	imports.KeyState = CL_KeyState;
	imports.Key_AnyKeyDown = Key_AnyKeyDown;
	imports.Key_IsDown = Key_IsDown;
	imports.Key_GetBinding = Key_GetBinding;
	imports.SetKeyEventDestination = PF_SetKeyEventDestination;
	imports.GetKeyEventDestination = PF_GetKeyEventDestination;

	imports.Print = PF_Print;
	imports.Error = PF_Error;

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

	imports.R_RegisterModel = PF_R_RegisterModel;
	imports.R_RegisterSkin = PF_R_RegisterSkin;
	imports.R_RegisterImage= PF_R_RegisterImage;
	
	imports.R_RegisterFont = PF_R_RegisterFont;
	imports.R_RegisterPic = PF_R_RegisterPic;
	imports.R_RegisterPic2 = PF_R_RegisterPic2;

	imports.R_ClearColor = PF_R_ClearColor;
	imports.R_SetAlpha = PF_R_SetAlpha;
	imports.R_SetAlphaScale = PF_R_SetAlphaScale;
	imports.R_SetColor = PF_R_SetColor;
	imports.R_SetClipRect = PF_R_SetClipRect;
	imports.R_ClampScale = PF_R_ClampScale;
	imports.R_SetScale = PF_R_SetScale;
	imports.R_DrawChar = PF_R_DrawChar;
	imports.R_DrawString = PF_R_DrawString;
	imports.R_GetPicSize = PF_R_GetPicSize;
	imports.R_DrawPic = PF_R_DrawPic;
	imports.R_DrawStretchPic = PF_R_DrawStretchPic;
	imports.R_DrawKeepAspectPic = PF_R_DrawKeepAspectPic;
	imports.R_DrawStretchRaw = PF_R_DrawStretchRaw;
	imports.R_TileClear = PF_R_TileClear;
	imports.R_DrawFill8 = PF_R_DrawFill8;
	imports.R_DrawFill32 = PF_R_DrawFill32;
	imports.R_UpdateRawPic = PF_R_UpdateRawPic;
	imports.R_DiscardRawPic = PF_R_DiscardRawPic;
	imports.R_AddDecal = PF_R_AddDecal;
	imports.R_Get8BitTo24BitTable = PF_R_Get8BitTo24BitTable;

	imports.SCR_GetColorName = SCR_GetColorName;
	imports.SCR_ParseColor = SCR_ParseColor;

	imports.S_StartSound = S_StartSound;
	imports.S_StartLocalSound = S_StartLocalSound;
	imports.S_StartLocalSoundOnce = S_StartLocalSoundOnce;
	imports.S_StopAllSounds = S_StopAllSounds;
	imports.S_RegisterSound = S_RegisterSound;
	imports.S_RegisterReverbEffect = S_RegisterReverbEffect;
	imports.S_SetupSpatialListener = S_SetupSpatialListener;
	imports.S_SetActiveReverbEffect = S_SetActiveReverbEffect;

	imports.TagMalloc = PF_TagMalloc;
	imports.TagFree = Z_Free;
	imports.FreeTags = PF_FreeTags;

	imports.V_RenderView = V_RenderView;
	imports.V_CalculateLocalPVS = V_CalculateLocalPVS;
	imports.V_AddEntity = V_AddEntity;
	imports.V_AddParticle = V_AddParticle;
	imports.V_AddSphereLight = V_AddSphereLight;
	imports.V_AddSpotLight = V_AddSpotLight;
	imports.V_AddSpotLightTexEmission = V_AddSpotLightTexEmission;
	imports.V_AddLight = V_AddLight;
	imports.V_AddLightStyle = V_AddLightStyle;

	imports.Z_Free = Z_Free;
	imports.Z_Freep = Z_Freep;
	imports.Z_Malloc = Z_Malloc;
	imports.Z_Realloc = Z_Realloc;

	imports.Prompt_AddMatch = Prompt_AddMatch;
	//#if USE_DEBUG
	// ShowNet
	imports.ShowNet = PF_ShowNet;
	// ShowClamp
	imports.ShowClamp = PF_ShowClamp;
	// ShowMiss
	imports.ShowMiss = PF_ShowMiss;
	//#endif

	imports.CheckForIgnore = PF_CL_CheckForIgnore;
	imports.CheckForIP = PF_CL_CheckForIP;
	imports.CheckForVersion = PF_CL_CheckForVersion;
	imports.Con_SkipNotify = Con_SkipNotify;

	imports.SetSpriteModelVerticality = PF_SetSpriteModelVerticality;
	imports.GetSpriteModelFrameCount = PF_GetSpriteModelFrameCount;
	imports.IsValidSpriteModelHandle = PF_IsValidSpriteModelHandle;

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