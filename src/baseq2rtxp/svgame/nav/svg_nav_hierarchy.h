/****************************************************************************************************************************************
*
*
*    SVGame: Navigation Hierarchy Helpers (portal overlays, hierarchy reset/build APIs)
*
*
**********************************************************************************************************************************/
#pragma once

#include <cstdint>

struct nav_mesh_t;
struct nav_portal_overlay_t;

/**
*    @brief	Clear all local portal overlay entries while leaving the static hierarchy untouched.
*    @param	mesh	Navigation mesh containing the hierarchy portal overlay.
*    @note	This is local runtime state only and must not trigger hierarchy regeneration.
**/
void SVG_Nav_Hierarchy_ClearPortalOverlays( nav_mesh_t * mesh );
/**
*    @brief	Update one local portal overlay entry without mutating the static portal graph.
*    @param	mesh	Navigation mesh containing the hierarchy portal overlay.
*    @param	portal_id	Stable compact portal id to update.
*    @param	overlay_flags	Overlay flags such as invalidated, blocked, or dirty.
*    @param	additional_traversal_cost	Optional local penalty added when the portal remains usable.
*    @return	True when the overlay entry was updated.
**/
bool SVG_Nav_Hierarchy_SetPortalOverlay( nav_mesh_t * mesh, int32_t portal_id, uint32_t overlay_flags, double additional_traversal_cost );
/**
*    @brief	Query one local portal overlay entry by compact portal id.
*    @param	mesh	Navigation mesh containing the hierarchy portal overlay.
*    @param	portal_id	Stable compact portal id to query.
*    @param	out_overlay	[out] Current local portal overlay entry.
*    @return	True when the portal id is valid and an overlay snapshot was returned.
**/
bool SVG_Nav_Hierarchy_TryGetPortalOverlay( const nav_mesh_t * mesh, int32_t portal_id, nav_portal_overlay_t * out_overlay );

/**
* 	@brief	Reset hierarchy membership on existing tiles and leaves.
* 	@param	mesh	Navigation mesh whose per-tile and per-leaf region membership should be cleared.
*	@note	This keeps the fine navmesh intact while removing any stale hierarchy ownership, by
* 			freesing leaf-owned region-id arrays and restores all memberships to sentinel values.
**/
void SVG_Nav_Hierarchy_ResetMembership( nav_mesh_t * mesh );
/**
*    @brief	Clear all internal hierarchy scaffolding owned by a navigation mesh.
*    @param	mesh	Navigation mesh whose hierarchy containers should be cleared.
*    @note	This preserves the fine navmesh while invalidating region/portal state and memberships.
**/
void SVG_Nav_Hierarchy_Clear( nav_mesh_t * mesh );
/**
* 	@brief	Build static tile adjacency, coarse regions, and the derived initial portal graph.
* 	@param	mesh	Navigation mesh whose canonical world tiles should be partitioned.
* 	@note	This derives conservative static connectivity from persisted tile edge metadata and merges traversable cross-region seams into initial portal records.
**/
void SVG_Nav_Hierarchy_BuildStaticRegions( nav_mesh_t * mesh );

/**
* 	@brief	Map a persisted edge slot onto its XY direction delta.
* 	@param	edge_dir_index	Persisted edge slot index (0..7).
* 	@param	out_dx		[out] Neighbor X delta.
* 	@param	out_dy		[out] Neighbor Y delta.
* 	@return	True when the slot index is valid.
**/
bool Nav_Hierarchy_EdgeDirDelta( const int32_t edge_dir_index, int32_t *out_dx, int32_t *out_dy );
/**
* 	@brief	Determine whether a persisted edge is suitable for static region connectivity.
* 	@param	edge_bits	Per-edge feature bits finalized during generation.
* 	@return	True when the edge represents conservative bidirectional static connectivity.
* 	@note	Walk-off-only, hard-wall, and jump-obstructed edges stay out of the static region graph.
**/
bool Nav_Hierarchy_EdgeAllowsStaticRegionPass( const uint32_t edge_bits );

