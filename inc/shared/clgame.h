/********************************************************************
*
*
*	ClientGame Header.
*
*
********************************************************************/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//
// clgame.h -- client game dll information visible to client
//
#define CLGAME_API_VERSION    4

// edict->svflags

//#define SVF_NOCLIENT            0x00000001  // don't send entity to clients, even if it has effects
//#define SVF_DEADMONSTER         0x00000002  // treat as CONTENTS_DEADMONSTER for collision
//#define SVF_MONSTER             0x00000004  // treat as CONTENTS_MONSTER for collision
//
//// edict->solid values
//typedef enum {
//    SOLID_NOT,          // no interaction with other objects
//    SOLID_TRIGGER,      // only touch when inside, after moving
//    SOLID_BOUNDS_BOX,         // touch on edge
//    SOLID_BSP           // bsp clip, touch on edge
//} solid_t;
//
//// extended features
//
//#define GMF_CLIENTNUM               0x00000001
//#define GMF_PROPERINUSE             0x00000002
////#define GMF_MVDSPEC                 0x00000004
//#define GMF_WANT_ALL_DISCONNECTS    0x00000008
//
//#define GMF_ENHANCED_SAVEGAMES      0x00000400
//#define GMF_VARIABLE_FPS            0x00000800
//#define GMF_EXTRA_USERINFO          0x00001000
//#define GMF_IPV6_ADDRESS_AWARE      0x00002000
//
////===============================================================
//
//! Client Packet Entities:
typedef struct centity_s centity_t;
//! Client Clients Struct:
typedef struct cclient_s cclient_t;
//! Client Info Type(name, model, skin, etc):
typedef struct clientinfo_s clientinfo_t;

// Include needed shared refresh types.
#include "refresh/shared_types.h"

#ifndef CLGAME_INCLUDE
/**
*	Client-side 'client' structure definition: This structure always has to
*	mirror the 'first part' of the structure defined within the Client-Game.
**/
typedef struct cclient_s {
//    player_state_t  ps;
//    int32_t			ping;
//	/**
//	*	The game dll can add anything it wants after this point in the structure.
//	**/
	int32_t             clientNum;
} cclient_t;
/**
*	Client-side Packet Entity structure definition: This structure always has to 
*	mirror the 'first part' of the structure defined within the Client-Game.
**/
typedef struct centity_s {
	//! Current(and thus last acknowledged and received) entity state.
	entity_state_t	current;
	//! Previous entity state. Will always be valid, but might be just a copy of the current state.
	entity_state_t	prev;

	//! Modelspace Mins/Maxs of Bounding Box.
	vec3_t	mins, maxs;
	//! Worldspace absolute Mins/Maxs/Size of Bounding Box.
	vec3_t	absmin, absmax, size;
	
	//! The (last) serverframe this entity was in. If not current, this entity isn't in the received frame.
	int64_t	serverframe;

	//! For diminishing grenade trails.
	int32_t	trailcount;         // for diminishing grenade trails
	//! for trails (variable hz)
	vec3_t	lerp_origin;

	//! Used for CL_FlyEffect and CL_TrapParticles to determine when to stop the effect.
	int32_t	fly_stoptime;

	//! Entity id for the refresh(render) entity.
	int32_t	id;

	// WID: 40hz
	int32_t	current_frame, last_frame;
	// Server Time of receiving the current frame.
	int64_t	frame_servertime;
	// Server Time of receiving a (state.renderfx & SF_STAIR_STEP) entity.
	int64_t	step_servertime;
	// Actual height of the step taken.
	float	step_height;
	// WID: 40hz

	/**
	*	The game dll can add anything it wants after this point in the structure.
	**/
} centity_t;
#else
#include "client/client_types.h"
#endif      // CLGAME_INCLUDE

//===============================================================

