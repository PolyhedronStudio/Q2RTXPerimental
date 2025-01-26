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
// g_local.h -- local definitions for ServerGame module

#include "shared/shared.h"
#include "shared/util_list.h"


// Should already have been defined by CMake for this ClientGame target.
//
// Define SVGAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#ifndef SVGAME_INCLUDE
#define SVGAME_INCLUDE
#endif
#include "shared/svgame.h"

// Extern here right after including shared/svgame.h
extern svgame_import_t gi;
extern svgame_export_t globals;

// SharedGame includes:
#include "../sharedgame/sg_shared.h"



//! Features this game supports.
static constexpr int32_t SVG_FEATURES = ( GMF_PROPERINUSE | GMF_WANT_ALL_DISCONNECTS );
// The "gameversion" client command will print this plus compile date.
static constexpr const char *GAMEVERSION = "BaseQ2RTXP";



/***
*
* 
*
*   General
*
*
* 
***/
// extern times.
extern sg_time_t FRAME_TIME_S;
extern sg_time_t FRAME_TIME_MS;

// For backwards compatibilities.
#define FRAMETIME BASE_FRAMETIME_1000 // OLD: 0.1f	NEW: 40hz makes for 0.025f

//! Spawnflag type.
typedef int32_t spawnflag_t;

/**
*	Memory tag IDs for specified group memory types, allowing for efficient cleanup of said group's memory.
**/
//! Clear when unloading the dll.
static constexpr int32_t TAG_SVGAME = 765;
//! Clear when loading a new level.
static constexpr int32_t TAG_SVGAME_LEVEL = 766;
//! Clear when loading a new level.
static constexpr int32_t TAG_SVGAME_LUA = 767;

/**
*   Handedness values
**/
static constexpr int32_t RIGHT_HANDED = 0;
static constexpr int32_t LEFT_HANDED = 1;
static constexpr int32_t CENTER_HANDED = 2;

/**
*   Other:
**/
// TODO: Fix the whole max shenanigan in shared.h,  because this is wrong...
#undef max
//! Just to, 'hold time', 'forever and ever'.
constexpr sg_time_t HOLD_FOREVER = sg_time_t::from_ms( std::numeric_limits<int64_t>::max() );

//! Actual Melee attack distance used for AI.
static constexpr float AI_MELEE_DISTANCE = 80.f;

//! Number of entities reserved for display use of dead player bodies.
static constexpr int32_t BODY_QUEUE_SIZE = 8;



/**
*
*
*
*   Extern CVars:
*
*
*
**/
extern cvar_t *dedicated;
extern cvar_t *password;
extern cvar_t *spectator_password;
extern cvar_t *needpass;
extern cvar_t *filterban;

extern cvar_t *maxclients;
extern cvar_t *maxspectators;
extern cvar_t *maxentities;
extern cvar_t *nomonsters;
extern cvar_t *aimfix;

extern cvar_t *gamemode;
extern cvar_t *deathmatch;
extern cvar_t *coop;
extern cvar_t *dmflags;
extern cvar_t *skill;
extern cvar_t *fraglimit;
extern cvar_t *timelimit;

extern cvar_t *sv_cheats;
extern cvar_t *sv_flaregun;
extern cvar_t *sv_maplist;
extern cvar_t *sv_features;

extern cvar_t *sv_airaccelerate;
extern cvar_t *sv_maxvelocity;
extern cvar_t *sv_gravity;

extern cvar_t *sv_rollspeed;
extern cvar_t *sv_rollangle;

extern cvar_t *flood_msgs;
extern cvar_t *flood_persecond;
extern cvar_t *flood_waitdelay;

extern cvar_t *g_select_empty;

// Moved to CLGame.
//extern cvar_t *gun_x;
//extern cvar_t *gun_y;
//extern cvar_t *gun_z;

// Moved to CLGame.
//extern cvar_t *run_pitch;
//extern cvar_t *run_roll;
//extern cvar_t *bob_up;
//extern cvar_t *bob_pitch;
//extern cvar_t *bob_roll;



