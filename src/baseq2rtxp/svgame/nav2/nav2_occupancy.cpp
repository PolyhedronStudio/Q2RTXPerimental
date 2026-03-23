/********************************************************************
*
*
*	ServerGame: Nav2 Sparse Occupancy and Dynamic Overlay Pruning - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_occupancy.h"

#include "svgame/nav2/nav2_entity_semantics.h"
#include "svgame/nav2/nav2_snapshot.h"

#include <algorithm>
#include <limits>

#include "shared/math/qm_math_cpp.h"


/**
*	@brief Allocate the next stable occupancy id in a grid.
*	@param grid Grid to inspect.
*	@return Stable id for the next appended record.
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
*	@brief Allocate the next stable overlay id in a collection.
*	@param overlay Overlay collection to inspect.
*	@return Stable id for the next appended overlay entry.
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
*	@brief Return whether the span should be treated as hard-blocked by contents or policy.
*	@param span Span to inspect.
*	@param out_soft_penalty [out] Soft penalty accumulated while classifying the span.
*	@return True when the span is considered hard blocked.
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
* @brief Resolve span occupancy decision from span metadata.
* @param span Span to classify.
* @param out_soft_penalty [out] Soft penalty accumulated while classifying the span.
* @return Decision representing span occupancy semantics.
**/
static nav2_occupancy_decision_t SVG_Nav2_DecideSpanOccupancy( const nav2_span_t &span, double *out_soft_penalty ) {
    /**
    *    Start with the base span hazard/soft-penalty classification.
    **/
    double softPenalty = 0.0;
    const bool hardBlocked = SVG_Nav2_ClassifySpanOccupancy( span, &softPenalty );
    if ( out_soft_penalty ) {
        *out_soft_penalty = softPenalty;
    }
    if ( hardBlocked ) {
        return nav2_occupancy_decision_t::Blocked;
    }

    /**
    *    Flag ladder/connector-heavy spans as interaction-dependent so callers can distinguish them from free traversal.
    **/
    if ( ( span.connector_hint_mask != 0 ) || ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 ) {
        return nav2_occupancy_decision_t::RequiresInteraction;
    }

    if ( softPenalty > 0.0 ) {
        return nav2_occupancy_decision_t::SoftPenalty;
    }
    return nav2_occupancy_decision_t::Free;
}

/**
*	@brief Write an occupancy record into the grid and update reverse indices.
*	@param grid Occupancy grid to mutate.
*	@param record Record to append.
*	@return True when the record was stored.
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
    if ( record.mover_anchor_id >= 0 ) {
        grid->by_mover_anchor_id[ record.mover_anchor_id ] = index;
    }
    if ( record.leaf_id >= 0 ) {
        grid->by_leaf_id[ record.leaf_id ].push_back( index );
    }
    if ( record.cluster_id >= 0 ) {
        grid->by_cluster_id[ record.cluster_id ].push_back( index );
    }
    if ( record.area_id >= 0 ) {
        grid->by_area_id[ record.area_id ].push_back( index );
    }
    return true;
}

/**
*	@brief Write an overlay entry into the collection and update its reverse index.
*	@param overlay Overlay collection to mutate.
*	@param entry Entry to append.
*	@return True when the entry was stored.
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
*	@brief Evaluate a topology reference against the overlay collection.
*	@param overlay Overlay collection to inspect.
*	@param topology	Topology reference to match.
*	@param mover_ref	Optional mover reference for mover-local pruning.
*	@param out_soft_penalty	[out] Soft penalty accumulated while classifying the overlay.
*	@return True when a matching overlay record should hard-block traversal.
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
* @brief Evaluate one sparse occupancy decision against summary counters.
* @param decision Decision to evaluate.
* @param out_summary [out] Summary counters to update.
* @return True when the decision remains traversable.
**/
static const bool SVG_Nav2_Occupancy_AccumulateDecision( const nav2_occupancy_decision_t decision, nav2_occupancy_summary_t *out_summary );

