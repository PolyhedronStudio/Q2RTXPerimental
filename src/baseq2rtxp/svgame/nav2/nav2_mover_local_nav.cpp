/********************************************************************
*
*
*	ServerGame: Nav2 Mover-Local Navigation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_mover_local_nav.h"

#include "svgame/nav2/nav2_fine_astar.h"
#include "svgame/nav2/nav2_snapshot.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>


/**
*
*
*	Nav2 Mover-Local Navigation Local Helpers:
*
*
**/
/**
* @brief Compute a mover-local orthonormal basis from the mover angles.
* @param	angles	World-space mover angles.
* @param	out_forward	[out] Forward basis vector.
* @param	out_right	[out] Right basis vector.
* @param	out_up	[out] Up basis vector.
* @return	True when the basis could be derived.
**/
static const bool SVG_Nav2_BuildMoverBasis( const Vector3 &angles, Vector3 *out_forward, Vector3 *out_right, Vector3 *out_up ) {
    // Require the output basis vectors.
    if ( !out_forward || !out_right || !out_up ) {
        return false;
    }

    // Reset outputs before attempting the basis conversion.
    *out_forward = {};
    *out_right = {};
    *out_up = {};

    // Convert mover angles into basis vectors using the engine math helper.
    QM_AngleVectors( angles, out_forward, out_right, out_up );

    // Validate the generated basis.
    const double forward_len2 = ( out_forward->x * out_forward->x ) + ( out_forward->y * out_forward->y ) + ( out_forward->z * out_forward->z );
    const double right_len2 = ( out_right->x * out_right->x ) + ( out_right->y * out_right->y ) + ( out_right->z * out_right->z );
    const double up_len2 = ( out_up->x * out_up->x ) + ( out_up->y * out_up->y ) + ( out_up->z * out_up->z );
    return forward_len2 > 0.0 && right_len2 > 0.0 && up_len2 > 0.0;
}

/**
* @brief Project a world-space point into mover-local coordinates.
* @param	transform	Mover transform snapshot.
* @param	world_origin	World-space point to project.
* @param	out_local_origin	[out] Projected local-space point.
* @return	True when the transform is usable.
**/
static const bool SVG_Nav2_ProjectWorldToLocal( const nav2_mover_transform_t &transform, const Vector3 &world_origin, Vector3 *out_local_origin ) {
    // Validate output storage and transform availability.
    if ( !out_local_origin || !transform.is_valid ) {
        return false;
    }

    // Convert the world point into a basis-relative offset.
    const Vector3 delta = world_origin - transform.world_origin;
  out_local_origin->x = QM_Vector3DotProductDP( delta, transform.local_forward );
    out_local_origin->y = QM_Vector3DotProductDP( delta, transform.local_right );
    out_local_origin->z = QM_Vector3DotProductDP( delta, transform.local_up );
    return true;
}

/**
* @brief Compute squared distance between two world-space positions.
* @param a First world-space point.
* @param b Second world-space point.
* @return Squared distance value.
**/
static const double SVG_Nav2_DistanceSquared( const Vector3 &a, const Vector3 &b ) {
    // Compute axis deltas once for stable and branch-free distance math.
    const double dx = ( double )a.x - ( double )b.x;
    const double dy = ( double )a.y - ( double )b.y;
    const double dz = ( double )a.z - ( double )b.z;
    return ( dx * dx ) + ( dy * dy ) + ( dz * dz );
}

