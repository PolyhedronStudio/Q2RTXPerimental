/********************************************************************
*
*
*	SVGame: Pistol Weapon Implementation.
*
*
********************************************************************/
#include "../g_local.h"



/**
*
*
*	Pistol Item Configuration:
*
*
**/
/**
*   Pistol Weapon Mode Animation Frames.
**/
weapon_item_info_t pistolItemInfo = {
    .modeFrames/*[ WEAPON_MODE_MAX ]*/ = {

        // Mode Animation: IDLE
        /*modeAnimationFrames[ WEAPON_MODE_IDLE ] = */{
            .startFrame = 0,
            .endFrame = 1,
            .durationFrames = 1
        },
        // Mode Animation: DRAWING
        /*modeAnimationFrames[ WEAPON_MODE_DRAWING ] = */{
            .startFrame = 86,
            .endFrame = 111,
            .durationFrames = ( 111 - 86 )
        },
        // Mode Animation: HOLSTERING
        /*modeAnimationFrames[ WEAPON_MODE_HOLSTERING ] = */{
            .startFrame = 54,
            .endFrame = 85,
            .durationFrames = ( 85 - 54 )
        },
        // Mode Animation: PRIMARY_FIRING
        /*modeAnimationFrames[ WEAPON_MODE_PRIMARY_FIRING ] =*/ {
            .startFrame = 1,
            .endFrame = 13,
            .durationFrames = ( 13 - 1 )
        },
        // Mode Animation: SECONDARY_FIRING -- UNUSED FOR PISTOL LOGIC.
        /*modeAnimationFrames[ WEAPON_MODE_PRIMARY_FIRING ] =*/ {
            .startFrame = 1,
            .endFrame = 13,
            .durationFrames = ( 13 - 1 )
        },
        // Mode Animation: RELOADING
        /*modeAnimationFrames[ WEAPON_MODE_RELOADING ] = */{
            .startFrame = 13,
            .endFrame = 54,
            .durationFrames = ( 54 - 13 )
        },
        // Mode Animation: AIMING IN
        /*modeAnimationFrames[ WEAPON_MODE_AIM_IN ] = */
        {
            .startFrame = 111,
            .endFrame = 126,
            .durationFrames = ( 126 - 111 )
        },
        // Mode Animation: AIMING FIRE
        /*modeAnimationFrames[ WEAPON_MODE_AIM_FIRE ] = */
        {
            .startFrame = 126,
            .endFrame = 143,
            .durationFrames = ( 143 - 126 )
        },
        // Mode Animation: AIMING OUT
        /*modeAnimationFrames[ WEAPON_MODE_AIM_OUT ] = */
        {
            .startFrame = 143,
            .endFrame = 156,
            .durationFrames = ( 156 - 143 )
        }
    },

    // TODO: Move quantity etc from gitem_t into this struct.
};



