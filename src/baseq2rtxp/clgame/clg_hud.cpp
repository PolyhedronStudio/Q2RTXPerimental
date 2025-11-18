/********************************************************************
*
*
*   Client 'Heads Up Display':
*
*
********************************************************************/
#include "clgame/clg_local.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_usetarget_hints.h"

#include "clgame/clg_hud.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_screen.h"



//
// CVars
//
//! We need this one.
extern cvar_t *scr_alpha;
extern cvar_t *scr_scale;
#if 0
cvar_t *hud_alpha = nullptr;
cvar_t *hud_scale = nullptr;
#endif
// For damage indicators.
cvar_t *hud_damage_indicators = nullptr;
cvar_t *hud_damage_indicator_time = nullptr;

//! The chat clg_hud.
cvar_t *hud_chat = nullptr;
cvar_t *hud_chat_lines = nullptr;
cvar_t *hud_chat_time = nullptr;
cvar_t *hud_chat_x = nullptr;
cvar_t *hud_chat_y = nullptr;

//! The crosshair.
cvar_t *hud_crosshair_type;
cvar_t *hud_crosshair_red;
cvar_t *hud_crosshair_green = nullptr;
cvar_t *hud_crosshair_blue = nullptr;
cvar_t *hud_crosshair_alpha = nullptr;
cvar_t *hud_crosshair_scale = nullptr;

// Client Game Hud state.
hud_state_t clg_hud = {};
static hud_static_t clg_hud_static = {};


//! Used in various places.
extern const bool SCR_ShouldDrawPause();

static void CLG_HUD_DrawAmmoIndicators();
static void CLG_HUD_DrawHealthIndicators();


/**
*
*
* 
*   CVar change and updating:
*
* 
*
**/
/**
*	@brief
**/
static void cl_timeout_changed( cvar_t *self ) {
    self->integer = 1000 * clgi.CVar_ClampValue( self, 0, 24 * 24 * 60 * 60 );
}

/**
*   @brief  Will set the crosshair color to a custom color which is defined by cvars.
**/
static void CLG_HUD_SetCrosshairColorByCVars() {
    clg_hud.crosshair.color.u8[ 0 ] = clgi.CVar_ClampValue( hud_crosshair_red, 0.f, 1.f ) * 255;
    clg_hud.crosshair.color.u8[ 1 ] = clgi.CVar_ClampValue( hud_crosshair_green, 0.f, 1.f ) * 255;
    clg_hud.crosshair.color.u8[ 2 ] = clgi.CVar_ClampValue( hud_crosshair_blue, 0.f, 1.f ) * 255;

    // We use a separate alpha value.
    clg_hud.crosshair.color.u8[ 3 ] = clgi.CVar_ClampValue( hud_crosshair_alpha, 0.f, 1.f ) * 255;
}
/**
*   @brief  Used to update crosshair color with.
**/
void CLG_HUD_SetCrosshairColor() {
    // Set it based on cvars.
    CLG_HUD_SetCrosshairColorByCVars();
    // Apply alpha value.
    //clg_hud.crosshair.alpha = clgi.CVar_ClampValue( hud_crosshair_alpha, 0.0f, 1.0f );
    //clg_hud.crosshair.color.u8[ 3 ] = clgi.CVar_ClampValue( ch_alpha, 0, 1 ) * 255;
}
/**
*	@brief
**/
static void scr_hud_crosshair_changed( cvar_t *self ) {
    CLG_HUD_SetCrosshairColor();
}



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
void CLG_HUD_Initialize( void ) {
    // Reset HUD.
    clg_hud = {};

    // Chat cvars.
    hud_chat = clgi.CVar_Get( "hud_chat", "0", 0 );
    hud_chat_lines = clgi.CVar_Get( "hud_chat_lines", "4", 0 );
    hud_chat_time = clgi.CVar_Get( "hud_chat_time", "0", 0 );
    hud_chat_time->changed = cl_timeout_changed;
    hud_chat_time->changed( hud_chat_time );
    hud_chat_x = clgi.CVar_Get( "hud_chat_x", "8", 0 );
    hud_chat_y = clgi.CVar_Get( "hud_chat_y", "-64", 0 );

    // Crosshair cvars.
    hud_crosshair_red = clgi.CVar_Get( "hud_crosshair_red", "1", CVAR_ARCHIVE );
    hud_crosshair_red->changed = scr_hud_crosshair_changed;
    hud_crosshair_green = clgi.CVar_Get( "hud_crosshair_green", "1", CVAR_ARCHIVE );
    hud_crosshair_green->changed = scr_hud_crosshair_changed;
    hud_crosshair_blue = clgi.CVar_Get( "hud_crosshair_blue", "1", CVAR_ARCHIVE );
    hud_crosshair_blue->changed = scr_hud_crosshair_changed;
    hud_crosshair_alpha = clgi.CVar_Get( "hud_crosshair_alpha", "1", CVAR_ARCHIVE );
    hud_crosshair_alpha->changed = scr_hud_crosshair_changed;
    hud_crosshair_scale = clgi.CVar_Get( "hud_crosshair_scale", "1", CVAR_ARCHIVE );
    hud_crosshair_scale->changed = scr_hud_crosshair_changed;

    hud_crosshair_type = clgi.CVar_Get( "hud_crosshair_type", "1", CVAR_ARCHIVE );
    hud_crosshair_type->changed = scr_hud_crosshair_changed;

    #if 0
    hud_alpha = clgi.CVar_Get( "hud_alpha", "1", CVAR_ARCHIVE );
    hud_alpha->changed = []( cvar_t *self ) {
        self->value = clgi.CVar_ClampValue( self, 0.f, 1.f );
    };
    hud_alpha->changed( hud_alpha );
    hud_scale = clgi.CVar_Get( "hud_scale", "1", CVAR_ARCHIVE );
    hud_scale->changed = []( cvar_t *self ) {
        self->value = /*clg_hud.hud_scale = */clgi.R_ClampScale( self );
    };
    hud_scale->changed( hud_scale );
    #endif

    hud_damage_indicators = clgi.CVar_Get( "hud_damage_indicators", "1", 0 );
    hud_damage_indicator_time = clgi.CVar_Get( "hud_damage_indicator_time", "3000", 0 );
}
/**
*   @brief  Called by PF_SCR_ModeChanged(video mode changed), or scr_scale_changed, in order to
*           notify about the new HUD scale.
**/
void CLG_HUD_ModeChanged( const float newHudScale ) {
    clgi.screen->hud_scale = newHudScale;
}
/**
*   @brief  Called by PF_SetScreenHUDAlpha, in order to notify about the new HUD alpha.
**/
void CLG_HUD_AlphaChanged( const float newHudAlpha ) {
    clgi.screen->hud_alpha = newHudAlpha;
}
/**
*   @brief  Called upon when clearing client state.
**/
void CLG_HUD_ClearTargetHints() {
    clg_hud.targetHints = hud_state_t::hud_state_targethints_s{};
}
/**
*	@brief	Called when screen module is drawing its 2D overlay(s).
**/
void CLG_HUD_ScaleFrame( refcfg_t *refcfg ) {
    #if 0
    // Recalculate hud height/width.
    clg_hud.hud_real_height = refcfg->height;
    clg_hud.hud_real_width = refcfg->width;

    // Set general alpha scale.
    clgi.R_SetAlphaScale( scr_alpha->value );

    // Set HUD scale.
    clgi.R_SetScale( scr_scale->value );

    // Determine screen width and height based on hud_scale.
    clg_hud.hud_scaled_height = Q_rint( clg_hud.hud_real_height * clg_hud.hud_scale );
    clg_hud.hud_scaled_width = Q_rint( clg_hud.hud_real_width * clg_hud.hud_scale );
    #else
    // Recalculate hud height/width.
    clgi.screen->hudRealHeight = refcfg->height;
    clgi.screen->hudRealWidth = refcfg->width;

    // Determine screen width and height based on hud_scale.
    clgi.screen->hudScaledHeight = Q_rint( clgi.screen->hudRealHeight * clgi.screen->hud_scale );
    clgi.screen->hudScaledWidth = Q_rint( clgi.screen->hudRealWidth * clgi.screen->hud_scale );
    #endif
}

