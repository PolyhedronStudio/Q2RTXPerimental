/********************************************************************
*
*
*	ServerGame: Nav2 Connector Extraction - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_connectors.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>


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

    // Establish a conservative Z band around the mover so corridor extraction can use the connector as a route commitment.
    out_connector->allowed_min_z = worldOrigin.z - 32.0;
    out_connector->allowed_max_z = worldOrigin.z + 32.0;
    out_connector->base_cost = ( out_connector->connector_kind & NAV2_CONNECTOR_KIND_MOVER_RIDE ) ? 10.0 : 1.0;
    out_connector->policy_penalty = ( out_connector->connector_kind & NAV2_CONNECTOR_KIND_DOOR_TRANSITION ) ? 2.0 : 0.0;
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
    out_connector->connector_kind = NAV2_CONNECTOR_KIND_PORTAL;
    out_connector->allowed_min_z = std::min( spanA.floor_z, spanB.floor_z );
    out_connector->allowed_max_z = std::max( spanA.ceiling_z, spanB.ceiling_z );
    out_connector->base_cost = 1.0;
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

    // Build trivial anchors in the absence of world-space derivation.	out_connector->start = SVG_Nav2_MakeConnectorAnchor( startRef, Vector3{ 0.0, 0.0, spanA.preferred_z }, Vector3{ 0.0, 0.0, spanA.preferred_z } );
    out_connector->end = SVG_Nav2_MakeConnectorAnchor( endRef, Vector3{ 0.0, 0.0, spanB.preferred_z }, Vector3{ 0.0, 0.0, spanB.preferred_z } );
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

    // Scan adjacent spans inside each column so local vertical transitions become explicit connectors.
    for ( const nav2_span_column_t &column : grid.columns ) {
        for ( size_t i = 0; i + 1 < column.spans.size(); ++i ) {
            const nav2_span_t &spanA = column.spans[ i ];
            const nav2_span_t &spanB = column.spans[ i + 1 ];
            nav2_connector_t connector = {};
            if ( SVG_Nav2_TryBuildSpanConnector( spanA, spanB, &connector ) ) {
                connector.connector_kind |= ( spanA.topology_flags & spanB.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) ? NAV2_CONNECTOR_KIND_LADDER_ENDPOINT : NAV2_CONNECTOR_KIND_STAIR_BAND;
                connector.connector_kind |= ( spanA.surface_flags & NAV_TILE_SUMMARY_WALK_OFF ) ? NAV2_CONNECTOR_KIND_OPENING : NAV2_CONNECTOR_KIND_NONE;
                SVG_Nav2_ConnectorList_Append( out_list, connector );
            }
        }
    }

    return !out_list->connectors.empty();
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

    // Print a compact header so the caller can see aggregate counts at a glance.
    gi.dprintf( "[NAV2][Connectors] count=%d report=%d\n", ( int32_t )list.connectors.size(), std::min( ( int32_t )list.connectors.size(), limit ) );
    const int32_t reportCount = std::min( ( int32_t )list.connectors.size(), limit );
    for ( int32_t i = 0; i < reportCount; ++i ) {
        const nav2_connector_t &connector = list.connectors[ ( size_t )i ];
        gi.dprintf( "[NAV2][Connectors] id=%d kind=0x%08x ent=%d model=%d spanA=%d spanB=%d z=(%.1f..%.1f) cost=%.1f penalty=%.1f avail=%d reusable=%d\n",
            connector.connector_id,
            connector.connector_kind,
            connector.owner_entnum,
            connector.inline_model_index,
            connector.start.span_ref.span_id,
            connector.end.span_ref.span_id,
            connector.allowed_min_z,
            connector.allowed_max_z,
            connector.base_cost,
            connector.policy_penalty,
            connector.dynamically_available ? 1 : 0,
            connector.reusable ? 1 : 0 );
    }
}

/**
*	@brief	Keep the nav2 connector module represented in the build.
**/
void SVG_Nav2_Connectors_ModuleAnchor( void ) {
}
