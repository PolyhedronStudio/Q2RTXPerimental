/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"


/**
*   @brief  (Re-)initialize an edict.
**/
void SVG_InitEdict( edict_t *e ) {
    e->inuse = true;
    e->classname = "noclass";
    e->gravity = 1.0f;
    e->s.number = e - g_edicts;

    // A generic entity type by default.
    e->s.entityType = ET_GENERIC;
}

/**
*   @brief  Either finds a free edict, or allocates a new one.
*   @remark This function tries to avoid reusing an entity that was recently freed, 
*           because it can cause the client to think the entity morphed into something 
*           else instead of being removed and recreated, which can cause interpolated
*           angles and bad trails.
**/
edict_t *SVG_AllocateEdict( void ) {
    int32_t i = game.maxclients + 1;
    edict_t *entity = &g_edicts[ game.maxclients + 1 ];
    edict_t *freedEntity = nullptr;

    for ( i; i < globals.num_edicts; i++, entity++ ) {
        // the first couple seconds of server time can involve a lot of
        // freeing and allocating, so relax the replacement policy
        if ( !entity->inuse && ( entity->freetime < 2_sec || level.time - entity->freetime > 500_ms ) ) {
            SVG_InitEdict( entity );
            return entity;
        }

        // this is going to be our second chance to spawn an entity in case all free
        // entities have been freed only recently
        if ( !freedEntity ) {
            freedEntity = entity;
        }
    }

    if ( i == game.maxentities ) {
        if ( freedEntity ) {
            SVG_InitEdict( freedEntity );
            return freedEntity;
        }
        gi.error( "SVG_AllocateEdict: no free edicts" );
    }

    globals.num_edicts++;
    SVG_InitEdict( entity );
    return entity;
}

/**
*   @brief  Marks the edict as free
**/
void SVG_FreeEdict( edict_t *ed ) {
    gi.unlinkentity( ed );        // unlink from world

    if ( ( ed - g_edicts ) <= ( maxclients->value + BODY_QUEUE_SIZE ) ) {
        #ifdef _DEBUG
        gi.dprintf( "tried to free special edict(#%d) within special edict range(%d)\n", ed - g_edicts, maxclients->value + BODY_QUEUE_SIZE );
        #endif
        return;
    }

    int32_t id = ed->spawn_count + 1;
    //memset( ed, 0, sizeof( *ed ) );
    
    // We actually got to make sure that we free the pushmover curve positions data block.
    if ( ed->pushMoveInfo.curve.positions ) {
        gi.TagFree( ed->pushMoveInfo.curve.positions );
    }

    // Clear the arguments std::vector just to be sure.
    ed->delayed.signalOut.arguments.clear();
    // Ues C++ method of 'memset' I guess, since we got a C++ container up there.
    *ed = {};

    // Recalculate its number.
    ed->s.number = ed - g_edicts;
    ed->classname = "freed";
    ed->freetime = level.time;
    ed->inuse = false;
    ed->spawn_count = id;
}

/**
*   @brief  Searches all active entities for the next one that holds
*           the matching string at fieldofs (use the FOFS_GENTITY() macro) in the structure.
*
*   @remark Searches beginning at the edict after from, or the beginning if NULL
*           NULL will be returned if the end of the list is reached.
**/
edict_t *SVG_Find( edict_t *from, const int32_t fieldofs, const char *match ) {
    char *s;

    // WID: Prevent nastyness when match is empty (Q_stricmp)
    if ( !match ) {
        return nullptr;
    }

    if ( !from ) {
        from = g_edicts;
    } else {
        from++;
    }

    for ( ; from < &g_edicts[ globals.num_edicts ]; from++ ) {
        if ( !from->inuse )
            continue;
        s = *(char **)( (byte *)from + fieldofs );
        if ( !s )
            continue;
        if ( !Q_stricmp( s, match ) )
            return from;
    }

    return NULL;
}

/**
*   @brief  Similar to SVG_Find, but, returns entities that have origins within a spherical area.
**/
edict_t *SVG_FindWithinRadius( edict_t *from, const vec3_t org, const float rad ) {
    vec3_t  eorg;
    int     j;

    if ( !from )
        from = g_edicts;
    else
        from++;
    for ( ; from < &g_edicts[ globals.num_edicts ]; from++ ) {
        if ( !from->inuse )
            continue;
        if ( from->solid == SOLID_NOT )
            continue;
        for ( j = 0; j < 3; j++ )
            eorg[ j ] = org[ j ] - ( from->s.origin[ j ] + ( from->mins[ j ] + from->maxs[ j ] ) * 0.5f );
        if ( VectorLength( eorg ) > rad )
            continue;
        return from;
    }

    return NULL;
}



