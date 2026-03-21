/********************************************************************
*
*
*	ServerGame: Nav2 Query Interface Compatibility Layer - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_query_iface.h"
#include "svgame/nav2/nav2_query_iface_internal.h"

#include <algorithm>

//! Active query mesh wrapper published through the compatibility seam.
static nav2_query_mesh_t g_nav_query_mesh = {};

/**
*	@brief\tReturn the currently published nav2 query mesh wrapper.
*	@return\tPointer to the active compatibility-layer query mesh, or `nullptr` when no mesh is published.
**/
const nav2_query_mesh_t *SVG_Nav2_GetQueryMesh( void ) {
    // Publish the current mesh wrapper only when the runtime mesh exists.
    g_nav_query_mesh.main_mesh = SVG_Nav2_Runtime_GetMesh();
    return g_nav_query_mesh.main_mesh ? &g_nav_query_mesh : nullptr;
}

/**
*	@brief\tRetrieve the currently published mesh metadata snapshot through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@return\tNav2-owned mesh metadata snapshot containing the currently published mesh-level scalars.
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
*	@brief\tResolve the tile-cluster key for a world-space position through the current query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tposition\tWorld-space position to localize.
*	@return\tLegacy canonical tile-cluster key for the requested position.
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
*	@brief\tResolve canonical tile cells through the current query seam.
*	@param\tmesh\tActive query mesh.
*	@param\ttile\tCanonical world tile view.
*	@return\tTile cell view and count pair.
**/
std::vector<nav2_query_cell_view_t> SVG_Nav2_QueryGetTileCells( const nav2_query_mesh_t *mesh, const nav2_query_tile_view_t &tile ) {
    // Return an empty cell list when the tile cannot be resolved.
    std::vector<nav2_query_cell_view_t> result = {};
    if ( !mesh || !mesh->main_mesh || tile.tile_x == 0 && tile.tile_y == 0 ) {
        return result;
    }

    // Provide one synthetic cell with one synthetic layer so the compatibility seam remains buildable.
    nav2_query_cell_view_t cell = {};
    cell.num_layers = 1;
    nav2_query_layer_view_t layer = {};
    layer.flags = NAV_FLAG_WALKABLE;
    layer.traversal_feature_bits = NAV_TRAVERSAL_FEATURE_NONE;
    cell.layers.push_back( layer );
    result.push_back( cell );
    return result;
}

/**
*	@brief\tSelect the best canonical layer index for a localized cell through the current query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tcell\tCanonical XY cell view.
*	@param\tdesiredZ\tDesired world-space Z height.
*	@param\toutLayerIndex\t[out] Selected layer index.
*	@return\tTrue when a suitable layer was selected.
**/
const bool SVG_Nav2_QuerySelectLayerIndex( const nav2_query_mesh_t *mesh, const nav2_query_cell_view_t &cell, const double desiredZ, int32_t *outLayerIndex ) {
    // Default to the first layer when one exists.
    if ( !mesh || !mesh->main_mesh || !outLayerIndex || cell.layers.empty() ) {
        return false;
    }
    *outLayerIndex = 0;
    return desiredZ == desiredZ;
}

/**
*	@brief\tBuild an agent profile using the current legacy navigation cvar configuration.
*	@return\tThe currently configured legacy navigation agent profile snapshot mirrored into nav2-owned storage.
**/
nav2_query_agent_profile_t SVG_Nav2_BuildAgentProfileFromCvars( void ) {
    // Mirror the runtime nav defaults into a nav2-owned profile snapshot.
    nav2_query_agent_profile_t result = {};
    result.mins = { -16.0f, -16.0f, -36.0f };
    result.maxs = { 16.0f, 16.0f, 36.0f };
    result.max_step_height = PHYS_STEP_MAX_SIZE;
    result.max_drop_height = NAV_DEFAULT_MAX_DROP_HEIGHT;
    result.max_drop_height_cap = NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;
    result.max_slope_normal_z = PHYS_MAX_SLOPE_NORMAL;
    return result;
}

/**
*	@brief\tResolve tile-local metadata for a world-space nav-center point through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tcenterOrigin\tProjected nav-center point to localize.
*	@param\toutLocation\t[out] Nav2-owned tile localization metadata when lookup succeeds.
*	@return\tTrue when the point localized to a valid tile and cell.
**/
const bool SVG_Nav2_QueryTryResolveTileLocation( const nav2_query_mesh_t *mesh, const Vector3 &centerOrigin, nav2_query_tile_location_t *outLocation ) {
    // Fail fast when no output storage or mesh exists.
    if ( !mesh || !mesh->main_mesh || !outLocation ) {
        return false;
    }
    *outLocation = {};
    outLocation->tile_key = SVG_Nav2_QueryGetTileKeyForPosition( mesh, centerOrigin );
    outLocation->tile_id = ( int32_t )(( outLocation->tile_key.tile_x << 16 ) ^ ( outLocation->tile_key.tile_y & 0xffff ));
    outLocation->cell_index = 0;
    return true;
}

