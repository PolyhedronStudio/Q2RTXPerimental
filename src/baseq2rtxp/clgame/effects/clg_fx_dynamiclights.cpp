/********************************************************************
*
*
*	ClientGame: Dynamic Lights for the client game module.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/effects/clg_fx_dynamiclights.h"

//!
static clg_dlight_t       cl_dlights[ MAX_DLIGHTS ];

/**
*   @brief
**/
void CLG_ClearDynamicLights( void ) {
    memset( cl_dlights, 0, sizeof( cl_dlights ) );
}

/**
*   @brief  
**/
clg_dlight_t *CLG_AllocateDynamicLight( const int32_t key ) {
    int     i;
    clg_dlight_t *dl;

    // first look for an exact key match
    if ( key ) {
        dl = cl_dlights;
        for ( i = 0; i < MAX_DLIGHTS; i++, dl++ ) {
            if ( dl->key == key ) {
                memset( dl, 0, sizeof( *dl ) );
                dl->key = key;
                return dl;
            }
        }
    }

    // then look for anything else
    dl = cl_dlights;
    for ( i = 0; i < MAX_DLIGHTS; i++, dl++ ) {
        if ( dl->die < clgi.client->time ) {
            memset( dl, 0, sizeof( *dl ) );
            dl->key = key;
            return dl;
        }
    }

    dl = &cl_dlights[ 0 ];
    memset( dl, 0, sizeof( *dl ) );
    dl->key = key;
    return dl;
}

/**
*   @brief
**/
void CLG_AddDynamicLights( void ) {
    int         i;
    clg_dlight_t *dl;

    dl = cl_dlights;
    for ( i = 0; i < MAX_DLIGHTS; i++, dl++ ) {
        if ( dl->die < clgi.client->time )
            continue;
        clgi.V_AddLight( &dl->origin.x, dl->radius,
            dl->color[ 0 ], dl->color[ 1 ], dl->color[ 2 ] );
    }
}