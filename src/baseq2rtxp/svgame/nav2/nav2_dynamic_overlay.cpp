/********************************************************************
*
*
*	ServerGame: Nav2 Dynamic Edge Cost and Availability Overlay - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_dynamic_overlay.h"

#include "svgame/nav2/nav2_memory.h"

#include <algorithm>
#include <limits>


/**
*
*
*	Nav2 Dynamic Overlay Local Helpers:
*
*
**/
/**
*	@brief	Append one stable index into a reverse-index bucket.
*	@param	value	Stable key value for the reverse-index channel.
*	@param	entry_index	Stable entry index to append.
*	@param	channel	Reverse-index channel to mutate.
**/
static void SVG_Nav2_DynamicOverlay_AppendChannelIndex( const int32_t value, const int32_t entry_index,
    std::unordered_map<int32_t, std::vector<int32_t>> *channel ) {
    /**
    *	Require a valid channel, entry index, and key before appending.
    **/
    if ( !channel || value < 0 || entry_index < 0 ) {
        return;
    }

    /**
    *	Append the entry index so localized invalidation can touch only affected entries.
    **/
    ( *channel )[ value ].push_back( entry_index );
}

/**
*	@brief	Rebuild all localized reverse-index channels from the current entry array.
*	@param	cache	Overlay cache to rebuild.
**/
static void SVG_Nav2_DynamicOverlay_RebuildReverseIndices( nav2_dynamic_edge_overlay_cache_t *cache ) {
    /**
    *	Require mutable cache storage before rebuilding reverse-index channels.
    **/
    if ( !cache ) {
        return;
    }

    /**
    *	Clear existing channels so the rebuilt index reflects the current deterministic entry array only.
    **/
    cache->by_overlay_id.clear();
    cache->by_connector_id.clear();
    cache->by_edge_id.clear();
    cache->by_leaf_id.clear();
    cache->by_cluster_id.clear();
    cache->by_area_id.clear();
    cache->by_mover_region_id.clear();
    cache->by_mover_anchor_id.clear();

    /**
    *	Re-index each entry and fan it out into all localized reverse-index channels.
    **/
    for ( int32_t i = 0; i < ( int32_t )cache->entries.size(); i++ ) {
        const nav2_dynamic_edge_overlay_entry_t &entry = cache->entries[ ( size_t )i ];
        cache->by_overlay_id[ entry.overlay_id ] = i;
        SVG_Nav2_DynamicOverlay_AppendChannelIndex( entry.connector_id, i, &cache->by_connector_id );
        SVG_Nav2_DynamicOverlay_AppendChannelIndex( entry.edge_id, i, &cache->by_edge_id );
        SVG_Nav2_DynamicOverlay_AppendChannelIndex( entry.topology.leaf_index, i, &cache->by_leaf_id );
        SVG_Nav2_DynamicOverlay_AppendChannelIndex( entry.topology.cluster_id, i, &cache->by_cluster_id );
        SVG_Nav2_DynamicOverlay_AppendChannelIndex( entry.topology.area_id, i, &cache->by_area_id );
        SVG_Nav2_DynamicOverlay_AppendChannelIndex( entry.mover_region_id, i, &cache->by_mover_region_id );
        SVG_Nav2_DynamicOverlay_AppendChannelIndex( entry.mover_anchor_id, i, &cache->by_mover_anchor_id );
    }
}

/**
*	@brief	Allocate the next stable overlay id inside one cache.
*	@param	cache	Overlay cache to inspect.
*	@return	Stable overlay id for a new entry.
**/
static int32_t SVG_Nav2_DynamicOverlay_AllocateOverlayId( const nav2_dynamic_edge_overlay_cache_t &cache ) {
    /**
    *	Start from id 1 so zero/negative values remain reserved for invalid references.
    **/
    int32_t nextId = 1;

    /**
    *	Scan current entries so ids remain monotonic and deterministic for diagnostics.
    **/
    for ( const nav2_dynamic_edge_overlay_entry_t &entry : cache.entries ) {
        nextId = std::max( nextId, entry.overlay_id + 1 );
    }
    return nextId;
}

