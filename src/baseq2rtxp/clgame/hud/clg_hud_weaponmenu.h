/********************************************************************
*
*
*	ClientGame: Half-Life 1 Style Weapon Selection Menu Header.
*
*
********************************************************************/
#pragma once

#include "sharedgame/sg_items.h"

/**
*	@brief	Initialize weapon selection HUD menu resources and state.
**/
void CLG_WeaponMenu_Init( void );

/**
*	@brief	Shutdown weapon selection HUD menu.
**/
void CLG_WeaponMenu_Shutdown( void );

/**
*	@brief	Per-frame update: handles auto-close countdown.
**/
void CLG_WeaponMenu_Update( void );

/**
*	@brief	Renders the Half-Life 1 style weapon selector bar and dropdown slots.
**/
void CLG_WeaponMenu_Draw( void );

/**
*	@brief	Select or cycle weapons in the specified category (1..7).
*	@param	category	The category requested (WEAPON_CATEGORY_MELEE..WEAPON_CATEGORY_OTHER).
**/
void CLG_WeaponMenu_SelectCategory( const weapon_category_t category );

/**
*	@brief	Move selection up to the previous owned weapon in the active category.
**/
void CLG_WeaponMenu_PrevSlot( void );

/**
*	@brief	Move selection down to the next owned weapon in the active category.
**/
void CLG_WeaponMenu_NextSlot( void );

/**
*	@brief	Confirm active weapon selection: sends "use <weapon>" to server.
**/
void CLG_WeaponMenu_Confirm( void );

/**
*	@brief	Close the weapon selection menu without switching weapon.
**/
void CLG_WeaponMenu_Close( void );

/**
*	@brief	Check whether the weapon selection menu is currently open.
*	@return	True if active.
**/
const bool CLG_WeaponMenu_IsOpen( void );


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
const bool IN_WeaponBar_ForwardDown( void );
/**
*	@brief	When +forward button is released (up).
**/
const bool IN_WeaponBar_ForwardUp( void );
/**
*	@brief	When +back button is pressed down.
*	@note	If the HUD weapon selector menu is open, only the DOWNARROW key navigates to the next slot.
*			Any other key (such as 'S') is ignored by the menu and forwarded to normal movement as usual.
**/
const bool IN_WeaponBar_BackDown( void );
/**
*	@brief	When +back button is released (up).
**/
const bool IN_WeaponBar_BackUp( void );