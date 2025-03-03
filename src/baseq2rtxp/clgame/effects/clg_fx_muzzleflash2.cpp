/********************************************************************
*
*
*	ClientGame: Muzzleflashes for entities/monsters.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_temp_entities.h"

// Removed??
#if 0
/**
*   @brief  Handles the parsed entities/monster muzzleflash effects.
**/
void CLG_MuzzleFlash2( void ) {
    centity_t *ent;
    vec3_t      origin;
    const vec_t *ofs;
    clg_dlight_t *dl;
    vec3_t      forward, right;
    char        soundname[ MAX_QPATH ];

    const int32_t entityNumber = level.parsedMessage.events.muzzleFlash.entity;
    const int32_t weaponID = level.parsedMessage.events.muzzleFlash.weapon;
    // locate the origin
    ent = &clg_entities[ entityNumber ];
    AngleVectors( ent->current.angles, forward, right, NULL );
    ofs = monster_flash_offset[ weaponID ];
    origin[ 0 ] = ent->current.origin[ 0 ] + forward[ 0 ] * ofs[ 0 ] + right[ 0 ] * ofs[ 1 ];
    origin[ 1 ] = ent->current.origin[ 1 ] + forward[ 1 ] * ofs[ 0 ] + right[ 1 ] * ofs[ 1 ];
    origin[ 2 ] = ent->current.origin[ 2 ] + forward[ 2 ] * ofs[ 0 ] + right[ 2 ] * ofs[ 1 ] + ofs[ 2 ];

    dl = CLG_AllocDlight( entityNumber );
    VectorCopy( origin, dl->origin );
    dl->radius = 200 + ( Q_rand() & 31 );
    dl->die = clgi.client->time + 16;

    switch ( weaponID ) {
    case MZ2_INFANTRY_MACHINEGUN_1:
    case MZ2_INFANTRY_MACHINEGUN_2:
    case MZ2_INFANTRY_MACHINEGUN_3:
    case MZ2_INFANTRY_MACHINEGUN_4:
    case MZ2_INFANTRY_MACHINEGUN_5:
    case MZ2_INFANTRY_MACHINEGUN_6:
    case MZ2_INFANTRY_MACHINEGUN_7:
    case MZ2_INFANTRY_MACHINEGUN_8:
    case MZ2_INFANTRY_MACHINEGUN_9:
    case MZ2_INFANTRY_MACHINEGUN_10:
    case MZ2_INFANTRY_MACHINEGUN_11:
    case MZ2_INFANTRY_MACHINEGUN_12:
    case MZ2_INFANTRY_MACHINEGUN_13:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "infantry/infatck1.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_SOLDIER_MACHINEGUN_1:
    case MZ2_SOLDIER_MACHINEGUN_2:
    case MZ2_SOLDIER_MACHINEGUN_3:
    case MZ2_SOLDIER_MACHINEGUN_4:
    case MZ2_SOLDIER_MACHINEGUN_5:
    case MZ2_SOLDIER_MACHINEGUN_6:
    case MZ2_SOLDIER_MACHINEGUN_7:
    case MZ2_SOLDIER_MACHINEGUN_8:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "soldier/solatck3.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_GUNNER_MACHINEGUN_1:
    case MZ2_GUNNER_MACHINEGUN_2:
    case MZ2_GUNNER_MACHINEGUN_3:
    case MZ2_GUNNER_MACHINEGUN_4:
    case MZ2_GUNNER_MACHINEGUN_5:
    case MZ2_GUNNER_MACHINEGUN_6:
    case MZ2_GUNNER_MACHINEGUN_7:
    case MZ2_GUNNER_MACHINEGUN_8:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "gunner/gunatck2.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_ACTOR_MACHINEGUN_1:
    case MZ2_SUPERTANK_MACHINEGUN_1:
    case MZ2_SUPERTANK_MACHINEGUN_2:
    case MZ2_SUPERTANK_MACHINEGUN_3:
    case MZ2_SUPERTANK_MACHINEGUN_4:
    case MZ2_SUPERTANK_MACHINEGUN_5:
    case MZ2_SUPERTANK_MACHINEGUN_6:
    case MZ2_TURRET_MACHINEGUN:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "infantry/infatck1.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_BOSS2_MACHINEGUN_L1:
    case MZ2_BOSS2_MACHINEGUN_L2:
    case MZ2_BOSS2_MACHINEGUN_L3:
    case MZ2_BOSS2_MACHINEGUN_L4:
    case MZ2_BOSS2_MACHINEGUN_L5:
    case MZ2_CARRIER_MACHINEGUN_L1:
    case MZ2_CARRIER_MACHINEGUN_L2:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "infantry/infatck1.wav" ), 1, ATTN_NONE, 0 );
        break;

    case MZ2_SOLDIER_BLASTER_1:
    case MZ2_SOLDIER_BLASTER_2:
    case MZ2_SOLDIER_BLASTER_3:
    case MZ2_SOLDIER_BLASTER_4:
    case MZ2_SOLDIER_BLASTER_5:
    case MZ2_SOLDIER_BLASTER_6:
    case MZ2_SOLDIER_BLASTER_7:
    case MZ2_SOLDIER_BLASTER_8:
    case MZ2_TURRET_BLASTER:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "soldier/solatck2.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_FLYER_BLASTER_1:
    case MZ2_FLYER_BLASTER_2:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "flyer/flyatck3.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_MEDIC_BLASTER_1:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "medic/medatck1.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_HOVER_BLASTER_1:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "hover/hovatck1.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_FLOAT_BLASTER_1:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "floater/fltatck1.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_SOLDIER_SHOTGUN_1:
    case MZ2_SOLDIER_SHOTGUN_2:
    case MZ2_SOLDIER_SHOTGUN_3:
    case MZ2_SOLDIER_SHOTGUN_4:
    case MZ2_SOLDIER_SHOTGUN_5:
    case MZ2_SOLDIER_SHOTGUN_6:
    case MZ2_SOLDIER_SHOTGUN_7:
    case MZ2_SOLDIER_SHOTGUN_8:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_SmokeAndFlash( origin );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "soldier/solatck1.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_TANK_BLASTER_1:
    case MZ2_TANK_BLASTER_2:
    case MZ2_TANK_BLASTER_3:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "tank/tnkatck3.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_TANK_MACHINEGUN_1:
    case MZ2_TANK_MACHINEGUN_2:
    case MZ2_TANK_MACHINEGUN_3:
    case MZ2_TANK_MACHINEGUN_4:
    case MZ2_TANK_MACHINEGUN_5:
    case MZ2_TANK_MACHINEGUN_6:
    case MZ2_TANK_MACHINEGUN_7:
    case MZ2_TANK_MACHINEGUN_8:
    case MZ2_TANK_MACHINEGUN_9:
    case MZ2_TANK_MACHINEGUN_10:
    case MZ2_TANK_MACHINEGUN_11:
    case MZ2_TANK_MACHINEGUN_12:
    case MZ2_TANK_MACHINEGUN_13:
    case MZ2_TANK_MACHINEGUN_14:
    case MZ2_TANK_MACHINEGUN_15:
    case MZ2_TANK_MACHINEGUN_16:
    case MZ2_TANK_MACHINEGUN_17:
    case MZ2_TANK_MACHINEGUN_18:
    case MZ2_TANK_MACHINEGUN_19:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        Q_snprintf( soundname, sizeof( soundname ), "tank/tnkatk2%c.wav", 'a' + irandom( 5 ) );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( soundname ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_CHICK_ROCKET_1:
    case MZ2_TURRET_ROCKET:
        VectorSet( dl->color, 1, 0.5f, 0.2f );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "chick/chkatck2.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_TANK_ROCKET_1:
    case MZ2_TANK_ROCKET_2:
    case MZ2_TANK_ROCKET_3:
        VectorSet( dl->color, 1, 0.5f, 0.2f );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "tank/tnkatck1.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_SUPERTANK_ROCKET_1:
    case MZ2_SUPERTANK_ROCKET_2:
    case MZ2_SUPERTANK_ROCKET_3:
    case MZ2_BOSS2_ROCKET_1:
    case MZ2_BOSS2_ROCKET_2:
    case MZ2_BOSS2_ROCKET_3:
    case MZ2_BOSS2_ROCKET_4:
    case MZ2_CARRIER_ROCKET_1:
        //  case MZ2_CARRIER_ROCKET_2:
        //  case MZ2_CARRIER_ROCKET_3:
        //  case MZ2_CARRIER_ROCKET_4:
        VectorSet( dl->color, 1, 0.5f, 0.2f );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "tank/rocket.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_GUNNER_GRENADE_1:
    case MZ2_GUNNER_GRENADE_2:
    case MZ2_GUNNER_GRENADE_3:
    case MZ2_GUNNER_GRENADE_4:
        VectorSet( dl->color, 1, 0.5f, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "gunner/gunatck3.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_GLADIATOR_RAILGUN_1:
    case MZ2_CARRIER_RAILGUN:
    case MZ2_WIDOW_RAIL:
        VectorSet( dl->color, 0.5f, 0.5f, 1.0f );
        break;

    case MZ2_MAKRON_BFG:
        VectorSet( dl->color, 0.5f, 1, 0.5f );
        //S_StartSound (NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound("makron/bfg_fire.wav"), 1, ATTN_NORM, 0);
        break;

    case MZ2_MAKRON_BLASTER_1:
    case MZ2_MAKRON_BLASTER_2:
    case MZ2_MAKRON_BLASTER_3:
    case MZ2_MAKRON_BLASTER_4:
    case MZ2_MAKRON_BLASTER_5:
    case MZ2_MAKRON_BLASTER_6:
    case MZ2_MAKRON_BLASTER_7:
    case MZ2_MAKRON_BLASTER_8:
    case MZ2_MAKRON_BLASTER_9:
    case MZ2_MAKRON_BLASTER_10:
    case MZ2_MAKRON_BLASTER_11:
    case MZ2_MAKRON_BLASTER_12:
    case MZ2_MAKRON_BLASTER_13:
    case MZ2_MAKRON_BLASTER_14:
    case MZ2_MAKRON_BLASTER_15:
    case MZ2_MAKRON_BLASTER_16:
    case MZ2_MAKRON_BLASTER_17:
        VectorSet( dl->color, 1, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "makron/blaster.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_JORG_MACHINEGUN_L1:
    case MZ2_JORG_MACHINEGUN_L2:
    case MZ2_JORG_MACHINEGUN_L3:
    case MZ2_JORG_MACHINEGUN_L4:
    case MZ2_JORG_MACHINEGUN_L5:
    case MZ2_JORG_MACHINEGUN_L6:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "boss3/xfire.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_JORG_MACHINEGUN_R1:
    case MZ2_JORG_MACHINEGUN_R2:
    case MZ2_JORG_MACHINEGUN_R3:
    case MZ2_JORG_MACHINEGUN_R4:
    case MZ2_JORG_MACHINEGUN_R5:
    case MZ2_JORG_MACHINEGUN_R6:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        break;

    case MZ2_JORG_BFG_1:
        VectorSet( dl->color, 0.5f, 1, 0.5f );
        break;

    case MZ2_BOSS2_MACHINEGUN_R1:
    case MZ2_BOSS2_MACHINEGUN_R2:
    case MZ2_BOSS2_MACHINEGUN_R3:
    case MZ2_BOSS2_MACHINEGUN_R4:
    case MZ2_BOSS2_MACHINEGUN_R5:
    case MZ2_CARRIER_MACHINEGUN_R1:
    case MZ2_CARRIER_MACHINEGUN_R2:
        VectorSet( dl->color, 1, 1, 0 );
        CLG_ParticleEffect( origin, forward, 0, 40 );
        CLG_SmokeAndFlash( origin );
        break;

    case MZ2_STALKER_BLASTER:
    case MZ2_DAEDALUS_BLASTER:
    case MZ2_MEDIC_BLASTER_2:
    case MZ2_WIDOW_BLASTER:
    case MZ2_WIDOW_BLASTER_SWEEP1:
    case MZ2_WIDOW_BLASTER_SWEEP2:
    case MZ2_WIDOW_BLASTER_SWEEP3:
    case MZ2_WIDOW_BLASTER_SWEEP4:
    case MZ2_WIDOW_BLASTER_SWEEP5:
    case MZ2_WIDOW_BLASTER_SWEEP6:
    case MZ2_WIDOW_BLASTER_SWEEP7:
    case MZ2_WIDOW_BLASTER_SWEEP8:
    case MZ2_WIDOW_BLASTER_SWEEP9:
    case MZ2_WIDOW_BLASTER_100:
    case MZ2_WIDOW_BLASTER_90:
    case MZ2_WIDOW_BLASTER_80:
    case MZ2_WIDOW_BLASTER_70:
    case MZ2_WIDOW_BLASTER_60:
    case MZ2_WIDOW_BLASTER_50:
    case MZ2_WIDOW_BLASTER_40:
    case MZ2_WIDOW_BLASTER_30:
    case MZ2_WIDOW_BLASTER_20:
    case MZ2_WIDOW_BLASTER_10:
    case MZ2_WIDOW_BLASTER_0:
    case MZ2_WIDOW_BLASTER_10L:
    case MZ2_WIDOW_BLASTER_20L:
    case MZ2_WIDOW_BLASTER_30L:
    case MZ2_WIDOW_BLASTER_40L:
    case MZ2_WIDOW_BLASTER_50L:
    case MZ2_WIDOW_BLASTER_60L:
    case MZ2_WIDOW_BLASTER_70L:
    case MZ2_WIDOW_RUN_1:
    case MZ2_WIDOW_RUN_2:
    case MZ2_WIDOW_RUN_3:
    case MZ2_WIDOW_RUN_4:
    case MZ2_WIDOW_RUN_5:
    case MZ2_WIDOW_RUN_6:
    case MZ2_WIDOW_RUN_7:
    case MZ2_WIDOW_RUN_8:
        VectorSet( dl->color, 0, 1, 0 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "tank/tnkatck3.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_WIDOW_DISRUPTOR:
        VectorSet( dl->color, -1, -1, -1 );
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "weapons/disint2.wav" ), 1, ATTN_NORM, 0 );
        break;

    case MZ2_WIDOW_PLASMABEAM:
    case MZ2_WIDOW2_BEAMER_1:
    case MZ2_WIDOW2_BEAMER_2:
    case MZ2_WIDOW2_BEAMER_3:
    case MZ2_WIDOW2_BEAMER_4:
    case MZ2_WIDOW2_BEAMER_5:
    case MZ2_WIDOW2_BEAM_SWEEP_1:
    case MZ2_WIDOW2_BEAM_SWEEP_2:
    case MZ2_WIDOW2_BEAM_SWEEP_3:
    case MZ2_WIDOW2_BEAM_SWEEP_4:
    case MZ2_WIDOW2_BEAM_SWEEP_5:
    case MZ2_WIDOW2_BEAM_SWEEP_6:
    case MZ2_WIDOW2_BEAM_SWEEP_7:
    case MZ2_WIDOW2_BEAM_SWEEP_8:
    case MZ2_WIDOW2_BEAM_SWEEP_9:
    case MZ2_WIDOW2_BEAM_SWEEP_10:
    case MZ2_WIDOW2_BEAM_SWEEP_11:
        dl->radius = 300 + ( Q_rand() & 100 );
        VectorSet( dl->color, 1, 1, 0 );
        dl->die = clgi.client->time + 200;
        break;
    }
}
#endif