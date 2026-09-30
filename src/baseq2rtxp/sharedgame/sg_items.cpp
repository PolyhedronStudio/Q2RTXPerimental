/********************************************************************
*
*
*	SharedGame: Master Item Definitions and Lookup Helpers.
*
*
********************************************************************/
#include "sharedgame/sg_items.h"
#include <cstring>

/**
*	Master Item List:
*	Both SVGame and CLGame share this catalogue of items.
*	SVGame hooks up its server-side function pointers in SVG_InitItems().
**/
//! Master items list.
sg_item_t sg_ItemList[] = {
	// Leave index 0 alone (Null item).
	{
		.classname = nullptr
	},

	//********************************************************
	//  Weapon Items:                                       **
	//********************************************************

	//
	//   classname(weapon_fists)
	//   Category #1: Melee, Slot 0
	//
	{
		.classname = "weapon_fists",
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = nullptr,
		.world_model_flags = 0,
		.view_model = "models/v_wep/fists/tris.iqm",
		.icon = "w_blaster",
		.pickup_name = "Fists",
		.count_width = 0,
		.quantity = 0,
		.clip_capacity = 0,
		.ammo = nullptr,
		.flags = ITEM_FLAG_WEAPON | ITEM_FLAG_STAY_COOP,
		.weapon_index = WEAP_FISTS,
		.category = WEAPON_CATEGORY_MELEE,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_WEAPON_FISTS,
		.precaches = "models/v_wep/fists/tris.iqm weapons/fists/fist1.wav weapons/fists/sway01.wav weapons/fists/sway02.wav weapons/fists/sway03.wav weapons/fists/sway04.wav weapons/fists/sway05.wav"
	},

	//
	//   classname(weapon_pistol)
	//   Category #2: Pistols, Slot 0
	//
	{
		.classname = "weapon_pistol",
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = "models/g_wep/pistol/tris.iqm",
		.world_model_flags = 0x00000001, // EF_ROTATE
		.view_model = "models/v_wep/pistol/tris.iqm",
		.icon = "w_blaster",
		.pickup_name = "Pistol",
		.count_width = 0,
		.quantity = 1,
		.clip_capacity = 13,
		.ammo = "Pistol Bullets",
		.flags = ITEM_FLAG_WEAPON | ITEM_FLAG_STAY_COOP,
		.weapon_index = WEAP_PISTOL,
		.category = WEAPON_CATEGORY_PISTOLS,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_WEAPON_PISTOL,
		.precaches = "models/g_wep/pistol/tris.iqm models/v_wep/pistol/tris.iqm items/weaponry_pickup.wav weapons/pistol/draw.wav weapons/pistol/holster.wav weapons/pistol/fire1.wav weapons/pistol/fire2.wav weapons/pistol/fire3.wav weapons/pistol/reload.wav weapons/pistol/noammo.wav"
	},

	//********************************************************
	//  Ammo Items:                                         **
	//********************************************************

	//
	//   classname(ammo_bullets_pistol)
	//
	{
		.classname = "ammo_bullets_pistol",
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = "models/items/ammo/bullets_pistol/tris.iqm",
		.world_model_flags = 0,
		.view_model = nullptr,
		.icon = "a_bullets",
		.pickup_name = "Pistol Bullets",
		.count_width = 3,
		.quantity = 50,
		.clip_capacity = 0,
		.ammo = nullptr,
		.flags = ITEM_FLAG_AMMO,
		.weapon_index = 0,
		.category = WEAPON_CATEGORY_NONE,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_AMMO_BULLETS_PISTOL,
		.precaches = "models/items/ammo/bullets_pistol/tris.iqm items/weaponry_pickup.wav"
	},

	//
	//   classname(ammo_bullets_rifle)
	//
	{
		.classname = "ammo_bullets_rifle",
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = "models/items/ammo/bullets_rifle/tris.iqm",
		.world_model_flags = 0,
		.view_model = nullptr,
		.icon = "a_bullets",
		.pickup_name = "Rifle Bullets",
		.count_width = 3,
		.quantity = 50,
		.clip_capacity = 0,
		.ammo = nullptr,
		.flags = ITEM_FLAG_AMMO,
		.weapon_index = 0,
		.category = WEAPON_CATEGORY_NONE,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_AMMO_BULLETS_RIFLE,
		.precaches = "models/items/ammo/bullets_rifle/tris.iqm items/weaponry_pickup.wav"
	},

	//
	//   classname(ammo_bullets_smg)
	//
	{
		.classname = "ammo_bullets_smg",
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = "models/items/ammo/bullets_smg/tris.iqm",
		.world_model_flags = 0,
		.view_model = nullptr,
		.icon = "a_bullets",
		.pickup_name = "SMG Bullets",
		.count_width = 3,
		.quantity = 50,
		.clip_capacity = 0,
		.ammo = nullptr,
		.flags = ITEM_FLAG_AMMO,
		.weapon_index = 0,
		.category = WEAPON_CATEGORY_NONE,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_AMMO_BULLETS_SMG,
		.precaches = "models/items/ammo/bullets_smg/tris.iqm items/weaponry_pickup.wav"
	},

	//
	//   classname(ammo_bullets_sniper)
	//
	{
		.classname = "ammo_bullets_sniper",
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = "models/items/ammo/bullets_sniper/tris.iqm",
		.world_model_flags = 0,
		.view_model = nullptr,
		.icon = "a_bullets",
		.pickup_name = "Sniper Bullets",
		.count_width = 3,
		.quantity = 50,
		.clip_capacity = 0,
		.ammo = nullptr,
		.flags = ITEM_FLAG_AMMO,
		.weapon_index = 0,
		.category = WEAPON_CATEGORY_NONE,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_AMMO_BULLETS_SNIPER,
		.precaches = "models/items/ammo/bullets_sniper/tris.iqm items/weaponry_pickup.wav"
	},

	//
	//   classname(ammo_shells_shotgun)
	//
	{
		.classname = "ammo_shells_shotgun",
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = "models/items/ammo/shells_shotgun/tris.iqm",
		.world_model_flags = 0,
		.view_model = nullptr,
		.icon = "a_shells",
		.pickup_name = "Shotgun Shells",
		.count_width = 3,
		.quantity = 10,
		.clip_capacity = 0,
		.ammo = nullptr,
		.flags = ITEM_FLAG_AMMO,
		.weapon_index = 0,
		.category = WEAPON_CATEGORY_NONE,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_AMMO_SHELLS_SHOTGUN,
		.precaches = "models/items/ammo/shells_shotgun/tris.iqm items/weaponry_pickup.wav"
	},

	//********************************************************
	//  Health Items:                                       **
	//********************************************************
	{
		.classname = nullptr,
		.precached = nullptr,
		.pickup = nullptr,
		.use = nullptr,
		.drop = nullptr,
		.weaponthink = nullptr,
		.pickup_sound = "items/weaponry_pickup.wav",
		.world_model = nullptr,
		.world_model_flags = 0,
		.view_model = nullptr,
		.icon = "i_health",
		.pickup_name = "Health",
		.count_width = 3,
		.quantity = 0,
		.clip_capacity = 0,
		.ammo = nullptr,
		.flags = 0,
		.weapon_index = 0,
		.category = WEAPON_CATEGORY_NONE,
		.category_slot = 0,
		.info = nullptr,
		.tag = ITEM_TAG_NONE,
		.precaches = "items/weaponry_pickup.wav items/s_health.wav items/n_health.wav items/l_health.wav items/m_health.wav"
	},

	//********************************************************
	//  End of list marker:                                 **
	//********************************************************
	{
		.classname = nullptr
	}
};

