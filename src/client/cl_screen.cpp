/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client/cl_client.h"
#include "client/client_types.h"
#include "refresh/images.h"

#define STAT_PICS       11
#define STAT_MINUS      (STAT_PICS - 1)  // num frame for '-' stats digit

// WID: C++20:
QEXTERN_C_ENCLOSE( cvar_t *scr_viewsize; );

//static cvar_t   *scr_centertime;
static cvar_t   *scr_showpause;

//static cvar_t   *scr_draw2d;
static cvar_t   *scr_lag_x;
static cvar_t   *scr_lag_y;
static cvar_t   *scr_lag_draw;
static cvar_t   *scr_lag_min;
static cvar_t   *scr_lag_max;

//static cvar_t   *scr_fps;
// 
//! Draw the demo bar on-screen.
static cvar_t   *scr_demobar;

//! Font used for rendering text.
static cvar_t   *scr_font;
//! Dominant Main Scale used for 2D rendering.
static cvar_t   *scr_scale;
//! Dominant Main Alpha used for 2D rendering.
static cvar_t   *scr_alpha;

//! Position of render window on screen.
vrect_t     scr_vrect;

const uint32_t colorTable[8] = {
    U32_BLACK, U32_RED, U32_GREEN, U32_YELLOW,
    U32_BLUE, U32_CYAN, U32_MAGENTA, U32_WHITE
};

// Actual screen shared data structure.
cl_screen_shared_t cl_scr = {
    .hud_scale = 1.0f,
    .hud_alpha = 1.0f,
};


/*
===============================================================================

UTILS

===============================================================================
*/

#define SCR_DrawString(x, y, flags, string) \
    SCR_DrawStringEx(x, y, flags, MAX_STRING_CHARS, string, cl_scr.font_pic)

/*
==============
SCR_DrawStringEx
==============
*/
int SCR_DrawStringEx(int x, int y, int flags, size_t maxlen,
                     const char *s, qhandle_t font)
{
    size_t len = strlen(s);

    if (len > maxlen) {
        len = maxlen;
    }

    if ((flags & UI_CENTER) == UI_CENTER) {
        x -= len * CHAR_WIDTH / 2;
    } else if (flags & UI_RIGHT) {
        x -= len * CHAR_WIDTH;
    }

    return R_DrawString(x, y, flags, maxlen, s, font);
}


/*
==============
SCR_DrawStringMulti
==============
*/
void SCR_DrawStringMulti(int x, int y, int flags, size_t maxlen,
                         const char *s, qhandle_t font)
{
    const char    *p; // WID: C++20: Had no const.
    size_t  len;

    while (*s) {
        p = strchr(s, '\n');
        if (!p) {
            SCR_DrawStringEx(x, y, flags, maxlen, s, font);
            break;
        }

        len = p - s;
        if (len > maxlen) {
            len = maxlen;
        }
        SCR_DrawStringEx(x, y, flags, len, s, font);

        y += CHAR_HEIGHT;
        s = p + 1;
    }
}

/**
*   @brief Fades alpha in and out, keeping the alpha visible for 'visTime' amount.
*   @return 'Alpha' value of the current moment in time. from(startTime) to( startTime + visTime ).
**/
const float SCR_FadeAlpha( const uint64_t startTime, const uint64_t visTime, const uint64_t fadeTime ) {
    // If time delta surpassed visTime, return 0 alpha.
    uint64_t timeDelta = cls.realtime - startTime;
    if (timeDelta >= visTime) {
        return 0;
    }

    // Cap fade time.
    float cappedFadeTime = ( fadeTime > visTime ? visTime : fadeTime );

    //  Determine alpha based on how much time is left for the fade.
    float alpha = 1.f;
    uint64_t timeLeft = visTime - timeDelta;
    if (timeLeft < fadeTime) {
        alpha = (float)timeLeft / fadeTime;
    }

    return alpha;
}

