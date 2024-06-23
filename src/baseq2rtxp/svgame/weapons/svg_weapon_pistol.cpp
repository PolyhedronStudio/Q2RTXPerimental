/********************************************************************
*
*
*	SVGame: Pistol Weapon Implementation.
*
*
********************************************************************/
#include "svgame/svg_local.h"



/**
*
*
*	Pistol Item Configuration:
*
*
**/
//! Bullet Spread for default firing.
static constexpr float PRIMARY_FIRE_BULLET_MIN_HSPREAD = 225.f;
static constexpr float PRIMARY_FIRE_BULLET_MIN_VSPREAD = 225.f;
//! Maximum Recoil Spread for default firing.
static constexpr float PRIMARY_FIRE_BULLET_RECOIL_MAX_HSPREAD = 950.f;
static constexpr float PRIMARY_FIRE_BULLET_RECOIL_MAX_VSPREAD = 950.f;
//! Recoil Spread for aimed targetting secondary fire.
static constexpr float SECONDARY_FIRE_BULLET_HSPREAD = 50.f;
static constexpr float SECONDARY_FIRE_BULLET_VSPREAD = 50.f;

/**
*   Pistol 'Weapon' Item Info.
**/
weapon_item_info_t pistolItemInfo = {
    // These are dynamically loaded from the IQM model data.
    .modeAnimations/*[ WEAPON_MODE_MAX ]*/ = { },

    // TODO: Move quantity etc from gitem_t into this struct.
};

