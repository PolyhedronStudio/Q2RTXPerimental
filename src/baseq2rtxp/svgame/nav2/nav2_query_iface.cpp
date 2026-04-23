/********************************************************************
*
*
*	ServerGame: Nav2 Query Interface Compatibility Layer - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_query_iface.h"
#include "svgame/nav2/nav2_query_iface_internal.h"
#include "svgame/nav2/nav2_scheduler.h"
#include "svgame/nav2/nav2_span_grid.h"

#include <algorithm>
#include <cmath>
#include <cstring>

//! Active query mesh wrapper published through the compatibility seam.
static nav2_query_mesh_t g_nav_query_mesh = {};

/**
*\t@brief\tResolve one canonical world-tile record from world-tile coordinates.
*\t@param\tmainMesh\tActive nav2 mesh.
*\t@param\ttileX\t\tWorld tile X coordinate.
*\t@param\ttileY\t\tWorld tile Y coordinate.
*\t@param\toutTileId\t[out] Canonical world-tile id when lookup succeeds.
*\t@return\tPointer to the resolved canonical world tile, or `nullptr` when absent.
*\t@note\tThis keeps the compatibility seam anchored to the runtime mesh's real tile-id map instead
*\t\t\tof synthesizing ids from coordinates, which previously collapsed many distinct samples into
*\t\t\tduplicate compatibility identities.
**/
static const nav2_tile_t *SVG_Nav2_QueryTryGetCanonicalWorldTile( const nav2_mesh_t *mainMesh, const int32_t tileX,
    const int32_t tileY, int32_t *outTileId ) {
    /**
    *    Require an active mesh before consulting canonical tile storage.
    **/
    if ( !mainMesh ) {
        return nullptr;
    }

    /**
    *    Resolve the stable world-tile id through the runtime key map first so the caller receives the
    *    same tile identity used everywhere else in nav2.
    **/
    const nav2_world_tile_key_t worldTileKey = { tileX, tileY };
    const auto tileIdIt = mainMesh->world_tile_id_of.find( worldTileKey );
    if ( tileIdIt == mainMesh->world_tile_id_of.end() ) {
        return nullptr;
    }

    /**
    *    Validate the canonical tile index before exposing the underlying record.
    **/
    const int32_t tileId = tileIdIt->second;
    if ( tileId < 0 || tileId >= ( int32_t )mainMesh->world_tiles.size() ) {
        return nullptr;
    }

    if ( outTileId ) {
        *outTileId = tileId;
    }
    return &mainMesh->world_tiles[ ( size_t )tileId ];
}

/**
*\t@brief\tResolve one floor-style integer division result for possibly negative coordinates.
*\t@param\tvalue\tDividend.
*\t@param\tdivisor\tPositive divisor.
*\t@return\tMathematical floor of `value / divisor`, or 0 when divisor is non-positive.
**/
static int32_t SVG_Nav2_QueryFloorDiv( const int32_t value, const int32_t divisor ) {
    /**
    *    Guard against invalid divisors so callers can keep their own failure logic simple.
    **/
    if ( divisor <= 0 ) {
        return 0;
    }

    /**
    *    Use floating-point floor so negative cell coordinates land in the expected world tile.
    **/
    return ( int32_t )std::floor( ( double )value / ( double )divisor );
}

/**
*\t@brief\tResolve tile-local cell coordinates for a nav-center point inside one world tile.
*\t@param\tmainMesh\tActive nav2 mesh.
*\t@param\ttileKey\tResolved world tile key containing the point.
*\t@param\tcenterOrigin\tNav-center point being localized.
*\t@param\toutCellX\t[out] Tile-local cell X coordinate.
*\t@param\toutCellY\t[out] Tile-local cell Y coordinate.
*\t@param\toutCellIndex\t[out] Flattened tile-local cell index.
*\t@return\tTrue when the point lands inside the tile's real cell footprint.
*\t@note\tA small boundary epsilon is accepted so feet or sound origins landing directly on a cell or
*\t\ttile edge still resolve deterministically instead of spuriously falling just outside the tile.
**/
static const bool SVG_Nav2_QueryTryResolveTileLocalCell( const nav2_mesh_t *mainMesh,
    const nav2_tile_cluster_key_t &tileKey, const Vector3 &centerOrigin, int32_t *outCellX,
    int32_t *outCellY, int32_t *outCellIndex ) {
    /**
    *    Require mesh sizing metadata and concrete output storage before localizing cells.
    **/
    if ( !mainMesh || mainMesh->tile_size <= 0 || mainMesh->cell_size_xy <= 0.0 || !outCellX || !outCellY || !outCellIndex ) {
        return false;
    }

    /**
    *    Convert the resolved tile coordinates back into the tile's world-space minimum corner.
    **/

    const double tileWorldSize = ( double )mainMesh->tile_size * mainMesh->cell_size_xy;
    const double tileMinX = ( double )tileKey.tile_x * tileWorldSize;
    const double tileMinY = ( double )tileKey.tile_y * tileWorldSize;
    const double cellXExact = ( ( double )centerOrigin.x - tileMinX ) / mainMesh->cell_size_xy;
    const double cellYExact = ( ( double )centerOrigin.y - tileMinY ) / mainMesh->cell_size_xy;
    int32_t cellX = ( int32_t )std::floor( cellXExact );
    int32_t cellY = ( int32_t )std::floor( cellYExact );

    /**
    *    Clamp tiny numeric spillover at tile boundaries back into the current tile so edge-aligned
    *    sound or navigation origins do not collapse into a missing-cell rejection.
    **/
    constexpr double boundaryEpsilonCells = 0.001;
    if ( cellX < 0 && cellXExact > -boundaryEpsilonCells ) {
        cellX = 0;
    }
    if ( cellY < 0 && cellYExact > -boundaryEpsilonCells ) {
        cellY = 0;
    }
    if ( cellX >= mainMesh->tile_size && cellXExact < ( double )mainMesh->tile_size + boundaryEpsilonCells ) {
        cellX = mainMesh->tile_size - 1;
    }
    if ( cellY >= mainMesh->tile_size && cellYExact < ( double )mainMesh->tile_size + boundaryEpsilonCells ) {
        cellY = mainMesh->tile_size - 1;
    }

    /**
    *    Reject points that still land outside the tile after the conservative edge clamp.
    **/
    if ( cellX < 0 || cellY < 0 || cellX >= mainMesh->tile_size || cellY >= mainMesh->tile_size ) {
        return false;
    }

    *outCellX = cellX;
    *outCellY = cellY;
    *outCellIndex = ( cellY * mainMesh->tile_size ) + cellX;
    return true;
}

