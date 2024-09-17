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



/**
*
*
*   General
*
*
**/
// extern times.
extern sg_time_t FRAME_TIME_S;
extern sg_time_t FRAME_TIME_MS;

// For backwards compatibilities.
#define FRAMETIME BASE_FRAMETIME_1000 // OLD: 0.1f	NEW: 40hz makes for 0.025f

// TODO: Fix the whole max shenanigan in shared.h,  because this is wrong...
#undef max

//! Just to, 'hold time', 'forever and ever'.
constexpr sg_time_t HOLD_FOREVER = sg_time_t::from_ms( std::numeric_limits<int64_t>::max( ) );

//! Features this game supports.
#define G_FEATURES  (GMF_PROPERINUSE|GMF_WANT_ALL_DISCONNECTS)
// The "gameversion" client command will print this plus compile date.
#define GAMEVERSION "BaseQ2RTXP"

/**
*	Memory tag IDs for specified group memory types, allowing for efficient
*   cleanup of said group's memory.
**/
//! Clear when unloading the dll.
static constexpr int32_t TAG_SVGAME = 765;
//! Clear when loading a new level.
static constexpr int32_t TAG_SVGAME_LEVEL = 766;

/**
*   Other:
**/
//! Actual Melee attack distance used for AI.
static constexpr float AI_MELEE_DISTANCE = 80.f;
//! Number of entities reserved for display use of dead player bodies.
static constexpr int32_t BODY_QUEUE_SIZE = 8;


/**
*
*
*   Q2RE View Pitching Times:
*
*
**/
//! View pitching times
static inline constexpr sg_time_t DAMAGE_TIME_SLACK( ) {
	return ( 100_ms - FRAME_TIME_MS );
}
//! Time for damage effect.
static inline constexpr sg_time_t DAMAGE_TIME( ) {
	return 500_ms + DAMAGE_TIME_SLACK( );
}
//! Time for falling.
static inline constexpr sg_time_t FALL_TIME( ) {
	return 300_ms + DAMAGE_TIME_SLACK( );
}
//! Time between ladder sounds.
static constexpr sg_time_t LADDER_SOUND_TIME = 375_ms;



/**
*
*
*   Damage Indicator and Means Of Death:
*
*
**/
// max number of individual damage indicators we'll track
static constexpr size_t MAX_DAMAGE_INDICATORS = 4;
/**
*   @brief  Stores data indicating where damage came from, and how much it damage it did.
**/
struct damage_indicator_t {
    //! Direction indicated hit came from.
    Vector3 from;
    //! Health taken.
    int32_t health;
    //! Armor taken.
    int32_t armor;
};

/**
*   @brief  Indicates the cause/reason/method of the entity's death.
**/
typedef enum {
    //! Unknown reasons.
    MEANS_OF_DEATH_UNKNOWN,
    
    //! When fists(or possibly in the future kicks) were the means of death.
    MEANS_OF_DEATH_HIT_FIGHTING,
    //! Shot down by a Pistol.
    MEANS_OF_DEATH_HIT_PISTOL,
    //! Shot down by a SMG.
    MEANS_OF_DEATH_HIT_SMG,
    //! Shot down by a rifle.
    MEANS_OF_DEATH_HIT_RIFLE,
    //! Shot down by a sniper.
    MEANS_OF_DEATH_HIT_SNIPER,

    //! Environmental drowned by water.
    MEANS_OF_DEATH_WATER,
    //! Environmental killed by slime.
    MEANS_OF_DEATH_SLIME,
    //! Environmental killed by lava.
    MEANS_OF_DEATH_LAVA,
    //! Environmental killed by a crusher.
    MEANS_OF_DEATH_CRUSHED,
    //! Environmental killed by someone teleporting to your location.
    MEANS_OF_DEATH_TELEFRAGGED,

    //! Entity suicide.
    MEANS_OF_DEATH_SUICIDE,
    //! Entity fell too harsh.
    MEANS_OF_DEATH_FALLING,

    //! Death by explosive.
    MEANS_OF_DEATH_EXPLOSIVE,
    //! Death by exploding misc_barrel.
    MEANS_OF_DEATH_EXPLODED_BARREL,

    //! Hit by a laser.
    MEANS_OF_DEATH_LASER,

    //! Hurt by a 'splash' particles of temp entity.
    MEANS_OF_DEATH_SPLASH,
    //! Hurt by a 'trigger_hurt' entity.
    MEANS_OF_DEATH_TRIGGER_HURT,

    //! Killed by bumping into a target_change_level while the gamemode does not allow/support it.
    MEANS_OF_DEATH_EXIT,

    //! Caused by a friendly fire.
    MEANS_OF_DEATH_FRIENDLY_FIRE
} sg_means_of_death_t;



/**
*
*
*   Entity (Spawn-)Flags
* 
* 
**/
/**
*   @brief  edict->spawnflags T
*           These are set with checkboxes on each entity in the map editor.
**/
#define SPAWNFLAG_NOT_EASY          0x00000100
#define SPAWNFLAG_NOT_MEDIUM        0x00000200
#define SPAWNFLAG_NOT_HARD          0x00000400
#define SPAWNFLAG_NOT_DEATHMATCH    0x00000800
#define SPAWNFLAG_NOT_COOP          0x00001000

/**
*   @brief  edict->flags
**/
typedef enum {
    FL_NONE                 = 0,
    FL_FLY                  = BIT( 1 ),
    FL_SWIM                 = BIT( 2 ), //! Implied immunity to drowining.
    FL_IMMUNE_LASER         = BIT( 3 ),
    FL_INWATER              = BIT( 4 ),
    FL_GODMODE              = BIT( 5 ),
    FL_NOTARGET             = BIT( 6 ),
    FL_DODGE                = BIT( 7 ), //! Monster should try to dodge this.
    FL_IMMUNE_SLIME         = BIT( 8 ),
    FL_IMMUNE_LAVA          = BIT( 9 ),
    FL_PARTIALGROUND        = BIT( 10 ),//! Not all corners are valid.
    FL_WATERJUMP            = BIT( 11 ),//! Player jumping out of water.
    FL_TEAMSLAVE            = BIT( 12 ),//! Not the first on the team.
    FL_NO_KNOCKBACK         = BIT( 13 ),
    FL_RESPAWN              = BIT( 14 ) //! Used for item respawning.
    //FL_POWER_ARMOR          = BIT( 15 ),//! Power armor (if any) is active
} ent_flags_t;




