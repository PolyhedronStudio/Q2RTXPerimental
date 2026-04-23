/********************************************************************
*
*
*	ServerGame: Nav2 Query Interface Compatibility Layer
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_goal_candidates.h"
#include "svgame/nav2/nav2_runtime.h"
#include "svgame/nav2/nav2_types.h"

struct nav2_mesh_t;
struct nav2_query_policy_t;

using nav_request_handle_t = int32_t;


/**
*	@brief	Nav2-owned traversal path snapshot used by the staged public query process state.
*	@note	This mirrors only the currently published waypoint buffer layout while nav2 absorbs full path-process ownership.
**/
struct nav2_query_traversal_path_t {
	//! Number of path points in the traversal path.
	int32_t num_points = 0;
	//! Array of world-space path points owned by the staged path process implementation.
	Vector3 *points = nullptr;
};

/**
*	@brief	Nav2-owned shared follow-state bundle returned by `nav2_query_process_t::QueryFollowState`.
*	@note	This mirrors the currently consumed staged follow-state semantics while nav2 owns the public query surface.
**/
struct nav2_query_follow_state_t {
	//! True when `move_direction3d` contains a usable direction for this frame.
	bool has_direction = false;
	//! Normalized 3D direction returned by the shared path query.
	Vector3 move_direction3d = {};
	//! Active waypoint converted into feet-origin/entity space.
	Vector3 active_waypoint_origin = {};
	//! Horizontal distance from current origin to the active waypoint.
	double active_waypoint_dist2d = 0.0;
	//! Absolute vertical delta between current origin and active waypoint.
	double active_waypoint_delta_z = 0.0;
	//! True when the active waypoint should keep tighter stair/corner centering.
	bool waypoint_needs_precise_centering = false;
	//! True when the current anchor sits inside a repeated step-sized vertical corridor.
	bool stepped_vertical_corridor_ahead = false;
	//! True when the current active waypoint is the final path point.
	bool approaching_final_goal = false;
};


/**
*	Legacy-query compatibility aliases.
*		- These aliases provide a future-facing seam for monster code while nav2 is staged in.
*		- The underlying implementation still routes through the quarantined legacy nav code for now.
**/
using nav2_query_agent_profile_t = nav2_agent_profile_t;

/**
*	@brief	Nav2-owned wrapper for staged navigation mesh access.
*	@note	This keeps the public nav2 query seam from exposing the `main mesh` type directly while preserving
*			pointer-like access for the current staged callers that still need read-only mesh metadata.
**/
struct nav2_query_mesh_t {
	//! Wrapped staged `main mesh` pointer.
	const nav2_mesh_t *main_mesh = nullptr;

	/**
	*	@brief	Return whether this wrapper references an active staged mesh.
	*	@return	True when `main_mesh` is not `nullptr`.
	**/
	const bool IsValid() const {
		return main_mesh != nullptr;
	}
};

/**
*	@brief	Nav2-owned mesh metadata snapshot exposed through the public query seam.
*	@note	This mirrors only the currently consumed mesh-level scalars so callers stop depending on staged mesh layout details.
**/
struct nav2_query_mesh_meta_t {
 //! Number of BSP leaves published by the active mesh when known.
	int32_t leaf_count = 0;
	//! Number of XY cells along one tile edge.
	int32_t tile_size = NAV_TILE_DEFAULT_SIZE;
	//! World-space XY size of one nav cell.
	double cell_size_xy = NAV_TILE_DEFAULT_CELL_SIZE_XY;
	//! World-space size multiplier used for quantized layer heights.
	double z_quant = NAV_TILE_DEFAULT_Z_QUANT;
	//! Agent bounding-box minimum extents mirrored from the active mesh when known.
	Vector3 agent_mins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
	//! Agent bounding-box maximum extents mirrored from the active mesh when known.
	Vector3 agent_maxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;

	/**
	*	@brief	Return whether the mirrored tile sizing metadata is usable.
	*	@return	True when both tile and cell sizing are positive.
	**/
	const bool HasTileSizing() const {
		return tile_size > 0 && cell_size_xy > 0.0;
	}

	/**
	*	@brief	Return whether the mirrored agent bounds are usable.
	*	@return	True when all max extents exceed the corresponding mins.
	**/
	const bool HasAgentBounds() const {
		return agent_maxs.x > agent_mins.x
			&& agent_maxs.y > agent_mins.y
			&& agent_maxs.z > agent_mins.z;
	}

	/**
	*	@brief	Return the world-space size of one tile edge.
	*	@return	Tile world size when sizing metadata is valid, otherwise 0.
	**/
	const double GetTileWorldSize() const {
		return HasTileSizing() ? ( ( double )tile_size * cell_size_xy ) : 0.0;
	}
};

