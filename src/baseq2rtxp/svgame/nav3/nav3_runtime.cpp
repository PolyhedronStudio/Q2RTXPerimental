/********************************************************************
*
*
*    ServerGame: Nav3 Runtime Ownership Seam - Implementation
*
*
********************************************************************/
#include "svgame/nav3/nav3_runtime.h"
#include "svgame/nav3/nav3_debug_draw.h"
#include "svgame/nav3/nav3_persistence.h"

#include <limits>
#include <utility>


/**
*
*
*    Nav3 Runtime Local State:
*
*
**/
//! Compact nav3 runtime status state.
static nav3_runtime_status_t s_nav3_runtime_status = {};
//! Runtime-owned stage-5 generated sparse span mesh.
static nav3_generated_mesh_t s_nav3_runtime_generated_mesh = {};
//! Bounded diagnostics snapshot from the most recent stage-5 generation attempt.
static nav3_generation_stats_t s_nav3_runtime_last_generation_stats = {};
//! Runtime nav3 build configuration summary used by status diagnostics.
static nav3_build_config_t s_nav3_runtime_build_config = {};
//! Cvar controlling whether nav3 backend routing is enabled.
static cvar_t *s_nav3_enable = nullptr;
//! Cvar controlling top-level nav3 debug behavior.
static cvar_t *s_nav3_debug = nullptr;


/**
*
*
*    Nav3 Runtime Local Helpers:
*
*
**/
/**
*    @brief  Bump one generation counter while preserving zero as uninitialized when possible.
*    @param  value  Counter to increment.
*    @return Updated counter value.
**/
static uint32_t Nav3_Runtime_BumpGeneration( uint32_t *value ) {
	/**
	*    Require writable counter storage before incrementing.
	**/
	if ( !value ) {
		return 0;
	}

	// Increment the generation counter and avoid wrapping to zero.
	( *value )++;
	if ( *value == 0 ) {
		*value = 1;
	}
	return *value;
}

/**
*    @brief  Register nav3 stage-1 cvars once.
**/
static void Nav3_Runtime_RegisterCvars( void ) {
	/**
	*    Register nav3 enable and debug cvars exactly once per module lifetime.
	**/
	if ( !s_nav3_enable ) {
		s_nav3_enable = gi.cvar( "s_nav3_enable", "0", 0 );
	}
	if ( !s_nav3_debug ) {
		s_nav3_debug = gi.cvar( "nav3_debug", "0", 0 );
	}
}

/**
*    @brief  Initialize default stage-3 build settings and derive quantized cell thresholds.
**/
static void Nav3_Runtime_InitializeBuildConfig( void ) {
	// Start from deterministic defaults and derive quantized walkable-cell thresholds.
	s_nav3_runtime_build_config = {};
	s_nav3_runtime_build_config.cell_size = 16.0f;
	s_nav3_runtime_build_config.cell_height = 8.0f;
	s_nav3_runtime_build_config.agent_height_units = 56.0f;
	s_nav3_runtime_build_config.agent_step_height_units = 18.0f;
	s_nav3_runtime_build_config.agent_radius_units = 16.0f;

	// Keep derived fields consistent even if future defaults are edited.
	if ( !SVG_Nav3_DeriveBuildConfigCells( &s_nav3_runtime_build_config ) ) {
		// Fall back to safe non-zero quantized defaults when derivation fails unexpectedly.
		s_nav3_runtime_build_config.walkable_height_cells = 1;
		s_nav3_runtime_build_config.walkable_climb_cells = 0;
		s_nav3_runtime_build_config.walkable_radius_cells = 1;
	}
}

/**
*    @brief  Resolve active runtime map identity name.
*    @return Stable map name string used by persistence compatibility checks.
**/
static const char *Nav3_Runtime_ResolveActiveMapName( void ) {
	return ( level.mapname[ 0 ] != '\0' ) ? level.mapname : "unknown";
}