/**
* 
* 
*	@brief	Functions and variable data that is provided by the main engine.
* 
* 
**/
typedef struct {
	/**
	*	Consts and Variables:
	**/
	//! Tick Rate in hz.
	uint32_t    tick_rate;
	//! Frametime in seconds.
	float       frame_time_s;
	//! Frametime in miliseconds.
	uint32_t    frame_time_ms;

	//! Client State. Shared with client game.
	struct client_state_s *client;


	/**
	*
	*	Client Static:
	* 
	**/
	// Demo:
	const qboolean ( *IsDemoPlayback )( );
	const uint64_t( *GetDemoFramesRead )( );
	const float( *GetDemoFileProgress )( );
	const int64_t( *GetDemoFileSize )( );
	// Time/State:
	const uint64_t ( *GetRealTime )( );
	const double ( *GetFrameTime )( );
	const int32_t ( *GetConnectionState )( );
	const ref_type_t( *GetRefreshType )( );
	// Actually from Sys_... :
	const uint64_t( *Sys_Milliseconds )( void );
	// FPS and Scale:
	const int32_t ( *GetClientFps )( void );
	const int32_t ( *GetRefreshFps )( void );
	const int32_t ( *GetResolutionScale )( void );
	// Netchan outgoing sequence, incoming acknowledged sequence.
	const int64_t( *Netchan_GetOutgoingSequence )( void );
	const int64_t( *Netchan_GetIncomingAcknowledged )( void );
	const int64_t( *Netchan_GetDropped )( void );

	/**
	*
	*	Clip Tracing:
	*
	**/
	const trace_t( *q_gameabi Trace )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const centity_t *passEntity, const contents_t contentmask );
	const trace_t( *q_gameabi Clip )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const centity_t *clipEntity, const contents_t contentmask );
	const contents_t( *q_gameabi PointContents )( const vec3_t point );



	/**
	*
	*	ConfigStrings:
	*
	**/
	configstring_t *( *GetConfigString )( const int32_t index );


	/**
	*
	*	Commands:
	*
	**/
	/**
	*	The functions that execute commands get their parameters with these
	*	functions. Cmd_Argv () will return an empty string, not a NULL
	*	if arg > argc, so string operations are always safe.
	**/
	const from_t  ( *Cmd_From )( void );
	const int32_t ( *Cmd_Argc )( void );
	char *( *Cmd_Argv )( const int32_t arg );
	char *( *Cmd_Args )( void );
	char *( *Cmd_RawArgs )( void );
	char *( *Cmd_ArgsFrom )( const int32_t from );
	char *( *Cmd_RawArgsFrom )( const int32_t from );
	char *( *Cmd_ArgsRange )( const int32_t from, const int32_t to );
	const size_t  ( *Cmd_ArgsBuffer )( char *buffer, const size_t size );
	const size_t  ( *Cmd_ArgvBuffer )( const int32_t arg, char *buffer, const size_t size );
	const int32_t ( *Cmd_ArgOffset )( const int32_t arg );
	const int32_t ( *Cmd_FindArgForOffset )( const int32_t offset );
	char *( *Cmd_RawString )( void ); // WID: C++20: Added const.
	void ( *Cmd_Shift )( void );

	//! Execute any possible triggers.
	void ( *Cmd_ExecTrigger )( const char *string );

	// These few functions attempt to find partial matching variable names for command line completetion.
	void ( *Cmd_Command_g )( genctx_t *ctx );
	void ( *Cmd_Alias_g )( genctx_t *ctx );
	void ( *Cmd_Macro_g )( genctx_t *ctx );
	void ( *Cmd_Config_g )( genctx_t *ctx );
	//! Find specified macro.
	cmd_macro_t *( *Cmd_FindMacro )( const char *name );

	/**
	*	@brief	Registers the specified function pointer as the 'name' command.
	**/
	void ( *Cmd_AddCommand )( const char *name, xcommand_t function );
	/**
	*	@brief	Registers the specified function pointer as the 'name' macro.
	**/
	void ( *Cmd_AddMacro )( const char *name, xmacro_t function );
	/**
	*	@brief	Removes the specified 'name' command registration.
	**/
	void ( *Cmd_RemoveCommand )( const char *name );
	/**
	*	@brief	Registers the specified function pointer as the 'name' command.
	**/
	void ( *Cmd_Register )( const cmdreg_t *reg );
	/**
	*	@brief	Deregisters the specified function pointer as the 'name' command.
	**/
	void ( *Cmd_Deregister )( const cmdreg_t *reg );



	/**
	*
	*	Console variable interaction:
	*
	**/
	cvar_t *( *CVar )( const char *var_name, const char *value, const int32_t flags );
	cvar_t *( *CVar_Get )( const char *var_name, const char *value, const int32_t flags );
	cvar_t *( *CVar_Set )( const char *var_name, const char *value );
	cvar_t *( *CVar_ForceSet )( const char *var_name, const char *value );
	void ( *CVar_SetInteger )( cvar_t *var, int value, from_t from );
	int ( *CVar_ClampInteger )( cvar_t *var, int min, int max );
	float ( *CVar_ClampValue )( cvar_t *var, float min, float max );
	void ( *CVar_Reset )( cvar_t *cvar );
	cvar_t * ( *CVar_WeakGet )( const char *var_name );
	//! Attempts to match a partial variable name for command line completion
	//! Returns NULL if nothing fits.
	void ( *CVar_Variable_g )( genctx_t *ctx );
	void ( *CVar_Default_g )( genctx_t *ctx );
	


	/**
	*
	*	Console:
	* 
	**/
	void ( *Con_ClearNotify_f )( void );


	
	/**
	*
	*	Collision Model (BSP):
	* 
	**/
	/**
	*   @return A pointer to nullleaf if there is no BSP model cached up, or the number is out of bounds.
	*           A pointer to the 'special case for solid leaf' if number == -1
	*           A pointer to the BSP Node that matched with 'number'.
	**/
	mnode_t *( *CM_NodeForNumber )( cm_t *cm, const int32_t number );
	/**
	*   @return The number that matched the node's pointer. -1 if node was a nullptr.
	**/
	const int32_t( *CM_NumberForNode )( cm_t *cm, mnode_t *node );
	/**
	*   @return A pointer to nullleaf if there is no BSP model cached up.
	*           A pointer to the BSP Leaf that matched with 'number'.
	**/
	mleaf_t *( *CM_LeafForNumber )( cm_t *cm, const int32_t number );

	/**
	*   @return The number that matched the leaf's pointer. 0 if leaf was a nullptr.
	**/
	const int32_t( *CM_NumberForLeaf )( cm_t *cm, mleaf_t *leaf );
	/**
	*   @Return True if any leaf under headnode has a cluster that
	*           is potentially visible
	**/
	const qboolean( *CM_HeadnodeVisible )( mnode_t *headnode, byte *visbits );

	/**
	*   @brief  Set the portal nums matching portal to open/closed state.
	**/
	void ( *CM_SetAreaPortalState )( cm_t *cm, const int32_t portalnum, const bool open );
	/**
	*   @return False(0) if the portal nums matching portal is closed, true(1) otherwise.
	**/
	const int32_t ( *CM_GetAreaPortalState )( cm_t *cm, const int32_t portalnum );
	/**
	*   @return True if the two areas are connected, false if not(or possibly blocked by a door for example.)
	**/
	const bool  ( *CM_AreasConnected )( cm_t *cm, const int32_t area1, const int32_t area2 );

	/**
	*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
	*           given box occupies in the data structure.
	**/
	const int32_t ( *CM_BoxLeafs )( cm_t *cm, const vec3_t mins, const vec3_t maxs,	mleaf_t **list, const int32_t listsize, mnode_t **topnode );
	/**
	*   @brief  Populates the list of leafs which the specified bounding box touches. If top_node is not
	*           set to NULL, it will contain a value copy of the the top node of the BSP tree that fully
	*           contains the box.
	**/
	const int32_t ( *CM_BoxLeafs_headnode )( cm_t *cm, const vec3_t mins, const vec3_t maxs, mleaf_t **list, int listsize, mnode_t *headnode, mnode_t **topnode );
	/**
	*   @return The contents mask of all leafs within the absolute bounds.
	**/
	const contents_t( *CM_BoxContents )( cm_t *cm, const vec3_t mins, const vec3_t maxs, mnode_t *headnode );


	/**
	*
	*	(Collision Model-) Entities:
	*
	**/
	/**
	*   @brief  Looks up the key/value cm_entity_t pair in the list for the cm_entity_t entity.
	*   @return If found, a pointer to the key/value pair, otherwise a pointer to the 'cm_null_entity'.
	**/
	const cm_entity_t *( *CM_EntityKeyValue )( const cm_entity_t *edict, const char *key );
	/**
	*   @brief  Used to check whether CM_EntityValue was able/unable to find a matching key in the cm_entity_t.
	*   @return Pointer to the collision model system's 'null' entity key/pair.
	**/
	const cm_entity_t *( *CM_GetNullEntity )( void );



	/**
	*
	*	FileSystem:
	*
	*
	**/
	/**
	*	@brief	Returns non 0 in case of existance.
	**/
	const int32_t ( *FS_FileExistsEx )( const char *path, const uint32_t flags );
	/**
	*	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
	*	@return	length < 0 indicates error.
	**/
	const int32_t ( *FS_LoadFile )( const char *path, void **buffer );


	/**
	*
	*	KeyButtons/KeyEvents:
	*
	**/
	//! Register a button as being 'held down'.
	void (* KeyDown )( keybutton_t *keyButton );
	//! Register a button as being 'released'.
	void ( *KeyUp )( keybutton_t *keyButton );
	//! Clear out a key's down state and msec, but maintain track of whether it is 'held'.
	void ( *KeyClear )( keybutton_t *keyButton );
	//! Returns the fraction of the command frame's interval for which the key was 'down'.
	const double ( *KeyState )( keybutton_t *keyButton );
	//! Returns total numbers of keys that are 'down'.
	const int32_t ( *Key_AnyKeyDown )( void );
	//! Returns key down status : if > 1, it is auto - repeating
	const int32_t ( *Key_IsDown )( const int32_t key );
	//!
	const char *( *Key_GetBinding )( const char *binding );

	//! Set the 'layer' of where key events are handled by.
	void ( *SetKeyEventDestination )( const keydest_t keyEventDestination );
	//! Returns the 'layer' of where key events are handled by.
	const keydest_t ( *GetKeyEventDestination )( void );



	/**
	*
	*	Network Messaging:
	*
	**/
	/**
	*   @return Signed 8 bit byte. Returns -1 if no more characters are available
	**/
	const int32_t( *MSG_ReadInt8 )( void );
	/**
	*   @return Unsigned 8 bit byte. Returns -1 if no more characters are available
	**/
	const int32_t( *MSG_ReadUint8 )( void );
	/**
	*   @return Signed 16 bit short.
	**/
	const int32_t( *MSG_ReadInt16 )( void );
	/**
	*   @return Unsigned 16 bit short.
	**/
	const int32_t( *MSG_ReadUint16 )( void );
	/**
	*   @return Signed 32 bit int.
	**/
	const int32_t( *MSG_ReadInt32 )( void );
	/**
	*   @return Signed 64 bit int.
	**/
	const int64_t( *MSG_ReadInt64 )( void );
	/**
	*   @return UnSigned 64 bit int.
	**/
	const uint64_t( *MSG_ReadUint64 )( void );
	/**
	*   @return Base 128 decoded unsigned integer.
	**/
	const uint64_t( *MSG_ReadUintBase128 )();
	/**
	*   @return Zig-Zac decoded signed integer.
	**/
	const int64_t( *MSG_ReadIntBase128 )();
	/**
	*   @return The full precision float.
	**/
	const float ( *MSG_ReadFloat )();
	/**
	*   @return A half float, converted to float, keep in mind that half floats have less precision.
	**/
	const float ( *MSG_ReadHalfFloat )();

	/**
	*   @return The full string until its end.
	**/
	const size_t( *MSG_ReadString )( char *dest, const size_t size );
	/**
	*   @return The part of the string data up till the first '\n'
	**/
	const size_t( *MSG_ReadStringLine )( char *dest, const size_t size );

	/**
	*	@brief Reads a byte and decodes it to a float angle.
	**/
	const float ( *MSG_ReadAngle8 )( void );
	/**
	*	@brief Reads a short and decodes it to a float angle.
	**/
	const float ( *MSG_ReadAngle16 )( void );
	/**
	*	@brief	Reads a short and decodes its 'half float' into a float angle.
	**/
	const float ( *MSG_ReadAngleHalfFloat )( void );
	/**
	*	@brief	Reads a byte, and decodes it using it as an index into our directions table.
	**/
	void ( *MSG_ReadDir8 )( vec3_t dir );
	/**
	*	@return The read positional coordinate. Optionally from 'short' to float. (Limiting in the range of -4096/+4096
	**/
	void ( *MSG_ReadPos )( vec3_t pos, const msgPositionEncoding_t encoding );



	/**
	*
	*	Printing:
	*
	**/
	void ( *q_printf( 2, 3 ) Print )( print_type_t printlevel, const char *fmt, ... );
	void ( *q_noreturn q_printf( 1, 2 ) Error )( const char *fmt, ... );



	/**
	*
	*	Refresh:
	*
	**/
	//! Register a model for the current load sequence.
	const qhandle_t( *R_RegisterModel )( const char *name );
	//! Register a skin for the current load sequence.
	const qhandle_t( *R_RegisterSkin )( const char *name );
	//! Register an image for the current load sequence.
	const qhandle_t ( *R_RegisterImage )( const char *name, imagetype_t type, imageflags_t flags );
	
	//! Wrapper around R_RegisterImage: Register a font for the current load sequence.
	const qhandle_t( *R_RegisterFont )( const char *name );
	//! Wrapper around R_RegisterImage: Register a 2D 'Picture' for the current load sequence.
	const qhandle_t( *R_RegisterPic )( const char *name );
	//! Wrapper around R_RegisterImage: Register a 2D 'Picture' that is permanently laoded until application shutdown.
	const qhandle_t( *R_RegisterPic2 )( const char *name );

	//!
	void ( *R_ClearColor )( void );
	//!
	void ( *R_SetAlpha )( const float alpha );
	//!
	void ( *R_SetAlphaScale )( const float alphaScale );
	//!
	void ( *R_SetColor )( const uint32_t color );
	//!
	void ( *R_SetClipRect )( const clipRect_t *clipRect );
	//!
	const float ( *R_ClampScale )( cvar_t *var );
	//!
	void ( *R_SetScale )( const float scale );
	//!
	void ( *R_DrawChar )( const int32_t x, const int32_t y, const int32_t flags, const int32_t ch, const qhandle_t font );
	//!
	const int32_t ( *R_DrawString )( const int32_t x, const int32_t y, const int32_t flags, const size_t maxChars, const char *str, const qhandle_t font );
	//! Returns transparency bit
	const qboolean ( *R_GetPicSize )( int32_t *w, int32_t *h, const qhandle_t pic );
	//!
	void ( *R_DrawPic )( const int32_t x, const int32_t y, const qhandle_t pic );
	//!
	void ( *R_DrawStretchPic )( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic );
	//!
	void ( *R_DrawRotateStretchPic )( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const float angle, const int32_t pivot_x, const int32_t pivot_y, const qhandle_t pic );
	//!
	void ( *R_DrawKeepAspectPic )( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic );
	//!
	void ( *R_DrawStretchRaw )( const int32_t x, const int32_t y, const int32_t w, const int32_t h );
	//!
	void ( *R_TileClear )( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic );
	//!
	void ( *R_DrawFill8 )( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const int32_t c );
	//!
	void ( *R_DrawFill32 )( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const uint32_t color );
	//!
	void ( *R_UpdateRawPic )( const int32_t pic_w, const int32_t pic_h, uint32_t *pic );
	//!
	void ( *R_DiscardRawPic )( void );

	//! Unimplemented in VKPT, but, adds a decal.
	void ( *R_AddDecal )( decal_t *d );
	
	// Very cheesy, but we need access to it however
	const uint32_t *( *R_Get8BitTo24BitTable )( void );



	/**
	*
	*	Screen:
	*
	**/
	const char *( *SCR_GetColorName )( color_index_t colorIndex );
	const qboolean( *SCR_ParseColor )( const char *s, color_t *color );



	/**
	*
	*	Sound:
	*
	**/
	void ( *S_StartSound )( const vec3_t origin, const int32_t entnum, const int32_t entchannel, const qhandle_t hSfx, const float vol, const float attenuation, const float timeofs );
	void ( *S_StartLocalSound )( const char *s );
	void ( *S_StartLocalSoundOnce )( const char *s );
	void ( *S_StopAllSounds )( void );
	qhandle_t( *S_RegisterSound )( const char *sample );
	/**
	*   @brief  Registers a reverb effect, returning a qhandle, which is -1 on failure, >= 0 otherwise.
	**/
	void ( *S_SetupSpatialListener )( const vec3_t viewOrigin, const vec3_t vForward, const vec3_t vRight, const vec3_t vUp );
	const qboolean ( *S_SetEAXEnvironmentProperties )( const sfx_eax_properties_t *properties );

	/**
	*
	*	'Tag' Managed Memory Allocation:
	*
	**/
	void *( *TagMalloc )( unsigned size, unsigned tag );
	void ( *TagFree )( void *block );
	void ( *FreeTags )( unsigned tag );



	/**
	*
	*	(Client-)View Scene
	*
	**/
	void ( *V_RenderView )( void );
	/**
	*   @brief  Calculate the client's PVS which is a necessity for culling out
	*           local client entities.
	**/
	void ( *V_CalculateLocalPVS )( const vec3_t viewOrigin );
	void ( *V_AddEntity )( entity_t *ent );
	void ( *V_AddParticle )( particle_t *p );
	void ( *V_AddSphereLight )( const vec3_t org, float intensity, float r, float g, float b, float radius );
	void ( *V_AddSpotLight )( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle );
	void ( *V_AddSpotLightTexEmission )( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex );
	void ( *V_AddLight )( const vec3_t org, float intensity, float r, float g, float b );
	void ( *V_AddLightStyle )( int style, float value );



	/**
	*
	*	Zone Memory Allocator:
	*
	**/
	void ( *Z_Free )( void *ptr );
	// Frees the memory block pointed at by (*ptr), if that's nonzero, and sets (*ptr) to zero.
	void ( *Z_Freep )( void **ptr );
	void *( *Z_Realloc )( void *ptr, size_t size );
	void *( *Z_Malloc )( size_t size ) q_malloc;



	/**
	*
	*	Command Prompt
	*
	**/
	void ( *Prompt_AddMatch )( genctx_t *ctx, const char *s );



	/**
	*
	*	Client Command Parameter Access:
	*
	**/
    //int (*argc)(void);
    //char *(*argv)(int n);
    //char *(*args)(void);     // concatenation of all argv >= 1

	///**
    //*	Add commands to the server console as if they were typed in
    //*	for map changing, etc
	//**/
    //void (*AddCommandString)(const char *text);

    //void (*DebugGraph)(float value, int color);



	/**
	*
	*	Debugging Utilities:
	*
	**/
	void ( *ShowNet )( const int32_t level, const char *fmt, ... );
	void ( *ShowClamp )( const int32_t level, const char *fmt, ... );
	void ( *ShowMiss )( const char *fmt, ... );



	/**
	*
	*	Other:
	*
	**/
	/**
	*	@brief	Scans the ignore list for the given nickname.
	*	@return	True if the nickname is found in the ignore list.
	**/
	const qboolean ( *CheckForIgnore )( const char *nickname );
	/**
	*	@brief	Attempt to scan out an IP address in dotted-quad notation and
	*			add it into circular array of recent addresses.
	**/
	void ( *CheckForIP )( const char *str );
	/**
	*	@brief	
	**/
	void ( *CheckForVersion )( const char *str );
	/**
	*	@brief
	**/
	void ( *Con_SkipNotify )( const qboolean skip );

	/**
	*	@brief
	**/
	void ( *SetSpriteModelVerticality )( const qhandle_t spriteHandle );
	/**
	*	@brief
	**/
	const int32_t( *GetSpriteModelFrameCount )( const qhandle_t spriteHandle );
	/**
	*	@brief
	**/
	const qboolean ( *IsValidSpriteModelHandle )( const qhandle_t spriteHandle );
} clgame_import_t;

