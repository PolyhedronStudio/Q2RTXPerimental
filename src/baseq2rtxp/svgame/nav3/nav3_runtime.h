/********************************************************************
*
*
*    ServerGame: Nav3 Runtime Ownership Seam
*
*
********************************************************************/
#pragma once

#include "svgame/nav3/nav3_core.h"
#include "svgame/nav3/nav3_generate.h"
#include "svgame/nav3/nav3_types.h"


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
void SVG_Nav3_Runtime_Init( void );

/**
*    @brief  Shutdown nav3 runtime ownership.
**/
void SVG_Nav3_Runtime_Shutdown( void );

/**
*    @brief  Unload the currently published nav3 mesh state.
**/
void SVG_Nav3_Runtime_Unload( void );

/**
*    @brief  Return whether nav3 backend routing is currently enabled.
*    @return True when `s_nav3_enable` is non-zero.
**/
const bool SVG_Nav3_Runtime_IsEnabled( void );

/**
*    @brief  Return whether nav3 currently has a published mesh.
*    @return True when runtime mesh publication state is active.
**/
const bool SVG_Nav3_Runtime_HasMesh( void );

/**
*    @brief  Run nav3 generation for the active level.
*    @return True when a mesh was generated and published.
*    @note   Stage 1 keeps this as a bounded no-publish path while command surfaces are wired.
**/
const bool SVG_Nav3_Runtime_Generate( void );

/**
*    @brief  Save the active nav3 runtime mesh.
*    @param  filename  Destination path to save.
*    @return True when save completed.
*    @note   Stage 4 persists an empty nav3 asset shell while mesh serialization remains future work.
**/
const bool SVG_Nav3_Runtime_Save( const char *filename );

/**
*    @brief  Load a nav3 runtime mesh.
*    @param  filename  Source path to load.
*    @return True when load completed and publish succeeded.
*    @note   Stage 4 validates/publishes an empty nav3 asset shell while mesh payload loading remains future work.
**/
const bool SVG_Nav3_Runtime_Load( const char *filename );

/**
*    @brief  Resolve the default nav3 asset path for the current level.
*    @param  outPath  [out] Resolved path buffer.
*    @param  outPathCount  Size of `outPath`.
*    @return True when the path was resolved.
**/
const bool SVG_Nav3_Runtime_ResolveDefaultAssetPath( char *outPath, const size_t outPathCount );

/**
*    @brief  Return bounded diagnostics from the most recent stage-5 generation pass.
*    @return Snapshot of last nav3 generation stats.
**/
const nav3_generation_stats_t &SVG_Nav3_Runtime_GetLastGenerationStats( void );

/**
*    @brief  Queue bounded debug-draw primitives for currently published generated spans.
*    @param  maxColumns  Maximum sparse columns to draw.
**/
void SVG_Nav3_Runtime_DebugDrawGeneratedSpans( const int32_t maxColumns );

/**
*    @brief  Queue bounded debug-draw primitives for currently retained stage-6 filter rejects.
*    @param  maxSamples  Maximum reject samples to draw.
**/
void SVG_Nav3_Runtime_DebugDrawFilterRejects( const int32_t maxSamples );

/**
*    @brief  Return a bounded nav3 runtime status snapshot.
*    @return Current runtime status snapshot.
**/
nav3_runtime_status_t SVG_Nav3_Runtime_GetStatus( void );

/**
*    @brief  Return the active nav3 build configuration snapshot.
*    @return Runtime build configuration used for stage diagnostics.
**/
const nav3_build_config_t &SVG_Nav3_Runtime_GetBuildConfig( void );

/**
*    @brief  Return the active nav3 stable type/schema version.
*    @return Stable nav3 schema version.
**/
const uint32_t SVG_Nav3_Runtime_GetTypeSchemaVersion( void );
