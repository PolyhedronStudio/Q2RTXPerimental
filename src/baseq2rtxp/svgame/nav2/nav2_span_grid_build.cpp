/********************************************************************
*
*
*	ServerGame: Nav2 Span Grid Build - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_span_grid_build.h"

#include <algorithm>
#include <array>

#include "shared/cm/cm_types.h"


/**
*
*
*	Nav2 Span Grid Build Local Helpers:
*
*
**/
//! Default fallback tile edge length used when the active mesh has not published sizing metadata yet.
static constexpr int32_t NAV2_SPAN_GRID_FALLBACK_TILE_SIZE = 8;
//! Default fallback XY cell size used when the active mesh has not published sizing metadata yet.
static constexpr double NAV2_SPAN_GRID_FALLBACK_CELL_SIZE_XY = 32.0;
//! Default fallback Z quantization used when the active mesh has not published sizing metadata yet.
static constexpr double NAV2_SPAN_GRID_FALLBACK_Z_QUANT = 16.0;
//! Minimum accepted vertical clearance for a conservative walkable span sample.
static constexpr double NAV2_SPAN_GRID_MIN_CLEARANCE = 56.0;
//! Maximum upward probe distance used when checking headroom above a sampled floor.
static constexpr double NAV2_SPAN_GRID_CLEARANCE_PROBE = 128.0;
//! Maximum downward probe distance used when searching for a support floor beneath a sampled column center.
static constexpr double NAV2_SPAN_GRID_FLOOR_PROBE = 256.0;
//! Epsilon used when offsetting support and clearance probes away from collision planes.
static constexpr double NAV2_SPAN_GRID_TRACE_EPSILON = 1.0;


/**
*	@brief	Resolve conservative rasterization parameters from the active mesh.
*	@param	mesh		Nav2-owned mesh providing sizing metadata.
*	@param	out_grid	[in,out] Grid receiving the resolved parameters.
*	@return	True when the output grid is available.
**/
static const bool SVG_Nav2_ResolveSpanGridParameters( const nav2_mesh_t *mesh, nav2_span_grid_t *out_grid ) {
	/**
	*    Require output storage before publishing any rasterization parameters.
	**/
	if ( !out_grid ) {
		return false;
	}

	/**
	*    Start from deterministic defaults and then widen them with mesh-authored values when present.
	**/
	out_grid->tile_size = NAV2_SPAN_GRID_FALLBACK_TILE_SIZE;
	out_grid->cell_size_xy = NAV2_SPAN_GRID_FALLBACK_CELL_SIZE_XY;
	out_grid->z_quant = NAV2_SPAN_GRID_FALLBACK_Z_QUANT;
	if ( !mesh ) {
		return true;
	}

	/**
	*    Prefer mesh-authored tile sizing because later serialization and corridor code already mirrors those values.
	**/
	if ( mesh->tile_size > 0 ) {
		out_grid->tile_size = mesh->tile_size;
	}
	if ( mesh->cell_size_xy > 0.0 ) {
		out_grid->cell_size_xy = mesh->cell_size_xy;
	}
	if ( mesh->z_quant > 0.0 ) {
		out_grid->z_quant = mesh->z_quant;
	}
	return true;
}

/**
*	@brief	Resolve the world-space rasterization bounds for the active nav2 mesh.
*	@param	mesh		Nav2-owned mesh providing world bounds.
*	@param	out_bounds	[out] Bounds used for span-grid rasterization.
*	@return	True when valid bounds were produced.
**/
static const bool SVG_Nav2_ResolveSpanGridWorldBounds( const nav2_mesh_t *mesh, BBox3 *out_bounds ) {
	/**
	*    Require both mesh storage and output storage before touching world bounds.
	**/
	if ( !mesh || !out_bounds ) {
		return false;
	}

	/**
	*    Start from the nav2-owned mesh bounds and reject obviously invalid ranges.
	**/
	const BBox3 meshBounds = mesh->world_bounds;
	if ( meshBounds.maxs.x <= meshBounds.mins.x || meshBounds.maxs.y <= meshBounds.mins.y || meshBounds.maxs.z <= meshBounds.mins.z ) {
		return false;
	}

	/**
	*    Publish the validated rasterization bounds.
	**/
	*out_bounds = meshBounds;
	return true;
}

