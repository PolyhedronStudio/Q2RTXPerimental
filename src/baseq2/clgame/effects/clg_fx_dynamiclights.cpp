/********************************************************************
*
*
*	ClientGame: Dynamic Lights for the client game module.
*
*
********************************************************************/
#include "../clg_local.h"

//!
static cdlight_t       cl_dlights[ MAX_DLIGHTS ];

/**
*   @brief
**/
void CLG_ClearDlights( void ) {
    memset( cl_dlights, 0, sizeof( cl_dlights ) );
}

/**
*   @brief  
**/
cdlight_t *CLG_AllocDlight( const int32_t key ) {
    int     i;
    cdlight_t *dl;

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
void CLG_AddDLights( void ) {
    int         i;
    cdlight_t *dl;

    dl = cl_dlights;
    for ( i = 0; i < MAX_DLIGHTS; i++, dl++ ) {
        if ( dl->die < clgi.client->time )
            continue;
        clgi.V_AddLight( dl->origin, dl->radius,
            dl->color[ 0 ], dl->color[ 1 ], dl->color[ 2 ] );
    }
}