/********************************************************************
*
*
*	SharedGame: GUI Menu IDs
*
*
********************************************************************/

/**
*	@brief	Enumerator for GUI menu IDs. These are used to identify which menu is currently active, 
*			and to open specific menus when requested. Only a single menu can be active at the same time,
*			though it can consist out of multiple Game UI windows of course.
**/
enum class game_ui_menu_id {
	//! Default menu ID for when no menu is active. If you ever get this value, something is badly wrong.
	NONE = 0,


	//! [Unimplemented] Menu to choose your team with in team-based gamemodes.
	TEAM,
	//! [Unimplemented] Menu to buy weapons and items with in objective-based gamemodes.
	ARMORY,

	//! Menu to test with.
	TEST,

	// Maximum number of menu IDs, must be last in the enum.
	MAX_ID
};