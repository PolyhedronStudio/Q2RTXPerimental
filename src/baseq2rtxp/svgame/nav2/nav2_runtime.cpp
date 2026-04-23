/********************************************************************
*
*
*	ServerGame: Nav2 Runtime Ownership Seam - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_runtime.h"
#include "svgame/nav2/nav2_format.h"
#include "svgame/nav2/nav2_span_grid_build.h"

#include <cmath>

#include "common/bsp.h"
#include "shared/cm/cm_model.h"

//! Nav2-owned runtime mesh published through the runtime seam.
nav2_mesh_raii_t g_nav_mesh = {};

//! Default nav2 tile edge size used when runtime generation has no prior sizing metadata.
static constexpr int32_t NAV2_RUNTIME_GENERATE_DEFAULT_TILE_SIZE = 8;
//! Default nav2 XY cell size used during runtime generation.
static constexpr double NAV2_RUNTIME_GENERATE_DEFAULT_CELL_SIZE_XY = 32.0;
//! Default nav2 Z quantization used during runtime generation.
static constexpr double NAV2_RUNTIME_GENERATE_DEFAULT_Z_QUANT = 16.0;


/**
*
*
*	Nav2 Runtime Local Helpers:
*
*
**/
/**
*	@brief	Populate runtime mesh sizing and agent defaults for generation.
*	@param	mesh	[in,out] Runtime mesh being generated.
**/
static void SVG_Nav2_Runtime_ApplyGenerationDefaults( nav2_mesh_t *mesh ) {
	/**
	*    Require mesh storage before applying defaults.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	*    Apply deterministic tile sizing defaults used by nav2 span-grid generation.
	**/
	mesh->tile_size = NAV2_RUNTIME_GENERATE_DEFAULT_TILE_SIZE;
	mesh->cell_size_xy = NAV2_RUNTIME_GENERATE_DEFAULT_CELL_SIZE_XY;
	mesh->z_quant = NAV2_RUNTIME_GENERATE_DEFAULT_Z_QUANT;

	/**
	*    Apply conservative humanoid traversal defaults so generation has a complete agent profile.
	**/
	mesh->agent_profile.mins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
	mesh->agent_profile.maxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
	mesh->agent_profile.max_step_height = NAV_DEFAULT_STEP_MAX_SIZE;
	mesh->agent_profile.max_drop_height = NAV_DEFAULT_MAX_DROP_HEIGHT;
	mesh->agent_profile.max_drop_height_cap = NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;
	mesh->agent_profile.max_slope_normal_z = NAV_DEFAULT_MAX_SLOPE_NORMAL;
}

/**
*	@brief	Resolve world bounds from the active collision BSP cache.
*	@param	out_world_bounds	[out] Resolved world bounds.
*	@return	True when active world bounds were resolved.
**/
static const bool SVG_Nav2_Runtime_ResolveWorldBounds( BBox3 *out_world_bounds ) {
	/**
	*    Require output storage before resolving runtime world bounds.
	**/
	if ( !out_world_bounds || !gi.GetCollisionModel ) {
		return false;
	}

	/**
	*    Require an active collision model and BSP cache to derive map bounds.
	**/
	const cm_t *collisionModel = gi.GetCollisionModel();
	if ( !collisionModel || !collisionModel->cache || !collisionModel->cache->models ) {
		return false;
	}

	/**
	*    Use BSP world model bounds as the authoritative active server map bounds.
	**/
	const mmodel_t &worldModel = collisionModel->cache->models[ 0 ];
	if ( worldModel.maxs[ 0 ] <= worldModel.mins[ 0 ] || worldModel.maxs[ 1 ] <= worldModel.mins[ 1 ] || worldModel.maxs[ 2 ] <= worldModel.mins[ 2 ] ) {
		return false;
	}

	/**
	*    Publish resolved world bounds into nav2 BBox form.
	**/
	out_world_bounds->mins = { worldModel.mins[ 0 ], worldModel.mins[ 1 ], worldModel.mins[ 2 ] };
	out_world_bounds->maxs = { worldModel.maxs[ 0 ], worldModel.maxs[ 1 ], worldModel.maxs[ 2 ] };
	return true;
}

