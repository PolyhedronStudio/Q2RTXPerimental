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
*	Weapon selector menu internal state.
**/
struct clg_weapon_menu_state_t {
	//! True when the HUD selector overlay is active.
	bool				isOpen = false;
	//! The currently expanded category (1..7).
	weapon_category_t	activeCategory = WEAPON_CATEGORY_NONE;
	//! The active highlighted item slot within the category (0..N-1).
	int32_t				activeSlotIndex = 0;
	//! Real-time timestamp when menu will auto-close due to inactivity.
	QMTime				autoCloseTime = 0_ms;
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
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BRIGHT	= MakeColor( 255, 175, 75, 255 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BASE		= MakeColor( 210, 125, 44, 220 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_GLOW		= MakeColor( 210, 125, 44, 180 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_DIM		= MakeColor( 210, 125, 44, 120 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BG		= MakeColor( 35, 18, 8, 220 );
static constexpr uint32_t COLOR_WEAPON_BAR_ORANGE_BG_DIM	= MakeColor( 25, 16, 10, 160 );
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
	const int32_t totalInCat = SG_Item_GetWeaponsInCategory( category, catWeapons, 16 );

	// Check if client state is available.
	if ( !clgi.client ) {
		return 0;
	}

	// Fetch owned weapons bitmask from synchronized player state stats.
	const uint64_t ownedMask = static_cast<uint64_t>( clgi.client->frame.ps.stats[ STAT_WEAPONS_OWNED ] );

	// Filter to weapons whose bit is set in the ownership mask.
	int32_t ownedCount = 0;
	for ( int32_t i = 0; i < totalInCat; i++ ) {
		const sg_item_t *weapon = catWeapons[ i ];
		if ( ( ownedMask & ( 1ULL << weapon->weapon_index ) ) != 0 ) {
			if ( ownedCount < maxWeapons ) {
				outWeapons[ ownedCount++ ] = weapon;
			}
		}
	}

	return ownedCount;
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
	CLG_WeaponMenu_Close();
}

/**
*	@brief	Per-frame update: handles auto-close countdown and death resets.
**/
void CLG_WeaponMenu_Update( void ) {
	/**
	*	Only update when menu is open.
	**/
	if ( !s_wpn_menu.isOpen ) {
		return;
	}

	// Check if client state is available.
	if ( !clgi.client ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Close menu immediately if the player is dead or in spectator/intermission.
	if ( clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 0 || clgi.client->frame.ps.pmove.pm_type >= PM_INTERMISSION ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Check if the auto-close timer has expired.
	const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );
	if ( realTime >= s_wpn_menu.autoCloseTime ) {
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
		s_wpn_menu.isOpen = true;
		s_wpn_menu.activeCategory = category;
		s_wpn_menu.activeSlotIndex = 0;
		clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
	} else {
		/**
		*	Same category already active: cycle to the next sub-item (wrap around).
		**/
		s_wpn_menu.activeSlotIndex = ( s_wpn_menu.activeSlotIndex + 1 ) % ownedCount;
		clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
	}

	// Reset auto-close timer to 4 seconds from now.
	s_wpn_menu.autoCloseTime = QMTime::FromMilliseconds( clgi.GetRealTime() ) + 4_sec;
}

/**
*	@brief	Move selection up to the previous owned weapon in the active category.
**/
void CLG_WeaponMenu_PrevSlot( void ) {
	/**
	*	Only operate if menu is open.
	**/
	if ( !s_wpn_menu.isOpen ) {
		return;
	}

	// Query owned weapons in current active category.
	const sg_item_t *ownedWeapons[ 16 ];
	const int32_t ownedCount = GetOwnedWeaponsInCategory( s_wpn_menu.activeCategory, ownedWeapons, 16 );
	if ( ownedCount <= 0 ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Decrement slot index with wrap-around.
	s_wpn_menu.activeSlotIndex = ( s_wpn_menu.activeSlotIndex + ownedCount - 1 ) % ownedCount;
	s_wpn_menu.autoCloseTime = QMTime::FromMilliseconds( clgi.GetRealTime() ) + 4_sec;
	clgi.S_StartLocalSound( "weapons/pistol/draw.wav" );
}

/**
*	@brief	Move selection down to the next owned weapon in the active category.
**/
void CLG_WeaponMenu_NextSlot( void ) {
	/**
	*	Only operate if menu is open.
	**/
	if ( !s_wpn_menu.isOpen ) {
		return;
	}

	// Query owned weapons in current active category.
	const sg_item_t *ownedWeapons[ 16 ];
	const int32_t ownedCount = GetOwnedWeaponsInCategory( s_wpn_menu.activeCategory, ownedWeapons, 16 );
	if ( ownedCount <= 0 ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Increment slot index with wrap-around.
	s_wpn_menu.activeSlotIndex = ( s_wpn_menu.activeSlotIndex + 1 ) % ownedCount;
	s_wpn_menu.autoCloseTime = QMTime::FromMilliseconds( clgi.GetRealTime() ) + 4_sec;
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
	const int32_t ownedCount = GetOwnedWeaponsInCategory( s_wpn_menu.activeCategory, ownedWeapons, 16 );

	// Validate selected slot index.
	if ( s_wpn_menu.activeSlotIndex < 0 || s_wpn_menu.activeSlotIndex >= ownedCount ) {
		CLG_WeaponMenu_Close();
		return;
	}

	const sg_item_t *selectedWeapon = ownedWeapons[ s_wpn_menu.activeSlotIndex ];
	if ( !selectedWeapon || !selectedWeapon->classname ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Check if client state is available.
	if ( !clgi.client ) {
		CLG_WeaponMenu_Close();
		return;
	}

	// Check if this weapon has available ammo.
	const uint64_t ammoMask = static_cast<uint64_t>( clgi.client->frame.ps.stats[ STAT_WEAPONS_AMMO ] );
	const bool hasAmmo = ( ( ammoMask & ( 1ULL << selectedWeapon->weapon_index ) ) != 0 );

	/**
	*	If out of ammo, play empty click sound and reject switch.
	**/
	if ( !hasAmmo ) {
		clgi.S_StartLocalSound( "weapons/pistol/noammo.wav" );
		return;
	}

	/**
	*	Play positive pickup sound, dispatch switch command, and close selector.
	**/
	clgi.S_StartLocalSound( "items/weaponry_pickup.wav" );

	char cmdBuf[ 64 ] = {};
	Q_scnprintf( cmdBuf, sizeof( cmdBuf ), "use %s\n", selectedWeapon->classname );
	clgi.CL_ClientCommand( cmdBuf );

	CLG_WeaponMenu_Close();
}

/**
*	@brief	Close the weapon selection menu without switching weapon.
**/
void CLG_WeaponMenu_Close( void ) {
	s_wpn_menu.isOpen = false;
	s_wpn_menu.activeCategory = WEAPON_CATEGORY_NONE;
	s_wpn_menu.activeSlotIndex = 0;
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
*	@brief	Renders the Half-Life 1 style weapon selector bar and dropdown slots.
**/
void CLG_WeaponMenu_Draw( void ) {
	/**
	*	Only render when the menu is active.
	**/
	if ( !s_wpn_menu.isOpen ) {
		return;
	}

	// Update auto-close countdown.
	CLG_WeaponMenu_Update();
	if ( !s_wpn_menu.isOpen ) {
		return;
	}

	// Check if client state is available.
	if ( !clgi.client ) {
		return;
	}

	// Fetch owned weapons mask and ammo availability mask.
	const uint64_t ownedMask = static_cast<uint64_t>( clgi.client->frame.ps.stats[ STAT_WEAPONS_OWNED ] );
	const uint64_t ammoMask  = static_cast<uint64_t>( clgi.client->frame.ps.stats[ STAT_WEAPONS_AMMO ] );

	/**
	*	Layout Dimensions:
	*	Centered top bar with 7 category columns.
	**/
	constexpr double COL_WIDTH		= 104.0;
	constexpr double COL_HEIGHT		= 26.0;
	constexpr double COL_GAP		= 6.0;
	constexpr double TOTAL_WIDTH = ( double )( ( double )WEAPON_CATEGORY_COUNT * ( double )COL_WIDTH ) + ( ( ( double )WEAPON_CATEGORY_COUNT - 1. ) * ( (double)COL_GAP ));

	// Calculate horizontal start position so the row is centered on screen.
	const double screenW = clgi.screen->hudRealWidth > 0. ? clgi.screen->hudRealWidth : clgi.screen->screenWidth;
	const double startX = 24.0;//( screenW - TOTAL_WIDTH ) * 0.5;
	const double startY = 24.0;

	/**
	*	Draw the 7 Category Headers:
	**/
	for ( int32_t c = 1; c <= WEAPON_CATEGORY_COUNT; c++ ) {
		const weapon_category_t cat = static_cast<weapon_category_t>( c );
		const double colX = startX + ( ( c - 1 ) * ( COL_WIDTH + COL_GAP ) );

		// Check if any weapons are owned in this category.
		const sg_item_t *catWeapons[ 16 ];
		const int32_t ownedCount = GetOwnedWeaponsInCategory( cat, catWeapons, 16 );
		const bool hasOwned = ( ownedCount > 0 );
		const bool isCatActive = ( cat == s_wpn_menu.activeCategory );

		// Determine colors and style based on category state.
		uint32_t boxFill = COLOR_WEAPON_BAR_ORANGE_BG_DIM;
		uint32_t boxStroke = COLOR_WEAPON_BAR_ORANGE_DIM;
		uint32_t textColor = COLOR_WEAPON_BAR_ORANGE_DIM;

		if ( isCatActive ) {
			boxFill = COLOR_WEAPON_BAR_ORANGE_BG;
			boxStroke = COLOR_WEAPON_BAR_ORANGE_BRIGHT;
			textColor = COLOR_WEAPON_BAR_ORANGE_BRIGHT;

			// Active category gets outer glow.
			clgi.R_SetOuterGlow( COLOR_WEAPON_BAR_ORANGE_GLOW, 6.0f );
		} else if ( hasOwned ) {
			boxFill = COLOR_WEAPON_BAR_ORANGE_BG_DIM;
			boxStroke = COLOR_WEAPON_BAR_ORANGE_BASE;
			textColor = COLOR_WEAPON_BAR_ORANGE_BASE;
		} else {
			// Unowned category: dim/faded background and outline.
			boxFill = MakeColor( 14, 10, 8, 120 );
			boxStroke = MakeColor( 80, 50, 30, 80 );
			textColor = MakeColor( 100, 70, 45, 120 );
		}

		// Apply styling and render header rectangle.
		clgi.R_SetStroke( boxStroke, isCatActive ? 1.5f : 1.0f );
		clgi.R_SetCornerRadius( 2.0f );
		clgi.R_DrawFill32( colX, startY, COL_WIDTH, COL_HEIGHT, boxFill );
		clgi.R_ClearStyle();

		// Draw category header text centered.
		const char *headerTitle = s_categoryNames[ c ];
		const int32_t textLen = static_cast<int32_t>( strlen( headerTitle ) );
		const double textX = colX + ( ( COL_WIDTH - ( textLen * CHAR_WIDTH ) ) * 0.5 );
		const double textY = startY + ( ( COL_HEIGHT - CHAR_HEIGHT ) * 0.5 );

		clgi.R_SetColor( textColor );
		SCR_DrawStringEx( static_cast<int32_t>( textX ), static_cast<int32_t>( textY ), 0, MAX_STRING_CHARS, headerTitle, precache.screen.font_pic );
		clgi.R_ClearColor();

		/**
		*	If this category is active, draw the dropdown list of weapon slots below it.
		**/
		if ( isCatActive && hasOwned ) {
			constexpr double SLOT_HEIGHT = 44.0;
			constexpr double SLOT_GAP    = 4.0;
			double dropY = startY + COL_HEIGHT + 6.0;

			for ( int32_t s = 0; s < ownedCount; s++ ) {
				const sg_item_t *w = catWeapons[ s ];
				const bool isSlotActive = ( s == s_wpn_menu.activeSlotIndex );
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
						clgi.R_SetOuterGlow( COLOR_RED_DISABLED_GLOW, 8.0f );
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
						clgi.R_SetOuterGlow( COLOR_WEAPON_BAR_ORANGE_GLOW, 8.0f );
					} else {
						slotFill = COLOR_WEAPON_BAR_ORANGE_BG_DIM;
						slotStroke = COLOR_WEAPON_BAR_ORANGE_BASE;
						slotTextColor = COLOR_WEAPON_BAR_ORANGE_BASE;
					}
				}

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

						clgi.R_SetColor( slotTextColor );
						clgi.R_DrawStretchPic( contentX, iconY, ICON_SIZE, ICON_SIZE, iconHandle );
						clgi.R_ClearColor();

						contentX += ICON_SIZE + 4.0;
					}
				}

				// Draw weapon name.
				const char *weaponName = w->pickup_name ? w->pickup_name : w->classname;
				const double nameY = contentCenterY - ( CHAR_HEIGHT * 0.5 );

				clgi.R_SetColor( slotTextColor );
				SCR_DrawStringEx( static_cast<int32_t>( contentX ), static_cast<int32_t>( nameY ), 0, MAX_STRING_CHARS, weaponName, precache.screen.font_pic );
				clgi.R_ClearColor();

				// If active, draw Half-Life style corner brackets "[  ]" indicator.
				if ( isSlotActive ) {
					clgi.R_SetColor( slotTextColor );
					SCR_DrawStringEx( static_cast<int32_t>( colX + 2.0 ), static_cast<int32_t>( nameY ), 0, 1, "[", precache.screen.font_pic );
					SCR_DrawStringEx( static_cast<int32_t>( colX + COL_WIDTH - 10.0 ), static_cast<int32_t>( nameY ), 0, 1, "]", precache.screen.font_pic );
					clgi.R_ClearColor();
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
