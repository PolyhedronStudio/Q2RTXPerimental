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

#include "../g_local.h"


/**
*
*
*	Core Weapon Mechanics:
*
*
**/
/**
*   Pistol Weapon Mode Animation Frames.
**/
weapon_mode_frames_t pistolAnimationFrames[ WEAPON_MODE_MAX ] = {

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
    // Mode Animation: RELOADING
    /*modeAnimationFrames[ WEAPON_MODE_RELOADING ] = */{
        .startFrame = 13,
        .endFrame = 54,
        .durationFrames = ( 54 - 13 )
    }
};



/**
*
*
*   PISTOL
*
*
**/
/**
*   @brief  Pistol weapon 'fire' method:
**/
void weapon_pistol_fire( edict_t *ent ) {
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 8;
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
    gi.WriteUint8( MZ_MACHINEGUN /*| is_silenced*/ );
    gi.multicast( ent->s.origin, MULTICAST_PVS, false );

    // Notify we're making noise.
    P_PlayerNoise( ent, start, PNOISE_WEAPON );

    // Decrease clip ammo.
    ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ]--;
    //if ( !( (int)dmflags->value & DF_INFINITE_AMMO ) )
    //    ent->client->pers.inventory[ ent->client->ammo_index ]--;

}

/**
*   @brief  Processes responses to the user input.
**/
static void Weapon_Pistol_ProcessUserInput( edict_t *ent ) {
    if ( ( ent->client->latched_buttons & BUTTON_ATTACK ) /*&& ( ent->client->buttons & BUTTON_ATTACK )*/ ) {
        // We need to have clip ammo.
        if ( ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] ) {
            P_Weapon_SwitchMode( ent, WEAPON_MODE_PRIMARY_FIRING, pistolAnimationFrames, false );
            // Otherwise, reload:
        } else {
            // Ammo amount to subtract.
            int32_t subtractAmmo = 13;
            // Get total ammo amount.
            int32_t totalAmmo = ent->client->pers.inventory[ ent->client->ammo_index ];
            if ( totalAmmo < 13 ) {
                subtractAmmo = totalAmmo;
            }
            if ( totalAmmo <= 0 ) {
                return;
            }

            ent->client->pers.weapon_clip_ammo[ ent->client->pers.weapon->weapon_index ] = subtractAmmo;
            ent->client->pers.inventory[ ent->client->ammo_index ] -= subtractAmmo;

        }
    }
}
/**
*   @brief  Pistol Weapon State Machine.
**/
void Weapon_Pistol( edict_t *ent ) {
    // Process the animation frames of the mode we're in.
    const bool isDoneAnimating = P_Weapon_ProcessModeAnimation( ent, &pistolAnimationFrames[ ent->client->weaponState.mode ] );

    //// If done animating, switch back to idle mode.
    //if ( isDoneAnimating ) {
    //    P_Weapon_SwitchMode( ent, WEAPON_MODE_IDLE );
    //}

    // If IDLE or NOT ANIMATING, process user input.
    if ( ent->client->weaponState.mode == WEAPON_MODE_IDLE || isDoneAnimating ) {
        // Respond to user input, which determines whether 
        Weapon_Pistol_ProcessUserInput( ent );
    }

    // Process logic for state specific frames.
    if ( ent->client->weaponState.mode == WEAPON_MODE_PRIMARY_FIRING ) {
        // Due to this being possibly called multiple times in the same frame, we depend on a timer for this to prevent
        // any earlier/double firing.
        if ( ent->client->weaponState.animation.currentFrame == ent->client->weaponState.animation.startFrame + 3 ) {
            // Wait out until enough time has passed.
            if ( ent->client->weaponState.timers.lastPrimaryFire <= ( level.time + 325_ms ) ) {
                // Fire the pistol bullet.
                weapon_pistol_fire( ent );
                // Store the time we last 'primary fired'.
                ent->client->weaponState.timers.lastPrimaryFire = level.time;
            }
        }
    }
}