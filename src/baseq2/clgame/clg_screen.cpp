/********************************************************************
*
*
*	ClientGame: Screen (2D Elements).
*
********************************************************************/
#include "clg_local.h"
#include "clg_screen.h"



/**
*
*   CVars
*
**/



/**
*
*   Screen Struct:
*
**/
static constexpr int32_t STAT_PICS = 11;
static constexpr int32_t STAT_MINUS = ( STAT_PICS - 1 );  // num frame for '-' stats digit

static struct {
    bool        initialized;        // ready to draw

    qhandle_t   crosshair_pic;
    int         crosshair_width, crosshair_height;
    color_t     crosshair_color;

    qhandle_t   pause_pic;
    int         pause_width, pause_height;

    qhandle_t   loading_pic;
    int         loading_width, loading_height;
    //bool        draw_loading;

    qhandle_t   sb_pics[ 2 ][ STAT_PICS ];
    qhandle_t   inven_pic;
    qhandle_t   field_pic;

    qhandle_t   backtile_pic;

    qhandle_t   net_pic;
    qhandle_t   font_pic;

    int         hud_width, hud_height;
    float       hud_scale;
    float       hud_alpha;
} scr;


//! Created by engine(client) fetched by Client Game:
static cvar_t *scr_viewsize;

//! Client Game Screen Specific:
static cvar_t *scr_centertime;
static cvar_t *scr_showpause;
#if USE_DEBUG
static cvar_t *scr_showstats;
static cvar_t *scr_showpmove;
#endif
static cvar_t *scr_showturtle;
static cvar_t *scr_showitemname;

static cvar_t *scr_draw2d;
static cvar_t *scr_lag_x;
static cvar_t *scr_lag_y;
static cvar_t *scr_lag_draw;
static cvar_t *scr_lag_min;
static cvar_t *scr_lag_max;
static cvar_t *scr_alpha;
static cvar_t *scr_fps;

static cvar_t *scr_demobar;
static cvar_t *scr_font;
static cvar_t *scr_scale;

static cvar_t *scr_crosshair;

static cvar_t *scr_chathud;
static cvar_t *scr_chathud_lines;
static cvar_t *scr_chathud_time;
static cvar_t *scr_chathud_x;
static cvar_t *scr_chathud_y;

static cvar_t *ch_health;
static cvar_t *ch_red;
static cvar_t *ch_green;
static cvar_t *ch_blue;
static cvar_t *ch_alpha;

static cvar_t *ch_scale;
static cvar_t *ch_x;
static cvar_t *ch_y;

vrect_t     scr_vrect;      // position of render window on screen

static const char *const sb_nums[ 2 ][ STAT_PICS ] = {
    {
        "num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
        "num_6", "num_7", "num_8", "num_9", "num_minus"
    },
    {
        "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
        "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
    }
};

const uint32_t colorTable[ 8 ] = {
    U32_BLACK, U32_RED, U32_GREEN, U32_YELLOW,
    U32_BLUE, U32_CYAN, U32_MAGENTA, U32_WHITE
};

/*
===============================================================================

UTILS

===============================================================================
*/
//static inline const int32_t SCR_DrawString( const int32_t x, const int32_t y, const int32_t flags, const char *str ) {
//    SCR_DrawStringEx( x, y, flags, MAX_STRING_CHARS, str, scr.font_pic );
//}
//#define SCR_DrawString(x, y, flags, string) \
//    SCR_DrawStringEx(x, y, flags, MAX_STRING_CHARS, string, scr.font_pic)

/*
==============
SCR_DrawStringEx
==============
*/
const int32_t SCR_DrawStringEx( int32_t x, int32_t y, int32_t flags, size_t maxlen,
    const char *s, qhandle_t font ) {
    size_t len = strlen( s );

    if ( len > maxlen ) {
        len = maxlen;
    }

    if ( ( flags & UI_CENTER ) == UI_CENTER ) {
        x -= len * CHAR_WIDTH / 2;
    } else if ( flags & UI_RIGHT ) {
        x -= len * CHAR_WIDTH;
    }

    return clgi.R_DrawString( x, y, flags, maxlen, s, font );
}

static inline const int32_t SCR_DrawString( const int32_t x, const int32_t y, const int32_t flags, const char *str ) {
    return SCR_DrawStringEx( x, y, flags, MAX_STRING_CHARS, str, scr.font_pic );
}

/*
==============
SCR_DrawStringMulti
==============
*/
void SCR_DrawStringMulti( int x, int y, int flags, size_t maxlen,
    const char *s, qhandle_t font ) {
    const char *p; // WID: C++20: Had no const.
    size_t  len;

    while ( *s ) {
        p = strchr( s, '\n' );
        if ( !p ) {
            SCR_DrawStringEx( x, y, flags, maxlen, s, font );
            break;
        }

        len = p - s;
        if ( len > maxlen ) {
            len = maxlen;
        }
        SCR_DrawStringEx( x, y, flags, len, s, font );

        y += CHAR_HEIGHT;
        s = p + 1;
    }
}


/*
=================
SCR_FadeAlpha
=================
*/
float SCR_FadeAlpha( unsigned startTime, unsigned visTime, unsigned fadeTime ) {
    float alpha;
    unsigned timeLeft, delta = clgi.GetRealTime()  - startTime;

    if ( delta >= visTime ) {
        return 0;
    }

    if ( fadeTime > visTime ) {
        fadeTime = visTime;
    }

    alpha = 1;
    timeLeft = visTime - delta;
    if ( timeLeft < fadeTime ) {
        alpha = (float)timeLeft / fadeTime;
    }

    return alpha;
}

//const qboolean SCR_ParseColor( const char *s, color_t *color ) {
//    int i;
//    int c[ 8 ];
//
//    // parse generic color
//    if ( *s == '#' ) {
//        s++;
//        for ( i = 0; s[ i ]; i++ ) {
//            if ( i == 8 ) {
//                return false;
//            }
//            c[ i ] = Q_charhex( s[ i ] );
//            if ( c[ i ] == -1 ) {
//                return false;
//            }
//        }
//
//        switch ( i ) {
//        case 3:
//            color->u8[ 0 ] = c[ 0 ] | ( c[ 0 ] << 4 );
//            color->u8[ 1 ] = c[ 1 ] | ( c[ 1 ] << 4 );
//            color->u8[ 2 ] = c[ 2 ] | ( c[ 2 ] << 4 );
//            color->u8[ 3 ] = 255;
//            break;
//        case 6:
//            color->u8[ 0 ] = c[ 1 ] | ( c[ 0 ] << 4 );
//            color->u8[ 1 ] = c[ 3 ] | ( c[ 2 ] << 4 );
//            color->u8[ 2 ] = c[ 5 ] | ( c[ 4 ] << 4 );
//            color->u8[ 3 ] = 255;
//            break;
//        case 8:
//            color->u8[ 0 ] = c[ 1 ] | ( c[ 0 ] << 4 );
//            color->u8[ 1 ] = c[ 3 ] | ( c[ 2 ] << 4 );
//            color->u8[ 2 ] = c[ 5 ] | ( c[ 4 ] << 4 );
//            color->u8[ 3 ] = c[ 7 ] | ( c[ 6 ] << 4 );
//            break;
//        default:
//            return false;
//        }
//
//        return true;
//    }
//
//    // parse name or index
//    i = Com_ParseColor( s );
//    if ( i >= q_countof( colorTable ) ) {
//        return false;
//    }
//
//    color->u32 = colorTable[ i ];
//    return true;
//}

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