/**
*   Combat:
**/
//! Combat related enums etc.
#include "svgame/svg_combat.h"
/**
*   Entity UseTargets:
**/
//! UseTargets related enums etc.
#include "svgame/svg_usetargets.h"
/**
*   Weaponry:
**/
#include "svgame/svg_weapons.h"
/**
*   Items:
**/
//! Include items data structures.
#include "svgame/svg_game_items.h"
//! For access all over.
extern  gitem_t itemlist[];



/**
*   Include the GAME locals.
**/
#include "svgame/svg_game_locals.h"
//! Extern, access all over game dll code.
extern  game_locals_t   game;
extern  int sm_meat_index;
extern  int snd_fry;
/**
*   Include the LEVEL locals.
**/
#include "svgame/svg_level_locals.h"
//! Extern, access all over game dll code.
extern level_locals_t level;

/**
*   SpawnTemp:
**/
#include "svgame/svg_spawn_temp.h"
//! Extern, access all over game dll code.
extern spawn_temp_t st;


/***
*
* 
*
*   Pusher/Mover- Move Info Data Structures:
*
* 
*
**/
#include "svgame/svg_pushmove_info.h"



/**
* 
* 
* 
*   Field Offset Macros:
* 
* 
* 
**/
#define FOFS_GENTITY( field )       q_offsetof( edict_t, field )
#define FOFS_GCLIENT( field )       q_offsetof( gclient_t, field )
#define FOFS_GAME_LOCALS( field )   q_offsetof( game_locals_t, field )
#define FOFS_LEVEL_LOCALS( field )  q_offsetof( level_locals_t, field )
#define FOFS_SPAWN_TEMP( field )    q_offsetof( spawn_temp_t, field )

#define random()    frand()
#define crandom()   crand()




/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool SVG_GetGamemodeNoSaveGames( const bool isDedicatedServer );
/**
*   @brief  
**/
void SVG_Command_Score_f(edict_t *ent);



/**
*
*
*
*   ItemList AND Entities:
*
*
*
**/
/**
*   @brief
**/
void SVG_PrecacheItem( const gitem_t *it);
/**
*   @brief
**/
void SVG_InitItems(void);
/**
*   @brief
**/
void SVG_SetItemNames(void);
/**
*   @brief
**/
const gitem_t *SVG_FindItem(const char *pickup_name);
/**
*   @brief
**/
const gitem_t *SVG_FindItemByClassname(const char *classname);
/**
*   @brief
**/
#define ITEM_INDEX(x) ((x)-itemlist)
/**
*   @brief
**/
edict_t *Drop_Item(edict_t *ent, const gitem_t *item);
/**
*   @brief
**/
void SVG_SetItemRespawn(edict_t *ent, float delay);
/**
*   @brief
**/
void SVG_SpawnItem(edict_t *ent, const gitem_t *item);
/**
*   @brief
**/
int PowerArmorType(edict_t *ent);
/**
*   @brief
**/
const gitem_t *SVG_GetItemByIndex(int index);
/**
*   @brief
**/
const bool Add_Ammo(edict_t *ent, const gitem_t *item, const int32_t count);
/**
*   @brief
**/
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

//
// g_utils.c
//
/**
*   @brief
**/
const bool    KillBox( edict_t *ent, const bool bspClipping );



/**
*
*
*
*   MoveWith Functionality:
*
*
*
**/
/**
*   @brief
**/
void SVG_MoveWith_AdjustToParent( const Vector3 &deltaParentOrigin, const Vector3 &deltaParentAngles, const Vector3 &parentVUp, const Vector3 &parentVRight, const Vector3 &parentVForward, edict_t *parentMover, edict_t *childMover );
/**
*   @brief
**/
//void SVG_MoveWith_Init( edict_t *self, edict_t *parent );
/**
*   @brief
**/
void SVG_MoveWith_SetChildEntityMovement( edict_t *self );
/**
*   @note   At the time of calling, parent entity has to reside in its default state.
*           (This so the actual offsets can be calculated easily.)
**/
void SVG_MoveWith_SetTargetParentEntity( const char *targetName, edict_t *parentMover, edict_t *childMover );