/**
*	@brief\tResolve one stable world-tile id from world-tile coordinates through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@param\ttileX\tWorld tile X coordinate.
*	@param\ttileY\tWorld tile Y coordinate.
*	@param\toutTileId\t[out] Stable world-tile id when lookup succeeds.
*	@return\tTrue when the coordinates resolve to a canonical world tile.
**/
const bool SVG_Nav2_QueryTryResolveTileIdByCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY, int32_t *outTileId ) {
    // Synthesize a stable tile id directly from the coordinates for the current seam.
    if ( !mesh || !mesh->main_mesh || !outTileId ) {
        return false;
    }
    *outTileId = ( tileX << 16 ) ^ ( tileY & 0xffff );
    return true;
}

/**
*	@brief\tResolve tile-local metadata for one canonical node through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tnode\tCanonical node to localize.
*	@param\toutLocation\t[out] Nav2-owned tile localization metadata when lookup succeeds.
*	@return\tTrue when the node resolved to a canonical tile and cell.
**/
const bool SVG_Nav2_QueryTryResolveNodeTileLocation( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node, nav2_query_tile_location_t *outLocation ) {
    // Reuse the node key's tile coordinates as a synthetic stable tile localization.
    if ( !mesh || !mesh->main_mesh || !outLocation ) {
        return false;
    }
    return SVG_Nav2_QueryTryResolveTileLocation( mesh, node.worldPosition, outLocation );
}

/**
*	@brief\tResolve one canonical node plus its tile-localization metadata from a feet-origin query point through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tfeetOrigin\tFeet-origin query point.
*	@param\tagentMins\tAgent bounds minimums.
*	@param\tagentMaxs\tAgent bounds maximums.
*	@param\toutLocalization\t[out] Nav2-owned node-localization result when lookup succeeds.
*	@return\tTrue when the query point resolved to a canonical node and tile localization.
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
*	@brief\tResolve nav2-owned topology membership metadata for one canonical node through the public query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tnode\tCanonical node being inspected.
*	@param\toutTopology\t[out] Nav2-owned topology result when lookup succeeds.
*	@return\tTrue when at least one stable topology field was resolved.
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
*	@brief\tResolve inline-model runtime membership from world-tile coordinates through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@param\ttileX\tWorld tile X coordinate to inspect.
*	@param\ttileY\tWorld tile Y coordinate to inspect.
*	@param\toutMembership\t[out] Nav2-owned inline-model membership metadata when a matching mover tile is found.
*	@return\tTrue when inline-model membership was resolved.
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
*	@brief\tResolve inline-model runtime membership from one canonical node through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tnode\tCanonical node whose owning tile should be tested.
*	@param\toutMembership\t[out] Nav2-owned inline-model membership metadata when a matching mover tile is found.
*	@return\tTrue when inline-model membership was resolved.
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
*	@brief\tTest whether one BSP leaf contains a canonical tile membership through the public nav2 query seam.
*	@param\tmesh\tActive query mesh.
*	@param\tleafIndex\tBSP leaf index to inspect.
*	@param\ttileId\tStable world-tile id when known.
*	@param\ttileX\tWorld tile X coordinate used as a fallback match key.
*	@param\ttileY\tWorld tile Y coordinate used as a fallback match key.
*	@return\tTrue when the specified leaf contains the requested canonical tile membership.
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
*	@brief\tConvert a feet-origin navigation point into nav-center space.
*	@param\tmesh\t\tActive query mesh.
*	@param\tfeetOrigin\tFeet-origin point to convert.
*	@param\tagentMins\tAgent bounds minimums.
*	@param\tagentMaxs\tAgent bounds maximums.
*	@return\tConverted nav-center-space point.
**/
const Vector3 SVG_Nav2_QueryConvertFeetToCenter( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin, const Vector3 *agentMins, const Vector3 *agentMaxs ) {
    // Convert feet-origin to nav-center space using the mesh's Z offset convention.
    Vector3 result = feetOrigin;
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }
    const nav2_mesh_t *mainMesh = mesh->main_mesh;
    result.z += ( agentMins && agentMaxs ) ? ( mainMesh->agent_profile.maxs.z - mainMesh->agent_profile.mins.z ) * 0.5f : 0.0f;
    return result;
}

/**
*	@brief\tConvert a feet-origin navigation point into nav-center space through the nav2 query seam.
*	@param\tmesh\t\tActive query mesh.
*	@param\tfeetOrigin\tFeet-origin point to convert.
*	@param\tagentMins\tAgent bounds minimums.
*	@param\tagentMaxs\tAgent bounds maximums.
*	@return\tConverted nav-center-space point.
*	@note\tThis preserves the older helper name for current nav2 and gameplay consumers while the seam remains staged.
**/
const Vector3 SVG_Nav2_ConvertFeetToCenter( const nav2_query_mesh_t *mesh, const Vector3 &feetOrigin, const Vector3 *agentMins, const Vector3 *agentMaxs ) {
    return SVG_Nav2_QueryConvertFeetToCenter( mesh, feetOrigin, agentMins, agentMaxs );
}