/**
* @brief Resolve one fine-search node by stable node id.
* @param fineState Fine-search state to inspect.
* @param nodeId Stable node id to resolve.
* @return Pointer to the requested node, or nullptr when missing.
**/
static const nav2_fine_astar_node_t *SVG_Nav2_FindFineNodeById( const nav2_fine_astar_state_t &fineState, const int32_t nodeId ) {
    /**
    *    Prefer stable id-to-index lookup first so resumable path ids remain deterministic.
    **/
    const auto it = fineState.node_id_to_index.find( nodeId );
    if ( it != fineState.node_id_to_index.end() ) {
        const int32_t nodeIndex = it->second;
        if ( nodeIndex >= 0 && nodeIndex < ( int32_t )fineState.nodes.size() ) {
            return &fineState.nodes[ ( size_t )nodeIndex ];
        }
    }

    /**
    *    Fall back to linear scan only when the lookup table is missing this id.
    **/
    for ( const nav2_fine_astar_node_t &node : fineState.nodes ) {
        if ( node.node_id == nodeId ) {
            return &node;
        }
    }
    return nullptr;
}

/**
* @brief Resolve an approximate world-space anchor for a mover-local refinement candidate node.
* @param fineState Fine-search state owning corridor segment anchors.
* @param node Candidate fine-search node.
* @param fallbackWorld Fallback world-space origin when no segment anchor matches.
* @param outWorld [out] Resolved world-space anchor.
* @return True when a world anchor was produced.
**/
static const bool SVG_Nav2_ResolveNodeWorldAnchor( const nav2_fine_astar_state_t &fineState, const nav2_fine_astar_node_t &node,
    const Vector3 &fallbackWorld, Vector3 *outWorld ) {
    /**
    *    Require output storage before resolving segment anchors.
    **/
    if ( !outWorld ) {
        return false;
    }

    /**
    *    Start with fallback world origin so callers always receive a stable value.
    **/
    *outWorld = fallbackWorld;

    /**
    *    Return fallback immediately when no corridor segments are available.
    **/
    if ( fineState.corridor.segments.empty() ) {
        return true;
    }

    /**
    *    Find the closest-matching segment by connector/topology/mover identity.
    **/
    const nav2_corridor_segment_t *bestSegment = nullptr;
    int32_t bestScore = std::numeric_limits<int32_t>::max();
    for ( const nav2_corridor_segment_t &segment : fineState.corridor.segments ) {
        int32_t score = 0;

        // Prioritize exact connector matches when available.
        if ( node.connector_id >= 0 && segment.topology.portal_id >= 0 && node.connector_id != segment.topology.portal_id ) {
            score += 8;
        }

        // Prioritize matching region commitments when both sides provide ids.
        if ( node.topology.region_id != NAV_REGION_ID_NONE && segment.topology.region_id != NAV_REGION_ID_NONE
            && node.topology.region_id != segment.topology.region_id ) {
            score += 4;
        }

        // Prefer matching mover ownership for mover-local anchors.
        if ( node.mover_entnum >= 0 && segment.mover_ref.owner_entnum >= 0 && node.mover_entnum != segment.mover_ref.owner_entnum ) {
            score += 4;
        }

        // Prefer matching cluster and area memberships when present.
        if ( node.topology.cluster_id >= 0 && segment.topology.cluster_id >= 0 && node.topology.cluster_id != segment.topology.cluster_id ) {
            score += 2;
        }
        if ( node.topology.area_id >= 0 && segment.topology.area_id >= 0 && node.topology.area_id != segment.topology.area_id ) {
            score += 2;
        }

        if ( !bestSegment || score < bestScore ) {
            bestSegment = &segment;
            bestScore = score;
        }
    }

    /**
    *    Use the best segment midpoint as the world anchor when one is available.
    **/
    if ( bestSegment ) {
        outWorld->x = ( bestSegment->start_anchor.x + bestSegment->end_anchor.x ) * 0.5f;
        outWorld->y = ( bestSegment->start_anchor.y + bestSegment->end_anchor.y ) * 0.5f;
        outWorld->z = ( bestSegment->start_anchor.z + bestSegment->end_anchor.z ) * 0.5f;
    }
    return true;
}

