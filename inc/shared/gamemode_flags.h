/********************************************************************
*
*
*   Game Mode Flags:
*
*
********************************************************************/
#pragma once


// dmflags->value flags
#define DF_NO_HEALTH        0x00000001  // 1
#define DF_NO_ITEMS         0x00000002  // 2
#define DF_WEAPONS_STAY     0x00000004  // 4
#define DF_NO_FALLING       0x00000008  // 8
#define DF_INSTANT_ITEMS    0x00000010  // 16
#define DF_SAME_LEVEL       0x00000020  // 32
#define DF_SKINTEAMS        0x00000040  // 64
#define DF_MODELTEAMS       0x00000080  // 128
#define DF_NO_FRIENDLY_FIRE 0x00000100  // 256
#define DF_SPAWN_FARTHEST   0x00000200  // 512
#define DF_FORCE_RESPAWN    0x00000400  // 1024
#define DF_NO_ARMOR         0x00000800  // 2048
#define DF_ALLOW_EXIT       0x00001000  // 4096
#define DF_INFINITE_AMMO    0x00002000  // 8192
#define DF_QUAD_DROP        0x00004000  // 16384
#define DF_FIXED_FOV        0x00008000  // 32768
// RAFAEL
#define DF_QUADFIRE_DROP    0x00010000  // 65536
//ROGUE
#define DF_NO_MINES         0x00020000
#define DF_NO_STACK_DOUBLE  0x00040000
#define DF_NO_NUKES         0x00080000
#define DF_NO_SPHERES       0x00100000
//ROGUE