typedef enum {
    DAMAGE_NO,
    DAMAGE_YES,         // will take damage if hit
    DAMAGE_AIM          // auto targeting recognizes this
} damage_t;

//typedef enum {
//    WEAPON_READY,
//    WEAPON_ACTIVATING,
//    WEAPON_DROPPING,
//    WEAPON_FIRING
//} weaponstate_t;

/**
*   @brief  Describes a weapon's current state.
**/
typedef enum {
    //! The weapon is not doing anything else but sitting there, waiting for use.
    WEAPON_MODE_IDLE,

    //! The weapon is being 'Drawn'.
    WEAPON_MODE_DRAWING,
    //! The weapon is being 'Holstered'.
    WEAPON_MODE_HOLSTERING,

    //! The weapon is actively 'Primary' firing a shot.
    WEAPON_MODE_PRIMARY_FIRING,
    //! The weapon is actively 'Secondary' firing a shot.
    WEAPON_MODE_SECONDARY_FIRING,

    //! The weapon is actively 'Reloading' its clip.
    WEAPON_MODE_RELOADING,

    //! The weapon is 'aiming in' to the center of screen.
    WEAPON_MODE_AIM_IN,
    //! The weapon is 'firing' in 'Aimed' mode.
    WEAPON_MODE_AIM_FIRE,
    //! The weapon is 'aiming out' to the center of screen.
    WEAPON_MODE_AIM_OUT,

    //! Maximum weapon modes available.
    WEAPON_MODE_MAX,
} weapon_mode_t;

/**
*   @brief  Used for constructing a weapon's 'mode' animation setup.
**/
typedef struct weapon_mode_animation_s {
    //! IQM Model Animation Index.
    int32_t modelAnimationID;
    //! Objective start frame index.
    int32_t startFrame;
    //! Objective end frame index.
    int32_t endFrame;
    //! Relative animation frame duration( endFrame - startFrame ).
    sg_time_t duration;
} weapon_mode_animation_t;

/**
*   @brief  Used as an item's internal 'info' pointer for describing the actual
*           weapon's properties.
**/
typedef struct weapon_item_info_s {
    //! Mode frames.
    weapon_mode_animation_t modeAnimations[ WEAPON_MODE_MAX ];
    //! TODO: Other info.
} weapon_item_info_t;

/**
*   @brief  Specific 'Item Tags' so we can identify what item category/type
*           we are dealing with.
**/
typedef enum {
    //! Default for non tagged items.
    ITEM_TAG_NONE = 0,

    // Ammo Types:
    ITEM_TAG_AMMO_BULLETS_PISTOL,
    ITEM_TAG_AMMO_BULLETS_RIFLE,
    ITEM_TAG_AMMO_BULLETS_SMG,
    ITEM_TAG_AMMO_BULLETS_SNIPER,
    ITEM_TAG_AMMO_SHELLS_SHOTGUN,

    // Armor Types:
    ITEM_TAG_ARMOR_NONE,
    ITEM_TAG_ARMOR_JACKET,
    ITEM_TAG_ARMOR_COMBAT,
    ITEM_TAG_ARMOR_BODY,
    ITEM_TAG_ARMOR_SHARD,

    // Weapon Types:
    ITEM_TAG_WEAPON_FISTS,
    ITEM_TAG_WEAPON_PISTOL

    //AMMO_BULLETS,
    //AMMO_SHELLS,
    //AMMO_ROCKETS,
    //AMMO_GRENADES,
    //AMMO_CELLS,
    //AMMO_SLUGS
} gitem_tag_t;


//deadflag
#define DEAD_NO                 0
#define DEAD_DYING              1
#define DEAD_DEAD               2
#define DEAD_RESPAWNABLE        3

//range
#define RANGE_MELEE             0
#define RANGE_NEAR              1
#define RANGE_MID               2
#define RANGE_FAR               3

//gib types
#define GIB_ORGANIC             0
#define GIB_METALLIC            1

//monster ai flags
#define AI_STAND_GROUND         0x00000001
#define AI_TEMP_STAND_GROUND    0x00000002
#define AI_SOUND_TARGET         0x00000004
#define AI_LOST_SIGHT           0x00000008
#define AI_PURSUIT_LAST_SEEN    0x00000010
#define AI_PURSUE_NEXT          0x00000020
#define AI_PURSUE_TEMP          0x00000040
#define AI_HOLD_FRAME           0x00000080
#define AI_GOOD_GUY             0x00000100
#define AI_BRUTAL               0x00000200
#define AI_NOSTEP               0x00000400
#define AI_DUCKED               0x00000800
#define AI_COMBAT_POINT         0x00001000
#define AI_MEDIC                0x00002000
#define AI_RESURRECTING         0x00004000
#define AI_HIGH_TICK_RATE		0x00008000

//monster attack state
#define AS_STRAIGHT             1
#define AS_SLIDING              2
#define AS_MELEE                3
#define AS_MISSILE              4



//// power armor types
//#define POWER_ARMOR_NONE        0
//#define POWER_ARMOR_SCREEN      1
//#define POWER_ARMOR_SHIELD      2

// handedness values
#define RIGHT_HANDED            0
#define LEFT_HANDED             1
#define CENTER_HANDED           2


// game.serverflags values
#define SFL_CROSS_TRIGGER_1     0x00000001
#define SFL_CROSS_TRIGGER_2     0x00000002
#define SFL_CROSS_TRIGGER_3     0x00000004
#define SFL_CROSS_TRIGGER_4     0x00000008
#define SFL_CROSS_TRIGGER_5     0x00000010
#define SFL_CROSS_TRIGGER_6     0x00000020
#define SFL_CROSS_TRIGGER_7     0x00000040
#define SFL_CROSS_TRIGGER_8     0x00000080
#define SFL_CROSS_TRIGGER_MASK  0x000000ff


// noise types for PlayerNoise
#define PNOISE_SELF             0
#define PNOISE_WEAPON           1
#define PNOISE_IMPACT           2