/**
*	@brief Classify one fine-search node against occupancy and overlay data.
*	@param grid Sparse occupancy grid.
*	@param overlay Dynamic overlay collection.
*	@param node Fine-search node to inspect.
*	@param mover_ref	Optional mover reference for mover-local pruning.
*	@param out_summary	[out] Optional summary for diagnostics.
*	@return True when the node remains traversable.
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
            if ( record.decision == nav2_occupancy_decision_t::HardBlock || record.decision == nav2_occupancy_decision_t::Blocked ) {
                decision = nav2_occupancy_decision_t::HardBlock;
            }
            else if ( record.decision == nav2_occupancy_decision_t::SoftPenalty ) {
                decision = nav2_occupancy_decision_t::SoftPenalty;
                softPenalty += record.soft_penalty;
            }
            else if ( record.decision == nav2_occupancy_decision_t::RequiresInteraction ) {
                decision = nav2_occupancy_decision_t::RequiresInteraction;
            }
            else if ( record.decision == nav2_occupancy_decision_t::TemporarilyUnavailable ) {
                decision = nav2_occupancy_decision_t::TemporarilyUnavailable;
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
    return SVG_Nav2_Occupancy_AccumulateDecision( decision, out_summary );
}

/**
* @brief Evaluate one sparse occupancy decision against summary counters.
* @param decision Decision to evaluate.
* @param out_summary [out] Summary counters to update.
* @return True when the decision remains traversable.
**/
static const bool SVG_Nav2_Occupancy_AccumulateDecision( const nav2_occupancy_decision_t decision, nav2_occupancy_summary_t *out_summary ) {
    /**
    *    Require output storage before mutating summary counters.
    **/
    if ( !out_summary ) {
        return decision != nav2_occupancy_decision_t::HardBlock && decision != nav2_occupancy_decision_t::Blocked;
    }

    /**
    *    Track each decision bucket explicitly so runtime validation can distinguish interaction and temporary blockers.
    **/
    if ( decision == nav2_occupancy_decision_t::HardBlock || decision == nav2_occupancy_decision_t::Blocked ) {
        out_summary->hard_block_count++;
        return false;
    }
    if ( decision == nav2_occupancy_decision_t::SoftPenalty ) {
        out_summary->soft_penalty_count++;
        return true;
    }
    if ( decision == nav2_occupancy_decision_t::RequiresInteraction ) {
        out_summary->requires_interaction_count++;
        return true;
    }
    if ( decision == nav2_occupancy_decision_t::TemporarilyUnavailable ) {
        out_summary->temporarily_unavailable_count++;
        return false;
    }
    if ( decision == nav2_occupancy_decision_t::Revalidate ) {
        out_summary->revalidate_count++;
        return true;
    }

    out_summary->free_count++;
    return true;
}

/**
* @brief Convert inline entity role and class metadata into an occupancy decision.
* @param info Inline BSP entity semantics payload.
* @param out_record [out] Record receiving decision flags.
**/
static void SVG_Nav2_Occupancy_ApplyEntitySemantics( const nav2_inline_bsp_entity_info_t &info, nav2_occupancy_record_t *out_record ) {
    /**
    *    Require output record storage before applying semantics.
    **/
    if ( !out_record ) {
        return;
    }

    /**
    *    Mirror stable entity metadata so localized occupancy remains pointer-free and persistence-friendly.
    **/
    out_record->entity_type = info.entity_type;
    out_record->solid_type = info.solid;
    out_record->move_type = ( int32_t )info.movetype;
    out_record->role_flags = info.role_flags;

    /**
    *    Mark semantic feature flags for diagnostics and per-kind lookup.
    **/
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_TIMED_CONNECTOR ) != 0 || ( info.role_flags & NAV2_INLINE_BSP_ROLE_OPENABLE_PASSAGE ) != 0 ) {
        out_record->flags |= NAV2_OCCUPANCY_FLAG_DOOR | NAV2_OCCUPANCY_FLAG_REQUIRES_INTERACTION;
    }
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_SURFACE ) != 0 || ( info.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_TRAVERSABLE ) != 0 ) {
        out_record->flags |= NAV2_OCCUPANCY_FLAG_PLAT;
    }
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_ROTATING_HAZARD ) != 0 ) {
        out_record->flags |= NAV2_OCCUPANCY_FLAG_HAZARD;
    }
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_MOVING_TRAVERSAL_SUBSPACE ) != 0 ) {
        out_record->flags |= NAV2_OCCUPANCY_FLAG_MOVER_ANCHOR;
    }

    /**
    *    Select occupancy decision from role semantics while preferring conservative hard blocks for explicit hazard/blocker roles.
    **/
    out_record->decision = nav2_occupancy_decision_t::Free;
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_BLOCKER_ONLY ) != 0 ) {
        out_record->decision = nav2_occupancy_decision_t::Blocked;
        out_record->flags |= NAV2_OCCUPANCY_FLAG_HARD_BLOCKED;
        return;
    }
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_ROTATING_HAZARD ) != 0 ) {
        out_record->decision = nav2_occupancy_decision_t::TemporarilyUnavailable;
        out_record->flags |= NAV2_OCCUPANCY_FLAG_TEMPORARILY_UNAVAILABLE;
        return;
    }
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_TIMED_CONNECTOR ) != 0 || ( info.role_flags & NAV2_INLINE_BSP_ROLE_OPENABLE_PASSAGE ) != 0 ) {
        out_record->decision = nav2_occupancy_decision_t::RequiresInteraction;
        out_record->flags |= NAV2_OCCUPANCY_FLAG_REQUIRES_INTERACTION;
        return;
    }
    if ( ( info.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_SURFACE ) != 0 || ( info.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_TRAVERSABLE ) != 0 ) {
        out_record->decision = nav2_occupancy_decision_t::SoftPenalty;
        out_record->soft_penalty += 1.0;
        out_record->flags |= NAV2_OCCUPANCY_FLAG_SOFT_PENALIZED;
    }
}

