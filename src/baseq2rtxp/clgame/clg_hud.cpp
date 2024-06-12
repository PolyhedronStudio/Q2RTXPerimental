/********************************************************************
*
*
*   Client 'Heads Up Display':
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_hud.h"
#include "clg_screen.h"

//
// CVars
//
//! We need this one.
static cvar_t *scr_alpha;

//! The crosshair.
static cvar_t *hud_crosshair;
static cvar_t *ch_health;
static cvar_t *ch_red;
static cvar_t *ch_green;
static cvar_t *ch_blue;
static cvar_t *ch_alpha;
static cvar_t *ch_scale;
static cvar_t *ch_x;
static cvar_t *ch_y;

/**
*   HUD Specifics:
**/
static struct {
    //! Generic display color values.
    struct {
        // Colors.
        static constexpr uint32_t ORANGE = MakeColor( 255, 150, 100, 255 );
        static constexpr uint32_t LESS_WHITE = MakeColor( 220, 220, 220, 75 );
        static constexpr uint32_t WHITE = MakeColor( 255, 255, 255, 255 );
    } colors;
    //! Crosshair information.
    struct {
        int32_t     width = 0, height = 0;
        color_t     color = (color_t)U32_WHITE;

        //! For recoil display scaling.
        struct {
            //! Stats values of the current and last frame.
            int32_t currentStatsValue = 0, lastStatsValue = 0;
            //! Decoded floating point values, scaled to between (0.0, 1.0).
            float   currentScaledValue = 0, lastScaledValue = 0;
            //! Realtime at which recoil value change took place.
            int64_t changeTime = 0;
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


/**
*
*
*   HUD Core:
*
*
**/
/**
*   @brief  Will use the amount of health to indicate the crosshair's color.
**/
void CLG_HUD_SetCrosshairColorByHealth() {
    int health;

    if ( !ch_health->integer ) {
        return;
    }

    health = clgi.client->frame.ps.stats[ STAT_HEALTH ];
    if ( health <= 0 ) {
        VectorSet( hud.crosshair.color.u8, 0, 0, 0 );
        return;
    }

    // red
    hud.crosshair.color.u8[ 0 ] = 255;

    // green
    if ( health >= 66 ) {
        hud.crosshair.color.u8[ 1 ] = 255;
    } else if ( health < 33 ) {
        hud.crosshair.color.u8[ 1 ] = 0;
    } else {
        hud.crosshair.color.u8[ 1 ] = ( 255 * ( health - 33 ) ) / 33;
    }

    // blue
    if ( health >= 99 ) {
        hud.crosshair.color.u8[ 2 ] = 255;
    } else if ( health < 66 ) {
        hud.crosshair.color.u8[ 2 ] = 0;
    } else {
        hud.crosshair.color.u8[ 2 ] = ( 255 * ( health - 66 ) ) / 33;
    }
}
/**
*   @brief  Will set the crosshair color to a custom color which is defined by cvars.
**/
static void CLG_HUD_SetCrosshairColorByCVars() {
    hud.crosshair.color.u8[ 0 ] = clgi.CVar_ClampValue( ch_red, 0, 1 ) * 255;
    hud.crosshair.color.u8[ 1 ] = clgi.CVar_ClampValue( ch_green, 0, 1 ) * 255;
    hud.crosshair.color.u8[ 2 ] = clgi.CVar_ClampValue( ch_blue, 0, 1 ) * 255;
}
/**
*   @brief  Used to update crosshair color with.
**/
void CLG_HUD_SetCrosshairColor() {
    //! Use health as color instead.
    if ( ch_health->integer ) {
        CLG_HUD_SetCrosshairColorByHealth();
    } else {
        CLG_HUD_SetCrosshairColorByCVars();
    }
    hud.crosshair.color.u8[ 3 ] = clgi.CVar_ClampValue( ch_alpha, 0, 1 ) * 255;
}
/**
*	@brief
**/
static void scr_crosshair_changed( cvar_t *self ) {
    int32_t crosshairWidth = 0;
    int32_t crosshairHeight = 0;

    if ( hud_crosshair->integer > 0 ) {
        //Q_snprintf( buffer, sizeof( buffer ), "ch%i", hud_crosshair->integer );
        //precache.hud.crosshair_pic = clgi.R_RegisterPic( buffer );
        precache.hud.crosshair_pic = clgi.R_RegisterPic( "crosshair01.png" );
        clgi.R_GetPicSize( &crosshairWidth, &crosshairHeight, precache.hud.crosshair_pic );

        // prescale
        const float crosshairScale = clgi.CVar_ClampValue( ch_scale, 0.1f, 9.0f );
        hud.crosshair.width = crosshairWidth;// *crosshairScale;
        hud.crosshair.height = crosshairHeight;// *crosshairScale;
        // Force it to have a width in case GetPicSize fails somehow.
        if ( hud.crosshair.width < 1 ) {
            hud.crosshair.width = 1;
        }
        if ( hud.crosshair.height < 1 ) {
            hud.crosshair.height = 1;
        }

        // Update crosshair colors.
        CLG_HUD_SetCrosshairColor();
    } else {
        precache.hud.crosshair_pic = 0;
    }
}

/**
*   @brief  Called when screen module is initialized.
**/
void CLG_HUD_Initialize( void ) {
    // Reset HUD.
    hud = {};

    // Get already initialized screen cvars we need often.
    scr_alpha = clgi.CVar_Get( "scr_alpha", nullptr, 0 );

    // Crosshair cvars.
    ch_health = clgi.CVar_Get( "ch_health", "0", 0 );
    ch_health->changed = scr_crosshair_changed;
    ch_red = clgi.CVar_Get( "ch_red", "1", 0 );
    ch_red->changed = scr_crosshair_changed;
    ch_green = clgi.CVar_Get( "ch_green", "1", 0 );
    ch_green->changed = scr_crosshair_changed;
    ch_blue = clgi.CVar_Get( "ch_blue", "1", 0 );
    ch_blue->changed = scr_crosshair_changed;
    ch_alpha = clgi.CVar_Get( "ch_alpha", "1", 0 );
    ch_alpha->changed = scr_crosshair_changed;
    ch_scale = clgi.CVar_Get( "ch_scale", "1", 0 );
    ch_scale->changed = scr_crosshair_changed;
    ch_x = clgi.CVar_Get( "ch_x", "0", 0 );
    ch_y = clgi.CVar_Get( "ch_y", "0", 0 );

    hud_crosshair = clgi.CVar_Get( "crosshair", "0", CVAR_ARCHIVE );
    hud_crosshair->changed = scr_crosshair_changed;
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
    scr_crosshair_changed( hud_crosshair );
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
    clgi.R_DrawFill32( backGroundX, backGroundY, 1, backGroundHeight, outlineColor ); // Top Line.
    clgi.R_DrawFill32( backGroundX, backGroundY, backGroundWidth, 1, outlineColor );  // Left Line.
    clgi.R_DrawFill32( backGroundX + backGroundWidth, backGroundY, 1, backGroundHeight + 1, outlineColor ); // Right Line.
    clgi.R_DrawFill32( backGroundX, backGroundY + backGroundHeight, backGroundWidth, 1, outlineColor ); // Bottom Line.
}



/**
*
*
*   HUD Crosshair Display:
*
*
**/
/**
*   @brief  Renders a pic based crosshair.
**/
void CLG_HUD_DrawPicCrosshair( ) {
    // This is the 'BASE' scale of the crosshair, which is when it is at rest meaning
    // it has zero influence of recoil.
    const float crosshairUserScale = clgi.CVar_ClampValue( ch_scale, 0.1f, 9.0f );

    // Calculate actual base size of crosshair.
    constexpr float crosshairBaseScale = 0.5f;
    float crosshairWidth = hud.crosshair.width * ( crosshairBaseScale );
    float crosshairHeight = hud.crosshair.height * ( crosshairBaseScale );

    // Calculate the crosshair scale based on the recoil's value, lerp it over 25ms.
    static constexpr int32_t SCALE_TIME = 25;
    //if ( clgi.GetRealTime() - recoilTime <= SCALE_TIME ) {
    const float lerpFrac = QM_Clampf( (float)( clgi.GetRealTime() - hud.crosshair.recoil.changeTime ) / ( 1.0f / (float)SCALE_TIME ), 0, 1 ); //QM_Clampf( ( (float)( clgi.GetRealTime() - recoilTime ) / ( (float)SCALE_TIME ) ), 0.f, 1.f );
    //}

    // Refresh the current frame and old frame recoil values.
    hud.crosshair.recoil.lastStatsValue = clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ];
    hud.crosshair.recoil.currentStatsValue = clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ];

