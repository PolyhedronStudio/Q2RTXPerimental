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
#include "svgame/svg_local.h"
#include "svgame/svg_chase.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_hud.h"




//void Weapon_Blaster(svg_base_edict_t *ent);
//void Weapon_Shotgun(svg_base_edict_t *ent);
void Weapon_Fists( svg_base_edict_t *ent, const bool processUserInputOnly );
void Weapon_Fists_Precached( const gitem_t *item );
void Weapon_Pistol( svg_base_edict_t *ent, const bool processUserInputOnly );
void Weapon_Pistol_Precached( const gitem_t *item );
//void Weapon_SuperShotgun(svg_base_edict_t *ent);
//void Weapon_Machinegun(svg_base_edict_t *ent);
//void Weapon_Chaingun(svg_base_edict_t *ent);
//void Weapon_HyperBlaster(svg_base_edict_t *ent);
//void Weapon_RocketLauncher(svg_base_edict_t *ent);
//void Weapon_Grenade(svg_base_edict_t *ent);
//void Weapon_GrenadeLauncher(svg_base_edict_t *ent);
//void Weapon_Railgun(svg_base_edict_t *ent);
//void Weapon_BFG(svg_base_edict_t *ent);
//void Weapon_FlareGun(svg_base_edict_t *ent);

// for passing into *info member of gitem_t.
extern weapon_item_info_t fistsItemInfo;
extern weapon_item_info_t pistolItemInfo;


static constexpr int32_t HEALTH_IGNORE_MAX = 1;
static constexpr int32_t HEALTH_TIMED = 2;


/**
*
*
*
*   Inventory Items:
*
*
*
**/
/**
*   @brief
**/
void SVG_Inventory_SelectNextItem( svg_base_edict_t *ent, int itflags ) {
    svg_client_t *cl;
    int         i, index;
    gitem_t *it;

    cl = ent->client;

    if ( cl->chase_target ) {
        SVG_ChaseCam_Next( ent );
        return;
    }

    // scan  for the next valid one
    for ( i = 1; i <= MAX_ITEMS; i++ ) {
        index = ( cl->pers.selected_item + i ) % MAX_ITEMS;
        if ( !cl->pers.inventory[ index ] )
            continue;
        it = &itemlist[ index ];
        if ( !it->use )
            continue;
        if ( !( it->flags & itflags ) )
            continue;

        cl->pers.selected_item = index;
        return;
    }

    cl->pers.selected_item = -1;
}

/**
*   @brief
**/
void SVG_Inventory_SelectPrevItem( svg_base_edict_t *ent, int itflags ) {
    svg_client_t *cl;
    int         i, index;
    gitem_t *it;

    cl = ent->client;

    if ( cl->chase_target ) {
        SVG_ChaseCam_Previous( ent );
        return;
    }

    // scan  for the next valid one
    for ( i = 1; i <= MAX_ITEMS; i++ ) {
        index = ( cl->pers.selected_item + MAX_ITEMS - i ) % MAX_ITEMS;
        if ( !cl->pers.inventory[ index ] )
            continue;
        it = &itemlist[ index ];
        if ( !it->use )
            continue;
        if ( !( it->flags & itflags ) )
            continue;

        cl->pers.selected_item = index;
        return;
    }

    cl->pers.selected_item = -1;
}
/**
*   @brief  Validates current selected item, if invalid, moves to the next valid item.
**/
void SVG_Inventory_ValidateSelectedItem( svg_base_edict_t *ent ) {
    svg_client_t *cl;

    cl = ent->client;

    if ( cl->pers.inventory[ cl->pers.selected_item ] )
        return;     // valid

    SVG_Inventory_SelectNextItem( ent, -1 );
}



