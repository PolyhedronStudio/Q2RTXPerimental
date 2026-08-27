//! Tactical cover point spatial queries, evaluation, and reservation management.
/********************************************************************
*
*
*	ServerGame: NavMesh Tactical Cover Query System
*
*	Provides fast spatial grid filtering, threat-relative dot product
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
#include <cmath>

//! External reference to the global list of generated cover points.
extern std::vector<nav_cover_point_t> g_nav_cover_points;

//! Spatial cell size in Quake units for tactical cover point spatial hashing.
static constexpr float NAV_COVER_GRID_CELL_SIZE = 256.0f;

//! Tactical cover 2D spatial grid acceleration structure for O(1) neighborhood queries.
struct nav_cover_grid_t {
	//! Minimum world X coordinate mapped by the spatial grid.
	float min_x = -4096.0f;
	//! Minimum world Y coordinate mapped by the spatial grid.
	float min_y = -4096.0f;
	//! Grid column count.
	int32_t width = 0;
	//! Grid row count.
	int32_t height = 0;
	//! Flattened 2D array of cover point indices per grid cell.
	std::vector<std::vector<int32_t>> cells = {};

	/**
	*	@brief	Construct spatial grid from the current global cover points list.
	**/
	void Build( const std::vector<nav_cover_point_t> &points ) {
		cells.clear();
		width = 0;
		height = 0;

		// If no cover points exist, return early.
		if ( points.empty() ) {
			return;
		}

		// Compute 2D bounding box across all cover points.
		float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
		for ( const nav_cover_point_t &cp : points ) {
			minX = std::min( minX, cp.local_position.x );
			minY = std::min( minY, cp.local_position.y );
			maxX = std::max( maxX, cp.local_position.x );
			maxY = std::max( maxY, cp.local_position.y );
		}

		// Pad bounding box by one cell to prevent boundary clamping artifacts.
		min_x = minX - NAV_COVER_GRID_CELL_SIZE;
		min_y = minY - NAV_COVER_GRID_CELL_SIZE;
		const float extent_x = ( maxX - min_x ) + NAV_COVER_GRID_CELL_SIZE * 2.0f;
		const float extent_y = ( maxY - min_y ) + NAV_COVER_GRID_CELL_SIZE * 2.0f;

		width = std::max( 1, static_cast<int32_t>( std::ceil( extent_x / NAV_COVER_GRID_CELL_SIZE ) ) );
		height = std::max( 1, static_cast<int32_t>( std::ceil( extent_y / NAV_COVER_GRID_CELL_SIZE ) ) );

		cells.resize( static_cast<size_t>( width * height ) );

		// Insert each cover point index into its spatial grid cell.
		for ( size_t i = 0; i < points.size(); i++ ) {
			const int32_t gx = std::clamp( static_cast<int32_t>( ( points[ i ].local_position.x - min_x ) / NAV_COVER_GRID_CELL_SIZE ), 0, width - 1 );
			const int32_t gy = std::clamp( static_cast<int32_t>( ( points[ i ].local_position.y - min_y ) / NAV_COVER_GRID_CELL_SIZE ), 0, height - 1 );
			cells[ static_cast<size_t>( gy * width + gx ) ].push_back( static_cast<int32_t>( i ) );
		}
	}

	/**
	*	@brief	Collect candidate cover point indices within a 2D circle from the spatial grid.
	**/
	void QueryRadius( const Vector3 &center, const float radius, std::vector<int32_t> &out_indices ) const {
		out_indices.clear();
		if ( cells.empty() || radius <= 0.0f ) {
			return;
		}

		const int32_t min_gx = std::clamp( static_cast<int32_t>( ( center.x - radius - min_x ) / NAV_COVER_GRID_CELL_SIZE ), 0, width - 1 );
		const int32_t max_gx = std::clamp( static_cast<int32_t>( ( center.x + radius - min_x ) / NAV_COVER_GRID_CELL_SIZE ), 0, width - 1 );
		const int32_t min_gy = std::clamp( static_cast<int32_t>( ( center.y - radius - min_y ) / NAV_COVER_GRID_CELL_SIZE ), 0, height - 1 );
		const int32_t max_gy = std::clamp( static_cast<int32_t>( ( center.y + radius - min_y ) / NAV_COVER_GRID_CELL_SIZE ), 0, height - 1 );

		for ( int32_t gy = min_gy; gy <= max_gy; gy++ ) {
			for ( int32_t gx = min_gx; gx <= max_gx; gx++ ) {
				const auto &cell = cells[ static_cast<size_t>( gy * width + gx ) ];
				out_indices.insert( out_indices.end(), cell.begin(), cell.end() );
			}
		}
	}

	/**
	*	@brief	Clear spatial grid data.
	**/
	void Clear( void ) {
		cells.clear();
		width = 0;
		height = 0;
	}
};

