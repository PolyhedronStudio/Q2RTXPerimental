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
*   @brief  Adds a damage indicator for the given damage using the given color pointing at given direction.
**/
void SCR_AddToDamageDisplay( const int32_t damage, const Vector3 &color, const Vector3 &dir );
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
*	@brief	Called whenever a delta frame has been succesfully dealt with.
*			It allows a moment for updating HUD/Screen related data.
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

/**
*   @brief  Draws a string using at x/y up till maxlen.
**/
const int32_t SCR_DrawStringEx( const int32_t x, const int32_t y, const int32_t flags, const size_t maxlen, const char *s, const qhandle_t font );
/**
*   @brief  Draws a string using SCR_DrawStringEx but using the default screen font.
**/
const int32_t SCR_DrawString( const int32_t x, const int32_t y, const int32_t flags, const char *str );
/**
*   @brief  Draws a multiline supporting string at location x/y.
**/
void SCR_DrawStringMulti( const int32_t x, const int32_t y, const int32_t flags, const size_t maxlen, const char *s, const qhandle_t font );
/**
*   @brief Fades alpha in and out, keeping the alpha visible for 'visTime' amount.
*   @return 'Alpha' value of the current moment in time. from(startTime) to( startTime + visTime ).
**/
const double SCR_FadeAlpha( const uint64_t startTime, const uint64_t visTime, const uint64_t fadeTime );