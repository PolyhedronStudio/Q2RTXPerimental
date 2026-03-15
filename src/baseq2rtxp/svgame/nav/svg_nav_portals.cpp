/****************************************************************************************************************************************
*
*
*    SVGame: Navigation Portal Helpers - Implementation
*
*    Contains static portal extraction and validation helpers split from the hierarchy TU.
*
**********************************************************************************************************************************/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_hierarchy.h"
#include "svgame/nav/svg_nav_portals.h"

/**
*	@brief	Temporary merged portal accumulator used while scanning fine cross-region boundaries.
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
*	@brief	Build merged portal records from traversable cross-region tile boundaries.
*	@param	mesh	Navigation mesh containing finalized tile adjacency and region ids.
*	@param	sorted_tile_ids	Deterministically ordered canonical world-tile ids.
**/
void Nav_Hierarchy_BuildPortals( nav_mesh_t * mesh, const std::vector<int32_t> &sorted_tile_ids ) {
    /**
    *	Sanity: require a valid mesh before scanning any cross-region boundaries.
    **/
    if ( !mesh ) {
        return;
    }

    /**
    *	Reset previous portal storage so every rebuild starts from a clean region graph.
    *	This clears persistent portal lists and statistics. The per-portal overlay state
    *	is cleared later after the new portals are produced.
    **/
    mesh->hierarchy.portals.clear();
    mesh->hierarchy.portal_overlays.clear();
    mesh->hierarchy.region_portal_refs.clear();
    mesh->hierarchy.debug_boundary_link_count = 0;
    mesh->hierarchy.debug_portal_count = 0;

    /**
    *	Prepare temporary accumulators and lookup map.
    *	We map a packed unordered-region pair key to an accumulator index and push
    *	accumulators only when we encounter a fresh region-pair boundary sample.
    **/
    std::unordered_map<uint64_t, int32_t> accumulator_index_of;
    std::vector<nav_portal_accumulator_t> accumulators;
    // Reserve capacity based on region count to reduce reallocations during scanning.
    accumulator_index_of.reserve( mesh->hierarchy.regions.size() * 2 );
    accumulators.reserve( mesh->hierarchy.regions.size() );

    /**
    *	Local constants used during sampling.
    **/
    const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
    const double tile_world_size = Nav_TileWorldSize( mesh );

    /**
    *	Main scan: iterate the supplied deterministically-sorted canonical world tiles.
    *	Only tiles that belong to a region (region_id != NAV_REGION_ID_NONE) and have a
    *	populated sparse cell array can contribute cross-region boundary samples.
    **/
    for ( const int32_t tile_id : sorted_tile_ids ) {
        const nav_tile_t &tile = mesh->world_tiles[ tile_id ];
        const int32_t source_region_id = tile.region_id;

        // Skip tiles that are not assigned to any region.
        if ( source_region_id == NAV_REGION_ID_NONE ) {
            continue;
        }

        /**
        *	Query the sparse cell array for this tile. If the tile has no populated
        *	cells or the representation is inconsistent, skip scanning it.
        **/
        auto cells_view = SVG_Nav_Tile_GetCells( mesh, &mesh->world_tiles[ tile_id ] );
        nav_xy_cell_t * cells_ptr = cells_view.first;
        const int32_t cells_count = cells_view.second;
        if ( !cells_ptr || cells_count != cells_per_tile || !tile.presence_bits ) {
            // No navigable cells in this canonical tile or inconsistent representation.
            continue;
        }

        /**
        *	Inspect every populated cell slot in the tile and examine each layer's persisted edges.
        *	We merge any traversable cross-tile edge that connects to a tile in a different region.
        **/
        for ( int32_t cell_index = 0; cell_index < cells_count; cell_index++ ) {
            // Check sparse presence bit for this cell slot.
            const int32_t word_index = cell_index >> 5;
            const int32_t bit_index = cell_index & 31;
            if ( ( tile.presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
                // Empty sparse slot -> skip.
                continue;
            }

            nav_xy_cell_t &cell = cells_ptr[ cell_index ];

            /**
            *	Get layer array for this cell. If there are no layers, the cell cannot
            *	provide edge connectivity samples.
            **/
            auto layers_view = SVG_Nav_Cell_GetLayers( &cell );
            nav_layer_t * layers_ptr = layers_view.first;
            const int32_t layer_count = layers_view.second;
            if ( !layers_ptr || layer_count <= 0 ) {
                continue;
            }

            // Compute local XY coordinates inside the tile for this cell index.
            // These are used to resolve neighbor deltas that step outside tile bounds.
            const int32_t local_cell_x = cell_index % mesh->tile_size;
            const int32_t local_cell_y = cell_index / mesh->tile_size;

            // Iterate every layer in this cell.
            for ( int32_t layer_index = 0; layer_index < layer_count; layer_index++ ) {
                const nav_layer_t &layer = layers_ptr[ layer_index ];

                /**
                *	Iterate every possible persisted edge direction for this layer.
                *	Skip directions that are not valid according to the layer's mask.
                **/
                for ( int32_t edge_dir_index = 0; edge_dir_index < NAV_LAYER_EDGE_DIR_COUNT; edge_dir_index++ ) {
                    if ( ( layer.edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
                        // Edge slot not populated/valid for this layer.
                        continue;
                    }

                    // Promote only conservative bidirectional static connectivity edges.
                    const uint32_t edge_bits = layer.edge_feature_bits[ edge_dir_index ];
                    if ( !Nav_Hierarchy_EdgeAllowsStaticRegionPass( edge_bits ) ) {
                        // Edge represents walk-off-only, blocked, or jump-obstructed -> skip.
                        continue;
                    }

                    // Decode the persisted edge slot into a local XY delta; skip invalid indexes.
                    int32_t cell_dx = 0;
                    int32_t cell_dy = 0;
                    if ( !Nav_Hierarchy_EdgeDirDelta( edge_dir_index, &cell_dx, &cell_dy ) ) {
                        continue;
                    }

                    /**
                    *	Resolve the target local coordinates and determine the canonical tile
                    *	coordinates for the target cell. We only care about true cross-tile edges
                    *	for portal extraction; intra-tile edges do not affect the tile-level graph.
                    **/
                    const int32_t target_local_x = local_cell_x + cell_dx;
                    const int32_t target_local_y = local_cell_y + cell_dy;

                    // Translate target local overshoot into a neighbor tile offset (-1,0,1).
                    const int32_t target_tile_x = tile.tile_x + ( target_local_x < 0 ? -1 : ( target_local_x >= mesh->tile_size ? 1 : 0 ) );
                    const int32_t target_tile_y = tile.tile_y + ( target_local_y < 0 ? -1 : ( target_local_y >= mesh->tile_size ? 1 : 0 ) );

                    // If the edge stays inside the same canonical tile, skip it.
                    if ( target_tile_x == tile.tile_x && target_tile_y == tile.tile_y ) {
                        continue;
                    }

                    // Look up the canonical id of the neighboring tile.
                    const int32_t neighbor_tile_id = Nav_Hierarchy_FindWorldTileId( mesh, target_tile_x, target_tile_y );
                    if ( neighbor_tile_id < 0 || neighbor_tile_id == tile_id ) {
                        // Neighbor tile is absent or equals this tile -> skip.
                        continue;
                    }

                    // Fetch the neighbor tile and its region id.
                    const nav_tile_t &neighbor_tile = mesh->world_tiles[ neighbor_tile_id ];
                    const int32_t target_region_id = neighbor_tile.region_id;
                    if ( target_region_id == NAV_REGION_ID_NONE || target_region_id == source_region_id ) {
                        // Neighbor belongs to no region or to the same region -> skip.
                        continue;
                    }

                    /**
                    *	We have a cross-region boundary sample. Normalize the region pair ordering
                    *	so the merged portal is keyed deterministically (lower id first).
                    **/
                    const int32_t region_a = std::min( source_region_id, target_region_id );
                    const int32_t region_b = std::max( source_region_id, target_region_id );
                    const uint64_t portal_key = Nav_Hierarchy_MakeRegionPairKey( region_a, region_b );

                    // Find or create an accumulator for this region pair.
                    auto accumulator_it = accumulator_index_of.find( portal_key );
                    int32_t accumulator_index = -1;
                    if ( accumulator_it == accumulator_index_of.end() ) {
                        // New accumulator: index is current vector size.
                        accumulator_index = ( int32_t )accumulators.size();
                        accumulator_index_of.emplace( portal_key, accumulator_index );
                        // Push a default-initialized accumulator with region endpoints set.
                        accumulators.push_back( nav_portal_accumulator_t{ .region_a = region_a, .region_b = region_b } );
                    } else {
                        accumulator_index = accumulator_it->second;
                    }

                    nav_portal_accumulator_t &accumulator = accumulators[ accumulator_index ];

                    /**
                    *	Record deterministic anchor tiles for the portal endpoints. We set a single
                    *	anchor tile (one per region) when the accumulator is first populated so
                    *	later consumers have a stable tile reference for the portal endpoints.
                    **/
                    if ( accumulator.tile_id_a < 0 || accumulator.tile_id_b < 0 ) {
                        if ( source_region_id == region_a ) {
                            accumulator.tile_id_a = tile_id;
                            accumulator.tile_id_b = neighbor_tile_id;
                        } else {
                            accumulator.tile_id_a = neighbor_tile_id;
                            accumulator.tile_id_b = tile_id;
                        }
                    }

                    /**
                    *	Accumulate representative geometry and a coarse traversal cost estimate.
                    *	We use the node world position for this sample as a representative point
                    *	and add the 2D euclidean cell delta scaled by tile_world_size to the
                    *	traversal cost sum. The final portal traversal cost will be averaged.
                    **/
                    const Vector3 sample_point = Nav_NodeWorldPosition( mesh, &tile, cell_index, &layer );
                    accumulator.representative_sum_x += sample_point.x;
                    accumulator.representative_sum_y += sample_point.y;
                    accumulator.representative_sum_z += sample_point.z;

                    // Add a coarse distance estimate: sqrt(dx^2 + dy^2) * world tile size.
                    accumulator.traversal_cost_sum += std::sqrt( ( double )( cell_dx * cell_dx ) + ( double )( cell_dy * cell_dy ) ) * tile_world_size;

                    // Increment sample count so we can compute averages later.
                    accumulator.sample_count++;

                    // Merge coarse compatibility hints from both tiles.
                    accumulator.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( tile );
                    accumulator.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( neighbor_tile );

                    // Promote compatibility into portal support flags for stairs/ladders/water.
                    if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_STAIR ) != 0 ) {
                        accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_STAIRS;
                    }
                    if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_LADDER ) != 0 ) {
                        accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_LADDER;
                    }
                    if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_WATER ) != 0 ) {
                        accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_WATER;
                    }

                    // Track the number of fine-grained boundary samples observed (debug only).
                    mesh->hierarchy.debug_boundary_link_count++;
                }
            }
        }
    }

    /**
    *	Flatten accumulators into stable portal records and construct per-region portal lists.
    *	Each accumulator becomes one `nav_portal_t`. We then collect portal ids per-region
    *	and store contiguous ranges in `mesh->hierarchy.region_portal_refs`.
    **/
    std::vector<std::vector<int32_t>> region_portal_lists( mesh->hierarchy.regions.size() );

    // Reserve final portal storage capacity to avoid repeated reallocations when pushing portals.
    mesh->hierarchy.portals.reserve( accumulators.size() );

    // Convert accumulators to persistent `nav_portal_t` records.
    for ( const nav_portal_accumulator_t &accumulator : accumulators ) {
        nav_portal_t portal = {};

        // Stable id: index into the persistent vector.
        portal.id = ( int32_t )mesh->hierarchy.portals.size();

        // Copy aggregated region endpoints, flags and compatibility hints.
        portal.region_a = accumulator.region_a;
        portal.region_b = accumulator.region_b;
        portal.flags = accumulator.flags;
        portal.compatibility_bits = accumulator.compatibility_bits;

        // Deterministic anchor tiles for the portal endpoints.
        portal.tile_id_a = accumulator.tile_id_a;
        portal.tile_id_b = accumulator.tile_id_b;

        /**
        *	Compute representative geometry (centroid of sample points) and averaged traversal cost.
        *	If no samples were collected (should be rare), fall back to a conservative default.
        **/
        if ( accumulator.sample_count > 0 ) {
            portal.representative_point = Vector3{
                ( float )( accumulator.representative_sum_x / ( double )accumulator.sample_count ),
                ( float )( accumulator.representative_sum_y / ( double )accumulator.sample_count ),
                ( float )( accumulator.representative_sum_z / ( double )accumulator.sample_count )
            };

            portal.traversal_cost = accumulator.traversal_cost_sum / ( double )accumulator.sample_count;
        } else {
            // Conservative default cost if no contributing samples exist.
            portal.traversal_cost = tile_world_size;
        }

        // Append portal to the persistent portal list and register its id in both owning-region lists.
        mesh->hierarchy.portals.push_back( portal );
        region_portal_lists[ portal.region_a ].push_back( portal.id );
        region_portal_lists[ portal.region_b ].push_back( portal.id );
    }

    /**
    *	Populate each region's coarse portal-reference range for future region-level routing.
    *	Portal id lists are sorted and deduplicated to provide stable ordering across builds.
    **/
    for ( nav_region_t &region : mesh->hierarchy.regions ) {
        // Grab the collected portal ids for this region.
        std::vector<int32_t> &portal_ids = region_portal_lists[ region.id ];

        // Deterministic ordering: sort by portal id so downstream consumers see a stable layout.
        std::sort( portal_ids.begin(), portal_ids.end() );

        // Remove duplicates produced by multiple boundary samples or symmetric insertion.
        portal_ids.erase( std::unique( portal_ids.begin(), portal_ids.end() ), portal_ids.end() );

        // Record the range start and count into the region and append ids to the global flat array.
        region.first_portal_ref = ( int32_t )mesh->hierarchy.region_portal_refs.size();
        region.num_portal_refs = ( int32_t )portal_ids.size();
        mesh->hierarchy.region_portal_refs.insert( mesh->hierarchy.region_portal_refs.end(), portal_ids.begin(), portal_ids.end() );
    }

    // Update debug counters and initialize local overlay state to match the new portal count.
    mesh->hierarchy.debug_portal_count = ( int32_t )mesh->hierarchy.portals.size();
    mesh->hierarchy.portal_overlays.assign( mesh->hierarchy.portals.size(), nav_portal_overlay_t{} );
    mesh->hierarchy.portal_overlay_serial = 0;
}

