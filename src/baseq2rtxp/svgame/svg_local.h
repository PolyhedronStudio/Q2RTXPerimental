/*********************************************************************
*
*
*	SVGame: Local Header Includes and Definitions.
*
*
********************************************************************/
#pragma once

//! Include all shared codebase func/type defs..
#include "shared/shared.h"
//! Include list data structure functionality.
#include "shared/util_list.h"


/**
*   Should already have been defined by CMake for this ServerGame target.
*
*   Define SVGAME_INCLUDE so that game.h does not define the
*   short, server-visible svg_client_t and edict_t structures,
*   because we define the full size ones in this file
**/
#ifndef SVGAME_INCLUDE
    #define SVGAME_INCLUDE
#endif

//! Include the servergame import/export structures.
#include "shared/sv_game.h"

/**
*   Extern for 'global scope' access right here, after including shared/sv_game.h
**/
// Imported engine API and vars.
extern svgame_import_t gi;
// Exported game API and vars.
extern svgame_export_t globals;

/**
*   Include the shared game headers for functions and types.
**/
// SharedGame includes:
#include "sharedgame/sg_shared.h"
// Typedef for edict_t.
typedef struct sg_usetarget_hint_s sg_usetarget_hint_t;

/**
*   Forward declared types:
**/
enum entity_flags_t;

struct edict_t;
struct svg_edict_pool_t;
struct svg_client_t;

struct svg_trace_t;

struct mm_ground_info_t;
struct mm_liquid_info_t;

/**
*   Typedefs for the Server Game module.
**/
typedef int32_t spawnflag_t;


/**
* 
* 
* 
*   ServerGame Global Constants:
* 
* 
* 
**/
//! Frame time in seconds.
extern QMTime FRAME_TIME_S;
//! Frame time in miliseconds.
extern QMTime FRAME_TIME_MS;
//! Just to, 'hold time', 'forever and ever'.
constexpr QMTime HOLD_FOREVER = QMTime::FromMilliseconds( std::numeric_limits<int64_t>::max() );

//! For backwards compatibilities.
#define FRAMETIME BASE_FRAMETIME_1000 // OLD: 0.1f	NEW: 40hz makes for 0.025f

/***
*   View Pitching Times: (Q2RE Style).
***/
//! Time between ladder sounds.
static constexpr QMTime LADDER_SOUND_TIME = 375_ms;
//! View pitching times
static inline constexpr QMTime DAMAGE_TIME_SLACK() {
    return ( 100_ms - FRAME_TIME_MS );
}
//! Time for damage effect.
static inline constexpr QMTime DAMAGE_TIME() {
    return 500_ms + DAMAGE_TIME_SLACK();
}
//! Time for falling.
static inline constexpr QMTime FALL_TIME() {
    return 300_ms + DAMAGE_TIME_SLACK();
}

/**
*   Weapon Handedness values.
**/
static constexpr int32_t RIGHT_HANDED = 0;
static constexpr int32_t LEFT_HANDED = 1;
static constexpr int32_t CENTER_HANDED = 2;



/**
* 
* 
*
*   ServerGame Configuration:
*
* 
* 
**/
//! Features this game supports.
static constexpr int32_t SVG_FEATURES = ( GMF_PROPERINUSE | GMF_WANT_ALL_DISCONNECTS );
// The "gameversion" client command will print this plus compile date.
static constexpr const char *GAMEVERSION = "BaseQ2RTXP";

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
* 
* 
* 
*   Memory Utility Objects:
* 
* 
* 
**/
/**
*   String Utility Objects:
**/
// Simple wrapper around char, dynamic string block allocated in TAG_SVGAME_LEVEL.
using svg_level_qstring_t = sg_qtag_string_t<char, TAG_SVGAME_LEVEL>;
// Simple wrapper around char, dynamic string block allocated in TAG_SVGAME space.
using svg_game_qstring_t = sg_qtag_string_t<char, TAG_SVGAME>;
#if 0
/**
*   Memory Buffer Objects:
**/
// Simple wrapper for variable sized tag memory blocks (re-)allocated in TAG_SVGAME_LEVEL.
using svg_level_qtag_memory_t = sg_qtag_memory_t<char, TAG_SVGAME_LEVEL>;
// Simple wrapper for variable sized tag memory blocks (re-)allocated in TAG_SVGAME.
using svg_game_qtag_memory_t = sg_qtag_memory_t<char, TAG_SVGAME>;
#endif



/**
*
*
*
*   Common Includes:
*
*
*
**/
/**
*   Combat related enums etc.
**/
#include "svgame/svg_combat.h"

/**
*   Pusher/Mover- Move Info Data Structures:
**/
#include "svgame/svg_pushmove_info.h"

/**
*   Signal I/O:
**/
#include "svgame/svg_signalio.h"

/**
*   UseTargets related enums etc.
**/
#include "svgame/svg_usetargets.h"

/**
*   Signal I/O:
**/
#include "svgame/svg_trigger.h"