/**
*
*
*   Item Inventory:
*
*
**/
/**
*   @brief  Will return a pointer to the matching index item, nullptr on failure.
**/
const gitem_t *SVG_GetItemByIndex(int index) {
    if ( index == 0 || index >= game.num_items ) {
        return nullptr;
    }

    return &itemlist[index];
}
/**
*   @brief  Will return a pointer to the matching classname item, nullptr on failure.
**/
const gitem_t *SVG_FindItemByClassname(const char *classname) {
    const gitem_t *it = itemlist;
    for ( int32_t i = 0 ; i < game.num_items ; i++, it++) {
        if (!it->classname)
            continue;
        if (!Q_stricmp(it->classname, classname))
            return it;
    }

    return nullptr;
}
/**
*   @brief  Will return a pointer to the matching pickup_name item, nullptr on failure.
**/
const gitem_t *SVG_FindItem(const char *pickup_name) {
    gitem_t *it = itemlist;
    for ( int32_t i = 0 ; i < game.num_items ; i++, it++ ) {
        if (!it->pickup_name)
            continue;
        if (!Q_stricmp(it->pickup_name, pickup_name))
            return it;
    }

    return nullptr;
}


//======================================================================
/**
*
*
*   Item Respawning:
*
*
**/
void DoRespawn(svg_base_edict_t *ent)
{
    if (ent->targetNames.team) {
        svg_base_edict_t *master;
        int count;
        int choice;

        master = ent->teammaster;

        for (count = 0, ent = master; ent; ent = ent->chain, count++)
            ;

        choice = Q_rand_uniform(count);

        for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
            ;
    }

    ent->svflags &= ~SVF_NOCLIENT;
    ent->solid = SOLID_TRIGGER;
    ent->s.entityType = ET_ITEM;
    gi.linkentity(ent);

    // send an effect
    ent->s.event = EV_ITEM_RESPAWN;
}

void SVG_SetItemRespawn(svg_base_edict_t *ent, float delay)
{
    ent->flags = static_cast<entity_flags_t>( ent->flags | FL_RESPAWN );
    ent->svflags |= SVF_NOCLIENT;
    ent->solid = SOLID_NOT;
    ent->nextthink = level.time + QMTime::FromSeconds( delay );
    ent->think = DoRespawn;
    gi.linkentity(ent);
}

/**
*   @brief
**/
void Drop_General(svg_base_edict_t *ent, gitem_t *item)
{
    Drop_Item(ent, item);
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    SVG_Inventory_ValidateSelectedItem(ent);
}



/**
*
*
*   Misc Item specific Pickup Callbacks:
*
*
**/



/**
*
*
*   Ammo:
*
*
**/
/**
*   @brief  Will attempt to add 'count' of item ammo to the entity's client inventory.
**/
const bool Add_Ammo(svg_base_edict_t *ent, const gitem_t *item, const int32_t count)
{
    if (!ent->client)
        return false;

    int32_t max = 0;
    if ( item->tag == ITEM_TAG_AMMO_BULLETS_PISTOL ) {
        max = ent->client->pers.ammoCapacities.pistol;
    } else if ( item->tag == ITEM_TAG_AMMO_BULLETS_RIFLE ) {
        max = ent->client->pers.ammoCapacities.rifle;
    } else if ( item->tag == ITEM_TAG_AMMO_BULLETS_SMG ) {
        max = ent->client->pers.ammoCapacities.smg;
    } else if ( item->tag == ITEM_TAG_AMMO_BULLETS_SNIPER ) {
        max = ent->client->pers.ammoCapacities.sniper;
    } else if ( item->tag == ITEM_TAG_AMMO_SHELLS_SHOTGUN ) {
        max = ent->client->pers.ammoCapacities.shotgun;
    } else {
        return false;
    }

    // Acquire the item's inventory slot index.
    const int32_t index = ITEM_INDEX(item);

    // Return false if we're already at the max limit capacity.
    if ( ent->client->pers.inventory[ index ] == max ) {
        return false;
    }

    // Not at our limit yet, add the extra ammo count.
    ent->client->pers.inventory[index] += count;

    // Do a check and adjust to keep it within max limit capacity.
    if ( ent->client->pers.inventory[ index ] > max ) {
        ent->client->pers.inventory[ index ] = max;
    }

    // Succesfully added the extra ammo count.
    return true;
}

