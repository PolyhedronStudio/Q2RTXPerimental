/********************************************************************
*
*
*    ClientGame: Decal Pool Runtime.
*
*
********************************************************************/
#pragma once

#include <cstdint>

#include "sharedgame/sg_decal_shared.h"

/**
*    @brief  One active-or-free decal runtime entry.
**/
typedef struct clg_decal_instance_s {
    sg_decal_spawn_params_t spawnParams;
    sg_decal_runtime_state_t runtime;
    vec3_t tangent;
    vec3_t bitangent;
    vec3_t attachedLocalOrigin;
    vec3_t attachedLocalNormal;
    vec3_t attachedLocalRight;
    int32_t attachedEntityNumber;
    uint32_t randomSeed;
} clg_decal_instance_t;

/**
*    @brief  Fixed-capacity decal pool state.
**/
typedef struct clg_decal_pool_s {
    clg_decal_instance_t *instances;
    int32_t capacity;
    int32_t activeCount;
    uint32_t nextId;
    uint32_t generation;
} clg_decal_pool_t;

/**
*    @brief  Initializes the decal pool.
*    @param  pool Pool state to initialize.
*    @param  capacity Number of decal entries to allocate.
*    @return True when allocation and setup succeeded.
**/
const bool CLG_DecalPool_Init( clg_decal_pool_t *pool, const int32_t capacity );

/**
*    @brief  Shuts down the decal pool state.
*    @param  pool Pool state to reset.
**/
void CLG_DecalPool_Shutdown( clg_decal_pool_t *pool );

/**
*    @brief  Clears all active decals from the pool.
*    @param  pool Pool state to clear.
**/
void CLG_DecalPool_Clear( clg_decal_pool_t *pool );

/**
*    @brief  Allocates a runtime entry, evicting oldest active entry when full.
*    @param  pool Pool state to allocate from.
*    @return Pointer to writable entry, or nullptr on failure.
**/
clg_decal_instance_t *CLG_DecalPool_Alloc( clg_decal_pool_t *pool );

/**
*    @brief  Frees one runtime entry by decal id.
*    @param  pool Pool state owning the entry.
*    @param  decalId Runtime decal id to remove.
*    @return True when a matching active entry was found and freed.
**/
const bool CLG_DecalPool_FreeById( clg_decal_pool_t *pool, const uint32_t decalId );

/**
*    @brief  Returns number of active decals in the pool.
*    @param  pool Pool state to query.
**/
int32_t CLG_DecalPool_GetActiveCount( const clg_decal_pool_t *pool );
