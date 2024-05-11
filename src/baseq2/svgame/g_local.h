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

// extern times.
extern sg_time_t FRAME_TIME_S;
extern sg_time_t FRAME_TIME_MS;

// For backwards compatibilities.
#define FRAMETIME BASE_FRAMETIME_1000 // OLD: 0.1f	NEW: 40hz makes for 0.025f

// TODO: Fix the whole max shenanigan in shared.h,  because this is wrong...
#undef max

// Just to, hold time, forever.
constexpr sg_time_t HOLD_FOREVER = sg_time_t::from_ms( std::numeric_limits<int64_t>::max( ) );

//==================================================================
// 
//==================================================================
// features this game supports
#define G_FEATURES  (GMF_PROPERINUSE|GMF_WANT_ALL_DISCONNECTS)

// the "gameversion" client command will print this plus compile date
#define GAMEVERSION "BaseQ2"


//==================================================================
// Q2RE: Random Number Utilities
//==================================================================
extern std::mt19937 mt_rand;

// uniform float [0, 1)
[[nodiscard]] inline float frandom( ) {
	return std::uniform_real_distribution<float>( )( mt_rand );
}

// uniform float [min_inclusive, max_exclusive)
[[nodiscard]] inline float frandom( float min_inclusive, float max_exclusive ) {
	return std::uniform_real_distribution<float>( min_inclusive, max_exclusive )( mt_rand );
}

// uniform float [0, max_exclusive)
[[nodiscard]] inline float frandom( float max_exclusive ) {
	return std::uniform_real_distribution<float>( 0, max_exclusive )( mt_rand );
}

// uniform time [min_inclusive, max_exclusive)
[[nodiscard]] inline sg_time_t random_time( sg_time_t min_inclusive, sg_time_t max_exclusive ) {
	return sg_time_t::from_ms( std::uniform_int_distribution<int64_t>( min_inclusive.milliseconds( ), max_exclusive.milliseconds( ) )( mt_rand ) );
}

// uniform time [0, max_exclusive)
[[nodiscard]] inline sg_time_t random_time( sg_time_t max_exclusive ) {
	return sg_time_t::from_ms( std::uniform_int_distribution<int64_t>( 0, max_exclusive.milliseconds( ) )( mt_rand ) );
}

// uniform float [-1, 1)
// note: closed on min but not max
// to match vanilla behavior
[[nodiscard]] inline float crandom( ) {
	return std::uniform_real_distribution<float>( -1.f, 1.f )( mt_rand );
}

// uniform float (-1, 1)
[[nodiscard]] inline float crandom_open( ) {
	return std::uniform_real_distribution<float>( std::nextafterf( -1.f, 0.f ), 1.f )( mt_rand );
}

// raw unsigned int32 value from random
[[nodiscard]] inline uint32_t irandom( ) {
	return mt_rand( );
}

// uniform int [min, max)
// always returns min if min == (max - 1)
// undefined behavior if min > (max - 1)
[[nodiscard]] inline int32_t irandom( int32_t min_inclusive, int32_t max_exclusive ) {
	if ( min_inclusive == max_exclusive - 1 )
		return min_inclusive;

	return std::uniform_int_distribution<int32_t>( min_inclusive, max_exclusive - 1 )( mt_rand );
}

// uniform int [0, max)
// always returns 0 if max <= 0
// note for Q2 code:
// - to fix rand()%x, do irandom(x)
// - to fix rand()&x, do irandom(x + 1)
[[nodiscard]] inline int32_t irandom( int32_t max_exclusive ) {
	if ( max_exclusive <= 0 )
		return 0;

	return irandom( 0, max_exclusive );
}

// uniform random index from given container
template<typename T>
[[nodiscard]] inline int32_t random_index( const T &container ) {
	return irandom( std::size( container ) );
}

// uniform random element from given container
template<typename T>
[[nodiscard]] inline auto random_element( T &container ) -> decltype( *std::begin( container ) ) {
	return *( std::begin( container ) + random_index( container ) );
}

// flip a coin
[[nodiscard]] inline bool brandom( ) {
	return irandom( 2 ) == 0;
}

//==================================================================
// Q2RE View Pitching Times:
//==================================================================
//#define DAMAGE_TIME     0.5f
//#define FALL_TIME       0.3f
// view pitching times
static inline constexpr sg_time_t DAMAGE_TIME_SLACK( ) {
	return ( 100_ms - FRAME_TIME_MS );
}