/**
*	@brief	Called when screen module is drawing its 2D overlay(s).
**/
void CLG_HUD_DrawFrame( refcfg_t *refcfg ) {
    clgi.R_ClearColor();
    // Set general alpha scale.
    //clgi.R_SetAlphaScale( clgi.screen->hud_alpha );
    // Set HUD scale.
    clgi.R_SetScale( hud_crosshair_scale->value );
    // Got an index of hint display, but its flagged as invisible.
    CLG_HUD_DrawCrosshair();
    // The rest of 2D elements share common alpha.
    clgi.R_ClearColor();
    // Set general alpha scale.
    clgi.R_SetAlphaScale( clgi.CVar_ClampValue( scr_alpha, 0, 1 ) );
    // Set HUD scale.
    //clgi.R_SetScale( 1.0 );
    // Display the use target hint information.
    CLG_HUD_DrawUseTargetHintInfos();
    // The rest of 2D elements share common alpha.
    clgi.R_ClearColor();
    clgi.R_SetAlphaScale( clgi.CVar_ClampValue( scr_alpha, 0, 1 ) );
    // Alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
    //clgi.R_SetScale( /*hud_scale->value * */scr_scale->value );
    // Weapon Name AND (Clip-)Ammo Indicators.
    CLG_HUD_DrawAmmoIndicators();
    clgi.R_ClearColor();
    clgi.R_SetAlphaScale( clgi.CVar_ClampValue( scr_alpha, 0, 1 ) );
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    clgi.R_SetScale( clgi.screen->hud_scale );
    // Health AND Armor Indicators.
    CLG_HUD_DrawHealthIndicators();
    clgi.R_ClearColor();
    clgi.R_SetAlphaScale( clgi.CVar_ClampValue( scr_alpha, 0, 1 ) );
    clgi.R_SetAlpha( scr_alpha->value );
    clgi.R_SetScale( 1.0f );
}

/**
*	@brief	Called when the screen module registers media.
**/
void CLG_HUD_RegisterScreenMedia( void ) {
    clg_hud_static.hud_element_background = clgi.R_RegisterPic( "hud/hud_elmnt_bg.tga" );

    clg_hud_static.hud_icon_health = clgi.R_RegisterPic( "hud/hud_icon_health.tga" );
    clg_hud_static.hud_icon_armor = clgi.R_RegisterPic( "hud/hud_icon_armor.tga" );

    clg_hud_static.hud_icon_slash = clgi.R_RegisterPic( "hud/hud_icon_slash.tga" );
    clg_hud_static.hud_icon_ammo_pistol = clgi.R_RegisterPic( "hud/hud_icon_ammo_pistol.tga" );

    for ( int32_t i = 0; i < 10; i++ ) {
        std::string numberPicName = "hud/hud_number" + std::to_string( i ) + "_l";
        clg_hud_static.hud_icon_numbers[ i ] = clgi.R_RegisterPic( numberPicName.c_str() );
    }

    // Precache crosshair.
    scr_hud_crosshair_changed( hud_crosshair_type );
}
/**
*   @brief  Called when screne module shutsdown.
**/
void CLG_HUD_Shutdown( void ) {

}