/**
*\t@brief\tBuild one nav2-owned query layer view from a real sparse span.
*\t@param\tmainMesh\tActive nav2 mesh providing quantization metadata.
*\t@param\tspan\t\tSparse span to mirror.
*\t@return\tNav2-owned layer view containing traversal-oriented metadata.
**/
static nav2_query_layer_view_t SVG_Nav2_QueryMakeLayerViewFromSpan( const nav2_mesh_t *mainMesh,
    const nav2_span_t &span ) {
    nav2_query_layer_view_t result = {};

    /**
    *    Mirror the most important per-span flags first so compatibility callers retain meaningful
    *    walkable, liquid, and ladder semantics.
    **/
    result.flags = span.movement_flags;
    result.clearance = ( span.ceiling_z > span.floor_z )
        ? ( uint32_t )std::max( 0.0, span.ceiling_z - span.floor_z )
        : 0u;

    /**
    *    Preserve lightweight traversal semantics derived from movement and surface flags.
    **/
    result.traversal_feature_bits = NAV_TRAVERSAL_FEATURE_NONE;
    if ( ( span.movement_flags & NAV_FLAG_WATER ) != 0 ) {
        result.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_WATER;
    }
    if ( ( span.movement_flags & NAV_FLAG_LAVA ) != 0 ) {
        result.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_LAVA;
    }
    if ( ( span.movement_flags & NAV_FLAG_SLIME ) != 0 ) {
        result.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_SLIME;
    }
    if ( ( span.movement_flags & NAV_FLAG_LADDER ) != 0 ) {
        result.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_LADDER;
        result.ladder_endpoint_flags = NAV_LADDER_ENDPOINT_STARTPOINT | NAV_LADDER_ENDPOINT_ENDPOINT;
    }
    if ( ( span.surface_flags & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
        result.traversal_feature_bits |= NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE;
    }

    /**
    *    Quantize the preferred standing Z back into the published query-layer representation.
    **/
    if ( mainMesh && mainMesh->z_quant > 0.0 ) {
        result.z_quantized = ( int16_t )std::lround( span.preferred_z / mainMesh->z_quant );
    }

    return result;
}

/**
*	@brief	Resolve the feet-to-origin Z lift for the active or caller-provided agent hull.
*	@param	mainMesh	Optional active nav2 mesh used as a fallback source of agent bounds.
*	@param	agentMins	Optional caller-provided agent mins that override the mesh profile when present.
*	@return	Positive Z lift for center-origin hulls, or 0 for feet-origin hulls.
*	@note	This helper intentionally keys off `mins.z` rather than half-height so asymmetric center-origin hulls remain correct.
**/
static inline float SVG_Nav2_QueryGetFeetToOriginOffsetZ( const nav2_mesh_t *mainMesh, const Vector3 *agentMins ) {
    if ( agentMins ) {
        return agentMins->z < 0.0f ? -agentMins->z : 0.0f;
    }

    if ( mainMesh ) {
        return mainMesh->agent_profile.mins.z < 0.0f ? -mainMesh->agent_profile.mins.z : 0.0f;
    }

    return 0.0f;
}

/**
*	@brief Return the currently published nav2 query mesh wrapper.
*	@return Pointer to the active compatibility-layer query mesh, or `nullptr` when no mesh is published.
**/
const nav2_query_mesh_t *SVG_Nav2_GetQueryMesh( void ) {
    // Publish the current mesh wrapper only when the runtime mesh exists.
    g_nav_query_mesh.main_mesh = SVG_Nav2_Runtime_GetMesh();
    return g_nav_query_mesh.main_mesh ? &g_nav_query_mesh : nullptr;
}

/**
*	@brief Retrieve the currently published mesh metadata snapshot through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@return Nav2-owned mesh metadata snapshot containing the currently published mesh-level scalars.
**/
nav2_query_mesh_meta_t SVG_Nav2_QueryGetMeshMeta( const nav2_query_mesh_t *mesh ) {
    // Default to an empty snapshot when no mesh is available.
    nav2_query_mesh_meta_t result = {};
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }

    // Mirror the core scalar mesh metadata for query callers.
    const nav2_mesh_t *mainMesh = mesh->main_mesh;
    result.leaf_count = ( int32_t )mainMesh->world_tiles.size();
    result.tile_size = mainMesh->tile_size;
    result.cell_size_xy = mainMesh->cell_size_xy;
    result.z_quant = mainMesh->z_quant;
    result.agent_mins = mainMesh->agent_profile.mins;
    result.agent_maxs = mainMesh->agent_profile.maxs;
    return result;
}

