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

//! Latest bounded diagnostics snapshot from the most recent span-grid build pass.
static nav2_span_grid_build_stats_t nav2_span_grid_last_build_stats = {};

/**
*	@brief	Resolve the runtime-origin lift needed to convert a feet/floor contact point into the actor origin used by collision bounds.
*	@param	agentMins	Agent local-space minimum bounds.
*	@return	Positive Z lift for center-origin hulls, or 0 for feet-origin hulls.
*	@note	Nav2 generation samples support floors in feet-contact space. Any later full-hull validation that operates in origin space
*			must lift that point by `-mins.z` whenever the hull extends below its origin.
**/
static inline float SVG_Nav2_SpanGrid_GetFeetToOriginOffsetZ( const Vector3 &agentMins ) {
	return agentMins.z < 0.0f ? -agentMins.z : 0.0f;
}

/**
*	@brief	Resolve the minimum standing clearance required for the active agent hull.
*	@param	agentMins	Agent local-space minimum bounds.
*	@param	agentMaxs	Agent local-space maximum bounds.
*	@return	Required walkable vertical clearance in world units.
*	@note	Older placeholder logic used a fixed 56-unit clearance that only matched a feet-origin style hull. Center-origin default
*			profiles such as `PHYS_DEFAULT_BBOX_STANDUP_MINS/MAXS` are taller, so the conservative clearance gate must honor the real
*			full hull height rather than silently deferring the rejection to the later solid-box test.
**/
static inline double SVG_Nav2_SpanGrid_GetRequiredStandingClearance( const Vector3 &agentMins, const Vector3 &agentMaxs ) {
	const double standingHeight = ( double )agentMaxs.z - ( double )agentMins.z;
	if ( standingHeight <= 0.0 ) {
		return NAV2_SPAN_GRID_MIN_CLEARANCE;
	}

	return std::max( NAV2_SPAN_GRID_MIN_CLEARANCE, standingHeight );
}


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
*	@brief	Resolve the inclusive-first sampled cell index whose center still lies inside one world-space bound.
*	@param	worldMin	World-space lower bound for one axis.
*	@param	cellSize	Resolved XY cell size.
*	@return	First sampled cell index whose center is not below the requested bound.
*	@note	Span sampling operates on cell centers at `(cell + 0.5) * cellSize`. Using raw `floor(min / cellSize)` expands the raster domain
*			by up to half a cell outside the BSP bounds, which is exactly how the first observed sample landed outside `world_bounds`.
**/
static inline int32_t SVG_Nav2_SpanGrid_GetMinCenteredCellIndex( const double worldMin, const double cellSize ) {
	if ( cellSize <= 0.0 ) {
		return 0;
	}

	return ( int32_t )std::ceil( ( worldMin / cellSize ) - 0.5 );
}

/**
*	@brief	Resolve the exclusive-end sampled cell index whose last center still lies inside one world-space bound.
*	@param	worldMax	World-space upper bound for one axis.
*	@param	cellSize	Resolved XY cell size.
*	@return	Exclusive end cell index suitable for `< maxCell` raster loops.
*	@note	This pairs with `SVG_Nav2_SpanGrid_GetMinCenteredCellIndex` so every emitted sample center satisfies
*			`worldMin <= center <= worldMax` on that axis.
**/
static inline int32_t SVG_Nav2_SpanGrid_GetMaxCenteredCellIndexExclusive( const double worldMax, const double cellSize ) {
	if ( cellSize <= 0.0 ) {
		return 0;
	}

	return ( int32_t )std::floor( ( worldMax / cellSize ) - 0.5 ) + 1;
}

