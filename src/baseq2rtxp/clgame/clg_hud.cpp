/********************************************************************
*
*
*   Client 'Heads Up Display':
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_hud.h"
#include "clg_main.h"
#include "clg_screen.h"

//
// CVars
//
//! We need this one.
static cvar_t *scr_alpha;

//! The crosshair.
static cvar_t *hud_crosshair_type;
static cvar_t *hud_crosshair_red;
static cvar_t *hud_crosshair_green;
static cvar_t *hud_crosshair_blue;
static cvar_t *hud_crosshair_alpha;
static cvar_t *hud_crosshair_scale;


/**
*   HUD Specifics:
**/
static struct {
    //! Generic display color values.
    struct {
        // Colors.
        static constexpr uint32_t ORANGE2 = MakeColor( 255, 150, 100, 75 );
        //static constexpr uint32_t RED = MakeColor( 255, 50, 30, 255 );
        //static constexpr uint32_t ORANGE = MakeColor( 210, 125, 44, 150 );
        static constexpr uint32_t ORANGE = MakeColor( 210, 125, 44, 250 );
        static constexpr uint32_t RED = MakeColor( 208, 70, 72, 255 );
        static constexpr uint32_t LESS_WHITE = MakeColor( 220, 220, 220, 75 );
        static constexpr uint32_t WHITE = MakeColor( 255, 255, 255, 255 );
    } colors;

    //! Crosshair information.
    struct {
        int32_t     pic_width = 0, pic_height = 0;
        color_t     color = (color_t)U32_WHITE;
        float       alpha = 1.f;

        //! For recoil display scaling.
        struct {
            ////! Stats values of the current and last frame.
            //int32_t currentStatsValue = 0, lastStatsValue = 0;
            //! Decoded floating point values, scaled to between (0.0, 1.0).
            double  currentRecoil = 0, lastRecoil = 0;
            //! Realtime at which recoil value change took place.
            uint64_t changeTime = 0;
            //! Duration of current ease.
            uint64_t easeDuration = 0;
            //! 
        } recoil = {};
    } crosshair = {};

    //! Real hud screen size.
    int32_t     hud_real_width = 0, hud_real_height = 0;
    //! Scaled hud screen size.
    int32_t     hud_scaled_width = 0, hud_scaled_height = 0;
    //! Scale.
    float       hud_scale = 1.f;
    //! Global alpha.
    float       hud_alpha = 1.f;
} hud = {};


//! Used in various places.
extern const bool SCR_ShouldDrawPause();