/**
*
* 
* 
*   HUD String Drawing: 
* 
* 
* 
**/
/**
*   @brief  A very cheap way of.. getting the total pixel length of display string tokens, yes.
**/
const int32_t HUD_GetTokenVectorDrawWidth( const std::vector<hud_usetarget_hint_token_t> &tokens ) {
    int32_t totalWidth = 0;
    int32_t addition = 1;
    for ( int32_t i = 0; i < tokens.size(); i++ ) {
        if ( i >= tokens.size() - 1 ) {
            addition = 0;
        }
        totalWidth += (tokens[ i ].value.size() + addition ) * CHAR_WIDTH;
    }
    return totalWidth;
}
/**
*   @brief  Get width in pixels of string.
**/
const int32_t HUD_GetStringDrawWidth( const char *str ) {
    return strlen( str ) * CHAR_WIDTH;
}
/**
*   @brief  
**/
const int32_t HUD_DrawString( const int32_t x, const int32_t y, const char *str ) {
    return clgi.R_DrawString( x, y, 0, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
const int32_t HUD_DrawString( const int32_t x, const int32_t y, const int32_t flags, const char *str ) {
    return clgi.R_DrawString( x, y, flags, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
const int32_t HUD_DrawAltString( const int32_t x, const int32_t y, const char *str ) {
    return clgi.R_DrawString( x, y, UI_XORCOLOR, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
const int32_t HUD_DrawAltString( const int32_t x, const int32_t y, const int32_t flags, const char *str ) {
    return clgi.R_DrawString( x, y, UI_XORCOLOR | flags, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
void HUD_DrawCenterString( const int32_t x, const int32_t y, const char *str ) {
    SCR_DrawStringMulti( x, y, UI_CENTER, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
void HUD_DrawAltCenterString( const int32_t x, const int32_t y, const char *str ) {
    SCR_DrawStringMulti( x, y, UI_CENTER | UI_XORCOLOR, MAX_STRING_CHARS, str, precache.screen.font_pic );
}



/**
*
*
*
*
*   Miscellaneous HUD Draw Functions:
*
*
*
*
**/
//! Minimal width and height before the HUD Element it's center, column and middle row are drawn.
static constexpr double HUD_ELEMENT_BACKGROUND_MIN_WIDTH    = 64.;
static constexpr double HUD_ELEMENT_BACKGROUND_MIN_HEIGHT   = 64.;
//! Half width and height of the minimal HUD Element background. These define the size of the corners.
static constexpr double HUD_ELEMENT_BACKGROUND_HALF_WIDTH   = HUD_ELEMENT_BACKGROUND_MIN_WIDTH / 2.;
static constexpr double HUD_ELEMENT_BACKGROUND_HALF_HEIGHT  = HUD_ELEMENT_BACKGROUND_MIN_HEIGHT / 2.;
/**
*   @brief Draws a background for HUD elements, such as health, ammo, etc. Operates as a simplified 9-grid system.
**/
void CLG_HUD_DrawElementBackground( const double &x, const double &y, const double &w, const double &h ) {
	static constexpr double HUD_ELEMENT_BACKGROUND_MIN_SIZE = 64.0;
    static constexpr double HUD_ELEMENT_BACKGROUND_CORNER_SIZE = 32.0;

    static constexpr double HUD_ELEMENT_BACKGROUND_WIDTH = 256.0;
    static constexpr double HUD_ELEMENT_BACKGROUND_HEIGHT = 256.0;

    // Set text color to orange.
    clgi.R_SetColor( clg_hud.colors.WHITE );
    // Apply generic crosshair alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );

    // The minimal width and height are 64x64.
    const double _w = w < HUD_ELEMENT_BACKGROUND_MIN_SIZE ? HUD_ELEMENT_BACKGROUND_MIN_SIZE : w;
    const double _h = h < HUD_ELEMENT_BACKGROUND_MIN_SIZE ? HUD_ELEMENT_BACKGROUND_MIN_SIZE : h;
    // Determine the X position.
    double elementX = x;
    // Determine the Y position.
    double elementY = y;
    // Determine the width and height.
    double elementWidth = _w;
    double elementHeight = _h;
        
    /**
    *   Top Row
    **/
    // Left - Corner
    clgi.R_DrawPicEx( 
        elementX, elementY, 
        HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
        clg_hud_static.hud_element_background,
        0, 0, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
    );
    // Center - Width Piece
    if ( elementWidth > HUD_ELEMENT_BACKGROUND_MIN_SIZE ) {
        // Top center piece.
        clgi.R_DrawPicEx(
            elementX + HUD_ELEMENT_BACKGROUND_CORNER_SIZE, elementY, elementWidth - HUD_ELEMENT_BACKGROUND_MIN_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
            clg_hud_static.hud_element_background,
            32/*random pow2 width from source image*/, 0, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
        );
    }
    // Right - Corner
    clgi.R_DrawPicEx( 
        elementX + ( elementWidth - HUD_ELEMENT_BACKGROUND_CORNER_SIZE ), elementY, 
        HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
        clg_hud_static.hud_element_background,
        HUD_ELEMENT_BACKGROUND_WIDTH - HUD_ELEMENT_BACKGROUND_CORNER_SIZE, 0, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
    );

    /**
    *   Center Row
    **/
    if ( elementHeight > HUD_ELEMENT_BACKGROUND_MIN_SIZE || elementWidth > HUD_ELEMENT_BACKGROUND_MIN_SIZE ) {
        // Left
        clgi.R_DrawPicEx(
            elementX, elementY + HUD_ELEMENT_BACKGROUND_CORNER_SIZE, 
            HUD_ELEMENT_BACKGROUND_CORNER_SIZE, elementHeight - HUD_ELEMENT_BACKGROUND_MIN_SIZE,
            clg_hud_static.hud_element_background,
            0, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
        );
        // Center - Width Piece.
        if ( elementWidth > HUD_ELEMENT_BACKGROUND_MIN_SIZE ) {
            // Bottom center piece.
            clgi.R_DrawPicEx(
                elementX + HUD_ELEMENT_BACKGROUND_CORNER_SIZE, elementY + HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
                elementWidth - HUD_ELEMENT_BACKGROUND_MIN_SIZE, ( elementHeight - HUD_ELEMENT_BACKGROUND_MIN_SIZE ),
                clg_hud_static.hud_element_background,
                32, HUD_ELEMENT_BACKGROUND_HEIGHT - HUD_ELEMENT_BACKGROUND_MIN_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
            );
        }
        // Right
        clgi.R_DrawPicEx(
            elementX + ( elementWidth - HUD_ELEMENT_BACKGROUND_CORNER_SIZE ), elementY + HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
            HUD_ELEMENT_BACKGROUND_CORNER_SIZE, elementHeight - HUD_ELEMENT_BACKGROUND_MIN_SIZE,
            clg_hud_static.hud_element_background,
            HUD_ELEMENT_BACKGROUND_WIDTH - HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
        );
    }

    /**
    *   Bottom Row
    **/
    // Left - Corner
    clgi.R_DrawPicEx( 
        elementX, elementY + ( elementHeight - HUD_ELEMENT_BACKGROUND_CORNER_SIZE ),
        HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
        clg_hud_static.hud_element_background,
        0, HUD_ELEMENT_BACKGROUND_HEIGHT - HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
    );
    // Center - Width Piece.
    if ( elementWidth > HUD_ELEMENT_BACKGROUND_MIN_SIZE ) {
        // Bottom center piece.
        clgi.R_DrawPicEx(
            elementX + 32, elementY + ( elementHeight - HUD_ELEMENT_BACKGROUND_CORNER_SIZE ), elementWidth - HUD_ELEMENT_BACKGROUND_MIN_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
            clg_hud_static.hud_element_background,
            32, HUD_ELEMENT_BACKGROUND_HEIGHT - HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
        );
    }
    // Right - Corner
    clgi.R_DrawPicEx( 
        elementX + ( elementWidth - HUD_ELEMENT_BACKGROUND_CORNER_SIZE ), elementY + ( elementHeight - HUD_ELEMENT_BACKGROUND_CORNER_SIZE ), 
        HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE,
        clg_hud_static.hud_element_background,
        HUD_ELEMENT_BACKGROUND_WIDTH - HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_HEIGHT - HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE, HUD_ELEMENT_BACKGROUND_CORNER_SIZE
    );

}

/**
*   @return Returns the next X offset for a HUD element context item positioned after the numbers last coordinates.
**/
const double CLG_HUD_GetWidthForElementNumberValue( const double numberPicWidth, const int32_t value ) {
    // Convert value to string.
    const std::string valueStr = std::to_string( value );
    // Get the length of the string.
    const int32_t valueStrLength = valueStr.length();
    // Add 32 for each missing character, so we can right focus the numbers.
    //const double centerX = w / 2.0;

    // Calculate the width of the string.
    const double valueStrWidth = valueStrLength * numberPicWidth;

    return valueStrWidth;
}
/**
*   @brief Draws the numbers 0-9 for on top of a HUD element background.
**/
const double CLG_HUD_DrawElementNumberValue( const double &startXOffset, const double &startY, const double &w, const double &h, const int32_t value ) {
	// Constant width/height for a number pic.
	const double numberPicWidth = w;
    const double numberPicHeight = h;

    // Convert value to string.
	const std::string valueStr = std::to_string( value );
	// Get the length of the string.
	const int32_t valueStrLength = valueStr.length();
	// Add 32 for each missing character, so we can right focus the numbers.
    //const double centerX = w / 2.0;

    // Calculate the width of the string.
    const double valueStrWidth = valueStrLength * numberPicWidth;
	// Returns X coordinate for next element after numbers are drawn.
	const double nextXOffset = startXOffset + ( valueStrLength * numberPicWidth );

    // Iterate over the string its characters.
    for ( int32_t i = 0; i < valueStrLength; i++ ) {
        // Get the character at the current index.
        const char numberChar = valueStr[ i ];
        // Convert the character to an integer.
        const int32_t number = numberChar - '0';
        // Make sure the number is in range.
        if ( number < 0 || number > 9 ) {
            continue; // Skip invalid characters.
        }

        // Calculate the X position for the number pic.
        //double numberX = _centerX - ( valueStrWidth / 4. ) + ( i * numberPicWidth / 2 );
        const double baseX = startXOffset;
        const double numberX = baseX + ( i * numberPicWidth );// -( numberPicWidth / 2.0 );
        // Draw the number pic.
        //CLG_HUD_DrawNumberPic( numberX, centerY, number );
        const qhandle_t numberPic = clg_hud_static.hud_icon_numbers[ number ];
        clgi.R_DrawStretchPic( numberX, startY,
            numberPicWidth, numberPicHeight, 
            numberPic 
		);    
	}

	return nextXOffset; // Return the next X offset.
}

/**
*
* 
*
*   HUD 'Regions' Draw Utilities:
*
* 
*
**/
/**
*   @brief  Does as it says it does.
**/
static void CLG_HUD_DrawOutlinedRectangle( const double &backGroundX, const double &backGroundY, const double &backGroundWidth, const double &backGroundHeight, const uint32_t fillColor, const uint32_t outlineColor ) {
    // Set global color to white
    clgi.R_SetColor( MakeColor( 255, 255, 255, 255 ) );
    // Draw bg color.
    clgi.R_DrawFill32f( backGroundX, backGroundY, backGroundWidth, backGroundHeight, fillColor );
    // Draw outlines:
    #if 0
    clgi.R_DrawFill32( backGroundX, backGroundY, 1., backGroundHeight, outlineColor ); // Left Line.
    clgi.R_DrawFill32( backGroundX, backGroundY, backGroundWidth, 1., outlineColor );  // Top Line.
    clgi.R_DrawFill32( backGroundX + backGroundWidth, backGroundY, 1., backGroundHeight + 1, outlineColor ); // Right Line.
    clgi.R_DrawFill32( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1., outlineColor ); // Bottom Line.
    #else
    // Left line. (Also covers first pixel of top line and bottom line.)
    clgi.R_DrawFill32f( backGroundX - 1., backGroundY - 1., 1., backGroundHeight + 2., outlineColor );
    // Right line. (Also covers last pixel of top line and bottom line.)
    clgi.R_DrawFill32f( backGroundX + backGroundWidth, backGroundY - 1., 1., backGroundHeight + 2., outlineColor );
    // Top line. (Skips first and last pixel, already covered by both the left and right lines.)
    clgi.R_DrawFill32f( backGroundX, backGroundY - 1., backGroundWidth, 1., outlineColor );
    // Bottom line. (Skips first and last pixel, already covered by both the left and right lines.)
    clgi.R_DrawFill32f( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1., outlineColor );
    #endif
}
/**
*   @brief  Does as it says it does.
**/
static void CLG_HUD_DrawCrosshairLine( const double &backGroundX, const double &backGroundY, const double &backGroundWidth, const double &backGroundHeight, const uint32_t fillColor, const uint32_t outlineColor, const bool thickCrossHair = false ) {

    if ( outlineColor ) {
        if ( thickCrossHair ) {
            // Left line. (Also covers first pixel of top line and bottom line.)
            clgi.R_DrawFill32f( backGroundX - 1., backGroundY - 1., 1., backGroundHeight + 2., outlineColor );
            // Right line. (Also covers last pixel of top line and bottom line.)
            clgi.R_DrawFill32f( backGroundX + backGroundWidth, backGroundY - 1., 1., backGroundHeight + 2., outlineColor );
            // Top line. (Skips first and last pixel, already covered by both the left and right lines.)
            clgi.R_DrawFill32f( backGroundX, backGroundY - 1., backGroundWidth, 1., outlineColor );
            // Bottom line. (Skips first and last pixel, already covered by both the left and right lines.)
            clgi.R_DrawFill32f( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1., outlineColor );
        } else {
            // Left line. (Also covers first pixel of top line and bottom line.)
            clgi.R_DrawFill32f( backGroundX, backGroundY, 1., backGroundHeight + 1., outlineColor );
            // Right line. (Also covers last pixel of top line and bottom line.)
            clgi.R_DrawFill32f( backGroundX + backGroundWidth, backGroundY, 1., backGroundHeight + 1., outlineColor );
            // Top line. (Skips first and last pixel, already covered by both the left and right lines.)
            clgi.R_DrawFill32f( backGroundX, backGroundY, backGroundWidth, 1., outlineColor );
            // Bottom line. (Skips first and last pixel, already covered by both the left and right lines.)
            clgi.R_DrawFill32f( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1., outlineColor );
        }
    }

    // Draw bg color.
    clgi.R_DrawFill32f( backGroundX + 1, backGroundY + 1, backGroundWidth - 1, backGroundHeight - 1, fillColor );
}



/**
*
*
*
*   HUD Chat:
*
*
*
**/
/**
*   @brief  Clear the chat HUD.
**/
void CLG_HUD_ClearChat_f( void ) {
    clg_hud.chatState = {};
    //memset( hud_chatlines, 0, sizeof( hud_chatlines ) );
    //hud_chathead = 0;
}

/**
*   @brief  Append text to chat HUD.
**/
void CLG_HUD_AddChatLine( const char *text ) {
    chatline_t *line;
    char *p;

    line = &clg_hud.chatState.chatlines[ clg_hud.chatState.chathead++ & HUD_CHAT_LINE_MASK ];
    Q_strlcpy( line->text, text, sizeof( line->text ) );
    line->time = QMTime::FromMilliseconds( clgi.GetRealTime() );

    p = strrchr( line->text, '\n' );
    if ( p )
        *p = 0;
}

/**
*   @brief  Draws chat hud to screen.
**/
void CLG_HUD_DrawChat( void ) {
    float alpha = 0;
    
    if ( hud_chat->integer == 0 ) {
        return;
    }

    int32_t x = hud_chat_x->integer;
    int32_t y = hud_chat_y->integer;

    int32_t flags = 0;
    if ( hud_chat->integer == 2 ) {
        flags = UI_ALTCOLOR;
    }
    // hud_x.
    if ( x < 0 ) {
        x += clgi.screen->hudRealWidth + 1;
        flags |= UI_RIGHT;
    } else {
        flags |= UI_LEFT;
    }
    // step.
    int32_t step = CHAR_HEIGHT;
    if ( y < 0 ) {
        y += clgi.screen->hudRealHeight - CHAR_HEIGHT + 1;
        step = -CHAR_HEIGHT;
    }

    int32_t lines = hud_chat_lines->integer;
    if ( lines > clg_hud.chatState.chathead ) {
        lines = clg_hud.chatState.chathead;
    }

    for ( int32_t i = 0; i < lines; i++ ) {
        chatline_t *line = &clg_hud.chatState.chatlines[ ( clg_hud.chatState.chathead - i - 1 ) & HUD_CHAT_LINE_MASK ];

        if ( hud_chat_time->integer ) {
            const float alpha = SCR_FadeAlpha( line->time.Milliseconds(), hud_chat_time->integer, 1000);
            if ( !alpha ) {
                break;
            }

            clgi.R_SetAlpha( alpha * scr_alpha->value );
            SCR_DrawString( x, y, flags, line->text );
            clgi.R_SetAlpha( scr_alpha->value );
        } else {
            SCR_DrawString( x, y, flags, line->text );
        }

        y += step;
    }
}



/**
*
*
* 
* 
*   HUD Crosshair:
*
* 
* 
*
**/
/**
*   @brief  Updates the hud's recoil status based on old/current frame difference and returns
*           the lerpFracion to use for scaling the recoil display.
*   @return Lerpfrac between last and current weapon recoil.
**/
static const double CLG_HUD_UpdateRecoilLerpScale( const double start, const double end, const QMTime &realTime, 
    const QMTime &duration, const double( *easeMethod )( const double &lerpFraction ) ) 
{
    // Delta Time.
    const QMTime recoilDeltaTime = realTime - clg_hud.crosshair.recoil.changeTime;

    // Method 01:
    #if 1
	// If we are at the start or end, return the respective value.
    if ( recoilDeltaTime <= 0_ms ) {
        return start;
    } else if ( recoilDeltaTime >= duration ) {
        return end;
    }

	// Get the ease fraction.
    const double easeFraction = (double)( recoilDeltaTime.Milliseconds() ) / duration.Milliseconds();
	// Ease to determine the ease factor for use with the lerp.
    const double easeFactor = easeMethod( easeFraction );

    // Lerp value.
	const double lerpFraction = QM_Lerp<double>( start, end, easeFactor );
    // Clamped lerp value.
	const double clampedLerpFraction = QM_Clamp<double>( lerpFraction, -1., 2. );
    // Return lerpfracion.
    return clampedLerpFraction;

    // Method 02:
    #else
    // If we are at the start or end, return the respective value.
    //if ( recoilDeltaTime + duration < 0_ms ) {
    if ( recoilDeltaTime < 0_ms ) {
        return start;
    } else if ( recoilDeltaTime >= duration ) {
        return end;
    }

    // Get the ease fraction.
    const double easeFraction = (double)( recoilDeltaTime.Milliseconds() ) / duration.Milliseconds();
    // Ease to determine the ease factor for use with the lerp.
    const double easeFactor = easeMethod( easeFraction );

    // Lerp value.
    const double lerpFraction = QM_Lerp( 0, 1, easeFactor );
    // Clamped lerp value.
    const double clampedLerpFraction = QM_Clampd( lerpFraction, 0, end );
    // Return lerpfracion.
    return clampedLerpFraction;
    #endif
}
/**
*   @brief  Renders a pic based crosshair.
**/
void CLG_HUD_DrawLineCrosshair( ) {
    /**
    *   (Default/Only, lol :-) -) Crosshair Configuration:
    **/
    // The base scale size of this crosshair.
    const double crosshairBaseScale = clgi.CVar_ClampValue( hud_crosshair_scale, 0.1f, 4.0f );;

    // Pixel 'radius', the offset from the center of the screen, for lines start origins to begin at.
    static constexpr double CROSSHAIR_ABSOLUTE_CENTER_ORIGIN_OFFSET= 3.;
    // Default Pixel Height/Width of the Horizontal Lines.
    static constexpr double CROSSHAIR_HORIZONTAL_HEIGHT = 2.;
    static constexpr double CROSSHAIR_HORIZONTAL_WIDTH = 8.;
    // Default Pixel Height/Width of the Vertical Lines.
    static constexpr double CROSSHAIR_VERTICAL_WIDTH = 2.;
    static constexpr double CROSSHAIR_VERTICAL_HEIGHT= 8.;

    /**
    *   Get Ease Fraction.
    **/
	// Get the real time.
    QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );
	// Check if the recoil value has changed.
    if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] != clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        // Decoded floating point recoil scale of current and last frame.
        clg_hud.crosshair.recoil.currentRecoil = half_to_float( clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] );
        clg_hud.crosshair.recoil.lastRecoil = half_to_float( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] );
        // Record time changed.
        clg_hud.crosshair.recoil.changeTime = realTime;
    }

    // If we're out of 'recoil', ease back in slowly.
    if ( clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] <= 0 ) {
		// Ease inwards in to the default crosshair values.
        clg_hud.crosshair.recoil.easeDuration = 100_ms;
        // Easing Inwards.
        clg_hud.crosshair.recoil.isEasingOut = false;
        clg_hud.crosshair.recoil.easeMethod = QM_QuarticEaseIn<double>;
    // If we're firing(ammo changed) do an ease out to the new recoil values.
    } else if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] > clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        // Ease outwards into the new crosshair values.
        clg_hud.crosshair.recoil.easeDuration = 25_ms;
        // Easing outwards.
        clg_hud.crosshair.recoil.isEasingOut = true;
        clg_hud.crosshair.recoil.easeMethod = QM_QuarticEaseOut<double>;
    }
    
	// Lerp the recoil scale based on the current and last frame's recoil values,
	// and the time it took to change from the last to the current frame, using the ease method.
    const double recoilScale = CLG_HUD_UpdateRecoilLerpScale(
        clg_hud.crosshair.recoil.lastRecoil, clg_hud.crosshair.recoil.currentRecoil,
        realTime, clg_hud.crosshair.recoil.easeDuration, 
        clg_hud.crosshair.recoil.easeMethod
    );

    /**
    *   Calculate the recoil derived offsets, as well as widths and heights.
    **/
    // Default idle crosshair values.
    double chCenterOffsetRadius = CROSSHAIR_ABSOLUTE_CENTER_ORIGIN_OFFSET * crosshairBaseScale;
    double chHorizontalWidth = CROSSHAIR_HORIZONTAL_WIDTH * crosshairBaseScale;
    double chVerticalHeight = CROSSHAIR_VERTICAL_HEIGHT * crosshairBaseScale;
    // Now to be adjusted by possible recoil.
    chCenterOffsetRadius = chCenterOffsetRadius + ( chCenterOffsetRadius * recoilScale );
    chHorizontalWidth = chHorizontalWidth + ( chHorizontalWidth * recoilScale );
    chVerticalHeight = chVerticalHeight + ( chVerticalHeight * recoilScale );

    // Developer Debug:
    #if 0
    if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] != clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        static uint64_t debugprintframe = 0;
        if ( debugprintframe != level.frameNumber ) {
            debugprintframe = level.frameNumber;
            clgi.Print( PRINT_DEVELOPER, "---------------------------------------------------\n" );
            clgi.Print( PRINT_DEVELOPER, "recoilScale(%f), lastRecoil(%f), currentRecoil(%f)\n",
                recoilScale,
                clg_hud.crosshair.recoil.lastRecoil, clg_hud.crosshair.recoil.currentRecoil  
            );
        }
    }
    #endif

    // Determine center x/y for crosshair display.
    const double center_x = ( clgi.screen->screenWidth ) / 2.;
    const double center_y = ( clgi.screen->screenHeight ) / 2.;

    // Apply overlay base color.
    clgi.R_SetColor( clg_hud.colors.WHITE );
    // Apply generic crosshair alpha.
    //clgi.R_SetAlphaScale( scr_alpha->value * clg_hud.crosshair.alpha );
    clgi.R_SetAlphaScale( scr_alpha->value );
    clgi.R_SetAlpha( clg_hud.crosshair.alpha );
    // Thicker crosshair? Type 2:
    const bool thickCrossHair = ( hud_crosshair_type->integer > 1 );

    // Draw the UP line.
    const double up_x = center_x - ( CROSSHAIR_VERTICAL_WIDTH / 2. );
    const double up_y = center_y - ( chCenterOffsetRadius + chVerticalHeight );
    CLG_HUD_DrawCrosshairLine( up_x, up_y, CROSSHAIR_VERTICAL_WIDTH, chVerticalHeight, clg_hud.crosshair.color.u32, clg_hud.colors.BLACK, thickCrossHair );

    // Draw the RIGHT line.
    const double right_x = center_x + ( chCenterOffsetRadius );//center_x - ( crosshairHorizontalWidth / 2. );
    const double right_y = center_y - ( CROSSHAIR_HORIZONTAL_HEIGHT / 2. ); //center_y - CROSSHAIR_ABS_CENTER_OFFSET + crosshairHorizontalHeight;
    CLG_HUD_DrawCrosshairLine( right_x, right_y, chHorizontalWidth, CROSSHAIR_HORIZONTAL_HEIGHT, clg_hud.crosshair.color.u32, clg_hud.colors.BLACK, thickCrossHair );

    // Draw the DOWN line.
    const double down_x = center_x - ( CROSSHAIR_VERTICAL_WIDTH / 2. );
    const double down_y = center_y + ( chCenterOffsetRadius );
    CLG_HUD_DrawCrosshairLine( down_x, down_y, CROSSHAIR_VERTICAL_WIDTH, chVerticalHeight, clg_hud.crosshair.color.u32, clg_hud.colors.BLACK, thickCrossHair );

    // Draw the LEFT line.
    const double left_x = center_x - ( chCenterOffsetRadius + chHorizontalWidth );//center_x - ( crosshairHorizontalWidth / 2. );
    const double left_y = center_y - ( CROSSHAIR_HORIZONTAL_HEIGHT / 2. ); //center_y - CROSSHAIR_ABS_CENTER_OFFSET + crosshairHorizontalHeight;
    CLG_HUD_DrawCrosshairLine( left_x, left_y, chHorizontalWidth, CROSSHAIR_HORIZONTAL_HEIGHT, clg_hud.crosshair.color.u32, clg_hud.colors.BLACK, thickCrossHair );

    // Reset color and alpha.
    clgi.R_SetColor( U32_WHITE );
    clgi.R_SetAlpha( scr_alpha->value );
}

