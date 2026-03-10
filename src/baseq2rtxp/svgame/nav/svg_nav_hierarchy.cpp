/****************************************************************************************************************************************
*
*
*    SVGame: Navigation Hierarchy Helpers - Implementation
*
*    Contains portal overlay helpers and public hierarchy APIs previously located in svg_nav.cpp
*
**********************************************************************************************************************************/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_hierarchy.h"

/**
*    @brief	Clear all local portal overlay entries while leaving the static hierarchy untouched.
*    @param	mesh	Navigation mesh containing the hierarchy portal overlay.
*    @note	This is local runtime state only and must not trigger hierarchy regeneration.
**/
void SVG_Nav_Hierarchy_ClearPortalOverlays( nav_mesh_t * mesh ) {
	if ( !mesh ) {
		return;
	}
	mesh->hierarchy.portal_overlays.assign( mesh->hierarchy.portals.size(), nav_portal_overlay_t{} );
	mesh->hierarchy.portal_overlay_serial = 0;
}

/**
*    @brief	Update one local portal overlay entry without mutating the static portal graph.
*    @param	mesh	Navigation mesh containing the hierarchy portal overlay.
*    @param	portal_id	Stable compact portal id to update.
*    @param	overlay_flags	Overlay flags such as invalidated, blocked, or dirty.
*    @param	additional_traversal_cost	Optional local penalty added when the portal remains usable.
*    @return	True when the overlay entry was updated.
**/
bool SVG_Nav_Hierarchy_SetPortalOverlay( nav_mesh_t * mesh, int32_t portal_id, uint32_t overlay_flags, double additional_traversal_cost ) {
	if ( !mesh || portal_id < 0 || portal_id >= ( int32_t )mesh->hierarchy.portals.size() ) {
		return false;
	}

	if ( mesh->hierarchy.portal_overlays.size() != mesh->hierarchy.portals.size() ) {
		mesh->hierarchy.portal_overlays.assign( mesh->hierarchy.portals.size(), nav_portal_overlay_t{} );
	}

	nav_portal_overlay_t &overlay = mesh->hierarchy.portal_overlays[ portal_id ];
	overlay.flags = overlay_flags & ( NAV_PORTAL_FLAG_INVALIDATED | NAV_PORTAL_FLAG_BLOCKED | NAV_PORTAL_FLAG_DIRTY );
	overlay.additional_traversal_cost = std::max( 0.0, additional_traversal_cost );
	overlay.invalidation_serial = ++mesh->hierarchy.portal_overlay_serial;
	return true;
}

/**
*    @brief	Query one local portal overlay entry by compact portal id.
*    @param	mesh	Navigation mesh containing the hierarchy portal overlay.
*    @param	portal_id	Stable compact portal id to query.
*    @param	out_overlay	[out] Current local portal overlay entry.
*    @return	True when the portal id is valid and an overlay snapshot was returned.
**/
bool SVG_Nav_Hierarchy_TryGetPortalOverlay( const nav_mesh_t * mesh, int32_t portal_id, nav_portal_overlay_t * out_overlay ) {
	if ( !mesh || !out_overlay || portal_id < 0 || portal_id >= ( int32_t )mesh->hierarchy.portals.size() ) {
		return false;
	}

	if ( portal_id >= ( int32_t )mesh->hierarchy.portal_overlays.size() ) {
		* out_overlay = {};
		return true;
	}

	* out_overlay = mesh->hierarchy.portal_overlays[ portal_id ];
	return true;
}

/**
* 	@brief	Reset hierarchy membership on existing tiles and leaves.
* 	@param	mesh	Navigation mesh whose per-tile and per-leaf region membership should be cleared.
* 	@note	This keeps the fine navmesh intact while removing any stale hierarchy ownership, by
* 			freesing leaf-owned region-id arrays and restores all memberships to sentinel values.
**/
void SVG_Nav_Hierarchy_ResetMembership( nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a mesh before touching hierarchy membership state.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	* 	Reset canonical world-tile region membership so future hierarchy builders start clean.
	**/
	for ( nav_tile_t &tile : mesh->world_tiles ) {
		tile.region_id = NAV_REGION_ID_NONE;
	}

	/**
	* 	Reset leaf region membership arrays because 3 will repopulate them deterministically.
	**/
	if ( mesh->leaf_data ) {
		for ( int32_t leaf_index = 0; leaf_index < mesh->num_leafs; leaf_index++ ) {
			nav_leaf_data_t &leaf = mesh->leaf_data[ leaf_index ];
			if ( leaf.region_ids ) {
				gi.TagFree( leaf.region_ids );
				leaf.region_ids = nullptr;
			}
			leaf.num_regions = 0;
		}
	}

	/**
	* 	Reset inline-model tile membership too so future overlay-aware hierarchy code starts from sentinels.
	**/
	if ( mesh->inline_model_data ) {
		for ( int32_t model_index = 0; model_index < mesh->num_inline_models; model_index++ ) {
			nav_inline_model_data_t &model = mesh->inline_model_data[ model_index ];
			for ( int32_t tile_index = 0; tile_index < model.num_tiles; tile_index++ ) {
				model.tiles[ tile_index ].region_id = NAV_REGION_ID_NONE;
			}
		}
	}
}