/**
* @brief Derive bounded topology membership from one localized occupancy sample point.
* @param point World-space point to classify.
* @param out_record [out] Occupancy record receiving leaf/cluster/area metadata.
**/
static void SVG_Nav2_Occupancy_ApplyPointTopology( const Vector3 &point, nav2_occupancy_record_t *out_record ) {
    /**
    *    Require output storage and BSP imports before attempting topology classification.
    **/
    if ( !out_record || !gi.GetCollisionModel || !gi.BSP_PointLeaf ) {
        return;
    }

    const cm_t *collisionModel = gi.GetCollisionModel();
    if ( !collisionModel || !collisionModel->cache || !collisionModel->cache->nodes ) {
        return;
    }

    const vec3_t pointVec = { point.x, point.y, point.z };
    mleaf_t *leaf = gi.BSP_PointLeaf( collisionModel->cache->nodes, pointVec );
    if ( !leaf ) {
        return;
    }

    out_record->cluster_id = leaf->cluster;
    out_record->area_id = leaf->area;
    if ( collisionModel->cache->leafs && leaf >= collisionModel->cache->leafs && leaf < ( collisionModel->cache->leafs + collisionModel->cache->numleafs ) ) {
        out_record->leaf_id = ( int32_t )( leaf - collisionModel->cache->leafs );
    }
    if ( out_record->leaf_id >= 0 ) {
        out_record->flags |= NAV2_OCCUPANCY_FLAG_HAS_LEAF;
    }
    if ( out_record->cluster_id >= 0 ) {
        out_record->flags |= NAV2_OCCUPANCY_FLAG_HAS_CLUSTER;
    }
    if ( out_record->area_id >= 0 ) {
        out_record->flags |= NAV2_OCCUPANCY_FLAG_HAS_AREA;
    }
}

