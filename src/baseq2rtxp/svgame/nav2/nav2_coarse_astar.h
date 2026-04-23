/********************************************************************
*
*
*	ServerGame: Nav2 Coarse A* Solver
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_goal_candidates.h"
#include "svgame/nav2/nav2_hierarchy_graph.h"
#include "svgame/nav2/nav2_scheduler.h"

#include <vector>


/**
*
*
*	Nav2 Coarse A* Enumerations:
*
*
**/
/**
* @brief Stable coarse search status used by resumable jobs and diagnostics.
**/
enum class nav2_coarse_astar_status_t : uint8_t {
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
* @brief Search-node kind used by the coarse frontier.
**/
enum class nav2_coarse_astar_node_kind_t : uint8_t {
    None = 0,
    Start,
    Goal,
    RegionLayer,
    Portal,
    Ladder,
    Stair,
    MoverBoarding,
    MoverRide,
    MoverExit,
    Fallback,
    Count
};

/**
* @brief Search-edge kind used by the coarse frontier.
**/
enum class nav2_coarse_astar_edge_kind_t : uint8_t {
    None = 0,
    RegionLink,
    PortalLink,
    LadderLink,
    StairLink,
    MoverBoarding,
    MoverRide,
    MoverExit,
    FallbackLink,
    Count
};

/**
* @brief Flags describing a coarse search node.
**/
enum nav2_coarse_astar_node_flag_t : uint32_t {
    NAV2_COARSE_ASTAR_NODE_FLAG_NONE = 0,
    NAV2_COARSE_ASTAR_NODE_FLAG_HAS_REGION_LAYER = ( 1u << 0 ),
    NAV2_COARSE_ASTAR_NODE_FLAG_HAS_PORTAL = ( 1u << 1 ),
    NAV2_COARSE_ASTAR_NODE_FLAG_HAS_LADDER = ( 1u << 2 ),
    NAV2_COARSE_ASTAR_NODE_FLAG_HAS_STAIR = ( 1u << 3 ),
    NAV2_COARSE_ASTAR_NODE_FLAG_HAS_MOVER = ( 1u << 4 ),
    NAV2_COARSE_ASTAR_NODE_FLAG_HAS_ALLOWED_Z_BAND = ( 1u << 5 ),
    NAV2_COARSE_ASTAR_NODE_FLAG_PARTIAL = ( 1u << 6 ),
    NAV2_COARSE_ASTAR_NODE_FLAG_PREFERRED = ( 1u << 7 )
};

/**
* @brief Flags describing a coarse search edge.
**/
enum nav2_coarse_astar_edge_flag_t : uint32_t {
    NAV2_COARSE_ASTAR_EDGE_FLAG_NONE = 0,
    NAV2_COARSE_ASTAR_EDGE_FLAG_PASSABLE = ( 1u << 0 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_PORTAL = ( 1u << 1 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_LADDER = ( 1u << 2 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_STAIR = ( 1u << 3 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_MOVER = ( 1u << 4 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_ALLOWS_BOARDING = ( 1u << 5 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_ALLOWS_RIDE = ( 1u << 6 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_ALLOWS_EXIT = ( 1u << 7 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_TOPOLOGY_MATCHED = ( 1u << 8 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_TOPOLOGY_PENALIZED = ( 1u << 9 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_HARD_INVALID = ( 1u << 10 ),
    NAV2_COARSE_ASTAR_EDGE_FLAG_PARTIAL = ( 1u << 11 )
};


/**
*
*
*	Nav2 Coarse A* Data Structures:
*
*
**/
/**
* @brief Stable reference to one coarse frontier node.
**/
struct nav2_coarse_astar_node_ref_t {
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
* @brief One coarse A* node in the resumable frontier.
**/
struct nav2_coarse_astar_node_t {
    //! Stable node id.
    int32_t node_id = -1;
    //! Semantic kind of the node.
    nav2_coarse_astar_node_kind_t kind = nav2_coarse_astar_node_kind_t::None;
    //! Stable hierarchy node id when known.
    int32_t hierarchy_node_id = -1;
    //! Stable region-layer id when known.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Stable connector id when known.
    int32_t connector_id = -1;
    //! Stable mover entity number when known.
    int32_t mover_entnum = -1;
    //! Stable inline model index when known.
    int32_t inline_model_index = -1;
    //! Source candidate id when this node originated from candidate endpoint selection.
    int32_t candidate_id = -1;
    //! Stable node flags.
    uint32_t flags = NAV2_COARSE_ASTAR_NODE_FLAG_NONE;
    //! Committed Z band for this node.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Stable topology metadata for the node.
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
* @brief One coarse A* edge in the resumable frontier.
**/
struct nav2_coarse_astar_edge_t {
    //! Stable edge id.
    int32_t edge_id = -1;
    //! Source node id.
    int32_t from_node_id = -1;
    //! Destination node id.
    int32_t to_node_id = -1;
    //! Semantic kind of the edge.
    nav2_coarse_astar_edge_kind_t kind = nav2_coarse_astar_edge_kind_t::None;
    //! Stable edge flags.
    uint32_t flags = NAV2_COARSE_ASTAR_EDGE_FLAG_NONE;
    //! Base traversal cost.
    double base_cost = 0.0;
    //! Additional penalty attached by the coarse solver.
    double topology_penalty = 0.0;
    //! Committed Z band for the edge.
    nav2_corridor_z_band_t allowed_z_band = {};
    //! Stable connector id when known.
    int32_t connector_id = -1;
    //! Stable region-layer id when known.
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    //! Stable mover reference when known.
    nav2_corridor_mover_ref_t mover_ref = {};
};

/**
* @brief One coarse A* frontier result path.
**/
struct nav2_coarse_astar_path_t {
    //! Nodes in start-to-goal order.
    std::vector<int32_t> node_ids = {};
    //! Edges in start-to-goal order.
    std::vector<int32_t> edge_ids = {};
};

/**
* @brief Solver diagnostics recorded while the coarse search advances.
**/
struct nav2_coarse_astar_diagnostics_t {
    //! Number of nodes expanded.
    uint32_t expansions = 0;
    //! Number of slices consumed.
    uint32_t slices_consumed = 0;
    //! Number of times the search was paused.
    uint32_t pauses = 0;
    //! Number of times the search was resumed.
    uint32_t resumes = 0;
    //! Number of frontier nodes pruned by stale snapshot checks.
    uint32_t stale_prunes = 0;
    //! Number of frontier nodes pruned by policy mismatch.
    uint32_t policy_prunes = 0;
    //! Number of frontier nodes pruned by mover mismatch.
    uint32_t mover_prunes = 0;
    //! Number of frontier nodes pruned by hard invalidity.
    uint32_t hard_invalid_prunes = 0;
};

/**
* @brief Resumable coarse A* job state.
**/
struct nav2_coarse_astar_state_t {
    //! Stable search status.
    nav2_coarse_astar_status_t status = nav2_coarse_astar_status_t::None;
    //! Scheduler query state used to bind budget and snapshots.
    nav2_query_state_t query_state = {};
    //! Accepted start candidate.
    nav2_goal_candidate_t start_candidate = {};
    //! Accepted goal candidate.
    nav2_goal_candidate_t goal_candidate = {};
 //! Bound hierarchy graph used for coarse expansion.
    const nav2_hierarchy_graph_t *hierarchy_graph = nullptr;
    //! True when a hierarchy graph has been bound to this solver instance.
    bool hierarchy_graph_bound = false;
    //! Frontier nodes available for expansion.
    std::vector<nav2_coarse_astar_node_t> nodes = {};
    //! Stored frontier edges.
    std::vector<nav2_coarse_astar_edge_t> edges = {};
    //! Reconstructed path when the solver succeeds.
    nav2_coarse_astar_path_t path = {};
    //! Search diagnostics accumulated over time.
    nav2_coarse_astar_diagnostics_t diagnostics = {};
    //! Stable identifier for this solver instance.
    uint64_t solver_id = 0;
    //! Next stable node id for O(1) frontier node id allocation during expansion.
    int32_t next_node_id = 1;
    //! Next stable edge id for O(1) frontier edge id allocation during expansion.
    int32_t next_edge_id = 1;
    //! True when the solver currently owns a frontier slice.
    bool has_frontier_slice = false;
};

/**
* @brief Compact result wrapper returned when the coarse solver completes or pauses.
**/
struct nav2_coarse_astar_result_t {
    //! Search status after the latest solver step.
    nav2_coarse_astar_status_t status = nav2_coarse_astar_status_t::None;
    //! True when a path was reconstructed.
    bool has_path = false;
    //! Stable path reconstructed from the solver state.
    nav2_coarse_astar_path_t path = {};
    //! Diagnostics snapshot at the time of the result.
    nav2_coarse_astar_diagnostics_t diagnostics = {};
};


/**
*
*
*	Nav2 Coarse A* Public API:
*
*
**/
/**
* @brief Reset a coarse A* solver state.
* @param state [out] Solver state to reset.
**/
void SVG_Nav2_CoarseAStar_Reset( nav2_coarse_astar_state_t *state );

/**
* @brief Initialize a coarse A* solver from a hierarchy graph and goal candidates.
* @param state [out] Solver state to initialize.
* @param hierarchyGraph Hierarchy graph to search.
* @param startCandidate Selected start candidate.
* @param goalCandidate Selected goal candidate.
* @param solverId Stable solver identifier.
* @return True when initialization succeeded.
**/
const bool SVG_Nav2_CoarseAStar_Init( nav2_coarse_astar_state_t *state, const nav2_hierarchy_graph_t &hierarchyGraph,
    const nav2_goal_candidate_t &startCandidate, const nav2_goal_candidate_t &goalCandidate, const uint64_t solverId );

/**
* @brief Validate the current solver state for stable ids and parent links.
* @param state Solver state to inspect.
* @return True when the frontier state is internally consistent.
**/
const bool SVG_Nav2_CoarseAStar_ValidateState( const nav2_coarse_astar_state_t *state );

/**
* @brief Advance a coarse A* solver by one budgeted slice.
* @param state Solver state to advance.
* @param budgetSlice Budget slice controlling this step.
* @param out_result [out] Optional result snapshot.
* @return True when the solver made progress or completed.
**/
const bool SVG_Nav2_CoarseAStar_Step( nav2_coarse_astar_state_t *state, const nav2_budget_slice_t &budgetSlice, nav2_coarse_astar_result_t *out_result = nullptr );

/**
*	@brief Return whether a coarse A* solver has reached a terminal state.
*	@param state Solver state to inspect.
*	@return True when the solver is done, cancelled, failed, or stale.
**/
const bool SVG_Nav2_CoarseAStar_IsTerminal( const nav2_coarse_astar_state_t *state );

/**
* @brief Emit a bounded debug summary for the coarse A* state.
* @param state Solver state to report.
* @param limit Maximum number of nodes or edges to print.
**/
void SVG_Nav2_DebugPrintCoarseAStar( const nav2_coarse_astar_state_t &state, const int32_t limit = 16 );

/**
* @brief Keep the nav2 coarse A* module represented in the build.
**/
void SVG_Nav2_CoarseAStar_ModuleAnchor( void );