/**
*
*
*   Crosshair(-color) CVar change and updating:
*
*
**/
/**
*   @brief  Will set the crosshair color to a custom color which is defined by cvars.
**/
static void CLG_HUD_SetCrosshairColorByCVars() {
    hud.crosshair.color.u8[ 0 ] = clgi.CVar_ClampValue( hud_crosshair_red, 0.f, 1.f ) * 255;
    hud.crosshair.color.u8[ 1 ] = clgi.CVar_ClampValue( hud_crosshair_green, 0.f, 1.f ) * 255;
    hud.crosshair.color.u8[ 2 ] = clgi.CVar_ClampValue( hud_crosshair_blue, 0.f, 1.f ) * 255;
    // We use a separate alpha value.
    hud.crosshair.color.u8[ 3 ] = 255;
}
/**
*   @brief  Used to update crosshair color with.
**/
void CLG_HUD_SetCrosshairColor() {
    // Set it based on cvars.
    CLG_HUD_SetCrosshairColorByCVars();
    // Apply alpha value.
    hud.crosshair.alpha = clgi.CVar_ClampValue( hud_crosshair_alpha, 0.0f, 1.0f );
    //hud.crosshair.color.u8[ 3 ] = clgi.CVar_ClampValue( ch_alpha, 0, 1 ) * 255;
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
*   HUD Core:
*
*
**/
/**
*   @brief  Called when screen module is initialized.
**/
void CLG_HUD_Initialize( void ) {
    // Reset HUD.
    hud = {};

    // Get already initialized screen cvars we need often.
    scr_alpha = clgi.CVar_Get( "scr_alpha", nullptr, 0 );

    // Crosshair cvars.
    hud_crosshair_red = clgi.CVar_Get( "hud_crosshair_red", "1", 0 );
    hud_crosshair_red->changed = scr_hud_crosshair_changed;
    hud_crosshair_green = clgi.CVar_Get( "hud_crosshair_green", "1", 0 );
    hud_crosshair_green->changed = scr_hud_crosshair_changed;
    hud_crosshair_blue = clgi.CVar_Get( "hud_crosshair_blue", "1", 0 );
    hud_crosshair_blue->changed = scr_hud_crosshair_changed;
    hud_crosshair_alpha = clgi.CVar_Get( "hud_crosshair_alpha", "1", 0 );
    hud_crosshair_alpha->changed = scr_hud_crosshair_changed;
    hud_crosshair_scale = clgi.CVar_Get( "hud_crosshair_scale", "1", 0 );
    hud_crosshair_scale->changed = scr_hud_crosshair_changed;

    hud_crosshair_type = clgi.CVar_Get( "hud_crosshair_type", "1", CVAR_ARCHIVE );
    hud_crosshair_type->changed = scr_hud_crosshair_changed;
}
/**
*   @brief  Called by PF_SCR_ModeChanged(video mode changed), or scr_scale_changed, in order to
*           notify about the new HUD scale.
**/
void CLG_HUD_ModeChanged( const float newHudScale ) {
    hud.hud_scale = newHudScale;
}
/**
*   @brief  Called by PF_SetScreenHUDAlpha, in order to notify about the new HUD alpha.
**/
void CLG_HUD_AlphaChanged( const float newHudAlpha ) {
    hud.hud_alpha = newHudAlpha;
}
/**
*	@brief	Called when the screen module registers media.
**/
void CLG_HUD_RegisterScreenMedia( void ) {
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
*   HUD String Drawing: 
* 
* 
**/
/**
*   @brief  
**/
void HUD_DrawString( const int32_t x, const int32_t y, const char *str ) {
    clgi.R_DrawString( x, y, 0, MAX_STRING_CHARS, str, precache.screen.font_pic );
}
/**
*   @brief
**/
void HUD_DrawAltString( const int32_t x, const int32_t y, const char *str ) {
    clgi.R_DrawString( x, y, UI_XORCOLOR, MAX_STRING_CHARS, str, precache.screen.font_pic );
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
*   HUD Draw Utilities:
*
*
**/
/**
*   @brief  Does as it says it does.
**/
static void CLG_HUD_DrawOutlinedRectangle( const uint32_t backGroundX, const uint32_t backGroundY, const uint32_t backGroundWidth, const uint32_t backGroundHeight, const uint32_t fillColor, const uint32_t outlineColor ) {
    // Set global color to white
    clgi.R_SetColor( MakeColor( 255, 255, 255, 255 ) );
    // Draw bg color.
    clgi.R_DrawFill32( backGroundX, backGroundY, backGroundWidth, backGroundHeight, fillColor );
    // Draw outlines:
    clgi.R_DrawFill32( backGroundX, backGroundY, 1, backGroundHeight, outlineColor ); // Left Line.
    clgi.R_DrawFill32( backGroundX, backGroundY, backGroundWidth, 1, outlineColor );  // Top Line.
    clgi.R_DrawFill32( backGroundX + backGroundWidth, backGroundY, 1, backGroundHeight + 1, outlineColor ); // Right Line.
    clgi.R_DrawFill32( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1, outlineColor ); // Bottom Line.
}
/**
*   @brief  Does as it says it does.
**/
static void CLG_HUD_DrawCrosshairLine( const uint32_t backGroundX, const uint32_t backGroundY, const uint32_t backGroundWidth, const uint32_t backGroundHeight, const uint32_t fillColor, const uint32_t outlineColor ) {
    // Set global color to white
    //clgi.R_SetColor( MakeColor( 255, 255, 255, 255 ) );
    #if 1
    // Draw bg color.
    clgi.R_DrawFill32( backGroundX, backGroundY, backGroundWidth, backGroundHeight, fillColor );
    if ( outlineColor ) {
        // Left line. (Also covers first pixel of top line and bottom line.)
        clgi.R_DrawFill32( backGroundX - 1, backGroundY - 1, 1, backGroundHeight + 2, outlineColor );
        // Right line. (Also covers last pixel of top line and bottom line.)
        clgi.R_DrawFill32( backGroundX + backGroundWidth, backGroundY - 1, 1, backGroundHeight + 2, outlineColor );
        // Top line. (Skips first and last pixel, already covered by both the left and right lines.)
        clgi.R_DrawFill32( backGroundX, backGroundY - 1, backGroundWidth, 1, outlineColor );
        // Bottom line. (Skips first and last pixel, already covered by both the left and right lines.)
        clgi.R_DrawFill32( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1, outlineColor );
    }
    // 
    //clgi.R_DrawFill32( backGroundX + 1, backGroundY + 1, backGroundWidth - 1, backGroundHeight - 1, fillColor );
    // Draw outlines:
    //clgi.R_DrawFill32( backGroundX, backGroundY, 1, backGroundHeight, outlineColor ); // Left Line.
    //clgi.R_DrawFill32( backGroundX, backGroundY, backGroundWidth, 1, outlineColor );  // Top Line.
    //clgi.R_DrawFill32( backGroundX + backGroundWidth, backGroundY, 1, backGroundHeight + 1, outlineColor ); // Right Line.
    //clgi.R_DrawFill32( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1, outlineColor ); // Bottom Line.
    #else
    // Draw bg color.
    clgi.R_DrawFill32( backGroundX + 1, backGroundY + 1, backGroundWidth - 1, backGroundHeight - 1, fillColor );
    // Draw outlines:
    clgi.R_DrawFill32( backGroundX, backGroundY, 1, backGroundHeight, outlineColor ); // Left Line.
    clgi.R_DrawFill32( backGroundX, backGroundY, backGroundWidth, 1, outlineColor );  // Top Line.
    clgi.R_DrawFill32( backGroundX + backGroundWidth, backGroundY, 1, backGroundHeight + 1, outlineColor ); // Right Line.
    clgi.R_DrawFill32( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1, outlineColor ); // Bottom Line.
    #endif
}



/**
*
*
*   HUD Crosshair Display:
*
*
**/
/**
*   @brief  Updates the hud's recoil status based on old/current frame difference and returns
*           the lerpFracion to use for scaling the recoil display.
*   @return Lerpfrac between last and current weapon recoil.
**/
const double CLG_HUD_UpdateRecoilScale( const uint64_t realTime, const uint64_t easeDuration ) {
    // Update realtime only if we're not paused.
    static uint64_t _realTime = realTime;
    if ( !CLG_IsGameplayPaused() ) {
        _realTime = realTime;
    }
    
    // Lerp fraction, clamp for discrepancies.
    const double lerpFraction = (double)( realTime - hud.crosshair.recoil.changeTime ) / easeDuration;

    // Update the exact time for lerping crosshair display, which is when the current recoil change took place.
    if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] != clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        // Decoded floating point recoil scale of current and last frame.
        hud.crosshair.recoil.currentRecoil = BYTE2BLEND( clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] );
        hud.crosshair.recoil.lastRecoil = BYTE2BLEND( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] );

        hud.crosshair.recoil.changeTime = realTime;
    }

    // Return lerpfracion.
    return lerpFraction;
}
/**
*   @brief  Renders a pic based crosshair.
**/
void CLG_HUD_DrawLineCrosshair( ) {
    // This is the 'BASE' scale of the crosshair, which is when it is at rest meaning
    // it has zero influence of recoil.
    const double crosshairUserScale = clgi.CVar_ClampValue( hud_crosshair_scale, 0.1f, 4.0f );

    // The base scale size of this crosshair.
    const double crosshairBaseScale = crosshairUserScale;

    /**
    *   (Default/Only, lol :-) -) Crosshair Configuration:
    **/
    // Pixel 'radius', the offset from the center of the screen, for lines start origins to begin at.
    static constexpr double CROSSHAIR_ABSOLUTE_CENTER_ORIGIN_OFFSET= 4.;
    // Default Pixel Height/Width of the Horizontal Lines.
    static constexpr double CROSSHAIR_HORIZONTAL_HEIGHT = 2.;
    static constexpr double CROSSHAIR_HORIZONTAL_WIDTH = 8.;
    // Default Pixel Height/Width of the Vertical Lines.
    static constexpr double CROSSHAIR_VERTICAL_WIDTH = 2.;
    static constexpr double CROSSHAIR_VERTICAL_HEIGHT= 8.;

    /**
    *   Get Ease Fraction.
    **/
    // Calculate the crosshair scale based on the recoil's value, lerp it over 25ms.
    static double recoilScale = 0.f;
    static uint64_t easeDuration = 25;
    static int32_t easeInOrOut = 1; // 0 == ease out, 1 == ease in.
    if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] > clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        easeDuration = 100;
        easeInOrOut = 1;
    } else if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] < clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        easeDuration = 25;
        easeInOrOut = 0;
    }

    // Default to 0.
    recoilScale = CLG_HUD_UpdateRecoilScale( clgi.GetRealTime(), easeDuration );
    double easeFraction = 0.;
    if ( easeInOrOut == 0 ) {
        easeFraction = QM_Clampd( 1.0 - QM_QuarticEaseOut( recoilScale ), 0., 1. );
    } else {
        easeFraction = QM_Clampd( 1.0 - QM_QuarticEaseIn( recoilScale ), 0., 1. );
    }

    /**
    *   Calculate the recoil derived offsets, as well as widths and heights.
    **/
    // Default idle crosshair values.
    double chCenterOffsetRadius = CROSSHAIR_ABSOLUTE_CENTER_ORIGIN_OFFSET;// *( crosshairBaseScale + easeFraction );
    double chHorizontalWidth = CROSSHAIR_HORIZONTAL_WIDTH; //+CROSSHAIR_HORIZONTAL_WIDTH * ( crosshairBaseScale + easeFraction );
    double chVerticalHeight = CROSSHAIR_VERTICAL_HEIGHT; //+CROSSHAIR_VERTICAL_HEIGHT * ( crosshairBaseScale + easeFraction );
    // Now to be adjusted by possible recoil.
    chCenterOffsetRadius = chCenterOffsetRadius + ( chCenterOffsetRadius * easeFraction );
    chHorizontalWidth = chHorizontalWidth + ( chHorizontalWidth * easeFraction );
    chVerticalHeight = chVerticalHeight + ( chVerticalHeight * easeFraction );
    // Now adjust by base scale to get the final result.
    chCenterOffsetRadius *= crosshairBaseScale;
    chHorizontalWidth *= crosshairBaseScale;
    chVerticalHeight *= crosshairBaseScale;

    // Developer Debug:
    if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] != clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        static uint64_t debugprintframe = 0;
        if ( debugprintframe != level.frameNumber ) {
            debugprintframe = level.frameNumber;
            clgi.Print( PRINT_DEVELOPER, "---------------------------------------------------\n" );
            clgi.Print( PRINT_DEVELOPER, "realTime=#%ull, level.time=#%ull, frameNumber=#%ull\n",
                clgi.GetRealTime(),
                level.time,
                level.frameNumber
            );
            clgi.Print( PRINT_DEVELOPER, "easeFraction(%f), lastRecoil(%f), currentRecoil(%f)\n",
                easeFraction, 
                hud.crosshair.recoil.lastRecoil, hud.crosshair.recoil.currentRecoil  
            );
            clgi.Print( PRINT_DEVELOPER, "chCenterOffsetRadius(%f), chHorizontalWidth(%f), chVerticalHeight(%f)\n",
                chCenterOffsetRadius,
                chHorizontalWidth, chVerticalHeight
            );
        }
    }

    // Determine center x/y for crosshair display.
    const int32_t center_x = ( hud.hud_scaled_width ) / 2;
    const int32_t center_y = ( hud.hud_scaled_height ) / 2;

    // Apply generic crosshair alpha.
    clgi.R_SetAlpha( hud.crosshair.alpha * scr_alpha->value );
    // Apply overlay base color.
    clgi.R_SetColor( hud.crosshair.color.u32 );

    // Draw the UP line.
    const int32_t up_x = center_x - ( CROSSHAIR_VERTICAL_WIDTH / 2 );
    const int32_t up_y = center_y - ( chCenterOffsetRadius + chVerticalHeight );
    CLG_HUD_DrawCrosshairLine( up_x, up_y, CROSSHAIR_VERTICAL_WIDTH, chVerticalHeight, hud.colors.LESS_WHITE, hud.colors.WHITE );

    // Draw the RIGHT line.
    const int32_t right_x = center_x + ( chCenterOffsetRadius );//center_x - ( crosshairHorizontalWidth / 2 );
    const int32_t right_y = center_y - ( CROSSHAIR_HORIZONTAL_HEIGHT / 2 ); //center_y - CROSSHAIR_ABS_CENTER_OFFSET + crosshairHorizontalHeight;
    CLG_HUD_DrawCrosshairLine( right_x, right_y, chHorizontalWidth, CROSSHAIR_HORIZONTAL_HEIGHT, hud.colors.LESS_WHITE, hud.colors.WHITE );

    // Draw the DOWN line.
    const int32_t down_x = center_x - ( CROSSHAIR_VERTICAL_WIDTH / 2 );
    const int32_t down_y = center_y + ( chCenterOffsetRadius );
    CLG_HUD_DrawCrosshairLine( down_x, down_y, CROSSHAIR_VERTICAL_WIDTH, chVerticalHeight, hud.colors.LESS_WHITE, hud.colors.WHITE );

    // Draw the LEFT line.
    const int32_t left_x = center_x - ( chCenterOffsetRadius + chHorizontalWidth );//center_x - ( crosshairHorizontalWidth / 2 );
    const int32_t left_y = center_y - ( CROSSHAIR_HORIZONTAL_HEIGHT / 2 ); //center_y - CROSSHAIR_ABS_CENTER_OFFSET + crosshairHorizontalHeight;
    CLG_HUD_DrawCrosshairLine( left_x, left_y, chHorizontalWidth, CROSSHAIR_HORIZONTAL_HEIGHT, hud.colors.LESS_WHITE, hud.colors.WHITE );

    // Reset color and alpha.
    clgi.R_SetColor( U32_WHITE );
    clgi.R_SetAlpha( scr_alpha->value );
}
#if 0
/**
*   @brief  Renders a pic based crosshair.
**/
void CLG_HUD_DrawPicCrosshair() {
    // This is the 'BASE' scale of the crosshair, which is when it is at rest meaning
    // it has zero influence of recoil.
    //const float crosshairUserScale = clgi.CVar_ClampValue( ch_scale, 0.1f, 9.0f );

    // Calculate actual base size of crosshair.
    constexpr double crosshairBaseScale = 0.5f;
    float crosshairWidth = hud.crosshair.pic_width * ( crosshairBaseScale );
    float crosshairHeight = hud.crosshair.pic_height * ( crosshairBaseScale );

    // Lerp fraction of recoil scale.
    const double lerpFrac = CLG_HUD_UpdateRecoilScale();

    // Additional up/down scaling for recoil effect display.
    const double additionalScaleW = QM_Clampd( ( hud.crosshair.recoil.currentValue + ( lerpFrac * ( hud.crosshair.recoil.currentValue - hud.crosshair.recoil.lastValue ) ) ), 0.f, 1.f );
    const double additionalScaleH = QM_Clampd( ( hud.crosshair.recoil.currentValue + ( lerpFrac * ( hud.crosshair.recoil.currentValue - hud.crosshair.recoil.lastValue ) ) ), 0.f, 1.f );

    // Developer Debug:
    //if ( hud.crosshair.recoil.currentStatsValue != hud.crosshair.recoil.lastStatsValue ) {
    //    clgi.Print( PRINT_DEVELOPER, "%s: additionalScaleW(%f), additionalScaleH(%f), lastRecoil(%i), currentRecoil(%i), lerpfrac(%f)\n"
    //        , __func__, additionalScaleW, additionalScaleH, hud.crosshair.recoil.lastStatsValue, hud.crosshair.recoil.currentStatsValue, lerpFrac );
    //}

    // Calculate final crosshair display width and height.
    //if ( additionalScaleW || additionalScaleH ) {
    crosshairWidth = crosshairWidth * ( ( 1.0 / crosshairWidth ) * additionalScaleW );//( ( ( 1.0f / 255 ) * additionalScale ) );
    crosshairHeight = crosshairHeight + ( ( 1.0 / crosshairHeight ) * additionalScaleH );//( ( ( 1.0f / 255 ) * additionalScale ) );
    //}

    // Determine x/y for crosshair display.
    const int32_t x = ( hud.hud_scaled_width - crosshairWidth ) / 2;
    const int32_t y = ( hud.hud_scaled_height - crosshairHeight ) / 2;

    // Apply the crosshair color.
    clgi.R_SetColor( hud.crosshair.color.u32 );

    // Draw the crosshair pic.
    clgi.R_DrawStretchPic( 
        x, y,
        crosshairWidth, crosshairHeight,
        precache.hud.crosshair_pic 
    );
}
#endif
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
    if ( clgi.client->predictedState.currentPs.stats[ STAT_WEAPON_FLAGS ] & STAT_WEAPON_FLAGS_IS_AIMING ) {
        return;
    }

    // > 0 Will draw specified crosshair type.
    if ( hud_crosshair_type->integer >= 1 ) {
        CLG_HUD_DrawLineCrosshair();
    }

    //! WID: Seemed a cool idea, but, I really do not like it lol.
    #if 0
        // Draw crosshair damage displays.
    SCR_DrawDamageDisplays();
    #endif
}



