/********************************************************************
*
*
*	ServerGame: Nav2 Connector Extraction - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_connectors.h"

#include "svgame/nav2/nav2_span_adjacency.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_set>


/**
*
*
*	Nav2 Connector Local Helpers:
*
*
**/
/**
*	@brief	Allocate the next stable connector id within a list.
*	@param	list	Connector list to inspect.
*	@return	Stable id chosen for the next appended connector.
**/
static int32_t SVG_Nav2_AllocateConnectorId( const nav2_connector_list_t &list ) {
    // Start after the current highest connector id.
    int32_t nextId = 1;
		
    // Scan existing connectors so we keep ids stable and monotonic.
    for ( const nav2_connector_t &connector : list.connectors ) {
        nextId = std::max( nextId, connector.connector_id + 1 );
    }

    return nextId;
}

/**
*	@brief	Build a topology anchor from a span reference and world position.
*	@param	spanRef	Pointer-free span reference to attach.
*	@param	worldOrigin	World-space anchor position.
*	@param	localOrigin	Mover-local anchor position.
*	@return	Anchor populated with stable topology metadata.
**/
static nav2_connector_anchor_t SVG_Nav2_MakeConnectorAnchor( const nav2_span_ref_t &spanRef, const Vector3 &worldOrigin, const Vector3 &localOrigin ) {
    // Keep anchor construction explicit so serialization-friendly fields remain obvious.
    nav2_connector_anchor_t anchor = {};
    anchor.span_ref = spanRef;
    anchor.world_origin = worldOrigin;
    anchor.local_origin = localOrigin;
    anchor.valid = spanRef.IsValid();
    return anchor;
}

/**
*\t@brief\tPopulate anchor topology metadata from one resolved span.
*\t@param\tspan\tResolved span payload.
*\t@param\tanchor\t[in,out] Anchor receiving topology fields.
**/
static void SVG_Nav2_Connector_ApplySpanTopology( const nav2_span_t &span, nav2_connector_anchor_t *anchor ) {
    // Require output storage before mutating the anchor metadata.
    if ( !anchor ) {
        return;
    }

    // Copy stable BSP topology identity from the source span.
    anchor->leaf_id = span.leaf_id;
    anchor->cluster_id = span.cluster_id;
    anchor->area_id = span.area_id;
    anchor->valid = true;
}

/**
*	@brief	Return explicit transition semantics implied by connector kind bits.
*	@param	connectorKind	Connector kind bitmask to translate.
*	@return	Stable transition semantic bitmask for downstream hierarchy and corridor code.
**/
static uint32_t SVG_Nav2_Connector_BuildTransitionSemanticsFromKind( const uint32_t connectorKind ) {
    // Start empty and accumulate only the semantics proven by connector extraction.
    uint32_t semantics = NAV2_CONNECTOR_TRANSITION_NONE;

    // Preserve explicit topology crossing semantics first.
    if ( ( connectorKind & NAV2_CONNECTOR_KIND_PORTAL ) != 0 || ( connectorKind & NAV2_CONNECTOR_KIND_DOOR_TRANSITION ) != 0 ) {
        semantics |= NAV2_CONNECTOR_TRANSITION_PORTAL;
    }
    if ( ( connectorKind & NAV2_CONNECTOR_KIND_OPENING ) != 0 ) {
        semantics |= NAV2_CONNECTOR_TRANSITION_OPENING;
    }

    // Preserve explicit vertical-routing commitments.
    if ( ( connectorKind & NAV2_CONNECTOR_KIND_LADDER_ENDPOINT ) != 0 ) {
        semantics |= NAV2_CONNECTOR_TRANSITION_VERTICAL | NAV2_CONNECTOR_TRANSITION_LADDER;
    }
    if ( ( connectorKind & NAV2_CONNECTOR_KIND_STAIR_BAND ) != 0 ) {
        semantics |= NAV2_CONNECTOR_TRANSITION_VERTICAL | NAV2_CONNECTOR_TRANSITION_STAIR;
    }

    // Preserve mover traversal semantics independently so later policy can distinguish lifts from generic portals.
    if ( ( connectorKind & ( NAV2_CONNECTOR_KIND_MOVER_BOARDING | NAV2_CONNECTOR_KIND_MOVER_RIDE | NAV2_CONNECTOR_KIND_MOVER_EXIT ) ) != 0 ) {
        semantics |= NAV2_CONNECTOR_TRANSITION_MOVER;
    }

    return semantics;
}