/**
*   @brief  Precache animation information.
**/
void Weapon_Pistol_Precached( const gitem_t *item ) {
    // Get the model we want the animation data from.
    const model_t *viewModel = gi.GetModelDataForName( item->view_model );
    if ( !viewModel ) {
        gi.dprintf( "%s: No viewmodel found. Failed to precache '%s' weapon animation data for item->view_model(%s)\n", __func__, item->classname, item->view_model );
        return;
    }
    // Validate the IQM data.
    if ( !viewModel->iqmData ) {
        gi.dprintf( "%s: No IQMData found. Failed to precache '%s' weapon animation data for item->view_model(%s)\n", __func__, item->classname, item->view_model );
        return;
    }

    // We can safely operate now.
    const iqm_model_t *iqmData = viewModel->iqmData;
    // Iterate animations.
    for ( int32_t animID = 0; animID < iqmData->num_animations; animID++ ) {
        // IQM animation.
        const iqm_anim_t *iqmAnim = &iqmData->animations[ animID ];

        if ( !strcmp( iqmAnim->name, "idle" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_IDLE );
        } else if ( !strcmp( iqmAnim->name, "draw" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_DRAWING );
        } else if ( !strcmp( iqmAnim->name, "holster" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_HOLSTERING );
        } else if ( !strcmp( iqmAnim->name, "fire" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_PRIMARY_FIRING );
        } else if ( !strcmp( iqmAnim->name, "reload" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_RELOADING );
        } else if ( !strcmp( iqmAnim->name, "aim_in" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_AIM_IN );
        } else if ( !strcmp( iqmAnim->name, "aim_fire" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_AIM_FIRE );
        } else if ( !strcmp( iqmAnim->name, "aim_out" ) ) {
            P_Weapon_ModeAnimationFromIQM( &pistolItemInfo, iqmAnim, WEAPON_MODE_AIM_OUT );
        } else {
            continue;
        }
    }
}



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
    // Get weapon state.
    gclient_t::weapon_state_s *weaponState = &ent->client->weaponState;
    // Default damage value.
    static constexpr int32_t damage = 10;
    // Additional recoil kick force strength.
    static constexpr float additionalKick = 2;
    // Get recoil amount.
    const float recoilAmount = weaponState->recoil.amount;
    // Default kickback force.
    const float kick = 8 + ( additionalKick * recoilAmount );

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    Vector3 forward = {}, right = { };
    QM_AngleVectors( ent->client->viewMove.viewAngles, &forward, &right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    // Project from source to shot destination.
    Vector3 shotOffset = { 0, 10, (float)ent->viewheight - 5.5f };
    Vector3 start = {};
    P_ProjectDistance( ent, ent->s.origin, &shotOffset.x, &forward.x, &right.x, &start.x );

    // Determine spread value determined by weaponState.
    // Make sure we're not in aiming mode.
    //if ( weaponState->aimState.isAiming == false ) {
    const float bulletHSpread = PRIMARY_FIRE_BULLET_MIN_HSPREAD + ( PRIMARY_FIRE_BULLET_RECOIL_MAX_HSPREAD * recoilAmount );
    const float bulletVSpread = PRIMARY_FIRE_BULLET_MIN_VSPREAD + ( PRIMARY_FIRE_BULLET_RECOIL_MAX_VSPREAD * recoilAmount );
    //}

    // Fire the actual bullet itself.
    fire_bullet( ent, &start.x, &forward.x, damage, kick, bulletHSpread, bulletVSpread, MEANS_OF_DEATH_HIT_PISTOL );

    // Project from source to muzzleflash destination.
    Vector3 muzzleFlashOffset = { 16.f, 10.f, (float)ent->viewheight };
    P_ProjectDistance( ent, ent->s.origin, &muzzleFlashOffset.x, &forward.x, &right.x, &start.x );

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
void weapon_pistol_aim_fire( edict_t *ent ) {
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
    fire_bullet( ent, &start.x, &forward.x, damage, kick, SECONDARY_FIRE_BULLET_HSPREAD + hSpreadMultiplier, SECONDARY_FIRE_BULLET_VSPREAD + vSpreadMultiplier, MEANS_OF_DEATH_HIT_PISTOL );

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
    // Reset recoil.
    ent->client->weaponState.recoil = {};

    if ( level.time >= ent->client->weaponState.timers.lastEmptyWeaponClick ) {
        gi.sound( ent, CHAN_VOICE, gi.soundindex( "weapons/pistol/noammo.wav" ), 1, ATTN_NORM, 0 );
        ent->client->weaponState.timers.lastEmptyWeaponClick = level.time + 75_ms;
    }
}

/**
*   @brief  Helper method for primary fire routine.
**/
static void Weapon_Pistol_AddRecoil( gclient_t::weapon_state_s *weaponState, const float amount, sg_time_t accumulatedTime ) {
    // Add accumulated time.
    weaponState->recoil.accumulatedTime += accumulatedTime;
    // Increment recoil heavily.
    weaponState->recoil.amount += amount;
    // Make sure we can't exceed 1.0 limit.
    if ( weaponState->recoil.amount >= 1.0f ) {
        weaponState->recoil.amount = 1.0f;
    }
}

/**
*   @brief  Processes responses to the user input.
**/
static void Weapon_Pistol_ProcessUserInput( edict_t *ent ) {
    // Acquire weapon state.
    gclient_t::weapon_state_s *weaponState = &ent->client->weaponState;

    /**
    *   isAiming Behavior Path:
    **/
    if ( weaponState->aimState.isAiming == true ) {
        /**
        *   Letting go of 'Secondary Fire': "Aim Out" of isAiming Mode:
        **/
        if ( !( ent->client->buttons & BUTTON_SECONDARY_FIRE )
                && ent->client->weaponState.mode != WEAPON_MODE_AIM_OUT ) {
            // Switch to aim out mode.
            P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_OUT, pistolItemInfo.modeAnimations, false );
            // Restore the original FOV.
            P_ResetPlayerStateFOV( ent->client );
            //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_OUT )\n", __func__ );
        /**
        *   Firing 'Primary Fire' of isAiming Mode:
        **/
        } else if ( ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) ) {
            // Switch to Firing mode if we have Clip Ammo:
            if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] ) {
                //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_FIRE )\n", __func__ );
                P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_FIRE, pistolItemInfo.modeAnimations, false );
            // Attempt to reload otherwise:
            } else {
                // We need to have enough ammo left to reload with.
                if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                    // When enabled, will move out of isAiming mode, reset FOV, and perform a reload, upon
                    // which if secondary fire is still pressed, it'll return to aiming in.
                    #if 0
                    //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_RELOADING )\n", __func__ );
                    // Exit isAiming mode.
                    weaponState->aimState.isAiming = false;
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
        if ( ( ent->client->buttons & BUTTON_SECONDARY_FIRE ) && ent->client->weaponState.mode != WEAPON_MODE_AIM_IN ) {
            if ( ent->client->weaponState.aimState.isAiming == false ) {
                //gi.dprintf( "%s: NOT isAiming -> SwitchMode( WEAPON_MODE_AIM_IN )\n", __func__ );
                P_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_IN, pistolItemInfo.modeAnimations, false );
                ent->client->ps.fov = 45;
            }

            return;

        /**
        *   @brief  Primary Fire Button Implementation:
        **/
        } else if ( ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) /*&& ( ent->client->buttons & BUTTON_ATTACK )*/ ) {
            // Switch to Firing mode if we have Clip Ammo:
            if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] ) {
                // Allow for rapidly firing, causing an increase in recoil.
                const sg_time_t &timeLastPrimaryFire = weaponState->timers.lastPrimaryFire;

                sg_time_t recoilTime = level.time - timeLastPrimaryFire;
                // When a shot is fired right after 75_ms(first 3 frames of the weapon.)
                sg_time_t timeRecoilStageA = 75_ms;
                // When a shot is fired right after 150_ms.(after first 3 frames, up to the 6th frame.)
                sg_time_t timeRecoilStageB = 150_ms;
                // When a shot is fired right after 150_ms.(after first 3 frames, up to the 6th frame.)
                sg_time_t timeRecoilStageC = 225_ms;
                // When a shot is fired right after 150_ms.(350_ms is the last frame of firing.)
                sg_time_t timeRecoilStageCap = 350_ms;

                // Fire for stage A:
                if ( recoilTime < timeRecoilStageA ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.35f, level.time - timeRecoilStageA );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: A [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.milliseconds(), level.time.milliseconds(), weaponState->recoil.amount );
                // Fire for stage B:
                } else if ( recoilTime < timeRecoilStageB ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.25f, level.time - timeRecoilStageA );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: B [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.milliseconds(), level.time.milliseconds(), weaponState->recoil.amount );                // Fire for stage C:
                } else if ( recoilTime < timeRecoilStageC ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.15f, level.time - timeRecoilStageA );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: C [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.milliseconds(), level.time.milliseconds(), weaponState->recoil.amount );
                } else if ( recoilTime < timeRecoilStageCap ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.15f, level.time - timeRecoilStageA );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: C [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.milliseconds(), level.time.milliseconds(), weaponState->recoil.amount );
                    // 
                // The player waited long enough to fire steady again:
                } else {
                    // Reset recoil.
                    weaponState->recoil = {};

                    //gi.dprintf( "%s: Pistol is steady firing! [lastPrimaryFire(%llu), level.time(%llu)]\n", __func__, timeLastPrimaryFire.milliseconds(), level.time.milliseconds(), weaponState->recoil.amount );
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, false );
                }
            // Attempt to reload otherwise:
            } else {
                // Reset recoil.
                ent->client->weaponState.recoil = {};

                // We need to have enough ammo left to reload with:
                if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                    P_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeAnimations, false );
                // Play out of ammo click sound:
                } else {
                    weapon_pistol_no_ammo( ent );
                }
            }
            return;
        }

        /**
        *   Reload Button Implementation:
        **/
        if ( ent->client->latched_buttons & BUTTON_RELOAD ) {
            // Reset recoil.
            ent->client->weaponState.recoil = {};

            // We need to have enough ammo left to reload with.
            if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                P_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeAnimations, false );
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
void Weapon_Pistol( edict_t *ent, const bool processUserInputOnly ) {
    // Process the animation frames of the mode we're in.
    const bool isDoneAnimating = P_Weapon_ProcessModeAnimation( ent, &pistolItemInfo.modeAnimations[ ent->client->weaponState.mode ] );

    // If IDLE or NOT ANIMATING, or PRIMARY_FIRING, process user input.
    if ( ent->client->weaponState.mode == WEAPON_MODE_IDLE
        || ent->client->weaponState.mode == WEAPON_MODE_PRIMARY_FIRING 
        || isDoneAnimating 
        || processUserInputOnly == true ) {
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
                    weapon_pistol_aim_fire( ent );
                    // Store the time we last 'primary fired'.
                    ent->client->weaponState.timers.lastAimedFire = level.time;
                }
            }
        // isAiming -> "Aim Out":
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_OUT ) {
            // Due to this being possibly called multiple times in the same frame, we depend on a timer for this to prevent
            // any earlier/double firing.
            if ( ent->client->weaponState.animation.endTime == level.time ) {
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
                //if ( ent->client->weaponState.timers.lastPrimaryFire <= ( level.time + 325_ms ) ) {
                if ( ent->client->weaponState.timers.lastPrimaryFire < level.time ) {
                    // Fire the pistol bullet.
                    weapon_pistol_primary_fire( ent );
                    // Store the time we last 'primary fired'.
                    ent->client->weaponState.timers.lastPrimaryFire = level.time;
                }
            } else {
                // Reset recoil after enough time has passed.
                //if ( ent->client->weaponState.timers.lastPrimaryFire < level.time 
                //    && level.time > ent->client->weaponState.animation.endTime ) {
                if ( ent->client->weaponState.animation.endTime <= level.time ) {
                    ent->client->weaponState.recoil.accumulatedTime = sg_time_t::from_ms( 0 );
                    ent->client->weaponState.recoil.amount = 0.f;
                }
            }
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_IN ) {
            // Set the isAiming state value for aiming specific behavior to true right at the end of its animation.
            if ( ent->client->weaponState.animation.endTime == level.time ) {
                //! Engage aiming mode.
                ent->client->weaponState.aimState.isAiming = true;
                //! Maintain end frame.
                ent->client->weaponState.animation.currentFrame = ent->client->weaponState.animation.endFrame;
            }
        // Reload Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_RELOADING ) {
            // Start playing clip reload sound. (Takes about the same duration as a pistol reload, 1 second.)
            if ( ent->client->weaponState.animation.startTime == level.time ) {//ent->client->weaponState.animation.startFrame ) {
                ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/reload.wav" );
            }
            // Stop audio and actually reload the clip stats at the end of the animation sequence.
            //if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.endFrame - 1 ) {
            if ( ent->client->weaponState.animation.endTime == level.time ) {
                ent->client->weaponState.activeSound = 0;
                weapon_pistol_reload_clip( ent );
            }
        // Draw Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_DRAWING ) {
            // Start playing drawing weapon sound at the very first frame.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1 ) {
                ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/draw.wav" );
                ent->client->weaponState.timers.lastDrawn = level.time;
            }
            // Enough time has passed, shutdown the sound.
            else if ( ent->client->weaponState.timers.lastDrawn <= level.time - 250_ms ) {
                ent->client->weaponState.activeSound = 0;
            }
        // Holster Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_HOLSTERING ) {
            // Start playing holster weapon sound at the very first frame.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1 ) {
                ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/holster.wav" );
                ent->client->weaponState.timers.lastHolster = level.time;
            }
            // Enough time has passed, shutdown the sound.
            else if ( ent->client->weaponState.timers.lastHolster <= level.time - 250_ms ) {
                ent->client->weaponState.activeSound = 0;
            }
        }
    }
}