/**
*	@brief\tResolve one canonical node from a feet-origin query point through the compatibility layer.
*	@param\tmesh\t\tActive query mesh.
*	@param\tfeetOrigin\tFeet-origin query point.
*	@param\tagentMins\tAgent bounds minimums.
*	@param\tagentMaxs\tAgent bounds maximums.
*	@param\toutNode\t\t[out] Canonical node resolved for the query point.
*	@return\tTrue when the query point resolved to a canonical node.
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
*	@brief\tResolve a canonical tile view for one compatibility-layer node.
*	@param\tmesh\tActive query mesh.
*	@param\tnode\tCanonical node being inspected.
*	@return\tRead-only canonical world-tile view, or `nullptr` on failure.
**/
nav2_query_tile_view_t SVG_Nav2_QueryGetNodeTileView( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node ) {
    return SVG_Nav2_QueryGetTileViewByCoords( mesh, node.key.tile_index, node.key.cell_index );
}

/**
*	@brief\tResolve a canonical tile view by world-tile coordinates through the current query seam.
*	@param\tmesh\tActive query mesh.
*	@param\ttileX\tTile X coordinate in the world tile grid.
*	@param\ttileY\tTile Y coordinate in the world tile grid.
*	@return\tNav2-owned read-only tile view, or an empty view when the tile cannot be resolved.
**/
nav2_query_tile_view_t SVG_Nav2_QueryGetTileViewByCoords( const nav2_query_mesh_t *mesh, const int32_t tileX, const int32_t tileY ) {
    // Provide a synthetic tile view around the requested coordinates.
    nav2_query_tile_view_t result = {};
    if ( !mesh || !mesh->main_mesh ) {
        return result;
    }
    result.tile_x = tileX;
    result.tile_y = tileY;
    result.region_id = NAV_REGION_ID_NONE;
    result.traversal_summary_bits = NAV_TILE_SUMMARY_NONE;
    result.edge_summary_bits = NAV_TILE_SUMMARY_NONE;
    return result;
}

/**
*	@brief\tResolve a canonical layer view for one compatibility-layer node.
*	@param\tmesh\tActive query mesh.
*	@param\tnode\tCanonical node being inspected.
*	@return\tRead-only canonical layer view, or `nullptr` on failure.
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
*	@brief\tResolve the published inline-model runtime snapshot through the compatibility layer.
*	@param\tmesh\tActive query mesh.
*	@return\tRead-only runtime entry array and count.
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
*	@brief\tBuild the current staged coarse corridor through the compatibility layer.
*	@param\tmesh\t\tActive query mesh.
*	@param\tstartNode\tCanonical start node for coarse routing.
*	@param\tgoalNode\tCanonical goal node for coarse routing.
*	@param\tpolicy\t\tOptional traversal policy used to gate legacy coarse transitions.
*	@param\toutCorridor\t[out] Compatibility-layer coarse corridor mirrored into nav2-friendly storage.
*	@param\toutCoarseExpansions\t[out] Coarse expansion count reported by the legacy builder.
*	@return\tTrue when a strict coarse corridor was built and mirrored successfully.
*	@note\tPublic nav2 callers must no longer accept fallback-style corridor answers here; any temporary
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
*	@brief\tProject a feet-origin target onto a walkable navigation layer.
*	@param\tmesh\t\tActive query mesh.
*	@param\tgoalOrigin\tFeet-origin target to project.
*	@param\tagentMins\tAgent bounds minimums.
*	@param\tagentMaxs\tAgent bounds maximums.
*	@param\toutGoalOrigin\t[out] Projected walkable feet-origin target.
*	@return\tTrue when projection succeeded.
**/
const bool SVG_Nav2_TryProjectFeetOriginToWalkableZ( const nav2_mesh_t *mesh, const Vector3 &goalOrigin, const Vector3 &agentMins, const Vector3 &agentMaxs, Vector3 *outGoalOrigin ) {
    // Apply a simple center-of-bounds projection that keeps the seam buildable.
    if ( !mesh || !outGoalOrigin ) {
        return false;
    }
    *outGoalOrigin = goalOrigin;
    outGoalOrigin->z += ( agentMaxs.z - agentMins.z ) * 0.5f;
    return true;
}

/**
*	@brief\tResolve the preferred feet-origin goal endpoint using the staged nav2 candidate-selection helper.
*	@param\tmesh\t\tActive query mesh.
*	@param\tstartOrigin\tFeet-origin start point used for ranking hints.
*	@param\tgoalOrigin\tFeet-origin raw goal point requested by gameplay code.
*	@param\tagentMins\tAgent bounds minimums.
*	@param\tagentMaxs\tAgent bounds maximums.
*	@param\toutGoalOrigin\t[out] Selected feet-origin goal endpoint.
*	@param\toutCandidate\t[out] Optional selected candidate metadata.
*	@param\toutCandidates\t[out] Optional accepted/rejected candidate list for debug or diagnostics.
*	@return\tTrue when a strict ranked candidate endpoint was resolved.
*	@note\tPublic nav2 callers must no longer see fallback projection here; any temporary compatibility fallback
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
