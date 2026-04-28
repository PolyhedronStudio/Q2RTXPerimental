/********************************************************************
*
*
*	ServerGame: Nav2 Span Adjacency
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_span_grid.h"

#include <vector>


/**
*	@brief Pointer-free movement classification for one local span adjacency edge.
*	@note These classes keep fine-search semantics explicit without depending on runtime pointers or legacy edge types.
**/
enum class nav2_span_edge_movement_t : uint8_t {
    //! No movement class has been assigned yet.
    None = 0,
    //! Same-band horizontal movement with no meaningful vertical step.
    HorizontalPass,
    //! Controlled step-up movement within the configured stair-step range.
    StepUp,
    //! Controlled step-down movement within the configured stair-step range.
    StepDown,
    //! Downward traversal that remains legal only as a deliberate walk-off or drop.
    ControlledDrop,
    //! Transition enters or remains inside liquid traversal space.
    LiquidTransition,
    Count
};

/**
*	@brief	Explicit legality classification for one local span adjacency edge.
*	@note	This separates hard-invalid transitions from passable-but-penalized transitions so later fine search can make cleaner policy decisions.
**/
enum class nav2_span_edge_legality_t : uint8_t {
    //! No legality has been assigned yet.
    None = 0,
    //! Transition is valid without any special caution beyond its regular edge cost.
    Passable,
    //! Transition remains valid but should incur an explicit soft penalty.
    SoftPenalized,
    //! Transition is not legal under the current conservative builder rules.
    HardInvalid,
    Count
};

/**
*	@brief Bitflags describing legality and feature metadata for one span adjacency edge.
*	@note These flags mirror the movement-policy concepts that later fine A* and corridor logic will consume.
**/
enum nav2_span_edge_flags_t : uint32_t {
    //! No flags set.
    NAV2_SPAN_EDGE_FLAG_NONE = 0,
    //! Destination span is considered reachable under the current conservative builder rules.
    NAV2_SPAN_EDGE_FLAG_PASSABLE = ( 1u << 0 ),
    //! Edge rises upward by a stair-step amount.
    NAV2_SPAN_EDGE_FLAG_STEP_UP = ( 1u << 1 ),
    //! Edge drops downward by a stair-step amount.
    NAV2_SPAN_EDGE_FLAG_STEP_DOWN = ( 1u << 2 ),
    //! Edge requires a deliberate walk-off / drop style transition.
    NAV2_SPAN_EDGE_FLAG_CONTROLLED_DROP = ( 1u << 3 ),
    //! Edge enters water traversal space.
    NAV2_SPAN_EDGE_FLAG_ENTERS_WATER = ( 1u << 4 ),
    //! Edge enters lava traversal space.
    NAV2_SPAN_EDGE_FLAG_ENTERS_LAVA = ( 1u << 5 ),
    //! Edge enters slime traversal space.
    NAV2_SPAN_EDGE_FLAG_ENTERS_SLIME = ( 1u << 6 ),
    //! Edge crosses between different BSP leaves.
    NAV2_SPAN_EDGE_FLAG_CROSSES_LEAF = ( 1u << 7 ),
    //! Edge crosses between different BSP clusters.
    NAV2_SPAN_EDGE_FLAG_CROSSES_CLUSTER = ( 1u << 8 ),
    //! Edge crosses between different BSP areas.
    NAV2_SPAN_EDGE_FLAG_CROSSES_AREA = ( 1u << 9 ),
    //! Edge exists only through diagonal XY adjacency.
    NAV2_SPAN_EDGE_FLAG_DIAGONAL = ( 1u << 10 ),
    //! Edge should be treated as an optional walk-off rather than default movement.
    NAV2_SPAN_EDGE_FLAG_OPTIONAL_WALK_OFF = ( 1u << 11 ),
    //! Edge should be treated as forbidden walk-off unless policy explicitly overrides it.
   NAV2_SPAN_EDGE_FLAG_FORBIDDEN_WALK_OFF = ( 1u << 12 ),
    //! Source span sits near a ledge or unsupported edge in the evaluated travel direction.
    NAV2_SPAN_EDGE_FLAG_SOURCE_LEDGE_ADJACENT = ( 1u << 13 ),
    //! Destination span sits near a ledge or unsupported edge in the evaluated travel direction.
    NAV2_SPAN_EDGE_FLAG_DEST_LEDGE_ADJACENT = ( 1u << 14 ),
    //! Source span is close to a wall-like boundary in the evaluated travel direction.
    NAV2_SPAN_EDGE_FLAG_SOURCE_WALL_ADJACENT = ( 1u << 15 ),
    //! Destination span is close to a wall-like boundary in the evaluated travel direction.
    NAV2_SPAN_EDGE_FLAG_DEST_WALL_ADJACENT = ( 1u << 16 ),
    //! Transition should carry an explicit soft-penalty bias even though it remains traversable.
    NAV2_SPAN_EDGE_FLAG_SOFT_PENALIZED = ( 1u << 17 ),
    //! Transition has been classified as hard-invalid by the current builder rules.
    NAV2_SPAN_EDGE_FLAG_HARD_INVALID = ( 1u << 18 )
};

