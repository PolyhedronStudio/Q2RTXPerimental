#include "cl_client.h"
#include "refresh/models.h"
#include "refresh/images.h"

#include "common/collisionmodel.h"
#include "common/skeletalmodels/cm_skm.h"
#include "common/skeletalmodels/cm_skm_posecache.h"

extern cl_screen_shared_t cl_scr;

/**
*
*
*	Prog Functions
*
*
**/
/**
*	@brief	
**/
static void PF_EmitDemoFrame( void ) {
	CL_EmitDemoFrame();
}
/**
*	@brief
**/
static void PF_InitialDemoFrame( void ) {
	CL_FirstDemoFrame();
}
/**
*	@brief
**/
static const qboolean PF_IsDemoRecording( ) {
	return cls.demo.recording && !cls.demo.paused && !cls.demo.seeking;
}
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
*
*
*	Client Static:
*
*
**/
/**
*	@return	The real system time since application boot time.
**/
static const uint64_t PF_GetRealTime() {
	return cls.realtime;
}
/**
*	@return	Seconds since last frame.
**/
static const double PF_GetFrameTime() {
	return cls.frametime;
}
/**
*	@return	Client's actual current 'connection' state.
**/
static const connstate_t PF_GetConnectionState() {
	return cls.state;
}
/**
*	@return	Type of renderer that is in-use.
**/
static const ref_type_t PF_GetRefreshType() {
	return cls.ref_type;
}

// Netchan outgoing sequence, incoming acknowledged sequence.
static const int64_t PF_Netchan_GetOutgoingSequence( void ) {
	return cls.netchan.outgoing_sequence;
}
static const int64_t PF_Netchan_GetIncomingAcknowledged( void ) {
	return cls.netchan.incoming_acknowledged;
}
static const int64_t PF_Netchan_GetDropped( void ) {
	return cls.netchan.dropped;
}

/**
*
*
*	Collision (Model):
* 
*
**/
/**
*   @return A pointer to nullleaf if there is no BSP model cached up, or the number is out of bounds.
*           A pointer to the 'special case for solid leaf' if number == -1
*           A pointer to the BSP Node that matched with 'number'.
**/
static mnode_t *PF_CM_NodeForNumber( const int32_t number ) {
	return CM_NodeForNumber( &cl.collisionModel, number );
}
/**
*   @return The number that matched the node's pointer. -1 if node was a nullptr.
**/
static const int32_t PF_CM_NumberForNode( mnode_t *node ) {
	cm_t *cm = &cl.collisionModel;
	return CM_NumberForNode( cm, node );
}

/**
*   @return A pointer to nullleaf if there is no BSP model cached up.
*           A pointer to the BSP Leaf that matched with 'number'.
**/
static mleaf_t *PF_CM_LeafForNumber( const int32_t number ) {
	cm_t *cm = &cl.collisionModel;
	return CM_LeafForNumber( cm, number );
}
/**
*   @return The number that matched the leaf's pointer. 0 if leaf was a nullptr.
**/
static const int32_t PF_CM_NumberForLeaf( mleaf_t *leaf ) {
	cm_t *cm = &cl.collisionModel;
	return CM_NumberForLeaf( cm, leaf );
}
/**
*   @Return True if any leaf under headnode has a cluster that is potentially visible
**/
static const qboolean PF_CM_HeadnodeVisible( mnode_t *headnode, byte *visbits ) {
	return CM_HeadnodeVisible( headnode, visbits );
}

void PF_CM_SetAreaPortalState( const int32_t portalnum, const int32_t open ) {
	CM_SetAreaPortalState( &cl.collisionModel, portalnum, open );
}
const int32_t PF_CM_GetAreaPortalState( const int32_t portalnum ) {
	return CM_GetAreaPortalState( &cl.collisionModel, portalnum );
}
const bool PF_CM_AreasConnected( const int32_t area1, const int32_t area2 ) {
	return CM_AreasConnected( &cl.collisionModel, area1, area2 );
}

