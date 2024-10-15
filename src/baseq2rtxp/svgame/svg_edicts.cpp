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
*           the matching string at fieldofs (use the FOFS() macro) in the structure.
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