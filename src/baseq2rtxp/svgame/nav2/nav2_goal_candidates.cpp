/********************************************************************
*
*
*	ServerGame: Nav2 Goal Candidate Endpoint Selection - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_goal_candidates.h"

#include "svgame/nav2/nav2_query_iface.h"
#include "svgame/nav2/nav2_query_iface_internal.h"
#include "svgame/nav2/nav2_topology.h"


/**
*
*
*	Nav2 Goal Candidate Local Helpers:
*
*
**/
/**
*	@brief	Record one rejected goal sample when the fixed-capacity rejection buffer still has room.
*	@param	candidates	Candidate buffer receiving the rejection metadata.
*	@param	reason		Stable rejection taxonomy entry.
*	@param	type		Semantic candidate type attempted for this sample.
*	@param	sampleDx	Relative sample X offset in cells.
*	@param	sampleDy	Relative sample Y offset in cells.
*	@param	tileX		Candidate tile X index when known.
*	@param	tileY		Candidate tile Y index when known.
*	@param	tileId		Candidate canonical tile id when known.
*	@param	cellIndex	Candidate tile-local cell index when known.
*	@param	layerIndex	Candidate layer index when known.
*	@note	This helper stays allocation-free so the staged candidate pass remains cheap and deterministic.
**/
static void SVG_Nav2_RecordGoalCandidateRejection( nav2_goal_candidate_list_t *candidates,
    const nav2_goal_candidate_rejection_t reason, const nav2_goal_candidate_type_t type, const int32_t sampleDx,
    const int32_t sampleDy, const int32_t tileX, const int32_t tileY, const int32_t tileId,
    const int32_t cellIndex, const int32_t layerIndex ) {
    /**
    *    Sanity check: require output storage before touching the rejection buffer.
    **/
    if ( !candidates ) {
        return;
    }

    /**
    *    Keep rejection recording bounded so endpoint selection never allocates or overruns local storage.
    **/
    if ( candidates->rejection_count >= NAV_GOAL_CANDIDATE_REJECTION_MAX_COUNT ) {
        return;
    }

    /**
    *    Append the rejected sample metadata in insertion order for deterministic debugging.
    **/
    nav2_goal_candidate_rejection_info_t &rejection = candidates->rejections[ candidates->rejection_count++ ];
    rejection.reason = reason;
    rejection.type = type;
    rejection.sample_dx = sampleDx;
    rejection.sample_dy = sampleDy;
    rejection.tile_x = tileX;
    rejection.tile_y = tileY;
    rejection.tile_id = tileId;
    rejection.cell_index = cellIndex;
    rejection.layer_index = layerIndex;
}

/**
*	@brief	Resolve BSP leaf, cluster, and area metadata for a world-space point.
*	@param	point		World-space point to classify.
*	@param	outLeafIndex	[out] BSP leaf index when available.
*	@param	outCluster	[out] BSP cluster id when available.
*	@param	outArea		[out] BSP area id when available.
*	@return	True when the world collision model and BSP point-leaf query succeeded.
*	@note	This keeps candidate ranking BSP-aware without requiring the full nav2 hierarchy yet.
**/
static const bool SVG_Nav2_TryResolveGoalLeafMeta( const Vector3 &point, int32_t *outLeafIndex,
    int32_t *outCluster, int32_t *outArea ) {
    /**
    *    Resolve the active collision model before attempting BSP leaf classification.
    **/
    const cm_t *collisionModel = gi.GetCollisionModel ? gi.GetCollisionModel() : nullptr;
    if ( !collisionModel || !collisionModel->cache || !collisionModel->cache->nodes || !collisionModel->cache->leafs ) {
        return false;
    }

    /**
    *    Use the authoritative BSP point-leaf helper to classify the world-space candidate endpoint.
    **/
    const vec_t pointVec[ 3 ] = { point.x, point.y, point.z };
    mleaf_t *leaf = gi.BSP_PointLeaf ? gi.BSP_PointLeaf( collisionModel->cache->nodes, pointVec ) : nullptr;
    if ( !leaf ) {
        return false;
    }

    /**
    *    Translate the returned leaf pointer into stable ids for ranking and future persistence-friendly metadata.
    **/
    if ( outLeafIndex ) {
        *outLeafIndex = ( int32_t )( leaf - collisionModel->cache->leafs );
    }
    if ( outCluster ) {
        *outCluster = leaf->cluster;
    }
    if ( outArea ) {
        *outArea = leaf->area;
    }
    return true;
}