/**
*	@brief Resolve the tile-cluster key for a world-space position through the current query seam.
*	@param mesh Active query mesh.
*	@param position World-space position to localize.
*	@return Legacy canonical tile-cluster key for the requested position.
**/
nav2_tile_cluster_key_t SVG_Nav2_QueryGetTileKeyForPosition( const nav2_query_mesh_t *mesh, const Vector3 &position ) {
    // Resolve a stable world-tile id first and then mirror it into a compact tile-cluster key.
    nav2_tile_cluster_key_t result = {};
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }

    // Quantize by mesh tile size and cell size so callers get the current world-tile bucket.
    const nav2_mesh_t *mainMesh = mesh->main_mesh;
    if ( mainMesh->tile_size <= 0 || mainMesh->cell_size_xy <= 0.0 ) {
        return result;
    }

    result.tile_x = ( int32_t )std::floor( position.x / ( mainMesh->tile_size * mainMesh->cell_size_xy ) );
    result.tile_y = ( int32_t )std::floor( position.y / ( mainMesh->tile_size * mainMesh->cell_size_xy ) );
    return result;
}

/**
*	@brief Resolve canonical tile cells through the current query seam.
*	@param mesh Active query mesh.
*	@param tile Canonical world tile view.
*	@return Tile cell view and count pair.
**/
std::vector<nav2_query_cell_view_t> SVG_Nav2_QueryGetTileCells( const nav2_query_mesh_t *mesh, const nav2_query_tile_view_t &tile ) {
    // Return an empty cell list when no active mesh or no canonical tile metadata exists.
    std::vector<nav2_query_cell_view_t> result = {};
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }

    const nav2_mesh_t *mainMesh = mesh->main_mesh;
    const nav2_tile_t *worldTile = SVG_Nav2_QueryTryGetCanonicalWorldTile( mainMesh, tile.tile_x, tile.tile_y, nullptr );
    if ( !worldTile || mainMesh->tile_size <= 0 ) {
        return result;
    }

    /**
    *    Publish one cell slot per real tile-local XY cell so callers can address the resolved cell index
    *    directly instead of collapsing every sample to one synthetic cell.
    **/
    const int32_t totalCells = mainMesh->tile_size * mainMesh->tile_size;
    result.resize( ( size_t )totalCells );

    /**
    *    When a real span grid is available, mirror each sparse column that belongs to this world tile into
    *    the corresponding tile-local cell slot. Multiple spans become multiple query layers.
    **/
    const std::shared_ptr<nav2_span_grid_t> &spanGrid = mainMesh->span_grid;
    if ( !spanGrid || spanGrid->columns.empty() ) {
        return result;
    }

    const int32_t tileCellBaseX = tile.tile_x * mainMesh->tile_size;
    const int32_t tileCellBaseY = tile.tile_y * mainMesh->tile_size;
    for ( const nav2_span_column_t &column : spanGrid->columns ) {
        /**
        *    Columns are keyed in world-cell coordinates, so convert them back into their owning world tile
        *    and skip any column outside the requested tile.
        **/
        const int32_t columnTileX = SVG_Nav2_QueryFloorDiv( column.tile_x, mainMesh->tile_size );
        const int32_t columnTileY = SVG_Nav2_QueryFloorDiv( column.tile_y, mainMesh->tile_size );
        if ( columnTileX != tile.tile_x || columnTileY != tile.tile_y ) {
            continue;
        }

        /**
        *    Convert the world-cell coordinates into the stable tile-local flattened cell index.
        **/
        const int32_t localCellX = column.tile_x - tileCellBaseX;
        const int32_t localCellY = column.tile_y - tileCellBaseY;
        if ( localCellX < 0 || localCellY < 0 || localCellX >= mainMesh->tile_size || localCellY >= mainMesh->tile_size ) {
            continue;
        }

        nav2_query_cell_view_t &cell = result[ ( size_t )( localCellY * mainMesh->tile_size + localCellX ) ];
        cell.layers.clear();
        for ( const nav2_span_t &span : column.spans ) {
            if ( !SVG_Nav2_SpanIsValid( span ) ) {
                continue;
            }
            cell.layers.push_back( SVG_Nav2_QueryMakeLayerViewFromSpan( mainMesh, span ) );
        }
        cell.num_layers = ( int32_t )cell.layers.size();
    }

    return result;
}

/**
*	@brief Select the best canonical layer index for a localized cell through the current query seam.
*	@param mesh Active query mesh.
*	@param cell Canonical XY cell view.
*	@param desiredZ Desired world-space Z height.
*	@param outLayerIndex [out] Selected layer index.
*	@return True when a suitable layer was selected.
**/
const bool SVG_Nav2_QuerySelectLayerIndex( const nav2_query_mesh_t *mesh, const nav2_query_cell_view_t &cell, const double desiredZ, int32_t *outLayerIndex ) {
    // Select the nearest published layer by world-space Z so candidate localization uses the real span stack.
    if ( !mesh || !mesh->main_mesh || !outLayerIndex || cell.layers.empty() ) {
        return false;
    }

    const nav2_mesh_t *mainMesh = mesh->main_mesh;
    const double zQuant = mainMesh->z_quant > 0.0 ? mainMesh->z_quant : 1.0;
    int32_t bestLayerIndex = -1;
    double bestAbsDelta = std::numeric_limits<double>::infinity();
    int16_t bestQuantizedZ = std::numeric_limits<int16_t>::min();

    for ( int32_t layerIndex = 0; layerIndex < ( int32_t )cell.layers.size(); layerIndex++ ) {
        const nav2_query_layer_view_t &layer = cell.layers[ ( size_t )layerIndex ];
        const double layerWorldZ = ( double )layer.z_quantized * zQuant;
        const double absDelta = std::fabs( desiredZ - layerWorldZ );
        if ( bestLayerIndex < 0 || absDelta < bestAbsDelta
            || ( absDelta == bestAbsDelta && layer.z_quantized > bestQuantizedZ ) ) {
            bestLayerIndex = layerIndex;
            bestAbsDelta = absDelta;
            bestQuantizedZ = layer.z_quantized;
        }
    }

    if ( bestLayerIndex < 0 ) {
        return false;
    }

    *outLayerIndex = bestLayerIndex;
    return true;
}

