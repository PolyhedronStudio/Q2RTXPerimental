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
            .durationMsec = sg_time_t::from_hz( 1 )
        },
        // Mode Animation: DRAWING
        /*modeAnimationFrames[ WEAPON_MODE_DRAWING ] = */{
            .startFrame = 82,
            .endFrame = 107,
            .durationMsec = sg_time_t::from_hz( 107 - 82 )
        },
        // Mode Animation: HOLSTERING
        /*modeAnimationFrames[ WEAPON_MODE_HOLSTERING ] = */{
            .startFrame = 56,
            .endFrame = 81,
            .durationMsec = sg_time_t::from_hz( 81 - 56 )
        },
        // Mode Animation: PRIMARY_FIRING
        /*modeAnimationFrames[ WEAPON_MODE_PRIMARY_FIRING ] =*/ {
            .startFrame = 1,
            .endFrame = 14,
            .durationMsec = sg_time_t::from_hz( 14 - 1 )
        },
        // Mode Animation: SECONDARY_FIRING -- UNUSED FOR PISTOL LOGIC.
        /*modeAnimationFrames[ WEAPON_MODE_PRIMARY_FIRING ] =*/ {
            .startFrame = 1,
            .endFrame = 14,
            .durationMsec = sg_time_t::from_hz( 14 - 1 )
        },
        // Mode Animation: RELOADING
        /*modeAnimationFrames[ WEAPON_MODE_RELOADING ] = */{
            .startFrame = 15,
            .endFrame = 55,
            .durationMsec = sg_time_t::from_hz( 55 - 15 )
        },
        // Mode Animation: AIMING IN
        /*modeAnimationFrames[ WEAPON_MODE_AIM_IN ] = */
        {
            .startFrame = 108,
            .endFrame = 122,
            .durationMsec = sg_time_t::from_hz( 122 - 108 )
        },
        // Mode Animation: AIMING FIRE
        /*modeAnimationFrames[ WEAPON_MODE_AIM_FIRE ] = */
        {
            .startFrame = 123,
            .endFrame = 136,
            .durationMsec = sg_time_t::from_hz( 136 - 123 )
        },
        // Mode Animation: AIMING OUT
        /*modeAnimationFrames[ WEAPON_MODE_AIM_OUT ] = */
        {
            .startFrame = 137,
            .endFrame = 151,
            .durationMsec = sg_time_t::from_hz( 151 - 137 )
        }
    },

    // TODO: Move quantity etc from gitem_t into this struct.
};

static constexpr float PRIMARY_FIRE_BULLET_HSPREAD = 300.f;
static constexpr float PRIMARY_FIRE_BULLET_VSPREAD = 300.f;