// edict->movetype values
typedef enum {
    MOVETYPE_NONE,          // never moves
    MOVETYPE_NOCLIP,        // origin and angles change with no interaction
    MOVETYPE_PUSH,          // no clip to world, push on box contact
    MOVETYPE_STOP,          // no clip to world, stops on box contact

    MOVETYPE_WALK,          // gravity
    MOVETYPE_STEP,          // gravity, special edge handling

    MOVETYPE_ROOTMOTION,    // gravity, animation root motion influences velocities.

    MOVETYPE_FLY,
    MOVETYPE_TOSS,          // gravity
    MOVETYPE_FLYMISSILE,    // extra size to monsters
    MOVETYPE_BOUNCE
} movetype_t;



typedef struct {
    int32_t base_count;
    int32_t max_count;
    float   normal_protection;
    float   energy_protection;
    int32_t armor;
} gitem_armor_t;


// gitem_t->flags
#define ITEM_FLAG_WEAPON       1       //! "+use" makes active weapon
#define ITEM_FLAG_AMMO         2
#define ITEM_FLAG_ARMOR        4
#define ITEM_FLAG_STAY_COOP    8
// WID: These don't exist anymore, but can still be reused of course.
//#define IT_KEY          16
//#define IT_POWERUP      32

// gitem_t->weapon_index for weapons indicates model index.
// NOTE: These must MATCH in ORDER to those in g_spawn.cpp
#define WEAP_FISTS              1
#define WEAP_PISTOL             2
//#define WEAP_SHOTGUN            3
//#define WEAP_SUPERSHOTGUN       4
//#define WEAP_MACHINEGUN         5
//#define WEAP_CHAINGUN           6
//#define WEAP_GRENADES           7
//#define WEAP_GRENADELAUNCHER    8
//#define WEAP_ROCKETLAUNCHER     9
//#define WEAP_HYPERBLASTER       10
//#define WEAP_RAILGUN            11
//#define WEAP_BFG                12
//#define WEAP_FLAREGUN           13

/**
*   @brief  Used to create the items array, where each gitem_t is assigned its
*           descriptive item values.
**/
typedef struct gitem_s {
	//! Classname.
    const char  *classname; // spawning name
    
    //! Called right after precaching the item, this allows for weapons to seek for
    //! the appropriate animation data required for each used distinct weapon mode.
    void        ( *precached )( const struct gitem_s *item );

    //! Pickup Callback.
    const bool  ( *pickup )( struct edict_s *ent, struct edict_s *other );
    //! Use Callback.
    void        ( *use )( struct edict_s *ent, const struct gitem_s *item );
    //! Drop Callback.
    void        ( *drop )( struct edict_s *ent, const struct gitem_s *item );

    //! WeaponThink Callback.
    void        ( *weaponthink )( struct edict_s *ent, const bool processUserInputOnly );

    //! Path: Pickup Sound.
	const char	*pickup_sound; // WID: C++20: Added const.
    //! Path: World Model
	const char	*world_model; // WID: C++20: Added const.
    //! World Model Entity Flags.
    int32_t     world_model_flags;
    //! Path: View Weapon Model.
	const char	*view_model;

    //! Client Side Info:
	const char	*icon;
    //! For printing on 'pickup'.
	const char	*pickup_name;
    //! Number of digits to display by icon
    int         count_width;


    //! For ammo how much is acquired when picking up, for weapons how much is used per shot.
    int32_t     quantity;
    //! Limit of this weapon's capacity per 'clip'.
    int32_t     clip_capacity;
    //! For weapons, the name referring to the used Ammo Item type.
	const char	*ammo;
    // IT_* specific flags.
    int32_t     flags;
    //! Weapon ('model'-)index (For weapons):
    int32_t     weapon_index;

    //! Pointer to item category/type specific info.
    void        *info;
    //! Identifier for the item's category/type.
    gitem_tag_t tag;

    //! String of all models, sounds, and images this item will use and needs to precache.
    const char	*precaches;
} gitem_t;

/**
*   @brief  This structure is left intact through an entire game
*           it should be initialized at dll load time, and read/written to
*           the server.ssv file for savegames
**/
typedef struct {
    //! [maxclients] of client pointers.
    gclient_t   *clients;

    //! Can't store spawnpoint in level, because
    //! it would get overwritten by the savegame restore.
    //!
    //! Needed for coop respawns.
    char        spawnpoint[512];    

    //! Store latched cvars here that we want to get at often.
    int32_t             maxclients;
    int32_t             maxentities;
    sg_gamemode_type_t  gamemode;
    
    //! Cross level triggers.
    int32_t     serverflags;
    //! Number of items.
    int32_t     num_items;
    //! Autosaved.
    bool        autosaved;

    /**
    *   Information for PUSH/STOP -movers.
    * 
    *   We use numbers so we can fetch entities in-frame ensuring that we always have valid pointers.
    **/
    int32_t num_movewithEntityStates;
    struct {
        //! The child entity that has to move with its parent entity.
        int32_t childNumber;
        //! The parent entity that has to move its child entity.
        int32_t parentNumber;
    } moveWithEntities[MAX_EDICTS];
} game_locals_t;
//! Extern, access all over game dll code.
extern  game_locals_t   game;

extern  int sm_meat_index;
extern  int snd_fry;

/**
*   @brief  This structure is cleared as each map is entered
*           it is read/written to the level.sav file for savegames
**/
typedef struct {
    int64_t         framenum;
    sg_time_t		time;

    char        level_name[MAX_QPATH];  // the descriptive name (Outer Base, etc)
    char        mapname[MAX_QPATH];     // the server name (base1, etc)
    char        nextmap[MAX_QPATH];     // go here when fraglimit is hit

    // intermission state
    int64_t         intermission_framenum;  // time the intermission was started

	// WID: C++20: Added const.
    const char	*changemap;
    int64_t     exitintermission;
    vec3_t      intermission_origin;
    vec3_t      intermission_angle;

    edict_t     *sight_client;  // changed once each frame for coop games

    edict_t		*sight_entity;
    int64_t		sight_entity_framenum;
    edict_t		*sound_entity;
	int64_t		sound_entity_framenum;
    edict_t		*sound2_entity;
	int64_t		sound2_entity_framenum;

    int         pic_health;

    int         total_secrets;
    int         found_secrets;

    int         total_goals;
    int         found_goals;

    int         total_monsters;
    int         killed_monsters;

    edict_t     *current_entity;    // entity running from G_RunFrame
    int         body_que;           // dead bodies
} level_locals_t;
//! Extern, access all over game dll code.
extern level_locals_t level;

