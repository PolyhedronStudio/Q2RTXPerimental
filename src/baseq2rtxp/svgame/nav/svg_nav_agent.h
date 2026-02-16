/********************************************************************
*
*
*	SVGame: Navigation Agent Helpers
*
*
********************************************************************/
#pragma once

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_path_process.h"

struct svg_base_edict_t;

/**
*	@brief	Resolve a feet-origin agent bbox suitable for nav queries.
*	@param	ent		Entity requesting a bbox (optional, may be null).
*	@param	out_mins	[out] Feet-origin mins.
*	@param	out_maxs	[out] Feet-origin maxs.
*	@note	Preference order:
*			1) active mesh generation bounds,
*			2) current nav agent cvar profile,
*			3) entity collision bounds,
*			4) zero extents fallback.
**/
void SVG_NavAgent_GetQueryBBox( const svg_base_edict_t *ent, Vector3 *out_mins, Vector3 *out_maxs );

/**
*	@brief	Cancel pending async work and clear a path-process cache.
**/
void SVG_NavAgent_ResetPathProcess( svg_nav_path_process_t *pathProcess );

/**
*	@brief	Standard async rebuild orchestration shared by AI entities.
*	@param	pathProcess		Entity-local path process state.
*	@param	ownerEntNum		Entity number used in debug logging.
*	@param	start_origin	Feet-origin start.
*	@param	goal_origin	Feet-origin goal.
*	@param	policy		Path policy used for rebuild heuristics.
*	@param	agent_mins	Feet-origin bbox mins.
*	@param	agent_maxs	Feet-origin bbox maxs.
*	@param	force		Bypass movement throttle heuristics.
*	@param	emitDebug	Emit verbose queue diagnostics.
*	@return	True when the request is queued or intentionally deferred.
**/
bool SVG_NavAgent_TryQueueRebuild( svg_nav_path_process_t *pathProcess, int32_t ownerEntNum,
	const Vector3 &start_origin, const Vector3 &goal_origin, const svg_nav_path_policy_t &policy,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, bool force = false, bool emitDebug = false );
