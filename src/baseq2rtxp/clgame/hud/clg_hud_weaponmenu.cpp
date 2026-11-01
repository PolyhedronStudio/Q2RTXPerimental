/********************************************************************
*
*
*	ClientGame: Half-Life 1 Style Weapon Selection Menu Implementation.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_screen.h"
#include "clgame/clg_precache.h"
#include "clgame/hud/clg_hud_weaponmenu.h"
#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_items.h"
#include "shared/math/qm_easing_methods.hpp"
#include "shared/math/qm_easing_state.hpp"

#include <string>
#include <vector>


/**
*
*
*	Weapon Bar user input Movement Intersceptors:
*
*
**/
/**
*	@brief	When +forward button is pressed down.
*	@note	If the HUD weapon selector menu is open, only the UPARROW key navigates to the previous slot.
*			Any other key (such as 'W') is ignored by the menu and forwarded to normal movement as usual.
**/
const bool IN_WeaponBar_ForwardDown( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		const int32_t key = atoi( clgi.Cmd_Argv( 1 ) );
		if ( key == K_UPARROW || key == K_KP_UPARROW ) {
			// Navigate to the previous slot in the weapon menu.
			CLG_WeaponMenu_PrevSlot();
			// We consume the key event so it is not forwarded to normal movement, which would cause the player to move backwards.
			return true;
		}
	}

	// Forward to normal movement when weapon menu is not open, or when the key is not UPARROW.
	return false;
}

/**
*	@brief	When +forward button is released (up).
**/
const bool IN_WeaponBar_ForwardUp( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		const int32_t key = atoi( clgi.Cmd_Argv( 1 ) );
		if ( key == K_UPARROW || key == K_KP_UPARROW ) {
			// We consume the key event so it is not forwarded to normal movement, which would cause the player to move backwards.
			return true;
		}
	}

	// Forward to normal movement when weapon menu is not open, or when the key is not UPARROW.
	return false;
}

/**
*	@brief	When +back button is pressed down.
*	@note	If the HUD weapon selector menu is open, only the DOWNARROW key navigates to the next slot.
*			Any other key (such as 'S') is ignored by the menu and forwarded to normal movement as usual.
**/
const bool IN_WeaponBar_BackDown( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		const int32_t key = atoi( clgi.Cmd_Argv( 1 ) );
		if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
			// Navigate to the next slot in the weapon menu.
			CLG_WeaponMenu_NextSlot();
			// We consume the key event so it is not forwarded to normal movement, which would cause the player to move backwards.
			return true;
		}
	}

	// Forward to normal movement when weapon menu is not open, or when the key is not UPARROW.
	return false;
}

/**
*	@brief	When +back button is released (up).
**/
const bool IN_WeaponBar_BackUp( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		const int32_t key = atoi( clgi.Cmd_Argv( 1 ) );
		if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
			// We consume the key event so it is not forwarded to normal movement, which would cause the player to move backwards.
			return true;
		}
	}
	// Forward to normal movement when weapon menu is not open, or when the key is not DOWNARROW.
	return false;
}


/**
*
*
*	Weaponbar Console Command Handlers:
*
*
**/
/**
*	@brief	Category selection commands for weapon slots 1 through 7.
**/
static void CLG_Cmd_Slot1_f( void ) {
	CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_MELEE );
}

static void CLG_Cmd_Slot2_f( void ) {
	CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_PISTOLS );
}

static void CLG_Cmd_Slot3_f( void ) {
	CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_SMG );
}

static void CLG_Cmd_Slot4_f( void ) {
	CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_RIFLES );
}

static void CLG_Cmd_Slot5_f( void ) {
	CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_GRENADES );
}

static void CLG_Cmd_Slot6_f( void ) {
	CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_EXPLOSIVES );
}

static void CLG_Cmd_Slot7_f( void ) {
	CLG_WeaponMenu_SelectCategory( WEAPON_CATEGORY_OTHER );
}

/**
*	@brief	Button-up release stub handler.
*	@note	Consumes button release events (+wpbar_slotX -> -wpbar_slotX) on the client
*			so they are never forwarded to the server as unhandled commands.
**/
static void CLG_Cmd_Wpbar_Up_Stub_f( void ) {
	// Consumed client-side: prevents unregistered command forwarding to server.
}

/**
*	@brief	Previous weapon menu item navigation command callback (+wpbar_prev / wpbar_prev / slot_prev).
*	@note	Opens the weapon menu if closed and steps to the previous item so it becomes active and ready for selection.
**/
static void CLG_Cmd_SlotPrev_f( void ) {
	// Navigate to the previous item in the weapon menu (opening the menu if not already open).
	CLG_WeaponMenu_PrevSlot();
}

/**
*	@brief	Next weapon menu item navigation command callback (+wpbar_next / wpbar_next / slot_next).
*	@note	Opens the weapon menu if closed and steps to the next item so it becomes active and ready for selection.
**/
static void CLG_Cmd_SlotNext_f( void ) {
	// Navigate to the next item in the weapon menu (opening the menu if not already open).
	CLG_WeaponMenu_NextSlot();
}

/**
*	@brief	Selection confirmation command callback.
**/
static void CLG_Cmd_SlotConfirm_f( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		CLG_WeaponMenu_Confirm();
	}
}

/**
*	@brief	Selection cancel command callback.
**/
static void CLG_Cmd_CancelSelect_f( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		CLG_WeaponMenu_Close();
	}
}

/**
*	@brief	Previous weapon command callback (weapprev).
*	@note	Operates the client-side weapon selection menu (never forwarded to server).
**/
static void CLG_Cmd_WeapPrev_f( void ) {
	// Navigate to the previous item in the weapon menu (or open on active weapon if closed).
	CLG_WeaponMenu_PrevSlot();
}

/**
*	@brief	Next weapon command callback (weapnext).
*	@note	Operates the client-side weapon selection menu (never forwarded to server).
**/
static void CLG_Cmd_WeapNext_f( void ) {
	// Navigate to the next item in the weapon menu (or open on active weapon if closed).
	CLG_WeaponMenu_NextSlot();
}

/**
*	@brief	Inventory use command callback (e.g. Enter / 'invuse').
*	@note	Confirms active selection when open; stubs when closed to prevent unhandled chat forwarding.
**/
static void CLG_Cmd_InvUse_f( void ) {
	if ( CLG_WeaponMenu_IsOpen() ) {
		CLG_WeaponMenu_Confirm();
	}
}

/**
*	@brief	Inventory navigation command callbacks.
*	@note	Operates the client-side weapon selection menu (never forwarded to server).
**/
static void CLG_Cmd_InvPrev_f( void ) {
	// Navigate to the previous item in the weapon menu (or open on active weapon if closed).
	CLG_WeaponMenu_PrevSlot();
}

static void CLG_Cmd_InvNext_f( void ) {
	// Navigate to the next item in the weapon menu (or open on active weapon if closed).
	CLG_WeaponMenu_NextSlot();
}

