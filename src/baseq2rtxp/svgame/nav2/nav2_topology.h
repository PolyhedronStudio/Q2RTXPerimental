/********************************************************************
*
*
*	ServerGame: Nav2 Topology Classification
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_span_grid.h"


/**
*
*
*	Nav2 Topology Enumerations:
*
*
**/
/**
*	@brief	Stable topology feature bits owned by the topology-classification stage.
*	@note	These bits normalize per-cell, per-layer, and per-endpoint navigation semantics into
*			a scheduler-friendly form so later stages do not need to infer intent repeatedly.
**/
enum nav2_topology_feature_bits_t : uint32_t {
	//! No explicit topology features assigned.
	NAV2_TOPOLOGY_FEATURE_NONE = 0,
	//! Classified location belongs to a walkable floor-style band.
	NAV2_TOPOLOGY_FEATURE_FLOOR = ( 1u << 0 ),
	//! Classified location participates in a stair band.
	NAV2_TOPOLOGY_FEATURE_STAIR = ( 1u << 1 ),
	//! Classified location participates in ladder traversal.
	NAV2_TOPOLOGY_FEATURE_LADDER = ( 1u << 2 ),
	//! Classified location is a ladder bottom/startpoint.
	NAV2_TOPOLOGY_FEATURE_LADDER_STARTPOINT = ( 1u << 3 ),
	//! Classified location is a ladder top/endpoint.
	NAV2_TOPOLOGY_FEATURE_LADDER_ENDPOINT = ( 1u << 4 ),
	//! Classified location sits on a portal/opening barrier.
	NAV2_TOPOLOGY_FEATURE_PORTAL_BARRIER = ( 1u << 5 ),
	//! Classified location references mover-aware traversal semantics.
	NAV2_TOPOLOGY_FEATURE_MOVER = ( 1u << 6 ),
	//! Classified location carries non-walk-off restrictions on at least one side.
	NAV2_TOPOLOGY_FEATURE_NON_WALK_OFF = ( 1u << 7 ),
	//! Classified location carries dynamic occupancy relevance.
	NAV2_TOPOLOGY_FEATURE_DYNAMIC_OCCUPANCY = ( 1u << 8 )
};

/**
*	@brief	Stable topology classification status for one publication.
**/
enum class nav2_topology_status_t : uint8_t {
	None = 0,
	Pending,
	Ready,
	Partial,
	Failed,
	Count
};


/**
*
*
*	Nav2 Topology Data Structures:
*
*
**/
/**
*	@brief	Stable topology endpoint semantics for one classified tile/cell/layer location.
*	@note	This record is pointer-free and publication-safe so scheduler stages can cache and
*			reuse it across candidate generation, hierarchy build, and corridor extraction.
**/
struct nav2_topology_semantics_t {
	//! Stable topology feature bits owned by topology classification.
	uint32_t feature_bits = NAV2_TOPOLOGY_FEATURE_NONE;
	//! Aggregated traversal features mirrored from source tile/cell/layer analysis.
	uint32_t traversal_feature_bits = NAV_TRAVERSAL_FEATURE_NONE;
	//! Aggregated edge features mirrored from source tile/cell analysis.
	uint32_t edge_feature_bits = NAV_EDGE_FEATURE_NONE;
	//! Aggregated tile summary bits mirrored from source static nav data.
	uint32_t tile_summary_bits = NAV_TILE_SUMMARY_NONE;
	//! Ladder endpoint flags when ladder semantics exist at this location.
	uint8_t ladder_endpoint_flags = NAV_LADDER_ENDPOINT_NONE;
	//! True when this location should be treated as a coarse portal barrier boundary.
	bool portal_barrier = false;
	//! True when this location should prefer ladder traversal over longer stair detours.
	bool ladder_preferred = false;
	//! True when this location forbids walk-off expansion unless policy explicitly overrides it.
	bool non_walk_off = false;
};

