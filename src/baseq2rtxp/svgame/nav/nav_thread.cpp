#include "nav_thread.h"
#include "nav_generate.h"
#include "nav_cover_generate.h"
#include "nav_path.h"
#include "nav_identity.h"
#include "nav_query.h"
#include "nav_worker.h"
#include "nav_corner_capture.h"
#include "nav_cover_types.h"
#include "shared/util/util_strings.h"

#include <algorithm>
#include <limits>
#include <array>
#include <cstdarg>
#include <cstdio>

//! External cover points vector defined in nav_cover_generate.cpp.
extern svg_vector_t<nav_cover_point_t, TAG_SVGAME_NAVMESH> g_nav_cover_points;

//! Tracks whether the server owner thread is actively inside a generation stage.
static bool s_nav_generation_worker = false;

//! Generation progress state maintained on the server owner thread.
static nav_gen_progress_t s_gen_progress = {};

//! Active generation stage index (0..4) for staged main-thread progression.
static int32_t s_gen_stage_index = 0;

//! Last server time in milliseconds when progress was printed.
static uint64_t s_last_progress_print_ms = 0;

/**
*	@brief	Read whether the server thread is currently executing a navmesh generation stage.
*	@return	True while a generation stage is actively running on the server thread.
**/
bool Nav_IsGenerationWorkerThread( void ) {
	return s_nav_generation_worker;
}

/**
*	@brief	Format and print a bounded generation message on the server owner thread.
*	@param	format	Printf-style message, consumed before return.
**/
void Nav_GenerationPrintf( const char *format, ... ) {
	/**
	*	Format into stack storage and forward directly to engine developer output.
	**/
	std::array<char, 2048> message{};
	va_list arguments;
	va_start( arguments, format );
	std::vsnprintf( message.data(), message.size(), format, arguments );
	va_end( arguments );

	// Emit diagnostic message if engine developer output is available.
	if ( gi.dprintf != nullptr ) {
		gi.dprintf( "%s", message.data() );
	}
}

/**
*	@brief	Finalize staged navmesh generation on the main server thread.
*	@note	Publishes mesh identity revisions, rebuilds dynamic edge tables, and logs completion summary.
**/
static void Nav_FinalizeGenerationMainThread( void ) {
	/**
	*	Update completion progress and elapsed clock metrics before publishing topology.
	**/
	s_nav_generation_worker = false;
	Nav_SetGenerationProgress( 1.0f, "Completed" );
	const uint64_t end = gi.GetRealSystemTime();
	s_gen_progress.current_time_ms = end;
	s_gen_progress.time_taken_ms = end - s_gen_progress.start_time_ms;
	s_gen_progress.estimated_time_left_ms = 0;
	s_gen_progress.is_generating = false;
	Q_strlcpy( s_gen_progress.stage, "Completed", sizeof( s_gen_progress.stage ) );

	/**
	*	Publish live topology on the owner thread after all build stages finish.
	**/
	// Issue publication identities through the epoch barrier; never silently wrap a live generation.
	g_nav_mesh_generation = Nav_IssueIdentityRevisionMainThread();
	Nav_InvalidateObstacleCornersTable();
	Nav_RebuildEntityEdgeRegistry();
	Nav_ResyncDynamicEntityEdges();

	/**
	*	Emit completion summary to the server console.
	**/
	gi.dprintf( "NavMesh Generation Completed in %.2f s. (Faces: %" PRIu32 ", Nodes: %" PRIu32 ", Cover Points: %" PRIu32 ")\n",
		s_gen_progress.time_taken_ms / 1000.0f, static_cast<uint32_t>( g_nav_faces.size() ),
		static_cast<uint32_t>( g_nav_nodes.size() ), static_cast<uint32_t>( g_nav_cover_points.size() ) );
}