/**
* 	@brief	Resolve a canonical world-tile id by tile-grid coordinates.
* 	@param	mesh	Navigation mesh owning the canonical world-tile lookup.
* 	@param	tile_x	Tile X coordinate.
* 	@param	tile_y	Tile Y coordinate.
* 	@return	Canonical world-tile id, or -1 when absent.
**/
int32_t Nav_Hierarchy_FindWorldTileId( const nav_mesh_t *mesh, const int32_t tile_x, const int32_t tile_y );

/**
* 	@brief	Build hierarchy compatibility bits from a tile's persisted summary metadata.
* 	@param	tile	Tile whose coarse summaries should be translated.
* 	@return	Compatibility bits suitable for future region or portal policy filtering.
**/
uint32_t Nav_Hierarchy_BuildTileCompatibilityBits( const nav_tile_t &tile );
/**
* 	@brief	Build initial region summary flags from a tile's persisted coarse metadata.
* 	@param	tile	Tile whose coarse summaries should be translated.
* 	@return	Region flags suitable for aggregated Phase 3 region summaries.
**/
uint32_t Nav_Hierarchy_BuildTileRegionFlags( const nav_tile_t &tile );
/**
* 	@brief	Sort canonical tile ids by tile-grid coordinates for deterministic hierarchy output.
* 	@param	mesh	Navigation mesh owning the canonical world tiles.
* 	@param	tile_ids	Tile-id array to sort in place.
* 	@note	Tile id is used as the final tie-breaker to keep ordering stable.
**/
void Nav_Hierarchy_SortTileIdsDeterministic( const nav_mesh_t *mesh, std::vector<int32_t> &tile_ids );
/**
* 	@brief	Derive canonical world-tile adjacency from persisted fine edge metadata.
* 	@param	mesh	Navigation mesh whose canonical world tiles should be scanned.
* 	@note	Only cross-tile, conservative static edges are promoted into the adjacency graph.
**/
void Nav_Hierarchy_BuildTileAdjacency( nav_mesh_t *mesh );

/**
* 	@brief	Build a compact ordered key for an unordered region pair.
* 	@param	region_a	First region id.
* 	@param	region_b	Second region id.
* 	@return	Packed 64-bit region-pair key with the lower id first.
**/
uint64_t Nav_Hierarchy_MakeRegionPairKey( const int32_t region_a, const int32_t region_b );
/**
* 	@brief	Validate that every region remains internally connected over the stored tile adjacency graph.
* 	@param	mesh	Navigation mesh containing the freshly built hierarchy.
* 	@return	True when every region validates successfully.
**/
bool Nav_Hierarchy_ValidateRegions( const nav_mesh_t *mesh );
/**
* 	@brief	Emit a concise summary of the current static region and portal graph.
* 	@param	mesh	Navigation mesh containing the freshly built hierarchy.
* 	@note	Logs only a capped subset of suspicious regions to avoid log spam.
**/
void Nav_Hierarchy_LogRegionSummary( const nav_mesh_t *mesh );


/**
*
*
*
*	Hierarchy Portals:
*
*
*
**/
/**
* 	@brief	Build merged portal records from traversable cross-region tile boundaries.
* 	@param	mesh	Navigation mesh containing finalized tile adjacency and region ids.
* 	@param	sorted_tile_ids	Deterministically ordered canonical world-tile ids.
**/
void Nav_Hierarchy_BuildPortals( nav_mesh_t *mesh, const std::vector<int32_t> &sorted_tile_ids );
/**
* 	@brief	Validate the merged portal graph produced from region boundaries.
* 	@param	mesh	Navigation mesh containing freshly built portal records.
* 	@return	True when the portal graph references remain internally consistent.
**/
bool Nav_Hierarchy_ValidatePortalGraph( const nav_mesh_t *mesh );