/**
*	@brief	Build lightweight topology-owned semantics for one localized query layer.
*	@param	layer		Localized query layer view.
*	@param	pointContents	Point contents sampled at the projected candidate center.
*	@return	Topology-owned semantic bundle derived from the currently published query-layer metadata.
*	@note	This is an incremental consumption seam: candidate generation starts reading topology-owned
*			semantic fields without yet depending on a fully materialized per-location topology table.
**/
static nav2_topology_semantics_t SVG_Nav2_MakeTopologySemanticsFromGoalLayer( const nav2_query_layer_view_t &layer,
    const cm_contents_t pointContents )
{
    nav2_topology_semantics_t semantics = {};
    semantics.traversal_feature_bits = layer.traversal_feature_bits;
    semantics.ladder_endpoint_flags = layer.ladder_endpoint_flags;

    /**
    *	Normalize topology-owned feature bits from layer traversal semantics.
    **/
    semantics.feature_bits |= NAV2_TOPOLOGY_FEATURE_FLOOR;
    if ( ( layer.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0 ) {
        semantics.feature_bits |= NAV2_TOPOLOGY_FEATURE_STAIR;
        semantics.tile_summary_bits |= NAV_TILE_SUMMARY_STAIR;
    }
    if ( ( layer.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 || layer.ladder_endpoint_flags != NAV_LADDER_ENDPOINT_NONE ) {
        semantics.feature_bits |= NAV2_TOPOLOGY_FEATURE_LADDER;
        semantics.tile_summary_bits |= NAV_TILE_SUMMARY_LADDER;
    }
    if ( ( layer.ladder_endpoint_flags & NAV_LADDER_ENDPOINT_STARTPOINT ) != 0 ) {
        semantics.feature_bits |= NAV2_TOPOLOGY_FEATURE_LADDER_STARTPOINT;
    }
    if ( ( layer.ladder_endpoint_flags & NAV_LADDER_ENDPOINT_ENDPOINT ) != 0 ) {
        semantics.feature_bits |= NAV2_TOPOLOGY_FEATURE_LADDER_ENDPOINT;
    }

    /**
    *	Preserve liquid semantics in topology-owned summary bits for future policy consumers.
    **/
    if ( ( layer.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_WATER ) != 0 ) {
        semantics.tile_summary_bits |= NAV_TILE_SUMMARY_WATER;
    }
    if ( ( layer.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_LAVA ) != 0 ) {
        semantics.tile_summary_bits |= NAV_TILE_SUMMARY_LAVA;
    }
    if ( ( layer.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_SLIME ) != 0 ) {
        semantics.tile_summary_bits |= NAV_TILE_SUMMARY_SLIME;
    }

    /**
    *	Keep solid-origin rejection visible through topology semantics for future diagnostics.
    **/
    if ( ( pointContents & CONTENTS_SOLID ) != 0 ) {
        semantics.portal_barrier = true;
        semantics.feature_bits |= NAV2_TOPOLOGY_FEATURE_PORTAL_BARRIER;
    }

    return semantics;
}

/**
*	@brief	Return whether the candidate list already contains the same canonical tile/cell/layer tuple.
*	@param	candidates	Candidate buffer to inspect.
*	@param	tileId		Canonical tile id.
*	@param	cellIndex	Tile-local cell index.
*	@param	layerIndex	Cell layer index.
*	@return	True when an equivalent candidate already exists.
**/
static const bool SVG_Nav2_HasDuplicateGoalCandidate( const nav2_goal_candidate_list_t &candidates,
    const int32_t tileId, const int32_t cellIndex, const int32_t layerIndex ) {
    /**
    *    Scan the accepted candidate buffer for an exact canonical location match.
    **/
    for ( int32_t i = 0; i < candidates.candidate_count; i++ ) {
        const nav2_goal_candidate_t &candidate = candidates.candidates[ i ];
        if ( candidate.tile_id == tileId && candidate.cell_index == cellIndex && candidate.layer_index == layerIndex ) {
            return true;
        }
    }
    return false;
}

/**
*	@brief	Resolve tile-local metadata for a projected nav-center point.
*	@param	mesh		Navigation mesh used for tile and cell lookup.
*	@param	centerOrigin	Projected nav-center point to localize.
*	@param	outTileKey	[out] Tile key when world-tile lookup succeeds.
*	@param	outTileId	[out] Canonical world-tile id when lookup succeeds.
*	@param	outCellIndex	[out] Tile-local cell index when lookup succeeds.
*	@return	True when the point localized to a valid tile and cell.
**/
static const bool SVG_Nav2_TryResolveGoalTileMeta( const nav2_query_mesh_t *mesh, const Vector3 &centerOrigin,
    nav2_tile_cluster_key_t *outTileKey, int32_t *outTileId, int32_t *outCellIndex ) {
    /**
    *    Sanity check: require a valid mesh before touching tile-localization state.
    **/
    if ( !mesh ) {
        return false;
    }

    /**
    *    Resolve nav2-owned tile localization metadata from the projected nav-center point.
    **/
    nav2_query_tile_location_t tileLocation = {};
    if ( !SVG_Nav2_QueryTryResolveTileLocation( mesh, centerOrigin, &tileLocation ) ) {
        return false;
    }

    /**
    *    Commit the resolved tile and cell metadata for the caller.
    **/
    if ( outTileKey ) {
        *outTileKey = tileLocation.tile_key;
    }
    if ( outTileId ) {
        *outTileId = tileLocation.tile_id;
    }
    if ( outCellIndex ) {
        *outCellIndex = tileLocation.cell_index;
    }
    return true;
}

/**
*	@brief	Compute a deterministic ranking score for one candidate endpoint.
*	@param	candidate	Candidate metadata to update.
*	@param	startLeaf	Start BSP leaf id when known.
*	@param	startCluster	Start BSP cluster id when known.
*	@param	startArea	Start BSP area id when known.
*	@param	rawGoalLeaf	Raw-goal BSP leaf id when known.
*	@param	rawGoalCluster	Raw-goal BSP cluster id when known.
*	@param	rawGoalArea	Raw-goal BSP area id when known.
**/
static void SVG_Nav2_ScoreGoalCandidate( nav2_goal_candidate_t *candidate, const int32_t startLeaf,
    const int32_t startCluster, const int32_t startArea, const int32_t rawGoalLeaf,
    const int32_t rawGoalCluster, const int32_t rawGoalArea ) {
    /**
    *    Sanity check: require a valid candidate record before applying ranking hints.
    **/
    if ( !candidate ) {
        return;
    }

    /**
    *    Start from the geometric lower bound and then layer in lightweight BSP and sampling heuristics.
    **/
    double rankingScore = candidate->lower_bound_score;

    /**
    *    Prefer endpoints that preserve the start BSP neighborhood to avoid unnecessary wrong-floor drift.
    **/
    if ( startLeaf >= 0 && candidate->leaf_index == startLeaf ) {
        candidate->flags |= NAV_GOAL_CANDIDATE_FLAG_SAME_LEAF_AS_START;
    } else if ( startLeaf >= 0 ) {
        rankingScore += 6.0;
    }
    if ( startCluster >= 0 && candidate->cluster_id == startCluster ) {
        candidate->flags |= NAV_GOAL_CANDIDATE_FLAG_SAME_CLUSTER_AS_START;
    } else if ( startCluster >= 0 ) {
        rankingScore += 10.0;
    }
    if ( startArea >= 0 && candidate->area_id == startArea ) {
        candidate->flags |= NAV_GOAL_CANDIDATE_FLAG_SAME_AREA_AS_START;
    } else if ( startArea >= 0 ) {
        rankingScore += 8.0;
    }

    /**
    *    Also prefer endpoints that remain consistent with the raw goal's BSP neighborhood.
    **/
    if ( rawGoalLeaf >= 0 && candidate->leaf_index == rawGoalLeaf ) {
        candidate->flags |= NAV_GOAL_CANDIDATE_FLAG_SAME_LEAF_AS_RAW_GOAL;
        rankingScore -= 1.5;
    }
    if ( rawGoalCluster >= 0 && candidate->cluster_id == rawGoalCluster ) {
        candidate->flags |= NAV_GOAL_CANDIDATE_FLAG_SAME_CLUSTER_AS_RAW_GOAL;
        rankingScore -= 1.0;
    }
    if ( rawGoalArea >= 0 && candidate->area_id == rawGoalArea ) {
        candidate->flags |= NAV_GOAL_CANDIDATE_FLAG_SAME_AREA_AS_RAW_GOAL;
    }

    /**
    *    Apply a small penalty to neighborhood samples so the same-cell projection remains preferred when otherwise equivalent.
    **/
    if ( candidate->sample_radius > 0 ) {
        candidate->flags |= NAV_GOAL_CANDIDATE_FLAG_NEIGHBOR_SAMPLE;
        rankingScore += 2.5 * ( double )candidate->sample_radius;
    }

    /**
    *    Prefer stair- and ladder-marked layers slightly because they are often meaningful vertical-intent candidates.
    **/
    if ( ( candidate->flags & NAV_GOAL_CANDIDATE_FLAG_HAS_STAIR_BITS ) != 0 ) {
        rankingScore -= 0.75;
    }
    if ( ( candidate->flags & NAV_GOAL_CANDIDATE_FLAG_HAS_LADDER_BITS ) != 0 ) {
        rankingScore -= 0.5;
    }
    if ( ( candidate->flags & NAV_GOAL_CANDIDATE_FLAG_IN_FAT_PVS ) != 0 ) {
        rankingScore -= 0.25;
    }

    /**
    *    Commit the final deterministic ranking score.
    **/
    candidate->ranking_score = rankingScore;
}

/**
*	@brief	Append one projected goal candidate to the fixed-capacity buffer.
*	@param	mesh		Navigation mesh used for tile and BSP metadata lookup.
*	@param	startOrigin	Feet-origin start point used for ranking hints.
*	@param	rawGoalOrigin	Feet-origin raw goal point used for lower-bound scoring.
*	@param	sampleGoalOrigin	Feet-origin sample point to project.
*	@param	agentMins	Navigation hull mins.
*	@param	agentMaxs	Navigation hull maxs.
*	@param	type		Semantic candidate type.
*	@param	sampleDx	Relative sample X offset in cells.
*	@param	sampleDy	Relative sample Y Offset in cells.
*	@param	sampleRadius	Relative sample radius in cells.
*	@param	candidates	Candidate buffer receiving the accepted endpoint.
*	@return	True when a new candidate was appended.
**/
static const bool SVG_Nav2_TryAppendGoalCandidate( const nav2_mesh_t *mesh, const Vector3 &startOrigin,
    const Vector3 &rawGoalOrigin, const Vector3 &sampleGoalOrigin, const Vector3 &agentMins, const Vector3 &agentMaxs,
    const nav2_goal_candidate_type_t type, const int32_t sampleDx, const int32_t sampleDy, const int32_t sampleRadius,
    nav2_goal_candidate_list_t *candidates ) {
    /**
    *    Sanity check: require mesh and candidate output storage before attempting projection.
    **/
    if ( !mesh || !candidates ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::MissingMesh, type,
            sampleDx, sampleDy, 0, 0, -1, -1, -1 );
        return false;
    }
    if ( !( agentMaxs.x > agentMins.x ) || !( agentMaxs.y > agentMins.y ) || !( agentMaxs.z > agentMins.z ) ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::InvalidAgentBounds, type,
            sampleDx, sampleDy, 0, 0, -1, -1, -1 );
        return false;
    }

	// Get the query mesh for the current nav mesh to pass to candidate builders and helpers. 
	// This is a very cheap operation because the query mesh is cached and shared across queries on the same nav mesh.
 const nav2_query_mesh_t queryMesh = SVG_Nav2_QueryMakeMesh( mesh );

    /**
    *    Project the sampled feet-origin goal onto the nearest walkable layer at that sample XY.
    **/
    Vector3 projectedFeetOrigin = {};
    if ( !SVG_Nav2_TryProjectFeetOriginToWalkableZ( mesh, sampleGoalOrigin, agentMins, agentMaxs, &projectedFeetOrigin ) ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::MissingCellLayers, type,
            sampleDx, sampleDy, 0, 0, -1, -1, -1 );
        return false;
    }

    /**
    *    Resolve nav-center, tile, and cell metadata for the projected candidate endpoint.
    **/
   const Vector3 projectedCenterOrigin = SVG_Nav2_QueryConvertFeetToCenter( &queryMesh, projectedFeetOrigin, &agentMins, &agentMaxs );
    nav2_tile_cluster_key_t tileKey = {};
    int32_t tileId = -1;
    int32_t cellIndex = -1;
 if ( !SVG_Nav2_TryResolveGoalTileMeta( &queryMesh, projectedCenterOrigin, &tileKey, &tileId, &cellIndex ) ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::MissingWorldTile, type,
            sampleDx, sampleDy, 0, 0, -1, -1, -1 );
        return false;
    }

    /**
    *    Resolve the layer index in the localized cell so duplicates stay tied to stable canonical ids.
    **/
   const nav2_query_tile_view_t tile = SVG_Nav2_QueryGetTileViewByCoords( &queryMesh, tileKey.tile_x, tileKey.tile_y );
    const auto cells = SVG_Nav2_QueryGetTileCells( &queryMesh, tile );
    const int32_t cellCount = ( int32_t )cells.size();
    if ( cells.empty() || cellIndex < 0 || cellIndex >= cellCount ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::MissingCellLayers, type,
            sampleDx, sampleDy, tileKey.tile_x, tileKey.tile_y, tileId, cellIndex, -1 );
        return false;
    }
 const nav2_query_cell_view_t &cell = cells[ cellIndex ];
    if ( cell.num_layers <= 0 || cell.layers.empty() ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::MissingCellLayers, type,
            sampleDx, sampleDy, tileKey.tile_x, tileKey.tile_y, tileId, cellIndex, -1 );
        return false;
    }
    int32_t layerIndex = -1;
    if ( !SVG_Nav2_QuerySelectLayerIndex( &queryMesh, cell, projectedCenterOrigin.z, &layerIndex ) || layerIndex < 0 || layerIndex >= cell.num_layers ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::MissingCellLayers, type,
            sampleDx, sampleDy, tileKey.tile_x, tileKey.tile_y, tileId, cellIndex, layerIndex );
        return false;
    }

    /**
    *    Suppress duplicate tile/cell/layer candidates so the list stays compact and deterministic.
    **/
    if ( SVG_Nav2_HasDuplicateGoalCandidate( *candidates, tileId, cellIndex, layerIndex ) ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::DuplicateCandidate, type,
            sampleDx, sampleDy, tileKey.tile_x, tileKey.tile_y, tileId, cellIndex, layerIndex );
        return false;
    }
    if ( candidates->candidate_count >= NAV_GOAL_CANDIDATE_MAX_COUNT ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::CandidateLimitReached, type,
            sampleDx, sampleDy, tileKey.tile_x, tileKey.tile_y, tileId, cellIndex, layerIndex );
        return false;
    }

    /**
    *    Reject candidates that still land in solid point contents after projection.
    **/
    const cm_contents_t pointContents = gi.pointcontents ? gi.pointcontents( &projectedCenterOrigin ) : CONTENTS_NONE;
    if ( ( pointContents & CONTENTS_SOLID ) != 0 ) {
        SVG_Nav2_RecordGoalCandidateRejection( candidates, nav2_goal_candidate_rejection_t::SolidPointContents, type,
            sampleDx, sampleDy, tileKey.tile_x, tileKey.tile_y, tileId, cellIndex, layerIndex );
        return false;
    }

    /**
    *    Append the accepted candidate with stable ids, BSP metadata, and cheap ranking inputs.
    **/
    nav2_goal_candidate_t &candidate = candidates->candidates[ candidates->candidate_count++ ];
    candidate = {};
    candidate.candidate_id = candidates->candidate_count - 1;
    candidate.type = type;
    candidate.feet_origin = projectedFeetOrigin;
    candidate.center_origin = projectedCenterOrigin;
    candidate.tile_x = tileKey.tile_x;
    candidate.tile_y = tileKey.tile_y;
    candidate.tile_id = tileId;
    candidate.cell_index = cellIndex;
    candidate.layer_index = layerIndex;
    candidate.sample_dx = sampleDx;
    candidate.sample_dy = sampleDy;
    candidate.sample_radius = sampleRadius;
    candidate.point_contents = pointContents;
    candidate.lower_bound_score = QM_Vector3Distance( startOrigin, projectedFeetOrigin ) + QM_Vector3Distance( projectedFeetOrigin, rawGoalOrigin );
    SVG_Nav2_TryResolveGoalLeafMeta( candidate.center_origin, &candidate.leaf_index, &candidate.cluster_id, &candidate.area_id );

    /**
    *    Promote existing layer traversal metadata into lightweight semantic ranking flags.
    **/
   const nav2_query_layer_view_t &layer = cell.layers[ layerIndex ];
    const nav2_topology_semantics_t topologySemantics = SVG_Nav2_MakeTopologySemanticsFromGoalLayer( layer, pointContents );
    if ( ( topologySemantics.feature_bits & NAV2_TOPOLOGY_FEATURE_STAIR ) != 0 ) {
        candidate.flags |= NAV_GOAL_CANDIDATE_FLAG_HAS_STAIR_BITS;
    }
    if ( ( topologySemantics.feature_bits & NAV2_TOPOLOGY_FEATURE_LADDER ) != 0 ) {
        candidate.flags |= NAV_GOAL_CANDIDATE_FLAG_HAS_LADDER_BITS;
    }

    /**
    *    Use fat-PVS only as a small relevance hint for ranking, never as a legality proof.
    **/
    if ( gi.inFatPVS ) {
        const Vector3 candidateBoxMins = QM_Vector3Add( projectedFeetOrigin, agentMins );
        const Vector3 candidateBoxMaxs = QM_Vector3Add( projectedFeetOrigin, agentMaxs );
        const vec_t startVec[ 3 ] = { startOrigin.x, startOrigin.y, startOrigin.z };
        const vec_t minsVec[ 3 ] = { candidateBoxMins.x, candidateBoxMins.y, candidateBoxMins.z };
        const vec_t maxsVec[ 3 ] = { candidateBoxMaxs.x, candidateBoxMaxs.y, candidateBoxMaxs.z };
        if ( gi.inFatPVS( &startVec, &minsVec, &maxsVec, DVIS_PVS ) ) {
            candidate.flags |= NAV_GOAL_CANDIDATE_FLAG_IN_FAT_PVS;
        }
    }

    return true;
}