/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
static const int32_t PF_CM_BoxLeafs( const vec3_t mins, const vec3_t maxs, mleaf_t **list, const int32_t listsize, mnode_t **topnode ) {
	cm_t *cm = &cl.collisionModel;
	return CM_BoxLeafs( cm, mins, maxs, list, listsize, topnode );
}
/**
*   @brief  Populates the list of leafs which the specified bounding box touches. If top_node is not
*           set to NULL, it will contain a value copy of the the top node of the BSP tree that fully
*           contains the box.
**/
static const int32_t PF_CM_BoxLeafs_headnode( const vec3_t mins, const vec3_t maxs, mleaf_t **list, int listsize, mnode_t *headnode, mnode_t **topnode ) {
	cm_t *cm = &cl.collisionModel;
	return CM_BoxLeafs_headnode( cm, mins, maxs, list, listsize, headnode, topnode );
}
/**
*   @return The contents mask of all leafs within the absolute bounds.
**/
static const cm_contents_t PF_CM_BoxContents( const vec3_t mins, const vec3_t maxs, mnode_t *headnode ) {
	cm_t *cm = &cl.collisionModel;
	if ( !cm->cache ) {
		//Com_Error( ERR_DROP, "%s: No collision model cached up!\n", __func__ );
		return CONTENTS_NONE;
	}
	return CM_BoxContents( cm, mins, maxs, headnode );
}


/**
*   @brief  Performs a 'Clipping' trace against the world, and all the active in-frame solidEntities.
**/
static const cm_trace_t PF_CM_BoxTrace( const Vector3 *start, const Vector3 *end, const Vector3 *mins, const Vector3 *maxs, mnode_t *headNode, const cm_contents_t brushMask ) {
	cm_trace_t trace = {};
	trace.surface = &nulltexinfo.c;
	trace.material = &cm_default_material;
	trace.surface2 = &nulltexinfo.c;
	trace.material2 = &cm_default_material;

	// Ensure we have a collision model cached up. Return an empty trace if not.
	if ( !cl.collisionModel.cache ) {
		//Com_Error( ERR_DROP, "%s: No collision model cached up!\n", __func__ );
		return trace;
	}

	// Get vector copies.
	const Vector3 _start = *start;
	const Vector3 _end = *end;
	// Perform the box trace.
	CM_BoxTrace( &cl.collisionModel, &trace, _start, _end, mins, maxs, headNode, brushMask );

	// Return the box trace.
	return trace;
}
/**
*   @brief  Performs a 'Clipping' trace against the world, and all the active in-frame solidEntities.
**/
static const cm_trace_t PF_CM_TransformedBoxTrace( 
	const Vector3 *start, const Vector3 *end,
	const Vector3 *mins, const Vector3 *maxs,
	mnode_t *headnode, const cm_contents_t brushMask,
	const Vector3 *origin, const Vector3 *angles ) {

	cm_trace_t trace = {};
	trace.surface = &nulltexinfo.c;
	trace.material = &cm_default_material;
	trace.surface2 = &nulltexinfo.c;
	trace.material2 = &cm_default_material;

	// Ensure we have a collision model cached up. Return an empty trace if not.
	if ( !cl.collisionModel.cache ) {
		//Com_Error( ERR_DROP, "%s: No collision model cached up!\n", __func__ );
		return trace;
	}

	// Get vector copies.
	const Vector3 _start = *start;
	const Vector3 _end = *end;
	const Vector3 _origin = *origin;
	const Vector3 _angles = *angles;

	// Perform the Transformed Box Trace.
	CM_TransformedBoxTrace( &cl.collisionModel, &trace, _start, _end, mins, maxs, headnode, brushMask, &_origin.x, &_angles.x );

	// Return the box trace.
	return trace;
}

/**
*   @return The type of 'contents' at the given point.
**/
static const cm_contents_t PF_CM_PointContents( const Vector3 *point, mnode_t *headNode ) {
	if ( !cl.collisionModel.cache ) {
		//Com_Error( ERR_DROP, "%s: No collision model cached up!\n", __func__ );
		return CONTENTS_NONE;
	}
	return CM_PointContents( &cl.collisionModel, &point->x, headNode );
}
/**
*   @return The type of 'contents' at the given point.
**/
static const cm_contents_t PF_CM_TransformedPointContents( const Vector3 *point, mnode_t *headnode, const Vector3 *origin, const Vector3 *angles ) {
	if ( !cl.collisionModel.cache ) {
		//Com_Error( ERR_DROP, "%s: No collision model cached up!\n", __func__ );
		return CONTENTS_NONE;
	}
	// Get vector pointers
	return CM_TransformedPointContents( &cl.collisionModel, &point->x, headnode, &origin->x, &angles->x );
}