static inline constexpr sg_time_t DAMAGE_TIME( ) {
	return 500_ms + DAMAGE_TIME_SLACK( );
}

static inline constexpr sg_time_t FALL_TIME( ) {
	return 300_ms + DAMAGE_TIME_SLACK( );
}

//! Time between ladder sounds.
static constexpr sg_time_t LADDER_SOUND_TIME = 375_ms;

//==================================================================
// 
//==================================================================
// edict->spawnflags
// these are set with checkboxes on each entity in the map editor
#define SPAWNFLAG_NOT_EASY          0x00000100
#define SPAWNFLAG_NOT_MEDIUM        0x00000200
#define SPAWNFLAG_NOT_HARD          0x00000400
#define SPAWNFLAG_NOT_DEATHMATCH    0x00000800
#define SPAWNFLAG_NOT_COOP          0x00001000

// edict->flags
typedef enum {
    FL_NONE                 = 0,
    FL_FLY                  = BIT( 1 ),
    FL_SWIM                 = BIT( 2 ),// implied immunity to drowining
    FL_IMMUNE_LASER         = BIT( 3 ),
    FL_INWATER              = BIT( 4 ),
    FL_GODMODE              = BIT( 5 ),
    FL_NOTARGET             = BIT( 6 ),
    FL_DODGE                = BIT( 7 ), // monster should try to dodge this
    FL_IMMUNE_SLIME         = BIT( 8 ),
    FL_IMMUNE_LAVA          = BIT( 9 ),
    FL_PARTIALGROUND        = BIT( 10 ),// not all corners are valid
    FL_WATERJUMP            = BIT( 11 ),// player jumping out of water
    FL_TEAMSLAVE            = BIT( 12 ),// not the first on the team
    FL_NO_KNOCKBACK         = BIT( 13 ),
    FL_POWER_ARMOR          = BIT( 14 ),// power armor (if any) is active
    FL_RESPAWN              = BIT( 15 ) // used for item respawning
} ent_flags_t;


/**
*	Memory tag IDs to allow dynamic memory to be cleaned up.
**/
#define TAG_SVGAME			765 // Clear when unloading the dll.
#define TAG_SVGAME_LEVEL	766 // Clear when loading a new level.


#define MELEE_DISTANCE  80

#define BODY_QUEUE_SIZE     8

typedef enum {
    DAMAGE_NO,
    DAMAGE_YES,         // will take damage if hit
    DAMAGE_AIM          // auto targeting recognizes this
} damage_t;

typedef enum {
    WEAPON_READY,
    WEAPON_ACTIVATING,
    WEAPON_DROPPING,
    WEAPON_FIRING
} weaponstate_t;

typedef enum {
    AMMO_BULLETS,
    AMMO_SHELLS,
    AMMO_ROCKETS,
    AMMO_GRENADES,
    AMMO_CELLS,
    AMMO_SLUGS
} ammo_t;


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

// armor types
#define ARMOR_NONE              0
#define ARMOR_JACKET            1
#define ARMOR_COMBAT            2
#define ARMOR_BODY              3
#define ARMOR_SHARD             4

// power armor types
#define POWER_ARMOR_NONE        0
#define POWER_ARMOR_SCREEN      1
#define POWER_ARMOR_SHIELD      2

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
    MOVETYPE_FLY,
    MOVETYPE_TOSS,          // gravity
    MOVETYPE_FLYMISSILE,    // extra size to monsters
    MOVETYPE_BOUNCE
} movetype_t;



typedef struct {
    int     base_count;
    int     max_count;
    float   normal_protection;
    float   energy_protection;
    int     armor;
} gitem_armor_t;


// gitem_t->flags
#define IT_WEAPON       1       // use makes active weapon
#define IT_AMMO         2
#define IT_ARMOR        4
#define IT_STAY_COOP    8
#define IT_KEY          16
#define IT_POWERUP      32

// gitem_t->weapmodel for weapons indicates model index
#define WEAP_BLASTER            1
#define WEAP_SHOTGUN            2
#define WEAP_SUPERSHOTGUN       3
#define WEAP_MACHINEGUN         4
#define WEAP_CHAINGUN           5
#define WEAP_GRENADES           6
#define WEAP_GRENADELAUNCHER    7
#define WEAP_ROCKETLAUNCHER     8
#define WEAP_HYPERBLASTER       9
#define WEAP_RAILGUN            10
#define WEAP_BFG                11
#define WEAP_FLAREGUN           12