/**
*	@brief	Accumulate one overlay entry into a modulation evaluation result.
*	@param	entry	Overlay entry to accumulate.
*	@param	out_eval	[in,out] Evaluation accumulator.
**/
static void SVG_Nav2_DynamicOverlay_AccumulateEval( const nav2_dynamic_edge_overlay_entry_t &entry, nav2_dynamic_overlay_eval_t *out_eval ) {
    /**
    *	Require output storage before mutating the accumulator.
    **/
    if ( !out_eval ) {
        return;
    }

    /**
    *	Count the contributing entry and accumulate additive/wait costs.
    **/
    out_eval->contributing_entries++;
    out_eval->additive_cost += std::max( 0.0, entry.additive_cost );
    out_eval->wait_cost_ms += std::max( 0.0, entry.wait_cost_ms );
    if ( ( entry.flags & NAV2_DYNAMIC_OVERLAY_FLAG_REQUIRES_REVALIDATION ) != 0 ) {
        out_eval->requires_revalidation = true;
    }

    /**
    *	Promote modulation severity monotonically so block dominates wait, wait dominates penalize, and penalize dominates pass.
    **/
    if ( entry.modulation == nav2_overlay_modulation_t::Block ) {
        out_eval->modulation = nav2_overlay_modulation_t::Block;
        return;
    }
    if ( out_eval->modulation == nav2_overlay_modulation_t::Block ) {
        return;
    }
    if ( entry.modulation == nav2_overlay_modulation_t::Wait ) {
        out_eval->modulation = nav2_overlay_modulation_t::Wait;
        return;
    }
    if ( out_eval->modulation == nav2_overlay_modulation_t::Wait ) {
        return;
    }
    if ( entry.modulation == nav2_overlay_modulation_t::Penalize ) {
        out_eval->modulation = nav2_overlay_modulation_t::Penalize;
        return;
    }
    if ( out_eval->modulation == nav2_overlay_modulation_t::Pass && entry.modulation == nav2_overlay_modulation_t::Pass ) {
        out_eval->modulation = nav2_overlay_modulation_t::Pass;
    }
}

/**
*	@brief	Return whether one overlay entry matches a mover reference filter.
*	@param	entry	Overlay entry being tested.
*	@param	mover_ref	Optional mover reference filter.
*	@return	True when mover filtering allows this entry.
**/
static const bool SVG_Nav2_DynamicOverlay_MatchesMover( const nav2_dynamic_edge_overlay_entry_t &entry, const nav2_corridor_mover_ref_t *mover_ref ) {
    /**
    *	When no mover filter is supplied, accept all entries.
    **/
    if ( !mover_ref || !mover_ref->IsValid() ) {
        return true;
    }

    /**
    *	Require a concrete entry mover identity when mover-local filtering is active.
    **/
    if ( !entry.mover_ref.IsValid() ) {
        return false;
    }

    /**
    *	Match both owner and model when provided so mover-local overlays stay precise.
    **/
    if ( mover_ref->owner_entnum >= 0 && entry.mover_ref.owner_entnum != mover_ref->owner_entnum ) {
        return false;
    }
    if ( mover_ref->model_index >= 0 && entry.mover_ref.model_index != mover_ref->model_index ) {
        return false;
    }
    return true;
}

/**
*	@brief	Append candidate entry indices from one reverse-index bucket.
*	@param	channel	Reverse-index channel to inspect.
*	@param	key	Stable channel key.
*	@param	out_indices	[in,out] Candidate entry indices.
**/
static void SVG_Nav2_DynamicOverlay_CollectChannelCandidates( const std::unordered_map<int32_t, std::vector<int32_t>> &channel,
    const int32_t key, std::vector<int32_t> *out_indices ) {
    /**
    *	Require output storage and a valid key before attempting channel lookup.
    **/
    if ( !out_indices || key < 0 ) {
        return;
    }

    /**
    *	Append all indexed entries for this key; deduplication happens in the caller.
    **/
    const auto it = channel.find( key );
    if ( it == channel.end() ) {
        return;
    }
    for ( const int32_t entryIndex : it->second ) {
        out_indices->push_back( entryIndex );
    }
}