/**
* 	@brief	Clear all internal hierarchy scaffolding owned by a navigation mesh.
* 	@param	mesh	Navigation mesh whose hierarchy containers should be cleared.
* 	@note	This preserves the fine navmesh while invalidating region/portal state and memberships.
**/
void SVG_Nav_Hierarchy_Clear( nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a mesh before clearing hierarchy state.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	* 	Reset per-tile and per-leaf membership first so no stale ids remain visible.
	**/
	SVG_Nav_Hierarchy_ResetMembership( mesh );

	/**
	* 	Clear the flat hierarchy containers and mark the hierarchy dirty for future rebuilds.
	**/
	mesh->hierarchy.regions.clear();
	mesh->hierarchy.tile_adjacency.clear();
	mesh->hierarchy.tile_neighbor_refs.clear();
	mesh->hierarchy.portals.clear();
	mesh->hierarchy.portal_overlays.clear();
	mesh->hierarchy.tile_refs.clear();
	mesh->hierarchy.leaf_refs.clear();
	mesh->hierarchy.region_portal_refs.clear();
	mesh->hierarchy.debug_region_count = 0;
	mesh->hierarchy.debug_adjacency_link_count = 0;
	mesh->hierarchy.debug_boundary_link_count = 0;
	mesh->hierarchy.debug_portal_count = 0;
	mesh->hierarchy.debug_isolated_region_count = 0;
	mesh->hierarchy.debug_oversized_region_count = 0;
	mesh->hierarchy.build_serial = 0;
	mesh->hierarchy.portal_overlay_serial = 0;
	mesh->hierarchy.dirty = true;
}

/**
* 	@brief	Map a persisted edge slot onto its XY direction delta.
* 	@param	edge_dir_index	Persisted edge slot index (0..7).
* 	@param	out_dx		[out] Neighbor X delta.
* 	@param	out_dy		[out] Neighbor Y delta.
* 	@return	True when the slot index is valid.
**/
bool Nav_Hierarchy_EdgeDirDelta( const int32_t edge_dir_index, int32_t *out_dx, int32_t *out_dy ) {
	/**
	* 	Sanity: require valid output pointers before decoding the edge slot.
	**/
	if ( !out_dx || !out_dy ) {
		return false;
	}

	/**
	* 	Translate the persisted edge slot ordering established during generation.
	**/
	switch ( edge_dir_index ) {
	case 0:
		*out_dx = 1;
		*out_dy = 0;
		return true;
	case 1:
		*out_dx = -1;
		*out_dy = 0;
		return true;
	case 2:
		*out_dx = 0;
		*out_dy = 1;
		return true;
	case 3:
		*out_dx = 0;
		*out_dy = -1;
		return true;
	case 4:
		*out_dx = 1;
		*out_dy = 1;
		return true;
	case 5:
		*out_dx = 1;
		*out_dy = -1;
		return true;
	case 6:
		*out_dx = -1;
		*out_dy = 1;
		return true;
	case 7:
		*out_dx = -1;
		*out_dy = -1;
		return true;
	default:
		return false;
	}
}

/**
* 	@brief	Determine whether a persisted edge is suitable for static region connectivity.
* 	@param	edge_bits	Per-edge feature bits finalized during generation.
* 	@return	True when the edge represents conservative bidirectional static connectivity.
* 	@note	Walk-off-only, hard-wall, and jump-obstructed edges stay out of the static region graph.
**/
bool Nav_Hierarchy_EdgeAllowsStaticRegionPass( const uint32_t edge_bits ) {
	/**
	* 	Require ordinary passability first because missing or forbidden edges must never join regions.
	**/
	if ( ( edge_bits & NAV_EDGE_FEATURE_PASSABLE ) == 0 ) {
		return false;
	}

	/**
	* 	Keep the first pass conservative by excluding optional or forbidden walk-offs.
	**/
	if ( ( edge_bits & ( NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF | NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF ) ) != 0 ) {
		return false;
	}

	/**
	* 	Exclude edges explicitly marked as blocked or requiring unsupported jump-style behavior.
	**/
	if ( ( edge_bits & ( NAV_EDGE_FEATURE_HARD_WALL_BLOCKED | NAV_EDGE_FEATURE_JUMP_OBSTRUCTION ) ) != 0 ) {
		return false;
	}

	return true;
}

