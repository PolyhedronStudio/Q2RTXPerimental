/********************************************************************
*
*
*	ServerGame: Nav2 Sparse Occupancy and Dynamic Overlay Pruning - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_occupancy.h"

#include <algorithm>
#include <limits>


/**
*	@brief\tAllocate the next stable occupancy id in a grid.
*	@param\tgrid\tGrid to inspect.
*	@return\tStable id for the next appended record.
**/
static int32_t SVG_Nav2_AllocateOccupancyId( const nav2_occupancy_grid_t &grid ) {
    // Keep occupancy ids monotonic so queries and diagnostics remain deterministic.
    int32_t nextId = 1;
    for ( const nav2_occupancy_record_t &record : grid.records ) {
        nextId = std::max( nextId, record.occupancy_id + 1 );
    }
    return nextId;
}

/**
*	@brief\tAllocate the next stable overlay id in a collection.
*	@param\toverlay\tOverlay collection to inspect.
*	@return\tStable id for the next appended overlay entry.
**/
static int32_t SVG_Nav2_AllocateOverlayId( const nav2_dynamic_overlay_t &overlay ) {
    // Keep overlay ids monotonic so debug output and updates remain stable.
    int32_t nextId = 1;
    for ( const nav2_occupancy_overlay_entry_t &entry : overlay.entries ) {
        nextId = std::max( nextId, entry.overlay_id + 1 );
    }
    return nextId;
}

/**
*	@brief\tReturn whether the span should be treated as hard-blocked by contents or policy.
*	@param\tspan\tSpan to inspect.
*	@param\tout_soft_penalty\t[out] Soft penalty accumulated while classifying the span.
*	@return\tTrue when the span is considered hard blocked.
**/
static const bool SVG_Nav2_ClassifySpanOccupancy( const nav2_span_t &span, double *out_soft_penalty ) {
    // Initialize the soft-penalty output before any classification.
    if ( out_soft_penalty ) {
        *out_soft_penalty = 0.0;
    }

    // Water and slime are soft-priority penalties; lava and explicit hard walls block outright.
    if ( ( span.surface_flags & NAV_TILE_SUMMARY_LAVA ) != 0 || ( span.topology_flags & NAV_TRAVERSAL_FEATURE_JUMP_OBSTRUCTION_PRESENCE ) != 0 ) {
        return true;
    }
    if ( ( span.surface_flags & NAV_TILE_SUMMARY_WATER ) != 0 || ( span.surface_flags & NAV_TILE_SUMMARY_SLIME ) != 0 ) {
        if ( out_soft_penalty ) {
            *out_soft_penalty += ( ( span.surface_flags & NAV_TILE_SUMMARY_WATER ) != 0 ) ? 1.5 : 2.5;
        }
    }
    if ( ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LEDGE_ADJACENCY ) != 0 || ( span.topology_flags & NAV_TRAVERSAL_FEATURE_WALL_ADJACENCY ) != 0 ) {
        if ( out_soft_penalty ) {
            *out_soft_penalty += 0.5;
        }
    }
    return false;
}

/**
*	@brief\tWrite an occupancy record into the grid and update reverse indices.
*	@param\tgrid\tOccupancy grid to mutate.
*	@param\trecord\tRecord to append.
*	@return\tTrue when the record was stored.
**/
static const bool SVG_Nav2_StoreOccupancyRecord( nav2_occupancy_grid_t *grid, const nav2_occupancy_record_t &record ) {
    // Reject invalid inputs so the sparse record table stays deterministic.
    if ( !grid || record.occupancy_id < 0 ) {
        return false;
    }

    // Append the record and register its stable lookups.
    const int32_t index = ( int32_t )grid->records.size();
    grid->records.push_back( record );
    if ( record.span_id >= 0 ) {
        grid->by_span_id[ record.span_id ] = index;
    }
    if ( record.edge_id >= 0 ) {
        grid->by_edge_id[ record.edge_id ] = index;
    }
    if ( record.connector_id >= 0 ) {
        grid->by_connector_id[ record.connector_id ] = index;
    }
    if ( record.mover_entnum >= 0 ) {
        grid->by_mover_entnum[ record.mover_entnum ] = index;
    }
    return true;
}