/**
*	@brief	Quantize a world-space point into the owning tile coordinate for the resolved grid parameters.
*	@param	grid		Resolved span-grid sizing metadata.
*	@param	point	World-space point to localize.
*	@return	Stable world-tile key for the point.
**/
static nav2_world_tile_key_t SVG_Nav2_SpanGrid_TileKeyForPoint( const nav2_span_grid_t &grid, const Vector3 &point ) {
	/**
	*    Derive the world-space tile edge length once so X and Y quantization stay consistent.
	**/
	const double tileWorldSize = ( double )grid.tile_size * grid.cell_size_xy;
	nav2_world_tile_key_t key = {};
	if ( tileWorldSize <= 0.0 ) {
		return key;
	}

	/**
	*    Quantize the XY point to the enclosing world tile using floor semantics for deterministic negative coordinates.
	**/
	key.tile_x = ( int32_t )std::floor( point.x / tileWorldSize );
	key.tile_y = ( int32_t )std::floor( point.y / tileWorldSize );
	return key;
}

/**
*	@brief	Resolve the canonical nav2 world-tile id for one sampled column center.
*	@param	mesh		Nav2-owned mesh providing canonical world-tile lookup.
*	@param	tile_key	Stable world-tile key derived from the sample point.
*	@return	Canonical world-tile id, or -1 when the tile has not been published yet.
**/
static int32_t SVG_Nav2_SpanGrid_TryResolveTileId( const nav2_mesh_t *mesh, const nav2_world_tile_key_t &tile_key ) {
	/**
	*    Return the unresolved sentinel when the mesh does not provide canonical world-tile storage yet.
	**/
	if ( !mesh ) {
		return -1;
	}

	/**
	*    Use the existing nav2-owned world-tile map so the span-grid builder stays serialization-friendly.
	**/
	const auto tileIt = mesh->world_tile_id_of.find( tile_key );
	if ( tileIt == mesh->world_tile_id_of.end() ) {
		return -1;
	}
	return tileIt->second;
}

/**
*	@brief	Append a new sparse column for a sampled XY point.
*	@param	mesh		Nav2-owned mesh providing canonical tile ids.
*	@param	grid		[in,out] Span grid receiving the new column.
*	@param	column_center	World-space column center.
*	@return	Reference to the appended sparse column.
**/
static nav2_span_column_t &SVG_Nav2_SpanGrid_AppendColumn( const nav2_mesh_t *mesh, nav2_span_grid_t *grid, const Vector3 &column_center ) {
	/**
	*    Append an empty sparse column first so the function can return a stable reference.
	**/
	grid->columns.emplace_back();
	nav2_span_column_t &column = grid->columns.back();

	/**
	*    Quantize the world-space point to the owning tile coordinates and resolve the canonical tile id when available.
	**/
	const nav2_world_tile_key_t tileKey = SVG_Nav2_SpanGrid_TileKeyForPoint( *grid, column_center );
	column.tile_x = tileKey.tile_x;
	column.tile_y = tileKey.tile_y;
	column.tile_id = SVG_Nav2_SpanGrid_TryResolveTileId( mesh, tileKey );
	return column;
}

/**
*	@brief	Resolve the collision-space leaf, cluster, and area for one world-space point.
*	@param	point		World-space point to classify.
*	@param	out_span	[in,out] Span receiving the resolved topology metadata.
**/
static void SVG_Nav2_SpanGrid_ResolvePointTopology( const Vector3 &point, nav2_span_t *out_span ) {
	/**
	*    Require output storage and the collision-model import before attempting any BSP classification.
	**/
	if ( !out_span || !gi.GetCollisionModel || !gi.BSP_PointLeaf ) {
		return;
	}

	/**
	*    Require an active collision model and BSP cache before dereferencing the world BSP tree.
	**/
	const cm_t *collisionModel = gi.GetCollisionModel();
	if ( !collisionModel || !collisionModel->cache || !collisionModel->cache->nodes ) {
		return;
	}

	/**
	*    Classify the point against the world BSP and mirror the leaf-derived cluster and area ids into the span metadata.
	**/
  const vec3_t pointVec = { point.x, point.y, point.z };
	mleaf_t *leaf = gi.BSP_PointLeaf( collisionModel->cache->nodes, pointVec );
	if ( !leaf ) {
		return;
	}
	out_span->cluster_id = leaf->cluster;
	out_span->area_id = leaf->area;

	/**
	*    Derive a stable leaf index through pointer subtraction only after verifying the leaf lives inside the world-leaf array.
	**/
    if ( collisionModel->cache->leafs && leaf >= collisionModel->cache->leafs && leaf < ( collisionModel->cache->leafs + collisionModel->cache->numleafs ) ) {
		out_span->leaf_id = ( int32_t )( leaf - collisionModel->cache->leafs );
	}
}