/**
*	@brief	Populate canonical world-tile storage for the runtime mesh from world bounds.
*	@param	mesh	[in,out] Runtime mesh receiving canonical world tiles.
*	@return	True when at least one world tile was generated.
**/
static const bool SVG_Nav2_Runtime_BuildWorldTiles( nav2_mesh_t *mesh ) {
	/**
	*    Require mesh storage and valid sizing metadata before deriving tiles.
	**/
	if ( !mesh || mesh->tile_size <= 0 || mesh->cell_size_xy <= 0.0 ) {
		return false;
	}

	/**
	*    Derive world-tile span from active world bounds and tile world size.
	**/
	const double tileWorldSize = ( double )mesh->tile_size * mesh->cell_size_xy;
	if ( tileWorldSize <= 0.0 ) {
		return false;
	}

	const int32_t minTileX = ( int32_t )std::floor( mesh->world_bounds.mins.x / tileWorldSize );
	const int32_t minTileY = ( int32_t )std::floor( mesh->world_bounds.mins.y / tileWorldSize );
	const int32_t maxTileX = ( int32_t )std::floor( mesh->world_bounds.maxs.x / tileWorldSize );
	const int32_t maxTileY = ( int32_t )std::floor( mesh->world_bounds.maxs.y / tileWorldSize );

	if ( maxTileX < minTileX || maxTileY < minTileY ) {
		return false;
	}

	/**
	*    Reserve deterministic tile capacity before appending canonical tile records.
	**/
	const int32_t tileCountX = ( maxTileX - minTileX ) + 1;
	const int32_t tileCountY = ( maxTileY - minTileY ) + 1;
	const int64_t totalTileCount = ( int64_t )tileCountX * ( int64_t )tileCountY;
	if ( totalTileCount <= 0 ) {
		return false;
	}

	mesh->world_tiles.reserve( ( size_t )totalTileCount );

	/**
	*    Emit deterministic canonical world tiles and tile-id lookup map.
	**/
	int32_t nextTileId = 0;
	for ( int32_t tileY = minTileY; tileY <= maxTileY; tileY++ ) {
		for ( int32_t tileX = minTileX; tileX <= maxTileX; tileX++ ) {
			nav2_tile_t tile = {};
			tile.tile_x = tileX;
			tile.tile_y = tileY;
			tile.tile_id = nextTileId++;
			tile.region_id = NAV_REGION_ID_NONE;
			tile.cluster_key.tile_x = tileX;
			tile.cluster_key.tile_y = tileY;

			const double tileMinX = ( double )tileX * tileWorldSize;
			const double tileMinY = ( double )tileY * tileWorldSize;
			const double tileMaxX = tileMinX + tileWorldSize;
			const double tileMaxY = tileMinY + tileWorldSize;
			tile.world_bounds.mins = { ( float )tileMinX, ( float )tileMinY, mesh->world_bounds.mins.z };
			tile.world_bounds.maxs = { ( float )tileMaxX, ( float )tileMaxY, mesh->world_bounds.maxs.z };

			mesh->world_tiles.push_back( tile );
			nav2_world_tile_key_t tileKey = {};
			tileKey.tile_x = tileX;
			tileKey.tile_y = tileY;
			mesh->world_tile_id_of[ tileKey ] = tile.tile_id;
		}
	}

	return !mesh->world_tiles.empty();
}

/**
*	@brief	Return the currently published nav2-owned navigation mesh.
*	@return	Pointer to the active nav2-owned mesh, or `nullptr` when no mesh is available.
*	@note	This returns the nav2-owned runtime container rather than the `main mesh` owner.
**/
nav2_mesh_t *SVG_Nav2_Runtime_GetMesh( void ) {
	// Return the currently published nav2-owned runtime mesh.
	return g_nav_mesh.get();
}

/**
*	@brief	Release the currently published navigation mesh through the nav2 ownership seam.
*	@note	This clears only nav2-owned mesh state.
**/
void SVG_Nav2_Runtime_FreeMesh( void ) {
	// Reset the nav2-owned mesh placeholder and release its owned storage.
	if ( g_nav_mesh || !g_nav_mesh ) {
		g_nav_mesh = nav2_mesh_raii_t{};
	}
}


/**
*	@brief	Initialize the staged navigation runtime through the nav2 ownership seam.
*	@note	This currently ensures the nav2-owned mesh placeholder exists.
**/
void SVG_Nav2_Runtime_Init( void ) {
	// Allocate the nav2-owned mesh placeholder if it does not exist yet.
    if ( g_nav_mesh.get() == nullptr ) {
		g_nav_mesh.create( TAG_SVGAME_NAVMESH );
	}
}

/**
*	@brief	Shutdown the staged navigation runtime through the nav2 ownership seam.
*	@note	This currently tears down only nav2-owned mesh state.
**/
void SVG_Nav2_Runtime_Shutdown( void ) {
	// Release nav2-owned mesh state during shutdown.
	SVG_Nav2_Runtime_FreeMesh();
}

/**
*	@brief	Load the staged navigation mesh for the current level through the nav2 ownership seam.
*	@param	levelName	Level name used to resolve the cached navigation asset path.
*	@return	Tuple containing success state and the resolved game-path filename used for the load attempt.
*	@note	This is currently a temporary nav2 ownership seam and does not depend on the oldnav folder.
**/
const std::tuple<const bool, const std::string> SVG_Nav2_Runtime_LoadMesh( const char *levelName ) {
	// Validate the caller-provided level name before reporting a load attempt.
	if ( !levelName || levelName[ 0 ] == '\0' ) {
		return std::make_tuple( false, std::string() );
	}

	// Create a nav2-owned mesh placeholder so the ownership seam is exercised without legacy dependencies.
	if ( !g_nav_mesh ) {
		g_nav_mesh.create( TAG_SVGAME_NAVMESH );
	}

	// Mark the placeholder mesh as loaded so query callers have a stable owned object to bind to.
	g_nav_mesh->loaded = true;
	g_nav_mesh->serialized_format_version = NAV_SERIALIZED_FORMAT_VERSION;
	return std::make_tuple( true, std::string( levelName ) );
}

