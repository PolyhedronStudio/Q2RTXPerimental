/********************************************************************
*
*
*	SVGame: Pistol Weapon Implementation.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/player/svg_player_client.h"

//! Enable to have the pistol auto reload when an empty clip occures while engaged in target aiming fire mode.
//#define WEAPON_PISTOL_ENABLE_RELOAD_ON_AIMFIRE_EMPTY_CLIP


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
    // These are dynamically loaded from the SKM(IQM) model data.
    // COULD be set by hand... if you want to. lol.
    .modeAnimations/*[ WEAPON_MODE_MAX ]*/ = {
        { .skmAnimationName = "idle" },     // WEAPON_MODE_IDLE
        { .skmAnimationName = "draw" },     // WEAPON_MODE_DRAWING
        { .skmAnimationName = "holster" },  // WEAPON_MODE_HOLSTERING
        { .skmAnimationName = "fire" },     // WEAPON_MODE_PRIMARY_FIRING
        { .skmAnimationName = "" },         // [UNUSED] WEAPON_MODE_SECONDARY_FIRING
        { .skmAnimationName = "reload" },   // WEAPON_MODE_RELOADING
        { .skmAnimationName = "aim_in" },   // WEAPON_MODE_AIM_IN
        { .skmAnimationName = "aim_fire" }, // WEAPON_MODE_AIM_FIRE
        { .skmAnimationName = "aim_idle" }, // WEAPON_MODE_AIM_IDLE
        { .skmAnimationName = "aim_out" },  // WEAPON_MODE_AIM_OUT
        //{ nullptr },  // WEAPON_MODE_MAX
    },

    // TODO: Move quantity etc from gitem_t into this struct.
};