/**
*	@brief	Execute the next pending navmesh generation stage on the main server thread.
*	@note	Advances through extraction, half-edge build, KD-tree, cover points, and regions/portals.
**/
static void Nav_RunNextGenerationStageMainThread( void ) {
	/**
	*	Guard against advancing when no generation is active.
	**/
	if ( !s_gen_progress.is_generating ) {
		return;
	}

	/**
	*	Execute the current build stage under the generation role flag so internal
	*	collision traces and profiler guards treat this work as build-time generation.
	**/
	s_nav_generation_worker = true;
	switch ( s_gen_stage_index ) {
	case 0:
		// Stage 0: Extract walkable windings and perform CSG clipping.
		Nav_DoExtractionWork();
		s_gen_stage_index = 1;
		break;
	case 1:
		// Stage 1: Weld vertices and construct half-edge topology.
		Nav_BuildHalfEdgeMesh();
		s_gen_stage_index = 2;
		break;
	case 2:
		// Stage 2: Build balanced 3D KD-tree and BSP leaf mappings.
		Nav_BuildKDTree();
		s_gen_stage_index = 3;
		break;
	case 3:
		// Stage 3: Precalculate tactical cover points along boundary edges.
		Nav_GenerateCoverPoints();
		s_gen_stage_index = 4;
		break;
	default:
		// Stage 4: Build spatial regions, portals, and sector connectivity, then finalize.
		Nav_BuildSpatialRegionsAndPortals();
		s_gen_stage_index = 5;
		break;
	}
	s_nav_generation_worker = false;

	/**
	*	Finalize publication once all stages have completed.
	**/
	if ( s_gen_stage_index >= 5 ) {
		Nav_FinalizeGenerationMainThread();
	}
}

/**
*	@brief	Start a new staged navmesh generation job on the server thread.
*	@note	Rejects duplicate requests while one job is already running.
**/
void Nav_StartAsyncGeneration( void ) {
	/**
	*	Check duplicate submission; owner-side submissions are serialized.
	**/
	if ( s_gen_progress.is_generating ) {
		gi.dprintf( "NavMesh generation is already in progress...\n" );
		return;
	}

	/**
	*	Close capture/snapshot acquisition before mutating live geometry.
	**/
	Nav_Corner_InvalidateCapture();
	Nav_InvalidateObstacleCornersTable();
	const uint64_t start = gi.GetRealSystemTime();
	s_gen_progress = {};
	s_gen_progress.is_generating = true;
	s_gen_progress.start_time_ms = start;
	s_gen_progress.current_time_ms = start;
	Q_strlcpy( s_gen_progress.stage, "Starting", sizeof( s_gen_progress.stage ) );
	s_gen_stage_index = 0;
	s_last_progress_print_ms = 0;

	gi.dprintf( "NavMesh Generation Started...\n" );
}

/**
*	@brief	Set the current navmesh generation progress percentage and optional stage label.
*	@param	progress_pct	The progression fraction between 0.0f and 1.0f.
*	@param	stage			Optional human-readable stage name to display in status reports.
*	@note	Clamps the value between 0.0f and 1.0f and ensures monotonic advancement.
**/
void Nav_SetGenerationProgress( const float progress_pct, const char *stage ) {
	/**
	*	Keep progress monotonic and update its optional bounded stage label.
	**/
	const float clamped = QM_Clamp( progress_pct, 0.0f, 1.0f );
	// Advance progress fraction monotonically.
	if ( clamped > s_gen_progress.progress_pct ) {
		s_gen_progress.progress_pct = clamped;
	}
	// A null/empty label means retain the current stage rather than erase useful progress.
	if ( stage != nullptr && stage[ 0 ] != '\0' ) {
		Q_strlcpy( s_gen_progress.stage, stage, sizeof( s_gen_progress.stage ) );
	}
}

/**
*	@brief	Set the current active generation stage label without modifying progress.
*	@param	stage	Human-readable stage name.
**/
void Nav_SetGenerationStage( const char *stage ) {
	/**
	*	Validate and copy the new stage label.
	**/
	// Null/empty input cannot replace a useful status label.
	if ( stage == nullptr || stage[ 0 ] == '\0' ) {
		return;
	}
	Q_strlcpy( s_gen_progress.stage, stage, sizeof( s_gen_progress.stage ) );
}

/**
*	@brief	Return the current generation progress snapshot.
*	@return	Copy of the generation progress state.
**/
nav_gen_progress_t Nav_GetGenerationProgress( void ) {
	return s_gen_progress;
}

