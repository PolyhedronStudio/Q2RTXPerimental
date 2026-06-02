/********************************************************************
*
*
*    ClientGame: Decal Runtime Scaffolding.
*
*
********************************************************************/
#pragma once

#include <cstdint>

#include "sharedgame/sg_decal_shared.h"

class QMTime;

/**
*    @brief  Initialize decal module state.
**/
void CLG_Decals_Init( void );

/**
*    @brief  Shutdown decal module state.
**/
void CLG_Decals_Shutdown( void );

/**
*    @brief  Clears all runtime decal entries.
**/
void CLG_Decals_Clear( void );

/**
*    @brief  Begin-frame hook for decal updates.
**/
void CLG_Decals_BeginFrame( void );

/**
*    @brief  End-frame hook for decal updates.
**/
void CLG_Decals_EndFrame( void );

/**
*    @brief  Advances decal lifetime state.
*    @param  now The current local frame time.
**/
void CLG_Decals_Update( const QMTime now );

/**
*    @brief  Queues a decal spawn request.
*    @return True if request was accepted.
**/
const bool CLG_Decals_QueueSpawn( const sg_decal_spawn_params_t &params );

/**
*    @brief  Returns active decal count.
**/
int32_t CLG_Decals_GetActiveCount( void );

/**
*    @brief  Handles gunshot impact event for decal spawning.
*    @param  origin Impact origin.
*    @param  exactNormal Full-precision impact normal carried by the temp event when available.
*    @param  direction Packed legacy direction byte used as fallback.
*    @param  count Impact intensity hint.
*    @param  hitEntityNumber Brush-model entity hit by the impact, or ENTITYNUM_WORLD.
**/
void CLG_DecalEvents_HandleImpactGunShot( const vec3_t origin, const vec3_t exactNormal, const uint8_t direction, const int32_t count, const int32_t hitEntityNumber );

/**
*    @brief  Handles bullet sparks impact event for decal spawning.
*    @param  origin Impact origin.
*    @param  exactNormal Full-precision impact normal carried by the temp event when available.
*    @param  direction Packed legacy direction byte used as fallback.
*    @param  count Impact intensity hint.
*    @param  hitEntityNumber Brush-model entity hit by the impact, or ENTITYNUM_WORLD.
**/
void CLG_DecalEvents_HandleImpactBulletSparks( const vec3_t origin, const vec3_t exactNormal, const uint8_t direction, const int32_t count, const int32_t hitEntityNumber );