typedef struct gitem_s {
	const char        *classname; // spawning name
    bool        (*pickup)(struct edict_s *ent, struct edict_s *other);
    void        (*use)(struct edict_s *ent, struct gitem_s *item);
    void        (*drop)(struct edict_s *ent, struct gitem_s *item);
    void        (*weaponthink)(struct edict_s *ent);
	const char	*pickup_sound; // WID: C++20: Added const.
	const char	*world_model; // WID: C++20: Added const.
    int         world_model_flags;
	const char	*view_model; // WID: C++20: Added const.

    // client side info
	const char	*icon; // WID: C++20: Added const.
	const char	*pickup_name;   // for printing on pickup	// WID: C++20: Added const.
    int         count_width;    // number of digits to display by icon

    int         quantity;       // for ammo how much, for weapons how much is used per shot
	const char	*ammo;          // for weapons	// WID: C++20: Added const.
    int         flags;          // IT_* flags

    int         weapmodel;      // weapon model index (for weapons)

    void        *info;
    int         tag;

    const char	*precaches;     // string of all models, sounds, and images this item will use	// WID: C++20: Added const.
} gitem_t;



//
// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
//
typedef struct {
    char        helpmessage1[512];
    char        helpmessage2[512];
    int         helpchanged;    // flash F1 icon if non 0, play sound
                                // and increment only if 1, 2, or 3

    gclient_t   *clients;       // [maxclients]

    // can't store spawnpoint in level, because
    // it would get overwritten by the savegame restore
    char        spawnpoint[512];    // needed for coop respawns

    // store latched cvars here that we want to get at often
    int         maxclients;
    int         maxentities;
    int         gamemode;
    // cross level triggers
    int         serverflags;

    // items
    int         num_items;

    bool        autosaved;
} game_locals_t;


//
// this structure is cleared as each map is entered
// it is read/written to the level.sav file for savegames
//
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

    int         power_cubes;        // ugly necessity for coop
} level_locals_t;


// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in edict_t during gameplay
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


typedef struct {
    // fixed data
    vec3_t      start_origin;
    vec3_t      start_angles;
    vec3_t      end_origin;
    vec3_t      end_angles;

    int         sound_start;
    int         sound_middle;
    int         sound_end;

    float       accel;
    float       speed;
    float       decel;
    float       distance;

    float       wait;

    // state data
    int         state;
    vec3_t      dir;
    float       current_speed;
    float       move_speed;
    float       next_speed;
    float       remaining_distance;
    float       decel_distance;
    void        (*endfunc)(edict_t *);
} moveinfo_t;


struct mframe_t {
    void    (*aifunc)(edict_t *self, float dist);
    float   dist = 0;
    void    (*thinkfunc)(edict_t *self);
	int64_t lerp_frame = -1;
};

typedef struct {
    int64_t     firstframe;
    int64_t     lastframe;
    mframe_t    *frame;
    void        (*endfunc)(edict_t *self);
} mmove_t;

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

    int         power_armor_type;
    int         power_armor_power;
} monsterinfo_t;



extern  game_locals_t   game;
extern  level_locals_t  level;
extern  spawn_temp_t    st;

extern  int sm_meat_index;
extern  int snd_fry;

//extern  int jacket_armor_index;
//extern  int combat_armor_index;
//extern  int body_armor_index;


// means of death
#define MOD_UNKNOWN         0
#define MOD_BLASTER         1
#define MOD_SHOTGUN         2
#define MOD_SSHOTGUN        3
#define MOD_MACHINEGUN      4
#define MOD_CHAINGUN        5
#define MOD_GRENADE         6
#define MOD_G_SPLASH        7
#define MOD_ROCKET          8
#define MOD_R_SPLASH        9
#define MOD_HYPERBLASTER    10
#define MOD_RAILGUN         11
#define MOD_BFG_LASER       12
#define MOD_BFG_BLAST       13
#define MOD_BFG_EFFECT      14
#define MOD_HANDGRENADE     15
#define MOD_HG_SPLASH       16
#define MOD_WATER           17
#define MOD_SLIME           18
#define MOD_LAVA            19
#define MOD_CRUSH           20
#define MOD_TELEFRAG        21
#define MOD_FALLING         22
#define MOD_SUICIDE         23
#define MOD_HELD_GRENADE    24
#define MOD_EXPLOSIVE       25
#define MOD_BARREL          26
#define MOD_BOMB            27
#define MOD_EXIT            28
#define MOD_SPLASH          29
#define MOD_TARGET_LASER    30
#define MOD_TRIGGER_HURT    31
#define MOD_HIT             32
#define MOD_TARGET_BLASTER  33
#define MOD_FRIENDLY_FIRE   0x8000000

