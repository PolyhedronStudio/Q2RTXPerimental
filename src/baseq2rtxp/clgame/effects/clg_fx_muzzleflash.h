/********************************************************************
*
*
*	ClientGame: 'MuzzleFlash' FX Implementation:
*
*
********************************************************************/
#pragma once

 


/**
*	@brief	Adds a muzzleflash dynamic light.
**/
clg_dlight_t *CLG_AddMuzzleflashDynamicLight( const centity_t *pl, const Vector3 &vForward, const Vector3 &vRight );
/**
*   @brief  Handles the parsed client muzzleflash effects.
**/
void CLG_MuzzleFlash( void );
#if 0 // WID: Removed
/**
*   @brief  Handles the parsed entities/monster muzzleflash effects.
**/
void CLG_MuzzleFlash2( void );
#endif