/**
* @brief Gather localized entity occupancy records around sparse span columns using BoxEdicts.
* @param grid Occupancy grid receiving entity records.
* @param overlay Overlay receiving portal/mover-local entries.
* @param span_grid Span-grid source used for localized bounds.
* @param entities Optional inline BSP semantics list used to map role flags.
* @param stamp_frame Frame stamp assigned to generated records.
* @param out_summary [out] Optional summary counters.
* @return True when at least one localized entity record was imported.
**/
static const bool SVG_Nav2_GatherLocalizedEntityOccupancy( nav2_occupancy_grid_t *grid, nav2_dynamic_overlay_t *overlay, const nav2_span_grid_t &span_grid,
    const nav2_inline_bsp_entity_list_t *entities, const int64_t stamp_frame, nav2_occupancy_summary_t *out_summary ) {
    /**
    *    Require output storage and BoxEdicts import before gathering localized dynamic occupancy.
    **/
    if ( !grid || !overlay || !gi.BoxEdicts ) {
        return false;
    }

    /**
    *    Build a fast semantics lookup by entity number so localized BoxEdicts hits can inherit stable nav roles.
    **/
    std::unordered_map<int32_t, const nav2_inline_bsp_entity_info_t *> byEntnum = {};
    if ( entities ) {
        for ( const nav2_inline_bsp_entity_info_t &info : entities->entities ) {
            byEntnum[ info.owner_entnum ] = &info;
        }
    }

    /**
    *    Scan sparse columns only, avoiding global entity scans when local topology narrowing exists.
    **/
    bool imported = false;
    for ( const nav2_span_column_t &column : span_grid.columns ) {
        if ( column.spans.empty() ) {
            continue;
        }

        // Derive conservative localized bounds from the first span in the sparse column.
        const nav2_span_t &anchorSpan = column.spans.front();
        const float halfExtent = ( float )std::max( 8.0, span_grid.cell_size_xy * 0.5 );
        const float heightPad = 64.0f;
        const Vector3 sampleCenter = {
            ( float )( ( ( double )column.tile_x + 0.5 ) * span_grid.cell_size_xy ),
            ( float )( ( ( double )column.tile_y + 0.5 ) * span_grid.cell_size_xy ),
            ( float )anchorSpan.preferred_z
        };
        const Vector3 queryMins = { sampleCenter.x - halfExtent, sampleCenter.y - halfExtent, sampleCenter.z - heightPad };
        const Vector3 queryMaxs = { sampleCenter.x + halfExtent, sampleCenter.y + halfExtent, sampleCenter.z + heightPad };

        // Gather nearby dynamic solid entities through localized broadphase query.
        svg_base_edict_t *hits[ 64 ] = { nullptr };
        const int32_t hitCount = gi.BoxEdicts( &queryMins, &queryMaxs, hits, 64, AREA_SOLID );
        if ( out_summary ) {
            out_summary->localized_entity_samples += std::max( 0, hitCount );
        }

        for ( int32_t i = 0; i < hitCount; i++ ) {
            svg_base_edict_t *ent = hits[ i ];
            if ( !ent || !ent->inUse ) {
                continue;
            }

            nav2_occupancy_record_t record = {};
            record.occupancy_id = SVG_Nav2_AllocateOccupancyId( *grid );
            record.kind = nav2_occupancy_kind_t::Entity;
            record.span_id = anchorSpan.span_id;
            record.mover_entnum = ent->s.number;
            record.inline_model_index = ent->s.modelindex;
            record.mover_anchor_id = ent->s.number;
            record.mover_region_id = anchorSpan.region_layer_id;
            record.allowed_z_band.min_z = anchorSpan.floor_z;
            record.allowed_z_band.max_z = anchorSpan.ceiling_z;
            record.allowed_z_band.preferred_z = anchorSpan.preferred_z;
            record.stamp_frame = stamp_frame;
            record.flags |= NAV2_OCCUPANCY_FLAG_DYNAMIC | NAV2_OCCUPANCY_FLAG_NPC_BLOCKER;

            // Populate conservative topology identity for localized routing constraints.
            SVG_Nav2_Occupancy_ApplyPointTopology( sampleCenter, &record );

            // Apply inline-model role semantics when available; otherwise classify by runtime solidity/movetype.
            const auto semanticIt = byEntnum.find( ent->s.number );
            if ( semanticIt != byEntnum.end() && semanticIt->second ) {
                SVG_Nav2_Occupancy_ApplyEntitySemantics( *semanticIt->second, &record );
            } else {
                record.entity_type = ent->s.entityType;
                record.solid_type = ent->solid;
                record.move_type = ent->movetype;
                record.decision = nav2_occupancy_decision_t::SoftPenalty;
                record.soft_penalty = 0.75;
                if ( ent->solid == SOLID_BSP && ( ent->movetype == MOVETYPE_PUSH || ent->movetype == MOVETYPE_STOP ) ) {
                    record.decision = nav2_occupancy_decision_t::RequiresInteraction;
                    record.flags |= NAV2_OCCUPANCY_FLAG_REQUIRES_INTERACTION | NAV2_OCCUPANCY_FLAG_DOOR;
                }
            }

            // Promote hazardous entity records into explicit hazard occupancy kind.
            if ( ( record.flags & NAV2_OCCUPANCY_FLAG_HAZARD ) != 0 ) {
                record.kind = nav2_occupancy_kind_t::Hazard;
            }

            // Import record into sparse occupancy and mirror a dynamic overlay entry for local portal/mover checks.
            if ( SVG_Nav2_StoreOccupancyRecord( grid, record ) ) {
                imported = true;
                if ( out_summary ) {
                    if ( record.kind == nav2_occupancy_kind_t::Entity ) {
                        out_summary->entity_count++;
                    } else if ( record.kind == nav2_occupancy_kind_t::Hazard ) {
                        out_summary->hazard_count++;
                    }
                    SVG_Nav2_Occupancy_AccumulateDecision( record.decision, out_summary );
                }

                nav2_occupancy_overlay_entry_t overlayEntry = {};
                overlayEntry.overlay_id = SVG_Nav2_AllocateOverlayId( *overlay );
                overlayEntry.topology.leaf_index = record.leaf_id;
                overlayEntry.topology.cluster_id = record.cluster_id;
                overlayEntry.topology.area_id = record.area_id;
                overlayEntry.topology.region_id = record.mover_region_id;
                overlayEntry.topology.portal_id = record.connector_id;
                overlayEntry.topology.cell_index = record.span_id;
                overlayEntry.mover_ref.owner_entnum = record.mover_entnum;
                overlayEntry.mover_ref.model_index = record.inline_model_index;
                overlayEntry.decision = record.decision;
                overlayEntry.soft_penalty = record.soft_penalty;
                overlayEntry.flags = record.flags | NAV2_OCCUPANCY_FLAG_PORTAL_OVERLAY;
                overlayEntry.stamp_frame = record.stamp_frame;
                SVG_Nav2_StoreOverlayEntry( overlay, overlayEntry );
                if ( out_summary ) {
                    out_summary->overlay_count++;
                }
            }
        }
    }

    return imported;
}