extern  int meansOfDeath;


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

extern cvar_t *flood_msgs;
extern cvar_t *flood_persecond;
extern cvar_t *flood_waitdelay;

extern cvar_t *g_select_empty;
extern cvar_t *g_instant_weapon_switch;

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
*	@return	True if the game mode is a legitimate existing one.
**/
const bool G_IsGamemodeIDValid( const int32_t gameModeID );
/**
*   @return True if the game mode is multiplayer.
**/
const bool G_IsMultiplayerGameMode( const int32_t gameModeID );
/**
*	@return	The current active game mode ID.
**/
const int32_t G_GetActiveGamemodeID();
/**
*	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
**/
const int32_t G_GetDefaultMultiplayerGamemodeID();
/**
*	@return	The actual ID of the current gamemode.
**/
const int32_t G_GetActiveGamemodeID( );
/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool G_GetGamemodeNoSaveGames( const bool isDedicatedServer );

//
// g_cmds.c
//
void Cmd_Help_f(edict_t *ent);
void Cmd_Score_f(edict_t *ent);

//
// g_items.c
//
void PrecacheItem(gitem_t *it);
void InitItems(void);
void SetItemNames(void);
gitem_t *FindItem(const char *pickup_name);
gitem_t *FindItemByClassname(const char *classname);
#define ITEM_INDEX(x) ((x)-itemlist)
edict_t *Drop_Item(edict_t *ent, gitem_t *item);
void SetRespawn(edict_t *ent, float delay);
void ChangeWeapon(edict_t *ent);
void SpawnItem(edict_t *ent, gitem_t *item);
void Think_Weapon(edict_t *ent);
int ArmorIndex(edict_t *ent);
int PowerArmorType(edict_t *ent);
gitem_t *GetItemByIndex(int index);
bool Add_Ammo(edict_t *ent, gitem_t *item, int count);
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

//
// g_utils.c
//
const bool    KillBox( edict_t *ent, const bool bspClipping );
void    G_ProjectSource(const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result);
edict_t *G_Find(edict_t *from, int fieldofs, const char *match); // WID: C++20: Added const.
edict_t *findradius(edict_t *from, vec3_t org, float rad);
edict_t *G_PickTarget(char *targetname);
void    G_UseTargets(edict_t *ent, edict_t *activator);
void    G_SetMovedir(vec3_t angles, vec3_t movedir);

void    G_InitEdict(edict_t *e);
edict_t *G_AllocateEdict(void);
void    G_FreeEdict(edict_t *e);

void    G_TouchTriggers(edict_t *ent);
void    G_TouchProjectiles( edict_t *ent, const Vector3 &previous_origin );
void    G_TouchSolids(edict_t *ent);

char    *G_CopyString(char *in);

//
// g_combat.c
//
bool OnSameTeam(edict_t *ent1, edict_t *ent2);
bool CanDamage(edict_t *targ, edict_t *inflictor);
void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t dir, vec3_t point, const vec3_t normal, int damage, int knockback, int dflags, int mod);
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod);

