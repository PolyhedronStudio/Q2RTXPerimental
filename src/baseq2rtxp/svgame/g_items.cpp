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
#include "g_local.h"




//void Weapon_Blaster(edict_t *ent);
//void Weapon_Shotgun(edict_t *ent);
void Weapon_Pistol( edict_t *ent );
//void Weapon_SuperShotgun(edict_t *ent);
//void Weapon_Machinegun(edict_t *ent);
//void Weapon_Chaingun(edict_t *ent);
//void Weapon_HyperBlaster(edict_t *ent);
//void Weapon_RocketLauncher(edict_t *ent);
//void Weapon_Grenade(edict_t *ent);
//void Weapon_GrenadeLauncher(edict_t *ent);
//void Weapon_Railgun(edict_t *ent);
//void Weapon_BFG(edict_t *ent);
//void Weapon_FlareGun(edict_t *ent);

// for passing into *info member of gitem_t.
gitem_armor_t jacketarmor_info  = { 25,  50, .30f, .00f, ITEM_TAG_ARMOR_JACKET};
gitem_armor_t combatarmor_info  = { 50, 100, .60f, .30f, ITEM_TAG_ARMOR_COMBAT};
gitem_armor_t bodyarmor_info    = {100, 200, .80f, .60f, ITEM_TAG_ARMOR_BODY};

//extern weapon_mode_frames_t pistolAnimationFrames[ WEAPON_MODE_MAX ];
extern weapon_item_info_t pistolItemInfo;

static int  jacket_armor_index;
static int  combat_armor_index;
static int  body_armor_index;
static int  power_screen_index;
static int  power_shield_index;

#define HEALTH_IGNORE_MAX   1
#define HEALTH_TIMED        2

void Use_Quad(edict_t *ent, gitem_t *item);
static sg_time_t  quad_drop_timeout_hack;

/**
*
*
*   Item Inventory:
*
*
**/
/**
*   @brief
**/
gitem_t *GetItemByIndex(int index) {
    if (index == 0 || index >= game.num_items)
        return NULL;

    return &itemlist[index];
}
/**
*   @brief
**/
gitem_t *FindItemByClassname(const char *classname) {
    int     i;
    gitem_t *it;

    it = itemlist;
    for (i = 0 ; i < game.num_items ; i++, it++) {
        if (!it->classname)
            continue;
        if (!Q_stricmp(it->classname, classname))
            return it;
    }

    return NULL;
}
/**
*   @brief
**/
gitem_t *FindItem(const char *pickup_name) {
    int     i;
    gitem_t *it;

    it = itemlist;
    for (i = 0 ; i < game.num_items ; i++, it++) {
        if (!it->pickup_name)
            continue;
        if (!Q_stricmp(it->pickup_name, pickup_name))
            return it;
    }

    return NULL;
}

//======================================================================
/**
*
*
*   Item Respawning:
*
*
**/
void DoRespawn(edict_t *ent)
{
    if (ent->team) {
        edict_t *master;
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
    gi.linkentity(ent);

    // send an effect
    ent->s.event = EV_ITEM_RESPAWN;
}

void SetRespawn(edict_t *ent, float delay)
{
    ent->flags = static_cast<ent_flags_t>( ent->flags | FL_RESPAWN );
    ent->svflags |= SVF_NOCLIENT;
    ent->solid = SOLID_NOT;
    ent->nextthink = level.time + sg_time_t::from_sec( delay );
    ent->think = DoRespawn;
    gi.linkentity(ent);
}

/**
*   @brief
**/
void Drop_General(edict_t *ent, gitem_t *item)
{
    Drop_Item(ent, item);
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);
}



