/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



/**
*   @brief  Called for important messages that should stay in the center of the screen
*           for a few moments
**/
void SCR_CenterPrint( const char *str );
/**
*   @brief  Clear the chat HUD.
**/
void SCR_ClearChatHUD_f( void );
/**
*   @brief  Append text to chat HUD.
**/
void SCR_AddToChatHUD( const char *text );
/**
*	@brief
**/
void PF_SCR_Init( void );
/**
*	@brief
**/
void PF_SCR_RegisterMedia( void );
/**
*	@brief
**/
void PF_SCR_ModeChanged( void );
/**
*	@brief
**/
void PF_SCR_DeltaFrame( void );
/**
*	@brief
**/
void PF_SCR_Shutdown( void );
/**
*	@return	Pointer to the current frame's render "view rectangle".
**/
vrect_t *PF_GetScreenVideoRect( void );
/**
*	@brief	Prepare and draw the current 'active' state's 2D and 3D views.
**/
void PF_DrawActiveState( refcfg_t *refcfg );
/**
*	@brief	Prepare and draw the loading screen's 2D state.
**/
void PF_DrawLoadState( refcfg_t *refcfg );

/**
*   @return The current active handle to the font image. (Used by ref/vkpt.)
**/
const qhandle_t PF_GetScreenFontHandle( void );
/**
*   @brief  Set the alpha value of the HUD. (Used by ref/vkpt.)
**/
void PF_SetScreenHUDAlpha( const float alpha );