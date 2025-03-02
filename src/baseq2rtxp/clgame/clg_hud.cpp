/********************************************************************
*
*
*   Client 'Heads Up Display':
*
*
********************************************************************/
#include "clg_local.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_usetarget_hints.h"

#include "clg_hud.h"
#include "clg_main.h"
#include "clg_screen.h"

//
// CVars
//
//! We need this one.
static cvar_t *scr_alpha;

//! The chat hud.
static cvar_t *hud_chat;
static cvar_t *hud_chat_lines;
static cvar_t *hud_chat_time;
static cvar_t *hud_chat_x;
static cvar_t *hud_chat_y;

//! The crosshair.
static cvar_t *hud_crosshair_type;
static cvar_t *hud_crosshair_red;
static cvar_t *hud_crosshair_green;
static cvar_t *hud_crosshair_blue;
static cvar_t *hud_crosshair_alpha;
static cvar_t *hud_crosshair_scale;

//
// Chat Configuration.
//
static constexpr int32_t MAX_CHAT_TEXT = 150;
static constexpr int32_t MAX_CHAT_LINES = 32;
static constexpr int32_t CHAT_LINE_MASK = MAX_CHAT_LINES - 1;

//! Chatline type.
typedef struct {
    char    text[ MAX_CHAT_TEXT ];
    QMTime  time;
} chatline_t;



/**
*   @brief  Will test index and flags for validity and displayabilty. If passing the validation test,
*           parses the hint string and displays it appropriately.
**/
static constexpr int32_t MAX_TARGET_HINTS = 2;
static constexpr int32_t TOP_TARGET_HINT = MAX_TARGET_HINTS - 1;
static constexpr int32_t BOTTOM_TARGET_HINT = 0;
static constexpr QMTime TARGET_INFO_EASE_DURATION = 100_ms;

/**
*   @brief  Type of HUD Hint Token.
**/
enum hud_usetarget_hint_token_type_t {
    //! Regular text.
    HUD_TOKEN_TYPE_NORMAL = 0,
    //! [+keyname] will be replaced by [KEYNAME] and colored in orange.
    HUD_TOKEN_TYPE_TOKEN_KEYNAME,
    //! Activate, Open, (+Activate)/(-Activate) etc will be replaced by Activate, Open, and colored in green/red.
    HUD_TOKEN_TYPE_ACTION_ACTIVATE,
    HUD_TOKEN_TYPE_ACTION_DEACTIVATE,
    //! (Note) will be replaced by Note, in a distinct color.
    HUD_TOKEN_TYPE_NOTE,
};
/**
*   @brief  A token, has a string value and a type used for colorization and/or 'quotation' of a char.
**/
typedef struct hud_usetarget_hint_token_s {
    //! The string data.
    std::string value;
    //! The color to use.
    hud_usetarget_hint_token_type_t type;
} hud_usetarget_hint_token_t;
/**
*   @brief  Stores state of a usetarget hint that is for display.
*           There is a TOP and BOTTOM instance, who get shuffled around when the hint changes.
*           This causes it to to mimick a fade-in to idle to fade-out effect.
**/
typedef struct hud_usetarget_hint_s {
    //! UseTargetHintID
    int32_t hintID;
    //! Actual unformatted hint string.
    std::string hintString;
    // Formatted string tokens.
    std::vector<hud_usetarget_hint_token_t> hintStringTokens;
    //! Ease States.
    QMEaseState easeState;
    //! Alpha value calculated by easing.
    float alpha;
} hud_usetarget_hint_t;



/**
*   HUD Specifics:
**/
static struct hud_state_t {
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

