/********************************************************************
*
*
*	ServerGame: Nav2 Corridor Extraction - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_corridor_build.h"

#include "svgame/nav2/nav2_debug_draw.h"

#include <algorithm>


/**
*	@brief	Translate one coarse node into a corridor segment type.
*	@param	node	Coarse node to classify.
*	@return	Corresponding corridor segment type.
**/
static nav2_corridor_segment_type_t SVG_Nav2_CoarseNodeToCorridorSegmentType( const nav2_coarse_astar_node_t &node ) {
	/**
	*	Preserve the coarse node semantics so corridor segments remain faithful to route commitments.
	**/
	// Translate each node kind into the matching corridor segment type.
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
*	@brief	Translate one coarse edge into a corridor segment type.
*	@param	edge	Coarse edge to classify.
*	@return	Corresponding corridor segment type, or None when no explicit segment is needed.
**/
static nav2_corridor_segment_type_t SVG_Nav2_CoarseEdgeToCorridorSegmentType( const nav2_coarse_astar_edge_t &edge ) {
	/**
	*	Only emit explicit corridor segments for connector-related edges.
	**/
	// Translate edge semantics into explicit corridor transitions.
	switch ( edge.kind ) {
		case nav2_coarse_astar_edge_kind_t::PortalLink:
			return nav2_corridor_segment_type_t::PortalTransition;
		case nav2_coarse_astar_edge_kind_t::LadderLink:
			return nav2_corridor_segment_type_t::LadderTransition;
		case nav2_coarse_astar_edge_kind_t::StairLink:
			return nav2_corridor_segment_type_t::StairBand;
		case nav2_coarse_astar_edge_kind_t::MoverBoarding:
			return nav2_corridor_segment_type_t::MoverBoarding;
		case nav2_coarse_astar_edge_kind_t::MoverRide:
			return nav2_corridor_segment_type_t::MoverRide;
		case nav2_coarse_astar_edge_kind_t::MoverExit:
			return nav2_corridor_segment_type_t::MoverExit;
		default:
			return nav2_corridor_segment_type_t::None;
	}
}

/**
*	@brief	Build a topology reference from one coarse node.
*	@param	node	Coarse node to mirror.
*	@return	Topology reference to store in the corridor.
**/
static nav2_corridor_topology_ref_t SVG_Nav2_MakeCorridorTopologyFromNode( const nav2_coarse_astar_node_t &node ) {
	/**
	*	Translate the coarse node metadata into the corridor's persistence-friendly topology record.
	**/
	// Mirror the stable topology identifiers into the corridor format.
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
	/**
	*	Mirror mover identifiers into the corridor so mover-relative path state stays explicit.
	**/
	// Copy mover identifiers into the persistence-friendly corridor reference.
	nav2_corridor_mover_ref_t mover = {};
	mover.owner_entnum = node.mover_entnum;
	mover.model_index = node.inline_model_index;
	return mover;
}

/**
*	@brief	Initialize the default refinement-policy commitments for a coarse corridor.
*	@param	policy	[out] Refinement policy receiving the default commitment flags and penalties.
**/
static void SVG_Nav2_InitializeCorridorRefinePolicy( nav2_corridor_refine_policy_t *policy ) {
	/**
	*	Sanity check: require output storage before assigning the default policy commitments.
	**/
	// Bail out early when the caller did not provide an output policy slot.
	if ( !policy ) {
		return;
	}

	/**
	*	Reset the policy so corridor extraction always publishes deterministic defaults.
	**/
	// Clear the policy structure before assigning default commitments.
	*policy = {};

	/**
	*	Publish conservative rejection and penalty rules that downstream refinement can reuse.
	**/
	// Reject out-of-band and topology-mismatched candidates while still allowing soft penalties.
	policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_OUT_OF_BAND;
	policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_TOPOLOGY_MISMATCH;
	policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_PENALIZE_TOPOLOGY_MISMATCH;
	policy->topology_mismatch_penalty = 12.0;
	policy->mover_state_penalty = 16.0;
}

/**
*	@brief	Resolve one corridor anchor into world space from persisted topology metadata.
*	@param	topology	Persisted tile/cell membership describing the anchor location.
*	@param	preferredZ	World-space Z height that should remain attached to the anchor.
*	@return	World-space anchor suitable for fine-path waypoint extraction.
*	@note	Corridor topology stores tile-grid identifiers, not direct world XY positions. Converting those
*			identifiers back into world-space cell or tile centers keeps connector-less fallback corridors from
*			publishing tiny grid-index coordinates that steer movers toward origin-like directions.
**/
static Vector3 SVG_Nav2_MakeWorldAnchorFromTopology( const nav2_corridor_topology_ref_t &topology, const double preferredZ ) {
	/**
	*	Start from a conservative fallback anchor that preserves the committed preferred Z.
	**/
	Vector3 anchor = { ( double )topology.tile_x, ( double )topology.tile_y, preferredZ };

	/**
	*	Prefer active nav2 mesh sizing so tile/cell identifiers become real world-space centers.
	**/
	const nav2_query_mesh_t *mesh = SVG_Nav2_GetQueryMesh();
	const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( mesh );
	if ( !meshMeta.HasTileSizing() || meshMeta.tile_size <= 0 ) {
		return anchor;
	}

	const double tileWorldSize = meshMeta.GetTileWorldSize();
	if ( tileWorldSize <= 0.0 ) {
		return anchor;
	}

	/**
	*	When a tile-local cell index is known, publish the center of that concrete cell so waypoint
	*	anchors stay as precise as the currently staged corridor metadata allows.
	**/
	if ( topology.cell_index >= 0 ) {
		const int32_t localCellX = topology.cell_index % meshMeta.tile_size;
		const int32_t localCellY = topology.cell_index / meshMeta.tile_size;
		anchor.x = ( ( double )topology.tile_x * tileWorldSize ) + ( ( ( double )localCellX + 0.5 ) * meshMeta.cell_size_xy );
		anchor.y = ( ( double )topology.tile_y * tileWorldSize ) + ( ( ( double )localCellY + 0.5 ) * meshMeta.cell_size_xy );
		return anchor;
	}

	/**
	*	Fallback to the world-space tile center when only tile membership is available.
	**/
	anchor.x = ( ( double )topology.tile_x * tileWorldSize ) + ( tileWorldSize * 0.5 );
	anchor.y = ( ( double )topology.tile_y * tileWorldSize ) + ( tileWorldSize * 0.5 );
	return anchor;
}

/**
*	@brief	Append one membership id into a corridor refinement list if it is not already present.
*	@param	value	Candidate membership id to append.
*	@param	values	[in,out] Membership list that receives the value.
**/
static void SVG_Nav2_CorridorAppendUniqueMembership( const int32_t value, std::vector<int32_t> *values ) {
	/**
	*	Skip invalid membership ids or missing output storage so the corridor remains defensive.
	**/
	// Guard the append to keep corridor membership lists stable.
	if ( !values || value < 0 ) {
		return;
	}

	/**
	*	Avoid duplicates so the membership lists remain compact and deterministic.
	**/
	// Iterate existing values and exit when the candidate already exists.
	for ( const int32_t existingValue : *values ) {
		if ( existingValue == value ) {
			return;
		}
	}

	// Append the new membership id when it was not already present.
	values->push_back( value );
}

/**
*	@brief	Append a mover reference into the corridor refinement list when it is unique.
*	@param	mover_ref	Mover reference to append.
*	@param	mover_refs	[in,out] Mover reference list receiving the entry.
**/
static void SVG_Nav2_CorridorAppendUniqueMoverRef( const nav2_corridor_mover_ref_t &mover_ref, std::vector<nav2_corridor_mover_ref_t> *mover_refs ) {
	/**
	*	Ignore invalid mover references or missing output storage.
	**/
	// Skip invalid mover references to avoid polluting the allow-list.
	if ( !mover_refs || !mover_ref.IsValid() ) {
		return;
	}

	/**
	*	Prevent duplicates so mover commitments remain compact and deterministic.
	**/
	// Scan for an existing entry that matches this mover reference.
	for ( const nav2_corridor_mover_ref_t &existingRef : *mover_refs ) {
		if ( existingRef.owner_entnum == mover_ref.owner_entnum && existingRef.model_index == mover_ref.model_index ) {
			return;
		}
	}

	// Append the unique mover reference.
	mover_refs->push_back( mover_ref );
}

/**
*	@brief	Map a coarse edge into a corridor policy segment flag mask.
*	@param	edge	Coarse edge to inspect.
*	@return	Segment flags that should be attached to the corridor segment.
**/
static uint32_t SVG_Nav2_CoarseEdgeToCorridorFlags( const nav2_coarse_astar_edge_t &edge ) {
	/**
	*	Preserve the solver edge semantics so the corridor reports the same route commitments.
	**/
	// Start with the base segment flag set.
	uint32_t flags = NAV_CORRIDOR_SEGMENT_FLAG_NONE;
	// Always record the allowed Z band and region membership on edge-derived segments.
	flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND;
	flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID;

	// Track connector identifiers when the coarse edge reports one.
	if ( edge.connector_id >= 0 ) {
		flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID;
	}

	// Track mover commitments when the coarse edge references a mover.
	if ( edge.mover_ref.IsValid() ) {
		flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF;
	}

	/**
	*	Mirror edge semantics into explicit corridor commitment flags.
	**/
	// Apply connector and mover staging flags based on the coarse edge kind.
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

	/**
	*	Mirror coarse-edge penalties into corridor policy flags.
	**/
	// Mark hard-invalid edges so refinement can reject them explicitly.
	if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_HARD_INVALID ) != 0 ) {
		flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT;
	}

	// Mark topology-penalized edges so refinement can apply soft penalties.
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
	/**
	*	Translate the coarse node into an explicit corridor segment with stable ids and policy flags.
	**/
	// Initialize the segment with deterministic defaults.
	nav2_corridor_segment_t segment = {};
	segment.segment_id = node.node_id;
	segment.type = SVG_Nav2_CoarseNodeToCorridorSegmentType( node );
	segment.flags = NAV_CORRIDOR_SEGMENT_FLAG_NONE;
	segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND;
	segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID;

	/**
	*	Populate explicit topology memberships carried by the coarse node.
	**/
	// Track portal, tile, and BSP membership flags when they are present.
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

	/**
	*	Mirror traversal features and mover stages from the coarse node.
	**/
	// Apply ladder and mover flags that describe traversal intent.
	if ( ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_HAS_LADDER ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT | NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES;
		segment.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_LADDER;
	}
	if ( ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_HAS_STAIR ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES | NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES;
		segment.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE;
		segment.edge_feature_bits |= NAV_EDGE_FEATURE_STAIR_STEP_UP | NAV_EDGE_FEATURE_STAIR_STEP_DOWN;
	}
	if ( ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_HAS_MOVER ) != 0 ) {
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

	/**
	*	Mark endpoint segments so refinement can detect start and goal commitments.
	**/
	// Record start and goal markers on the appropriate segments.
	if ( is_start ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_START_ENDPOINT;
	}
	if ( is_goal ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_GOAL_ENDPOINT;
	}

	/**
	*	Populate the anchor points and metadata published to refinement and debug consumers.
	**/
	// Convert persisted tile/cell metadata back into world-space anchor positions.
	segment.start_anchor = SVG_Nav2_MakeWorldAnchorFromTopology( node.topology, node.allowed_z_band.preferred_z );
	segment.end_anchor = segment.start_anchor;
	segment.allowed_z_band = node.allowed_z_band;
	segment.topology = SVG_Nav2_MakeCorridorTopologyFromNode( node );
	if ( ( node.endpoint_semantics & NAV2_CONNECTOR_ENDPOINT_PORTAL_BARRIER ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID;
	}
	if ( ( node.transition_semantics & NAV2_CONNECTOR_TRANSITION_LADDER ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT | NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES;
		segment.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_LADDER;
	}
	if ( ( node.transition_semantics & NAV2_CONNECTOR_TRANSITION_STAIR ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES | NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES;
		segment.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE;
		segment.edge_feature_bits |= NAV_EDGE_FEATURE_STAIR_STEP_UP | NAV_EDGE_FEATURE_STAIR_STEP_DOWN;
	}
	if ( ( node.transition_semantics & NAV2_CONNECTOR_TRANSITION_OPENING ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES;
		segment.edge_feature_bits |= NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF;
	}
	segment.ladder_endpoint_flags = NAV_LADDER_ENDPOINT_NONE;
	if ( ( node.endpoint_semantics & NAV2_CONNECTOR_ENDPOINT_LADDER_BOTTOM ) != 0 ) {
		segment.ladder_endpoint_flags |= NAV_LADDER_ENDPOINT_STARTPOINT;
	}
	if ( ( node.endpoint_semantics & NAV2_CONNECTOR_ENDPOINT_LADDER_TOP ) != 0 ) {
		segment.ladder_endpoint_flags |= NAV_LADDER_ENDPOINT_ENDPOINT;
	}
	segment.mover_ref = SVG_Nav2_MakeCorridorMoverFromNode( node );

	/**
	*	Publish a conservative policy penalty when the node was marked partial.
	**/
	// Apply a small penalty for partial nodes so refinement can bias away from them.
	segment.policy_penalty = ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_PARTIAL ) != 0 ? 4.0 : 0.0;
	segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE;

	(void)state;
	return segment;
}

/**
*	@brief	Build a corridor edge from one coarse edge.
*	@param	edge	Coarse edge to mirror.
*	@return	Corridor-compatible policy flags.
**/
static uint32_t SVG_Nav2_CoarseEdgeToCorridorSegmentFlags( const nav2_coarse_astar_edge_t &edge );

/**
*	@brief	Build a corridor segment from one coarse edge and its endpoint nodes.
*	@param	edge	Coarse edge to mirror.
*	@param	from_node	Coarse node at the start of the edge.
*	@param	to_node	Coarse node at the end of the edge.
*	@param	segment_id	Stable segment identifier to assign.
*	@return	Corridor segment describing the connector transition.
**/
static nav2_corridor_segment_t SVG_Nav2_MakeCorridorSegmentFromEdge( const nav2_coarse_astar_edge_t &edge, const nav2_coarse_astar_node_t &from_node,
	const nav2_coarse_astar_node_t &to_node, const int32_t segment_id ) {
	/**
	*	Translate the coarse edge into an explicit connector segment when the edge represents a commitment.
	**/
	// Initialize the segment with the edge-derived semantics.
	nav2_corridor_segment_t segment = {};
	segment.segment_id = segment_id;
	segment.type = SVG_Nav2_CoarseEdgeToCorridorSegmentType( edge );
	segment.flags = SVG_Nav2_CoarseEdgeToCorridorSegmentFlags( edge );
	segment.allowed_z_band = edge.allowed_z_band;
	segment.mover_ref = edge.mover_ref;
	segment.traversal_feature_bits = NAV_TRAVERSAL_FEATURE_NONE;
	segment.edge_feature_bits = NAV_EDGE_FEATURE_NONE;
	segment.policy_penalty = edge.topology_penalty;
	if ( ( edge.transition_semantics & NAV2_CONNECTOR_TRANSITION_LADDER ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT | NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES;
		segment.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_LADDER;
	}
	if ( ( edge.transition_semantics & NAV2_CONNECTOR_TRANSITION_STAIR ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES | NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES;
		segment.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE;
		segment.edge_feature_bits |= NAV_EDGE_FEATURE_STAIR_STEP_UP | NAV_EDGE_FEATURE_STAIR_STEP_DOWN;
	}
	if ( ( edge.transition_semantics & NAV2_CONNECTOR_TRANSITION_PORTAL ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID;
	}
	if ( ( edge.transition_semantics & NAV2_CONNECTOR_TRANSITION_OPENING ) != 0 ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES;
		segment.edge_feature_bits |= NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF;
	}

	/**
	*	Mirror topology and anchors from the edge endpoints so refinement can reconstruct transition intent.
	**/
	// Use the destination node topology as the corridor topology reference.
	segment.topology = SVG_Nav2_MakeCorridorTopologyFromNode( to_node );
	if ( segment.mover_ref.IsValid() ) {
		segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF;
	}

	// Convert persisted topology memberships back into world-space connector anchors.
	segment.start_anchor = SVG_Nav2_MakeWorldAnchorFromTopology( from_node.topology, edge.allowed_z_band.preferred_z );
	segment.end_anchor = SVG_Nav2_MakeWorldAnchorFromTopology( to_node.topology, edge.allowed_z_band.preferred_z );
	if ( ( from_node.endpoint_semantics & NAV2_CONNECTOR_ENDPOINT_LADDER_BOTTOM ) != 0 ) {
		segment.ladder_endpoint_flags |= NAV_LADDER_ENDPOINT_STARTPOINT;
	}
	if ( ( to_node.endpoint_semantics & NAV2_CONNECTOR_ENDPOINT_LADDER_TOP ) != 0 ) {
		segment.ladder_endpoint_flags |= NAV_LADDER_ENDPOINT_ENDPOINT;
	}
	return segment;
}

/**
*	@brief	Build a corridor edge from one coarse edge.
*	@param	edge	Coarse edge to mirror.
*	@return	Corridor-compatible policy flags.
**/
static uint32_t SVG_Nav2_CoarseEdgeToCorridorSegmentFlags( const nav2_coarse_astar_edge_t &edge ) {
	/**
	*	Translate coarse-edge semantics into the corridor segment's policy flags.
	**/
	// Start with the base coarse-edge flags.
	uint32_t flags = SVG_Nav2_CoarseEdgeToCorridorFlags( edge );

	// Mark passable edges as policy compatible.
	if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_PASSABLE ) != 0 ) {
		flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE;
	}

	// Apply soft-penalty flags for topology-mismatched edges.
	if ( ( edge.flags & NAV2_COARSE_ASTAR_EDGE_FLAG_TOPOLOGY_PENALIZED ) != 0 ) {
		flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED;
	}

	// Apply hard rejection flags when the edge is invalid.
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
	/**
	*	Require output storage and a solved coarse path before generating corridor commitments.
	**/
	// Bail out when the caller did not provide output storage.
	if ( !out_corridor ) {
		return false;
	}

	// Bail out when the coarse search produced no nodes or no path.
	if ( coarse_state.nodes.empty() || coarse_state.path.node_ids.empty() ) {
		return false;
	}

	/**
	*	Reset the corridor so callers receive a clean, deterministic result.
	**/
	// Reset the corridor output to a known state.
	*out_corridor = {};
	out_corridor->source = nav2_corridor_source_t::Hierarchy;
	out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_REGION_PATH | NAV_CORRIDOR_FLAG_HAS_PORTAL_PATH | NAV_CORRIDOR_FLAG_HAS_REFINEMENT_POLICY | NAV_CORRIDOR_FLAG_HIERARCHY_DERIVED;
	out_corridor->coarse_expansion_count = ( int32_t )coarse_state.diagnostics.expansions;
	out_corridor->policy_mismatch_count = ( int32_t )coarse_state.diagnostics.policy_prunes;
	out_corridor->policy_penalty_total = 0.0;

	/**
	*	Seed the refinement policy defaults before collecting membership constraints.
	**/
	// Initialize conservative policy commitments that downstream refinement can honor.
	SVG_Nav2_InitializeCorridorRefinePolicy( &out_corridor->refine_policy );

	/**
	*	Seed the global Z-band from the coarse endpoints before per-segment refinement.
	**/
	// Build a conservative corridor-wide Z band using the endpoint candidates.
	out_corridor->global_z_band = coarse_state.goal_candidate.center_origin.z >= coarse_state.start_candidate.center_origin.z
		? nav2_corridor_z_band_t{ coarse_state.start_candidate.center_origin.z - 32.0, coarse_state.goal_candidate.center_origin.z + 32.0, coarse_state.goal_candidate.center_origin.z }
		: nav2_corridor_z_band_t{ coarse_state.goal_candidate.center_origin.z - 32.0, coarse_state.start_candidate.center_origin.z + 32.0, coarse_state.start_candidate.center_origin.z };
	out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_GLOBAL_Z_BAND;

	/**
	*	Reserve storage for the corridor commitments to avoid repeated allocations during extraction.
	**/
	// Reserve enough space for region and portal paths plus explicit segments.
	out_corridor->region_path.reserve( coarse_state.path.node_ids.size() );
	out_corridor->portal_path.reserve( coarse_state.path.edge_ids.size() );
	out_corridor->segments.reserve( coarse_state.path.node_ids.size() + coarse_state.path.edge_ids.size() );

	/**
	*	Mirror coarse path nodes into corridor region path and node segments.
	**/
	// Track the resolved start and goal node pointers for endpoint metadata.
	const nav2_coarse_astar_node_t *startNode = nullptr;
	const nav2_coarse_astar_node_t *goalNode = nullptr;

	// Iterate through the node ids in path order.
	for ( size_t i = 0; i < coarse_state.path.node_ids.size(); ++i ) {
		// Locate the concrete coarse node for this path id.
		const int32_t nodeId = coarse_state.path.node_ids[ i ];
		const nav2_coarse_astar_node_t *node = nullptr;
		for ( const nav2_coarse_astar_node_t &candidateNode : coarse_state.nodes ) {
			// Match by stable node id so path order stays deterministic.
			if ( candidateNode.node_id == nodeId ) {
				node = &candidateNode;
				break;
			}
		}

		// Skip unresolved nodes but continue scanning so the corridor remains as complete as possible.
		if ( !node ) {
			continue;
		}

		// Record the first and last resolved nodes as endpoints.
		if ( i == 0 ) {
			startNode = node;
		}
		if ( i + 1 == coarse_state.path.node_ids.size() ) {
			goalNode = node;
		}

		/**
		*	Append the region-path identifier and the corresponding node segment.
		**/
		// Append region-layer ids when they are valid.
		if ( node->region_layer_id != NAV_REGION_ID_NONE ) {
			out_corridor->region_path.push_back( node->region_layer_id );
		}

		// Build the explicit corridor segment for this node.
		const bool isStart = i == 0;
		const bool isGoal = i + 1 == coarse_state.path.node_ids.size();
		nav2_corridor_segment_t segment = SVG_Nav2_MakeCorridorSegmentFromNode( coarse_state, *node, isStart, isGoal );

		/**
		*	Preserve the exact selected candidate endpoints on start/goal node segments so connector-less
		*	fallback corridors still publish real world-space anchors instead of coarse tile-center approximations.
		**/
		if ( isStart ) {
			segment.start_anchor = coarse_state.start_candidate.center_origin;
			segment.end_anchor = coarse_state.start_candidate.center_origin;
		}
		if ( isGoal ) {
			segment.start_anchor = coarse_state.goal_candidate.center_origin;
			segment.end_anchor = coarse_state.goal_candidate.center_origin;
		}
		out_corridor->segments.push_back( segment );
		out_corridor->policy_penalty_total += segment.policy_penalty;

		/**
		*	Promote node-level traversal commitments into top-level corridor flags.
		**/
		// Track ladder, stair, and mover commitments discovered in node segments.
		if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT ) != 0 ) {
			out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_LADDER_COMMITMENT;
		}
		if ( segment.type == nav2_corridor_segment_type_t::StairBand ) {
			out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_STAIR_COMMITMENT;
		}
		if ( segment.mover_ref.IsValid() ) {
			out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_MOVER_COMMITMENT;
		}

		/**
		*	Accumulate corridor-wide refinement-policy memberships from the node segment.
		**/
		// Append stable membership ids for refinement filtering.
		SVG_Nav2_CorridorAppendUniqueMembership( segment.topology.leaf_index, &out_corridor->refine_policy.allowed_leaf_indices );
		SVG_Nav2_CorridorAppendUniqueMembership( segment.topology.cluster_id, &out_corridor->refine_policy.allowed_cluster_ids );
		SVG_Nav2_CorridorAppendUniqueMembership( segment.topology.area_id, &out_corridor->refine_policy.allowed_area_ids );

		// Track mover references when they are valid.
		if ( segment.mover_ref.IsValid() ) {
			SVG_Nav2_CorridorAppendUniqueMoverRef( segment.mover_ref, &out_corridor->refine_policy.mover_refs );
		}
	}

	/**
	*	Mark mover-policy commitments when mover references were collected from the node path.
	**/
	// Enable mover policy flags after collecting mover references from node segments.
	if ( !out_corridor->refine_policy.mover_refs.empty() ) {
		out_corridor->refine_policy.flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT;
	}

	/**
	*	Publish endpoint topology metadata derived from the resolved path endpoints.
	**/
	// Populate the corridor endpoint topology when resolved nodes are available.
	if ( startNode ) {
		out_corridor->start_topology = SVG_Nav2_MakeCorridorTopologyFromNode( *startNode );
	}
	if ( goalNode ) {
		out_corridor->goal_topology = SVG_Nav2_MakeCorridorTopologyFromNode( *goalNode );
	}

	/**
	*	Bridge connector-less coarse routes by falling back to selected endpoint candidates when
	*	path-node topology omitted fine-cell metadata.
	**/
	// Preserve start endpoint tile/cell hints from the selected start candidate when missing.
	if ( out_corridor->start_topology.tile_id < 0 && coarse_state.start_candidate.tile_id >= 0 ) {
		out_corridor->start_topology.tile_id = coarse_state.start_candidate.tile_id;
		out_corridor->start_topology.tile_x = ( int32_t )coarse_state.start_candidate.tile_x;
		out_corridor->start_topology.tile_y = ( int32_t )coarse_state.start_candidate.tile_y;
		out_corridor->start_topology.cell_index = coarse_state.start_candidate.cell_index;
		out_corridor->start_topology.layer_index = coarse_state.start_candidate.layer_index;
		out_corridor->start_topology.leaf_index = coarse_state.start_candidate.leaf_index;
		out_corridor->start_topology.cluster_id = coarse_state.start_candidate.cluster_id;
		out_corridor->start_topology.area_id = coarse_state.start_candidate.area_id;
	}

	// Preserve goal endpoint tile/cell hints from the selected goal candidate when missing.
	if ( out_corridor->goal_topology.tile_id < 0 && coarse_state.goal_candidate.tile_id >= 0 ) {
		out_corridor->goal_topology.tile_id = coarse_state.goal_candidate.tile_id;
		out_corridor->goal_topology.tile_x = ( int32_t )coarse_state.goal_candidate.tile_x;
		out_corridor->goal_topology.tile_y = ( int32_t )coarse_state.goal_candidate.tile_y;
		out_corridor->goal_topology.cell_index = coarse_state.goal_candidate.cell_index;
		out_corridor->goal_topology.layer_index = coarse_state.goal_candidate.layer_index;
		out_corridor->goal_topology.leaf_index = coarse_state.goal_candidate.leaf_index;
		out_corridor->goal_topology.cluster_id = coarse_state.goal_candidate.cluster_id;
		out_corridor->goal_topology.area_id = coarse_state.goal_candidate.area_id;
	}

	/**
	*	Mirror coarse path edges into portal commitments and connector segments.
	**/
	// Iterate over coarse path edges in order.
	for ( size_t edgeIndex = 0; edgeIndex < coarse_state.path.edge_ids.size(); ++edgeIndex ) {
		// Resolve the coarse edge by stable id.
		const int32_t edgeId = coarse_state.path.edge_ids[ edgeIndex ];
		const nav2_coarse_astar_edge_t *edge = nullptr;
		for ( const nav2_coarse_astar_edge_t &candidateEdge : coarse_state.edges ) {
			// Match by stable id to preserve deterministic order.
			if ( candidateEdge.edge_id == edgeId ) {
				edge = &candidateEdge;
				break;
			}
		}

		// Skip unresolved edges but continue building the corridor.
		if ( !edge ) {
			continue;
		}

		/**
		*	Append portal identifiers and corridor flags based on the edge requirements.
		**/
		// Record portal ids when available.
		if ( edge->connector_id >= 0 ) {
			out_corridor->portal_path.push_back( edge->connector_id );
		}

		// Accumulate soft penalties from topology mismatches.
		out_corridor->policy_penalty_total += edge->topology_penalty;

		// Record ladder, stair, and mover commitments on the corridor flags.
		if ( ( edge->flags & NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_LADDER ) != 0 ) {
			out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_LADDER_COMMITMENT;
		}
		if ( ( edge->flags & NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_STAIR ) != 0 ) {
			out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_STAIR_COMMITMENT;
		}
		if ( ( edge->flags & NAV2_COARSE_ASTAR_EDGE_FLAG_REQUIRES_MOVER ) != 0 ) {
			out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_MOVER_COMMITMENT;
		}

		/**
		*	Append explicit connector segments when the edge represents a connector commitment.
		**/
		// Resolve the endpoint nodes needed to build connector anchors.
		const nav2_coarse_astar_node_t *fromNode = nullptr;
		const nav2_coarse_astar_node_t *toNode = nullptr;
		for ( const nav2_coarse_astar_node_t &candidateNode : coarse_state.nodes ) {
			if ( candidateNode.node_id == edge->from_node_id ) {
				fromNode = &candidateNode;
			}
			if ( candidateNode.node_id == edge->to_node_id ) {
				toNode = &candidateNode;
			}
		}

		// Emit a connector segment when the edge kind maps to a corridor transition.
		if ( fromNode && toNode && SVG_Nav2_CoarseEdgeToCorridorSegmentType( *edge ) != nav2_corridor_segment_type_t::None ) {
			const int32_t segmentId = edge->edge_id;
			const nav2_corridor_segment_t connectorSegment = SVG_Nav2_MakeCorridorSegmentFromEdge( *edge, *fromNode, *toNode, segmentId );
			out_corridor->segments.push_back( connectorSegment );
			out_corridor->policy_penalty_total += connectorSegment.policy_penalty;
		}
	}

	/**
	*	Populate the exact tile route from the coarse path endpoints and node topologies.
	**/
	// Mirror key tile references from the path nodes into the exact tile route.
	for ( const nav2_coarse_astar_node_t &node : coarse_state.nodes ) {
		// Record start, goal, and preferred nodes as tile seeds.
		if ( node.kind == nav2_coarse_astar_node_kind_t::Start || node.kind == nav2_coarse_astar_node_kind_t::Goal
			|| ( node.flags & NAV2_COARSE_ASTAR_NODE_FLAG_PREFERRED ) != 0 ) {
			nav2_corridor_tile_ref_t tileRef = {};
			tileRef.tile_x = node.topology.tile_x;
			tileRef.tile_y = node.topology.tile_y;
			tileRef.tile_id = node.topology.tile_id;
			out_corridor->exact_tile_route.push_back( tileRef );
		}
	}

	// Keep the route non-empty when the coarse solver only reported a minimal frontier path.
	if ( out_corridor->exact_tile_route.empty() ) {
		nav2_corridor_tile_ref_t tileRef = {};
		tileRef.tile_x = ( int32_t )coarse_state.start_candidate.tile_x;
		tileRef.tile_y = ( int32_t )coarse_state.start_candidate.tile_y;
		tileRef.tile_id = coarse_state.start_candidate.tile_id;
		out_corridor->exact_tile_route.push_back( tileRef );

		/**
		*	When only one fallback tile seed exists, append the goal candidate tile too so fine-init
		*	can resolve both endpoints in connector-less routes.
		**/
		// Append a deterministic goal-side tile fallback when it differs from the start seed.
		if ( coarse_state.goal_candidate.tile_id >= 0
			&& ( coarse_state.goal_candidate.tile_id != coarse_state.start_candidate.tile_id
				|| coarse_state.goal_candidate.tile_x != coarse_state.start_candidate.tile_x
				|| coarse_state.goal_candidate.tile_y != coarse_state.start_candidate.tile_y ) ) {
			nav2_corridor_tile_ref_t goalTileRef = {};
			goalTileRef.tile_x = ( int32_t )coarse_state.goal_candidate.tile_x;
			goalTileRef.tile_y = ( int32_t )coarse_state.goal_candidate.tile_y;
			goalTileRef.tile_id = coarse_state.goal_candidate.tile_id;
			out_corridor->exact_tile_route.push_back( goalTileRef );
		}
	}
	out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_EXACT_TILE_ROUTE;

	/**
	*	Finalize refinement-policy flags based on collected memberships and mover commitments.
	**/
	// Enable membership flags when allow-lists were populated.
	if ( !out_corridor->refine_policy.allowed_leaf_indices.empty() ) {
		out_corridor->refine_policy.flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_LEAF_MEMBERSHIP;
	}
	if ( !out_corridor->refine_policy.allowed_cluster_ids.empty() ) {
		out_corridor->refine_policy.flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_CLUSTER_MEMBERSHIP;
	}
	if ( !out_corridor->refine_policy.allowed_area_ids.empty() ) {
		out_corridor->refine_policy.flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_AREA_MEMBERSHIP;
	}

	// Record mover constraints when mover references were captured.
	if ( !out_corridor->refine_policy.mover_refs.empty() ) {
		out_corridor->refine_policy.flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT;
	}

	/**
	*	Mark provisional results so downstream refinement can revalidate corridor commitments.
	**/
	// Mark partial corridors whenever the coarse solver did not finish successfully.
	if ( !coarse_state.query_state.has_provisional_result || coarse_state.status != nav2_coarse_astar_status_t::Success ) {
		out_corridor->flags |= NAV_CORRIDOR_FLAG_PARTIAL_RESULT;
	}

	return !out_corridor->segments.empty();
}

/**
*	@brief	Emit a bounded debug summary for a corridor extracted from a coarse A* state.
*	@param	coarse_state	Coarse A* state supplying the reconstructed path.
*	@param	max_segments	Maximum number of explicit corridor segments to emit.
*	@return	True when a corridor was extracted and reported.
*	@note	This keeps Task 8.2 diagnostics aligned with the nav2-owned corridor extraction path.
**/
const bool SVG_Nav2_DebugPrintCorridorFromCoarseAStar( const nav2_coarse_astar_state_t &coarse_state, const int32_t max_segments ) {
	/**
	*	Sanity check: require a coarse state with a reconstructed path before reporting.
	**/
	// Bail out early when the coarse state cannot yield a corridor.
	if ( coarse_state.nodes.empty() || coarse_state.path.node_ids.empty() ) {
		return false;
	}

	/**
	*	Build a corridor from the coarse state so the debug output stays aligned with Task 8.2 extraction logic.
	**/
	// Construct the corridor that will be reported.
	nav2_corridor_t corridor = {};
	if ( !SVG_Nav2_BuildCorridorFromCoarseAStar( coarse_state, &corridor ) ) {
		return false;
	}

	/**
	*	Emit a bounded corridor summary using the existing debug draw seam.
	**/
	// Clamp the segment count so debug output remains bounded.
	const int32_t clampedSegments = std::max( 0, std::min( max_segments, 32 ) );
	SVG_Nav2_DebugDrawCorridor( corridor,
		clampedSegments > 0 ? nav2_debug_corridor_verbosity_t::IncludeSegments : nav2_debug_corridor_verbosity_t::SummaryOnly,
		clampedSegments );
	return true;
}

/**
*	@brief	Keep the nav2 corridor-extraction helper module represented in the build.
**/
void SVG_Nav2_CorridorBuild_ModuleAnchor( void ) {
}