/**
*	@brief	Nav2-owned tile localization metadata derived from a world-space point.
*	@note	This keeps tile-id and cell-index localization available without exposing staged world-tile lookup structures publicly.
**/
struct nav2_query_tile_location_t {
	//! Tile key resolved from the queried point.
	nav2_tile_cluster_key_t tile_key = {};
	//! Stable world-tile id when known.
	int32_t tile_id = -1;
	//! Tile-local cell index when known.
	int32_t cell_index = -1;

	/**
	*	@brief	Return whether the tile localization contains a stable world-tile id.
	*	@return	True when `tile_id` is non-negative.
	**/
	const bool HasTileId() const {
		return tile_id >= 0;
	}

	/**
	*	@brief	Return whether the tile localization contains a valid tile-local cell index.
	*	@return	True when `cell_index` is non-negative.
	**/
	const bool HasCellIndex() const {
		return cell_index >= 0;
	}
};

/**
*	@brief	Nav2-owned inline-model tile membership result resolved through the public query seam.
*	@note	This keeps mover/runtime membership pointer-free while exposing only the stable identifiers current corridor code consumes.
**/
struct nav2_query_inline_model_membership_t {
	//! Owning entity number for the matched inline model when known.
	int32_t owner_entnum = ENTITYNUM_NONE;
	//! Inline-model index for the matched runtime entry when known.
	int32_t model_index = -1;

	/**
	*	@brief	Return whether the membership identifies a concrete inline-model runtime entry.
	*	@return	True when `owner_entnum` or `model_index` is valid.
	**/
	const bool IsValid() const {
		return owner_entnum >= 0 || model_index >= 0;
	}
};

/**
*	@brief	Nav2-owned wrapper for staged per-entity path-process state.
*	@note	This conservatively inherits the staged legacy process layout for now so gameplay field and method access
*			remain source-compatible while nav2 removes the public alias surface first.
**/
struct nav2_query_process_t {
	/**
	*	@brief	Shared follow-state bundle returned by `QueryFollowState`.
	**/
	using follow_state_t = nav2_query_follow_state_t;

	//! Current traversal path.
	nav2_query_traversal_path_t path = {};
	//! Current index along the path.
	int32_t path_index = 0;
	//! Last path goal and start positions.
	Vector3 path_goal_position = {};
	//! Start position used for the current path.
	Vector3 path_start_position = {};
	//! Next time at which a path rebuild can be attempted.
	QMTime next_rebuild_time = 0_ms;
	//! Time until which rebuild attempts are backed off due to failures.
	QMTime backoff_until = 0_ms;
	//! Number of consecutive path rebuild failures.
	int32_t consecutive_failures = 0;
	//! Time of the last rebuild failure.
	QMTime last_failure_time = 0_ms;
	//! Last failing goal position recorded when a rebuild failed.
	Vector3 last_failure_pos = {};
	//! Last failing direction/yaw recorded when a rebuild failed.
	float last_failure_yaw = 0.0f;
	//! Indicates whether an asynchronous rebuild request is currently outstanding.
	bool rebuild_in_progress = false;
	//! Stores the handle of the pending async rebuild request for cancellation/logging.
	int32_t pending_request_handle = 0;
	//! Monotonic request generation used to discard stale async results when requests are replaced.
	uint32_t request_generation = 0;
	//! Time of the last async prep/enqueue attempt for this process.
	QMTime last_prep_time = 0_ms;
	//! Last prep/start position used when the last async prep was performed.
	Vector3 last_prep_start = {};
	//! Last prep/goal position used when the last async prep was performed.
	Vector3 last_prep_goal = {};
	//! Z offset used to convert external feet-origin queries into internal nav-center space.
	float path_center_offset_z = 0.0f;

 /**
	*	Path-process behavior.
	**/
	void Reset( void );
	const bool CommitAsyncPathFromPoints( const std::vector<Vector3> &points, const Vector3 &start_origin, const Vector3 &goal_origin, const nav2_query_policy_t &policy );
	const bool CanRebuild( const nav2_query_policy_t &policy ) const;
	const bool QueryFollowState( const Vector3 &current_origin, const nav2_query_policy_t &policy, follow_state_t *out_state );
};

