/********************************************************************
*
*
*	ServerGame: Nav2 Distance-Derived Utility Fields
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_span_grid.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Distance Field Enumerations:
*
*
**/
/**
*	@brief	Flags describing which utility metrics are valid for one span-distance record.
*	@note	Task 12.1 keeps validity explicit so worker consumers and serializers do not infer
*			metric availability from sentinel floating-point values.
**/
enum nav2_distance_field_flag_t : uint32_t {
    //! No utility metric has been populated yet.
    NAV2_DISTANCE_FIELD_FLAG_NONE = 0,
    //! Goal-distance metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_GOAL_DISTANCE = ( 1u << 0 ),
    //! Corridor-segment distance metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_CORRIDOR_DISTANCE = ( 1u << 1 ),
    //! Nearest portal endpoint metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_PORTAL_DISTANCE = ( 1u << 2 ),
    //! Nearest stair connector metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_STAIR_DISTANCE = ( 1u << 3 ),
    //! Nearest ladder endpoint metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_LADDER_DISTANCE = ( 1u << 4 ),
    //! Nearest region-boundary metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_REGION_BOUNDARY_DISTANCE = ( 1u << 5 ),
    //! Roof-edge / ledge / drop metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_LEDGE_DISTANCE = ( 1u << 6 ),
    //! Open-space centerline proxy metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_CENTERLINE_DISTANCE = ( 1u << 7 ),
    //! Obstacle-boundary metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_OBSTACLE_DISTANCE = ( 1u << 8 ),
    //! Clearance-bottleneck metric is available.
    NAV2_DISTANCE_FIELD_FLAG_HAS_CLEARANCE_BOTTLENECK_DISTANCE = ( 1u << 9 ),
    //! Fat-PVS relevance bias metadata was evaluated for this span.
    NAV2_DISTANCE_FIELD_FLAG_HAS_FAT_PVS_RELEVANCE = ( 1u << 10 )
};


/**
*
*
*	Nav2 Distance Field Data Structures:
*
*
**/
/**
*	@brief	Build policy and tuning knobs for Task 12.1 distance-field generation.
*	@note	The settings remain pointer-free and serialization-friendly so static asset workflows can
*			persist policy metadata when caching utility fields.
**/
struct nav2_distance_field_build_params_t {
    //! Optional world-space goal anchor used by the goal-distance metric.
    Vector3 goal_origin = {};
    //! True when `goal_origin` should participate in utility-field generation.
    bool has_goal_origin = false;
    //! Maximum neighborhood radius (in sparse XY cells) used for region-boundary probing.
    int32_t boundary_search_radius_cells = 6;
    //! Maximum radial probe distance used for obstacle-boundary detection.
    double obstacle_probe_radius = 128.0;
    //! Incremental radial step used by obstacle-boundary probing.
    double obstacle_probe_step = 16.0;
    //! Clearance threshold below which a span is considered bottlenecked.
    double clearance_bottleneck_threshold = 72.0;
    //! Half-width estimate used when deriving open-space centerline deviation.
    double open_space_half_width = 64.0;
    //! True when optional fat-PVS relevance hints should be evaluated.
    bool use_fat_pvs_relevance = false;
    //! Point-of-interest used by optional fat-PVS relevance checks.
    Vector3 fat_pvs_origin = {};
    //! Bounding box minimum extents used by optional fat-PVS relevance checks.
    Vector3 fat_pvs_mins = { -16.0f, -16.0f, -16.0f };
    //! Bounding box maximum extents used by optional fat-PVS relevance checks.
    Vector3 fat_pvs_maxs = { 16.0f, 16.0f, 16.0f };
    //! Visibility mask used by optional fat-PVS checks.
    int32_t fat_pvs_vis = 0;
    //! Tie-breaker bonus applied when optional fat-PVS relevance confirms span relevance.
    double fat_pvs_relevance_bonus = 8.0;
};

/**
*	@brief	Per-span utility-field metrics derived from sparse topology and connector anchors.
*	@note	Record identity uses stable span and column indices so consumers can avoid pointer-only
*			references during worker publication and serialization.
**/
struct nav2_distance_field_record_t {
    //! Stable span id for this record.
    int32_t span_id = -1;
    //! Owning sparse-column index for this record.
    int32_t column_index = -1;
    //! Owning span index inside the sparse column.
    int32_t span_index = -1;
    //! Stable world-space representative point for this span.
    Vector3 span_world_origin = {};
    //! Utility-field validity flags.
    uint32_t flags = NAV2_DISTANCE_FIELD_FLAG_NONE;
    //! Distance from span origin to query goal.
    double distance_to_goal = 0.0;
    //! Distance from span origin to nearest active corridor segment.
    double distance_to_corridor_segment = 0.0;
    //! Distance from span origin to nearest portal connector anchor.
    double distance_to_portal_endpoint = 0.0;
    //! Distance from span origin to nearest stair connector anchor.
    double distance_to_stair_connector = 0.0;
    //! Distance from span origin to nearest ladder connector anchor.
    double distance_to_ladder_endpoint = 0.0;
    //! Distance from span origin to nearest region-boundary mismatch.
    double distance_to_region_boundary = 0.0;
    //! Distance from span origin to nearest ledge/drop style hazard cue.
    double distance_to_ledge_or_drop = 0.0;
    //! Distance from span origin to the local open-space centerline proxy.
    double distance_to_open_space_centerline = 0.0;
    //! Distance from span origin to nearest obstacle boundary.
    double distance_to_obstacle_boundary = 0.0;
    //! Distance from span origin to nearest clearance bottleneck threshold.
    double distance_to_clearance_bottleneck = 0.0;
    //! Stable connector id for the nearest portal endpoint when available.
    int32_t nearest_portal_connector_id = -1;
    //! Stable connector id for the nearest stair connector when available.
    int32_t nearest_stair_connector_id = -1;
    //! Stable connector id for the nearest ladder endpoint when available.
    int32_t nearest_ladder_connector_id = -1;
    //! Stable region-layer id mirrored from the source span when available.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Optional tie-breaker signal for heuristic ordering.
    double heuristic_tie_breaker = 0.0;
};