/**
*   @brief  Precache animation information.
**/
void Weapon_Pistol_Precached( const gitem_t *item ) {
    // Precache the view model animation data for each weapon mode described in our pistolItemInfo struct.
    SVG_Player_Weapon_PrecacheItemInfo( &pistolItemInfo, item->classname, item->view_model );
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
    static constexpr int32_t shotDamage = 10;
    // Additional recoil kick force strength.
    static constexpr float additionalKick = 2;
    // Get recoil amount.
    const float recoilAmount = weaponState->recoil.amount;
    // Default kickback force.
    const float shotKick = 8 + ( additionalKick * recoilAmount );

    // Acquire these for usage.
    Vector3 forward = ent->client->viewMove.viewForward, right = ent->client->viewMove.viewRight;
    // Determine shot kick offset.
    ent->client->weaponKicks.offsetOrigin = QM_Vector3Scale( forward, -2 );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    // Project from source to shot destination.
    Vector3 shotOffset = { 0.f, 10.f, (float)ent->viewheight - 5.5f };
    Vector3 shotStart = {};
    shotStart = SVG_Player_ProjectDistance( ent, ent->s.origin, shotOffset, forward, right );

    // Determine spread value determined by weaponState.
    // Make sure we're not in aiming mode.
    //if ( weaponState->aimState.isAiming == false ) {
    const float bulletHSpread = PRIMARY_FIRE_BULLET_MIN_HSPREAD + ( PRIMARY_FIRE_BULLET_RECOIL_MAX_HSPREAD * recoilAmount );
    const float bulletVSpread = PRIMARY_FIRE_BULLET_MIN_VSPREAD + ( PRIMARY_FIRE_BULLET_RECOIL_MAX_VSPREAD * recoilAmount );
    //}

    // Fire the actual bullet itself.
    fire_bullet( ent, 
        &shotStart.x, &forward.x, shotDamage, shotKick,
        bulletHSpread, bulletVSpread, 
        MEANS_OF_DEATH_HIT_PISTOL
    );

    // Project muzzleflash destination from source to source+offset, and clip it to any obstacles.
    Vector3 muzzleFlashOffset = { 16.f, 10.f, (float)ent->viewheight };
    //SVG_Player_ProjectDistance( ent, ent->s.origin, &muzzleFlashOffset.x, &forward.x, &right.x, &start.x );
    Vector3 muzzleFlashOrigin = SVG_MuzzleFlash_ProjectAndTraceToPoint( ent, muzzleFlashOffset, forward, right );

    // Send a muzzle flash event.
    gi.WriteUint8( svc_muzzleflash );
    gi.WriteInt16( ent - g_edicts );
    gi.WriteUint8( MZ_PISTOL /*| is_silenced*/ );
    gi.multicast( &muzzleFlashOrigin.x/*ent->s.origin*/, MULTICAST_PVS, false );

    // Notify we're making noise.
    SVG_Player_PlayerNoise( ent, &muzzleFlashOrigin.x, PNOISE_WEAPON );

    // Decrease clip ammo.
    //if ( !( (int)dmflags->value & DF_INFINITE_AMMO ) )
    ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ]--;

}
/**
*   @brief  Supplements the Secondary Firing routine by actually performing a more precise 'single bullet' shot.
**/
void weapon_pistol_aim_fire( edict_t *ent ) {
    constexpr int32_t shotDamage = 14;
    constexpr int32_t recoilKick = 10;

    // Acquire these for usage.
    Vector3 forward = ent->client->viewMove.viewForward, right = ent->client->viewMove.viewRight;
    // Determine shot kick offset.
    ent->client->weaponKicks.offsetOrigin = QM_Vector3Scale( forward, -2 );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    // Project from source to shot destination.
    Vector3 shotOffset = { 0.f, 0.f, (float)ent->viewheight };
    Vector3 shotStart = {};
    shotStart = SVG_Player_ProjectDistance( ent, ent->s.origin, shotOffset, forward, right );

    // Determine the amount to multiply bullet spread with based on the player's velocity.
    // TODO: Use stats array or so for storing the actual move speed limit, since this
    // may vary.
    constexpr float moveThreshold = 300.0f;
    // Don't spread multiply if we're pretty much standing still. This allows for a precise shot.
    const float hSpreadMultiplier = ( ent->client->ps.xyzSpeed > 5 ? moveThreshold * ent->client->ps.bobMove : 0 );
    const float vSpreadMultiplier = ( ent->client->ps.xyzSpeed > 5 ? moveThreshold * ent->client->ps.bobMove : 0 );
    //gi.dprintf( " ----- ----- ----- \n" );
    //gi.dprintf( "%s: hSpreadMultiplier(%f) vSpreadMultiplier(%f)\n", __func__, hSpreadMultiplier, vSpreadMultiplier );

    // Fire the actual bullet itself.
    fire_bullet( ent, 
        &shotStart.x, &forward.x, shotDamage, recoilKick, 
        SECONDARY_FIRE_BULLET_HSPREAD + hSpreadMultiplier, 
        SECONDARY_FIRE_BULLET_VSPREAD + vSpreadMultiplier, 
        MEANS_OF_DEATH_HIT_PISTOL 
    );

    // Project muzzleflash destination from source to source+offset, and clip it to any obstacles.
    vec3_t muzzleFlashOffset = { 16, 0, (float)ent->viewheight };
    //SVG_Player_ProjectDistance( ent, ent->s.origin, &muzzleFlashOffset.x, &forward.x, &right.x, &start.x );
    Vector3 muzzleFlashOrigin = SVG_MuzzleFlash_ProjectAndTraceToPoint( ent, muzzleFlashOffset, forward, right );

    // Send a muzzle flash event.
    gi.WriteUint8( svc_muzzleflash );
    gi.WriteInt16( ent - g_edicts );
    gi.WriteUint8( MZ_PISTOL /*| is_silenced*/ );
    gi.multicast( &muzzleFlashOrigin.x, MULTICAST_PVS, false );

    // Notify we're making noise.
    SVG_Player_PlayerNoise( ent, &muzzleFlashOrigin.x, PNOISE_WEAPON );

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
static void Weapon_Pistol_AddRecoil( gclient_t::weapon_state_s *weaponState, const float amount, QMTime accumulatedTime ) {
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
    // Acquire user button input states.
    gclient_t::userinput_s &userInput = ent->client->userInput;

    /**
    *   AIMING Path: We're AIMING, if the current MODE(and animation) >= WEAPON_MODE_AIM_IN
    **/
    if ( ent->client->weaponState.mode >= WEAPON_MODE_AIM_IN ) {
        // If we're still aiming, but IDLE, respond to key presses again:
        if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_IDLE ) {
            // Engage into aiming out mode in case the SECONDARY_FIRE button isn't HELD anymore.
            if ( !( ent->client->userInput.heldButtons & BUTTON_SECONDARY_FIRE ) ) {
                // Switch to aim out mode.
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_OUT, pistolItemInfo.modeAnimations, false );
                // Revert POV.
                SVG_Player_ResetPlayerStateFOV( ent->client );
                // Exit.
                return;
            }

            // Attempt to fire:
            if ( ( ent->client->userInput.pressedButtons & BUTTON_PRIMARY_FIRE ) ) {
                // Switch to Firing mode if we have Clip Ammo:
                if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] > 0 ) {
                    //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_FIRE )\n", __func__ );
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_FIRE, pistolItemInfo.modeAnimations, false );
                    // No ammo audio.
                } else {
                    // When enabled, will move out of isAiming mode, reset FOV, and perform a reload, upon
                    // which if secondary fire is still pressed, it'll return to aiming in.
                    #ifdef WEAPON_PISTOL_ENABLE_RELOAD_ON_AIMFIRE_EMPTY_CLIP
                    // We need to have enough ammo left to reload with.
                    if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                        //gi.dprintf( "%s: isAiming -> SwitchMode( WEAPON_MODE_AIM_RELOADING )\n", __func__ );
                        // Exit isAiming mode.
                        weaponState->aimState.isAiming = false;
                        // Restore the original FOV.
                        SVG_Player_ResetPlayerStateFOV( ent->client );
                        // Screen should be lerping back to FOV, engage into reload mode.
                        SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeFrames, false );
                    } else {
                        // Play out of ammo click sound.
                        weapon_pistol_no_ammo( ent );
                    }
                    #else
                    // Play out of ammo click sound.
                    weapon_pistol_no_ammo( ent );
                    #endif
                }
            }
        }
    /**
    *   REGULAR Path: We're NOT aiming, the current MODE(and animation) <= WEAPON_MODE_RELOADING
    **/
    } else {
        /**
        *   Secondary Fire Button Implementation: "Aim In" to engage for 'isAiming' Mode:
        **/
        if ( ( ( ent->client->userInput.pressedButtons & BUTTON_SECONDARY_FIRE ) ||
            ( ent->client->userInput.heldButtons & BUTTON_SECONDARY_FIRE ) ) //( ent->client->buttons & BUTTON_SECONDARY_FIRE ) 
            && ent->client->weaponState.mode < WEAPON_MODE_RELOADING ) {
                //gi.dprintf( "%s: NOT isAiming -> SwitchMode( WEAPON_MODE_AIM_IN )\n", __func__ );
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_IN, pistolItemInfo.modeAnimations, false );
        /**
        *   @brief  Primary Fire Button Implementation:
        **/
        } else if ( ( ent->client->userInput.pressedButtons & BUTTON_PRIMARY_FIRE ) /*&& ( ent->client->buttons & BUTTON_ATTACK )*/ ) {
            // Switch to Firing mode if we have Clip Ammo:
            if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] > 0 ) {
                // Allow for rapidly firing, causing an increase in recoil.
                const QMTime &timeLastPrimaryFire = weaponState->timers.lastPrimaryFire;
                // Actual recoil time so far.
                QMTime recoilTime = timeLastPrimaryFire - level.time;

                // When a shot is fired right after 75_ms(first 3 frames of the weapon.)
                constexpr QMTime timeRecoilStageA = 75_ms;
                // When a shot is fired right after 150_ms.(after first 3 frames, up to the 6th frame.)
                constexpr QMTime timeRecoilStageB = 150_ms;
                // When a shot is fired right after 300_ms.(after first 6 frames, up to the 9th frame.)
                constexpr QMTime timeRecoilStageC = 250_ms;
                // When a shot is fired right after 350_ms.(350_ms is the last frame of firing.)
                constexpr QMTime timeRecoilStageCap = 300_ms;

                // Fire for stage A:
                if ( recoilTime < timeRecoilStageA ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.15f, timeRecoilStageA );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: A [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.Milliseconds(), level.time.Milliseconds(), weaponState->recoil.amount );
                // Fire for stage B:
                } else if ( recoilTime < timeRecoilStageB ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.25f, timeRecoilStageB );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: B [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.Milliseconds(), level.time.Milliseconds(), weaponState->recoil.amount );                // Fire for stage C:
                } else if ( recoilTime < timeRecoilStageC ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.30f, level.time - timeRecoilStageC );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: C [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.Milliseconds(), level.time.Milliseconds(), weaponState->recoil.amount );
                } else if ( recoilTime < timeRecoilStageCap ) {
                    // Add recoil.
                    Weapon_Pistol_AddRecoil( weaponState, 0.45f, level.time - timeRecoilStageCap );
                    // Engage FORCEFULLY in another Primary Firing mode.
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, true );
                    // Output total
                    //gi.dprintf( "%s: Pistol is rapidly firing(Recoil Stage: C [lastPrimaryFire(%llu), level.time(%llu), recoil(%f)]\n", __func__, timeLastPrimaryFire.Milliseconds(), level.time.Milliseconds(), weaponState->recoil.amount );
                    // 
                // The player waited long enough to fire steady again:
                } else {
                    // Reset recoil.
                    weaponState->recoil = {};

                    //gi.dprintf( "%s: Pistol is steady firing! [lastPrimaryFire(%llu), level.time(%llu)]\n", __func__, timeLastPrimaryFire.Milliseconds(), level.time.Milliseconds(), weaponState->recoil.amount );
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolItemInfo.modeAnimations, false );
                }
            // Attempt to reload otherwise:
            } else {
                // Reset recoil.
                ent->client->weaponState.recoil = {};

                // We need to have enough ammo left to reload with:
                if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeAnimations, false );
                // Play out of ammo click sound:
                } else {
                    weapon_pistol_no_ammo( ent );
                }
            }
            return;
        /**
        *   Reload Button Implementation:
        **/
        } else if ( ent->client->userInput.pressedButtons & BUTTON_RELOAD ) {//ent->client->latched_buttons & BUTTON_RELOAD ) {
            // Reset recoil.
            ent->client->weaponState.recoil = {};

            // We need to have enough ammo left to reload with.
            if ( ent->client->pers.inventory[ ent->client->ammo_index ] > 0 ) {
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_RELOADING, pistolItemInfo.modeAnimations, false );
            } else {
                // Play out of ammo click sound.
                weapon_pistol_no_ammo( ent );
            }
            return;
        }
    }
}
/**
*   @brief  Pistol Weapon Think function acting as a 'State Machine'.
*           If a weapon mode requests user input, the mode is done animating, or
*           the mode in specific allows user input, proceed to processing that first.
* 
*           The mode acting itself is dealt with by this function.
**/
void Weapon_Pistol( edict_t *ent, const bool processUserInputOnly ) {
    // Process the animation frames of the mode we're in.
    const bool isDoneAnimating = SVG_Player_Weapon_ProcessModeAnimation( ent, &pistolItemInfo.modeAnimations[ ent->client->weaponState.mode ] );

    // Process User Input ONLY If:
    // Weapon mode is IDLE:
    if ( ent->client->weaponState.mode == WEAPON_MODE_IDLE
        // For firing when precision aiming:
        || ent->client->weaponState.mode == WEAPON_MODE_AIM_IDLE
        // For rapidly performing primary firing:
        || ent->client->weaponState.mode == WEAPON_MODE_PRIMARY_FIRING
        // Done Animating:
        || isDoneAnimating
        // Function was called to deal with userinput specifically for this frame.
        || processUserInputOnly == true ) {
            // Respond to user input, which determines whether 
            Weapon_Pistol_ProcessUserInput( ent );
    }

    /**
    *   AIMING Path: We're AIMING, if the current MODE(and animation) >= WEAPON_MODE_AIM_IN
    **/
    if ( ent->client->weaponState.mode >= WEAPON_MODE_AIM_IN ) {
        if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_IN ) {
            // Adjust the POV.
            if ( ent->client->weaponState.animation.currentFrame >= ent->client->weaponState.animation.startFrame ) {
                // Zoomed POV.
                ent->client->ps.fov = 45;
                // Disable any possible active sounds.
                //ent->client->weaponState.activeSound = 0;
            }

            // Set the isAiming state value for aiming specific behavior to true right at the end of its animation.
            if ( level.time == ent->client->weaponState.animation.endTime ) {
                // Switch to AIM Idle.
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_IDLE, pistolItemInfo.modeAnimations, false );
            }
        // isAiming -> Idle:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_IDLE ) {
        
        // isAiming -> Fire:
        } if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_FIRE ) {
            // Due to this being possibly called multiple times in the same frame, we depend on a timer for this to prevent
            // any earlier/double firing.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame /*+ 3 This is realistic, visually, but, plays very annoying and unnaturally.. */ ) {
                // Wait out until enough time has passed.
                if ( ent->client->weaponState.timers.lastAimedFire <= ( level.time - 325_ms ) ) {
                    // Fire the pistol bullet.
                    weapon_pistol_aim_fire( ent );
                    // Store the time we last 'aim fired'.
                    ent->client->weaponState.timers.lastAimedFire = level.time;
                }
            } else if ( ent->client->weaponState.animation.endTime == level.time ) {
                // Back to idle.
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_AIM_IDLE, pistolItemInfo.modeAnimations, false );
            }
        // isAiming -> "Aim Out":
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_AIM_OUT ) {
            // Restore the original FOV.
            SVG_Player_ResetPlayerStateFOV( ent->client );

            // Due to this being possibly called multiple times in the same frame, we depend on a timer for this to prevent
            // any earlier/double firing.
            if ( level.time == ent->client->weaponState.animation.endTime ) {
                // Disengage aiming state.
                ent->client->weaponState.aimState = {};
                // Back to idle.
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_IDLE, pistolItemInfo.modeAnimations, false );
            }
        }
    /**
    *   REGULAR Path: We're NOT aiming, the current MODE(and animation) <= WEAPON_MODE_RELOADING
    **/
    } else {
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
                    ent->client->weaponState.recoil.accumulatedTime = QMTime::FromMilliseconds( 0 );
                    ent->client->weaponState.recoil.amount = 0.f;
                    // Switch to AIM Idle.
                    SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_IDLE, pistolItemInfo.modeAnimations, false );
                }
            }
        // Reload Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_RELOADING ) {
            // Start playing clip reload sound. (Takes about the same duration as a pistol reload, 1 second.)
            if ( ent->client->weaponState.animation.startTime == level.time ) {//ent->client->weaponState.animation.startFrame ) {
                ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/reload.wav" );
            }
            // Stop audio and actually reload the clip stats at the end of the animation sequence.
            else if ( level.time == ent->client->weaponState.animation.endTime ) {
                // Stop sound.
                ent->client->weaponState.activeSound = 0;
                // Switch to idle.
                weapon_pistol_reload_clip( ent );
                // Switch to idle.
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_IDLE, pistolItemInfo.modeAnimations, false );
            }
        // Draw Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_DRAWING ) {
            // Start playing drawing weapon sound at the very first frame.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
                ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/draw.wav" );
                ent->client->weaponState.timers.lastDrawn = level.time;
            }
            // Enough time has passed, shutdown the sound, switch to idle state.
            else if ( ent->client->weaponState.timers.lastDrawn <= ( level.time - 250_ms ) ) {
                // Stop sound.
                ent->client->weaponState.activeSound = 0;
                // Switch to idle.
                SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_IDLE, pistolItemInfo.modeAnimations, false );
            }
        // Holster Weapon:
        } else if ( ent->client->weaponState.mode == WEAPON_MODE_HOLSTERING ) {
            // Start playing holster weapon sound at the very first frame.
            if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
                ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/holster.wav" );
                ent->client->weaponState.timers.lastHolster = level.time;
            }
            // Enough time has passed, shutdown the sound.
            else if ( ent->client->weaponState.timers.lastHolster <= ( level.time - 250_ms ) ) {
                ent->client->weaponState.activeSound = 0;
            }
        }
    }
}