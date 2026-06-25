#pragma once

#include "nav_core.h"
#include "shared/asynchronous_work.h"

/**
* @brief Asynchronous navmesh generation progress snapshot.
* @note The worker thread updates these fields while the server reads them for status output.
**/
struct nav_gen_progress_t {
    //! Fractional completion in the range 0.0f..1.0f.
    float progress_pct = 0.0f;
    //! Server time when generation started.
    uint32_t start_time_ms = 0;
    //! Latest sampled server time.
    uint32_t current_time_ms = 0;
    //! Elapsed time in milliseconds since generation began.
    uint32_t time_taken_ms = 0;
    //! Estimated time remaining in milliseconds.
    uint32_t estimated_time_left_ms = 0;
    //! True while the asynchronous generation job is active.
    bool is_generating = false;
};

/**
* @brief Start asynchronous navmesh generation.
* @note Submits the extraction and build work to the engine async queue.
**/
void Nav_StartAsyncGeneration( void );

/**
* @brief Read the current generation progress snapshot.
* @return Reference to the global progress state.
**/
const nav_gen_progress_t &Nav_GetGenerationProgress( void );

/**
* @brief Advance the async generation status from the main server loop.
* @note Used to refresh elapsed time and emit bounded progress messages.
**/
void Nav_UpdateAsyncGeneration( void );

/**
* @brief Time-sliced pathfinding request state.
* @note The query is intentionally small so it can be processed over multiple frames.
**/
struct nav_path_query_t {
    //! World-space starting position.
    Vector3 start_pos = {};
    //! World-space target position.
    Vector3 end_pos = {};
    //! Current graph node being processed.
    int32_t current_node = -1;
    //! Goal graph node for the path query.
    int32_t goal_node = -1;
    //! True once the query has finished processing.
    bool is_finished = false;
    //! True when a valid path has been found.
    bool path_found = false;
};

/**
* @brief Create a new path query evaluated over multiple frames.
* @param start World-space starting position.
* @param end World-space ending position.
* @return Pointer to an allocated query object, or nullptr on allocation failure.
**/
nav_path_query_t *Nav_StartPathQuery( const Vector3 &start, const Vector3 &end );

/**
* @brief Process a slice of the path query within the time budget.
* @param query Query object to advance.
* @param max_time_ms Maximum time budget in milliseconds for this slice.
**/
void Nav_ProcessPathQuerySliced( nav_path_query_t *query, uint32_t max_time_ms );

/**
* @brief Release a path query allocated by Nav_StartPathQuery.
* @param query Query object to destroy.
**/
void Nav_FreePathQuery( nav_path_query_t *query );
