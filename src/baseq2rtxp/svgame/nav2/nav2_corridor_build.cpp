/********************************************************************
*
*
*	ServerGame: Nav2 Corridor Extraction - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_corridor_build.h"

#include <algorithm>


/**
*	@brief	Translate one coarse node into a corridor segment type.
*	@param	node	Coarse node to classify.
*	@return	Corresponding corridor segment type.
**/
static nav2_corridor_segment_type_t SVG_Nav2_CoarseNodeToCorridorSegmentType( const nav2_coarse_astar_node_t &node ) {
    // Preserve the coarse node semantics so corridor segments remain faithful to route commitments.
    switch ( node.kind ) {
        case nav2_coarse_astar_node_kind_t::Portal:
            return nav2_corridor_segment_type_t::PortalTransition;
        case nav2_coarse_astar_node_kind_t::Ladder:
            return nav2_corridor_segment_type_t::LadderTransition;
        case nav2_coarse_astar_node_kind_t::Stair:
            return nav2_corridor_segment_type_t::StairBand;
        case nav2_coarse_astar_node_kind_t::MoverBoarding:
            return nav2_corridor_segment_type_t::MoverBoarding;
        case nav2_coarse_astar_node_kind_t::MoverRide:
            return nav2_corridor_segment_type_t::MoverRide;
        case nav2_coarse_astar_node_kind_t::MoverExit:
            return nav2_corridor_segment_type_t::MoverExit;
        case nav2_coarse_astar_node_kind_t::Goal:
            return nav2_corridor_segment_type_t::GoalEndpoint;
        case nav2_coarse_astar_node_kind_t::Start:
            return nav2_corridor_segment_type_t::TileSeed;
        case nav2_coarse_astar_node_kind_t::RegionLayer:
            return nav2_corridor_segment_type_t::RegionBand;
        default:
            return nav2_corridor_segment_type_t::RegionBand;
    }
}

/**
*	@brief	Build a topology reference from one coarse node.
*	@param	node	Coarse node to mirror.
*	@return	Topology reference to store in the corridor.
**/
static nav2_corridor_topology_ref_t SVG_Nav2_MakeCorridorTopologyFromNode( const nav2_coarse_astar_node_t &node ) {
    // Translate the coarse node metadata into the corridor's persistence-friendly topology record.
    nav2_corridor_topology_ref_t topology = {};
    topology.leaf_index = node.topology.leaf_index;
    topology.cluster_id = node.topology.cluster_id;
    topology.area_id = node.topology.area_id;
    topology.region_id = node.region_layer_id;
    topology.portal_id = node.topology.portal_id;
    topology.tile_id = node.topology.tile_id;
    topology.tile_x = node.topology.tile_x;
    topology.tile_y = node.topology.tile_y;
    topology.cell_index = node.topology.cell_index;
    topology.layer_index = node.topology.layer_index;
    return topology;
}

/**
*	@brief	Build a mover reference from one coarse node.
*	@param	node	Coarse node to mirror.
*	@return	Mover reference to store in the corridor.
**/
static nav2_corridor_mover_ref_t SVG_Nav2_MakeCorridorMoverFromNode( const nav2_coarse_astar_node_t &node ) {
    // Mirror mover identifiers into the corridor so mover-relative path state stays explicit.
    nav2_corridor_mover_ref_t mover = {};
    mover.owner_entnum = node.mover_entnum;
    mover.model_index = node.inline_model_index;
    return mover;
}

/**
*	@brief	Map a coarse edge into a corridor policy segment flag mask.
*	@param	edge	Coarse edge to inspect.
*	@return	Segment flags that should be attached to the corridor segment.
**/
static uint32_t SVG_Nav2_CoarseEdgeToCorridorFlags( const nav2_coarse_astar_edge_t &edge ) {
    // Preserve the solver edge semantics so the corridor reports the same route commitments.
    uint32_t flags = NAV_CORRIDOR_SEGMENT_FLAG_NONE;
    flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND;
    flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID;
    if ( edge.connector_id >= 0 ) {
        flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID;
    }
    if ( edge.mover_ref.IsValid() ) {
        flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF;
    }
    switch ( edge.kind ) {
        case nav2_coarse_astar_edge_kind_t::PortalLink:
            flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID;
            break;
        case nav2_coarse_astar_edge_kind_t::LadderLink:
            flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT;
            break;
        case nav2_coarse_astar_edge_kind_t::StairLink:
            flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES;
            break;
        case nav2_coarse_astar_edge_kind_t::MoverBoarding:
            flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_BOARDING;
            break;
        case nav2_coarse_astar_edge_kind_t::MoverRide:
            flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_RIDE;
            break;
        case nav2_coarse_astar_edge_kind_t::MoverExit:
            flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_EXIT;
            break;
        default:
            break;
    }
    if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_HARD_INVALID ) != 0 ) {
        flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT;
    }
    if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_TOPOLOGY_PENALIZED ) != 0 ) {
        flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED;
    }
    return flags;
}