/**
*   @brief  Will (try to) 'pickup' the specified ammo item 'ent' and add it to the 'other' entity client's inventory.
**/
const bool Pickup_Ammo(svg_base_edict_t *itemEntity, svg_base_edict_t *other) {
    // Count that is picked up.
    int32_t count = 0;
    
    // Special treatment for weapons with Deathmatch Flag Infinite Ammo.
    const bool isWeapon = ( itemEntity->item->flags & ITEM_FLAG_WEAPON );
    if ( ( isWeapon ) && ( (int)dmflags->value & DF_INFINITE_AMMO ) ) {
        count = 1000;
    // Use the entity's 'count' key field's value and override ent->item->quantity.
    } else if ( itemEntity->count ) {
        count = itemEntity->count;
    // Otherwise, use the specified item quantity.
    } else {
        count = itemEntity->item->quantity;
    }

    // Acquire the old inventory item count score.
    int32_t oldCount = other->client->pers.inventory[ ITEM_INDEX( itemEntity->item ) ];

    // Couldn't add ammo.
    if ( !Add_Ammo( other, itemEntity->item, count ) ) {
        return false;
    }

    // If it is a weapon and we did NOT have it in our inventory yet:
    if (isWeapon && !oldCount) {
        //if (other->client->pers.weapon != itemEntity->item && (!deathmatch->value || other->client->pers.weapon == SVG_FindItem("blaster")))
        //    other->client->newweapon = itemEntity->item;
        if ( other->client->pers.weapon != itemEntity->item ) {
            other->client->newweapon = itemEntity->item;
        }
    }

    // Set an item respawn for DM mode.
    if ( !( itemEntity->spawnflags & ( DROPPED_ITEM | DROPPED_PLAYER_ITEM ) ) && ( deathmatch->value ) ) {
        SVG_SetItemRespawn( itemEntity, 30 );
    }
    return true;
}

/**
*   @brief  Drops ammo.
**/
void Drop_Ammo(svg_base_edict_t *ent, const gitem_t *item) {
    int32_t index = ITEM_INDEX(item);
    svg_base_edict_t *dropped = Drop_Item(ent, item);
    if (ent->client->pers.inventory[index] >= item->quantity)
        dropped->count = item->quantity;
    else
        dropped->count = ent->client->pers.inventory[index];

    //if (ent->client->pers.weapon &&
    //    ent->client->pers.weapon->tag == AMMO_GRENADES &&
    //    item->tag == AMMO_GRENADES &&
    //    ent->client->pers.inventory[index] - dropped->count <= 0) {
    //    gi.cprintf(ent, PRINT_HIGH, "Can't drop current weapon\n");
    //    SVG_FreeEdict(dropped);
    //    return;
    //}

    ent->client->pers.inventory[index] -= dropped->count;
    SVG_Inventory_ValidateSelectedItem(ent);
}


/**
*
*
*   Health:
*
*
**/
/**
*   @brief
**/
void MegaHealth_think(svg_base_edict_t *self)
{
    if (self->owner->health > self->owner->max_health) {
        self->nextthink = level.time + 1_sec;
        self->owner->health -= 1;
        return;
    }

    if (!(self->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SVG_SetItemRespawn(self, 20);
    else
        SVG_FreeEdict(self);
}
/**
*   @brief
**/
const bool Pickup_Health(svg_base_edict_t *ent, svg_base_edict_t *other)
{
    if (!(ent->style & HEALTH_IGNORE_MAX))
        if (other->health >= other->max_health)
            return false;

    other->health += ent->count;

    if (!(ent->style & HEALTH_IGNORE_MAX)) {
        if (other->health > other->max_health)
            other->health = other->max_health;
    }

    if (ent->style & HEALTH_TIMED) {
        ent->think = MegaHealth_think;
        ent->nextthink = level.time + 5_sec;
        ent->owner = other;
        ent->flags = static_cast<entity_flags_t>( ent->flags | FL_RESPAWN );
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
    } else {
        if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
            SVG_SetItemRespawn(ent, 30);
    }

    return true;
}



/**
*
* 
*
*   Item Entity:
*
* 
*
**/
/**
*   @brief  
**/
void Touch_Item(svg_base_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf)
{
    bool    taken;

    if (!other->client)
        return;
    if (other->health < 1)
        return;     // dead people can't pickup
    if (!ent->item->pickup)
        return;     // not a grabbable item?

    taken = ent->item->pickup(ent, other);

    if (taken) {
        // flash the screen
        other->client->bonus_alpha = 0.25f;

        // show icon and name on status bar
        other->client->ps.stats[STAT_PICKUP_ICON] = gi.imageindex(ent->item->icon);
        other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ITEM_INDEX(ent->item);
        other->client->pickup_msg_time = level.time + 3_sec;

        // change selected item
        if (ent->item->use)
            other->client->pers.selected_item = other->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(ent->item);

        if (ent->item->pickup == Pickup_Health) {
            if (ent->count == 2)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/s_health.wav"), 1, ATTN_NORM, 0);
            else if (ent->count == 10)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/n_health.wav"), 1, ATTN_NORM, 0);
            else if (ent->count == 25)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/l_health.wav"), 1, ATTN_NORM, 0);
            else // (ent->count == 100)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/m_health.wav"), 1, ATTN_NORM, 0);
        } else if (ent->item->pickup_sound) {
            gi.sound(other, CHAN_ITEM, gi.soundindex(ent->item->pickup_sound), 1, ATTN_NORM, 0);
        }
    }

    if (!(ent->spawnflags & ITEM_TARGETS_USED)) {
        SVG_UseTargets(ent, other);
        ent->spawnflags |= ITEM_TARGETS_USED;
    }

    if (!taken)
        return;

    if (!((coop->value) && (ent->item->flags & ITEM_FLAG_STAY_COOP)) || (ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM))) {
        if (ent->flags & FL_RESPAWN)
            ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_RESPAWN );
        else
            SVG_FreeEdict(ent);
    }
}

