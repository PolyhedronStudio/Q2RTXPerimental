/********************************************************************
*
*
*	ClientGame: View Header
*
*
********************************************************************/
#pragma once


/**
*   @brief  Adds the first person view its weapon model entity.
**/
void CLG_AddViewWeapon( void );
/**
*   @brief  Calculates the gun view origin offset, based on the bobcycle and its sinfrac2.
*           After that, it lerps the gun offset to the target position based on whether the player is moving or not.
* 	@note   Will lerp back to center position when not moving or secondary firing.
**/
void CLG_CalculateViewWeaponOffset( player_state_t *ops, player_state_t *ps, const double lerpFrac );
/**
*   @brief  Calculates the gun view angles by lerping between the old and new angles, adding a
*           swing-like delay effect based on view movement.
**/
void CLG_CalculateViewWeaponAngles( player_state_t *ops, player_state_t *ps, const double lerpFrac );