/**
*   (Player-)Weapon Related.
**/
#include "svgame/svg_weapons.h"

/**
*   Include items data structures.
**/
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



/**
* 
* 
* 
*   Field Offset Macros:
* 
* 
* 
**/
//! For game entity fields.
#define FOFS_GENTITY( field )       q_offsetof( edict_t, field )
//! For game client fields.
#define FOFS_GCLIENT( field )       q_offsetof( svg_client_t, field )
//! For game locals fields.
#define FOFS_GAME_LOCALS( field )   q_offsetof( game_locals_t, field )
//! For level locals fields.
#define FOFS_LEVEL_LOCALS( field )  q_offsetof( level_locals_t, field )
//! For spawn temp fields.
#define FOFS_SPAWN_TEMP( field )    q_offsetof( spawn_temp_t, field )

///**
//*   @return
//**/
//static inline const float _frand() {
//    return ( (int32_t)Q_rand() * 0x1p-32f + 0.5f );
//}
///**
//*   @return 
//**/
//static inline const float _crand() {
//    return ( (int32_t)Q_rand() * 0x1p-31f );
//}

//! Define.
#define random()    frand()
#define crandom()   crand()



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
void Touch_Item(edict_t *ent, edict_t *other, const cm_plane_t *plane, cm_surface_t *surf);



/**
*
*
*
*   Edicts(Entities):
*
*
* 
**/
/**
*   @brief
**/
void monster_fire_bullet( edict_t *self, vec3_t start, vec3_t dir, const float damage, const float kick, const float hspread, const float vspread, int flashtype );
/**
*   @brief
**/
void monster_fire_shotgun( edict_t *self, vec3_t start, vec3_t aimdir, const float damage, const float kick, const float hspread, const float vspread, int count, int flashtype );
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



/**
* 
* 
* 
*   Generic Weapon Functions:
* 
* 
* 
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
void fire_bullet( edict_t *self, const vec3_t start, const vec3_t aimdir, const float damage, const float kick, const float hspread, const float vspread, const sg_means_of_death_t meansOfDeath );
/**
*   @brief  Shoots shotgun pellets.  Used by shotgun and super shotgun.
**/
void fire_shotgun( edict_t *self, const vec3_t start, const vec3_t aimdir, const float damage, const float kick, const float hspread, const float vspread, int count, const sg_means_of_death_t meansOfDeath );


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
*   Client Data Structures:
*
*
*
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
/**
*   @brief  Stores the final ground information results.
**/
struct mm_ground_info_t {
    //! Pointer to the actual ground entity we are on.(nullptr if none).
    edict_t *entity;
    //! Ground entity link count.
    int32_t         entityLinkCount;

    //! A copy of the plane data from the ground entity.
    cm_plane_t      plane;
    //! A copy of the ground plane's surface data. (May be none, in which case, it has a 0 name.)
    cm_surface_t    surface;
    //! A copy of the contents data from the ground entity's brush.
    contents_t      contents;
    //! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
    cm_material_t *material;
};

/**
*   @brief  Stores the final 'liquid' information results. This can be lava, slime, or water, or none.
**/
struct mm_liquid_info_t {
    //! The actual BSP liquid 'contents' type we're intersecting with, or inside of.
    contents_t      type;
    //! The level of degree at which we're intersecting with, or inside of the liquid 'solid' brush.
    liquid_level_t	level;
};

#include "svgame/svg_game_edict.h"
// Extern access.
extern edict_t *g_edicts;
// World entity.
#define world   (&g_edicts[0])
// Base Entity Functions.
#include "svgame/svg_edicts.h"

// Edict Pool:
#include "svgame/svg_edict_pool.h"
// Extern access.
extern svg_edict_pool_t g_edict_pool;



/**
* 
* 
* 
*   Physics:
* 
* 
* 
**/
/**
*   @brief  Server Game expansion implementation of svg_trace_t.
*           handles fetching the edict_t from the trace_t entityNumber.
**/
struct svg_trace_t : public cm_trace_t {
    [[nodiscard]] svg_trace_t() = default;
    [[nodiscard]] svg_trace_t( const cm_trace_t &move ) : cm_trace_t( move ) {
        ent = g_edict_pool.EdictForNumber( move.entityNumber );
    }
    [[nodiscard]] svg_trace_t( const cm_trace_t &&move ) : cm_trace_t( move ) {
        ent = g_edict_pool.EdictForNumber( move.entityNumber );
    }

    //! Override type.
    edict_t *ent = nullptr;
};

/**
*   @brief Processes the impact of an SVG trace on an entity.
*   @param e1       A pointer to the entity (edict_t) involved in the impact.
*   @param trace    A pointer to the SVG trace (svg_trace_t) containing information about the impact.
**/
void SVG_Impact( edict_t *e1, svg_trace_t *trace );
/**
*   @brief  Get the clip mask for the entity.
**/
const contents_t SVG_GetClipMask( edict_t *ent );
/**
*   @brief  Run the entity's think function.
**/
void SVG_RunEntity( edict_t *ent );