/**
*	@brief	Serialize the currently published navigation mesh through the nav2 ownership seam.
*	@param	levelName	Level name used to resolve the cached navigation asset path.
*	@return	True when the current mesh was serialized successfully.
*	@note	This temporary seam only reports success when a nav2-owned mesh exists.
**/
const bool SVG_Nav2_Runtime_SaveMesh( const char *levelName ) {
	// Validate the caller-provided level name and require an owned mesh to exist before saving.
    if ( !levelName || levelName[ 0 ] == '\0' || g_nav_mesh.get() == nullptr ) {
		return false;
	}

	// The real persistence path will be filled in after the nav2 data model replaces the scaffold.
	return true;
}

/**
*	@brief	Generate a nav2 mesh for the currently loaded server BSP through the runtime seam.
*	@param	out_span_build_stats	[out] Optional bounded span-grid generation diagnostics.
*	@return	True when generation completed and published a loaded nav2 runtime mesh.
**/
const bool SVG_Nav2_Runtime_GenerateMesh( nav2_span_grid_build_stats_t *out_span_build_stats ) {
	/**
	*    Require active collision BSP services before generation.
	**/
	if ( !gi.GetCollisionModel || !gi.BSP_PointLeaf || !gi.CM_BoxContents || !gi.trace || !gi.pointcontents ) {
		if ( out_span_build_stats ) {
			*out_span_build_stats = {};
		}
		return false;
	}

	/**
	*    Ensure nav2 runtime mesh ownership exists before generation.
	**/
	if ( !g_nav_mesh || !g_nav_mesh.get() ) {
		g_nav_mesh.create( TAG_SVGAME_NAVMESH );
	}
	if ( !g_nav_mesh.get() ) {
		if ( out_span_build_stats ) {
			*out_span_build_stats = {};
		}
		return false;
	}
	g_nav_mesh->span_grid.reset();

	/**
	*    Reset runtime mesh and apply deterministic generation defaults.
	**/
	g_nav_mesh->ResetMesh();
	SVG_Nav2_Runtime_ApplyGenerationDefaults( g_nav_mesh.get() );

	/**
	*    Resolve active BSP world bounds from collision model cache.
	**/
	auto cm = gi.GetCollisionModel();

	Vector3 wMins = cm->cache->models[ 0 ].mins;
	Vector3 wMaxs = cm->cache->models[ 0 ].maxs;// , cm->
	BBox3 worldBounds = { wMins, wMaxs }; //{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	if ( !SVG_Nav2_Runtime_ResolveWorldBounds( &worldBounds ) ) {
		if ( out_span_build_stats ) {
			*out_span_build_stats = {};
		}
		return false;
	}
	g_nav_mesh->world_bounds = worldBounds;


	/**
	*    Build canonical world tile storage for the active BSP map bounds.
	**/
	if ( !SVG_Nav2_Runtime_BuildWorldTiles( g_nav_mesh.get() ) ) {
		if ( out_span_build_stats ) {
			*out_span_build_stats = {};
		}
		return false;
	}

	/**
	*    Mark the mesh as actively published before span-grid generation so the build path can
	*    run against this runtime-owned mesh instance.
	**/
	g_nav_mesh->loaded = true;
	g_nav_mesh->serialized_format_version = NAV_SERIALIZED_FORMAT_VERSION;

	/**
	*    Trigger span-grid generation pass as the runtime generation proof for active BSP content.
	**/
	g_nav_mesh->span_grid = std::make_shared<nav2_span_grid_t>();
	nav2_span_grid_build_stats_t spanBuildStats = {};
	if ( !SVG_Nav2_BuildSpanGridFromMesh( g_nav_mesh.get(), g_nav_mesh->span_grid.get(), &spanBuildStats ) ) {
		/**
		*    Roll back published load state when generation fails so callers do not observe a
		*    partially initialized runtime mesh.
		**/
		g_nav_mesh->loaded = false;
		g_nav_mesh->serialized_format_version = 0;
		g_nav_mesh->span_grid.reset();

		if ( out_span_build_stats ) {
			*out_span_build_stats = spanBuildStats;
		}
		return false;
	}

	/**
	*    Treat an empty span-grid as a failed generation because the mesh would otherwise be
	*    published as "loaded" despite containing no usable nav data.
	**/
	if ( !g_nav_mesh->span_grid || g_nav_mesh->span_grid->columns.empty() ) {
		g_nav_mesh->loaded = false;
		g_nav_mesh->serialized_format_version = 0;
		g_nav_mesh->span_grid.reset();
		if ( out_span_build_stats ) {
			*out_span_build_stats = spanBuildStats;
		}
		return false;
	}

	/**
	*    Return bounded generation diagnostics when requested.
	**/
	if ( out_span_build_stats ) {
		*out_span_build_stats = spanBuildStats;
	}

	return true;
}
