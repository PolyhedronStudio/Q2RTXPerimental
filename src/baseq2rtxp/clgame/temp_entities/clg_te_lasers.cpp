/********************************************************************
*
*
*	ClientGame: Lasers.
*
*
********************************************************************/
#include "../clg_local.h"

#define MAX_LASERS  32

static laser_t  clg_lasers[ MAX_LASERS ];

void CLG_ClearLasers( void ) {
    memset( clg_lasers, 0, sizeof( clg_lasers ) );
}

laser_t *CLG_AllocLaser( void ) {
    laser_t *l;
    int i;

    for ( i = 0, l = clg_lasers; i < MAX_LASERS; i++, l++ ) {
        if ( clgi.client->time - l->starttime >= l->lifetime ) {
            memset( l, 0, sizeof( *l ) );
            l->starttime = clgi.client->time;
            return l;
        }
    }

    return NULL;
}

void CLG_AddLasers( void ) {
    laser_t *l;
    entity_t    ent;
    int         i;
    int         time;

    memset( &ent, 0, sizeof( ent ) );

    for ( i = 0, l = clg_lasers; i < MAX_LASERS; i++, l++ ) {
        time = l->lifetime - ( clgi.client->time - l->starttime );
        if ( time < 0 ) {
            continue;
        }

        if ( l->color == -1 ) {
            ent.rgba = l->rgba;
            ent.alpha = (float)time / (float)l->lifetime;
        } else {
            ent.alpha = 0.30f;
        }

        ent.skinnum = l->color;
        ent.flags = RF_TRANSLUCENT | RF_BEAM;
        VectorCopy( l->start, ent.origin );
        VectorCopy( l->end, ent.oldorigin );
        ent.frame = l->width;

        clgi.V_AddEntity( &ent );
    }
}

void CLG_ParseLaser( const int32_t colors ) {
    laser_t *l;

    l = CLG_AllocLaser();
    if ( !l )
        return;

    VectorCopy( level.parsedMessage.events.tempEntity.pos1, l->start );
    VectorCopy( level.parsedMessage.events.tempEntity.pos2, l->end );
    l->lifetime = 100;
    l->color = ( colors >> ( ( irandom( 4 ) ) * 8 ) ) & 0xff;
    l->width = 4;
}