/**
*   @brief
**/
void drop_temp_touch(svg_base_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf)
{
    if (other == ent->owner)
        return;

    Touch_Item(ent, other, plane, surf);
}
/**
*   @brief
**/
void drop_make_touchable(svg_base_edict_t *ent)
{
    ent->touch = Touch_Item;
    if (deathmatch->value) {
        ent->nextthink = level.time + 29_sec;
        ent->think = SVG_FreeEdict;
    }
}
/**
*   @brief
**/
svg_base_edict_t *Drop_Item(svg_base_edict_t *ent, const gitem_t *item)
{
    svg_base_edict_t *dropped;
    vec3_t  forward, right;
    vec3_t  offset;

    dropped = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();

    dropped->classname = item->classname;
    dropped->item = item;
    dropped->spawnflags = DROPPED_ITEM;
    dropped->s.entityType = ET_ITEM;
    dropped->s.effects = item->world_model_flags;
    dropped->s.renderfx = RF_GLOW;
    VectorSet(dropped->mins, -15, -15, -15);
    VectorSet(dropped->maxs, 15, 15, 15);
    gi.setmodel(dropped, dropped->item->world_model);
    dropped->solid = SOLID_TRIGGER;
    dropped->movetype = MOVETYPE_TOSS;
    dropped->touch = drop_temp_touch;
    dropped->owner = ent;

    if (ent->client) {
        svg_trace_t trace;

        AngleVectors( &ent->client->viewMove.viewAngles.x, forward, right, NULL );
        VectorSet(offset, 24, 0, -16);
        SVG_Util_ProjectSource(ent->s.origin, offset, forward, right, dropped->s.origin);
        trace = SVG_Trace(ent->s.origin, dropped->mins, dropped->maxs,
                         dropped->s.origin, ent, CONTENTS_SOLID);
        VectorCopy(trace.endpos, dropped->s.origin);
    } else {
        AngleVectors(ent->s.angles, forward, right, NULL);
        VectorCopy(ent->s.origin, dropped->s.origin);
    }

    VectorScale(forward, 100, dropped->velocity);
    dropped->velocity[2] = 300;

    dropped->think = drop_make_touchable;
    dropped->nextthink = level.time + 1_sec;

    gi.linkentity(dropped);

    return dropped;
}
/**
*   @brief
**/
void Use_Item(svg_base_edict_t *ent, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue )
{
    ent->svflags &= ~SVF_NOCLIENT;
    ent->use = NULL;

    if (ent->spawnflags & ITEM_NO_TOUCH) {
        ent->solid = SOLID_BOUNDS_BOX;
        ent->touch = NULL;
    } else {
        ent->solid = SOLID_TRIGGER;
        ent->touch = Touch_Item;
    }

    gi.linkentity(ent);
}

//======================================================================