/**
*	@brief	Build a corridor segment from one coarse node.
*	@param	state	Coarse solver state.
*	@param	node	Coarse node to mirror.
*	@param	is_start	True when the node represents the start of the path.
*	@param	is_goal	True when the node represents the goal of the path.
*	@return	Corridor segment extracted from the coarse node.
**/
static nav2_corridor_segment_t SVG_Nav2_MakeCorridorSegmentFromNode( const nav2_coarse_astar_state_t &state, const nav2_coarse_astar_node_t &node, const bool is_start, const bool is_goal ) {
    // Translate the coarse node into an explicit corridor segment with stable ids and policy flags.
    nav2_corridor_segment_t segment = {};
    segment.segment_id = node.node_id;
    segment.type = SVG_Nav2_CoarseNodeToCorridorSegmentType( node );
    segment.flags = NAV_CORRIDOR_SEGMENT_FLAG_NONE;
    segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND;
    segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID;
    if ( node.topology.portal_id >= 0 ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID;
    }
    if ( node.topology.tile_id >= 0 ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TILE_REF;
    }
    if ( node.topology.leaf_index >= 0 ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LEAF_MEMBERSHIP;
    }
    if ( node.topology.cluster_id >= 0 ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_CLUSTER_MEMBERSHIP;
    }
    if ( node.topology.area_id >= 0 ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_AREA_MEMBERSHIP;
    }
    if ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_HAS_LADDER ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT;
    }
    if ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_HAS_MOVER ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF;
    }
    if ( node.kind == nav2_coarse_astar_node_kind_t::MoverBoarding ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_BOARDING;
    }
    if ( node.kind == nav2_coarse_astar_node_kind_t::MoverRide ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_RIDE;
    }
    if ( node.kind == nav2_coarse_astar_node_kind_t::MoverExit ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_EXIT;
    }
    if ( is_start ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_START_ENDPOINT;
    }
    if ( is_goal ) {
        segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_GOAL_ENDPOINT;
    }
    segment.start_anchor = node.topology.tile_x >= 0 || node.topology.tile_y >= 0 ? Vector3{ ( double )node.topology.tile_x, ( double )node.topology.tile_y, node.allowed_z_band.preferred_z } : Vector3{ 0.0, 0.0, node.allowed_z_band.preferred_z };
    segment.end_anchor = segment.start_anchor;
    segment.allowed_z_band = node.allowed_z_band;
    segment.topology = SVG_Nav2_MakeCorridorTopologyFromNode( node );
    segment.traversal_feature_bits = 0;
    segment.edge_feature_bits = NAV_EDGE_FEATURE_NONE;
    segment.ladder_endpoint_flags = ( node.kind == nav2_coarse_astar_node_kind_t::Ladder ? NAV_LADDER_ENDPOINT_STARTPOINT : NAV_LADDER_ENDPOINT_NONE );
    segment.mover_ref = SVG_Nav2_MakeCorridorMoverFromNode( node );
    segment.policy_penalty = ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_PARTIAL ) != 0 ? 4.0 : 0.0;
    segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE;
    return segment;
}

/**
*	@brief	Build a corridor edge from one coarse edge.
*	@param	edge	Coarse edge to mirror.
*	@return	Corridor-compatible policy flags.
**/
static uint32_t SVG_Nav2_CoarseEdgeToCorridorSegmentFlags( const nav2_coarse_astar_edge_t &edge ) {
    // Translate coarse-edge semantics into the corridor segment's policy flags.
    uint32_t flags = SVG_Nav2_CoarseEdgeToCorridorFlags( edge );
    if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_PASSABLE ) != 0 ) {
        flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE;
    }
    if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_TOPOLOGY_PENALIZED ) != 0 ) {
        flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED;
    }
    if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_HARD_INVALID ) != 0 ) {
        flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT;
    }
    return flags;
}