/**
*
*
*
*   Signals:
*
*
*
**/
/**
*   @brief  Describes the value type of a signal's argument.
**/
typedef enum {
    //! Argument type wasn't set!
    SIGNAL_ARGUMENT_TYPE_NONE = 0,

    //! Boolean type.
    SIGNAL_ARGUMENT_TYPE_BOOLEAN,
    //! Integer type.
    //SIGNAL_ARGUMENT_TYPE_INTEGER,
    //! Double type.
    SIGNAL_ARGUMENT_TYPE_NUMBER,
    //! String type.
    SIGNAL_ARGUMENT_TYPE_STRING,
    //! (Nullptr) type.
    SIGNAL_ARGUMENT_TYPE_NULLPTR,
} svg_signal_argument_type_t;
/**
*   @brief  Holds a Signal's argument its key, type, as well as its value.
**/
typedef struct svg_signal_argument_s {
    //! Type.
    svg_signal_argument_type_t type;
    //! Key index name of the argument for usage within the lua table.
    const char *key;
    //! Value.
    union {
        // Boolean representation.
        bool boolean;
        //! Integer representation.
        int64_t integer;
        //! Number(double) representation.
        double number;
        //! String representation.
        const char *str;
    } value;
} svg_signal_argument_t;
//! Typedef an std::vector for varying argument counts.
typedef std::vector<svg_signal_argument_t> svg_signal_argument_array_t;

/**
*   @brief
**/
//void SVG_SignalOut( edict_t *ent, edict_t *sender, edict_t *activator, const char *signalName, const svg_signal_argument_t *signalArguments = nullptr, const int32_t numberOfSignalArguments = 0 );
void SVG_SignalOut( edict_t *ent, edict_t *signaller, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments = {} );



/**
*
*
*
*   Utilities:
*
*
*
**/
/**
*   @brief  Wraps up the new more modern SVG_ProjectSource.
**/
void    SVG_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result );
/**
*   @brief  Project vector from source. 
**/
const Vector3 SVG_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right );

/**
*   @brief
**/
char *SVG_CopyString( const char *in );

/**
*   @brief
**/
void SVG_TouchTriggers( edict_t *ent );
/**
*   @brief
**/
void SVG_TouchProjectiles( edict_t *ent, const Vector3 &previous_origin );
/**
*   @brief
**/
void SVG_TouchSolids( edict_t *ent );
/**
*   @brief
**/

/**
*   @brief
**/
void SVG_SetMoveDir( vec3_t angles, Vector3 &movedir );

/**
*   @brief
**/
edict_t *SVG_PickTarget( char *targetname );
/**
*   @brief
**/
void SVG_UseTargets( edict_t *ent, edict_t *activator, const entity_usetarget_type_t useType = entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, const int32_t useValue = 0 );



/**
*
*
*
*   Edicts(Entities):
*
*
* 
**/

//
// g_monster.c
//
/**
*   @brief
**/
void monster_fire_bullet( edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype );
/**
*   @brief
**/
void monster_fire_shotgun( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype );
/**
*   @brief
**/
void M_droptofloor( edict_t *ent );
/**
*   @brief
**/
void M_CatagorizePosition( edict_t *ent, const Vector3 &in_point, liquid_level_t &liquidlevel, contents_t &liquidtype );
/**
*   @brief
**/
void M_CheckGround( edict_t *ent, const contents_t mask );
/**
*   @brief
**/
void M_WorldEffects( edict_t *ent );


//
// g_misc.c
//
/**
*   @brief
**/
void SVG_Misc_ThrowHead( edict_t *self, const char *gibname, int damage, int type );
/**
*   @brief
**/
void SVG_Misc_ThrowClientHead( edict_t *self, int damage );
/**
*   @brief
**/
void SVG_Misc_ThrowGib( edict_t *self, const char *gibname, int damage, int type );
/**
*   @brief
**/
void SVG_Misc_ThrowDebris( edict_t *self, const char *modelname, float speed, vec3_t origin );
/**
*   @brief
**/
void SVG_Misc_BecomeExplosion1( edict_t *self );

#define CLOCK_MESSAGE_SIZE  16
/**
*   @brief
**/
void func_clock_think( edict_t *self );
/**
*   @brief
**/
void func_clock_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