    // Update the exact time for lerping crosshair display, which is when the current recoil change took place.
    if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] != clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        hud.crosshair.recoil.changeTime = clgi.GetRealTime();
    }

    // Decoded floating point recoil scale of current and last frame.
    hud.crosshair.recoil.currentScaledValue = BYTE2BLEND( hud.crosshair.recoil.currentStatsValue );
    hud.crosshair.recoil.lastScaledValue = BYTE2BLEND( hud.crosshair.recoil.lastStatsValue );

    // Additional up/down scaling for recoil effect display.
    const float additionalScaleW = QM_Clampf( ( hud.crosshair.recoil.currentScaledValue + ( lerpFrac * ( hud.crosshair.recoil.currentScaledValue - hud.crosshair.recoil.lastScaledValue ) ) ), 0.f, 1.f );
    const float additionalScaleH = QM_Clampf( ( hud.crosshair.recoil.currentScaledValue + ( lerpFrac * ( hud.crosshair.recoil.currentScaledValue - hud.crosshair.recoil.lastScaledValue ) ) ), 0.f, 1.f );

    // Developer Debug:
    if ( hud.crosshair.recoil.currentStatsValue != hud.crosshair.recoil.lastStatsValue ) {
        clgi.Print( PRINT_DEVELOPER, "%s: additionalScaleW(%f), additionalScaleH(%f), lastRecoil(%i), currentRecoil(%i), lerpfrac(%f)\n"
            , __func__, additionalScaleW, additionalScaleH, hud.crosshair.recoil.lastStatsValue, hud.crosshair.recoil.currentStatsValue, lerpFrac );
    }

    // Calculate final crosshair display width and height.
    //if ( additionalScaleW || additionalScaleH ) {
    crosshairWidth = crosshairWidth + ( crosshairWidth * additionalScaleW );//( ( ( 1.0f / 255 ) * additionalScale ) );
    crosshairHeight = crosshairHeight + ( crosshairHeight * additionalScaleH );//( ( ( 1.0f / 255 ) * additionalScale ) );
    //}

    // Determine x/y for crosshair display.
    const int32_t x = ( hud.hud_scaled_width - crosshairWidth ) / 2;
    const int32_t y = ( hud.hud_scaled_height - crosshairHeight ) / 2;

    // Apply the crosshair color.
    clgi.R_SetColor( hud.crosshair.color.u32 );

    // Draw the crosshair pic.
    clgi.R_DrawStretchPic( x + ch_x->integer,
        y + ch_y->integer,
        crosshairWidth,
        crosshairHeight,
        precache.hud.crosshair_pic );
}
/**
*   @brief  Renders a pic based crosshair.
**/
void CLG_HUD_DrawLineCrosshair( ) {
    // This is the 'BASE' scale of the crosshair, which is when it is at rest meaning
    // it has zero influence of recoil.
    const float crosshairUserScale = clgi.CVar_ClampValue( ch_scale, 0.1f, 9.0f );

    // Calculate actual base size of crosshair.
    constexpr float crosshairBaseScale = 0.5f;
    float crosshairWidth = 2;//hud.crosshair.width * ( crosshairBaseScale );
    float crosshairHeight = 16; //hud.crosshair.height * ( crosshairBaseScale );

    // Calculate the crosshair scale based on the recoil's value, lerp it over 25ms.
    static constexpr int32_t SCALE_TIME = 25;
    //if ( clgi.GetRealTime() - recoilTime <= SCALE_TIME ) {
    const float lerpFrac = QM_Clampf( (float)( clgi.GetRealTime() - hud.crosshair.recoil.changeTime ) / ( 1.0f / (float)SCALE_TIME ), 0, 1 ); //QM_Clampf( ( (float)( clgi.GetRealTime() - recoilTime ) / ( (float)SCALE_TIME ) ), 0.f, 1.f );
    //}

    // Refresh the current frame and old frame recoil values.
    hud.crosshair.recoil.lastStatsValue = clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ];
    hud.crosshair.recoil.currentStatsValue = clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ];

    // Update the exact time for lerping crosshair display, which is when the current recoil change took place.
    if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] != clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        hud.crosshair.recoil.changeTime = clgi.GetRealTime();
    }

    // Decoded floating point recoil scale of current and last frame.
    hud.crosshair.recoil.currentScaledValue = BYTE2BLEND( hud.crosshair.recoil.currentStatsValue );
    hud.crosshair.recoil.lastScaledValue = BYTE2BLEND( hud.crosshair.recoil.lastStatsValue );

    // Additional up/down scaling for recoil effect display.
    const float additionalScaleW = QM_Clampf( ( hud.crosshair.recoil.currentScaledValue + ( lerpFrac * ( hud.crosshair.recoil.currentScaledValue - hud.crosshair.recoil.lastScaledValue ) ) ), 0.f, 1.f );
    const float additionalScaleH = QM_Clampf( ( hud.crosshair.recoil.currentScaledValue + ( lerpFrac * ( hud.crosshair.recoil.currentScaledValue - hud.crosshair.recoil.lastScaledValue ) ) ), 0.f, 1.f );

    // Developer Debug:
    if ( hud.crosshair.recoil.currentStatsValue != hud.crosshair.recoil.lastStatsValue ) {
        clgi.Print( PRINT_DEVELOPER, "%s: additionalScaleW(%f), additionalScaleH(%f), lastRecoil(%i), currentRecoil(%i), lerpfrac(%f)\n"
            , __func__, additionalScaleW, additionalScaleH, hud.crosshair.recoil.lastStatsValue, hud.crosshair.recoil.currentStatsValue, lerpFrac );
    }

    // Calculate final crosshair display width and height.
    //if ( additionalScaleW || additionalScaleH ) {
    crosshairWidth = crosshairWidth + ( crosshairWidth * additionalScaleW );//( ( ( 1.0f / 255 ) * additionalScale ) );
    crosshairHeight = crosshairHeight + ( crosshairHeight * additionalScaleH );//( ( ( 1.0f / 255 ) * additionalScale ) );
    //}

    // Determine x/y for crosshair display.
    const int32_t x = ( hud.hud_scaled_width - crosshairWidth ) / 2;
    const int32_t y = ( hud.hud_scaled_height - crosshairHeight ) / 2;

    // Apply overlay base color.
    clgi.R_SetColor( U32_WHITE );

    // Draw the UP line.

    // Draw the RIGHT line.

    // Draw the DOWN line.

    // Draw the LEFT line.

}

