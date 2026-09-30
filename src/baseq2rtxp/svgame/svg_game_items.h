/********************************************************************
*
*
*	ServerGame: All Item Related Data Structures.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


// Include shared item definitions (sg_item_t, sg_ItemList, categories, tags, flags).
#include "sharedgame/sg_items.h"

// Forward declarations
struct svg_item_edict_t;
struct svg_base_edict_t;

/**
*	@brief	Precache models, sounds and assets for the specified item.
*	@param	it	Pointer to the item to precache.
**/
void SVG_PrecacheItem( const gitem_t *it );
/**
*	@brief	Initialize server items list and bind server callbacks.
**/
void SVG_InitItems( void );
/**
*	@brief	Set crowdID skin for used by crowd system.
**/
void SVG_RegisterCrowdIDSkins( void );
/**
*	@brief	Set item names in server configstrings.
**/
void SVG_SetItemNames( void );
/**
*	@brief	Look up an item by its pickup name.
*	@param	pickup_name	User-visible pickup text.
*	@return	Pointer to matched item or nullptr.
**/
const gitem_t *SVG_Item_FindByPickupName( const char *pickup_name );
/**
*	@brief	Look up an item by its classname.
*	@param	classname	Classname string.
*	@return	Pointer to matched item or nullptr.
**/
const gitem_t *SVG_Item_FindByClassName( const char *classname );
/**
*   @brief
**/
svg_item_edict_t *Drop_Item( svg_base_edict_t *ent, const gitem_t *item );
/**
*   @brief
**/
void SVG_Item_SetRespawn( svg_item_edict_t *ent, float delay );
/**
*   @brief
**/
void SVG_Item_Spawn( svg_item_edict_t *ent, const gitem_t *item );
/**
*   @brief
**/
const gitem_t *SVG_Item_GetByIndex( int index );
/**
*   @brief
**/
const bool SVG_ItemAmmo_Add( svg_base_edict_t *ent, const gitem_t *item, const int32_t count );
/**
*   @brief
**/
void Touch_Item( svg_base_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
