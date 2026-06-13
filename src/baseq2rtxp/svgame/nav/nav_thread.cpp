#include "nav_thread.h"
#include "nav_generate.h"

static nav_gen_progress_t s_gen_progress = {};
static asyncwork_t s_gen_work = {};

static void Nav_AsyncGenerationWork(void* arg) {
    // This runs on a background thread.
    uint32_t start = gi.GetRealTime();
    s_gen_progress.start_time_ms = start;
    
    // Run the actual BSP extraction logic here
    Nav_DoExtractionWork();
    
    // Stub: simulate work for now. Real BSP extraction will be implemented in Step 2.
    for (int i = 0; i <= 100; i++) {
        // Update progress
        s_gen_progress.progress_pct = i / 100.0f;
        s_gen_progress.current_time_ms = gi.GetRealTime();
        s_gen_progress.time_taken_ms = s_gen_progress.current_time_ms - start;
        
        if (i > 0) {
            float time_per_pct = (float)s_gen_progress.time_taken_ms / i;
            s_gen_progress.estimated_time_left_ms = (uint32_t)(time_per_pct * (100 - i));
        } else {
            s_gen_progress.estimated_time_left_ms = 0;
        }
    }
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
    s_gen_progress.is_generating = true;
    s_gen_progress.start_time_ms = gi.GetRealTime();
    
    s_gen_work.work_cb = Nav_AsyncGenerationWork;
    s_gen_work.done_cb = Nav_AsyncGenerationDone;
    s_gen_work.cb_arg = nullptr;
    
    gi.Com_QueueAsyncWork(&s_gen_work);
    gi.dprintf("NavMesh Generation Started...\n");
}

const nav_gen_progress_t& Nav_GetGenerationProgress() {
    return s_gen_progress;
}

void Nav_UpdateAsyncGeneration() {
    if (s_gen_progress.is_generating) {
        // Optional tick update (e.g., printing progress every few seconds)
    }
}

nav_path_query_t* Nav_StartPathQuery(const Vector3& start, const Vector3& end) {
    nav_path_query_t* query = (nav_path_query_t*)gi.TagMallocz(sizeof(nav_path_query_t), TAG_SVGAME_NAVMESH);
    if (!query) return nullptr;
    
    query->start_pos = start;
    query->end_pos = end;
    query->is_finished = false;
    query->path_found = false;
    return query;
}

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

void Nav_FreePathQuery(nav_path_query_t* query) {
    if (query) {
        gi.TagFree(query);
    }
}