/**
* 	@brief	Resolve a canonical world-tile id by tile-grid coordinates.
* 	@param	mesh	Navigation mesh owning the canonical world-tile lookup.
* 	@param	tile_x	Tile X coordinate.
* 	@param	tile_y	Tile Y coordinate.
* 	@return	Canonical world-tile id, or -1 when absent.
**/
int32_t Nav_Hierarchy_FindWorldTileId( const nav_mesh_t *mesh, const int32_t tile_x, const int32_t tile_y ) {
	/**
	* 	Sanity: require a valid mesh before querying the world tile lookup table.
	**/
	if ( !mesh ) {
		return -1;
	}

	/**
	* 	Consult the canonical `(tile_x,tile_y)` lookup used everywhere else in the nav mesh.
	**/
	const nav_world_tile_key_t key = { .tile_x = tile_x, .tile_y = tile_y };
	auto it = mesh->world_tile_id_of.find( key );
	if ( it == mesh->world_tile_id_of.end() ) {
		return -1;
	}

	return it->second;
}

/**
* 	@brief	Build hierarchy compatibility bits from a tile's persisted summary metadata.
* 	@param	tile	Tile whose coarse summaries should be translated.
* 	@return	Compatibility bits suitable for future region or portal policy filtering.
**/
uint32_t Nav_Hierarchy_BuildTileCompatibilityBits( const nav_tile_t &tile ) {
	/**
	* 	Every navigable tile participates in ordinary ground traversal by default.
	**/
	uint32_t compatibility_bits = NAV_HIERARCHY_COMPAT_GROUND;
	const uint32_t summary_bits = tile.traversal_summary_bits | tile.edge_summary_bits;

	/**
	* 	Promote persisted traversal summaries into coarse compatibility hooks.
	**/
	if ( ( summary_bits & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_STAIR;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_LADDER;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_WATER ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_WATER;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LAVA ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_LAVA;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_SLIME ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_SLIME;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_WALK_OFF ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_WALK_OFF;
	}

	return compatibility_bits;
}

/**
* 	@brief	Build initial region summary flags from a tile's persisted coarse metadata.
* 	@param	tile	Tile whose coarse summaries should be translated.
* 	@return	Region flags suitable for aggregated Phase 3 region summaries.
**/
uint32_t Nav_Hierarchy_BuildTileRegionFlags( const nav_tile_t &tile ) {
	uint32_t region_flags = NAV_REGION_FLAG_NONE;
	const uint32_t summary_bits = tile.traversal_summary_bits | tile.edge_summary_bits;

	/**
	* 	Promote tile summaries into region-level feature flags.
	**/
	if ( ( summary_bits & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_STAIRS;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_LADDERS;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_WATER ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_WATER;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LAVA ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_LAVA;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_SLIME ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_SLIME;
	}

	return region_flags;
}



/**
* 	@brief	Sort canonical tile ids by tile-grid coordinates for deterministic hierarchy output.
* 	@param	mesh	Navigation mesh owning the canonical world tiles.
* 	@param	tile_ids	Tile-id array to sort in place.
* 	@note	Tile id is used as the final tie-breaker to keep ordering stable.
**/
void Nav_Hierarchy_SortTileIdsDeterministic( const nav_mesh_t *mesh, std::vector<int32_t> &tile_ids ) {
	/**
	* 	Sanity: require a mesh before looking up tile coordinates for sorting.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	* 	Sort by `(tile_y, tile_x, tile_id)` so region seeding and neighbor lists remain deterministic.
	**/
	std::sort( tile_ids.begin(), tile_ids.end(), [mesh]( const int32_t lhs, const int32_t rhs ) {
		const nav_tile_t &lhs_tile = mesh->world_tiles[ lhs ];
		const nav_tile_t &rhs_tile = mesh->world_tiles[ rhs ];
		if ( lhs_tile.tile_y != rhs_tile.tile_y ) {
			return lhs_tile.tile_y < rhs_tile.tile_y;
		}
		if ( lhs_tile.tile_x != rhs_tile.tile_x ) {
			return lhs_tile.tile_x < rhs_tile.tile_x;
		}
		return lhs < rhs;
		} );
}

/**
* 	@brief	Derive canonical world-tile adjacency from persisted fine edge metadata.
* 	@param	mesh	Navigation mesh whose canonical world tiles should be scanned.
* 	@note	Only cross-tile, conservative static edges are promoted into the adjacency graph.
**/
void Nav_Hierarchy_BuildTileAdjacency( nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a mesh before building any adjacency storage.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	* 	Size the adjacency records to the canonical world-tile count and clear previous neighbor refs.
	**/
	mesh->hierarchy.tile_adjacency.clear();
	mesh->hierarchy.tile_neighbor_refs.clear();
	mesh->hierarchy.tile_adjacency.resize( mesh->world_tiles.size() );
	mesh->hierarchy.debug_adjacency_link_count = 0;

	/**
	* 	Accumulate neighbor ids per tile first so we can sort and compact them deterministically.
	**/
	std::vector<std::vector<int32_t>> temp_neighbors( mesh->world_tiles.size() );
	const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		nav_tile_t &tile = mesh->world_tiles[ tile_id ];
		nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
		adjacency.tile_id = tile_id;
		adjacency.compatibility_bits = Nav_Hierarchy_BuildTileCompatibilityBits( tile );

		/**
		* 	Inspect only populated sparse cells because empty slots cannot contribute connectivity.
		**/
		auto cells_view = SVG_Nav_Tile_GetCells( mesh, &tile );
		nav_xy_cell_t *cells_ptr = cells_view.first;
		const int32_t cells_count = cells_view.second;
		if ( !cells_ptr || cells_count != cells_per_tile || !tile.presence_bits ) {
			continue;
		}

		// Iterate every populated XY cell in this canonical world tile.
		for ( int32_t cell_index = 0; cell_index < cells_count; cell_index++ ) {
			const int32_t word_index = cell_index >> 5;
			const int32_t bit_index = cell_index & 31;
			if ( ( tile.presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
				continue;
			}

			nav_xy_cell_t &cell = cells_ptr[ cell_index ];
			auto layers_view = SVG_Nav_Cell_GetLayers( &cell );
			nav_layer_t *layers_ptr = layers_view.first;
			const int32_t layer_count = layers_view.second;
			if ( !layers_ptr || layer_count <= 0 ) {
				continue;
			}

			const int32_t local_cell_x = cell_index % mesh->tile_size;
			const int32_t local_cell_y = cell_index / mesh->tile_size;
			// Inspect every layer and promote cross-tile passable edges into canonical tile adjacency.
			for ( int32_t layer_index = 0; layer_index < layer_count; layer_index++ ) {
				const nav_layer_t &layer = layers_ptr[ layer_index ];
				for ( int32_t edge_dir_index = 0; edge_dir_index < NAV_LAYER_EDGE_DIR_COUNT; edge_dir_index++ ) {
					if ( ( layer.edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
						continue;
					}

					const uint32_t edge_bits = layer.edge_feature_bits[ edge_dir_index ];
					if ( !Nav_Hierarchy_EdgeAllowsStaticRegionPass( edge_bits ) ) {
						continue;
					}

					int32_t cell_dx = 0;
					int32_t cell_dy = 0;
					if ( !Nav_Hierarchy_EdgeDirDelta( edge_dir_index, &cell_dx, &cell_dy ) ) {
						continue;
					}

					const int32_t target_local_x = local_cell_x + cell_dx;
					const int32_t target_local_y = local_cell_y + cell_dy;
					const int32_t target_tile_x = tile.tile_x + ( target_local_x < 0 ? -1 : ( target_local_x >= mesh->tile_size ? 1 : 0 ) );
					const int32_t target_tile_y = tile.tile_y + ( target_local_y < 0 ? -1 : ( target_local_y >= mesh->tile_size ? 1 : 0 ) );

					// Keep the region graph coarse at tile granularity by promoting only true cross-tile edges.
					if ( target_tile_x == tile.tile_x && target_tile_y == tile.tile_y ) {
						continue;
					}

					const int32_t neighbor_tile_id = Nav_Hierarchy_FindWorldTileId( mesh, target_tile_x, target_tile_y );
					if ( neighbor_tile_id < 0 || neighbor_tile_id == tile_id ) {
						continue;
					}

					temp_neighbors[ tile_id ].push_back( neighbor_tile_id );
				}
			}
		}
	}

	/**
	* 	Sort and compact neighbor ids before flattening them into the persistent adjacency store.
	**/
	for ( int32_t tile_id = 0; tile_id < ( int32_t )temp_neighbors.size(); tile_id++ ) {
		std::vector<int32_t> &neighbors = temp_neighbors[ tile_id ];
		Nav_Hierarchy_SortTileIdsDeterministic( mesh, neighbors );
		neighbors.erase( std::unique( neighbors.begin(), neighbors.end() ), neighbors.end() );

		nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
		adjacency.first_neighbor_ref = ( int32_t )mesh->hierarchy.tile_neighbor_refs.size();
		adjacency.num_neighbor_refs = ( int32_t )neighbors.size();
		mesh->hierarchy.tile_neighbor_refs.insert( mesh->hierarchy.tile_neighbor_refs.end(), neighbors.begin(), neighbors.end() );
	}

	mesh->hierarchy.debug_adjacency_link_count = ( int32_t )mesh->hierarchy.tile_neighbor_refs.size();
}


/**
* 	@brief	Build a compact ordered key for an unordered region pair.
* 	@param	region_a	First region id.
* 	@param	region_b	Second region id.
* 	@return	Packed 64-bit region-pair key with the lower id first.
**/
uint64_t Nav_Hierarchy_MakeRegionPairKey( const int32_t region_a, const int32_t region_b ) {
	/**
	* 	Normalize the pair ordering first so the same boundary maps to one stable key.
	**/
	const uint32_t lo = ( uint32_t )std::min( region_a, region_b );
	const uint32_t hi = ( uint32_t )std::max( region_a, region_b );
	return ( ( uint64_t )lo << 32 ) | ( uint64_t )hi;
}


/**
* 	@brief	Validate that every region remains internally connected over the stored tile adjacency graph.
* 	@param	mesh	Navigation mesh containing the freshly built hierarchy.
* 	@return	True when every region validates successfully.
**/
bool Nav_Hierarchy_ValidateRegions( const nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a mesh before validating any region connectivity.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	* 	Build a reusable region lookup table over canonical tile ids for cheap membership checks.
	**/
	std::vector<int32_t> tile_region_lookup( mesh->world_tiles.size(), NAV_REGION_ID_NONE );
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		tile_region_lookup[ tile_id ] = mesh->world_tiles[ tile_id ].region_id;
	}

	/**
	* 	Walk each region over stored tile adjacency and confirm the visited count matches the region tile count.
	**/
	for ( const nav_region_t &region : mesh->hierarchy.regions ) {
		if ( region.num_tile_refs <= 1 ) {
			continue;
		}

		std::vector<int32_t> stack;
		stack.reserve( ( size_t )region.num_tile_refs );
		std::vector<uint8_t> visited( mesh->world_tiles.size(), 0 );
		const int32_t start_tile_id = mesh->hierarchy.tile_refs[ region.first_tile_ref ].tile_id;
		stack.push_back( start_tile_id );
		visited[ start_tile_id ] = 1;

		int32_t visited_count = 0;
		while ( !stack.empty() ) {
			const int32_t tile_id = stack.back();
			stack.pop_back();
			visited_count++;

			const nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
			for ( int32_t neighbor_index = 0; neighbor_index < adjacency.num_neighbor_refs; neighbor_index++ ) {
				const int32_t neighbor_tile_id = mesh->hierarchy.tile_neighbor_refs[ adjacency.first_neighbor_ref + neighbor_index ];
				if ( neighbor_tile_id < 0 || neighbor_tile_id >= ( int32_t )tile_region_lookup.size() ) {
					continue;
				}
				if ( tile_region_lookup[ neighbor_tile_id ] != region.id || visited[ neighbor_tile_id ] != 0 ) {
					continue;
				}

				visited[ neighbor_tile_id ] = 1;
				stack.push_back( neighbor_tile_id );
			}
		}

		// Reject any region whose stored tile set cannot be re-traversed through the formalized adjacency graph.
		if ( visited_count != region.num_tile_refs ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Region %d connectivity mismatch: expected=%d visited=%d\n",
				region.id,
				region.num_tile_refs,
				visited_count );
			return false;
		}
	}

	return true;
}

/**
* 	@brief	Temporary merged portal accumulator used while scanning fine cross-region boundaries.
**/
struct nav_portal_accumulator_t {
	//! Ordered region id on one side of the merged portal.
	int32_t region_a = NAV_REGION_ID_NONE;
	//! Ordered region id on the opposite side of the merged portal.
	int32_t region_b = NAV_REGION_ID_NONE;
	//! Deterministic anchor tile id for `region_a`.
	int32_t tile_id_a = -1;
	//! Deterministic anchor tile id for `region_b`.
	int32_t tile_id_b = -1;
	//! Aggregated portal flags derived from contributing boundary samples.
	uint32_t flags = NAV_PORTAL_FLAG_NONE;
	//! Aggregated compatibility bits derived from contributing boundary samples.
	uint32_t compatibility_bits = NAV_HIERARCHY_COMPAT_NONE;
	//! Running sum of representative sample X positions.
	double representative_sum_x = 0.0;
	//! Running sum of representative sample Y positions.
	double representative_sum_y = 0.0;
	//! Running sum of representative sample Z positions.
	double representative_sum_z = 0.0;
	//! Running sum of coarse traversal cost estimates.
	double traversal_cost_sum = 0.0;
	//! Number of contributing boundary samples merged into this accumulator.
	int32_t sample_count = 0;
};

/**
* 	@brief	Build merged portal records from traversable cross-region tile boundaries.
* 	@param	mesh	Navigation mesh containing finalized tile adjacency and region ids.
* 	@param	sorted_tile_ids	Deterministically ordered canonical world-tile ids.
**/
void Nav_Hierarchy_BuildPortals( nav_mesh_t *mesh, const std::vector<int32_t> &sorted_tile_ids ) {
	/**
	* 	Sanity: require a mesh before scanning any cross-region boundaries.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	* 	Reset previous portal storage so every rebuild starts from a clean region graph.
	**/
	mesh->hierarchy.portals.clear();
	mesh->hierarchy.portal_overlays.clear();
	mesh->hierarchy.region_portal_refs.clear();
	mesh->hierarchy.debug_boundary_link_count = 0;
	mesh->hierarchy.debug_portal_count = 0;

	/**
	* 	Accumulate one merged portal per unordered region pair while scanning fine edge samples.
	**/
	std::unordered_map<uint64_t, int32_t> accumulator_index_of;
	std::vector<nav_portal_accumulator_t> accumulators;
	accumulator_index_of.reserve( mesh->hierarchy.regions.size() * 2 );
	accumulators.reserve( mesh->hierarchy.regions.size() );
	const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
	const double tile_world_size = Nav_TileWorldSize( mesh );
	for ( const int32_t tile_id : sorted_tile_ids ) {
		const nav_tile_t &tile = mesh->world_tiles[ tile_id ];
		const int32_t source_region_id = tile.region_id;
		if ( source_region_id == NAV_REGION_ID_NONE ) {
			continue;
		}

		/**
		* 	Inspect only populated sparse cells because empty slots cannot contribute portal boundary samples.
		**/
		auto cells_view = SVG_Nav_Tile_GetCells( mesh, &mesh->world_tiles[ tile_id ] );
		nav_xy_cell_t *cells_ptr = cells_view.first;
		const int32_t cells_count = cells_view.second;
		if ( !cells_ptr || cells_count != cells_per_tile || !tile.presence_bits ) {
			continue;
		}

		// Inspect every populated source cell/layer and merge traversable cross-region boundaries by region pair.
		for ( int32_t cell_index = 0; cell_index < cells_count; cell_index++ ) {
			const int32_t word_index = cell_index >> 5;
			const int32_t bit_index = cell_index & 31;
			if ( ( tile.presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
				continue;
			}

			nav_xy_cell_t &cell = cells_ptr[ cell_index ];
			auto layers_view = SVG_Nav_Cell_GetLayers( &cell );
			nav_layer_t *layers_ptr = layers_view.first;
			const int32_t layer_count = layers_view.second;
			if ( !layers_ptr || layer_count <= 0 ) {
				continue;
			}

			const int32_t local_cell_x = cell_index % mesh->tile_size;
			const int32_t local_cell_y = cell_index / mesh->tile_size;
			for ( int32_t layer_index = 0; layer_index < layer_count; layer_index++ ) {
				const nav_layer_t &layer = layers_ptr[ layer_index ];
				for ( int32_t edge_dir_index = 0; edge_dir_index < NAV_LAYER_EDGE_DIR_COUNT; edge_dir_index++ ) {
					if ( ( layer.edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
						continue;
					}

					const uint32_t edge_bits = layer.edge_feature_bits[ edge_dir_index ];
					if ( !Nav_Hierarchy_EdgeAllowsStaticRegionPass( edge_bits ) ) {
						continue;
					}

					int32_t cell_dx = 0;
					int32_t cell_dy = 0;
					if ( !Nav_Hierarchy_EdgeDirDelta( edge_dir_index, &cell_dx, &cell_dy ) ) {
						continue;
					}

					const int32_t target_local_x = local_cell_x + cell_dx;
					const int32_t target_local_y = local_cell_y + cell_dy;
					const int32_t target_tile_x = tile.tile_x + ( target_local_x < 0 ? -1 : ( target_local_x >= mesh->tile_size ? 1 : 0 ) );
					const int32_t target_tile_y = tile.tile_y + ( target_local_y < 0 ? -1 : ( target_local_y >= mesh->tile_size ? 1 : 0 ) );

					// Keep portal extraction coarse at tile granularity by considering only true cross-tile boundaries.
					if ( target_tile_x == tile.tile_x && target_tile_y == tile.tile_y ) {
						continue;
					}

					const int32_t neighbor_tile_id = Nav_Hierarchy_FindWorldTileId( mesh, target_tile_x, target_tile_y );
					if ( neighbor_tile_id < 0 || neighbor_tile_id == tile_id ) {
						continue;
					}

					const nav_tile_t &neighbor_tile = mesh->world_tiles[ neighbor_tile_id ];
					const int32_t target_region_id = neighbor_tile.region_id;
					if ( target_region_id == NAV_REGION_ID_NONE || target_region_id == source_region_id ) {
						continue;
					}

					const int32_t region_a = std::min( source_region_id, target_region_id );
					const int32_t region_b = std::max( source_region_id, target_region_id );
					const uint64_t portal_key = Nav_Hierarchy_MakeRegionPairKey( region_a, region_b );
					auto accumulator_it = accumulator_index_of.find( portal_key );
					int32_t accumulator_index = -1;
					if ( accumulator_it == accumulator_index_of.end() ) {
						accumulator_index = ( int32_t )accumulators.size();
						accumulator_index_of.emplace( portal_key, accumulator_index );
						accumulators.push_back( nav_portal_accumulator_t{ .region_a = region_a, .region_b = region_b } );
					} else {
						accumulator_index = accumulator_it->second;
					}

					nav_portal_accumulator_t &accumulator = accumulators[ accumulator_index ];
					if ( accumulator.tile_id_a < 0 || accumulator.tile_id_b < 0 ) {
						if ( source_region_id == region_a ) {
							accumulator.tile_id_a = tile_id;
							accumulator.tile_id_b = neighbor_tile_id;
						} else {
							accumulator.tile_id_a = neighbor_tile_id;
							accumulator.tile_id_b = tile_id;
						}
					}

					const Vector3 sample_point = Nav_NodeWorldPosition( mesh, &tile, cell_index, &layer );
					accumulator.representative_sum_x += sample_point.x;
					accumulator.representative_sum_y += sample_point.y;
					accumulator.representative_sum_z += sample_point.z;
					accumulator.traversal_cost_sum += std::sqrt( ( double )( cell_dx * cell_dx ) + ( double )( cell_dy * cell_dy ) ) * tile_world_size;
					accumulator.sample_count++;
					accumulator.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( tile );
					accumulator.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( neighbor_tile );
					if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_STAIR ) != 0 ) {
						accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_STAIRS;
					}
					if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_LADDER ) != 0 ) {
						accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_LADDER;
					}
					if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_WATER ) != 0 ) {
						accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_WATER;
					}
					mesh->hierarchy.debug_boundary_link_count++;
				}
			}
		}
	}

	/**
	* 	Flatten the merged region-pair accumulators into stable portal records and region-to-portal refs.
	**/
	std::vector<std::vector<int32_t>> region_portal_lists( mesh->hierarchy.regions.size() );
	mesh->hierarchy.portals.reserve( accumulators.size() );
	for ( const nav_portal_accumulator_t &accumulator : accumulators ) {
		nav_portal_t portal = {};
		portal.id = ( int32_t )mesh->hierarchy.portals.size();
		portal.region_a = accumulator.region_a;
		portal.region_b = accumulator.region_b;
		portal.flags = accumulator.flags;
		portal.compatibility_bits = accumulator.compatibility_bits;
		portal.tile_id_a = accumulator.tile_id_a;
		portal.tile_id_b = accumulator.tile_id_b;
		if ( accumulator.sample_count > 0 ) {
			portal.representative_point = Vector3{
				( float )( accumulator.representative_sum_x / ( double )accumulator.sample_count ),
				( float )( accumulator.representative_sum_y / ( double )accumulator.sample_count ),
				( float )( accumulator.representative_sum_z / ( double )accumulator.sample_count )
			};
			portal.traversal_cost = accumulator.traversal_cost_sum / ( double )accumulator.sample_count;
		} else {
			portal.traversal_cost = tile_world_size;
		}

		mesh->hierarchy.portals.push_back( portal );
		region_portal_lists[ portal.region_a ].push_back( portal.id );
		region_portal_lists[ portal.region_b ].push_back( portal.id );
	}

	/**
	* 	Populate each region's coarse portal-reference range for future region-level routing.
	**/
	for ( nav_region_t &region : mesh->hierarchy.regions ) {
		std::vector<int32_t> &portal_ids = region_portal_lists[ region.id ];
		std::sort( portal_ids.begin(), portal_ids.end() );
		portal_ids.erase( std::unique( portal_ids.begin(), portal_ids.end() ), portal_ids.end() );
		region.first_portal_ref = ( int32_t )mesh->hierarchy.region_portal_refs.size();
		region.num_portal_refs = ( int32_t )portal_ids.size();
		mesh->hierarchy.region_portal_refs.insert( mesh->hierarchy.region_portal_refs.end(), portal_ids.begin(), portal_ids.end() );
	}

	mesh->hierarchy.debug_portal_count = ( int32_t )mesh->hierarchy.portals.size();
	mesh->hierarchy.portal_overlays.assign( mesh->hierarchy.portals.size(), nav_portal_overlay_t{} );
	mesh->hierarchy.portal_overlay_serial = 0;
}

