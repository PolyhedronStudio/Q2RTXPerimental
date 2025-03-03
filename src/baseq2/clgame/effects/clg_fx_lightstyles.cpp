/********************************************************************
*
*
*	ClientGame: LightStyles for the client game module.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"

//!
typedef struct {
    int     length;
    float   map[ MAX_QPATH - 1 ];
} clightstyle_t;

//!
static clightstyle_t    clg_lightstyles[ MAX_LIGHTSTYLES ];

/**
*   @brief
**/
void CLG_ClearLightStyles( void ) {
    memset( clg_lightstyles, 0, sizeof( clg_lightstyles ) );
}

/**
*   @brief  
**/
void CLG_SetLightStyle( const int32_t index, const char *s ) {
    int     i;
    clightstyle_t *ls;

    ls = &clg_lightstyles[ index ];
    ls->length = strlen( s );
    if ( ls->length > MAX_QPATH ) {
        Com_Error( ERR_DROP, "%s: oversize style", __func__ );
    }

    for ( i = 0; i < ls->length; i++ )
        ls->map[ i ] = (float)( s[ i ] - 'a' ) / (float)( 'm' - 'a' );
}

/**
*   @brief
**/
void CLG_AddLightStyles( void ) {
    static constexpr int32_t ls_frametime = 100;
    const int32_t ofs = clgi.client->time / ls_frametime;
    clightstyle_t *ls = clg_lightstyles;

    for ( int32_t i = 0; i < MAX_LIGHTSTYLES; i++, ls++ ) {
        // Determine value based on the offset into the 'light style' string. 
        float value = ls->length ? ls->map[ ofs % ls->length ] : 1.0f;

        // Add lightstyle to the view scene.
        clgi.V_AddLightStyle( i, value );
    }
}