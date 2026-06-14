#include "nav_thread.h"
#include "nav_generate.h"

static nav_gen_progress_t s_gen_progress = {};
static asyncwork_t s_gen_work = {};
//! Last server time in milliseconds when the KD-tree generation progress was printed.
static uint32_t s_last_progress_print_ms = 0;

static void Nav_AsyncGenerationWork(void* arg) {
    // This runs on a background thread.
    uint32_t start = gi.GetRealTime();
    s_gen_progress.start_time_ms = start;
    s_gen_progress.current_time_ms = start;
    s_gen_progress.time_taken_ms = 0;
    s_gen_progress.estimated_time_left_ms = 0;
    s_gen_progress.progress_pct = 0.0f;
    
    // Run the actual BSP extraction logic here
    Nav_DoExtractionWork();
    
    s_gen_progress.progress_pct = 0.5f;
    s_gen_progress.current_time_ms = gi.GetRealTime();
    s_gen_progress.time_taken_ms = s_gen_progress.current_time_ms - start;

    // Build KD-Tree
    Nav_BuildKDTree();

    s_gen_progress.progress_pct = 1.0f;
    s_gen_progress.current_time_ms = gi.GetRealTime();
    s_gen_progress.time_taken_ms = s_gen_progress.current_time_ms - start;
    s_gen_progress.estimated_time_left_ms = 0;
}

static void Nav_AsyncGenerationDone(void* arg) {
    // This runs on the main thread after work_cb completes.
    s_gen_progress.is_generating = false;
    gi.dprintf("NavMesh Generation Completed in %d ms.\n", s_gen_progress.time_taken_ms);
}

void Nav_StartAsyncGeneration() {
    if (s_gen_progress.is_generating) {
        gi.dprintf("NavMesh generation is already in progress...\n");
        return;
    }
    
    memset(&s_gen_progress, 0, sizeof(s_gen_progress));
    s_last_progress_print_ms = 0;
    s_gen_progress.is_generating = true;
    s_gen_progress.start_time_ms = gi.GetRealTime();
    s_gen_progress.current_time_ms = s_gen_progress.start_time_ms;
    
    s_gen_work.work_cb = Nav_AsyncGenerationWork;
    s_gen_work.done_cb = Nav_AsyncGenerationDone;
    s_gen_work.cb_arg = nullptr;
    
    gi.Com_QueueAsyncWork(&s_gen_work);
    gi.dprintf("NavMesh Generation Started...\n");
}

const nav_gen_progress_t& Nav_GetGenerationProgress() {
    return s_gen_progress;
}

/**
*	@brief	Optional tick update for the asynchronous generation process. This can be called from the main server loop to print progress or perform time-based updates.
**/
void Nav_UpdateAsyncGeneration() {
    if (s_gen_progress.is_generating) {
        s_gen_progress.current_time_ms = gi.GetRealTime();
        s_gen_progress.time_taken_ms = s_gen_progress.current_time_ms - s_gen_progress.start_time_ms;

        // Print progress at a gentle cadence so long builds remain visible without flooding the console.
        if ( s_gen_progress.current_time_ms - s_last_progress_print_ms < 1000 ) {
            return;
        }
        s_last_progress_print_ms = s_gen_progress.current_time_ms;

        // Optional tick update (e.g., printing progress every few seconds)
		gi.dprintf( "NavMesh Generation Progress: %.2f%%, Time Elapsed: %u ms\n", 
			s_gen_progress.progress_pct * 100.0f, 
			s_gen_progress.current_time_ms - s_gen_progress.start_time_ms 
		);
    }
}

/**
*	@brief	Start a new pathfinding query from `start` to `end`. This initializes the query state and returns a pointer to it.
*	@param	start The starting position of the path query.
*	@param	end The target position of the path query.
**/
nav_path_query_t* Nav_StartPathQuery(const Vector3& start, const Vector3& end) {
    nav_path_query_t* query = (nav_path_query_t*)gi.TagMallocz(sizeof(nav_path_query_t), TAG_SVGAME_NAVMESH);
    if (!query) return nullptr;
    
    query->start_pos = start;
    query->end_pos = end;
    query->is_finished = false;
    query->path_found = false;
    return query;
}

/**
*	@brief	Process a slice of the pathfinding query. This should be called repeatedly until `query->is_finished` is true.
*	@param	query The path query to process.
*	@param	max_time_ms The maximum amount of time (in milliseconds) to spend processing this
**/
void Nav_ProcessPathQuerySliced(nav_path_query_t* query, uint32_t max_time_ms) {
    if (!query || query->is_finished) return;
    
    uint32_t start_time = gi.GetRealTime();
    
    while (!query->is_finished) {
        // Stub: A* step processing goes here
        
        // Time slice check
        if ((gi.GetRealTime() - start_time) >= max_time_ms) {
            break; // Yield back to the server frame
        }
    }
}

/**
*	@brief	Free a path query and its associated resources.
*	@param	query The path query to free.
**/
void Nav_FreePathQuery(nav_path_query_t* query) {
    if (query) {
        gi.TagFree(query);
    }
}