/**
*	@brief	Nav2-owned wrapper for staged path-follow policy state.
*	@note	This conservatively inherits the staged legacy policy layout for now so gameplay tuning fields remain
*			source-compatible while nav2 removes the public alias surface first.
**/
struct nav2_query_policy_t {
	//! Minimum time interval between path rebuild attempts.
	QMTime rebuild_interval = 0_ms;
	//! Base time for exponential backoff on path rebuild failures.
	QMTime fail_backoff_base = 25_ms;
	//! Maximum exponent for backoff time (2^n).
	int32_t fail_backoff_max_pow = 4;
	//! If true, ignore visibility when deciding whether to rebuild the path.
	bool ignore_visibility = true;
	//! If true, ignore distance in front of the monster when deciding whether to rebuild the path.
	bool ignore_infront = true;
	//! Radius around waypoints to consider reached.
	double waypoint_radius = NAV_DEFAULT_WAYPOINT_RADIUS;
	//! 2D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_2d = NAV_DEFAULT_GOAL_REBUILD_2D_DISTANCE;
	//! 3D distance change in goal position that triggers a path rebuild.
	double rebuild_goal_dist_3d = NAV_DEFAULT_GOAL_REBUILD_3D_DISTANCE;
	//! If true, apply horizontal-distance-based blending from local/query Z toward goal Z during layer selection.
	bool enable_goal_z_layer_blend = true;
	//! Horizontal distance threshold where goal-height influence begins.
	double blend_start_dist = NAV_DEFAULT_BLEND_DIST_START;
	//! Horizontal distance threshold where the desired selection Z becomes fully goal-biased.
	double blend_full_dist = NAV_DEFAULT_BLEND_DIST_FULL;
	//! If true, apply the hierarchical tile-route filter during async A* prep.
	bool enable_cluster_route_filter = true;
	//! If true, forbid edges that explicitly enter water.
	bool forbid_water = true;
	//! If true, forbid edges that explicitly enter lava.
	bool forbid_lava = true;
	//! If true, forbid edges that explicitly enter slime.
	bool forbid_slime = true;
	//! If true, allow explicitly optional walk-off edges.
	bool allow_optional_walk_off = false;
	//! If true, allow explicitly forbidden walk-off edges.
	bool allow_forbidden_walk_off = false;
	//! If true, allow async A* to skip long-hop probes already predicted as pass-through chains.
	bool enable_pass_through_pruning = true;
	//! If true, prefer ladders when they reach the same or a relatively close goal height more directly.
	bool prefer_ladders = true;
	//! Cost bonus applied to ladder-favoring transitions when ladder preference is active.
	double ladder_preference_bias = 12.0;
	//! Maximum goal-height slack within which ladder endpoints are considered meaningfully close to the target height.
	double ladder_preferred_height_slack = 96.0;
	//! If true, consult sparse dynamic occupancy during async neighbor expansion.
	bool use_dynamic_occupancy = true;
	//! If true, occupancy-marked cells remain hard blockers.
	bool hard_block_dynamic_occupancy = false;
	//! Additional scale applied to sparse occupancy soft costs.
	double dynamic_occupancy_soft_cost_scale = 1.0;
	//! Minimal normal angle that will be stepped over.
	double min_step_normal = PHYS_MAX_SLOPE_NORMAL;
	//! Minimum step height that is considered and can be stepped over.
	double min_step_height = PHYS_STEP_MIN_SIZE;
	//! Maximum step height that can be stepped over.
	double max_step_height = PHYS_STEP_MAX_SIZE;
	//! If true, try to perform a small obstacle jump when blocked.
	bool allow_small_obstruction_jump = true;
	//! Max obstruction height allowed to jump over.
	double max_obstruction_jump_height = NAV_DEFAULT_MAX_OBSTRUCTION_JUMP_SIZE;
	//! If true, do not allow moving into a drop deeper than max_drop_height.
	bool enable_max_drop_height_cap = true;
	//! Max allowed drop height.
	double max_drop_height = NAV_DEFAULT_MAX_DROP_HEIGHT;
	//! Drop cap applied when rejecting large downward transitions.
	double max_drop_height_cap = NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;
	//! Agent bounding box minimum extents in local origin space.
	Vector3 agent_mins = { -16.0f, -16.0f, -36.0f };
	//! Agent bounding box maximum extents in local origin space.
	Vector3 agent_maxs = { 16.0f, 16.0f, 36.0f };
	//! Minimum walkable surface normal Z threshold.
	double max_slope_normal_z = PHYS_MAX_SLOPE_NORMAL;
	//! If true, prefer the highest layer in a multi-layer XY cell when the desired Z is close.
	bool layer_select_prefer_top = true;
	//! Z distance in world units under which the top/bottom preference bias applies.
	double layer_select_prefer_z_threshold = NAV_DEFAULT_STEP_MAX_SIZE + NAV_DEFAULT_STEP_MIN_SIZE;
};

/**
*	@brief	Nav2-owned wrapper for an asynchronous query request handle.
*	@note	This keeps the nav2 public query seam from exposing the legacy request-handle typedef directly while
*			remaining trivially copyable for staged migration work.
**/
struct nav2_query_handle_t {
	//! Legacy request identifier mirrored into nav2-owned storage.
	int32_t value = 0;

	/**
	*	@brief	Return whether this wrapper contains a usable request identifier.
	*	@return	True when the wrapped request handle is positive.
	**/
	const bool IsValid() const {
		return value > 0;
	}
};

