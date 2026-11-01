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
*	@brief	Initialize ClientGame key event handling.
**/
void CLG_Keys_Init( void ) {
	// Initialization tasks if any needed for key handling.
}

/**
*	@brief	Shutdown and remove ClientGame key event handling.
**/
void CLG_Keys_Shutdown( void ) {
	// Shutdown tasks if any needed for key handling.
}

/**
*	@brief	Key event handler called from engine Key_Event during KEY_GAME.
*	@param	key		Key code (K_ESCAPE, etc.).
*	@param	down	True if key was pressed, false if released.
*	@return	True if consumed by ClientGame, false if passed through to engine.
*	@note	Escape is the only hardcoded button that cancels and closes the weapon
*			menu without going through game action bindings (preventing engine menu).
*			All other actions (fire, secondary fire, slots, navigation) are routed
*			through configurable game action bindings.
**/
const bool CLG_Keys_KeyEvent( const int32_t key, const bool down ) {
	/**
	*	Weapon selection menu is open:
	*	Only K_ESCAPE is intercepted here so the engine pause menu is not opened.
	**/
	if ( CLG_WeaponMenu_IsOpen() ) {
		// Escape cancels/closes the selection menu.
		if ( key == K_ESCAPE ) {
			if ( down ) {
				CLG_WeaponMenu_Close();
			}
			return true; // Consumed: closes menu and prevents pause menu from opening.
		}

		// Allow all other keys to pass through to engine Key_DispatchBinding.
		return false;
	}

	// Menu is closed: not intercepted.
	return false;
}