/**
*	@brief  Renders the crosshair to screen.
**/
void CLG_HUD_DrawCrosshair( void ) {
    // Only display if enabled.
    if ( !hud_crosshair_type->integer ) {
        return;
    }
    if ( SCR_ShouldDrawPause() ) {
        return;
    }

    // Don't show when 'is aiming' weapon mode is true.
    if ( game.predictedState.currentPs.stats[ STAT_WEAPON_FLAGS ] & STAT_WEAPON_FLAGS_IS_AIMING ) {
        return;
    }

    // > 0 Will draw specified crosshair type.
    if ( hud_crosshair_type->integer >= 1 ) {
        CLG_HUD_DrawLineCrosshair();
    }

    //! WID: Seemed a cool idea, but, I really do not like it lol.
    #if 0
        // Draw crosshair damage displays.
    CLG_HUD_DrawDamageDisplays();
    #endif
}



/**
*
*
*
*
*   Healt, Armor and Clip/WeaponAmmo Indicators:
*
*
*
**/
/**
*	@brief  Renders the player's health and armor status to screen.
**/
static void CLG_HUD_DrawHealthIndicators() {
    /**
    *   Health Indicating Element:
    **/
    // Size details for the element.
	static constexpr double HUD_ELEMENT_OFFSET = 16.; // Offset from the bottom and left of the screen.
	static constexpr double HUD_ELEMENT_HALF_OFFSET = ( HUD_ELEMENT_OFFSET / 2. ); // Half offset.
    static constexpr double HUD_ELEMENT_PADDING = 12.; // Actual offset that takes the 'outer glow' space of the background in mind.
	static constexpr double HUD_ELEMENT_HEIGHT = 72.; // Height of the element.

    static constexpr double HUD_ELEMENT_NUMBERS_DEST_HEIGHT = 64.; // Height of the element.
    static constexpr double HUD_ELEMENT_NUMBERS_DEST_WIDTH  = 32.; // Height of the element.

	// Start X position for the health element.
    double backGroundStartX = HUD_ELEMENT_OFFSET;
    double backGroundStartY = clgi.screen->hudScaledHeight - ( HUD_ELEMENT_OFFSET + HUD_ELEMENT_HEIGHT );
    // Start X position for the health element.
    double iconStartX = backGroundStartX + HUD_ELEMENT_PADDING;
    double iconStartY = backGroundStartY + HUD_ELEMENT_PADDING;
	// We precalculate the width of the health element, so we can use it for rendering the background first.
    double numberStartX = iconStartX + 48. + 10;
    double numberStartY = ( backGroundStartY + 4 ); // ( HUD_ELEMENT_HEIGHT / 2. ) ) - ( HUD_ELEMENT_NUMBERS_DEST_HEIGHT / 2. );
	// Width of the health element.
    //double backGroundWidth = numberStartX + CLG_HUD_GetElementNumberValueSizePosition( HUD_ELEMENT_NUMBERS_DEST_WIDTH, clgi.client->frame.ps.stats[ STAT_HEALTH ] ) + HUD_ELEMENT_HALF_OFFSET;
    // Yes, 10 is a hard constant value, suck it.
    double backGroundWidth = ( numberStartX - backGroundStartX ) + 10 + CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, clgi.client->frame.ps.stats[ STAT_HEALTH ] );

    // Draw its background.
    CLG_HUD_DrawElementBackground( 
        backGroundStartX, backGroundStartY,
        backGroundWidth, HUD_ELEMENT_HEIGHT 
    );

    // Icon is reddish.
	clgi.R_SetColor( MakeColor( 217, 87, 99, 225 ) ); // == Mandy color in Krita Pixel
    // Apply generic crosshair alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
    // Draw the health icon.
    clgi.R_DrawStretchPic( 
        iconStartX, iconStartY,
        48, 48, // Icon size.
        clg_hud_static.hud_icon_health 
    );
    // Draw the health count numbers.
    clgi.R_SetColor( MakeColor( 255, 255, 255, 164 ) );
    // Apply generic crosshair alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
	// Note: We draw these from the right to left, so the X coordinate has to be set to the right side of the element.
    CLG_HUD_DrawElementNumberValue( 
        numberStartX, // Center X for the health numbers.
        numberStartY, // Center Y for the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_WIDTH, // Width of the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_HEIGHT, // Height of the health numbers.
        clgi.client->frame.ps.stats[ STAT_HEALTH ] // Health value to display.
	);
    //clgi.R_ClearColor();

    /**
	*   Armor Indicating Element:
    **/
    // Start X position for the armor element.
    backGroundStartX += backGroundWidth + 4;
    backGroundStartY = clgi.screen->hudScaledHeight - ( HUD_ELEMENT_OFFSET + HUD_ELEMENT_HEIGHT );
    // Start X position for the health element.
    iconStartX = backGroundStartX + HUD_ELEMENT_PADDING;
    iconStartY = backGroundStartY + HUD_ELEMENT_PADDING;
    // We precalculate the width of the health element, so we can use it for rendering the background first.
    numberStartX = iconStartX + 48. + 10;
    numberStartY = ( backGroundStartY + 4 ); // ( HUD_ELEMENT_HEIGHT / 2. ) ) - ( HUD_ELEMENT_NUMBERS_DEST_HEIGHT / 2. );
    // Width of the health element.
    //double backGroundWidth = numberStartX + CLG_HUD_GetElementNumberValueSizePosition( HUD_ELEMENT_NUMBERS_DEST_WIDTH, clgi.client->frame.ps.stats[ STAT_HEALTH ] ) + HUD_ELEMENT_HALF_OFFSET;
    // Yes, 10 is a hard constant value, suck it.
    backGroundWidth = ( numberStartX - backGroundStartX ) + 10 + CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, clgi.client->frame.ps.stats[ STAT_ARMOR ] );

    // Draw its background.
    CLG_HUD_DrawElementBackground(
        backGroundStartX, backGroundStartY,
        backGroundWidth, HUD_ELEMENT_HEIGHT
    );

    // Icon is reddish.
    clgi.R_SetColor( MakeColor( 99, 155, 255, 225 ) ); // == Cornflower color in Krita Pixel
    // Apply generic crosshair alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
    // Draw the health icon.
    clgi.R_DrawStretchPic(
        iconStartX, iconStartY,
        48, 48, // Icon size.
        clg_hud_static.hud_icon_armor
    );
    // Draw the health count numbers.
    clgi.R_SetColor( MakeColor( 255, 255, 255, 164 ) );
    // Apply generic crosshair alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
    // Note: We draw these from the right to left, so the X coordinate has to be set to the right side of the element.
    CLG_HUD_DrawElementNumberValue(
        numberStartX, // Center X for the health numbers.
        numberStartY, // Center Y for the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_WIDTH, // Width of the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_HEIGHT, // Height of the health numbers.
        clgi.client->frame.ps.stats[ STAT_ARMOR ] // Health value to display.
    );

    clgi.R_ClearColor();
}