/**
*	@brief	Apply explicit endpoint semantics to one connector anchor pair.
*	@param	connector	[in,out] Connector receiving semantic endpoint metadata.
*	@note	This keeps ladder top/bottom and mover boarding/exit roles explicit even when the
*			anchors share the same entity origin, so downstream policy can stay pointer-free.
**/
static void SVG_Nav2_Connector_AssignEndpointSemantics( nav2_connector_t *connector ) {
    // Require output storage before assigning endpoint roles.
    if ( !connector ) {
        return;
    }

    // Reset endpoint semantics before rebuilding them from current connector state.
    connector->start.endpoint_semantics = NAV2_CONNECTOR_ENDPOINT_NONE;
    connector->end.endpoint_semantics = NAV2_CONNECTOR_ENDPOINT_NONE;

    // Door and portal-style barriers mark both anchors as barrier endpoints.
    if ( ( connector->connector_kind & NAV2_CONNECTOR_KIND_PORTAL ) != 0 || ( connector->connector_kind & NAV2_CONNECTOR_KIND_DOOR_TRANSITION ) != 0 ) {
        connector->start.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_PORTAL_BARRIER;
        connector->end.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_PORTAL_BARRIER;
    }

    // Ladder transitions require an explicit bottom and top endpoint based on preferred anchor height.
    if ( ( connector->connector_kind & NAV2_CONNECTOR_KIND_LADDER_ENDPOINT ) != 0 ) {
        if ( connector->start.world_origin.z <= connector->end.world_origin.z ) {
            connector->start.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_LADDER_BOTTOM;
            connector->end.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_LADDER_TOP;
        } else {
            connector->start.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_LADDER_TOP;
            connector->end.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_LADDER_BOTTOM;
        }
    }

    // Stair transitions also preserve low-to-high entry and exit intent for later routing policy.
    if ( ( connector->connector_kind & NAV2_CONNECTOR_KIND_STAIR_BAND ) != 0 ) {
        if ( connector->start.world_origin.z <= connector->end.world_origin.z ) {
            connector->start.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_STAIR_ENTRY;
            connector->end.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_STAIR_EXIT;
        } else {
            connector->start.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_STAIR_EXIT;
            connector->end.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_STAIR_ENTRY;
        }
    }

    // Mover traversal semantics preserve the staged boarding, ride, and exit phases explicitly.
    if ( ( connector->connector_kind & NAV2_CONNECTOR_KIND_MOVER_BOARDING ) != 0 ) {
        connector->start.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_MOVER_BOARDING;
    }
    if ( ( connector->connector_kind & NAV2_CONNECTOR_KIND_MOVER_RIDE ) != 0 ) {
        connector->start.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_MOVER_RIDE;
        connector->end.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_MOVER_RIDE;
    }
    if ( ( connector->connector_kind & NAV2_CONNECTOR_KIND_MOVER_EXIT ) != 0 ) {
        connector->end.endpoint_semantics |= NAV2_CONNECTOR_ENDPOINT_MOVER_EXIT;
    }
}

/**
*\t@brief\tBuild world-space anchor coordinates for one sparse span slot.
*\t@param\tgrid\tSparse precision grid containing the source span.
*\t@param\tcolumn\tOwning sparse column.
*\t@param\tspan\tSpan payload to anchor.
*\t@param\tcolumn_index\tStable column index used by pointer-free span references.
*\t@param\tspan_index\tStable span index inside the owning column.
*\t@return\tConnector anchor carrying span and BSP metadata.
**/
static nav2_connector_anchor_t SVG_Nav2_Connector_MakeSpanAnchor( const nav2_span_grid_t &grid, const nav2_span_column_t &column,
    const nav2_span_t &span, const int32_t column_index, const int32_t span_index ) {
    // Convert sparse XY coordinates into a deterministic world-space column center.
    const double cellSizeXY = grid.cell_size_xy > 0.0 ? grid.cell_size_xy : 32.0;
    const Vector3 worldOrigin = {
        ( float )( ( ( double )column.tile_x + 0.5 ) * cellSizeXY ),
        ( float )( ( ( double )column.tile_y + 0.5 ) * cellSizeXY ),
        ( float )span.preferred_z
    };

    // Build a pointer-free span reference so connectors remain persistence-friendly.
    nav2_span_ref_t spanRef = {};
    spanRef.span_id = span.span_id;
    spanRef.column_index = column_index;
    spanRef.span_index = span_index;

    // Start from world/local parity because span anchors are world-derived in this pass.
    nav2_connector_anchor_t anchor = SVG_Nav2_MakeConnectorAnchor( spanRef, worldOrigin, worldOrigin );
    SVG_Nav2_Connector_ApplySpanTopology( span, &anchor );
    return anchor;
}

/**
*\t@brief\tBuild a conservative area-portal availability state for one connector.
*\t@param\tconnector\t[in,out] Connector receiving availability metadata.
**/
static void SVG_Nav2_Connector_UpdateAreaPortalAvailability( nav2_connector_t *connector ) {
    // Require output storage before mutating availability flags.
    if ( !connector ) {
        return;
    }

    // Start from available and only downgrade when area-portal checks prove disconnection.
    connector->dynamically_available = true;
    connector->availability_version = 0;

    // Require area connectivity imports and valid area ids before evaluating dynamic availability.
    if ( !gi.AreasConnected || connector->start.area_id < 0 || connector->end.area_id < 0 ) {
        return;
    }

    // Mark connectors unavailable when area connectivity is currently blocked.
    if ( !gi.AreasConnected( connector->start.area_id, connector->end.area_id ) ) {
        connector->dynamically_available = false;
        connector->availability_version = 1;
    }
}