/**
*
*
*	Nav2 Occupancy Public API:
*
*
**/
/**
* @brief Clear a sparse occupancy grid to an empty state.
* @param grid Occupancy grid to reset.
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
    grid->by_mover_anchor_id.clear();
    grid->by_leaf_id.clear();
    grid->by_cluster_id.clear();
    grid->by_area_id.clear();
}

/**
* @brief Clear a dynamic overlay collection to an empty state.
* @param overlay Overlay collection to reset.
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
* @brief Append or update a sparse occupancy record.
* @param grid Occupancy grid to mutate.
* @param record Record payload to append.
* @return True when the record was stored.
**/
const bool SVG_Nav2_OccupancyGrid_Upsert( nav2_occupancy_grid_t *grid, const nav2_occupancy_record_t &record ) {
    // Reject null grids and incomplete records.
    if ( !grid || record.occupancy_id < 0 ) {
        return false;
    }

    // Prefer replacing an existing slot so updates remain stable.
    const auto it = std::find_if( grid->records.begin(), grid->records.end(), [ & ]( const nav2_occupancy_record_t &candidate ) { return candidate.occupancy_id == record.occupancy_id; } );
    if ( it != grid->records.end() ) {
        /**
        *    Replace in-place and then rebuild reverse indices so all lookup channels remain coherent.
        **/
        *it = record;
        grid->by_span_id.clear();
        grid->by_edge_id.clear();
        grid->by_connector_id.clear();
        grid->by_mover_entnum.clear();
        grid->by_mover_anchor_id.clear();
        grid->by_leaf_id.clear();
        grid->by_cluster_id.clear();
        grid->by_area_id.clear();
        for ( int32_t i = 0; i < ( int32_t )grid->records.size(); ++i ) {
            const nav2_occupancy_record_t &existing = grid->records[ ( size_t )i ];
            if ( existing.span_id >= 0 ) {
                grid->by_span_id[ existing.span_id ] = i;
            }
            if ( existing.edge_id >= 0 ) {
                grid->by_edge_id[ existing.edge_id ] = i;
            }
            if ( existing.connector_id >= 0 ) {
                grid->by_connector_id[ existing.connector_id ] = i;
            }
            if ( existing.mover_entnum >= 0 ) {
                grid->by_mover_entnum[ existing.mover_entnum ] = i;
            }
            if ( existing.mover_anchor_id >= 0 ) {
                grid->by_mover_anchor_id[ existing.mover_anchor_id ] = i;
            }
            if ( existing.leaf_id >= 0 ) {
                grid->by_leaf_id[ existing.leaf_id ].push_back( i );
            }
            if ( existing.cluster_id >= 0 ) {
                grid->by_cluster_id[ existing.cluster_id ].push_back( i );
            }
            if ( existing.area_id >= 0 ) {
                grid->by_area_id[ existing.area_id ].push_back( i );
            }
        }
        return true;
    }
    return SVG_Nav2_StoreOccupancyRecord( grid, record );
}

/**
* @brief Append or update a dynamic overlay entry.
* @param overlay Overlay collection to mutate.
* @param entry Overlay payload to append.
* @return True when the entry was stored.
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
* @brief Resolve a sparse occupancy record by span id.
* @param grid Occupancy grid to inspect.
* @param span_id Stable span id to resolve.
* @param out_record [out] Resolved occupancy record.
* @return True when a matching record exists.
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
 * @brief Resolve a sparse occupancy record by connector id.
 * @param grid Occupancy grid to inspect.
 * @param connector_id Stable connector id to resolve.
 * @param out_record [out] Resolved occupancy record.
 * @return True when a matching record exists.
 **/
const bool SVG_Nav2_OccupancyGrid_TryResolveConnector( const nav2_occupancy_grid_t &grid, const int32_t connector_id, nav2_occupancy_record_t *out_record ) {
    // Require an output record so callers receive a stable copy.
    if ( !out_record ) {
        return false;
    }
    *out_record = {};

    const auto it = grid.by_connector_id.find( connector_id );
    if ( it == grid.by_connector_id.end() ) {
        return false;
    }
    *out_record = grid.records[ ( size_t )it->second ];
    return true;
}

/**
 * @brief Resolve a sparse occupancy record by mover anchor id.
 * @param grid Occupancy grid to inspect.
 * @param mover_anchor_id Stable mover anchor id to resolve.
 * @param out_record [out] Resolved occupancy record.
 * @return True when a matching record exists.
 **/
const bool SVG_Nav2_OccupancyGrid_TryResolveMoverAnchor( const nav2_occupancy_grid_t &grid, const int32_t mover_anchor_id, nav2_occupancy_record_t *out_record ) {
    // Require an output record so callers receive a stable copy.
    if ( !out_record ) {
        return false;
    }
    *out_record = {};

    const auto it = grid.by_mover_anchor_id.find( mover_anchor_id );
    if ( it == grid.by_mover_anchor_id.end() ) {
        return false;
    }
    *out_record = grid.records[ ( size_t )it->second ];
    return true;
}

