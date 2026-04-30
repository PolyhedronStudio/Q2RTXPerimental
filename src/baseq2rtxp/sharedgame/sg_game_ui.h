/********************************************************************
*
*
*	SharedGame: GUI Menu IDs
*
*
********************************************************************/


enum {
	//! Default menu ID for when no menu is active. If you ever get this value, something is badly wrong.
	GAME_UI_MENU_ID_NONE = 0,

	//! Menu to test with.
	GAME_UI_MENU_ID_TEST,

	//! Menu to choose your team with in team-based gamemodes.
	GAME_UI_MENU_ID_TEAM,
	//! Menu to buy weapons and items with in objective-based gamemodes.
	GAME_UI_MENU_ID_ARMORY,
};