/**
*	@brief	Build one modulation entry from connector and occupancy context.
*	@param	connector	Connector source record.
*	@param	occupancy	Sparse occupancy grid used for role and dynamic-state lookup.
*	@param	occupancy_overlay	Occupancy overlay used for topology attachment and localized behavior.
*	@param	overlay_version	Published overlay version.
*	@param	stamp_frame	Frame stamp for this rebuild.
*	@param	out_entry	[out] Resulting overlay entry.
**/
static void SVG_Nav2_DynamicOverlay_BuildEntryFromConnector( const nav2_connector_t &connector,
    const nav2_occupancy_grid_t &occupancy, const nav2_dynamic_overlay_t &occupancy_overlay,
    const uint32_t overlay_version, const int64_t stamp_frame, nav2_dynamic_edge_overlay_entry_t *out_entry ) {
    /**
    *	Require output storage before populating connector-derived modulation data.
    **/
    if ( !out_entry ) {
        return;
    }
    *out_entry = {};

    /**
    *	Seed stable connector/topology/mover identity fields used by evaluation and localized invalidation.
    **/
    out_entry->connector_id = connector.connector_id;
    out_entry->edge_id = connector.connector_id;
    out_entry->from_span_id = connector.start.span_ref.span_id;
    out_entry->to_span_id = connector.end.span_ref.span_id;
    out_entry->mover_ref.owner_entnum = connector.owner_entnum;
    out_entry->mover_ref.model_index = connector.inline_model_index;
    out_entry->mover_region_id = NAV_REGION_ID_NONE;
    out_entry->mover_anchor_id = connector.owner_entnum;
    out_entry->topology.leaf_index = connector.start.leaf_id >= 0 ? connector.start.leaf_id : connector.end.leaf_id;
    out_entry->topology.cluster_id = connector.start.cluster_id >= 0 ? connector.start.cluster_id : connector.end.cluster_id;
    out_entry->topology.area_id = connector.start.area_id >= 0 ? connector.start.area_id : connector.end.area_id;
    out_entry->topology.portal_id = connector.connector_id;
    out_entry->topology.region_id = NAV_REGION_ID_NONE;
    out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_HAS_CONNECTOR | NAV2_DYNAMIC_OVERLAY_FLAG_HAS_EDGE | NAV2_DYNAMIC_OVERLAY_FLAG_HAS_TOPOLOGY | NAV2_DYNAMIC_OVERLAY_FLAG_LOCALIZED;
    if ( out_entry->mover_ref.IsValid() ) {
        out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_HAS_MOVER_REF;
    }

    /**
    *	Default to pass-through and layer in penalties/blocks from connector and occupancy semantics.
    **/
    out_entry->modulation = nav2_overlay_modulation_t::Pass;
    out_entry->reason = nav2_overlay_reason_t::None;
    out_entry->additive_cost = 0.0;
    out_entry->wait_cost_ms = 0.0;

    /**
    *	Treat unavailable connectors as hard blocks with an explicit area-disconnected reason.
    **/
    if ( !connector.dynamically_available ) {
        out_entry->modulation = nav2_overlay_modulation_t::Block;
        out_entry->reason = nav2_overlay_reason_t::AreaDisconnected;
        out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_REQUIRES_REVALIDATION;
    }

    /**
    *	Translate door state into wait or block modulation depending on current dynamic availability.
    **/
    if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_DOOR_TRANSITION ) != 0 ) {
        out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_DOOR;
        if ( out_entry->modulation != nav2_overlay_modulation_t::Block ) {
            out_entry->modulation = nav2_overlay_modulation_t::Wait;
            out_entry->reason = nav2_overlay_reason_t::DoorClosed;
            out_entry->wait_cost_ms = std::max( out_entry->wait_cost_ms, 250.0 );
        }
    }

    /**
    *	Translate mover boarding/ride/exit semantics into mover availability modulation.
    **/
    if ( ( connector.connector_kind & ( NAV2_CONNECTOR_KIND_MOVER_BOARDING | NAV2_CONNECTOR_KIND_MOVER_RIDE | NAV2_CONNECTOR_KIND_MOVER_EXIT ) ) != 0 ) {
        out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_MOVER;
        if ( out_entry->modulation != nav2_overlay_modulation_t::Block ) {
            out_entry->modulation = nav2_overlay_modulation_t::Wait;
            out_entry->reason = nav2_overlay_reason_t::MoverUnavailable;
            out_entry->wait_cost_ms = std::max( out_entry->wait_cost_ms, 200.0 );
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_BOARDING ) != 0 ) {
            out_entry->reason = nav2_overlay_reason_t::MoverBoardingUnavailable;
        }
        if ( ( connector.connector_kind & NAV2_CONNECTOR_KIND_MOVER_EXIT ) != 0 ) {
            out_entry->reason = nav2_overlay_reason_t::MoverExitUnavailable;
        }
    }

    /**
    *	Use connector policy penalty and movement restrictions as soft modulation signals.
    **/
    if ( connector.policy_penalty > 0.0 || ( connector.movement_restrictions & ( NAV_EDGE_FEATURE_ENTERS_LAVA | NAV_EDGE_FEATURE_ENTERS_SLIME ) ) != 0 ) {
        if ( out_entry->modulation == nav2_overlay_modulation_t::Pass ) {
            out_entry->modulation = nav2_overlay_modulation_t::Penalize;
            out_entry->reason = nav2_overlay_reason_t::Hazard;
        }
        out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_HAZARD;
        out_entry->additive_cost += std::max( 0.0, connector.policy_penalty );
        if ( ( connector.movement_restrictions & NAV_EDGE_FEATURE_ENTERS_LAVA ) != 0 ) {
            out_entry->additive_cost += 3.0;
        }
        if ( ( connector.movement_restrictions & NAV_EDGE_FEATURE_ENTERS_SLIME ) != 0 ) {
            out_entry->additive_cost += 1.5;
        }
    }

    /**
    *	Fold localized occupancy semantics for the connector into final modulation and cost.
    **/
    nav2_occupancy_record_t connectorOccupancy = {};
    if ( SVG_Nav2_OccupancyGrid_TryResolveConnector( occupancy, connector.connector_id, &connectorOccupancy ) ) {
        if ( connectorOccupancy.mover_region_id != NAV_REGION_ID_NONE ) {
            out_entry->mover_region_id = connectorOccupancy.mover_region_id;
        }
        if ( connectorOccupancy.mover_anchor_id >= 0 ) {
            out_entry->mover_anchor_id = connectorOccupancy.mover_anchor_id;
        }
        if ( connectorOccupancy.leaf_id >= 0 ) {
            out_entry->topology.leaf_index = connectorOccupancy.leaf_id;
        }
        if ( connectorOccupancy.cluster_id >= 0 ) {
            out_entry->topology.cluster_id = connectorOccupancy.cluster_id;
        }
        if ( connectorOccupancy.area_id >= 0 ) {
            out_entry->topology.area_id = connectorOccupancy.area_id;
        }

        if ( connectorOccupancy.decision == nav2_occupancy_decision_t::HardBlock || connectorOccupancy.decision == nav2_occupancy_decision_t::Blocked ) {
            out_entry->modulation = nav2_overlay_modulation_t::Block;
            out_entry->reason = nav2_overlay_reason_t::OccupancyConflict;
            out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_REQUIRES_REVALIDATION;
        }
        if ( connectorOccupancy.decision == nav2_occupancy_decision_t::TemporarilyUnavailable && out_entry->modulation != nav2_overlay_modulation_t::Block ) {
            out_entry->modulation = nav2_overlay_modulation_t::Wait;
            out_entry->reason = nav2_overlay_reason_t::MoverUnavailable;
            out_entry->wait_cost_ms = std::max( out_entry->wait_cost_ms, 180.0 );
        }
        if ( connectorOccupancy.decision == nav2_occupancy_decision_t::SoftPenalty && out_entry->modulation == nav2_overlay_modulation_t::Pass ) {
            out_entry->modulation = nav2_overlay_modulation_t::Penalize;
            out_entry->reason = nav2_overlay_reason_t::NpcCongestion;
            out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_NPC_CONGESTION;
        }
        out_entry->additive_cost += std::max( 0.0, connectorOccupancy.soft_penalty );
    }

    /**
    *	Fold occupancy-overlay soft penalties and temporary unavailable decisions by topology portal id.
    **/
    for ( const nav2_occupancy_overlay_entry_t &occupancyEntry : occupancy_overlay.entries ) {
        if ( occupancyEntry.topology.portal_id != connector.connector_id ) {
            continue;
        }
        out_entry->additive_cost += std::max( 0.0, occupancyEntry.soft_penalty );
        if ( occupancyEntry.decision == nav2_occupancy_decision_t::TemporarilyUnavailable && out_entry->modulation != nav2_overlay_modulation_t::Block ) {
            out_entry->modulation = nav2_overlay_modulation_t::Wait;
            out_entry->reason = nav2_overlay_reason_t::OccupancyConflict;
            out_entry->wait_cost_ms = std::max( out_entry->wait_cost_ms, 100.0 );
        }
        if ( occupancyEntry.decision == nav2_occupancy_decision_t::HardBlock || occupancyEntry.decision == nav2_occupancy_decision_t::Blocked ) {
            out_entry->modulation = nav2_overlay_modulation_t::Block;
            out_entry->reason = nav2_overlay_reason_t::OccupancyConflict;
            out_entry->flags |= NAV2_DYNAMIC_OVERLAY_FLAG_REQUIRES_REVALIDATION;
        }
    }

    /**
    *	Stamp publication and frame metadata for worker-safe diagnostics and revalidation.
    **/
    out_entry->stamp_frame = stamp_frame;
    out_entry->overlay_version = overlay_version;
}