/**
*	@brief	Validate the merged portal graph produced from region boundaries.
*	@param	mesh	Navigation mesh containing freshly built portal records.
*	@return	True when the portal graph references remain internally consistent.
*	@note	This function performs several sanity checks:
*			- Ensures `mesh` is valid.
*			- Ensures every `nav_portal_t` has valid and distinct region endpoints.
*			- Detects duplicate merged portals for the same unordered region pair.
*			- Verifies each portal ID appears in both owning regions' portal reference ranges.
**/
bool Nav_Hierarchy_ValidatePortalGraph( const nav_mesh_t * mesh ) {
    /**
    *	Sanity: require a mesh before validating any portal graph references.
    **/
    if ( !mesh ) {
        return false;
    }

    /**
    *	Reject invalid portal endpoints and duplicate region pairs before checking region-reference symmetry.
    *	We build a temporary map from the unordered region-pair key to the first-seen portal id so we can
    *	detect duplicates deterministically. This prevents symmetric or repeated portal insertions from
    *	creating multiple merged portals for the same pair of regions.
    **/
    std::unordered_map<uint64_t, int32_t> portal_pair_seen;
    for ( const nav_portal_t &portal : mesh->hierarchy.portals ) {
        // Validate region endpoint indices and ensure the portal does not connect a region to itself.
        if ( portal.region_a < 0 || portal.region_b < 0 || portal.region_a >= ( int32_t )mesh->hierarchy.regions.size() || portal.region_b >= ( int32_t )mesh->hierarchy.regions.size() || portal.region_a == portal.region_b ) {
            SVG_Nav_Log( "[WARNING][NavHierarchy] Portal %d has invalid region endpoints (%d,%d).\n", portal.id, portal.region_a, portal.region_b );
            return false;
        }

        // Normalize unordered pair into a stable 64-bit key and check for duplicates.
        const uint64_t key = Nav_Hierarchy_MakeRegionPairKey( portal.region_a, portal.region_b );
        if ( portal_pair_seen.find( key ) != portal_pair_seen.end() ) {
            SVG_Nav_Log( "[WARNING][NavHierarchy] Duplicate merged portal detected for region pair (%d,%d).\n", portal.region_a, portal.region_b );
            return false;
        }
        portal_pair_seen.emplace( key, portal.id );
    }

    /**
    *	Verify that every portal appears in both owning region reference ranges.
    *	For each merged portal we scan the owning regions' portal-ref ranges to ensure
    *	the portal id is present in both lists. This enforces symmetric references which
    *	are required by higher-level routing code that expects each portal to be reachable
    *	from either side's region list.
    **/
    for ( const nav_portal_t &portal : mesh->hierarchy.portals ) {
        bool found_in_a = false;
        bool found_in_b = false;

        // Acquire references to the owning regions for readability.
        const nav_region_t &region_a = mesh->hierarchy.regions[ portal.region_a ];
        const nav_region_t &region_b = mesh->hierarchy.regions[ portal.region_b ];

        // Scan region A's portal range for this portal id.
        for ( int32_t portal_ref_index = 0; portal_ref_index < region_a.num_portal_refs; portal_ref_index++ ) {
            if ( mesh->hierarchy.region_portal_refs[ region_a.first_portal_ref + portal_ref_index ] == portal.id ) {
                found_in_a = true;
                break;
            }
        }

        // Scan region B's portal range for this portal id.
        for ( int32_t portal_ref_index = 0; portal_ref_index < region_b.num_portal_refs; portal_ref_index++ ) {
            if ( mesh->hierarchy.region_portal_refs[ region_b.first_portal_ref + portal_ref_index ] == portal.id ) {
                found_in_b = true;
                break;
            }
        }

        // If either side does not reference the portal, the graph is inconsistent.
        if ( !found_in_a || !found_in_b ) {
            SVG_Nav_Log( "[WARNING][NavHierarchy] Portal %d missing symmetric region references (a=%d b=%d).\n", portal.id, portal.region_a, portal.region_b );
            return false;
        }
    }

    // All checks passed -> portal graph appears consistent.
    return true;
}