/**
*	@brief	Stable classified location published by the topology stage.
*	@note	This is the primary scheduler-owned topology product for later query stages.
**/
struct nav2_topology_location_t {
	//! Stable topology reference carrying tile/cell/layer/BSP ids.
	nav2_corridor_topology_ref_t topology = {};
	//! Committed local Z band for the classified location.
	nav2_corridor_z_band_t allowed_z_band = {};
	//! Preferred world-space center origin for this location when known.
	Vector3 center_origin = {};
	//! Preferred world-space feet origin for this location when known.
	Vector3 feet_origin = {};
	//! Stable semantic feature bundle for the location.
	nav2_topology_semantics_t semantics = {};
	//! Stable span id when the classification resolves to a concrete sparse span.
	int32_t span_id = -1;
	//! Stable region-layer id when the location already maps to a known region layer.
	int32_t region_layer_id = NAV_REGION_ID_NONE;
	//! Stable connector id when the location is associated with a connector endpoint.
	int32_t connector_id = -1;
	//! True when this location contains enough metadata for downstream stages to use directly.
	bool valid = false;
};

/**
*	@brief	Snapshot-scoped topology publication owned by the topology-classification stage.
*	@note	The initial contract stays lightweight and pointer-free so scheduler runtime can cache
*			and invalidate it deterministically while richer lookup tables are added later.
**/
struct nav2_topology_artifact_t {
	//! Status of the currently published topology artifact.
	nav2_topology_status_t status = nav2_topology_status_t::None;
	//! Static-nav version this publication was derived from.
	uint32_t static_nav_version = 0;
	//! Number of world tiles represented by the publication when known.
	int32_t world_tile_count = 0;
	//! Number of sparse columns represented by the publication when known.
	int32_t span_column_count = 0;
	//! Number of spans represented by the publication when known.
	int32_t span_count = 0;
	//! True when tile sizing metadata was available while building the artifact.
	bool has_tile_sizing = false;
	//! True when the publication contains enough metadata for downstream topology-dependent work.
	bool ready = false;
};

/**
*	@brief	Bounded diagnostics for one topology-classification publication.
**/
struct nav2_topology_summary_t {
	//! Number of classified floor-style locations.
	int32_t floor_count = 0;
	//! Number of classified stair-band locations.
	int32_t stair_count = 0;
	//! Number of classified ladder-aware locations.
	int32_t ladder_count = 0;
	//! Number of classified portal-barrier locations.
	int32_t portal_barrier_count = 0;
	//! Number of classified mover-aware locations.
	int32_t mover_count = 0;
	//! Number of classified non-walk-off constrained locations.
	int32_t non_walk_off_count = 0;
	//! Number of classified locations carrying dynamic occupancy relevance.
	int32_t dynamic_occupancy_count = 0;
};



/**
*	@brief	Keep the nav2 topology module represented in the build.
*	@note	This remains as a lightweight build anchor while the fuller topology stage contract is introduced.
**/
void SVG_Nav2_Topology_ModuleAnchor( void );

/**
*\t@brief\tReturn whether the nav2 topology layer currently has enough published runtime data to be meaningful.
*\t@return\tTrue when a query mesh is published and exposes usable tile sizing metadata.
*\t@note\tThis is intentionally lightweight so callers can gate topology-dependent work without forcing any rebuild.
**/
const bool SVG_Nav2_Topology_IsReady( void );

/**
*	@brief	Build a lightweight topology artifact from the currently published nav2 runtime state.
*	@param	out_artifact	[out] Topology artifact receiving snapshot-scoped publication metadata.
*	@param	out_summary	[out] Optional bounded semantic summary for diagnostics and milestone validation.
*	@return	True when the topology artifact was built successfully.
**/
const bool SVG_Nav2_Topology_BuildArtifact( nav2_topology_artifact_t *out_artifact, nav2_topology_summary_t *out_summary = nullptr );

/**
*	@brief	Validate that a topology artifact contains enough metadata for downstream stage ownership.
*	@param	artifact	Topology artifact to inspect.
*	@return	True when the artifact is in a ready/usable state.
**/
const bool SVG_Nav2_Topology_ValidateArtifact( const nav2_topology_artifact_t &artifact );