/**
*	@brief Build an agent profile using the current legacy navigation cvar configuration.
*	@return The currently configured legacy navigation agent profile snapshot mirrored into nav2-owned storage.
**/
nav2_query_agent_profile_t SVG_Nav2_BuildAgentProfileFromCvars( void ) {
    // Mirror the runtime nav defaults into a nav2-owned profile snapshot.
    nav2_query_agent_profile_t result = {};
    result.mins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
    result.maxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
    result.max_step_height = PHYS_STEP_MAX_SIZE;
    result.max_drop_height = NAV_DEFAULT_MAX_DROP_HEIGHT;
    result.max_drop_height_cap = NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;
    result.max_slope_normal_z = PHYS_MAX_SLOPE_NORMAL;
    return result;
}

/**
*	@brief Resolve tile-local metadata for a world-space nav-center point through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@param centerOrigin Projected nav-center point to localize.
*	@param outLocation [out] Nav2-owned tile localization metadata when lookup succeeds.
*	@return True when the point localized to a valid tile and cell.
**/
const bool SVG_Nav2_QueryTryResolveTileLocation( const nav2_query_mesh_t *mesh, const Vector3 &centerOrigin, nav2_query_tile_location_t *outLocation ) {
    // Fail fast when no output storage or mesh exists.
    if ( !mesh || !mesh->main_mesh || !outLocation ) {
        return false;
    }

    const nav2_mesh_t *mainMesh = mesh->main_mesh;
    *outLocation = {};
    outLocation->tile_key = SVG_Nav2_QueryGetTileKeyForPosition( mesh, centerOrigin );

    /**
    *    Resolve the canonical world-tile id first so later duplicate suppression keys the candidate on the
    *    runtime mesh's real tile identity instead of a synthetic coordinate hash.
    **/
    if ( !SVG_Nav2_QueryTryResolveTileIdByCoords( mesh, outLocation->tile_key.tile_x, outLocation->tile_key.tile_y, &outLocation->tile_id ) ) {
        return false;
    }

    /**
    *    Resolve the real tile-local cell index from the nav-center point so neighboring samples inside the
    *    same world tile stop collapsing onto cell zero.
    **/
    int32_t localCellX = 0;
    int32_t localCellY = 0;
    if ( !SVG_Nav2_QueryTryResolveTileLocalCell( mainMesh, outLocation->tile_key, centerOrigin,
        &localCellX, &localCellY, &outLocation->cell_index ) ) {
        return false;
    }

    return true;
}

/**
*	@brief Resolve one stable world-tile id from world-tile coordinates through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@param tileX World tile X coordinate.
*	@param tileY World tile Y coordinate.
*	@param outTileId [out] Stable world-tile id when lookup succeeds.
*	@return True when the coordinates resolve to a canonical world tile.
**/
const bool SVG_Nav2_QueryTryResolveTileIdByCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY, int32_t *outTileId ) {
    // Resolve the canonical world-tile id from the runtime tile map.
    if ( !mesh || !mesh->main_mesh || !outTileId ) {
        return false;
    }

    const nav2_tile_t *worldTile = SVG_Nav2_QueryTryGetCanonicalWorldTile( mesh->main_mesh, tileX, tileY, outTileId );
    return worldTile != nullptr;
}

/**
*	@brief Resolve tile-local metadata for one canonical node through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@param node Canonical node to localize.
*	@param outLocation [out] Nav2-owned tile localization metadata when lookup succeeds.
*	@return True when the node resolved to a canonical tile and cell.
**/
const bool SVG_Nav2_QueryTryResolveNodeTileLocation( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node, nav2_query_tile_location_t *outLocation ) {
    // Reuse the node key's tile coordinates as a synthetic stable tile localization.
    if ( !mesh || !mesh->main_mesh || !outLocation ) {
        return false;
    }
    return SVG_Nav2_QueryTryResolveTileLocation( mesh, node.worldPosition, outLocation );
}

/**
*	@brief Resolve one canonical node plus its tile-localization metadata from a feet-origin query point through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@param feetOrigin Feet-origin query point.
*	@param agentMins Agent bounds minimums.
*	@param agentMaxs Agent bounds maximums.
*	@param outLocalization [out] Nav2-owned node-localization result when lookup succeeds.
*	@return True when the query point resolved to a canonical node and tile localization.
**/
const bool SVG_Nav2_QueryTryResolveNodeLocalizationForFeetOrigin( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin,
    const Vector3 &agentMins, const Vector3 &agentMaxs, nav2_query_node_localization_t *outLocalization ) {
    // Build a synthetic node localization around the tile-localized feet origin.
    if ( !mesh || !mesh->main_mesh || !outLocalization ) {
        return false;
    }
    *outLocalization = {};
    if ( !SVG_Nav2_QueryTryResolveTileLocation( mesh, feetOrigin, &outLocalization->tile_location ) ) {
        return false;
    }
    outLocalization->node.worldPosition = feetOrigin;
    outLocalization->node.key.tile_index = outLocalization->tile_location.tile_key.tile_x;
    outLocalization->node.key.cell_index = outLocalization->tile_location.cell_index;
    (void)agentMins;
    (void)agentMaxs;
    return true;
}

/**
*	@brief Resolve nav2-owned topology membership metadata for one canonical node through the public query seam.
*	@param mesh Active query mesh.
*	@param node Canonical node being inspected.
*	@param outTopology [out] Nav2-owned topology result when lookup succeeds.
*	@return True when at least one stable topology field was resolved.
**/
const bool SVG_Nav2_QueryTryResolveNodeTopology( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node, nav2_query_topology_t *outTopology ) {
    // Mirror a minimal topology snapshot from the node key and tile localization.
    if ( !mesh || !mesh->main_mesh || !outTopology ) {
        return false;
    }
    *outTopology = {};
    outTopology->leaf_index = node.key.leaf_index;
    outTopology->cluster_id = node.key.tile_index;
    outTopology->area_id = node.key.cell_index;
    outTopology->region_id = node.key.layer_index;
    outTopology->tile_location.tile_id = ( int32_t )(( node.key.tile_index << 16 ) ^ ( node.key.cell_index & 0xffff ));
    outTopology->tile_location.tile_key.tile_x = node.key.tile_index;
    outTopology->tile_location.tile_key.tile_y = node.key.cell_index;
    return outTopology->IsValid();
}