/**
*
*
*   HUD UseTarget Hint Display:
*
*
**/
/**
*	@brief  Renders the 'UseTarget' display info to the screen.
**/
static const char *useTargetHints[] = {
    "",
    "Press [+usetarget] to open the door.",
    "Press [+usetarget] to close the door.",
    "Press [+usetarget] to activate the button.",
    "Press [+usetarget] to deactivate the button.",
    "",
    nullptr
};
// Total useTargetHints.
static constexpr int32_t useTargetHintsCount = q_countof( useTargetHints );
/**
*   @brief  Will test index and flags for validity and displayabilty. If passing the validation test,
*           parses the hint string and displays it appropriately.
**/
void CLG_HUD_DrawUseTargetInfo( const int32_t useTargetHintIndex, const int32_t useTargetHintFlags ) {
    if ( SCR_ShouldDrawPause() ) {
        return;
    }

    // Ensure we got data for, and meant, to display.
    if ( ( useTargetHintFlags & STAT_USETARGET_HINT_FLAGS_DISPLAY ) == 0 ) {
        return;
    }
    // Ensure the index is a valid string.
    if ( useTargetHintIndex <= 0 || useTargetHintIndex > useTargetHintsCount ) {
        return;
    }

    // Determine center of screen.
    const int32_t x = ( hud.hud_scaled_width/*- scr.pause_width */ ) / 2;
    const int32_t y = ( hud.hud_scaled_height /*- scr.pause_height */ ) / 2;

    // Use our 'Orange' color.
    clgi.R_SetColor( MakeColor( 255, 150, 100, 255 ) );
    
    // Draw hint text info string.
    int _y = ( CHAR_HEIGHT * 4.5 );
    HUD_DrawCenterString( x, _y, useTargetHints[useTargetHintIndex]);
    // Reset R color.
    clgi.R_ClearColor();
}


