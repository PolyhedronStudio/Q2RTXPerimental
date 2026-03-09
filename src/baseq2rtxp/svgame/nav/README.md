# Navigation Voxelmesh / Navmesh API (`svgame/nav`)

This folder implements the runtime navigation voxelmesh used by SVGame entities for A*  pathfinding and waypoint-following.

It is not a traditional polygon navmesh. Instead it is a sparse, tiled, multi-layer sample grid designed to work well with Quake 2 BSP collision and PMove-style stepping.

Primary public headers:

- `svg_nav.h` (core mesh structures, agent profile, inline-model runtime)
- `svg_nav_traversal.h` (A*  traversal path generation and path querying)
- `svg_nav_path_process.h` (per-entity path process: throttling, backoff, direction queries)
- `svg_nav_request.h` (async request queue)
- `svg_nav_clusters.h` (coarse tile cluster routing)

---

## 1) What the “navmesh” is in this codebase

### 1.1 Voxelmesh, not polygons
The nav system stores walkable surface samples on a fixed XY grid (cell size configured via cvars). Each XY cell can have multiple Z layers to represent multi-floor spaces.

At runtime, A*  expands across this grid and validates candidate transitions using real collision traces and conservative step logic.

### 1.2 What it consists of
Core structures (see `svg_nav.h`):

- `nav_layer_t`
  - One sampled walkable surface at an XY cell.
  - Stores quantized Z (`z_quantized`), `flags` (water/lava/slime/ladder), and optional `clearance`.

- `nav_xy_cell_t`
  - One XY cell entry.
  - Stores `num_layers` and a `layers` array.

- `nav_tile_t`
  - Tiles group many XY cells to keep lookup cache-friendly and allocations sparse.
  - Uses `presence_bits` to indicate which XY cells exist.

- `nav_leaf_data_t`
  - World navigation data is organized per BSP leaf.

- `nav_mesh_t`
  - The complete runtime mesh.
  - Holds world tiles, per-leaf lookup data, default agent bounds, and inline-model data.

### 1.3 Layer flags and environmental classification
`nav_layer_flags_t` values:

- `NAV_FLAG_WALKABLE`
- `NAV_FLAG_WATER`
- `NAV_FLAG_LAVA`
- `NAV_FLAG_SLIME`
- `NAV_FLAG_LADDER`

These are used for:

- traversal heuristics (weighting/hazard avoidance)
- filtering and constraints
- debug drawing/diagnostics

---

## 2) Coordinate spaces (critical for correct usage)

The nav system commonly talks about two related spaces:

### 2.1 Feet-origin space (entity gameplay space)
Most gameplay code uses a “feet-origin” with Z representing ground contact.

All high-level traversal APIs accept feet-origin coordinates.

### 2.2 Nav-center space (A*  internal space)
Node lookups and edge validation are performed in “nav-center space,” which is the feet-origin translated by the hull center offset.

Helper:

- `SVG_Nav_ConvertFeetToCenter( mesh, feet_origin, agent_mins, agent_maxs )`

Why this matters:

- start/goal node selection is highly sensitive to the Z layer chosen at a given XY
- supplying the wrong bbox (or mixing feet vs. center conventions) can shift the origin into the wrong Z layer and make A*  fail

Rule of thumb:

- prefer the navmesh agent bbox (or the cvar-derived profile) for all traversal requests
- only use entity collision bounds as last-resort fallback

---

## 3) API use categories

The API is easiest to use as a stack of layers:

1. * * Generation / load / save* * : build or load a mesh for the map.
2. * * Traversal (A* )* * : build a waypoint path between two origins.
3. * * Path process (per-entity)* * : keep and follow a path over time; throttle rebuild.
4. * * Async request queue* * : amortize path rebuild cost across frames.
5. * * Coarse cluster routing* * : speed up long-distance routes.
6. * * Debug tools* * : visualize mesh and paths.