/**
*    @brief  Count spans across all sparse generated columns.
*    @param  generatedMesh  Generated sparse mesh payload.
*    @return Total span count clamped to uint32_t range.
**/
static uint32_t Nav3_Runtime_CountGeneratedSpans( const nav3_generated_mesh_t &generatedMesh ) {
	uint64_t spanCount = 0;
	for ( const nav3_generated_column_t &column : generatedMesh.columns ) {
		spanCount += static_cast<uint64_t>( column.spans.size() );
	}

	if ( spanCount > static_cast<uint64_t>( std::numeric_limits<uint32_t>::max() ) ) {
		return std::numeric_limits<uint32_t>::max();
	}

	return static_cast<uint32_t>( spanCount );
}

/**
*    @brief  Clear runtime-owned generated span mesh and reset last generation diagnostics.
**/
static void Nav3_Runtime_ClearGeneratedMesh( void ) {
	s_nav3_runtime_generated_mesh = {};
	s_nav3_runtime_last_generation_stats = {};
}

/**
*    @brief  Rebuild runtime-only lookup tables after a successful asset load.
 *    @note   Stage-7 generated asset payloads do not publish graph reverse indices yet, so this remains a no-op seam.
**/
static void Nav3_Runtime_RebuildLookupTablesAfterLoad( void ) {
	// Stage-7 generated assets publish sparse columns/spans only; reverse graph indices arrive in later stages.
}


/**
*
*
*    Nav3 Runtime Public API:
*
*
**/
/**
*    @brief  Initialize nav3 runtime ownership and register nav3 cvars.
**/
void SVG_Nav3_Runtime_Init( void ) {
	/**
	*    Register cvars first so status snapshots can reflect current server settings immediately.
	**/
	Nav3_Runtime_RegisterCvars();
	Nav3_Runtime_InitializeBuildConfig();
	// Initialize nav3 debug draw queue and subsystem cvars for stage-2 diagnostics.
	SVG_Nav3_DebugDraw_Init();

	// Mark runtime initialized and clear stage-1 publish state.
	Nav3_Runtime_ClearGeneratedMesh();
	s_nav3_runtime_status.initialized = true;
	s_nav3_runtime_status.enabled = s_nav3_enable && ( s_nav3_enable->integer != 0 );
	s_nav3_runtime_status.debug_enabled = s_nav3_debug && ( s_nav3_debug->integer != 0 );
	s_nav3_runtime_status.has_mesh = false;
	s_nav3_runtime_status.generated_column_count = 0;
	s_nav3_runtime_status.generated_span_count = 0;
	s_nav3_runtime_status.generated_mesh_checksum = 0;
	s_nav3_runtime_status.mesh_generation = 0;
	s_nav3_runtime_status.publish_state = nav3_runtime_publish_state_t::Empty;
	Nav3_Runtime_BumpGeneration( &s_nav3_runtime_status.runtime_generation );
}

/**
*    @brief  Shutdown nav3 runtime ownership.
**/
void SVG_Nav3_Runtime_Shutdown( void ) {
	/**
	*    Always clear published mesh state before marking runtime unavailable.
	**/
	SVG_Nav3_Runtime_Unload();
	// Shutdown nav3 debug draw runtime state.
	SVG_Nav3_DebugDraw_Shutdown();

	// Mark runtime as unavailable while preserving cvar registration across reloads.
	s_nav3_runtime_status.initialized = false;
	s_nav3_runtime_status.enabled = false;
	s_nav3_runtime_status.debug_enabled = false;
	Nav3_Runtime_BumpGeneration( &s_nav3_runtime_status.runtime_generation );
}

/**
*    @brief  Unload the currently published nav3 mesh state.
**/
void SVG_Nav3_Runtime_Unload( void ) {
	// Reset stage-5 generated mesh and runtime publication fields to empty.
	Nav3_Runtime_ClearGeneratedMesh();
	s_nav3_runtime_status.has_mesh = false;
	s_nav3_runtime_status.generated_column_count = 0;
	s_nav3_runtime_status.generated_span_count = 0;
	s_nav3_runtime_status.generated_mesh_checksum = 0;
	s_nav3_runtime_status.mesh_generation = 0;
	s_nav3_runtime_status.publish_state = nav3_runtime_publish_state_t::Empty;
}