/**
*	@brief	Sort accepted candidates by ranking score and then by candidate id.
*	@param	candidates	Candidate buffer to sort in place.
**/
static void SVG_Nav2_SortGoalCandidates( nav2_goal_candidate_list_t *candidates ) {
    /**
    *    Sanity check: nothing to do for missing buffers or single-candidate inputs.
    **/
    if ( !candidates || candidates->candidate_count <= 1 ) {
        return;
    }

    /**
    *    Use a simple in-place stable ordering pass because the candidate set is intentionally tiny.
    **/
    for ( int32_t i = 0; i < candidates->candidate_count - 1; i++ ) {
        for ( int32_t j = i + 1; j < candidates->candidate_count; j++ ) {
            const nav2_goal_candidate_t &lhs = candidates->candidates[ i ];
            const nav2_goal_candidate_t &rhs = candidates->candidates[ j ];
            const bool shouldSwap = ( rhs.ranking_score < lhs.ranking_score )
                || ( rhs.ranking_score == lhs.ranking_score && rhs.candidate_id < lhs.candidate_id );
            if ( shouldSwap ) {
                nav2_goal_candidate_t tmp = candidates->candidates[ i ];
                candidates->candidates[ i ] = candidates->candidates[ j ];
                candidates->candidates[ j ] = tmp;
            }
        }
    }
}