/**
*
*
*   Misc Item specific Pickup Callbacks:
*
*
**/
/**
*   @brief
**/
const bool Pickup_Powerup( edict_t *ent, edict_t *other ) {
    int     quantity;

    quantity = other->client->pers.inventory[ ITEM_INDEX( ent->item ) ];
    if ( ( skill->value == 1 && quantity >= 2 ) || ( skill->value >= 2 && quantity >= 1 ) )
        return false;

    if ( ( coop->value ) && ( ent->item->flags & ITEM_FLAG_STAY_COOP ) && ( quantity > 0 ) )
        return false;

    other->client->pers.inventory[ ITEM_INDEX( ent->item ) ]++;

    if ( deathmatch->value ) {
        if ( !( ent->spawnflags & DROPPED_ITEM ) )
            SetRespawn( ent, ent->item->quantity );
        if ( ( (int)dmflags->value & DF_INSTANT_ITEMS ) || ( ( ent->item->use == Use_Quad ) && ( ent->spawnflags & DROPPED_PLAYER_ITEM ) ) ) {
            if ( ( ent->item->use == Use_Quad ) && ( ent->spawnflags & DROPPED_PLAYER_ITEM ) )
                quad_drop_timeout_hack = ent->nextthink - level.time;
            ent->item->use( other, ent->item );
        }
    }

    return true;
}
/**
*   @brief
**/
const bool Pickup_Adrenaline(edict_t *ent, edict_t *other)
{
    if (!deathmatch->value)
        other->max_health += 1;

    if (other->health < other->max_health)
        other->health = other->max_health;

    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, ent->item->quantity);

    return true;
}
/**
*   @brief
**/
const bool Pickup_AncientHead(edict_t *ent, edict_t *other)
{
    other->max_health += 2;

    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, ent->item->quantity);

    return true;
}
/**
*   @brief
**/
const bool Pickup_Bandolier(edict_t *ent, edict_t *other)
{
    gitem_t *item;
    int     index;

    //if (other->client->pers.max_bullets < 250)
    //    other->client->pers.max_bullets = 250;
    //if (other->client->pers.max_shells < 150)
    //    other->client->pers.max_shells = 150;
    //if (other->client->pers.max_cells < 250)
    //    other->client->pers.max_cells = 250;
    //if (other->client->pers.max_slugs < 75)
    //    other->client->pers.max_slugs = 75;

    //item = FindItem("Bullets");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_bullets)
    //        other->client->pers.inventory[index] = other->client->pers.max_bullets;
    //}

    //item = FindItem("Shells");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_shells)
    //        other->client->pers.inventory[index] = other->client->pers.max_shells;
    //}

    //if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
    //    SetRespawn(ent, ent->item->quantity);

    return true;
}
/**
*   @brief  
**/
const bool Pickup_Pack(edict_t *ent, edict_t *other)
{
    gitem_t *item;
    int     index;

    //if (other->client->pers.max_bullets < 300)
    //    other->client->pers.max_bullets = 300;
    //if (other->client->pers.max_shells < 200)
    //    other->client->pers.max_shells = 200;
    //if (other->client->pers.max_rockets < 100)
    //    other->client->pers.max_rockets = 100;
    //if (other->client->pers.max_grenades < 100)
    //    other->client->pers.max_grenades = 100;
    //if (other->client->pers.max_cells < 300)
    //    other->client->pers.max_cells = 300;
    //if (other->client->pers.max_slugs < 100)
    //    other->client->pers.max_slugs = 100;

    //item = FindItem("Bullets");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_bullets)
    //        other->client->pers.inventory[index] = other->client->pers.max_bullets;
    //}

    //item = FindItem("Shells");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_shells)
    //        other->client->pers.inventory[index] = other->client->pers.max_shells;
    //}

    //item = FindItem("Cells");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_cells)
    //        other->client->pers.inventory[index] = other->client->pers.max_cells;
    //}

    //item = FindItem("Grenades");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_grenades)
    //        other->client->pers.inventory[index] = other->client->pers.max_grenades;
    //}

    //item = FindItem("Rockets");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_rockets)
    //        other->client->pers.inventory[index] = other->client->pers.max_rockets;
    //}

    //item = FindItem("Slugs");
    //if (item) {
    //    index = ITEM_INDEX(item);
    //    other->client->pers.inventory[index] += item->quantity;
    //    if (other->client->pers.inventory[index] > other->client->pers.max_slugs)
    //        other->client->pers.inventory[index] = other->client->pers.max_slugs;
    //}

    //if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
    //    SetRespawn(ent, ent->item->quantity);

    return true;
}



//======================================================================