/**
*	@brief	Nav2-owned wrapper for one canonical query node.
*	@note	This mirrors only nav2-owned foundational key and position data so nav2 headers no longer expose
*			the legacy node-reference type directly.
**/
struct nav2_query_node_t {
	//! Stable canonical node key mirrored into nav2-owned storage.
	nav2_node_key_t key = {};
	//! Stable world-space position mirrored into nav2-owned storage.
	Vector3 worldPosition = { 0., 0., 0. };
};

/**
*	@brief	Nav2-owned node localization result resolved through the public query seam.
*	@note	This bundles the canonical node, tile-localization metadata, and validity state so callers can consume one nav2-native result instead of reconstructing node semantics from staged helpers.
**/
struct nav2_query_node_localization_t {
	//! Canonical nav2 node resolved for the query.
	nav2_query_node_t node = {};
	//! Tile-localization metadata associated with the canonical node when available.
	nav2_query_tile_location_t tile_location = {};

	/**
	*	@brief	Return whether this localization contains any resolved canonical node identity.
	*	@return	True when the node key exposes at least one non-negative canonical index.
	**/
	const bool HasNode() const {
		return node.key.leaf_index >= 0
			|| node.key.tile_index >= 0
			|| node.key.cell_index >= 0
			|| node.key.layer_index >= 0;
	}
};

/**
*	@brief	Nav2-owned topology membership result resolved through the public query seam.
*	@note	This publishes stable topology identifiers directly so corridor and gameplay code do not need to reconstruct topology from staged node and tile lookup mechanics.
**/
struct nav2_query_topology_t {
	//! BSP leaf id when known.
	int32_t leaf_index = NAV_LEAF_INDEX_NONE;
	//! BSP cluster id when known.
	int32_t cluster_id = -1;
	//! BSP area id when known.
	int32_t area_id = -1;
	//! Coarse hierarchy region id when known.
	int32_t region_id = NAV_REGION_ID_NONE;
	//! Coarse hierarchy portal id when known.
	int32_t portal_id = NAV_PORTAL_ID_NONE;
	//! Tile-localization metadata associated with this topology when available.
	nav2_query_tile_location_t tile_location = {};

	/**
	*	@brief	Return whether this topology contains any resolved stable identifier.
	*	@return	True when any topology id or tile localization field is valid.
	**/
	const bool IsValid() const {
		return leaf_index >= 0
			|| cluster_id >= 0
			|| area_id >= 0
			|| region_id != NAV_REGION_ID_NONE
			|| portal_id != NAV_PORTAL_ID_NONE
			|| tile_location.HasTileId();
	}
};

/**
*	@brief	Nav2-owned read-only layer view mirrored from the staged runtime layer storage.
*	@note	This copies only stable scalar metadata so nav2 callers can inspect traversal semantics without
*			exposing the legacy layer type directly through nav2 headers.
**/
struct nav2_query_layer_view_t {
	//! Quantized Z position mirrored from the staged runtime layer.
	int16_t z_quantized = 0;
	//! Legacy compatibility content flags mirrored into nav2-owned storage.
	uint32_t flags = 0;
	//! Traversal-oriented node feature bits mirrored into nav2-owned storage.
	uint32_t traversal_feature_bits = NAV_TRAVERSAL_FEATURE_NONE;
	//! Persisted per-edge traversal metadata mirrored into nav2-owned storage.
	uint32_t edge_feature_bits[ NAV_LAYER_EDGE_DIR_COUNT ] = {};
	//! Bitmask of edge slots containing explicit metadata.
	uint8_t edge_valid_mask = 0;
	//! Ladder endpoint flags mirrored into nav2-owned storage.
	uint8_t ladder_endpoint_flags = NAV_LADDER_ENDPOINT_NONE;
	//! Quantized ladder startpoint for the ladder span when present.
	int16_t ladder_start_z_quantized = 0;
	//! Quantized ladder endpoint for the ladder span when present.
	int16_t ladder_end_z_quantized = 0;
	//! Clearance value mirrored from the staged runtime layer.
	uint32_t clearance = 0;
};

/**
*	@brief	Nav2-owned read-only cell view mirrored from the staged runtime cell storage.
*	@note	This wraps copied layer metadata so nav2 headers can expose cell semantics without leaking the legacy cell type.
**/
struct nav2_query_cell_view_t {
	//! Number of layers mirrored into `layers`.
	int32_t num_layers = 0;
	//! Copied layer metadata in canonical order.
	std::vector<nav2_query_layer_view_t> layers = {};
};

