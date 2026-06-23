#pragma once

#include "sv_server.h"

#if USE_CLIENT

/**
*	@brief	Initialize or reset the persistent debug draw queues.
*	@note	Called by the engine server once per game frame (40hz).
**/
void SV_ClearDebugDrawQueues( void );

/**
*	@brief	Submit the persistent debug draw queues to the client renderer.
*	@note	Called by the engine client once per render frame (60hz+).
**/
void SV_SubmitDebugDrawQueues( void );

/**
*	@brief	Engine-wrapped game import implementations that push primitives
*			into the thread-safe persistent debug queues.
**/
void PF_SV_R_DrawDebugBox( const vec3_t mins, const vec3_t maxs, uint32_t color );
void PF_SV_R_DrawDebugLine( const vec3_t start, const vec3_t end, uint32_t color );
void PF_SV_R_DrawDebugArrow( const vec3_t start, const vec3_t end, float head_length, uint32_t color );
void PF_SV_R_DrawDebugSphere( const vec3_t center, float radius, uint32_t color );
void PF_SV_R_DrawDebugCapsule( const vec3_t start, const vec3_t end, float radius, uint32_t color );
void PF_SV_R_DrawDebugCylinder( const vec3_t start, const vec3_t end, float radius, uint32_t color );

#endif // USE_CLIENT
