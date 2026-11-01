#pragma once
#include "svgame/nav/nav_query.h"
#include "svgame/nav/nav_worker_service.h"

//! Production graph buffers remain allocated/reclaimed on the server owner.
using svg_nav_worker_service_t = nav_worker_service_t<svg_nav_query_buffer_t>;

/**
*	@brief	Start an explicitly requested graph worker after owner capacity and snapshot checks.
*	@note	Diagnostic commands and opt-in actor activation share lifecycle but never an entity-key namespace.
**/
bool Nav_StartQueryWorkerMainThread( void );
/**
*	@brief	Join the dedicated query reader before any publication, generation or module lifetime reset.
**/
void Nav_StopQueryWorkerMainThread( void );
/**
*	@brief	Drain at most two replies under a cooperative 500us owner budget before thinking.
**/
void Nav_QueryWorkerBeginFrameMainThread( void );
/**
*	@brief	Publish a frame's collected submissions after crowd/entity work.
**/
void Nav_QueryWorkerEndFrameMainThread( void );
/**
*	@brief	Print copied worker budgets and lifecycle counters on the owner.
**/
void Nav_QueryWorkerStatusCommand( void );
/**
*	@brief	Run bounded native graph transport parity/saturation and record measured capacities.
**/
void Nav_QueryWorkerCheckCommand( void );

/**
*	@brief	Drain both navigation readers for engine collision/map/module lifetime changes.
*	@note	Ordinary client callbacks only need generation exclusion, not a copied-graph worker stop.
**/
void Nav_DrainNavigationReadersMainThread( void );

/**
*	@brief	Cancel a copied actor request without clearing its safely committed movement route.
**/
void Nav_CancelQueryWorkerEntityMainThread( const int32_t entity );
/**
*	@brief	Queue a synthetic busy batch for native clear/map/generation/shutdown stress.
**/
void Nav_QueryWorkerQueueCommand( void );

/**
*	@brief	Read opt-in production scheduling mode on the game owner.
**/
bool Nav_IsActorQueryWorkerEnabledMainThread();
/**
*	@brief	Copy and enqueue one actor graph query with bounded transport ownership.
*	@return	False means explicit capacity/publication backoff, never synchronous escape.
**/
bool Nav_SubmitActorQueryMainThread( svg_nav_request_t &request, const int32_t start, const int32_t goal,
	const nav_path_policy_t &policy, const nav_query_cost_policy_t &cost, const std::span<const uint32_t> marks,
	const uint32_t stamp, const Vector3DP &feet, const Vector3DP &target, const nav_query_identity_t &identity,
	const uint32_t priority, const int32_t group, uint64_t *out_earliest_tick );
