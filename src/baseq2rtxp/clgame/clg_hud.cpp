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
#include "clgame/hud/clg_hud_weaponmenu.h"
#include "shared/math/qm_easing_methods.hpp"
#include "shared/math/qm_easing_state.hpp"



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
static void clg_timeout_changed( cvar_t *self ) {
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
    hud_chat_time->changed = clg_timeout_changed;
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
	#endif
	#if 1
    cvar_t *hud_scale = clgi.CVar_Get( "hud_scale", "1", CVAR_ARCHIVE );
	hud_scale->changed = []( cvar_t *self ) {
        self->value = clgi.screen->hud_scale = clgi.R_ClampScale( self );
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
	// Has to be valid.
	if ( !clgi.client ) {
		return;
	}

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

    // Weapon Selection HUD Menu (Half-Life 1 style).
    CLG_WeaponMenu_Draw();

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
    return SCR_DrawString( x, y, 0, str );
}
/**
*   @brief
**/
const int32_t HUD_DrawString( const int32_t x, const int32_t y, const int32_t flags, const char *str ) {
    return SCR_DrawStringEx( x, y, flags, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
const int32_t HUD_DrawAltString( const int32_t x, const int32_t y, const char *str ) {
	clgi.R_SetColor( U32_ORANGE );
	int32_t retval = SCR_DrawStringEx( x, y, UI_XORCOLOR, MAX_STRING_CHARS, str, precache.screen.font_pic );
	clgi.R_ClearColor();
	return retval;
}
/**
*   @brief
**/
const int32_t HUD_DrawAltString( const int32_t x, const int32_t y, const int32_t flags, const char *str ) {
	clgi.R_SetColor( U32_ORANGE );
    int32_t retval = SCR_DrawStringEx( x, y, UI_XORCOLOR | flags, MAX_STRING_CHARS, str, precache.screen.font_pic );
	clgi.R_ClearColor();
	return retval;
}
/**
*   @brief
**/
void HUD_DrawCenterString( const int32_t x, const int32_t y, const char *str ) {
    SCR_DrawStringMultiEx( x, y, UI_CENTER, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
void HUD_DrawAltCenterString( const int32_t x, const int32_t y, const char *str ) {
    SCR_DrawStringMultiEx( x, y, UI_CENTER | UI_XORCOLOR, MAX_STRING_CHARS, str, precache.screen.font_pic );
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
			SCR_DrawStringEx( x, y, flags, Q_strnlen( line->text, MAX_STRING_CHARS ), line->text, clgi.screen->font_pic );
            clgi.R_SetAlpha( scr_alpha->value );
        } else {
            SCR_DrawStringEx( x, y, flags, Q_strnlen( line->text, MAX_STRING_CHARS ), line->text, clgi.screen->font_pic );
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
*	Palette Definitions for Half-Life 1 Inspired Bottom HUD (#df7126):
**/
//! Primary base amber/orange color (#df7126).
static constexpr uint32_t COLOR_HUD_HL1_ORANGE_BASE	= MakeColor( 223, 113, 38, 220 );
//! Bright amber/orange color for pulse peaks and high visibility.
static constexpr uint32_t COLOR_HUD_HL1_ORANGE_BRIGHT	= MakeColor( 255, 175, 75, 255 );
//! Amber/orange glow color.
static constexpr uint32_t COLOR_HUD_HL1_ORANGE_GLOW		= MakeColor( 223, 113, 38, 200 );
//! Dim amber/orange color for subtle border strokes.
static constexpr uint32_t COLOR_HUD_HL1_ORANGE_DIM		= MakeColor( 223, 113, 38, 140 );
//! Dark translucent amber background container fill.
static constexpr uint32_t COLOR_HUD_HL1_BG				= MakeColor( 35, 18, 8, 180 );

//! Warning red color for critical health or damage pulses (#d95763).
static constexpr uint32_t COLOR_HUD_RED_WARNING			= MakeColor( 217, 87, 99, 230 );
//! Warning red glow color for damage pulse glow.
static constexpr uint32_t COLOR_HUD_RED_GLOW			= MakeColor( 217, 87, 99, 200 );
//! Dark translucent reddish background container fill for warning state.
static constexpr uint32_t COLOR_HUD_RED_BG				= MakeColor( 40, 14, 14, 180 );

//! State tracking for bottom HUD glow transitions and ease decay.
struct hud_stat_pulse_t {
	//! Previous value used to detect transitions (-1 indicates uninitialized).
	int32_t		lastValue = -1;
	//! Ease state for the transition glow decay.
	QMEaseState	easeState = {};
	//! Color of the active glow pulse.
	uint32_t	glowColor = 0;
};

//! Static pulse states for bottom HUD indicators.
static struct {
	//! Pulse tracking for health indicator.
	hud_stat_pulse_t	health;
	//! Pulse tracking for armor indicator.
	hud_stat_pulse_t	armor;
	//! Pulse tracking for weapon ammo indicator.
	hud_stat_pulse_t	ammo;
} s_hud_pulses = {};

/**
*	@brief	Interpolates linearly between two RGBA 32-bit packed colors.
*	@param	c1		Start color.
*	@param	c2		End color.
*	@param	frac	Interpolation fraction clamped to [0.0, 1.0].
*	@return	Interpolated 32-bit packed RGBA color.
**/
static inline uint32_t ColorLerp( const uint32_t c1, const uint32_t c2, const float frac ) {
	const float f = QM_Clamp( frac, 0.0f, 1.0f );
	const float invF = 1.0f - f;

	const uint32_t r = static_cast<uint32_t>( ( ( c1 >> 0 ) & 0xFF ) * invF + ( ( c2 >> 0 ) & 0xFF ) * f );
	const uint32_t g = static_cast<uint32_t>( ( ( c1 >> 8 ) & 0xFF ) * invF + ( ( c2 >> 8 ) & 0xFF ) * f );
	const uint32_t b = static_cast<uint32_t>( ( ( c1 >> 16 ) & 0xFF ) * invF + ( ( c2 >> 16 ) & 0xFF ) * f );
	const uint32_t a = static_cast<uint32_t>( ( ( c1 >> 24 ) & 0xFF ) * invF + ( ( c2 >> 24 ) & 0xFF ) * f );

	return MakeColor( r, g, b, a );
}

/**
*	@brief	Draw a Half-Life 1 styled HUD container box with corner bracket accents and optional glow.
*	@param	x			Left coordinate.
*	@param	y			Top coordinate.
*	@param	w			Width.
*	@param	h			Height.
*	@param	fillColor	Interior background color.
*	@param	strokeColor	Border / bracket color.
*	@param	glowColor	Outer glow color.
*	@param	glowRadius	Outer glow radius.
**/
static void CLG_HUD_DrawHL1Container( const double x, const double y, const double w, const double h,
	const uint32_t fillColor, const uint32_t strokeColor, const uint32_t glowColor, const float glowRadius ) {
	/**
	*	Set outer glow if active.
	**/
	// Only apply outer glow if radius is greater than threshold.
	if ( glowRadius > 0.1f ) {
		clgi.R_SetOuterGlow( glowColor, glowRadius );
	}

	/**
	*	Draw filled background rectangle with subtle rounded corners.
	**/
	// Apply thin border stroke and corner radius.
	clgi.R_SetStroke( strokeColor, 1.0f );
	clgi.R_SetCornerRadius( 2.0f );
	// Draw the background fill.
	clgi.R_DrawFill32( x, y, w, h, fillColor );
	// Clear the 2D rendering style to avoid leaking state.
	clgi.R_ClearStyle();

	/**
	*	Draw HL1 corner brackets [ ] for authentic visual aesthetic.
	**/
	// Dimensions for the corner bracket accents.
	constexpr double bracketLen = 6.0;
	constexpr double bracketThick = 2.0;

	// Top-left bracket corner.
	clgi.R_DrawFill32( x, y, bracketLen, bracketThick, strokeColor );
	clgi.R_DrawFill32( x, y, bracketThick, bracketLen, strokeColor );

	// Top-right bracket corner.
	clgi.R_DrawFill32( x + w - bracketLen, y, bracketLen, bracketThick, strokeColor );
	clgi.R_DrawFill32( x + w - bracketThick, y, bracketThick, bracketLen, strokeColor );

	// Bottom-left bracket corner.
	clgi.R_DrawFill32( x, y + h - bracketThick, bracketLen, bracketThick, strokeColor );
	clgi.R_DrawFill32( x, y + h - bracketLen, bracketThick, bracketLen, strokeColor );

	// Bottom-right bracket corner.
	clgi.R_DrawFill32( x + w - bracketLen, y + h - bracketThick, bracketLen, bracketThick, strokeColor );
	clgi.R_DrawFill32( x + w - bracketThick, y + h - bracketLen, bracketThick, bracketLen, strokeColor );
}

/**
*	@brief	Renders the player's health and armor status to screen in Half-Life 1 style with value-change glow pulses.
**/
static void CLG_HUD_DrawHealthIndicators() {
	/**
	*	Retrieve current real-time and player stat values.
	**/
	// Real-time timestamp used to calculate easing progress.
	const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

	// Player health and armor stat values from current frame.
	const int32_t currentHealth = clgi.client->frame.ps.stats[ STAT_HEALTH ];
	const int32_t currentArmor = clgi.client->frame.ps.stats[ STAT_ARMOR ];

	/**
	*	Health Indicating Element Layout Dimensions:
	**/
	// Offset from the bottom and left edges of the screen.
	static constexpr double HUD_ELEMENT_OFFSET = 16.0;
	// Padding inside the element container box.
	static constexpr double HUD_ELEMENT_PADDING = 12.0;
	// Height of the HUD element container box.
	static constexpr double HUD_ELEMENT_HEIGHT = 72.0;

	// Dimensions of each digit sprite in the indicator.
	static constexpr double HUD_ELEMENT_NUMBERS_DEST_HEIGHT = 64.0;
	static constexpr double HUD_ELEMENT_NUMBERS_DEST_WIDTH  = 32.0;

	// Start X and Y coordinates for health element container box.
	double backGroundStartX = HUD_ELEMENT_OFFSET;
	const double backGroundStartY = clgi.screen->hudScaledHeight - ( HUD_ELEMENT_OFFSET + HUD_ELEMENT_HEIGHT );

	// Icon origin.
	const double iconStartX = backGroundStartX + HUD_ELEMENT_PADDING;
	const double iconStartY = backGroundStartY + HUD_ELEMENT_PADDING;

	// Number digits origin (icon width 48 + 10 px padding).
	const double numberStartX = iconStartX + 48.0 + 10.0;
	const double numberStartY = backGroundStartY + 4.0;

	// Width of the health container box based on digit count.
	const double backGroundWidth = ( numberStartX - backGroundStartX ) + 10.0 + CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, currentHealth );

	/**
	*	Health Value Transition & Glow Pulse Detection:
	**/
	// If health value has changed, trigger a glow pulse.
	if ( s_hud_pulses.health.lastValue != -1 && s_hud_pulses.health.lastValue != currentHealth ) {
		// If health decreased, player suffered damage -> red warning pulse.
		if ( currentHealth < s_hud_pulses.health.lastValue ) {
			s_hud_pulses.health.glowColor = COLOR_HUD_RED_GLOW;
		} else {
			// Health increased (healed) -> bright amber pulse.
			s_hud_pulses.health.glowColor = COLOR_HUD_HL1_ORANGE_BRIGHT;
		}
		// Initiate 400ms ease out decay.
		s_hud_pulses.health.easeState = QMEaseState::new_ease_state( realTime, 400_ms );
	}
	// Record current health as last value for future frame comparisons.
	s_hud_pulses.health.lastValue = currentHealth;

	/**
	*	Calculate Health Ease-Out Pulse Factor (1.0 -> 0.0):
	**/
	double healthPulseFactor = 0.0;
	if ( s_hud_pulses.health.easeState.GetEaseMode() != QMEaseState::QM_EASE_STATE_MODE_DONE ) {
		healthPulseFactor = 1.0 - s_hud_pulses.health.easeState.EaseOut( realTime, QM_QuadraticEaseOut<double> );
		healthPulseFactor = QM_Clamp( healthPulseFactor, 0.0, 1.0 );
	}

	/**
	*	Determine Health Colors Based on Status and Pulse:
	**/
	// Critical health threshold (<= 25).
	const bool isLowHealth = ( currentHealth <= 25 );
	uint32_t healthColor = isLowHealth ? COLOR_HUD_RED_WARNING : COLOR_HUD_HL1_ORANGE_BASE;
	const uint32_t healthBg = isLowHealth ? COLOR_HUD_RED_BG : COLOR_HUD_HL1_BG;
	uint32_t healthStroke = isLowHealth ? COLOR_HUD_RED_WARNING : COLOR_HUD_HL1_ORANGE_DIM;

	// Blend pulse glow color if actively pulsing.
	if ( healthPulseFactor > 0.001 ) {
		healthColor = ColorLerp( healthColor, s_hud_pulses.health.glowColor, static_cast<float>( healthPulseFactor ) );
		healthStroke = ColorLerp( healthStroke, s_hud_pulses.health.glowColor, static_cast<float>( healthPulseFactor ) );
	}
	const float healthGlowRadius = static_cast<float>( 8.0 * healthPulseFactor );

	/**
	*	Render Health Container Box:
	**/
	CLG_HUD_DrawHL1Container(
		backGroundStartX, backGroundStartY,
		backGroundWidth, HUD_ELEMENT_HEIGHT,
		healthBg, healthStroke, s_hud_pulses.health.glowColor, healthGlowRadius
	);

	/**
	*	Render Health '+' Icon (hud_icon_health):
	**/
	clgi.R_SetColor( healthColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	clgi.R_DrawStretchPic(
		iconStartX, iconStartY,
		48.0, 48.0,
		clg_hud_static.hud_icon_health
	);

	/**
	*	Render Health Number Digits:
	**/
	clgi.R_SetColor( healthColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	CLG_HUD_DrawElementNumberValue(
		numberStartX,
		numberStartY,
		HUD_ELEMENT_NUMBERS_DEST_WIDTH,
		HUD_ELEMENT_NUMBERS_DEST_HEIGHT,
		currentHealth
	);

	/**
	*	Armor Indicating Element (HEV Suit):
	**/
	// Position armor container box directly adjacent to health container box.
	backGroundStartX += backGroundWidth + 8.0;

	// Armor icon origin.
	const double armorIconStartX = backGroundStartX + HUD_ELEMENT_PADDING;
	const double armorIconStartY = backGroundStartY + HUD_ELEMENT_PADDING;

	// Armor number digits origin.
	const double armorNumberStartX = armorIconStartX + 48.0 + 10.0;
	const double armorNumberStartY = backGroundStartY + 4.0;

	// Width of armor container box based on digit count.
	const double armorBackGroundWidth = ( armorNumberStartX - backGroundStartX ) + 10.0 + CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, currentArmor );

	/**
	*	Armor Value Transition & Glow Pulse Detection:
	**/
	// If armor value has changed, trigger a glow pulse.
	if ( s_hud_pulses.armor.lastValue != -1 && s_hud_pulses.armor.lastValue != currentArmor ) {
		s_hud_pulses.armor.glowColor = COLOR_HUD_HL1_ORANGE_BRIGHT;
		// Initiate 400ms ease out decay.
		s_hud_pulses.armor.easeState = QMEaseState::new_ease_state( realTime, 400_ms );
	}
	// Record current armor as last value for future frame comparisons.
	s_hud_pulses.armor.lastValue = currentArmor;

	/**
	*	Calculate Armor Ease-Out Pulse Factor (1.0 -> 0.0):
	**/
	double armorPulseFactor = 0.0;
	if ( s_hud_pulses.armor.easeState.GetEaseMode() != QMEaseState::QM_EASE_STATE_MODE_DONE ) {
		armorPulseFactor = 1.0 - s_hud_pulses.armor.easeState.EaseOut( realTime, QM_QuadraticEaseOut<double> );
		armorPulseFactor = QM_Clamp( armorPulseFactor, 0.0, 1.0 );
	}

	/**
	*	Determine Armor Colors Based on Status and Pulse:
	**/
	uint32_t armorColor = ( currentArmor > 0 ) ? COLOR_HUD_HL1_ORANGE_BASE : COLOR_HUD_HL1_ORANGE_DIM;
	const uint32_t armorBg = COLOR_HUD_HL1_BG;
	uint32_t armorStroke = COLOR_HUD_HL1_ORANGE_DIM;

	// Blend pulse glow color if actively pulsing.
	if ( armorPulseFactor > 0.001 ) {
		armorColor = ColorLerp( armorColor, s_hud_pulses.armor.glowColor, static_cast<float>( armorPulseFactor ) );
		armorStroke = ColorLerp( armorStroke, s_hud_pulses.armor.glowColor, static_cast<float>( armorPulseFactor ) );
	}
	const float armorGlowRadius = static_cast<float>( 8.0 * armorPulseFactor );

	/**
	*	Render Armor Container Box:
	**/
	CLG_HUD_DrawHL1Container(
		backGroundStartX, backGroundStartY,
		armorBackGroundWidth, HUD_ELEMENT_HEIGHT,
		armorBg, armorStroke, s_hud_pulses.armor.glowColor, armorGlowRadius
	);

	/**
	*	Render Armor HEV Icon (hud_icon_armor):
	**/
	clgi.R_SetColor( armorColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	clgi.R_DrawStretchPic(
		armorIconStartX, armorIconStartY,
		48.0, 48.0,
		clg_hud_static.hud_icon_armor
	);

	/**
	*	Render Armor Number Digits:
	**/
	clgi.R_SetColor( armorColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	CLG_HUD_DrawElementNumberValue(
		armorNumberStartX,
		armorNumberStartY,
		HUD_ELEMENT_NUMBERS_DEST_WIDTH,
		HUD_ELEMENT_NUMBERS_DEST_HEIGHT,
		currentArmor
	);

	// Reset renderer color.
	clgi.R_ClearColor();
}

/**
*	@brief	Renders the player's weapon (clip-)ammo status to screen in Half-Life 1 style with value-change glow pulses.
**/
static void CLG_HUD_DrawAmmoIndicators() {
	/**
	*	Sanity check: ensure player is currently holding a valid weapon.
	**/
	if ( !clgi.client->frame.ps.gun.modelIndex ) {
		return;
	}

	// Real-time timestamp used to calculate easing progress.
	const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

	// Fetch current weapon clip ammo and reserve ammo.
	const int32_t currentClip = clgi.client->frame.ps.stats[ STAT_WEAPON_CLIP_AMMO ];
	const int32_t currentReserve = clgi.client->frame.ps.stats[ STAT_AMMO ];

	// Active ammo icon (pistol ammo default).
	const qhandle_t ammoIcon = clg_hud_static.hud_icon_ammo_pistol;

	/**
	*	Clip Ammo Indicating Element Layout Dimensions:
	**/
	static constexpr double HUD_ELEMENT_OFFSET = 16.0;
	static constexpr double HUD_ELEMENT_PADDING = 12.0;
	static constexpr double HUD_ELEMENT_HEIGHT = 72.0;

	static constexpr double HUD_ELEMENT_NUMBERS_DEST_HEIGHT = 64.0;
	static constexpr double HUD_ELEMENT_NUMBERS_DEST_WIDTH = 32.0;

	// Calculate total width of ammo box: icon (48) + padding + clip digits + slash (32) + reserve digits.
	double backGroundWidth = 48.0 + ( HUD_ELEMENT_PADDING * 2.0 ) + 8.0;
	backGroundWidth += CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, currentClip );
	backGroundWidth += HUD_ELEMENT_NUMBERS_DEST_WIDTH; // Slash "/" icon width.
	backGroundWidth += CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, currentReserve );

	// Anchor ammo box to the bottom-right corner of the HUD.
	const double backGroundStartX = clgi.screen->hudScaledWidth - ( backGroundWidth + HUD_ELEMENT_OFFSET );
	const double backGroundStartY = clgi.screen->hudScaledHeight - ( HUD_ELEMENT_OFFSET + HUD_ELEMENT_HEIGHT );

	// Icon origin.
	const double iconStartX = backGroundStartX + HUD_ELEMENT_PADDING;
	const double iconStartY = backGroundStartY + HUD_ELEMENT_PADDING;

	// Number digits origin.
	double numberStartX = iconStartX + 48.0 + 8.0;
	const double numberStartY = backGroundStartY + 4.0;

	/**
	*	Ammo Value Transition & Glow Pulse Detection:
	**/
	// Pack clip and reserve into a single 32-bit key to detect either changing.
	const int32_t combinedAmmo = ( ( currentClip & 0xFFFF ) << 16 ) | ( currentReserve & 0xFFFF );
	if ( s_hud_pulses.ammo.lastValue != -1 && s_hud_pulses.ammo.lastValue != combinedAmmo ) {
		// If clip ammo is empty, trigger red warning pulse; otherwise bright amber pulse.
		if ( currentClip == 0 ) {
			s_hud_pulses.ammo.glowColor = COLOR_HUD_RED_GLOW;
		} else {
			s_hud_pulses.ammo.glowColor = COLOR_HUD_HL1_ORANGE_BRIGHT;
		}
		// Initiate 350ms ease out decay.
		s_hud_pulses.ammo.easeState = QMEaseState::new_ease_state( realTime, 350_ms );
	}
	// Record current ammo as last value for future frame comparisons.
	s_hud_pulses.ammo.lastValue = combinedAmmo;

	/**
	*	Calculate Ammo Ease-Out Pulse Factor (1.0 -> 0.0):
	**/
	double ammoPulseFactor = 0.0;
	if ( s_hud_pulses.ammo.easeState.GetEaseMode() != QMEaseState::QM_EASE_STATE_MODE_DONE ) {
		ammoPulseFactor = 1.0 - s_hud_pulses.ammo.easeState.EaseOut( realTime, QM_QuadraticEaseOut<double> );
		ammoPulseFactor = QM_Clamp( ammoPulseFactor, 0.0, 1.0 );
	}

	/**
	*	Determine Ammo Colors Based on Status and Pulse:
	**/
	const bool isOutOfAmmo = ( currentClip == 0 );
	uint32_t ammoColor = isOutOfAmmo ? COLOR_HUD_RED_WARNING : COLOR_HUD_HL1_ORANGE_BASE;
	const uint32_t ammoBg = isOutOfAmmo ? COLOR_HUD_RED_BG : COLOR_HUD_HL1_BG;
	uint32_t ammoStroke = isOutOfAmmo ? COLOR_HUD_RED_WARNING : COLOR_HUD_HL1_ORANGE_DIM;

	// Blend pulse glow color if actively pulsing.
	if ( ammoPulseFactor > 0.001 ) {
		ammoColor = ColorLerp( ammoColor, s_hud_pulses.ammo.glowColor, static_cast<float>( ammoPulseFactor ) );
		ammoStroke = ColorLerp( ammoStroke, s_hud_pulses.ammo.glowColor, static_cast<float>( ammoPulseFactor ) );
	}
	const float ammoGlowRadius = static_cast<float>( 8.0 * ammoPulseFactor );

	/**
	*	Render Ammo Container Box:
	**/
	CLG_HUD_DrawHL1Container(
		backGroundStartX, backGroundStartY,
		backGroundWidth, HUD_ELEMENT_HEIGHT,
		ammoBg, ammoStroke, s_hud_pulses.ammo.glowColor, ammoGlowRadius
	);

	/**
	*	Render Ammo Bullet Icon (hud_icon_ammo_pistol):
	**/
	clgi.R_SetColor( ammoColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	clgi.R_DrawStretchPic(
		iconStartX, iconStartY,
		48.0, 48.0,
		ammoIcon
	);

	/**
	*	Render Clip Ammo Digits:
	**/
	clgi.R_SetColor( ammoColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	CLG_HUD_DrawElementNumberValue(
		numberStartX,
		numberStartY,
		HUD_ELEMENT_NUMBERS_DEST_WIDTH,
		HUD_ELEMENT_NUMBERS_DEST_HEIGHT,
		currentClip
	);

	/**
	*	Render Slash "/" Separator (hud_icon_slash):
	**/
	numberStartX += CLG_HUD_GetWidthForElementNumberValue( HUD_ELEMENT_NUMBERS_DEST_WIDTH, currentClip );
	clgi.R_SetColor( ammoColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	clgi.R_DrawStretchPic(
		numberStartX,
		numberStartY,
		HUD_ELEMENT_NUMBERS_DEST_WIDTH,
		HUD_ELEMENT_NUMBERS_DEST_HEIGHT,
		clg_hud_static.hud_icon_slash
	);

	/**
	*	Render Reserve Ammo Digits:
	**/
	numberStartX += HUD_ELEMENT_NUMBERS_DEST_WIDTH;
	clgi.R_SetColor( ammoColor );
	clgi.R_SetAlpha( clgi.screen->hud_alpha );
	clgi.R_SetScale( clgi.screen->hud_scale );
	CLG_HUD_DrawElementNumberValue(
		numberStartX,
		numberStartY,
		HUD_ELEMENT_NUMBERS_DEST_WIDTH,
		HUD_ELEMENT_NUMBERS_DEST_HEIGHT,
		currentReserve
	);

	// Reset renderer color and alpha.
	clgi.R_ClearColor();
	clgi.R_SetAlpha( 1.0f );
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