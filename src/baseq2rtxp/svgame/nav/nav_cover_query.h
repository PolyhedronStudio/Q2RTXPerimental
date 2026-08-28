#pragma once

#include "svgame/nav/nav_cover_types.h"
#include <vector>

/**
*	@brief		Find valid cover points protecting against a threat within a search radius (Vector3DP double precision).
*	@param	search_origin		Center origin of the search area in Vector3DP.
*	@param	threat_origin		Position of the enemy/threat to seek cover from in Vector3DP.
*	@param	radius				Maximum search distance from search_origin in double precision.
*	@param	requester_ent		Entity ID requesting cover (used to filter claim reservations).
*	@param	out_cover_indices	[out] List of valid cover point indices ranked by tactical score.
*	@param	min_cover_type		Posture requirement filter (NAV_COVER_LOW, NAV_COVER_HIGH, or NAV_COVER_NONE).
*	@param	threat_forward		Optional normalized horizontal forward direction the threat is facing (Vector3DP).
*	@param	max_results			Maximum number of candidate cover points to collect (default: 12).
*	@return	True when one or more suitable cover points were found.
**/
const bool Nav_FindCoverPoints( const Vector3DP &search_origin, const Vector3DP &threat_origin,
	const double radius, const int32_t requester_ent, std::vector<int32_t> *out_cover_indices,
	const nav_cover_type_t min_cover_type = NAV_COVER_NONE,
	const Vector3DP &threat_forward = Vector3DP{ 0.0, 0.0, 0.0 },
	const size_t max_results = 12 );

/**
*	@brief		Find valid cover points protecting against a threat within a search radius (single-precision wrapper).
*	@param	search_origin		Center origin of the search area.
*	@param	threat_origin		Position of the enemy/threat to seek cover from.
*	@param	radius				Maximum search distance from search_origin.
*	@param	requester_ent		Entity ID requesting cover (used to filter claim reservations).
*	@param	out_cover_indices	[out] List of valid cover point indices ranked by tactical score.
*	@param	min_cover_type		Posture requirement filter (NAV_COVER_LOW, NAV_COVER_HIGH, or NAV_COVER_NONE for any posture).
*	@return	True when one or more suitable cover points were found.
**/
inline const bool Nav_FindCoverPoints( const Vector3 &search_origin, const Vector3 &threat_origin,
	const float radius, const int32_t requester_ent, std::vector<int32_t> *out_cover_indices,
	const nav_cover_type_t min_cover_type = NAV_COVER_NONE ) {
	return Nav_FindCoverPoints( Vector3DP( search_origin ), Vector3DP( threat_origin ), static_cast<double>( radius ), requester_ent, out_cover_indices, min_cover_type );
}

/**
*	@brief		Evaluate the tactical protection score of a specific cover point against a threat (Vector3DP double precision).
*	@param	cover_idx			Index into the global cover points array.
*	@param	threat_origin		Position of the threat to evaluate against in Vector3DP.
*	@param	perform_trace_check	When true, performs a line-of-sight trace to verify occlusion.
*	@return	Score between 0.0f (no cover/exposed) and 1.0f (ideal directional occlusion).
**/
const float Nav_EvaluateCoverForThreat( const int32_t cover_idx, const Vector3DP &threat_origin, const bool perform_trace_check = true );

/**
*	@brief		Evaluate the tactical protection score of a specific cover point against a threat (single-precision wrapper).
*	@param	cover_idx			Index into the global cover points array.
*	@param	threat_origin		Position of the threat to evaluate against.
*	@param	perform_trace_check	When true, performs a line-of-sight trace to verify occlusion.
*	@return	Score between 0.0f (no cover/exposed) and 1.0f (ideal directional occlusion).
**/
inline const float Nav_EvaluateCoverForThreat( const int32_t cover_idx, const Vector3 &threat_origin, const bool perform_trace_check = true ) {
	return Nav_EvaluateCoverForThreat( cover_idx, Vector3DP( threat_origin ), perform_trace_check );
}

/**
*	@brief		Claim/reserve a cover point for an entity for a specified duration.
*	@param	cover_idx			Index into the global cover points array.
*	@param	entity_id			Entity ID claiming the point.
*	@param	duration			Lease duration for the reservation (default 3000ms).
*	@return	True when the reservation was successfully acquired.
**/
const bool Nav_ClaimCoverPoint( const int32_t cover_idx, const int32_t entity_id, const QMTime duration = 3000_ms );

/**
*	@brief		Release an active cover point reservation.
*	@param	cover_idx			Index into the global cover points array.
*	@param	entity_id			Entity ID releasing the reservation.
**/
void Nav_ReleaseCoverPoint( const int32_t cover_idx, const int32_t entity_id );

/**
*	@brief		Place a cover point on cooldown to prevent immediate reuse after being compromised.
*	@param	cover_idx	Index into the global cover points array.
*	@param	duration	Cooldown duration.
**/
void Nav_SetCoverPointCooldown( const int32_t cover_idx, const QMTime duration );

/**
*	@brief		Check whether a cover point is currently claimed by another entity.
*	@param	cover_idx			Index into the global cover points array.
*	@param	requester_ent		Optional entity ID requesting check (returns false if claimed by requester).
*	@return	True when currently claimed by another entity and the reservation has not expired.
**/
const bool Nav_IsCoverPointClaimed( const int32_t cover_idx, const int32_t requester_ent = ENTITYNUM_NONE );

/**
*	@brief		Check whether a cover point or any neighboring cover point within an exclusion radius is claimed by another entity.
*	@param	cover_idx			Index into the global cover points array.
*	@param	requester_ent		Optional entity ID requesting check (returns false if claimed by requester).
*	@param	exclusion_radius	Spatial radius in units around other claimed cover spots to consider occupied.
*	@return	True when the cover point or its neighborhood is claimed by another entity.
**/
const bool Nav_IsCoverPointSpatiallyClaimed( const int32_t cover_idx, const int32_t requester_ent = ENTITYNUM_NONE, const float exclusion_radius = 128.0f );

/**
*	@brief		Get a pointer to a cover point by index.
*	@param	cover_idx			Index into the global cover points array.
*	@return	Pointer to the cover point record, or nullptr if index is out of bounds.
**/
const nav_cover_point_t *Nav_GetCoverPoint( const int32_t cover_idx );

/**
*	@brief		Get the total number of precalculated cover points in the active navmesh.
*	@return	Cover point count.
**/
const int32_t Nav_GetCoverPointCount( void );

/**
*	@brief		Build or rebuild the 2D spatial grid acceleration index for tactical cover points.
**/
void Nav_RebuildCoverSpatialIndex( void );

/**
*	@brief		Clear the tactical cover point spatial grid acceleration index.
**/
void Nav_ClearCoverSpatialIndex( void );