/**
*	@brief	Resolve mathematical floor-division for possibly negative integer coordinates.
*	@param	value	Integer value to divide.
*	@param	divisor	Positive divisor.
*	@return	Floor-divided quotient, or 0 when divisor is invalid.
**/
static inline int32_t SVG_Nav2_SpanGrid_FloorDiv( const int32_t value, const int32_t divisor ) {
	/**
	*    Guard against invalid divisors so callers can keep deterministic fallback behavior.
	**/
	if ( divisor <= 0 ) {
		return 0;
	}

	/**
	*    Use floating-point floor to preserve expected tile ownership for negative cell coordinates.
	**/
	return ( int32_t )std::floor( ( double )value / ( double )divisor );
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
static nav2_span_column_t &SVG_Nav2_SpanGrid_AppendColumn( const nav2_mesh_t *mesh, nav2_span_grid_t *grid, const int32_t cell_x, const int32_t cell_y ) {
	/**
	*    Append an empty sparse column first so the function can return a stable reference.
	**/
	grid->columns.emplace_back();
	nav2_span_column_t &column = grid->columns.back();

	/**
	*    Persist sparse-column XY as world-cell coordinates so query-side tile-cell reconstruction can map
	*    each sampled column back to the correct tile-local cell slot.
	**/
	column.tile_x = cell_x;
	column.tile_y = cell_y;

	/**
	*    Resolve canonical world-tile ownership from world-cell coordinates and configured tile size.
	**/
	nav2_world_tile_key_t tileKey = {};
	tileKey.tile_x = SVG_Nav2_SpanGrid_FloorDiv( cell_x, grid->tile_size );
	tileKey.tile_y = SVG_Nav2_SpanGrid_FloorDiv( cell_y, grid->tile_size );
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
    const int32_t span_id, nav2_span_t *out_span, nav2_span_grid_build_stats_t *build_stats ) {
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
	Vector3 agentMins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
	Vector3 agentMaxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
	if ( mesh && mesh->agent_profile.maxs.x > mesh->agent_profile.mins.x ) {
		agentMins = mesh->agent_profile.mins;
		agentMaxs = mesh->agent_profile.maxs;
	}
	const float feetToOriginOffsetZ = SVG_Nav2_SpanGrid_GetFeetToOriginOffsetZ( agentMins );
	const double requiredStandingClearance = SVG_Nav2_SpanGrid_GetRequiredStandingClearance( agentMins, agentMaxs );

	/**
	*    Reject samples whose probe center already sits inside a solid leaf because they cannot produce a valid standing span.
	**/
	const cm_contents_t pointContents = gi.pointcontents( &column_center );
	if ( ( pointContents & CONTENTS_SOLID ) != 0 ) {
       if ( build_stats ) {
			build_stats->rejected_solid_point++;
		}
		return false;
	}

	/**
	*    Trace downward from above the sample center to find conservative standing support for the column.
	**/
	Vector3 floorProbeStart = column_center;
	floorProbeStart.z += ( float )( NAV2_SPAN_GRID_CLEARANCE_PROBE * 0.5 );
	Vector3 floorProbeEnd = column_center;
	floorProbeEnd.z -= ( float )NAV2_SPAN_GRID_FLOOR_PROBE;
	const Vector3 traceMins = { agentMins.x, agentMins.y, 0.0f };
	const Vector3 traceMaxs = { agentMaxs.x, agentMaxs.y, 0.0f };
	const cm_trace_t floorTrace = gi.trace( &floorProbeStart, &traceMins, &traceMaxs, &floorProbeEnd, nullptr, CM_CONTENTMASK_PLAYERSOLID );
	if ( floorTrace.startsolid || floorTrace.allsolid || floorTrace.fraction >= 1.0 ) {
       if ( build_stats ) {
			if ( floorTrace.startsolid || floorTrace.allsolid ) {
				build_stats->rejected_floor_trace++;
			} else {
				build_stats->rejected_floor_miss++;
			}
		}
		return false;
	}

	/**
	*    Require a meaningfully walkable plane normal so the first span pass does not accept walls or extreme slopes.
	**/
	const double minimumSlopeNormal = ( mesh && mesh->agent_profile.max_slope_normal_z > 0.0 ) ? mesh->agent_profile.max_slope_normal_z : PHYS_MAX_SLOPE_NORMAL;
 if ( floorTrace.plane.type != PLANE_Z && floorTrace.plane.normal[ 2 ] < minimumSlopeNormal ) {
		if ( build_stats ) {
			build_stats->rejected_slope++;
		}
		return false;
	}

	/**
	*    Probe upward from just above the detected floor so the first span pass can estimate conservative headroom.
	**/
	Vector3 clearanceProbeStart = floorTrace.endpos;
	clearanceProbeStart.z += ( float )NAV2_SPAN_GRID_TRACE_EPSILON;
	Vector3 clearanceProbeEnd = clearanceProbeStart;
	clearanceProbeEnd.z += ( float )NAV2_SPAN_GRID_CLEARANCE_PROBE;
	const cm_trace_t clearanceTrace = gi.trace( &clearanceProbeStart, &traceMins, &traceMaxs, &clearanceProbeEnd, nullptr, CM_CONTENTMASK_PLAYERSOLID );
	const double floorZ = floorTrace.endpos.z;
	const double ceilingZ = clearanceTrace.endpos.z;
	if ( ( ceilingZ - floorZ ) < requiredStandingClearance ) {
       if ( build_stats ) {
			build_stats->rejected_clearance++;
		}
		return false;
	}

	/**
	*    Validate the sampled standing volume using a real brush-level occupancy test first, then gather
	*    non-solid environment flags separately.
	*
	*    Runtime debugging confirmed that `CM_BoxContents` still reported `CONTENTS_SOLID` even after the
	*    validation hull was inset one unit above the support plane. That behavior is expected from the
	*    current collision-model implementation: `CM_BoxContents` recursively accumulates the OR of every
	*    touched BSP leaf's `leaf->contents`, which means merely spanning a neighboring solid leaf can mark
	*    the entire sample as solid even when the hull does not actually penetrate any brush volume.
	*
	*    For binary occupancy validation we therefore rely on an in-place zero-length `gi.trace`, which
	*    executes the same brush-level hull test used by gameplay movement. We keep the separate box-contents
	*    query only for broad environment classification (water/slime/lava), and explicitly ignore its solid
	*    bit because solid rejection is already handled by the trace result.
	**/
	cm_contents_t boxContents = CONTENTS_NONE;
	std::array<mleaf_t *, 16> leafList = {};
	mnode_t *topNode = nullptr;
	Vector3 sampleMins = QM_Vector3Zero();
	Vector3 sampleMaxs = QM_Vector3Zero();
	Vector3 validationMins = agentMins;
	Vector3 validationMaxs = agentMaxs;
	const Vector3 standingOrigin = floorTrace.endpos;
	Vector3 standingOriginForBox = standingOrigin;

	/**
	*    Recenter floor-contact origins for box-contents validation using the hull's authored Z origin.
	*
	*    `standingOrigin` is expressed in floor-contact space (feet on floor). `CM_BoxContents`, however,
	*    expects true world-space mins/maxs for the actor hull at its runtime origin.
	*
	*    For any hull with negative mins.z (classic center-origin or asymmetric center-ish setups),
	*    the runtime origin must be lifted by `-mins.z` so that:
	*      worldMins.z == floorZ
	*      worldMaxs.z == floorZ + (maxs.z - mins.z)
	*
	*    This formulation is intentionally robust for asymmetric profiles (e.g. mins.z != -maxs.z).
	*    A half-height shift can over-lift such hulls and produce false `CONTENTS_SOLID` rejections,
	*    which in turn collapses span emission (`rejected_solid_box`) even when floor/clearance probes
	*    are otherwise valid.
	*
	*    Feet-origin hulls (mins.z >= 0) are left unchanged because their origin is already floor-relative.
	**/
	standingOriginForBox.z += feetToOriginOffsetZ;

	sampleMins = QM_Vector3Add( standingOriginForBox, agentMins );
	sampleMaxs = QM_Vector3Add( standingOriginForBox, agentMaxs );

	/**
	*    Inset the vertical validation bounds slightly away from exact support and ceiling planes before
	*    querying `CM_BoxContents`.
	*
	*    The floor and clearance traces above already validated that the sampled column has real support and
	*    enough standing height for the full authored hull. The remaining `CM_BoxContents` pass is meant to
	*    detect true volume penetration and liquid membership. When the validation AABB is left exactly flush
	*    with the floor plane (`sampleMins.z == floorZ`), the collision system can conservatively report
	*    `CONTENTS_SOLID` for simple touching contact, which falsely rejects otherwise walkable columns.
	*
	*    A symmetric Z inset preserves the authored XY footprint and keeps the validation hull strictly inside
	*    the already-approved standing slab, avoiding boundary-contact false positives at both the floor and
	*    the ceiling without weakening the real clearance requirement.
	**/
	const float verticalValidationInset = ( float )NAV2_SPAN_GRID_TRACE_EPSILON;
	const float sampleHeight = sampleMaxs.z - sampleMins.z;
	if ( verticalValidationInset > 0.0f && sampleHeight > ( verticalValidationInset * 2.0f ) ) {
		sampleMins.z += verticalValidationInset;
		sampleMaxs.z -= verticalValidationInset;
		validationMins.z += verticalValidationInset;
		validationMaxs.z -= verticalValidationInset;
	}

	/**
	*    Run an in-place hull trace to validate true brush penetration at the sampled standing origin.
	*
	*    Using identical start/end points forces the trace code down its position-test path, which answers the
	*    question nav generation actually cares about here: "would this hull start embedded in solid at this
	*    standing origin?" That is stricter than point contents, but much less falsely conservative than the
	*    leaf-OR semantics of `CM_BoxContents` for solid classification.
	**/
	const cm_trace_t occupancyTrace = gi.trace( &standingOriginForBox, &validationMins, &validationMaxs,
		&standingOriginForBox, nullptr, CM_CONTENTMASK_PLAYERSOLID );
	if ( occupancyTrace.startsolid || occupancyTrace.allsolid ) {
		if ( build_stats ) {
			build_stats->rejected_solid_box++;
		}
		return false;
	}

    const vec3_t sampleMinsVec = { sampleMins.x, sampleMins.y, sampleMins.z };
	const vec3_t sampleMaxsVec = { sampleMaxs.x, sampleMaxs.y, sampleMaxs.z };
	(void)gi.CM_BoxContents( sampleMinsVec, sampleMaxsVec, &boxContents, leafList.data(), ( int32_t )leafList.size(), &topNode );
	boxContents = ( cm_contents_t )( boxContents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) );

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
       if ( build_stats ) {
			build_stats->spans_with_water++;
		}
	}
	if ( ( pointContents & CONTENTS_LAVA ) != 0 || ( boxContents & CONTENTS_LAVA ) != 0 ) {
		out_span->movement_flags |= NAV_FLAG_LAVA;
		out_span->surface_flags |= NAV_TILE_SUMMARY_LAVA;
       if ( build_stats ) {
			build_stats->spans_with_lava++;
		}
	}
	if ( ( pointContents & CONTENTS_SLIME ) != 0 || ( boxContents & CONTENTS_SLIME ) != 0 ) {
		out_span->movement_flags |= NAV_FLAG_SLIME;
		out_span->surface_flags |= NAV_TILE_SUMMARY_SLIME;
       if ( build_stats ) {
			build_stats->spans_with_slime++;
		}
	}
	/**
	*    Resolve BSP topology from the runtime origin used by the collision hull rather than the exact floor-contact point.
	*
	*    This keeps center-origin agents off floor planes and other tile/cell boundaries during topology lookup, which reduces
	*    ambiguous leaf/cluster/area classification for otherwise valid standing samples.
	**/
	SVG_Nav2_SpanGrid_ResolvePointTopology( standingOriginForBox, out_span );
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
const bool SVG_Nav2_BuildSpanGridFromMesh( const nav2_mesh_t *mesh, nav2_span_grid_t *out_grid,
	nav2_span_grid_build_stats_t *out_stats ) {
	/**
	*    Require both mesh storage and output storage before beginning any rasterization work.
	**/
	if ( !mesh || !out_grid ) {
		return false;
	}
	*out_grid = {};
	nav2_span_grid_build_stats_t buildStats = {};

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
	const int32_t minCellX = SVG_Nav2_SpanGrid_GetMinCenteredCellIndex( rasterBounds.mins.x, cellSizeXY );
	const int32_t maxCellX = SVG_Nav2_SpanGrid_GetMaxCenteredCellIndexExclusive( rasterBounds.maxs.x, cellSizeXY );
	const int32_t minCellY = SVG_Nav2_SpanGrid_GetMinCenteredCellIndex( rasterBounds.mins.y, cellSizeXY );
	const int32_t maxCellY = SVG_Nav2_SpanGrid_GetMaxCenteredCellIndexExclusive( rasterBounds.maxs.y, cellSizeXY );
	if ( maxCellX <= minCellX || maxCellY <= minCellY ) {
		return false;
	}

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
			*    Count every sampled XY column so diagnostics can report raster-domain size independent of emitted spans.
			**/
			buildStats.sampled_columns++;

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
         if ( !SVG_Nav2_SpanGrid_TryBuildSpanAtPoint( mesh, *out_grid, columnCenter, nextSpanId, &span, &buildStats ) ) {
				continue;
			}

			/**
			*    Append the owning sparse column only after a valid traversable span was produced.
			**/
			nav2_span_column_t &column = SVG_Nav2_SpanGrid_AppendColumn( mesh, out_grid, cellX, cellY );
			column.spans.push_back( span );
           buildStats.emitted_columns++;
			buildStats.emitted_spans++;
			nextSpanId++;
		}
	}

	/**
	*	Rebuild reverse indices after rasterization so topology-local lookup helpers can consume stable pointer-free memberships.
	**/
	(void)SVG_Nav2_SpanGrid_RebuildReverseIndices( out_grid );

	/**
	*    Publish diagnostics snapshots for optional callers and deferred debug reporting.
	**/
	nav2_span_grid_last_build_stats = buildStats;
	if ( out_stats ) {
		*out_stats = buildStats;
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
    return SVG_Nav2_BuildSpanGridFromMesh( mesh, out_grid, nullptr );
}

/**
*	@brief	Return bounded diagnostics from the most recent span-grid build pass.
*	@param	out_stats	[out] Receives the latest builder diagnostics snapshot.
*	@return	True when diagnostics were copied.
**/
const bool SVG_Nav2_GetLastSpanGridBuildStats( nav2_span_grid_build_stats_t *out_stats ) {
	/**
	*    Require output storage before copying diagnostics.
	**/
	if ( !out_stats ) {
		return false;
	}

	/**
	*    Copy the latest bounded diagnostics snapshot.
	**/
	*out_stats = nav2_span_grid_last_build_stats;
	return true;
}