// damage flags
#define DAMAGE_NONE             0x00000000
#define DAMAGE_RADIUS           0x00000001  // damage was indirect
#define DAMAGE_NO_ARMOR         0x00000002  // armour does not protect from this damage
#define DAMAGE_ENERGY           0x00000004  // damage is from an energy based weapon
#define DAMAGE_NO_KNOCKBACK     0x00000008  // do not affect velocity, just view angles
#define DAMAGE_BULLET           0x00000010  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION    0x00000020  // armor, shields, invulnerability, and godmode have no effect

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
void monster_fire_bullet(edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype);
void monster_fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype);
void monster_fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect);
void monster_fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int flashtype);
void monster_fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype);
void monster_fire_railgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int flashtype);
void monster_fire_bfg(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int kick, float damage_radius, int flashtype);
void M_droptofloor(edict_t *ent);
void monster_think(edict_t *self);
void walkmonster_start(edict_t *self);
void swimmonster_start(edict_t *self);
void flymonster_start(edict_t *self);
void AttackFinished(edict_t *self, float time);
void monster_death_use(edict_t *self);
void M_CatagorizePosition( edict_t *ent, const Vector3 &in_point, liquid_level_t &liquidlevel, contents_t &liquidtype );
bool M_CheckAttack(edict_t *self);
void M_FlyCheck(edict_t *self);
void M_CheckGround(edict_t *ent, const contents_t mask);
void M_WorldEffects( edict_t *ent );
void M_SetAnimation( edict_t *self, mmove_t *move, bool instant = true );


//
// g_misc.c
//
// WID: C++20: Added const.
void ThrowHead(edict_t *self, const char *gibname, int damage, int type);
void ThrowClientHead(edict_t *self, int damage);
// WID: C++20: Added const.
void ThrowGib(edict_t *self, const char *gibname, int damage, int type);
void BecomeExplosion1(edict_t *self);

#define CLOCK_MESSAGE_SIZE  16
void func_clock_think(edict_t *self);
void func_clock_use(edict_t *self, edict_t *other, edict_t *activator);

//
// g_ai.c
//
void AI_SetSightClient(void);

void ai_stand(edict_t *self, float dist);
void ai_move(edict_t *self, float dist);
void ai_walk(edict_t *self, float dist);
void ai_turn(edict_t *self, float dist);
void ai_run(edict_t *self, float dist);
void ai_charge(edict_t *self, float dist);
int range(edict_t *self, edict_t *other);

void FoundTarget(edict_t *self);
bool infront(edict_t *self, edict_t *other);
bool visible(edict_t *self, edict_t *other);
bool FacingIdeal(edict_t *self);

//
// g_weapon.c
//
// WID: C++20: Added const.
void ThrowDebris(edict_t *self, const char *modelname, float speed, vec3_t origin);
bool fire_hit(edict_t *self, vec3_t aim, int damage, int kick);
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod);
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod);
void fire_blaster(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int effect, bool hyper);
void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, sg_time_t timer, float damage_radius);
void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, sg_time_t timer, float damage_radius, bool held);
void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick);
void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius);

//
// g_ptrail.c
//
void PlayerTrail_Init(void);
void PlayerTrail_Add(vec3_t spot);
void PlayerTrail_New(vec3_t spot);
edict_t *PlayerTrail_PickFirst(edict_t *self);
edict_t *PlayerTrail_PickNext(edict_t *self);
edict_t *PlayerTrail_LastSpot(void);

//
// g_client.c
//
void respawn(edict_t *ent);
void BeginIntermission(edict_t *targ);
void PutClientInServer(edict_t *ent);
void InitClientPersistantData(edict_t *ent, gclient_t *client);
void InitClientRespawnData(gclient_t *client);
void InitBodyQue(void);
void ClientBeginServerFrame(edict_t *ent);

//
// g_player.c
//
void player_pain(edict_t *self, edict_t *other, float kick, int damage);
void player_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

//
// g_svcmds.c
//
void ServerCommand(void);
bool SV_FilterPacket(char *from);

//
// p_view.c
//
void ClientEndServerFrame(edict_t *ent);

//
// p_hud.c
//
void MoveClientToIntermission(edict_t *client);
void G_SetStats(edict_t *ent);
void G_SetSpectatorStats(edict_t *ent);
void G_CheckChaseStats(edict_t *ent);
void ValidateSelectedItem(edict_t *ent);
void DeathmatchScoreboardMessage(edict_t *client, edict_t *killer);

//
// g_pweapon.c
//
void PlayerNoise(edict_t *who, const vec3_t where, int type);

//
// m_move.c
//
bool M_CheckBottom(edict_t *ent);
bool M_walkmove(edict_t *ent, float yaw, float dist);
void M_MoveToGoal(edict_t *ent, float dist);
void M_ChangeYaw(edict_t *ent);

//
// g_phys.c
//
void SV_Impact( edict_t *e1, trace_t *trace );
const contents_t G_GetClipMask( edict_t *ent );
void G_RunEntity(edict_t *ent);