/**
*	Functions and Variables Exported by the ClientGame subsystem:
**/
typedef struct {
    int         apiversion;

	/**
	*	Init/Shutdown:
	*
	*	TODO: Is this following still correct for ClientGame?
	* 
	*	The init function will only be called when a game starts,
	*	not each time a level is loaded.  Persistant data for clients
	*	and the server can be allocated in init
	**/
	//! Called during client initialization.
	void ( *PreInit ) ( void );
	//! Called during client initialization.
	void ( *Init )( void );
	//! Called when the client is about to shutdown, giving us a last minute shot at accessing possible required data.
	void ( *PreShutdown )( void );
	//! Called after the client has shutdown.
	void ( *Shutdown )( void );



	/**
	* 
	*	Connecting and State:
	* 
	**/
	/**
	*	@brief	Called when the client wants to 'clear state', this happens during Disconnecting and when 
	*			the first server data message, an svc_serverdata(ParsingServerData) event is received.
	*			
	*			Used to clear any state data that should not persist over multiple server connections.
	**/
	void ( *ClearState ) ( void );
	/**
	*	@brief	Called when the client state has moved into being active and the game begins.
	**/
	void ( *ClientBegin ) ( void );
	/**
	*	@brief	Called when the client state has moved into being properly connected to server.
	**/
	void ( *ClientConnected ) ( void );
	/**
	*	@brief	Called when the client state has moved into a disconnected state, before ending
	*			the loading plague and starting to clear its state. (So it is still accessible.)
	**/
	void ( *ClientDisconnected ) ( void );
	/**
	*	@brief	Called to update the client's local game entities, it runs at the same framerate
	*			as the server game logic does.
	**/
	void ( *ClientLocalFrame ) ( void );
	/**
	*	@brief	Called at the rate equal to that of the refresh frames.
	**/
	void ( *ClientRefreshFrame ) ( void );


	/**
	*
	*	Entities:
	*
	**/
	/**
	*	@brief	Called from the client's Begin command, gives the client game a shot at
	*			spawning local client entities so they can join in the image/model/sound
	*			precaching context.
	**/
	void ( *SpawnEntities )( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );
	/**
	*   @brief  The sound code makes callbacks to the client for entitiy position
	*           information, so entities can be dynamically re-spatialized.
	**/
	void ( *GetEntitySoundOrigin )( const int32_t entityNumber, vec3_t origin );
	/**
	*	@brief	Returns a qhandle_t to the loaded EAX reverb effect profile resource.
	**/
	qhandle_t ( *GetEAXBySoundOrigin )( const int32_t entityNumber, vec3_t origin );
	/**
	*	@brief	Parsess entity events.
	**/
	void ( *ParseEntityEvent )( const int32_t entityNumber );



	/**
	* 
	*	GameModes:
	* 
	**/
	//! Returns the string name of specified game mode ID.
	const char *( *GetGamemodeName )( const int32_t gameModeID );



	/**
	*
	*	Player Movement:
	*
	**/
	//! Will shuffle current viewheight into previous before updating the current viewheight, and record the time of changing.
	void ( *AdjustViewHeight )( const int32_t viewHeight );
	//! Returns false if cl_predict == 0, or player move inquired to perform no prediction.
	const qboolean( *UsePrediction )( void );
	/**
	*   @brief  Checks for prediction if desired. Will determine the error margin
	*           between our predicted state and the server returned state. In case
	*           the margin is too high, snap back to server provided player state.
	**/
	void ( *CheckPredictionError )( const int64_t frameIndex, const uint64_t commandIndex, const pmove_state_t *in, struct client_movecmd_s *moveCommand, struct client_predicted_state_s *out );
	//! Sets the predicted view angles.
	void ( *PredictAngles )( void );
	/**
	*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
	*           as the pending user move command. To finally store the predicted outcome
	*           into the cl.predictedState struct.
	**/
	void ( *PredictMovement )( uint64_t acknowledgedCommandNumber, const uint64_t currentCommandNumber );
	//! Setup the basic player move configuration parameters. (Used by server for new clients.)
	void ( *ConfigurePlayerMoveParameters )( pmoveParams_t *pmp );



	/**
	*
	*	Precache:
	*
	**/
	/**
	*	@brief	Called right before loading all received configstring (server-) models.
	**/
	void ( *PrecacheClientModels )( void );
	/**
	*	@brief	Called right before loading all received configstring (server-) sounds.
	**/
	void ( *PrecacheClientSounds )( void );
	/**
	*   @brief  Called to precache/update precache of 'View'-models. (Mainly, weapons.)
	**/
	void ( *PrecacheViewModels )( void );
	/**
	*	@brief	Called to precache client info specific media.
	**/
	void ( *PrecacheClientInfo )( clientinfo_t *ci, const char *s );

	/**
	*	@brief	Used for the client in a scenario where it might have to download view models.
	*	@return	The number of precached view models.
	**/
	const uint32_t ( *GetNumberOfViewModels )( void );
	/**
	*	@brief	Used for the client in a scenario where it might have to download view models.
	*	@return	The filename of the precached view model that has the matching index.
	**/
	const char *( *GetViewModelFilename )( const uint32_t index );



	/**
	*
	*	Screen:
	*
	**/
	/**
	*	@brief
	**/
	void ( *ScreenInit )( void );
	void ( *ScreenShutdown )( void );
	void ( *ScreenRegisterMedia )( void );
	void ( *ScreenModeChanged )( void );
	void ( *ScreenSetCrosshairColor )( void );
	vrect_t *( *GetScreenVideoRect )( void );

	/**
	*	@brief	Prepare and draw the current 'active' state's 2D and 3D views.
	**/
	void ( *DrawActiveState )( refcfg_t *refcfg );
	/**
	*	@brief	Prepare and draw the loading screen's 2D state.
	**/
	void ( *DrawLoadState )( refcfg_t *refcfg );
	
	/**
	*   @return The current active handle to the font image. (Used by ref/vkpt.)
	**/
	const qhandle_t( *GetScreenFontHandle )( void );
	/**
	*   @brief  Set the alpha value of the HUD. (Used by ref/vkpt.)
	**/
	void ( *SetScreenHUDAlpha )( const float alpha );



	/**
	*
	*	Server Messages:
	*
	**/
	/**
	*	@brief	Gives the client game a chance to interscept and respond to configstring updates.
	*			Returns true if interscepted.
	**/
	const qboolean ( *UpdateConfigString )( const int32_t index );

	/**
	*	@brief	Called by the client BEFORE all server messages have been parsed.
	**/
	void ( *StartServerMessage )( const qboolean isDemoPlayback );
	/**
	*	@brief	Called by the client AFTER all server messages have been parsed.
	**/
	void ( *EndServerMessage )( const qboolean isDemoPlayback );
	/**
	*	@brief	Called by the client when it does not recognize the server message itself,
	*			so it gives the client game a chance to handle and respond to it.
	*	@return	True if the message was handled properly. False otherwise.
	**/
	const qboolean ( *ParseServerMessage )( const int32_t serverMessage );
	/**
	*	@brief	A variant of ParseServerMessage that skips over non-important action messages,
	*			used for seeking in demos.
	*	@return	True if the message was handled properly. False otherwise.
	**/
	const qboolean ( *SeekDemoMessage )( const int32_t serverMessage );
	/**
	*	@brief	Parses a clientinfo configstring.
	**/
	void( *ParsePlayerSkin )( char *name, char *model, char *skin, const char *s );


	/**
	*
	*	User Input:
	*
	**/
	//! Called upon when mouse movement is detected.
	void ( *MouseMove )( const float deltaX, const float deltaY, const float moveX, const float moveY, const float speed );
	//! Called upon to register user input keybuttons.
	void ( *RegisterUserInput )( void );
	//!
	void ( *UpdateMoveCommand )( const int64_t msec, struct client_movecmd_s *moveCommand, struct client_mouse_motion_s *mouseMotion );
	//!
	void ( *FinalizeMoveCommand )( struct client_movecmd_s *moveCommand );
	//!
	void ( *ClearMoveCommand )( struct client_movecmd_s *moveCommand );



	/**
	*
	*	(Client-)View Scene:
	*
	**/
	/**
	*   @brief  Calculates the client's field of view.
	**/
	const float ( *CalculateFieldOfView )( const float fov_x, const float width, const float height );
	/**
	*   @brief  Sets cl.refdef view values and sound spatialization params.
	*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
	*           loop if rendering is disabled but sound is running.
	**/
	void ( *CalculateViewValues )( void );
	/**
	*	@brief	
	**/
	void ( *ClearViewScene )( void );
	/**
	*   @brief  Prepares the current frame's view scene for the refdef by
	*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
	*           and temp entities) to the refresh definition.
	**/
	void ( *PrepareViewEntities )( void );



	/**
	*	Global variables shared between game and client.
	*
	*	The entities array is allocated in the client game dll so it
	*	can vary in size from one game to another.
	*
	*	The size will be fixed when clge->Init() is called
	**/
	struct centity_s *entities;
	int32_t	entity_size;
	int32_t	num_entities;     // current number, <= max_entities
	int32_t	max_entities;
} clgame_export_t;

#ifdef __cplusplus
}
#endif