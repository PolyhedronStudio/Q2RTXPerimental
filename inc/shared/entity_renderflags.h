/********************************************************************
*
*
*   Entity "Render FX" Flags:
*
*
********************************************************************/
#pragma once


//! entity_state_t->renderfx flags
#define RF_NONE             0
#define RF_MINLIGHT         1       // allways have some light (viewmodel)
#define RF_VIEWERMODEL      2       // don't draw through eyes, only mirrors
#define RF_WEAPONMODEL      4       // only draw through eyes
#define RF_FULLBRIGHT       8       // allways draw full intensity
#define RF_DEPTHHACK        16      // for view weapon Z crunching
#define RF_TRANSLUCENT      32
#define RF_FRAMELERP        64
#define RF_BEAM             128
#define RF_CUSTOMSKIN       256     // skin is an index in image_precache
#define RF_GLOW             512     // pulse lighting for bonus items
#define RF_SHELL_RED        1024
#define RF_SHELL_GREEN      2048
#define RF_SHELL_BLUE       4096
#define RF_NOSHADOW         8192    // used by YQ2
#define	RF_OLD_FRAME_LERP	16384	// [Paril-KEX] force model to lerp from oldframe in entity state; otherwise it uses last frame client received
#define RF_STAIR_STEP		32768	// [Paril-KEX] re-tuned, now used to handle stair steps for monsters

//ROGUE
#define RF_IR_VISIBLE       0x00008000      // 32768
#define RF_SHELL_DOUBLE     0x00010000      // 65536
#define RF_SHELL_HALF_DAM   0x00020000
#define RF_USE_DISGUISE     0x00040000
//ROGUE