/**
*	@brief	Record one entry into a bounded overlay summary.
*	@param	entry	Entry being accumulated.
*	@param	out_summary	[in,out] Summary to mutate.
**/
static void SVG_Nav2_DynamicOverlay_AccumulateSummary( const nav2_dynamic_edge_overlay_entry_t &entry, nav2_dynamic_overlay_summary_t *out_summary ) {
    /**
    *	Require output storage before updating summary counters.
    **/
    if ( !out_summary ) {
        return;
    }

    /**
    *	Track total entry count and modulation buckets.
    **/
    out_summary->entry_count++;
    if ( entry.modulation == nav2_overlay_modulation_t::Pass ) {
        out_summary->pass_count++;
    } else if ( entry.modulation == nav2_overlay_modulation_t::Penalize ) {
        out_summary->penalize_count++;
    } else if ( entry.modulation == nav2_overlay_modulation_t::Wait ) {
        out_summary->wait_count++;
    } else if ( entry.modulation == nav2_overlay_modulation_t::Block ) {
        out_summary->block_count++;
    }

    /**
    *	Track source semantics for validation output.
    **/
    if ( ( entry.flags & NAV2_DYNAMIC_OVERLAY_FLAG_DOOR ) != 0 ) {
        out_summary->door_count++;
    }
    if ( ( entry.flags & NAV2_DYNAMIC_OVERLAY_FLAG_MOVER ) != 0 ) {
        out_summary->mover_count++;
    }
    if ( ( entry.flags & NAV2_DYNAMIC_OVERLAY_FLAG_NPC_CONGESTION ) != 0 ) {
        out_summary->congestion_count++;
    }
    if ( ( entry.flags & NAV2_DYNAMIC_OVERLAY_FLAG_HAZARD ) != 0 ) {
        out_summary->hazard_count++;
    }
}


