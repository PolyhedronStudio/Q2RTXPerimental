/**
*
*
*   Color Indexing Table, used to be in Common/ but we need it in the client game too.
*
*
**/
#pragma once 

typedef enum {
    COLOR_BLACK,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_WHITE,

    COLOR_ALT,
    COLOR_NONE,

    COLOR_COUNT
} color_index_t;

// <Q2RTXP>: WID: Lame but required though to keep things working as they were. :-)
#ifdef __cplusplus
static inline color_index_t &operator++( color_index_t &color ) {
    switch ( color ) {
    case COLOR_BLACK: color = COLOR_RED; break;
    case COLOR_RED: color = COLOR_GREEN; break;
    case COLOR_GREEN: color = COLOR_YELLOW; break;
    case COLOR_YELLOW: color = COLOR_BLUE; break;
    case COLOR_BLUE: color = COLOR_CYAN; break;
    case COLOR_CYAN: color = COLOR_MAGENTA; break;
    case COLOR_MAGENTA: color = COLOR_WHITE; break;
    case COLOR_WHITE: color = COLOR_ALT; break;
    case COLOR_ALT: color = COLOR_NONE; break;
    case COLOR_NONE: color = COLOR_BLACK; break;
    default:
        break;
    }

    return color;
}
#endif // #ifdef __cplusplus

#define U32_BLACK   MakeColor(  0,   0,   0, 255)
#define U32_RED     MakeColor(208,  70,  72, 255)
#define U32_GREEN   MakeColor(109, 170,  44, 255)
#define U32_YELLOW  MakeColor(218, 212,  94, 255)
#define U32_BLUE    MakeColor( 89, 125, 206, 255)
#define U32_CYAN    MakeColor(109, 194, 202, 255)
#define U32_MAGENTA MakeColor(210, 170, 153, 255)
#define U32_WHITE   MakeColor(255, 255, 255, 255)