/********************************************************************
*
*
*   Entity "Render FX" Flags:
*
*	NOTE: These are also passed into Lua, make sure to (re-)adjust `RenderFx` enum in svg_lua_gamelib.cpp
*
********************************************************************/
#pragma once

//! entity_state_t->renderfx flags
#define RF_NONE             0
#define RF_MINLIGHT         BIT( 0 ) // 1       // allways have some light (viewmodel)
#define RF_VIEWERMODEL      BIT( 1 ) // 2       // don't draw through eyes, only mirrors
#define RF_WEAPONMODEL      BIT( 2 ) // 4       // only draw through eyes
#define RF_FULLBRIGHT       BIT( 3 ) // 8       // allways draw full intensity
#define RF_DEPTHHACK        BIT( 4 ) // 16      // for view weapon Z crunching
#define RF_TRANSLUCENT      BIT( 5 ) // 32
#define RF_FRAMELERP        BIT( 6 ) // 64
#define RF_BEAM             BIT( 7 ) // 128
#define RF_CUSTOMSKIN       BIT( 8 ) // 256     // skin is an index in image_precache
#define RF_GLOW             BIT( 9 ) // 512     // pulse lighting for bonus items
#define RF_SHELL_RED        BIT( 10 ) // 1024
#define RF_SHELL_GREEN      BIT( 11 ) // 2048
#define RF_SHELL_BLUE       BIT( 12 ) // 4096
#define RF_NOSHADOW         BIT( 13 ) // 8192    // <Q2RTXP>: WID: Will have the entity(alias or brush model) not cast any shadow.
#define	RF_OLD_FRAME_LERP	BIT( 14 ) // 16384	// [Paril-KEX] force model to lerp from oldframe in entity state; otherwise it uses last frame client received
#define RF_STAIR_STEP		BIT( 15 ) // 32768	// [Paril-KEX] re-tuned, now used to handle stair steps for monsters

//ROGUE
#define RF_IR_VISIBLE       BIT( 16 ) // 0x00008000
#define RF_SHELL_DOUBLE     BIT( 17 ) // 0x00010000      // 65536
#define RF_SHELL_HALF_DAM   BIT( 18 ) // 0x00020000
#define RF_USE_DISGUISE     BIT( 19 ) // 0x00040000
//#define RF_IR_VISIBLE       0x00008000      // 32768
//#define RF_SHELL_DOUBLE     0x00010000      // 65536
//#define RF_SHELL_HALF_DAM   0x00020000
//#define RF_USE_DISGUISE     0x00040000
//ROGUE

// <Q2RTXP>: WID: 
#define RF_BRUSHTEXTURE_SET_FRAME_INDEX	BIT( 20 ) // Will NOT automatically proceed to the 'next' frame.