/**
* @brief Return whether one fine-search node should be treated as a mover-local anchor candidate.
* @param node Candidate fine-search node.
* @return True when the node carries mover-local anchor semantics.
**/
static const bool SVG_Nav2_IsMoverAnchorCandidate( const nav2_fine_astar_node_t &node ) {
    /**
    *    Treat explicit mover-anchor nodes and nodes with mover references as candidates.
    **/
    return node.kind == nav2_fine_astar_node_kind_t::MoverAnchor
        || node.mover_entnum >= 0
        || node.inline_model_index >= 0
        || ( node.flags & NAV2_FINE_ASTAR_NODE_FLAG_HAS_MOVER ) != 0;
}

/**
* @brief Return whether one candidate node is compatible with the current mover transform identity.
* @param transform Current mover transform snapshot.
* @param node Candidate fine-search node.
* @return True when mover identity remains compatible.
**/
static const bool SVG_Nav2_IsCandidateTransformCompatible( const nav2_mover_transform_t &transform, const nav2_fine_astar_node_t &node ) {
    /**
    *    Reject candidates with explicit mover entnum mismatch.
    **/
    if ( node.mover_entnum >= 0 && transform.owner_entnum >= 0 && node.mover_entnum != transform.owner_entnum ) {
        return false;
    }

    /**
    *    Reject candidates with explicit inline-model mismatch.
    **/
    if ( node.inline_model_index >= 0 && transform.inline_model_index >= 0 && node.inline_model_index != transform.inline_model_index ) {
        return false;
    }
    return true;
}

/**
* @brief Project a mover-local point back into world coordinates.
* @param	transform	Mover transform snapshot.
* @param	local_origin	Mover-local point to project.
* @param	out_world_origin	[out] Projected world-space point.
* @return	True when the transform is usable.
**/
static const bool SVG_Nav2_ProjectLocalToWorld( const nav2_mover_transform_t &transform, const Vector3 &local_origin, Vector3 *out_world_origin ) {
    // Validate output storage and transform availability.
    if ( !out_world_origin || !transform.is_valid ) {
        return false;
    }

    // Rebuild the world point from the local basis vectors.
    *out_world_origin = transform.world_origin;
    *out_world_origin += transform.local_forward * local_origin.x;
    *out_world_origin += transform.local_right * local_origin.y;
    *out_world_origin += transform.local_up * local_origin.z;
    return true;
}


/**
*
*
*	Nav2 Mover-Local Navigation Public API:
*
*
**/
/**
* @brief Build a mover transform snapshot from a classified inline BSP entity.
* @param	entityInfo	Inline BSP entity classification record.
* @param	ent	Entity to snapshot.
* @param	out_transform	[out] Transform snapshot receiving mover-local basis data.
* @return	True when the transform was derived successfully.
**/
const bool SVG_Nav2_BuildMoverTransform( const nav2_inline_bsp_entity_info_t &entityInfo, const svg_base_edict_t *ent, nav2_mover_transform_t *out_transform ) {
    // Validate inputs.
    if ( !ent || !out_transform ) {
        return false;
    }

    // Reset the output transform.
    *out_transform = {};
    out_transform->owner_entnum = entityInfo.owner_entnum;
    out_transform->inline_model_index = entityInfo.inline_model_index;
    out_transform->world_origin = ent->currentOrigin;
    out_transform->world_angles = ent->currentAngles;
    out_transform->has_inline_model = entityInfo.has_inline_model && entityInfo.headnode_valid;

    // Derive an orthonormal basis from the mover angles.
    if ( !SVG_Nav2_BuildMoverBasis( ent->currentAngles, &out_transform->local_forward, &out_transform->local_right, &out_transform->local_up ) ) {
        return false;
    }

    // Compute a local origin that keeps mover-relative storage anchored around the current world position.
    if ( !SVG_Nav2_ProjectWorldToLocal( *out_transform, ent->currentOrigin, &out_transform->local_origin ) ) {
        return false;
    }

    // Mark the transform valid only after all required data was resolved.
    out_transform->is_valid = out_transform->has_inline_model;
    return out_transform->is_valid;
}

