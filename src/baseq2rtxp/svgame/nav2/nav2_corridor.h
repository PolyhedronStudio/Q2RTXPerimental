/********************************************************************
*
*
*	ServerGame: Nav2 Corridor Commitments
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_query_iface.h"

#include <vector>


/**
*
*
*	Nav2 Corridor Enumerations:
*
*
**/
/**
*	@brief	Stable coarse source classification for a nav2 corridor.
*	@note	This mirrors the current legacy coarse sources while remaining future-compatible with
*			region-layer graph output and other staged corridor producers.
**/
enum class nav2_corridor_source_t : uint8_t {
    None = 0,
    Hierarchy,
    ClusterFallback,
    Count
};

/**
*	@brief	Stable segment taxonomy for persistence-friendly corridor commitments.
*	@note	Segment types remain explicit so later fine refinement, save/load continuity, and debug
*			visualization can reason about corridor intent without relying on transient legacy internals.
**/
enum class nav2_corridor_segment_type_t : uint8_t {
    None = 0,
    RegionBand,
    PortalTransition,
    TileSeed,
    LadderTransition,
    StairBand,
    MoverBoarding,
    MoverRide,
    MoverExit,
    GoalEndpoint,
    Count
};

/**
*	@brief	Stable top-level corridor flags describing what commitments are present.
**/
enum nav2_corridor_flag_t : uint32_t {
    NAV_CORRIDOR_FLAG_NONE = 0,
    NAV_CORRIDOR_FLAG_HAS_REGION_PATH = ( 1u << 0 ),
    NAV_CORRIDOR_FLAG_HAS_PORTAL_PATH = ( 1u << 1 ),
    NAV_CORRIDOR_FLAG_HAS_EXACT_TILE_ROUTE = ( 1u << 2 ),
    NAV_CORRIDOR_FLAG_HAS_GLOBAL_Z_BAND = ( 1u << 3 ),
    NAV_CORRIDOR_FLAG_PARTIAL_RESULT = ( 1u << 4 ),
    NAV_CORRIDOR_FLAG_HIERARCHY_DERIVED = ( 1u << 5 ),
    NAV_CORRIDOR_FLAG_CLUSTER_FALLBACK = ( 1u << 6 ),
    NAV_CORRIDOR_FLAG_HAS_REFINEMENT_POLICY = ( 1u << 7 ),
    NAV_CORRIDOR_FLAG_HAS_LADDER_COMMITMENT = ( 1u << 8 ),
    NAV_CORRIDOR_FLAG_HAS_STAIR_COMMITMENT = ( 1u << 9 ),
    NAV_CORRIDOR_FLAG_HAS_MOVER_COMMITMENT = ( 1u << 10 )
};