static void draw_progress_bar( float progress, bool paused, int framenum ) {
    char buffer[ 16 ];
    int x, w, h;
    size_t len;

    w = Q_rint( scr.hud_width * progress );
    h = Q_rint( CHAR_HEIGHT / scr.hud_scale );

    scr.hud_height -= h;

    clgi.R_DrawFill8( 0, scr.hud_height, w, h, 4 );
    clgi.R_DrawFill8( w, scr.hud_height, scr.hud_width - w, h, 0 );

    clgi.R_SetScale( scr.hud_scale );

    w = Q_rint( scr.hud_width * scr.hud_scale );
    h = Q_rint( scr.hud_height * scr.hud_scale );

    len = Q_scnprintf( buffer, sizeof( buffer ), "%.f%%", progress * 100 );
    x = ( w - len * CHAR_WIDTH ) / 2;
    clgi.R_DrawString( x, h, 0, MAX_STRING_CHARS, buffer, scr.font_pic );

    if ( scr_demobar->integer > 1 ) {
        int sec = framenum / 10;
        int min = sec / 60; sec %= 60;

        Q_scnprintf( buffer, sizeof( buffer ), "%d:%02d.%d", min, sec, framenum % 10 );
        clgi.R_DrawString( 0, h, 0, MAX_STRING_CHARS, buffer, scr.font_pic );
    }

    if ( paused ) {
        SCR_DrawString( w, h, UI_RIGHT, "[PAUSED]" );
    }

    clgi.R_SetScale( 1.0f );
}

