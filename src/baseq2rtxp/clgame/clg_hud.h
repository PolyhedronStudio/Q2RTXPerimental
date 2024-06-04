/********************************************************************
*
*
*   Client 'Heads Up Display':
*
*
********************************************************************/
#pragma once



/**
*
*
*   HUD Core:
*
*
**/
/**
*   @brief  Called when screen module is initialized.
**/
void CLG_HUD_Initialize( void );
/**
*   @brief
**/
void CLG_HUD_SetCrosshairColor();
/**
*   @brief  Called by PF_SCR_ModeChanged(video mode changed), or scr_scale_changed, in order to
*           notify about the new HUD scale.
**/
void CLG_HUD_ModeChanged( const float newHudScale );
/**
*   @brief  Called by PF_SetScreenHUDAlpha, in order to notify about the new HUD alpha.
**/
void CLG_HUD_AlphaChanged( const float newHudAlpha );
/**
*	@brief	Called when the screen module registers media.
**/
void CLG_HUD_RegisterScreenMedia( void );
/**
*   @brief  Called when screne module shutsdown.
**/
void CLG_HUD_Shutdown( void );
/**
*	@brief	Called when screen module is drawing its 2D overlay.
**/
void CLG_HUD_Draw( refcfg_t *refcfg );



/**
*
*
*   HUD String Drawing:
*
*
**/
/**
*   @brief
**/
void HUD_DrawString( const int32_t x, const int32_t y, const char *str );
/**
*   @brief
**/
void HUD_DrawAltString( const int32_t x, const int32_t y, const char *str );
/**
*   @brief
**/
void HUD_DrawCenterString( const int32_t x, const int32_t y, const char *str );
/**
*   @brief
**/
void HUD_DrawAltCenterString( const int32_t x, const int32_t y, const char *str );