/**
*	@brief  Renders the player's weapon name and (clip-)ammo status to screen.
**/
static void CLG_HUD_DrawAmmoIndicators() {
    if ( !clgi.client->frame.ps.gun.modelIndex ) {
        return;
    }

    // <Q2RTXP>: WID: Determine this based on the weapon that is selected.
    const qhandle_t ammoIcon = clg_hud_static.hud_icon_ammo_pistol;

    /**
    *   Clip Ammo Indicating Element:
    **/
    // Size details for the element.
    static constexpr double HUD_ELEMENT_OFFSET = 16.; // Offset from the bottom and left of the screen.
    static constexpr double HUD_ELEMENT_HALF_OFFSET = ( HUD_ELEMENT_OFFSET / 2. ); // Half offset.
    static constexpr double HUD_ELEMENT_PADDING = 12.; // Actual offset that takes the 'outer glow' space of the background in mind.
    static constexpr double HUD_ELEMENT_HEIGHT = 72.; // Height of the element.

    static constexpr double HUD_ELEMENT_NUMBERS_DEST_HEIGHT = 64.; // Height of the element.
    static constexpr double HUD_ELEMENT_NUMBERS_DEST_WIDTH = 32.; // Height of the element.

    double backGroundStartX = 0;
    double backGroundStartY = clgi.screen->hudScaledHeight - ( HUD_ELEMENT_OFFSET + HUD_ELEMENT_HEIGHT );
    
    double iconStartX = backGroundStartX + HUD_ELEMENT_PADDING;
    double iconStartY = backGroundStartY + HUD_ELEMENT_PADDING;
    
    double numberStartX = iconStartX + 48.;
    double numberStartY = ( backGroundStartY + 4 ); // ( HUD_ELEMENT_HEIGHT / 2. ) ) - ( HUD_ELEMENT_NUMBERS_DEST_HEIGHT / 2. );
    
    // Width of the element.
    double backGroundWidth = 48. + HUD_ELEMENT_OFFSET + 8 + CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, clgi.client->frame.ps.stats[ STAT_WEAPON_CLIP_AMMO ] );
    backGroundWidth += HUD_ELEMENT_NUMBERS_DEST_WIDTH; // Account for the "/" separator pic.
    backGroundWidth += CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, clgi.client->frame.ps.stats[ STAT_AMMO ] );

    // Calculate this here now we have the total estimated width.
	backGroundStartX = clgi.screen->hudScaledWidth - ( backGroundWidth + HUD_ELEMENT_OFFSET );

    //// Draw its background.
    CLG_HUD_DrawElementBackground(
        backGroundStartX, backGroundStartY,
        backGroundWidth, HUD_ELEMENT_HEIGHT
    );

    // Icon is orangie.
    clgi.R_SetColor( MakeColor( 217, 160, 102, 225 ) ); // == Orangie color in Krita Pixel
    // Apply generic HUD alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
    // Draw the health icon.
    clgi.R_DrawStretchPic(
        backGroundStartX + iconStartX, iconStartY,
        48, 48, // Icon size.
        ammoIcon
    );
    // Draw the health count numbers.
    clgi.R_SetColor( MakeColor( 255, 255, 255, 164 ) );
    // Apply generic HUD alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
    // Note: We draw these from the right to left, so the X coordinate has to be set to the right side of the element.
    CLG_HUD_DrawElementNumberValue(
        backGroundStartX + numberStartX, // Center X for the health numbers.
        numberStartY, // Center Y for the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_WIDTH, // Width of the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_HEIGHT, // Height of the health numbers.
        clgi.client->frame.ps.stats[ STAT_WEAPON_CLIP_AMMO ] // Health value to display.
    );
    /**
	*   The slash "/" separator pic, which is drawn between the clip ammo and the total ammo.
    **/
    // Draw the health count numbers.
    clgi.R_SetColor( MakeColor( 255, 255, 255, 164 ) );
    // Apply generic HUD alpha.
    clgi.R_SetAlpha( clgi.screen->hud_alpha );
    // Scale.
    clgi.R_SetScale( clgi.screen->hud_scale );
	// Draw the / pic.
    numberStartX += CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, clgi.client->frame.ps.stats[ STAT_WEAPON_CLIP_AMMO ] );
    // Draw the health icon.
    clgi.R_DrawStretchPic(
        backGroundStartX + numberStartX,
        numberStartY,
        HUD_ELEMENT_NUMBERS_DEST_WIDTH, 
        HUD_ELEMENT_NUMBERS_DEST_HEIGHT, // Icon size.
        clg_hud_static.hud_icon_slash
    );
	numberStartX += HUD_ELEMENT_NUMBERS_DEST_WIDTH;
    // Note: We draw these from the right to left, so the X coordinate has to be set to the right side of the element.
    CLG_HUD_DrawElementNumberValue(
        backGroundStartX + numberStartX, // Center X for the health numbers.
        numberStartY, // Center Y for the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_WIDTH, // Width of the health numbers.
        HUD_ELEMENT_NUMBERS_DEST_HEIGHT, // Height of the health numbers.
        clgi.client->frame.ps.stats[ STAT_AMMO ] // Health value to display.
    );
    clgi.R_ClearColor();
    clgi.R_SetAlpha( 1.f );
}