/**
*	@brief	Nav2-owned read-only tile view mirrored from the staged runtime tile storage.
*	@note	This keeps tile summary metadata and copied cell views available to nav2 callers without exposing the legacy tile type.
**/
struct nav2_query_tile_view_t {
	//! Tile X coordinate in the world tile grid.
	int32_t tile_x = 0;
	//! Tile Y coordinate in the world tile grid.
	int32_t tile_y = 0;
	//! Coarse hierarchy region id when known.
	int32_t region_id = NAV_REGION_ID_NONE;
	//! Coarse traversal summary bits mirrored into nav2-owned storage.
	uint32_t traversal_summary_bits = NAV_TILE_SUMMARY_NONE;
	//! Coarse edge summary bits mirrored into nav2-owned storage.
	uint32_t edge_summary_bits = NAV_TILE_SUMMARY_NONE;
	//! Copied tile-local cell metadata in canonical order when requested.
	std::vector<nav2_query_cell_view_t> cells = {};
};

/**
*	@brief	Nav2-owned read-only inline-model runtime view mirrored from the staged runtime storage.
*	@note	This keeps mover/runtime metadata pointer-free so nav2 headers stop exposing the legacy runtime type directly.
**/
struct nav2_query_inline_model_runtime_view_t {
	//! Inline-model index mirrored from the staged runtime entry.
	int32_t model_index = -1;
	//! Owning entity number when known.
	int32_t owner_entnum = -1;
	//! Current world-space origin mirrored from the staged runtime entry.
	Vector3 origin = {};
	//! Current world-space angles mirrored from the staged runtime entry.
	Vector3 angles = {};
	//! True when the staged runtime entry is marked dirty.
	bool dirty = false;
};

/**
*	@brief	Build a nav2-owned query handle wrapper from a staged legacy request identifier.
*	@param	legacyHandle	Legacy request handle produced by the staged implementation.
*	@return	Nav2-owned request-handle wrapper.
**/
inline nav2_query_handle_t SVG_Nav2_QueryMakeHandle( const nav_request_handle_t legacyHandle ) {
	return nav2_query_handle_t{ .value = legacyHandle };
}

/**
*	@brief	Return the staged legacy request identifier wrapped by a nav2 query handle.
*	@param	handle	Nav2-owned request-handle wrapper.
*	@return	Legacy request identifier used by the staged implementation.
**/
inline nav_request_handle_t SVG_Nav2_QueryGetLegacyHandle( const nav2_query_handle_t handle ) {
	return handle.value;
}

/**
*	@brief	Build a nav2-owned mesh wrapper from a staged `main mesh` pointer.
*	@param	legacyMesh	Legacy mesh pointer published by the staged runtime.
*	@return	Nav2-owned mesh wrapper.
**/
inline nav2_query_mesh_t SVG_Nav2_QueryMakeMesh( const nav2_mesh_t *legacyMesh ) {
	return nav2_query_mesh_t { .main_mesh = legacyMesh };
}

/**
*	@brief	Stable compatibility-layer tile reference used when legacy coarse routing is mirrored into nav2.
**/
struct nav2_query_tile_ref_t {
	//! Tile X coordinate in the world tile grid.
	int32_t tile_x = 0;
	//! Tile Y coordinate in the world tile grid.
	int32_t tile_y = 0;
};

/**
*	@brief	Stable compatibility-layer coarse corridor result mirrored from the current legacy solver.
*	@note	This keeps direct `oldnav` coarse-corridor types out of nav2 public APIs while the migration shell remains active.
**/
struct nav2_query_coarse_corridor_t {
	/**
	*	@brief	Stable coarse-source classification mirrored from the compatibility layer.
	**/
	enum class source_t : uint8_t {
		None = 0,
		Hierarchy,
		ClusterFallback
	};

	//! Coarse source that produced this corridor.
	source_t source = source_t::None;
	//! Ordered compact region path when hierarchy routing produced this corridor.
	std::vector<int32_t> region_path = {};
	//! Ordered compact portal path when hierarchy routing produced this corridor.
	std::vector<int32_t> portal_path = {};
	//! Ordered exact tile route mirrored into lightweight tile keys.
	std::vector<nav2_query_tile_ref_t> exact_tile_route = {};
	//! Number of local portal overlay rejections encountered while building the corridor.
	int32_t overlay_block_count = 0;
	//! Number of local portal overlay penalties encountered while building the corridor.
	int32_t overlay_penalty_count = 0;

	/**
	*	@brief	Return true when the coarse corridor contains exact tile guidance.
	*	@return	True when `exact_tile_route` is not empty.
	**/
	const bool HasExactTileRoute() const {
		return !exact_tile_route.empty();
	}
};

// <Q2RTXP>: WID: TODO: Remove this when nav2 owns the full path-process implementation and can publish its own native path state type.
struct nav2_path_process_t {};

