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
    //! Crosshair information.
    struct {
        int32_t     width, height;
        color_t     color;
    } crosshair;

    //! Real hud screen size.
    int32_t     hud_real_width, hud_real_height;
    //! Scaled hud screen size.
    int32_t     hud_scaled_width, hud_scaled_height;
    //! Scale.
    float       hud_scale = 1.f;
    //! Global alpha.
    float       hud_alpha = 1.f;
} hud;

/**
*
*
*   HUD Core:
*
*
**/
/**
*   @brief  
**/
void CLG_HUD_SetCrosshairColor() {
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
*	@brief
**/
static void scr_crosshair_changed( cvar_t *self ) {
    char buffer[ 16 ];
    int w, h;
    float scale;

    if ( hud_crosshair->integer > 0 ) {
        Q_snprintf( buffer, sizeof( buffer ), "ch%i", hud_crosshair->integer );
        precache.hud.crosshair_pic = clgi.R_RegisterPic( buffer );
        clgi.R_GetPicSize( &w, &h, precache.hud.crosshair_pic );

        // prescale
        scale = clgi.CVar_ClampValue( ch_scale, 0.1f, 9.0f );
        hud.crosshair.width = w * scale;
        hud.crosshair.height = h * scale;
        if ( hud.crosshair.width < 1 )
            hud.crosshair.width = 1;
        if ( hud.crosshair.height < 1 )
            hud.crosshair.height = 1;

        if ( ch_health->integer ) {
            CLG_HUD_SetCrosshairColor();
        } else {
            hud.crosshair.color.u8[ 0 ] = clgi.CVar_ClampValue( ch_red, 0, 1 ) * 255;
            hud.crosshair.color.u8[ 1 ] = clgi.CVar_ClampValue( ch_green, 0, 1 ) * 255;
            hud.crosshair.color.u8[ 2 ] = clgi.CVar_ClampValue( ch_blue, 0, 1 ) * 255;
        }
        hud.crosshair.color.u8[ 3 ] = clgi.CVar_ClampValue( ch_alpha, 0, 1 ) * 255;
    } else {
        precache.hud.crosshair_pic = 0;
    }
}

/**
*   @brief  Called when screen module is initialized.
**/
void CLG_HUD_Initialize( void ) {
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
*   HUD Drawing:
*
*
**/
/**
*	@brief  Renders the crosshair to screen.
**/
static void CLG_HUD_DrawCrosshair( void ) {
    // Only display if enabled.
    if ( !hud_crosshair->integer ) {
        return;
    }

    // Don't show when 'is aiming' weapon mode is true.
    if ( clgi.client->predictedState.currentPs.stats[ STAT_WEAPON_FLAGS ] & STAT_WEAPON_FLAGS_IS_AIMING ) {
        return;
    }

    // Determine x/y for crosshair.
    const int32_t x = ( hud.hud_scaled_width - hud.crosshair.width ) / 2;
    const int32_t y = ( hud.hud_scaled_height - hud.crosshair.height ) / 2;

    // Apply the crosshair color.
    clgi.R_SetColor( hud.crosshair.color.u32 );

    // Draw the crosshair pic.
    clgi.R_DrawStretchPic( x + ch_x->integer,
        y + ch_y->integer,
        hud.crosshair.width,
        hud.crosshair.height,
        precache.hud.crosshair_pic );

    //! WID: Seemed a cool idea, but, I really do not like it lol.
    #if 0
        // Draw crosshair damage displays.
    SCR_DrawDamageDisplays();
    #endif
}

/**
*	@brief	Called when screen module is drawing its 2D overlay.
**/
void CLG_HUD_Draw( refcfg_t *refcfg ) {
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

    // Crosshair has its own color and alpha.
    CLG_HUD_DrawCrosshair();

    // The rest of 2D elements share common alpha.
    clgi.R_ClearColor();
    clgi.R_SetAlpha( clgi.CVar_ClampValue( scr_alpha, 0, 1 ) );
}