//! Total count of items in sg_ItemList (excluding the sentinel).
int32_t num_sg_ItemList = ( sizeof( sg_ItemList ) / sizeof( sg_ItemList[ 0 ] ) ) - 1;

/**
*	@brief	Look up an item by its classname.
*	@param	classname	String identifier (e.g. "weapon_pistol").
*	@return	Pointer to matched sg_item_t or nullptr.
**/
const sg_item_t *SG_Item_FindByClassName( const char *classname ) {
	/**
	*	Sanity check: reject null classname queries.
	**/
	if ( !classname ) {
		return nullptr;
	}

	/**
	*	Iterate the master list and compare classnames case-insensitively.
	**/
	const sg_item_t *it = sg_ItemList;
	for ( int32_t i = 0; i < num_sg_ItemList; i++, it++ ) {
		if ( !it->classname ) {
			continue;
		}
		if ( !Q_stricmp( it->classname, classname ) ) {
			return it;
		}
	}

	return nullptr;
}

/**
*	@brief	Look up an item by its pickup name.
*	@param	pickup_name	User-visible pickup text (e.g. "Pistol").
*	@return	Pointer to matched sg_item_t or nullptr.
**/
const sg_item_t *SG_Item_FindByPickupName( const char *pickup_name ) {
	/**
	*	Sanity check: reject null queries.
	**/
	if ( !pickup_name ) {
		return nullptr;
	}

	/**
	*	Iterate the master list and compare pickup names.
	**/
	const sg_item_t *it = sg_ItemList;
	for ( int32_t i = 0; i < num_sg_ItemList; i++, it++ ) {
		if ( !it->pickup_name ) {
			continue;
		}
		if ( !Q_stricmp( it->pickup_name, pickup_name ) ) {
			return it;
		}
	}

	return nullptr;
}

