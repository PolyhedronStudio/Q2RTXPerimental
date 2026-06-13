#pragma once

#include "nav_core.h"
#include "shared/asynchronous_work.h"

// ---------------------------------------------------------
// Asynchronous Generation
// ---------------------------------------------------------

struct nav_gen_progress_t {
    float progress_pct;      // 0.0f to 1.0f
    uint32_t start_time_ms;
    uint32_t current_time_ms;
    uint32_t time_taken_ms;
    uint32_t estimated_time_left_ms;
    bool is_generating;
};

// Starts an asynchronous navigation generation task.
void Nav_StartAsyncGeneration();

// Checks the current progress of the generator.
const nav_gen_progress_t& Nav_GetGenerationProgress();

// Call this from the server's main loop to check if generation finished and to print progress.
void Nav_UpdateAsyncGeneration();

// ---------------------------------------------------------
// Time-Sliced Pathfinding
// ---------------------------------------------------------

// Context for a time-sliced pathfinding request.
struct nav_path_query_t {
    Vector3 start_pos;
    Vector3 end_pos;
    int32_t current_node; 
    int32_t goal_node;
    bool is_finished;
    bool path_found;
    // Future: A* open/closed lists will be added here
};

// Start a path query that will be evaluated over multiple frames.
nav_path_query_t* Nav_StartPathQuery(const Vector3& start, const Vector3& end);

// Process a chunk of the path query within the given time budget.
void Nav_ProcessPathQuerySliced(nav_path_query_t* query, uint32_t max_time_ms);

// Free the query when done
void Nav_FreePathQuery(nav_path_query_t* query);
