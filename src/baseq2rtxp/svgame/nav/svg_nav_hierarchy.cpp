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
* 	@return	void
* 	@note	Only cross-tile, conservative static edges are promoted into the adjacency graph.
* 	@details
* 		Scans every populated cell and layer for each canonical world tile. For each valid
* 		edge sample that represents conservative, bidirectional static connectivity the
* 		corresponding neighboring tile id is accumulated. Neighbor lists are collected
* 		per-tile, sorted deterministically and compacted into the persistent adjacency
* 		containers on the mesh. This function produces the coarse tile-level adjacency
* 		graph used by later region-building phases.
**/
void Nav_Hierarchy_BuildTileAdjacency( nav_mesh_t *mesh ) {
	/**
	* 	Sanity: require a mesh before building any adjacency storage.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	* 	Prepare adjacency containers sized to the canonical world-tile count.
	* 	Clear any previous neighbor references so the rebuild starts from a clean slate.
	**/
	mesh->hierarchy.tile_adjacency.clear();
	mesh->hierarchy.tile_neighbor_refs.clear();
	mesh->hierarchy.tile_adjacency.resize( mesh->world_tiles.size() );
	mesh->hierarchy.debug_adjacency_link_count = 0;

	/**
	* 	Accumulate neighbor ids per tile first so we can sort and compact them deterministically.
	* 	We collect into a temporary per-tile vector to avoid repeated allocation and to keep the
	* 	final storage layout stable across builds.
	**/
	std::vector<std::vector<int32_t>> temp_neighbors( mesh->world_tiles.size() );
	const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;

	/**
	* 	Iterate canonical world tiles and inspect only populated sparse cells because empty
	* 	slots cannot contribute connectivity information.
	**/
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		nav_tile_t &tile = mesh->world_tiles[ tile_id ];
		nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];

		/**
		* 	Record basic adjacency metadata: canonical tile id and coarse compatibility bits
		* 	derived from the tile's persisted summaries.
		**/
		adjacency.tile_id = tile_id;
		adjacency.compatibility_bits = Nav_Hierarchy_BuildTileCompatibilityBits( tile );

		/**
		* 	Query the sparse cell array for this tile. If the tile has no populated cells
		* 	or lacks presence bits (sparse representation), skip it — there is nothing to scan.
		**/
		auto cells_view = SVG_Nav_Tile_GetCells( mesh, &tile );
		nav_xy_cell_t *cells_ptr = cells_view.first;
		const int32_t cells_count = cells_view.second;
		if ( !cells_ptr || cells_count != cells_per_tile || !tile.presence_bits ) {
			// No navigable cells in this canonical tile or inconsistent representation.
			continue;
		}

		// Iterate every populated XY cell in this canonical world tile.
		for ( int32_t cell_index = 0; cell_index < cells_count; cell_index++ ) {
			// Check the sparse presence bit for this cell index.
			const int32_t word_index = cell_index >> 5;
			const int32_t bit_index = cell_index & 31;
			if ( ( tile.presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
				// This cell slot is empty in the sparse representation.
				continue;
			}

			nav_xy_cell_t &cell = cells_ptr[ cell_index ];

			/**
			* 	Get layer array for this cell. If there are no layers, the cell cannot provide
			* 	edge connectivity samples and is skipped.
			**/
			auto layers_view = SVG_Nav_Cell_GetLayers( &cell );
			nav_layer_t *layers_ptr = layers_view.first;
			const int32_t layer_count = layers_view.second;
			if ( !layers_ptr || layer_count <= 0 ) {
				continue;
			}

			/**
			* 	Compute local XY within tile for this cell so neighbor offsets can be resolved
			* 	into canonical tile coordinates when edge deltas step outside the tile bounds.
			**/
			const int32_t local_cell_x = cell_index % mesh->tile_size;
			const int32_t local_cell_y = cell_index / mesh->tile_size;

			// Inspect every layer and promote cross-tile passable edges into canonical tile adjacency.
			for ( int32_t layer_index = 0; layer_index < layer_count; layer_index++ ) {
				const nav_layer_t &layer = layers_ptr[ layer_index ];

				/**
				* 	Iterate every possible persisted edge direction. Skip directions that are
				* 	not valid for this layer (edge_valid_mask) because they do not represent
				* 	a real boundary in the fine navmesh.
				**/
				for ( int32_t edge_dir_index = 0; edge_dir_index < NAV_LAYER_EDGE_DIR_COUNT; edge_dir_index++ ) {
					if ( ( layer.edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
						// Edge slot not populated/valid for this layer.
						continue;
					}

					/**
					* 	Only promote edges that represent conservative bidirectional static connectivity.
					* 	This filters out walk-off-only, forbidden walk-offs, hard-wall blocked and
					* 	jump-obstructed edges from joining the coarse adjacency graph.
					**/
					const uint32_t edge_bits = layer.edge_feature_bits[ edge_dir_index ];
					if ( !Nav_Hierarchy_EdgeAllowsStaticRegionPass( edge_bits ) ) {
						continue;
					}

					/**
					* 	Translate the persisted edge slot index into an XY cell delta. If the index is
					* 	invalid, skip the sample.
					**/
					int32_t cell_dx = 0;
					int32_t cell_dy = 0;
					if ( !Nav_Hierarchy_EdgeDirDelta( edge_dir_index, &cell_dx, &cell_dy ) ) {
						continue;
					}

					/**
					* 	Resolve target local coordinates and then canonical tile coordinates. We only
					* 	promote cross-tile edges — internal tile edges do not affect the tile-level graph.
					**/
					const int32_t target_local_x = local_cell_x + cell_dx;
					const int32_t target_local_y = local_cell_y + cell_dy;
					const int32_t target_tile_x = tile.tile_x + ( target_local_x < 0 ? -1 : ( target_local_x >= mesh->tile_size ? 1 : 0 ) );
					const int32_t target_tile_y = tile.tile_y + ( target_local_y < 0 ? -1 : ( target_local_y >= mesh->tile_size ? 1 : 0 ) );

					// Keep the region graph coarse at tile granularity by promoting only true cross-tile edges.
					if ( target_tile_x == tile.tile_x && target_tile_y == tile.tile_y ) {
						// Edge stays inside the same canonical tile.
						continue;
					}

					/**
					* 	Look up the canonical id of the neighboring tile. If the neighbor is
					* 	absent or equals this tile, skip the edge sample.
					**/
					const int32_t neighbor_tile_id = Nav_Hierarchy_FindWorldTileId( mesh, target_tile_x, target_tile_y );
					if ( neighbor_tile_id < 0 || neighbor_tile_id == tile_id ) {
						continue;
					}

					// Accumulate the neighbor id for this tile; duplicates will be compacted later.
					temp_neighbors[ tile_id ].push_back( neighbor_tile_id );
				}
			}
		}
	}

	/**
	* 	Sort and compact neighbor ids before flattening them into the persistent adjacency store.
	* 	This step produces deterministic ordering (tile_y, tile_x, tile_id) and removes duplicates
	* 	so higher-level algorithms see a stable adjacency layout.
	**/
	for ( int32_t tile_id = 0; tile_id < ( int32_t )temp_neighbors.size(); tile_id++ ) {
		std::vector<int32_t> &neighbors = temp_neighbors[ tile_id ];

		// Deterministic sort by tile coordinates and id to make builds reproducible.
		Nav_Hierarchy_SortTileIdsDeterministic( mesh, neighbors );

		// Compact duplicate neighbor entries produced by multiple contributing cell samples.
		neighbors.erase( std::unique( neighbors.begin(), neighbors.end() ), neighbors.end() );

		/**
		* 	Flatten the per-tile neighbor list into the mesh's persistent adjacency arrays,
		* 	recording offsets and counts for each tile adjacency entry.
		**/
		nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
		adjacency.first_neighbor_ref = ( int32_t )mesh->hierarchy.tile_neighbor_refs.size();
		adjacency.num_neighbor_refs = ( int32_t )neighbors.size();
		mesh->hierarchy.tile_neighbor_refs.insert( mesh->hierarchy.tile_neighbor_refs.end(), neighbors.begin(), neighbors.end() );
	}

	// Update debug counters for the total number of adjacency links stored.
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
* 	@note	This function performs a per-region DFS over the coarse tile adjacency graph and verifies
* 			that the number of visited tiles equals the stored tile count for the region. Useful as
* 			a post-build consistency check for Phase 3 region generation.
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
	* 	This allows O(1) region membership queries while traversing the adjacency graph.
	**/
	std::vector<int32_t> tile_region_lookup( mesh->world_tiles.size(), NAV_REGION_ID_NONE );
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		tile_region_lookup[ tile_id ] = mesh->world_tiles[ tile_id ].region_id;
	}

	/**
	* 	Walk each region over stored tile adjacency and confirm the visited count matches the region tile count.
	* 	We skip trivial regions (0 or 1 tile) since they are vacuously connected.
	**/
	for ( const nav_region_t &region : mesh->hierarchy.regions ) {
		// Trivial connectivity: nothing to validate for regions with <= 1 tiles.
		if ( region.num_tile_refs <= 1 ) {
			continue;
		}

		/**
		* 	Prepare traversal state:
		* 	 - `stack` holds tile ids to visit (DFS).
		* 	 - `visited` marks canonical tiles we've already pushed to avoid re-visits.
		* 	 - `start_tile_id` is a deterministic anchor from the region's flat tile-ref range.
		**/
		std::vector<int32_t> stack;
		stack.reserve( ( size_t )region.num_tile_refs );
		std::vector<uint8_t> visited( mesh->world_tiles.size(), 0 );

		// Acquire deterministic start tile from the region's tile-ref list.
		const int32_t start_tile_id = mesh->hierarchy.tile_refs[ region.first_tile_ref ].tile_id;
		stack.push_back( start_tile_id );
		visited[ start_tile_id ] = 1;

		/**
		* 	DFS over the tile adjacency graph restricted to tiles that belong to this region.
		* 	We only push neighbor tiles that:
		* 	 - are valid canonical tile ids,
		* 	 - belong to the same region (checked via `tile_region_lookup`),
		* 	 - have not been visited yet.
		**/
		int32_t visited_count = 0;
		while ( !stack.empty() ) {
			const int32_t tile_id = stack.back();
			stack.pop_back();
			visited_count++;

			// Read adjacency for the current canonical tile.
			const nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
			// Iterate neighbor refs and push eligible neighbors.
			for ( int32_t neighbor_index = 0; neighbor_index < adjacency.num_neighbor_refs; neighbor_index++ ) {
				const int32_t neighbor_tile_id = mesh->hierarchy.tile_neighbor_refs[ adjacency.first_neighbor_ref + neighbor_index ];

				// Skip invalid neighbor ids (safety guard for corrupted adjacency arrays).
				if ( neighbor_tile_id < 0 || neighbor_tile_id >= ( int32_t )tile_region_lookup.size() ) {
					continue;
				}

				// Require neighbor to belong to the same region and be unvisited.
				if ( tile_region_lookup[ neighbor_tile_id ] != region.id || visited[ neighbor_tile_id ] != 0 ) {
					continue;
				}

				// Mark and schedule the neighbor for traversal.
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

	// All regions validated successfully.
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