/**
*
*
*	Nav2 Dynamic Overlay Public API:
*
*
**/
/**
*	@brief	Clear a dynamic edge overlay cache to an empty state.
*	@param	cache	Overlay cache to clear.
**/
void SVG_Nav2_DynamicOverlay_ClearCache( nav2_dynamic_edge_overlay_cache_t *cache ) {
    /**
    *	Guard against null output storage.
    **/
    if ( !cache ) {
        return;
    }

    /**
    *	Reset entries and all reverse-index channels.
    **/
    cache->entries.clear();
    cache->by_overlay_id.clear();
    cache->by_connector_id.clear();
    cache->by_edge_id.clear();
    cache->by_leaf_id.clear();
    cache->by_cluster_id.clear();
    cache->by_area_id.clear();
    cache->by_mover_region_id.clear();
    cache->by_mover_anchor_id.clear();
}

/**
*	@brief	Append or update one dynamic overlay entry while preserving stable overlay identity.
*	@param	cache	Overlay cache receiving the entry.
*	@param	entry	Overlay entry payload.
*	@return	True when the entry was stored.
**/
const bool SVG_Nav2_DynamicOverlay_UpsertEntry( nav2_dynamic_edge_overlay_cache_t *cache, const nav2_dynamic_edge_overlay_entry_t &entry ) {
    /**
    *	Require mutable cache storage and valid overlay id.
    **/
    if ( !cache || entry.overlay_id < 0 ) {
        return false;
    }

    /**
    *	Replace an existing entry in-place when overlay id already exists.
    **/
    const auto existingIt = cache->by_overlay_id.find( entry.overlay_id );
    if ( existingIt != cache->by_overlay_id.end() ) {
        const int32_t entryIndex = existingIt->second;
        if ( entryIndex < 0 || entryIndex >= ( int32_t )cache->entries.size() ) {
            return false;
        }
        cache->entries[ ( size_t )entryIndex ] = entry;
        SVG_Nav2_DynamicOverlay_RebuildReverseIndices( cache );
        return true;
    }

    /**
    *	Append new entry and rebuild reverse indices so localized invalidation channels remain coherent.
    **/
    cache->entries.push_back( entry );
    SVG_Nav2_DynamicOverlay_RebuildReverseIndices( cache );
    return true;
}

/**
*	@brief	Evaluate one span adjacency edge against the dynamic overlay cache.
*	@param	cache	Overlay cache to inspect.
*	@param	edge	Span adjacency edge to evaluate.
*	@param	mover_ref	Optional mover reference for mover-local filtering.
*	@param	out_eval	[out] Aggregated modulation result.
*	@return	True when evaluation succeeded.
**/
const bool SVG_Nav2_DynamicOverlay_EvaluateSpanEdge( const nav2_dynamic_edge_overlay_cache_t &cache, const nav2_span_edge_t &edge,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_dynamic_overlay_eval_t *out_eval ) {
    /**
    *	Require evaluation output storage.
    **/
    if ( !out_eval ) {
        return false;
    }
    *out_eval = {};
    out_eval->modulation = nav2_overlay_modulation_t::Pass;

    /**
    *	Collect localized candidate entries from edge id and span ids.
    **/
    std::vector<int32_t> candidateIndices = {};
    candidateIndices.reserve( 16 );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_edge_id, edge.edge_id, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_edge_id, edge.from_span_id, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_edge_id, edge.to_span_id, &candidateIndices );

    /**
    *	Fallback to full scan when no localized channel candidates were found.
    **/
    if ( candidateIndices.empty() ) {
        candidateIndices.resize( cache.entries.size() );
        for ( int32_t i = 0; i < ( int32_t )cache.entries.size(); i++ ) {
            candidateIndices[ ( size_t )i ] = i;
        }
    }

    /**
    *	Deduplicate candidate indices and accumulate matching entries.
    **/
    std::unordered_set<int32_t> uniqueIndices = {};
    for ( const int32_t candidateIndex : candidateIndices ) {
        if ( candidateIndex < 0 || candidateIndex >= ( int32_t )cache.entries.size() ) {
            continue;
        }
        if ( uniqueIndices.find( candidateIndex ) != uniqueIndices.end() ) {
            continue;
        }
        uniqueIndices.insert( candidateIndex );

        const nav2_dynamic_edge_overlay_entry_t &entry = cache.entries[ ( size_t )candidateIndex ];
        if ( entry.edge_id >= 0 && entry.edge_id != edge.edge_id && entry.edge_id != edge.from_span_id && entry.edge_id != edge.to_span_id ) {
            continue;
        }
        if ( entry.from_span_id >= 0 && entry.from_span_id != edge.from_span_id ) {
            continue;
        }
        if ( entry.to_span_id >= 0 && entry.to_span_id != edge.to_span_id ) {
            continue;
        }
        if ( !SVG_Nav2_DynamicOverlay_MatchesMover( entry, mover_ref ) ) {
            continue;
        }
        SVG_Nav2_DynamicOverlay_AccumulateEval( entry, out_eval );
    }

    return true;
}