/**
* @brief Resolve a sparse occupancy record by edge id.
* @param grid Occupancy grid to inspect.
* @param edge_id Stable edge id to resolve.
* @param out_record [out] Resolved occupancy record.
* @return True when a matching record exists.
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
* @brief Evaluate a span against the sparse occupancy grid and dynamic overlay.
* @param grid Occupancy grid to inspect.
* @param overlay Overlay collection to inspect.
* @param span Span being considered for pruning.
* @param mover_ref Optional mover reference for mover-local pruning.
* @param out_summary [out] Optional summary receiving query results.
* @return True when the span remains traversable after local pruning.
**/
const bool SVG_Nav2_EvaluateSpanOccupancy( const nav2_occupancy_grid_t &grid, const nav2_dynamic_overlay_t &overlay, const nav2_span_t &span,
    const nav2_corridor_mover_ref_t *mover_ref, nav2_occupancy_summary_t *out_summary ) {
    // Initialize a local summary if the caller asked for one.
    nav2_occupancy_summary_t localSummary = {};
    nav2_occupancy_summary_t *summaryPtr = out_summary ? out_summary : &localSummary;
    summaryPtr->span_count++;

    // Classify the span itself before applying overlay pruning.
    double softPenalty = 0.0;
    nav2_occupancy_decision_t decision = SVG_Nav2_DecideSpanOccupancy( span, &softPenalty );
    if ( !SVG_Nav2_Occupancy_AccumulateDecision( decision, summaryPtr ) ) {
        return false;
    }

    // Convert the span into a transient occupancy record so the sparse grid lookup can be reused.
    nav2_occupancy_record_t record = {};
    record.occupancy_id = 1;
    record.kind = nav2_occupancy_kind_t::Span;
    record.span_id = span.span_id;
    record.leaf_id = span.leaf_id;
    record.cluster_id = span.cluster_id;
    record.area_id = span.area_id;
    record.decision = decision;
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
    if ( decision == nav2_occupancy_decision_t::Blocked || decision == nav2_occupancy_decision_t::HardBlock ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_HARD_BLOCKED;
    }
    if ( decision == nav2_occupancy_decision_t::SoftPenalty ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_SOFT_PENALIZED;
    }
    if ( decision == nav2_occupancy_decision_t::RequiresInteraction ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_REQUIRES_INTERACTION;
    }
    if ( decision == nav2_occupancy_decision_t::TemporarilyUnavailable ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_TEMPORARILY_UNAVAILABLE;
    }
    if ( decision == nav2_occupancy_decision_t::Revalidate ) {
        record.flags |= NAV2_OCCUPANCY_FLAG_REQUIRES_REVALIDATION;
    }

    // Apply the sparse occupancy grid lookup and keep the caller visible summary current.
    const auto gridIt = grid.by_span_id.find( span.span_id );
    if ( gridIt != grid.by_span_id.end() ) {
        const nav2_occupancy_record_t &existing = grid.records[ ( size_t )gridIt->second ];
        if ( existing.decision == nav2_occupancy_decision_t::HardBlock || existing.decision == nav2_occupancy_decision_t::Blocked ) {
            SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
            return false;
        }
        if ( existing.decision == nav2_occupancy_decision_t::SoftPenalty ) {
            SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
            softPenalty += existing.soft_penalty;
        }
        if ( existing.decision == nav2_occupancy_decision_t::RequiresInteraction ) {
            decision = nav2_occupancy_decision_t::RequiresInteraction;
            SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
        }
        if ( existing.decision == nav2_occupancy_decision_t::TemporarilyUnavailable ) {
            decision = nav2_occupancy_decision_t::TemporarilyUnavailable;
            SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
            return false;
        }
    }

    // Apply dynamic overlay pruning to catch localized invalidation without rebuilding the whole hierarchy.
    if ( SVG_Nav2_EvaluateOverlayTopology( overlay, record.allowed_z_band.min_z >= 0.0 ? nav2_corridor_topology_ref_t{} : nav2_corridor_topology_ref_t{}, mover_ref, &softPenalty ) ) {
        SVG_Nav2_Occupancy_AccumulateDecision( nav2_occupancy_decision_t::HardBlock, summaryPtr );
        return false;
    }

    // Apply mover-local pruning if the candidate is attached to a specific mover and the caller supplied a current mover reference.
    if ( mover_ref && mover_ref->IsValid() && ( record.flags & NAV2_OCCUPANCY_FLAG_REQUIRES_REVALIDATION ) != 0 ) {
        if ( record.decision == nav2_occupancy_decision_t::Revalidate ) {
            SVG_Nav2_Occupancy_AccumulateDecision( nav2_occupancy_decision_t::Revalidate, summaryPtr );
        }
    }

    return decision != nav2_occupancy_decision_t::TemporarilyUnavailable;
}