/**
*
*
*   Pistol Function Implementations:
*
*
**/
/**
*   @brief  Supplements the Primary Firing routine by actually performing a 'single bullet' shot.
**/
void weapon_pistol_primary_fire( edict_t *ent ) {
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 14;
    int         kick = 8;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    AngleVectors( ent->client->v_angle, forward, right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    // Project from source to shot destination.
    VectorSet( offset, 0, 10, (float)ent->viewheight - 5.5f ); // VectorSet( offset, 0, 8, ent->viewheight - 8 );
    P_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

    // Fire the actual bullet itself.
    fire_bullet( ent, start, forward, damage, kick, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MOD_CHAINGUN );

    // Send a muzzle flash event.
    gi.WriteUint8( svc_muzzleflash );
    gi.WriteInt16( ent - g_edicts );
    gi.WriteUint8( MZ_PISTOL /*| is_silenced*/ );
    gi.multicast( ent->s.origin, MULTICAST_PVS, false );

    // Notify we're making noise.
    P_PlayerNoise( ent, start, PNOISE_WEAPON );

    // Decrease clip ammo.
    ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ]--;
    //if ( !( (int)dmflags->value & DF_INFINITE_AMMO ) )
    //    ent->client->pers.inventory[ ent->client->ammo_index ]--;

}
/**
*   @brief  Supplements the Secondary Firing routine by actually performing a more precise 'single bullet' shot.
**/
void weapon_pistol_secondary_fire( edict_t *ent ) {
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 14;
    int         kick = 8;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    AngleVectors( ent->client->v_angle, forward, right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    // Project from source to shot destination.
    VectorSet( offset, 0, 0, (float)ent->viewheight - 5.5f ); // VectorSet( offset, 0, 8, ent->viewheight - 8 );
    P_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

    // Fire the actual bullet itself.
    fire_bullet( ent, start, forward, damage, kick, DEFAULT_BULLET_HSPREAD / 3, DEFAULT_BULLET_VSPREAD / 3, MOD_CHAINGUN );

    // Send a muzzle flash event.
    gi.WriteUint8( svc_muzzleflash );
    gi.WriteInt16( ent - g_edicts );
    gi.WriteUint8( MZ_PISTOL /*| is_silenced*/ );
    gi.multicast( ent->s.origin, MULTICAST_PVS, false );

    // Notify we're making noise.
    P_PlayerNoise( ent, start, PNOISE_WEAPON );

    // Decrease clip ammo.
    ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ]--;
    //if ( !( (int)dmflags->value & DF_INFINITE_AMMO ) )
    //    ent->client->pers.inventory[ ent->client->ammo_index ]--;

}
/**
*   @brief  Will do the maths to reload the weapon clip.
**/
static const bool weapon_pistol_reload_clip( edict_t *ent ) {
    // Clip capacity.
    const int32_t clipCapacity = ent->client->pers.weapon->clip_capacity;
    // Ammo to stuff into the clip.
    int32_t clipAmmo = clipCapacity;
    // Get total ammo amount.
    int32_t totalAmmo = ent->client->pers.inventory[ ent->client->ammo_index ];
    if ( totalAmmo < clipCapacity ) {
        clipAmmo = totalAmmo;
    }
    // This ends badly for the player, no more ammo to reload with.
    if ( totalAmmo <= 0 ) {
        return false;
    }

    // Refill the clip ammo.
    ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] = clipAmmo;
    // Subtract the total clip ammo from the carrying inventory ammo.
    ent->client->pers.inventory[ ent->client->ammo_index ] -= clipAmmo;

    // Return true if we reloaded succesfully.
    return true;
}