/**
* 	@brief	Validate the merged portal graph produced from region boundaries.
* 	@param	mesh	Navigation mesh containing freshly built portal records.
* 	@return	True when the portal graph references remain internally consistent.
**/
bool Nav_Hierarchy_ValidatePortalGraph( const nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a mesh before validating any portal graph references.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	* 	Reject invalid portal endpoints and duplicate region pairs before checking region-reference symmetry.
	**/
	std::unordered_map<uint64_t, int32_t> portal_pair_seen;
	for ( const nav_portal_t &portal : mesh->hierarchy.portals ) {
		if ( portal.region_a < 0 || portal.region_b < 0 || portal.region_a >= ( int32_t )mesh->hierarchy.regions.size() || portal.region_b >= ( int32_t )mesh->hierarchy.regions.size() || portal.region_a == portal.region_b ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Portal %d has invalid region endpoints (%d,%d).\n", portal.id, portal.region_a, portal.region_b );
			return false;
		}

		const uint64_t key = Nav_Hierarchy_MakeRegionPairKey( portal.region_a, portal.region_b );
		if ( portal_pair_seen.find( key ) != portal_pair_seen.end() ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Duplicate merged portal detected for region pair (%d,%d).\n", portal.region_a, portal.region_b );
			return false;
		}
		portal_pair_seen.emplace( key, portal.id );
	}

	/**
	* 	Verify that every portal appears in both owning region reference ranges.
	**/
	for ( const nav_portal_t &portal : mesh->hierarchy.portals ) {
		bool found_in_a = false;
		bool found_in_b = false;
		const nav_region_t &region_a = mesh->hierarchy.regions[ portal.region_a ];
		const nav_region_t &region_b = mesh->hierarchy.regions[ portal.region_b ];
		for ( int32_t portal_ref_index = 0; portal_ref_index < region_a.num_portal_refs; portal_ref_index++ ) {
			if ( mesh->hierarchy.region_portal_refs[ region_a.first_portal_ref + portal_ref_index ] == portal.id ) {
				found_in_a = true;
				break;
			}
		}
		for ( int32_t portal_ref_index = 0; portal_ref_index < region_b.num_portal_refs; portal_ref_index++ ) {
			if ( mesh->hierarchy.region_portal_refs[ region_b.first_portal_ref + portal_ref_index ] == portal.id ) {
				found_in_b = true;
				break;
			}
		}

		if ( !found_in_a || !found_in_b ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Portal %d missing symmetric region references (a=%d b=%d).\n", portal.id, portal.region_a, portal.region_b );
			return false;
		}
	}

	return true;
}

