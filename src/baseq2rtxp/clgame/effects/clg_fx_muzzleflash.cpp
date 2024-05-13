/********************************************************************
*
*
*	ClientGame: Muzzleflashes for the client.
*
*
********************************************************************/
#include "../clg_local.h"
#include "../clg_effects.h"
#include "../clg_entities.h"

// Need it here.
extern cvar_t *cl_dlight_hacks;

/**
*   @brief  Handles the parsed client muzzleflash effects. 
**/
void CLG_MuzzleFlash( void ) {
    vec3_t      fv, rv;
    clg_dlight_t *dl;
    centity_t *pl;
    float       volume;
    char        soundname[ MAX_QPATH ];

    #if USE_DEBUG
    if ( developer->integer ) {
        CLG_CheckEntityPresent( level.parsedMessage.events.muzzleFlash.entity, "muzzleflash" );
    }
    #endif

    pl = &clg_entities[ level.parsedMessage.events.muzzleFlash.entity ];

    dl = CLG_AllocDlight( level.parsedMessage.events.muzzleFlash.entity );
    VectorCopy( pl->current.origin, dl->origin );
    AngleVectors( pl->current.angles, fv, rv, NULL );
    VectorMA( dl->origin, 18, fv, dl->origin );
    VectorMA( dl->origin, 16, rv, dl->origin );
    dl->radius = 100 * ( 2 - level.parsedMessage.events.muzzleFlash.silenced ) + ( Q_rand() & 31 );
    dl->die = clgi.client->time + 16;

    volume = 1.0f - 0.8f * level.parsedMessage.events.muzzleFlash.silenced;

    switch ( level.parsedMessage.events.muzzleFlash.weapon ) {
    case MZ_BLASTER:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/blastf1a.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_BLUEHYPERBLASTER:
        VectorSet( dl->color, 0, 0, 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/hyprbf1a.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_HYPERBLASTER:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/hyprbf1a.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_PISTOL:
        VectorSet( dl->color, 1, 1, 0 );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/pistol/fire%i.wav", ( irandom( 2 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0 );
        break;
    case MZ_MACHINEGUN:
        VectorSet( dl->color, 1, 1, 0 );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/machgf%ib.wav", ( irandom( 5 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0 );
        break;
    case MZ_SHOTGUN:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/shotgf1b.wav" ), volume, ATTN_NORM, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_AUTO, clgi.S_RegisterSound( "weapons/shotgr1b.wav" ), volume, ATTN_NORM, 0.1f );
        break;
    case MZ_SSHOTGUN:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/sshotf1b.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_CHAINGUN1:
        dl->radius = 200 + ( Q_rand() & 31 );
        VectorSet( dl->color, 1, 0.25f, 0 );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/machgf%ib.wav", ( irandom( 5 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0 );
        break;
    case MZ_CHAINGUN2:
        dl->radius = 225 + ( Q_rand() & 31 );
        VectorSet( dl->color, 1, 0.5f, 0 );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/machgf%ib.wav", ( irandom( 5 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0 );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/machgf%ib.wav", ( irandom( 5 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0.05f );
        break;
    case MZ_CHAINGUN3:
        dl->radius = 250 + ( Q_rand() & 31 );
        VectorSet( dl->color, 1, 1, 0 );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/machgf%ib.wav", ( irandom( 5 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0 );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/machgf%ib.wav", ( irandom( 5 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0.033f );
        Q_snprintf( soundname, sizeof( soundname ), "weapons/machgf%ib.wav", ( irandom( 5 ) ) + 1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), volume, ATTN_NORM, 0.066f );
        break;
    case MZ_RAILGUN:
        VectorSet( dl->color, 0.5f, 0.5f, 1.0f );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/railgf1a.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_ROCKET:
        VectorSet( dl->color, 1, 0.5f, 0.2f );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/rocklf1a.wav" ), volume, ATTN_NORM, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_AUTO, clgi.S_RegisterSound( "weapons/rocklr1b.wav" ), volume, ATTN_NORM, 0.1f );
        break;
    case MZ_GRENADE:
        VectorSet( dl->color, 1, 0.5f, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/grenlf1a.wav" ), volume, ATTN_NORM, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_AUTO, clgi.S_RegisterSound( "weapons/grenlr1b.wav" ), volume, ATTN_NORM, 0.1f );
        break;
    case MZ_BFG:
        VectorSet( dl->color, 0, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/bfg__f1y.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_LOGIN:
        VectorSet( dl->color, 0, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/grenlf1a.wav" ), 1, ATTN_NORM, 0 );
        CLG_LogoutEffect( pl->current.origin, level.parsedMessage.events.muzzleFlash.weapon );
        break;
    case MZ_LOGOUT:
        VectorSet( dl->color, 1, 0, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/grenlf1a.wav" ), 1, ATTN_NORM, 0 );
        CLG_LogoutEffect( pl->current.origin, level.parsedMessage.events.muzzleFlash.weapon );
        break;
    case MZ_RESPAWN:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/grenlf1a.wav" ), 1, ATTN_NORM, 0 );
        CLG_LogoutEffect( pl->current.origin, level.parsedMessage.events.muzzleFlash.weapon );
        break;
    case MZ_PHALANX:
        VectorSet( dl->color, 1, 0.5f, 0.5f );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/plasshot.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_IONRIPPER:
        VectorSet( dl->color, 1, 0.5f, 0.5f );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/rippfire.wav" ), volume, ATTN_NORM, 0 );
        break;

    case MZ_ETF_RIFLE:
        VectorSet( dl->color, 0.9f, 0.7f, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/nail1.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_SHOTGUN2:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/shotg2.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_HEATBEAM:
        VectorSet( dl->color, 1, 1, 0 );
        dl->die = clgi.client->time + 100;
        //      S_StartSound (NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound("weapons/bfg__l1a.wav"), volume, ATTN_NORM, 0);
        break;
    case MZ_BLASTER2:
        VectorSet( dl->color, 0, 1, 0 );
        // FIXME - different sound for blaster2 ??
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/blastf1a.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_TRACKER:
        // negative flashes handled the same in gl/soft until CL_AddDLights
        VectorSet( dl->color, -1, -1, -1 );
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/disint2.wav" ), volume, ATTN_NORM, 0 );
        break;
    case MZ_NUKE1:
        VectorSet( dl->color, 1, 0, 0 );
        dl->die = clgi.client->time + 100;
        break;
    case MZ_NUKE2:
        VectorSet( dl->color, 1, 1, 0 );
        dl->die = clgi.client->time + 100;
        break;
    case MZ_NUKE4:
        VectorSet( dl->color, 0, 0, 1 );
        dl->die = clgi.client->time + 100;
        break;
    case MZ_NUKE8:
        VectorSet( dl->color, 0, 1, 1 );
        dl->die = clgi.client->time + 100;
        break;

        // Q2RTX
    case MZ_FLARE:
        dl->radius = 0;
        clgi.S_StartSound( NULL, level.parsedMessage.events.muzzleFlash.entity, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/flaregun.wav" ), volume, ATTN_NORM, 0 );
        break;
        // Q2RTX
    }

    if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
        // don't add muzzle flashes in RTX mode
        dl->radius = 0;
    }

    if ( cl_dlight_hacks->integer & DLHACK_NO_MUZZLEFLASH ) {
        switch ( level.parsedMessage.events.muzzleFlash.weapon ) {
        case MZ_MACHINEGUN:
        case MZ_CHAINGUN1:
        case MZ_CHAINGUN2:
        case MZ_CHAINGUN3:
            memset( dl, 0, sizeof( *dl ) );
            break;
        }
    }
}