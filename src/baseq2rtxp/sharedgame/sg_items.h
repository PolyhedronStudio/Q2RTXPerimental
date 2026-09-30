/********************************************************************
*
*
*	SharedGame: All Item Related Data Structures.
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"

// Forward declarations for ServerGame callbacks.
struct svg_item_edict_t;
struct svg_base_edict_t;

/**
*	Item SpawnFlags:
**/
static constexpr int32_t ITEM_TRIGGER_SPAWN		= 0x00000001;
static constexpr int32_t ITEM_NO_TOUCH			= 0x00000002;
static constexpr int32_t DROPPED_ITEM			= 0x00010000;
static constexpr int32_t DROPPED_PLAYER_ITEM	= 0x00020000;
static constexpr int32_t ITEM_TARGETS_USED		= 0x00040000;

/**
*	@brief	Specific 'Item Tags' to identify category/type.
**/
typedef enum gitem_tag_e {
	//! Default for non-tagged items.
	ITEM_TAG_NONE = 0,

	// Ammo Types:
	ITEM_TAG_AMMO_BULLETS_PISTOL,
	ITEM_TAG_AMMO_BULLETS_RIFLE,
	ITEM_TAG_AMMO_BULLETS_SMG,
	ITEM_TAG_AMMO_BULLETS_SNIPER,
	ITEM_TAG_AMMO_SHELLS_SHOTGUN,

	// Weapon Types:
	ITEM_TAG_WEAPON_FISTS,
	ITEM_TAG_WEAPON_PISTOL
} gitem_tag_t;

/**
*	Item Flags:
**/
#define ITEM_FLAG_WEAPON		1	//! "+use" makes active weapon
#define ITEM_FLAG_AMMO			2
#define ITEM_FLAG_ARMOR			4
#define ITEM_FLAG_STAY_COOP		8

/**
*	Weapon indices for weapons (indicates model index).
**/
#define WEAP_FISTS				1
#define WEAP_PISTOL				2

/**
*	@brief	The 7 weapon categories for HUD selection and input routing.
**/
enum weapon_category_t : int32_t {
	WEAPON_CATEGORY_NONE		= 0,
	WEAPON_CATEGORY_MELEE		= 1,	//! #1: Fists, melee
	WEAPON_CATEGORY_PISTOLS		= 2,	//! #2: Pistols, sidearms
	WEAPON_CATEGORY_SMG			= 3,	//! #3: Submachine guns
	WEAPON_CATEGORY_RIFLES		= 4,	//! #4: Rifles, shotguns
	WEAPON_CATEGORY_GRENADES	= 5,	//! #5: Grenades
	WEAPON_CATEGORY_EXPLOSIVES	= 6,	//! #6: Explosives / heavy weaponry
	WEAPON_CATEGORY_OTHER		= 7,	//! #7: Other / special weaponry
	WEAPON_CATEGORY_COUNT		= 7
};

/**
*	@brief	Used to create the items array, where each sg_item_t is assigned its
*			descriptive item values.
**/
typedef struct sg_item_s {
	//! Classname.
	const char	*classname;

	//! Called right after precaching the item, this allows for weapons to seek for
	//! the appropriate animation data required for each used distinct weapon mode.
	void		( *precached )( const struct sg_item_s *item );

	//! Pickup Callback (SVGame).
	const bool	( *pickup )( struct svg_item_edict_t *ent, struct svg_base_edict_t *other );
	//! Use Callback (SVGame).
	void		( *use )( struct svg_base_edict_t *ent, const struct sg_item_s *item );
	//! Drop Callback (SVGame).
	void		( *drop )( struct svg_base_edict_t *ent, const struct sg_item_s *item );

	//! WeaponThink Callback (SVGame).
	void		( *weaponthink )( struct svg_base_edict_t *ent, const bool processUserInputOnly );

	//! Path: Pickup Sound.
	const char	*pickup_sound;
	//! Path: World Model.
	const char	*world_model;
	//! World Model Entity Flags.
	int32_t		world_model_flags;
	//! Path: View Weapon Model.
	const char	*view_model;

	//! Client Side Info:
	const char	*icon;
	//! For printing on 'pickup'.
	const char	*pickup_name;
	//! Number of digits to display by icon.
	int32_t		count_width;

	//! For ammo how much is acquired when picking up, for weapons how much is used per shot.
	int32_t		quantity;
	//! Limit of this weapon's capacity per 'clip'.
	int32_t		clip_capacity;
	//! For weapons, the name referring to the used Ammo Item type.
	const char	*ammo;
	//! ITEM_FLAG_* specific flags.
	int32_t		flags;
	//! Weapon ('model'-)index (For weapons):
	int32_t		weapon_index;

	//! Category (1..7) for weapon selector menu.
	weapon_category_t	category;
	//! Slot index within category (0..N).
	int32_t				category_slot;

	//! Pointer to item category/type specific info.
	void		*info;
	//! Identifier for the item's category/type.
	gitem_tag_t	tag;

	//! String of all models, sounds, and images this item will use and needs to precache.
	const char	*precaches;
} sg_item_t;

//! Backwards compatibility alias for SVGame:
using gitem_t = sg_item_t;
using gitem_s = sg_item_s;

//! Master items list defined in sg_items.cpp:
extern sg_item_t sg_ItemList[];
extern int32_t num_sg_ItemList;

//! Backwards compatibility define for SVGame:
#define itemlist sg_ItemList

/**
*	@brief	Get index of an item in the master list.
**/
#define ITEM_INDEX(x) ((x) - sg_ItemList)

/**
*	@brief	Look up an item by its classname.
**/
const sg_item_t *SG_Item_FindByClassName( const char *classname );
/**
*	@brief	Look up an item by its pickup name.
**/
const sg_item_t *SG_Item_FindByPickupName( const char *pickup_name );
/**
*	@brief	Get an item by its master index.
**/
const sg_item_t *SG_Item_GetByIndex( const int32_t index );
/**
*	@brief	Get a weapon item by its weapon_index.
**/
const sg_item_t *SG_Item_GetByWeaponIndex( const int32_t weaponIndex );
/**
*	@brief	Gather all weapons belonging to a specific category.
*	@param	category	The weapon category (1..7).
*	@param	outWeapons	Array of pointers to receive the matched items.
*	@param	maxWeapons	Capacity of outWeapons.
*	@return	Number of weapons written into outWeapons.
**/
int32_t SG_Item_GetWeaponsInCategory( const weapon_category_t category, const sg_item_t **outWeapons, const int32_t maxWeapons );