/**
*	@brief	Get an item by its master index.
*	@param	index	Item index within sg_ItemList.
*	@return	Pointer to item or nullptr if index is out of bounds.
**/
const sg_item_t *SG_Item_GetByIndex( const int32_t index ) {
	/**
	*	Bounds check: slot 0 is the null sentinel.
	**/
	if ( index <= 0 || index >= num_sg_ItemList ) {
		return nullptr;
	}

	return &sg_ItemList[ index ];
}

/**
*	@brief	Get a weapon item by its weapon_index.
*	@param	weaponIndex	Engine weapon model index (e.g. WEAP_FISTS, WEAP_PISTOL).
*	@return	Pointer to matched weapon item or nullptr.
**/
const sg_item_t *SG_Item_GetByWeaponIndex( const int32_t weaponIndex ) {
	/**
	*	Sanity check: weapon indices are positive.
	**/
	if ( weaponIndex <= 0 ) {
		return nullptr;
	}

	/**
	*	Search for the weapon matching weapon_index.
	**/
	const sg_item_t *it = sg_ItemList;
	for ( int32_t i = 0; i < num_sg_ItemList; i++, it++ ) {
		if ( ( it->flags & ITEM_FLAG_WEAPON ) && it->weapon_index == weaponIndex ) {
			return it;
		}
	}

	return nullptr;
}

/**
*	@brief	Gather all weapons belonging to a specific category.
*	@param	category	The weapon category (1..7).
*	@param	outWeapons	Array of pointers to receive the matched items.
*	@param	maxWeapons	Capacity of outWeapons.
*	@return	Number of weapons written into outWeapons.
**/
int32_t SG_Item_GetWeaponsInCategory( const weapon_category_t category, const sg_item_t **outWeapons, const int32_t maxWeapons ) {
	/**
	*	Sanity checks: ensure valid output buffer and category.
	**/
	if ( !outWeapons || maxWeapons <= 0 || category <= WEAPON_CATEGORY_NONE || category > WEAPON_CATEGORY_COUNT ) {
		return 0;
	}

	/**
	*	Collect all matching weapons up to maxWeapons.
	**/
	int32_t count = 0;
	const sg_item_t *it = sg_ItemList;
	for ( int32_t i = 0; i < num_sg_ItemList; i++, it++ ) {
		if ( !( it->flags & ITEM_FLAG_WEAPON ) ) {
			continue;
		}
		if ( it->category == category ) {
			outWeapons[ count++ ] = it;
			if ( count >= maxWeapons ) {
				break;
			}
		}
	}

	return count;
}
