//! Tactical cover point spatial queries, evaluation, and reservation management.
/********************************************************************
*
*
*	ServerGame: NavMesh Tactical Cover Query System
*
*	Provides fast spatial filtering, threat-relative dot product
*	evaluation, line-of-sight validation, and multi-agent claim tokens.
*
*
********************************************************************/
#include "nav_cover_query.h"
#include "nav_cover_types.h"
#include "nav_core.h"
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_level_locals.h"
#include <algorithm>
#include <vector>

//! External reference to the global list of generated cover points.
extern std::vector<nav_cover_point_t> g_nav_cover_points;

/**
*	@brief		Get a pointer to a cover point by index.
*	@param	cover_idx	Index into the global cover points array.
*	@return	Pointer to the cover point record, or nullptr if index is out of bounds.
**/
const nav_cover_point_t *Nav_GetCoverPoint( const int32_t cover_idx ) {
	/**
	*	Sanity checks: Validate index bounds.
	**/
	// Ensure index is within valid vector range.
	if ( cover_idx < 0 || cover_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
		return nullptr;
	}

	return &g_nav_cover_points[ cover_idx ];
}

/**
*	@brief		Get the total number of precalculated cover points in the active navmesh.
*	@return	Cover point count.
**/
const int32_t Nav_GetCoverPointCount( void ) {
	return static_cast<int32_t>( g_nav_cover_points.size() );
}

/**
*	@brief		Check whether a cover point is currently claimed by another entity.
*	@param	cover_idx		Index into the global cover points array.
*	@param	requester_ent	Optional entity ID requesting check (returns false if claimed by requester).
*	@return	True when currently claimed by another entity and the reservation has not expired.
**/
const bool Nav_IsCoverPointClaimed( const int32_t cover_idx, const int32_t requester_ent ) {
	/**
	*	Validate index bounds.
	**/
	if ( cover_idx < 0 || cover_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
		return true; // Out of bounds treated as unavailable.
	}

	const nav_cover_point_t &cover = g_nav_cover_points[ cover_idx ];

	/**
	*	Check reservation state and expiration against level.time.
	**/
	// If no entity claimed this or reservation expired, it is unclaimed.
	if ( cover.claimed_by_ent == ENTITYNUM_NONE || level.time >= cover.claim_expiration ) {
		return false;
	}

	// If claimed by the requester itself, it is considered available for the requester.
	if ( requester_ent != ENTITYNUM_NONE && cover.claimed_by_ent == requester_ent ) {
		return false;
	}

	return true;
}

