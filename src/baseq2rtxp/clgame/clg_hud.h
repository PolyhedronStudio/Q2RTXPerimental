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
*
*   HUD Core:
*
*
* 
**/
/**
*   @brief  Called when screen module is initialized.
**/
void CLG_HUD_Initialize( void );
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
*	@brief	Called when screen modfule is drawing its 2D overlay(s).
**/
void CLG_HUD_ScaleFrame( refcfg_t *refcfg );
/**
*	@brief	Called when screen module is drawing its 2D overlay(s).
**/
void CLG_HUD_DrawFrame( refcfg_t *refcfg );
/**
*	@brief	Called when the screen module registers media.
**/
void CLG_HUD_RegisterScreenMedia( void );
/**
*   @brief  Called when screne module shutsdown.
**/
void CLG_HUD_Shutdown( void );



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
const int32_t HUD_DrawString( const int32_t x, const int32_t y, const char *str );
/**
*   @brief
**/
const int32_t HUD_DrawString( const int32_t x, const int32_t y, const int32_t flags, const char *str );
/**
*   @brief
**/
const int32_t HUD_DrawAltString( const int32_t x, const int32_t y, const char *str );
/**
*   @brief
**/
const int32_t HUD_DrawAltString( const int32_t x, const int32_t y, const int32_t flags, const char *str );
/**
*   @brief
**/
void HUD_DrawCenterString( const int32_t x, const int32_t y, const char *str );
/**
*   @brief
**/
void HUD_DrawAltCenterString( const int32_t x, const int32_t y, const char *str );



/**
*
*
*
*	HUD TargetHints:
*
*
*
**/
/**
*   @brief  Called upon when clearing client state.
**/
void CLG_HUD_ClearTargetHints();
/**
*	@brief  Renders the 'UseTarget' display info to the screen.
**/
void CLG_HUD_DrawUseTargetHintInfos();



/**
*
*
*
*	HUD CrossHair:
*
*
*
**/
/**
*   @brief
**/
void CLG_HUD_SetCrosshairColor();
/**
*	@brief  Renders the crosshair to screen.
**/
void CLG_HUD_DrawCrosshair( void );



/**
*
*
*
*	HUD Chat:
*
*
*
**/
/**
*   @brief  Clear the chat HUD.
**/
void CLG_HUD_ClearChat_f( void );
/**
*   @brief  Append text to chat HUD.
**/
void CLG_HUD_AddChatLine( const char *text );
/**
*   @brief  Draws chat hud to screen.
**/
void CLG_HUD_DrawChat( void );