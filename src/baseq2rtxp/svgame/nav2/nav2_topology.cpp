/********************************************************************
*
*
*	ServerGame: Nav2 Topology Module Anchor - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_topology.h"

#include "svgame/nav2/nav2_query_iface.h"
#include "svgame/nav2/nav2_runtime.h"


/**
*
*
*	Nav2 Topology Local Helpers:
*
*
**/
/**
*	@brief	Accumulate bounded topology summary counts from one sparse span.
*	@param	span		Source sparse span being classified.
*	@param	summary	[in,out] Summary receiving feature-class counts.
**/
static void SVG_Nav2_Topology_AccumulateSpanSummary( const nav2_span_t &span, nav2_topology_summary_t *summary ) {
	/**
	*	Require output storage before touching counters.
	**/
	if ( !summary ) {
		return;
	}

	/**
	*	Every valid traversable span contributes at least one floor-style classified location.
	**/
	summary->floor_count++;

	/**
	*	Accumulate normalized feature classes from source span metadata.
	**/
	if ( ( span.topology_flags & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0
		|| ( span.surface_flags & NAV_TILE_SUMMARY_STAIR ) != 0 )
	{
		summary->stair_count++;
	}
	if ( ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LADDER ) != 0
		|| ( span.movement_flags & NAV_FLAG_LADDER ) != 0
		|| ( span.surface_flags & NAV_TILE_SUMMARY_LADDER ) != 0 )
	{
		summary->ladder_count++;
	}
	if ( ( span.connector_hint_mask & NAV2_CONNECTOR_KIND_PORTAL ) != 0
		|| ( span.connector_hint_mask & NAV2_CONNECTOR_KIND_OPENING ) != 0 )
	{
		summary->portal_barrier_count++;
	}
	if ( ( span.connector_hint_mask & ( NAV2_CONNECTOR_KIND_MOVER_BOARDING | NAV2_CONNECTOR_KIND_MOVER_RIDE | NAV2_CONNECTOR_KIND_MOVER_EXIT ) ) != 0 ) {
		summary->mover_count++;
	}
	if ( ( span.surface_flags & NAV_TILE_SUMMARY_WALK_OFF ) != 0 ) {
		summary->non_walk_off_count++;
	}
	if ( span.occupancy.hard_block || span.occupancy.soft_cost > 0.0 ) {
		summary->dynamic_occupancy_count++;
	}
}


/**
*	@brief	Keep the nav2 module wired into the build while the rewrite starts.
*	@note	The anchor intentionally performs no work yet.
**/
void SVG_Nav2_Topology_ModuleAnchor( void ) {
}

/**
*	@brief	Validate that a topology artifact contains enough metadata for downstream stage ownership.
*	@param	artifact	Topology artifact to inspect.
*	@return	True when the artifact is ready and exposes basic classified topology counts.
**/
const bool SVG_Nav2_Topology_ValidateArtifact( const nav2_topology_artifact_t &artifact ) {
	/**
	*	Require a ready status, explicit readiness marker, and usable tile sizing publication.
	**/
	if ( artifact.status != nav2_topology_status_t::Ready || !artifact.ready || !artifact.has_tile_sizing ) {
		return false;
	}

	/**
	*	Require a stable static-nav version so cached topology ownership can be invalidated deterministically.
	**/
	if ( artifact.static_nav_version == 0 ) {
		return false;
	}

	/**
	*	Require at least one published tile or sparse column so the artifact represents meaningful classified data.
	**/
	if ( artifact.world_tile_count <= 0 && artifact.span_column_count <= 0 ) {
		return false;
	}

	return true;
}

