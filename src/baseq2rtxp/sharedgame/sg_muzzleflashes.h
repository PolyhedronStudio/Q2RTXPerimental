/********************************************************************
*
*
*   Muzzleflashes:
*
*
********************************************************************/
#pragma once


typedef enum sg_muzzleflash_e {
    MZ_FIST_LEFT,
    MZ_FIST_RIGHT,

    MZ_PISTOL,

    MZ_LOGIN,
    MZ_LOGOUT,

    MZ_RESPAWN,
    MZ_ITEMRESPAWN,

    //MZ_SILENCED = 128,  // bit flag ORed with one of the above numbers
} sg_muzzleflash_t;
QENUM_BIT_FLAGS( sg_muzzleflash_t );