/**
*	@brief	Retrieve the currently published navigation mesh used by the compatibility layer.
*	@return	Pointer to the active legacy nav mesh, or `nullptr` when no mesh is loaded.
*	@note	This is a staging seam only; later nav2 work can redirect this helper without touching callers.
**/
const nav2_query_mesh_t *SVG_Nav2_GetQueryMesh( void );

/**
*	@brief	Retrieve the currently published mesh metadata snapshot through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@return	Nav2-owned mesh metadata snapshot containing the currently published mesh-level scalars.
**/
nav2_query_mesh_meta_t SVG_Nav2_QueryGetMeshMeta( const nav2_query_mesh_t *mesh );

/**
*	@brief	Resolve the tile-cluster key for a world-space position through the current query seam.
*	@param	mesh	Active query mesh.
*	@param	position	World-space position to localize.
*	@return	Legacy canonical tile-cluster key for the requested position.
*	@note	This keeps direct oldnav helper ownership confined to the compatibility seam while nav2 callers adopt nav2-owned wrappers.
**/
nav2_tile_cluster_key_t SVG_Nav2_QueryGetTileKeyForPosition( const nav2_query_mesh_t *mesh, const Vector3 &position );

/**
*	@brief	Resolve canonical tile cells through the current query seam.
*	@param	mesh	Active query mesh.
*	@param	tile	Canonical world tile view.
*	@return	Tile cell view and count pair.
*	@note	This localizes the remaining oldnav storage helper behind the compatibility seam.
**/
std::vector<nav2_query_cell_view_t> SVG_Nav2_QueryGetTileCells( const nav2_query_mesh_t *mesh, const nav2_query_tile_view_t &tile );

/**
*	@brief	Select the best canonical layer index for a localized cell through the current query seam.
*	@param	mesh	Active query mesh.
*	@param	cell	Canonical XY cell view.
*	@param	desiredZ	Desired world-space Z height.
*	@param	outLayerIndex	[out] Selected layer index.
*	@return	True when a suitable layer was selected.
*	@note	This keeps the legacy layer-selection helper behind the nav2 seam until nav2 owns the full query stack.
**/
const bool SVG_Nav2_QuerySelectLayerIndex( const nav2_query_mesh_t *mesh, const nav2_query_cell_view_t &cell, const double desiredZ, int32_t *outLayerIndex );

/**
*	@brief	Build an agent profile using the current legacy navigation cvar configuration.
*	@return	The currently configured legacy navigation agent profile snapshot mirrored into nav2-owned storage.
*	@note	The compatibility layer preserves the legacy public behavior until nav2 owns the query path.
**/
nav2_query_agent_profile_t SVG_Nav2_BuildAgentProfileFromCvars( void );

/**
*	@brief	Resolve tile-local metadata for a world-space nav-center point through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@param	centerOrigin	Projected nav-center point to localize.
*	@param	outLocation	[out] Nav2-owned tile localization metadata when lookup succeeds.
*	@return	True when the point localized to a valid tile and cell.
**/
const bool SVG_Nav2_QueryTryResolveTileLocation( const nav2_query_mesh_t *mesh, const Vector3 &centerOrigin, nav2_query_tile_location_t *outLocation );

/**
*	@brief	Resolve one stable world-tile id from world-tile coordinates through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@param	tileX	World tile X coordinate.
*	@param	tileY	World tile Y coordinate.
*	@param	outTileId	[out] Stable world-tile id when lookup succeeds.
*	@return	True when the coordinates resolve to a canonical world tile.
**/
const bool SVG_Nav2_QueryTryResolveTileIdByCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY, int32_t *outTileId );

/**
*	@brief	Resolve tile-local metadata for one canonical node through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@param	node	Canonical node to localize.
*	@param	outLocation	[out] Nav2-owned tile localization metadata when lookup succeeds.
*	@return	True when the node resolved to a canonical tile and cell.
**/
const bool SVG_Nav2_QueryTryResolveNodeTileLocation( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node, nav2_query_tile_location_t *outLocation );

/**
*	@brief	Resolve one canonical node plus its tile-localization metadata from a feet-origin query point through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@param	feetOrigin	Feet-origin query point.
*	@param	agentMins	Agent bounds minimums.
*	@param	agentMaxs	Agent bounds maximums.
*	@param	outLocalization	[out] Nav2-owned node-localization result when lookup succeeds.
*	@return	True when the query point resolved to a canonical node and tile localization.
**/
const bool SVG_Nav2_QueryTryResolveNodeLocalizationForFeetOrigin( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin,
	const Vector3 &agentMins, const Vector3 &agentMaxs, nav2_query_node_localization_t *outLocalization );

/**
*	@brief	Resolve nav2-owned topology membership metadata for one canonical node through the public query seam.
*	@param	mesh	Active query mesh.
*	@param	node	Canonical node being inspected.
*	@param	outTopology	[out] Nav2-owned topology result when lookup succeeds.
*	@return	True when at least one stable topology field was resolved.
**/
const bool SVG_Nav2_QueryTryResolveNodeTopology( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node, nav2_query_topology_t *outTopology );

