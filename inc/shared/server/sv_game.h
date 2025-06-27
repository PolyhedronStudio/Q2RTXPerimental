/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#pragma once


// Include needed shared refresh types.
// ( We need some data so we can partially get and process animation data. )
#include "refresh/shared_types.h"

//
// sv_game.h -- server game dll information visible to server
//

#define SVGAME_API_VERSION    1337

// edict->svflags

// Old Vanilla Q2 flags.
//#define SVF_NOCLIENT            0x00000001  // Don't send entity to clients, even if it has effects
//#define SVF_DEADMONSTER         0x00000002  // Treat as CONTENTS_DEADMONSTER for collision
//#define SVF_MONSTER             0x00000004  // Treat as CONTENTS_MONSTER for collision
//#define SVF_HULL	0x00000008	// When touching the trigger's bounding box, perform an additional clip to trigger brush. (Used for G_TouchTriggers)

// Some [KEX] flags.
#define SVF_NONE            0           // No serverflags.
#define SVF_NOCLIENT        BIT( 0 )    // Don't send entity to clients, even if it has effects.
#define SVF_DEADMONSTER     BIT( 1 )    // Treat as CONTENTS_DEADMONSTER for collision.
#define SVF_MONSTER         BIT( 2 )    // Treat as CONTENTS_MONSTER for collision.
#define SVF_PLAYER          BIT( 3 )    // [Paril-KEX] Treat as CONTENTS_PLAYER for collision.
//#define SVF_BOT           BIT( 4 )    // Entity is controlled by a bot AI.
//#define SVF_NOBOTS        BIT( 5 )    // Don't allow bots to use/interact with entity.
//#define SVF_RESPAWNING    BIT( 6 )    // Entity will respawn on it's next think.
#define SVF_PROJECTILE      BIT( 7 )    // Treat as CONTENTS_PROJECTILE for collision.
//#define SVF_INSTANCED     BIT( 8 )    // Entity has different visibility per player.
#define SVF_DOOR            BIT( 9 )    // Entity is a door of some kind.
//#define SVF_NOCULL        BIT( 10 )   // Always send, even if we normally wouldn't.
#define SVF_HULL            BIT( 11 )   // Always use hull when appropriate (triggers, etc; for gi.clip).

// extended features

#define GMF_CLIENTNUM               0x00000001 //! Client number for the edict, used for client->clientNum.
#define GMF_PROPERINUSE             0x00000002 //! Proper inuse check for edicts.
//#define GMF_MVDSPEC               0x00000004
#define GMF_WANT_ALL_DISCONNECTS    0x00000008 //!! Want all disconnects to be sent to the client.

//#define GMF_ENHANCED_SAVEGAMES    0x00000400
//#define GMF_VARIABLE_FPS          0x00000800
#define GMF_EXTRA_USERINFO          0x00001000 //! Extra userinfo for the client, like the 'cl_guid' and 'cl_steamid'.
#define GMF_IPV6_ADDRESS_AWARE      0x00002000 //! Whether the server is aware of IPv6 addresses and can handle them properly.

//=============================================================================================
//=============================================================================================
// Forward declarations of types used in the Server Game dll.
struct edict_t;
struct svg_client_t;

//=============================================================================================
//=============================================================================================

