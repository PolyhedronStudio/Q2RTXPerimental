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
//    SOLID_BBOX,         // touch on edge
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
//#define MAX_ENT_CLUSTERS    16


typedef struct centity_s centity_t;
//typedef struct gclient_s gclient_t;

// Include needed shared refresh types.
#include "refresh/shared_types.h"

#ifndef CLGAME_INCLUDE
//
//struct gclient_s {
//    player_state_t  ps;     // communicated by server to clients
//    int             ping;
//
//    // the game dll can add anything it wants after
//    // this point in the structure
//    int             clientNum;
//};
//
//
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

//    vec3_t      absmin, absmax, size;
//    solid_t     solid;
//    int         clipmask;
//    edict_t     *owner;
	// The game dll can add anything it wants after this point in the structure.
} centity_t;
//struct edict_s {
//    entity_state_t  s;
//    struct gclient_s    *client;
//    qboolean    inuse;
//    int         linkcount;
//
//    // FIXME: move these fields to a server private sv_entity_t
//    list_t      area;               // linked to a division node or leaf
//
//    int         num_clusters;       // if -1, use headnode instead
//    int         clusternums[MAX_ENT_CLUSTERS];
//    int         headnode;           // unused if num_clusters != -1
//    int         areanum, areanum2;
//
//    //================================
//
//    int         svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
//    vec3_t      mins, maxs;
//    vec3_t      absmin, absmax, size;
//    solid_t     solid;
//    int         clipmask;
//    edict_t     *owner;
//
//    // the game dll can add anything it wants after
//    // this point in the structure
//};
//
#else
#include "client/client_types.h"
#endif      // CLGAME_INCLUDE

//===============================================================

/**
*	@brief	Functions and variable data that is provided by the main engine.
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
	const qboolean ( *IsDemoPlayback )( );
	const uint64_t ( *GetRealTime )( );
	const int32_t ( *GetConnectionState )( );
	const ref_type_t( *GetRefreshType )( );



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
	*	Console variable interaction:
	*
	**/
	cvar_t *( *CVar )( const char *var_name, const char *value, const int32_t flags );
	cvar_t *( *CVar_Get )( const char *var_name, const char *value, const int32_t flags );
	cvar_t *( *CVar_Set )( const char *var_name, const char *value );
	cvar_t *( *CVar_ForceSet )( const char *var_name, const char *value );
	float ( *CVar_ClampValue )( cvar_t *var, float min, float max );
	void ( *CVar_Reset )( cvar_t *cvar );



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
	void ( *MSG_ReadPos )( vec3_t pos, const qboolean decodeFromShort );



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
	//! Register a model.
	const qhandle_t( *R_RegisterModel )( const char *name );
	//! Register a skin.
	const qhandle_t( *R_RegisterSkin )( const char *name );

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
	void ( *S_SetupSpatialListener )( const vec3_t viewOrigin, const vec3_t vForward, const vec3_t vRight, const vec3_t vUp );


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
	void ( *V_AddEntity )( entity_t *ent );
	void ( *V_AddParticle )( particle_t *p );
	void ( *V_AddSphereLight )( const vec3_t org, float intensity, float r, float g, float b, float radius );
	void ( *V_AddSpotLight )( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle );
	void ( *V_AddSpotLightTexEmission )( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex );
	void ( *V_AddLight )( const vec3_t org, float intensity, float r, float g, float b );
	void ( *V_AddLightStyle )( int style, float value );



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
	void ( *SetSpriteModelVerticality )( const qhandle_t spriteHandle );
	const int32_t( *GetSpriteModelFrameCount )( const qhandle_t spriteHandle );
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
	//! Called during client shutdown.
	void ( *Shutdown )( void );



	/**
	*	Connecting and State:
	**/
	//! Called when the client wants to 'clear state', this happens during Disconnecting and when 
	//! the first server data message, an svc_serverdata(ParsingServerData) event is received..
	void ( *ClearState ) ( void );
	//! Called when the client state has moved into being active and the game begins.
	void ( *ClientBegin ) ( void );
	//! Called when the client state has moved into being properly connected to server.
	void ( *ClientConnected ) ( void );
	//! Called when the client state has moved into a disconnected state. Before ending
	//! the loading plague and starting to clear its state. (So it is still accessible.)
	void ( *ClientDisconnected ) ( void );



	/**
	*	GameModes:
	**/
	//! Returns the string name of specified game mode ID.
	const char *( *GetGamemodeName )( const int32_t gameModeID );



	/**
	*
	*	Player Movement:
	*
	**/
	//! Returns false if cl_predict == 0, or player move inquired to perform no prediction.
	const qboolean ( *UsePrediction )( void );
	//! Will shuffle current viewheight into previous before updating the current viewheight, and record the time of changing.
	void ( *AdjustViewHeight )( const int32_t viewHeight );
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
	void ( *RegisterTEntModels )( void );
	void ( *RegisterTEntSounds )( void );



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
	*	@brief	Parsess entity events.
	**/
	void ( *ParseEntityEvent )( const int32_t entityNumber );



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