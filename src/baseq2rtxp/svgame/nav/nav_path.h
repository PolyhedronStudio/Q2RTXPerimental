#pragma once

#include "svgame/nav/nav_types.h"
#include <vector>

// Height thresholds for vertical connections
static constexpr float NAV_MAX_STEP_SIZE = 18.25f; // explicit navigation max step size (with epsilon to clear 18-unit physical steps)
static constexpr float PM_DROPOFF_ALLOWED_SIZE = 128.0f; // safe drop-off distance
static constexpr float PM_DROPOFF_MAX_SIZE = 196.0f; // max drop-off considered (above is lethal)

static constexpr float WAYPOINT_EPS_SQR = 4.0f * 4.0f;
static constexpr float PORTAL_EPS_SQR = 16.0f * 16.0f; // radius for portal crossing (16 to ensure monster center is near portal before steering next)

/**
*   @brief Navigation path policy containing movement constraints.
*   @note Fields are based on legacy nav2_path_policy_t used in older code.
*/
struct nav_path_policy_t {
    // Step and drop heights (in world units).
	float min_step_height = 0.0f;			// MM_STEP_MIN_SIZE
	    float max_step_height = 18.25f;          // Updated from 18.0f to clear exact 18-unit physical steps
    float max_drop_height = 128.0f;         // Increased allowable drop height
    // Optional cap on drop height.
    bool enable_max_drop_height_cap = true;
    float max_drop_height_cap = PM_DROPOFF_MAX_SIZE;

    // Waypoint and rebuild thresholds.
    float waypoint_radius = 32.0f; // increased from 16 to match entity bbox width
    float rebuild_goal_dist_2d = 32.0f;
    float rebuild_goal_dist_3d = 32.0f;
    int32_t rebuild_interval = 25; // milliseconds
    // Obstruction jumping.
    bool allow_small_obstruction_jump = true;
    float max_obstruction_jump_height = 48.0f;
    // Extra vertical clearance before attempting a step (helps avoid early stair entry).
    float step_clearance = 4.0f; // Reduced from 16.0f to allow earlier stair entry.

    // Visibility/visibility ignore flags.
    bool ignore_visibility = false;
    bool ignore_infront = false;
    
    // Gap jumping features.
    bool allow_gap_jumping = true; // Enabled by default for testing as per request
    float max_jump_distance = 256.0f;
    float min_gap_width = 24.0f;
};

/**
*   @brief  Walk the KD‑tree to locate the leaf node that contains @p point.
*   @param  point   World‑space position to query.
*   @return Index of the leaf node (-1 if the point lies outside the world bounds).
**/
int32_t Nav_FindLeafNode( const Vector3 &point );

/**
*   @brief  Check if a point lies within the 2D projection of a face.
*   @param  point   World-space position.
*   @param  face    Face to check against.
*   @return True if the point is inside.
**/
bool Nav_PointInsideFace2D( const Vector3 &point, const nav_face_t &face );

/**
*   @brief  Return the nav face that actually contains @p point.
*   @param  point   World‑space position.
*   @return Index of the face, or -1 if none found.
**/
int32_t Nav_FindPolyInLeaf( const Vector3 &point ); // Keeps same name so that usages don't break immediately, but can be updated later.

/**
*   @brief  Compute a path using A* from start to goal face.
*   @param  startFace   Index of the starting face.
*   @param  goalFace    Index of the goal face.
*   @param  outPath     Output vector to store the sequence of face indices.
*   @return True if a path was found.
**/
bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy );

/**
*   @brief  Fallback to find the absolutely closest face globally by 3D distance.
*   @param  point   The 3D point to search from.
*   @return The index of the closest face, or -1 if none exist.
**/
int32_t Nav_FindClosestPolyGlobal( const Vector3 &point ); // Keeping same name to limit API breakage right now.

/**
 *   @brief  Calculate portal midpoint and edge vector between two adjacent nav faces.
 *   @param  faceA       Index of the first face.
 *   @param  faceB       Index of the second face.
 *   @param  outMidpoint Output vector receiving the portal midpoint.
 *   @param  outEdgeVec  Optional output vector receiving the edge direction (v1 - v0).
 *   @return True if a shared edge was identified (including stairs/drop-offs). Returns false on invalid indices.
 */
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3 *outV0, Vector3 *outV1 );