/**
*   Weapon:
**/
const Vector3 SVG_MuzzleFlash_ProjectAndTraceToPoint( edict_t *ent, const Vector3 &muzzleFlashOffset, const Vector3 &forward, const Vector3 &right );
#if 0
bool fire_hit( edict_t *self, vec3_t aim, int damage, int kick );
#endif
/**
*   @brief  Used for all impact (fighting kick/punch) attacks
**/
const bool fire_hit_punch_impact( edict_t *self, const Vector3 &start, const Vector3 &aimDir, const int32_t damage, const int32_t kick );
/**
*   @brief  Fires a single round. Used for machinegun and chaingun.  Would be fine for
*           pistols, rifles, etc....
**/
void fire_bullet( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, const sg_means_of_death_t meansOfDeath );
/**
*   @brief  Shoots shotgun pellets.  Used by shotgun and super shotgun.
**/
void fire_shotgun( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, const sg_means_of_death_t meansOfDeath );


/**
*   Player:
**/
/**
*   @brief
**/
void player_pain( edict_t *self, edict_t *other, float kick, int damage );
/**
*   @brief
**/
void player_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
/**
*   @brief  Will reset the entity client's 'Field of View' back to its defaults.
**/
void SVG_Player_ResetPlayerStateFOV( gclient_t *client );
/**
*   @brief  For SinglePlayer: Called only once, at game first initialization.
*           For Multiplayer Modes: Called after each death, and level change.
**/
void SVG_Player_InitPersistantData( edict_t *ent, gclient_t *client );
/**
*   @brief  Clears respawnable client data, which stores the timing of respawn as well as a copy of
*           the client persistent data, to reapply after respawn.
**/
void SVG_Player_InitRespawnData( gclient_t *client );
/**
*    @brief  Some information that should be persistant, like health, is still stored in the edict structure.
*            So it needs to be mirrored out to the client structure before all the edicts are wiped.
**/
void SVG_Player_SaveClientData( void );
/**
*   @brief  Restore the client stored persistent data to reinitialize several client entity fields.
**/
void SVG_Player_RestoreClientData( edict_t *ent );
/**
*   @brief  Only called when pers.spectator changes.
*   @note   That resp.spectator should be the opposite of pers.spectator here
**/
void SVG_Player_SelectSpawnPoint( edict_t *ent, Vector3 &origin, Vector3 &angles );
/**
*   @brief
**/
void SVG_Player_Obituary( edict_t *self, edict_t *inflictor, edict_t *attacker );
/**
*   @brief  Called either when a player connects to a server, OR respawns in a multiplayer game.
*
*           Will look up a spawn point, spawn(placing) the player 'body' into the server and (re-)initializing
*           saved entity and persistant data. (This includes actually raising the weapon up.)
**/
void SVG_Player_PutInServer( edict_t *ent );

/**
*   @brief
**/
void SVG_Client_RespawnPlayer( edict_t *self );
/**
*   @brief  Only called when pers.spectator changes.
*   @note   That resp.spectator should be the opposite of pers.spectator here
**/
void SVG_Client_RespawnSpectator( edict_t *ent );
/**
*   @brief  called whenever the player updates a userinfo variable.
*
*           The game can override any of the settings in place
*           (forcing skins or names, etc) before copying it off.
**/
void SVG_Client_UserinfoChanged( edict_t *ent, char *userinfo );



/**
* 
* 
* 
*   Player Trail:
* 
* 
* 
**/
/**
*   @brief
**/
void PlayerTrail_Init( void );
/**
*   @brief
**/
void PlayerTrail_Add( vec3_t spot );
/**
*   @brief
**/
void PlayerTrail_New( vec3_t spot );
/**
*   @brief
**/
edict_t *PlayerTrail_PickFirst( edict_t *self );
/**
*   @brief
**/
edict_t *PlayerTrail_PickNext( edict_t *self );
/**
*   @brief
**/
edict_t *PlayerTrail_LastSpot( void );




/**
* 
* 
* 
*   Server Commands:
* 
* 
* 
**/
/**
*   @brief  SVG_ServerCommand will be called when an "sv" command is issued.
*           The game can issue gi.argc() / gi.argv() commands to get the rest
*           of the parameters.
**/
void SVG_ServerCommand( void );
/**
*   @brief
**/
bool SVG_FilterPacket( char *from );



/**
* 
* 
* 
*   View:
* 
* 
* 
**/
/**
*   @brief  This will be called once for each server frame, before running any other entities in the world.
**/
void SVG_Client_BeginServerFrame( edict_t *ent );
/**
*	@brief	Called for each player at the end of the server frame, and right after spawning.
**/
void SVG_Client_EndServerFrame( edict_t *ent );