/**
*   @brief
**/
void droptofloor(svg_base_edict_t *ent)
{
    svg_trace_t     tr;
    vec3_t      dest;

    // First set model.
    if ( ent->model ) {
        gi.setmodel( ent, ent->model );
    } else if ( ent->item && ent->item->world_model ) {
        gi.setmodel( ent, ent->item->world_model );
    }
    
    // Then the bbox.
    if ( ent->item && ent->item->tag >= ITEM_TAG_AMMO_BULLETS_PISTOL && ent->item->tag <= ITEM_TAG_AMMO_SHELLS_SHOTGUN ) {
        VectorSet( ent->mins, -8, -8, -8 );
        VectorSet( ent->maxs, 8, 8, 8 );
    } else if ( ent->item ) {
        VectorSet( ent->mins, -16, -16, -16 );
        VectorSet( ent->maxs, 16, 16, 16 );
    }

    ent->solid = SOLID_TRIGGER;
    ent->movetype = MOVETYPE_TOSS;
    ent->touch = Touch_Item;

    VectorCopy(ent->s.origin, dest);
    dest[2] -= 128;

    tr = SVG_Trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, CM_CONTENTMASK_SOLID);
    if (tr.startsolid) {
        gi.dprintf("droptofloor: %s startsolid at %s\n", (const char *)ent->classname, vtos(ent->s.origin));
        SVG_FreeEdict(ent);
        return;
    }

    VectorCopy(tr.endpos, ent->s.origin);

    if (ent->targetNames.team) {
        ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_TEAMSLAVE );
        ent->chain = ent->teamchain;
        ent->teamchain = NULL;

        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
        if (ent == ent->teammaster) {
            ent->nextthink = level.time + 10_hz;
            ent->think = DoRespawn;
        }
    }

    if (ent->spawnflags & ITEM_NO_TOUCH) {
        ent->solid = SOLID_BOUNDS_BOX;
        ent->touch = NULL;
        ent->s.effects &= ~EF_ROTATE;
        ent->s.renderfx &= ~RF_GLOW;
    }

    if (ent->spawnflags & ITEM_TRIGGER_SPAWN) {
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
        ent->use = Use_Item;
    }

    gi.linkentity(ent);
}


/**
*   @brief  Precaches all data needed for a given item.
*           This will be called for each item spawned in a level,
*           and for each item in each client's inventory.
**/
void SVG_PrecacheItem( const gitem_t *it)
{
	// WID: C++20: Added const.
    const char *s = nullptr;
	const char *start = nullptr;
    char    data[ MAX_QPATH ] = {};
    int     len;

    if ( !it ) {
        return;
    }

    if ( it->pickup_sound ) {
        gi.soundindex( it->pickup_sound );
    }
    if ( it->world_model ) {
        gi.modelindex( it->world_model );
    }
    if ( it->view_model ) {
        gi.modelindex( it->view_model );
    }
    if ( it->icon ) {
        gi.imageindex( it->icon );
    }

    // parse everything for its ammo
    if ( it->ammo && it->ammo[ 0 ] ) {
        const gitem_t *ammo = SVG_FindItem( it->ammo );
        if ( ammo != it ) {
            SVG_PrecacheItem( ammo );
        }
    }

    // parse the space seperated precache string for other items
    s = it->precaches;
    if ( !s || !s[ 0 ] ) {
        return;
    }

    while (*s) {
        start = s;
        while ( *s && *s != ' ' ) {
            s++;
        }

        len = s - start;
        if ( len >= MAX_QPATH || len < 5 ) {
            gi.error( "SVG_PrecacheItem: item(\"%s\") has unknown extension in precache string: \"%s\"\n", it->classname, data );
        }
        memcpy( data, start, len );
        data[ len ] = 0;
        if ( *s )
            s++;

        // determine type based on extension
        if ( !strcmp( data + len - 4, ".iqm" ) ) {
            gi.modelindex( data );
        } else if ( !strcmp( data + len - 4, ".md2" ) ) {
            gi.modelindex( data );
        } else if ( !strcmp( data + len - 4, ".md3" ) ) {
            gi.modelindex( data );
        } else if ( !strcmp( data + len - 4, ".sp2" ) ) {
            gi.modelindex( data );
        } else if ( !strcmp( data + len - 4, ".wav" ) ) {
            gi.soundindex( data );
        } else if ( !strcmp( data + len - 4, ".pcx" ) ) {
            gi.imageindex( data );
        } else {
            gi.error( "SVG_PrecacheItem: item(\"%s\") has unknown extension in precache string: \"%s\"\n", it->classname, data );
        }
    }

    // Used for sourcing information from precached data.
    if ( it->precached ) {
        it->precached( it );
    }
}