#ifndef SVGAME_INCLUDE
    struct svg_client_t {
        player_state_t  ps;     // communicated by server to clients
        player_state_t  ops;    // old player state from the previous frame.
        int64_t         ping;

        // the game dll can add anything it wants after
        // this point in the structure
        int32_t         clientNum;
    };
    //struct edict_t {
    //    entity_state_t  s;
    //    svg_client_t *client;   //! NULL if not a player the server expects the first part
    //                                //! of gclient_s to be a player_state_t but the rest of it is opaque
    //    qboolean inuse;
    //    int32_t linkcount;

    //    // FIXME: move these fields to a server private sv_entity_t
    //    list_t area;    //! Linked to a division node or leaf

    //    int32_t num_clusters;       // If -1, use headnode instead.
    //    int32_t clusternums[MAX_ENT_CLUSTERS];
    //    int32_t headnode;           // Unused if num_clusters != -1
    //
    //    int32_t areanum, areanum2;

    //    //================================

    //    int32_t     svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    //    vec3_t      mins, maxs;
    //    vec3_t      absmin, absmax, size;
    //    cm_solid_t     solid;
    //    cm_contents_t  clipmask;
    //    cm_contents_t  hullContents;
    //    edict_t *owner;

    //    const cm_entity_t *entityDictionary;

    //    // the game dll can add anything it wants after
    //    // this point in the structure
    //};

    struct sv_edict_t : public sv_shared_edict_t<sv_edict_t, svg_client_t> { };
    typedef sv_edict_t edict_ptr_t;
#else
    struct svg_base_edict_t;
    typedef struct svg_base_edict_t edict_ptr_t;
    //struct svg_base_client_t;
	//typedef struct svg_base_client_t client_ptr_t;
#endif      // SVGAME_INCLUDE




//=============================================================================================
//=============================================================================================
/**
*   @brief  Interface to be implemented by the ServerGame which is a
*           Memory Pool for game allocated EDICTS.
**/
struct sv_edict_pool_i {
    //! Don't allow instancing of this 'interface'.
    sv_edict_pool_i() = default;
    //! Virtual destructor.
    virtual ~sv_edict_pool_i() = default;

    //! For accessing as if it were a regular edicts array.
    virtual edict_ptr_t *operator[]( const size_t index ) = 0;

    //! Gets the edict ptr for the matching number's slot.
    virtual edict_ptr_t *EdictForNumber( const int32_t number ) = 0;
    //! Gets the number for the matching edict ptr.
    virtual const int32_t NumberForEdict( const edict_ptr_t *edict ) = 0;

    //! Pointer to edicts data array.
    edict_ptr_t **edicts = nullptr;
    //! Size of edict type.
    //int32_t         edict_size;

    //! Number of active edicts.
    int32_t num_edicts = 0;     // current number, <= max_edicts
    //! Maximum edicts.
    int32_t max_edicts = 0;
};

//=============================================================================================
//=============================================================================================