/**
*	@brief	Container for one built set of nav2 distance-derived utility fields.
**/
struct nav2_distance_field_set_t {
    //! Build parameters used for the currently stored metrics.
    nav2_distance_field_build_params_t params = {};
    //! Dense span metric records in deterministic order.
    std::vector<nav2_distance_field_record_t> records = {};
    //! Stable lookup from span id to `records` index.
    std::unordered_map<int32_t, int32_t> by_span_id = {};
};

/**
*	@brief	Bounded summary counters for one distance-field build pass.
*	@note	Task 12.1 diagnostics stay compact by reporting aggregate metric counts and ranges.
**/
struct nav2_distance_field_summary_t {
    //! Number of generated records.
    int32_t record_count = 0;
    //! Number of records with goal-distance metrics.
    int32_t with_goal_distance_count = 0;
    //! Number of records with corridor-distance metrics.
    int32_t with_corridor_distance_count = 0;
    //! Number of records with portal-distance metrics.
    int32_t with_portal_distance_count = 0;
    //! Number of records with stair-distance metrics.
    int32_t with_stair_distance_count = 0;
    //! Number of records with ladder-distance metrics.
    int32_t with_ladder_distance_count = 0;
    //! Number of records marked as fat-PVS relevant.
    int32_t with_fat_pvs_relevance_count = 0;
    //! Minimum goal distance across all records with valid goal-distance metrics.
    double min_goal_distance = 0.0;
    //! Maximum goal distance across all records with valid goal-distance metrics.
    double max_goal_distance = 0.0;
    //! Average obstacle-boundary distance across all records with valid obstacle metrics.
    double average_obstacle_distance = 0.0;
    //! Average clearance-bottleneck distance across all records with valid bottleneck metrics.
    double average_clearance_bottleneck_distance = 0.0;
};


/**
*
*
*	Nav2 Distance Field Public API:
*
*
**/
/**
*	@brief	Clear one distance-field set back to deterministic defaults.
*	@param	fields	[in,out] Distance-field set to clear.
**/
void SVG_Nav2_DistanceField_Clear( nav2_distance_field_set_t *fields );

/**
*	@brief	Build per-span distance-derived utility fields from sparse topology and connector data.
*	@param	grid	Sparse precision-layer span grid.
*	@param	connectors	Connector list providing portal/stair/ladder anchors.
*	@param	corridor	Optional corridor used for corridor-segment distance metrics.
*	@param	params	Build policy and tuning knobs.
*	@param	out_fields	[out] Distance-field set receiving generated metrics.
*	@param	out_summary	[out] Optional bounded summary counters.
*	@return	True when at least one span record was generated.
*	@note	This helper computes guidance metrics only; it must not bypass corridor correctness or connector commitments.
**/
const bool SVG_Nav2_BuildDistanceFields( const nav2_span_grid_t &grid, const nav2_connector_list_t &connectors, const nav2_corridor_t *corridor,
    const nav2_distance_field_build_params_t &params, nav2_distance_field_set_t *out_fields, nav2_distance_field_summary_t *out_summary = nullptr );

/**
*	@brief	Resolve one distance-field record by stable span id.
*	@param	fields	Distance-field set to inspect.
*	@param	span_id	Stable span id to resolve.
*	@return	Pointer to the matching record, or `nullptr` when unavailable.
**/
const nav2_distance_field_record_t *SVG_Nav2_DistanceField_TryResolve( const nav2_distance_field_set_t &fields, const int32_t span_id );

/**
*	@brief	Build a compact heuristic tie-breaker score from one distance-field record.
*	@param	record	Distance-field record to evaluate.
*	@param	out_score	[out] Composite score where lower is preferred.
*	@return	True when a score was produced.
*	@note	The score is guidance-only and must not replace legality checks in coarse/fine traversal.
**/
const bool SVG_Nav2_DistanceField_BuildTieBreakerScore( const nav2_distance_field_record_t &record, double *out_score );

/**
*	@brief	Emit a bounded debug summary for one built distance-field set.
*	@param	fields	Distance-field set to summarize.
*	@param	summary	Optional precomputed summary to print.
*	@param	limit	Maximum number of per-record lines to emit.
**/
void SVG_Nav2_DebugPrintDistanceFields( const nav2_distance_field_set_t &fields, const nav2_distance_field_summary_t *summary, const int32_t limit = 12 );

/**
*	@brief	Keep the nav2 distance-field module represented in the build.
**/
void SVG_Nav2_DistanceFields_ModuleAnchor( void );