/**
* @brief Build a mover-local nav model snapshot from entity classification and transform state.
* @param	entityInfo	Inline BSP entity classification record.
* @param	transform	Mover transform snapshot.
* @param	out_model	[out] Mover-local nav model receiving stable metadata.
* @return	True when the mover-local nav model is usable.
**/
const bool SVG_Nav2_BuildMoverLocalModel( const nav2_inline_bsp_entity_info_t &entityInfo, const nav2_mover_transform_t &transform, nav2_mover_local_model_t *out_model ) {
    // Validate output storage.
    if ( !out_model ) {
        return false;
    }

    // Reset the output model.
    *out_model = {};
    out_model->owner_entnum = entityInfo.owner_entnum;
    out_model->inline_model_index = transform.inline_model_index;
    out_model->headnode_valid = entityInfo.headnode_valid;

    // Assign mover-local semantics from the entity role flags.
    if ( ( entityInfo.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_SURFACE ) != 0 ) {
        out_model->role_flags |= NAV2_MOVER_LOCAL_ROLE_RIDEABLE;
    }
    if ( ( entityInfo.role_flags & NAV2_INLINE_BSP_ROLE_RIDEABLE_TRAVERSABLE ) != 0 ) {
        out_model->role_flags |= NAV2_MOVER_LOCAL_ROLE_TRAVERSABLE;
    }
    if ( ( entityInfo.role_flags & NAV2_INLINE_BSP_ROLE_TIMED_CONNECTOR ) != 0 ) {
        out_model->role_flags |= NAV2_MOVER_LOCAL_ROLE_TIMED_CONNECTOR;
    }
    if ( ( entityInfo.role_flags & NAV2_INLINE_BSP_ROLE_ROTATING_HAZARD ) != 0 ) {
        out_model->role_flags |= NAV2_MOVER_LOCAL_ROLE_ROTATING_HAZARD;
    }
    if ( out_model->role_flags == NAV2_MOVER_LOCAL_ROLE_NONE ) {
        out_model->role_flags = NAV2_MOVER_LOCAL_ROLE_BLOCKER_ONLY;
    }

    // Mover-local reuse is only safe when the inline model is known and the transform is valid.
    out_model->reusable_local_space = transform.is_valid && entityInfo.headnode_valid;
    return true;
}

/**
* @brief Convert a world-space point into mover-local coordinates.
* @param	transform	Mover transform snapshot.
* @param	world_origin	World-space point to convert.
* @param	out_local_origin	[out] Local-space point.
* @return	True when the transform was valid.
**/
const bool SVG_Nav2_WorldToMoverLocal( const nav2_mover_transform_t &transform, const Vector3 &world_origin, Vector3 *out_local_origin ) {
    // Project into mover-local coordinates using the stored basis.
    return SVG_Nav2_ProjectWorldToLocal( transform, world_origin, out_local_origin );
}

/**
* @brief Convert a mover-local point back into world-space coordinates.
* @param	transform	Mover transform snapshot.
* @param	local_origin	Mover-local point to convert.
* @param	out_world_origin	[out] World-space point.
* @return	True when the transform was valid.
**/
const bool SVG_Nav2_MoverLocalToWorld( const nav2_mover_transform_t &transform, const Vector3 &local_origin, Vector3 *out_world_origin ) {
    // Project back into world coordinates using the stored basis.
    return SVG_Nav2_ProjectLocalToWorld( transform, local_origin, out_world_origin );
}