void Use_Quad(edict_t *ent, gitem_t *item)
{
    sg_time_t     timeout;

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (quad_drop_timeout_hack) {
        timeout = quad_drop_timeout_hack;
        quad_drop_timeout_hack = 0_ms;
    } else {
        timeout = 300_ms;
    }

    if (ent->client->quad_time > level.time)
        ent->client->quad_time += timeout;
    else
        ent->client->quad_time = level.time + timeout;

    gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Breather(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

	ent->client->breather_time = std::max( level.time, ent->client->breather_time ) + 30_sec;

//  gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Envirosuit(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

	ent->client->enviro_time = std::max( level.time, ent->client->enviro_time ) + 30_sec;

//  gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void    Use_Invulnerability(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

	ent->client->invincible_time = std::max( level.time, ent->client->invincible_time ) + 30_sec;

    gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void    Use_Silencer(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);
    ent->client->silencer_shots += 30;

//  gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

const bool Pickup_Key(edict_t *ent, edict_t *other)
{
    if (coop->value) {
        if (strcmp(ent->classname, "key_power_cube") == 0) {
            if (other->client->pers.power_cubes & ((ent->spawnflags & 0x0000ff00) >> 8))
                return false;
            other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
            other->client->pers.power_cubes |= ((ent->spawnflags & 0x0000ff00) >> 8);
        } else {
            if (other->client->pers.inventory[ITEM_INDEX(ent->item)])
                return false;
            other->client->pers.inventory[ITEM_INDEX(ent->item)] = 1;
        }
        return true;
    }
    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
    return true;
}

static constexpr int32_t testvar = 6 / 2 * ( 2 + 1 );

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
const bool Add_Ammo(edict_t *ent, gitem_t *item, int count)
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
const bool Pickup_Ammo(edict_t *itemEntity, edict_t *other) {
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
        //if (other->client->pers.weapon != itemEntity->item && (!deathmatch->value || other->client->pers.weapon == FindItem("blaster")))
        //    other->client->newweapon = itemEntity->item;
        if ( other->client->pers.weapon != itemEntity->item ) {
            other->client->newweapon = itemEntity->item;
        }
    }

    // Set an item respawn for DM mode.
    if ( !( itemEntity->spawnflags & ( DROPPED_ITEM | DROPPED_PLAYER_ITEM ) ) && ( deathmatch->value ) ) {
        SetRespawn( itemEntity, 30 );
    }
    return true;
}

/**
*   @brief  Drops ammo.
**/
void Drop_Ammo(edict_t *ent, gitem_t *item) {
    int32_t index = ITEM_INDEX(item);
    edict_t *dropped = Drop_Item(ent, item);
    if (ent->client->pers.inventory[index] >= item->quantity)
        dropped->count = item->quantity;
    else
        dropped->count = ent->client->pers.inventory[index];

    //if (ent->client->pers.weapon &&
    //    ent->client->pers.weapon->tag == AMMO_GRENADES &&
    //    item->tag == AMMO_GRENADES &&
    //    ent->client->pers.inventory[index] - dropped->count <= 0) {
    //    gi.cprintf(ent, PRINT_HIGH, "Can't drop current weapon\n");
    //    G_FreeEdict(dropped);
    //    return;
    //}

    ent->client->pers.inventory[index] -= dropped->count;
    ValidateSelectedItem(ent);
}


/**
*
*
*   Health:
*
*
**/
void MegaHealth_think(edict_t *self)
{
    if (self->owner->health > self->owner->max_health) {
        self->nextthink = level.time + 1_sec;
        self->owner->health -= 1;
        return;
    }

    if (!(self->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(self, 20);
    else
        G_FreeEdict(self);
}

const bool Pickup_Health(edict_t *ent, edict_t *other)
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
        ent->flags = static_cast<ent_flags_t>( ent->flags | FL_RESPAWN );
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
    } else {
        if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
            SetRespawn(ent, 30);
    }

    return true;
}



/**
*
*
*   Armor:
*
*
**/
int ArmorIndex(edict_t *ent)
{
    if (!ent->client)
        return 0;

    if (ent->client->pers.inventory[jacket_armor_index] > 0)
        return jacket_armor_index;

    if (ent->client->pers.inventory[combat_armor_index] > 0)
        return combat_armor_index;

    if (ent->client->pers.inventory[body_armor_index] > 0)
        return body_armor_index;

    return 0;
}

const bool Pickup_Armor(edict_t *ent, edict_t *other)
{
    int             old_armor_index;
    gitem_armor_t   *oldinfo;
    gitem_armor_t   *newinfo;
    int             newcount;
    float           salvage;
    int             salvagecount;

    // get info on new armor
    newinfo = (gitem_armor_t *)ent->item->info;

    old_armor_index = ArmorIndex(other);

    // handle armor shards specially
    if (ent->item->tag == ITEM_TAG_ARMOR_SHARD) {
        if (!old_armor_index)
            other->client->pers.inventory[jacket_armor_index] = 2;
        else
            other->client->pers.inventory[old_armor_index] += 2;
    }

    // if player has no armor, just use it
    else if (!old_armor_index) {
        other->client->pers.inventory[ITEM_INDEX(ent->item)] = newinfo->base_count;
    }

    // use the better armor
    else {
        // get info on old armor
        if (old_armor_index == jacket_armor_index)
            oldinfo = &jacketarmor_info;
        else if (old_armor_index == combat_armor_index)
            oldinfo = &combatarmor_info;
        else // (old_armor_index == body_armor_index)
            oldinfo = &bodyarmor_info;

        if (newinfo->normal_protection > oldinfo->normal_protection) {
            // calc new armor values
            salvage = oldinfo->normal_protection / newinfo->normal_protection;
            salvagecount = salvage * other->client->pers.inventory[old_armor_index];
            newcount = newinfo->base_count + salvagecount;
            if (newcount > newinfo->max_count)
                newcount = newinfo->max_count;

            // zero count of old armor so it goes away
            other->client->pers.inventory[old_armor_index] = 0;

            // change armor to new item with computed value
            other->client->pers.inventory[ITEM_INDEX(ent->item)] = newcount;
        } else {
            // calc new armor values
            salvage = newinfo->normal_protection / oldinfo->normal_protection;
            salvagecount = salvage * newinfo->base_count;
            newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
            if (newcount > oldinfo->max_count)
                newcount = oldinfo->max_count;

            // if we're already maxed out then we don't need the new armor
            if (other->client->pers.inventory[old_armor_index] >= newcount)
                return false;

            // update current armor value
            other->client->pers.inventory[old_armor_index] = newcount;
        }
    }

    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, 20);

    return true;
}