/**
*	@brief	Inventory drop command stub to prevent unhandled command forwarding to server.
**/
static void CLG_Cmd_InvDrop_Stub_f( void ) {
	// Consumed client-side.
}

/**
*	@brief	Weaponbar command bindings that might need intercepting for the Half-Life 1 style weapon selection menu. 
*	@note	These are client-side commands that trigger menu behavior and filter out stubs from reaching the server.
**/
cmdreg_t clg_weaponbar_cmds[] = {
	// Category selection: slot1..slot7 and +wpbar_slot1..+wpbar_slot7
	{ "slot1", CLG_Cmd_Slot1_f },
	{ "+wpbar_slot1", CLG_Cmd_Slot1_f },
	{ "-wpbar_slot1", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot2", CLG_Cmd_Slot2_f },
	{ "+wpbar_slot2", CLG_Cmd_Slot2_f },
	{ "-wpbar_slot2", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot3", CLG_Cmd_Slot3_f },
	{ "+wpbar_slot3", CLG_Cmd_Slot3_f },
	{ "-wpbar_slot3", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot4", CLG_Cmd_Slot4_f },
	{ "+wpbar_slot4", CLG_Cmd_Slot4_f },
	{ "-wpbar_slot4", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot5", CLG_Cmd_Slot5_f },
	{ "+wpbar_slot5", CLG_Cmd_Slot5_f },
	{ "-wpbar_slot5", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot6", CLG_Cmd_Slot6_f },
	{ "+wpbar_slot6", CLG_Cmd_Slot6_f },
	{ "-wpbar_slot6", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot7", CLG_Cmd_Slot7_f },
	{ "+wpbar_slot7", CLG_Cmd_Slot7_f },
	{ "-wpbar_slot7", CLG_Cmd_Wpbar_Up_Stub_f },

	// Navigation & confirmation commands
	{ "slot_prev", CLG_Cmd_SlotPrev_f },
	{ "wpbar_prev", CLG_Cmd_SlotPrev_f },
	{ "+wpbar_prev", CLG_Cmd_SlotPrev_f },
	{ "-wpbar_prev", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot_next", CLG_Cmd_SlotNext_f },
	{ "wpbar_next", CLG_Cmd_SlotNext_f },
	{ "+wpbar_next", CLG_Cmd_SlotNext_f },
	{ "-wpbar_next", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "slot_confirm", CLG_Cmd_SlotConfirm_f },
	{ "+wpbar_confirm", CLG_Cmd_SlotConfirm_f },
	{ "-wpbar_confirm", CLG_Cmd_Wpbar_Up_Stub_f },

	{ "cancelselect", CLG_Cmd_CancelSelect_f },
	{ "+wpbar_cancel", CLG_Cmd_CancelSelect_f },
	{ "-wpbar_cancel", CLG_Cmd_Wpbar_Up_Stub_f },

	// Weapon scrolling interception
	{ "weapprev", CLG_Cmd_WeapPrev_f },
	{ "weapnext", CLG_Cmd_WeapNext_f },

	// Client-side stubs for legacy/unhandled commands to filter out and prevent reaching server
	{ "invuse", CLG_Cmd_InvUse_f },
	{ "invprev", CLG_Cmd_InvPrev_f },
	{ "invnext", CLG_Cmd_InvNext_f },
	{ "invdrop", CLG_Cmd_InvDrop_Stub_f },
	{ "inven_drop", CLG_Cmd_InvDrop_Stub_f },

	{ nullptr, nullptr }
};

/**
*	Rapid fade-in / fade-out easing duration for the weapon selector bar and category dropdowns.
**/
//! Rapid ease duration (120ms) so menu transitions feel smooth yet responsive and immediate.
static constexpr QMTime WEAPON_MENU_EASE_DURATION = 120_ms;

/**
*	@brief	Tracks smooth fade-in and fade-out alpha transitions using QMEaseState.
**/
struct clg_weapon_menu_fader_t {
	//! Current evaluated alpha value in [0.0, 1.0].
	double		currentAlpha = 0.0;
	//! Starting alpha when the active ease transition was initiated.
	double		startAlpha = 0.0;
	//! Target alpha (1.0 when fading in, 0.0 when fading out).
	double		targetAlpha = 0.0;
	//! Underlying QMEaseState tracking start time and duration.
	QMEaseState	easeState = {};

	/**
	*	@brief	Initiate a smooth fade-in towards alpha 1.0 from the current alpha.
	*	@param	realTime	Current client real-time timestamp.
	*	@param	duration	Total fade-in duration.
	**/
	void StartFadeIn( const QMTime &realTime, const QMTime &duration ) {
		/**
		*	If already targeting 1.0 and either actively easing or completed, keep current transition.
		**/
		if ( targetAlpha >= 1.0 && ( easeState.GetEaseMode() != QMEaseState::QM_EASE_STATE_MODE_DONE || currentAlpha >= 1.0 ) ) {
			return;
		}
		// Capture current alpha as the starting point to avoid visual pops on interrupted transitions.
		startAlpha = currentAlpha;
		// Set target alpha to fully opaque.
		targetAlpha = 1.0;
		// Initialize new easing state.
		easeState = QMEaseState::new_ease_state( realTime, duration );
	}

	/**
	*	@brief	Initiate a smooth fade-out towards alpha 0.0 from the current alpha.
	*	@param	realTime	Current client real-time timestamp.
	*	@param	duration	Total fade-out duration.
	**/
	void StartFadeOut( const QMTime &realTime, const QMTime &duration ) {
		/**
		*	If already targeting 0.0 and either actively easing or completed, keep current transition.
		**/
		if ( targetAlpha <= 0.0 && ( easeState.GetEaseMode() != QMEaseState::QM_EASE_STATE_MODE_DONE || currentAlpha <= 0.0 ) ) {
			return;
		}
		// Capture current alpha as the starting point to avoid visual pops on interrupted transitions.
		startAlpha = currentAlpha;
		// Set target alpha to fully transparent.
		targetAlpha = 0.0;
		// Initialize new easing state.
		easeState = QMEaseState::new_ease_state( realTime, duration );
	}

	/**
	*	@brief	Evaluate and update the current normalized alpha for the given real-time timestamp.
	*	@param	realTime	Current client real-time timestamp.
	*	@return	Evaluated alpha factor in [0.0, 1.0].
	**/
	const double Evaluate( const QMTime &realTime ) {
		/**
		*	Evaluate ease-out curve for both fade-in and fade-out so initial response is immediate.
		**/
		// Check if there is a difference between start and target alpha.
		if ( std::fabs( targetAlpha - startAlpha ) > 0.0001 ) {
			// Evaluate quadratic ease-out factor for snappy, responsive menu motion.
			const double easeFactor = easeState.EaseOut( realTime, QM_QuadraticEaseOut<double> );
			// Interpolate between startAlpha and targetAlpha and clamp to [0.0, 1.0].
			currentAlpha = QM_Clamp( QM_Lerp<double>( startAlpha, targetAlpha, easeFactor ), 0.0, 1.0 );
		} else {
			// Already at target alpha.
			currentAlpha = targetAlpha;
		}
		// Return evaluated alpha.
		return currentAlpha;
	}
};

/**
*	Weapon selector menu internal state.
**/
struct clg_weapon_menu_state_t {
	//! True when the HUD selector overlay is active and intercepting menu input.
	bool						isOpen = false;
	//! The currently expanded category (1..7).
	weapon_category_t			activeCategory = WEAPON_CATEGORY_NONE;
	//! The active highlighted item slot within the category (0..N-1).
	int32_t						activeSlotIndex = 0;
	//! Real-time timestamp when menu will auto-close due to inactivity.
	QMTime						autoCloseTime = 0_ms;
	//! Overall weapon bar fade-in / fade-out easing state.
	clg_weapon_menu_fader_t		menuFader = {};
	//! Per-category dropdown fade-in / fade-out easing states (1-indexed: 1..WEAPON_CATEGORY_COUNT).
	clg_weapon_menu_fader_t		categoryFaders[ WEAPON_CATEGORY_COUNT + 1 ] = {};
	//! Last highlighted slot index per category so fading-out categories preserve their selection visual.
	int32_t						categoryLastSlot[ WEAPON_CATEGORY_COUNT + 1 ] = {};
};

//! Static weapon menu state instance.
static clg_weapon_menu_state_t s_wpn_menu = {};

//! Category display titles matching indices 1..7.
static const char *s_categoryNames[ WEAPON_CATEGORY_COUNT + 1 ] = {
	"",
	"1. MELEE",
	"2. PISTOLS",
	"3. SMG",
	"4. RIFLES",
	"5. GRENADES",
	"6. EXPLOSIVES",
	"7. OTHER"
};

/**
*	Palette Definitions:
*	Primary HL1 Orange: #df7126 (223, 113, 38)
*	Warning Red:        #d95763 (217, 87, 99)
**/
// For owned and inactive inventory items, we use a more opaque background and stroke to indicate availability.
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BRIGHT	= MakeColor( 255, 175, 75, 255 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BASE		= MakeColor( 210, 125, 44, 220 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_GLOW		= MakeColor( 210, 125, 44, 180 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_DIM		= MakeColor( 210, 125, 44, 120 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BG		= MakeColor( 35, 18, 8, 220 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BG_DIM	= MakeColor( 25, 16, 10, 160 );
// For unowned weapons, we use a more translucent background and stroke to indicate unavailability.
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_UNOWNED_BG		= MakeColor( 14, 10, 8, 120 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_UNOWNED_STROKE	= MakeColor( 80, 50, 30, 80 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_UNOWNED_TEXT		= MakeColor( 100, 70, 45, 120 );

/**
*	Palette Definitions for Half-Life 1 Inspired Bottom HUD (#d27d2c):
**/
//! Primary base amber/orange color (#d27d2c).
static constexpr uint32_t COLOR_HUD_ORANGE_BASE = MakeColor( 210, 125, 44, 220 );
//! Bright amber/orange color for pulse peaks and high visibility.
static constexpr uint32_t COLOR_HUD_ORANGE_BRIGHT = MakeColor( 223, 113, 38, 255 );
//! Amber/orange glow color.
static constexpr uint32_t COLOR_HUD_ORANGE_GLOW = MakeColor( 223, 113, 38, 200 );
//! Dim amber/orange color for subtle border strokes.
static constexpr uint32_t COLOR_HUD_ORANGE_DIM = MakeColor( 223, 113, 38, 140 );
//! Dark translucent amber background container fill.
static constexpr uint32_t COLOR_HUD_BG = MakeColor( 35, 18, 8, 220 );

static constexpr uint32_t COLOR_RED_DISABLED		= MakeColor( 217, 87, 99, 230 );
static constexpr uint32_t COLOR_RED_DISABLED_GLOW	= MakeColor( 217, 87, 99, 180 );
static constexpr uint32_t COLOR_RED_DISABLED_DIM	= MakeColor( 217, 87, 99, 120 );
static constexpr uint32_t COLOR_RED_DISABLED_BG		= MakeColor( 48, 16, 16, 220 );
static constexpr uint32_t COLOR_RED_DISABLED_BG_DIM	= MakeColor( 24, 10, 10, 160 );

/**
*	@brief	Scales the alpha channel of a packed 32-bit RGBA color by a normalized factor [0.0, 1.0].
*	@param	color		Source 32-bit packed RGBA color.
*	@param	alphaScale	Normalized alpha multiplier in [0.0, 1.0].
*	@return	Packed 32-bit RGBA color with scaled alpha channel.
**/
static inline uint32_t ScaleColorAlpha( const uint32_t color, const double alphaScale ) {
	// Clamp scale factor to [0.0, 1.0].
	const double clampedScale = QM_Clamp( alphaScale, 0.0, 1.0 );
	// Extract RGB channels and scale the alpha channel.
	const uint32_t r = ( color >> 0 ) & 0xFF;
	const uint32_t g = ( color >> 8 ) & 0xFF;
	const uint32_t b = ( color >> 16 ) & 0xFF;
	const uint32_t a = static_cast<uint32_t>( Q_rint( static_cast<double>( ( color >> 24 ) & 0xFF ) * clampedScale ) );
	// Return repacked 32-bit color.
	return MakeColor( r, g, b, QM_Clamp<uint32_t>( a, 0U, 255U ) );
}

/**
*	@brief	Linearly interpolates between two 32-bit packed RGBA colors.
*	@param	colorA	Starting 32-bit packed RGBA color (at factor 0.0).
*	@param	colorB	Ending 32-bit packed RGBA color (at factor 1.0).
*	@param	factor	Interpolation factor in [0.0, 1.0].
*	@return	Interpolated 32-bit packed RGBA color.
**/
static inline uint32_t LerpColorRGBA( const uint32_t colorA, const uint32_t colorB, const double factor ) {
	// Clamp interpolation factor to [0.0, 1.0].
	const double t = QM_Clamp( factor, 0.0, 1.0 );
	// Compute inverse factor for linear blend.
	const double invT = 1.0 - t;

	// Interpolate each channel independently.
	const uint32_t r = static_cast<uint32_t>( Q_rint( ( static_cast<double>( ( colorA >> 0 ) & 0xFF ) * invT ) + ( static_cast<double>( ( colorB >> 0 ) & 0xFF ) * t ) ) );
	const uint32_t g = static_cast<uint32_t>( Q_rint( ( static_cast<double>( ( colorA >> 8 ) & 0xFF ) * invT ) + ( static_cast<double>( ( colorB >> 8 ) & 0xFF ) * t ) ) );
	const uint32_t b = static_cast<uint32_t>( Q_rint( ( static_cast<double>( ( colorA >> 16 ) & 0xFF ) * invT ) + ( static_cast<double>( ( colorB >> 16 ) & 0xFF ) * t ) ) );
	const uint32_t a = static_cast<uint32_t>( Q_rint( ( static_cast<double>( ( colorA >> 24 ) & 0xFF ) * invT ) + ( static_cast<double>( ( colorB >> 24 ) & 0xFF ) * t ) ) );

	// Return repacked 32-bit RGBA color.
	return MakeColor(
		QM_Clamp<uint32_t>( r, 0U, 255U ),
		QM_Clamp<uint32_t>( g, 0U, 255U ),
		QM_Clamp<uint32_t>( b, 0U, 255U ),
		QM_Clamp<uint32_t>( a, 0U, 255U )
	);
}

/**
*	@brief	Draws a weapon menu string with alpha-scaled text fill and font stroke outline.
*	@param	x			Horizontal draw coordinate.
*	@param	y			Vertical draw coordinate.
*	@param	maxlen		Maximum character count to render.
*	@param	str			Null-terminated string to render.
*	@param	textColor	Base RGBA text color (prior to alphaScale).
*	@param	alphaScale	Normalized alpha multiplier in [0.0, 1.0].
**/
static void DrawWeaponMenuString( const int32_t x, const int32_t y, const size_t maxlen, const char *str, const uint32_t textColor, const double alphaScale ) {
	/**
	*	Sanity check: ensure valid string and visible alpha.
	**/
	if ( !str || str[ 0 ] == '\0' || alphaScale <= 0.001 ) {
		return;
	}

	// Set alpha-scaled text color.
	clgi.R_SetColor( ScaleColorAlpha( textColor, alphaScale ) );

	// Apply alpha-scaled font outline stroke if enabled by cvar.
	if ( scr_font_stroke && scr_font_stroke->value > 0.0f ) {
		clgi.R_SetStroke( ScaleColorAlpha( scr_stroke_color.u32, alphaScale ), scr_font_stroke->value );
	}

	// Render string using the screen font.
	clgi.R_DrawString( x, y, 0, maxlen, str, precache.screen.font_pic );

	// Clean up style and color state.
	if ( scr_font_stroke && scr_font_stroke->value > 0.0f ) {
		clgi.R_ClearStyle();
	}
	clgi.R_ClearColor();
}

/**
*	@brief	Collect all weapons belonging to a category that the client currently owns.
*	@param	category	The weapon category (1..7).
*	@param	outWeapons	Buffer to receive pointers to owned weapon items.
*	@param	maxWeapons	Capacity of outWeapons.
*	@return	Number of owned weapons found.
**/
static int32_t GetOwnedWeaponsInCategory( const weapon_category_t category, const sg_item_t **outWeapons, const int32_t maxWeapons ) {
	/**
	*	Sanity checks: ensure valid output buffer and category.
	**/
	if ( !outWeapons || maxWeapons <= 0 || category <= WEAPON_CATEGORY_NONE || category > WEAPON_CATEGORY_COUNT ) {
		return 0;
	}

	// Temporary buffer to query all registered weapons in the category.
	const sg_item_t *catWeapons[ 16 ];
	// Get all weapons in the specified category (up to 16).
	const int32_t totalInCat = SG_Item_GetWeaponsInCategory( category, catWeapons, 16 );

	// Check if client state is available.
	if ( !clgi.client ) {
		// Client state unavailable: cannot determine owned weapons.
		return 0;
	}

	// Fetch owned weapons bitmask from synchronized player state stats.
	const uint64_t ownedMask = static_cast<uint64_t>( clgi.client->frame.ps.stats[ STAT_WEAPONS_OWNED ] );

	// Filter to weapons whose bit is set in the ownership mask.
	int32_t ownedCount = 0;
	// Iterate through all weapons in the category and check ownership.
	for ( int32_t i = 0; i < totalInCat; i++ ) {
		// Check if the weapon is owned by testing the corresponding bit in the ownedMask.
		const sg_item_t *weapon = catWeapons[ i ];
		// Ensure the weapon pointer is valid and has a valid weapon index.
		if ( ( ownedMask & ( 1ULL << weapon->weapon_index ) ) != 0 ) {
			// Weapon is owned: add to output buffer if there's space.
			if ( ownedCount < maxWeapons ) {
				// Store the pointer to the owned weapon in the output array.
				outWeapons[ ownedCount++ ] = weapon;
			}
		}
	}

	// Return the total number of owned weapons found in this category.
	return ownedCount;
}

/**
*	@brief	Helper to activate a category and slot while triggering rapid fade-in/out easing.
*	@param	newCategory		Target weapon category (1..7) to open.
*	@param	newSlotIndex	Target weapon slot index within the category (0..N-1).
**/
static void ActivateCategoryAndSlot( const weapon_category_t newCategory, const int32_t newSlotIndex ) {
	// Fetch current real-time timestamp for easing transitions.
	const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

	/**
	*	Ensure the overall weapon bar menu is open and fading in.
	**/
	s_wpn_menu.isOpen = true;
	s_wpn_menu.menuFader.StartFadeIn( realTime, WEAPON_MENU_EASE_DURATION );

	/**
	*	If switching from a different active category, fade out the old category dropdown.
	**/
	if ( s_wpn_menu.activeCategory != WEAPON_CATEGORY_NONE && s_wpn_menu.activeCategory != newCategory ) {
		// Start fade-out ease on the previously active category.
		s_wpn_menu.categoryFaders[ s_wpn_menu.activeCategory ].StartFadeOut( realTime, WEAPON_MENU_EASE_DURATION );
	}

	/**
	*	Set the new active category and slot, and trigger fade-in on the new category dropdown.
	**/
	s_wpn_menu.activeCategory = newCategory;
	s_wpn_menu.activeSlotIndex = newSlotIndex;
	s_wpn_menu.categoryLastSlot[ newCategory ] = newSlotIndex;
	s_wpn_menu.categoryFaders[ newCategory ].StartFadeIn( realTime, WEAPON_MENU_EASE_DURATION );

	// Reset auto-close timer to 4 seconds from now.
	s_wpn_menu.autoCloseTime = realTime + 4_sec;
}

/**
*	@brief	Initialize weapon selection HUD menu resources and state.
**/
void CLG_WeaponMenu_Init( void ) {
	/**
	*	Reset state to closed defaults.
	**/
	s_wpn_menu = {};
}

/**
*	@brief	Shutdown weapon selection HUD menu.
**/
void CLG_WeaponMenu_Shutdown( void ) {
	/**
	*	Close and clear state on shutdown.
	**/
	// Close the menu and reset state.
	s_wpn_menu = {};
}

/**
*	@brief	Per-frame update: handles auto-close countdown and death resets.
**/
void CLG_WeaponMenu_Update( void ) {
	/**
	*	Only update auto-close and death checks when menu is open.
	**/
	if ( !s_wpn_menu.isOpen ) {
		// Menu is closed: nothing to update.
		return;
	}

	// Check if client state is available.
	if ( !clgi.client ) {
		// Client state unavailable: close the menu.
		CLG_WeaponMenu_Close();
		return;
	}

	// Close menu immediately if the player is dead or in spectator/intermission.
	if ( clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 0 || clgi.client->frame.ps.pmove.pm_type >= PM_INTERMISSION ) {
		// Player is dead or in intermission: close the menu.
		CLG_WeaponMenu_Close();
		return;
	}

	// Check if the auto-close timer has expired.
	const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );
	// If the current real time exceeds the auto-close time, close the menu.
	if ( realTime >= s_wpn_menu.autoCloseTime ) {
		// Auto-close timer expired: close the menu.
		CLG_WeaponMenu_Close();
	}
}

/**
*	@brief	Select or cycle weapons in the specified category (1..7).
*	@param	category	The category requested (WEAPON_CATEGORY_MELEE..WEAPON_CATEGORY_OTHER).
**/
void CLG_WeaponMenu_SelectCategory( const weapon_category_t category ) {
	/**
	*	Sanity check: validate category range.
	**/
	if ( category <= WEAPON_CATEGORY_NONE || category > WEAPON_CATEGORY_COUNT ) {
		return;
	}

	// Query owned weapons in this category.
	const sg_item_t *ownedWeapons[ 16 ];
	// Get the count of owned weapons in the category.
	const int32_t ownedCount = GetOwnedWeaponsInCategory( category, ownedWeapons, 16 );

	/**
	*	If the player does not possess any weapon in this category, play invalid sound.
	**/
	if ( ownedCount <= 0 ) {
		clgi.S_StartLocalSound( "weapons/pistol/noammo.wav" );
		return;
	}

	/**
	*	If menu was closed or a different category was selected, open and focus slot 0.
	**/
	if ( !s_wpn_menu.isOpen || s_wpn_menu.activeCategory != category ) {
		// Activate the requested category and focus its 1st weapon item (slot 0).
		ActivateCategoryAndSlot( category, 0 );
		// Play draw sound to indicate selection change.
		clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
	} else {
		/**
		*	Same category already active: cycle to the next sub-item (wrap around).
		**/
		// Increment slot index with wrap-around.
		const int32_t nextSlot = ( s_wpn_menu.activeSlotIndex + 1 ) % ownedCount;
		// Update active slot and refresh category fade-in / auto-close timer.
		ActivateCategoryAndSlot( category, nextSlot );
		// Play draw sound to indicate selection change.
		clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
	}
}

/**
*	@brief	Locate the category and slot index of the player's currently equipped weapon.
*	@param	outCategory		[out] Receives the category (1..7) containing the equipped weapon.
*	@param	outSlotIndex	[out] Receives the owned-weapon slot index within that category.
*	@return	True if the currently equipped weapon was found among owned weapons.
**/
static const bool FindEquippedWeaponCategoryAndSlot( weapon_category_t *outCategory, int32_t *outSlotIndex ) {
	/**
	*	Sanity checks: validate output pointers and client state.
	**/
	if ( !outCategory || !outSlotIndex || !clgi.client ) {
		return false;
	}

	// Fetch the currently equipped weapon index from synchronized player state stats.
	const int32_t equippedWeaponID = static_cast<int32_t>( clgi.client->frame.ps.stats[ STAT_WEAPON_ID ] );
	if ( equippedWeaponID <= 0 ) {
		return false;
	}

	/**
	*	Search all weapon categories (1..WEAPON_CATEGORY_COUNT) for the equipped weapon.
	**/
	for ( int32_t c = 1; c <= WEAPON_CATEGORY_COUNT; c++ ) {
		const weapon_category_t cat = static_cast<weapon_category_t>( c );
		const sg_item_t *ownedWeapons[ 16 ];
		const int32_t ownedCount = GetOwnedWeaponsInCategory( cat, ownedWeapons, 16 );

		// Inspect each owned weapon in this category.
		for ( int32_t s = 0; s < ownedCount; s++ ) {
			if ( ownedWeapons[ s ] && ownedWeapons[ s ]->weapon_index == equippedWeaponID ) {
				*outCategory = cat;
				*outSlotIndex = s;
				return true;
			}
		}
	}

	// Equipped weapon was not found in any owned category.
	return false;
}

/**
*	@brief	Open the weapon menu focused on the player's currently active weapon category and item index.
*	@return	True if an owned weapon was found and focused.
**/
static const bool OpenMenuOnActiveWeapon( void ) {
	/**
	*	Locate the player's currently equipped weapon category and slot index.
	**/
	weapon_category_t equippedCategory = WEAPON_CATEGORY_NONE;
	int32_t equippedSlotIndex = 0;
	if ( FindEquippedWeaponCategoryAndSlot( &equippedCategory, &equippedSlotIndex ) ) {
		// Open the weapon menu focused on the current active weapon category and item index.
		ActivateCategoryAndSlot( equippedCategory, equippedSlotIndex );
		clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
		return true;
	}

	/**
	*	Fallback if no active weapon is equipped: open the first category that has owned weapons at slot 0.
	**/
	for ( int32_t c = 1; c <= WEAPON_CATEGORY_COUNT; c++ ) {
		const weapon_category_t candidateCat = static_cast<weapon_category_t>( c );
		const sg_item_t *candidateWeapons[ 16 ];
		if ( GetOwnedWeaponsInCategory( candidateCat, candidateWeapons, 16 ) > 0 ) {
			ActivateCategoryAndSlot( candidateCat, 0 );
			clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
			return true;
		}
	}

	return false;
}

/**
*	@brief	Move selection up to the previous owned weapon in the weapon menu.
*	@note	If the menu is not open, opens it focused on the current active weapon
*			category and item index. If already open and at the end of the current
*			category list, closes the current category and opens the previous owned
*			category focusing on its 1st weapon item.
**/
void CLG_WeaponMenu_PrevSlot( void ) {
	/**
	*	Ensure client state is valid and the player is alive and not in intermission.
	**/
	if ( !clgi.client || clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 0 || clgi.client->frame.ps.pmove.pm_type >= PM_INTERMISSION ) {
		return;
	}

	/**
	*	If the weapon menu is not open, open it with focus on the current active weapon category and item index.
	**/
	if ( !s_wpn_menu.isOpen || s_wpn_menu.activeCategory <= WEAPON_CATEGORY_NONE || s_wpn_menu.activeCategory > WEAPON_CATEGORY_COUNT ) {
		OpenMenuOnActiveWeapon();
		return;
	}

	// Query owned weapons in the current active category.
	const weapon_category_t currentCategory = s_wpn_menu.activeCategory;
	const int32_t currentSlotIndex = s_wpn_menu.activeSlotIndex;
	const sg_item_t *ownedWeapons[ 16 ];
	// Get the count of owned weapons in the current category.
	const int32_t ownedCount = GetOwnedWeaponsInCategory( currentCategory, ownedWeapons, 16 );
	// If no owned weapons in the current category, close the menu.
	if ( ownedCount <= 0 ) {
		// Close the menu if no owned weapons are found.
		CLG_WeaponMenu_Close();
		return;
	}

	/**
	*	If there is a preceding weapon item in the current category, step up to it.
	**/
	if ( currentSlotIndex > 0 ) {
		// Move to the previous slot within the same category.
		ActivateCategoryAndSlot( currentCategory, currentSlotIndex - 1 );
		// Play draw sound to indicate selection change.
		clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
		return;
	}

	/**
	*	At the end of the current category list (index 0): search backwards for the
	*	previous category that has owned weapons, close the current category, and open
	*	that category focusing on its 1st weapon item element (index 0).
	**/
	weapon_category_t prevCategory = WEAPON_CATEGORY_NONE;
	const int32_t currentCatIndex = static_cast<int32_t>( currentCategory );
	// Iterate backwards through all categories with wrap-around.
	for ( int32_t offset = 1; offset <= WEAPON_CATEGORY_COUNT; offset++ ) {
		// Compute candidate category index in 1..WEAPON_CATEGORY_COUNT.
		const int32_t candidateIndex = ( ( currentCatIndex - 1 + WEAPON_CATEGORY_COUNT - offset ) % WEAPON_CATEGORY_COUNT ) + 1;
		const weapon_category_t candidateCat = static_cast<weapon_category_t>( candidateIndex );
		const sg_item_t *candidateWeapons[ 16 ];
		// Check if the player owns any weapons in this candidate category.
		if ( GetOwnedWeaponsInCategory( candidateCat, candidateWeapons, 16 ) > 0 ) {
			prevCategory = candidateCat;
			break;
		}
	}

	// If no owned weapons exist in any category, close the menu.
	if ( prevCategory == WEAPON_CATEGORY_NONE ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Close current category and open the target category focusing on its 1st weapon item (slot 0).
	ActivateCategoryAndSlot( prevCategory, 0 );
	// Play draw sound to indicate selection change.
	clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
}

/**
*	@brief	Move selection down to the next owned weapon in the weapon menu.
*	@note	If the menu is not open, opens it focused on the current active weapon
*			category and item index. If already open and at the end of the current
*			category list, closes the current category and opens the next owned
*			category focusing on its 1st weapon item.
**/
void CLG_WeaponMenu_NextSlot( void ) {
	/**
	*	Ensure client state is valid and the player is alive and not in intermission.
	**/
	if ( !clgi.client || clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 0 || clgi.client->frame.ps.pmove.pm_type >= PM_INTERMISSION ) {
		return;
	}

	/**
	*	If the weapon menu is not open, open it with focus on the current active weapon category and item index.
	**/
	if ( !s_wpn_menu.isOpen || s_wpn_menu.activeCategory <= WEAPON_CATEGORY_NONE || s_wpn_menu.activeCategory > WEAPON_CATEGORY_COUNT ) {
		OpenMenuOnActiveWeapon();
		return;
	}

	// Query owned weapons in the current active category.
	const weapon_category_t currentCategory = s_wpn_menu.activeCategory;
	const int32_t currentSlotIndex = s_wpn_menu.activeSlotIndex;
	const sg_item_t *ownedWeapons[ 16 ];
	// Get the count of owned weapons in the current category.
	const int32_t ownedCount = GetOwnedWeaponsInCategory( currentCategory, ownedWeapons, 16 );
	// If no owned weapons in the current category, close the menu.
	if ( ownedCount <= 0 ) {
		CLG_WeaponMenu_Close();
		return;
	}

	/**
	*	If there is a subsequent weapon item in the current category, step down to it.
	**/
	if ( ( currentSlotIndex + 1 ) < ownedCount ) {
		// Move to the next slot within the same category.
		ActivateCategoryAndSlot( currentCategory, currentSlotIndex + 1 );
		// Play draw sound to indicate selection change.
		clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
		return;
	}

	/**
	*	At the end of the current category list: search forward for the next category
	*	that has owned weapons, close the current category, and open the next category
	*	focusing on its 1st weapon item element (index 0).
	**/
	weapon_category_t nextCategory = WEAPON_CATEGORY_NONE;
	const int32_t currentCatIndex = static_cast<int32_t>( currentCategory );
	// Iterate forward through all categories with wrap-around.
	for ( int32_t offset = 1; offset <= WEAPON_CATEGORY_COUNT; offset++ ) {
		// Compute candidate category index in 1..WEAPON_CATEGORY_COUNT.
		const int32_t candidateIndex = ( ( currentCatIndex - 1 + offset ) % WEAPON_CATEGORY_COUNT ) + 1;
		const weapon_category_t candidateCat = static_cast<weapon_category_t>( candidateIndex );
		const sg_item_t *candidateWeapons[ 16 ];
		// Check if the player owns any weapons in this candidate category.
		if ( GetOwnedWeaponsInCategory( candidateCat, candidateWeapons, 16 ) > 0 ) {
			nextCategory = candidateCat;
			break;
		}
	}

	// If no owned weapons exist in any category, close the menu.
	if ( nextCategory == WEAPON_CATEGORY_NONE ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Close current category and open the next category focusing on its 1st weapon item (slot 0).
	ActivateCategoryAndSlot( nextCategory, 0 );
	// Play draw sound to indicate selection change.
	clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
}

/**
*	@brief	Confirm active weapon selection: sends "use <weapon>" to server.
**/
void CLG_WeaponMenu_Confirm( void ) {
	/**
	*	Ensure menu is open.
	**/
	if ( !s_wpn_menu.isOpen ) {
		return;
	}

	// Query owned weapons in active category.
	const sg_item_t *ownedWeapons[ 16 ];
	// Get the count of owned weapons in the active category.
	const int32_t ownedCount = GetOwnedWeaponsInCategory( s_wpn_menu.activeCategory, ownedWeapons, 16 );

	// Validate selected slot index.
	if ( s_wpn_menu.activeSlotIndex < 0 || s_wpn_menu.activeSlotIndex >= ownedCount ) {
		// Invalid slot index: close the menu and return.
		CLG_WeaponMenu_Close();
		return;
	}
	// Get the selected weapon based on the active slot index.
	const sg_item_t *selectedWeapon = ownedWeapons[ s_wpn_menu.activeSlotIndex ];
	if ( !selectedWeapon || !selectedWeapon->classname ) {
		// Invalid weapon pointer or classname: close the menu and return.
		CLG_WeaponMenu_Close();
		return;
	}

	// Check if client state is available.
	if ( !clgi.client ) {
		// Client state unavailable: close the menu and return.
		CLG_WeaponMenu_Close();
		return;
	}

	// Check if this weapon has available ammo.
	const uint64_t ammoMask = static_cast<uint64_t>( clgi.client->frame.ps.stats[ STAT_WEAPONS_AMMO ] );
	// Determine if the selected weapon's bit is set in the ammo mask.
	const bool hasAmmo = ( ( ammoMask & ( 1ULL << selectedWeapon->weapon_index ) ) != 0 );

	/**
	*	If out of ammo, play empty click sound and reject switch.
	**/
	if ( !hasAmmo ) {
		// Play "no ammo" sound to indicate the weapon cannot be selected.
		clgi.S_StartLocalSound( "weapons/pistol/noammo.wav" );
		return;
	}

	/**
	*	Play positive pickup sound, dispatch switch command, and close selector.
	**/
	// Play weapon pickup sound to indicate successful selection.
	clgi.S_StartLocalSound( "items/weaponry_pickup.wav" );

	char cmdBuf[ 64 ] = {};
	// Format the command to switch to the selected weapon using its classname.
	Q_scnprintf( cmdBuf, sizeof( cmdBuf ), "use %s\n", selectedWeapon->classname );
	// Send the command to the server to switch weapons.
	clgi.CL_ClientCommand( cmdBuf );

	// Close the weapon selection menu after confirming the selection.
	CLG_WeaponMenu_Close();
}

/**
*	@brief	Close the weapon selection menu without switching weapon, initiating rapid fade-out easing.
**/
void CLG_WeaponMenu_Close( void ) {
	// Fetch current real-time timestamp for fade-out easing.
	const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

	/**
	*	Trigger fade-out easing on the overall weapon bar and any open category dropdown.
	**/
	if ( s_wpn_menu.isOpen ) {
		s_wpn_menu.menuFader.StartFadeOut( realTime, WEAPON_MENU_EASE_DURATION );
	}
	if ( s_wpn_menu.activeCategory > WEAPON_CATEGORY_NONE && s_wpn_menu.activeCategory <= WEAPON_CATEGORY_COUNT ) {
		s_wpn_menu.categoryFaders[ s_wpn_menu.activeCategory ].StartFadeOut( realTime, WEAPON_MENU_EASE_DURATION );
	}

	// Immediately mark menu closed so gameplay input focus returns without delay.
	s_wpn_menu.isOpen = false;
	// Clear active category and slot index.
	s_wpn_menu.activeCategory = WEAPON_CATEGORY_NONE;
	// Reset active slot index to 0.
	s_wpn_menu.activeSlotIndex = 0;
	// Clear auto-close timer.
	s_wpn_menu.autoCloseTime = 0_ms;
}

/**
*	@brief	Check whether the weapon selection menu is currently open.
*	@return	True if active.
**/
const bool CLG_WeaponMenu_IsOpen( void ) {
	return s_wpn_menu.isOpen;
}

/**
*	@brief	Renders the Half-Life 1 style weapon selector bar and dropdown slots with rapid fade-in/out easing.
**/
void CLG_WeaponMenu_Draw( void ) {
	// Update auto-close countdown if menu is open.
	CLG_WeaponMenu_Update();

	// Check if client state is available.
	if ( !clgi.client ) {
		return;
	}

	/**
	*	Evaluate overall menu fade-in / fade-out alpha and bail out if fully invisible.
	**/
	const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );
	const double menuAlpha = s_wpn_menu.menuFader.Evaluate( realTime );
	if ( !s_wpn_menu.isOpen && menuAlpha <= 0.001 ) {
		return;
	}

	// Fetch ammo availability mask for owned weapons.
	const uint64_t ammoMask = static_cast<uint64_t>( clgi.client->frame.ps.stats[ STAT_WEAPONS_AMMO ] );

	/**
	*	Layout Dimensions:
	*	Top bar with 7 category columns.
	**/
	constexpr double COL_WIDTH = 140.0; // Width of each category column.
	constexpr double COL_HEIGHT = 26.0; // Height of each category column.
	constexpr double COL_GAP = 6.0;     // Gap between category columns.

	// Horizontal start position for the category bar.
	const double startX = 24.0;
	// Vertical start position for the category bar.
	const double startY = 24.0;

	/**
	*	Draw the 7 Category Headers and any active or fading-out Category Dropdowns:
	**/
	for ( int32_t c = 1; c <= WEAPON_CATEGORY_COUNT; c++ ) {
		// Cast the loop index to the weapon category enum.
		const weapon_category_t cat = static_cast<weapon_category_t>( c );
		// Calculate the X position for this category column.
		const double colX = startX + ( ( c - 1 ) * ( COL_WIDTH + COL_GAP ) );

		// Check if any weapons are owned in this category.
		const sg_item_t *catWeapons[ 16 ];
		// Get the count of owned weapons in this category.
		const int32_t ownedCount = GetOwnedWeaponsInCategory( cat, catWeapons, 16 );
		// Determine if the player owns any weapons in this category.
		const bool hasOwned = ( ownedCount > 0 );
		// Evaluate per-category fade-in / fade-out alpha factor in [0.0, 1.0].
		const double catAlpha = s_wpn_menu.categoryFaders[ c ].Evaluate( realTime );

		// Determine base colors and style based on category ownership and active ease factor.
		uint32_t boxFill = COLOR_WEAPON_BAR_ORANGE_BG_DIM;
		uint32_t boxStroke = COLOR_WEAPON_BAR_ORANGE_DIM;
		uint32_t textColor = COLOR_WEAPON_BAR_ORANGE_DIM;

		/**
		*	Blend category header between inactive and active styling using catAlpha,
		*	and scale all header colors by the overall menuAlpha.
		**/
		if ( hasOwned ) {
			// Owned category: blend between dim/base orange and bright active orange using catAlpha.
			boxFill = LerpColorRGBA( COLOR_WEAPON_BAR_ORANGE_BG_DIM, COLOR_WEAPON_BAR_ORANGE_BG, catAlpha );
			boxStroke = LerpColorRGBA( COLOR_WEAPON_BAR_ORANGE_BASE, COLOR_WEAPON_BAR_ORANGE_BRIGHT, catAlpha );
			textColor = LerpColorRGBA( COLOR_WEAPON_BAR_ORANGE_BASE, COLOR_WEAPON_BAR_ORANGE_BRIGHT, catAlpha );

			// Apply outer glow proportional to category active fade factor.
			if ( catAlpha > 0.01 ) {
				clgi.R_SetOuterGlow(
					ScaleColorAlpha( COLOR_WEAPON_BAR_ORANGE_GLOW, catAlpha * menuAlpha ),
					static_cast<float>( 6.0 * catAlpha )
				);
			}
		} else {
			// Unowned category: dim/faded background and outline.
			boxFill = COLOR_WEAPON_BAR_ORANGE_UNOWNED_BG;
			boxStroke = COLOR_WEAPON_BAR_ORANGE_UNOWNED_STROKE;
			textColor = COLOR_WEAPON_BAR_ORANGE_UNOWNED_TEXT;
		}

		// Apply overall menuAlpha to header fill and stroke.
		boxFill = ScaleColorAlpha( boxFill, menuAlpha );
		boxStroke = ScaleColorAlpha( boxStroke, menuAlpha );

		// Apply styling and render header rectangle.
		const float headerStrokeThickness = static_cast<float>( QM_Lerp<double>( 1.0, 1.5, catAlpha ) );
		clgi.R_SetStroke( boxStroke, headerStrokeThickness );
		clgi.R_SetCornerRadius( 2.0f );
		clgi.R_DrawFill32( colX, startY, COL_WIDTH, COL_HEIGHT, boxFill );
		clgi.R_ClearStyle();

		// Draw category header text centered.
		const char *headerTitle = s_categoryNames[ c ];
		const int32_t textLen = static_cast<int32_t>( strlen( headerTitle ) );
		const double textX = colX + ( ( COL_WIDTH - ( textLen * CHAR_WIDTH ) ) * 0.5 );
		const double textY = startY + ( ( COL_HEIGHT - CHAR_HEIGHT ) * 0.5 );

		DrawWeaponMenuString( static_cast<int32_t>( textX ), static_cast<int32_t>( textY ), MAX_STRING_CHARS, headerTitle, textColor, menuAlpha );

		/**
		*	If this category dropdown is open or fading out, draw its weapon slots below the header.
		**/
		const double dropdownAlpha = menuAlpha * catAlpha;
		if ( dropdownAlpha > 0.001 && hasOwned ) {
			constexpr double SLOT_HEIGHT = 44.0;
			constexpr double SLOT_GAP    = 4.0;
			double dropY = startY + COL_HEIGHT + 6.0;

			// Use activeSlotIndex when this category is currently active, or categoryLastSlot while fading out.
			const int32_t highlightedSlot = ( cat == s_wpn_menu.activeCategory )
				? s_wpn_menu.activeSlotIndex
				: s_wpn_menu.categoryLastSlot[ c ];

			// Render each owned weapon slot in the dropdown list.
			for ( int32_t s = 0; s < ownedCount; s++ ) {
				const sg_item_t *w = catWeapons[ s ];
				const bool isSlotActive = ( s == highlightedSlot );
				const bool weaponHasAmmo = ( ( ammoMask & ( 1ULL << w->weapon_index ) ) != 0 );

				// Configure slot colors: red warning tint if out of ammo, orange if ammo is available.
				uint32_t slotFill = 0;
				uint32_t slotStroke = 0;
				uint32_t slotTextColor = 0;

				if ( !weaponHasAmmo ) {
					if ( isSlotActive ) {
						slotFill = COLOR_RED_DISABLED_BG;
						slotStroke = COLOR_RED_DISABLED;
						slotTextColor = COLOR_RED_DISABLED;
						clgi.R_SetOuterGlow(
							ScaleColorAlpha( COLOR_RED_DISABLED_GLOW, dropdownAlpha ),
							static_cast<float>( 8.0 * dropdownAlpha )
						);
					} else {
						slotFill = COLOR_RED_DISABLED_BG_DIM;
						slotStroke = COLOR_RED_DISABLED_DIM;
						slotTextColor = COLOR_RED_DISABLED_DIM;
					}
				} else {
					if ( isSlotActive ) {
						slotFill = COLOR_WEAPON_BAR_ORANGE_BG;
						slotStroke = COLOR_WEAPON_BAR_ORANGE_BRIGHT;
						slotTextColor = COLOR_WEAPON_BAR_ORANGE_BRIGHT;
						clgi.R_SetOuterGlow(
							ScaleColorAlpha( COLOR_WEAPON_BAR_ORANGE_GLOW, dropdownAlpha ),
							static_cast<float>( 8.0 * dropdownAlpha )
						);
					} else {
						slotFill = COLOR_WEAPON_BAR_ORANGE_BG_DIM;
						slotStroke = COLOR_WEAPON_BAR_ORANGE_BASE;
						slotTextColor = COLOR_WEAPON_BAR_ORANGE_BASE;
					}
				}

				// Scale slot box fill and stroke alpha by the combined dropdownAlpha.
				slotFill = ScaleColorAlpha( slotFill, dropdownAlpha );
				slotStroke = ScaleColorAlpha( slotStroke, dropdownAlpha );

				// Apply styling and render slot box.
				clgi.R_SetStroke( slotStroke, isSlotActive ? 2.0f : 1.0f );
				clgi.R_SetCornerRadius( 2.0f );
				clgi.R_DrawFill32( colX, dropY, COL_WIDTH, SLOT_HEIGHT, slotFill );
				clgi.R_ClearStyle();

				/**
				*	Slot Contents: Weapon Icon and Pickup Name.
				**/
				double contentX = colX + 6.0;
				const double contentCenterY = dropY + ( SLOT_HEIGHT * 0.5 );

				// Draw weapon icon if available.
				if ( w->icon && w->icon[ 0 ] != '\0' ) {
					const qhandle_t iconHandle = clgi.R_RegisterPic( w->icon );
					if ( iconHandle ) {
						constexpr double ICON_SIZE = 28.0;
						const double iconY = contentCenterY - ( ICON_SIZE * 0.5 );

						clgi.R_SetColor( ScaleColorAlpha( slotTextColor, dropdownAlpha ) );
						clgi.R_DrawStretchPic( contentX, iconY, ICON_SIZE, ICON_SIZE, iconHandle );
						clgi.R_ClearColor();

						contentX += ICON_SIZE + 4.0;
					}
				}

				// Draw weapon name.
				const char *weaponName = w->pickup_name ? w->pickup_name : w->classname;
				const double nameY = contentCenterY - ( CHAR_HEIGHT * 0.5 );

				DrawWeaponMenuString( static_cast<int32_t>( contentX ), static_cast<int32_t>( nameY ), MAX_STRING_CHARS, weaponName, slotTextColor, dropdownAlpha );

				// If active, draw Half-Life style corner brackets "[  ]" indicator.
				if ( isSlotActive ) {
					DrawWeaponMenuString( static_cast<int32_t>( colX + 2.0 ), static_cast<int32_t>( nameY ), 1, "[", slotTextColor, dropdownAlpha );
					DrawWeaponMenuString( static_cast<int32_t>( colX + COL_WIDTH - 10.0 ), static_cast<int32_t>( nameY ), 1, "]", slotTextColor, dropdownAlpha );
				}

				dropY += SLOT_HEIGHT + SLOT_GAP;
			}
		}
	}

	// Reset any remaining render state.
	clgi.R_ClearStyle();
	clgi.R_ClearColor();
	clgi.R_SetAlpha( 1.0f );
}