/**
*
* 
*
*   Dead Body Queue:
*
*
* 
**/
/**
*   @brief
**/
void SVG_InitBodyQue( void ) {
    int     i;
    edict_t *ent;

    level.body_que = 0;
    for ( i = 0; i < BODY_QUEUE_SIZE; i++ ) {
        ent = SVG_AllocateEdict();
        ent->classname = "bodyque";
    }
}
/**
*   @brief
**/
void body_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    int n;

    if ( self->health < -40 ) {
        gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
        for ( n = 0; n < 4; n++ ) {
            SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
        }
        self->s.origin[ 2 ] -= 48;
        SVG_Misc_ThrowClientHead( self, damage );
        self->takedamage = DAMAGE_NO;
    }
}

/**
*   @brief  Get a que slot, leave an effect, and remove body into the queue.
**/
void SVG_CopyToBodyQue( edict_t *ent ) {
    edict_t *body;

    gi.unlinkentity( ent );

    // grab a body que and cycle to the next one
    body = &g_edicts[ game.maxclients + level.body_que + 1 ];
    level.body_que = ( level.body_que + 1 ) % BODY_QUEUE_SIZE;

    // send an effect on the removed body
    if ( body->s.modelindex ) {
        gi.WriteUint8( svc_temp_entity );
        gi.WriteUint8( TE_BLOOD );
        gi.WritePosition( body->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        gi.WriteDir8( vec3_origin );
        gi.multicast( body->s.origin, MULTICAST_PVS, false );
    }

    gi.unlinkentity( body );

    body->s.number = body - g_edicts;
    VectorCopy( ent->s.origin, body->s.origin );
    VectorCopy( ent->s.origin, body->s.old_origin );
    VectorCopy( ent->s.angles, body->s.angles );
    body->s.modelindex = ent->s.modelindex;
    body->s.frame = ent->s.frame;
    body->s.skinnum = ent->s.skinnum;
    body->s.event = EV_OTHER_TELEPORT;

    body->svflags = ent->svflags;
    VectorCopy( ent->mins, body->mins );
    VectorCopy( ent->maxs, body->maxs );
    VectorCopy( ent->absmin, body->absmin );
    VectorCopy( ent->absmax, body->absmax );
    VectorCopy( ent->size, body->size );
    VectorCopy( ent->velocity, body->velocity );
    VectorCopy( ent->avelocity, body->avelocity );
    body->solid = ent->solid;
    body->clipmask = ent->clipmask;
    body->owner = ent->owner;
    body->movetype = ent->movetype;
    body->groundInfo.entity = ent->groundInfo.entity;

    body->s.entityType = ET_PLAYER_CORPSE;

    body->die = body_die;
    body->takedamage = DAMAGE_YES;

    gi.linkentity( body );
}



/**
*
*
*
*   Visibility Testing:
*
*
*
**/
/**
*   @return True if the entity is visible to self, even if not infront ()
**/
const bool SVG_IsEntityVisible( edict_t *self, edict_t *other ) {
    vec3_t  spot1;
    vec3_t  spot2;
    trace_t trace;

    VectorCopy( self->s.origin, spot1 );
    spot1[ 2 ] += self->viewheight;
    VectorCopy( other->s.origin, spot2 );
    spot2[ 2 ] += other->viewheight;
    trace = gi.trace( spot1, vec3_origin, vec3_origin, spot2, self, MASK_OPAQUE );

    if ( trace.fraction == 1.0f )
        return true;
    return false;
}

/**
*   @return True if the entity is in front (in sight) of self
**/
const bool SVG_IsEntityInFrontOf( edict_t *self, edict_t *other ) {
    // If a client, use its forward vector.
    Vector3 forward = {};
    if ( SVG_IsClientEntity( self ) ) {
        self->client->viewMove.viewForward;
    // Calculate forward vector:
    } else {
        QM_AngleVectors( self->s.angles, &forward, nullptr, nullptr );
    }
    // Get direction.
    Vector3 direction = Vector3( other->s.origin ) - Vector3( self->s.origin );
    // Normalize direction.
    Vector3 normalizedDirection = QM_Vector3Normalize( direction );
    // Get dot product from normalized direction / forward.
    const float dot = QM_Vector3DotProduct( normalizedDirection, forward );
    // In 'front' of.
    if ( dot > 0.3f ) {
        return true;
    }
    // Not in 'front' of.
    return false;
}
/**
*   @return True if the testOrigin point is in front of entity 'self'.
**/
const bool SVG_IsEntityInFrontOf( edict_t *self, const Vector3 &testOrigin ) {
    // If a client, use its forward vector.
    Vector3 forward = {};
    if ( SVG_IsClientEntity( self ) ) {
        self->client->viewMove.viewForward;
        // Calculate forward vector:
    } else {
        QM_AngleVectors( self->s.angles, &forward, nullptr, nullptr );
    }
    // Get direction.
    Vector3 direction = testOrigin - self->s.origin;
    // Normalize direction.
    Vector3 normalizedDirection = QM_Vector3Normalize( direction );
    // Get dot product from normalized direction / forward.
    const float dot = QM_Vector3DotProduct( normalizedDirection, forward );
    // In 'front' of.
    if ( dot > 0.3f ) {
        return true;
    }
    // Not in 'front' of.
    return false;
}