/**
*   @brief  Processes responses to the user input.
**/
static void Weapon_Pistol_ProcessUserInput( edict_t *ent ) {
    /**
    *   isAiming Behavior Path:
    **/
    if ( ent->client->weaponState.isAiming == true ) {
        /**
        *   Letting go of 'Secondary Fire': "Aim Out" of isAiming Mode:
        **/
        if ( !( ent->client->buttons & BUTTON_SECONDARY_FIRE ) ) {
            P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_OUT, pistolItemInfo.modeFrames, false );
            gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_OUT )\n", __func__ );
        }
        /**
        *   Firing 'Primary Fire' of isAiming Mode:
        **/
        if ( ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) ) {
            // Switch to Firing mode if we have Clip Ammo:
            if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] ) {
                P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_FIRE, pistolItemInfo.modeFrames, false );
                // Attempt to reload otherwise:
            } else {
                // We need to have enough ammo left to reload with.
                if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                    gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_RELOADING )\n", __func__ );
                    //P_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeFrames, false );
                } else {
                    // TODO: Play out of ammo sound, switch weapon?
                }
            }
            gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_FIRE )\n", __func__ );
        }
    /**
    *   Regular Behavior Path:
    **/
    } else {
        /**
        *   Pressing 'Secondary Fire': "Aim In" to engage for 'isAiming' Mode:
        **/
        if ( ( ent->client->buttons & BUTTON_SECONDARY_FIRE ) ) {
            P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_IN, pistolItemInfo.modeFrames, false );
            gi.dprintf( "%s: NOT isAiming -> SwitchMode( WEAPON_MODE_AIM_IN )\n", __func__ );
            return;
        }

        /**
        *   @brief  Primary Fire Button Implementation:
        **/
        if ( ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) /*&& ( ent->client->buttons & BUTTON_ATTACK )*/ ) {
            // Switch to Firing mode if we have Clip Ammo:
            if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] ) {
                P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeFrames, false );
                // Attempt to reload otherwise:
            } else {
                // We need to have enough ammo left to reload with.
                if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeFrames, false );
                } else {
                    // TODO: Play out of ammo sound, switch weapon?
                }
            }
            return;
        }
        /**
        *   Reload Button Implementation:
        **/
        if ( ent->client->latched_buttons & BUTTON_RELOAD ) {
            // We need to have enough ammo left to reload with.
            if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                P_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeFrames, false );
            } else {
                // TODO: Play out of ammo sound, switch weapon?
            }
            return;
        }
    }
}
/**
*   @brief  Pistol Weapon 'State Machine'.
**/
void Weapon_Pistol( edict_t *ent ) {
    // Process the animation frames of the mode we're in.
    const bool isDoneAnimating = P_Weapon_ProcessModeAnimation( ent, &pistolItemInfo.modeFrames[ ent->client->weaponState.mode ] );

    // If done animating, switch back to idle mode.
    //if ( isDoneAnimating ) {
    //    P_Weapon_SwitchMode( ent, WEAPON_MODE_IDLE, pistolAnimationFrames, true );
    //}

    // If IDLE or NOT ANIMATING, process user input.
    if ( ent->client->weaponState.mode == WEAPON_MODE_IDLE || isDoneAnimating ) {
        // Respond to user input, which determines whether 
        Weapon_Pistol_ProcessUserInput( ent );
    }

    /**
    *   isAiming Behavior Path:
    **/
    if ( ent->client->weaponState.isAiming == true ) {
        // isAiming -> Fire:
        if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_FIRE ) {
            // Due to this being possibly called multiple times in the same frame, we depend on a timer for this to prevent
            // any earlier/double firing.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 3 ) {
                // Wait out until enough time has passed.
                if ( ent->client->weaponState.timers.lastAimedFire <= ( level.time + 325_ms ) ) {
                    // Fire the pistol bullet.
                    weapon_pistol_secondary_fire( ent );
                    // Store the time we last 'primary fired'.
                    ent->client->weaponState.timers.lastAimedFire = level.time;
                }
            }
        // isAiming -> "Aim Out":
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_OUT ) {
            // Due to this being possibly called multiple times in the same frame, we depend on a timer for this to prevent
            // any earlier/double firing.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.endFrame ) {
                ent->client->weaponState.isAiming = false;
            }
        }
    } else {
        // Process logic for state specific modes and their frames.
        // Primary Fire:
        if ( ent->client->weaponState.mode == WEAPON_MODE_PRIMARY_FIRING ) {
            // Due to this being possibly called multiple times in the same frame, we depend on a timer for this to prevent
            // any earlier/double firing.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 3 ) {
                // Wait out until enough time has passed.
                if ( ent->client->weaponState.timers.lastPrimaryFire <= ( level.time + 325_ms ) ) {
                    // Fire the pistol bullet.
                    weapon_pistol_primary_fire( ent );
                    // Store the time we last 'primary fired'.
                    ent->client->weaponState.timers.lastPrimaryFire = level.time;
                }
            }
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_IN ) {
            // Set the isAiming state value for aiming specific behavior to true.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.endFrame ) {
                ent->client->weaponState.isAiming = true;
            }
        // Reload Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_RELOADING ) {
            // Start playing clip reload sound. (Takes about the same duration as a pistol reload, 1 second.)
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
                ent->client->weapon_sound = gi.soundindex( "weapons/pistol/reload.wav" );
            }
            // Stop audio and actually reload the clip stats at the end of the animation sequence.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.endFrame - 1 ) {
                ent->client->weapon_sound = 0;
                weapon_pistol_reload_clip( ent );
           }
        // Draw Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_DRAWING ) {
            // Start playing drawing weapon sound at the very first frame.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
                ent->client->weapon_sound = gi.soundindex( "weapons/pistol/draw.wav" );
                ent->client->weaponState.timers.lastDrawn = level.time;
            }
            // Enough time has passed, shutdown the sound.
            if ( ent->client->weaponState.timers.lastDrawn <= level.time + 250_ms ) {
                ent->client->weapon_sound = 0;
            }
        // Holster Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_HOLSTERING ) {
            // Start playing holster weapon sound at the very first frame.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
                ent->client->weapon_sound = gi.soundindex( "weapons/pistol/holster.wav" );
                ent->client->weaponState.timers.lastHolster = level.time;
            }
            // Enough time has passed, shutdown the sound.
            if ( ent->client->weaponState.timers.lastHolster <= level.time + 250_ms ) {
                ent->client->weapon_sound = 0;
            }
        }
    }
}