/**
*	@brief	Evaluate one corridor segment against the dynamic overlay cache.
*	@param	cache	Overlay cache to inspect.
*	@param	segment	Corridor segment to evaluate.
*	@param	out_eval	[out] Aggregated modulation result.
*	@return	True when evaluation succeeded.
**/
const bool SVG_Nav2_DynamicOverlay_EvaluateCorridorSegment( const nav2_dynamic_edge_overlay_cache_t &cache, const nav2_corridor_segment_t &segment,
    nav2_dynamic_overlay_eval_t *out_eval ) {
    /**
    *	Require evaluation output storage.
    **/
    if ( !out_eval ) {
        return false;
    }
    *out_eval = {};
    out_eval->modulation = nav2_overlay_modulation_t::Pass;

    /**
    *	Collect localized candidates by connector and topology channels.
    **/
    std::vector<int32_t> candidateIndices = {};
    candidateIndices.reserve( 16 );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_connector_id, segment.topology.portal_id, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_leaf_id, segment.topology.leaf_index, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_cluster_id, segment.topology.cluster_id, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_area_id, segment.topology.area_id, &candidateIndices );

    /**
    *	Fallback to full scan when localized channels produced no candidates.
    **/
    if ( candidateIndices.empty() ) {
        candidateIndices.resize( cache.entries.size() );
        for ( int32_t i = 0; i < ( int32_t )cache.entries.size(); i++ ) {
            candidateIndices[ ( size_t )i ] = i;
        }
    }

    /**
    *	Deduplicate and accumulate entries that match the segment topology and mover identity.
    **/
    std::unordered_set<int32_t> uniqueIndices = {};
    for ( const int32_t candidateIndex : candidateIndices ) {
        if ( candidateIndex < 0 || candidateIndex >= ( int32_t )cache.entries.size() ) {
            continue;
        }
        if ( uniqueIndices.find( candidateIndex ) != uniqueIndices.end() ) {
            continue;
        }
        uniqueIndices.insert( candidateIndex );

        const nav2_dynamic_edge_overlay_entry_t &entry = cache.entries[ ( size_t )candidateIndex ];
        if ( entry.connector_id >= 0 && entry.connector_id != segment.topology.portal_id ) {
            continue;
        }
        if ( entry.topology.leaf_index >= 0 && segment.topology.leaf_index >= 0 && entry.topology.leaf_index != segment.topology.leaf_index ) {
            continue;
        }
        if ( entry.topology.cluster_id >= 0 && segment.topology.cluster_id >= 0 && entry.topology.cluster_id != segment.topology.cluster_id ) {
            continue;
        }
        if ( entry.topology.area_id >= 0 && segment.topology.area_id >= 0 && entry.topology.area_id != segment.topology.area_id ) {
            continue;
        }
        if ( !SVG_Nav2_DynamicOverlay_MatchesMover( entry, segment.mover_ref.IsValid() ? &segment.mover_ref : nullptr ) ) {
            continue;
        }
        SVG_Nav2_DynamicOverlay_AccumulateEval( entry, out_eval );
    }

    return true;
}

/**
*	@brief	Evaluate one fine-search edge against the dynamic overlay cache.
*	@param	cache	Overlay cache to inspect.
*	@param	edge	Fine-search edge to evaluate.
*	@param	mover_ref	Optional mover reference for mover-local filtering.
*	@param	out_eval	[out] Aggregated modulation result.
*	@return	True when evaluation succeeded.
**/
const bool SVG_Nav2_DynamicOverlay_EvaluateFineEdge( const nav2_dynamic_edge_overlay_cache_t &cache, const nav2_fine_astar_edge_t &edge,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_dynamic_overlay_eval_t *out_eval ) {
    /**
    *	Require evaluation output storage.
    **/
    if ( !out_eval ) {
        return false;
    }
    *out_eval = {};
    out_eval->modulation = nav2_overlay_modulation_t::Pass;

    /**
    *	Collect localized candidates by connector id and mover region channels.
    **/
    std::vector<int32_t> candidateIndices = {};
    candidateIndices.reserve( 16 );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_connector_id, edge.connector_id, &candidateIndices );
    if ( edge.mover_ref.IsValid() ) {
        SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache.by_mover_anchor_id, edge.mover_ref.owner_entnum, &candidateIndices );
    }

    /**
    *	Fallback to full scan when localized channels produced no candidates.
    **/
    if ( candidateIndices.empty() ) {
        candidateIndices.resize( cache.entries.size() );
        for ( int32_t i = 0; i < ( int32_t )cache.entries.size(); i++ ) {
            candidateIndices[ ( size_t )i ] = i;
        }
    }

    /**
    *	Select effective mover filter from explicit function argument or edge mover reference.
    **/
    const nav2_corridor_mover_ref_t *effectiveMoverRef = mover_ref;
    if ( !effectiveMoverRef && edge.mover_ref.IsValid() ) {
        effectiveMoverRef = &edge.mover_ref;
    }

    /**
    *	Deduplicate and accumulate matching entries.
    **/
    std::unordered_set<int32_t> uniqueIndices = {};
    for ( const int32_t candidateIndex : candidateIndices ) {
        if ( candidateIndex < 0 || candidateIndex >= ( int32_t )cache.entries.size() ) {
            continue;
        }
        if ( uniqueIndices.find( candidateIndex ) != uniqueIndices.end() ) {
            continue;
        }
        uniqueIndices.insert( candidateIndex );

        const nav2_dynamic_edge_overlay_entry_t &entry = cache.entries[ ( size_t )candidateIndex ];
        if ( entry.connector_id >= 0 && entry.connector_id != edge.connector_id ) {
            continue;
        }
        if ( !SVG_Nav2_DynamicOverlay_MatchesMover( entry, effectiveMoverRef ) ) {
            continue;
        }
        SVG_Nav2_DynamicOverlay_AccumulateEval( entry, out_eval );
    }

    return true;
}