/**
*	@brief  Renders the player's health and armor status to screen.
**/
static void CLG_HUD_DrawHealthIndicators() {
    // Health Background:
    static constexpr int32_t HEALTH_BACKGROUND_WIDTH = 75;
    static constexpr int32_t HEALTH_BACKGROUND_HEIGHT = 40;
    static constexpr int32_t HEALTH_BACKGROUND_OFFSET_LEFT = 10;
    static constexpr int32_t HEALTH_BACKGROUND_OFFSET_BOTTOM = 10;
    int32_t healthBackGroundX = ( HEALTH_BACKGROUND_OFFSET_LEFT );
    int32_t healthBackGroundY = ( hud.hud_scaled_height - ( HEALTH_BACKGROUND_HEIGHT + HEALTH_BACKGROUND_OFFSET_BOTTOM ) );
    // Health Name:
    const std::string strHealthName = "Health:";
    // Determine x/y for health name..
    const int32_t healthNameX = HEALTH_BACKGROUND_OFFSET_LEFT + 10;
    const int32_t healthNameY = ( hud.hud_scaled_height - 20 ) - ( CHAR_HEIGHT * 2 ) - 4;
    // Health:
    const std::string strHealth = std::to_string( clgi.client->frame.ps.stats[ STAT_HEALTH ] );
    // String length.
    const int32_t strHealthSize = strHealth.length() * CHAR_WIDTH;
    // Adjust offset.
    const int32_t healthX = ( 20 );
    const int32_t healthY = ( hud.hud_scaled_height - 20 ) - ( CHAR_HEIGHT ) ;

    // Armor Background:
    static constexpr int32_t ARMOR_BACKGROUND_WIDTH = 65;
    static constexpr int32_t ARMOR_BACKGROUND_HEIGHT = 40;
    static constexpr int32_t ARMOR_BACKGROUND_OFFSET_LEFT = ( 10 + HEALTH_BACKGROUND_OFFSET_LEFT + HEALTH_BACKGROUND_WIDTH );
    static constexpr int32_t ARMOR_BACKGROUND_OFFSET_BOTTOM = 10;
    int32_t armorBackGroundX = ( ARMOR_BACKGROUND_OFFSET_LEFT );
    int32_t armorBackGroundY = ( hud.hud_scaled_height - ( ARMOR_BACKGROUND_HEIGHT + ARMOR_BACKGROUND_OFFSET_BOTTOM ) );
    // Armor Name:
    const std::string strArmorName = "Armor:";
    // Determine x/y for armor name..
    const int32_t armorNameX = ARMOR_BACKGROUND_OFFSET_LEFT + 10;
    const int32_t armorNameY = ( hud.hud_scaled_height - 20 ) - ( CHAR_HEIGHT * 2 ) - 4;
    // Armor:
    const std::string strArmor = std::to_string( clgi.client->frame.ps.stats[ STAT_ARMOR ] );
    // String length.
    const int32_t strArmorSize = strArmor.length() * CHAR_WIDTH;
    // Adjust offset.
    const int32_t armorX = armorNameX;
    const int32_t armorY = ( hud.hud_scaled_height - 20 ) - ( CHAR_HEIGHT ) ;

    /**
    *   Draw Health Indicator:
    **/
    // Background with an Outlined Rectangle.
    CLG_HUD_DrawOutlinedRectangle( healthBackGroundX, healthBackGroundY, HEALTH_BACKGROUND_WIDTH, HEALTH_BACKGROUND_HEIGHT, hud.colors.ORANGE2, hud.colors.ORANGE );
    // Set text color to orange.
    clgi.R_SetColor( hud.colors.WHITE );
    // Draw health name.
    HUD_DrawString( healthNameX, healthNameY, strHealthName.c_str() );
    // Set text color to white.
    if ( clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 20 ) {
        clgi.R_SetColor( hud.colors.RED );
    } else {
        clgi.R_SetColor( hud.colors.WHITE );
    }
    // Draw health value.
    HUD_DrawString( healthX, healthY, strHealth.c_str() );

    /**
    *   Draw Armor Indicator:
    **/
    // Background with an Outlined Rectangle.
    CLG_HUD_DrawOutlinedRectangle( armorBackGroundX, armorBackGroundY, ARMOR_BACKGROUND_WIDTH, ARMOR_BACKGROUND_HEIGHT, hud.colors.ORANGE2, hud.colors.ORANGE );
    // Set text color to orange.
    clgi.R_SetColor( hud.colors.WHITE );
    // Draw armor display name.
    HUD_DrawString( armorNameX, armorNameY, strArmorName.c_str() );
    // Set text color to white.
    clgi.R_SetColor( hud.colors.WHITE );
    // Draw armor value.
    HUD_DrawString( armorX, armorY, strArmor.c_str() );
}