/**
*\t@brief\tClassify one span transition connector kind from span semantics and vertical delta.
*\t@param\tspanA\tSource span.
*\t@param\tspanB\tDestination span.
*\t@return\tConnector kind bitmask for the span transition.
**/
static uint32_t SVG_Nav2_Connector_ClassifySpanTransitionKind( const nav2_span_t &spanA, const nav2_span_t &spanB ) {
    // Start from a portal/opening transition because every span connector implies a coarse topology crossing.
    uint32_t kindBits = NAV2_CONNECTOR_KIND_PORTAL;

    // Prefer explicit ladder endpoint semantics when either span exposes ladder traversal features.
    if ( ( spanA.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 || ( spanB.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0
        || ( spanA.surface_flags & NAV_TILE_SUMMARY_LADDER ) != 0 || ( spanB.surface_flags & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
        kindBits |= NAV2_CONNECTOR_KIND_LADDER_ENDPOINT;
        return kindBits;
    }

    // Prefer explicit stair-band semantics when stair metadata or meaningful step deltas are present.
    const double verticalDelta = std::fabs( spanB.preferred_z - spanA.preferred_z );
    if ( ( spanA.topology_flags & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0 || ( spanB.topology_flags & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0
        || ( spanA.surface_flags & NAV_TILE_SUMMARY_STAIR ) != 0 || ( spanB.surface_flags & NAV_TILE_SUMMARY_STAIR ) != 0
        || verticalDelta > 8.0 ) {
        kindBits |= NAV2_CONNECTOR_KIND_STAIR_BAND;
    }

    // Mark opening transitions when either side carries explicit walk-off/opening-like semantics.
    if ( ( spanA.surface_flags & NAV_TILE_SUMMARY_WALK_OFF ) != 0 || ( spanB.surface_flags & NAV_TILE_SUMMARY_WALK_OFF ) != 0 ) {
        kindBits |= NAV2_CONNECTOR_KIND_OPENING;
    }

    return kindBits;
}

/**
*\t@brief\tBuild movement restriction bits for one span-derived connector.
*\t@param\tspanA\tSource span.
*\t@param\tspanB\tDestination span.
*\t@return\tMovement restriction feature bits.
**/
static uint32_t SVG_Nav2_Connector_BuildSpanRestrictions( const nav2_span_t &spanA, const nav2_span_t &spanB ) {
    // Start with no restrictions and widen from observed transition semantics.
    uint32_t restrictions = NAV_EDGE_FEATURE_NONE;

    // Preserve explicit walk-off caution and hard-wall semantics from both spans.
    if ( ( spanA.surface_flags & NAV_TILE_SUMMARY_WALK_OFF ) != 0 || ( spanB.surface_flags & NAV_TILE_SUMMARY_WALK_OFF ) != 0 ) {
        restrictions |= NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF;
    }
    if ( ( spanA.surface_flags & NAV_TILE_SUMMARY_HARD_WALL ) != 0 || ( spanB.surface_flags & NAV_TILE_SUMMARY_HARD_WALL ) != 0 ) {
        restrictions |= NAV_EDGE_FEATURE_HARD_WALL_BLOCKED;
    }

    // Preserve liquid transitions so connector-aware coarse routing can penalize hazardous transitions early.
    if ( ( spanA.movement_flags & NAV_FLAG_WATER ) != 0 || ( spanB.movement_flags & NAV_FLAG_WATER ) != 0 ) {
        restrictions |= NAV_EDGE_FEATURE_ENTERS_WATER;
    }
    if ( ( spanA.movement_flags & NAV_FLAG_LAVA ) != 0 || ( spanB.movement_flags & NAV_FLAG_LAVA ) != 0 ) {
        restrictions |= NAV_EDGE_FEATURE_ENTERS_LAVA;
    }
    if ( ( spanA.movement_flags & NAV_FLAG_SLIME ) != 0 || ( spanB.movement_flags & NAV_FLAG_SLIME ) != 0 ) {
        restrictions |= NAV_EDGE_FEATURE_ENTERS_SLIME;
    }

    return restrictions;
}

/**
*	@brief	Create a connector record from one inline BSP entity classification.
*	@param	ent	Entity being inspected.
*	@param	entityInfo	Navigation classification for the entity.
*	@param	in_transform	Optional mover-local transform.
*	@param	out_connector	[out] Connector record receiving the extracted data.
*	@return	True when the entity produced a connector candidate.
**/
static const bool SVG_Nav2_TryBuildEntityConnector( const svg_base_edict_t *ent, const nav2_inline_bsp_entity_info_t &entityInfo, const nav2_mover_transform_t *in_transform, nav2_connector_t *out_connector ) {
    // Validate inputs before touching output storage.
    if ( !ent || !out_connector ) {
        return false;
    }

    // Clear the output so failed paths cannot leak stale state.
    *out_connector = {};

    // Only classify entities that actually expose inline BSP geometry.
    if ( !entityInfo.has_inline_model ) {
        return false;
    }

    // Assign stable identifiers and source metadata.
    out_connector->owner_entnum = entityInfo.owner_entnum;
    out_connector->inline_model_index = entityInfo.inline_model_index;
    out_connector->source_role_flags = entityInfo.role_flags;
    out_connector->transition_semantics = NAV2_CONNECTOR_TRANSITION_NONE;
    out_connector->dynamically_available = true;
    out_connector->reusable = entityInfo.headnode_valid;

    // Doors become timed connectors and explicit openings.
    if ( strcmp( ent->classname.ptr, "func_door" ) == 0 || strcmp( ent->classname.ptr, "func_door_rotating" ) == 0 ) {
        out_connector->connector_kind |= NAV2_CONNECTOR_KIND_DOOR_TRANSITION | NAV2_CONNECTOR_KIND_PORTAL | NAV2_CONNECTOR_KIND_OPENING;
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF;
    }

    // Platforms expose boarding, ride, and exit semantics.
    if ( strcmp( ent->classname.ptr, "func_plat" ) == 0 ) {
        out_connector->connector_kind |= NAV2_CONNECTOR_KIND_MOVER_BOARDING | NAV2_CONNECTOR_KIND_MOVER_RIDE | NAV2_CONNECTOR_KIND_MOVER_EXIT;
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF;
    }

    // Rotating brushes become hazard-aware mover transitions.
    if ( strcmp( ent->classname.ptr, "func_rotating" ) == 0 ) {
        out_connector->connector_kind |= NAV2_CONNECTOR_KIND_MOVER_RIDE | NAV2_CONNECTOR_KIND_MOVER_EXIT;
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_JUMP_OBSTRUCTION;
    }

    // Wall-style brush entities can still act as pass-through opening connectors.
    if ( strcmp( ent->classname.ptr, "func_wall" ) == 0 ) {
        out_connector->connector_kind |= NAV2_CONNECTOR_KIND_PORTAL | NAV2_CONNECTOR_KIND_OPENING;
    }

    // Default mover semantics: any traversal-capable brush should still produce a usable connector.
    if ( out_connector->connector_kind == NAV2_CONNECTOR_KIND_NONE ) {
        if ( ( entityInfo.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_TRAVERSABLE ) != 0 ) {
            out_connector->connector_kind |= NAV2_CONNECTOR_KIND_MOVER_BOARDING | NAV2_CONNECTOR_KIND_MOVER_RIDE | NAV2_CONNECTOR_KIND_MOVER_EXIT;
        } else if ( ( entityInfo.role_flags & NAV2_INLINE_BSP_ROLE_TIMED_CONNECTOR ) != 0 ) {
            out_connector->connector_kind |= NAV2_CONNECTOR_KIND_DOOR_TRANSITION | NAV2_CONNECTOR_KIND_PORTAL;
        } else if ( ( entityInfo.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_SURFACE ) != 0 ) {
            out_connector->connector_kind |= NAV2_CONNECTOR_KIND_MOVER_BOARDING;
        } else {
            // Non-traversable movers are still useful as explicit openings for hierarchy invalidation.
            out_connector->connector_kind |= NAV2_CONNECTOR_KIND_OPENING;
        }
    }

    // Anchor the connector at the mover origin when we have a transform snapshot.
    Vector3 worldOrigin = ent->currentOrigin;
    Vector3 localOrigin = ent->currentOrigin;
    if ( in_transform && in_transform->is_valid ) {
        localOrigin = in_transform->local_origin;
    }

    // Use the entity number as a stable and deterministic anchor id source when the caller did not provide span-local topology.
    nav2_span_ref_t entitySpanRef = {};
    entitySpanRef.span_id = entityInfo.inline_model_index;
    entitySpanRef.column_index = entityInfo.owner_entnum;
    entitySpanRef.span_index = 0;
    out_connector->start = SVG_Nav2_MakeConnectorAnchor( entitySpanRef, worldOrigin, localOrigin );
    out_connector->end = out_connector->start;

    // Mirror coarse topology metadata so area-portal checks can determine dynamic connector availability.
    out_connector->start.area_id = ent->areaNumber0;
    out_connector->end.area_id = ent->areaNumber1 >= 0 ? ent->areaNumber1 : ent->areaNumber0;
    out_connector->start.leaf_id = entityInfo.headnode_valid ? entityInfo.inline_model_index : -1;
    out_connector->end.leaf_id = out_connector->start.leaf_id;
    out_connector->start.cluster_id = -1;
    out_connector->end.cluster_id = -1;

    // Downgrade availability when area-portal connectivity currently blocks this connector.
    SVG_Nav2_Connector_UpdateAreaPortalAvailability( out_connector );

    // Establish a conservative Z band around the mover so corridor extraction can use the connector as a route commitment.
    out_connector->allowed_min_z = worldOrigin.z - 32.0;
    out_connector->allowed_max_z = worldOrigin.z + 32.0;
    out_connector->base_cost = ( out_connector->connector_kind & NAV2_CONNECTOR_KIND_MOVER_RIDE ) ? 10.0 : 1.0;
    out_connector->policy_penalty = ( out_connector->connector_kind & NAV2_CONNECTOR_KIND_DOOR_TRANSITION ) ? 2.0 : 0.0;
    out_connector->transition_semantics = SVG_Nav2_Connector_BuildTransitionSemanticsFromKind( out_connector->connector_kind );
    SVG_Nav2_Connector_AssignEndpointSemantics( out_connector );
    return true;
}

/**
*	@brief	Build a connector record from a span-grid row transition.
*	@param	spanA	First span participating in the transition.
*	@param	spanB	Second span participating in the transition.
*	@param	out_connector	[out] Connector record receiving the extracted data.
*	@return	True when the span pair forms a connector candidate.
**/
static const bool SVG_Nav2_TryBuildSpanConnector( const nav2_span_t &spanA, const nav2_span_t &spanB, nav2_connector_t *out_connector ) {
    // Only valid spans can form topology connectors.
    if ( !out_connector || !SVG_Nav2_SpanIsValid( spanA ) || !SVG_Nav2_SpanIsValid( spanB ) ) {
        return false;
    }

    // Clear output before populating the connector candidate.
    *out_connector = {};
    out_connector->connector_kind = SVG_Nav2_Connector_ClassifySpanTransitionKind( spanA, spanB );
    out_connector->allowed_min_z = std::min( spanA.floor_z, spanB.floor_z );
    out_connector->allowed_max_z = std::max( spanA.ceiling_z, spanB.ceiling_z );
    out_connector->base_cost = 1.0;
    out_connector->movement_restrictions = SVG_Nav2_Connector_BuildSpanRestrictions( spanA, spanB );
    out_connector->transition_semantics = SVG_Nav2_Connector_BuildTransitionSemanticsFromKind( out_connector->connector_kind );
    out_connector->reusable = true;

    // Derive a stable span reference pair from the source spans.
    nav2_span_ref_t startRef = {};
    startRef.span_id = spanA.span_id;
    startRef.column_index = spanA.leaf_id;
    startRef.span_index = spanA.cluster_id;

    nav2_span_ref_t endRef = {};
    endRef.span_id = spanB.span_id;
    endRef.column_index = spanB.leaf_id;
    endRef.span_index = spanB.cluster_id;

    // Build trivial anchors in the absence of world-space derivation.
    const Vector3 spanAOrigin = { 0.f, 0.f, ( float )spanA.preferred_z };
    const Vector3 spanBOrigin = { 0.f, 0.f, ( float )spanB.preferred_z };
    out_connector->start = SVG_Nav2_MakeConnectorAnchor( startRef, spanAOrigin, spanAOrigin );
    out_connector->end = SVG_Nav2_MakeConnectorAnchor( endRef, spanBOrigin, spanBOrigin );
    SVG_Nav2_Connector_ApplySpanTopology( spanA, &out_connector->start );
    SVG_Nav2_Connector_ApplySpanTopology( spanB, &out_connector->end );
    SVG_Nav2_Connector_AssignEndpointSemantics( out_connector );

    // Span connectors default to available unless area-portal checks prove otherwise.
    SVG_Nav2_Connector_UpdateAreaPortalAvailability( out_connector );
    return true;
}

/**
 *	@brief	Build one span-derived connector from a validated local adjacency edge.
 *	@param	grid		Sparse span grid owning the source and destination span slots.
 *	@param	edge		Adjacency edge being promoted into a connector candidate.
 *	@param	out_connector	[out] Connector receiving the translated adjacency semantics.
 *	@return	True when the adjacency edge produced a reusable connector candidate.
 **/
static const bool SVG_Nav2_TryBuildSpanConnectorFromAdjacencyEdge( const nav2_span_grid_t &grid, const nav2_span_edge_t &edge,
    nav2_connector_t *out_connector ) {
    /**
    *	Require output storage and skip edges that the adjacency builder already classified as hard-invalid.
    **/
    if ( !out_connector || edge.legality == nav2_span_edge_legality_t::HardInvalid ) {
        return false;
    }

    /**
    *	Resolve stable source and destination sparse-column/span slots from the pointer-free adjacency metadata.
    **/
    if ( edge.from_column_index < 0 || edge.to_column_index < 0
        || edge.from_column_index >= ( int32_t )grid.columns.size()
        || edge.to_column_index >= ( int32_t )grid.columns.size() ) {
        return false;
    }

    const nav2_span_column_t &fromColumn = grid.columns[ ( size_t )edge.from_column_index ];
    const nav2_span_column_t &toColumn = grid.columns[ ( size_t )edge.to_column_index ];
    if ( edge.from_span_index < 0 || edge.to_span_index < 0
        || edge.from_span_index >= ( int32_t )fromColumn.spans.size()
        || edge.to_span_index >= ( int32_t )toColumn.spans.size() ) {
        return false;
    }

    const nav2_span_t &fromSpan = fromColumn.spans[ ( size_t )edge.from_span_index ];
    const nav2_span_t &toSpan = toColumn.spans[ ( size_t )edge.to_span_index ];
    if ( fromSpan.span_id < 0 || toSpan.span_id < 0 ) {
        return false;
    }

    /**
    *	Start from the existing span-pair connector translation so stair/ladder/portal semantics remain centralized.
    **/
    if ( !SVG_Nav2_TryBuildSpanConnector( fromSpan, toSpan, out_connector ) ) {
        return false;
    }

    /**
    *	Re-anchor the connector to the actual source and destination sparse-column world positions so corridor and
    *	hierarchy stages can preserve the directional spatial relationship instead of collapsing both endpoints to one column.
    **/
    out_connector->start = SVG_Nav2_Connector_MakeSpanAnchor( grid, fromColumn, fromSpan, edge.from_column_index, edge.from_span_index );
    out_connector->end = SVG_Nav2_Connector_MakeSpanAnchor( grid, toColumn, toSpan, edge.to_column_index, edge.to_span_index );

    /**
    *	Mirror adjacency-derived legality and movement restrictions onto the connector so later hierarchy and corridor policy
    *	can distinguish soft walk-off/liquid caution edges from ordinary horizontal passages.
    **/
    out_connector->base_cost = std::max( out_connector->base_cost, edge.cost );
    out_connector->policy_penalty += std::max( 0.0, edge.soft_penalty_cost );
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_STEP_UP ) != 0 ) {
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_STAIR_STEP_UP;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_STEP_DOWN ) != 0 ) {
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_STAIR_STEP_DOWN;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_ENTERS_WATER ) != 0 ) {
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_ENTERS_WATER;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_ENTERS_LAVA ) != 0 ) {
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_ENTERS_LAVA;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_ENTERS_SLIME ) != 0 ) {
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_ENTERS_SLIME;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_OPTIONAL_WALK_OFF ) != 0 ) {
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF;
    }
    if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_FORBIDDEN_WALK_OFF ) != 0 ) {
        out_connector->movement_restrictions |= NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF;
    }

    /**
    *	Recompute endpoint semantics after re-anchoring so vertical directionality stays tied to the final world-space anchors.
    **/
    out_connector->transition_semantics = SVG_Nav2_Connector_BuildTransitionSemanticsFromKind( out_connector->connector_kind );
    SVG_Nav2_Connector_AssignEndpointSemantics( out_connector );
    SVG_Nav2_Connector_UpdateAreaPortalAvailability( out_connector );
    return true;
}