/**
*	@brief	Build a nav2 corridor from a solved coarse A* state.
*	@param	coarse_state	Solved or partially solved coarse search state.
*	@param	out_corridor	[out] Corridor receiving the committed topology and policy data.
*	@return	True when a corridor was produced.
*	@note	This adapter translates coarse node and edge commitments into the explicit nav2 corridor model.
**/
const bool SVG_Nav2_BuildCorridorFromCoarseAStar( const nav2_coarse_astar_state_t &coarse_state, nav2_corridor_t *out_corridor ) {
    // Require output storage and at least one coarse node to translate.
    if ( !out_corridor || coarse_state.nodes.empty() ) {
        return false;
    }

    // Reset the corridor so callers receive a clean, deterministic result.
    *out_corridor = {};
    out_corridor->source = nav2_corridor_source_t::Hierarchy;
    out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_REGION_PATH | NAV_CORRIDOR_FLAG_HAS_PORTAL_PATH | NAV_CORRIDOR_FLAG_HAS_REFINEMENT_POLICY | NAV_CORRIDOR_FLAG_HIERARCHY_DERIVED;
    out_corridor->coarse_expansion_count = ( int32_t )coarse_state.diagnostics.expansions;
    out_corridor->policy_mismatch_count = ( int32_t )coarse_state.diagnostics.policy_prunes;
    out_corridor->policy_penalty_total = 0.0;
    out_corridor->global_z_band = coarse_state.goal_candidate.center_origin.z >= coarse_state.start_candidate.center_origin.z
        ? nav2_corridor_z_band_t{ coarse_state.start_candidate.center_origin.z - 32.0, coarse_state.goal_candidate.center_origin.z + 32.0, coarse_state.goal_candidate.center_origin.z }
        : nav2_corridor_z_band_t{ coarse_state.goal_candidate.center_origin.z - 32.0, coarse_state.start_candidate.center_origin.z + 32.0, coarse_state.start_candidate.center_origin.z };
    out_corridor->start_topology = SVG_Nav2_MakeCorridorTopologyFromNode( coarse_state.nodes.front() );
    out_corridor->goal_topology = SVG_Nav2_MakeCorridorTopologyFromNode( coarse_state.nodes.back() );
    out_corridor->region_path.reserve( coarse_state.path.node_ids.size() );
    out_corridor->portal_path.reserve( coarse_state.path.edge_ids.size() );

    // Mirror the coarse path into the corridor region and portal paths.
    for ( size_t i = 0; i < coarse_state.path.node_ids.size(); ++i ) {
        const int32_t nodeId = coarse_state.path.node_ids[ i ];
        const nav2_coarse_astar_node_t *node = nullptr;
        for ( const nav2_coarse_astar_node_t &candidateNode : coarse_state.nodes ) {
            if ( candidateNode.node_id == nodeId ) {
                node = &candidateNode;
                break;
            }
        }
        if ( !node ) {
            continue;
        }
        out_corridor->region_path.push_back( node->region_layer_id );
        const bool isStart = i == 0;
        const bool isGoal = i + 1 == coarse_state.path.node_ids.size();
        const nav2_corridor_segment_t segment = SVG_Nav2_MakeCorridorSegmentFromNode( coarse_state, *node, isStart, isGoal );
        out_corridor->segments.push_back( segment );
        out_corridor->policy_penalty_total += segment.policy_penalty;
    }
    for ( const int32_t edgeId : coarse_state.path.edge_ids ) {
        const nav2_coarse_astar_edge_t *edge = nullptr;
        for ( const nav2_coarse_astar_edge_t &candidateEdge : coarse_state.edges ) {
            if ( candidateEdge.edge_id == edgeId ) {
                edge = &candidateEdge;
                break;
            }
        }
        if ( !edge ) {
            continue;
        }
        out_corridor->portal_path.push_back( edge->connector_id );
        out_corridor->policy_penalty_total += edge->topology_penalty;
        if ( ( edge->flags & NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_LADDER ) != 0 ) {
            out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_LADDER_COMMITMENT;
        }
        if ( ( edge->flags & NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_STAIR ) != 0 ) {
            out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_STAIR_COMMITMENT;
        }
        if ( ( edge->flags & NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_MOVER ) != 0 ) {
            out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_MOVER_COMMITMENT;
        }
    }

    // Populate the exact tile route from the coarse path endpoints and node topologies.
    for ( const nav2_coarse_astar_node_t &node : coarse_state.nodes ) {
        if ( node.kind == nav2_coarse_astar_node_kind_t::Start || node.kind == nav2_coarse_astar_node_kind_t::Goal || node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_PREFERRED ) {
            nav2_corridor_tile_ref_t tileRef = {};
            tileRef.tile_x = node.topology.tile_x;
            tileRef.tile_y = node.topology.tile_y;
            tileRef.tile_id = node.topology.tile_id;
            out_corridor->exact_tile_route.push_back( tileRef );
        }
    }
    if ( out_corridor->exact_tile_route.empty() ) {
        // Keep the route non-empty when the coarse solver only reported a minimal frontier path.
        nav2_corridor_tile_ref_t tileRef = {};
        tileRef.tile_x = ( int32_t )coarse_state.start_candidate.tile_x;
        tileRef.tile_y = ( int32_t )coarse_state.start_candidate.tile_y;
        tileRef.tile_id = coarse_state.start_candidate.tile_id;
        out_corridor->exact_tile_route.push_back( tileRef );
    }
    out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_EXACT_TILE_ROUTE;
    if ( !coarse_state.query_state.has_provisional_result || coarse_state.status != nav2_coarse_astar_status_t::Success ) {
        out_corridor->flags |= NAV_CORRIDOR_FLAG_PARTIAL_RESULT;
    }
    return !out_corridor->segments.empty();
}

/**
*	@brief	Keep the nav2 corridor-extraction helper module represented in the build.
**/
void SVG_Nav2_CorridorBuild_ModuleAnchor( void ) {
}
