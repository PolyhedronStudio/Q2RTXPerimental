/********************************************************************
*
*
*	ServerGame: Nav2 Goal Candidate Endpoint Selection
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_types.h"


/**
*	Forward declarations:
**/
struct nav2_query_mesh_t;

/**
*	Forward declarations:
**/


/**
*
*
*	Nav2 Goal Candidate Enumerations:
*
*
**/
/**
*	@brief	Semantic source classification for a generated goal candidate.
*	@note	The enum is future-facing so later corridor or connector work can keep stable identifiers
*			for ladder, stair, portal, and mover-related endpoint classes.
**/
enum class nav2_goal_candidate_type_t : uint8_t {
    None = 0,
    RawGoal,
    ProjectedSameCell,
    NearbySupport,
    SameLeafHint,
    SameClusterHint,
    PortalEndpoint,
    LadderEndpoint,
    StairBand,
    MoverBoarding,
    MoverExit,
    LegacyProjectedFallback,
    Count
};

/**
*	@brief	Stable rejection taxonomy recorded while building candidate endpoints.
*	@note	This keeps debug reporting explicit without requiring callers to infer why nearby samples were skipped.
**/
enum class nav2_goal_candidate_rejection_t : uint8_t {
    None = 0,
    MissingMesh,
    InvalidAgentBounds,
    MissingCollisionModel,
    MissingWorldTile,
    MissingCellLayers,
    SolidPointContents,
    SolidBoxContents,
    DuplicateCandidate,
    CandidateLimitReached,
    Count
};

/**
*	@brief	Auxiliary ranking flags recorded on a candidate.
*	@note	These are stable bitflags rather than transient pointers so future save/load or compare-mode tools
*			can reason about endpoint selection without fragile identity assumptions.
**/
enum nav2_goal_candidate_flag_t : uint32_t {
    NAV_GOAL_CANDIDATE_FLAG_NONE = 0,
    NAV_GOAL_CANDIDATE_FLAG_SAME_LEAF_AS_START = ( 1u << 0 ),
    NAV_GOAL_CANDIDATE_FLAG_SAME_CLUSTER_AS_START = ( 1u << 1 ),
    NAV_GOAL_CANDIDATE_FLAG_SAME_AREA_AS_START = ( 1u << 2 ),
    NAV_GOAL_CANDIDATE_FLAG_SAME_LEAF_AS_RAW_GOAL = ( 1u << 3 ),
    NAV_GOAL_CANDIDATE_FLAG_SAME_CLUSTER_AS_RAW_GOAL = ( 1u << 4 ),
    NAV_GOAL_CANDIDATE_FLAG_SAME_AREA_AS_RAW_GOAL = ( 1u << 5 ),
    NAV_GOAL_CANDIDATE_FLAG_IN_FAT_PVS = ( 1u << 6 ),
    NAV_GOAL_CANDIDATE_FLAG_HAS_LADDER_BITS = ( 1u << 7 ),
    NAV_GOAL_CANDIDATE_FLAG_HAS_STAIR_BITS = ( 1u << 8 ),
    NAV_GOAL_CANDIDATE_FLAG_NEIGHBOR_SAMPLE = ( 1u << 9 )
};


/**
*
*
*	Nav2 Goal Candidate Data Structures:
*
*
**/
//! Maximum number of accepted goal candidates retained for one endpoint-selection request.
static constexpr int32_t NAV_GOAL_CANDIDATE_MAX_COUNT = 32;
//! Maximum number of rejected goal samples retained for one endpoint-selection request.
static constexpr int32_t NAV_GOAL_CANDIDATE_REJECTION_MAX_COUNT = 32;

/**
*	@brief	Stable metadata for one candidate goal endpoint.
*	@note	The structure intentionally stores indices, ids, and scalar metadata rather than pointers so it can
*			be logged, compared, or persisted later without redesign.
**/
struct nav2_goal_candidate_t {
    //! Stable candidate id within the local request.
    int32_t candidate_id = -1;
    //! Semantic source classification for the candidate.
    nav2_goal_candidate_type_t type = nav2_goal_candidate_type_t::None;
    //! Feet-origin world-space endpoint consumed by current path-query callers.
    Vector3 feet_origin = {};
    //! Nav-center world-space endpoint used for nav tile/cell and BSP lookups.
    Vector3 center_origin = {};
    //! Candidate tile X index when resolved against the world-tile grid.
    int32_t tile_x = 0;
    //! Candidate tile Y index when resolved against the world-tile grid.
    int32_t tile_y = 0;
    //! Canonical world-tile storage id when available.
    int32_t tile_id = -1;
    //! Tile-local cell index when available.
    int32_t cell_index = -1;
    //! Layer index inside the resolved cell when available.
    int32_t layer_index = -1;
    //! BSP leaf index backing the candidate position when available.
    int32_t leaf_index = -1;
    //! BSP cluster id backing the candidate position when available.
    int32_t cluster_id = -1;
    //! BSP area id backing the candidate position when available.
    int32_t area_id = -1;
    //! Relative sample offset in cells from the raw goal query.
    int32_t sample_dx = 0;
    //! Relative sample offset in cells from the raw goal query.
    int32_t sample_dy = 0;
    //! Sample radius in cells from the raw goal query.
    int32_t sample_radius = 0;
    //! Point contents sampled at the candidate nav-center location.
    cm_contents_t point_contents = CONTENTS_NONE;
    //! Aggregated box contents sampled across the candidate agent AABB.
    cm_contents_t box_contents = CONTENTS_NONE;
    //! Cheap lower-bound score used for ranking.
    double lower_bound_score = 0.0;
    //! Final ranking score used to choose the preferred candidate.
    double ranking_score = 0.0;
    //! Stable ranking flags recorded for diagnostics and future persistence-friendly state.
    uint32_t flags = NAV_GOAL_CANDIDATE_FLAG_NONE;
};