/**
*   @brief  Sets the clipping size and plants the object on the floor.
*
*           Items can't be immediately dropped to floor, because they might
*           be on an entity that hasn't spawned yet.
**/
void SVG_SpawnItem(svg_base_edict_t *ent, const gitem_t *item)
{
    SVG_PrecacheItem(item);

    if (ent->spawnflags) {
        if (strcmp((const char *)ent->classname, "key_power_cube") != 0) {
            ent->spawnflags = 0;
            gi.dprintf("%s at %s has invalid spawnflags set\n", (const char *)ent->classname, vtos(ent->s.origin));
        }
    }

    // some items will be prevented in deathmatch
    if (deathmatch->value) {
        //if ((int)dmflags->value & DF_NO_ARMOR) {
        //    if ( item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor) {
        //        SVG_FreeEdict(ent);
        //        return;
        //    }
        //}
        //if ((int)dmflags->value & DF_NO_ITEMS) {
        //    if (item->pickup == Pickup_Powerup) {
        //        SVG_FreeEdict(ent);
        //        return;
        //    }
        //}
        if ((int)dmflags->value & DF_NO_HEALTH) {
            if (item->pickup == Pickup_Health ) {
                SVG_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_INFINITE_AMMO) {
            if ((item->flags == ITEM_FLAG_AMMO) /*|| (strcmp(ent->classname, "weapon_bfg") == 0 )*/) {
                SVG_FreeEdict(ent);
                return;
            }
        }
    }

    // Don't let them drop items that stay in a coop game.
    //if ((coop->value) && (item->flags & ITEM_FLAG_STAY_COOP)) {
    //    item->drop = NULL;
    //}

    ent->item = item;
    ent->nextthink = level.time + 20_hz;    // items start after other solids
    ent->think = droptofloor;
    ent->s.effects = item->world_model_flags;
    ent->s.renderfx = RF_GLOW;
    ent->s.entityType = ET_ITEM;
    if (ent->model)
        gi.modelindex(ent->model);
}

/**
* 
* 
*   Item List:
* 
* 
**/
gitem_t itemlist[] = {
    // Leave index 0 alone
    // WID: I have no clue why.
    {
        NULL
    },

    //
    // ARMOR
    //

    // QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16)
    //{
    //    .classname = "item_armor_body",
    //    .pickup = Pickup_Armor,
    //    .use = NULL,
    //    .drop = NULL,
    //    .weaponthink = NULL,
    //    .pickup_sound = "misc/ar1_pkup.wav",
    //    .world_model = "models/items/armor/body/tris.md2", 
    //    .world_model_flags = EF_ROTATE,
    //    .view_model = NULL,
    //    /* icon */      .icon = "i_bodyarmor",
    //    /* pickup */    .pickup_name = "Body Armor",
    //    /* width */     .count_width = 3,
    //    .quantity = 0,
    //    .clip_capacity = 0,
    //    .ammo = NULL,
    //    .flags = ITEM_FLAG_ARMOR,
    //    .weapon_index = 0,
    //    .info = &bodyarmor_info,
    //    .tag = ITEM_TAG_ARMOR_BODY,
    //    /* precache */ .precaches = ""
    //},


    //********************************************************
    //  Weapon Items:                                       **
    //********************************************************
    
    //
    //   classname(weapon_blaster) bbox(-16 -16 -16) (16 16 16)
    //   NOTE: It is always 'owned' and never dropped/placed in the world.
    //
    {
        .classname = "weapon_fists",
        
        .precached = Weapon_Fists_Precached,

        .pickup = SVG_Player_Weapon_Pickup,
        .use = SVG_Player_Weapon_Use,
        .drop = SVG_Player_Weapon_Drop,
        .weaponthink = Weapon_Fists,

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

        .info = &fistsItemInfo,
        .tag = ITEM_TAG_WEAPON_FISTS,
        .precaches = "models/v_wep/fists/tris.iqm weapons/fists/fist1.wav weapons/fists/sway01.wav weapons/fists/sway02.wav weapons/fists/sway03.wav weapons/fists/sway04.wav weapons/fists/sway05.wav"
    },

    //
    //   classname(weapon_pistol) bbox(-16 -16 -16) (16 16 16)
    //
    {
        .classname = "weapon_pistol",

        .precached = Weapon_Pistol_Precached,

        .pickup = SVG_Player_Weapon_Pickup,
        .use =  SVG_Player_Weapon_Use,
        .drop = SVG_Player_Weapon_Drop,
        .weaponthink = Weapon_Pistol,
        
        .pickup_sound = "items/weaponry_pickup.wav",
        .world_model = "models/g_wep/pistol/tris.iqm", 
        .world_model_flags = EF_ROTATE,
        .view_model = "models/v_wep/pistol/tris.iqm",
        
        .icon = "w_blaster", // .icon = "w_pistol",
        .pickup_name = "Pistol",
        
        .count_width = 0,
        .quantity = 1,
        .clip_capacity = 13,

        .ammo = "Pistol Bullets",
        .flags = ITEM_FLAG_WEAPON | ITEM_FLAG_STAY_COOP,
        .weapon_index = WEAP_PISTOL,

        .info = &pistolItemInfo,
        .tag = ITEM_TAG_WEAPON_PISTOL,

        .precaches = "models/g_wep/pistol/tris.iqm models/v_wep/pistol/tris.iqm items/weaponry_pickup.wav weapons/pistol/draw.wav weapons/pistol/holster.wav weapons/pistol/fire1.wav weapons/pistol/fire2.wav weapons/pistol/fire3.wav weapons/pistol/reload.wav weapons/pistol/noammo.wav"
    },


    //********************************************************
    //  Ammo Items:                                         **
    //********************************************************
    //
    //   classname(ammo_bullets_pistol) bbox(-16 -16 -16) (16 16 16)
    //
    {
        .classname = "ammo_bullets_pistol",
        .precached = nullptr,
        .pickup = Pickup_Ammo,
        .use = NULL,
        .drop = Drop_Ammo,
        .weaponthink = NULL,
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
        .info = nullptr,
        .tag = ITEM_TAG_AMMO_BULLETS_PISTOL,
        // Precache.
        .precaches = "models/items/ammo/bullets_pistol/tris.iqm items/weaponry_pickup.wav"
    },

    //
    //   classname(ammo_bullets_rifle) bbox(-16 -16 -16) (16 16 16)
    //
    {
        .classname = "ammo_bullets_rifle",
        .precached = nullptr,
        .pickup = Pickup_Ammo,
        .use = nullptr,
        .drop = Drop_Ammo,
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
        .info = nullptr,
        .tag = ITEM_TAG_AMMO_BULLETS_RIFLE,
        // Precache
        .precaches = "models/items/ammo/bullets_rifle/tris.iqm items/weaponry_pickup.wav"
    },
    
    //
    //   classname(ammo_bullets_smg) bbox(-16 -16 -16) (16 16 16)
    //
    {
        .classname = "ammo_bullets_smg",
        .precached = nullptr,
        .pickup = Pickup_Ammo,
        .use = nullptr,
        .drop = Drop_Ammo,
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
        .info = nullptr,
        .tag = ITEM_TAG_AMMO_BULLETS_SMG,
        // Precache.
        .precaches = "models/items/ammo/bullets_smg/tris.iqm items/weaponry_pickup.wav"
    },

    //
    //   classname(ammo_bullets_sniper) bbox(-16 -16 -16) (16 16 16)
    //
    {
        .classname = "ammo_bullets_sniper",
        .precached = nullptr,
        .pickup = Pickup_Ammo,
        .use = nullptr,
        .drop = Drop_Ammo,
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
        .info = nullptr,
        .tag = ITEM_TAG_AMMO_BULLETS_SNIPER,
        // Precache.
        .precaches = "models/items/ammo/bullets_sniper/tris.iqm items/weaponry_pickup.wav"
    },

    //
    //   classname(ammo_shells_shotgun) bbox(-16 -16 -16) (16 16 16)
    //
    {
        .classname = "ammo_shells_shotgun",
        .precached = nullptr,
        .pickup = Pickup_Ammo,
        .use = NULL,
        .drop = Drop_Ammo,
        .weaponthink = NULL,
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
        .info = nullptr,
        .tag = ITEM_TAG_AMMO_SHELLS_SHOTGUN,
        
        // Precache
        .precaches = "models/items/ammo/shells_shotgun/tris.iqm items/weaponry_pickup.wav"
    },


    //********************************************************
    //  PowerUp Items:                                      **
    //********************************************************
    // None as of yet.


    //********************************************************
    //  Health: Are spawned by spawn functions, see below:  **
    //********************************************************
    //
    //   classname(item_health_small)   bbox(-16 -16 -16) (16 16 16)
    //   classname(item_health_large)   bbox(-16 -16 -16) (16 16 16)
    //   classname(item_health_mega)    bbox(-16 -16 -16) (16 16 16)
    //
    {
        .classname = NULL,
        .precached = nullptr,
        .pickup = Pickup_Health,
        .use = nullptr,
        .drop = nullptr,
        .weaponthink = nullptr,
        .pickup_sound = "items/weaponry_pickup.wav", // TODO: WID: Give custom sound perhaps?
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
        .info = nullptr,
        .tag = ITEM_TAG_NONE,
        .precaches = "items/weaponry_pickup.wav items/s_health.wav items/n_health.wav items/l_health.wav items/m_health.wav"
    },


    //********************************************************
    //  End of list marker:                                 **
    //********************************************************
    {NULL}
};


/**
*   @brief  item_health (.3 .3 1) ( -16 - 16 - 16 ) ( 16 16 16 )
**/
void SP_item_health(svg_base_edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        SVG_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/medium/tris.md2";
    self->count = 10;
    SVG_SpawnItem(self, SVG_FindItem("Health"));
    gi.soundindex("items/n_health.wav");
}

/**
*   @brief  item_health_small (.3 .3 1) ( -16 - 16 - 16 ) ( 16 16 16 )
**/
void SP_item_health_small(svg_base_edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        SVG_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/stimpack/tris.md2";
    self->count = 2;
    SVG_SpawnItem(self, SVG_FindItem("Health"));
    self->style = HEALTH_IGNORE_MAX;
    gi.soundindex("items/s_health.wav");
}

/**
*   @brief  item_health_large (.3 .3 1) ( -16 - 16 - 16 ) ( 16 16 16 )
**/
void SP_item_health_large(svg_base_edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        SVG_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/large/tris.md2";
    self->count = 25;
    SVG_SpawnItem(self, SVG_FindItem("Health"));
    gi.soundindex("items/l_health.wav");
}

/**
*   @brief  item_health_mega (.3 .3 1) ( -16 - 16 - 16 ) ( 16 16 16 )
**/
void SP_item_health_mega(svg_base_edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        SVG_FreeEdict(self);
        return;
    }

    self->model = "models/items/mega_h/tris.md2";
    self->count = 100;
    SVG_SpawnItem(self, SVG_FindItem("Health"));
    gi.soundindex("items/m_health.wav");
    self->style = HEALTH_IGNORE_MAX | HEALTH_TIMED;
}

/**
*   @brief  Calculate the number of items value.
**/
void SVG_InitItems(void)
{
    game.num_items = sizeof(itemlist) / sizeof(itemlist[0]) - 1;
}



/*
===============
SVG_SetItemNames

Called by worldspawn
===============
*/
void SVG_SetItemNames(void)
{
    int     i;
    gitem_t *it;

    for (i = 0 ; i < game.num_items ; i++) {
        it = &itemlist[i];
        gi.configstring(CS_ITEMS + i, it->pickup_name);
    }

    //jacket_armor_index = ITEM_INDEX(SVG_FindItem("Jacket Armor"));
    //combat_armor_index = ITEM_INDEX(SVG_FindItem("Combat Armor"));
    //body_armor_index   = ITEM_INDEX(SVG_FindItem("Body Armor"));
    //power_screen_index = ITEM_INDEX(SVG_FindItem("Power Screen"));
    //power_shield_index = ITEM_INDEX(SVG_FindItem("Power Shield"));
}
