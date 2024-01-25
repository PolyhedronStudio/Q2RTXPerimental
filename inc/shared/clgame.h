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
#endif      // SVGAME_INCLUDE

//===============================================================

/**
*	@brief	Functions and variable data that is provided by the main engine.
**/
typedef struct {
	/**
	*	Consts and Variables:
	**/
	uint32_t    tick_rate;
	float       frame_time_s;
	uint32_t    frame_time_ms;


	/**
	*
	*	ConfigStrings:
	*
	*
	**/

	configstring_t *( *GetConfigString )( const int32_t index );

	/**
	*
	*	Console variable interaction:
	*
	**/
	cvar_t *( *CVar )( const char *var_name, const char *value, int flags );
	cvar_t *( *CVar_Set )( const char *var_name, const char *value );
	cvar_t *( *CVar_ForceSet )( const char *var_name, const char *value );

	/**
	*
	*	Printing:
	*
	**/
	void ( *q_printf( 2, 3 ) Print )( print_type_t printlevel, const char *fmt, ... );
	void ( *q_noreturn q_printf( 1, 2 ) Error )( const char *fmt, ... );

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
	*	Network Messaging:
	*
	**/

	
	/**
	*
	*	ClientCommand Parameter Access:
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
	void ( *Init )( void );
	//! Called during client shutdown.
	void ( *Shutdown )( void );

	/**
	*	Connecting and State:
	**/
	//! Called when the client wants to 'clear state', this happens during Disconnecting and when 
	//! the first server data message, an svc_serverdata(ParsingServerData) event is received..
	void ( *ClearState ) ( void );
	//! Called when the client state has moved into being properly connected to server.
	void ( *ClientConnected ) ( void );

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
	//! Perform a frame's worth of player movement using specified pmoveParams configuration.
	void ( *PlayerMove )( pmove_t *pmove, pmoveParams_t *params );
	//! Setup the basic player move configuration parameters. (Used by server for new clients.)
	void ( *ConfigurePlayerMoveParameters )( pmoveParams_t *pmp );

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
	int32_t	num_entities;     // current number, <= max_edicts
	int32_t	max_entities;
} clgame_export_t;

#ifdef __cplusplus
}
#endif