/**
* 
* 
* 
*   HUD:
* 
* 
* 
**/
//#include "svgame/player/svg_player_hud.h"


/**
* 
* 
* 
*   Player Weapon:
* 
* 
* 
**/
#include "svgame/player/svg_player_weapon.h"



/**
* 
* 
* 
*   Physics:
* 
* 
* 
**/
void SVG_Impact( edict_t *e1, trace_t *trace );
const contents_t SVG_GetClipMask( edict_t *ent );
void SVG_RunEntity( edict_t *ent );



/**
* 
* 
* 
*   ChaseCam:
* 
* 
* 
**/
void SVG_ChaseCam_Update( edict_t *ent );
void SVG_ChaseCam_Next( edict_t *ent );
void SVG_ChaseCam_Previous( edict_t *ent );
void SVG_ChaseCam_GetTarget( edict_t *ent );
//============================================================================



/**
*   @brief  Stores the final ground information results.
**/
typedef struct mm_ground_info_s {
    //! Pointer to the actual ground entity we are on.(nullptr if none).
    struct edict_s *entity;
    //! Ground entity link count.
    int32_t         entityLinkCount;

    //! A copy of the plane data from the ground entity.
    cplane_t        plane;
    //! A copy of the ground plane's surface data. (May be none, in which case, it has a 0 name.)
    csurface_t      surface;
    //! A copy of the contents data from the ground entity's brush.
    contents_t      contents;
    //! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
    cm_material_t *material;
} mm_ground_info_t;

/**
*   @brief  Stores the final 'liquid' information results. This can be lava, slime, or water, or none.
**/
typedef struct mm_liquid_info_s {
    //! The actual BSP liquid 'contents' type we're intersecting with, or inside of.
    contents_t      type;
    //! The level of degree at which we're intersecting with, or inside of the liquid 'solid' brush.
    liquid_level_t	level;
} mm_liquid_info_t;

/**
*   @brief  edict->flags
**/
typedef enum {
    FL_NONE = 0,
    FL_FLY = BIT( 1 ),
    FL_SWIM = BIT( 2 ), //! Implied immunity to drowining.
    FL_IMMUNE_LASER = BIT( 3 ),
    FL_INWATER = BIT( 4 ),
    FL_GODMODE = BIT( 5 ),
    FL_NOTARGET = BIT( 6 ),
    FL_DODGE = BIT( 7 ), //! Monster should try to dodge this.
    FL_IMMUNE_SLIME = BIT( 8 ),
    FL_IMMUNE_LAVA = BIT( 9 ),
    FL_PARTIALGROUND = BIT( 10 ),//! Not all corners are valid.
    FL_WATERJUMP = BIT( 11 ),//! Player jumping out of water.
    FL_TEAMSLAVE = BIT( 12 ),//! Not the first on the team.
    FL_NO_KNOCKBACK = BIT( 13 ),
    FL_RESPAWN = BIT( 14 ) //! Used for item respawning.
    //FL_POWER_ARMOR          = BIT( 15 ),//! Power armor (if any) is active
} entity_flags_t;
// Enumerator Type Bit Flags Support:
QENUM_BIT_FLAGS( entity_flags_t );

/**
*   Include Client Data Structures:
**/
#include "svgame/svg_game_client.h"
/**
*
* 
*
*   ServerGame Side Entity:
* 
* 
* 
**/
#include "svgame/svg_game_edict.h"
// Extern access.
extern edict_t *g_edicts;
#define world   (&g_edicts[0])


/***
*
*
*
*   View Pitching Times: (Q2RE Style).
*
*
*
***/
//! Time between ladder sounds.
static constexpr sg_time_t LADDER_SOUND_TIME = 375_ms;
//! View pitching times
static inline constexpr sg_time_t DAMAGE_TIME_SLACK() {
    return ( 100_ms - FRAME_TIME_MS );
}
//! Time for damage effect.
static inline constexpr sg_time_t DAMAGE_TIME() {
    return 500_ms + DAMAGE_TIME_SLACK();
}
//! Time for falling.
static inline constexpr sg_time_t FALL_TIME() {
    return 300_ms + DAMAGE_TIME_SLACK();
}