/**
*	@brief\tWrite an overlay entry into the collection and update its reverse index.
*	@param\toverlay\tOverlay collection to mutate.
*	@param\tentry\tEntry to append.
*	@return\tTrue when the entry was stored.
**/
static const bool SVG_Nav2_StoreOverlayEntry( nav2_dynamic_overlay_t *overlay, const nav2_occupancy_overlay_entry_t &entry ) {
    // Reject invalid inputs so the overlay table stays deterministic.
    if ( !overlay || entry.overlay_id < 0 ) {
        return false;
    }

    // Append the overlay entry and register the stable lookup.
    const int32_t index = ( int32_t )overlay->entries.size();
    overlay->entries.push_back( entry );
    overlay->by_overlay_id[ entry.overlay_id ] = index;
    return true;
}

/**
*	@brief\tEvaluate a topology reference against the overlay collection.
*	@param\toverlay\tOverlay collection to inspect.
*	@param\ttopology	Topology reference to match.
*	@param\tmover_ref	Optional mover reference for mover-local pruning.
*	@param\tout_soft_penalty	[out] Soft penalty accumulated while classifying the overlay.
*	@return\tTrue when a matching overlay record should hard-block traversal.
**/
static const bool SVG_Nav2_EvaluateOverlayTopology( const nav2_dynamic_overlay_t &overlay, const nav2_corridor_topology_ref_t &topology,
    const nav2_corridor_mover_ref_t *mover_ref, double *out_soft_penalty ) {
    // Search the overlay collection for the first matching topology record.
    for ( const nav2_occupancy_overlay_entry_t &entry : overlay.entries ) {
        if ( entry.topology.portal_id >= 0 && topology.portal_id >= 0 && entry.topology.portal_id == topology.portal_id ) {
            if ( out_soft_penalty ) {
                *out_soft_penalty += entry.soft_penalty;
            }
            if ( entry.decision == nav2_occupancy_decision_t::HardBlock ) {
                return true;
            }
            if ( entry.decision == nav2_occupancy_decision_t::SoftPenalty ) {
                if ( out_soft_penalty ) {
                    *out_soft_penalty += entry.soft_penalty;
                }
            }
        }
        if ( mover_ref && entry.mover_ref.IsValid() && mover_ref->IsValid() && entry.mover_ref.owner_entnum == mover_ref->owner_entnum && entry.mover_ref.model_index == mover_ref->model_index ) {
            if ( entry.decision == nav2_occupancy_decision_t::HardBlock ) {
                return true;
            }
            if ( entry.decision == nav2_occupancy_decision_t::SoftPenalty && out_soft_penalty ) {
                *out_soft_penalty += entry.soft_penalty;
            }
        }
    }
    return false;
}

/**
*	@brief\tClassify one fine-search node against occupancy and overlay data.
*	@param\tgrid\tSparse occupancy grid.
*	@param\toverlay\tDynamic overlay collection.
*	@param\tnode\tFine-search node to inspect.
*	@param\tmover_ref	Optional mover reference for mover-local pruning.
*	@param\tout_summary	[out] Optional summary for diagnostics.
*	@return\tTrue when the node remains traversable.
**/
static const bool SVG_Nav2_EvaluateFineNodeLocal( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay,
    const nav2_fine_astar_node_t &node, const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary ) {
    // Start with a permissive assumption and tighten it as records match the node.
    nav2_occupancy_decision_t decision = nav2_occupancy_decision_t::Free;
    double softPenalty = 0.0;

    // Check any span-backed occupancy record first because it is the most specific local signal.
    if ( node.span_id >= 0 ) {
        const auto it = grid.by_span_id.find( node.span_id );
        if ( it != grid.by_span_id.end() ) {
            const nav2_occupancy_record_t &record = grid.records[ ( size_t )it->second ];
            if ( record.decision == nav2_occupancy_decision_t::HardBlock ) {
                decision = nav2_occupancy_decision_t::HardBlock;
            }
            else if ( record.decision == nav2_occupancy_decision_t::SoftPenalty ) {
                decision = nav2_occupancy_decision_t::SoftPenalty;
                softPenalty += record.soft_penalty;
            }
            if ( !record.allowed_z_band.IsValid() || ( node.allowed_z_band.IsValid() && ( node.allowed_z_band.max_z < record.allowed_z_band.min_z || node.allowed_z_band.min_z > record.allowed_z_band.max_z ) ) ) {
                decision = nav2_occupancy_decision_t::HardBlock;
            }
        }
    }

    // Check the topology overlay next so portal and mover invalidation can prune locally without rebuilding the hierarchy.
    if ( decision != nav2_occupancy_decision_t::HardBlock ) {
        const bool overlayBlocked = SVG_Nav2_EvaluateOverlayTopology( overlay, node.topology, mover_ref, &softPenalty );
        if ( overlayBlocked ) {
            decision = nav2_occupancy_decision_t::HardBlock;
        }
    }

    // Apply mover-local pruning if the node carries a mover identity that no longer matches the active candidate.
    if ( decision != nav2_occupancy_decision_t::HardBlock && mover_ref && node.mover_entnum >= 0 ) {
        if ( node.mover_entnum != mover_ref->owner_entnum || ( node.inline_model_index >= 0 && node.inline_model_index != mover_ref->model_index ) ) {
            decision = nav2_occupancy_decision_t::Revalidate;
        }
    }

    // Record the summary and decide the final traversability result.
    if ( out_summary ) {
        if ( decision == nav2_occupancy_decision_t::HardBlock ) {
            out_summary->hard_block_count++;
        }
        else if ( decision == nav2_occupancy_decision_t::SoftPenalty ) {
            out_summary->soft_penalty_count++;
        }
        else if ( decision == nav2_occupancy_decision_t::Revalidate ) {
            out_summary->revalidate_count++;
        }
        else {
            out_summary->free_count++;
        }
    }
    return decision != nav2_occupancy_decision_t::HardBlock;
}