/**
*   @brief  spawn_temp_t is only used to hold entity field values that
*           can be set from the editor, but aren't actualy present
*           in edict_t during gameplay
**/
typedef struct {
    // world vars
    char        *sky;
    float       skyrotate;
    int         skyautorotate;
    vec3_t      skyaxis;
    char        *nextmap;
    char        *musictrack;

    int         lip;
    int         distance;
    int         height;
	// WID: C++20: Added const.
    const char	*noise;
    float       pausetime;
    char        *item;
    char        *gravity;

    float       minyaw;
    float       maxyaw;
    float       minpitch;
    float       maxpitch;
} spawn_temp_t;
//! Extern, access all over game dll code.
extern spawn_temp_t st;



/**
*
*
*   Pusher/Mover- Move Info Data Structures:
*
*
**/
/**
*   @brief  Stores movement info for 'Pushers(also known as Movers)'.
***/
typedef struct {
    // Fixed Data.
    Vector3     start_origin;
    Vector3     start_angles;
    Vector3     end_origin;
    Vector3     end_angles;

    int         sound_start;
    int         sound_middle;
    int         sound_end;

    float       accel;
    float       speed;
    float       decel;
    float       distance;

    float       wait;

    // Dynamic State Data
    int         state;
    Vector3     dir;
    float       current_speed;
    float       move_speed;
    float       next_speed;
    float       remaining_distance;
    float       decel_distance;
    void        (*endfunc)(edict_t *);

    // WID: MoveWith:
    Vector3 lastVelocity;
} g_pusher_moveinfo_t;

/**
*   @brief  Data for each 'movement' frame of a monster.
**/
struct mframe_t {
    void    (*aifunc)(edict_t *self, float dist);
    float   dist = 0;
    void    (*thinkfunc)(edict_t *self);
	int64_t lerp_frame = -1;
};

/**
*   @brief  Monster movement information.
**/
typedef struct {
    int64_t     firstframe;
    int64_t     lastframe;
    mframe_t    *frame;
    void        (*endfunc)(edict_t *self);
} mmove_t;

/**
*   @brief  Monster Info :-)
**/
typedef struct {
    mmove_t     *currentmove;
	mmove_t		*nextmove;
    int         aiflags;
    int64_t     nextframe;	// if next_move is set, this is ignored until a frame is ran
    float       scale;

    void        (*stand)(edict_t *self);
    void        (*idle)(edict_t *self);
    void        (*search)(edict_t *self);
    void        (*walk)(edict_t *self);
    void        (*run)(edict_t *self);
    void        (*dodge)(edict_t *self, edict_t *other, float eta);
    void        (*attack)(edict_t *self);
    void        (*melee)(edict_t *self);
    void        (*sight)(edict_t *self, edict_t *other);
    bool        (*checkattack)(edict_t *self);

	sg_time_t	next_move_time; // high tick rate

    sg_time_t	pause_time;
    sg_time_t	attack_finished;
	sg_time_t	fire_wait;

    vec3_t      saved_goal;
	sg_time_t	search_time;
	sg_time_t	trail_time;
    vec3_t      last_sighting;
    int         attack_state;
    int         lefty;
    sg_time_t	idle_time;
    int         linkcount;
} monsterinfo_t;

// Extern access.
extern  edict_t         *g_edicts;

#define FOFS(x) q_offsetof(edict_t, x)
#define STOFS(x) q_offsetof(spawn_temp_t, x)
#define LLOFS(x) q_offsetof(level_locals_t, x)
#define GLOFS(x) q_offsetof(game_locals_t, x)
#define CLOFS(x) q_offsetof(gclient_t, x)

#define random()    frand()
#define crandom()   crand()

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

#define world   (&g_edicts[0])

// item spawnflags
#define ITEM_TRIGGER_SPAWN      0x00000001
#define ITEM_NO_TOUCH           0x00000002
// 6 bits reserved for editor flags
// 8 bits used as power cube id bits for coop games
#define DROPPED_ITEM            0x00010000
#define DROPPED_PLAYER_ITEM     0x00020000
#define ITEM_TARGETS_USED       0x00040000

extern  gitem_t itemlist[];

//
// g_gamemode.cpp
//
/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool G_GetGamemodeNoSaveGames( const bool isDedicatedServer );

//
// g_cmds.c
//
void Cmd_Score_f(edict_t *ent);

//
// g_items.c
//
void PrecacheItem( const gitem_t *it);
void InitItems(void);
void SetItemNames(void);
const gitem_t *FindItem(const char *pickup_name);
const gitem_t *FindItemByClassname(const char *classname);
#define ITEM_INDEX(x) ((x)-itemlist)
edict_t *Drop_Item(edict_t *ent, const gitem_t *item);
void SetRespawn(edict_t *ent, float delay);
void SpawnItem(edict_t *ent, const gitem_t *item);
int PowerArmorType(edict_t *ent);
const gitem_t *GetItemByIndex(int index);
const bool Add_Ammo(edict_t *ent, const gitem_t *item, const int32_t count);
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

//
// g_utils.c
//
/**
*   @brief
**/
const bool    KillBox( edict_t *ent, const bool bspClipping );

/**
*   @brief
**/
void G_MoveWith_AdjustToParent( const Vector3 &parentOriginDelta, edict_t *parentMover, edict_t *childMover );
/**
*   @brief
**/
//void G_MoveWith_Init( edict_t *self, edict_t *parent );
/**
*   @brief
**/
void G_MoveWith_SetChildEntityMovement( edict_t *self );
/**
*   @note   At the time of calling, parent entity has to reside in its default state.
*           (This so the actual offsets can be calculated easily.)
**/
void G_MoveWith_SetTargetParentEntity( const char *targetName, edict_t *parentMover, edict_t *childMover );

/**
*   @brief  Wraps up the new more modern G_ProjectSource.
**/
void    G_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result );
/**
*   @brief  Project vector from source. 
**/
const Vector3 G_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right );
edict_t *G_Find( edict_t *from, int fieldofs, const char *match ); // WID: C++20: Added const.
edict_t *findradius( edict_t *from, vec3_t org, float rad );
edict_t *G_PickTarget( char *targetname );
void    G_UseTargets( edict_t *ent, edict_t *activator );
void    G_SetMovedir( vec3_t angles, Vector3 &movedir );