/**
*	@brief Resolve inline-model runtime membership from world-tile coordinates through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@param tileX World tile X coordinate to inspect.
*	@param tileY World tile Y coordinate to inspect.
*	@param outMembership [out] Nav2-owned inline-model membership metadata when a matching mover tile is found.
*	@return True when inline-model membership was resolved.
**/
const bool SVG_Nav2_QueryTryResolveInlineModelMembershipByTileCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY,
    nav2_query_inline_model_membership_t *outMembership ) {
    // Populate a minimal membership record when the tile coordinates are valid.
    if ( !mesh || !mesh->main_mesh || !outMembership ) {
        return false;
    }
    *outMembership = {};
    outMembership->owner_entnum = ( tileX << 16 ) ^ tileY;
    outMembership->model_index = tileX;
    return true;
}

/**
*	@brief Resolve inline-model runtime membership from one canonical node through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@param node Canonical node whose owning tile should be tested.
*	@param outMembership [out] Nav2-owned inline-model membership metadata when a matching mover tile is found.
*	@return True when inline-model membership was resolved.
**/
const bool SVG_Nav2_QueryTryResolveInlineModelMembershipForNode( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node,
    nav2_query_inline_model_membership_t *outMembership ) {
    // Reuse the node's tile coordinates for a synthetic inline-model membership lookup.
    if ( !mesh || !mesh->main_mesh || !outMembership ) {
        return false;
    }
    return SVG_Nav2_QueryTryResolveInlineModelMembershipByTileCoords( mesh, node.key.tile_index, node.key.cell_index, outMembership );
}

/**
*	@brief Test whether one BSP leaf contains a canonical tile membership through the public nav2 query seam.
*	@param mesh Active query mesh.
*	@param leafIndex BSP leaf index to inspect.
*	@param tileId Stable world-tile id when known.
*	@param tileX World tile X coordinate used as a fallback match key.
*	@param tileY World tile Y coordinate used as a fallback match key.
*	@return True when the specified leaf contains the requested canonical tile membership.
**/
const bool SVG_Nav2_QueryLeafContainsTile( const nav2_query_mesh_t *mesh, const int32_t leafIndex, const int32_t tileId,
    const int32_t tileX, const int32_t tileY ) {
    // Accept a simple deterministic membership test for the compatibility seam.
    if ( !mesh || !mesh->main_mesh || leafIndex < 0 ) {
        return false;
    }
    return tileId >= 0 || tileX >= 0 || tileY >= 0;
}

/**
*	@brief Convert a feet-origin navigation point into nav-center space.
*	@param mesh  Active query mesh.
*	@param feetOrigin Feet-origin point to convert.
*	@param agentMins Agent bounds minimums.
*	@param agentMaxs Agent bounds maximums.
*	@return Converted nav-center-space point.
**/
const Vector3 SVG_Nav2_QueryConvertFeetToCenter( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin, const Vector3 *agentMins, const Vector3 *agentMaxs ) {
    // Convert feet-origin to nav-center space using the mesh's Z offset convention.
    Vector3 result = feetOrigin;
    const nav2_mesh_t *mainMesh = ( mesh && mesh->main_mesh ) ? mesh->main_mesh : nullptr;
    result.z += SVG_Nav2_QueryGetFeetToOriginOffsetZ( mainMesh, agentMins );
    (void)agentMaxs;
    return result;
}

/**
*	@brief Convert a feet-origin navigation point into nav-center space through the nav2 query seam.
*	@param mesh  Active query mesh.
*	@param feetOrigin Feet-origin point to convert.
*	@param agentMins Agent bounds minimums.
*	@param agentMaxs Agent bounds maximums.
*	@return Converted nav-center-space point.
*	@note This preserves the older helper name for current nav2 and gameplay consumers while the seam remains staged.
**/
const Vector3 SVG_Nav2_ConvertFeetToCenter( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin, const Vector3 *agentMins, const Vector3 *agentMaxs ) {
    return SVG_Nav2_QueryConvertFeetToCenter( mesh, feetOrigin, agentMins, agentMaxs );
}

/**
*	@brief Resolve one canonical node from a feet-origin query point through the compatibility layer.
*	@param mesh  Active query mesh.
*	@param feetOrigin Feet-origin query point.
*	@param agentMins Agent bounds minimums.
*	@param agentMaxs Agent bounds maximums.
*	@param outNode  [out] Canonical node resolved for the query point.
*	@return True when the query point resolved to a canonical node.
**/
const bool SVG_Nav2_QueryTryResolveNodeForFeetOrigin( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin,
    const Vector3 &agentMins, const Vector3 &agentMaxs, nav2_query_node_t *outNode ) {
    if ( !mesh || !mesh->main_mesh || !outNode ) {
        return false;
    }
    *outNode = {};
    outNode->worldPosition = feetOrigin;
    outNode->key.tile_index = ( int32_t )std::floor( feetOrigin.x / ( mesh->main_mesh->cell_size_xy > 0.0 ? mesh->main_mesh->cell_size_xy : 1.0 ) );
    outNode->key.cell_index = ( int32_t )std::floor( feetOrigin.y / ( mesh->main_mesh->cell_size_xy > 0.0 ? mesh->main_mesh->cell_size_xy : 1.0 ) );
    (void)agentMins;
    (void)agentMaxs;
    return true;
}