const qboolean SCR_ParseColor(const char *s, color_t *color)
{
    int i;
    int c[8];

    // parse generic color
    if (*s == '#') {
        s++;
        for (i = 0; s[i]; i++) {
            if (i == 8) {
                return false;
            }
            c[i] = Q_charhex(s[i]);
            if (c[i] == -1) {
                return false;
            }
        }

        switch (i) {
        case 3:
            color->u8[0] = c[0] | (c[0] << 4);
            color->u8[1] = c[1] | (c[1] << 4);
            color->u8[2] = c[2] | (c[2] << 4);
            color->u8[3] = 255;
            break;
        case 6:
            color->u8[0] = c[1] | (c[0] << 4);
            color->u8[1] = c[3] | (c[2] << 4);
            color->u8[2] = c[5] | (c[4] << 4);
            color->u8[3] = 255;
            break;
        case 8:
            color->u8[0] = c[1] | (c[0] << 4);
            color->u8[1] = c[3] | (c[2] << 4);
            color->u8[2] = c[5] | (c[4] << 4);
            color->u8[3] = c[7] | (c[6] << 4);
            break;
        default:
            return false;
        }

        return true;
    }

    // parse name or index
    i = Com_ParseColor(s);
    if (i >= q_countof(colorTable)) {
        return false;
    }

    color->u32 = colorTable[i];
    return true;
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_WIDTH   48LL
#define LAG_HEIGHT  48LL

#define LAG_CRIT_BIT    (1U << 31)
#define LAG_WARN_BIT    (1U << 30)

#define LAG_BASE    0xD5
#define LAG_WARN    0xDC
#define LAG_CRIT    0xF2

static struct {
    uint64_t samples[LAG_WIDTH];
    uint64_t head;
} lag;

void SCR_LagClear(void)
{
    lag.head = 0;
}

void SCR_LagSample(void)
{
    int64_t i = cls.netchan.incoming_acknowledged & CMD_MASK;
    client_usercmd_history_t *h = &cl.history[i];
    uint64_t ping;

    h->timeReceived = cls.realtime;
    if (!h->commandNumber || h->timeReceived < h->timeSent) {
        return;
    }

    ping = h->timeReceived - h->timeSent;
    for (i = 0; i < cls.netchan.dropped; i++) {
        lag.samples[lag.head % LAG_WIDTH] = ping | LAG_CRIT_BIT;
        lag.head++;
    }

    if (cl.frameflags & FF_SUPPRESSED) {
        ping |= LAG_WARN_BIT;
    }
    lag.samples[lag.head % LAG_WIDTH] = ping;
    lag.head++;
}

static void SCR_LagDraw(int x, int y)
{
    int64_t i, j, v, c, v_min, v_max, v_range;

    v_min = Cvar_ClampInteger(scr_lag_min, 0, LAG_HEIGHT * 10);
    v_max = Cvar_ClampInteger(scr_lag_max, 0, LAG_HEIGHT * 10);

    v_range = v_max - v_min;
    if (v_range < 1)
        return;

    for (i = 0; i < LAG_WIDTH; i++) {
        j = lag.head - i - 1;
        if (j < 0) {
            break;
        }

        v = lag.samples[j % LAG_WIDTH];

        if (v & LAG_CRIT_BIT) {
            c = LAG_CRIT;
        } else if (v & LAG_WARN_BIT) {
            c = LAG_WARN;
        } else {
            c = LAG_BASE;
        }

        v &= ~(LAG_WARN_BIT | LAG_CRIT_BIT);
        v = (v - v_min) * LAG_HEIGHT / v_range;
        v = std::clamp(v, 0LL, LAG_HEIGHT);

        R_DrawFill8(x + LAG_WIDTH - i - 1, y + LAG_HEIGHT - v, 1, v, c);
    }
}

static void SCR_DrawNet(void)
{
    int x = scr_lag_x->integer;
    int y = scr_lag_y->integer;

    if (x < 0) {
        x += cl_scr.screenWidth - LAG_WIDTH + 1;
    }
    if (y < 0) {
        y += cl_scr.screenHeight - LAG_HEIGHT + 1;
    }

    // draw ping graph
    if (scr_lag_draw->integer) {
        if (scr_lag_draw->integer > 1) {
            R_DrawFill8(x, y, LAG_WIDTH, LAG_HEIGHT, 4);
        }
        SCR_LagDraw(x, y);
    }

    // draw phone jack
    if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged >= CMD_BACKUP) {
        if ((cls.realtime >> 8) & 3) {
            R_DrawStretchPic(x, y, LAG_WIDTH, LAG_HEIGHT, cl_scr.net_pic);
        }
    }
}

