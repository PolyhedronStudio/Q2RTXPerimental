/********************************************************************
*
*
*	ServerGame: Nav2 Mover-Local Navigation
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_entity_semantics.h"
#include "svgame/nav2/nav2_query_iface.h"


/**
*
*
*	Nav2 Mover-Local Navigation Enumerations:
*
*
**/
/**
* @brief Runtime mover-local traversal role tags.
* @note These are pointer-free so mover-local state can be snapshotted safely.
**/
enum nav2_mover_local_role_bits_t : uint32_t {
    //! No mover-local role assigned.
    NAV2_MOVER_LOCAL_ROLE_NONE = 0,
    //! Mover behaves as a pure blocker for local nav.
    NAV2_MOVER_LOCAL_ROLE_BLOCKER_ONLY = ( 1u << 0 ),
    //! Mover provides a rideable surface.
    NAV2_MOVER_LOCAL_ROLE_RIDEABLE = ( 1u << 1 ),
    //! Mover provides a traversable local subspace.
    NAV2_MOVER_LOCAL_ROLE_TRAVERSABLE = ( 1u << 2 ),
    //! Mover acts as a timed connector.
    NAV2_MOVER_LOCAL_ROLE_TIMED_CONNECTOR = ( 1u << 3 ),
    //! Mover is a rotating hazard.
    NAV2_MOVER_LOCAL_ROLE_ROTATING_HAZARD = ( 1u << 4 )
};


/**
*
*
*	Nav2 Mover-Local Navigation Data Structures:
*
*
**/
/**
* @brief Stable transform snapshot for mover-local query work.
* @note The world/local matrices are stored as raw scalar vectors to remain serialization-friendly.
**/
struct nav2_mover_transform_t {
    //! Mover entity number.
    int32_t owner_entnum = -1;
    //! Inline model index when available.
    int32_t inline_model_index = -1;
    //! World-space origin.
    Vector3 world_origin = {};
    //! World-space angles.
    Vector3 world_angles = {};
    //! Local-space origin derived from the mover transform.
    Vector3 local_origin = {};
    //! Local-space axis basis used by transform helpers.
    Vector3 local_forward = {};
    Vector3 local_right = {};
    Vector3 local_up = {};
    //! True when the transform was derived from a valid inline model.
    bool has_inline_model = false;
    //! True when the transform basis and headnode metadata were resolved.
    bool is_valid = false;
};

/**
* @brief Stable mover-local nav model snapshot.
* @note This exposes only pointer-free metadata required for runtime query validation.
**/
struct nav2_mover_local_model_t {
    //! Entity number owning this mover-local model.
    int32_t owner_entnum = -1;
    //! Inline model index for the mover-local model.
    int32_t inline_model_index = -1;
    //! Resolved role flags for mover-local traversal semantics.
    uint32_t role_flags = NAV2_MOVER_LOCAL_ROLE_NONE;
    //! Headnode pointer-free availability flag.
    bool headnode_valid = false;
    //! True when the model is treated as a reusable local navigation space.
    bool reusable_local_space = false;
};

/**
* @brief Mover-relative waypoint stored against a mover-local nav space.
* @note Waypoints are stored in mover-local coordinates so active movers can be updated without stale world-space drift.
**/
struct nav2_mover_relative_waypoint_t {
    //! Owning mover entity number.
    int32_t mover_entnum = -1;
    //! Inline-model index used when the waypoint was created.
    int32_t inline_model_index = -1;
    //! Waypoint position in mover-local coordinates.
    Vector3 local_origin = {};
    //! Waypoint position in world coordinates at the time of capture.
    Vector3 world_origin = {};
    //! True when the waypoint was captured from a transform-aware mover-local query.
    bool valid = false;
};


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
const bool SVG_Nav2_BuildMoverTransform( const nav2_inline_bsp_entity_info_t &entityInfo, const svg_base_edict_t *ent, nav2_mover_transform_t *out_transform );

/**
* @brief Build a mover-local nav model snapshot from entity classification and transform state.
* @param	entityInfo	Inline BSP entity classification record.
* @param	transform	Mover transform snapshot.
* @param	out_model	[out] Mover-local nav model receiving stable metadata.
* @return	True when the mover-local nav model is usable.
**/
const bool SVG_Nav2_BuildMoverLocalModel( const nav2_inline_bsp_entity_info_t &entityInfo, const nav2_mover_transform_t &transform, nav2_mover_local_model_t *out_model );