/**
*	@brief		Check whether a cover point or any neighboring cover point within an exclusion radius is claimed by another entity.
*	@param	cover_idx			Index into the global cover points array.
*	@param	requester_ent		Optional entity ID requesting check (returns false if claimed by requester).
*	@param	exclusion_radius	Spatial radius in units around other claimed cover spots to consider occupied.
*	@return	True when the cover point or its neighborhood is claimed by another entity.
**/
const bool Nav_IsCoverPointSpatiallyClaimed( const int32_t cover_idx, const int32_t requester_ent, const float exclusion_radius ) {
	/**
	*	Validate index bounds.
	**/
	if ( cover_idx < 0 || cover_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
		return true;
	}

	const nav_cover_point_t &target_cp = g_nav_cover_points[ cover_idx ];

	// Check if this cover point is currently on cooldown (recently compromised or abandoned).
	if ( level.time < target_cp.cooldown_until ) {
		return true;
	}

	// Check if this specific cover point is directly claimed by someone else.
	if ( Nav_IsCoverPointClaimed( cover_idx, requester_ent ) ) {
		return true;
	}

	// Resolve world-space coordinates of the candidate cover point.
	Vector3 my_pos = {}, my_norm = {};
	if ( !Nav_GetCoverPointWorld( target_cp, &my_pos, &my_norm ) ) {
		return false;
	}

	/**
	*	Check proximity against all other active reservations held by other entities.
	**/
	if ( exclusion_radius > 0.0f ) {
		const float excl_sqr = exclusion_radius * exclusion_radius;
		const int32_t num_points = static_cast<int32_t>( g_nav_cover_points.size() );

		for ( int32_t j = 0; j < num_points; j++ ) {
			if ( j == cover_idx ) {
				continue;
			}

			const nav_cover_point_t &other = g_nav_cover_points[ j ];
			// Skip unclaimed or self-claimed reservations.
			if ( other.claimed_by_ent == ENTITYNUM_NONE || other.claimed_by_ent == requester_ent || level.time >= other.claim_expiration ) {
				continue;
			}

			Vector3 other_pos = {}, other_norm = {};
			if ( !Nav_GetCoverPointWorld( other, &other_pos, &other_norm ) ) {
				continue;
			}

			// If candidate is within the spatial exclusion bubble of another entity's cover spot, treat as claimed.
			if ( QM_Vector3DistanceSqr( my_pos, other_pos ) <= excl_sqr ) {
				return true;
			}
		}
	}

	/**
	*	Check physical monster body proximity:
	*	Reject candidate if any other living monster entity is currently standing within 80 units.
	**/
	constexpr float monster_body_dist_sqr = 80.0f * 80.0f;
	for ( int32_t ent_idx = game.maxclients + 1; ent_idx < g_edict_pool.num_edicts; ent_idx++ ) {
		if ( ent_idx == requester_ent ) {
			continue;
		}

		const svg_base_edict_t *other_ent = g_edict_pool.EdictForNumber( ent_idx );
		if ( !other_ent || !other_ent->inUse || other_ent->health <= 0 ) {
			continue;
		}

		// Check if another living entity is physically occupying this position.
		if ( QM_Vector3DistanceSqr( other_ent->currentOrigin, my_pos ) <= monster_body_dist_sqr ) {
			return true;
		}
	}

	return false;
}

/**
*	@brief		Claim/reserve a cover point for an entity for a specified duration.
*	@param	cover_idx	Index into the global cover points array.
*	@param	entity_id	Entity ID claiming the point.
*	@param	duration	Lease duration for the reservation (default 3000ms).
*	@return	True when the reservation was successfully acquired.
**/
const bool Nav_ClaimCoverPoint( const int32_t cover_idx, const int32_t entity_id, const QMTime duration ) {
	/**
	*	Sanity checks: Validate index bounds and claiming entity ID.
	**/
	if ( cover_idx < 0 || cover_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) || entity_id <= 0 ) {
		return false;
	}

	// Check if already claimed by someone else.
	if ( Nav_IsCoverPointClaimed( cover_idx, entity_id ) ) {
		return false;
	}

	/**
	*	Commit reservation token and expiration time.
	**/
	nav_cover_point_t &cover = g_nav_cover_points[ cover_idx ];
	cover.claimed_by_ent = entity_id;
	cover.claim_expiration = level.time + duration;
	return true;
}

/**
*	@brief		Release an active cover point reservation.
*	@param	cover_idx	Index into the global cover points array.
*	@param	entity_id	Entity ID releasing the reservation.
**/
void Nav_ReleaseCoverPoint( const int32_t cover_idx, const int32_t entity_id ) {
	/**
	*	Validate index bounds.
	**/
	if ( cover_idx < 0 || cover_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
		return;
	}

	nav_cover_point_t &cover = g_nav_cover_points[ cover_idx ];
	// Release only if this entity holds the claim.
	if ( cover.claimed_by_ent == entity_id ) {
		cover.claimed_by_ent = ENTITYNUM_NONE;
		cover.claim_expiration = 0_ms;
	}
}

/**
*	@brief		Place a cover point on cooldown to prevent immediate reuse after being compromised.
*	@param	cover_idx	Index into the global cover points array.
*	@param	duration	Cooldown duration.
**/
void Nav_SetCoverPointCooldown( const int32_t cover_idx, const QMTime duration ) {
	/**
	*	Validate index bounds.
	**/
	if ( cover_idx < 0 || cover_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
		return;
	}

	nav_cover_point_t &cover = g_nav_cover_points[ cover_idx ];
	cover.cooldown_until = level.time + duration;
}

