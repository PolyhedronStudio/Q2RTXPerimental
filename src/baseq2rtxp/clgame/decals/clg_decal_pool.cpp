/********************************************************************
*
*
*    ClientGame: Decal Pool Runtime.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/decals/clg_decal_pool.h"

/**
*    @brief  Finds the oldest active pool entry for deterministic eviction.
*    @param  pool Pool state to inspect.
*    @return Oldest active entry or nullptr when no active entries exist.
**/
static clg_decal_instance_t *CLG_DecalPool_FindOldestActive( clg_decal_pool_t *pool ) {
    if ( !pool || !pool->instances || pool->activeCount <= 0 ) {
        return nullptr;
    }

    clg_decal_instance_t *oldest = nullptr;
    for ( int32_t i = 0; i < pool->capacity; i++ ) {
        clg_decal_instance_t *instance = &pool->instances[ i ];
        if ( instance->runtime.active == qfalse ) {
            continue;
        }

        if ( oldest == nullptr || instance->runtime.spawnTime < oldest->runtime.spawnTime ) {
            oldest = instance;
        }
    }

    return oldest;
}

const bool CLG_DecalPool_Init( clg_decal_pool_t *pool, const int32_t capacity ) {
    if ( !pool || capacity <= 0 ) {
        return false;
    }

    clg_decal_instance_t *instances = (clg_decal_instance_t *)clgi.TagMalloc( sizeof( clg_decal_instance_t ) * capacity, TAG_CLGAME );
    if ( !instances ) {
        return false;
    }

    memset( instances, 0, sizeof( clg_decal_instance_t ) * capacity );

    pool->instances = instances;
    pool->capacity = capacity;
    pool->activeCount = 0;
    pool->nextId = 1u;
    pool->generation = 1u;
    return true;
}

void CLG_DecalPool_Shutdown( clg_decal_pool_t *pool ) {
    if ( !pool ) {
        return;
    }

    if ( pool->instances ) {
        clgi.TagFree( pool->instances );
    }

    pool->instances = nullptr;
    pool->capacity = 0;
    pool->activeCount = 0;
    pool->nextId = 1u;
    pool->generation = 1u;
}

void CLG_DecalPool_Clear( clg_decal_pool_t *pool ) {
    if ( !pool || !pool->instances || pool->capacity <= 0 ) {
        return;
    }

    memset( pool->instances, 0, sizeof( clg_decal_instance_t ) * pool->capacity );
    pool->activeCount = 0;
    pool->generation++;
}

clg_decal_instance_t *CLG_DecalPool_Alloc( clg_decal_pool_t *pool ) {
    if ( !pool || !pool->instances || pool->capacity <= 0 ) {
        return nullptr;
    }

    for ( int32_t i = 0; i < pool->capacity; i++ ) {
        clg_decal_instance_t *instance = &pool->instances[ i ];
        if ( instance->runtime.active != qfalse ) {
            continue;
        }

        memset( instance, 0, sizeof( *instance ) );
        pool->activeCount++;
        return instance;
    }

    clg_decal_instance_t *oldest = CLG_DecalPool_FindOldestActive( pool );
    if ( !oldest ) {
        return nullptr;
    }

    memset( oldest, 0, sizeof( *oldest ) );
    return oldest;
}

const bool CLG_DecalPool_FreeById( clg_decal_pool_t *pool, const uint32_t decalId ) {
    if ( !pool || !pool->instances || decalId == 0u ) {
        return false;
    }

    for ( int32_t i = 0; i < pool->capacity; i++ ) {
        clg_decal_instance_t *instance = &pool->instances[ i ];
        if ( instance->runtime.active == qfalse ) {
            continue;
        }

        if ( instance->runtime.decalId != decalId ) {
            continue;
        }

        memset( instance, 0, sizeof( *instance ) );
        pool->activeCount = ( pool->activeCount > 0 ) ? ( pool->activeCount - 1 ) : 0;
        return true;
    }

    return false;
}

int32_t CLG_DecalPool_GetActiveCount( const clg_decal_pool_t *pool ) {
    if ( !pool ) {
        return 0;
    }

    return pool->activeCount;
}
