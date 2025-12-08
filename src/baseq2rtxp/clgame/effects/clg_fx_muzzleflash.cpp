/********************************************************************
*
*
*	ClientGame: Muzzleflashes for the client.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_precache.h"
#include "clgame/effects/clg_fx_muzzleflash.h"


#include "sharedgame/sg_muzzleflashes.h"


/**
*   @brief Adds a muzzleflash dynamic light effect.
**/
clg_dlight_t *CLG_AddMuzzleflashDynamicLight( const centity_t *pl, const Vector3 &vForward, const Vector3 &vRight ) {
    clg_dlight_t *dynamicLight = CLG_AllocateDynamicLight( pl->current.number );
    VectorCopy( pl->current.origin, dynamicLight->origin );

    //QM_AngleVectors( pl->current.angles, &vForward, &vRight, NULL );

    VectorMA( dynamicLight->origin, 18, vForward, dynamicLight->origin );
    VectorMA( dynamicLight->origin, 16, vRight, dynamicLight->origin );

    dynamicLight->radius = 100 * ( 2 ) + ( Q_rand() & 31 );
    dynamicLight->die = clgi.client->time + 16;

    return dynamicLight;
}
/**
*   @brief  Handles the parsed client muzzleflash effects. 
**/
void CLG_MuzzleFlash( void ) {
    // For storing sound effect path.
    char soundname[ MAX_QPATH ];

#if USE_DEBUG
    if ( developer->integer ) {
        CLG_CheckEntityPresent( level.parsedMessage.events.muzzleFlash.entity, "muzzleflash" );
    }
#endif // #if USE_DEBUG

    // Get player client entity.
    centity_t *pl = &clg_entities[ level.parsedMessage.events.muzzleFlash.entity ];
    // Create the angle vectors.
    Vector3 fv = {}, rv = {};
    QM_AngleVectors( pl->current.angles, &fv, &rv, NULL );

    /**
    *   Figure out the effects that do NOT want a DLight. If not in the list, a DLight
    *   is allocated for the effect to use.
    **/
    // DLight pointer.
    clg_dlight_t *dl = nullptr;
    // Figure out the effects that desire a dynamic muzzleflash light effect.
    const int32_t muzzleFlashWeapon = level.parsedMessage.events.muzzleFlash.weapon;
    // All effects that do NOT use the DLight.
    switch ( muzzleFlashWeapon ) {
        case MZ_FIST_LEFT:
        case MZ_FIST_RIGHT:
            dl = nullptr;
            break;
        // Default to creating a DLight for other effects:
        default:
            dl = CLG_AddMuzzleflashDynamicLight( pl, fv, rv );
            break;
    }

    /**
    *   Now respond appropriately to the specified MuzzleFlash Effect.
    **/
    // Determine its volume.
    const float volume = 1.0f - 0.8f * level.parsedMessage.events.muzzleFlash.silenced;
    // Determine the effect to apply.
    switch ( muzzleFlashWeapon ) {
        /**
        *   NO DLight Effects:
        **/
        // Weapon( Fists ) Effect for throwing a left fist punch:
        case MZ_FIST_LEFT:
            Q_snprintf( soundname, sizeof( soundname ), "weapons/fists/punch%i.wav", 1/*( irandom( 2 ) ) + 1*/ );
            clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, precache.sfx.weapons.fists.punch1, volume, ATTN_NORM, 0 );
            break;
        // Weapon( Fists ) Effect for throwing a right fist punch:
        case MZ_FIST_RIGHT:
            Q_snprintf( soundname, sizeof( soundname ), "weapons/fists/punch%i.wav", 1/*( irandom( 2 ) ) + 1*/ );
            clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, precache.sfx.weapons.fists.punch1, volume, ATTN_NORM, 0 );
            break;

        /**
        *   DLight Effects:
        **/
        // Weapon( Pistol ) Fire Flash:
        case MZ_PISTOL:
        {
            VectorSet( dl->color, 0.93, 0.82, 0.71 );
            const int32_t fireSoundIndex = ( irandom( 2 ) );
            clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, precache.sfx.weapons.pistol.fire[ fireSoundIndex ], volume, ATTN_NORM, 0 );
            break;
        }
        // Event( Login ) Particles(By having just connected and performing a first spawn):
        case MZ_LOGIN:
            VectorSet( dl->color, 0, 1, 0 );
            clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, precache.sfx.world.mz_login, 1, ATTN_NORM, 0 );
            CLG_FX_LogoutEffect( pl->current.origin, level.parsedMessage.events.muzzleFlash.weapon );
            break;
        // Event( Logout ) Particles(Disconnected UnSpawn):
        case MZ_LOGOUT:
            VectorSet( dl->color, 1, 0, 0 );
            clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, precache.sfx.world.mz_logout, 1, ATTN_NORM, 0 );
            CLG_FX_LogoutEffect( pl->current.origin, level.parsedMessage.events.muzzleFlash.weapon );
            break;
        // Event( Respawn ) Particles:
        case MZ_RESPAWN:
            VectorSet( dl->color, 1, 1, 0 );
            clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, precache.sfx.world.mz_login, 1, ATTN_NORM, 0 );
            CLG_FX_LogoutEffect( pl->current.origin, level.parsedMessage.events.muzzleFlash.weapon );
            break;
        default:
            clgi.Print( PRINT_DEVELOPER, "%s: WARNING(Unhandled muzzle flash effect[#%i])!\n", __func__, muzzleFlashWeapon );
            break;
    }
}