/**
*
*
*
*   Damage Indicators:
*
*
*
**/
static constexpr int32_t DAMAGE_ENTRY_BASE_SIZE = 32;

/**
*   @brief
**/
hud_damage_entry_t *CLG_HUD_AllocateDamageDisplay( const Vector3 &dir ) {
    hud_damage_entry_t *entry = clg_hud.damageDisplay.indicatorEntries;

    for ( int i = 0; i < hud_state_t::hud_state_damage_entries_s::MAX_DAMAGE_INDICATOR_ENTRIES; i++, entry++ ) {
        if ( entry->time <= clgi.GetRealTime() ) {
            goto new_entry;
        }

        float dot = QM_Vector3DotProduct( entry->dir, dir ); // DotProduct( entry->dir, dir );

        if ( dot >= 0.95f ) {
            return entry;
        }
    }

    entry = clg_hud.damageDisplay.indicatorEntries;;

new_entry:
    entry->damage = 0;
    VectorClear( entry->color );
    return entry;
}
/**
*   @brief  Adds a damage indicator for the given damage using the given color pointing at given direction.
**/
void CLG_HUD_AddToDamageDisplay( const int32_t damage, const Vector3 &color, const Vector3 &dir ) {
    if ( !hud_damage_indicators->integer ) {
        return;
    }

    hud_damage_entry_t *entry = CLG_HUD_AllocateDamageDisplay( dir );

    entry->damage += damage;
    entry->color += color;
    entry->color = QM_Vector3Normalize( entry->color );
    entry->dir = dir;
    entry->time = clgi.GetRealTime() + hud_damage_indicator_time->integer;
}
/**
*   @brief
**/
void CLG_HUD_DrawDamageDisplays( void ) {
    for ( int32_t i = 0; i < hud_state_t::hud_state_damage_entries_s::MAX_DAMAGE_INDICATOR_ENTRIES; i++ ) {
        hud_damage_entry_t *entry = &clg_hud.damageDisplay.indicatorEntries[ i ];

        if ( entry->time <= clgi.GetRealTime() ) {
            continue;
        }

        const double lerpFraction = ( entry->time - clgi.GetRealTime() ) / hud_damage_indicator_time->value;

        float clientYawAngle = game.predictedState.currentPs.viewangles[ YAW ];
        //vec3_t angles;
        //vectoangles2( entry->dir, angles );
        //Vector3 angles = QM_Vector3ToAngles( entry->dir );
        float damageYawAngle = QM_Vector3ToYaw( entry->dir );// angles[ YAW ];
        float yawDifference = damageYawAngle/*- 180*/;// ( clientYawAngle - damageYawAngle ) - 180;
        if ( yawDifference > 180 ) {
            yawDifference -= 360;
        }
        if ( yawDifference < -180 ) {
            yawDifference += 360;
        }
        //yawDifference = DEG2RAD( yawDifference );

        clgi.R_SetColor( MakeColor(
            (int)( entry->color[ 0 ] * 255.f ),
            (int)( entry->color[ 1 ] * 255.f ),
            (int)( entry->color[ 2 ] * 255.f ),
            (int)( lerpFraction * 255.f ) ) );

        const int32_t size_x = std::min( clg_hud.damageDisplay.indicatorWidth, ( 32/*DAMAGE_ENTRY_BASE_SIZE */ * entry->damage ) );
        const int32_t size_y = std::min( clg_hud.damageDisplay.indicatorHeight, ( 32/*DAMAGE_ENTRY_BASE_SIZE*/ * entry->damage ) );

        const int32_t x = ( clgi.screen->hudRealWidth - clg_hud.damageDisplay.indicatorWidth ) / 2;
        const int32_t y = ( clgi.screen->hudRealHeight - clg_hud.damageDisplay.indicatorHeight ) / 2;


        //clgi.R_DrawStretchPic( x, y, clgi.screen->damage_display_height, clgi.screen->damage_display_width, clgi.screen->damage_display_pic );
        clgi.R_DrawRotateStretchPic( x, y, 
            size_x, clg_hud.damageDisplay.indicatorHeight,
            yawDifference, 
            ( clg_hud.damageDisplay.indicatorWidth / 2 ),
            ( clg_hud.damageDisplay.indicatorHeight / 2 ), 
            precache.screen.damage_display_pic 
        );
    }
}