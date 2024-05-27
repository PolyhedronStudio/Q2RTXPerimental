/********************************************************************
*
*
*	SVGame: Fists 'Weapon' Implementation.
*
*
********************************************************************/
#include "../g_local.h"



/**
*
*
*	Fists Item Configuration:
*
*
**/
/**
*   Fists 'Weapon' Mode Animation Frames.
**/
weapon_item_info_t fistsItemInfo = {
    .modeFrames/*[ WEAPON_MODE_MAX ]*/ = {

    // Mode Animation: IDLE
    /*modeAnimationFrames[ WEAPON_MODE_IDLE ] = */{
        .startFrame = 0,
        .endFrame = 1,
        .durationMsec = sg_time_t::from_hz( 1 )
    },
    // Mode Animation: DRAWING
    /*modeAnimationFrames[ WEAPON_MODE_DRAWING ] = */{
        .startFrame = 1,
        .endFrame = 26,
        .durationMsec = sg_time_t::from_hz( 26 - 1 )
    },
    // Mode Animation: HOLSTERING
    /*modeAnimationFrames[ WEAPON_MODE_HOLSTERING ] = */{
        .startFrame = 27,
        .endFrame = 52,
        .durationMsec = sg_time_t::from_hz( 52 - 27 )
    },
    // Mode Animation: PRIMARY_FIRING
    /*modeAnimationFrames[ WEAPON_MODE_PRIMARY_FIRING ] =*/ {
        .startFrame = 53,
        .endFrame = 73,
        .durationMsec = sg_time_t::from_hz( 53 - 73 )
    },
    // Mode Animation: SECONDARY_FIRING -- UNUSED FOR PISTOL LOGIC.
    /*modeAnimationFrames[ WEAPON_MODE_PRIMARY_FIRING ] =*/ {
        .startFrame = 74,
        .endFrame = 92,
        .durationMsec = sg_time_t::from_hz( 92 - 74 )
    },
    // Mode Animation: RELOADING
    /*modeAnimationFrames[ WEAPON_MODE_RELOADING ] = */{
        .startFrame = 0,
        .endFrame = 1,
        .durationMsec = sg_time_t::from_hz( 1 )
    },
    // Mode Animation: AIMING IN
    /*modeAnimationFrames[ WEAPON_MODE_AIM_IN ] = */
    {
        .startFrame = 0,
        .endFrame = 1,
        .durationMsec = sg_time_t::from_hz( 1 )
    },
    // Mode Animation: AIMING FIRE
    /*modeAnimationFrames[ WEAPON_MODE_AIM_FIRE ] = */
    {
        .startFrame = 0,
        .endFrame = 1,
        .durationMsec = sg_time_t::from_hz( 1 )
    },
    // Mode Animation: AIMING OUT
    /*modeAnimationFrames[ WEAPON_MODE_AIM_OUT ] = */
    {
        .startFrame = 0,
        .endFrame = 1,
        .durationMsec = sg_time_t::from_hz( 1 )
    }
},

// TODO: Move quantity etc from gitem_t into this struct.
};

//static constexpr float PRIMARY_FIRE_BULLET_HSPREAD = 300.f;
//static constexpr float PRIMARY_FIRE_BULLET_VSPREAD = 300.f;
//
//static constexpr float SECONDARY_FIRE_BULLET_HSPREAD = 50.f;
//static constexpr float SECONDARY_FIRE_BULLET_VSPREAD = 50.f;

/**
*
*
*   Pistol Function Implementations:
*
*
**/
/**
*   @brief
**/
const bool fire_hit_punch_impact( edict_t *self, const vec3_t start, const vec3_t aimDir, const int32_t damage, const int32_t kick );
void weapon_fists_primary_fire( edict_t *ent ) {
    vec3_t      start;
    vec3_t      forward, right;
    int         damage = 1;
    int         kick = 8;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    AngleVectors( ent->client->v_angle, forward, right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    ent->client->weaponKicks.offsetAngles[ 1 ] = -2; // Left punch angle.

    // Project from source to shot destination.
    vec3_t fistOffset = { 0, -10, (float)ent->viewheight - 5.5f }; // VectorSet( offset, 0, 8, ent->viewheight - 8 );
    P_ProjectDistance( ent, ent->s.origin, fistOffset, forward, right, start ); //P_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

    // Fire the actual bullet itself.
    //fire_bullet( ent, start, forward, damage, kick, PRIMARY_FIRE_BULLET_HSPREAD, PRIMARY_FIRE_BULLET_VSPREAD, MOD_CHAINGUN );
    if ( fire_hit_punch_impact( ent, start, forward, 5, 55 ) ) {
        // Send a muzzle flash event.
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_FIST_LEFT /*| is_silenced*/ );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    // Notify we're making noise.
    P_PlayerNoise( ent, start, PNOISE_WEAPON );
}
/**
*   @brief  
**/
void weapon_fists_secondary_fire( edict_t *ent ) {
    vec3_t      start;
    vec3_t      forward, right;
    int         damage = 1;
    int         kick = 8;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    AngleVectors( ent->client->v_angle, forward, right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = -2;
    ent->client->weaponKicks.offsetAngles[ 1 ] = 2; // Right punch angle.

    // Project from source to shot destination.
    vec3_t fistOffset = { 0, 10, (float)ent->viewheight - 5.5f }; // VectorSet( offset, 0, 8, ent->viewheight - 8 );
    P_ProjectDistance( ent, ent->s.origin, fistOffset, forward, right, start ); //P_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

    // Fire the actual bullet itself.
    //fire_bullet( ent, start, forward, damage, kick, PRIMARY_FIRE_BULLET_HSPREAD, PRIMARY_FIRE_BULLET_VSPREAD, MOD_CHAINGUN );
    if ( fire_hit_punch_impact( ent, start, forward, 8, 85 ) ) {
        // Send a muzzle flash event.
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_FIST_RIGHT /*| is_silenced*/ );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    // Notify we're making noise.
    P_PlayerNoise( ent, start, PNOISE_WEAPON );
}

/**
*   @brief  Processes responses to the user input.
**/
static void Weapon_Fists_ProcessUserInput( edict_t *ent ) {
    if ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) {
        P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, fistsItemInfo.modeFrames, false );
        return;
    } else if ( ent->client->latched_buttons & BUTTON_SECONDARY_FIRE ) {
        P_Weapon_SwitchMode( ent, WEAPON_MODE_SECONDARY_FIRING, fistsItemInfo.modeFrames, false );
        return;
    }
}