/**
*
*
*	Nav2 Goal Candidate Public API:
*
*
**/
/**
*	@brief	Build a BSP-aware candidate endpoint set for a start/goal query pair.
*	@param	mesh		Navigation mesh used for tile, layer, and world metadata lookup.
*	@param	start_origin	Feet-origin start point used for same-leaf/cluster ranking hints.
*	@param	goal_origin	Feet-origin raw goal point requested by gameplay code.
*	@param	agent_mins	Feet-origin navigation hull mins.
*	@param	agent_maxs	Feet-origin navigation hull maxs.
*	@param	out_candidates	[out] Fixed-capacity candidate buffer receiving accepted and rejected samples.
*	@return	True when at least one viable candidate was produced.
*	@note	This standalone Task 3.1 implementation keeps the staged candidate builder out of the compatibility seam.
**/
const bool SVG_Nav2_BuildGoalCandidates( const nav2_mesh_t *mesh, const Vector3 &start_origin, const Vector3 &goal_origin,
    const Vector3 &agent_mins, const Vector3 &agent_maxs, nav2_goal_candidate_list_t *out_candidates ) {
    /**
    *    Sanity check: require output storage and a valid mesh before generating candidates.
    **/
    if ( !out_candidates ) {
        return false;
    }
    *out_candidates = {};
    if ( !mesh ) {
        SVG_Nav2_RecordGoalCandidateRejection( out_candidates, nav2_goal_candidate_rejection_t::MissingMesh,
            nav2_goal_candidate_type_t::ProjectedSameCell, 0, 0, 0, 0, -1, -1, -1 );
        return false;
    }
    /**
    *    Resolve the start and raw-goal BSP neighborhoods once so all candidates can share the same ranking context.
    **/
    const nav2_query_mesh_t queryMeshWrapper = SVG_Nav2_QueryMakeMesh( mesh );
    const Vector3 start_center = SVG_Nav2_QueryConvertFeetToCenter( &queryMeshWrapper, start_origin, &agent_mins, &agent_maxs );
    const Vector3 raw_goal_center = SVG_Nav2_QueryConvertFeetToCenter( &queryMeshWrapper, goal_origin, &agent_mins, &agent_maxs );
    const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( &queryMeshWrapper );
    int32_t startLeaf = -1;
    int32_t startCluster = -1;
    int32_t startArea = -1;
    SVG_Nav2_TryResolveGoalLeafMeta( start_center, &startLeaf, &startCluster, &startArea );
    int32_t rawGoalLeaf = -1;
    int32_t rawGoalCluster = -1;
    int32_t rawGoalArea = -1;
    SVG_Nav2_TryResolveGoalLeafMeta( raw_goal_center, &rawGoalLeaf, &rawGoalCluster, &rawGoalArea );

    /**
    *    Always try the same-cell projection first so existing behavior remains the low-risk baseline candidate.
    **/
    SVG_Nav2_TryAppendGoalCandidate( mesh, start_origin, goal_origin, goal_origin, agent_mins, agent_maxs,
        nav2_goal_candidate_type_t::ProjectedSameCell, 0, 0, 0, out_candidates );

    /**
    *    Sample the immediate 3x3 XY neighborhood so nearby support positions can compete with the raw projected goal.
    **/
    // Use bounded concentric sampling to improve robustness for boundary and non-LOS endpoints.
    constexpr int32_t maxSampleRadiusCells = 2;
    for ( int32_t sampleRadius = 1; sampleRadius <= maxSampleRadiusCells; sampleRadius++ ) {
        for ( int32_t sampleDy = -sampleRadius; sampleDy <= sampleRadius; sampleDy++ ) {
            for ( int32_t sampleDx = -sampleRadius; sampleDx <= sampleRadius; sampleDx++ ) {
                /**
                *    Skip inner cells that were already covered by a prior radius ring.
                **/
                if ( std::max( std::abs( sampleDx ), std::abs( sampleDy ) ) != sampleRadius ) {
                    continue;
                }

                /**
                *    Offset the raw goal by sampled cells in XY to probe nearby walkable support positions.
                **/
                Vector3 sampledGoalOrigin = goal_origin;
                sampledGoalOrigin.x += ( float )( sampleDx * meshMeta.cell_size_xy );
                sampledGoalOrigin.y += ( float )( sampleDy * meshMeta.cell_size_xy );
                SVG_Nav2_TryAppendGoalCandidate( mesh, start_origin, goal_origin, sampledGoalOrigin, agent_mins, agent_maxs,
                    nav2_goal_candidate_type_t::NearbySupport, sampleDx, sampleDy, sampleRadius, out_candidates );
            }
        }
    }

    /**
    *    Apply deterministic BSP-aware ranking hints and then sort the final accepted candidate list.
    **/
    for ( int32_t i = 0; i < out_candidates->candidate_count; i++ ) {
        SVG_Nav2_ScoreGoalCandidate( &out_candidates->candidates[ i ], startLeaf, startCluster, startArea,
            rawGoalLeaf, rawGoalCluster, rawGoalArea );
    }
    SVG_Nav2_SortGoalCandidates( out_candidates );
    return out_candidates->candidate_count > 0;
}