/**
*	@brief  Renders the crosshair to screen.
**/
void CLG_HUD_DrawCrosshair( void ) {
    // Only display if enabled.
    if ( !hud_crosshair->integer || !precache.hud.crosshair_pic ) {
        return;
    }

    // Don't show when 'is aiming' weapon mode is true.
    if ( clgi.client->predictedState.currentPs.stats[ STAT_WEAPON_FLAGS ] & STAT_WEAPON_FLAGS_IS_AIMING ) {
        return;
    }



    // Determine the type of crosshair(pic based, or line based.)
    if ( 1 == 1 ) {
        CLG_HUD_DrawLineCrosshair( );
    } else {
        CLG_HUD_DrawPicCrosshair( );
    }
    //! WID: Seemed a cool idea, but, I really do not like it lol.
    #if 0
        // Draw crosshair damage displays.
    SCR_DrawDamageDisplays();
    #endif
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
    CLG_HUD_DrawOutlinedRectangle( healthBackGroundX, healthBackGroundY, HEALTH_BACKGROUND_WIDTH, HEALTH_BACKGROUND_HEIGHT, hud.colors.LESS_WHITE, hud.colors.WHITE );
    // Set text color to orange.
    clgi.R_SetColor( hud.colors.ORANGE );
    // Draw health name.
    HUD_DrawString( healthNameX, healthNameY, strHealthName.c_str() );
    // Set text color to white.
    clgi.R_SetColor( hud.colors.WHITE );
    // Draw health value.
    HUD_DrawString( healthX, healthY, strHealth.c_str() );

    /**
    *   Draw Armor Indicator:
    **/
    // Background with an Outlined Rectangle.
    CLG_HUD_DrawOutlinedRectangle( armorBackGroundX, armorBackGroundY, ARMOR_BACKGROUND_WIDTH, ARMOR_BACKGROUND_HEIGHT, hud.colors.LESS_WHITE, hud.colors.WHITE );
    // Set text color to orange.
    clgi.R_SetColor( hud.colors.ORANGE );
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
    if ( !clgi.client->frame.ps.gunindex ) {
        return;
    }

    // Colors.
    constexpr uint32_t colorOrange = MakeColor( 255, 150, 100, 255 );
    constexpr uint32_t colorLessWhite = MakeColor( 220, 220, 220, 75 );
    constexpr uint32_t colorWhite = MakeColor( 255, 255, 255, 255 );

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
    CLG_HUD_DrawOutlinedRectangle( ammoBackGroundX, ammoBackGroundY, AMMO_BACKGROUND_WIDTH, AMMO_BACKGROUND_HEIGHT, colorLessWhite, colorWhite );
    // Set text color to orange.
    clgi.R_SetColor( colorOrange );
    // Draw weapon name.
    HUD_DrawString( weaponNameX, weaponNameY, strWeaponName.c_str() );

    // Set text color to white.
    clgi.R_SetColor( colorWhite );
    // Draw ammo.
    HUD_DrawString( totalAmmoX, totalAmmoY, strTotalAmmo.c_str() );

    // Set text color to orange.
    clgi.R_SetColor( colorOrange );
    // Draw clip ammo.
    HUD_DrawString( separatorX, separatorY, strSeparator.c_str() );

    // Set text color to white.
    clgi.R_SetColor( colorWhite );
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