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
*   CVars:
**/
//! We need this one.
extern cvar_t *scr_alpha;

//! The chat hud.
extern cvar_t *hud_chat;
extern cvar_t *hud_chat_lines;
extern cvar_t *hud_chat_time;
extern cvar_t *hud_chat_x;
extern cvar_t *hud_chat_y;

//! The crosshair.
extern cvar_t *hud_crosshair_type;
extern cvar_t *hud_crosshair_red;
extern cvar_t *hud_crosshair_green;
extern cvar_t *hud_crosshair_blue;
extern cvar_t *hud_crosshair_alpha;
extern cvar_t *hud_crosshair_scale;


/**
*   HUD UseTargetHint Configuration:
**/
//! Maximum target hint slots. (We really just need 2.)
static constexpr int32_t HUD_MAX_TARGET_HINTS = 2;
//! Index of the top most slot.
static constexpr int32_t HUD_TOP_TARGET_HINT = HUD_MAX_TARGET_HINTS - 1;
//! Bottom slot index.
static constexpr int32_t HUD_BOTTOM_TARGET_HINT = 0;
//! Duration of the in/oude ease fades.
static constexpr QMTime HUD_TARGET_INFO_EASE_DURATION = 100_ms;

/**
*   Chat Configuration:
**/
static constexpr int32_t HUD_MAX_CHAT_TEXT = 150;
static constexpr int32_t HUD_MAX_CHAT_LINES = 32;
static constexpr int32_t HUD_CHAT_LINE_MASK = HUD_MAX_CHAT_LINES - 1;
//! Chatline type.
typedef struct {
    char    text[ HUD_MAX_CHAT_TEXT ];
    QMTime  time;
} chatline_t;



/**
* 
* 
*   UseTargetHint:
* 
* 
**/
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
* 
* 
*   HUD State:
* 
* 
**/
struct hud_state_t {
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
        hud_usetarget_hint_t hints[ HUD_MAX_TARGET_HINTS ] = {};
    } targetHints = {};

    /**
    *   @brief  Chat State:
    **/
    struct {
        //! Chat lines.
        chatline_t      chatlines[ HUD_MAX_CHAT_LINES ];
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
};
//! Extern access for sub hud translation units.
extern hud_state_t clg_hud;

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
*   @brief  A very cheap way of.. getting the total pixel length of display string tokens, yes.
**/
const int32_t HUD_GetTokenVectorDrawWidth( const std::vector<hud_usetarget_hint_token_t> &tokens );
/**
*   @brief  Get width in pixels of string.
**/
const int32_t HUD_GetStringDrawWidth( const char *str );
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