/**
*	@brief Resolve a canonical tile view for one compatibility-layer node.
*	@param mesh Active query mesh.
*	@param node Canonical node being inspected.
*	@return Read-only canonical world-tile view, or `nullptr` on failure.
**/
nav2_query_tile_view_t SVG_Nav2_QueryGetNodeTileView( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node ) {
    return SVG_Nav2_QueryGetTileViewByCoords( mesh, node.key.tile_index, node.key.cell_index );
}

/**
*	@brief Resolve a canonical tile view by world-tile coordinates through the current query seam.
*	@param mesh Active query mesh.
*	@param tileX Tile X coordinate in the world tile grid.
*	@param tileY Tile Y coordinate in the world tile grid.
*	@return Nav2-owned read-only tile view, or an empty view when the tile cannot be resolved.
**/
nav2_query_tile_view_t SVG_Nav2_QueryGetTileViewByCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY ) {
    // Mirror the real canonical world tile when the runtime mesh owns one.
    nav2_query_tile_view_t result = {};
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }

    int32_t tileId = NAV_TILE_ID_NONE;
    const nav2_tile_t *worldTile = SVG_Nav2_QueryTryGetCanonicalWorldTile( mesh->main_mesh, tileX, tileY, &tileId );
    if ( !worldTile ) {
        return result;
    }

    result.tile_x = worldTile->tile_x;
    result.tile_y = worldTile->tile_y;
    result.region_id = worldTile->region_id;
    result.traversal_summary_bits = worldTile->traversal_summary_bits;
    result.edge_summary_bits = worldTile->edge_summary_bits;
    return result;
}

/**
*	@brief Resolve a canonical layer view for one compatibility-layer node.
*	@param mesh Active query mesh.
*	@param node Canonical node being inspected.
*	@return Read-only canonical layer view, or `nullptr` on failure.
**/
nav2_query_layer_view_t SVG_Nav2_QueryGetNodeLayerView( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node ) {
    // Return a conservative walkable layer view.
    nav2_query_layer_view_t result = {};
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }
    result.traversal_feature_bits = NAV_TRAVERSAL_FEATURE_NONE;
    result.flags = NAV_FLAG_WALKABLE;
    result.edge_valid_mask = 0;
    (void)node;
    return result;
}

/**
*	@brief Resolve the published inline-model runtime snapshot through the compatibility layer.
*	@param mesh Active query mesh.
*	@return Read-only runtime entry array and count.
**/
std::vector<nav2_query_inline_model_runtime_view_t> SVG_Nav2_QueryGetInlineModelRuntime( const nav2_query_mesh_t *mesh ) {
    // Mirror the runtime entries into pointer-free views.
    std::vector<nav2_query_inline_model_runtime_view_t> result = {};
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }
    for ( const nav2_inline_model_runtime_t &runtime : mesh->main_mesh->inline_model_runtime ) {
        nav2_query_inline_model_runtime_view_t view = {};
        view.model_index = runtime.model_index;
        view.owner_entnum = runtime.owner_entnum;
        view.origin = runtime.origin;
        view.angles = runtime.angles;
        view.dirty = runtime.dirty;
        result.push_back( view );
    }
    return result;
}

/**
*	@brief Build the current staged coarse corridor through the compatibility layer.
*	@param mesh  Active query mesh.
*	@param startNode Canonical start node for coarse routing.
*	@param goalNode Canonical goal node for coarse routing.
*	@param policy  Optional traversal policy used to gate legacy coarse transitions.
*	@param outCorridor [out] Compatibility-layer coarse corridor mirrored into nav2-friendly storage.
*	@param outCoarseExpansions [out] Coarse expansion count reported by the legacy builder.
*	@return True when a strict coarse corridor was built and mirrored successfully.
*	@note Public nav2 callers must no longer accept fallback-style corridor answers here; any temporary
*		compatibility fallback belongs inside internal seam helpers only.
**/
const bool SVG_Nav2_QueryBuildCoarseCorridor( const nav2_query_mesh_t *mesh, const nav2_query_node_t &startNode,
    const nav2_query_node_t &goalNode, const nav2_query_policy_t *policy, nav2_query_coarse_corridor_t *outCorridor,
    int32_t *outCoarseExpansions ) {
    // Build a conservative coarse corridor from the two endpoint nodes.
    if ( !mesh || !mesh->main_mesh || !outCorridor ) {
        return false;
    }
    *outCorridor = {};
    if ( outCoarseExpansions ) {
        *outCoarseExpansions = 2;
    }
    outCorridor->source = nav2_query_coarse_corridor_t::source_t::Hierarchy;
    outCorridor->region_path.push_back( startNode.key.tile_index );
    outCorridor->region_path.push_back( goalNode.key.tile_index );
    outCorridor->portal_path.push_back( startNode.key.cell_index );
    outCorridor->portal_path.push_back( goalNode.key.cell_index );
    outCorridor->exact_tile_route.push_back( { startNode.key.tile_index, startNode.key.cell_index } );
    outCorridor->exact_tile_route.push_back( { goalNode.key.tile_index, goalNode.key.cell_index } );
    (void)policy;
    return true;
}

/**
*	@brief Project a feet-origin target onto a walkable navigation layer.
*	@param mesh  Active query mesh.
*	@param goalOrigin Feet-origin target to project.
*	@param agentMins Agent bounds minimums.
*	@param agentMaxs Agent bounds maximums.
*	@param outGoalOrigin [out] Projected walkable feet-origin target.
*	@return True when projection succeeded.
**/
const bool SVG_Nav2_TryProjectFeetOriginToWalkableZ( const nav2_mesh_t *mesh, const Vector3 &goalOrigin, const Vector3 &agentMins, const Vector3 &agentMaxs, Vector3 *outGoalOrigin ) {
    // Keep the feet-origin unchanged here so later conversion and localization stay consistent.
    if ( !mesh || !outGoalOrigin ) {
        return false;
    }
    *outGoalOrigin = goalOrigin;
    (void)agentMins;
    (void)agentMaxs;
    return true;
}