/**
*
*
*	Nav2 Connector Public API:
*
*
**/
/**
*	@brief	Clear a connector list to an empty state.
*	@param	list	Connector collection to reset.
**/
void SVG_Nav2_ConnectorList_Clear( nav2_connector_list_t *list ) {
    // Guard against null output storage.
    if ( !list ) {
        return;
    }

    // Reset the containers to remove stale connector state.
    list->connectors.clear();
    list->by_id.clear();
}

/**
*	@brief	Append a connector to a connector list and register its stable id.
*	@param	list	Connector collection to mutate.
*	@param	connector	Connector payload to append.
*	@return	True when the connector was appended.
**/
const bool SVG_Nav2_ConnectorList_Append( nav2_connector_list_t *list, const nav2_connector_t &connector ) {
    // Validate the target list before mutating it.
    if ( !list || connector.connector_kind == NAV2_CONNECTOR_KIND_NONE ) {
        return false;
    }

    // Copy the connector into the collection.
    nav2_connector_t stored = connector;
    if ( stored.connector_id < 0 ) {
        stored.connector_id = SVG_Nav2_AllocateConnectorId( *list );
    }

    // Append and register the stable id in one place so lookups stay deterministic.
    const int32_t index = ( int32_t )list->connectors.size();
    list->connectors.push_back( stored );
    list->by_id[ stored.connector_id ] = { stored.connector_id, index };
    return true;
}

