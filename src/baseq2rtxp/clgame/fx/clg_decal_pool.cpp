/********************************************************************
*
*
*    ClientGame: Decal Pool Runtime.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/decals/clg_decal_pool.h"

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
    pool->headIndex = 0;
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
    pool->headIndex = 0;
    pool->nextId = 1u;
    pool->generation = 1u;
}

void CLG_DecalPool_Clear( clg_decal_pool_t *pool ) {
    if ( !pool || !pool->instances || pool->capacity <= 0 ) {
        return;
    }

    memset( pool->instances, 0, sizeof( clg_decal_instance_t ) * pool->capacity );
    pool->activeCount = 0;
    pool->headIndex = 0;
    pool->generation++;
}

clg_decal_instance_t *CLG_DecalPool_Alloc( clg_decal_pool_t *pool ) {
    if ( !pool || !pool->instances || pool->capacity <= 0 ) {
        return nullptr;
    }

    // O(1) Circular FIFO allocation: grab next slot at headIndex
    clg_decal_instance_t *instance = &pool->instances[ pool->headIndex ];
    pool->headIndex = ( pool->headIndex + 1 ) % pool->capacity;

    if ( instance->runtime.active == qfalse ) {
        pool->activeCount++;
    }

    // Reset entry in O(1) without iterating
    memset( instance, 0, sizeof( *instance ) );
    return instance;
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
