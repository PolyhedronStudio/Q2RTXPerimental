# Navigation System (NavMesh & Pathing)

This directory contains the navigation mesh (NavMesh) generation and A* pathfinding system for ServerGame entities. The system extracts walkable geometry directly from the BSP collision model, builds an optimized half-edge mesh and KD-Tree for fast spatial queries, and provides a flexible pathing API for AI movement.

## 1. NavMesh Generation (Chronological Order)

The NavMesh generation is an asynchronous process designed to extract walkable floors from the map's geometry without halting the server. The workflow occurs in the following chronological order:

### A. Initialization & Asynchronous Trigger
*   **Command**: The process is manually initiated via `Nav_GenerateCommand()`, which subsequently invokes `Nav_StartAsyncGeneration()`.
*   **Thread**: To avoid stalling the main game loop, generation is offloaded to a background worker thread. You can query its progress via `Nav_StatusCommand()`.

### B. BSP Geometry Extraction (`Nav_DoExtractionWork`)
*   **Collision Model**: The generator accesses the global collision model (`cm_t`) and its BSP cache.
*   **Brush Filtering**: It iterates through all BSP brushes, strictly filtering for `CONTENTS_SOLID`, `CONTENTS_DETAIL`, and `CONTENTS_MONSTERCLIP` brushes.
*   **Walkable Surface Filtering**: For every brush side, it checks the surface normal. Only planes with a Z normal >= `NAV_MIN_WALKABLE_Z` (0.65) are considered walkable floors.
*   **Winding Construction**: It constructs a base polygon (winding) for the walkable plane and chops it against all other sides of the parent brush to ensure the polygon perfectly fits the brush's convex volume.
*   **Boolean Subtraction**: Polygons are clipped against other intersecting solid brushes to remove overlapping areas (e.g., pillars resting on the floor). The resulting convex fragments become raw `nav_poly_t` structures.

### C. Topology Construction (`Nav_BuildHalfEdgeMesh`)
*   **Vertex Welding**: The system gathers all raw polygon vertices and welds nearby points to form a unified vertex array (`g_nav_vertices`).
*   **Half-Edge Linking**: The raw polygons are converted into a Half-Edge data structure (`nav_halfedge_t` and `nav_face_t`). Each edge knows its twin (the edge of an adjacent polygon). 
*   **Height Deltas**: During twin linking, vertical differences (`z_diff`) are recorded. This allows the mesh to represent stairs and drop-offs continuously.

### D. Spatial Partitioning & Optimization (`Nav_BuildKDTree`)
*   **KD-Tree Generation**: A 3D KD-Tree (`nav_kdtree_node_t`) is constructed, subdividing the faces along the X, Y, or Z axis. This provides $O(\log N)$ spatial queries when locating which face a 3D point belongs to.
*   **BSP Leaf Mapping**: For even faster $O(1)$ lookups, the system creates a mapping (`nav_leaf_link_t`) between traditional BSP leaf IDs and NavMesh faces. If an entity knows its current BSP leaf, it can instantly look up the subset of faces contained within that leaf.

### E. Persistence (`nav_persistence.cpp`)
*   **Saving**: The compiled navigation data is serialized and saved to disk as a `.nav7` file format containing a magic header (`NAV7_MAGIC`), the map's BSP checksum, and flat arrays of vertices, half-edges, faces, and KD-Tree nodes.
*   **Versioning**: `NAV7_VERSION` is bumped whenever nav extraction semantics change so stale cache files are rejected and regenerated.

---

## 2. Pathing System (A* Navigation)

Once the NavMesh is loaded into memory, AI entities can query it to find paths to targets. The pathing system enforces movement rules through a customizable policy.

### A. Localization
Before an entity can find a path, it must determine which nav face it is currently standing on:
*   `Nav_FindPolyInLeaf(point)`: Uses the KD-Tree (or BSP leaf shortcuts) to find the face containing the 3D point. It performs a 2D projection check and a vertical distance threshold.
*   `Nav_FindClosestPolyGlobal(point)`: A slower fallback that searches globally if the entity has somehow drifted entirely off the mesh.

### B. Policy Definition (`nav_path_policy_t`)
The pathfinding algorithms accept a policy struct that defines what the entity is physically capable of:
*   **Step Height** (`max_step_height`): Maximum height the entity can step up (e.g., 18.25 units).
*   **Drop Height** (`max_drop_height`): Maximum safe drop distance before pathing considers a cliff lethal.
*   **Gap Jumping** (`allow_gap_jumping` / `max_jump_distance`): Whether the entity can jump across disjointed faces.
*   **Clearance**: Avoidance thresholds.

### C. A* Path Search (`Nav_FindPath`)
*   The system runs an A* (A-Star) search across the half-edge mesh from the `startFace` to the `goalFace`.
*   During neighbor expansion, it checks the shared twin edge and validates the `z_diff` against the entity's `nav_path_policy_t` to see if a step or drop is legal.
*   It outputs a sequence of `int32_t` face IDs representing the successful path.

### D. Portal Traversal (`Nav_GetPortalEndpoints`)
*   As the entity moves along the face sequence, it needs specific 3D coordinates to steer towards.
*   `Nav_GetPortalEndpoints` calculates the shared edge (portal) between the current face and the next face, outputting the two vertices (`outV0`, `outV1`).
*   The entity uses these endpoints to compute a portal midpoint or use string-pulling (funnel algorithm) to walk smoothly to the next polygon.

---

## 3. External API Usage (How-To for Entities)

Below is an example of how an entity should interact with the navigation system to request and follow a path.

```cpp
#include "svgame/nav/nav_path.h"

// 1. Define movement capabilities for the entity
nav_path_policy_t policy;
policy.max_step_height = 18.25f;
policy.max_drop_height = 128.0f;
policy.allow_gap_jumping = true;
policy.waypoint_radius = 32.0f;

// 2. Localize the entity and its target onto the NavMesh
int32_t startFace = Nav_FindPolyInLeaf( entity->s.origin );
int32_t goalFace = Nav_FindPolyInLeaf( target_origin );

if ( startFace == -1 || goalFace == -1 ) {
    // Entity or target is off the NavMesh
    return false;
}

// 3. Request an A* path
std::vector<int32_t> facePath;
bool success = Nav_FindPath( startFace, goalFace, facePath, policy );

if ( success && facePath.size() > 1 ) {
    // 4. Extract the portal to the immediate next face in the path
    int32_t currentFace = facePath[0];
    int32_t nextFace = facePath[1];
    
    Vector3 portalEdgeStart, portalEdgeEnd;
    if ( Nav_GetPortalEndpoints( currentFace, nextFace, &portalEdgeStart, &portalEdgeEnd ) ) {
        // Calculate the center of the portal to walk towards
        Vector3 portalMidpoint = QM_Vector3Scale( QM_Vector3Add( portalEdgeStart, portalEdgeEnd ), 0.5f );
        
        // Steer the entity towards 'portalMidpoint'
        Vector3 moveDir = QM_Vector3Normalize( QM_Vector3Subtract( portalMidpoint, entity->s.origin ) );
        
        // ... apply moveDir to entity velocity ...
    }
}