/**
*	@brief	Resolve inline-model runtime membership from world-tile coordinates through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@param	tileX	World tile X coordinate to inspect.
*	@param	tileY	World tile Y coordinate to inspect.
*	@param	outMembership	[out] Nav2-owned inline-model membership metadata when a matching mover tile is found.
*	@return	True when inline-model membership was resolved.
**/
const bool SVG_Nav2_QueryTryResolveInlineModelMembershipByTileCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY,
	nav2_query_inline_model_membership_t *outMembership );

/**
*	@brief	Resolve inline-model runtime membership from one canonical node through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@param	node	Canonical node whose owning tile should be tested.
*	@param	outMembership	[out] Nav2-owned inline-model membership metadata when a matching mover tile is found.
*	@return	True when inline-model membership was resolved.
**/
const bool SVG_Nav2_QueryTryResolveInlineModelMembershipForNode( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node,
	nav2_query_inline_model_membership_t *outMembership );

/**
*	@brief	Test whether one BSP leaf contains a canonical tile membership through the public nav2 query seam.
*	@param	mesh	Active query mesh.
*	@param	leafIndex	BSP leaf index to inspect.
*	@param	tileId	Stable world-tile id when known.
*	@param	tileX	World tile X coordinate used as a fallback match key.
*	@param	tileY	World tile Y coordinate used as a fallback match key.
*	@return	True when the specified leaf contains the requested canonical tile membership.
**/
const bool SVG_Nav2_QueryLeafContainsTile( const nav2_query_mesh_t *mesh, const int32_t leafIndex, const int32_t tileId,
	const int32_t tileX, const int32_t tileY );

/**
*	@brief	Convert a feet-origin navigation point into nav-center space.
*	@param	mesh		Active query mesh.
*	@param	feetOrigin	Feet-origin point to convert.
*	@param	agentMins	Agent bounds minimums.
*	@param	agentMaxs	Agent bounds maximums.
*	@return	Converted nav-center-space point.
**/
const Vector3 SVG_Nav2_QueryConvertFeetToCenter( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin, const Vector3 *agentMins, const Vector3 *agentMaxs );

/**
*	@brief	Convert a feet-origin navigation point into nav-center space through the nav2 query seam.
*	@param	mesh		Active query mesh.
*	@param	feetOrigin	Feet-origin point to convert.
*	@param	agentMins	Agent bounds minimums.
*	@param	agentMaxs	Agent bounds maximums.
*	@return	Converted nav-center-space point.
*	@note	This preserves the older helper name for current nav2 and gameplay consumers while the seam remains staged.
**/
const Vector3 SVG_Nav2_ConvertFeetToCenter( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin, const Vector3 *agentMins, const Vector3 *agentMaxs );

/**
*	@brief	Resolve one canonical node from a feet-origin query point through the compatibility layer.
*	@param	mesh		Active query mesh.
*	@param	feetOrigin	Feet-origin query point.
*	@param	agentMins	Agent bounds minimums.
*	@param	agentMaxs	Agent bounds maximums.
*	@param	outNode		[out] Canonical node resolved for the query point.
*	@return	True when the query point resolved to a canonical node.
**/
const bool SVG_Nav2_QueryTryResolveNodeForFeetOrigin( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin,
	const Vector3 &agentMins, const Vector3 &agentMaxs, nav2_query_node_t *outNode );

/**
*	@brief	Resolve a canonical tile view for one compatibility-layer node.
*	@param	mesh	Active query mesh.
*	@param	node	Canonical node being inspected.
*	@return	Read-only canonical world-tile view, or `nullptr` on failure.
**/
nav2_query_tile_view_t SVG_Nav2_QueryGetNodeTileView( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node );

/**
*	@brief	Resolve a canonical tile view by world-tile coordinates through the current query seam.
*	@param	mesh	Active query mesh.
*	@param	tileX	Tile X coordinate in the world tile grid.
*	@param	tileY	Tile Y coordinate in the world tile grid.
*	@return	Nav2-owned read-only tile view, or an empty view when the tile cannot be resolved.
**/
nav2_query_tile_view_t SVG_Nav2_QueryGetTileViewByCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY );

/**
*	@brief	Resolve a canonical layer view for one compatibility-layer node.
*	@param	mesh	Active query mesh.
*	@param	node	Canonical node being inspected.
*	@return	Read-only canonical layer view, or `nullptr` on failure.
**/
nav2_query_layer_view_t SVG_Nav2_QueryGetNodeLayerView( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node );

