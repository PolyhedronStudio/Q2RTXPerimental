/********************************************************************
*
*
*	SVGame: Fists 'Weapon' Implementation.
*
*
********************************************************************/
#include "svgame/svg_local.h"



/**
*
*
*	Fists Item Configuration:
*
*
**/
/**
*   Fists 'Weapon' Item Info.
**/
weapon_item_info_t fistsItemInfo = {
    // These are dynamically loaded from the IQM model data.
    .modeAnimations/*[ WEAPON_MODE_MAX ]*/ = { },

    // TODO: Move quantity etc from gitem_t into this struct.
};

/**
*   @brief  Precache animation information.
**/
void Weapon_Fists_Precached( const gitem_t *item ) {
    // Get the model we want the animation data from.
    const model_t *viewModel = gi.GetModelDataForName( item->view_model );
    if ( !viewModel ) {
        gi.dprintf( "%s: No viewmodel found. Failed to precache '%s' weapon animation data for item->view_model(%s)\n", __func__, item->classname, item->view_model );
        return;
    }
    // Validate the IQM data.
    if ( !viewModel->skmData ) {
        gi.dprintf( "%s: No SKM Data found. Failed to precache '%s' weapon animation data for item->view_model(%s)\n", __func__, item->classname, item->view_model );
        return;
    }

    // We can safely operate now.
    const skm_model_t *skmData = viewModel->skmData;
    // Iterate animations.
    for ( int32_t animID = 0; animID < skmData->num_animations; animID++ ) {
        // IQM animation.
        const skm_anim_t *skmAnim = &skmData->animations[ animID ];

        // idle.
        if ( !strcmp( skmAnim->name, "idle" ) ) {
            SVG_Player_Weapon_ModeAnimationFromSKM( &fistsItemInfo, skmAnim, WEAPON_MODE_IDLE, animID );
        } else if ( !strcmp( skmAnim->name, "draw" ) ) {
            SVG_Player_Weapon_ModeAnimationFromSKM( &fistsItemInfo, skmAnim, WEAPON_MODE_DRAWING, animID );
        } else if ( !strcmp( skmAnim->name, "holster" ) ) {
            SVG_Player_Weapon_ModeAnimationFromSKM( &fistsItemInfo, skmAnim, WEAPON_MODE_HOLSTERING, animID );
        } else if ( !strcmp( skmAnim->name, "primary_fire" ) ) {
            SVG_Player_Weapon_ModeAnimationFromSKM( &fistsItemInfo, skmAnim, WEAPON_MODE_PRIMARY_FIRING, animID );
        } else if ( !strcmp( skmAnim->name, "secondary_fire" ) ) {
            SVG_Player_Weapon_ModeAnimationFromSKM( &fistsItemInfo, skmAnim, WEAPON_MODE_SECONDARY_FIRING, animID );
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
*   @brief
**/
const bool fire_hit_punch_impact( edict_t *self, const Vector3 &start, const Vector3 &aimDir, const int32_t damage, const int32_t kick );
void weapon_fists_primary_fire( edict_t *ent ) {
    vec3_t      start;
    Vector3     forward, right;
    int         damage = 1;
    int         kick = 8;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    QM_AngleVectors( ent->client->viewMove.viewAngles, &forward, &right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = 2;
    ent->client->weaponKicks.offsetAngles[ 1 ] = -2; // Left punch angle.

    // Project from source to shot destination.
    vec3_t fistOffset = { 0, -10, (float)ent->viewheight - 5.5f }; // VectorSet( offset, 0, 8, ent->viewheight - 8 );
    SVG_Player_ProjectDistance( ent, ent->s.origin, fistOffset, &forward.x, &right.x, start ); //SVG_Player_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

    // Fire the actual bullet itself.
    //fire_bullet( ent, start, forward, damage, kick, PRIMARY_FIRE_BULLET_HSPREAD, PRIMARY_FIRE_BULLET_VSPREAD, MOD_CHAINGUN );
    if ( fire_hit_punch_impact( ent, start, &forward.x, 5, 55 ) ) {
        // Send a muzzle flash event.
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_FIST_LEFT /*| is_silenced*/ );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    // Notify we're making noise.
    SVG_Player_PlayerNoise( ent, start, PNOISE_WEAPON );
}
/**
*   @brief  
**/
void weapon_fists_secondary_fire( edict_t *ent ) {
    Vector3     start;
    Vector3     forward, right;
    int         damage = 1;
    int         kick = 8;

    // TODO: These are already calculated, right?
    // Calculate angle vectors.
    QM_AngleVectors( ent->client->viewMove.viewAngles, &forward, &right, NULL );
    // Determine shot kick offset.
    VectorScale( forward, -2, ent->client->weaponKicks.offsetOrigin );
    ent->client->weaponKicks.offsetAngles[ 0 ] = 2;
    ent->client->weaponKicks.offsetAngles[ 1 ] = 2; // Right punch angle.

    // Project from source to shot destination.
    vec3_t fistOffset = { 0, 10, (float)ent->viewheight - 5.5f }; // VectorSet( offset, 0, 8, ent->viewheight - 8 );
    SVG_Player_ProjectDistance( ent, ent->s.origin, fistOffset, &forward.x, &right.x, &start.x); //SVG_Player_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

    // Fire the actual bullet itself.
    //fire_bullet( ent, start, forward, damage, kick, PRIMARY_FIRE_BULLET_HSPREAD, PRIMARY_FIRE_BULLET_VSPREAD, MOD_CHAINGUN );
    if ( fire_hit_punch_impact( ent, &start.x, &forward.x, 8, 85 ) ) {
        // Send a muzzle flash event.
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_FIST_RIGHT /*| is_silenced*/ );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    // Notify we're making noise.
    SVG_Player_PlayerNoise( ent, &start.x, PNOISE_WEAPON );
}

/**
*   @brief  Processes responses to the user input.
**/
static void Weapon_Fists_ProcessUserInput( edict_t *ent ) {
    if ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) {
        SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, fistsItemInfo.modeAnimations, false );
        return;
    } else if ( ent->client->latched_buttons & BUTTON_SECONDARY_FIRE ) {
        SVG_Player_Weapon_SwitchMode( ent, WEAPON_MODE_SECONDARY_FIRING, fistsItemInfo.modeAnimations, false );
        return;
    }
}

/**
*   @brief  Pistol Weapon 'State Machine'.
**/
void Weapon_Fists( edict_t *ent, const bool processUserInputOnly ) {
    // Process the animation frames of the mode we're in.
    const bool isDoneAnimating = SVG_Player_Weapon_ProcessModeAnimation( ent, &fistsItemInfo.modeAnimations[ ent->client->weaponState.mode ] );

    // If IDLE or NOT ANIMATING, process user input.
    if ( ent->client->weaponState.mode == WEAPON_MODE_IDLE 
        || isDoneAnimating 
        || processUserInputOnly == true ) {
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
            ent->client->weaponState.activeSound = gi.soundindex( swayFile.c_str() );
            // Store the time we last 'primary fired'.
            ent->client->weaponState.timers.lastPrimaryFire = level.time;
        }

        // Since this logic is ran each frame, use a timer to wait out and stop the sway sound after 200ms.
        if ( ent->client->weaponState.timers.lastPrimaryFire <= ( level.time - 200_ms ) ) {
            ent->client->weaponState.activeSound = 0;
        }

        // Fire the hit trace when the model is at its sweet spot for punching.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 8 ) {
            // Swing the left fist.
            weapon_fists_primary_fire( ent );
        }
    // Secondary Fire, throws a right punch fist.
    } else if ( ent->client->weaponState.mode == WEAPON_MODE_SECONDARY_FIRING ) {
        // Play a sway sound at the start of the animation.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
            std::string swayFile = "weapons/fists/sway0";
            swayFile += std::to_string( irandom( 1, 5 ) );
            swayFile += ".wav";
            ent->client->weaponState.activeSound = gi.soundindex( swayFile.c_str() );
            // Store the time we last 'primary fired'.
            ent->client->weaponState.timers.lastSecondaryFire = level.time;
        }

        // Since this logic is ran each frame, use a timer to wait out and stop the sway sound after 200ms.
        if ( ent->client->weaponState.timers.lastSecondaryFire <= ( level.time - 200_ms ) ) {
            ent->client->weaponState.activeSound = 0;
        }

        // Fire the hit trace when the model is at its sweet spot for punching.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 14 ) {
            // Swing the left fist.
            weapon_fists_secondary_fire( ent );
        }
    // Draw Weapon:
    } else if ( ent->client->weaponState.mode == WEAPON_MODE_DRAWING ) {
        // Start playing drawing weapon sound at the very first frame.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
            ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/draw.wav" );
            ent->client->weaponState.timers.lastDrawn = level.time;
        }
        // Enough time has passed, shutdown the sound.
        if ( ent->client->weaponState.timers.lastDrawn <= level.time - 250_ms ) {
            ent->client->weaponState.activeSound = 0;
        }
    // Holster Weapon:
    } else if ( ent->client->weaponState.mode == WEAPON_MODE_HOLSTERING ) {
        // Start playing holster weapon sound at the very first frame.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame ) {
            ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/holster.wav" );
            ent->client->weaponState.timers.lastHolster = level.time;
        }
        // Enough time has passed, shutdown the sound.
        if ( ent->client->weaponState.timers.lastHolster <= level.time - 250_ms ) {
            ent->client->weaponState.activeSound = 0;
        }
    }
}