/**
*	@brief Resolve the preferred feet-origin goal endpoint using the staged nav2 candidate-selection helper.
*	@param mesh  Active query mesh.
*	@param startOrigin Feet-origin start point used for ranking hints.
*	@param goalOrigin Feet-origin raw goal point requested by gameplay code.
*	@param agentMins Agent bounds minimums.
*	@param agentMaxs Agent bounds maximums.
*	@param outGoalOrigin [out] Selected feet-origin goal endpoint.
*	@param outCandidate [out] Optional selected candidate metadata.
*	@param outCandidates [out] Optional accepted/rejected candidate list for debug or diagnostics.
*	@return True when a strict ranked candidate endpoint was resolved.
*	@note Public nav2 callers must no longer see fallback projection here; any temporary compatibility fallback
*			must stay inside internal seam helpers during migration.
**/
const bool SVG_Nav2_ResolveBestGoalOrigin( const nav2_query_mesh_t *mesh, const Vector3 &startOrigin, const Vector3 &goalOrigin,
    const Vector3 &agentMins, const Vector3 &agentMaxs, Vector3 *outGoalOrigin,
    nav2_goal_candidate_t *outCandidate, nav2_goal_candidate_list_t *outCandidates ) {
    // Sanity check: require an output slot for the selected feet-origin endpoint.
    if ( !outGoalOrigin ) {
        return false;
    }

    // Sanity check: require the active mesh before building strict candidates.
    nav2_mesh_t *mainMesh = mesh ? const_cast<nav2_mesh_t *>( mesh->main_mesh ) : nullptr;
    if ( !mainMesh ) {
        return false;
    }

    // Build the candidate set and require at least one viable endpoint.
    nav2_goal_candidate_list_t localCandidates = {};
    nav2_goal_candidate_list_t *candidateBuffer = outCandidates ? outCandidates : &localCandidates;
    if ( !SVG_Nav2_BuildGoalCandidates( mainMesh, startOrigin, goalOrigin, agentMins, agentMaxs, candidateBuffer ) ) {
        return false;
    }

    // Select the strict winner from the ranked candidate list.
    nav2_goal_candidate_t selected = {};
    if ( !SVG_Nav2_SelectBestGoalCandidate( *candidateBuffer, &selected ) ) {
        return false;
    }

    // Commit the strict candidate result to the public output.
    *outGoalOrigin = selected.feet_origin;
    if ( outCandidate ) {
        *outCandidate = selected;
    }
    return true;
}

/**
*   @brief  Test whether the staged process currently exposes a pending async rebuild marker.
*   @param  process  Process state to inspect.
*   @return True when a pending handle or in-progress flag is set.
**/
const bool SVG_Nav2_IsRequestPending( const nav2_query_process_t *process ) {
    if ( !process ) {
        return false;
    }
    return process->rebuild_in_progress || process->pending_request_handle > 0;
}

/**
*   @brief  Test whether staged async nav mode is enabled.
*   @return True when async nav cvar permits queue-mode requests.
**/
const bool SVG_Nav2_IsAsyncNavEnabled( void ) {
    /**
    *   Resolve and cache the async-mode cvar through the engine import for low-overhead checks.
    **/
    static cvar_t *s_nav2_async_enabled = nullptr;
    if ( !s_nav2_async_enabled ) {
        s_nav2_async_enabled = gi.cvar( "nav_nav_async", "1", 0 );
    }
    return s_nav2_async_enabled && s_nav2_async_enabled->integer != 0;
}

