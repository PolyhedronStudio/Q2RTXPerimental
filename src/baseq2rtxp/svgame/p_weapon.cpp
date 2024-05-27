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
// g_weapon.c

#include "g_local.h"
#include "m_player.h"


static bool     is_quad;
static byte     is_silenced;

//void weapon_grenade_fire(edict_t *ent, bool held);
/**
*   @detail Each player can have two noise objects associated with it:
*           a personal noise (jumping, pain, weapon firing), and a weapon
*           target noise (bullet wall impacts)
*
*           Monsters that don't directly see the player can move
*           to a noise in hopes of seeing the player from there.
**/
void P_PlayerNoise( edict_t *who, const vec3_t where, int type ) {
    edict_t *noise;

    if ( type == PNOISE_WEAPON ) {
        if ( who->client->silencer_shots ) {
            who->client->silencer_shots--;
            return;
        }
    }

    if ( deathmatch->value )
        return;

    if ( who->flags & FL_NOTARGET )
        return;


    if ( !who->mynoise ) {
        noise = G_AllocateEdict();
        noise->classname = "player_noise";
        VectorSet( noise->mins, -8, -8, -8 );
        VectorSet( noise->maxs, 8, 8, 8 );
        noise->owner = who;
        noise->svflags = SVF_NOCLIENT;
        who->mynoise = noise;

        noise = G_AllocateEdict();
        noise->classname = "player_noise";
        VectorSet( noise->mins, -8, -8, -8 );
        VectorSet( noise->maxs, 8, 8, 8 );
        noise->owner = who;
        noise->svflags = SVF_NOCLIENT;
        who->mynoise2 = noise;
    }

    if ( type == PNOISE_SELF || type == PNOISE_WEAPON ) {
        noise = who->mynoise;
        level.sound_entity = noise;
        level.sound_entity_framenum = level.framenum;
    } else { // type == PNOISE_IMPACT
        noise = who->mynoise2;
        level.sound2_entity = noise;
        level.sound2_entity_framenum = level.framenum;
    }

    VectorCopy( where, noise->s.origin );
    VectorSubtract( where, noise->maxs, noise->absmin );
    VectorAdd( where, noise->maxs, noise->absmax );
    noise->last_sound_time = level.time;
    gi.linkentity( noise );
}

/** 
*   @brief  Called when a weapon item has been touched.
**/
const bool P_Weapon_Pickup( edict_t *ent, edict_t *other ) {
    int         index;

    // Get the item index.
    index = ITEM_INDEX(ent->item);

    // Leave the weapon for others to pickup if we already got one.
    if ((((int)(dmflags->value) & DF_WEAPONS_STAY) || coop->value)
        && other->client->pers.inventory[index]) {
        if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)))
            return false;   // leave the weapon for others to pickup
    }

    // Increment item count.
    other->client->pers.inventory[index]++;

    if (!(ent->spawnflags & DROPPED_ITEM)) {
        // Pointer to ammo type.
        const gitem_t *ammo;
        // give them some ammo with it
        ammo = FindItem(ent->item->ammo);
        if ( (int)dmflags->value & DF_INFINITE_AMMO ) {
            Add_Ammo( other, ammo, 1000 );
        } else {
            Add_Ammo( other, ammo, ammo->quantity );
        }

        // If weapon wasn't dropped by a player.
        if ( !( ent->spawnflags & DROPPED_PLAYER_ITEM ) ) {
            // Deathmatch Path:
            if ( deathmatch->value ) {
                // Apply item entity FL_RESPAWN flag immediately.
                if ( (int)( dmflags->value ) & DF_WEAPONS_STAY ) {
                    ent->flags = static_cast<ent_flags_t>( ent->flags | FL_RESPAWN );
                // Set a duration for respawning:
                } else {
                    SetRespawn( ent, 30 );
                }
            }
            // Coop Path:
            if ( coop->value ) {
                // Apply item entity FL_RESPAWN flag.
                ent->flags = static_cast<ent_flags_t>( ent->flags | FL_RESPAWN );
            }
        }
    }

    // Set item as the new weapon to switch to.
    if ( other->client->pers.weapon != ent->item &&
        ( other->client->pers.inventory[ index ] == 1 ) &&
        ( !deathmatch->value || other->client->pers.weapon == FindItem( "pistol" ) ) ) {
        other->client->newweapon = ent->item;
    }

    return true;
}