/**
*
* 
*   Power Armor: 
* 
* 
**/
int PowerArmorType(edict_t *ent)
{
    if (!ent->client)
        return POWER_ARMOR_NONE;

    if (!(ent->flags & FL_POWER_ARMOR))
        return POWER_ARMOR_NONE;

    if (ent->client->pers.inventory[power_shield_index] > 0)
        return POWER_ARMOR_SHIELD;

    if (ent->client->pers.inventory[power_screen_index] > 0)
        return POWER_ARMOR_SCREEN;

    return POWER_ARMOR_NONE;
}

void Use_PowerArmor(edict_t *ent, gitem_t *item)
{
    int     index;

    if (ent->flags & FL_POWER_ARMOR) {
        ent->flags = static_cast<ent_flags_t>( ent->flags & ~FL_POWER_ARMOR );
        gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
    } else {
        index = ITEM_INDEX(FindItem("cells"));
        if (!ent->client->pers.inventory[index]) {
            gi.cprintf(ent, PRINT_HIGH, "No cells for power armor.\n");
            return;
        }
        ent->flags = static_cast<ent_flags_t>( ent->flags | FL_POWER_ARMOR );

        gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
    }
}

const bool Pickup_PowerArmor(edict_t *ent, edict_t *other)
{
    int     quantity;

    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (deathmatch->value) {
        if (!(ent->spawnflags & DROPPED_ITEM))
            SetRespawn(ent, ent->item->quantity);
        // auto-use for DM only if we didn't already have one
        if (!quantity)
            ent->item->use(other, ent->item);
    }

    return true;
}

void Drop_PowerArmor(edict_t *ent, gitem_t *item)
{
    if ((ent->flags & FL_POWER_ARMOR) && (ent->client->pers.inventory[ITEM_INDEX(item)] == 1))
        Use_PowerArmor(ent, item);
    Drop_General(ent, item);
}