/**
*	@brief	Resolve a connector by stable id.
*	@param	list	Connector collection to inspect.
*	@param	connector_id	Stable connector id to resolve.
*	@param	out_ref	[out] Resolved connector reference.
*	@return	True when the connector exists.
**/
const bool SVG_Nav2_ConnectorList_TryResolve( const nav2_connector_list_t &list, const int32_t connector_id, nav2_connector_ref_t *out_ref ) {
    // Require a valid output pointer and a resolvable id.
    if ( !out_ref ) {
        return false;
    }

    // Clear the output before attempting lookup.
    *out_ref = {};
    const auto it = list.by_id.find( connector_id );
    if ( it == list.by_id.end() ) {
        return false;
    }

    *out_ref = it->second;
    return out_ref->IsValid();
}

/**
*	@brief	Build a single connector candidate from one inline BSP entity.
*	@param	ent	Entity being inspected.
*	@param	entityInfo	Navigation classification for the entity.
*	@param	in_transform	Optional mover-local transform when available.
*	@param	out_connector	[out] Connector payload receiving the extracted state.
*	@return	True when the entity produced a connector candidate.
**/
const bool SVG_Nav2_BuildConnectorFromEntity( const svg_base_edict_t *ent, const nav2_inline_bsp_entity_info_t &entityInfo, const nav2_mover_transform_t *in_transform, nav2_connector_t *out_connector ) {
    // Build the entity-derived connector and reuse the helper above for the actual extraction.
    return SVG_Nav2_TryBuildEntityConnector( ent, entityInfo, in_transform, out_connector );
}