/**
*	@brief  Renders the player's weapon name and (clip-)ammo status to screen.
**/
static void CLG_HUD_DrawAmmoIndicators() {
    if ( !clgi.client->frame.ps.gun.modelIndex ) {
        return;
    }

    /**
    *   Calculate position and sizes for each indicator item we render.
    **/
    // Ammo Background:
    static constexpr int32_t AMMO_BACKGROUND_WIDTH = 85;
    static constexpr int32_t AMMO_BACKGROUND_HEIGHT = 40;
    static constexpr int32_t AMMO_BACKGROUND_OFFSET_RIGHT = 10;
    static constexpr int32_t AMMO_BACKGROUND_OFFSET_BOTTOM = 10;
    int32_t ammoBackGroundX = ( hud.hud_scaled_width - ( AMMO_BACKGROUND_WIDTH + AMMO_BACKGROUND_OFFSET_RIGHT ));
    int32_t ammoBackGroundY = ( hud.hud_scaled_height - ( AMMO_BACKGROUND_HEIGHT + AMMO_BACKGROUND_OFFSET_BOTTOM ) );


    // WeaponName:
    // Generate weapon display string, defaults to Fists.
    const int32_t weaponItemID = clgi.client->frame.ps.stats[ STAT_WEAPON_ITEM ];
    std::string strWeaponName = clgi.client->configstrings[ CS_ITEMS + weaponItemID ];
    strWeaponName += ":";
    // Determine x/y for weapon name..
    const int32_t weaponNameX = ( hud.hud_scaled_width - 20 ) - strWeaponName.length() * CHAR_WIDTH;
    const int32_t weaponNameY = ( hud.hud_scaled_height - 20 ) - ( CHAR_HEIGHT * 2 ) - 4;

    // Ammo:
    // Display String.
    const std::string strTotalAmmo = std::to_string( clgi.client->frame.ps.stats[ STAT_AMMO ] );
    // String length.
    const int32_t strTotalAmmoSize = strTotalAmmo.length() * CHAR_WIDTH;
    // Adjust offset.
    const int32_t totalAmmoX = ( hud.hud_scaled_width - 20 ) - strTotalAmmoSize;
    const int32_t totalAmmoY = ( hud.hud_scaled_height - 20 ) - ( CHAR_HEIGHT ) ;
    
    // Separator(/) for 'Clip Ammo / Ammo' Display:
    // Display String.
    const std::string strSeparator = " / ";
    // String length.
    const int32_t strSeparatorWidth = strSeparator.length() * CHAR_WIDTH;
    // Screen Position.
    const int32_t separatorX = totalAmmoX - strSeparatorWidth;
    const int32_t separatorY = totalAmmoY;

    // Clip Ammo:
    // Display String.
    const std::string strClipAmmo = std::to_string( clgi.client->frame.ps.stats[ STAT_WEAPON_CLIP_AMMO ] );
    // Display String length.
    const int32_t strClipAmmoSize = strClipAmmo.length() * CHAR_WIDTH;
    // Screen Position.
    const int32_t clipAmmoX = separatorX - strClipAmmoSize;
    const int32_t clipAmmoY = separatorY;

    /**
    *   Draw Ammo Indicator:
    **/
    // Background with an Outlined Rectangle.
    CLG_HUD_DrawOutlinedRectangle( ammoBackGroundX, ammoBackGroundY, AMMO_BACKGROUND_WIDTH, AMMO_BACKGROUND_HEIGHT, hud.colors.ORANGE2, hud.colors.ORANGE );
    // Set text color to orange.
    clgi.R_SetColor( hud.colors.WHITE );
    // Draw weapon name.
    HUD_DrawString( weaponNameX, weaponNameY, strWeaponName.c_str() );

    // Set text color to white/red.
    if ( clgi.client->frame.ps.stats[ STAT_AMMO ] <= 30 ) {
        clgi.R_SetColor( hud.colors.RED );
    } else {
        clgi.R_SetColor( hud.colors.WHITE );
    }
    // Draw ammo.
    HUD_DrawString( totalAmmoX, totalAmmoY, strTotalAmmo.c_str() );

    // Set text color to orange.
    clgi.R_SetColor( hud.colors.WHITE );
    // Draw clip ammo.
    HUD_DrawString( separatorX, separatorY, strSeparator.c_str() );

    // Set text color to white/red.
    if ( clgi.client->frame.ps.stats[ STAT_WEAPON_CLIP_AMMO ] <= 3 ) {
        clgi.R_SetColor( hud.colors.RED );
    } else {
        clgi.R_SetColor( hud.colors.WHITE );
    }
    // Draw Clip Ammo count.
    HUD_DrawString( clipAmmoX, clipAmmoY, strClipAmmo.c_str() );
}

/**
*	@brief	Called when screen module is drawing its 2D overlay(s).
**/
void CLG_HUD_ScaleFrame( refcfg_t *refcfg ) {
    // Recalculate hud height/width.
    hud.hud_real_height = refcfg->height;
    hud.hud_real_width = refcfg->width;

    // Set general alpha scale.
    clgi.R_SetAlphaScale( hud.hud_alpha );

    // Set HUD scale.
    clgi.R_SetScale( hud.hud_scale );

    // Determine screen width and height based on hud_scale.
    hud.hud_scaled_height = Q_rint( hud.hud_real_height * hud.hud_scale );
    hud.hud_scaled_width = Q_rint( hud.hud_real_width * hud.hud_scale );
}

/**
*	@brief	Called when screen module is drawing its 2D overlay(s).
**/
void CLG_HUD_DrawFrame( refcfg_t *refcfg ) {
    // Weapon Name AND (Clip-)Ammo Indicators.
    CLG_HUD_DrawAmmoIndicators( );
    // Health AND Armor Indicators.
    CLG_HUD_DrawHealthIndicators( );
}