/**
*	@brief		Evaluate the tactical protection score of a specific cover point against a threat.
*	@param	cover_idx			Index into the global cover points array.
*	@param	threat_origin		Position of the threat to evaluate against.
*	@param	perform_trace_check	When true, performs a line-of-sight trace to verify occlusion.
*	@return	Score between 0.0f (no cover/exposed) and 1.0f (ideal directional occlusion).
**/
const float Nav_EvaluateCoverForThreat( const int32_t cover_idx, const Vector3 &threat_origin, const bool perform_trace_check ) {
	/**
	*	Validate cover point pointer.
	**/
	const nav_cover_point_t *cover = Nav_GetCoverPoint( cover_idx );
	if ( !cover ) {
		return 0.0f;
	}

	// Reject if this cover point is currently on cooldown.
	if ( level.time < cover->cooldown_until ) {
		return 0.0f;
	}

	/**
	*	Resolve world-space coordinates.
	**/
	Vector3 world_pos = {}, world_normal = {};
	if ( !Nav_GetCoverPointWorld( *cover, &world_pos, &world_normal ) ) {
		return 0.0f;
	}

	/**
	*	Line-of-sight and obstruction orientation check:
	*	The obstruction must be what the threat sees first (never angularly hide in front of the cover brush).
	**/
	Vector3 to_threat = QM_Vector3Subtract( threat_origin, world_pos );
	to_threat.z = 0.0f;
	const float to_threat_len2 = QM_Vector3LengthSqr( to_threat );
	if ( to_threat_len2 > 1.0f ) {
		const Vector3 to_threat_dir = QM_Vector3Scale( to_threat, 1.0f / std::sqrt( to_threat_len2 ) );
		const float front_alignment = QM_Vector3DotProduct( to_threat_dir, world_normal );
		// If threat is on the open side of the cover normal (> 0.15f), the entity is standing in front of the brush!
		if ( front_alignment > 0.15f ) {
			return 0.0f;
		}
	}

	if ( perform_trace_check ) {
		// Eye position crouched or standing behind the cover obstacle.
		const float eye_z = ( cover->cover_type == NAV_COVER_LOW ) ? 24.0f : 48.0f;
		const Vector3 eye_pos = QM_Vector3Add( world_pos, Vector3{ 0.0f, 0.0f, eye_z } );

		// Trace toward threat eye level (+48 units).
		const Vector3 threat_eye = QM_Vector3Add( threat_origin, Vector3{ 0.0f, 0.0f, 48.0f } );
		const svg_trace_t tr = SVG_Trace( eye_pos, vec3_origin, vec3_origin, threat_eye, nullptr, CM_CONTENTMASK_SOLID );

		// If trace reached the threat without hitting solid geometry, there is no line-of-sight obstruction!
		if ( tr.fraction >= 1.0f ) {
			// Exposed to direct line of sight; invalid cover against this threat.
			return 0.0f;
		}

		// Occluded! The solid world geometry blocks direct sight/fire.
		return 1.0f;
	}

	/**
	*	Directional heuristic when trace check is skipped (fast broad-phase filter).
	**/
	const float dist = QM_Vector3Length( to_threat );
	if ( dist < 1.0f ) {
		return 0.0f;
	}

	const Vector3 to_threat_dir = QM_Vector3Normalize( to_threat );
	const float wall_alignment = QM_Vector3DotProduct( to_threat_dir, QM_Vector3Scale( world_normal, -1.0f ) );
	return std::max<float>( 0.0f, wall_alignment );
}

/**
*	@brief		Candidate cover point scoring helper.
**/
struct nav_cover_candidate_t {
	int32_t index = -1;
	float score = 0.0f;
};