/**
*	@brief	Build a conservative sampling AABB centered on one XY column.
*	@param	sample_center	World-space column center.
*	@param	half_extent_xy	Half-extent used for XY box sampling.
*	@param	half_height	Half-extent used for the vertical box sampling.
*	@param	out_mins	[out] World-space minimum bounds.
*	@param	out_maxs	[out] World-space maximum bounds.
**/
static void SVG_Nav2_SpanGrid_BuildSampleBounds( const Vector3 &sample_center, const double half_extent_xy, const double half_height,
	Vector3 *out_mins, Vector3 *out_maxs ) {
	/**
	*    Require both output bounds before publishing any sampling volume.
	**/
	if ( !out_mins || !out_maxs ) {
		return;
	}

	/**
	*    Build a compact centered AABB used for box contents and leaf overlap sampling.
	**/
	const Vector3 extents = {
		( float )half_extent_xy,
		( float )half_extent_xy,
		( float )half_height
	};
	*out_mins = QM_Vector3Subtract( sample_center, extents );
	*out_maxs = QM_Vector3Add( sample_center, extents );
}

/**
*	@brief	Try resolve a conservative traversable span from one sampled column center.
*	@param	mesh		Nav2-owned mesh providing agent and sizing metadata.
*	@param	grid		Resolved span-grid sizing metadata.
*	@param	column_center	World-space center of the sampled XY column.
*	@param	span_id	Stable span id to assign on success.
*	@param	out_span	[out] Span receiving the rasterized metadata.
*	@return	True when the sampled column produced a conservative traversable span.
**/
static const bool SVG_Nav2_SpanGrid_TryBuildSpanAtPoint( const nav2_mesh_t *mesh, const nav2_span_grid_t &grid, const Vector3 &column_center,
	const int32_t span_id, nav2_span_t *out_span ) {
	/**
	*    Require output storage and the collision trace imports before attempting support classification.
	**/
	if ( !out_span || !gi.trace || !gi.pointcontents || !gi.CM_BoxContents ) {
		return false;
	}
	*out_span = {};

	/**
	*    Start from the mesh-authored agent hull when available and otherwise use conservative humanoid defaults.
	**/
	Vector3 agentMins = { -16.0f, -16.0f, 0.0f };
	Vector3 agentMaxs = { 16.0f, 16.0f, 56.0f };
	if ( mesh && mesh->agent_profile.maxs.x > mesh->agent_profile.mins.x ) {
		agentMins = mesh->agent_profile.mins;
		agentMaxs = mesh->agent_profile.maxs;
	}

	/**
	*    Reject samples whose probe center already sits inside a solid leaf because they cannot produce a valid standing span.
	**/
	const cm_contents_t pointContents = gi.pointcontents( &column_center );
	if ( ( pointContents & CONTENTS_SOLID ) != 0 ) {
		return false;
	}

	/**
	*    Trace downward from above the sample center to find conservative standing support for the column.
	**/
	Vector3 floorProbeStart = column_center;
	floorProbeStart.z += ( float )( NAV2_SPAN_GRID_CLEARANCE_PROBE * 0.5 );
	Vector3 floorProbeEnd = column_center;
	floorProbeEnd.z -= ( float )NAV2_SPAN_GRID_FLOOR_PROBE;
	const cm_trace_t floorTrace = gi.trace( &floorProbeStart, &agentMins, &agentMaxs, &floorProbeEnd, nullptr, CM_CONTENTMASK_PLAYERSOLID );
	if ( floorTrace.startsolid || floorTrace.allsolid || floorTrace.fraction >= 1.0 ) {
		return false;
	}

	/**
	*    Require a meaningfully walkable plane normal so the first span pass does not accept walls or extreme slopes.
	**/
	const double minimumSlopeNormal = ( mesh && mesh->agent_profile.max_slope_normal_z > 0.0 ) ? mesh->agent_profile.max_slope_normal_z : PHYS_MAX_SLOPE_NORMAL;
 if ( floorTrace.plane.type != PLANE_Z && floorTrace.plane.normal[ 2 ] < minimumSlopeNormal ) {
		return false;
	}

	/**
	*    Probe upward from just above the detected floor so the first span pass can estimate conservative headroom.
	**/
	Vector3 clearanceProbeStart = floorTrace.endpos;
	clearanceProbeStart.z += ( float )NAV2_SPAN_GRID_TRACE_EPSILON;
	Vector3 clearanceProbeEnd = clearanceProbeStart;
	clearanceProbeEnd.z += ( float )NAV2_SPAN_GRID_CLEARANCE_PROBE;
	const cm_trace_t clearanceTrace = gi.trace( &clearanceProbeStart, &agentMins, &agentMaxs, &clearanceProbeEnd, nullptr, CM_CONTENTMASK_PLAYERSOLID );
	const double floorZ = floorTrace.endpos.z;
	const double ceilingZ = clearanceTrace.endpos.z;
	if ( ( ceilingZ - floorZ ) < NAV2_SPAN_GRID_MIN_CLEARANCE ) {
		return false;
	}

	/**
	*    Validate the sampled standing volume with a centered box-contents query so liquid and solid flags are mirrored into the span metadata.
	**/
	cm_contents_t boxContents = CONTENTS_NONE;
	std::array<mleaf_t *, 16> leafList = {};
	mnode_t *topNode = nullptr;
	const Vector3 standingCenter = {
		floorTrace.endpos.x,
		floorTrace.endpos.y,
		( float )( floorZ + ( ( ceilingZ - floorZ ) * 0.5 ) )
	};
	Vector3 sampleMins = QM_Vector3Zero();
	Vector3 sampleMaxs = QM_Vector3Zero();
	SVG_Nav2_SpanGrid_BuildSampleBounds( standingCenter, grid.cell_size_xy * 0.35, ( ceilingZ - floorZ ) * 0.5, &sampleMins, &sampleMaxs );
    const vec3_t sampleMinsVec = { sampleMins.x, sampleMins.y, sampleMins.z };
	const vec3_t sampleMaxsVec = { sampleMaxs.x, sampleMaxs.y, sampleMaxs.z };
	(void)gi.CM_BoxContents( sampleMinsVec, sampleMaxsVec, &boxContents, leafList.data(), ( int32_t )leafList.size(), &topNode );
	if ( ( boxContents & CONTENTS_SOLID ) != 0 ) {
		return false;
	}

	/**
	*    Publish the rasterized span metadata in pointer-free form so later serialization can consume it directly.
	**/
	out_span->span_id = span_id;
	out_span->floor_z = floorZ;
	out_span->ceiling_z = ceilingZ;
	out_span->preferred_z = floorZ;
	out_span->movement_flags = NAV_FLAG_WALKABLE;
	out_span->surface_flags = NAV_TILE_SUMMARY_NONE;
	out_span->connector_hint_mask = 0;
	if ( ( pointContents & CONTENTS_WATER ) != 0 || ( boxContents & CONTENTS_WATER ) != 0 ) {
		out_span->movement_flags |= NAV_FLAG_WATER;
		out_span->surface_flags |= NAV_TILE_SUMMARY_WATER;
	}
	if ( ( pointContents & CONTENTS_LAVA ) != 0 || ( boxContents & CONTENTS_LAVA ) != 0 ) {
		out_span->movement_flags |= NAV_FLAG_LAVA;
		out_span->surface_flags |= NAV_TILE_SUMMARY_LAVA;
	}
	if ( ( pointContents & CONTENTS_SLIME ) != 0 || ( boxContents & CONTENTS_SLIME ) != 0 ) {
		out_span->movement_flags |= NAV_FLAG_SLIME;
		out_span->surface_flags |= NAV_TILE_SUMMARY_SLIME;
	}
	SVG_Nav2_SpanGrid_ResolvePointTopology( standingCenter, out_span );
	return SVG_Nav2_SpanIsValid( *out_span );
}