Each category below describes when/what/how to use.

---

## 4) Generation / initialization / persistence

### 4.1 Mesh generation (`svg_nav_generate.* `)
When to use:

- during development to generate nav data for the current map
- when your workflow supports runtime generation from BSP

How to use:

- console command: `sv nav_gen_voxelmesh`

Important cvars (common ones):

- grid/tile: `nav_cell_size_xy`, `nav_tile_size`, `nav_z_quant`
- traversal constraints: `nav_max_step`, `nav_max_drop`, `nav_max_drop_height_cap`, `nav_max_slope_normal_z`
- agent bbox: `nav_agent_mins_* `, `nav_agent_maxs_* `

Pitfall:

- any changes to agent bbox or traversal limit assumptions typically require regeneration (or ensuring saved data was built with the same constraints).

### 4.2 Save/load (`svg_nav_save.* `, `svg_nav_load.* `)
When to use:

- persist generated nav data so it doesn’t need regeneration each run

What it does:

- serializes the sparse tiles/cells/layers and mesh metadata
- restores the mesh and its derived lookup state on load

---

## 5) Inline models (movers)

Maps contain movers (brush models like `func_door`, `func_plat`). The nav system supports inline model nav data by storing it separately and applying runtime transforms.

Key update call:

- `SVG_Nav_RefreshInlineModelRuntime()`

When to call:

- once per frame (or from the nav tick)

---

## 6) Traversal (A* ) paths

Traversal APIs are in `svg_nav_traversal.h`. They generate a `nav_traversal_path_t` (an owned set of waypoints and metadata).

### 6.1 Synchronous path generation
When to use:

- dev tooling
- low-agent-count gameplay scenarios
- fallback when async is disabled

Main entry points:

- `SVG_Nav_GenerateTraversalPathForOrigin( start, goal, out_path )`
- `SVG_Nav_GenerateTraversalPathForOriginEx( start, goal, out_path, enable_blend, blend_start_dist, blend_full_dist )`
- `SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( start, goal, out_path, agent_mins, agent_maxs, policy )`
- `SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( ... )`

How to use:

1. Pick the agent bbox to use for the query.
2. Pick a `svg_nav_path_policy_t` (or pass `nullptr` in lower-level APIs where allowed).
3. Call a `...WithAgentBBox` overload.
4. If it succeeds, follow the returned path (query directions each tick).
5. Free when done.

Cleanup:

- `SVG_Nav_FreeTraversalPath( &path )`

### 6.2 Path querying / steering
To follow an existing `nav_traversal_path_t`:

- `SVG_Nav_QueryMovementDirection( path, current_origin, waypoint_radius, policy, &index, &out_direction )`

Typical use:

- store `path_index` as part of your entity
- call the query function each frame
- use the returned normalized direction for facing and velocity

---

## 7) Movement feasibility and edge validation helpers

These functions are used by traversal internally (to validate grid transitions using traces), but are also useful for diagnostics and custom movement checks.

From `svg_nav_traversal.h`:

- `Nav_Trace( start, mins, maxs, end, clip_entity, mask )`
- `Nav_CanTraverseStep( mesh, start_center, end_center, clip_entity )`
- `Nav_CanTraverseStep_ExplicitBBox( mesh, start_center, end_center, mins, maxs, clip_entity, policy )`

Important:

- these step tests operate in nav-center space for the start/end positions.

---

## 8) Per-entity path processing (`svg_nav_path_process_t`)

`svg_nav_path_process_t` is intended to be embedded in an entity to provide stable path state and sensible rebuild throttling.

What it provides:

- current `path` plus `path_index`
- start/goal change tracking
- backoff after repeated failures
- synchronous rebuild helpers
- direction query helpers (2D and 3D)
- async request ownership (handle + generation)

Key methods (see `svg_nav_path_process.h`):