/**
*	@brief	Resolve the published inline-model runtime snapshot through the compatibility layer.
*	@param	mesh	Active query mesh.
*	@return	Read-only runtime entry array and count.
**/
std::vector<nav2_query_inline_model_runtime_view_t> SVG_Nav2_QueryGetInlineModelRuntime( const nav2_query_mesh_t *mesh );

/**
*	@brief	Build the current staged coarse corridor through the compatibility layer.
*	@param	mesh		Active query mesh.
*	@param	startNode	Canonical start node for coarse routing.
*	@param	goalNode	Canonical goal node for coarse routing.
*	@param	policy		Optional traversal policy used to gate legacy coarse transitions.
*	@param	outCorridor	[out] Compatibility-layer coarse corridor mirrored into nav2-friendly storage.
*	@param	outCoarseExpansions	[out] Coarse expansion count reported by the legacy builder.
*	@return	True when a coarse corridor was built and mirrored successfully.
**/
const bool SVG_Nav2_QueryBuildCoarseCorridor( const nav2_query_mesh_t *mesh, const nav2_query_node_t &startNode,
	const nav2_query_node_t &goalNode, const nav2_query_policy_t *policy, nav2_query_coarse_corridor_t *outCorridor,
	int32_t *outCoarseExpansions );

/**
*	@brief	Project a feet-origin target onto a walkable navigation layer.
*	@param	mesh			Active query mesh.
*	@param	goalOrigin	Feet-origin target to project.
*	@param	agentMins	Agent bounds minimums.
*	@param	agentMaxs	Agent bounds maximums.
*	@param	outGoalOrigin	[out] Projected walkable feet-origin target.
*	@return	True when projection succeeded.
**/
const bool SVG_Nav2_TryProjectFeetOriginToWalkableZ( const nav2_mesh_t *mesh, const Vector3 &goalOrigin, const Vector3 &agentMins, const Vector3 &agentMaxs, Vector3 *outGoalOrigin );

/**
*	@brief	Resolve the preferred feet-origin goal endpoint using the staged nav2 candidate-selection helper.
*	@param	mesh		Active query mesh.
*	@param	startOrigin	Feet-origin start point used for ranking hints.
*	@param	goalOrigin	Feet-origin raw goal point requested by gameplay code.
*	@param	agentMins	Agent bounds minimums.
*	@param	agentMaxs	Agent bounds maximums.
*	@param	outGoalOrigin	[out] Selected feet-origin goal endpoint.
*	@param	outCandidate	[out] Optional selected candidate metadata.
*	@param	outCandidates	[out] Optional accepted/rejected candidate list for debug or diagnostics.
*	@return	True when a ranked candidate endpoint was resolved.
*	@note	This compatibility-layer helper now stays strict on the public path and only reports a result when
*		the staged candidate builder produced one. Any temporary fallback behavior must remain isolated to
*		internal seams, not the public nav2 surface.
**/
const bool SVG_Nav2_ResolveBestGoalOrigin( const nav2_query_mesh_t *mesh, const Vector3 &startOrigin, const Vector3 &goalOrigin,
	const Vector3 &agentMins, const Vector3 &agentMaxs, Vector3 *outGoalOrigin,
	nav2_goal_candidate_t *outCandidate = nullptr, nav2_goal_candidate_list_t *outCandidates = nullptr );

/**
*	@brief	Test whether an asynchronous navigation request is still pending.
*	@param	process	Path process state to inspect.
*	@return	True when the process has an outstanding async request.
**/
const bool SVG_Nav2_IsRequestPending( const nav2_query_process_t *process );

/**
*	@brief	Test whether asynchronous navigation is globally enabled.
*	@return	True when legacy async navigation processing is enabled.
**/
const bool SVG_Nav2_IsAsyncNavEnabled( void );

/**
*	@brief	Submit an asynchronous path request through the compatibility layer.
*	@param	process		Path process that will own the request state.
*	@param	startOrigin	Feet-origin start point.
*	@param	goalOrigin	Feet-origin goal point.
*	@param	policy		Path policy snapshot.
*	@param	agentMins	Agent bounds minimums.
*	@param	agentMaxs	Agent bounds maximums.
*	@param	force		When true, force a refresh.
*	@param	startIgnoreThreshold	Start-drift tolerance for request reuse.
*	@return	Queued request handle, or a non-positive value on failure.
**/
nav2_query_handle_t SVG_Nav2_RequestPathAsync( nav2_query_process_t *process, const Vector3 &startOrigin, const Vector3 &goalOrigin, const nav2_query_policy_t &policy, const Vector3 &agentMins, const Vector3 &agentMaxs, const bool force, const double startIgnoreThreshold );

/**
*	@brief	Cancel a queued asynchronous path request.
*	@param	handle	Request handle to cancel.
*	@note	Handles less than or equal to zero are ignored by callers before reaching this helper.
**/
void SVG_Nav2_CancelRequest( const nav2_query_handle_t handle );
