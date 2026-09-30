/********************************************************************
*
*
*	ClientGame: Key and Command Handling for HUD Weapon Selector.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_keys.h"
#include "clgame/hud/clg_hud_weaponmenu.h"
#include "shared/keys/key_numbers.h"

/**
*	Console Command Handlers:
**/
static void CLG_Cmd_Slot1_f( void ) { CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_MELEE ); }
static void CLG_Cmd_Slot2_f( void ) { CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_PISTOLS ); }
static void CLG_Cmd_Slot3_f( void ) { CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_SMG ); }
static void CLG_Cmd_Slot4_f( void ) { CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_RIFLES ); }
static void CLG_Cmd_Slot5_f( void ) { CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_GRENADES ); }
static void CLG_Cmd_Slot6_f( void ) { CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_EXPLOSIVES ); }
static void CLG_Cmd_Slot7_f( void ) { CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_OTHER ); }

static void CLG_Cmd_CancelSelect_f( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		CLG_WeaponMenu_Close();
	}
}

static void CLG_Cmd_SlotPrev_f( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		CLG_WeaponMenu_PrevSlot();
	}
}

static void CLG_Cmd_SlotNext_f( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		CLG_WeaponMenu_NextSlot();
	}
}

static void CLG_Cmd_SlotConfirm_f( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		CLG_WeaponMenu_Confirm();
	}
}

/**
*	@brief	Initialize ClientGame weapon selection console commands (slot1..slot7, cancelselect, etc.).
**/
void CLG_Keys_Init( void ) {
	/**
	*	Register slot1..slot7 and menu control commands.
	**/
	clgi.Cmd_AddCommand( "slot1", CLG_Cmd_Slot1_f );
	clgi.Cmd_AddCommand( "slot2", CLG_Cmd_Slot2_f );
	clgi.Cmd_AddCommand( "slot3", CLG_Cmd_Slot3_f );
	clgi.Cmd_AddCommand( "slot4", CLG_Cmd_Slot4_f );
	clgi.Cmd_AddCommand( "slot5", CLG_Cmd_Slot5_f );
	clgi.Cmd_AddCommand( "slot6", CLG_Cmd_Slot6_f );
	clgi.Cmd_AddCommand( "slot7", CLG_Cmd_Slot7_f );

	clgi.Cmd_AddCommand( "cancelselect", CLG_Cmd_CancelSelect_f );
	clgi.Cmd_AddCommand( "slot_prev", CLG_Cmd_SlotPrev_f );
	clgi.Cmd_AddCommand( "slot_next", CLG_Cmd_SlotNext_f );
	clgi.Cmd_AddCommand( "slot_confirm", CLG_Cmd_SlotConfirm_f );
}

/**
*	@brief	Shutdown and remove ClientGame console commands.
**/
void CLG_Keys_Shutdown( void ) {
	/**
	*	Remove registered commands.
	**/
	clgi.Cmd_RemoveCommand( "slot1" );
	clgi.Cmd_RemoveCommand( "slot2" );
	clgi.Cmd_RemoveCommand( "slot3" );
	clgi.Cmd_RemoveCommand( "slot4" );
	clgi.Cmd_RemoveCommand( "slot5" );
	clgi.Cmd_RemoveCommand( "slot6" );
	clgi.Cmd_RemoveCommand( "slot7" );

	clgi.Cmd_RemoveCommand( "cancelselect" );
	clgi.Cmd_RemoveCommand( "slot_prev" );
	clgi.Cmd_RemoveCommand( "slot_next" );
	clgi.Cmd_RemoveCommand( "slot_confirm" );
}

/**
*	@brief	Key event handler called from engine Key_Event during KEY_GAME.
*	@param	key		Key code (K_ESCAPE, '1'..'7', K_UPARROW, K_DOWNARROW, K_ENTER, 'e', etc.).
*	@param	down	True if key was pressed, false if released.
*	@return	True if consumed by ClientGame, false if passed through to engine.
**/
const bool CLG_Keys_KeyEvent( const int32_t key, const bool down ) {
	/**
	*	State 1: Weapon selection menu is currently open.
	**/
	if ( CLG_WeaponMenu_IsOpen() ) {
		// Escape or Backspace cancels/closes the selection menu.
		if ( key == K_ESCAPE || key == K_BACKSPACE ) {
			if ( down ) {
				CLG_WeaponMenu_Close();
			}
			return true; // Consumed: prevents pause menu from opening.
		}

		// Up Arrow moves to previous slot.
		if ( key == K_UPARROW ) {
			if ( down ) {
				CLG_WeaponMenu_PrevSlot();
			}
			return true;
		}

		// Down Arrow moves to next slot.
		if ( key == K_DOWNARROW ) {
			if ( down ) {
				CLG_WeaponMenu_NextSlot();
			}
			return true;
		}

		// Enter or 'E' confirms the selection and switches weapon.
		if ( key == K_ENTER || key == 'e' || key == 'E' ) {
			if ( down ) {
				CLG_WeaponMenu_Confirm();
			}
			return true;
		}

		// Numeric keys 1..7 cycle slots within that category or switch category.
		if ( key >= '1' && key <= '7' ) {
			if ( down ) {
				const weapon_category_t cat = static_cast<weapon_category_t>( key - '0' );
				CLG_WeaponMenu_SelectCategory( cat );
			}
			return true;
		}

		// Allow other keys (WASD, mouse attack, jump) to flow naturally to gameplay.
		return false;
	}

	/**
	*	State 2: Weapon selection menu is closed.
	*	Numeric keys '1'..'7' trigger opening the respective category.
	**/
	if ( key >= '1' && key <= '7' ) {
		if ( down ) {
			const weapon_category_t cat = static_cast<weapon_category_t>( key - '0' );
			CLG_WeaponMenu_SelectCategory( cat );
		}
		return true; // Consumed: activates weapon selection HUD.
	}

	// Not intercepted; let normal key binding dispatch execute.
	return false;
}