/**
*   @brief  Called when the 'Old Weapon' has been dropped all the way. This function will
*           make the 'newweapon' as the client's current weapon.
**/
void P_Weapon_Change(edict_t *ent) {
    int32_t i = 0;

    //if (ent->client->grenade_time) {
    //    ent->client->grenade_time = level.time;
    //    ent->client->weapon_sound = 0;
    //    weapon_grenade_fire(ent, false);
    //    ent->client->grenade_time = 0_ms;
    //}

    // Can we already change modes at all? (Is our weapon occupied still performing some other mode of action?)
    const bool canChangeMode = ( ent->client->weaponState.canChangeMode || ent->client->weaponState.mode == WEAPON_MODE_IDLE );
    // Determine some other needed things.
    const bool isAiming = ent->client->weaponState.aimState.isAiming;
    const bool isHolsterMode = ( ent->client->weaponState.mode == WEAPON_MODE_HOLSTERING );
    const bool isAnimationFinished = ( ent->client->weaponState.animation.currentFrame >= ent->client->weaponState.animation.endFrame );

    /**
    *   Bit of an ugly ... approach, I'll admit that, but it still beats the OG vanilla code.
    **/
    // If we have an active weapon.
    if ( ent->client->pers.weapon != ent->client->newweapon ) {
        // Engage into holster if:
        if ( canChangeMode && ( !isHolsterMode && !isAiming ) ) {
            P_Weapon_SwitchMode( ent, WEAPON_MODE_HOLSTERING, (const weapon_mode_frames_t *)ent->client->pers.weapon->info, true );
            return;
        }
        // We know this function will continue to be called if newweapon is not nulled out.
        // So make sure not to continue this function unless we are finished with holstering the weapon.
        //
        // We will continue with the function when the weapon is fully down, and only then engage into the
        // actual weapon switch.
        if ( isHolsterMode && isAnimationFinished ) {
            // Allow the change to take place.
            goto allowchange;
        // We aren't ready to make the switch YET.
        } else {
            return;
        }
        // Also allow the change to take place.
    }
    //return;
allowchange:
    ent->client->pers.lastweapon = ent->client->pers.weapon;
    ent->client->pers.weapon = ent->client->newweapon;
    ent->client->newweapon = nullptr;

    // Set visible weapon model.
    if (ent->s.modelindex == MODELINDEX_PLAYER ) {
        if ( ent->client->pers.weapon ) {
            i = ( ( ent->client->pers.weapon->weapon_index & 0xff ) << 8 );
        } else {
            i = 0;
        }
        ent->s.skinnum = (ent - g_edicts - 1) | i;
    }

    // Find the appropriate matching ammo index.
    if ( ent->client->pers.weapon && ent->client->pers.weapon->ammo ) {
        ent->client->ammo_index = ITEM_INDEX( FindItem( ent->client->pers.weapon->ammo ) );
    } else {
        ent->client->ammo_index = 0;
    }

    if (!ent->client->pers.weapon) {
        // dead
        ent->client->ps.gunindex = 0;
        return;
    }

    // Now switch the actual model up for the player state.
    ent->client->ps.gunindex = gi.modelindex( ent->client->pers.weapon->view_model );

    // Engage into "Drawing" weapon mode.
    P_Weapon_SwitchMode( ent, WEAPON_MODE_DRAWING, (const weapon_mode_frames_t*)ent->client->pers.weapon->info, true );

    // ADjust player client animation.
    ent->client->anim_priority = ANIM_PAIN;
    if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
        ent->s.frame = FRAME_crpain1;
        ent->client->anim_end = FRAME_crpain4;
    } else {
        ent->s.frame = FRAME_pain301;
        ent->client->anim_end = FRAME_pain304;
    }
	ent->client->anim_time = 0_ms;
}

/**
*   @brief  Callback for when a weapon change is enforced to occure due to having ran out of ammo.
**/
//static void P_NoAmmoWeaponChange( edict_t *ent, bool sound = false ) {
//	if ( sound ) {
//		if ( level.time >= ent->client->empty_weapon_click_sound ) {
//			gi.sound( ent, CHAN_VOICE, gi.soundindex( "weapons/noammo.wav" ), 1, ATTN_NORM, 0 );
//			ent->client->empty_weapon_click_sound = level.time + 1_sec;
//		}
//	}
//
//    // Find the next best weapon to utilize.
//    //if (ent->client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
//    //    &&  ent->client->pers.inventory[ITEM_INDEX(FindItem("machinegun"))]) {
//    //    ent->client->newweapon = FindItem("machinegun");
//    //    return;
//    //}
//    //if (ent->client->pers.inventory[ITEM_INDEX(FindItem("shells"))] > 1
//    //    &&  ent->client->pers.inventory[ITEM_INDEX(FindItem("super shotgun"))]) {
//    //    ent->client->newweapon = FindItem("super shotgun");
//    //    return;
//    //}
//    //if (ent->client->pers.inventory[ITEM_INDEX(FindItem("shells"))]
//    //    &&  ent->client->pers.inventory[ITEM_INDEX(FindItem("shotgun"))]) {
//    //    ent->client->newweapon = FindItem("shotgun");
//    //    return;
//    //}
//    
//    // We got no other weapons yet.
//    ent->client->newweapon = FindItem( "fists" );
//}



