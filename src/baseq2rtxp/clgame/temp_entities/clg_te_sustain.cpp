/********************************************************************
*
*
*	ClientGame: Sustains(decals).
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"

#define MAX_SUSTAINS    32

static clg_sustain_t     clg_sustains[ MAX_SUSTAINS ];

void CLG_ClearSustains( void ) {
    memset( clg_sustains, 0, sizeof( clg_sustains ) );
}

clg_sustain_t *CLG_AllocSustain( void ) {
    clg_sustain_t *s;
    int             i;

    for ( i = 0, s = clg_sustains; i < MAX_SUSTAINS; i++, s++ ) {
        if ( s->id == 0 )
            return s;
    }

    return NULL;
}

void CLG_ProcessSustain( void ) {
    clg_sustain_t *s;
    int             i;

    for ( i = 0, s = clg_sustains; i < MAX_SUSTAINS; i++, s++ ) {
        if ( s->id ) {
            if ( ( s->endtime >= clgi.client->time ) && ( clgi.client->time >= s->nextthink ) )
                s->think( s );
            else if ( s->endtime < clgi.client->time )
                s->id = 0;
        }
    }
}

void CLG_ParseSteam( void ) {
    clg_sustain_t *s;

    if ( level.parsedMessage.events.tempEntity.entity1 == -1 ) {
        CLG_FX_ParticleSteamEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, level.parsedMessage.events.tempEntity.color & 0xff, level.parsedMessage.events.tempEntity.count, level.parsedMessage.events.tempEntity.entity2 );
        return;
    }

    s = CLG_AllocSustain();
    if ( !s )
        return;

    s->id = level.parsedMessage.events.tempEntity.entity1;
    s->count = level.parsedMessage.events.tempEntity.count;
    VectorCopy( level.parsedMessage.events.tempEntity.pos1, s->org );
    VectorCopy( level.parsedMessage.events.tempEntity.dir, s->dir );
    s->color = level.parsedMessage.events.tempEntity.color & 0xff;
    s->magnitude = level.parsedMessage.events.tempEntity.entity2;
    s->endtime = clgi.client->time + level.parsedMessage.events.tempEntity.time;
    s->think = CLG_FX_ParticleSteamEffect2;
    s->nextthink = clgi.client->time;
}

void CLG_ParseWidow( void ) {
    clg_sustain_t *s;

    s = CLG_AllocSustain();
    if ( !s )
        return;

    s->id = level.parsedMessage.events.tempEntity.entity1;
    VectorCopy( level.parsedMessage.events.tempEntity.pos1, s->org );
    s->endtime = clgi.client->time + 2100;
    s->think = CLG_FX_Widowbeamout;
    s->nextthink = clgi.client->time;
}

void CLG_ParseNuke( void ) {
    clg_sustain_t *s;

    s = CLG_AllocSustain();
    if ( !s )
        return;

    s->id = 21000;
    VectorCopy( level.parsedMessage.events.tempEntity.pos1, s->org );
    s->endtime = clgi.client->time + 1000;
    s->think = CLG_FX_Nukeblast;
    s->nextthink = clgi.client->time;
}