        static constexpr uint32_t BLACK = MakeColor( 51, 51, 51, 255 );
    } colors;

    //! Crosshair information.
    struct {
        color_t     color = (color_t)U32_WHITE;
        float       alpha = 1.f;

        //! For recoil display scaling.
        struct {
            ////! Stats values of the current and last frame.
            int32_t currentRecoilStats = 0, lastRecoilStats = 0;
            //! Decoded floating point values, scaled to between (0.0, 1.0).
            double  currentRecoil = 0, lastRecoil = 0;
            //! Realtime at which recoil value change took place.
            QMTime changeTime = 0_ms;
            //! Duration of current ease.
            QMTime easeDuration = 0_ms;
            //! Easing in or out.
            bool isEasingOut = false;
            //! Actual easing method to use for the crosshair.
            const double( *easeMethod )( const double &lerpFraction ) = QM_QuarticEaseOut<double>;
        } recoil = {};
    } crosshair = {};

    /**
    *   @brief  UseTargetHint state:
    **/
    struct {
        //! Current ID.
        int32_t currentID = 0;
        //! Last ID.
        int32_t lastID = 0;

        //! Current 'head' index.
        int32_t headIndex = 0;
        //! Hint States.
        hud_usetarget_hint_t hints[ MAX_TARGET_HINTS ] = {};
    } targetHints = {};

    /**
    *   @brief  Chat State:
    **/
    struct {
        //! Chat lines.
        chatline_t      chatlines[ MAX_CHAT_LINES ];
        //! Head index of chat lines.
        uint32_t        chathead;
    } chatState = {};

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
    hud.crosshair.color.u8[ 0 ] = clgi.CVar_ClampValue( hud_crosshair_red, 0.f, 1.f ) * 255;
    hud.crosshair.color.u8[ 1 ] = clgi.CVar_ClampValue( hud_crosshair_green, 0.f, 1.f ) * 255;
    hud.crosshair.color.u8[ 2 ] = clgi.CVar_ClampValue( hud_crosshair_blue, 0.f, 1.f ) * 255;

    // We use a separate alpha value.
    hud.crosshair.color.u8[ 3 ] = clgi.CVar_ClampValue( hud_crosshair_alpha, 0.f, 1.f ) * 255;
}
/**
*   @brief  Used to update crosshair color with.
**/
void CLG_HUD_SetCrosshairColor() {
    // Set it based on cvars.
    CLG_HUD_SetCrosshairColorByCVars();
    // Apply alpha value.
    //hud.crosshair.alpha = clgi.CVar_ClampValue( hud_crosshair_alpha, 0.0f, 1.0f );
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
    hud = {};

    // Get already initialized screen cvars we need often.
    scr_alpha = clgi.CVar_Get( "scr_alpha", nullptr, 0 );

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
*   @brief  Called upon when clearing client state.
**/
void CLG_HUD_ClearTargetHints() {
    hud.targetHints = {};
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
    // Got an index of hint display, but its flagged as invisible.
    CLG_HUD_DrawCrosshair();
    // Display the use target hint information.
    CLG_HUD_DrawUseTargetHintInfos();

    // The rest of 2D elements share common alpha.
    clgi.R_ClearColor();
    clgi.R_SetAlpha( clgi.CVar_ClampValue( scr_alpha, 0, 1 ) );

    // Weapon Name AND (Clip-)Ammo Indicators.
    CLG_HUD_DrawAmmoIndicators();
    // Health AND Armor Indicators.
    CLG_HUD_DrawHealthIndicators();
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
    // Draw bg color.
    clgi.R_DrawFill32f( backGroundX, backGroundY, backGroundWidth, backGroundHeight, fillColor );
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
    hud.chatState = {};
    //memset( hud_chatlines, 0, sizeof( hud_chatlines ) );
    //hud_chathead = 0;
}

/**
*   @brief  Append text to chat HUD.
**/
void CLG_HUD_AddChatLine( const char *text ) {
    chatline_t *line;
    char *p;

    line = &hud.chatState.chatlines[ hud.chatState.chathead++ & CHAT_LINE_MASK ];
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
        x += hud.hud_real_width + 1;
        flags |= UI_RIGHT;
    } else {
        flags |= UI_LEFT;
    }
    // step.
    int32_t step = CHAR_HEIGHT;
    if ( y < 0 ) {
        y += hud.hud_real_height - CHAR_HEIGHT + 1;
        step = -CHAR_HEIGHT;
    }

    int32_t lines = hud_chat_lines->integer;
    if ( lines > hud.chatState.chathead ) {
        lines = hud.chatState.chathead;
    }

    for ( int32_t i = 0; i < lines; i++ ) {
        chatline_t *line = &hud.chatState.chatlines[ ( hud.chatState.chathead - i - 1 ) & CHAT_LINE_MASK ];

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
    const QMTime recoilDeltaTime = realTime - hud.crosshair.recoil.changeTime;

    // Method 01:
    #if 1
	// If we are at the start or end, return the respective value.
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
	const double lerpFraction = QM_Lerp( start, end, easeFactor );
    // Clamped lerp value.
	const double clampedLerpFraction = QM_Clampd( lerpFraction, 0, 1 );
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
        hud.crosshair.recoil.currentRecoil = half_to_float( clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] );
        hud.crosshair.recoil.lastRecoil = half_to_float( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] );
        // Record time changed.
        hud.crosshair.recoil.changeTime = realTime;
    }

    // If we're out of 'recoil', ease back in slowly.
    if ( clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] <= 0 ) {
		// Ease inwards in to the default crosshair values.
        hud.crosshair.recoil.easeDuration = 100_ms;
        // Easing Inwards.
        hud.crosshair.recoil.isEasingOut = false;
        hud.crosshair.recoil.easeMethod = QM_QuarticEaseIn<double>;
    // If we're firing(ammo changed) do an ease out to the new recoil values.
    } else if ( clgi.client->oldframe.ps.stats[ STAT_WEAPON_RECOIL ] > clgi.client->frame.ps.stats[ STAT_WEAPON_RECOIL ] ) {
        // Ease outwards into the new crosshair values.
        hud.crosshair.recoil.easeDuration = 50_ms;
        // Easing outwards.
        hud.crosshair.recoil.isEasingOut = true;
        hud.crosshair.recoil.easeMethod = QM_QuarticEaseOut<double>;
    }
    
	// Lerp the recoil scale based on the current and last frame's recoil values,
	// and the time it took to change from the last to the current frame, using the ease method.
    const double recoilScale = CLG_HUD_UpdateRecoilLerpScale(
        hud.crosshair.recoil.lastRecoil, hud.crosshair.recoil.currentRecoil,
        realTime, hud.crosshair.recoil.easeDuration, 
        hud.crosshair.recoil.easeMethod
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
                hud.crosshair.recoil.lastRecoil, hud.crosshair.recoil.currentRecoil  
            );
        }
    }
    #endif

    // Determine center x/y for crosshair display.
    const double center_x = ( hud.hud_scaled_width ) / 2.;
    const double center_y = ( hud.hud_scaled_height ) / 2.;

    // Apply overlay base color.
    clgi.R_SetColor( hud.colors.WHITE );
    // Apply generic crosshair alpha.
    clgi.R_SetAlpha( scr_alpha->value * hud.crosshair.alpha );

    // Thicker crosshair? Type 2:
    const bool thickCrossHair = ( hud_crosshair_type->integer > 1 );

    // Draw the UP line.
    const double up_x = center_x - ( CROSSHAIR_VERTICAL_WIDTH / 2. );
    const double up_y = center_y - ( chCenterOffsetRadius + chVerticalHeight );
    CLG_HUD_DrawCrosshairLine( up_x, up_y, CROSSHAIR_VERTICAL_WIDTH, chVerticalHeight, hud.crosshair.color.u32, hud.colors.BLACK, thickCrossHair );

    // Draw the RIGHT line.
    const double right_x = center_x + ( chCenterOffsetRadius );//center_x - ( crosshairHorizontalWidth / 2. );
    const double right_y = center_y - ( CROSSHAIR_HORIZONTAL_HEIGHT / 2. ); //center_y - CROSSHAIR_ABS_CENTER_OFFSET + crosshairHorizontalHeight;
    CLG_HUD_DrawCrosshairLine( right_x, right_y, chHorizontalWidth, CROSSHAIR_HORIZONTAL_HEIGHT, hud.crosshair.color.u32, hud.colors.BLACK, thickCrossHair );

    // Draw the DOWN line.
    const double down_x = center_x - ( CROSSHAIR_VERTICAL_WIDTH / 2. );
    const double down_y = center_y + ( chCenterOffsetRadius );
    CLG_HUD_DrawCrosshairLine( down_x, down_y, CROSSHAIR_VERTICAL_WIDTH, chVerticalHeight, hud.crosshair.color.u32, hud.colors.BLACK, thickCrossHair );

    // Draw the LEFT line.
    const double left_x = center_x - ( chCenterOffsetRadius + chHorizontalWidth );//center_x - ( crosshairHorizontalWidth / 2. );
    const double left_y = center_y - ( CROSSHAIR_HORIZONTAL_HEIGHT / 2. ); //center_y - CROSSHAIR_ABS_CENTER_OFFSET + crosshairHorizontalHeight;
    CLG_HUD_DrawCrosshairLine( left_x, left_y, chHorizontalWidth, CROSSHAIR_HORIZONTAL_HEIGHT, hud.crosshair.color.u32, hud.colors.BLACK, thickCrossHair );

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
*
*
*   Health and Ammo Indicators:
*
*
*
**/
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
*
*
* 
* 
*   HUD UseTarget Hint Display:
*
* 
* 
*
**/
/**
*   @brief  Cheesy, cheap, utility. I hate strings, don't you? :-)
**/
static void Q_std_string_replace_all( std::string &str, const std::string &from, const std::string &to ) {
    if ( from.empty() )
        return;
    size_t start_pos = 0;
    while ( ( start_pos = str.find( from, start_pos ) ) != std::string::npos ) {
        str.replace( start_pos, from.length(), to );
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}
/**
*   @brief  Cheesy, cheap, utility. I hate strings, don't you? :-)
**/
std::vector<std::string_view> Q_std_string_split( std::string_view buffer, const std::string_view delimiter = " " ) {
    std::vector<std::string_view> result;
    std::string_view::size_type pos;

    while ( ( pos = buffer.find( delimiter ) ) != std::string_view::npos ) {
        auto match = buffer.substr( 0, pos );
        if ( !match.empty() ) {
            result.push_back( match );
        }
        buffer.remove_prefix( pos + delimiter.size() );
    }

    if ( !buffer.empty() ) {
        result.push_back( buffer );
    }

    return result;
}

/**
*   @brief  Will look for [command] in the string, and replace it by the matching [KEY] value.
*   @note   Operates on the data contained in the reference of hudToken.
**/
static const bool HUD_FormatToken_KeyForCommand( std::string_view &stringToken, hud_usetarget_hint_token_t &hudToken ) {
    // Does it begin with a '[' ?
    const bool startWithAngleBracket = stringToken.starts_with( "[" );
    // Does it end with a '[' ?
    const bool endWithAngleBracket = stringToken.ends_with( "]" );

    // If both are valid, acquire us the keyname for the action.
    if ( startWithAngleBracket && endWithAngleBracket ) {
        // Get action name.
        size_t firstAngleBracketPos = hudToken.value.find_first_of( '[' ) + 1;
        size_t nextAngleBracketPos = hudToken.value.find_first_of( ']' ) -1 ;
        hudToken.value = hudToken.value.substr( firstAngleBracketPos, nextAngleBracketPos );
        
        const std::string_view actionName = hudToken.value;

        // Invalid/empty action name.
        if ( hudToken.value.empty() ) {
            return false;
        }

        // Find the keyname for the given action.
        std::string keyName = clgi.Key_GetBinding( hudToken.value.c_str() );
        // Failure if keyname == ""
        if ( keyName == "" ) {
            return false;
        }
        
        // Transform keyname to uppercase.
        std::transform( keyName.begin(), keyName.end(), keyName.begin(), ::toupper );

        // Replace our stringToken by [keyName]
        hudToken.value = std::string("[" + keyName + "]");
        hudToken.type = HUD_TOKEN_TYPE_TOKEN_KEYNAME;

        // True.
        return true;
    }

    return false;
}
/**
*   @brief  Will look for (+Action) or (-Action) in the string, in which case it is token tagged as ACTIVE/DEACTIVATE
*   @note   Operates on the data contained in the reference of hudToken.
**/
static const bool HUD_FormatToken_ForAction( std::string_view &stringToken, hud_usetarget_hint_token_t &hudToken ) {
    // Does it begin with a '(' ?
    const bool startWithCurlyBrace = hudToken.value.starts_with( "(" );
    // Does it end with a ')' ?
    const bool endWithCurlyBrace = hudToken.value.ends_with( ")" );

    // If both are valid, acquire us the keyname for the action.
    if ( startWithCurlyBrace && endWithCurlyBrace ) {
        // Get action name.
        size_t firstCurlyBracePos = hudToken.value.find_first_of( '(' ) + 1;
        size_t nextCurlyBracePos = hudToken.value.find_first_of( ')' ) - 1;
        // Determine whether it is a + or -, thus Activate or Deactivate.
        size_t firstOfPlus = hudToken.value.find_first_of( '+' );
        size_t firstOfMinus = hudToken.value.find_first_of( '-' );
        hudToken.value = hudToken.value.substr( firstCurlyBracePos, nextCurlyBracePos );

        // Invalid/empty action name.
        if ( hudToken.value.empty() ) {
            return false;

        }
        //clgi.Print( PRINT_DEVELOPER, "%s\n", hudToken.value.c_str() );

        if ( firstOfMinus == std::string::npos && firstOfPlus == std::string::npos ) {
            hudToken.value = hudToken.value.substr( 0, hudToken.value.size() ); // hudToken.value.substr( 1, hudToken.value.size() - 2 );
            // TODO: WID: Yeah??
            hudToken.type = HUD_TOKEN_TYPE_NOTE; // Or _REGULAR??
        // If we found a plus, redo the string value.
        } else if ( firstOfPlus != std::string::npos ) {
            hudToken.value = hudToken.value.substr( firstOfPlus, hudToken.value.size() ),
            hudToken.type = HUD_TOKEN_TYPE_ACTION_ACTIVATE;
        // If we found a minus, redo the string value.
        } else if ( firstOfMinus != std::string::npos ) {
            hudToken.value = hudToken.value.substr( firstOfMinus, hudToken.value.size() ),
            hudToken.type = HUD_TOKEN_TYPE_ACTION_DEACTIVATE;
        // Otherwise it is of type 'NOTE', use for referring to 'things'.
        } else {

        }

        // True.
        return true;
    }

    return false;
}

/**
*   @brief  Parse the usetarget info string properly, replace [+usetarget] etc.
**/
static const std::vector<hud_usetarget_hint_token_t> HUD_FormatUseTargetHintString( const std::string &useTargetHintString ) {
    // Actual final resulting tokens.
    std::vector<hud_usetarget_hint_token_t> hintTokens;

    // First split up the string by spaces right into a vector of strings. I know, inefficient.
    std::vector<std::string_view> stringTokens = Q_std_string_split( useTargetHintString, " " );
    // Iterate all tokens and determine their type, then push them back into the hintTokens vector.
    for ( std::string_view &stringToken : stringTokens ) {
        // The type of token, default to normal.
        hud_usetarget_hint_token_t hudToken = {
            .value = std::string( stringToken ),
            .type = HUD_TOKEN_TYPE_NORMAL,
        };
        // Format for a keys action. [+usetarget] etc.
        if ( HUD_FormatToken_KeyForCommand( stringToken, hudToken ) ) {

        // Format for an Action('Activate/Deactivate') defined by (+ActionName)/(-ActionName).
        } else if ( HUD_FormatToken_ForAction( stringToken, hudToken ) ) {

        }
        // Push back the token into the vector.
        hintTokens.push_back( hudToken );
    }

    // Return tokens.
    return hintTokens;
}

/**
*   @brief  Takes care of the actual drawing of specified targetHintInfo.
**/
void HUD_DrawTargetHintInfo( hud_usetarget_hint_t *targetHintInfo ) {
    // Clear the original color.
    clgi.R_ClearColor();
    clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );

    // Determine center of screen.
    const int32_t x = ( hud.hud_scaled_width/*- scr.pause_width */ ) / 2;
    const int32_t y = ( hud.hud_scaled_height /*- scr.pause_height */ ) / 2;

    // Current draw offset X axis.
    int32_t xOffset = 0;
    const int32_t yOffset = CHAR_HEIGHT * 4.5;
    // Iterate to acquire total width.. sight lol.
    const int32_t xWidth = HUD_GetTokenVectorDrawWidth( targetHintInfo->hintStringTokens );
    // X Start location.
    int32_t xDrawLocation = x - ( xWidth / 2 );
    // Iterate.
    for ( auto &hintStringToken : targetHintInfo->hintStringTokens ) {
        // Draw piece, special color if it is the keyname.
        if ( hintStringToken.type == HUD_TOKEN_TYPE_TOKEN_KEYNAME ) {
            // Clear the original color.
            clgi.R_ClearColor();
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawAltString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else if ( hintStringToken.type == HUD_TOKEN_TYPE_ACTION_ACTIVATE ) {
            // Set color.
            clgi.R_SetColor( U32_GREEN );
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else if ( hintStringToken.type == HUD_TOKEN_TYPE_ACTION_DEACTIVATE ) {
            // Set color.
            clgi.R_SetColor( U32_RED );
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else if ( hintStringToken.type == HUD_TOKEN_TYPE_NOTE ) {
            // Set color.
            //clgi.R_SetColor( hud.colors.ORANGE2 );
            clgi.R_SetColor( U32_YELLOW );
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else {
            // Clear the original color.
            clgi.R_ClearColor();
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        }
        // Increment the piece its width to the offset, including a space.(one more character)
        xOffset += ( HUD_GetStringDrawWidth( hintStringToken.value.c_str() ) + 1 ) + CHAR_WIDTH;
    }
}

/**
*   @brief  Handles detecting changes in the most recent UseTargetHint entity we're targeting,
*           as well as shuffling the hints whilst easing them in/out.
**/
void CLG_HUD_DrawUseTargetHintInfos( ) {
    //! Don't display if paused.
    if ( SCR_ShouldDrawPause() ) {
        return;
    }

    // Get QMTime for realtime.
    const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

    // Get the useTargetHintIDs from the stats.
    const int32_t currentID = clgi.client->frame.ps.stats[ STAT_USETARGET_HINT_ID ];
    const int32_t lastID = clgi.client->oldframe.ps.stats[ STAT_USETARGET_HINT_ID ];

    //
    // New ID, shuffle current to last, reassign current to fresh new current value.
    //
    if ( currentID != lastID ) {
        // Is it the same currentID as the one we got set?
        if ( currentID == hud.targetHints.currentID ) {
            // Do nothing here.
        // It is a new currentID which we are not displaying.
        } else {
            // Get the useTargetHint information that matches the ID.
            const sg_usetarget_hint_t *currentUseTargetHint = SG_UseTargetHint_GetByID( currentID );

            // Move the current 'top' slot its hud target hint data into the 'bottom' slot.
            hud.targetHints.hints[ BOTTOM_TARGET_HINT ] = hud.targetHints.hints[ TOP_TARGET_HINT ];
            // Initiate a new ease.
            hud.targetHints.hints[ BOTTOM_TARGET_HINT ].easeState = QMEaseState::new_ease_state( realTime, TARGET_INFO_EASE_DURATION );
            // Store it as our new lastID.
            hud.targetHints.lastID = hud.targetHints.hints[ BOTTOM_TARGET_HINT ].hintID;

            // It is a valid usetarget.
            if ( currentUseTargetHint && currentUseTargetHint->hintString && ( currentUseTargetHint->index != hud.targetHints.currentID ) ) {
                // Apply new top ID.
                hud.targetHints.hints[ TOP_TARGET_HINT ] = {
                    .hintID = hud.targetHints.currentID = currentID,
                    .hintString = currentUseTargetHint->hintString,
                    .hintStringTokens = HUD_FormatUseTargetHintString( currentUseTargetHint->hintString ),
                    .easeState = QMEaseState::new_ease_state( realTime + TARGET_INFO_EASE_DURATION, TARGET_INFO_EASE_DURATION ),
                    .alpha = 0
                };
            } else {
                // Store lastID.
                hud.targetHints.lastID = hud.targetHints.currentID;
                // Apply new top ID.
                hud.targetHints.hints[ TOP_TARGET_HINT ] = {
                    .hintID = hud.targetHints.currentID = currentID,
                    .hintString = "",
                    .hintStringTokens = {},
                    .easeState = QMEaseState::new_ease_state( realTime, 0_ms ),
                    .alpha = 0
                };
            }
        }
    }

    // Iterate the use target hints, ease them if necessary, and render them in order.
    for ( int32_t i = 0; i < MAX_TARGET_HINTS; i++ ) {
        // Get pointer to info data.
        hud_usetarget_hint_t *targetHintInfo = &hud.targetHints.hints[ i ];

        // The head info always is new, thus eases in. (It looks better if we apply EaseOut so.)
        if ( i == TOP_TARGET_HINT ) {
            targetHintInfo->alpha = targetHintInfo->easeState.EaseInOut( realTime, QM_ExponentialEaseInOut );
        // The others however, ease out. (It looks better if we apply EaseIn so.)
        } else {
            targetHintInfo->alpha = 1.0 - targetHintInfo->easeState.EaseInOut( realTime, QM_ExponentialEaseInOut );
        }

        // Draw the info.
        HUD_DrawTargetHintInfo( targetHintInfo );

        // Reset R color.
        clgi.R_ClearColor();
    }
}