/**
* @brief Evaluate a corridor segment against the sparse occupancy grid and dynamic overlay.
* @param grid Occupancy grid to inspect.
* @param overlay Overlay collection to inspect.
* @param segment Corridor segment being considered.
* @param mover_ref Optional mover reference for mover-local pruning.
* @param out_summary [out] Optional summary receiving query results.
* @return True when the segment remains compatible after local pruning.
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
    record.decision = ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ? nav2_occupancy_decision_t::Blocked
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
        SVG_Nav2_Occupancy_AccumulateDecision( nav2_occupancy_decision_t::HardBlock, summaryPtr );
        return false;
    }

    // Apply sparse-grid occupancy if the segment can be resolved to a known edge identifier.
    if ( record.edge_id >= 0 ) {
        const auto it = grid.by_edge_id.find( record.edge_id );
        if ( it != grid.by_edge_id.end() ) {
            const nav2_occupancy_record_t &existing = grid.records[ ( size_t )it->second ];
            if ( existing.decision == nav2_occupancy_decision_t::HardBlock || existing.decision == nav2_occupancy_decision_t::Blocked ) {
                SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
                return false;
            }
            if ( existing.decision == nav2_occupancy_decision_t::SoftPenalty ) {
                SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
                record.soft_penalty += existing.soft_penalty;
            }
            if ( existing.decision == nav2_occupancy_decision_t::RequiresInteraction ) {
                SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
                record.decision = nav2_occupancy_decision_t::RequiresInteraction;
            }
            if ( existing.decision == nav2_occupancy_decision_t::TemporarilyUnavailable ) {
                SVG_Nav2_Occupancy_AccumulateDecision( existing.decision, summaryPtr );
                return false;
            }
        }
    }

    // Apply mover-local pruning when the segment is attached to a mover-specific route.
    if ( mover_ref && mover_ref->IsValid() && segment.mover_ref.IsValid() ) {
        if ( mover_ref->owner_entnum != segment.mover_ref.owner_entnum || mover_ref->model_index != segment.mover_ref.model_index ) {
            SVG_Nav2_Occupancy_AccumulateDecision( nav2_occupancy_decision_t::Revalidate, summaryPtr );
        }
    }

    if ( record.decision == nav2_occupancy_decision_t::HardBlock || record.decision == nav2_occupancy_decision_t::Blocked ) {
        SVG_Nav2_Occupancy_AccumulateDecision( record.decision, summaryPtr );
        return false;
    }
    if ( record.decision == nav2_occupancy_decision_t::SoftPenalty ) {
        SVG_Nav2_Occupancy_AccumulateDecision( record.decision, summaryPtr );
    }
    if ( record.decision == nav2_occupancy_decision_t::RequiresInteraction ) {
        SVG_Nav2_Occupancy_AccumulateDecision( record.decision, summaryPtr );
    }
    if ( record.decision == nav2_occupancy_decision_t::TemporarilyUnavailable ) {
        SVG_Nav2_Occupancy_AccumulateDecision( record.decision, summaryPtr );
        return false;
    }
    return true;
}

/**
* @brief Evaluate a fine-search node against occupancy and dynamic overlay state.
* @param grid Occupancy grid to inspect.
* @param overlay Overlay collection to inspect.
* @param node Fine-search node being considered.
* @param mover_ref Optional mover reference for mover-local pruning.
* @param out_summary [out] Optional summary receiving query results.
* @return True when the node remains compatible after local pruning.
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
* @brief Merge a span-grid occupancy source into the sparse occupancy grid.
* @param grid Occupancy grid to mutate.
* @param span_grid	Span grid source to translate.
* @param stamp_frame	Frame stamp to record on imported records.
* @return True when at least one record was imported.
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
            record.decision = SVG_Nav2_DecideSpanOccupancy( span, &softPenalty );
            record.soft_penalty = softPenalty;
            if ( record.decision == nav2_occupancy_decision_t::HardBlock || record.decision == nav2_occupancy_decision_t::Blocked ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_HARD_BLOCKED;
            }
            if ( softPenalty > 0.0 ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_SOFT_PENALIZED;
            }
            if ( record.decision == nav2_occupancy_decision_t::RequiresInteraction ) {
                record.flags |= NAV2_OCCUPANCY_FLAG_REQUIRES_INTERACTION;
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
* @brief Merge a corridor into sparse occupancy and overlay summaries.
* @param grid Occupancy grid to mutate.
* @param corridor	Corridor to import.
* @param stamp_frame	Frame stamp to record on imported records.
* @return True when at least one record was imported.
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
        record.decision = ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ? nav2_occupancy_decision_t::Blocked
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
 * @brief Rebuild sparse occupancy and overlay data from span-grid and inline-entity metadata.
 * @param grid Occupancy grid to rebuild.
 * @param overlay Dynamic overlay to rebuild.
 * @param span_grid Span-grid source used for localized occupancy seeding.
 * @param entities Optional inline BSP entity semantics to import.
 * @param snapshot_runtime Optional snapshot runtime to bump occupancy publication version.
 * @param stamp_frame Frame stamp to record on imported records.
 * @param out_summary [out] Optional bounded summary for diagnostics.
 * @return True when at least one occupancy or overlay record was imported.
 **/
