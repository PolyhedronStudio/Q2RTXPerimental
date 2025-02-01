/********************************************************************
*
*
*	Shared: User Interface.
*
*
********************************************************************/
#pragma once



#define CHAR_WIDTH  8
#define CHAR_HEIGHT 8

#define UI_LEFT             BIT( 0 )	// 0x00000001
#define UI_RIGHT            BIT( 1 )	// 0x00000002
#define UI_CENTER           (UI_LEFT | UI_RIGHT)
#define UI_BOTTOM           BIT( 2 )	// 0x00000004
#define UI_TOP              BIT( 3 )	// 0x00000008
#define UI_MIDDLE           (UI_BOTTOM | UI_TOP)
#define UI_DROPSHADOW       BIT( 4 )	// 0x00000010
#define UI_ALTCOLOR         BIT( 5 )	// 0x00000020
#define UI_IGNORECOLOR      BIT( 6 )	// 0x00000040
#define UI_XORCOLOR         BIT( 7 )	// 0x00000080
#define UI_AUTOWRAP         BIT( 8 )	// 0x00000100
#define UI_MULTILINE        BIT( 9 )	// 0x00000200
#define UI_DRAWCURSOR       BIT( 10 )	// 0x00000400