/**
*	@brief	Functions and variable data that is provided by the main engine.
**/
typedef struct {
	/**
	*	Consts and Variables:
	**/
    //! The hz we are ticking at. (10hz, 40hz.. etc.)
	uint32_t    tick_rate;
    //! The time for each frame in 'seconds', 0.1 for 100ms(10hz), 0.025 for 25ms(40hz)
	double      frame_time_s;
    //! The time for each frame in 'miliseconds'.
	uint32_t    frame_time_ms;
    /**
    *   @return The realtime of the server since boot time.
    **/
    const uint64_t( *GetRealTime )( void );
    /**
    *   @brief  The actual server frame number we are processing.
    **/
    const int64_t ( *GetServerFrameNumber )( void );


    /**
    *
    *	Special Messages:
    *
    **/
    void ( *q_printf( 2, 3 ) bprintf )( int printlevel, const char *fmt, ... );
    void ( *q_printf( 1, 2 ) dprintf )( const char *fmt, ... );
    void ( *q_printf( 3, 4 ) cprintf )( edict_ptr_t *ent, int printlevel, const char *fmt, ... );
    void ( *q_printf( 2, 3 ) centerprintf )( edict_ptr_t *ent, const char *fmt, ... );
    void ( *sound )( edict_ptr_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs );
    void ( *positioned_sound )( const vec3_t origin, edict_ptr_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs );


    /**
    *	Config strings hold all the index strings, the lightstyles, the models that are in-use,
    *	and also misc data like the sky definition and cdtrack.
    *
    *	All of the current configstrings are sent to clients when they connect,
    *	and in case of any changes, which too are sent to all connected clients.
    **/
    configstring_t *( *GetConfigString )( const int32_t index );
    void ( *configstring )( int num, const char *string );

    void ( *q_noreturn q_printf( 1, 2 ) error )( const char *fmt, ... );

    //! Get access to the actual pointer of a loaded configstring model.
    const model_t *( *GetModelDataForName )( const char *name );
    const model_t *( *GetModelDataForHandle )( const qhandle_t handle );

    //! Get access to the actual pointer of a loaded bsp inline-model.
    const mmodel_t *( *GetInlineModelDataForName )( const char *name );
    const mmodel_t *( *GetInlineModelDataForHandle )( const qhandle_t handle );

    // the *index functions create configstrings(precache iqm models) and some internal server state.
    // these are sent over to the client 
    int ( *modelindex )( const char *name );
    int ( *soundindex )( const char *name );
    int ( *imageindex )( const char *name );

    void ( *setmodel )( edict_ptr_t *ent, const char *name );


    /**
    *
    *	Collision Detection:
    *
    **/
    //! Perform a trace through the world and its entities with a bbox from start to end point.
    const cm_trace_t( *trace )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_ptr_t *passent, const cm_contents_t contentmask );
    //! Perform a trace clip to a single entity. Effectively skipping looping over many if you were using trace instead.
    const cm_trace_t( *clip )( edict_ptr_t *entity, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const cm_contents_t contentmask );
    //! Returns a cm_contents_t of the BSP 'solid' residing at point. SOLID_NONE if in open empty space.
    const cm_contents_t( *pointcontents )( const vec3_t point );
    /**
    *   @return True if the points p1 to p2 are within two visible areas.
    *   @note   Also checks portalareas so that doors block sight.
    **/
    const bool( *inPVS )( const vec3_t p1, const vec3_t p2 );
    /**
    *   @return True if the points p1 to p2 are within two hearable areas.
    *   @note   Also checks portalareas so that doors block hearing.
    **/
    const bool( *inPHS )( const vec3_t p1, const vec3_t p2 );
    //! Set the state of the matching area portal number.
    void ( *SetAreaPortalState )( const int32_t portalnum, const bool open );
    //! Get state of the matching area portal number.
    const int32_t( *GetAreaPortalState )( const int32_t portalnum );
    //! Returns true if the areas are connected.
    const bool( *AreasConnected )( const int32_t area1, const int32_t area2 );
    /**
    *	An entity will never be sent to a client or used for collision if it is not passed to linkentity.
    *   If the size, position, solidity, clipmask, hullContents, or owner changes, it must be relinked.
    **/
    //! Link the entity into the world area grid for collision detection.
    void ( *linkentity )( edict_ptr_t *ent );
    //! UnLink the entity from the world area grid for collision detection.
    void ( *unlinkentity )( edict_ptr_t *ent );
    //! Return all entities that are inside or touching the bounding box area into the list array.
    const int32_t( *BoxEdicts )( const vec3_t mins, const vec3_t maxs, edict_ptr_t **list, const int32_t maxcount, const int32_t areatype );


    /**
    *
    *	(Collision Model-) Entities:
    *
    **/
    /**
    *   @brief  Returns the number of the cm_entity_t list root key/value pair within the cm->entities array.
    *   @note   This only works on the actual root key/value pair of the cm_entity_t list. Otherwise it returns -1.
    **/
    const int32_t( *CM_EntityNumber )( const cm_entity_t *entity );
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
    *
    *	FileSystem:
    *
    *
    **/
    /**
    *	@brief	Returns non 0 in case of existance.
    **/
    const int32_t( *FS_FileExistsEx )( const char *path, const uint32_t flags );
    /**
    *	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
    *	@return	length < 0 indicates error.
    **/
    const int32_t( *FS_LoadFile )( const char *path, void **buffer );
    /**
    *	@brief	Frees FS_FILESYSTEM Tag Malloc file buffer.
    **/
    void ( *FS_FreeFile )( void *buffer );


    /**
    *
    *	Network Messaging:
    *
    **/
    //! Will broadcast the current written message write buffer to all(multiple) clients (optional: that are in the same PVS/PHS as origin.)
    void ( *multicast )( const vec3_t origin, multicast_t to, bool reliable );
    //! Will broadcast the current written message write buffer to the client that is attached to ent.
    void ( *unicast )( edict_ptr_t *ent, bool reliable );
    void ( *WriteInt8 )( const int32_t c );
    void ( *WriteUint8 )( const int32_t c );
    void ( *WriteInt16 )( const int32_t c );
    void ( *WriteUint16 )( const int32_t c );
    void ( *WriteInt32 )( const int32_t c );
    void ( *WriteInt64 )( const int64_t c );
    void ( *WriteIntBase128 )( int64_t c );
    void ( *WriteUintBase128 )( uint64_t c );
    void ( *WriteHalfFloat )( const float f );
    void ( *WriteTruncatedFloat )( const float f );
    void ( *WriteFloat )( const float f );
    void ( *WriteDouble )( const double d );
    void ( *WriteString )( const char *s );
    void ( *WritePosition )( const vec3_t pos, const msgPositionEncoding_t encoding );    // some fractional bits
    void ( *WriteDir8 )( const vec3_t pos );         // single byte encoded, very coarse
    void ( *WriteAngle8 )( const float f );
    void ( *WriteAngle16 )( const float f );


    /**
    *
    *	'Tag' Managed Memory Allocation:
    *
    **/
    //! Resize an already TagMalloced block of memory.
    void *( *TagReMalloc )( void *ptr, unsigned newsize );
    /**
    *   @brief Allocates memory with a specific tag for tracking purposes.
    *   @param size The size of the memory block to allocate, in bytes.
    *   @param tag The tag used to categorize the allocated memory. Must not exceed UINT16_MAX - TAG_MAX.
    *   @note  The memory is NOT initialized to zero state.
    *   @return A pointer to the allocated memory block, or NULL if the allocation fails.
    **/
    void *( *TagMalloc )( const uint32_t size, const memtag_t tag );
    /**
    *   @brief Allocates memory with a specific tag for tracking purposes.
    *   @param size The size of the memory block to allocate, in bytes.
    *   @param tag The tag used to categorize the allocated memory. Must not exceed UINT16_MAX - TAG_MAX.
    *   @note  The memory is initialized to zero state.
    *   @return A pointer to the allocated memory block, or NULL if the allocation fails.
    **/
    void *( *TagMallocz )( const uint32_t size, const memtag_t tag );
    //! Free a tag allocated memory block.
    void ( *TagFree )( void *ptr );
    void ( *TagFreeP )( void **ptr );
    //! Free all SVGAME_ related tag memory blocks. (Essentially resetting memory.)
    void ( *FreeTags )( const memtag_t tag );


    /**
    *
    *	Console Variable Interaction:
    *
    **/
    //! Will create the cvar if it is non-existing, use with nullptr value and 0 flags to acquire an already created cvar instead.
    cvar_t *( *cvar )( const char *var_name, const char *value, int flags );
    //! Will set a cvar its value.
    cvar_t *( *cvar_set )( const char *var_name, const char *value );
    //! Will forcefully set a cvar its value.
    cvar_t *( *cvar_forceset )( const char *var_name, const char *value );


    /**
    *
    *	ClientCommand and ServerCommand parameter access:
    *
    **/
    //! Get argument count of server game command that was received.
    const int32_t( *argc )( void );
    //! Get argument value of server game command by its index.
    char *( *argv )( int n );
    //! Get the full raw command string as it was received.
    char *( *args )( void );     // concatenation of all argv >= 1
    //! Add commands to the server console as if they were typed in, useful for for map changing, etc
    void ( *AddCommandString )( const char *text );


    /**
    *
    *   Other:
    * 
    **/
    //! Returns current error number.
    const int32_t( *Q_ErrorNumber )( void );
    //! Returns matching string for error number.
    const char *(* Q_ErrorString)( const int32_t error );

    //! Does nothing atm.
    void (*DebugGraph)(float value, int color);
} svgame_import_t;