static void SCR_DrawDemo( void ) {
    if ( !scr_demobar->integer ) {
        return;
    }

    if ( clgi.IsDemoPlayback() ) {
        if ( clgi.GetDemoFileSize() ) {
            draw_progress_bar(
                clgi.GetDemoFileProgress(),
                sv_paused->integer &&
                cl_paused->integer &&
                scr_showpause->integer == 2,
                clgi.GetDemoFramesRead() );
        }
        return;
    }
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

static char     scr_centerstring[ MAX_STRING_CHARS ];
static unsigned scr_centertime_start;   // for slow victory printing
static int      scr_center_lines;

/**
*   @brief  Called for important messages that should stay in the center of the screen
*           for a few moments
**/
void SCR_CenterPrint( const char *str ) {
    const char *s;

    scr_centertime_start = clgi.GetRealTime();
    if ( !strcmp( scr_centerstring, str ) ) {
        return;
    }

    Q_strlcpy( scr_centerstring, str, sizeof( scr_centerstring ) );

    // count the number of lines for centering
    scr_center_lines = 1;
    s = str;
    while ( *s ) {
        if ( *s == '\n' )
            scr_center_lines++;
        s++;
    }

    // echo it to the console
    Com_Printf( "%s\n", scr_centerstring );
    clgi.Con_ClearNotify_f();
}

static void SCR_DrawCenterString( void ) {
    int y;
    float alpha;

    clgi.CVar_ClampValue( scr_centertime, 0.3f, 10.0f );

    alpha = SCR_FadeAlpha( scr_centertime_start, scr_centertime->value * 1000, 300 );
    if ( !alpha ) {
        return;
    }

    clgi.R_SetAlpha( alpha * scr_alpha->value );

    y = scr.hud_height / 4 - scr_center_lines * 8 / 2;

    SCR_DrawStringMulti( scr.hud_width / 2, y, UI_CENTER,
        MAX_STRING_CHARS, scr_centerstring, scr.font_pic );

    clgi.R_SetAlpha( scr_alpha->value );
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

//#define LAG_WIDTH   48
//#define LAG_HEIGHT  48
//
//#define LAG_CRIT_BIT    (1U << 31)
//#define LAG_WARN_BIT    (1U << 30)
//
//#define LAG_BASE    0xD5
//#define LAG_WARN    0xDC
//#define LAG_CRIT    0xF2
//
//static struct {
//    unsigned samples[ LAG_WIDTH ];
//    unsigned head;
//} lag;
//
//void SCR_LagClear( void ) {
//    lag.head = 0;
//}
//
//void SCR_LagSample( void ) {
//    int i = cls.netchan.incoming_acknowledged & CMD_MASK;
//    client_usercmd_history_t *h = &clgi.client->history[ i ];
//    unsigned ping;
//
//    h->timeReceived = clgi.GetRealTime();
//    if ( !h->commandNumber || h->timeReceived < h->timeSent ) {
//        return;
//    }
//
//    ping = h->timeReceived - h->timeSent;
//    for ( i = 0; i < cls.netchan.dropped; i++ ) {
//        lag.samples[ lag.head % LAG_WIDTH ] = ping | LAG_CRIT_BIT;
//        lag.head++;
//    }
//
//    if ( clgi.client->frameflags & FF_SUPPRESSED ) {
//        ping |= LAG_WARN_BIT;
//    }
//    lag.samples[ lag.head % LAG_WIDTH ] = ping;
//    lag.head++;
//}
//
//static void SCR_LagDraw( int x, int y ) {
//    int i, j, v, c, v_min, v_max, v_range;
//
//    v_min = clgi.CVar_ClampInteger( scr_lag_min, 0, LAG_HEIGHT * 10 );
//    v_max = clgi.CVar_ClampInteger( scr_lag_max, 0, LAG_HEIGHT * 10 );
//
//    v_range = v_max - v_min;
//    if ( v_range < 1 )
//        return;
//
//    for ( i = 0; i < LAG_WIDTH; i++ ) {
//        j = lag.head - i - 1;
//        if ( j < 0 ) {
//            break;
//        }
//
//        v = lag.samples[ j % LAG_WIDTH ];
//
//        if ( v & LAG_CRIT_BIT ) {
//            c = LAG_CRIT;
//        } else if ( v & LAG_WARN_BIT ) {
//            c = LAG_WARN;
//        } else {
//            c = LAG_BASE;
//        }
//
//        v &= ~( LAG_WARN_BIT | LAG_CRIT_BIT );
//        v = ( v - v_min ) * LAG_HEIGHT / v_range;
//        clamp( v, 0, LAG_HEIGHT );
//
//        clgi.R_DrawFill8( x + LAG_WIDTH - i - 1, y + LAG_HEIGHT - v, 1, v, c );
//    }
//}
//
//static void SCR_DrawNet( void ) {
//    int x = scr_lag_x->integer;
//    int y = scr_lag_y->integer;
//
//    if ( x < 0 ) {
//        x += scr.hud_width - LAG_WIDTH + 1;
//    }
//    if ( y < 0 ) {
//        y += scr.hud_height - LAG_HEIGHT + 1;
//    }
//
//    // draw ping graph
//    if ( scr_lag_draw->integer ) {
//        if ( scr_lag_draw->integer > 1 ) {
//            clgi.R_DrawFill8( x, y, LAG_WIDTH, LAG_HEIGHT, 4 );
//        }
//        SCR_LagDraw( x, y );
//    }
//
//    // draw phone jack
//    if ( cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged >= CMD_BACKUP ) {
//        if ( ( clgi.GetRealTime() >> 8 ) & 3 ) {
//            clgi.R_DrawStretchPic( x, y, LAG_WIDTH, LAG_HEIGHT, scr.net_pic );
//        }
//    }
//}


/*
===============================================================================

DRAW OBJECTS

===============================================================================
*/

typedef struct {
    list_t          entry;
    int             x, y;
    cvar_t *cvar;
    cmd_macro_t *macro;
    int             flags;
    color_t         color;
} drawobj_t;

#define FOR_EACH_DRAWOBJ(obj) \
    LIST_FOR_EACH(drawobj_t, obj, &scr_objects, entry)
#define FOR_EACH_DRAWOBJ_SAFE(obj, next) \
    LIST_FOR_EACH_SAFE(drawobj_t, obj, next, &scr_objects, entry)

static LIST_DECL( scr_objects );

///**
//*   @return A lowercase string matching the textual name of the color for colorIndex.
//**/
//const char *SCR_GetColorName( const color_index_t colorIndex ) {
//    if ( colorIndex >= 0 && colorIndex < COLOR_COUNT ) {
//        return colorNames[ colorIndex ];
//    }
//
//    return nullptr;
//}

void SCR_Color_g( genctx_t *ctx ) {
    int color;

    for ( color = 0; color < COLOR_COUNT; color++ ) {
        //Prompt_AddMatch( ctx, colorNames[ color ] );
        clgi.Prompt_AddMatch( ctx, clgi.SCR_GetColorName( (color_index_t)color ) );
    }
}

static void SCR_Draw_c( genctx_t *ctx, int argnum ) {
    if ( argnum == 1 ) {
        clgi.CVar_Variable_g( ctx );
        clgi.Cmd_Macro_g( ctx );
    } else if ( argnum == 4 ) {
        SCR_Color_g( ctx );
    }
}

// draw cl_fps -1 80
static void SCR_Draw_f( void ) {
    int x, y;
    const char *s, *c;
    drawobj_t *obj;
    cmd_macro_t *macro;
    cvar_t *cvar;
    color_t color;
    int flags;
    int argc = clgi.Cmd_Argc();

    if ( argc == 1 ) {
        if ( LIST_EMPTY( &scr_objects ) ) {
            Com_Printf( "No draw strings registered.\n" );
            return;
        }
        Com_Printf( "Name               X    Y\n"
            "--------------- ---- ----\n" );
        FOR_EACH_DRAWOBJ( obj ) {
            s = obj->macro ? obj->macro->name : obj->cvar->name;
            Com_Printf( "%-15s %4d %4d\n", s, obj->x, obj->y );
        }
        return;
    }

    if ( argc < 4 ) {
        Com_Printf( "Usage: %s <name> <x> <y> [color]\n", clgi.Cmd_Argv( 0 ) );
        return;
    }

    color.u32 = U32_BLACK;
    flags = UI_IGNORECOLOR;

    s = clgi.Cmd_Argv( 1 );
    x = atoi( clgi.Cmd_Argv( 2 ) );
    y = atoi( clgi.Cmd_Argv( 3 ) );

    if ( x < 0 ) {
        flags |= UI_RIGHT;
    }

    if ( argc > 4 ) {
        c = clgi.Cmd_Argv( 4 );
        if ( !strcmp( c, "alt" ) ) {
            flags |= UI_ALTCOLOR;
        } else if ( strcmp( c, "none" ) ) {
            if ( !clgi.SCR_ParseColor( c, &color ) ) {
                Com_Printf( "Unknown color '%s'\n", c );
                return;
            }
            flags &= ~UI_IGNORECOLOR;
        }
    }

    cvar = NULL;
    macro = clgi.Cmd_FindMacro( s );
    if ( !macro ) {
        cvar = clgi.CVar_WeakGet( s );
    }

    FOR_EACH_DRAWOBJ( obj ) {
        if ( obj->macro == macro && obj->cvar == cvar ) {
            obj->x = x;
            obj->y = y;
            obj->flags = flags;
            obj->color.u32 = color.u32;
            return;
        }
    }

    //obj = static_cast<drawobj_t *>( Z_Malloc( sizeof( *obj ) ) ); // WID: C++20: Was without cast.
    obj = static_cast<drawobj_t *>( clgi.Z_Malloc( sizeof( *obj ) ) ); // WID: C++20: Was without cast.
    obj->x = x;
    obj->y = y;
    obj->cvar = cvar;
    obj->macro = macro;
    obj->flags = flags;
    obj->color.u32 = color.u32;

    List_Append( &scr_objects, &obj->entry );
}

static void SCR_Draw_g( genctx_t *ctx ) {
    drawobj_t *obj;
    const char *s;

    if ( LIST_EMPTY( &scr_objects ) ) {
        return;
    }

    clgi.Prompt_AddMatch( ctx, "all" );

    FOR_EACH_DRAWOBJ( obj ) {
        s = obj->macro ? obj->macro->name : obj->cvar->name;
        clgi.Prompt_AddMatch( ctx, s );
    }
}

static void SCR_UnDraw_c( genctx_t *ctx, int argnum ) {
    if ( argnum == 1 ) {
        SCR_Draw_g( ctx );
    }
}

static void SCR_UnDraw_f( void ) {
    const char *s; // WID: C++20: Added const.
    drawobj_t *obj, *next;
    cmd_macro_t *macro;
    cvar_t *cvar;

    if ( clgi.Cmd_Argc() != 2 ) {
        Com_Printf( "Usage: %s <name>\n", clgi.Cmd_Argv( 0 ) );
        return;
    }

    if ( LIST_EMPTY( &scr_objects ) ) {
        Com_Printf( "No draw strings registered.\n" );
        return;
    }

    s = clgi.Cmd_Argv( 1 );
    if ( !strcmp( s, "all" ) ) {
        FOR_EACH_DRAWOBJ_SAFE( obj, next ) {
            clgi.Z_Free( obj );
        }
        List_Init( &scr_objects );
        Com_Printf( "Deleted all draw strings.\n" );
        return;
    }

    cvar = NULL;
    macro = clgi.Cmd_FindMacro( s );
    if ( !macro ) {
        cvar = clgi.CVar_WeakGet( s );
    }

    FOR_EACH_DRAWOBJ_SAFE( obj, next ) {
        if ( obj->macro == macro && obj->cvar == cvar ) {
            List_Remove( &obj->entry );
            clgi.Z_Free( obj );
            return;
        }
    }

    Com_Printf( "Draw string '%s' not found.\n", s );
}

static void SCR_DrawObjects( void ) {
    char buffer[ MAX_QPATH ];
    int x, y;
    drawobj_t *obj;

    FOR_EACH_DRAWOBJ( obj ) {
        x = obj->x;
        y = obj->y;
        if ( x < 0 ) {
            x += scr.hud_width + 1;
        }
        if ( y < 0 ) {
            y += scr.hud_height - CHAR_HEIGHT + 1;
        }
        if ( !( obj->flags & UI_IGNORECOLOR ) ) {
            clgi.R_SetColor( obj->color.u32 );
        }
        if ( obj->macro ) {
            obj->macro->function( buffer, sizeof( buffer ) );
            SCR_DrawString( x, y, obj->flags, buffer );
        } else {
            SCR_DrawString( x, y, obj->flags, obj->cvar->string );
        }
        if ( !( obj->flags & UI_IGNORECOLOR ) ) {
            clgi.R_ClearColor();
            clgi.R_SetAlpha( scr_alpha->value );
        }
    }
}

static void SCR_DrawFPS( void ) {
    if ( scr_fps->integer == 0 ) {
        return;
    }

    const int32_t fps = clgi.GetRefreshFps();
    const int32_t scale = clgi.GetResolutionScale();

    char buffer[ MAX_QPATH ];
    if ( scr_fps->integer == 2 && clgi.GetRefreshType() == REF_TYPE_VKPT ) {
        Q_snprintf( buffer, MAX_QPATH, "%d FPS at %3d%%", fps, scale );
    } else {
        Q_snprintf( buffer, MAX_QPATH, "%d FPS", fps );
    }

    const int32_t x = scr.hud_width - 2;
    const int32_t y = 1;

    clgi.R_SetColor( ~0u );
    SCR_DrawString( x, y, UI_RIGHT, buffer );
}

/*
===============================================================================

CHAT HUD

===============================================================================
*/

#define MAX_CHAT_TEXT       150
#define MAX_CHAT_LINES      32
#define CHAT_LINE_MASK      (MAX_CHAT_LINES - 1)

typedef struct {
    char        text[ MAX_CHAT_TEXT ];
    unsigned    time;
} chatline_t;

static chatline_t   scr_chatlines[ MAX_CHAT_LINES ];
static unsigned     scr_chathead;

/**
*   @brief  Clear the chat HUD.
**/
void SCR_ClearChatHUD_f( void ) {
    memset( scr_chatlines, 0, sizeof( scr_chatlines ) );
    scr_chathead = 0;
}

/**
*   @brief  Append text to chat HUD.
**/
void SCR_AddToChatHUD( const char *text ) {
    chatline_t *line;
    char *p;

    line = &scr_chatlines[ scr_chathead++ & CHAT_LINE_MASK ];
    Q_strlcpy( line->text, text, sizeof( line->text ) );
    line->time = clgi.GetRealTime();

    p = strrchr( line->text, '\n' );
    if ( p )
        *p = 0;
}

static void SCR_DrawChatHUD( void ) {
    int x, y, i, lines, flags, step;
    float alpha;
    chatline_t *line;

    if ( scr_chathud->integer == 0 )
        return;

    x = scr_chathud_x->integer;
    y = scr_chathud_y->integer;

    if ( scr_chathud->integer == 2 )
        flags = UI_ALTCOLOR;
    else
        flags = 0;

    if ( x < 0 ) {
        x += scr.hud_width + 1;
        flags |= UI_RIGHT;
    } else {
        flags |= UI_LEFT;
    }

    if ( y < 0 ) {
        y += scr.hud_height - CHAR_HEIGHT + 1;
        step = -CHAR_HEIGHT;
    } else {
        step = CHAR_HEIGHT;
    }

    lines = scr_chathud_lines->integer;
    if ( lines > scr_chathead )
        lines = scr_chathead;

    for ( i = 0; i < lines; i++ ) {
        line = &scr_chatlines[ ( scr_chathead - i - 1 ) & CHAT_LINE_MASK ];

        if ( scr_chathud_time->integer ) {
            alpha = SCR_FadeAlpha( line->time, scr_chathud_time->integer, 1000 );
            if ( !alpha )
                break;

            clgi.R_SetAlpha( alpha * scr_alpha->value );
            SCR_DrawString( x, y, flags, line->text );
            clgi.R_SetAlpha( scr_alpha->value );
        } else {
            SCR_DrawString( x, y, flags, line->text );
        }

        y += step;
    }
}

/*
===============================================================================

DEBUG STUFF

===============================================================================
*/

static void SCR_DrawTurtle( void ) {
    int x, y;

    if ( scr_showturtle->integer <= 0 )
        return;

    if ( !clgi.client->frameflags )
        return;

    x = CHAR_WIDTH;
    y = scr.hud_height - 11 * CHAR_HEIGHT;

    #define DF(f) \
        if (clgi.client->frameflags & FF_##f) { \
            SCR_DrawString(x, y, UI_ALTCOLOR, #f); \
            y += CHAR_HEIGHT; \
        }

    if ( scr_showturtle->integer > 1 ) {
        DF( SUPPRESSED )
    }
    DF( CLIENTPRED )
        if ( scr_showturtle->integer > 1 ) {
            DF( CLIENTDROP )
                DF( SERVERDROP )
        }
    DF( BADFRAME )
        DF( OLDFRAME )
        DF( OLDENT )
        DF( NODELTA )

        #undef DF
}

#if USE_DEBUG

static void SCR_DrawDebugStats( void ) {
    char buffer[ MAX_QPATH ];
    int i, j;
    int x, y;

    j = scr_showstats->integer;
    if ( j <= 0 )
        return;

    if ( j > MAX_STATS )
        j = MAX_STATS;

    x = CHAR_WIDTH;
    y = ( scr.hud_height - j * CHAR_HEIGHT ) / 2;
    for ( i = 0; i < j; i++ ) {
        Q_snprintf( buffer, sizeof( buffer ), "%2d: %d", i, clgi.client->frame.ps.stats[ i ] );
        if ( clgi.client->oldframe.ps.stats[ i ] != clgi.client->frame.ps.stats[ i ] ) {
            clgi.R_SetColor( U32_RED );
        }
        clgi.R_DrawString( x, y, 0, MAX_STRING_CHARS, buffer, scr.font_pic );
        clgi.R_ClearColor();
        y += CHAR_HEIGHT;
    }
}

static void SCR_DrawDebugPmove( void ) {
    static const char *const types[] = {
        "NORMAL", "GRAPPLE", "NOCLIP", "SPECTATOR", 
        "DEAD", "GIB", "FREEZE"
    };
    static const char *const flags[] = {
        //"NONE",
        "DUCKED", "JUMP_HELD", "ON_GROUND",
        "TIME_WATERJUMP", "TIME_LAND", "TIME_TELEPORT",
        "NO_POSITIONAL_PREDICTION", //"TELEPORT_BIT"
        "ON_LADDER",
        "NO_ANGULAR_PREDICTION",
        "IGNORE_PLAYER_COLLISION",
        "TIME_TRICK_JUMP"
    };
    unsigned i, j;
    int x, y;

    if ( !scr_showpmove->integer )
        return;

    x = CHAR_WIDTH;
    y = ( scr.hud_height - 2 * CHAR_HEIGHT ) / 2;

    i = clgi.client->frame.ps.pmove.pm_type;
    if ( i > PM_FREEZE )
        i = PM_FREEZE;

    clgi.R_DrawString( x, y, 0, MAX_STRING_CHARS, types[ i ], scr.font_pic );
    y += CHAR_HEIGHT;

    j = clgi.client->frame.ps.pmove.pm_flags;
    for ( i = 0; i < 11; i++ ) {
        if ( j & ( 1 << i ) ) {
            x = clgi.R_DrawString( x, y, 0, MAX_STRING_CHARS, flags[ i ], scr.font_pic );
            x += CHAR_WIDTH;
        }
    }
}

#endif

//============================================================================

// Sets scr_vrect, the coordinates of the rendered window
void SCR_CalcVrect( void ) {
    scr_vrect.width = scr.hud_width;
    scr_vrect.height = scr.hud_height;
    scr_vrect.x = 0;
    scr_vrect.y = 0;
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
static void SCR_SizeUp_f( void ) {
    int delta = ( scr_viewsize->integer < 100 ) ? 5 : 10;
    clgi.CVar_SetInteger( scr_viewsize, scr_viewsize->integer + delta, FROM_CONSOLE );
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
static void SCR_SizeDown_f( void ) {
    int delta = ( scr_viewsize->integer <= 100 ) ? 5 : 10;
    clgi.CVar_SetInteger( scr_viewsize, scr_viewsize->integer - delta, FROM_CONSOLE );
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed. If empty sky name is provided, falls
back to server defaults.
=================
*/
//static void SCR_Sky_f( void ) {
//    char *name;
//    float   rotate;
//    vec3_t  axis;
//    int     argc = clgi.Cmd_Argc();
//
//    if ( argc < 2 ) {
//        Com_Printf( "Usage: sky <basename> [rotate] [axis x y z]\n" );
//        return;
//    }
//
//    if ( clgi.GetConnectionState() != ca_active ) {
//        Com_Printf( "No map loaded.\n" );
//        return;
//    }
//
//    name = clgi.Cmd_Argv( 1 );
//    if ( !*name ) {
//        clgi.CL_SetSky();
//        return;
//    }
//
//    if ( argc > 2 )
//        rotate = atof( clgi.Cmd_Argv( 2 ) );
//    else
//        rotate = 0;
//
//    if ( argc == 6 ) {
//        axis[ 0 ] = atof( clgi.Cmd_Argv( 3 ) );
//        axis[ 1 ] = atof( clgi.Cmd_Argv( 4 ) );
//        axis[ 2 ] = atof( clgi.Cmd_Argv( 5 ) );
//    } else
//        VectorSet( axis, 0, 0, 1 );
//
//    clgi.R_SetSky( name, rotate, 1, axis );
//}

/*
================
SCR_TimeRefresh_f
================
*/
//static void SCR_TimeRefresh_f( void ) {
//    int     i;
//    uint64_t start, stop;
//    float       time;
//
//    if ( cls.state != ca_active ) {
//        Com_Printf( "No map loaded.\n" );
//        return;
//    }
//
//    start = Sys_Milliseconds();
//
//    if ( Cmd_Argc() == 2 ) {
//        // run without page flipping
//        clgi.R_BeginFrame();
//        for ( i = 0; i < 128; i++ ) {
//            clgi.client->refdef.viewangles[ 1 ] = i / 128.0f * 360.0f;
//            clgi.R_RenderFrame( &clgi.client->refdef );
//        }
//        clgi.R_EndFrame();
//    } else {
//        for ( i = 0; i < 128; i++ ) {
//            clgi.client->refdef.viewangles[ 1 ] = i / 128.0f * 360.0f;
//
//            clgi.R_BeginFrame();
//            clgi.R_RenderFrame( &clgi.client->refdef );
//            clgi.R_EndFrame();
//        }
//    }
//
//    stop = Sys_Milliseconds();
//    time = ( stop - start ) * 0.001f;
//    Com_Printf( "%f seconds (%f fps)\n", time, 128.0f / time );
//}


//============================================================================

/**
*	@brief
**/
static void scr_crosshair_changed( cvar_t *self ) {
    char buffer[ 16 ];
    int w, h;
    float scale;

    if ( scr_crosshair->integer > 0 ) {
        Q_snprintf( buffer, sizeof( buffer ), "ch%i", scr_crosshair->integer );
        scr.crosshair_pic = clgi.R_RegisterPic( buffer );
        clgi.R_GetPicSize( &w, &h, scr.crosshair_pic );

        // prescale
        scale = clgi.CVar_ClampValue( ch_scale, 0.1f, 9.0f );
        scr.crosshair_width = w * scale;
        scr.crosshair_height = h * scale;
        if ( scr.crosshair_width < 1 )
            scr.crosshair_width = 1;
        if ( scr.crosshair_height < 1 )
            scr.crosshair_height = 1;

        if ( ch_health->integer ) {
            PF_SCR_SetCrosshairColor();
        } else {
            scr.crosshair_color.u8[ 0 ] = clgi.CVar_ClampValue( ch_red, 0, 1 ) * 255;
            scr.crosshair_color.u8[ 1 ] = clgi.CVar_ClampValue( ch_green, 0, 1 ) * 255;
            scr.crosshair_color.u8[ 2 ] = clgi.CVar_ClampValue( ch_blue, 0, 1 ) * 255;
        }
        scr.crosshair_color.u8[ 3 ] = clgi.CVar_ClampValue( ch_alpha, 0, 1 ) * 255;
    } else {
        scr.crosshair_pic = 0;
    }
}
/**
*	@brief
**/
void PF_SCR_SetCrosshairColor( void ) {
    int health;

    if ( !ch_health->integer ) {
        return;
    }

    health = clgi.client->frame.ps.stats[ STAT_HEALTH ];
    if ( health <= 0 ) {
        VectorSet( scr.crosshair_color.u8, 0, 0, 0 );
        return;
    }

    // red
    scr.crosshair_color.u8[ 0 ] = 255;

    // green
    if ( health >= 66 ) {
        scr.crosshair_color.u8[ 1 ] = 255;
    } else if ( health < 33 ) {
        scr.crosshair_color.u8[ 1 ] = 0;
    } else {
        scr.crosshair_color.u8[ 1 ] = ( 255 * ( health - 33 ) ) / 33;
    }

    // blue
    if ( health >= 99 ) {
        scr.crosshair_color.u8[ 2 ] = 255;
    } else if ( health < 66 ) {
        scr.crosshair_color.u8[ 2 ] = 0;
    } else {
        scr.crosshair_color.u8[ 2 ] = ( 255 * ( health - 66 ) ) / 33;
    }
}
/**
*	@brief
**/
void PF_SCR_ModeChanged( void ) {
    //IN_Activate();
    //Con_CheckResize();
    //UI_ModeChanged();
    //cls.disable_screen = 0;
    if ( scr.initialized )
        scr.hud_scale = clgi.R_ClampScale( scr_scale );

    scr.hud_alpha = 1.f;
}

/**
*	@brief
**/
void PF_SCR_RegisterMedia( void ) {
    int     i, j;

    for ( i = 0; i < 2; i++ )
        for ( j = 0; j < STAT_PICS; j++ )
            scr.sb_pics[ i ][ j ] = clgi.R_RegisterPic( sb_nums[ i ][ j ] );

    scr.inven_pic = clgi.R_RegisterPic( "inventory" );
    scr.field_pic = clgi.R_RegisterPic( "field_3" );

    scr.backtile_pic = clgi.R_RegisterImage( "backtile", IT_PIC, static_cast<imageflags_t>( IF_PERMANENT | IF_REPEAT ) ); // WID: C++20: Added static cast

    scr.pause_pic = clgi.R_RegisterPic( "pause" );
    clgi.R_GetPicSize( &scr.pause_width, &scr.pause_height, scr.pause_pic );

    scr.loading_pic = clgi.R_RegisterPic( "loading" );
    clgi.R_GetPicSize( &scr.loading_width, &scr.loading_height, scr.loading_pic );

    scr.net_pic = clgi.R_RegisterPic( "net" );
    scr.font_pic = clgi.R_RegisterFont( scr_font->string );

    scr_crosshair_changed( scr_crosshair );
}
/**
*	@brief
**/
static void scr_font_changed( cvar_t *self ) {
    scr.font_pic = clgi.R_RegisterFont( self->string );
}
/**
*	@brief
**/
static void scr_scale_changed( cvar_t *self ) {
    scr.hud_scale = clgi.R_ClampScale( self );
}
/**
*	@brief
**/
void cl_timeout_changed( cvar_t *self ) {
    self->integer = 1000 * clgi.CVar_ClampValue( self, 0, 24 * 24 * 60 * 60 );
}

static const cmdreg_t scr_cmds[] = {
    //{ "timerefresh", SCR_TimeRefresh_f },
    { "sizeup", SCR_SizeUp_f },
    { "sizedown", SCR_SizeDown_f },
    //{ "sky", SCR_Sky_f },
    { "draw", SCR_Draw_f, SCR_Draw_c },
    { "undraw", SCR_UnDraw_f, SCR_UnDraw_c },
    { "clearchathud", SCR_ClearChatHUD_f },
    { NULL }
};

/**
*	@brief
**/
void PF_SCR_Init( void ) {
    // Fetch created by engine CVars.
    scr_viewsize = clgi.CVar_Get( "viewsize", nullptr, 0 );

    // Created by client game:
    scr_showpause = clgi.CVar_Get( "scr_showpause", "1", 0 );
    scr_centertime = clgi.CVar_Get( "scr_centertime", "2.5", 0 );
    scr_demobar = clgi.CVar_Get( "scr_demobar", "1", 0 );
    scr_font = clgi.CVar_Get( "scr_font", "conchars", 0 );
    scr_font->changed = scr_font_changed;
    scr_scale = clgi.CVar_Get( "scr_scale", "0", 0 );
    scr_scale->changed = scr_scale_changed;
    scr_crosshair = clgi.CVar_Get( "crosshair", "0", CVAR_ARCHIVE );
    scr_crosshair->changed = scr_crosshair_changed;

    scr_chathud = clgi.CVar_Get( "scr_chathud", "0", 0 );
    scr_chathud_lines = clgi.CVar_Get( "scr_chathud_lines", "4", 0 );
    scr_chathud_time = clgi.CVar_Get( "scr_chathud_time", "0", 0 );
    scr_chathud_time->changed = cl_timeout_changed;
    scr_chathud_time->changed( scr_chathud_time );
    scr_chathud_x = clgi.CVar_Get( "scr_chathud_x", "8", 0 );
    scr_chathud_y = clgi.CVar_Get( "scr_chathud_y", "-64", 0 );

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

    scr_draw2d = clgi.CVar_Get( "scr_draw2d", "2", 0 );
    scr_showturtle = clgi.CVar_Get( "scr_showturtle", "1", 0 );
    scr_showitemname = clgi.CVar_Get( "scr_showitemname", "1", CVAR_ARCHIVE );
    scr_lag_x = clgi.CVar_Get( "scr_lag_x", "-1", 0 );
    scr_lag_y = clgi.CVar_Get( "scr_lag_y", "-1", 0 );
    scr_lag_draw = clgi.CVar_Get( "scr_lag_draw", "0", 0 );
    scr_lag_min = clgi.CVar_Get( "scr_lag_min", "0", 0 );
    scr_lag_max = clgi.CVar_Get( "scr_lag_max", "200", 0 );
    scr_alpha = clgi.CVar_Get( "scr_alpha", "1", 0 );
    #if USE_DEBUG
    scr_fps = clgi.CVar_Get( "scr_fps", "2", CVAR_ARCHIVE );
    #else
    scr_fps = clgi.CVar_Get( "scr_fps", "0", CVAR_ARCHIVE );
    #endif
    #ifdef USE_DEBUG
    scr_showstats = clgi.CVar_Get( "scr_showstats", "0", 0 );
    scr_showpmove = clgi.CVar_Get( "scr_showpmove", "0", 0 );
    #endif

    clgi.Cmd_Register( scr_cmds );

    scr_scale_changed( scr_scale );

    scr.initialized = true;
}
/**
*	@brief  
**/
void PF_SCR_Shutdown( void ) {
    clgi.Cmd_Deregister( scr_cmds );
    scr.initialized = false;
}

/**
*	@return	Pointer to the current frame's render "view rectangle".
**/
vrect_t *PF_GetScreenVideoRect( void ) {
    return &scr_vrect;
}

//=============================================================================

// Clear any parts of the tiled background that were drawn on last frame
static void SCR_TileClear( void ) {
}

/*
===============================================================================

STAT PROGRAMS

===============================================================================
*/

#define ICON_WIDTH  24
#define ICON_HEIGHT 24
#define DIGIT_WIDTH 16
#define ICON_SPACE  8

#define HUD_DrawString(x, y, string) \
    clgi.R_DrawString(x, y, 0, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawAltString(x, y, string) \
    clgi.R_DrawString(x, y, UI_XORCOLOR, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawCenterString(x, y, string) \
    SCR_DrawStringMulti(x, y, UI_CENTER, MAX_STRING_CHARS, string, scr.font_pic)

#define HUD_DrawAltCenterString(x, y, string) \
    SCR_DrawStringMulti(x, y, UI_CENTER | UI_XORCOLOR, MAX_STRING_CHARS, string, scr.font_pic)

static void HUD_DrawNumber( int x, int y, int color, int width, int value ) {
    char    num[ 16 ], *ptr;
    int     l;
    int     frame;

    if ( width < 1 )
        return;

    // draw number string
    if ( width > 5 )
        width = 5;

    color &= 1;

    l = Q_scnprintf( num, sizeof( num ), "%i", value );
    if ( l > width )
        l = width;
    x += 2 + DIGIT_WIDTH * ( width - l );

    ptr = num;
    while ( *ptr && l ) {
        if ( *ptr == '-' )
            frame = STAT_MINUS;
        else
            frame = *ptr - '0';

        clgi.R_DrawPic( x, y, scr.sb_pics[ color ][ frame ] );
        x += DIGIT_WIDTH;
        ptr++;
        l--;
    }
}

#define DISPLAY_ITEMS   17

static void SCR_DrawInventory( void ) {
    int     i;
    int     num, selected_num, item;
    int     index[ MAX_ITEMS ];
    char    string[ MAX_STRING_CHARS ];
    int     x, y;
    const char *bind;
    int     selected;
    int     top;

    if ( !( clgi.client->frame.ps.stats[ STAT_LAYOUTS ] & 2 ) )
        return;

    selected = clgi.client->frame.ps.stats[ STAT_SELECTED_ITEM ];

    num = 0;
    selected_num = 0;
    for ( i = 0; i < MAX_ITEMS; i++ ) {
        if ( i == selected ) {
            selected_num = num;
        }
        if ( clgi.client->inventory[ i ] ) {
            index[ num++ ] = i;
        }
    }

    // determine scroll point
    top = selected_num - DISPLAY_ITEMS / 2;
    if ( top > num - DISPLAY_ITEMS ) {
        top = num - DISPLAY_ITEMS;
    }
    if ( top < 0 ) {
        top = 0;
    }

    x = ( scr.hud_width - 256 ) / 2;
    y = ( scr.hud_height - 240 ) / 2;

    clgi.R_DrawPic( x, y + 8, scr.inven_pic );
    y += 24;
    x += 24;

    HUD_DrawString( x, y, "hotkey ### item" );
    y += CHAR_HEIGHT;

    HUD_DrawString( x, y, "------ --- ----" );
    y += CHAR_HEIGHT;

    for ( i = top; i < num && i < top + DISPLAY_ITEMS; i++ ) {
        item = index[ i ];
        // search for a binding
        Q_concat( string, sizeof( string ), "use ", clgi.client->configstrings[ CS_ITEMS + item ] );
        bind = clgi.Key_GetBinding( string );

        Q_snprintf( string, sizeof( string ), "%6s %3i %s",
            bind, clgi.client->inventory[ item ], clgi.client->configstrings[ CS_ITEMS + item ] );

        if ( item != selected ) {
            HUD_DrawAltString( x, y, string );
        } else {    // draw a blinky cursor by the selected item
            HUD_DrawString( x, y, string );
            if ( ( clgi.GetRealTime() >> 8 ) & 1 ) {
                clgi.R_DrawChar( x - CHAR_WIDTH, y, 0, 15, scr.font_pic );
            }
        }

        y += CHAR_HEIGHT;
    }
}

static void SCR_DrawSelectedItemName( int x, int y, int item ) {
    static int display_item = -1;
    static int64_t display_start_time = 0;

    double duration = 0.f;
    if ( display_item != item ) {
        display_start_time = clgi.Sys_Milliseconds();
        display_item = item;
    } else {
        duration = (double)( clgi.Sys_Milliseconds() - display_start_time ) * 0.001f;
    }

    float alpha;
    if ( scr_showitemname->integer < 2 )
        alpha = std::max( (double)0., min( 1., 5. - 4. * duration ) ); // show and hide
    else
        alpha = 1; // always show

    if ( alpha > 0.f ) {
        clgi.R_SetAlpha( alpha * scr_alpha->value );

        int index = CS_ITEMS + item;
        HUD_DrawString( x, y, clgi.client->configstrings[ index ] );

        clgi.R_SetAlpha( scr_alpha->value );
    }
}

static void SCR_ExecuteLayoutString( const char *s ) {
    char    buffer[ MAX_QPATH ];
    int     x, y;
    int     value;
    char *token;
    int     width;
    int     index;
    clientinfo_t *ci;

    if ( !s[ 0 ] )
        return;

    x = 0;
    y = 0;

    while ( s ) {
        token = COM_Parse( &s );
        if ( token[ 2 ] == 0 ) {
            if ( token[ 0 ] == 'x' ) {
                if ( token[ 1 ] == 'l' ) {
                    token = COM_Parse( &s );
                    x = atoi( token );
                    continue;
                }

                if ( token[ 1 ] == 'r' ) {
                    token = COM_Parse( &s );
                    x = scr.hud_width + atoi( token );
                    continue;
                }

                if ( token[ 1 ] == 'v' ) {
                    token = COM_Parse( &s );
                    x = scr.hud_width / 2 - 160 + atoi( token );
                    continue;
                }
            }

            if ( token[ 0 ] == 'y' ) {
                if ( token[ 1 ] == 't' ) {
                    token = COM_Parse( &s );
                    y = atoi( token );
                    continue;
                }

                if ( token[ 1 ] == 'b' ) {
                    token = COM_Parse( &s );
                    y = scr.hud_height + atoi( token );
                    continue;
                }

                if ( token[ 1 ] == 'v' ) {
                    token = COM_Parse( &s );
                    y = scr.hud_height / 2 - 120 + atoi( token );
                    continue;
                }
            }
        }

        if ( !strcmp( token, "pic" ) ) {
            // draw a pic from a stat number
            token = COM_Parse( &s );
            value = atoi( token );
            if ( value < 0 || value >= MAX_STATS ) {
                Com_Error( ERR_DROP, "%s: invalid stat index", __func__ );
            }
            index = clgi.client->frame.ps.stats[ value ];
            if ( index < 0 || index >= MAX_IMAGES ) {
                Com_Error( ERR_DROP, "%s: invalid pic index", __func__ );
            }
            token = clgi.client->configstrings[ CS_IMAGES + index ];
            if ( token[ 0 ] && clgi.client->image_precache[ index ] ) {
                qhandle_t pic = clgi.client->image_precache[ index ];
                // hack for action mod scope scaling
                if ( x == scr.hud_width / 2 - 160 &&
                    y == scr.hud_height / 2 - 120 &&
                    Com_WildCmp( "scope?x", token ) ) {
                    int w = 320 * ch_scale->value;
                    int h = 240 * ch_scale->value;
                    clgi.R_DrawStretchPic( ( scr.hud_width - w ) / 2 + ch_x->integer,
                        ( scr.hud_height - h ) / 2 + ch_y->integer,
                        w, h, pic );
                } else {
                    clgi.R_DrawPic( x, y, pic );
                }
            }

            if ( value == STAT_SELECTED_ICON && scr_showitemname->integer ) {
                SCR_DrawSelectedItemName( x + 32, y + 8, clgi.client->frame.ps.stats[ STAT_SELECTED_ITEM ] );
            }
            continue;
        }

        if ( !strcmp( token, "client" ) ) {
            // draw a deathmatch client block
            int     score, ping, time;

            token = COM_Parse( &s );
            x = scr.hud_width / 2 - 160 + atoi( token );
            token = COM_Parse( &s );
            y = scr.hud_height / 2 - 120 + atoi( token );

            token = COM_Parse( &s );
            value = atoi( token );
            if ( value < 0 || value >= MAX_CLIENTS ) {
                Com_Error( ERR_DROP, "%s: invalid client index", __func__ );
            }
            ci = &clgi.client->clientinfo[ value ];

            token = COM_Parse( &s );
            score = atoi( token );

            token = COM_Parse( &s );
            ping = atoi( token );

            token = COM_Parse( &s );
            time = atoi( token );

            HUD_DrawAltString( x + 32, y, ci->name );
            HUD_DrawString( x + 32, y + CHAR_HEIGHT, "Score: " );
            Q_snprintf( buffer, sizeof( buffer ), "%i", score );
            HUD_DrawAltString( x + 32 + 7 * CHAR_WIDTH, y + CHAR_HEIGHT, buffer );
            Q_snprintf( buffer, sizeof( buffer ), "Ping:  %i", ping );
            HUD_DrawString( x + 32, y + 2 * CHAR_HEIGHT, buffer );
            Q_snprintf( buffer, sizeof( buffer ), "Time:  %i", time );
            HUD_DrawString( x + 32, y + 3 * CHAR_HEIGHT, buffer );

            if ( !ci->icon ) {
                ci = &clgi.client->baseclientinfo;
            }
            clgi.R_DrawPic( x, y, ci->icon );
            continue;
        }

        if ( !strcmp( token, "ctf" ) ) {
            // draw a ctf client block
            int     score, ping;

            token = COM_Parse( &s );
            x = scr.hud_width / 2 - 160 + atoi( token );
            token = COM_Parse( &s );
            y = scr.hud_height / 2 - 120 + atoi( token );

            token = COM_Parse( &s );
            value = atoi( token );
            if ( value < 0 || value >= MAX_CLIENTS ) {
                Com_Error( ERR_DROP, "%s: invalid client index", __func__ );
            }
            ci = &clgi.client->clientinfo[ value ];

            token = COM_Parse( &s );
            score = atoi( token );

            token = COM_Parse( &s );
            ping = atoi( token );
            if ( ping > 999 )
                ping = 999;

            Q_snprintf( buffer, sizeof( buffer ), "%3d %3d %-12.12s",
                score, ping, ci->name );
            if ( value == clgi.client->frame.clientNum ) {
                HUD_DrawAltString( x, y, buffer );
            } else {
                HUD_DrawString( x, y, buffer );
            }
            continue;
        }

        if ( !strcmp( token, "picn" ) ) {
            // draw a pic from a name
            token = COM_Parse( &s );
            clgi.R_DrawPic( x, y, clgi.R_RegisterPic2( token ) );
            continue;
        }

        if ( !strcmp( token, "num" ) ) {
            // draw a number
            token = COM_Parse( &s );
            width = atoi( token );
            token = COM_Parse( &s );
            value = atoi( token );
            if ( value < 0 || value >= MAX_STATS ) {
                Com_Error( ERR_DROP, "%s: invalid stat index", __func__ );
            }
            value = clgi.client->frame.ps.stats[ value ];
            HUD_DrawNumber( x, y, 0, width, value );
            continue;
        }

        if ( !strcmp( token, "hnum" ) ) {
            // health number
            int     color;

            width = 3;
            value = clgi.client->frame.ps.stats[ STAT_HEALTH ];
            if ( value > 25 )
                color = 0;  // green
            else if ( value > 0 )
                color = ( ( clgi.client->frame.number ) >> 2 ) & 1;     // flash
            else
                color = 1;

            if ( clgi.client->frame.ps.stats[ STAT_FLASHES ] & 1 )
                clgi.R_DrawPic( x, y, scr.field_pic );

            HUD_DrawNumber( x, y, color, width, value );
            continue;
        }

        if ( !strcmp( token, "anum" ) ) {
            // ammo number
            int     color;

            width = 3;
            value = clgi.client->frame.ps.stats[ STAT_AMMO ];
            if ( value > 5 )
                color = 0;  // green
            else if ( value >= 0 )
                color = ( ( clgi.client->frame.number ) >> 2 ) & 1;     // flash
            else
                continue;   // negative number = don't show

            if ( clgi.client->frame.ps.stats[ STAT_FLASHES ] & 4 )
                clgi.R_DrawPic( x, y, scr.field_pic );

            HUD_DrawNumber( x, y, color, width, value );
            continue;
        }

        if ( !strcmp( token, "rnum" ) ) {
            // armor number
            int     color;

            width = 3;
            value = clgi.client->frame.ps.stats[ STAT_ARMOR ];
            if ( value < 1 )
                continue;

            color = 0;  // green

            if ( clgi.client->frame.ps.stats[ STAT_FLASHES ] & 2 )
                clgi.R_DrawPic( x, y, scr.field_pic );

            HUD_DrawNumber( x, y, color, width, value );
            continue;
        }

        if ( !strcmp( token, "stat_string" ) ) {
            token = COM_Parse( &s );
            index = atoi( token );
            if ( index < 0 || index >= MAX_STATS ) {
                Com_Error( ERR_DROP, "%s: invalid stat index", __func__ );
            }
            index = clgi.client->frame.ps.stats[ index ];
            if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
                Com_Error( ERR_DROP, "%s: invalid string index", __func__ );
            }
            HUD_DrawString( x, y, clgi.client->configstrings[ index ] );
            continue;
        }

        if ( !strcmp( token, "cstring" ) ) {
            token = COM_Parse( &s );
            HUD_DrawCenterString( x + 320 / 2, y, token );
            continue;
        }

        if ( !strcmp( token, "cstring2" ) ) {
            token = COM_Parse( &s );
            HUD_DrawAltCenterString( x + 320 / 2, y, token );
            continue;
        }

        if ( !strcmp( token, "string" ) ) {
            token = COM_Parse( &s );
            HUD_DrawString( x, y, token );
            continue;
        }

        if ( !strcmp( token, "string2" ) ) {
            token = COM_Parse( &s );
            HUD_DrawAltString( x, y, token );
            continue;
        }

        if ( !strcmp( token, "if" ) ) {
            token = COM_Parse( &s );
            value = atoi( token );
            if ( value < 0 || value >= MAX_STATS ) {
                Com_Error( ERR_DROP, "%s: invalid stat index", __func__ );
            }
            value = clgi.client->frame.ps.stats[ value ];
            if ( !value ) {   // skip to endif
                while ( strcmp( token, "endif" ) ) {
                    token = COM_Parse( &s );
                    if ( !s ) {
                        break;
                    }
                }
            }
            continue;
        }

        // Q2PRO extension
        if ( !strcmp( token, "color" ) ) {
            color_t     color;

            token = COM_Parse( &s );
            if ( clgi.SCR_ParseColor( token, &color ) ) {
                color.u8[ 3 ] *= scr_alpha->value;
                clgi.R_SetColor( color.u32 );
            }
            continue;
        }
    }

    clgi.R_ClearColor();
    clgi.R_SetAlpha( scr_alpha->value );
}

//=============================================================================

static void SCR_DrawPause( void ) {
    int x, y;

    if ( !sv_paused->integer )
        return;
    if ( !cl_paused->integer )
        return;
    if ( scr_showpause->integer != 1 )
        return;

    x = ( scr.hud_width - scr.pause_width ) / 2;
    y = ( scr.hud_height - scr.pause_height ) / 2;

    clgi.R_DrawPic( x, y, scr.pause_pic );
}

static void SCR_DrawCrosshair( void ) {
    int x, y;

    if ( !scr_crosshair->integer )
        return;

    x = ( scr.hud_width - scr.crosshair_width ) / 2;
    y = ( scr.hud_height - scr.crosshair_height ) / 2;

    clgi.R_SetColor( scr.crosshair_color.u32 );

    clgi.R_DrawStretchPic( x + ch_x->integer,
        y + ch_y->integer,
        scr.crosshair_width,
        scr.crosshair_height,
        scr.crosshair_pic );
}

// The status bar is a small layout program that is based on the stats array
static void SCR_DrawStats( void ) {
    if ( scr_draw2d->integer <= 1 )
        return;

    SCR_ExecuteLayoutString( clgi.client->configstrings[ CS_STATUSBAR ] );
}

static void SCR_DrawLayout( void ) {
    if ( scr_draw2d->integer == 3 && !clgi.Key_IsDown( K_F1 ) ) {
        return;     // turn off for GTV
    }

    if ( clgi.IsDemoPlayback() && clgi.Key_IsDown( K_F1 ) ) {
        goto draw;
    }

    if ( !( clgi.client->frame.ps.stats[ STAT_LAYOUTS ] & 1 ) )
        return;

draw:
    SCR_ExecuteLayoutString( clgi.client->layout );
}

static void SCR_Draw2D( void ) {
    // Turn off for screenshots.
    if ( scr_draw2d->integer <= 0 ) {
        return;
    }

    // Don't draw if we're in a menu.
    if ( clgi.GetKeyEventDestination() & KEY_MENU ) {
        return;
    }

    // Set general alpha scale.
    clgi.R_SetAlphaScale( scr.hud_alpha );

    // Set HUD scale.
    clgi.R_SetScale( scr.hud_scale );

    // Determine screen width and height based on hud_scale.
    scr.hud_height = Q_rint( scr.hud_height * scr.hud_scale );
    scr.hud_width = Q_rint( scr.hud_width * scr.hud_scale );

    // Crosshair has its own color and alpha.
    SCR_DrawCrosshair();

    // The rest of 2D elements share common alpha.
    clgi.R_ClearColor();
    clgi.R_SetAlpha( clgi.CVar_ClampValue( scr_alpha, 0, 1 ) );

    SCR_DrawStats();

    SCR_DrawLayout();

    SCR_DrawInventory();

    SCR_DrawCenterString();

    //SCR_DrawNet();

    SCR_DrawObjects();

    SCR_DrawFPS();

    SCR_DrawChatHUD();

    SCR_DrawTurtle();

    SCR_DrawPause();

    // debug stats have no alpha
    clgi.R_ClearColor();

    #if USE_DEBUG
    SCR_DrawDebugStats();
    SCR_DrawDebugPmove();
    #endif

    clgi.R_SetScale( 1.0f );
    clgi.R_SetAlphaScale( 1.0f );
}


/**
*
*
*	Prog Funcs Entry Points:
*
*
**/
/**
*	@brief	Prepare and draw the current 'active' state's 2D and 3D views.
**/
void PF_DrawActiveState( refcfg_t *refcfg ) {
    // Otherwise, start with full screen HUD.
    scr.hud_height = refcfg->height;
    scr.hud_width = refcfg->width;

    // Draw any demo specific data.
    SCR_DrawDemo();

    // Calculate actual 'view' rectangle.
    SCR_CalcVrect();

    // Clear any dirty part of the background.
    SCR_TileClear();

    // Draw 3D game view.
    clgi.V_RenderView();

    // Draw all 2D elements.
    SCR_Draw2D();
}

/**
*	@brief	Prepare and draw the loading screen's 2D state.
**/
void PF_DrawLoadState( refcfg_t *refcfg ) {
    int x, y;

    //if ( !scr.draw_loading ) {
    //    return;
    //}

    //scr.draw_loading = false;

    clgi.R_SetScale( scr.hud_scale );

    x = ( refcfg->width * scr.hud_scale - scr.loading_width ) / 2;
    y = ( refcfg->height * scr.hud_scale - scr.loading_height ) / 2;

    clgi.R_DrawPic( x, y, scr.loading_pic );

    clgi.R_SetScale( 1.0f );
}

/**
*   @return The current active handle to the font image. (Used by ref/vkpt.)
**/
const qhandle_t PF_GetScreenFontHandle( void ) {
    return scr.font_pic;
}

/**
*   @brief  Set the alpha value of the HUD. (Used by ref/vkpt.)
**/
void PF_SetScreenHUDAlpha( const float alpha ) {
    scr.hud_alpha = alpha;
}