/**
*   @brief  Will prepare a switch to the newly passed weapon.
**/
void P_Weapon_Use( edict_t *ent, const gitem_t *item ) {
    int32_t ammo_index = 0;
    const gitem_t *ammo_item = nullptr;

    // Escape if we're already using it
    if ( item == ent->client->pers.weapon ) {
        return;
    }

    // See if the weapon we're going to use has any ammo in its clip.
    bool hasClipAmmo = false;
    if ( ent->client->pers.weapon ) {
        if ( ent->client->pers.weapon->weapon_index ) {
            if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] >= 1 ) {
                hasClipAmmo = true;
            }
        }
    }

    if ( item->ammo && !g_select_empty->value && !( item->flags & ITEM_FLAG_AMMO ) ) {
        ammo_item = FindItem( item->ammo );
        ammo_index = ITEM_INDEX( ammo_item );

        if ( !hasClipAmmo && !ent->client->pers.inventory[ ammo_index ] ) {
            gi.cprintf( ent, PRINT_HIGH, "No %s for %s.\n", ammo_item->pickup_name, item->pickup_name );
            return;
        }

        if ( !hasClipAmmo && ent->client->pers.inventory[ ammo_index ] < item->quantity ) {
            gi.cprintf( ent, PRINT_HIGH, "Not enough %s for %s.\n", ammo_item->pickup_name, item->pickup_name );
            return;
        }
    }

    // Weapons require the info pointer to be set to their specific description struct.
    if ( item->info ) {
        // change to this weapon when down
        ent->client->newweapon = item;
    }
}
/**
*   @brief  Called if the weapon item is wanted to be dropped by the player.
**/
void P_Weapon_Drop( edict_t *ent, const gitem_t *item ) {
    int     index;

    // Don't allow dropping in WEAPONS_STAY Deathmatch mode.
    if ( (int)( dmflags->value ) & DF_WEAPONS_STAY ) {
        return;
    }

    // Get item index.
    index = ITEM_INDEX( item );

    // See if we're already using it (it is an active weapon), if we are, exit.
    if ( ( ( item == ent->client->pers.weapon ) || ( item == ent->client->newweapon ) ) && ( ent->client->pers.inventory[ index ] == 1 ) ) {
        gi.cprintf( ent, PRINT_HIGH, "Can't drop current weapon\n" );
        return;
    }

    // Drop the item.
    Drop_Item( ent, item );
    // Decrement item inventory count.
    ent->client->pers.inventory[ index ]--;
}



/**
*
*
*	Weapon Target Projecting:
*
*
**/
/**
*   @brief Project the 'ray of fire' from the source to its (source + dir * distance) target.
**/
void P_ProjectDistance( edict_t *ent, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result ) {
    // Adjust distance to handedness.
    Vector3 _distance = distance;
    if ( ent->client->pers.hand == LEFT_HANDED ) {
        _distance[ 1 ] *= -1;
    } else if ( ent->client->pers.hand == CENTER_HANDED ) {
        _distance[ 1 ] = 0;
    }
    G_ProjectSource( point, &_distance.x, forward, right, result );

    // Aim fix from Yamagi Quake 2.
    // Now the projectile hits exactly where the scope is pointing.
    //if ( aimfix->value ) {
    //    vec3_t start, end;
    //    VectorSet( start, ent->s.origin[ 0 ], ent->s.origin[ 1 ], ent->s.origin[ 2 ] + (float)ent->viewheight );
    //    VectorMA( start, CM_MAX_WORLD_SIZE, forward, end );

    //    trace_t	tr = gi.trace( start, NULL, NULL, end, ent, MASK_SHOT );
    //    if ( tr.fraction < 1 ) {
    //        VectorSubtract( tr.endpos, result, forward );
    //        VectorNormalize( forward );
    //    }
    //}
}
/**
*   @brief Project the 'ray of fire' from the source, and then target the crosshair/scope's center screen
*          point as the final destination.
*   @note   The forward vector is normalized.
**/
void P_ProjectSource( edict_t *ent, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result ) {
    // Adjust distance to handedness.
    Vector3 _distance = distance;
    if ( ent->client->pers.hand == LEFT_HANDED ) {
        _distance[ 1 ] *= -1;
    } else if ( ent->client->pers.hand == CENTER_HANDED ) {
        _distance[ 1 ] = 0;
    }
    G_ProjectSource( point, &_distance.x, forward, right, result );

    // Aim fix from Yamagi Quake 2.
    // Now the projectile hits exactly where the scope is pointing.
    const Vector3 start = { ent->s.origin[ 0 ], ent->s.origin[ 1 ], ent->s.origin[ 2 ] + (float)ent->viewheight };
    const Vector3 end = QM_Vector3MultiplyAdd( start, CM_MAX_WORLD_SIZE, forward );
    trace_t	tr = gi.trace( &start.x, NULL, NULL, &end.x, ent, MASK_SHOT );
    if ( tr.fraction < 1 ) {
        VectorSubtract( tr.endpos, result, forward );
        VectorNormalize( forward );
    }
}