void    G_InitEdict( edict_t *e );
edict_t *G_AllocateEdict( void );
void    G_FreeEdict( edict_t *e );

void    G_TouchTriggers( edict_t *ent );
void    G_TouchProjectiles( edict_t *ent, const Vector3 &previous_origin );
void    G_TouchSolids( edict_t *ent );

char *G_CopyString( char *in );

//
// g_combat.c
//
bool OnSameTeam( edict_t *ent1, edict_t *ent2 );
bool CanDamage( edict_t *targ, edict_t *inflictor );
void T_Damage( edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t dir, vec3_t point, const vec3_t normal, const int32_t damage, const int32_t knockBack, const int32_t dflags, const sg_means_of_death_t meansOfDeath );
void T_RadiusDamage( edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, const sg_means_of_death_t meansOfDeath );

// damage flags
#define DAMAGE_NONE             BIT( 0 )
#define DAMAGE_RADIUS           BIT( 1 )  // Damage was indirect.
#define DAMAGE_NO_ARMOR         BIT( 2 )  // Armour does not protect from this damage.
#define DAMAGE_ENERGY           BIT( 3 )  // Damage is from an energy based weapon.
#define DAMAGE_NO_KNOCKBACK     BIT( 4 )  // Do not affect velocity, just view angles.
#define DAMAGE_BULLET           BIT( 5 )  // Damage is from a bullet (used for ricochets).
#define DAMAGE_NO_PROTECTION    BIT( 6 )  // Armor, shields, invulnerability, and godmode have no effect.
#define DAMAGE_NO_INDICATOR     BIT( 7 )  // For clients: No damage indicators.

#define DEFAULT_BULLET_HSPREAD  300
#define DEFAULT_BULLET_VSPREAD  500
#define DEFAULT_SHOTGUN_HSPREAD 1000
#define DEFAULT_SHOTGUN_VSPREAD 500
#define DEFAULT_DEATHMATCH_SHOTGUN_COUNT    12
#define DEFAULT_SHOTGUN_COUNT   12
#define DEFAULT_SSHOTGUN_COUNT  20

//
// g_monster.c
//
void monster_fire_bullet( edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype );
void monster_fire_shotgun( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype );
void M_droptofloor( edict_t *ent );
void M_CatagorizePosition( edict_t *ent, const Vector3 &in_point, liquid_level_t &liquidlevel, contents_t &liquidtype );
void M_CheckGround( edict_t *ent, const contents_t mask );
void M_WorldEffects( edict_t *ent );
void M_SetAnimation( edict_t *self, mmove_t *move, bool instant = true );


//
// g_misc.c
//
// WID: C++20: Added const.
void ThrowHead( edict_t *self, const char *gibname, int damage, int type );
void ThrowClientHead( edict_t *self, int damage );
// WID: C++20: Added const.
void ThrowGib( edict_t *self, const char *gibname, int damage, int type );
void BecomeExplosion1( edict_t *self );

#define CLOCK_MESSAGE_SIZE  16
void func_clock_think( edict_t *self );
void func_clock_use( edict_t *self, edict_t *other, edict_t *activator );

//
// g_ai.c
//
//void AI_SetSightClient(void);
//
//void ai_stand(edict_t *self, float dist);
//void ai_move(edict_t *self, float dist);
//void ai_walk(edict_t *self, float dist);
//void ai_turn(edict_t *self, float dist);
//void ai_run(edict_t *self, float dist);
//void ai_charge(edict_t *self, float dist);
//int range(edict_t *self, edict_t *other);
//
//void FoundTarget(edict_t *self);
//bool infront(edict_t *self, edict_t *other);
//bool visible(edict_t *self, edict_t *other);
//bool FacingIdeal(edict_t *self);

//
// g_weapon.c
//
// WID: C++20: Added const.
void ThrowDebris( edict_t *self, const char *modelname, float speed, vec3_t origin );
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

//
// g_ptrail.c
//
void PlayerTrail_Init( void );
void PlayerTrail_Add( vec3_t spot );
void PlayerTrail_New( vec3_t spot );
edict_t *PlayerTrail_PickFirst( edict_t *self );
edict_t *PlayerTrail_PickNext( edict_t *self );
edict_t *PlayerTrail_LastSpot( void );

//
// g_client.c
//
void respawn( edict_t *ent );
void BeginIntermission( edict_t *targ );
/**
*   @brief  Will reset the entity client's 'Field of View' back to its defaults.
**/
void P_ResetPlayerStateFOV( gclient_t *client );
void PutClientInServer( edict_t *ent );
void InitClientPersistantData( edict_t *ent, gclient_t *client );
void InitClientRespawnData( gclient_t *client );
void InitBodyQue( void );
void ClientBeginServerFrame( edict_t *ent );

//
// g_player.c
//
void player_pain( edict_t *self, edict_t *other, float kick, int damage );
void player_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );

//
// g_svcmds.c
//
void ServerCommand( void );
bool SV_FilterPacket( char *from );

//
// p_view.c
//
void ClientEndServerFrame( edict_t *ent );

//
// p_hud.c
//
void MoveClientToIntermission( edict_t *client );
void G_SetStats( edict_t *ent );
void G_SetSpectatorStats( edict_t *ent );
void G_CheckChaseStats( edict_t *ent );
void ValidateSelectedItem( edict_t *ent );
void DeathmatchScoreboardMessage( edict_t *client, edict_t *killer );

//
// g_pweapon.c
//
/**
*   @brief  Wraps up the new more modern G_ProjectSource.
**/
void    G_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result );
/**
*   @brief  Project vector from source.
**/
const Vector3 G_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right );
/**
*   @brief  Wraps up the new more modern P_ProjectDistance.
**/
void P_ProjectDistance( edict_t *ent, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result );
/**
*   @brief Project the 'ray of fire' from the source to its (source + dir * distance) target.
**/
const Vector3 P_ProjectDistance( edict_t *ent, Vector3 &point, Vector3 &distance, Vector3 &forward, Vector3 &right );
void P_ProjectSource( edict_t *ent, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result );
void P_PlayerNoise( edict_t *who, const vec3_t where, int type );