/**
*    @brief  Return whether nav3 backend routing is currently enabled.
*    @return True when `s_nav3_enable` is non-zero.
**/
const bool SVG_Nav3_Runtime_IsEnabled( void ) {
	// Query the live cvar directly so runtime command toggles apply immediately.
	return s_nav3_enable && ( s_nav3_enable->integer != 0 );
}

/**
*    @brief  Return whether nav3 currently has a published mesh.
*    @return True when runtime mesh publication state is active.
**/
const bool SVG_Nav3_Runtime_HasMesh( void ) {
	return s_nav3_runtime_status.has_mesh;
}

/**
*    @brief  Run nav3 generation for the active level.
*    @return True when a mesh was generated and published.
*    @note   Stage 1 keeps this as a bounded no-publish path while command surfaces are wired.
**/
const bool SVG_Nav3_Runtime_Generate( void ) {
	/**
	*    Reject generation when nav3 is disabled so backend wrapper behavior remains explicit.
	**/
	if ( !SVG_Nav3_Runtime_IsEnabled() ) {
		return false;
	}

	/**
	*    Reject generation before runtime initialization to avoid publishing partially initialized state.
	**/
	if ( !s_nav3_runtime_status.initialized ) {
		return false;
	}

	/**
	*    Run stage-5 BSP span generation into temporary storage before publishing runtime state.
	**/
	nav3_generated_mesh_t generatedMesh = {};
	const bool generated = SVG_Nav3_GenerateStaticSpans( s_nav3_runtime_build_config, &generatedMesh );
	s_nav3_runtime_last_generation_stats = generatedMesh.stats;

	/**
	*    Reject publication when generation failed or produced no sparse spans.
	**/
	if ( !generated || generatedMesh.stats.emitted_spans <= 0 || generatedMesh.columns.empty() ) {
		// Preserve last generation stats for bounded diagnostics even on failed publication.
		s_nav3_runtime_generated_mesh = {};
		s_nav3_runtime_status.has_mesh = false;
		s_nav3_runtime_status.generated_column_count = 0;
		s_nav3_runtime_status.generated_span_count = 0;
		s_nav3_runtime_status.generated_mesh_checksum = 0;
		s_nav3_runtime_status.publish_state = nav3_runtime_publish_state_t::Empty;
		Nav3_Runtime_BumpGeneration( &s_nav3_runtime_status.runtime_generation );
		return false;
	}

	/**
	*    Publish generated stage-5 span data and bump mesh/runtime generations.
	**/
	s_nav3_runtime_generated_mesh = generatedMesh;
	s_nav3_runtime_last_generation_stats = s_nav3_runtime_generated_mesh.stats;
	s_nav3_runtime_status.has_mesh = true;
	s_nav3_runtime_status.generated_column_count = ( uint32_t )s_nav3_runtime_generated_mesh.columns.size();
	s_nav3_runtime_status.generated_span_count = Nav3_Runtime_CountGeneratedSpans( s_nav3_runtime_generated_mesh );
	s_nav3_runtime_status.generated_mesh_checksum = SVG_Nav3_Serialize_BuildGeneratedMeshChecksum( s_nav3_runtime_generated_mesh );
	s_nav3_runtime_status.publish_state = nav3_runtime_publish_state_t::Published;
	Nav3_Runtime_BumpGeneration( &s_nav3_runtime_status.mesh_generation );
	Nav3_Runtime_BumpGeneration( &s_nav3_runtime_status.runtime_generation );
	return true;
}

