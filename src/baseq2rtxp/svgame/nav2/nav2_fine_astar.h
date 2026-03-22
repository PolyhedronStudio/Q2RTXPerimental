/********************************************************************
*
*
*	ServerGame: Nav2 Corridor-Constrained Fine A*
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_corridor_build.h"
#include "svgame/nav2/nav2_span_adjacency.h"

#include <vector>


/**
*
*
*	Nav2 Fine A* Enumerations:
*
*
**/
/**
* @brief Stable corridor-local fine search status.
**/
enum class nav2_fine_astar_status_t : uint8_t {
    None = 0,
    Pending,
    Running,
    Partial,
    Success,
    Failed,
    Cancelled,
    Stale,
    Count
};

/**
* @brief Stable node kind for corridor-constrained fine search.
**/
enum class nav2_fine_astar_node_kind_t : uint8_t {
    None = 0,
    Start,
    Goal,
    Span,
    Connector,
    MoverAnchor,
    Fallback,
    Count
};

/**
* @brief Stable edge kind for corridor-constrained fine search.
**/
enum class nav2_fine_astar_edge_kind_t : uint8_t {
    None = 0,
    SpanLink,
    ConnectorLink,
    MoverLink,
    FallbackLink,
    Count
};

/**
* @brief Flags describing one fine-search node.
**/
enum nav2_fine_astar_node_flag_t : uint32_t {
    NAV2_FINE_ASTAR_NODE_FLAG_NONE = 0,
    NAV2_FINE_ASTAR_NODE_FLAG_HAS_SPAN = ( 1u << 0 ),
    NAV2_FINE_ASTAR_NODE_FLAG_HAS_CONNECTOR = ( 1u << 1 ),
    NAV2_FINE_ASTAR_NODE_FLAG_HAS_MOVER = ( 1u << 2 ),
    NAV2_FINE_ASTAR_NODE_FLAG_HAS_ALLOWED_Z_BAND = ( 1u << 3 ),
    NAV2_FINE_ASTAR_NODE_FLAG_PARTIAL = ( 1u << 4 ),
    NAV2_FINE_ASTAR_NODE_FLAG_PREFERRED = ( 1u << 5 )
};