/**
*   @brief  Acts as a sub method for cleaner code, used by weapon item animation data precaching.
**/
void P_Weapon_ModeAnimationFromSKM( weapon_item_info_t *itemInfo, const skm_anim_t *iqmAnim, const int32_t modeID, const int32_t iqmAnimationID );
/**
*   @brief
**/
const bool P_Weapon_Pickup( edict_t *ent, edict_t *other );
/**
*   @brief
**/
void P_Weapon_Drop( edict_t *ent, const gitem_t *inv );
/**
*   @brief
**/
void P_Weapon_Use( edict_t *ent, const gitem_t *inv );
/**
*   @brief
**/
void P_Weapon_Change( edict_t *ent );
/**
*   @brief  Will switch the weapon to its 'newMode' if it can, unless enforced(force == true).
**/
void P_Weapon_SwitchMode( edict_t *ent, const weapon_mode_t newMode, const weapon_mode_animation_t *weaponModeAnimations, const bool force );
/**
*   @brief  Advances the animation of the 'mode' we're currently in.
**/
const bool P_Weapon_ProcessModeAnimation( edict_t *ent, const weapon_mode_animation_t *weaponModeAnimation );
/**
*   @brief
**/
void P_Weapon_Think( edict_t *ent, const bool processUserInputOnly );


//
// g_phys.c
//
void SV_Impact( edict_t *e1, trace_t *trace );
const contents_t G_GetClipMask( edict_t *ent );
void G_RunEntity( edict_t *ent );


//
// g_main.c
//
void SaveClientData( void );
void FetchClientEntData( edict_t *ent );


//
// g_chase.c
//
void UpdateChaseCam( edict_t *ent );
void ChaseNext( edict_t *ent );
void ChasePrev( edict_t *ent );
void GetChaseTarget( edict_t *ent );

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
    //! The actual BSP liquid 'contents' type we're residing in.
    contents_t      type;
    //! The depth of the player in the actual liquid.
    liquid_level_t	level;
} mm_liquid_info_t;




//============================================================================
// client_t->anim_priority
#define ANIM_BASIC      0       // stand / run
#define ANIM_WAVE       1
#define ANIM_JUMP       2
#define ANIM_PAIN       3
#define ANIM_ATTACK     4
#define ANIM_DEATH      5
#define ANIM_REVERSED   6		// animation is played in reverse.


/**
*   @brief  Client data that persists to exist across multiple level loads.
**/
typedef struct {
    //! String buffer of the client's user info.
    char        userinfo[MAX_INFO_STRING];
    //! The 'nickname' this client has for display.
    char        netname[16];
    //! The 'hand' we're holding our weapon with.
    int32_t     hand;

    //! A loadgame will leave valid entities that just don't have a connection yet.
    bool        connected;
    //! Stores whether spawned or not. 
    bool        spawned;

    /**
    *   Values saved and restored from edicts when changing levels
    **/
    //! Current health.
    int32_t         health;
    //! Maximum allowed health.
    int32_t         max_health;
    //! Saved entity flags.
    ent_flags_t     savedFlags;

    //! The currently selected item.
    int32_t         selected_item;
    //! A simple integer count for inventory of all item IDs.
    int32_t         inventory[MAX_ITEMS];


    //! A pointer to the item matching the currently 'used' ACTIVE weapon.
    const gitem_t *weapon;
    //! A pointer to the last(previously used) matching weapon item.
    const gitem_t *lastweapon;
    //! Stores the current clip ammo for each weapon that uses it.
    int32_t weapon_clip_ammo[ MAX_ITEMS ];

    //! Maximum carry ammo capacities.
    struct {
        //! Pistol Bullets.
        int32_t pistol;
        //! Rifle Bullets.
        int32_t rifle;
        //! SMG Bullets.
        int32_t smg;
        //! Sniper Bullets.
        int32_t sniper;
        //! Shotgun Shells.
        int32_t shotgun;
    } ammoCapacities;

    //! For calculating total unit score in coop games.
    int32_t     score;

    //! If true, this client is engaged in 'Spectator' mode.
    bool        spectator;
} client_persistant_t;

/**
*   @brief  Client respawn data that stays across multiplayer mode respawns.
**/
typedef struct {
    client_persistant_t pers_respawn;	// what to set client->pers to on a respawn

    int64_t enterframe;     // level.framenum the client entered the game
    sg_time_t entertime;    // the moment in time the client entered the game.

    int32_t score;      // Frags, etc
    vec3_t cmd_angles;  // Angles sent over in the last command.

    bool spectator;     // Client is a spectator
} client_respawn_t;

// this structure is cleared on each PutClientInServer(),
// except for 'client->pers'
struct gclient_s {
    /**
    *	Known and Shared with the Server:
    /**/
    player_state_t  ps;             // communicated by server to clients
    player_state_t  ops;            // old player state from the previous frame.
    int64_t         ping;			// WID: 64-bit-frame

	/**
    *	Private to game:
	**/
    client_persistant_t pers;
    client_respawn_t    resp;
    pmove_state_t       old_pmove;  // for detecting out-of-pmove changes

    /**
    *	Layout(s):
    **/
    bool        showscores;         //! Set layout stat
    bool        showinventory;      //! Set layout stat
    bool        showhelp;
    bool        showhelpicon;


    /**
    *	User Imput:
    **/
    int32_t         buttons;
    int32_t         oldbuttons;
    int32_t         latched_buttons;

	/**
	*	Weapon Related:
	**/
	// Actual current weapon ammo type, inventory index.
    int32_t	ammo_index;

	// Set when we want to switch weapons.
	const gitem_t *newweapon;

    //! If true, the weapon thinking process has been executed by a 
    //! usercmd_t in ClientThink. Otherwise it'll be dealt with by the
    //! ClientBeginServerFrame instead.
	bool weapon_thunk;

	sg_time_t	grenade_time;
	sg_time_t	grenade_finished_time;
	bool        grenade_blew_up;


	/**
	*	Damage Related:
	**/
    // sum up damage over an entire frame, so shotgun and/or other blasts 
    // give a single big kick.
    struct {
        int32_t     armor;       //! Damage absorbed by armor.
        int32_t     blood;       //! Damage taken out of health.
        int32_t     knockBack;   //! Damage taken out of health
        Vector3     from;        //! Origin for vector calculation.
        //! Damage indicators.
        damage_indicator_t		  damage_indicators[ MAX_DAMAGE_INDICATORS ];
        //! Number of current frame damage indicators.
        uint8_t                   num_damage_indicators;
    } frameDamage;

