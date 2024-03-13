/********************************************************************
*
*
*	ClientGame: Beams.
*
*
********************************************************************/
#include "../clg_local.h"
#include "../clg_effects.h"


#define MAX_BEAMS   32

typedef struct {
    int         entity;
    int         dest_entity;
    qhandle_t   model;
    int64_t     endtime;
    vec3_t      offset;
    vec3_t      start, end;
} beam_t;

static beam_t   cl_beams[ MAX_BEAMS ];
static beam_t   cl_playerbeams[ MAX_BEAMS ];

void CLG_ClearBeams( void ) {
    memset( cl_beams, 0, sizeof( cl_beams ) );
    memset( cl_playerbeams, 0, sizeof( cl_playerbeams ) );
}

void CLG_ParseBeam( const qhandle_t model ) {
    beam_t *b;
    int     i;

    // override any beam with the same source AND destination entities
    for ( i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++ )
        if ( b->entity == level.parsedMessage.events.tempEntity.entity1 && b->dest_entity == level.parsedMessage.events.tempEntity.entity2 )
            goto override;

    // find a free beam
    for ( i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++ ) {
        if ( !b->model || b->endtime < clgi.client->time ) {
            override :
                b->entity = level.parsedMessage.events.tempEntity.entity1;
            b->dest_entity = level.parsedMessage.events.tempEntity.entity2;
            b->model = model;
            b->endtime = clgi.client->time + 200;
            VectorCopy( level.parsedMessage.events.tempEntity.pos1, b->start );
            VectorCopy( level.parsedMessage.events.tempEntity.pos2, b->end );
            VectorCopy( level.parsedMessage.events.tempEntity.offset, b->offset );
            return;
        }
    }
}

void CLG_ParsePlayerBeam( const qhandle_t model ) {
    beam_t *b;
    int     i;

    // override any beam with the same entity
    for ( i = 0, b = cl_playerbeams; i < MAX_BEAMS; i++, b++ ) {
        if ( b->entity == level.parsedMessage.events.tempEntity.entity1 ) {
            b->entity = level.parsedMessage.events.tempEntity.entity1;
            b->model = model;
            b->endtime = clgi.client->time + 200;
            VectorCopy( level.parsedMessage.events.tempEntity.pos1, b->start );
            VectorCopy( level.parsedMessage.events.tempEntity.pos2, b->end );
            VectorCopy( level.parsedMessage.events.tempEntity.offset, b->offset );
            return;
        }
    }

    // find a free beam
    for ( i = 0, b = cl_playerbeams; i < MAX_BEAMS; i++, b++ ) {
        if ( !b->model || b->endtime < clgi.client->time ) {
            b->entity = level.parsedMessage.events.tempEntity.entity1;
            b->model = model;
            b->endtime = clgi.client->time + 100;     // PMM - this needs to be 100 to prevent multiple heatbeams
            VectorCopy( level.parsedMessage.events.tempEntity.pos1, b->start );
            VectorCopy( level.parsedMessage.events.tempEntity.pos2, b->end );
            VectorCopy( level.parsedMessage.events.tempEntity.offset, b->offset );
            return;
        }
    }

}

/*
=================
CL_AddBeams
=================
*/
void CLG_AddBeams( void ) {
    int         i, j;
    beam_t *b;
    vec3_t      dist, org;
    float       d;
    entity_t    ent;
    vec3_t      angles;
    float       len, steps;
    float       model_length;

    // update beams
    for ( i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++ ) {
        if ( !b->model || b->endtime < clgi.client->time )
            continue;

        // if coming from the player, update the start position
        if ( b->entity == clgi.client->frame.clientNum + 1 )
            VectorAdd( clgi.client->playerEntityOrigin, b->offset, org );
        else
            VectorAdd( b->start, b->offset, org );

        // calculate pitch and yaw
        VectorSubtract( b->end, org, dist );
        QM_Vector3ToAngles( dist, angles );

        // add new entities for the beams
        d = VectorNormalize( dist );
        if ( b->model == precache.cl_mod_lightning ) {
            model_length = 35.0f;
            d -= 20.0f; // correction so it doesn't end in middle of tesla
        } else {
            model_length = 30.0f;
        }
        steps = ceil( d / model_length );
        len = ( d - model_length ) / ( steps - 1 );

        memset( &ent, 0, sizeof( ent ) );
        ent.model = b->model;

        // PMM - special case for lightning model .. if the real length is shorter than the model,
        // flip it around & draw it from the end to the start.  This prevents the model from going
        // through the tesla mine (instead it goes through the target)
        if ( ( b->model == precache.cl_mod_lightning ) && ( d <= model_length ) ) {
            VectorCopy( b->end, ent.origin );
            ent.flags = RF_FULLBRIGHT;
            ent.angles[ 0 ] = angles[ 0 ];
            ent.angles[ 1 ] = angles[ 1 ];
            ent.angles[ 2 ] = irandom( 360 );
            clgi.V_AddEntity( &ent );
            return;
        }

        while ( d > 0 ) {
            VectorCopy( org, ent.origin );
            if ( b->model == precache.cl_mod_lightning ) {
                ent.flags = RF_FULLBRIGHT;
                ent.angles[ 0 ] = -angles[ 0 ];
                ent.angles[ 1 ] = angles[ 1 ] + 180.0f;
                ent.angles[ 2 ] = irandom( 360 );
            } else {
                ent.angles[ 0 ] = angles[ 0 ];
                ent.angles[ 1 ] = angles[ 1 ];
                ent.angles[ 2 ] = irandom( 360 );
            }

            clgi.V_AddEntity( &ent );

            for ( j = 0; j < 3; j++ )
                org[ j ] += dist[ j ] * len;
            d -= model_length;
        }
    }
}