/**
*	@brief	One adjacency edge between two span-grid spans.
**/
struct nav2_span_edge_t {
    //! Stable edge id within the owning builder output.
    int32_t edge_id = -1;
    //! Source span id.
    int32_t from_span_id = -1;
    //! Destination span id.
    int32_t to_span_id = -1;
    //! Source sparse-column index.
    int32_t from_column_index = -1;
    //! Destination sparse-column index.
    int32_t to_column_index = -1;
    //! Source span index within the source sparse column.
    int32_t from_span_index = -1;
    //! Destination span index within the destination sparse column.
    int32_t to_span_index = -1;
    //! Signed X offset in sparse-column coordinates.
    int32_t delta_x = 0;
    //! Signed Y offset in sparse-column coordinates.
    int32_t delta_y = 0;
    //! Signed preferred-height delta from source span to destination span.
    double vertical_delta = 0.0;
    //! Edge cost used by later fine search.
    double cost = 0.0;
   //! Additional soft penalty applied on top of the base movement cost.
    double soft_penalty_cost = 0.0;
   //! Conservative movement class for this edge.
    nav2_span_edge_movement_t movement = nav2_span_edge_movement_t::None;
    //! Explicit legality classification for this edge.
    nav2_span_edge_legality_t legality = nav2_span_edge_legality_t::None;
    //! Fine-search feature mask mirrored from the underlying span relationship.
    uint32_t feature_bits = NAV_EDGE_FEATURE_NONE;
    //! Hard/soft legality flags.
    uint32_t flags = 0;
};

/**
*	@brief	Adjacency result for one span grid.
**/
struct nav2_span_adjacency_t {
    //! Generated edges in deterministic order.
    std::vector<nav2_span_edge_t> edges = {};
};

/**
*	@brief	Bounded summary statistics describing one built adjacency result.
*	@note	This keeps Task 4.3 validation cheap and explicit without requiring per-edge log spam.
**/
struct nav2_span_adjacency_summary_t {
    //! Total generated edges.
    int32_t edge_count = 0;
    //! Passable edges.
    int32_t passable_count = 0;
    //! Soft-penalized edges.
    int32_t soft_penalized_count = 0;
    //! Hard-invalid edges.
    int32_t hard_invalid_count = 0;
    //! Edges produced by explicit trace-based hard invalidation.
    int32_t trace_invalidated_count = 0;
    //! Edges produced by explicit contents-based hard invalidation.
    int32_t contents_invalidated_count = 0;
    //! Edges tagged as controlled drops.
    int32_t controlled_drop_count = 0;
    //! Edges tagged as liquid transitions.
    int32_t liquid_transition_count = 0;
};

/**
 *	@brief	Agent-aware policy used while deriving local span adjacency legality.
 *	@note	This mirrors the same movement-facing constraints that live monster steering cares about,
 *		so span adjacency can validate transitions against the requesting mover's real hull and
 *		step/drop limits instead of assuming one hard-coded default stand box.
 **/
struct nav2_span_adjacency_policy_t {
    //! Agent bounding-box minimum extents in local origin space.
    Vector3 agent_mins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
    //! Agent bounding-box maximum extents in local origin space.
    Vector3 agent_maxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
    //! Collision mask used by transition legality probes.
    cm_contents_t collision_mask = CM_CONTENTMASK_MONSTERSOLID;
    //! Minimum walkable slope normal used by the requesting movement policy.
    double min_step_normal = PHYS_MAX_SLOPE_NORMAL;
    //! Minimum vertical delta considered a meaningful stair step.
    double min_step_height = PHYS_STEP_MIN_SIZE;
    //! Maximum vertical delta that remains a direct step rather than a larger traversal.
    double max_step_height = PHYS_STEP_MAX_SIZE;
    //! Maximum downward drop height allowed for controlled walk-off edges.
    double max_drop_height = NAV_DEFAULT_MAX_DROP_HEIGHT;
    //! Optional additional downward cap used when rejecting large drops.
    double max_drop_height_cap = NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;
    //! True when the drop cap should clamp the effective controlled-drop distance.
    bool enable_max_drop_height_cap = true;

    /**
     *	@brief	Return whether the authored hull bounds describe a usable collision volume.
     *	@return	True when every max extent exceeds the corresponding min extent.
     **/
    const bool HasAgentBounds() const {
        return agent_maxs.x > agent_mins.x
            && agent_maxs.y > agent_mins.y
            && agent_maxs.z > agent_mins.z;
    }

    /**
     *	@brief	Resolve the effective controlled-drop limit after optional cap application.
     *	@return	Conservative maximum downward traversal depth.
     **/
    const double EffectiveMaxDropHeight() const {
        if ( enable_max_drop_height_cap && max_drop_height_cap > 0.0 ) {
            return std::min( max_drop_height, max_drop_height_cap );
        }
        return max_drop_height;
    }
};

/**
*	@brief	Build local span adjacency from a sparse span grid.
*	@param	grid	Sparse span grid to analyze.
*	@param	out_adjacency	[out] Adjacency output.
*	@return	True when adjacency was built successfully.
**/
const bool SVG_Nav2_BuildSpanAdjacency( const nav2_span_grid_t &grid, nav2_span_adjacency_t *out_adjacency );

/**
 *	@brief	Build local span adjacency from a sparse span grid using an explicit movement policy.
 *	@param	grid	Sparse span grid to analyze.
 *	@param	policy	Agent-aware hull and traversal thresholds used for legality classification.
 *	@param	out_adjacency	[out] Adjacency output.
 *	@return	True when adjacency was built successfully.
 **/
const bool SVG_Nav2_BuildSpanAdjacency( const nav2_span_grid_t &grid, const nav2_span_adjacency_policy_t &policy, nav2_span_adjacency_t *out_adjacency );

/**
*	@brief	Build a bounded summary for one adjacency result.
*	@param	adjacency	Adjacency output to summarize.
*	@param	out_summary	[out] Receives the compact summary statistics.
*	@return	True when the summary was produced.
**/
const bool SVG_Nav2_BuildSpanAdjacencySummary( const nav2_span_adjacency_t &adjacency, nav2_span_adjacency_summary_t *out_summary );