- `Reset()`
- `RebuildPathTo( start_origin, goal_origin, policy )`
- `RebuildPathToWithAgentBBox( start_origin, goal_origin, policy, agent_mins, agent_maxs, force )`
- Rebuild gates: `CanRebuild`, `ShouldRebuildForGoal2D/3D`, `ShouldRebuildForStart2D/3D`
- Steering: `QueryDirection2D`, `QueryDirection3D`
- Utility: `GetNextPathPointEntitySpace`

When to prefer `QueryDirection3D`:

- stairs/slopes/step handling matters and you want a direction consistent with the vertical structure of waypoints.

---

## 9) Async request queue (`svg_nav_request.* `)

The async queue exists to avoid doing blocking A*  solves for many agents in the same frame.

When to use:

- monsters that pursue targets continuously
- any scenario where many entities may retarget/rebuild at once

Key API (see `svg_nav_request.h`):

- `SVG_Nav_RequestPathAsync( pathProcess, start, goal, policy, agent_mins, agent_maxs, force )`
- `SVG_Nav_CancelRequest( handle )`
- `SVG_Nav_IsRequestPending( pathProcess )`
- `SVG_Nav_ProcessRequestQueue()`

Configuration helpers:

- `SVG_Nav_IsAsyncNavEnabled()`
- `SVG_Nav_GetAsyncRequestBudget()`
- `SVG_Nav_GetAsyncRequestExpansions()`

Usage rules:

- always cancel pending requests when swapping goals/modes or freeing the entity
- do not rely on `rebuild_in_progress` alone for determining if work is pending; use `SVG_Nav_IsRequestPending`

---

## 10) Coarse tile cluster routing (`svg_nav_clusters.* `)

The cluster graph is a coarse adjacency graph over world tiles.

Intended usage:

1. resolve start/goal tiles
2. BFS across tile adjacency to get a tile route
3. run fine A*  restricted to tiles on that route

This reduces fine A*  search space on large maps.

---

## 11) Debug / visualization (`svg_nav_debug.* `)

Use these utilities to:

- verify generated tiles/layers
- visualize paths and waypoint indices
- diagnose why traversal failed (blocked step, too steep, too large drop, no layer match)

---

## 12) Example: testdummy debug monster (`MoveAStarToOrigin` pattern)

The test dummy debug monster demonstrates the intended runtime flow: embed a `svg_nav_path_process_t`, enqueue rebuilds (preferably async), and follow the path each tick.

Relevant code:

- `src/baseq2rtxp/svgame/entities/monster/svg_monster_testdummy_debug.cpp`
- helper routines in `src/baseq2rtxp/svgame/entities/monster/svg_monster_testdummy_puppet.cpp`

Conceptual workflow (mirrors how `MoveAStarToOrigin(...)` is used):

1. Determine the nav agent bbox
   - use `svg_monster_testdummy_debug_t::GetNavigationAgentBounds( &agent_mins, &agent_maxs )`
   - prefer mesh/cvar agent bounds so feet->center conversion matches mesh generation

2. Decide if a rebuild is needed
   - throttle with `pathNavigationState.process.CanRebuild( policy )`
   - detect goal changes with `pathNavigationState.process.ShouldRebuildForGoal2D/3D( goal, policy )`

3. Enqueue an async rebuild
   - `handle = SVG_Nav_RequestPathAsync( &pathNavigationState.process, start, goal, policy, agent_mins, agent_maxs, force )`
   - store `handle` for cancellation if the goal changes

4. While async runs, optionally apply fallback pursuit
   - keeps motion responsive while waiting for the new waypoint list

5. Follow the committed path
   - `pathNavigationState.process.QueryDirection3D( self->currentOrigin, policy, &dir3d )`
   - translate direction into facing/velocity
   - run physics (`PerformSlideMove`) and update entity angles

6. On mode switch / deactivation
   - cancel pending async requests and free cached paths (see helper like `SVG_Monster_ResetNavigationPath( self )`)