/**
* @brief Capture a mover-relative waypoint from a world-space query result.
* @param	transform	Mover transform snapshot.
* @param	world_origin	World-space waypoint to capture.
* @param	out_waypoint	[out] Waypoint snapshot receiving local and world coordinates.
* @return	True when the waypoint was captured successfully.
**/
const bool SVG_Nav2_CaptureMoverRelativeWaypoint( const nav2_mover_transform_t &transform, const Vector3 &world_origin, nav2_mover_relative_waypoint_t *out_waypoint ) {
    // Validate output storage and transform state.
    if ( !out_waypoint || !transform.is_valid ) {
        return false;
    }

    // Reset the waypoint snapshot before populating it.
    *out_waypoint = {};
    out_waypoint->mover_entnum = transform.owner_entnum;
    out_waypoint->inline_model_index = transform.inline_model_index;
    out_waypoint->world_origin = world_origin;

    // Convert the waypoint into mover-local space.
    if ( !SVG_Nav2_ProjectWorldToLocal( transform, world_origin, &out_waypoint->local_origin ) ) {
        return false;
    }

    // Mark the waypoint valid only after both coordinate spaces were captured.
    out_waypoint->valid = true;
    return true;
}

/**
* @brief Capture a mover-relative waypoint with explicit snapshot version metadata.
* @param transform Mover transform snapshot.
* @param snapshot Snapshot handle used by the query stage.
* @param world_origin World-space waypoint to capture.
* @param out_waypoint [out] Waypoint snapshot receiving local/world coordinates and snapshot versions.
* @return True when the waypoint was captured successfully.
**/
const bool SVG_Nav2_CaptureMoverRelativeWaypointWithSnapshot( const nav2_mover_transform_t &transform, const nav2_snapshot_handle_t &snapshot,
    const Vector3 &world_origin, nav2_mover_relative_waypoint_t *out_waypoint ) {
    /**
    *    Capture standard mover-relative waypoint fields first.
    **/
    if ( !SVG_Nav2_CaptureMoverRelativeWaypoint( transform, world_origin, out_waypoint ) ) {
        return false;
    }

    /**
    *    Attach snapshot versions so load/revalidation logic can detect stale waypoint state.
    **/
    out_waypoint->snapshot_mover_version = snapshot.mover_version;
    out_waypoint->snapshot_model_version = snapshot.model_version;
    return true;
}

/**
* @brief Resolve a mover-relative waypoint back into current world space.
* @param	transform	Current mover transform snapshot.
* @param	waypoint	Mover-relative waypoint to resolve.
* @param	out_world_origin	[out] Current world-space waypoint.
* @return	True when the waypoint could be resolved.
**/
const bool SVG_Nav2_ResolveMoverRelativeWaypointWorld( const nav2_mover_transform_t &transform, const nav2_mover_relative_waypoint_t &waypoint, Vector3 *out_world_origin ) {
    // Require a valid transform and waypoint.
    if ( !out_world_origin || !transform.is_valid || !waypoint.valid ) {
        return false;
    }

    // Convert the stored local waypoint back into world space.
    return SVG_Nav2_ProjectLocalToWorld( transform, waypoint.local_origin, out_world_origin );
}

/**
* @brief Compare a mover-relative waypoint against a current world-space position for drift.
* @param	transform	Current mover transform snapshot.
* @param	waypoint	Mover-relative waypoint to validate.
* @param	current_world_origin	Current world-space point to compare.
* @param	out_world_drift	[out] Optional drift vector.
* @return	True when the waypoint remains valid within transform-aware tolerance.
**/
const bool SVG_Nav2_ValidateMoverRelativeWaypointDrift( const nav2_mover_transform_t &transform, const nav2_mover_relative_waypoint_t &waypoint, const Vector3 &current_world_origin, Vector3 *out_world_drift ) {
    // Resolve the waypoint against the current mover transform.
    Vector3 resolved_world = {};
    if ( !SVG_Nav2_ResolveMoverRelativeWaypointWorld( transform, waypoint, &resolved_world ) ) {
        return false;
    }

    // Compute the drift between the stored world-space capture and the current resolved world position.
    const Vector3 drift = current_world_origin - resolved_world;
    if ( out_world_drift ) {
        *out_world_drift = drift;
    }

    // Use a conservative tolerance so stale mover-relative points can be invalidated early.
    const double drift_len2 = ( drift.x * drift.x ) + ( drift.y * drift.y ) + ( drift.z * drift.z );
    return drift_len2 <= ( 16.0 * 16.0 );
}