/**
*	@brief	Select the best-ranked candidate from a previously built candidate list.
*	@param	candidates	Accepted/rejected candidate list built for the current query.
*	@param	out_candidate	[out] Selected candidate record.
*	@return	True when a candidate was selected.
**/
const bool SVG_Nav2_SelectBestGoalCandidate( const nav2_goal_candidate_list_t &candidates, nav2_goal_candidate_t *out_candidate ) {
    /**
    *    Sanity check: require output storage and at least one accepted candidate.
    **/
    if ( !out_candidate || candidates.candidate_count <= 0 ) {
        return false;
    }

    /**
    *    The builder already sorted the list, so the first accepted candidate is the deterministic winner.
    **/
    *out_candidate = candidates.candidates[ 0 ];
    return true;
}

/**
*	@brief	Resolve the preferred feet-origin goal endpoint for a start/goal query pair.
*	@param	mesh		Navigation mesh used for endpoint selection.
*	@param	start_origin	Feet-origin start point used for ranking hints.
*	@param	goal_origin	Feet-origin raw goal point requested by gameplay code.
*	@param	agent_mins	Feet-origin navigation hull mins.
*	@param	agent_maxs	Feet-origin navigation hull maxs.
*	@param	out_goal_origin	[out] Selected feet-origin endpoint.
*	@param	out_candidate	[out] Optional selected candidate metadata.
*	@param	out_candidates	[out] Optional accepted/rejected candidate list for debug or diagnostics.
*	@return	True when the candidate builder produced a viable endpoint.
*	@note	This public nav2 path is strict; any temporary fallback projection must remain inside the
*           internal seam and not surface here.
**/
const bool SVG_Nav2_ResolveBestGoalCandidate( const nav2_mesh_t *mesh, const Vector3 &startOrigin, const Vector3 &goalOrigin,
    const Vector3 &agentMins, const Vector3 &agentMaxs, Vector3 *outGoalOrigin,
    nav2_goal_candidate_t *outCandidate, nav2_goal_candidate_list_t *outCandidates ) {
    // Sanity check: require an output slot for the selected feet-origin endpoint.
    if ( !outGoalOrigin ) {
        return false;
    }

    // Sanity check: require the active mesh before building strict candidates.
    if ( !mesh ) {
        return false;
    }

    // Build the candidate set and require at least one viable endpoint.
    nav2_goal_candidate_list_t localCandidates = {};
    nav2_goal_candidate_list_t *candidateBuffer = outCandidates ? outCandidates : &localCandidates;
    if ( !SVG_Nav2_BuildGoalCandidates( mesh, startOrigin, goalOrigin, agentMins, agentMaxs, candidateBuffer ) ) {
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
*	@brief	Emit a concise debug dump of generated candidates and recorded rejections.
*	@param	candidates	Candidate list to report.
*	@param	selected_candidate	Optional selected candidate to highlight in the dump.
*	@note	Callers should keep this gated because the user explicitly prefers minimal extra logging.
**/
void SVG_Nav2_DebugPrintGoalCandidates( const nav2_goal_candidate_list_t &candidates, const nav2_goal_candidate_t *selected_candidate ) {
    /**
    *    Emit a compact header first so later per-candidate prints have a stable summary line.
    **/
    gi.dprintf( "[NAV2][GoalCandidates] accepted=%d rejected=%d\n", candidates.candidate_count, candidates.rejection_count );

    /**
    *    Dump the accepted candidates in deterministic ranking order.
    **/
    for ( int32_t i = 0; i < candidates.candidate_count; i++ ) {
        const nav2_goal_candidate_t &candidate = candidates.candidates[ i ];
        const bool selected = selected_candidate && candidate.candidate_id == selected_candidate->candidate_id
            && candidate.type == selected_candidate->type;
        gi.dprintf( "[NAV2][GoalCandidates] %s idx=%d type=%d score=%.2f feet=(%.1f %.1f %.1f) tile=(%d,%d) cell=%d layer=%d leaf=%d cluster=%d area=%d\n",
            selected ? "selected" : "candidate",
            i,
            ( int32_t )candidate.type,
            candidate.ranking_score,
            candidate.feet_origin.x,
            candidate.feet_origin.y,
            candidate.feet_origin.z,
            candidate.tile_x,
            candidate.tile_y,
            candidate.cell_index,
            candidate.layer_index,
            candidate.leaf_index,
            candidate.cluster_id,
            candidate.area_id );
    }

    /**
    *    Dump rejected samples afterwards so boundary-origin or missing-cell cases are easy to spot during diagnostics.
    **/
    for ( int32_t i = 0; i < candidates.rejection_count; i++ ) {
        const nav2_goal_candidate_rejection_info_t &rejection = candidates.rejections[ i ];
        gi.dprintf( "[NAV2][GoalCandidates] rejection idx=%d reason=%d type=%d tile=(%d,%d) tileId=%d cell=%d layer=%d sample=(%d,%d)\n",
            i,
            ( int32_t )rejection.reason,
            ( int32_t )rejection.type,
            rejection.tile_x,
            rejection.tile_y,
            rejection.tile_id,
            rejection.cell_index,
            rejection.layer_index,
            rejection.sample_dx,
            rejection.sample_dy );
    }
}

/**
*	@brief	Keep the nav2 goal candidate module represented in the build.
*	@note	This is now a lightweight no-op anchor because Task 3.1 owns a real standalone implementation in this file.
**/
void SVG_Nav2_GoalCandidates_ModuleAnchor( void ) {
}