/**
*
*
*	Nav2 Occupancy Public API:
*
*
**/
/**
*\t@brief\tClear a sparse occupancy grid to an empty state.
*\t@param\tgrid\tOccupancy grid to reset.
**/
void SVG_Nav2_OccupancyGrid_Clear( nav2_occupancy_grid_t *grid ) {
    // Guard against null output storage.
    if ( !grid ) {
        return;
    }
    grid->records.clear();
    grid->by_span_id.clear();
    grid->by_edge_id.clear();
    grid->by_connector_id.clear();
    grid->by_mover_entnum.clear();
}

/**
*\t@brief\tClear a dynamic overlay collection to an empty state.
*\t@param\toverlay\tOverlay collection to reset.
**/
void SVG_Nav2_DynamicOverlay_Clear( nav2_dynamic_overlay_t *overlay ) {
    // Guard against null output storage.
    if ( !overlay ) {
        return;
    }
    overlay->entries.clear();
    overlay->by_overlay_id.clear();
}

/**
*\t@brief\tAppend or update a sparse occupancy record.
*\t@param\tgrid\tOccupancy grid to mutate.
*\t@param\trecord\tRecord payload to append.
*\t@return\tTrue when the record was stored.
**/
const bool SVG_Nav2_OccupancyGrid_Upsert( nav2_occupancy_grid_t *grid, const nav2_occupancy_record_t &record ) {
    // Reject null grids and incomplete records.
    if ( !grid || record.occupancy_id < 0 ) {
        return false;
    }

    // Prefer replacing an existing slot so updates remain stable.
    const auto it = std::find_if( grid->records.begin(), grid->records.end(), [ & ]( const nav2_occupancy_record_t &candidate ) { return candidate.occupancy_id == record.occupancy_id; } );
    if ( it != grid->records.end() ) {
        *it = record;
        return true;
    }
    return SVG_Nav2_StoreOccupancyRecord( grid, record );
}

/**
*\t@brief\tAppend or update a dynamic overlay entry.
*\t@param\toverlay\tOverlay collection to mutate.
*\t@param\tentry\tOverlay payload to append.
*\t@return\tTrue when the entry was stored.
**/
const bool SVG_Nav2_DynamicOverlay_Upsert( nav2_dynamic_overlay_t *overlay, const nav2_occupancy_overlay_entry_t &entry ) {
    // Reject null overlays and incomplete entries.
    if ( !overlay || entry.overlay_id < 0 ) {
        return false;
    }

    // Prefer replacing an existing slot so overlay updates remain stable.
    const auto it = std::find_if( overlay->entries.begin(), overlay->entries.end(), [ & ]( const nav2_occupancy_overlay_entry_t &candidate ) { return candidate.overlay_id == entry.overlay_id; } );
    if ( it != overlay->entries.end() ) {
        *it = entry;
        return true;
    }
    return SVG_Nav2_StoreOverlayEntry( overlay, entry );
}