/**
* @brief Evaluate whether a mover-relative waypoint can still be used with the current transform.
* @param	transform	Current mover transform snapshot.
* @param	waypoint	Mover-relative waypoint to validate.
* @param	current_world_origin	Current mover world origin to compare.
* @param	out_world_drift	[out] Optional drift vector.
* @return	True when the waypoint remains within tolerance and can be reused.
* @note	Fine-search and mover-follow code should call this before trusting a cached mover-relative waypoint.
**/
const bool SVG_Nav2_ValidateMoverRelativeWaypoint( const nav2_mover_transform_t &transform, const nav2_mover_relative_waypoint_t &waypoint, const Vector3 &current_world_origin, Vector3 *out_world_drift ) {
    // Reject unusable transform or waypoint state up front.
    if ( !transform.is_valid || !waypoint.valid ) {
        return false;
    }

    // Reuse the drift validator so callers get the same tolerance policy in one place.
    return SVG_Nav2_ValidateMoverRelativeWaypointDrift( transform, waypoint, current_world_origin, out_world_drift );
}

/**
* @brief Validate waypoint snapshot versions against current mover/model snapshot versions.
* @param snapshot Current query snapshot handle.
* @param waypoint Waypoint to validate.
* @return True when mover/model snapshot versions remain compatible.
**/
const bool SVG_Nav2_ValidateMoverRelativeWaypointSnapshot( const nav2_snapshot_handle_t &snapshot, const nav2_mover_relative_waypoint_t &waypoint ) {
    /**
    *    Reject invalid waypoints up front.
    **/
    if ( !waypoint.valid ) {
        return false;
    }

    /**
    *    Treat zeroed capture versions as unconstrained compatibility fields.
    **/
    const bool moverVersionMatch = ( waypoint.snapshot_mover_version == 0 ) || ( waypoint.snapshot_mover_version == snapshot.mover_version );
    const bool modelVersionMatch = ( waypoint.snapshot_model_version == 0 ) || ( waypoint.snapshot_model_version == snapshot.model_version );
    return moverVersionMatch && modelVersionMatch;
}

/**
* @brief Capture a mover-relative waypoint from a world-space result and apply a conservative drift tolerance.
* @param	transform	Mover transform snapshot.
* @param	world_origin	World-space waypoint to capture.
* @param	out_waypoint	[out] Waypoint snapshot receiving local and world coordinates.
* @return	True when the waypoint was captured successfully.
* @note	This helper is the preferred entrypoint when caller code wants a validated mover-relative waypoint snapshot.
**/
const bool SVG_Nav2_CaptureValidatedMoverRelativeWaypoint( const nav2_mover_transform_t &transform, const Vector3 &world_origin, nav2_mover_relative_waypoint_t *out_waypoint ) {
    // Capture the waypoint first.
    if ( !SVG_Nav2_CaptureMoverRelativeWaypoint( transform, world_origin, out_waypoint ) ) {
        return false;
    }

    // Revalidate against the original world point so the caller gets a ready-to-use snapshot.
    return SVG_Nav2_ValidateMoverRelativeWaypoint( transform, *out_waypoint, world_origin, nullptr );
}