    //! Yaw direction towards the killer entity.
    float   killer_yaw;         //! When dead, look at killer.

    /**
    *   Weapon State:
    **/
    struct weapon_state_s {
        //! The 'mode' is what the weapon is actually doing during its current 'state'.
        weapon_mode_t mode;
        //! The 'old mode' is what the weapon was actually doing during its previous 'state'.
        weapon_mode_t oldMode;
        
        //! True if the mode has changed for the current caller of weaponthink(ClientThink/BeginServerFrame)
        //! that executed weapon logic. It sets the weapon animationID of the player state, with a toggle bit
        //! in case of it finding itself in repetitive mode changes.
        //!
        //! In other words, this allows the client to be made aware of when an animation changed OR restarted.
        qboolean updatePlayerStateAnimationID;

        //! Determines if the weapon can change 'mode'.
        qboolean canChangeMode;
        //! If set, will be applied to the client entity's state sound member.
        int32_t activeSound;

        //! State for 'Aiming' weapon modes.
        struct {
            //! If true, the weapon is using secondary fire to engage in 'aim' mode.
            qboolean isAiming;
        } aimState;

        //! Stores the 'Weapon Animation' data, which if still actively being processed
        //! prevents the weapon from changing 'mode'.
        struct {
            //! Model AnimationID.
            uint8_t modelAnimationID;
            
            //! The frame matching for the time processed so far with the current animation.
            int32_t currentFrame;

            //! The starting frame.
            int32_t startFrame;
            //! The frame we want to be at by the end of the animation.
            int32_t endFrame;

            //! Start time of animation.
            sg_time_t startTime;
            //! End time of animation.
            sg_time_t endTime;
            
            //! Current time of animation relative to startTime.
            sg_time_t currentTime;

            //! Optional callback function pointer.
            //void ( *finished_animating )( edict_t *ent );
        } animation;

        //! Recoil.
        struct {
            //! Amount
            float amount;
            //! Total accumulated 'ms' by rapidly firing.
            sg_time_t accumulatedTime;
        } recoil;

        //! Timers
        struct {
            //! Last time we played an 'empty weapon click' sound.
            sg_time_t lastEmptyWeaponClick;

            //! Used to prevent firing too rapidly
            sg_time_t lastPrimaryFire;
            //! Used to prevent firing too rapidly
            sg_time_t lastSecondaryFire;
            //! Used to prevent firing too rapidly.
            sg_time_t lastAimedFire;
            //! Time the weapon was drawn. (Used for sound).
            sg_time_t lastDrawn;
            //! Time the weapon was holstered. (Used for sound).
            sg_time_t lastHolster;
        } timers;
    } weaponState;
    

    /**
    *   View Angles/Movement/Offset:
    **/
    //! Summed Weapon Kicks, to be applied to client 'viewoffset' and 'kick_angles'.
    struct {
        //! Additional weapon 'Kick Effect' angles to be added to playerState.kick_angles.
        Vector3 offsetAngles;
        //! Additional weapon 'Kick Effect' origin to be added to viewOffset.
        Vector3 offsetOrigin;
    } weaponKicks;
    //! Stores view movement related information.
    struct {
        //! Aiming direction.
        Vector3 viewAngles, viewForward;

        // View Movement Timers:
        sg_time_t	damageTime;
        sg_time_t	fallTime;
        sg_time_t	quakeTime;
        //! View Damage Kicks.
        float       damageRoll, damagePitch;
        //! For view drop on fall.
        float		fallValue;
    } viewMove;


    /**
    *   View Blends:
    **/
    float       damage_alpha;
    float       bonus_alpha;
    vec3_t      damage_blend;
    int64_t     bobCycle;
    int64_t     oldBobCycle;
    double      bobFracSin;
    //float       bobtime;            // Store it, so we know where we're at (To Prevent off-ground from changing it).
    
    /**
    *   Old/Previous frames data:
    **/
    uint64_t    last_stair_step_frame;

    vec3_t last_ladder_pos; // For ladder step sounds.
    sg_time_t last_ladder_sound;

    vec3_t      oldviewangles;
    vec3_t      oldvelocity;
    edict_t     *oldgroundentity; // [Paril-KEX]
    liquid_level_t	old_waterlevel;
    sg_time_t       flash_time; // [Paril-KEX] for high tickrate


	/**
	*   Misc:
	**/
	sg_time_t		next_drown_time;

	/**
    *	Animation Related:
	**/
    //! Used for 'Humanoid Skeletal Model' entities. (Monsters, Players, ...)
    //! The body states are encoded into the 'frame' state value.
    //! Each body state is made out of 9 bits, where the 9th bit is the animation toggle flag.
    sg_skm_animation_mixer_t animationMixer;


	/**
	*	Item/Use Event Timers:
	**/
	//sg_time_t	quad_time;
	//sg_time_t	invincible_time;
	//sg_time_t	breather_time;
	//sg_time_t	enviro_time;

    sg_time_t	pickup_msg_time;

    /**
    *	Chat Flood Related:
    **/
    sg_time_t	flood_locktill;     // locked from talking
    sg_time_t	flood_when[10];     // when messages were said
    int64_t		flood_whenhead;     // head pointer for when said

    sg_time_t	respawn_time;		// can respawn when time > this

    edict_t     *chase_target;      // player we are chasing
    bool        update_chase;       // need to update chase info?
};


struct edict_s {
    entity_state_t  s;
    struct gclient_s *client;   //! NULL if not a player the server expects the first part
                                //! of gclient_s to be a player_state_t but the rest of it is opaque
    qboolean inuse;
    int32_t linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t area;    //! Linked to a division node or leaf

    int32_t num_clusters;       // If -1, use headnode instead.
    int32_t clusternums[MAX_ENT_CLUSTERS];
    int32_t headnode;           // Unused if num_clusters != -1
    
    int32_t areanum, areanum2;

    //================================

    int32_t     svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    contents_t  clipmask;
    contents_t  hullContents;
    edict_t     *owner;

    const cm_entity_t *entityDictionary;

    /**
    *   DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    *   EXPECTS THE FIELDS IN THAT ORDER!
    **/
    /**
    *
    *
    * The following fields are only used locally in game, not by server.
    * 
    * 
    **/
    //! Used for projectile skip checks and in general for checking if the entity has happened to been respawned.
    int32_t     spawn_count;
    //! sv.time when the object was freed
    sg_time_t   freetime;
    //! Used for deferring client info so that disconnected, etc works
    sg_time_t	timestamp;