static constexpr float SECONDARY_FIRE_BULLET_HSPREAD = 50.f;
static constexpr float SECONDARY_FIRE_BULLET_VSPREAD = 50.f;

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
    Vector3     start;
    Vector3     forward, right;
    int         damage = 10;
    int         kick = 8;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    QM_AngleVectors( ent->client->viewMove.viewAngles, &forward, &right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    // Project from source to shot destination.
    vec3_t shotOffset = { 0, 10, (float)ent->viewheight - 5.5f };
    P_ProjectDistance( ent, ent->s.origin, shotOffset, &forward.x, &right.x, &start.x );

    // Fire the actual bullet itself.
    fire_bullet( ent, &start.x, &forward.x, damage, kick, PRIMARY_FIRE_BULLET_HSPREAD, PRIMARY_FIRE_BULLET_VSPREAD, MOD_CHAINGUN );

    // Project from source to muzzleflash destination.
    vec3_t muzzleFlashOffset = { 16, 10, (float)ent->viewheight };
    P_ProjectDistance( ent, ent->s.origin, muzzleFlashOffset, &forward.x, &right.x, &start.x );

    // Send a muzzle flash event.
    gi.WriteUint8( svc_muzzleflash );
    gi.WriteInt16( ent - g_edicts );
    gi.WriteUint8( MZ_PISTOL /*| is_silenced*/ );
    gi.multicast( &start.x/*ent->s.origin*/, MULTICAST_PVS, false );

    // Notify we're making noise.
    P_PlayerNoise( ent, &start.x, PNOISE_WEAPON );

    // Decrease clip ammo.
    //if ( !( (int)dmflags->value & DF_INFINITE_AMMO ) )
    ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ]--;

}
/**
*   @brief  Supplements the Secondary Firing routine by actually performing a more precise 'single bullet' shot.
**/
void weapon_pistol_secondary_fire( edict_t *ent ) {
    Vector3      start;
    Vector3      forward, right;
    int         damage = 14;
    int         kick = 10;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    QM_AngleVectors( ent->client->viewMove.viewAngles, &forward, &right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    // Project from source to shot destination.
    vec3_t shotOffset = { 0, 0, (float)ent->viewheight };
    P_ProjectSource( ent, ent->s.origin, shotOffset, &forward.x, &right.x, &start.x );

    // Determine the amount to multiply bullet spread with based on the player's velocity.
    // TODO: Use stats array or so for storing the actual move speed limit, since this
    // may vary.
    constexpr float moveThreshold = 300.0f;
    // Don't spread multiply if we're pretty much standing still. This allows for a precise shot.
    const float hSpreadMultiplier = ( ent->client->ps.xyzSpeed > 5 ? moveThreshold * ent->client->ps.bobMove : 0 );
    float vSpreadMultiplier = ( ent->client->ps.xyzSpeed > 5 ? moveThreshold * ent->client->ps.bobMove : 0 );
    //gi.dprintf( " ----- ----- ----- \n" );
    //gi.dprintf( "%s: hSpreadMultiplier(%f) vSpreadMultiplier(%f)\n", __func__, hSpreadMultiplier, vSpreadMultiplier );

    // Fire the actual bullet itself.
    fire_bullet( ent, &start.x, &forward.x, damage, kick, SECONDARY_FIRE_BULLET_HSPREAD + hSpreadMultiplier, SECONDARY_FIRE_BULLET_VSPREAD + vSpreadMultiplier, MOD_CHAINGUN );

    // Project from source to muzzleflash destination.
    vec3_t muzzleFlashOffset = { 16, 0, (float)ent->viewheight };
    P_ProjectDistance( ent, ent->s.origin, muzzleFlashOffset, &forward.x, &right.x, &start.x ); //P_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

    // Send a muzzle flash event.
    gi.WriteUint8( svc_muzzleflash );
    gi.WriteInt16( ent - g_edicts );
    gi.WriteUint8( MZ_PISTOL /*| is_silenced*/ );
    gi.multicast( &start.x, MULTICAST_PVS, false );

    // Notify we're making noise.
    P_PlayerNoise( ent, &start.x, PNOISE_WEAPON );

    // Decrease clip ammo.
    //if ( !( (int)dmflags->value & DF_INFINITE_AMMO ) )
    ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ]--;

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
    if ( clipCapacity > totalAmmo ) {
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
*   @brief  Will play out of ammo sound.
**/
static void weapon_pistol_no_ammo( edict_t *ent ) {
    if ( level.time >= ent->client->weaponState.timers.lastEmptyWeaponClick ) {
        gi.sound( ent, CHAN_VOICE, gi.soundindex( "weapons/pistol/noammo.wav" ), 1, ATTN_NORM, 0 );
        ent->client->weaponState.timers.lastEmptyWeaponClick = level.time + 75_ms;
    }
}

/**
*   @brief  Processes responses to the user input.
**/
static void Weapon_Pistol_ProcessUserInput( edict_t *ent ) {
    /**
    *   isAiming Behavior Path:
    **/
    if ( ent->client->weaponState.aimState.isAiming == true ) {
        /**
        *   Letting go of 'Secondary Fire': "Aim Out" of isAiming Mode:
        **/
        if ( !( ent->client->buttons & BUTTON_SECONDARY_FIRE ) ) {
            //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_OUT )\n", __func__ );
            P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_OUT, pistolItemInfo.modeFrames, false );
            // Restore the original FOV.
            P_ResetPlayerStateFOV( ent->client );
        }
        /**
        *   Firing 'Primary Fire' of isAiming Mode:
        **/
        if ( ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) ) {
            // Switch to Firing mode if we have Clip Ammo:
            if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] ) {
                //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_FIRE )\n", __func__ );
                P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_FIRE, pistolItemInfo.modeFrames, false );
            // Attempt to reload otherwise:
            } else {
                // We need to have enough ammo left to reload with.
                if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                    // When enabled, will move out of isAiming mode, reset FOV, and perform a reload, upon
                    // which if secondary fire is still pressed, it'll return to aiming in.
                    #if 0
                    //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_RELOADING )\n", __func__ );
                    // Exit isAiming mode.
                    ent->client->weaponState.aimState.isAiming = false;
                    // Restore the original FOV.
                    P_ResetPlayerStateFOV( ent->client );
                    // Screen should be lerping back to FOV, engage into reload mode.
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeFrames, false );
                    #else
                    // Play out of ammo click sound.
                    weapon_pistol_no_ammo( ent );
                    #endif
                } else {
                    // Play out of ammo click sound.
                    weapon_pistol_no_ammo( ent );
                }
            }
        }
    /**
    *   Regular Behavior Path:
    **/
    } else {
        /**
        *   Pressing 'Secondary Fire': "Aim In" to engage for 'isAiming' Mode:
        **/
        if ( ( ent->client->buttons & BUTTON_SECONDARY_FIRE ) ) {
            //gi.dprintf( "%s: NOT isAiming -> SwitchMode( WEAPON_MODE_AIM_IN )\n", __func__ );
            P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_IN, pistolItemInfo.modeFrames, false );
            ent->client->ps.fov = 45;

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
                    // Play out of ammo click sound.
                    weapon_pistol_no_ammo( ent );
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
                // Play out of ammo click sound.
                weapon_pistol_no_ammo( ent );
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
    if ( ent->client->weaponState.aimState.isAiming == true ) {
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
                // Disengage aiming state.
                ent->client->weaponState.aimState = {};
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
            // Set the isAiming state value for aiming specific behavior to true right at the end of its animation.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.endFrame ) {
                //! Engage aiming mode.
                ent->client->weaponState.aimState.isAiming = true;
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
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1 ) {
                ent->client->weapon_sound = gi.soundindex( "weapons/pistol/draw.wav" );
                ent->client->weaponState.timers.lastDrawn = level.time;
            }
            // Enough time has passed, shutdown the sound.
            else if ( ent->client->weaponState.timers.lastDrawn <= level.time - 250_ms ) {
                ent->client->weapon_sound = 0;

                // Store the client's fieldOfView.
                ent->client->weaponState.clientFieldOfView = ent->client->ps.fov;
            }
        // Holster Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_HOLSTERING ) {
            // Start playing holster weapon sound at the very first frame.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1 ) {
                ent->client->weapon_sound = gi.soundindex( "weapons/pistol/holster.wav" );
                ent->client->weaponState.timers.lastHolster = level.time;
            }
            // Enough time has passed, shutdown the sound.
            else if ( ent->client->weaponState.timers.lastHolster <= level.time - 250_ms ) {
                ent->client->weapon_sound = 0;
            }
        }
    }
}