/**
* @brief Flags describing one fine-search edge.
**/
enum nav2_fine_astar_edge_flag_t : uint32_t {
    NAV2_FINE_ASTAR_EDGE_FLAG_NONE = 0,
    NAV2_FINE_ASTAR_EDGE_FLAG_PASSABLE = ( 1u << 0 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_REQUIRES_PORTAL = ( 1u << 1 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_REQUIRES_LADDER = ( 1u << 2 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_REQUIRES_STAIR = ( 1u << 3 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_REQUIRES_MOVER = ( 1u << 4 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_ALLOWS_BOARDING = ( 1u << 5 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_ALLOWS_RIDE = ( 1u << 6 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_ALLOWS_EXIT = ( 1u << 7 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_TOPOLOGY_MATCHED = ( 1u << 8 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_TOPOLOGY_PENALIZED = ( 1u << 9 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_HARD_INVALID = ( 1u << 10 ),
    NAV2_FINE_ASTAR_EDGE_FLAG_PARTIAL = ( 1u << 11 )
};


/**
*
*
*	Nav2 Fine A* Data Structures:
*
*
**/
/**
* @brief Stable reference to one fine frontier node.
**/
struct nav2_fine_astar_node_ref_t {
    //! Stable node id.
    int32_t node_id = -1;
    //! Stable node index.
    int32_t node_index = -1;

    /**
    * @brief Return whether this reference points to a concrete frontier node.
    * @return True when the id and index are both valid.
    **/
    const bool IsValid() const {
        return node_id >= 0 && node_index >= 0;
    }
};

/**
* @brief One corridor-local fine-search node.
**/
struct nav2_fine_astar_node_t {
    //! Stable node id.
    int32_t node_id = -1;
    //! Semantic kind of the node.
    nav2_fine_astar_node_kind_t kind = nav2_fine_astar_node_kind_t::None;
    //! Stable span id when this node mirrors a precision-layer span.
    int32_t span_id = -1;
    //! Stable span-column index when known.
    int32_t column_index = -1;
    //! Stable span index inside the source column when known.
    int32_t span_index = -1;
    //! Stable connector id when known.
    int32_t connector_id = -1;
    //! Stable mover entity number when known.
    int32_t mover_entnum = -1;
    //! Stable inline model index when known.
    int32_t inline_model_index = -1;
    //! Stable node flags.
    uint32_t flags = NAV2_FINE_ASTAR_NODE_FLAG_NONE;
    //! Allowed Z band for this node.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Corridor topology metadata for this node.
    nav2_corridor_topology_ref_t topology = {};
    //! G-score accumulated so far.
    double g_score = 0.0;
    //! Heuristic score to the goal.
    double h_score = 0.0;
    //! F-score used by the frontier.
    double f_score = 0.0;
    //! Parent node id used for path reconstruction.
    int32_t parent_node_id = -1;
    //! Incoming edge id used for path reconstruction.
    int32_t parent_edge_id = -1;
};

/**
* @brief One corridor-local fine-search edge.
**/
struct nav2_fine_astar_edge_t {
    //! Stable edge id.
    int32_t edge_id = -1;
    //! Source node id.
    int32_t from_node_id = -1;
    //! Destination node id.
    int32_t to_node_id = -1;
    //! Semantic kind of the edge.
    nav2_fine_astar_edge_kind_t kind = nav2_fine_astar_edge_kind_t::None;
    //! Stable edge flags.
    uint32_t flags = NAV2_FINE_ASTAR_EDGE_FLAG_NONE;
    //! Base traversal cost.
    double base_cost = 0.0;
    //! Additional penalty attached by the fine solver.
    double topology_penalty = 0.0;
    //! Allowed Z band for the edge.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Stable connector id when known.
    int32_t connector_id = -1;
    //! Stable mover reference when known.
    nav2_corridor_mover_ref_t mover_ref = {};
};

/**
* @brief Fine-search path buffer.
**/
struct nav2_fine_astar_path_t {
    //! Nodes in start-to-goal order.
    std::vector<int32_t> node_ids = {};
    //! Edges in start-to-goal order.
    std::vector<int32_t> edge_ids = {};
};

/**
* @brief Fine-search diagnostics accumulated while advancing the solver.
**/
struct nav2_fine_astar_diagnostics_t {
    //! Number of nodes expanded.
    uint32_t expansions = 0;
    //! Number of slices consumed.
    uint32_t slices_consumed = 0;
    //! Number of times the search was paused.
    uint32_t pauses = 0;
    //! Number of times the search was resumed.
    uint32_t resumes = 0;
    //! Number of nodes pruned by corridor Z-band mismatch.
    uint32_t z_prunes = 0;
    //! Number of nodes pruned by topology mismatch.
    uint32_t topology_prunes = 0;
    //! Number of nodes pruned by mover mismatch.
    uint32_t mover_prunes = 0;
    //! Number of nodes pruned by hard invalidity.
    uint32_t hard_invalid_prunes = 0;
};

/**
* @brief Resumable corridor-constrained fine-search state.
**/
struct nav2_fine_astar_state_t {
    //! Stable solver status.
    nav2_fine_astar_status_t status = nav2_fine_astar_status_t::None;
    //! Scheduler query state used to bind budget and snapshots.
    nav2_query_state_t query_state = {};
    //! Corridor being refined.
    nav2_corridor_t corridor = {};
    //! Fine-search start node.
    nav2_fine_astar_node_t start_node = {};
    //! Fine-search goal node.
    nav2_fine_astar_node_t goal_node = {};
    //! Frontier nodes available for expansion.
    std::vector<nav2_fine_astar_node_t> nodes = {};
    //! Stored frontier edges.
    std::vector<nav2_fine_astar_edge_t> edges = {};
    //! Reconstructed path when the solver succeeds.
    nav2_fine_astar_path_t path = {};
    //! Search diagnostics accumulated over time.
    nav2_fine_astar_diagnostics_t diagnostics = {};
    //! Stable identifier for this solver instance.
    uint64_t solver_id = 0;
    //! True when the solver currently owns a frontier slice.
    bool has_frontier_slice = false;
};

/**
* @brief Compact result wrapper returned when the fine solver completes or pauses.
**/
struct nav2_fine_astar_result_t {
    //! Search status after the latest solver step.
    nav2_fine_astar_status_t status = nav2_fine_astar_status_t::None;
    //! True when a path was reconstructed.
    bool has_path = false;
    //! Stable path reconstructed from the solver state.
    nav2_fine_astar_path_t path = {};
    //! Diagnostics snapshot at the time of the result.
    nav2_fine_astar_diagnostics_t diagnostics = {};
};


/**
*
*
*	Nav2 Fine A* Public API:
*
*
**/
/**
* @brief Reset a fine A* solver state.
* @param state [out] Solver state to reset.
**/
void SVG_Nav2_FineAStar_Reset( nav2_fine_astar_state_t *state );

/**
* @brief Initialize a fine A* solver from a corridor and its coarse state.
* @param state [out] Solver state to initialize.
* @param corridor Corridor to refine.
* @param solverId Stable solver identifier.
* @return True when initialization succeeded.
**/
const bool SVG_Nav2_FineAStar_Init( nav2_fine_astar_state_t *state, const nav2_corridor_t &corridor, const uint64_t solverId );

/**
* @brief Advance a fine A* solver by one budgeted slice.
* @param state Solver state to advance.
* @param budgetSlice Budget slice controlling this step.
* @param out_result [out] Optional result snapshot.
* @return True when the solver made progress or completed.
**/
const bool SVG_Nav2_FineAStar_Step( nav2_fine_astar_state_t *state, const nav2_budget_slice_t &budgetSlice, nav2_fine_astar_result_t *out_result = nullptr );

/**
* @brief Return whether a fine A* solver has reached a terminal state.
* @param state Solver state to inspect.
* @return True when the solver is done, cancelled, failed, or stale.
**/
const bool SVG_Nav2_FineAStar_IsTerminal( const nav2_fine_astar_state_t *state );

/**
* @brief Emit a bounded debug summary for the fine A* state.
* @param state Solver state to report.
* @param limit Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintFineAStar( const nav2_fine_astar_state_t &state, const int32_t limit = 16 );

/**
* @brief Keep the nav2 fine A* module represented in the build.
**/
void SVG_Nav2_FineAStar_ModuleAnchor( void );