/**
* @brief Validate transform-aware mover boarding and exit mapping anchors.
* @param transform Current mover transform snapshot.
* @param boarding_waypoint Boarding waypoint to validate.
* @param exit_waypoint Exit waypoint to validate.
* @param out_validation [out] Mapping validation result.
* @return True when both mapping checks pass.
**/
const bool SVG_Nav2_ValidateMoverBoardingExitMapping( const nav2_mover_transform_t &transform,
    const nav2_mover_relative_waypoint_t &boarding_waypoint, const nav2_mover_relative_waypoint_t &exit_waypoint,
    nav2_mover_local_mapping_validation_t *out_validation ) {
    /**
    *    Require output storage for mapping validation details.
    **/
    if ( !out_validation ) {
        return false;
    }
    *out_validation = {};

    /**
    *    Validate boarding and exit waypoint drift independently against their captured world-space anchors.
    **/
    out_validation->boarding_valid = SVG_Nav2_ValidateMoverRelativeWaypointDrift( transform, boarding_waypoint,
        boarding_waypoint.world_origin, &out_validation->boarding_drift );
    out_validation->exit_valid = SVG_Nav2_ValidateMoverRelativeWaypointDrift( transform, exit_waypoint,
        exit_waypoint.world_origin, &out_validation->exit_drift );
    return out_validation->boarding_valid && out_validation->exit_valid;
}

/**
* @brief Run bounded mover-local refinement over an existing fine-search path.
* @param transform Current mover transform snapshot.
* @param snapshot Current query snapshot handle.
* @param fine_state Fine-search state supplying path and node metadata.
* @param max_waypoints Maximum mover-relative waypoints to emit.
* @param out_result [out] Mover-local refinement result.
* @return True when the refinement pass ran successfully.
**/
const bool SVG_Nav2_RunMoverLocalRefinement( const nav2_mover_transform_t &transform, const nav2_snapshot_handle_t &snapshot,
    const nav2_fine_astar_state_t &fine_state, const int32_t max_waypoints, nav2_mover_local_refine_result_t *out_result ) {
    /**
    *    Require output storage and a valid mover transform before refinement begins.
    **/
    if ( !out_result || !transform.is_valid ) {
        return false;
    }
    *out_result = {};

    /**
    *    Clamp waypoint emission cap so debug callers cannot force unbounded output.
    **/
    const int32_t waypointCap = std::max( 0, std::min( max_waypoints, 64 ) );

    /**
    *    Iterate the fine path in order and capture mover-local waypoints for compatible mover-anchor nodes.
    **/
    for ( const int32_t nodeId : fine_state.path.node_ids ) {
        // Track inspected nodes for bounded diagnostics.
        out_result->diagnostics.inspected_nodes++;

        // Resolve path node metadata through stable ids.
        const nav2_fine_astar_node_t *node = SVG_Nav2_FindFineNodeById( fine_state, nodeId );
        if ( !node ) {
            continue;
        }

        // Keep only mover-anchor compatible nodes for this refinement pass.
        if ( !SVG_Nav2_IsMoverAnchorCandidate( *node ) ) {
            continue;
        }
        out_result->diagnostics.mover_anchor_candidates++;

        // Reject transform-mismatched anchor candidates.
        if ( !SVG_Nav2_IsCandidateTransformCompatible( transform, *node ) ) {
            out_result->diagnostics.transform_rejects++;
            continue;
        }

        // Resolve world anchor for the candidate from corridor commitments.
        Vector3 worldAnchor = {};
        if ( !SVG_Nav2_ResolveNodeWorldAnchor( fine_state, *node, transform.world_origin, &worldAnchor ) ) {
            out_result->diagnostics.mapping_rejects++;
            continue;
        }

        // Capture snapshot-aware mover-relative waypoint state.
        nav2_mover_relative_waypoint_t waypoint = {};
        if ( !SVG_Nav2_CaptureMoverRelativeWaypointWithSnapshot( transform, snapshot, worldAnchor, &waypoint ) ) {
            out_result->diagnostics.mapping_rejects++;
            continue;
        }

        // Reject stale snapshot captures.
        if ( !SVG_Nav2_ValidateMoverRelativeWaypointSnapshot( snapshot, waypoint ) ) {
            out_result->diagnostics.stale_rejects++;
            continue;
        }

        // Validate transform-aware drift against the selected anchor.
        if ( !SVG_Nav2_ValidateMoverRelativeWaypoint( transform, waypoint, worldAnchor, nullptr ) ) {
            out_result->diagnostics.mapping_rejects++;
            continue;
        }

        // Append accepted waypoint and stop at cap.
        out_result->waypoints.push_back( waypoint );
        out_result->diagnostics.mover_anchor_accepts++;
        if ( waypointCap > 0 && ( int32_t )out_result->waypoints.size() >= waypointCap ) {
            break;
        }
    }

    /**
    *    Publish compact result status for callers.
    **/
    out_result->has_waypoints = !out_result->waypoints.empty();
    out_result->success = true;
    return true;
}