/**
*\t@brief\tResolve a sparse occupancy record by span id.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\tspan_id\tStable span id to resolve.
*\t@param\tout_record\t[out] Resolved occupancy record.
*\t@return\tTrue when a matching record exists.
**/
const bool SVG_Nav2_OccupancyGrid_TryResolveSpan( const nav2_occupancy_grid_t &grid, const int32_t span_id, nav2_occupancy_record_t *out_record ) {
    // Require an output record so callers receive a stable copy.
    if ( !out_record ) {
        return false;
    }
    *out_record = {};
    const auto it = grid.by_span_id.find( span_id );
    if ( it == grid.by_span_id.end() ) {
        return false;
    }
    *out_record = grid.records[ ( size_t )it->second ];
    return true;
}

/**
*\t@brief\tResolve a sparse occupancy record by edge id.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\tedge_id\tStable edge id to resolve.
*\t@param\tout_record\t[out] Resolved occupancy record.
*\t@return\tTrue when a matching record exists.
**/
const bool SVG_Nav2_OccupancyGrid_TryResolveEdge( const nav2_occupancy_grid_t &grid, const int32_t edge_id, nav2_occupancy_record_t *out_record ) {
    // Require an output record so callers receive a stable copy.
    if ( !out_record ) {
        return false;
    }
    *out_record = {};
    const auto it = grid.by_edge_id.find( edge_id );
    if ( it == grid.by_edge_id.end() ) {
        return false;
    }
    *out_record = grid.records[ ( size_t )it->second ];
    return true;
}

/**
*\t@brief\tEvaluate a span against the sparse occupancy grid and dynamic overlay.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\toverlay\tOverlay collection to inspect.
*\t@param\tspan\tSpan being considered for pruning.
*\t@param\tmover_ref\tOptional mover reference for mover-local pruning.
*\t@param\tout_summary\t[out] Optional summary receiving query results.
*\t@return\tTrue when the span remains traversable after local pruning.
**/
const bool SVG_Nav2_EvaluateSpanOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_span_t &span,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary ) {
    // Initialize a local summary if the caller asked for one.
    nav2_occupancy_summary_t localSummary = {};
    nav2_occupancy_summary_t *summaryPtr = out_summary ? out_summary : &localSummary;
    summaryPtr->span_count++;

    // Classify the span itself before applying overlay pruning.
    double softPenalty = 0.0;
    const bool hardBlocked = SVG_Nav2_ClassifySpanOccupancy( span, &softPenalty );
    if ( hardBlocked ) {
        summaryPtr->hard_block_count++;
        return false;
    }
    if ( softPenalty > 0.0 ) {
        summaryPtr->soft_penalty_count++;
    }

    // Convert the span into a transient occupancy record so the sparse grid lookup can be reused.
    nav2_occupancy_record_t record = {};
    record.occupancy_id = 1;
    record.kind = nav2_occupancy_kind_t::Span;
    record.span_id = span.span_id;
    record.leaf_id = span.leaf_id;
    record.cluster_id = span.cluster_id;
    record.area_id = span.area_id;
    record.decision = softPenalty > 0.0 ? nav2_occupancy_decision_t::SoftPenalty : nav2_occupancy_decision_t::Free;
    record.soft_penalty = softPenalty;
    record.allowed_z_band.min_z = span.floor_z;
    record.allowed_z_band.max_z = span.ceiling_z;
    record.allowed_z_band.preferred_z = span.preferred_z;
    if ( span.leaf_id >= 0 ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_LEAF;
    }
    if ( span.cluster_id >= 0 ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_CLUSTER;
    }
    if ( span.area_id >= 0 ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_AREA;
    }
    if ( ( span.connector_hint_mask != 0 ) || ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_REQUIRES_REVALIDATION;
    }

    // Apply the sparse occupancy grid lookup and keep the caller visible summary current.
    const auto gridIt = grid.by_span_id.find( span.span_id );
    if ( gridIt != grid.by_span_id.end() ) {
        const nav2_occupancy_record_t &existing = grid.records[ ( size_t )gridIt->second ];
        if ( existing.decision == nav2_occupancy_decision_t::HardBlock ) {
            summaryPtr->hard_block_count++;
            return false;
        }
        if ( existing.decision == nav2_occupancy_decision_t::SoftPenalty ) {
            summaryPtr->soft_penalty_count++;
            softPenalty += existing.soft_penalty;
        }
    }

    // Apply dynamic overlay pruning to catch localized invalidation without rebuilding the whole hierarchy.
    if ( SVG_Nav2_EvaluateOverlayTopology( overlay, record.allowed_z_band.min_z >= 0.0 ? nav2_corridor_topology_ref_t{} : nav2_corridor_topology_ref_t{}, mover_ref, &softPenalty ) ) {
        summaryPtr->hard_block_count++;
        return false;
    }

    // Apply mover-local pruning if the candidate is attached to a specific mover and the caller supplied a current mover reference.
    if ( mover_ref && mover_ref->IsValid() && record.flags & NAV2_OCCUPANCY_FLAG_REQUIRES_REVALIDATION ) {
        if ( record.decision == nav2_occupancy_decision_t::Revalidate ) {
            summaryPtr->revalidate_count++;
        }
    }

    return true;
}