/**
*	Functions and Variables Exported by the ServerGame subsystem:
**/
typedef struct {
    int32_t apiversion;
	
	/**
	*	Init/Shutdown:
	* 
    *	The preinit and init functions will only be called when a game starts,
    *	not each time a level is loaded.  
    *
    *   All 'Persistant' data for clients AND the server, can be allocated in Init.
	**/
    void ( *PreInit )( void );
    void ( *Init )( void );
    void ( *Shutdown )( void );

    //! Each new level entered will cause a call to SpawnEntities
    void ( *SpawnEntities )( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );


	/**
	*	GameModes:
	**/
    /**
    *	@return	True if the game mode is a legitimate existing one.
    **/
    const bool ( *IsValidGameModeType )( const int32_t gameModeID );
    /**
    *   @return True if the game mode is multiplayer.
    **/
    const bool ( *IsMultiplayerGameMode ) ( const int32_t gameModeID );
    /**
    *	@return	The current active game mode ID.
    **/
    const int32_t( *GetActiveGameModeType )( void );
    /**
    *	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
    **/
    const int32_t( *GetDefaultMultiplayerGamemodeType )( void );
    /**
    *	@return	The actual ID of the current gamemode.
    **/
    const char *( *GetGameModeName )( const int32_t gameModeID );
    /**
    *	@return	True in case the current gamemode allows for saving the game.
    *			(This should only be true for single and cooperative play modes.)
    **/
    const bool ( *GameModeAllowSaveGames )( const bool isDedicated );


    /**
    *	Read/Write Game:
    *	Is for storing persistant cross level information about the world state and the clients.
    *
    *	WriteGame is called every time a level is exited.
    *	ReadGame is called on a loadgame.
    **/
    void ( *WriteGame )( const char *filename, const bool isAutoSave );
    void ( *ReadGame )( const char *filename );


    /**
    *	Read/Write Level:
    *
    *	ReadLevel is called after the default map information has been
    *	loaded with SpawnEntities
    **/
    void ( *WriteLevel )( const char *filename );
    void ( *ReadLevel )( const char *filename );


    /**
    *
    *	Client(s):
    *
    **/
    const bool ( *ClientConnect )( edict_ptr_t *ent, char *userinfo );
    void ( *ClientBegin )( edict_ptr_t *ent );
    void ( *ClientUserinfoChanged )( edict_ptr_t *ent, char *userinfo );
    void ( *ClientDisconnect )( edict_ptr_t *ent );
    void ( *ClientCommand )( edict_ptr_t *ent );
    void ( *ClientThink )( edict_ptr_t *ent, usercmd_t *cmd );


    /**
    *
    *	Player Movement:
    *
    **/
    //! Perform a frame's worth of player movement using specified pmoveParams configuration.
    void ( *PlayerMove )( struct pmove_s *pmove, struct pmoveParams_s *params );
    //! Setup the basic player move configuration parameters. (Used by server for new clients.)
    //void ( *ConfigurePlayerMoveParameters )( pmoveParams_t *pmp );


    /**
    *
    *	Frames:
    *
    *
    **/
    //! Runs a single frame of the server game.
    void ( *RunFrame )( void );


    /**
    *
    *	ServerCommand:
    *
    **/
    //! ServerCommand will be called when an "sv <command>" command is issued on the
    //! server console.
    //! The game can issue gi.argc() / gi.argv() commands to get the rest
    //! of the parameters
    void ( *ServerCommand )( void );


    /**
    *	Global variables shared between game and server
    *
    *	The edict array is allocated in the game dll so it
    *	can vary in size from one game to another.
    *
    *	The size will be fixed when ge->Init() is called
    **/
    sv_edict_pool_i *edictPool;
} svgame_export_t;
