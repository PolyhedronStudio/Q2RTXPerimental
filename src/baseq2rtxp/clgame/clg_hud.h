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
*	@brief	Called when screen module is drawing its 2D overlay.
**/
void CLG_HUD_Draw( refcfg_t *refcfg );
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
