#include "nav_thread.h"
#include "nav_generate.h"
#include "nav_path.h"

//! Shared progress snapshot updated by the async generation worker.
static nav_gen_progress_t s_gen_progress = {};
//! Engine work item used to submit nav generation onto the async queue.
static asyncwork_t s_gen_work = {};
//! Last server time in milliseconds when progress was printed.
static uint32_t s_last_progress_print_ms = 0;

/**
*	@brief	Perform the navmesh extraction and build work on the background thread.
*	@note	This function runs outside the main server loop.
**/
static void Nav_AsyncGenerationWork( void *arg ) {
	(void)arg;

	// Seed the progress snapshot at the worker start time.
	const uint32_t start = gi.GetRealTime();
	s_gen_progress.start_time_ms = start;
	s_gen_progress.current_time_ms = start;
	s_gen_progress.time_taken_ms = 0;
	s_gen_progress.estimated_time_left_ms = 0;
	s_gen_progress.progress_pct = 0.0f;

	// Extract walkable geometry from the current collision model.
	Nav_DoExtractionWork();

	// Record mid-point progress once extraction finishes.
	s_gen_progress.progress_pct = 0.5f;
	s_gen_progress.current_time_ms = gi.GetRealTime();
	s_gen_progress.time_taken_ms = s_gen_progress.current_time_ms - start;

	// Build the half-edge mesh and KD-tree from the extracted polygons.
	Nav_BuildHalfEdgeMesh();
	Nav_BuildKDTree();

	// Mark the job as complete in the progress snapshot.
	s_gen_progress.progress_pct = 1.0f;
	s_gen_progress.current_time_ms = gi.GetRealTime();
	s_gen_progress.time_taken_ms = s_gen_progress.current_time_ms - start;
	s_gen_progress.estimated_time_left_ms = 0;
}

/**
*	@brief	Finalize async generation on the main thread after the worker finishes.
*	@note	This clears the active flag and prints a completion summary.
**/
static void Nav_AsyncGenerationDone( void *arg ) {
	(void)arg;

	// Reapply mover/wall state after the mesh registry has been rebuilt.
	Nav_ResyncDynamicEntityEdges();

	// The worker is finished, so the progress snapshot can be marked idle.
	s_gen_progress.is_generating = false;
	gi.dprintf( "NavMesh Generation Completed in %u ms. (Faces: %u, Nodes: %u)\n",
		s_gen_progress.time_taken_ms,
		static_cast<unsigned int>( g_nav_faces.size() ),
		static_cast<unsigned int>( g_nav_nodes.size() ) );
}

/**
*	@brief	Start a new asynchronous navmesh generation job.
*	@note	Rejects duplicate requests while one job is already running.
**/
void Nav_StartAsyncGeneration() {
	// Do not start a second generation job while the first is still active.
	if ( s_gen_progress.is_generating ) {
		gi.dprintf( "NavMesh generation is already in progress...\n" );
		return;
	}

	// Reset the shared progress snapshot before queueing fresh work.
	memset( &s_gen_progress, 0, sizeof( s_gen_progress ) );
	s_last_progress_print_ms = 0;
	s_gen_progress.is_generating = true;
	s_gen_progress.start_time_ms = gi.GetRealTime();
	s_gen_progress.current_time_ms = s_gen_progress.start_time_ms;

	// Bind the worker and completion callbacks to the async work item.
	s_gen_work.work_cb = Nav_AsyncGenerationWork;
	s_gen_work.done_cb = Nav_AsyncGenerationDone;
	s_gen_work.cb_arg = nullptr;

	// Submit the job to the engine async queue.
	gi.Com_QueueAsyncWork( &s_gen_work );
	gi.dprintf( "NavMesh Generation Started...\n" );
}

/**
*	@brief	Return the current generation progress snapshot.
*	@return	Reference to the shared progress state.
**/
const nav_gen_progress_t &Nav_GetGenerationProgress() {
	return s_gen_progress;
}

/**
*	@brief	Refresh and print async generation status from the main server loop.
*	@note	Console output is rate-limited to avoid flooding during long builds.
**/
void Nav_UpdateAsyncGeneration() {
	// Nothing to do once the worker has already completed.
	if ( !s_gen_progress.is_generating ) {
		return;
	}

	// Refresh elapsed time from the current server clock.
	s_gen_progress.current_time_ms = gi.GetRealTime();
	s_gen_progress.time_taken_ms = s_gen_progress.current_time_ms - s_gen_progress.start_time_ms;

	// Print at a gentle cadence so operators can track long builds without console spam.
	if ( s_gen_progress.current_time_ms - s_last_progress_print_ms < 1000 ) {
		return;
	}
	s_last_progress_print_ms = s_gen_progress.current_time_ms;

	// Emit a bounded progress line for server operators.
	gi.dprintf( "NavMesh Generation Progress: %.2f%%, Time Elapsed: %u ms\n",
		s_gen_progress.progress_pct * 100.0f,
		s_gen_progress.current_time_ms - s_gen_progress.start_time_ms );
}

/**
*	@brief	Allocate and initialize a new sliced pathfinding query.
*	@param	start	World-space starting position.
*	@param	end	World-space target position.
*	@return	Pointer to the query, or nullptr if allocation fails.
**/
nav_path_query_t *Nav_StartPathQuery( const Vector3 &start, const Vector3 &end ) {
	// Allocate zeroed query storage from the navmesh tag pool.
	nav_path_query_t *query = ( nav_path_query_t * )gi.TagMallocz( sizeof( nav_path_query_t ), TAG_SVGAME_NAVMESH );
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
*	@param	query	Query to process.
*	@param	max_time_ms	Maximum number of milliseconds to spend on this slice.
**/
void Nav_ProcessPathQuerySliced( nav_path_query_t *query, uint32_t max_time_ms ) {
	// Ignore null queries and already-completed queries.
	if ( !query || query->is_finished ) {
		return;
	}

	// Track the start time for this processing slice.
	const uint32_t start_time = gi.GetRealTime();

	// Keep processing until the query finishes or the slice budget is exhausted.
	while ( !query->is_finished ) {
		// Stub: A* step processing goes here.

		// Yield back to the server frame once the slice budget is spent.
		if ( ( gi.GetRealTime() - start_time ) >= max_time_ms ) {
			break;
		}
	}
}

/**
*	@brief	Free a sliced pathfinding query.
*	@param	query	Query to release.
**/
void Nav_FreePathQuery( nav_path_query_t *query ) {
	if ( query ) {
		gi.TagFree( query );
	}
}