/**
*	@brief		Find valid cover points protecting against a threat within a search radius.
*	@param	search_origin		Center origin of the search area (typically monster origin).
*	@param	threat_origin		Position of the enemy/threat to seek cover from.
*	@param	radius				Maximum search distance from search_origin.
*	@param	requester_ent		Entity ID requesting cover (used to filter claim reservations).
*	@param	out_cover_indices	[out] List of valid cover point indices ranked by tactical score.
*	@param	min_cover_type		Minimum acceptable posture (NAV_COVER_LOW or NAV_COVER_HIGH).
*	@return	True when one or more suitable cover points were found.
**/
const bool Nav_FindCoverPoints( const Vector3 &search_origin, const Vector3 &threat_origin,
	const float radius, const int32_t requester_ent, std::vector<int32_t> *out_cover_indices,
	const nav_cover_type_t min_cover_type ) {
	/**
	*	Sanity checks: Ensure output pointer and cover points list are valid.
	**/
	if ( !out_cover_indices || g_nav_cover_points.empty() || radius <= 0.0f ) {
		return false;
	}

	out_cover_indices->clear();

	// Fetch optional requester edict pointer for mover rider tests.
	const svg_base_edict_t *requester_edict = ( requester_ent > 0 && requester_ent < g_edict_pool.num_edicts )
		? g_edicts[ requester_ent ] : nullptr;

	const float radius_sqr = radius * radius;
	std::vector<nav_cover_candidate_t> candidates = {};
	candidates.reserve( 32 );

	/**
	*	Iterate over all precalculated cover points and filter by distance, mover validity, and threat occlusion.
	**/
	const int32_t num_points = static_cast<int32_t>( g_nav_cover_points.size() );
	for ( int32_t i = 0; i < num_points; i++ ) {
		const nav_cover_point_t &cp = g_nav_cover_points[ i ];

		// 1. Check cover posture constraint.
		if ( cp.cover_type < min_cover_type ) {
			continue;
		}

		// 2. Check reservation tokens (prevent multi-monster clustering with 128-unit spatial exclusion).
		if ( Nav_IsCoverPointSpatiallyClaimed( i, requester_ent, 128.0f ) ) {
			continue;
		}

		// 3. Check dynamic mover validity (doors open/closed, moving platform velocity).
		if ( !Nav_IsCoverPointUsable( cp, requester_edict ) ) {
			continue;
		}

		// 4. Resolve current world-space coordinates.
		Vector3 world_pos = {}, world_normal = {};
		if ( !Nav_GetCoverPointWorld( cp, &world_pos, &world_normal ) ) {
			continue;
		}

		// 5. Distance check relative to search origin.
		const Vector3 to_point = QM_Vector3Subtract( world_pos, search_origin );
		const float dist_sqr = QM_Vector3DotProduct( to_point, to_point );
		if ( dist_sqr > radius_sqr ) {
			continue;
		}

		// 6. Evaluate protection against threat.
		const float protection = Nav_EvaluateCoverForThreat( i, threat_origin, false );
		if ( protection <= 0.15f ) {
			continue;
		}

		/**
		*	Calculate comprehensive tactical score:
		*	Score favors high protection alignment and closer proximity to requester.
		**/
		const float dist = std::sqrt( dist_sqr );
		const float dist_factor = 1.0f - ( dist / radius );
		const float total_score = ( protection * 0.7f ) + ( dist_factor * 0.3f );

		candidates.push_back( { i, total_score } );
	}

	// Return false if no suitable cover points passed all filters.
	if ( candidates.empty() ) {
		return false;
	}

	/**
	*	Sort candidate cover points by score descending (highest score first).
	**/
	std::sort( candidates.begin(), candidates.end(), []( const nav_cover_candidate_t &a, const nav_cover_candidate_t &b ) {
		return a.score > b.score;
	} );

	/**
	*	Populate output indices list.
	**/
	out_cover_indices->reserve( candidates.size() );
	for ( const auto &cand : candidates ) {
		out_cover_indices->push_back( cand.index );
	}

	return true;
}