/**
*	@brief	Build a lightweight topology artifact from the currently published nav2 runtime state.
*	@param	out_artifact	[out] Topology artifact receiving snapshot-scoped publication metadata.
*	@param	out_summary	[out] Optional bounded semantic summary for diagnostics and milestone validation.
*	@return	True when the topology artifact was built successfully.
*	@note	This initial publication seam intentionally summarizes the currently published mesh and span-grid
*			state without yet taking full scheduler-stage ownership of per-job classified locations.
**/
const bool SVG_Nav2_Topology_BuildArtifact( nav2_topology_artifact_t *out_artifact, nav2_topology_summary_t *out_summary ) {
	/**
	*	Require output storage before publishing artifact state.
	**/
	if ( !out_artifact ) {
		return false;
	}

	*out_artifact = {};
	if ( out_summary ) {
		*out_summary = {};
	}

	/**
	*	Require an initialized query mesh and usable tile sizing before building publication metadata.
	**/
	const nav2_query_mesh_t *queryMesh = SVG_Nav2_GetQueryMesh();
	if ( !queryMesh || !queryMesh->IsValid() || !queryMesh->main_mesh || !queryMesh->main_mesh->loaded ) {
		out_artifact->status = nav2_topology_status_t::Failed;
		return false;
	}

	const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( queryMesh );
	if ( !meshMeta.HasTileSizing() ) {
		out_artifact->status = nav2_topology_status_t::Failed;
		return false;
	}

	/**
	*	Populate artifact metadata from the currently published runtime mesh and optional span-grid snapshot.
	**/
	const nav2_mesh_t *runtimeMesh = SVG_Nav2_Runtime_GetMesh();
	if ( !runtimeMesh ) {
		out_artifact->status = nav2_topology_status_t::Failed;
		return false;
	}

	out_artifact->static_nav_version = runtimeMesh->serialized_format_version;
	out_artifact->world_tile_count = ( int32_t )runtimeMesh->world_tiles.size();
	out_artifact->has_tile_sizing = meshMeta.HasTileSizing();
	out_artifact->status = nav2_topology_status_t::Partial;

	/**
	*	Summarize the currently published span-grid when available so future scheduler ownership has
	*	a deterministic publication seam to consume.
	**/
	if ( runtimeMesh->span_grid ) {
		out_artifact->span_column_count = ( int32_t )runtimeMesh->span_grid->columns.size();
		for ( const nav2_span_column_t &column : runtimeMesh->span_grid->columns ) {
			for ( const nav2_span_t &span : column.spans ) {
				out_artifact->span_count++;
				SVG_Nav2_Topology_AccumulateSpanSummary( span, out_summary );
			}
		}
	}

	/**
	*	Mark the artifact ready only when the lightweight publication contract validates successfully.
	**/
	out_artifact->ready = true;
	out_artifact->status = nav2_topology_status_t::Ready;
	if ( !SVG_Nav2_Topology_ValidateArtifact( *out_artifact ) ) {
		out_artifact->ready = false;
		out_artifact->status = nav2_topology_status_t::Failed;
		return false;
	}

	return true;
}

/**
*	@brief	Return whether the nav2 topology layer currently has enough published runtime data to be meaningful.
*	@return	True when a query mesh is published and exposes usable tile sizing metadata.
*	@note	This helper is intentionally lightweight and does not trigger generation/rebuild work.
**/
const bool SVG_Nav2_Topology_IsReady( void ) {
	/**
	*	Require an active query mesh publication before topology-dependent work can run.
	**/
	const nav2_query_mesh_t *queryMesh = SVG_Nav2_GetQueryMesh();
	if ( !queryMesh || !queryMesh->IsValid() || !queryMesh->main_mesh ) {
		return false;
	}

	/**
	*	Require loaded mesh state so topology callers do not treat placeholder ownership-only
	*	mesh allocations as usable navigation topology.
	**/
	if ( !queryMesh->main_mesh->loaded ) {
		return false;
	}

	/**
	*	Require tile sizing metadata from the query seam.
	**/
	const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( queryMesh );
	if ( !meshMeta.HasTileSizing() ) {
		return false;
	}

	return true;
}