/**
*\t@brief\tEvaluate a corridor segment against the sparse occupancy grid and dynamic overlay.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\toverlay\tOverlay collection to inspect.
*\t@param\tsegment\tCorridor segment being considered.
*\t@param\tmover_ref\tOptional mover reference for mover-local pruning.
*\t@param\tout_summary\t[out] Optional summary receiving query results.
*\t@return\tTrue when the segment remains compatible after local pruning.
**/
const bool SVG_Nav2_EvaluateCorridorSegmentOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_corridor_segment_t &segment,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary ) {
    // Initialize a local summary if the caller asked for one.
    nav2_occupancy_summary_t localSummary = {};
    nav2_occupancy_summary_t *summaryPtr = out_summary ? out_summary : &localSummary;
    summaryPtr->edge_count++;

    // Create a temporary record that mirrors the corridor segment for sparse occupancy lookups.
    nav2_occupancy_record_t record = {};
    record.occupancy_id = 1;
    record.kind = nav2_occupancy_kind_t::Edge;
    record.edge_id = segment.segment_id;
    record.connector_id = segment.topology.portal_id;
    record.mover_entnum = segment.mover_ref.owner_entnum;
    record.inline_model_index = segment.mover_ref.model_index;
    record.leaf_id = segment.topology.leaf_index;
    record.cluster_id = segment.topology.cluster_id;
    record.area_id = segment.topology.area_id;
    record.allowed_z_band = segment.allowed_z_band;
    record.decision = ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ? nav2_occupancy_decision_t::HardBlock
        : ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED ) != 0 ? nav2_occupancy_decision_t::SoftPenalty
        : nav2_occupancy_decision_t::Free;
    record.soft_penalty = segment.policy_penalty;
    if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_LEAF_MEMBERSHIP ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_LEAF;
    }
    if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_CLUSTER_MEMBERSHIP ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_CLUSTER;
    }
    if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_AREA_MEMBERSHIP ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_AREA;
    }
    if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_MOVER;
    }
    if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HAS_PORTAL;
    }
    if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HARD_BLOCKED;
    }
    if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_SOFT_PENALIZED;
    }

    // Reuse the overlay query so portal and mover invalidation remains local.
    if ( SVG_Nav2_EvaluateOverlayTopology( overlay, segment.topology, mover_ref, &record.soft_penalty ) ) {
        summaryPtr->hard_block_count++;
        return false;
    }

    // Apply sparse-grid occupancy if the segment can be resolved to a known edge identifier.
    if ( record.edge_id >= 0 ) {
        const auto it = grid.by_edge_id.find( record.edge_id );
        if ( it != grid.by_edge_id.end() ) {
            const nav2_occupancy_record_t &existing = grid.records[ ( size_t )it->second ];
            if ( existing.decision == nav2_occupancy_decision_t::HardBlock ) {
                summaryPtr->hard_block_count++;
                return false;
            }
            if ( existing.decision == nav2_occupancy_decision_t::SoftPenalty ) {
                summaryPtr->soft_penalty_count++;
                record.soft_penalty += existing.soft_penalty;
            }
        }
    }

    // Apply mover-local pruning when the segment is attached to a mover-specific route.
    if ( mover_ref && mover_ref->IsValid() && segment.mover_ref.IsValid() ) {
        if ( mover_ref->owner_entnum != segment.mover_ref.owner_entnum || mover_ref->model_index != segment.mover_ref.model_index ) {
            summaryPtr->revalidate_count++;
        }
    }

    if ( record.decision == nav2_occupancy_decision_t::HardBlock ) {
        summaryPtr->hard_block_count++;
        return false;
    }
    if ( record.decision == nav2_occupancy_decision_t::SoftPenalty ) {
        summaryPtr->soft_penalty_count++;
    }
    return true;
}