/**
*	@return	A pointer to the default material.
**/
static const cm_material_t *PF_CM_GetDefaultMaterial() {
	return &cm_default_material;
}
/**
*	@return A pointer to the null texture info surface.
**/
static const cm_surface_t *PF_CM_GetNullSurface() {
	return &nulltexinfo.c;
}
/**
*	@return	The hull node for the specified entity.
**/
static mnode_t *PF_CL_GetEntityHullNode( const centity_t *ent/*, const bool includeSolidTriggers = false */ ) {
	return CL_GetEntityHullNode( ent/*, includeSolidTriggers */ );
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
void PF_Cmd_AddMacro( const char *name, xmacro_t function ) {
	Cmd_AddMacro( name, function );
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
*	FileSystem:
*
*
**/
/**
*	@brief	Returns non 0 in case of existance.
**/
static const int32_t PF_FS_FileExistsEx( const char *path, const uint32_t flags ) {
	return FS_FileExistsEx( path, flags | FS_TYPE_ANY | FS_PATH_GAME );
}
/**
*	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
*	@return	length < 0 indicates error.
**/
static const int32_t PF_FS_LoadFile( const char *path, void **buffer ) {
	return FS_LoadFileEx( path, buffer, 0 | FS_TYPE_ANY | FS_PATH_GAME, TAG_FILESYSTEM );
}
/**
*	@brief	Frees FS_FILESYSTEM Tag Malloc file buffer.
**/
static void PF_FS_FreeFile( void *buffer ) {
	FS_FreeFile( buffer );
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
static q_noreturn void PF_Error( error_type_t errorType, const char *fmt, ... ) {
	char        msg[ MAXERRORMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	Com_Error( errorType, "ClientGame Error: %s", msg );
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
void PF_R_RenderFrame( refdef_t *fd ) {
	R_RenderFrame( fd );
}

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
	return R_RegisterFont( name );
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
void PF_R_DrawPicEx( double destX, double destY, double destW, double destH, qhandle_t pic,
	double srcX, double srcY, double srcW, double srcH ) {
	R_DrawPicEx( destX, destY, destW, destH, pic, srcX, srcY, srcW, srcH );
}
void PF_R_DrawStretchPic( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic ) {
	R_DrawStretchPic( x, y, w, h, pic );
}
void PF_R_DrawRotateStretchPic( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const float angle, const int32_t pivot_x, const int32_t pivot_y, const qhandle_t pic ) {
	R_DrawRotateStretchPic( x, y, w, h, angle, pivot_x, pivot_y, pic );
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
//!
void PF_R_DrawFill8f( float x, float y, float w, float h, int32_t c ) {
	R_DrawFill8f( x, y, w, h, c );
}
//!
void PF_R_DrawFill32f( float x, float y, float w, float h, uint32_t color ) {
	R_DrawFill32f( x, y, w, h, color );
}
//!
void PF_R_DiscardRawPic( void ) {
	R_DiscardRawPic();
}

/**
*
*
*	Skeletal Model Pose Cache:
* 
* 
**/
skm_transform_t *PF_SKM_PoseCache_AcquireCachedMemoryBlock( const uint32_t size ) {
	return SKM_PoseCache_AcquireCachedMemoryBlock( cls.clientPoseCache, size );
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
static void *PF_TagMalloc( const uint32_t size, const memtag_t tag ) {
	Q_assert( tag <= UINT16_MAX - TAG_MAX );
	return Z_TagMalloc( size, static_cast<memtag_t>( tag /*+ TAG_MAX*/ ) );
}
/**
*	@brief	Malloc tag.
**/
static void *PF_TagMallocz( const uint32_t size, const memtag_t tag ) {
	Q_assert( tag <= UINT16_MAX - TAG_MAX );
	return Z_TagMallocz( size, static_cast<memtag_t>( tag /*+ TAG_MAX*/ ) );
}
/**
*	@brief	Malloc tag.
**/
static void *PF_TagReMalloc( void *ptr, const uint32_t size ) {
	return Z_Realloc( ptr, size );
}
/**
*	@brief	Free tag memory.
**/
static void PF_FreeTags( const memtag_t tag ) {
	Q_assert( tag <= UINT16_MAX - TAG_MAX );
	Z_FreeTags( static_cast<memtag_t>( tag /*+ TAG_MAX*/ ) );
}
/**
*	@brief	Malloc tag.
**/
static void PF_TagFree( void *ptr ) {
	Z_Free( ptr );
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
*
*
*	(Skeletal though-) Alias Models:
*
*
**/
/**
*   @brief  Pointer to model data matching the name, otherwise a (nullptr) on failure.
**/
static const model_t *PF_R_GetModelDataForName( const char *name ) {
	return MOD_ForName( name );
}
/**
*   @return Pointer to model data matching the resource handle, otherwise a (nullptr) on failure.
**/
static const model_t *PF_R_GetModelDataForHandle( const qhandle_t handle ) {
	return MOD_ForHandle( handle );
}



/**
*
*
*	Sprite Models:
*
*
**/
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

QEXTERN_C_ENCLOSE( clgame_export_t *clge; );

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

	// The actual module name to load.
	#if _DEBUG 
	if ( Q_concat( path, sizeof( path ), sys_libdir->string,
		PATH_SEP_STRING, game, PATH_SEP_STRING,
		prefix, "clgame" CPUSTRING "_d" LIBSUFFIX ) >= sizeof( path ) ) {
		Com_EPrintf( "ClientGame library path length exceeded\n" );
		return NULL;
	}
	#else
	if ( Q_concat( path, sizeof( path ), sys_libdir->string,
		PATH_SEP_STRING, game, PATH_SEP_STRING,
		prefix, "clgame" CPUSTRING LIBSUFFIX ) >= sizeof( path ) ) {
		Com_EPrintf( "ClientGame library path length exceeded\n" );
		return NULL;
	}
	#endif

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
	imports.screen = &cl_scr;

	// Client Static:
	imports.EmitDemoFrame = PF_EmitDemoFrame;
	imports.InitialDemoFrame = PF_InitialDemoFrame;

	imports.IsDemoRecording = PF_IsDemoRecording;
	imports.IsDemoPlayback = PF_IsDemoPlayback;
	
	imports.GetDemoFramesRead = PF_GetDemoFramesRead;
	imports.GetDemoFileProgress = PF_GetDemoFileProgress;
	imports.GetDemoFileSize = PF_GetDemoFileSize;
	//
	imports.GetRealTime = PF_GetRealTime;
	imports.GetFrameTime = PF_GetFrameTime;
	imports.GetConnectionState = PF_GetConnectionState;
	imports.GetRefreshType = PF_GetRefreshType;
	imports.Sys_Milliseconds = Sys_Milliseconds;
	//
	imports.GetClientFps = CL_GetClientFps;
	imports.GetRefreshFps = CL_GetRefreshFps;
	imports.GetResolutionScale = CL_GetResolutionScale;
	//
	imports.Netchan_GetOutgoingSequence = PF_Netchan_GetOutgoingSequence;
	imports.Netchan_GetIncomingAcknowledged = PF_Netchan_GetIncomingAcknowledged;
	imports.Netchan_GetDropped = PF_Netchan_GetDropped;
	// End of Client Static.
	
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
	imports.Cmd_AddMacro = PF_Cmd_AddMacro;
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

	imports.Con_ClearNotificationTexts_f = Con_ClearNotificationTexts_f;

	imports.CM_NodeForNumber = PF_CM_NodeForNumber;
	imports.CM_NumberForNode = PF_CM_NumberForNode;
	imports.CM_LeafForNumber = PF_CM_LeafForNumber;
	imports.CM_NumberForLeaf = PF_CM_NumberForLeaf;
	imports.CM_HeadnodeVisible = PF_CM_HeadnodeVisible;

	imports.CM_SetAreaPortalState = PF_CM_SetAreaPortalState;
	imports.CM_GetAreaPortalState = PF_CM_GetAreaPortalState;
	imports.CM_AreasConnected = PF_CM_AreasConnected;

	imports.CM_BoxTrace = PF_CM_BoxTrace;
	imports.CM_TransformedBoxTrace = PF_CM_TransformedBoxTrace;
	imports.CM_PointContents = PF_CM_PointContents;
	imports.CM_TransformedPointContents = PF_CM_TransformedPointContents;

	imports.CM_BoxLeafs = PF_CM_BoxLeafs;
	imports.CM_BoxLeafs_headnode = PF_CM_BoxLeafs_headnode;
	imports.CM_BoxContents = PF_CM_BoxContents;

	imports.CM_EntityKeyValue = CM_EntityKeyValue;
	imports.CM_GetNullEntity = CM_GetNullEntity;
	imports.CM_GetDefaultMaterial = PF_CM_GetDefaultMaterial;
	imports.CM_GetNullSurface = PF_CM_GetNullSurface;
	imports.GetEntityHullNode = PF_CL_GetEntityHullNode;

	imports.FS_FileExistsEx = PF_FS_FileExistsEx;
	imports.FS_LoadFile = PF_FS_LoadFile;
	imports.FS_FreeFile = PF_FS_FreeFile;

	imports.ActivateInput = IN_Activate;
	imports.KeyDown = CL_KeyDown;
	imports.KeyUp = CL_KeyUp;
	imports.KeyClear = CL_KeyClear;
	imports.KeyState = CL_KeyState;
	imports.Key_AnyKeyDown = Key_AnyKeyDown;
	imports.Key_IsDown = Key_IsDown;
	imports.Key_GetBinding = Key_GetBinding;
	imports.SetKeyEventDestination = PF_SetKeyEventDestination;
	imports.GetKeyEventDestination = PF_GetKeyEventDestination;

	imports.CL_ProcessMouseMove = CL_ProcessMouseMove;

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
	imports.MSG_ReadTruncatedFloat = MSG_ReadTruncatedFloat;
	imports.MSG_ReadDouble = MSG_ReadDouble;
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

	imports.R_RenderFrame = PF_R_RenderFrame;
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
	imports.R_DrawPicEx = PF_R_DrawPicEx;
	imports.R_DrawStretchPic = PF_R_DrawStretchPic;
	imports.R_DrawRotateStretchPic = PF_R_DrawRotateStretchPic;
	imports.R_DrawKeepAspectPic = PF_R_DrawKeepAspectPic;
	imports.R_DrawStretchRaw = PF_R_DrawStretchRaw;
	imports.R_TileClear = PF_R_TileClear;
	imports.R_DrawFill8 = PF_R_DrawFill8;
	imports.R_DrawFill32 = PF_R_DrawFill32;
	imports.R_DrawFill8f = PF_R_DrawFill8f;
	imports.R_DrawFill32f = PF_R_DrawFill32f;
	imports.R_UpdateRawPic = PF_R_UpdateRawPic;
	imports.R_DiscardRawPic = PF_R_DiscardRawPic;
	imports.R_AddDecal = PF_R_AddDecal;
	imports.R_Get8BitTo24BitTable = PF_R_Get8BitTo24BitTable;

	imports.SCR_GetColorName = SCR_GetColorName;
	imports.SCR_ParseColor = SCR_ParseColor;

	//
	//	Skeletal Model Common:
	//
	imports.SKM_GetBoneByName = SKM_GetBoneByName;
	imports.SKM_GetBoneByNumber = SKM_GetBoneByNumber;
	//
	//	Pose Cache Memory Manager:
	//
	imports.SKM_PoseCache_AcquireCachedMemoryBlock = PF_SKM_PoseCache_AcquireCachedMemoryBlock;
	//	Skeletal Model(IQM) Pose Transform and Lerping:
	//
	imports.SKM_LerpBonePoses = SKM_LerpBonePoses;
	imports.SKM_GetBonePosesForFrame = SKM_GetBonePosesForFrame;
	imports.SKM_ComputeLerpBonePoses = SKM_ComputeLerpBonePoses;
	imports.SKM_RecursiveBlendFromBone = SKM_RecursiveBlendFromBone;
	imports.SKM_TransformBonePosesLocalSpace = SKM_TransformBonePosesLocalSpace;
	imports.SKM_TransformBonePosesWorldSpace = SKM_TransformBonePosesWorldSpace;


	imports.S_StartSound = S_StartSound;
	imports.S_StartLocalSound = S_StartLocalSound;
	imports.S_StartLocalSoundOnce = S_StartLocalSoundOnce;
	imports.S_StopAllSounds = S_StopAllSounds;
	imports.S_RegisterSound = S_RegisterSound;
	imports.S_SoundNameForHandle = S_SoundNameForHandle;
	imports.S_SetEAXEnvironmentProperties = S_SetEAXEnvironmentProperties;
	imports.S_SetupSpatialListener = S_SetupSpatialListener;

	imports.TagMalloc = PF_TagMalloc;
	imports.TagMallocz = PF_TagMallocz;
	imports.TagReMalloc = PF_TagReMalloc;
	imports.TagFree = PF_TagFree;
	imports.FreeTags = PF_FreeTags;

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

	imports.Q_ErrorNumber = Q_ErrorNumber;
	imports.Q_ErrorString = Q_ErrorString;
	imports.CheckForIgnore = PF_CL_CheckForIgnore;
	imports.CheckForIP = PF_CL_CheckForIP;
	imports.CheckForVersion = PF_CL_CheckForVersion;
	imports.Con_SkipNotify = Con_SkipNotify;

	imports.R_GetModelDataForName = PF_R_GetModelDataForName;
	imports.R_GetModelDataForHandle = PF_R_GetModelDataForHandle;

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

