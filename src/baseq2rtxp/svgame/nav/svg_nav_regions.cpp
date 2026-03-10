/****************************************************************************************************************************************
*
*
*    SVGame: Navigation Region Helpers - Implementation
*
*    Minimal stub implementation to satisfy link dependencies until full region builder is added.
*
**********************************************************************************************************************************/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_regions.h"
#include "svgame/nav/svg_nav_hierarchy.h"


/**
 * 	@brief	Build static regions and the derived initial portal graph from canonical world tiles.
* 	@param	mesh	Navigation mesh whose canonical world tiles should be partitioned.
 * 	@note	The first pass stays conservative and tile-granular, using a deterministic coarse region budget so traversable inter-region seams exist for merged portal extraction.
**/
void SVG_Nav_Hierarchy_BuildStaticRegions( nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a valid mesh before attempting Phase 3 hierarchy construction.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	* 	Start from a clean hierarchy state so load/regenerate paths cannot retain stale region membership.
	**/
	SVG_Nav_Hierarchy_Clear( mesh );
	if ( mesh->world_tiles.empty() ) {
		return;
	}

	/**
	* 	Formalize conservative canonical tile adjacency from the persisted fine edge metadata.
	**/
	Nav_Hierarchy_BuildTileAdjacency( mesh );

	/**
	* 	Seed flood fills in deterministic tile-coordinate order so region ids remain stable across rebuilds.
	**/
	std::vector<int32_t> sorted_tile_ids;
	sorted_tile_ids.reserve( mesh->world_tiles.size() );
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		sorted_tile_ids.push_back( tile_id );
	}
	Nav_Hierarchy_SortTileIdsDeterministic( mesh, sorted_tile_ids );

	mesh->hierarchy.regions.reserve( mesh->world_tiles.size() );
	mesh->hierarchy.tile_refs.reserve( mesh->world_tiles.size() );
	mesh->hierarchy.debug_region_count = 0;
	mesh->hierarchy.debug_boundary_link_count = 0;
	mesh->hierarchy.debug_portal_count = 0;
	mesh->hierarchy.debug_isolated_region_count = 0;
	mesh->hierarchy.debug_oversized_region_count = 0;

	/**
	* 	Flood-fill deterministic coarse regions over the canonical tile adjacency graph.
	* 	A fixed tile budget intentionally leaves cross-region seams so we can extract merged portals.
	**/
	for ( const int32_t seed_tile_id : sorted_tile_ids ) {
		if ( mesh->world_tiles[ seed_tile_id ].region_id != NAV_REGION_ID_NONE ) {
			continue;
		}

		nav_region_t region = {};
		region.id = ( int32_t )mesh->hierarchy.regions.size();
		region.first_tile_ref = ( int32_t )mesh->hierarchy.tile_refs.size();
		region.debug_anchor_tile_id = seed_tile_id;

		std::vector<int32_t> stack;
		stack.push_back( seed_tile_id );
		mesh->world_tiles[ seed_tile_id ].region_id = region.id;

		while ( !stack.empty() ) {
			const int32_t tile_id = stack.back();
			stack.pop_back();

			nav_tile_t &tile = mesh->world_tiles[ tile_id ];
			mesh->hierarchy.tile_refs.push_back( nav_region_tile_ref_t{ .tile_id = tile_id } );
			region.num_tile_refs++;
			region.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( tile );
			region.flags |= Nav_Hierarchy_BuildTileRegionFlags( tile );

			const nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
			for ( int32_t neighbor_index = 0; neighbor_index < adjacency.num_neighbor_refs; neighbor_index++ ) {
				const int32_t neighbor_tile_id = mesh->hierarchy.tile_neighbor_refs[ adjacency.first_neighbor_ref + neighbor_index ];
				if ( neighbor_tile_id < 0 || neighbor_tile_id >= ( int32_t )mesh->world_tiles.size() ) {
					continue;
				}
				if ( mesh->world_tiles[ neighbor_tile_id ].region_id != NAV_REGION_ID_NONE ) {
					continue;
				}
				// Stop region growth once the deterministic coarse budget is saturated.
				if ( ( region.num_tile_refs + ( int32_t )stack.size() ) >= NAV_HIERARCHY_MAX_TILES_PER_REGION ) {
					continue;
				}

				mesh->world_tiles[ neighbor_tile_id ].region_id = region.id;
				stack.push_back( neighbor_tile_id );
			}
		}

	 // Mark and count regions that look suspiciously disconnected or saturated for future review.
		if ( region.num_tile_refs == 1 ) {
			const nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ seed_tile_id ];
			if ( adjacency.num_neighbor_refs == 0 ) {
				region.flags |= NAV_REGION_FLAG_ISOLATED;
				mesh->hierarchy.debug_isolated_region_count++;
			}
		}
		if ( region.num_tile_refs >= NAV_HIERARCHY_OVERSIZED_REGION_TILE_COUNT ) {
			mesh->hierarchy.debug_oversized_region_count++;
		}

		mesh->hierarchy.regions.push_back( region );
	}

	/**
	* 	Sort each region's tile refs deterministically now that flood-fill membership is known.
	**/
	for ( nav_region_t &region : mesh->hierarchy.regions ) {
		std::vector<int32_t> region_tile_ids;
		region_tile_ids.reserve( ( size_t )region.num_tile_refs );
		for ( int32_t tile_ref_index = 0; tile_ref_index < region.num_tile_refs; tile_ref_index++ ) {
			region_tile_ids.push_back( mesh->hierarchy.tile_refs[ region.first_tile_ref + tile_ref_index ].tile_id );
		}
		Nav_Hierarchy_SortTileIdsDeterministic( mesh, region_tile_ids );
		for ( int32_t tile_ref_index = 0; tile_ref_index < region.num_tile_refs; tile_ref_index++ ) {
			mesh->hierarchy.tile_refs[ region.first_tile_ref + tile_ref_index ].tile_id = region_tile_ids[ tile_ref_index ];
		}
		region.debug_anchor_tile_id = region_tile_ids.empty() ? -1 : region_tile_ids.front();
	}

	/**
	* 	Rebuild leaf membership from the existing leaf->tile mapping so regions remain inspectable by BSP leaf.
	**/
	std::vector<std::vector<int32_t>> region_leaf_lists( mesh->hierarchy.regions.size() );
	for ( int32_t leaf_index = 0; leaf_index < mesh->num_leafs; leaf_index++ ) {
		nav_leaf_data_t &leaf = mesh->leaf_data[ leaf_index ];
		auto tile_ids_view = SVG_Nav_Leaf_GetTileIds( &leaf );
		const int32_t *tile_ids = tile_ids_view.first;
		const int32_t tile_count = tile_ids_view.second;
		if ( !tile_ids || tile_count <= 0 ) {
			continue;
		}

		std::vector<int32_t> region_ids;
		region_ids.reserve( ( size_t )tile_count );
		for ( int32_t tile_offset = 0; tile_offset < tile_count; tile_offset++ ) {
			const int32_t tile_id = tile_ids[ tile_offset ];
			if ( tile_id < 0 || tile_id >= ( int32_t )mesh->world_tiles.size() ) {
				continue;
			}

			const int32_t region_id = mesh->world_tiles[ tile_id ].region_id;
			if ( region_id != NAV_REGION_ID_NONE ) {
				region_ids.push_back( region_id );
			}
		}

		std::sort( region_ids.begin(), region_ids.end() );
		region_ids.erase( std::unique( region_ids.begin(), region_ids.end() ), region_ids.end() );
		if ( region_ids.empty() ) {
			continue;
		}

		leaf.num_regions = ( int32_t )region_ids.size();
		leaf.region_ids = ( int32_t * )gi.TagMallocz( sizeof( int32_t ) * ( size_t )leaf.num_regions, TAG_SVGAME_NAVMESH );
		memcpy( leaf.region_ids, region_ids.data(), sizeof( int32_t ) * ( size_t )leaf.num_regions );
		for ( const int32_t region_id : region_ids ) {
			region_leaf_lists[ region_id ].push_back( leaf_index );
		}
	}

	/**
	* 	Flatten per-region leaf memberships into the shared hierarchy leaf-ref store.
	**/
	for ( nav_region_t &region : mesh->hierarchy.regions ) {
		std::vector<int32_t> &leaf_ids = region_leaf_lists[ region.id ];
		std::sort( leaf_ids.begin(), leaf_ids.end() );
		leaf_ids.erase( std::unique( leaf_ids.begin(), leaf_ids.end() ), leaf_ids.end() );
		region.first_leaf_ref = ( int32_t )mesh->hierarchy.leaf_refs.size();
		region.num_leaf_refs = ( int32_t )leaf_ids.size();
		for ( const int32_t leaf_index : leaf_ids ) {
			mesh->hierarchy.leaf_refs.push_back( nav_region_leaf_ref_t{ .leaf_index = leaf_index } );
		}
	}

	/**
	* 	Extract merged portals now that deterministic coarse region seams exist across connected tile space.
	**/
	Nav_Hierarchy_BuildPortals( mesh, sorted_tile_ids );

	/**
 * 	Run validation passes and then log a concise summary for the current hierarchy graph.
	**/
	mesh->hierarchy.debug_region_count = ( int32_t )mesh->hierarchy.regions.size();
	mesh->hierarchy.build_serial++;
	mesh->hierarchy.dirty = false;
	if ( !Nav_Hierarchy_ValidateRegions( mesh ) ) {
		SVG_Nav_Log( "[WARNING][NavHierarchy] Static region validation reported a connectivity mismatch.\n" );
	}
	if ( !Nav_Hierarchy_ValidatePortalGraph( mesh ) ) {
		SVG_Nav_Log( "[WARNING][NavHierarchy] Portal graph validation reported a reference mismatch.\n" );
	}
	Nav_Hierarchy_LogRegionSummary( mesh );
}