/**
*\t@brief\tEvaluate a fine-search node against occupancy and dynamic overlay state.
*\t@param\tgrid\tOccupancy grid to inspect.
*\t@param\toverlay\tOverlay collection to inspect.
*\t@param\tnode\tFine-search node being considered.
*\t@param\tmover_ref\tOptional mover reference for mover-local pruning.
*\t@param\tout_summary\t[out] Optional summary receiving query results.
*\t@return\tTrue when the node remains compatible after local pruning.
**/
const bool SVG_Nav2_EvaluateFineNodeOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_fine_astar_node_t &node,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary ) {
    // Convert the fine-search node into a transient span-shaped record so the local pruning path stays consistent.
    nav2_span_t span = {};
    span.span_id = node.span_id;
    span.floor_z = node.allowed_z_band.min_z;
    span.ceiling_z = node.allowed_z_band.max_z;
    span.preferred_z = node.allowed_z_band.preferred_z;
    span.leaf_id = node.topology.leaf_index;
    span.cluster_id = node.topology.cluster_id;
    span.area_id = node.topology.area_id;
    span.region_layer_id = node.topology.region_id;
    span.topology_flags = 0;
    span.surface_flags = 0;
    if ( node.flags & NAV2_FINE_ASTAR_NODE_FLAG_HAS_MOVER ) {
        span.connector_hint_mask |= 1u;
    }
    if ( node.flags & NAV2_FINE_ASTAR_NODE_FLAG_PARTIAL ) {
        span.topology_flags |= NAV_TRAVERSAL_FEATURE_LEDGE_ADJACENCY;
    }
    return SVG_Nav2_EvaluateSpanOccupancy( grid, overlay, span, mover_ref, out_summary );
}

/**
*\t@brief\tMerge a span-grid occupancy source into the sparse occupancy grid.
*\t@param\tgrid\tOccupancy grid to mutate.
*\t@param\tspan_grid	Span grid source to translate.
*\t@param\tstamp_frame	Frame stamp to record on imported records.
*\t@return\tTrue when at least one record was imported.
**/
const bool SVG_Nav2_ImportSpanGridOccupancy( nav2_occupancy_grid_t *grid, const nav2_span_grid_t &span_grid, const int64_t stamp_frame ) {
    // Reject null grids or empty span sources.
    if ( !grid || span_grid.columns.empty() ) {
        return false;
    }

    // Translate each span into a sparse occupancy record.
    bool imported = false;
    for ( const nav2_span_column_t &column : span_grid.columns ) {
        for ( const nav2_span_t &span : column.spans ) {
            nav2_occupancy_record_t record = {};
            record.occupancy_id = SVG_Nav2_AllocateOccupancyId( *grid );
            record.kind = nav2_occupancy_kind_t::Span;
            record.span_id = span.span_id;
            record.leaf_id = span.leaf_id;
            record.cluster_id = span.cluster_id;
            record.area_id = span.area_id;
            record.allowed_z_band.min_z = span.floor_z;
            record.allowed_z_band.max_z = span.ceiling_z;
            record.allowed_z_band.preferred_z = span.preferred_z;
            record.stamp_frame = stamp_frame;
            double softPenalty = 0.0;
            const bool hardBlock = SVG_Nav2_ClassifySpanOccupancy( span, &softPenalty );
            record.decision = hardBlock ? nav2_occupancy_decision_t::HardBlock : ( softPenalty > 0.0 ? nav2_occupancy_decision_t::SoftPenalty : nav2_occupancy_decision_t::Free );
            record.soft_penalty = softPenalty;
            if ( hardBlock ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_HARD_BLOCKED;
            }
            if ( softPenalty > 0.0 ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_SOFT_PENALIZED;
            }
            if ( span.leaf_id >= 0 ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_HAS_LEAF;
            }
            if ( span.cluster_id >= 0 ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_HAS_CLUSTER;
            }
            if ( span.area_id >= 0 ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_HAS_AREA;
            }
            SVG_Nav2_StoreOccupancyRecord( grid, record );
            imported = true;
        }
    }
    return imported;
}