/*
=================
CL_AddPlayerBeams

Draw player locked beams. Currently only used by the plasma beam.
=================
*/
void CLG_AddPlayerBeams( void ) {
    int         i, j;
    beam_t *b;
    vec3_t      dist, org;
    float       d;
    entity_t    ent;
    vec3_t      angles;
    float       len, steps;
    int         framenum;
    float       model_length;
    float       hand_multiplier;
    player_state_t *ps, *ops;

    if ( info_hand->integer == 2 )
        hand_multiplier = 0;
    else if ( info_hand->integer == 1 )
        hand_multiplier = -1;
    else
        hand_multiplier = 1;

    // update beams
    for ( i = 0, b = cl_playerbeams; i < MAX_BEAMS; i++, b++ ) {
        if ( !b->model || b->endtime < clgi.client->time )
            continue;

        // if coming from the player, update the start position
        if ( b->entity == clgi.client->frame.clientNum + 1 ) {
            // set up gun position
            ps = &clgi.client->frame.ps;
            ops = &clgi.client->oldframe.ps;

            for ( j = 0; j < 3; j++ )
                b->start[ j ] = clgi.client->refdef.vieworg[ j ] + ops->gunoffset[ j ] +
                clgi.client->lerpfrac * ( ps->gunoffset[ j ] - ops->gunoffset[ j ] );

            VectorMA( b->start, ( hand_multiplier * b->offset[ 0 ] ), clgi.client->v_right, org );
            VectorMA( org, b->offset[ 1 ], clgi.client->v_forward, org );
            VectorMA( org, b->offset[ 2 ], clgi.client->v_up, org );
            if ( info_hand->integer == 2 )
                VectorMA( org, -1, clgi.client->v_up, org );

            // calculate pitch and yaw
            VectorSubtract( b->end, org, dist );

            // FIXME: don't add offset twice?
            d = VectorLength( dist );
            VectorScale( clgi.client->v_forward, d, dist );
            VectorMA( dist, ( hand_multiplier * b->offset[ 0 ] ), clgi.client->v_right, dist );
            VectorMA( dist, b->offset[ 1 ], clgi.client->v_forward, dist );
            VectorMA( dist, b->offset[ 2 ], clgi.client->v_up, dist );
            if ( info_hand->integer == 2 )
                VectorMA( org, -1, clgi.client->v_up, org );

            // FIXME: use cl.refdef.viewangles?
            QM_Vector3ToAngles( dist, angles );

            // if it's the heatbeam, draw the particle effect
            CLG_Heatbeam( org, dist );

            framenum = 1;
        } else {
            VectorCopy( b->start, org );

            // calculate pitch and yaw
            VectorSubtract( b->end, org, dist );
            QM_Vector3ToAngles( dist, angles );

            // if it's a non-origin offset, it's a player, so use the hardcoded player offset
            if ( !VectorEmpty( b->offset ) ) {
                vec3_t  tmp, f, r, u;

                tmp[ 0 ] = angles[ 0 ];
                tmp[ 1 ] = angles[ 1 ] + 180.0f;
                tmp[ 2 ] = 0;
                AngleVectors( tmp, f, r, u );

                VectorMA( org, -b->offset[ 0 ] + 1, r, org );
                VectorMA( org, -b->offset[ 1 ], f, org );
                VectorMA( org, -b->offset[ 2 ] - 10, u, org );
            } else {
                // if it's a monster, do the particle effect
                CLG_MonsterPlasma_Shell( b->start );
            }

            framenum = 2;
        }

        // add new entities for the beams
        d = VectorNormalize( dist );
        model_length = 32.0f;
        steps = ceil( d / model_length );
        len = ( d - model_length ) / ( steps - 1 );

        memset( &ent, 0, sizeof( ent ) );
        ent.model = b->model;
        ent.frame = framenum;
        ent.flags = RF_FULLBRIGHT;
        ent.angles[ 0 ] = -angles[ 0 ];
        ent.angles[ 1 ] = angles[ 1 ] + 180.0f;
        ent.angles[ 2 ] = clgi.client->time % 360;

        while ( d > 0 ) {
            VectorCopy( org, ent.origin );

            clgi.V_AddEntity( &ent );

            for ( j = 0; j < 3; j++ )
                org[ j ] += dist[ j ] * len;
            d -= model_length;
        }
    }
}