/**
*	@brief	Extract connector candidates from sparse span-grid topology.
*	@param	grid	Sparse precision grid to inspect.
*	@param	out_list	[out] Connector list receiving extracted connectors.
*	@return	True when at least one span-derived connector was produced.
**/
const bool SVG_Nav2_ExtractSpanConnectors( const nav2_span_grid_t &grid, nav2_connector_list_t *out_list ) {
    // Clear the output before populating new results.
    SVG_Nav2_ConnectorList_Clear( out_list );
    if ( !out_list || grid.columns.empty() ) {
        return false;
    }

    // Track emitted span-pair keys so duplicate connectors are not emitted across repeated scans.
    std::unordered_set<uint64_t> emittedPairs = {};

    // Scan adjacent spans inside each column so local vertical transitions remain explicit once the span builder grows multi-band columns.
    for ( int32_t columnIndex = 0; columnIndex < ( int32_t )grid.columns.size(); columnIndex++ ) {
        const nav2_span_column_t &column = grid.columns[ ( size_t )columnIndex ];
        for ( size_t i = 0; i + 1 < column.spans.size(); ++i ) {
            const nav2_span_t &spanA = column.spans[ i ];
            const nav2_span_t &spanB = column.spans[ i + 1 ];

            // Skip unstable span ids because connector persistence depends on stable ids.
            if ( spanA.span_id < 0 || spanB.span_id < 0 ) {
                continue;
            }

            // Build a deterministic pair key and skip duplicates.
            const int32_t minSpanId = std::min( spanA.span_id, spanB.span_id );
            const int32_t maxSpanId = std::max( spanA.span_id, spanB.span_id );
            const uint64_t pairKey = ( ( uint64_t )( uint32_t )minSpanId << 32 ) | ( uint64_t )( uint32_t )maxSpanId;
            if ( emittedPairs.find( pairKey ) != emittedPairs.end() ) {
                continue;
            }

            nav2_connector_t connector = {};
            if ( SVG_Nav2_TryBuildSpanConnector( spanA, spanB, &connector ) ) {
                // Re-anchor span connectors with resolved world-space positions and stable column/span indices.
                connector.start = SVG_Nav2_Connector_MakeSpanAnchor( grid, column, spanA, columnIndex, ( int32_t )i );
                connector.end = SVG_Nav2_Connector_MakeSpanAnchor( grid, column, spanB, columnIndex, ( int32_t )i + 1 );

                // Mark this span-pair as emitted once the connector candidate is accepted.
                emittedPairs.insert( pairKey );
                SVG_Nav2_ConnectorList_Append( out_list, connector );
            }
        }
    }

    /**
    *	Build local span adjacency and translate passable/soft-penalized inter-column transitions into connector candidates.
    *	This is the primary path for current generated meshes, which typically emit one span per sparse column.
    **/
    nav2_span_adjacency_t adjacency = {};
    if ( SVG_Nav2_BuildSpanAdjacency( grid, &adjacency ) ) {
        for ( const nav2_span_edge_t &edge : adjacency.edges ) {
            // Skip edges that do not carry stable span ids or were already rejected by the adjacency legality pass.
            if ( edge.from_span_id < 0 || edge.to_span_id < 0 || edge.legality == nav2_span_edge_legality_t::HardInvalid ) {
                continue;
            }

            // Deduplicate the undirected span pair so bidirectional adjacency edges do not emit duplicate connectors.
            const int32_t minSpanId = std::min( edge.from_span_id, edge.to_span_id );
            const int32_t maxSpanId = std::max( edge.from_span_id, edge.to_span_id );
            const uint64_t pairKey = ( ( uint64_t )( uint32_t )minSpanId << 32 ) | ( uint64_t )( uint32_t )maxSpanId;
            if ( emittedPairs.find( pairKey ) != emittedPairs.end() ) {
                continue;
            }

            nav2_connector_t connector = {};
            if ( SVG_Nav2_TryBuildSpanConnectorFromAdjacencyEdge( grid, edge, &connector ) ) {
                emittedPairs.insert( pairKey );
                SVG_Nav2_ConnectorList_Append( out_list, connector );
            }
        }
    }

    return !out_list->connectors.empty();
}