//! Global spatial grid instance for fast cover point lookups.
static nav_cover_grid_t s_cover_grid = {};

//! List of currently claimed cover point indices for fast O(C) reservation queries.
static std::vector<int32_t> s_active_claimed_cover_indices = {};

/**
*	@brief		Build or rebuild the 2D spatial grid acceleration index for tactical cover points.
**/
void Nav_RebuildCoverSpatialIndex( void ) {
	s_cover_grid.Build( g_nav_cover_points );
	s_active_claimed_cover_indices.clear();
}

/**
*	@brief		Clear the tactical cover point spatial grid acceleration index.
**/
void Nav_ClearCoverSpatialIndex( void ) {
	s_cover_grid.Clear();
	s_active_claimed_cover_indices.clear();
}

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
	*	High-performance active claims test: check proximity against active reservations only.
	*	Prune expired claims on the fly for O(C) efficiency where C is the number of active claims.
	**/
	if ( exclusion_radius > 0.0f ) {
		const float excl_sqr = exclusion_radius * exclusion_radius;

		for ( size_t k = 0; k < s_active_claimed_cover_indices.size(); ) {
			const int32_t active_idx = s_active_claimed_cover_indices[ k ];
			if ( active_idx < 0 || active_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
				s_active_claimed_cover_indices.erase( s_active_claimed_cover_indices.begin() + k );
				continue;
			}

			const nav_cover_point_t &other = g_nav_cover_points[ active_idx ];
			// If claim has expired, prune it from active tracking list.
			if ( other.claimed_by_ent == ENTITYNUM_NONE || level.time >= other.claim_expiration ) {
				other.claimed_by_ent = ENTITYNUM_NONE;
				other.claim_expiration = 0_ms;
				s_active_claimed_cover_indices.erase( s_active_claimed_cover_indices.begin() + k );
				continue;
			}

			// If another entity holds this claim, check spatial exclusion distance.
			if ( active_idx != cover_idx && other.claimed_by_ent != requester_ent ) {
				Vector3 other_pos = {}, other_norm = {};
				if ( Nav_GetCoverPointWorld( other, &other_pos, &other_norm ) ) {
					if ( QM_Vector3DistanceSqr( my_pos, other_pos ) <= excl_sqr ) {
						return true;
					}
				}
			}

			k++;
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

	// Register with active claims index if not already present.
	if ( std::find( s_active_claimed_cover_indices.begin(), s_active_claimed_cover_indices.end(), cover_idx ) == s_active_claimed_cover_indices.end() ) {
		s_active_claimed_cover_indices.push_back( cover_idx );
	}

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

		// Remove from active claims index.
		auto it = std::find( s_active_claimed_cover_indices.begin(), s_active_claimed_cover_indices.end(), cover_idx );
		if ( it != s_active_claimed_cover_indices.end() ) {
			s_active_claimed_cover_indices.erase( it );
		}
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
	Vector3 to_threat = QM_Vector3Subtract( threat_origin, world_pos );
	to_threat.z = 0.0f;
	const float dist = QM_Vector3Length( to_threat );
	if ( dist < 1.0f ) {
		return 0.0f;
	}

	const Vector3 to_threat_dir = QM_Vector3Scale( to_threat, 1.0f / dist );
	const float wall_alignment = QM_Vector3DotProduct( to_threat_dir, QM_Vector3Scale( world_normal, -1.0f ) );
	return QM_Clamp( wall_alignment, -1.0f, 1.0f );
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
*	@param	min_cover_type		Posture requirement filter (NAV_COVER_LOW, NAV_COVER_HIGH, or NAV_COVER_NONE for any posture).
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

	// Auto-build spatial acceleration grid if empty.
	if ( s_cover_grid.cells.empty() && !g_nav_cover_points.empty() ) {
		Nav_RebuildCoverSpatialIndex();
	}

	// Fetch optional requester edict pointer for mover rider tests.
	const svg_base_edict_t *requester_edict = ( requester_ent > 0 && requester_ent < g_edict_pool.num_edicts )
		? g_edicts[ requester_ent ] : nullptr;

	const float radius_sqr = radius * radius;
	const float dist_self_to_threat = QM_Vector3Distance( search_origin, threat_origin );

	/**
	*	Phase 1: Fast Spatial Grid Query (O(1) localized candidate collection).
	**/
	std::vector<int32_t> grid_candidates = {};
	s_cover_grid.QueryRadius( search_origin, radius, grid_candidates );

	// Fallback to full iteration if spatial grid query returned no cells.
	if ( grid_candidates.empty() ) {
		grid_candidates.resize( g_nav_cover_points.size() );
		for ( size_t i = 0; i < g_nav_cover_points.size(); i++ ) {
			grid_candidates[ i ] = static_cast<int32_t>( i );
		}
	}

	std::vector<nav_cover_candidate_t> candidates = {};
	candidates.reserve( 32 );

	/**
	*	Phase 2: Fast Zero-Raycast Broad-Phase Filter & Tactical Scoring (pure arithmetic).
	**/
	for ( const int32_t cp_idx : grid_candidates ) {
		if ( cp_idx < 0 || cp_idx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
			continue;
		}

		const nav_cover_point_t &cp = g_nav_cover_points[ cp_idx ];

		// 1. Check cover posture constraint.
		if ( min_cover_type != NAV_COVER_NONE && cp.cover_type != min_cover_type ) {
			continue;
		}

		// 2. Reject if this cover point is currently on cooldown.
		if ( level.time < cp.cooldown_until ) {
			continue;
		}

		// 3. Check reservation tokens (fast active claims check).
		if ( Nav_IsCoverPointSpatiallyClaimed( cp_idx, requester_ent, 128.0f ) ) {
			continue;
		}

		// 4. Check dynamic mover validity (doors open/closed, moving platform velocity).
		if ( !Nav_IsCoverPointUsable( cp, requester_edict ) ) {
			continue;
		}

		// 5. Resolve current world-space coordinates.
		Vector3 world_pos = {}, world_normal = {};
		if ( !Nav_GetCoverPointWorld( cp, &world_pos, &world_normal ) ) {
			continue;
		}

		// 6. Distance check relative to search origin.
		const Vector3 to_point = QM_Vector3Subtract( world_pos, search_origin );
		const float dist_sqr = QM_Vector3DotProduct( to_point, to_point );
		if ( dist_sqr > radius_sqr ) {
			continue;
		}

		/**
		*	Calculate comprehensive broad-phase tactical score (0 raycasts):
		*	Favors distance from threat (flee gain), directional wall occlusion, and closer proximity to monster.
		**/
		Vector3 to_threat = QM_Vector3Subtract( threat_origin, world_pos );
		to_threat.z = 0.0f;
		const float dist_cover_to_threat = QM_Vector3Length( to_threat );
		const float dist_from_self = std::sqrt( dist_sqr );
		const float flee_gain = dist_cover_to_threat - dist_self_to_threat; // Positive = increases distance from threat

		float wall_alignment = 0.0f;
		if ( dist_cover_to_threat > 1.0f ) {
			const Vector3 to_threat_dir = QM_Vector3Scale( to_threat, 1.0f / dist_cover_to_threat );
			wall_alignment = QM_Vector3DotProduct( to_threat_dir, QM_Vector3Scale( world_normal, -1.0f ) );
		}

		// Base candidate score (always positive so all valid spots can compete)
		float score = 500.0f;
		// Directional wall alignment bonus (-150 to +150)
		score += wall_alignment * 150.0f;
		// Flee distance gain bonus
		score += QM_Clamp( flee_gain, -200.0f, 600.0f ) * 0.8f;
		// Travel distance penalty
		score -= dist_from_self * 0.25f;
		// Crouch posture preference bonus
		if ( cp.cover_type == NAV_COVER_LOW ) {
			score += 150.0f;
		}

		candidates.push_back( { cp_idx, score } );
	}

	// Return false if no suitable cover points passed broad-phase filtering.
	if ( candidates.empty() ) {
		return false;
	}

	/**
	*	Phase 3: Rank candidate cover points by score descending.
	**/
	std::sort( candidates.begin(), candidates.end(), []( const nav_cover_candidate_t &a, const nav_cover_candidate_t &b ) {
		return a.score > b.score;
	} );

	/**
	*	Phase 4: Budgeted Narrow-Phase Raycast Validation (Max 8 traces per query!).
	**/
	constexpr size_t MAX_TRACE_VALIDATIONS = 8;
	size_t traces_performed = 0;

	for ( const auto &cand : candidates ) {
		if ( traces_performed < MAX_TRACE_VALIDATIONS ) {
			traces_performed++;
			const float trace_prot = Nav_EvaluateCoverForThreat( cand.index, threat_origin, true );
			if ( trace_prot > 0.0f ) {
				out_cover_indices->push_back( cand.index );
				// If we have collected enough confirmed occluded spots, early exit narrow phase!
				if ( out_cover_indices->size() >= 4 ) {
					break;
				}
			}
		} else {
			break;
		}
	}

	// If narrow-phase filtered out all points due to open terrain, fall back to best broad-phase point.
	if ( out_cover_indices->empty() && !candidates.empty() ) {
		out_cover_indices->push_back( candidates[ 0 ].index );
	}

	return !out_cover_indices->empty();
}