/**
*	@brief	Advance and print staged generation status from the main server loop.
*	@note	Console output is rate-limited to avoid flooding during long builds.
**/
void Nav_UpdateAsyncGeneration( void ) {
	/**
	*	Avoid clock reads and stage checks when no generation is active.
	**/
	if ( !s_gen_progress.is_generating ) {
		return;
	}

	/**
	*	Execute the next generation stage on the main server thread.
	**/
	Nav_RunNextGenerationStageMainThread();
	// Return immediately if the stage completed and finalized generation.
	if ( !s_gen_progress.is_generating ) {
		return;
	}

	/**
	*	Refresh elapsed time and estimated remaining duration.
	**/
	const uint64_t now = gi.GetRealSystemTime();
	s_gen_progress.current_time_ms = now;
	s_gen_progress.time_taken_ms = now - s_gen_progress.start_time_ms;
	// Estimate remaining time when progress has advanced past 1%.
	if ( s_gen_progress.progress_pct > 0.01f ) {
		const double remaining = static_cast<double>( s_gen_progress.time_taken_ms ) *
			( 1.0 - s_gen_progress.progress_pct ) / s_gen_progress.progress_pct;
		s_gen_progress.estimated_time_left_ms = static_cast<uint64_t>( std::min( remaining, static_cast<double>( UINT32_MAX ) ) );
	} else {
		s_gen_progress.estimated_time_left_ms = 0;
	}

	/**
	*	Throttle progress output on the server owner.
	**/
	if ( now - s_last_progress_print_ms < 1000 ) {
		return;
	}
	s_last_progress_print_ms = now;
	gi.dprintf( "NavMesh Generation [%s]: %.2f%%, Time Elapsed: %.2f s\n",
		s_gen_progress.stage[ 0 ] != '\0' ? s_gen_progress.stage : "Building", s_gen_progress.progress_pct * 100.0f,
		s_gen_progress.time_taken_ms / 1000.0f );
}

/**
*	@brief	Finish any remaining generation stages synchronously on the server owner before a lifetime reset.
*	@note	Ensures live geometry is fully built and published before clear, map change, or shutdown.
**/
void Nav_DrainAsyncGenerationMainThread( void ) {
	/**
	*	Run all remaining generation stages to completion on the main server thread.
	**/
	while ( s_gen_progress.is_generating ) {
		Nav_RunNextGenerationStageMainThread();
	}
}

/**
*	@brief	Allocate and initialize a new sliced pathfinding query.
*	@param	start	World-space starting position.
*	@param	end		World-space target position.
*	@return	Pointer to the query, or nullptr if allocation fails.
**/
nav_path_query_t *Nav_StartPathQuery( const Vector3 &start, const Vector3 &end ) {
	/**
	*	Allocate zeroed query storage from the navmesh tag pool.
	**/
	nav_path_query_t *query = static_cast<nav_path_query_t *>( gi.TagMallocz( sizeof( nav_path_query_t ), TAG_SVGAME_NAVMESH ) );
	// Return nullptr if zone allocation failed.
	if ( !query ) {
		return nullptr;
	}

	// Seed the query endpoints and mark it as active.
	query->start_pos = start;
	query->end_pos = end;
	query->is_finished = false;
	query->path_found = false;
	return query;
}

/**
*	@brief	Advance a sliced pathfinding query within a time budget.
*	@param	query		Query to process.
*	@param	max_time_ms	Maximum number of milliseconds to spend on this slice.
**/
void Nav_ProcessPathQuerySliced( nav_path_query_t *query, uint32_t max_time_ms ) {
	/**
	*	Sanity check: ignore null queries and already-completed queries.
	**/
	if ( !query || query->is_finished ) {
		return;
	}

	// Track the start time for this processing slice.
	const uint64_t start_time = gi.GetRealSystemTime();

	// Keep processing until the query finishes or the slice budget is exhausted.
	while ( !query->is_finished ) {
		// Stub: A* step processing goes here.

		// Yield back to the server frame once the slice budget is spent.
		if ( ( gi.GetRealSystemTime() - start_time ) >= max_time_ms ) {
			break;
		}
	}
}

/**
*	@brief	Free a sliced pathfinding query.
*	@param	query	Query to release.
**/
void Nav_FreePathQuery( nav_path_query_t *query ) {
	/**
	*	Release query block if non-null.
	**/
	if ( query ) {
		gi.TagFree( query );
	}
}