/**
*	@brief	Stable per-segment flags describing attached topology and endpoint semantics.
**/
enum nav2_corridor_segment_flag_t : uint32_t {
    NAV_CORRIDOR_SEGMENT_FLAG_NONE = 0,
    NAV_CORRIDOR_SEGMENT_FLAG_START_ENDPOINT = ( 1u << 0 ),
    NAV_CORRIDOR_SEGMENT_FLAG_GOAL_ENDPOINT = ( 1u << 1 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND = ( 1u << 2 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID = ( 1u << 3 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID = ( 1u << 4 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_TILE_REF = ( 1u << 5 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_LEAF_MEMBERSHIP = ( 1u << 6 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_CLUSTER_MEMBERSHIP = ( 1u << 7 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_AREA_MEMBERSHIP = ( 1u << 8 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES = ( 1u << 9 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES = ( 1u << 10 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT = ( 1u << 11 ),
    NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF = ( 1u << 12 )
};

/**
*	@brief	Stable refinement-policy flags carried by a nav2 corridor.
*	@note	These flags are nav2-owned and persistence-friendly so Task 3.2 can describe
*			future fine-search rules without modifying `oldnav`.
**/
enum nav2_corridor_refine_policy_flag_t : uint32_t {
    NAV_CORRIDOR_REFINE_POLICY_FLAG_NONE = 0,
    NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_LEAF_MEMBERSHIP = ( 1u << 0 ),
    NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_CLUSTER_MEMBERSHIP = ( 1u << 1 ),
    NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_AREA_MEMBERSHIP = ( 1u << 2 ),
    NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_OUT_OF_BAND = ( 1u << 3 ),
    NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_TOPOLOGY_MISMATCH = ( 1u << 4 ),
    NAV_CORRIDOR_REFINE_POLICY_FLAG_PENALIZE_TOPOLOGY_MISMATCH = ( 1u << 5 ),
    NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT = ( 1u << 6 )
};


/**
*
*
*	Nav2 Corridor Data Structures:
*
*
**/
/**
*	@brief	Persistence-friendly Z-band commitment for a corridor or segment.
*	@note	The band is intentionally explicit so future fine refinement can reject out-of-band
*			expansions without rediscovering vertical intent from scratch.
**/
struct nav2_corridor_z_band_t {
    //! Minimum committed Z height in world units.
    double min_z = 0.0;
    //! Maximum committed Z height in world units.
    double max_z = 0.0;
    //! Preferred representative Z height inside the committed band.
    double preferred_z = 0.0;

    /**
    *	@brief	Return true when the Z band contains a valid numeric range.
    *	@return	True when `min_z <= max_z`.
    **/
    const bool IsValid() const {
        return min_z <= max_z;
    }
};

/**
*	@brief	Stable tile reference used by explicit tile-seed corridor guidance.
**/
struct nav2_corridor_tile_ref_t {
    //! Tile X coordinate in the world tile grid.
    int32_t tile_x = 0;
    //! Tile Y coordinate in the world tile grid.
    int32_t tile_y = 0;
    //! Canonical world-tile storage id when available.
    int32_t tile_id = -1;
};

/**
*	@brief	Stable topology reference attached to a corridor endpoint or segment.
*	@note	These fields are scalar IDs only so the structure stays persistence-friendly and safe
*			for worker publication.
**/
struct nav2_corridor_topology_ref_t {
    //! BSP leaf id when known.
    int32_t leaf_index = -1;
    //! BSP cluster id when known.
    int32_t cluster_id = -1;
    //! BSP area id when known.
    int32_t area_id = -1;
    //! Coarse hierarchy region id when known.
    int32_t region_id = NAV_REGION_ID_NONE;
    //! Coarse hierarchy portal id when known.
    int32_t portal_id = NAV_PORTAL_ID_NONE;
    //! Canonical world-tile storage id when known.
    int32_t tile_id = -1;
    //! World tile X coordinate when known.
    int32_t tile_x = 0;
    //! World tile Y coordinate when known.
    int32_t tile_y = 0;
    //! Tile-local cell index when known.
    int32_t cell_index = -1;
    //! Tile-local layer index when known.
    int32_t layer_index = -1;
};

/**
*	@brief	Stable mover reference attached to a corridor commitment when known.
*	@note	This stays pointer-free so mover-related path state remains save/load friendly.
**/
struct nav2_corridor_mover_ref_t {
    //! Owning entity number for the mover when known.
    int32_t owner_entnum = -1;
    //! Inline-model index when known.
    int32_t model_index = -1;

    /**
    *	@brief	Return true when this mover reference identifies a concrete runtime entity.
    *	@return	True when `owner_entnum` or `model_index` is valid.
    **/
    const bool IsValid() const {
        return owner_entnum >= 0 || model_index >= 0;
    }
};

/**
 * 	@brief	Stable refinement-policy metadata owned by nav2 corridor state.
 * 	@note	This lets nav2 publish commitment intent without mutating `oldnav` refinement structures.
**/
struct nav2_corridor_refine_policy_t {
    //! Top-level refinement commitment flags published by nav2.
    uint32_t flags = NAV_CORRIDOR_REFINE_POLICY_FLAG_NONE;
    //! Soft penalty applied when refinement drifts outside preferred topology membership.
    double topology_mismatch_penalty = 0.0;
    //! Soft penalty applied when refinement drifts away from mover-aware commitments.
    double mover_state_penalty = 0.0;
    //! Stable allowed BSP leaf memberships for incremental fine-search filtering.
    std::vector<int32_t> allowed_leaf_indices = {};
    //! Stable allowed BSP cluster memberships for incremental fine-search filtering.
    std::vector<int32_t> allowed_cluster_ids = {};
    //! Stable allowed BSP area memberships for incremental fine-search filtering.
    std::vector<int32_t> allowed_area_ids = {};
    //! Stable mover references touched by the corridor when known.
    std::vector<nav2_corridor_mover_ref_t> mover_refs = {};
};

/**
*	@brief	One explicit corridor commitment segment used by future refinement and diagnostics.
**/
struct nav2_corridor_segment_t {
    //! Stable local segment id within the owning corridor.
    int32_t segment_id = -1;
    //! Semantic type of this commitment segment.
    nav2_corridor_segment_type_t type = nav2_corridor_segment_type_t::None;
    //! Stable segment flags describing attached commitments.
    uint32_t flags = NAV_CORRIDOR_SEGMENT_FLAG_NONE;
    //! Start anchor for this segment when known.
    Vector3 start_anchor = {};
    //! End anchor for this segment when known.
    Vector3 end_anchor = {};
    //! Allowed Z-band for this segment when known.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Topology metadata attached to this segment.
    nav2_corridor_topology_ref_t topology = {};
  //! Traversal-oriented node feature bits attached to this segment when known.
    uint32_t traversal_feature_bits = NAV_TRAVERSAL_FEATURE_NONE;
    //! Traversal-oriented edge feature bits attached to this segment when known.
    uint32_t edge_feature_bits = NAV_EDGE_FEATURE_NONE;
    //! Ladder endpoint bits attached to this segment when known.
    uint8_t ladder_endpoint_flags = NAV_LADDER_ENDPOINT_NONE;
    //! Stable mover reference attached to this segment when known.
    nav2_corridor_mover_ref_t mover_ref = {};
};

/**
*	@brief	Persistence-friendly nav2 corridor description produced by staged coarse routing.
*	@note	This wraps the current legacy refine corridor while adding explicit Z-band and segment
*			commitments so future fine refinement can adopt it incrementally.
**/
struct nav2_corridor_t {
    //! Coarse source that produced this corridor.
    nav2_corridor_source_t source = nav2_corridor_source_t::None;
    //! Stable top-level corridor flags.
    uint32_t flags = NAV_CORRIDOR_FLAG_NONE;
    //! Coarse expansion count reported while building the corridor.
    int32_t coarse_expansion_count = 0;
    //! Number of local portal overlay rejections encountered while building the corridor.
    int32_t overlay_block_count = 0;
    //! Number of local portal overlay penalties encountered while building the corridor.
    int32_t overlay_penalty_count = 0;
    //! Topology metadata for the corridor start endpoint.
    nav2_corridor_topology_ref_t start_topology = {};
    //! Topology metadata for the corridor goal endpoint.
    nav2_corridor_topology_ref_t goal_topology = {};
    //! Global allowed Z-band for the corridor.
    nav2_corridor_z_band_t global_z_band = {};
    //! Ordered compact region path when hierarchy routing produced this corridor.
    std::vector<int32_t> region_path = {};
    //! Ordered compact portal path when hierarchy routing produced this corridor.
    std::vector<int32_t> portal_path = {};
    //! Ordered exact tile route used as the seed for buffered fine-search restriction.
    std::vector<nav2_corridor_tile_ref_t> exact_tile_route = {};
    //! Explicit segment list carrying route commitments in persistence-friendly form.
    std::vector<nav2_corridor_segment_t> segments = {};
    //! Incremental refinement-policy commitments mirrored into the current legacy seam.
    nav2_corridor_refine_policy_t refine_policy = {};

    /**
    *	@brief	Return true when the corridor contains exact tile guidance.
    *	@return	True when `exact_tile_route` is not empty.
    **/
    const bool HasExactTileRoute() const {
        return !exact_tile_route.empty();
    }

    /**
    *	@brief	Return true when the corridor contains explicit commitment segments.
    *	@return	True when `segments` is not empty.
    **/
    const bool HasSegments() const {
        return !segments.empty();
    }
};


/**
*
*
*	Nav2 Corridor Public API:
*
*
**/
/**
 *	@brief	Build a nav2 corridor wrapper from the staged compatibility-layer coarse corridor result.
 *	@param	mesh		Navigation mesh used to resolve canonical tile metadata.
 *	@param	start_node	Canonical start node used to seed endpoint topology and Z-band commitments.
 *	@param	goal_node	Canonical goal node used to seed endpoint topology and Z-band commitments.
 *	@param	coarse_corridor	Compatibility-layer coarse corridor result to mirror into nav2.
 *	@param	coarse_expansions	Coarse expansion count reported by the compatibility layer.
*	@param	out_corridor	[out] Persistence-friendly nav2 corridor description.
 *	@return	True when the staged coarse corridor was translated successfully.
 *	@note	This is the low-risk Task 3.2 adapter that lets current compatibility callers keep using the
    *			staged bridge while nav2 publishes explicit corridor commitments through nav2-owned types.
**/
const bool SVG_Nav2_BuildCorridorFromQueryCoarseCorridor( const nav2_query_mesh_t *mesh, const nav2_query_node_t &start_node,
    const nav2_query_node_t &goal_node, const nav2_query_coarse_corridor_t &coarse_corridor, const int32_t coarse_expansions,
    nav2_corridor_t *out_corridor );

/**
*	@brief	Return a short debug label for a corridor source classification.
*	@param	source	Corridor source classification to stringify.
*	@return	Stable debug label for the requested corridor source.
**/
const char *SVG_Nav2_CorridorSourceName( const nav2_corridor_source_t source );

/**
*	@brief	Return a short debug label for a corridor segment type.
*	@param	type	Corridor segment type to stringify.
*	@return	Stable debug label for the requested segment type.
**/
const char *SVG_Nav2_CorridorSegmentTypeName( const nav2_corridor_segment_type_t source );

/**
*	@brief	Emit a concise debug dump of the explicit nav2 corridor commitments.
*	@param	corridor	Corridor to report.
*	@note	Callers are expected to gate this behind their own debug policy to avoid log spam.
**/
void SVG_Nav2_DebugPrintCorridor( const nav2_corridor_t &corridor );

/**
*	@brief	Build a nav2 corridor directly from world-space endpoints through the legacy coarse builder.
*	@param	mesh		Navigation mesh used for endpoint lookup and legacy corridor generation.
*	@param	start_origin	Feet-origin start point for the corridor query.
*	@param	goal_origin	Feet-origin goal point for the corridor query.
*	@param	policy		Optional traversal policy used to gate legacy coarse transitions.
*	@param	agent_mins	Agent bounds minimums used for feet-to-center conversion.
*	@param	agent_maxs	Agent bounds maximums used for feet-to-center conversion.
*	@param	out_corridor	[out] Persistence-friendly nav2 corridor description.
*	@return	True when the legacy coarse builder produced a corridor and the nav2 wrapper mirrored it.
*	@note	This is the low-risk compatibility seam for current refinement consumers while oldnav still
*			owns the actual coarse routing and fine-search enforcement.
**/
const bool SVG_Nav2_BuildCorridorForEndpoints( const nav2_query_mesh_t *mesh, const Vector3 &start_origin, const Vector3 &goal_origin,
    const nav2_query_policy_t *policy, const Vector3 &agent_mins, const Vector3 &agent_maxs, nav2_corridor_t *out_corridor );