/**
*   @brief  Pistol Weapon 'State Machine'.
**/
void Weapon_Fists( edict_t *ent ) {
    // Process the animation frames of the mode we're in.
    const bool isDoneAnimating = P_Weapon_ProcessModeAnimation( ent, &fistsItemInfo.modeFrames[ ent->client->weaponState.mode ] );

    // If IDLE or NOT ANIMATING, process user input.
    if ( ent->client->weaponState.mode == WEAPON_MODE_IDLE || isDoneAnimating ) {
        // Respond to user input, which determines whether 
        Weapon_Fists_ProcessUserInput( ent );
    }

    // Primary Fire, throws a left punch fist.
    if ( ent->client->weaponState.mode == WEAPON_MODE_PRIMARY_FIRING ) {
        // Play a sway sound at the start of the animation.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
            std::string swayFile = "weapons/fists/sway0";
            swayFile += std::to_string( irandom( 1, 5 ) );
            swayFile += ".wav";
            ent->client->weapon_sound = gi.soundindex( swayFile.c_str() );
            // Store the time we last 'primary fired'.
            ent->client->weaponState.timers.lastPrimaryFire = level.time;
        }

        // Since this logic is ran each frame, use a timer to wait out and stop the sway sound after 200ms.
        if ( ent->client->weaponState.timers.lastPrimaryFire <= ( level.time - 200_ms ) ) {
            ent->client->weapon_sound = 0;
        }

        // Fire the hit trace when the model is at its sweet spot for punching.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 8 ) {
            // Swing the left fist.
            weapon_fists_primary_fire( ent );
        }
    // Primary Fire, throws a left punch fist.
    } else if ( ent->client->weaponState.mode == WEAPON_MODE_SECONDARY_FIRING ) {
        // Play a sway sound at the start of the animation.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
            std::string swayFile = "weapons/fists/sway0";
            swayFile += std::to_string( irandom( 1, 5 ) );
            swayFile += ".wav";
            ent->client->weapon_sound = gi.soundindex( swayFile.c_str() );
            // Store the time we last 'primary fired'.
            ent->client->weaponState.timers.lastSecondaryFire = level.time;
        }

        // Since this logic is ran each frame, use a timer to wait out and stop the sway sound after 200ms.
        if ( ent->client->weaponState.timers.lastSecondaryFire <= ( level.time - 200_ms ) ) {
            ent->client->weapon_sound = 0;
        }

        // Fire the hit trace when the model is at its sweet spot for punching.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 14 ) {
            // Swing the left fist.
            weapon_fists_secondary_fire( ent );

        }
    // Draw Weapon:
    } else if ( ent->client->weaponState.mode == WEAPON_MODE_DRAWING ) {
        // Start playing drawing weapon sound at the very first frame.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1) {
            ent->client->weapon_sound = gi.soundindex( "weapons/pistol/draw.wav" );
            ent->client->weaponState.timers.lastDrawn = level.time;
        }
        // Enough time has passed, shutdown the sound.
        if ( ent->client->weaponState.timers.lastDrawn <= level.time - 250_ms ) {
            ent->client->weapon_sound = 0;

            // Store the client's fieldOfView.
            ent->client->weaponState.clientFieldOfView = ent->client->ps.fov;
        }
    // Holster Weapon:
    } else if ( ent->client->weaponState.mode == WEAPON_MODE_HOLSTERING ) {
        // Start playing holster weapon sound at the very first frame.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1) {
            ent->client->weapon_sound = gi.soundindex( "weapons/pistol/holster.wav" );
            ent->client->weaponState.timers.lastHolster = level.time;
        }
        // Enough time has passed, shutdown the sound.
        if ( ent->client->weaponState.timers.lastHolster <= level.time - 250_ms ) {
            ent->client->weapon_sound = 0;
        }
    }
}