/**
* @brief Convert a world-space point into mover-local coordinates.
* @param	transform	Mover transform snapshot.
* @param	world_origin	World-space point to convert.
* @param	out_local_origin	[out] Local-space point.
* @return	True when the transform was valid.
**/
const bool SVG_Nav2_WorldToMoverLocal( const nav2_mover_transform_t &transform, const Vector3 &world_origin, Vector3 *out_local_origin );

/**
* @brief Convert a mover-local point back into world-space coordinates.
* @param	transform	Mover transform snapshot.
* @param	local_origin	Mover-local point to convert.
* @param	out_world_origin	[out] World-space point.
* @return	True when the transform was valid.
**/
const bool SVG_Nav2_MoverLocalToWorld( const nav2_mover_transform_t &transform, const Vector3 &local_origin, Vector3 *out_world_origin );

/**
* @brief Capture a mover-relative waypoint from a world-space query result.
* @param	transform	Mover transform snapshot.
* @param	world_origin	World-space waypoint to capture.
* @param	out_waypoint	[out] Waypoint snapshot receiving local and world coordinates.
* @return	True when the waypoint was captured successfully.
**/
const bool SVG_Nav2_CaptureMoverRelativeWaypoint( const nav2_mover_transform_t &transform, const Vector3 &world_origin, nav2_mover_relative_waypoint_t *out_waypoint );

/**
* @brief Resolve a mover-relative waypoint back into current world space.
* @param	transform	Current mover transform snapshot.
* @param	waypoint	Mover-relative waypoint to resolve.
* @param	out_world_origin	[out] Current world-space waypoint.
* @return	True when the waypoint could be resolved.
**/
const bool SVG_Nav2_ResolveMoverRelativeWaypointWorld( const nav2_mover_transform_t &transform, const nav2_mover_relative_waypoint_t &waypoint, Vector3 *out_world_origin );

/**
* @brief Compare a mover-relative waypoint against a current world-space position for drift.
* @param	transform	Current mover transform snapshot.
* @param	waypoint	Mover-relative waypoint to validate.
* @param	current_world_origin	Current world-space point to compare.
* @param	out_world_drift	[out] Optional drift vector.
* @return	True when the waypoint remains valid within transform-aware tolerance.
**/
const bool SVG_Nav2_ValidateMoverRelativeWaypointDrift( const nav2_mover_transform_t &transform, const nav2_mover_relative_waypoint_t &waypoint, const Vector3 &current_world_origin, Vector3 *out_world_drift = nullptr );

/**
* @brief Evaluate whether a mover-relative waypoint can still be used with the current transform.
* @param	transform	Current mover transform snapshot.
* @param	waypoint	Mover-relative waypoint to validate.
* @param	current_world_origin	Current mover world origin to compare.
* @param	out_world_drift	[out] Optional drift vector.
* @return	True when the waypoint remains within tolerance and can be reused.
* @note	This is the safety gate fine-search and mover-follow logic should consult before trusting cached mover-relative path state.
**/
const bool SVG_Nav2_ValidateMoverRelativeWaypoint( const nav2_mover_transform_t &transform, const nav2_mover_relative_waypoint_t &waypoint, const Vector3 &current_world_origin, Vector3 *out_world_drift = nullptr );

/**
* @brief Capture a mover-relative waypoint from a world-space result and apply a conservative drift tolerance.
* @param	transform	Mover transform snapshot.
* @param	world_origin	World-space waypoint to capture.
* @param	out_waypoint	[out] Waypoint snapshot receiving local and world coordinates.
* @return	True when the waypoint was captured successfully.
* @note	This is the preferred helper for fine-search and follow-state code because it validates the snapshot in one step.
**/
const bool SVG_Nav2_CaptureValidatedMoverRelativeWaypoint( const nav2_mover_transform_t &transform, const Vector3 &world_origin, nav2_mover_relative_waypoint_t *out_waypoint );

/**
* @brief Emit a bounded debug summary for mover-local nav data.
* @param	model	Mover-local nav model snapshot.
* @param	transform	Mover transform snapshot.
**/
void SVG_Nav2_DebugPrintMoverLocalNav( const nav2_mover_local_model_t &model, const nav2_mover_transform_t &transform );

/**
* @brief Keep the nav2 mover-local nav module represented in the build.
**/
void SVG_Nav2_MoverLocalNav_ModuleAnchor( void );