    //! Entity spawnflags key/value.
    int32_t     spawnflags;
    //! Entity classname key/value.
    const char  *classname;
    //! Path to model.
    const char  *model;
    //! Message to display, usually center printed. (When triggers are triggered etc.)
    char        *message;
    // Key Spawn Angle.
    float       angle;          // set in qe3, -1 = up, -2 = down

    //! Entity flags.
    ent_flags_t flags;


    //
    // Target Fields:
    //
    const char *targetname;
    struct {
        char *target;
        char *kill;
        char *team;
        char *path;
        char *death;
        char *combat;
        const char *movewith;
    } targetNames;

    //
    // Target Entities:
    //
    struct {
        //! The default 'target' entity.
        edict_t *target;
        //! The parent entity to move along with.
        edict_t *movewith;
        //! Next child in the list of this 'movewith group' chain.
        edict_t *movewith_next;
    } targetEntities;

    //
    // Lua Properties:
    //
    struct {
        //! The name which its script methods are prepended by.
        const char *luaName;
    } luaProperties;

    //
    // Physics Related:
    //
    //! For move with parent entities.
    struct {
        //! Initial absolute origin of child.
        Vector3 absoluteOrigin;
        //! Initial origin offset between parent and child.
        Vector3 absoluteParentOriginOffset;
        //! Relative delta offset to the absoluteOriginOffset.
        Vector3 relativeDeltaOffset;

        //! Initial delta angles between parent and child.
        Vector3 spawnDeltaAngles;
        //! Initial angles set during spawn time.
        Vector3 spawnParentAttachAngles;
    } moveWith;

    //! Specified physics movetype.
    int32_t     movetype;
    //! Move velocity.
    Vector3     velocity;
    //! Angular(Move) Velocity.
    Vector3     avelocity;
    //! Weight(mass) of entity.
    int32_t     mass;
	//! Per entity gravity multiplier (1.0 is normal) use for lowgrav artifact, flares.
    float       gravity;        
    //! Categorized liquid entity state.
    mm_liquid_info_t liquidInfo;
    //! Categorized ground information.
    mm_ground_info_t groundInfo;


    //
    // Not per se, but mostly used for Pushers(Movers):
    //
    g_pusher_moveinfo_t pusherMoveInfo;
    float   speed;
    float   accel;
    float   decel;
    Vector3 movedir;
    Vector3 pos1;
    Vector3 pos2;
    Vector3 lastOrigin;
    Vector3 lastAngles;
    edict_t *movetarget;

    //
    // NextThinkg + Entity Callbacks:
    //
    sg_time_t   nextthink;
    void        ( *postspawn )( edict_t *ent );
    void        ( *prethink )( edict_t *ent );
    void        ( *think )( edict_t *self );
    void        ( *postthink )( edict_t *ent );
    void        ( *blocked )( edict_t *self, edict_t *other );         // move to moveinfo?
    void        ( *touch )( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
    void        ( *use )( edict_t *self, edict_t *other, edict_t *activator );
    void        ( *pain )( edict_t *self, edict_t *other, float kick, int damage );
    void        ( *die )( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );


    //
    //  Entity Pointers:
    //
    //! Will be nullptr or world if not currently angry at anyone.
    edict_t *enemy;
    //! Previous Enemy.
    edict_t *oldenemy;
    //! The next path spot to walk toward.If.enemy, ignore.movetarget. When an enemy is killed, the monster will try to return to it's path.
    edict_t *goalentity;

    //! Chain Entity.
    edict_t *chain;
    //! Team Chain.
    edict_t *teamchain;
    //! Team master.
    edict_t *teammaster;

    //! Trigger Activator.
    edict_t *activator;
    //! WID: LUA: Other Trigger.
    edict_t *other;

    //
    //  Player Noise/Trail:
    //
    //! Pointer to noise entity.
    edict_t *mynoise;       // can go in client only
    edict_t *mynoise2;
    //! Noise indexes.
    int32_t     noise_index;
    int32_t     noise_index2;


    //
    //  Sound:
    //
    float       volume;
    float       attenuation;
    sg_time_t   last_sound_time;


    //
    // Trigger(s):
    //
    float       wait;
    float       delay;          // before firing targets
    float       random;


    //
    // Timers:
    //
    sg_time_t   air_finished_time;
    sg_time_t   damage_debounce_time;
    sg_time_t   fly_sound_debounce_time;    // move to clientinfo
    sg_time_t   last_move_time;
	sg_time_t   touch_debounce_time;        // are all these legit?  do we need more/less of them?
	sg_time_t   pain_debounce_time;
    sg_time_t   show_hostile;


    //
    // Various.
    //
    //! Set when the entity gets hurt(T_Damage) and might be its cause of death.
    sg_means_of_death_t meansOfDeath;
    //! Used for target_changelevel. Set as key/value.
    const char *map;

    //! The entity's height above its 'origin', used to state where eyesight is determined.
    int32_t     viewheight;
    //! To take damage or not.
    int32_t     takedamage;
    //! Damage entity will do.
    int32_t     dmg;
    //! Size of the radius where entities within will be damaged.
    int32_t     radius_dmg;
    float       dmg_radius;
    //! Light
	float		light;
    //! For certain pushers/movers to use sounds or not. -1 usually is not, sometimes a positive value indices the set.
    int32_t     sounds;
    //! Count of ... whichever depends on entity.
    int32_t     count;


    //
    // Health Conditions:
    //
    int32_t     health;
    int32_t     max_health;
    int32_t     gib_health;
    int32_t     deadflag;

    //
    //  Lights:
    // 
    //! (Light-)Style.
    int32_t     style;          // also used as areaportal number
	const char  *customLightStyle;	// It is as it says.


    //
    //  Items: 
    // 
    //! If not nullptr, will point to one of the items in the itemlist.
    const gitem_t *item;          // for bonus items


    //
    //  Monster Related:
    // 
    
    //! How many degrees the yaw should rotate per frame in order to reach its 'ideal_yaw'.
    float   yaw_speed;
    //! Ideal yaw to face to.
    float   ideal_yaw;


    // Only used for g_turret.cpp - WID: Remove?
    Vector3 move_origin;
    Vector3 move_angles;
};