/**
* @brief Emit bounded debug output for mover-local refinement results.
* @param result Mover-local refinement result to report.
* @param max_print Maximum waypoint lines to emit.
**/
void SVG_Nav2_DebugPrintMoverLocalRefinement( const nav2_mover_local_refine_result_t &result, const int32_t max_print ) {
    /**
    *    Clamp print count to bounded diagnostics volume.
    **/
    const int32_t printCount = std::max( 0, std::min( max_print, ( int32_t )result.waypoints.size() ) );

    /**
    *    Emit one compact summary line first.
    **/
    gi.dprintf( "[NAV2][MoverRefine] success=%d waypoints=%d inspected=%u candidates=%u accepts=%u transformReject=%u mappingReject=%u staleReject=%u\n",
        result.success ? 1 : 0,
        ( int32_t )result.waypoints.size(),
        result.diagnostics.inspected_nodes,
        result.diagnostics.mover_anchor_candidates,
        result.diagnostics.mover_anchor_accepts,
        result.diagnostics.transform_rejects,
        result.diagnostics.mapping_rejects,
        result.diagnostics.stale_rejects );

    /**
    *    Emit bounded waypoint details for live diagnostics.
    **/
    for ( int32_t i = 0; i < printCount; i++ ) {
        const nav2_mover_relative_waypoint_t &waypoint = result.waypoints[ ( size_t )i ];
        gi.dprintf( "[NAV2][MoverRefine] #%d mover=%d model=%d local=(%.1f %.1f %.1f) world=(%.1f %.1f %.1f) snap=(mover=%u model=%u) valid=%d\n",
            i,
            waypoint.mover_entnum,
            waypoint.inline_model_index,
            waypoint.local_origin.x, waypoint.local_origin.y, waypoint.local_origin.z,
            waypoint.world_origin.x, waypoint.world_origin.y, waypoint.world_origin.z,
            waypoint.snapshot_mover_version,
            waypoint.snapshot_model_version,
            waypoint.valid ? 1 : 0 );
    }
}

/**
* @brief Emit a bounded debug summary for mover-local nav data.
* @param	model	Mover-local nav model snapshot.
* @param	transform	Mover transform snapshot.
**/
void SVG_Nav2_DebugPrintMoverLocalNav( const nav2_mover_local_model_t &model, const nav2_mover_transform_t &transform ) {
    // Keep diagnostics concise and stable.
    gi.dprintf( "[NAV2][MoverLocal] ent=%d inline=%d roles=0x%08x reusable=%d transform=%d headnode=%d origin=(%.1f %.1f %.1f) angles=(%.1f %.1f %.1f)\n",
        model.owner_entnum,
        model.inline_model_index,
        model.role_flags,
        model.reusable_local_space ? 1 : 0,
        transform.is_valid ? 1 : 0,
        model.headnode_valid ? 1 : 0,
        transform.world_origin.x, transform.world_origin.y, transform.world_origin.z,
        transform.world_angles.x, transform.world_angles.y, transform.world_angles.z );
}

/**
* @brief Keep the nav2 mover-local nav module represented in the build.
**/
void SVG_Nav2_MoverLocalNav_ModuleAnchor( void ) {
}