/**
*	@brief	Rebuild dynamic edge overlay entries from occupancy and connector state.
*	@param	cache	Overlay cache to rebuild.
*	@param	connectors	Connector registry used for connector-local overlay identities.
*	@param	occupancy	Sparse occupancy grid used for localized dynamic state.
*	@param	occupancy_overlay	Occupancy overlay entries used for topology-local overlay projection.
*	@param	snapshot_runtime	Optional snapshot runtime used to publish connector overlay version.
*	@param	stamp_frame	Frame stamp recorded on rebuilt entries.
*	@param	out_summary	[out] Optional bounded rebuild summary.
*	@return	True when at least one overlay entry was published.
**/
const bool SVG_Nav2_DynamicOverlay_Rebuild( nav2_dynamic_edge_overlay_cache_t *cache, const nav2_connector_list_t &connectors,
    const nav2_occupancy_grid_t &occupancy, const nav2_dynamic_overlay_t &occupancy_overlay, nav2_snapshot_runtime_t *snapshot_runtime,
    const int64_t stamp_frame, nav2_dynamic_overlay_summary_t *out_summary ) {
    /**
    *	Require mutable cache storage before rebuilding overlay entries.
    **/
    if ( !cache ) {
        return false;
    }

    /**
    *	Reset cache and summary so rebuild output is deterministic for this publication tick.
    **/
    SVG_Nav2_DynamicOverlay_ClearCache( cache );
    if ( out_summary ) {
        *out_summary = {};
    }

    /**
    *	Resolve next connector overlay version using snapshot runtime when available.
    **/
    uint32_t overlayVersion = snapshot_runtime ? SVG_Nav2_Snapshot_BumpConnectorVersion( snapshot_runtime ) : 0;
    if ( out_summary ) {
        out_summary->published_connector_version = overlayVersion;
    }

    /**
    *	Create one overlay entry per connector while preserving stable connector identity.
    **/
    for ( const nav2_connector_t &connector : connectors.connectors ) {
        nav2_dynamic_edge_overlay_entry_t entry = {};
        entry.overlay_id = SVG_Nav2_DynamicOverlay_AllocateOverlayId( *cache );
        SVG_Nav2_DynamicOverlay_BuildEntryFromConnector( connector, occupancy, occupancy_overlay, overlayVersion, stamp_frame, &entry );
        if ( !SVG_Nav2_DynamicOverlay_UpsertEntry( cache, entry ) ) {
            continue;
        }
        SVG_Nav2_DynamicOverlay_AccumulateSummary( entry, out_summary );
    }

    /**
    *	Apply localized spatial query counters for diagnostics when imports are available.
    **/
    if ( out_summary ) {
        if ( gi.BoxEdicts && !occupancy.records.empty() ) {
            for ( const nav2_occupancy_record_t &record : occupancy.records ) {
                if ( record.leaf_id < 0 || record.area_id < 0 ) {
                    continue;
                }
                const Vector3 mins = { -16.0f, -16.0f, ( float )record.allowed_z_band.min_z };
                const Vector3 maxs = { 16.0f, 16.0f, ( float )record.allowed_z_band.max_z };
                svg_base_edict_t *hits[ 8 ] = { nullptr };
                const int32_t hitCount = gi.BoxEdicts( &mins, &maxs, hits, 8, AREA_SOLID );
                if ( hitCount > 0 ) {
                    out_summary->localized_entity_query_count++;
                }
            }
        }

        if ( gi.CM_BoxLeafs && !occupancy_overlay.entries.empty() ) {
            for ( const nav2_occupancy_overlay_entry_t &overlayEntry : occupancy_overlay.entries ) {
                const vec3_t mins = { -8.0f, -8.0f, -8.0f };
                const vec3_t maxs = { 8.0f, 8.0f, 8.0f };
                mleaf_t *leafList[ 8 ] = { nullptr };
                mnode_t *topNode = nullptr;
                const int32_t leafCount = gi.CM_BoxLeafs( mins, maxs, leafList, 8, &topNode );
                if ( leafCount > 0 ) {
                    out_summary->localized_leaf_query_count++;
                }
                if ( gi.CM_BoxContents ) {
                    cm_contents_t contents = {};
                    const int32_t contentsCount = gi.CM_BoxContents( mins, maxs, &contents, leafList, 8, &topNode );
                    if ( contentsCount > 0 ) {
                        out_summary->localized_contents_query_count++;
                    }
                }
            }
        }
    }

    return !cache->entries.empty();
}