//
// g_main.c
//
void SaveClientData(void);
void FetchClientEntData(edict_t *ent);

//
// g_chase.c
//
void UpdateChaseCam(edict_t *ent);
void ChaseNext(edict_t *ent);
void ChasePrev(edict_t *ent);
void GetChaseTarget(edict_t *ent);

//============================================================================

// client_t->anim_priority
#define ANIM_BASIC      0       // stand / run
#define ANIM_WAVE       1
#define ANIM_JUMP       2
#define ANIM_PAIN       3
#define ANIM_ATTACK     4
#define ANIM_DEATH      5
#define ANIM_REVERSED   6		// animation is played in reverse.


// client data that stays across multiple level loads
typedef struct {
    char        userinfo[MAX_INFO_STRING];
    char        netname[16];
    int         hand;

    bool        connected;  // A loadgame will leave valid entities that just don't have a connection yet.
    bool        spawned;    // Stores whether spawned or not. 

    // values saved and restored from edicts when changing levels
    int         health;
    int         max_health;
    ent_flags_t savedFlags;

    int         selected_item;
    int         inventory[MAX_ITEMS];

    // ammo capacities
    int         max_bullets;
    int         max_shells;
    int         max_rockets;
    int         max_grenades;
    int         max_cells;
    int         max_slugs;

    gitem_t     *weapon;
    gitem_t     *lastweapon;

    int         power_cubes;    // used for tracking the cubes in coop games
    int         score;          // for calculating total unit score in coop games

    int         game_helpchanged;
    int         helpchanged;

    bool        spectator;      // client is a spectator
} client_persistant_t;

