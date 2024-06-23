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

        // idle.
        if ( !strcmp( iqmAnim->name, "idle" ) ) {
            P_Weapon_ModeAnimationFromIQM( &fistsItemInfo, iqmAnim, WEAPON_MODE_IDLE );
        } else if ( !strcmp( iqmAnim->name, "draw" ) ) {
            P_Weapon_ModeAnimationFromIQM( &fistsItemInfo, iqmAnim, WEAPON_MODE_DRAWING );
        } else if ( !strcmp( iqmAnim->name, "holster" ) ) {
            P_Weapon_ModeAnimationFromIQM( &fistsItemInfo, iqmAnim, WEAPON_MODE_HOLSTERING );
        } else if ( !strcmp( iqmAnim->name, "primary_fire" ) ) {
            P_Weapon_ModeAnimationFromIQM( &fistsItemInfo, iqmAnim, WEAPON_MODE_PRIMARY_FIRING );
        } else if ( !strcmp( iqmAnim->name, "secondary_fire" ) ) {
            P_Weapon_ModeAnimationFromIQM( &fistsItemInfo, iqmAnim, WEAPON_MODE_SECONDARY_FIRING );
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
    P_ProjectDistance( ent, ent->s.origin, fistOffset, &forward.x, &right.x, start ); //P_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

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
    P_PlayerNoise( ent, start, PNOISE_WEAPON );
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
    P_ProjectDistance( ent, ent->s.origin, fistOffset, &forward.x, &right.x, &start.x); //P_ProjectSource( ent, ent->s.origin, offset, forward, right, start );

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
    P_PlayerNoise( ent, &start.x, PNOISE_WEAPON );
}

/**
*   @brief  Processes responses to the user input.
**/
static void Weapon_Fists_ProcessUserInput( edict_t *ent ) {
    if ( ent->client->latched_buttons & BUTTON_PRIMARY_FIRE ) {
        P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, fistsItemInfo.modeAnimations, false );
        return;
    } else if ( ent->client->latched_buttons & BUTTON_SECONDARY_FIRE ) {
        P_Weapon_SwitchMode( ent, WEAPON_MODE_SECONDARY_FIRING, fistsItemInfo.modeAnimations, false );
        return;
    }
}

/**
*   @brief  Pistol Weapon 'State Machine'.
**/
void Weapon_Fists( edict_t *ent, const bool processUserInputOnly ) {
    // Process the animation frames of the mode we're in.
    const bool isDoneAnimating = P_Weapon_ProcessModeAnimation( ent, &fistsItemInfo.modeAnimations[ ent->client->weaponState.mode ] );

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
    // Primary Fire, throws a left punch fist.
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
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1) {
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
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 1) {
            ent->client->weaponState.activeSound = gi.soundindex( "weapons/pistol/holster.wav" );
            ent->client->weaponState.timers.lastHolster = level.time;
        }
        // Enough time has passed, shutdown the sound.
        if ( ent->client->weaponState.timers.lastHolster <= level.time - 250_ms ) {
            ent->client->weaponState.activeSound = 0;
        }
    }
}








///**
//* @brief	Calculates the current frame for the current time since the start time stamp.
//*
//* @return   The frame and interpolation fraction for current time in an animation started at a given time.
//*           When the animation is finished it will return frame -1. Takes looping into account. Looping animations
//*           are never finished.
//**/
//double SG_FrameForTime( int32_t *frame, const GameTime &currentTimestamp, const GameTime &startTimeStamp, float frameTime, int32_t startFrame, int32_t endFrame, int32_t loopingFrames, bool forceLoop ) {
//    // Always set frame to start frame if the current time stamp is lower than the animation start timestamp.
//    if ( currentTimestamp <= startTimeStamp ) {
//        *frame = startFrame;
//        return 0.0f;
//    }
//
//    // If start frame == end frame, it means we have no animation to begin with, return 1.0 fraction and set frame to startframe.
//    if ( startFrame == endFrame ) {
//        *frame = startFrame;
//        return 1.0f;
//    }
//
//    // Calculate current amount of time this animation is running for.
//    GameTime runningTime = currentTimestamp - startTimeStamp;
//
//    // Calculate frame fraction.
//    double frameFraction = ( runningTime / frameTime ).count();
//    int64_t frameCount = (int64_t)frameFraction;
//    frameFraction -= frameCount;
//
//    // Calculate current frame.
//    uint32_t currentFrame = startFrame + frameCount;
//
//    // If current frame is higher than last frame...
//    if ( currentFrame >= endFrame ) {
//        if ( forceLoop && !loopingFrames ) {
//            loopingFrames = endFrame - startFrame;
//        }
//
//        if ( loopingFrames ) {
//            // Calculate current loop start #.
//            uint32_t startCount = ( endFrame - startFrame ) - loopingFrames;
//
//            // Calculate the number of loops left to do.
//            uint32_t numberOfLoops = ( frameCount - startCount ) / loopingFrames;
//
//            // Calculate current frame.
//            currentFrame -= loopingFrames * numberOfLoops;
//
//            // Special frame fraction handling to play an animation just once.
//            if ( loopingFrames == 1 ) {
//                frameFraction = 1.0;
//            }
//        } else {
//            // Animation's finished, set current frame to -1 and get over with it.
//            currentFrame = -1;
//        }
//    }
//
//    // Assign new frame value.
//    *frame = currentFrame;
//
//    // Return frame fraction.
//    return frameFraction;
//}
//
//
//
///*
//* GS_FrameForTime
//* Returns the frame and interpolation fraction for current time in an animation started at a given time.
//* When the animation is finished it will return frame -1. Takes looping into account. Looping animations
//* are never finished.
//*/
//float GS_FrameForTime( int *frame, int64_t curTime, int64_t startTimeStamp, float frametime, int firstframe, int lastframe, int loopingframes, bool forceLoop ) {
//    int64_t runningtime, framecount;
//    int curframe;
//    float framefrac;
//
//    if ( curTime <= startTimeStamp ) {
//        *frame = firstframe;
//        return 0.0f;
//    }
//
//    if ( firstframe == lastframe ) {
//        *frame = firstframe;
//        return 1.0f;
//    }
//
//    runningtime = curTime - startTimeStamp;
//    framefrac = ( (double)runningtime / (double)frametime );
//    framecount = (unsigned int)framefrac;
//    framefrac -= framecount;
//
//    curframe = firstframe + framecount;
//    if ( curframe > lastframe ) {
//        if ( forceLoop && !loopingframes ) {
//            loopingframes = lastframe - firstframe;
//        }
//
//        if ( loopingframes ) {
//            unsigned int numloops;
//            unsigned int startcount;
//
//            startcount = ( lastframe - firstframe ) - loopingframes;
//
//            numloops = ( framecount - startcount ) / loopingframes;
//            curframe -= loopingframes * numloops;
//            if ( loopingframes == 1 ) {
//                framefrac = 1.0f;
//            }
//        } else {
//            curframe = -1;
//        }
//    }
//
//    *frame = curframe;
//
//    return framefrac;
//}