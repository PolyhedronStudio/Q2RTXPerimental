/********************************************************************
*
*
*	ServerGame: All Item Related Data Structures.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   Item SpawnFlags:
**/
static constexpr spawnflag_t ITEM_TRIGGER_SPAWN = 0x00000001;
static constexpr spawnflag_t ITEM_NO_TOUCH = 0x00000002;
// 6 bits reserved for editor flags
// 8 bits used as power cube id bits for coop games
static constexpr spawnflag_t DROPPED_ITEM = 0x00010000;
static constexpr spawnflag_t DROPPED_PLAYER_ITEM = 0x00020000;
static constexpr spawnflag_t ITEM_TARGETS_USED = 0x00040000;

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

    #if 0
    // Armor Types:
    ITEM_TAG_ARMOR_NONE,
    ITEM_TAG_ARMOR_JACKET,
    ITEM_TAG_ARMOR_PLATING,
    ITEM_TAG_ARMOR_HELMET,
    #endif

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


/**
*   gitem_t->flags
**/
#define ITEM_FLAG_WEAPON       1       //! "+use" makes active weapon
#define ITEM_FLAG_AMMO         2
#define ITEM_FLAG_ARMOR        4
#define ITEM_FLAG_STAY_COOP    8
// WID: These don't exist anymore, but can still be reused of course.
//#define IT_KEY          16
//#define IT_POWERUP      32


/**
*   gitem_t->weapon_index for weapons indicates model index.
*   NOTE: These must MATCH in ORDER to those in g_spawn.cpp
**/
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
    const char *classname; // spawning name

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
    const char *pickup_sound; // WID: C++20: Added const.
    //! Path: World Model
    const char *world_model; // WID: C++20: Added const.
    //! World Model Entity Flags.
    int32_t     world_model_flags;
    //! Path: View Weapon Model.
    const char *view_model;

    //! Client Side Info:
    const char *icon;
    //! For printing on 'pickup'.
    const char *pickup_name;
    //! Number of digits to display by icon
    int         count_width;


    //! For ammo how much is acquired when picking up, for weapons how much is used per shot.
    int32_t     quantity;
    //! Limit of this weapon's capacity per 'clip'.
    int32_t     clip_capacity;
    //! For weapons, the name referring to the used Ammo Item type.
    const char *ammo;
    // IT_* specific flags.
    int32_t     flags;
    //! Weapon ('model'-)index (For weapons):
    int32_t     weapon_index;

    //! Pointer to item category/type specific info.
    void *info;
    //! Identifier for the item's category/type.
    gitem_tag_t tag;

    //! String of all models, sounds, and images this item will use and needs to precache.
    const char *precaches;
} gitem_t;