/**
*    @brief  Save the active nav3 runtime mesh.
*    @param  filename  Destination path to save.
*    @return True when save completed.
*    @note   Stage 7 persists generated sparse columns/spans and build config state.
**/
const bool SVG_Nav3_Runtime_Save( const char *filename ) {
	/**
	*    Require a writable filename before attempting any save path.
	**/
	if ( !filename || filename[ 0 ] == '\0' ) {
		return false;
	}

	/**
	*    Reject save requests while nav3 backend is disabled.
	**/
	if ( !SVG_Nav3_Runtime_IsEnabled() ) {
		return false;
	}

	/**
	*    Reject save requests before runtime initialization completes.
	**/
	if ( !s_nav3_runtime_status.initialized ) {
		return false;
	}

	/**
	*    Reject save requests when no generated runtime mesh is currently published.
	**/
	if ( !s_nav3_runtime_status.has_mesh || s_nav3_runtime_generated_mesh.columns.empty() ) {
		return false;
	}

	/**
	*    Persist a stage-7 generated nav3 asset for the active map/build-config context.
	**/
	const nav3_persistence_result_t saveResult = SVG_Nav3_Persistence_SaveGeneratedAsset(
		filename,
		Nav3_Runtime_ResolveActiveMapName(),
		s_nav3_runtime_build_config,
		s_nav3_runtime_generated_mesh );
	return saveResult.success;
}

/**
*    @brief  Load a nav3 runtime mesh.
*    @param  filename  Source path to load.
*    @return True when load completed and publish succeeded.
*    @note   Stage 7 validates/publishes generated sparse columns/spans from serialized asset data.
**/
const bool SVG_Nav3_Runtime_Load( const char *filename ) {
	/**
	*    Require a readable filename before attempting any load path.
	**/
	if ( !filename || filename[ 0 ] == '\0' ) {
		return false;
	}

	/**
	*    Reject load requests while nav3 backend is disabled.
	**/
	if ( !SVG_Nav3_Runtime_IsEnabled() ) {
		return false;
	}

	/**
	*    Reject load requests before runtime initialization completes.
	**/
	if ( !s_nav3_runtime_status.initialized ) {
		return false;
	}

	/**
	*    Load and validate a stage-7 generated nav3 asset into temporary decode storage first.
	**/
	nav3_serialized_header_t loadedHeader = {};
	nav3_generated_mesh_t loadedGeneratedMesh = {};
	const nav3_persistence_result_t loadResult = SVG_Nav3_Persistence_LoadGeneratedAsset(
		filename,
		Nav3_Runtime_ResolveActiveMapName(),
		s_nav3_runtime_build_config,
		&loadedHeader,
		&loadedGeneratedMesh );
	if ( !loadResult.success ) {
		return false;
	}
	(void)loadedHeader;

	/**
	*    Reject asset publication when decoded generated payload is empty.
	**/
	if ( loadedGeneratedMesh.columns.empty() || loadResult.generated_span_count == 0 ) {
		return false;
	}

	/**
	*    Recompute checksum from decoded payload and reject mismatched runtime decode state.
	**/
	const uint32_t loadedMeshChecksum = SVG_Nav3_Serialize_BuildGeneratedMeshChecksum( loadedGeneratedMesh );
	if ( loadResult.generated_mesh_checksum != 0 && loadedMeshChecksum != loadResult.generated_mesh_checksum ) {
		return false;
	}

	/**
	*    Rebuild runtime-only lookup seams before publishing decoded state.
	**/
	s_nav3_runtime_generated_mesh = std::move( loadedGeneratedMesh );
	s_nav3_runtime_last_generation_stats = s_nav3_runtime_generated_mesh.stats;
	Nav3_Runtime_RebuildLookupTablesAfterLoad();

	/**
	*    Publish loaded stage-7 generated asset state only after all validation/rebuild steps succeeded.
	**/
	s_nav3_runtime_status.has_mesh = true;
	s_nav3_runtime_status.generated_column_count = static_cast<uint32_t>( s_nav3_runtime_generated_mesh.columns.size() );
	s_nav3_runtime_status.generated_span_count = Nav3_Runtime_CountGeneratedSpans( s_nav3_runtime_generated_mesh );
	s_nav3_runtime_status.generated_mesh_checksum = loadedMeshChecksum;
	s_nav3_runtime_status.publish_state = nav3_runtime_publish_state_t::Published;
	Nav3_Runtime_BumpGeneration( &s_nav3_runtime_status.mesh_generation );
	Nav3_Runtime_BumpGeneration( &s_nav3_runtime_status.runtime_generation );
	return true;
}