// Client data that stays across deathmatch respawns
typedef struct {
    client_persistant_t coop_respawn;	// what to set client->pers to on a respawn

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
    bool        showscores;         // set layout stat
    bool        showinventory;      // set layout stat
    bool        showhelp;
    bool        showhelpicon;


    /**
    *	User Imput:
    **/
    int         buttons;
    int         oldbuttons;
    int         latched_buttons;

	/**
	*	Weapon Related:
	**/
	// Actual current 'weapon', based on 'ammo_index'.
	int	ammo_index;

	// Set when we want to switch weapons.
	gitem_t *newweapon;

	// weapon cannot fire until this time is up
	sg_time_t weapon_fire_finished;
	// time between processing individual animation frames
	sg_time_t weapon_think_time;
	// if we latched fire between server frames but before
	// the weapon fire finish has elapsed, we'll "press" it
	// automatically when we have a chance
	bool weapon_fire_buffered;
	bool weapon_thunk;

	// Last time we played an 'empty weapon click' sound.
	sg_time_t	empty_weapon_click_sound;

	sg_time_t	grenade_time;
	sg_time_t	grenade_finished_time;
	bool        grenade_blew_up;

	int         silencer_shots;
	int         weapon_sound;

	/**
	*	Damage Related:
	**/
    // sum up damage over an entire frame, so
    // shotgun blasts give a single big kick
    int         damage_armor;       // damage absorbed by armor
    int         damage_parmor;      // damage absorbed by power armor
    int         damage_blood;       // damage taken out of health
    int         damage_knockback;   // impact damage
    vec3_t      damage_from;        // origin for vector calculation

    float       killer_yaw;         // when dead, look at killer

    weaponstate_t   weaponstate;

    /**
    *   View Movement:
    **/
    vec3_t      v_angle, v_forward; // aiming direction

    //! Summed Weapon Kicks, to be applied to client 'viewoffset' and 'kick_angles'.
    struct {
        //! Additional weapon 'Kick Effect' angles to be added to playerState.kick_angles.
        vec3_t offsetAngles;
        //! Additional weapon 'Kick Effect' origin to be added to viewOffset.
        vec3_t offsetOrigin;
    } weaponKicks;

    // view movement:
    sg_time_t	v_dmg_time;
    sg_time_t	fall_time;
    sg_time_t	quake_time;
    float       v_dmg_roll, v_dmg_pitch;    // damage kicks
    float		fall_value;      // for view drop on fall

    /**
    *   View Blends:
    **/
    float       damage_alpha;
    float       bonus_alpha;
    
    vec3_t      damage_blend;
    
    float       bobtime;            // Store it, so we know where we're at (To Prevent off-ground from changing it).
    
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

	/**
	*   Misc:
	**/
	sg_time_t		next_drown_time;
    int32_t         breather_sound;
    int32_t         machinegun_shots;   // for weapon raising

	/**
    *	Animation Related:
	**/
    int			anim_end;
    int			anim_priority;
    bool		anim_duck;
    bool		anim_run;
	sg_time_t	anim_time;

	/**
	*	Powerup Timers:
	**/
	sg_time_t	quad_time;
	sg_time_t	invincible_time;
	sg_time_t	breather_time;
	sg_time_t	enviro_time;

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


    // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    // EXPECTS THE FIELDS IN THAT ORDER!

    const cm_entity_t *entityDictionary;
    //================================
    int32_t     spawn_count;
    int32_t     movetype;
    ent_flags_t flags;

	// WID: C++20: added const.
    const char  *model;
	sg_time_t   freetime;           // sv.time when the object was freed

    //
    // only used locally in game, not by server
    //
    char        *message;
	// WID: C++20: Added const.
    const char	*classname;
    int32_t     spawnflags;

	sg_time_t	timestamp;

    float       angle;          // set in qe3, -1 = up, -2 = down
    char        *target;
	// WID: C++20: Added const.
    const char	*targetname;
    char        *killtarget;
    char        *team;
    char        *pathtarget;
    char        *deathtarget;
    char        *combattarget;
    edict_t     *target_ent;

    float       speed, accel, decel;
    vec3_t      movedir;
    vec3_t      pos1, pos2;

    vec3_t      velocity;
    vec3_t      avelocity;
    int32_t     mass;
	sg_time_t   air_finished_time;
    float       gravity;        // per entity gravity multiplier (1.0 is normal)
                                // use for lowgrav artifact, flares

    edict_t     *goalentity;
    edict_t     *movetarget;
    float       yaw_speed;
    float       ideal_yaw;

    sg_time_t         nextthink;
    void        ( *postspawn )( edict_t *ent );
    void        ( *prethink )( edict_t *ent );
    void        ( *think )( edict_t *self );
    void        ( *postthink )( edict_t *ent );
    void        ( *blocked )( edict_t *self, edict_t *other );         // move to moveinfo?
    void        ( *touch )( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
    void        ( *use )( edict_t *self, edict_t *other, edict_t *activator );
    void        ( *pain )( edict_t *self, edict_t *other, float kick, int damage );
    void        ( *die )( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );

	sg_time_t		touch_debounce_time;        // are all these legit?  do we need more/less of them?
	sg_time_t		pain_debounce_time;
	sg_time_t		damage_debounce_time;
    sg_time_t		fly_sound_debounce_time;    // move to clientinfo
	sg_time_t		last_move_time;

    int32_t     health;
    int32_t     max_health;
    int32_t     gib_health;
    int32_t     deadflag;
    sg_time_t   show_hostile;

	sg_time_t   powerarmor_time;

	// WID: C++20: Added const.
    const char  *map;           // target_changelevel

    int32_t     viewheight;     // height above origin where eyesight is determined
    int32_t     takedamage;
    int32_t     dmg;
    int32_t     radius_dmg;
    float       dmg_radius;
	float		light;
    int32_t     sounds;         // make this a spawntemp var?
    int32_t     count;

    edict_t     *chain;
    edict_t     *enemy;
    edict_t     *oldenemy;
    edict_t     *activator;
    edict_t     *groundentity;
    int32_t     groundentity_linkcount;
    edict_t     *teamchain;
    edict_t     *teammaster;

    edict_t     *mynoise;       // can go in client only
    edict_t     *mynoise2;

    int32_t     noise_index;
    int32_t     noise_index2;
    float       volume;
    float       attenuation;

    // timing variables
    float       wait;
    float       delay;          // before firing targets
    float       random;

    sg_time_t		last_sound_time;

    contents_t      liquidtype;
	liquid_level_t	liquidlevel;

    vec3_t      move_origin;
    vec3_t      move_angles;

    // move this to clientinfo?
    int32_t     light_level;

    int32_t     style;          // also used as areaportal number
	const char *customLightStyle;	// It is as it says.

    gitem_t     *item;          // for bonus items

    // common data blocks
    moveinfo_t      moveinfo;
    monsterinfo_t   monsterinfo;
};

