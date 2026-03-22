/********************************************************************
*
*
*	ServerGame: Nav2 Mover-Local Navigation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_mover_local_nav.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>


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