/**
*    @brief  Resolve the default nav3 asset path for the current level.
*    @param  outPath  [out] Resolved path buffer.
*    @param  outPathCount  Size of `outPath`.
*    @return True when the path was resolved.
**/
const bool SVG_Nav3_Runtime_ResolveDefaultAssetPath( char *outPath, const size_t outPathCount ) {
	/**
	*    Require destination storage before formatting default path output.
	**/
	if ( !outPath || outPathCount == 0 ) {
		return false;
	}

	/**
	*    Resolve active map name, falling back to a deterministic placeholder when unavailable.
	**/
	const char *mapName = ( level.mapname[ 0 ] != '\0' ) ? level.mapname : "unknown";
	const size_t writeLength = Q_snprintf( outPath, outPathCount, "baseq2rtxp/nav/%s.nav3", mapName );
	if ( writeLength >= outPathCount ) {
		outPath[ 0 ] = '\0';
		return false;
	}
	return true;
}

/**
*    @brief  Return bounded diagnostics from the most recent stage-5 generation pass.
*    @return Snapshot of last nav3 generation stats.
**/
const nav3_generation_stats_t &SVG_Nav3_Runtime_GetLastGenerationStats( void ) {
	return s_nav3_runtime_last_generation_stats;
}

/**
*    @brief  Queue bounded debug-draw primitives for currently published generated spans.
*    @param  maxColumns  Maximum sparse columns to draw.
**/
void SVG_Nav3_Runtime_DebugDrawGeneratedSpans( const int32_t maxColumns ) {
	/**
	*    Skip draw submission when no generated spans are currently published.
	**/
	if ( !s_nav3_runtime_status.has_mesh || s_nav3_runtime_generated_mesh.columns.empty() ) {
		return;
	}

	SVG_Nav3_Generate_DebugDrawSpans( s_nav3_runtime_generated_mesh, maxColumns );
}

/**
*    @brief  Queue bounded debug-draw primitives for currently retained stage-6 filter rejects.
*    @param  maxSamples  Maximum reject samples to draw.
**/
void SVG_Nav3_Runtime_DebugDrawFilterRejects( const int32_t maxSamples ) {
	/**
	*    Skip draw submission when no generation pass has produced reject diagnostics.
	**/
	if ( s_nav3_runtime_last_generation_stats.filter_reject_sample_count <= 0 ) {
		return;
	}

	SVG_Nav3_Generate_DebugDrawFilterRejects( s_nav3_runtime_generated_mesh, maxSamples );
}

/**
*    @brief  Return a bounded nav3 runtime status snapshot.
*    @return Current runtime status snapshot.
**/
nav3_runtime_status_t SVG_Nav3_Runtime_GetStatus( void ) {
	// Return a copy so callers cannot mutate internal runtime owner state directly.
	nav3_runtime_status_t status = s_nav3_runtime_status;
	status.enabled = SVG_Nav3_Runtime_IsEnabled();
	status.debug_enabled = s_nav3_debug && ( s_nav3_debug->integer != 0 );
	status.generated_column_count = s_nav3_runtime_status.generated_column_count;
	status.generated_span_count = s_nav3_runtime_status.generated_span_count;
	status.generated_mesh_checksum = s_nav3_runtime_status.generated_mesh_checksum;
	return status;
}

/**
*    @brief  Return the active nav3 build configuration snapshot.
*    @return Runtime build configuration used for stage diagnostics.
**/
const nav3_build_config_t &SVG_Nav3_Runtime_GetBuildConfig( void ) {
	return s_nav3_runtime_build_config;
}

/**
*    @brief  Return the active nav3 stable type/schema version.
*    @return Stable nav3 schema version.
**/
const uint32_t SVG_Nav3_Runtime_GetTypeSchemaVersion( void ) {
	return NAV3_TYPE_SCHEMA_VERSION;
}