/**
*
*
*	Core Weapon Mechanics:
* 
* 
**/
/**
*   @brief  Will switch the weapon to its 'newMode' if it can, unless enforced(force == true).
**/
void P_Weapon_SwitchMode( edict_t *ent, const weapon_mode_t newMode, const weapon_mode_frames_t *weaponModeFrames, const bool force = false ) {
    // Only switch if we're allowed to.
    if ( ( ent->client->weaponState.canChangeMode || force ) /* &&
        ( ent->client->weaponState.mode != ent->client->weaponState.oldMode )*/ ) {
        // We can switch, fetch animation data for the new mode.
        const weapon_mode_frames_t *modeFrames= &weaponModeFrames[ newMode ];

        // Assign the new state's new mode.
        ent->client->weaponState.oldMode = ent->client->weaponState.mode;
        ent->client->weaponState.mode = newMode;

        // Setup the new animation frames for the state work with.
        ent->client->weaponState.animation.startFrame = modeFrames->startFrame;
        ent->client->weaponState.animation.currentFrame = modeFrames->startFrame;
        ent->client->weaponState.animation.endFrame = modeFrames->endFrame;

        // Make sure that the client's player state frame is adjusted also.
        //ent->client->ps.gunframe = modeFrames->startFrame;

        // Enforce a wait to finish animating mode before allowing yet another change.
        ent->client->weaponState.canChangeMode = false;
    }
}

/**
*   @brief  Advances the animation of the 'mode' we're currently in.
**/
const bool P_Weapon_ProcessModeAnimation( edict_t *ent, const weapon_mode_frames_t *weaponModeFrames ) {
    if ( !ent->client->pers.weapon ) {
        gi.dprintf( "%s: if ( !ent->client->pers.weapon) {..\n", __func__ );
        return false;
    }

    // Get current gunframe.
    int32_t gunFrame = ent->client->weaponState.animation.currentFrame;
    // Increment.
    gunFrame++;
    // Debug
    //gi.dprintf( "%s: gunFrame(%i)\n", __func__, gunFrame );

    // Determine whether we are finished processing the mode.
    if ( gunFrame > ent->client->weaponState.animation.endFrame ) {
        // Enable mode switching again.
        ent->client->weaponState.canChangeMode = true;
        // Keep gun frame in check.
        gunFrame = ent->client->ps.gunframe = ent->client->weaponState.animation.endFrame;
        // Finished animating.
        return true;
    } else {
        // Store current gun frame.
        ent->client->weaponState.animation.currentFrame = gunFrame;
        // Assign newly calculated frame to player state.
        ent->client->ps.gunframe = gunFrame;
    }

    // Still animating.
    return false;
}

/**
*   @brief  Perform the weapon's logical 'think' routine. This is Is either
*           called by ClientBeginServerFrame or ClientThink.
**/
void P_Weapon_Think( edict_t *ent ) {
    // If we just died, put the weapon away.
    if ( ent->health < 1 ) {
        // Select no weapon.
        ent->client->newweapon = nullptr;
        // Apply an instant change since we're dead.
        P_Weapon_Change( ent );
        
        // Escape, since we won't be performing any more weapon thinking from this point on until we respawn properly.
        return;
    }

    // Call active weapon think routine if any at all.
    const bool hasActiveWeapon = ( ent->client->pers.weapon != nullptr ? true : false );
    const bool hasThinkRoutine = ( hasActiveWeapon && ent->client->pers.weapon->weaponthink != nullptr ? true : false );
    if ( hasActiveWeapon && hasThinkRoutine ) {
        ent->client->pers.weapon->weaponthink( ent );
    }
}