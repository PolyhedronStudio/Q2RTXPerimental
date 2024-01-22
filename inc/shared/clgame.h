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


typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;


//#ifndef SVGAME_INCLUDE
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
//#endif      // SVGAME_INCLUDE

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
	// Called during client initialization.
	void ( *Init )( void );
	// Called during client shutdown.
	void ( *Shutdown )( void );

	/**
	*	GameModes:
	**/
	const char *( *GetGamemodeName )( const int32_t gameModeID );

	/**
	*
	*	Player Movement:
	*
	**/
	void ( *PlayerMove )( pmove_t *pmove, pmoveParams_t *params );
	void ( *ConfigurePlayerMoveParameters )( pmoveParams_t *pmp );

} clgame_export_t;

#ifdef __cplusplus
}
#endif