/**
*\t@brief\tMerge a corridor into sparse occupancy and overlay summaries.
*\t@param\tgrid\tOccupancy grid to mutate.
*\t@param\tcorridor	Corridor to import.
*\t@param\tstamp_frame	Frame stamp to record on imported records.
*\t@return\tTrue when at least one record was imported.
**/
const bool SVG_Nav2_ImportCorridorOccupancy( nav2_occupancy_grid_t *grid, const nav2_corridor_t &corridor, const int64_t stamp_frame ) {
    // Reject null grids or empty corridor sources.
    if ( !grid || corridor.segments.empty() ) {
        return false;
    }

    // Import each corridor segment as a sparse occupancy record so future pruning can reuse the same table.
    bool imported = false;
    for ( const nav2_corridor_segment_t &segment : corridor.segments ) {
        nav2_occupancy_record_t record = {};
        record.occupancy_id = SVG_Nav2_AllocateOccupancyId( *grid );
        record.kind = nav2_occupancy_kind_t::Edge;
        record.edge_id = segment.segment_id;
        record.connector_id = segment.topology.portal_id;
        record.mover_entnum = segment.mover_ref.owner_entnum;
        record.inline_model_index = segment.mover_ref.model_index;
        record.leaf_id = segment.topology.leaf_index;
        record.cluster_id = segment.topology.cluster_id;
        record.area_id = segment.topology.area_id;
        record.allowed_z_band = segment.allowed_z_band;
        record.stamp_frame = stamp_frame;
        record.decision = ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ? nav2_occupancy_decision_t::HardBlock
            : ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED ) != 0 ? nav2_occupancy_decision_t::SoftPenalty
            : nav2_occupancy_decision_t::Free;
        record.soft_penalty = segment.policy_penalty;
        if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_LEAF_MEMBERSHIP ) {
            record.flags |= NAV2_OCCUPANCY_FLAG_HAS_LEAF;
        }
        if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_CLUSTER_MEMBERSHIP ) {
            record.flags |= NAV2_OCCUPANCY_FLAG_HAS_CLUSTER;
        }
        if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_AREA_MEMBERSHIP ) {
            record.flags |= NAV2_OCCUPANCY_FLAG_HAS_AREA;
        }
        if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF ) {
            record.flags |= NAV2_OCCUPANCY_FLAG_HAS_MOVER;
        }
        if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID ) {
            record.flags |= NAV2_OCCUPANCY_FLAG_HAS_PORTAL;
        }
        if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) {
            record.flags |= NAV2_OCCUPANCY_FLAG_HARD_BLOCKED;
        }
        if ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED ) {
            record.flags |= NAV2_OCCUPANCY_FLAG_SOFT_PENALIZED;
        }
        SVG_Nav2_StoreOccupancyRecord( grid, record );
        imported = true;
    }
    return imported;
}

/**
*\t@brief\tEmit a bounded debug summary for occupancy and overlay data.
*\t@param\tgrid\tOccupancy grid to report.
*\t@param\toverlay\tDynamic overlay to report.
*\t@param\tlimit\tMaximum number of entries to print.
**/
void SVG_Nav2_DebugPrintOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const int32_t limit ) {
    // Skip diagnostics when nothing useful could be printed.
    if ( limit <= 0 ) {
        return;
    }

    // Print a stable summary header first.
    gi.dprintf( "[NAV2][Occupancy] records=%d overlays=%d limit=%d\n", ( int32_t )grid.records.size(), ( int32_t )overlay.entries.size(), limit );
    const int32_t reportCount = std::min( ( int32_t )grid.records.size(), limit );
    for ( int32_t i = 0; i < reportCount; ++i ) {
        const nav2_occupancy_record_t &record = grid.records[ ( size_t )i ];
        gi.dprintf( "[NAV2][Occupancy] id=%d kind=%d span=%d edge=%d conn=%d mover=%d leaf=%d cluster=%d area=%d decision=%d soft=%.2f flags=0x%08x z=(%.1f..%.1f) stamp=%lld\n",
            record.occupancy_id,
            ( int32_t )record.kind,
            record.span_id,
            record.edge_id,
            record.connector_id,
            record.mover_entnum,
            record.leaf_id,
            record.cluster_id,
            record.area_id,
            ( int32_t )record.decision,
            record.soft_penalty,
            record.flags,
            record.allowed_z_band.min_z,
            record.allowed_z_band.max_z,
            ( long long )record.stamp_frame );
    }
}

/**
*\t@brief\tKeep the nav2 occupancy module represented in the build.
**/
void SVG_Nav2_Occupancy_ModuleAnchor( void ) {
}