/**
*\t@brief\tBuild a bounded connector-type summary for diagnostics and runtime validation.
*\t@param\tlist\tConnector list to summarize.
*\t@param\tout_summary\t[out] Compact summary counters.
*\t@return\tTrue when the summary was produced.
**/
const bool SVG_Nav2_BuildConnectorSummary( const nav2_connector_list_t &list, nav2_connector_summary_t *out_summary ) {
    // Require output storage before writing summary counters.
    if ( !out_summary ) {
        return false;
    }

    // Reset output storage so callers never observe stale counters.
    *out_summary = {};

    // Iterate every connector once and accumulate compact kind/availability counters.
    for ( const nav2_connector_t &connector : list.connectors ) {
        out_summary->total_count++;

        // Track each connector kind explicitly so Task 6.3 coverage remains visible in bounded diagnostics.
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_PORTAL ) != 0 || ( connector.connector_kind & NAV2_CONNECTOR_KIND_OPENING ) != 0 ) {
            out_summary->portal_count++;
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_STAIR_BAND ) != 0 ) {
            out_summary->stair_count++;
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_LADDER_ENDPOINT ) != 0 ) {
            out_summary->ladder_count++;
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_DOOR_TRANSITION ) != 0 ) {
            out_summary->door_count++;
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_BOARDING ) != 0 ) {
            out_summary->mover_boarding_count++;
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_RIDE ) != 0 ) {
            out_summary->mover_ride_count++;
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_EXIT ) != 0 ) {
            out_summary->mover_exit_count++;
        }

        // Track source-type and availability coverage.
        if ( connector.owner_entnum >= 0 ) {
            out_summary->entity_connector_count++;
        } else {
            out_summary->span_connector_count++;
        }
        if ( !connector.dynamically_available ) {
            out_summary->unavailable_count++;
        }
        if ( connector.reusable ) {
            out_summary->reusable_count++;
        }
    }

    return true;
}