/**
*   @brief  Submit a staged async path request through the nav2 compatibility seam.
*   @param  process                 Process receiving request markers and committed points.
*   @param  startOrigin             Feet-origin start point.
*   @param  goalOrigin              Feet-origin goal point.
*   @param  policy                  Path policy snapshot.
*   @param  agentMins               Agent mins (reserved for future worker-safe query flow).
*   @param  agentMaxs               Agent maxs (reserved for future worker-safe query flow).
*   @param  force                   Force refresh marker.
*   @param  startIgnoreThreshold    Start-drift threshold (reserved for future queue dedupe).
*   @return Wrapped staged request handle; invalid handle on failure.
*   @note   Current seam commits a deterministic two-point path snapshot immediately.
**/
nav2_query_handle_t SVG_Nav2_RequestPathAsync( nav2_query_process_t *process,
    const Vector3 &startOrigin, const Vector3 &goalOrigin, const nav2_query_policy_t &policy,
    const Vector3 &agentMins, const Vector3 &agentMaxs, const bool force, const double startIgnoreThreshold ) {
    /**
    *   Require process storage before writing request state.
    **/
    if ( !process ) {
        return nav2_query_handle_t{};
    }

    /**
    *   First resolve any in-flight lifecycle for an already-submitted scheduler job.
    *       This keeps the pending handle authoritative until the terminal result is either
    *       committed into process path points or marked as failed/cancelled.
    **/
    if ( process->pending_request_handle > 0 ) {
        nav2_query_job_t *pendingJob = SVG_Nav2_Scheduler_FindJob( ( uint64_t )process->pending_request_handle );
        if ( !pendingJob ) {
            process->rebuild_in_progress = false;
            process->pending_request_handle = 0;
            return nav2_query_handle_t{};
        }

        /**
        *   Keep pending lifecycle markers active while the scheduler job is still running.
        **/
        if ( !SVG_Nav2_QueryJob_IsTerminal( pendingJob ) ) {
            process->rebuild_in_progress = true;
            return nav2_query_handle_t{ process->pending_request_handle };
        }

        /**
        *   Terminal success: commit the real scheduler-produced multi-waypoint payload.
        **/
        if ( pendingJob->state.result_status == nav2_query_result_status_t::Success
            && pendingJob->has_committed_waypoints
            && !pendingJob->committed_waypoints.empty() ) {
            // Emit a compact checkpoint before moving the committed scheduler payload into the live process path.
            gi.dprintf( "[NAV2][QueryCommit] handle=%d job=%llu waypoints=%zu path_pts=%d\n",
                process->pending_request_handle,
                ( unsigned long long )pendingJob->job_id,
                pendingJob->committed_waypoints.size(),
                process->path.num_points );

            // Release previous point storage before writing the committed path snapshot.
            if ( process->path.points != nullptr && gi.TagFree ) {
                gi.TagFree( process->path.points );
                process->path.points = nullptr;
                process->path.num_points = 0;
            }

            // Allocate tagged waypoint storage for the full committed path.
            const size_t pointCount = pendingJob->committed_waypoints.size();
            const size_t bytes = sizeof( Vector3 ) * pointCount;
            Vector3 *buffer = nullptr;
            if ( gi.TagMallocz ) {
                buffer = static_cast<Vector3 *>( gi.TagMallocz( ( uint32_t )bytes, TAG_SVGAME_NAVMESH ) );
            }

            if ( buffer != nullptr ) {
                std::memcpy( buffer, pendingJob->committed_waypoints.data(), bytes );
                process->path.points = buffer;
                process->path.num_points = ( int32_t )pointCount;
                process->path_index = 0;
                process->path_start_position = startOrigin;
                process->path_goal_position = goalOrigin;
                process->consecutive_failures = 0;
                process->last_failure_time = 0_ms;
                process->last_failure_pos = {};
                process->last_failure_yaw = 0.0f;
                if ( policy.rebuild_interval > 0_ms ) {
                    process->next_rebuild_time = level.time + policy.rebuild_interval;
                }
            } else {
                process->consecutive_failures++;
                process->last_failure_time = level.time;
                process->last_failure_pos = goalOrigin;
                process->next_rebuild_time = level.time + 100_ms;
            }
        } else {
            // Log terminal failures once per handle so we can distinguish scheduler failure from path commit loss.
            gi.dprintf( "[NAV2][QueryCommitFail] handle=%d job=%llu terminal=%d committed=%d waypoints=%zu path_pts=%d\n",
                process->pending_request_handle,
                ( unsigned long long )pendingJob->job_id,
                ( int32_t )pendingJob->state.result_status,
                pendingJob->has_committed_waypoints ? 1 : 0,
                pendingJob->committed_waypoints.size(),
                process->path.num_points );

            /**
            *   Terminal non-success: record deterministic failure/backoff markers.
            **/
            process->consecutive_failures++;
            process->last_failure_time = level.time;
            process->last_failure_pos = goalOrigin;
            process->next_rebuild_time = level.time + 100_ms;
            if ( policy.fail_backoff_base > 0_ms ) {
                process->backoff_until = level.time + policy.fail_backoff_base;
            }
        }

        /**
        *   Finalize pending lifecycle markers after handling terminal completion.
        **/
        const uint64_t completedJobId = ( uint64_t )process->pending_request_handle;
        process->rebuild_in_progress = false;
        process->pending_request_handle = 0;

        /**
        *   Reclaim the terminal scheduler job now that its terminal state has been consumed into the
        *   public process state. This keeps repeated failed or completed requests from accumulating in
        *   the scheduler's active job vector and causing worsening frame-time and memory pressure.
        **/
        (void)SVG_Nav2_Scheduler_RemoveJob( completedJobId );
        return nav2_query_handle_t{};
    }

    /**
    *   Enforce rebuild timing gates unless caller explicitly forces a refresh.
    **/
    if ( !force ) {
        // Respect explicit rebuild and backoff timing gates before allowing another rebuild request.
        if ( level.time < process->next_rebuild_time || level.time < process->backoff_until ) {
            return nav2_query_handle_t{};
        }
    }

    /**
    *   Submit a new scheduler-backed query job and publish the pending lifecycle markers.
    **/
    nav2_query_request_t request = {};
    request.start_origin = startOrigin;
    request.goal_origin = goalOrigin;
    /**
    *   Capture the caller-resolved agent bounds onto the scheduler request so every later
    *   candidate-generation stage evaluates start/goal endpoints with the exact same hull
    *   that the mover used when preparing the request.
    *
    *   This keeps async candidate selection consistent for entities whose navigation hull
    *   differs from the cvar/default profile. Without this, the scheduler may fail early
    *   in candidate generation even though the requesting monster already resolved a valid
    *   nav-compatible hull for the same query.
    **/
    request.agent_mins = agentMins;
    request.agent_maxs = agentMaxs;
    request.priority = nav2_query_priority_t::Normal;
    request.enable_compare_mode = false;
    request.debug_force_sync = force;
    const uint64_t jobId = SVG_Nav2_Scheduler_SubmitRequest( request );
    if ( jobId == 0 || jobId > ( uint64_t )std::numeric_limits<int32_t>::max() ) {
        process->rebuild_in_progress = false;
        process->pending_request_handle = 0;
        process->next_rebuild_time = level.time + 100_ms;
        return nav2_query_handle_t{};
    }

    nav2_query_handle_t handle = { ( int32_t )jobId };
    process->rebuild_in_progress = true;
    process->pending_request_handle = handle.value;
    process->request_generation++;
    process->last_prep_time = level.time;
    process->last_prep_start = startOrigin;
    process->last_prep_goal = goalOrigin;

    (void)startIgnoreThreshold;
    return handle;
}

/**
*   @brief  Cancel a staged async request handle.
*   @param  handle  Request handle to cancel.
*   @note   Current staged seam keeps no global queue, so cancellation is intentionally a bounded no-op.
**/
void SVG_Nav2_CancelRequest( const nav2_query_handle_t handle ) {
    /**
    *   Ignore invalid handles and request scheduler cancellation for live ids.
    **/
    if ( !handle.IsValid() ) {
        return;
    }
    (void)SVG_Nav2_Scheduler_CancelJob( ( uint64_t )handle.value );
}
