/********************************************************************
*
*
*   Muzzleflashes:
*
*
********************************************************************/
#pragma once


enum {
    MZ_BLASTER,
    MZ_MACHINEGUN,
    MZ_SHOTGUN,
    MZ_CHAINGUN1,
    MZ_CHAINGUN2,
    MZ_CHAINGUN3,
    MZ_RAILGUN,
    MZ_ROCKET,
    MZ_GRENADE,
    MZ_LOGIN,
    MZ_LOGOUT,
    MZ_RESPAWN,
    MZ_BFG,
    MZ_SSHOTGUN,
    MZ_HYPERBLASTER,
    MZ_ITEMRESPAWN,

    // RAFAEL
    MZ_IONRIPPER,
    MZ_BLUEHYPERBLASTER,
    MZ_PHALANX,

    //ROGUE
    MZ_ETF_RIFLE = 30,
    MZ_UNUSED,
    MZ_SHOTGUN2,
    MZ_HEATBEAM,
    MZ_BLASTER2,
    MZ_TRACKER,
    MZ_NUKE1,
    MZ_NUKE2,
    MZ_NUKE4,
    MZ_NUKE8,
    //ROGUE
    // Q2RTX
    #define MZ_FLARE            40
    // Q2RTX

    MZ_SILENCED = 128,  // bit flag ORed with one of the above numbers
};