/**
*	@brief	One rejected candidate sample record kept for debug reporting.
*	@note	This keeps the rejection reason stable without storing heavyweight transient builder state.
**/
struct nav2_goal_candidate_rejection_info_t {
    //! Stable rejection category.
    nav2_goal_candidate_rejection_t reason = nav2_goal_candidate_rejection_t::None;
    //! Candidate semantic type attempted for the rejected sample.
    nav2_goal_candidate_type_t type = nav2_goal_candidate_type_t::None;
    //! Relative sample offset in cells from the raw goal query.
    int32_t sample_dx = 0;
    //! Relative sample offset in cells from the raw goal query.
    int32_t sample_dy = 0;
    //! Candidate tile X index when known.
    int32_t tile_x = 0;
    //! Candidate tile Y index when known.
    int32_t tile_y = 0;
    //! Canonical world-tile storage id when known.
    int32_t tile_id = -1;
    //! Tile-local cell index when known.
    int32_t cell_index = -1;
    //! Layer index inside the resolved cell when known.
    int32_t layer_index = -1;
};

/**
*	@brief	Fixed-size result buffer holding accepted and rejected endpoint samples.
*	@note	A fixed-capacity buffer keeps the stage allocation-free and worker-friendly while the API surface hardens.
**/
struct nav2_goal_candidate_list_t {
    //! Number of accepted candidates stored in `candidates`.
    int32_t candidate_count = 0;
    //! Number of rejected samples stored in `rejections`.
    int32_t rejection_count = 0;
    //! Accepted candidate records ordered by insertion, not ranking.
    nav2_goal_candidate_t candidates[ NAV_GOAL_CANDIDATE_MAX_COUNT ] = {};
    //! Rejected sample records recorded while building the candidate set.
    nav2_goal_candidate_rejection_info_t rejections[ NAV_GOAL_CANDIDATE_REJECTION_MAX_COUNT ] = {};
};


/**
*
*
*	Nav2 Goal Candidate Public API:
*
*
**/
/**
*	@brief	Build a BSP-aware candidate endpoint set for a start/goal query pair.
*	@param	mesh		Navigation mesh used for tile, layer, and world metadata lookup.
*	@param	start_origin	Feet-origin start point used for same-leaf/cluster ranking hints.
*	@param	goal_origin	Feet-origin raw goal point requested by gameplay code.
*	@param	agent_mins	Feet-origin navigation hull mins.
*	@param	agent_maxs	Feet-origin navigation hull maxs.
*	@param	out_candidates	[out] Fixed-capacity candidate buffer receiving accepted and rejected samples.
*	@return	True when at least one viable candidate was produced.
*	@note	This stage is intentionally low-risk: it only materializes and ranks candidate endpoints and does not yet
*			replace the downstream legacy path solver.
**/
const bool SVG_Nav2_BuildGoalCandidates( const nav2_mesh_t *mesh, const Vector3 &start_origin, const Vector3 &goal_origin,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, nav2_goal_candidate_list_t *out_candidates );

/**
*	@brief	Select the best-ranked candidate from a previously built candidate list.
*	@param	candidates	Accepted/rejected candidate list built for the current query.
*	@param	out_candidate	[out] Selected candidate record.
*	@return	True when a candidate was selected.
*	@note	Selection is deterministic: ties are broken by local candidate id order after ranking score.
**/
const bool SVG_Nav2_SelectBestGoalCandidate( const nav2_goal_candidate_list_t &candidates, nav2_goal_candidate_t *out_candidate );

/**
*	@brief	Resolve the preferred feet-origin goal endpoint for a start/goal query pair.
*	@param	mesh		Navigation mesh used for endpoint selection.
*	@param	start_origin	Feet-origin start point used for ranking hints.
*	@param	goal_origin	Feet-origin raw goal point requested by gameplay code.
*	@param	agent_mins	Feet-origin navigation hull mins.
*	@param	agent_maxs	Feet-origin navigation hull maxs.
*	@param	out_goal_origin	[out] Selected feet-origin endpoint.
*	@param	out_candidate	[out] Optional selected candidate metadata.
*	@param	out_candidates	[out] Optional accepted/rejected candidate list for debug or diagnostics.
*	@return	True when either the candidate builder or the legacy projection fallback produced an endpoint.
*	@note	This is the low-risk integration helper for current query consumers. It prefers ranked candidates first and
*			falls back to the existing legacy projection behavior only when needed.
**/
const bool SVG_Nav2_ResolveBestGoalCandidate( const nav2_mesh_t *mesh, const Vector3 &start_origin, const Vector3 &goal_origin,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, Vector3 *out_goal_origin,
    nav2_goal_candidate_t *out_candidate = nullptr, nav2_goal_candidate_list_t *out_candidates = nullptr );

/**
*	@brief	Emit a concise debug dump of generated candidates and recorded rejections.
*	@param	candidates	Candidate list to report.
*	@param	selected_candidate	Optional selected candidate to highlight in the dump.
*	@note	Callers are expected to gate this behind their own debug policy to avoid log spam.
**/
void SVG_Nav2_DebugPrintGoalCandidates( const nav2_goal_candidate_list_t &candidates, const nav2_goal_candidate_t *selected_candidate = nullptr );

/**
*	@brief	Keep the nav2 goal candidate module represented in the build while the staged implementation lives in the query seam.
*	@note	This anchor is temporary and can be removed once the implementation is extracted back into its own module.
**/
void SVG_Nav2_GoalCandidates_ModuleAnchor( void );