/**
* 	@brief	Emit a concise summary of the current static region and portal graph.
* 	@param	mesh	Navigation mesh containing the freshly built hierarchy.
* 	@note	Logs only a capped subset of suspicious regions to avoid log spam.
**/
void Nav_Hierarchy_LogRegionSummary( const nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a mesh and a built hierarchy before emitting summary diagnostics.
	**/
	if ( !mesh ) {
		return;
	}

	const int32_t region_count = ( int32_t )mesh->hierarchy.regions.size();
	const int32_t portal_count = ( int32_t )mesh->hierarchy.portals.size();
	const int32_t world_tile_count = ( int32_t )mesh->world_tiles.size();
	const double avg_tiles_per_region = ( region_count > 0 ) ? ( ( double )world_tile_count / ( double )region_count ) : 0.0;
	const double avg_portals_per_region = ( region_count > 0 ) ? ( ( double )mesh->hierarchy.region_portal_refs.size() / ( double )region_count ) : 0.0;

	/**
	* 	Print the one-line Phase 3 summary first so benchmarks have a stable high-level snapshot.
	**/
	SVG_Nav_Log( "[NavHierarchy] tile_links=%d region_boundaries=%d regions=%d portals=%d avg_tiles_per_region=%.2f avg_portals_per_region=%.2f isolated=%d oversized=%d\n",
		mesh->hierarchy.debug_adjacency_link_count,
		mesh->hierarchy.debug_boundary_link_count,
		region_count,
		portal_count,
		avg_tiles_per_region,
		avg_portals_per_region,
		mesh->hierarchy.debug_isolated_region_count,
		mesh->hierarchy.debug_oversized_region_count );

	/**
	* 	Cap suspicious-region logging so the hierarchy summary stays readable on noisy maps.
	**/
	int32_t isolated_printed = 0;
	int32_t oversized_printed = 0;
	for ( const nav_region_t &region : mesh->hierarchy.regions ) {
		if ( ( region.flags & NAV_REGION_FLAG_ISOLATED ) != 0 && isolated_printed < 8 ) {
			const int32_t anchor_tile_id = region.debug_anchor_tile_id;
			if ( anchor_tile_id >= 0 && anchor_tile_id < ( int32_t )mesh->world_tiles.size() ) {
				const nav_tile_t &anchor_tile = mesh->world_tiles[ anchor_tile_id ];
				SVG_Nav_Log( "[NavHierarchy] isolated_region id=%d tiles=%d anchor=(%d,%d)\n",
					region.id,
					region.num_tile_refs,
					anchor_tile.tile_x,
					anchor_tile.tile_y );
				isolated_printed++;
			}
		}

		if ( region.num_tile_refs >= NAV_HIERARCHY_OVERSIZED_REGION_TILE_COUNT && oversized_printed < 8 ) {
			const int32_t anchor_tile_id = region.debug_anchor_tile_id;
			if ( anchor_tile_id >= 0 && anchor_tile_id < ( int32_t )mesh->world_tiles.size() ) {
				const nav_tile_t &anchor_tile = mesh->world_tiles[ anchor_tile_id ];
				SVG_Nav_Log( "[NavHierarchy] oversized_region id=%d tiles=%d anchor=(%d,%d)\n",
					region.id,
					region.num_tile_refs,
					anchor_tile.tile_x,
					anchor_tile.tile_y );
				oversized_printed++;
			}
		}
	}
}