/**
*	@brief	Invalidate localized overlay entries by topology scope and update their publication version.
*	@param	cache	Overlay cache to mutate.
*	@param	leaf_id	Optional leaf id scope.
*	@param	cluster_id	Optional cluster id scope.
*	@param	connector_id	Optional connector id scope.
*	@param	mover_region_id	Optional mover-local region id scope.
*	@param	overlay_version	Published overlay version to stamp onto invalidated entries.
*	@param	out_invalidated_count	[out] Optional count of entries touched by invalidation.
*	@return	True when one or more entries were invalidated.
**/
const bool SVG_Nav2_DynamicOverlay_InvalidateLocalized( nav2_dynamic_edge_overlay_cache_t *cache,
    const int32_t leaf_id, const int32_t cluster_id, const int32_t connector_id, const int32_t mover_region_id,
    const uint32_t overlay_version, int32_t *out_invalidated_count ) {
    /**
    *	Require mutable cache storage before running invalidation.
    **/
    if ( !cache ) {
        return false;
    }

    /**
    *	Initialize output invalidation count.
    **/
    if ( out_invalidated_count ) {
        *out_invalidated_count = 0;
    }

    /**
    *	Gather candidate indices from all localized channels requested by the caller.
    **/
    std::vector<int32_t> candidateIndices = {};
    candidateIndices.reserve( 32 );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache->by_leaf_id, leaf_id, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache->by_cluster_id, cluster_id, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache->by_connector_id, connector_id, &candidateIndices );
    SVG_Nav2_DynamicOverlay_CollectChannelCandidates( cache->by_mover_region_id, mover_region_id, &candidateIndices );

    /**
    *	When no explicit scope was provided, invalidate every entry conservatively.
    **/
    if ( candidateIndices.empty() && leaf_id < 0 && cluster_id < 0 && connector_id < 0 && mover_region_id == NAV_REGION_ID_NONE ) {
        candidateIndices.resize( cache->entries.size() );
        for ( int32_t i = 0; i < ( int32_t )cache->entries.size(); i++ ) {
            candidateIndices[ ( size_t )i ] = i;
        }
    }

    /**
    *	Deduplicate candidate indices and stamp invalidated entries with revalidation + version markers.
    **/
    std::unordered_set<int32_t> uniqueIndices = {};
    int32_t invalidatedCount = 0;
    for ( const int32_t candidateIndex : candidateIndices ) {
        if ( candidateIndex < 0 || candidateIndex >= ( int32_t )cache->entries.size() ) {
            continue;
        }
        if ( uniqueIndices.find( candidateIndex ) != uniqueIndices.end() ) {
            continue;
        }
        uniqueIndices.insert( candidateIndex );

        nav2_dynamic_edge_overlay_entry_t &entry = cache->entries[ ( size_t )candidateIndex ];
        entry.flags |= NAV2_DYNAMIC_OVERLAY_FLAG_REQUIRES_REVALIDATION;
        entry.overlay_version = overlay_version;
        invalidatedCount++;
    }

    if ( out_invalidated_count ) {
        *out_invalidated_count = invalidatedCount;
    }
    return invalidatedCount > 0;
}

/**
*	@brief	Emit bounded debug diagnostics for dynamic edge overlay state.
*	@param	cache	Overlay cache to report.
*	@param	limit	Maximum number of entries to print.
**/
void SVG_Nav2_DebugPrintDynamicOverlay( const nav2_dynamic_edge_overlay_cache_t &cache, const int32_t limit ) {
    /**
    *	Skip diagnostics when the report limit is non-positive.
    **/
    if ( limit <= 0 ) {
        return;
    }

    /**
    *	Print one compact overlay summary header.
    **/
    gi.dprintf( "[NAV2][DynamicOverlay] entries=%d limit=%d\n", ( int32_t )cache.entries.size(), limit );

    /**
    *	Emit bounded per-entry diagnostics.
    **/
    const int32_t reportCount = std::min( ( int32_t )cache.entries.size(), limit );
    for ( int32_t i = 0; i < reportCount; i++ ) {
        const nav2_dynamic_edge_overlay_entry_t &entry = cache.entries[ ( size_t )i ];
        gi.dprintf( "[NAV2][DynamicOverlay] id=%d conn=%d edge=%d spans=(%d->%d) topo=(leaf:%d cluster:%d area:%d region:%d portal:%d) mover=(%d,%d) mod=%d reason=%d add=%.2f wait=%.1f flags=0x%08x ver=%u stamp=%lld\n",
            entry.overlay_id,
            entry.connector_id,
            entry.edge_id,
            entry.from_span_id,
            entry.to_span_id,
            entry.topology.leaf_index,
            entry.topology.cluster_id,
            entry.topology.area_id,
            entry.topology.region_id,
            entry.topology.portal_id,
            entry.mover_ref.owner_entnum,
            entry.mover_ref.model_index,
            ( int32_t )entry.modulation,
            ( int32_t )entry.reason,
            entry.additive_cost,
            entry.wait_cost_ms,
            entry.flags,
            entry.overlay_version,
            ( long long )entry.stamp_frame );
    }
}

/**
*	@brief	Keep the nav2 dynamic-overlay module represented in the build.
**/
void SVG_Nav2_DynamicOverlay_ModuleAnchor( void ) {
}