/**
*	@brief	Extract connector candidates from inline BSP entities.
*	@param	entities	Entity set to inspect.
*	@param	out_list	[out] Connector list receiving extracted connectors.
*	@return	True when at least one entity-derived connector was produced.
**/
const bool SVG_Nav2_ExtractEntityConnectors( const std::vector<svg_base_edict_t *> &entities, nav2_connector_list_t *out_list ) {
    // Reset the destination so results remain deterministic.
    SVG_Nav2_ConnectorList_Clear( out_list );
    if ( !out_list ) {
        return false;
    }

    // Walk every supplied entity and classify it through the nav2 entity semantics seam.
    for ( svg_base_edict_t *ent : entities ) {
        if ( !ent ) {
            continue;
        }

        nav2_inline_bsp_entity_info_t info = {};
        if ( !SVG_Nav2_ClassifyInlineBspEntity( ent, &info ) ) {
            continue;
        }

        nav2_mover_transform_t transform = {};
        const nav2_mover_transform_t *transformPtr = nullptr;
        if ( SVG_Nav2_BuildMoverTransform( info, ent, &transform ) ) {
            transformPtr = &transform;
        }

        nav2_connector_t connector = {};
        if ( SVG_Nav2_BuildConnectorFromEntity( ent, info, transformPtr, &connector ) ) {
            SVG_Nav2_ConnectorList_Append( out_list, connector );
        }
    }

    return !out_list->connectors.empty();
}

/**
*	@brief	Extract connector candidates from both sparse spans and inline BSP entities.
*	@param	grid	Sparse precision grid to inspect.
*	@param	entities	Entity set to inspect.
*	@param	out_list	[out] Connector list receiving extracted connectors.
*	@return	True when at least one connector was produced.
**/
const bool SVG_Nav2_ExtractConnectors( const nav2_span_grid_t &grid, const std::vector<svg_base_edict_t *> &entities, nav2_connector_list_t *out_list ) {
    // Merge both extraction sources into one canonical connector list.
    SVG_Nav2_ConnectorList_Clear( out_list );
    if ( !out_list ) {
        return false;
    }

    nav2_connector_list_t spanConnectors = {};
    SVG_Nav2_ExtractSpanConnectors( grid, &spanConnectors );
    for ( const nav2_connector_t &connector : spanConnectors.connectors ) {
        SVG_Nav2_ConnectorList_Append( out_list, connector );
    }

    nav2_connector_list_t entityConnectors = {};
    SVG_Nav2_ExtractEntityConnectors( entities, &entityConnectors );
    for ( const nav2_connector_t &connector : entityConnectors.connectors ) {
        SVG_Nav2_ConnectorList_Append( out_list, connector );
    }

    return !out_list->connectors.empty();
}

/**
*	@brief	Emit a bounded debug summary for connector extraction results.
*	@param	list	Connector list to report.
*	@param	limit	Maximum number of connectors to print.
**/
void SVG_Nav2_DebugPrintConnectors( const nav2_connector_list_t &list, const int32_t limit ) {
    // Skip diagnostics when there is nothing sensible to report.
    if ( limit <= 0 ) {
        return;
    }

    // Build one compact summary payload so high-level connector coverage stays visible.
    nav2_connector_summary_t summary = {};
    SVG_Nav2_BuildConnectorSummary( list, &summary );

    // Print a compact header so the caller can see aggregate counts at a glance.
    gi.dprintf( "[NAV2][Connectors] count=%d report=%d portal=%d stair=%d ladder=%d door=%d mover(board=%d ride=%d exit=%d) span=%d entity=%d unavailable=%d reusable=%d\n",
        summary.total_count,
        std::min( summary.total_count, limit ),
        summary.portal_count,
        summary.stair_count,
        summary.ladder_count,
        summary.door_count,
        summary.mover_boarding_count,
        summary.mover_ride_count,
        summary.mover_exit_count,
        summary.span_connector_count,
        summary.entity_connector_count,
        summary.unavailable_count,
        summary.reusable_count );
    const int32_t reportCount = std::min( ( int32_t )list.connectors.size(), limit );
    for ( int32_t i = 0; i < reportCount; ++i ) {
        const nav2_connector_t &connector = list.connectors[ ( size_t )i ];
        gi.dprintf( "[NAV2][Connectors] id=%d kind=0x%08x ent=%d model=%d spanA=%d spanB=%d topo=(leaf:%d/%d cluster:%d/%d area:%d/%d) z=(%.1f..%.1f) cost=%.1f penalty=%.1f restrict=0x%08x avail=%d ver=%u reusable=%d\n",
            connector.connector_id,
            connector.connector_kind,
            connector.owner_entnum,
            connector.inline_model_index,
            connector.start.span_ref.span_id,
            connector.end.span_ref.span_id,
            connector.start.leaf_id,
            connector.end.leaf_id,
            connector.start.cluster_id,
            connector.end.cluster_id,
            connector.start.area_id,
            connector.end.area_id,
            connector.allowed_min_z,
            connector.allowed_max_z,
            connector.base_cost,
            connector.policy_penalty,
            connector.movement_restrictions,
            connector.dynamically_available ? 1 : 0,
            connector.availability_version,
            connector.reusable ? 1 : 0 );
    }
}

/**
*	@brief	Keep the nav2 connector module represented in the build.
**/
void SVG_Nav2_Connectors_ModuleAnchor( void ) {
}