/**
*
*
*   Item Entity:
*
*
**/
/**
*   @brief  
**/
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
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
        G_UseTargets(ent, other);
        ent->spawnflags |= ITEM_TARGETS_USED;
    }

    if (!taken)
        return;

    if (!((coop->value) && (ent->item->flags & ITEM_FLAG_STAY_COOP)) || (ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM))) {
        if (ent->flags & FL_RESPAWN)
            ent->flags = static_cast<ent_flags_t>( ent->flags & ~FL_RESPAWN );
        else
            G_FreeEdict(ent);
    }
}

/**
*   @brief
**/
void drop_temp_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    Touch_Item(ent, other, plane, surf);
}
/**
*   @brief
**/
void drop_make_touchable(edict_t *ent)
{
    ent->touch = Touch_Item;
    if (deathmatch->value) {
        ent->nextthink = level.time + 29_sec;
        ent->think = G_FreeEdict;
    }
}
/**
*   @brief
**/
edict_t *Drop_Item(edict_t *ent, gitem_t *item)
{
    edict_t *dropped;
    vec3_t  forward, right;
    vec3_t  offset;

    dropped = G_AllocateEdict();

    dropped->classname = item->classname;
    dropped->item = item;
    dropped->spawnflags = DROPPED_ITEM;
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
        trace_t trace;

        AngleVectors(ent->client->v_angle, forward, right, NULL);
        VectorSet(offset, 24, 0, -16);
        G_ProjectSource(ent->s.origin, offset, forward, right, dropped->s.origin);
        trace = gi.trace(ent->s.origin, dropped->mins, dropped->maxs,
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
void Use_Item(edict_t *ent, edict_t *other, edict_t *activator)
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
void droptofloor(edict_t *ent)
{
    trace_t     tr;
    vec3_t      dest;

    if ( ent->item && ent->item->tag >= ITEM_TAG_AMMO_BULLETS_PISTOL && ent->item->tag <= ITEM_TAG_AMMO_SHELLS_SHOTGUN ) {
        VectorSet( ent->mins, -8, -8, -8 );
        VectorSet( ent->maxs, 8, 8, 8 );
    } else if ( ent->item ) {
        VectorSet( ent->mins, -16, -16, -16 );
        VectorSet( ent->maxs, 16, 16, 16 );
    }

    if (ent->model)
        gi.setmodel(ent, ent->model);
    else
        gi.setmodel(ent, ent->item->world_model);
    ent->solid = SOLID_TRIGGER;
    ent->movetype = MOVETYPE_TOSS;
    ent->touch = Touch_Item;

    VectorCopy(ent->s.origin, dest);
    dest[2] -= 128;

    tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
    if (tr.startsolid) {
        gi.dprintf("droptofloor: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
        G_FreeEdict(ent);
        return;
    }

    VectorCopy(tr.endpos, ent->s.origin);

    if (ent->team) {
        ent->flags = static_cast<ent_flags_t>( ent->flags & ~FL_TEAMSLAVE );
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
void PrecacheItem(gitem_t *it)
{
	// WID: C++20: Added const.
    const char    *s;
	const char *start;
    char    data[MAX_QPATH];
    int     len;
    gitem_t *ammo;

    if (!it)
        return;

    if (it->pickup_sound)
        gi.soundindex(it->pickup_sound);
    if (it->world_model)
        gi.modelindex(it->world_model);
    if (it->view_model)
        gi.modelindex(it->view_model);
    if (it->icon)
        gi.imageindex(it->icon);

    // parse everything for its ammo
    if (it->ammo && it->ammo[0]) {
        ammo = FindItem(it->ammo);
        if (ammo != it)
            PrecacheItem(ammo);
    }

    // parse the space seperated precache string for other items
    s = it->precaches;
    if (!s || !s[0])
        return;

    while (*s) {
        start = s;
        while (*s && *s != ' ')
            s++;

        len = s - start;
        if (len >= MAX_QPATH || len < 5)
            gi.error("PrecacheItem: %s has bad precache string", it->classname);
        memcpy(data, start, len);
        data[len] = 0;
        if (*s)
            s++;

        // determine type based on extension
        if (!strcmp(data + len - 3, "md2"))
            gi.modelindex(data);
        else if (!strcmp(data + len - 3, "sp2"))
            gi.modelindex(data);
        else if (!strcmp(data + len - 3, "wav"))
            gi.soundindex(data);
        else if (!strcmp(data + len - 3, "pcx"))
            gi.imageindex(data);
    }
}

/**
*   @brief  Sets the clipping size and plants the object on the floor.
*
*           Items can't be immediately dropped to floor, because they might
*           be on an entity that hasn't spawned yet.
**/
void SpawnItem(edict_t *ent, gitem_t *item)
{
    PrecacheItem(item);

    if (ent->spawnflags) {
        if (strcmp(ent->classname, "key_power_cube") != 0) {
            ent->spawnflags = 0;
            gi.dprintf("%s at %s has invalid spawnflags set\n", ent->classname, vtos(ent->s.origin));
        }
    }

    // some items will be prevented in deathmatch
    if (deathmatch->value) {
        if ((int)dmflags->value & DF_NO_ARMOR) {
            if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor) {
                G_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_NO_ITEMS) {
            if (item->pickup == Pickup_Powerup) {
                G_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_NO_HEALTH) {
            if (item->pickup == Pickup_Health || item->pickup == Pickup_Adrenaline || item->pickup == Pickup_AncientHead) {
                G_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_INFINITE_AMMO) {
            if ((item->flags == ITEM_FLAG_AMMO) || (strcmp(ent->classname, "weapon_bfg") == 0)) {
                G_FreeEdict(ent);
                return;
            }
        }
    }

    if (coop->value && (strcmp(ent->classname, "key_power_cube") == 0)) {
        ent->spawnflags |= (1 << (8 + level.power_cubes));
        level.power_cubes++;
    }

    // don't let them drop items that stay in a coop game
    if ((coop->value) && (item->flags & ITEM_FLAG_STAY_COOP)) {
        item->drop = NULL;
    }

    ent->item = item;
    ent->nextthink = level.time + 20_hz;    // items start after other solids
    ent->think = droptofloor;
    ent->s.effects = item->world_model_flags;
    ent->s.renderfx = RF_GLOW;
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

    /*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "item_armor_body",
        .pickup = Pickup_Armor,
        .use = NULL,
        .drop = NULL,
        .weaponthink = NULL,
        .pickup_sound = "misc/ar1_pkup.wav",
        .world_model = "models/items/armor/body/tris.md2", 
        .world_model_flags = EF_ROTATE,
        .view_model = NULL,
        /* icon */      .icon = "i_bodyarmor",
        /* pickup */    .pickup_name = "Body Armor",
        /* width */     .count_width = 3,
        .quantity = 0,
        .clip_capacity = 0,
        .ammo = NULL,
        .flags = ITEM_FLAG_ARMOR,
        .weapon_index = 0,
        .info = &bodyarmor_info,
        .tag = ITEM_TAG_ARMOR_BODY,
        /* precache */ .precaches = ""
    },

    /*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_armor_combat",
        Pickup_Armor,
        NULL,
        NULL,
        NULL,
        "misc/ar1_pkup.wav",
        "models/items/armor/combat/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_combatarmor",
        /* pickup */    "Combat Armor",
        /* width */     3,
        0,
        0,
        NULL,
        ITEM_FLAG_ARMOR,
        0,
        &combatarmor_info,
        ITEM_TAG_ARMOR_COMBAT,
        /* precache */ ""
    },

    /*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_armor_jacket",
        Pickup_Armor,
        NULL,
        NULL,
        NULL,
        "misc/ar1_pkup.wav",
        "models/items/armor/jacket/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_jacketarmor",
        /* pickup */    "Jacket Armor",
        /* width */     3,
        0,
        0,
        NULL,
        ITEM_FLAG_ARMOR,
        0,
        &jacketarmor_info,
        ITEM_TAG_ARMOR_JACKET,
        /* precache */ ""
    },

    /*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_armor_shard",
        Pickup_Armor,
        NULL,
        NULL,
        NULL,
        "misc/ar2_pkup.wav",
        "models/items/armor/shard/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_jacketarmor",
        /* pickup */    "Armor Shard",
        /* width */     3,
        0,
        0,
        NULL,
        ITEM_FLAG_ARMOR,
        0,
        NULL,
        ITEM_TAG_ARMOR_SHARD,
        /* precache */ ""
    },


    /*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_power_screen",
        Pickup_PowerArmor,
        Use_PowerArmor,
        Drop_PowerArmor,
        NULL,
        "misc/ar3_pkup.wav",
        "models/items/armor/screen/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_powerscreen",
        /* pickup */    "Power Screen",
        /* width */     0,
        60,
        0,
        NULL,
        ITEM_FLAG_ARMOR,
        0,
        NULL,
        ITEM_TAG_NONE,
        /* precache */ ""
    },

    /*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16)     */
    {
        "item_power_shield",
        Pickup_PowerArmor,
        Use_PowerArmor,
        Drop_PowerArmor,
        NULL,
        "misc/ar3_pkup.wav",
        "models/items/armor/shield/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_powershield",
        /* pickup */    "Power Shield",
        /* width */     0,
        60,
        0,
        NULL,
        ITEM_FLAG_ARMOR,
        0,
        NULL,
        ITEM_TAG_NONE,
        /* precache */ "misc/power2.wav misc/power1.wav"
    },


    //
    // WEAPONS
    //

    ///* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16)
    //always owned, never in the world
    //*/
    //{
    //    "weapon_blaster",
    //    NULL,
    //    Use_Weapon,
    //    NULL,
    //    Weapon_Blaster,
    //    "misc/w_pkup.wav",
    //    NULL, 0,
    //    "models/weapons/v_blast/tris.md2",
    //    /* icon */      "w_blaster",
    //    /* pickup */    "Blaster",
    //    0,
    //    0,
    //    NULL,
    //    ITEM_FLAG_WEAPON | ITEM_FLAG_STAY_COOP,
    //    WEAP_BLASTER,
    //    NULL,
    //    0,
    //    /* precache */ "models/objects/laser/tris.md2 weapons/blastf1a.wav misc/lasfly.wav"
    //},

    //*QUAKED weapon_pistol (.3 .3 1) (-16 -16 -16) (16 16 16)     
    {
        .classname = "weapon_pistol",
        .pickup = P_Weapon_Pickup,
        .use =  P_Weapon_Use,
        .drop = P_Weapon_Drop,
        .weaponthink = Weapon_Pistol,
        
        .pickup_sound = "misc/w_pkup.wav",
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

        .precaches = "models/g_wep/pistol/tris.iqm models/v_wep/pistol/tris.iqm weapons/pistol/draw.wav weapons/pistol/holster.wav weapons/pistol/fire1.wav weapons/pistol/fire2.wav weapons/pistol/fire3.wav weapons/pistol/reload.wav"
    },


    //
    // AMMO ITEMS
    //
    // QUAKED ammo_bullets_pistol (.3 .3 1) (-8 -8 -8) (8 8 8)
    {
        "ammo_bullets_pistol",
        Pickup_Ammo,
        NULL,
        Drop_Ammo,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/bullets_pistol/tris.iqm", 0,
        NULL,
        /* icon */      "a_bullets",
        /* pickup */    "Pistol Bullets",
        /* width */     3,
        50,
        0,
        NULL,
        ITEM_FLAG_AMMO,
        0,
        NULL,
        ITEM_TAG_AMMO_BULLETS_PISTOL,
        // Precache.
        "models/items/ammo/bullets_pistol/tris.iqm misc/am_pkup.wav"
    },
    // QUAKED ammo_bullets_rifle (.3 .3 1) (-8 -8 -8) (8 8 8)
    {
        "ammo_bullets_rifle",
        Pickup_Ammo,
        NULL,
        Drop_Ammo,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/bullets_rifle/tris.iqm", 0,
        NULL,
        /* icon */      "a_bullets",
        /* pickup */    "Rifle Bullets",
        /* width */     3,
        50,
        0,
        NULL,
        ITEM_FLAG_AMMO,
        0,
        NULL,
        ITEM_TAG_AMMO_BULLETS_RIFLE,
        // Precache
        "models/items/ammo/bullets_rifle/tris.iqm misc/am_pkup.wav"
    },
    
    // QUAKED ammo_bullets_smg (.3 .3 1) (-8 -8 -8) (8 8 8)
    {
        "ammo_bullets_smg",
        Pickup_Ammo,
        NULL,
        Drop_Ammo,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/bullets_smg/tris.iqm", 0,
        NULL,
        /* icon */      "a_bullets",
        /* pickup */    "SMG Bullets",
        /* width */     3,
        50,
        0,
        NULL,
        ITEM_FLAG_AMMO,
        0,
        NULL,
        ITEM_TAG_AMMO_BULLETS_SMG,
        // Precache.
        "models/items/ammo/bullets_smg/tris.iqm misc/am_pkup.wav"
    },

    // QUAKED ammo_bullets_sniper (.3 .3 1) (-8 -8 -8) (8 8 8)
    {
        "ammo_bullets_sniper",
        Pickup_Ammo,
        NULL,
        Drop_Ammo,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/bullets_sniper/tris.iqm", 0,
        NULL,
        /* icon */      "a_bullets",
        /* pickup */    "Sniper Bullets",
        /* width */     3,
        50,
        0,
        NULL,
        ITEM_FLAG_AMMO,
        0,
        NULL,
        ITEM_TAG_AMMO_BULLETS_SNIPER,
        // Precache.
        "models/items/ammo/bullets_sniper/tris.iqm misc/am_pkup.wav"
    },

    /*QUAKED ammo_shells_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "ammo_shells_shotgun",
        Pickup_Ammo,
        NULL,
        Drop_Ammo,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/shells_shotgun/tris.iqm", 0,
        NULL,
        /* icon */      "a_shells",
        /* pickup */    "Shotgun Shells",
        /* width */     3,
        10,
        0,
        NULL,
        ITEM_FLAG_AMMO,
        0,
        NULL,
        ITEM_TAG_AMMO_SHELLS_SHOTGUN,
        
        // Precache
        "models/items/ammo/shells_shotgun/tris.iqm misc/am_pkup.wav"
    },

    //
    // POWERUP ITEMS
    //



    //
    //  Health
    //
    {
        NULL,
        Pickup_Health,
        NULL,
        NULL,
        NULL,
        "items/pkup.wav",
        NULL, 0,
        NULL,
        /* icon */      "i_health",
        /* pickup */    "Health",
        /* width */     3,
        0,
        0,
        NULL,
        0,
        0,
        NULL,
        ITEM_TAG_NONE,
        /* precache */ "items/pkup.wav items/s_health.wav items/n_health.wav items/l_health.wav items/m_health.wav"
    },

    // end of list marker
    {NULL}
};


/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/medium/tris.md2";
    self->count = 10;
    SpawnItem(self, FindItem("Health"));
    gi.soundindex("items/n_health.wav");
}

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health_small(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/stimpack/tris.md2";
    self->count = 2;
    SpawnItem(self, FindItem("Health"));
    self->style = HEALTH_IGNORE_MAX;
    gi.soundindex("items/s_health.wav");
}

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health_large(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/large/tris.md2";
    self->count = 25;
    SpawnItem(self, FindItem("Health"));
    gi.soundindex("items/l_health.wav");
}

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health_mega(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/mega_h/tris.md2";
    self->count = 100;
    SpawnItem(self, FindItem("Health"));
    gi.soundindex("items/m_health.wav");
    self->style = HEALTH_IGNORE_MAX | HEALTH_TIMED;
}


void InitItems(void)
{
    game.num_items = sizeof(itemlist) / sizeof(itemlist[0]) - 1;
}



/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames(void)
{
    int     i;
    gitem_t *it;

    for (i = 0 ; i < game.num_items ; i++) {
        it = &itemlist[i];
        gi.configstring(CS_ITEMS + i, it->pickup_name);
    }

    jacket_armor_index = ITEM_INDEX(FindItem("Jacket Armor"));
    combat_armor_index = ITEM_INDEX(FindItem("Combat Armor"));
    body_armor_index   = ITEM_INDEX(FindItem("Body Armor"));
    power_screen_index = ITEM_INDEX(FindItem("Power Screen"));
    power_shield_index = ITEM_INDEX(FindItem("Power Shield"));
}