/**
*   @return A lowercase string matching the textual name of the color for colorIndex. 
**/
const char *SCR_GetColorName( const color_index_t colorIndex ) {
    if ( colorIndex >= 0 && colorIndex < COLOR_COUNT ) {
        return colorNames[ colorIndex ];
    }

    return nullptr;
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed. If empty sky name is provided, falls
back to server defaults.
=================
*/
static void SCR_Sky_f(void)
{
    char    *name;
    float   rotate;
    vec3_t  axis;
    int     argc = Cmd_Argc();

    if (argc < 2) {
        Com_Printf("Usage: sky <basename> [rotate] [axis x y z]\n");
        return;
    }

    if (cls.state != ca_active) {
        Com_Printf("No map loaded.\n");
        return;
    }

    name = Cmd_Argv(1);
    if (!*name) {
        CL_SetSky();
        return;
    }

    if (argc > 2)
        rotate = atof(Cmd_Argv(2));
    else
        rotate = 0;

    if (argc == 6) {
        axis[0] = atof(Cmd_Argv(3));
        axis[1] = atof(Cmd_Argv(4));
        axis[2] = atof(Cmd_Argv(5));
    } else
        VectorSet(axis, 0, 0, 1);

    R_SetSky(name, rotate, 1, axis);
}

/**
*   @brief  Time how fast we can render? 
**/
static void SCR_TimeRefresh_f(void)
{
    int     i;
    uint64_t start, stop;
    float       time;

    if (cls.state != ca_active) {
        Com_Printf("No map loaded.\n");
        return;
    }

    start = Sys_Milliseconds();

    if (Cmd_Argc() == 2) {
        // run without page flipping
        R_BeginFrame();
        for (i = 0; i < 128; i++) {
            cl.refdef.viewangles[1] = i / 128.0f * 360.0f;
            R_RenderFrame(&cl.refdef);
        }
        R_EndFrame();
    } else {
        for (i = 0; i < 128; i++) {
            cl.refdef.viewangles[1] = i / 128.0f * 360.0f;

            R_BeginFrame();
            R_RenderFrame(&cl.refdef);
            R_EndFrame();
        }
    }

    stop = Sys_Milliseconds();
    time = (stop - start) * 0.001f;
    Com_Printf("%f seconds (%f fps)\n", time, 128.0f / time);
}


//============================================================================

void SCR_DeltaFrame(void) {
    clge->ScreenDeltaFrame();
}

void SCR_ModeChanged(void)
{
    IN_Activate();
    Con_CheckResize();
    UI_ModeChanged();
    cls.disable_screen = 0;

    clge->ScreenModeChanged();
    //if (scr.initialized)
    //    scr.hud_scale = R_ClampScale(scr_scale);

    //scr.hud_alpha = 1.f;
}

/*
==================
SCR_RegisterMedia
==================
*/
void SCR_RegisterMedia(void) {
    clge->ScreenRegisterMedia();
}

static const cmdreg_t scr_cmds[] = {
    { "timerefresh", SCR_TimeRefresh_f },
    { "sky", SCR_Sky_f },
    { NULL }
};

/*
==================
SCR_Init
==================
*/
void SCR_Init(void)
{
//    scr_draw2d = Cvar_Get("scr_draw2d", "2", 0);
//    scr_showturtle = Cvar_Get("scr_showturtle", "1", 0);
//    scr_showitemname = Cvar_Get("scr_showitemname", "1", CVAR_ARCHIVE);
//    scr_lag_x = Cvar_Get("scr_lag_x", "-1", 0);
//    scr_lag_y = Cvar_Get("scr_lag_y", "-1", 0);
//    scr_lag_draw = Cvar_Get("scr_lag_draw", "0", 0);
//    scr_lag_min = Cvar_Get("scr_lag_min", "0", 0);
//    scr_lag_max = Cvar_Get("scr_lag_max", "200", 0);


	// <Q2RTXP>: WID: This badly needs to be refactored.
    // 
	// The client won't be notified about these cvar changes.
    
    // Register screen related commands.
    Cmd_Register(scr_cmds);
    // Give the client game a shot at doing the same (Register commands, create/fetch cvars.).
    clge->ScreenInit();

    // <Q2RTXP>: WID: These are created by the client game module, we merely want access to them right here.
    scr_viewsize = Cvar_Get( "viewsize", nullptr, 0 );
    
    scr_scale = Cvar_Get( "scr_scale", nullptr, 0 );
    //scr_scale->changed( scr_scale );
    scr_alpha = Cvar_Get( "scr_alpha", nullptr, 0 );
    //scr_alpha->changed( scr_alpha );

    scr_font = Cvar_Get( "scr_font", nullptr, 0 );
    //scr_font->changed( scr_font );

    scr_showpause = Cvar_Get( "scr_showpause", nullptr, 0 );
    scr_demobar = Cvar_Get( "scr_demobar", nullptr, 0 );

    // We're in initialized screen state.
    cl_scr.initialized = true;
}

void SCR_Shutdown(void)
{
    // We're in uninitialized screen state again.
    cl_scr.initialized = false;
    // Deregister any screen related commands.
    Cmd_Deregister(scr_cmds);
    // Give the client game a shot at doing the same.
    clge->ScreenShutdown();
    
}

//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque(void)
{
    if (!cls.state) {
        return;
    }

    S_StopAllSounds();
    OGG_Stop();

    if (cls.disable_screen) {
        return;
    }

#if USE_DEBUG
    if (developer->integer) {
        return;
    }
#endif

    // if at console or menu, don't bring up the plaque
    if (cls.key_dest & (KEY_CONSOLE | KEY_MENU)) {
        return;
    }

    cl_scr.draw_loading = true;
    SCR_UpdateScreen();

    cls.disable_screen = Sys_Milliseconds();
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque(void)
{
    if (!cls.state) {
        return;
    }
    cls.disable_screen = 0;
    Con_ClearNotify_f();
}

// Clear any parts of the tiled background that were drawn on last frame
static void SCR_TileClear(void)
{
}

/**
*   @brief  Passes on to the CLGame for allowing custom load screens.
**/
static void SCR_DrawLoading(void) {
    // Never pass on the draw load state to the client game module if we're not loading.
    if ( !cl_scr.draw_loading ) {
        return;
    }
    // Set to false again.
    cl_scr.draw_loading = false;
    // Draws a single frame of the loading state.
    clge->DrawLoadState( &r_config );
}

/**
*   Draws the active client's "state" in which it resides.
**/
static void SCR_DrawActive(void)
{
    // If a non-transparant fullscreen menu is up, do nothing at all.
    if ( !UI_IsTransparent() ) {
        return;
    }

    // Draw black background if not active
    if (cls.state < ca_active) {
        R_DrawFill8(0, 0, r_config.width, r_config.height, 0);
        return;
    }

    // If we're in cinematic mode, draw the cinematic and return.
    if (cls.state == ca_cinematic) {
        SCR_DrawCinematic();
        return;
    }

    // Call into client game for drawing active state.
    clge->DrawActiveState( &r_config );
}

//=======================================================

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen(void)
{
    static int recursive;

    if (!cl_scr.initialized) {
        return;             // not initialized yet
    }

    // if the screen is disabled (loading plaque is up), do nothing at all
    if (cls.disable_screen) {
        uint64_t delta = Sys_Milliseconds() - cls.disable_screen;

        // Check for a possible time-out.
        if (delta < 120 * 1000) {
            return;
        }
        cls.disable_screen = 0;
        Com_Printf("Loading plaque timed out.\n");
    }

    if (recursive > 1) {
        Com_Error(ERR_FATAL, "%s: recursively called", __func__);
    }

    recursive++;

    R_BeginFrame();

    // Do refresh drawing of the active state, initialize the view,
    // determine which entities to render, and draw the game's HUD 2D Overlay.
    SCR_DrawActive();
    // Draw main menu screen overlay.
    UI_Draw( cls.realtime );
    // Draw the console overlay.
    Con_DrawConsole();
    // Draw loading plaque overlay.
    SCR_DrawLoading();

    R_EndFrame();

    recursive--;
}

const qhandle_t SCR_GetFont( void )
{
    return cl_scr.font_pic;
}

void SCR_SetHudAlpha( const float alpha )
{
    //scr.hud_alpha = alpha;
    clge->SetScreenHUDAlpha( alpha );
}