/**
*
*
*	Nav2 Span Grid Public API:
*
*
**/
/**
*	@brief	Build a sparse span-grid from the currently published nav2 mesh using BSP-aware sampling.
*	@param	mesh		Nav2-owned runtime mesh providing world bounds and sizing metadata.
*	@param	out_grid	[out] Grid receiving the generated span data.
*	@return	True when at least a valid rasterization pass completed.
*	@note	This Milestone 4 pass intentionally performs one conservative floor/clearance sample per XY column.
**/
const bool SVG_Nav2_BuildSpanGridFromMesh( const nav2_mesh_t *mesh, nav2_span_grid_t *out_grid ) {
	/**
	*    Require both mesh storage and output storage before beginning any rasterization work.
	**/
	if ( !mesh || !out_grid ) {
		return false;
	}
	*out_grid = {};

	/**
	*    Reject rasterization when the active mesh is not published or the collision-model imports are unavailable.
	**/
	if ( !mesh->loaded || !gi.GetCollisionModel || !gi.trace || !gi.pointcontents || !gi.CM_BoxContents ) {
		return false;
	}

	/**
	*    Resolve the conservative rasterization parameters and world-space bounds first so later sampling stays deterministic.
	**/
	if ( !SVG_Nav2_ResolveSpanGridParameters( mesh, out_grid ) ) {
		return false;
	}
	BBox3 rasterBounds = mesh->world_bounds;
	if ( !SVG_Nav2_ResolveSpanGridWorldBounds( mesh, &rasterBounds ) ) {
		return false;
	}

	/**
	*    Derive the world-space XY raster domain from the resolved bounds and cell size.
	**/
	const double cellSizeXY = out_grid->cell_size_xy;
	if ( cellSizeXY <= 0.0 ) {
		return false;
	}
	const int32_t minCellX = ( int32_t )std::floor( rasterBounds.mins.x / cellSizeXY );
	const int32_t maxCellX = ( int32_t )std::ceil( rasterBounds.maxs.x / cellSizeXY );
	const int32_t minCellY = ( int32_t )std::floor( rasterBounds.mins.y / cellSizeXY );
	const int32_t maxCellY = ( int32_t )std::ceil( rasterBounds.maxs.y / cellSizeXY );

	/**
	*    Rasterize each XY column conservatively and append sparse columns only when the sampled point produced a traversable span.
	**/
	int32_t nextSpanId = 0;
	for ( int32_t cellY = minCellY; cellY < maxCellY; cellY++ ) {
		/**
		*    Walk the row in deterministic X order so later serialization and compare-mode tools observe stable column order.
		**/
		for ( int32_t cellX = minCellX; cellX < maxCellX; cellX++ ) {
			/**
			*    Build the world-space center point for the sampled XY column.
			**/
			const Vector3 columnCenter = {
				( float )( ( ( double )cellX + 0.5 ) * cellSizeXY ),
				( float )( ( ( double )cellY + 0.5 ) * cellSizeXY ),
				( float )( rasterBounds.maxs.z - ( NAV2_SPAN_GRID_CLEARANCE_PROBE * 0.25 ) )
			};

			/**
			*    Attempt one conservative span sample for the current XY column and skip empty space without emitting dead columns.
			**/
			nav2_span_t span = {};
			if ( !SVG_Nav2_SpanGrid_TryBuildSpanAtPoint( mesh, *out_grid, columnCenter, nextSpanId, &span ) ) {
				continue;
			}

			/**
			*    Append the owning sparse column only after a valid traversable span was produced.
			**/
			nav2_span_column_t &column = SVG_Nav2_SpanGrid_AppendColumn( mesh, out_grid, columnCenter );
			column.spans.push_back( span );
			nextSpanId++;
		}
	}

	/**
	*    Report success once the rasterization pass completed, even if the active bounds currently produced no traversable columns.
	**/
	return true;
}

/**
*	@brief	Build a sparse span-grid from the currently published nav2 runtime mesh.
*	@param	out_grid	[out] Grid receiving the generated span data.
*	@return	True when the active nav2 runtime mesh produced a rasterized span grid.
*	@note	This convenience wrapper keeps current callers simple while the runtime owns mesh publication.
**/
const bool SVG_Nav2_BuildSpanGrid( nav2_span_grid_t *out_grid ) {
	/**
	*    Resolve the currently published nav2 runtime mesh before delegating to the mesh-owned builder.
	**/
	const nav2_mesh_t *mesh = SVG_Nav2_Runtime_GetMesh();
	if ( !mesh ) {
		return false;
	}
	return SVG_Nav2_BuildSpanGridFromMesh( mesh, out_grid );
}