const bool SVG_Nav2_RebuildDynamicOccupancy( nav2_occupancy_grid_t *grid, nav2_dynamic_overlay_t *overlay, const nav2_span_grid_t &span_grid,
    const nav2_inline_bsp_entity_list_t *entities, nav2_snapshot_runtime_t *snapshot_runtime, const int64_t stamp_frame, nav2_occupancy_summary_t *out_summary ) {
    /**
    *    Require output storage before rebuilding sparse occupancy state.
    **/
    if ( !grid || !overlay ) {
        return false;
    }

    /**
    *    Reset occupancy and overlay storage so the rebuilt view remains deterministic per publication tick.
    **/
    SVG_Nav2_OccupancyGrid_Clear( grid );
    SVG_Nav2_DynamicOverlay_Clear( overlay );
    if ( out_summary ) {
        *out_summary = {};
    }

    /**
    *    Import conservative static occupancy from the current span-grid publication first.
    **/
    const bool importedSpanOccupancy = SVG_Nav2_ImportSpanGridOccupancy( grid, span_grid, stamp_frame );
    if ( out_summary ) {
        out_summary->span_count = ( int32_t )grid->by_span_id.size();
    }

    /**
    *    Gather localized dynamic entity occupancy and attach dynamic overlay entries.
    **/
    const bool importedEntityOccupancy = SVG_Nav2_GatherLocalizedEntityOccupancy( grid, overlay, span_grid, entities, stamp_frame, out_summary );

    /**
    *    Publish occupancy version when requested so worker snapshots can revalidate against rebuilt overlay state.
    **/
    if ( snapshot_runtime ) {
        const uint32_t publishedVersion = SVG_Nav2_Snapshot_BumpOccupancyVersion( snapshot_runtime );
        if ( out_summary ) {
            out_summary->published_occupancy_version = publishedVersion;
        }
    }

    return importedSpanOccupancy || importedEntityOccupancy;
}

/**
* @brief Emit a bounded debug summary for occupancy and overlay data.
* @param grid Occupancy grid to report.
* @param overlay Dynamic overlay to report.
* @param limit Maximum number of entries to print.
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
        gi.dprintf( "[NAV2][Occupancy] id=%d kind=%d span=%d edge=%d conn=%d mover=%d anchor=%d mregion=%d leaf=%d cluster=%d area=%d decision=%d soft=%.2f flags=0x%08x role=0x%08x move=%d solid=%d z=(%.1f..%.1f) stamp=%lld\n",
            record.occupancy_id,
            ( int32_t )record.kind,
            record.span_id,
            record.edge_id,
            record.connector_id,
            record.mover_entnum,
            record.mover_anchor_id,
            record.mover_region_id,
            record.leaf_id,
            record.cluster_id,
            record.area_id,
            ( int32_t )record.decision,
            record.soft_penalty,
            record.flags,
            record.role_flags,
            record.move_type,
            record.solid_type,
            record.allowed_z_band.min_z,
            record.allowed_z_band.max_z,
            ( long long )record.stamp_frame );
    }

    // Emit bounded overlay details so localized portal/mover invalidation state is visible during runtime validation.
    const int32_t overlayReportCount = std::min( ( int32_t )overlay.entries.size(), limit );
    for ( int32_t i = 0; i < overlayReportCount; ++i ) {
        const nav2_occupancy_overlay_entry_t &entry = overlay.entries[ ( size_t )i ];
        gi.dprintf( "[NAV2][OccupancyOverlay] id=%d leaf=%d cluster=%d area=%d region=%d portal=%d cell=%d mover=(%d,%d) decision=%d soft=%.2f flags=0x%08x stamp=%lld\n",
            entry.overlay_id,
            entry.topology.leaf_index,
            entry.topology.cluster_id,
            entry.topology.area_id,
            entry.topology.region_id,
            entry.topology.portal_id,
            entry.topology.cell_index,
            entry.mover_ref.owner_entnum,
            entry.mover_ref.model_index,
            ( int32_t )entry.decision,
            entry.soft_penalty,
            entry.flags,
            ( long long )entry.stamp_frame );
    }
}

/**
* @brief Keep the nav2 occupancy module represented in the build.
**/
void SVG_Nav2_Occupancy_ModuleAnchor( void ) {
}
