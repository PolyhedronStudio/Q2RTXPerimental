# Navigation System (NavMesh, Pathfinding & Locomotion)

A high-performance, deterministic 3D navigation and kinematic locomotion pipeline for ServerGame entities. Built around a double-precision half-edge navigation mesh, KD-Tree accelerated spatial queries, corridor-constrained Funnel string pulling with convex corner standoffs, precalculated tactical cover evaluation, discrete topological sector graphs, and custom step-slide capsule physics.

---

## Table of Contents
1. [Architectural Overview & Design Principles](#1-architectural-overview--design-principles)
2. [The BAAS Framework](#2-the-baas-framework)
3. [Bake: NavMesh Generation, CSG & Spatial Indexing](#3-bake-navmesh-generation-csg--spatial-indexing)
4. [Activate: Agent Hull, Stance Configuration & Locomotion Policy](#4-activate-agent-hull-stance-configuration--locomotion-policy)
5. [Assign: Target Assignment, Connected Components & Path Planning](#5-assign-target-assignment-connected-components--path-planning)
6. [Simulate: Locomotion, Kinematic Slide-Move & Active Steering](#6-simulate-locomotion-kinematic-slide-move--active-steering)
7. [How-To Guide: Monster Navigation from A to Z](#7-how-to-guide-monster-navigation-from-a-to-z)
	- [7.1 Stage A: Entity Spawning, Bounding Boxes & Hull Collision Setup](#71-stage-a-entity-spawning-bounding-boxes--hull-collision-setup)
	- [7.2 Stage B: Traversal Policy Configuration & Custom Edge Costing](#72-stage-b-traversal-policy-configuration--custom-edge-costing)
	- [7.3 Stage C: Server Think Loop Lifecycle (`GenericThinkBegin` / `GenericThinkFinish`)](#73-stage-c-server-think-loop-lifecycle-genericthinkbegin--genericthinkfinish)
	- [7.4 Stage D: Target Assignment, Pursuit & Path Recalculation (`MoveAStarToOrigin`)](#74-stage-d-target-assignment-pursuit--path-recalculation-moveastartorigin)
	- [7.5 Stage E: Waypoint Progression, Lookahead & Corner Steering](#75-stage-e-waypoint-progression-lookahead--corner-steering)
	- [7.6 Stage F: Milestone Callbacks & Tactical Event Interception (`OnWaypointReached`, `EvaluateWaypointArrival`)](#76-stage-f-milestone-callbacks--tactical-event-interception-onwaypointreached-evaluatewaypointarrival)
	- [7.7 Stage G: Tactical Cover Seeking, Dynamic Occlusion & Ambush Postures (`OnCoverPointReached`)](#77-stage-g-tactical-cover-seeking-dynamic-occlusion--ambush-postures-oncoverpointreached)
	- [7.8 Stage H: Chokepoint Queuing, Sector Traversal & Right-of-Way](#78-stage-h-chokepoint-queuing-sector-traversal--right-of-way)
	- [7.9 Stage I: Contact Stalling, Blocked Frames & Autonomous Unstick Recovery](#79-stage-i-contact-stalling-blocked-frames--autonomous-unstick-recovery)
8. [Comprehensive Public API Reference](#8-comprehensive-public-api-reference)
	- [8.1 Core Types, Bitmasks & Geometric Limits](#81-core-types-bitmasks--geometric-limits)
	- [8.2 Agent Traversal Policy: `nav_path_policy_t`](#82-agent-traversal-policy-nav_path_policy_t)
	- [8.3 Path Planning & Spatial Localization API](#83-path-planning--spatial-localization-api)
	- [8.4 Topological Geometric Queries & Half-Edge Raycasting](#84-topological-geometric-queries--half-edge-raycasting)
	- [8.5 Topological Sector Graph Subsystem API](#85-topological-sector-graph-subsystem-api)
	- [8.6 Tactical Cover Point Subsystem API](#86-tactical-cover-point-subsystem-api)
	- [8.7 Kinematic Locomotion & SlideMove Subsystem](#87-kinematic-locomotion--slidemove-subsystem)
	- [8.8 Monster Base Navigation Interface](#88-monster-base-navigation-interface)
	- [8.9 Server Debug Primitive Drawing API](#89-server-debug-primitive-drawing-api)
9. [Tactical Behavior Bitflags & Crowd Formations](#9-tactical-behavior-bitflags--crowd-formations)
10. [Elaborative Interrogation (Deep-Dive Design Rationale)](#10-elaborative-interrogation-deep-dive-design-rationale)
11. [Integration Guide 1: Custom Entity (`svg_base_edict_t`)](#11-integration-guide-1-custom-entity-svg_base_edict_t)
12. [Integration Guide 2: Production Monster (`svg_monster_tactical_example_t`)](#12-integration-guide-2-production-monster-svg_monster_tactical_example_t)
13. [Level Design, Map Architecture & Brush Guidelines](#13-level-design-map-architecture--brush-guidelines)
14. [CVars, Console Commands & Visual Diagnostics](#14-cvars-console-commands--visual-diagnostics)

---

## 1. Architectural Overview & Design Principles

The navigation system provides full-stack pathfinding, geometric reasoning, and steering for AI agents:
* **Geometry Engine**: Extracts walkable brush windings directly from the BSP collision model (`cm_t`), performs CSG boolean clipping, dissolves sliver artifacts, and constructs a topological Half-Edge mesh.
* **Spatial Acceleration**: Combines a balanced 3D KD-Tree with a direct BSP Leaf Mapping table for instant $O(1)$ point-to-polygon localization with robust $O(\log N)$ global geometric fallbacks.
* **Path Search**: Evaluates multi-criteria A\* over half-edge twins, handling stairs, step-ups, drops, dynamic door states, and custom cost callbacks.
* **Corridor Funneling**: Converts A\* face corridors into smooth, minimum-distance polylines via the Simple, Stupid Funnel Algorithm (SSFA) in double precision ([`Vector3DP`](../../../shared/math/qm_vector3_dp.h)).
* **Convex Corner Decoupling**: Analytically projects obstacle corner bisectors to insert standoff waypoints, guaranteeing physical capsule clearance around sharp geometry.
* **Topological Sector Graph**: Automatically partitions face clusters into semantic rooms, corridors, stairs, ramps, and platforms, calculating all-pairs topological routing and doorway aperture right-of-way.
* **Tactical Cover Point System**: Derives precalculated cover points along navigable perimeters with validated posture classification, dynamic mover binding, and threat-relative scoring.
* **Kinematic Simulation**: Steers capsules using [`SVG_MMove_StepSlideMove`](../monsters/svg_mmove.h#L339) (multi-plane sliding with predictive step-ups), completely bypassing legacy `SV_WalkMove`.

---

## 2. The BAAS Framework

The navigation lifecycle follows four modular stages:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. BAKE: Offline / Background Geometry Compilation                     │
│    BSP Brushes ──> Surface Normal Filter ──> CSG Subtraction ──>        │
│    Sliver Dissolution ──> Half-Edge Topology ──> KD-Tree ──> .nav7     │
└────────────────────────────────────────────────────┬────────────────────┘
                                                     │
┌────────────────────────────────────────────────────▼────────────────────┐
│ 2. ACTIVATE: Entity Bounds & Policy Definition                          │
│    SOLID_CAPSULE ──> nav_path_policy_t ──> mm_move_t Locomotion State  │
└────────────────────────────────────────────────────┬────────────────────┘
                                                     │
┌────────────────────────────────────────────────────▼────────────────────┐
│ 3. ASSIGN: Goal Localization & Path Planning                            │
│    KD-Tree Leaf Query ──> A* Graph Search ──> Funnel String Pulling ──> │
│    Corner Standoff Enforcement ──> Collinear Decimation LOS Guard       │
└────────────────────────────────────────────────────┬────────────────────┘
                                                     │
┌────────────────────────────────────────────────────▼────────────────────┐
│ 4. SIMULATE: Server Tick Physics & Steering                             │
│    Waypoint Tracking ──> Lookahead Gating ──> StepSlideMove Physics ──> │
│    Crowd Formation Repulsion ──> Wall-Stall Unstick Recovery            │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Bake: NavMesh Generation, CSG & Spatial Indexing

The NavMesh compiler extracts walkable surfaces directly from the BSP collision model asynchronously on a dedicated worker thread ([`Nav_StartAsyncGeneration`](nav_thread.h)), ensuring the server never stalls.

### Step-by-Step Compilation Pipeline

#### 1. Brush Filtering & Winding Construction ([`Nav_DoExtractionWork`](nav_generate.h#L59))
* Scans all BSP brushes in the map for `CONTENTS_SOLID`, `CONTENTS_DETAIL`, and `CONTENTS_MONSTERCLIP`.
* Brushes explicitly flagged with `CONTENTS_NO_NAVMESH` (or helper origin brushes) are completely excluded from the generation equation, regardless of whether they belong to static world geometry or separate dynamic model entities.
* Discards surfaces flagged with `CM_SURFACE_FLAG_SKY` or `CM_SURFACE_NO_NAVMESH`.
* Examines every brush plane. Only surfaces with normal $Z \ge \text{NAV\_MIN\_WALKABLE\_Z}$ ($0.65$, max slope $\approx 49.5^\circ$) are considered walkable ground.
* Generates a base winding on the plane and chops it against all other bounding planes of the brush, creating a convex walkable polygon ([`nav_poly_t`](nav_types.h#L101)).

#### 2. CSG Boolean Subtraction & Sliver Dissolution ([`nav_csg.cpp`](nav_csg.cpp))
* Clips walkable polygons against intersecting solid brushes (e.g. pillars, walls, curbs) to carve out obstacles.
* **Sliver Dissolution**: High-poly BSP cuts often create degenerate, razor-thin sliver triangles. Polygons are evaluated for feature width:
  $$w = \frac{2 \times \text{Area}}{\text{Perimeter}}$$
  Any fragment with $w < 2.0\,\text{units}$, bounding extent $< 2.0\,\text{units}$, or $\text{Area} < 4.0\,\text{units}^2$ is dissolved and merged into neighboring polygons to prevent routing through sub-hull gaps.

#### 3. Half-Edge Topology Construction ([`nav_generate.cpp`](nav_generate.cpp))
* Welds co-located vertices within spatial tolerance into a unified vertex array ([`g_nav_vertices`](nav_path.h#L533)).
* Builds half-edge pairs ([`nav_halfedge_t`](nav_types.h#L123) and [`nav_face_t`](nav_types.h#L244)). Each half-edge stores:
  * `vertex_idx`: Originating vertex index.
  * `next_idx`: Next half-edge in counter-clockwise order around the face.
  * `twin_idx`: Index of the opposing half-edge belonging to the neighboring polygon ($-1$ if solid boundary).
  * `z_diff`: Vertical difference across the seam. If $|z_{\text{diff}}| \le \text{NAV\_MAX\_STEP\_HEIGHT}$ (derived as `PHYS_STEP_MAX_SIZE + PHYS_STEP_GROUND_DIST`, currently $18.25\,\text{units}$), it is tagged as a traversable step; if higher, it represents a one-way drop-off or impassable cliff.
  * `wall_offset`: For boundary edges, how far the edge was pushed inward from the original brush geometry (runtime inspection metadata).
  * `edge_entity_id`: Dynamic entity (e.g. door) this edge transitions into, or `ENTITYNUM_NONE`.
  * `flags`: Dynamic attributes from [`nav_edge_flags_t`](nav_types.h#L145) — `NAV_EDGE_DISABLED` (temporarily blocked, e.g. closed door), `NAV_EDGE_WALL` (solid upward barrier), and `NAV_EDGE_DROPOFF` (elevated ledge/cliff dropping into lower space).
* Builds [`g_nav_entity_edges`](nav_path.h#L537), an entity-to-half-edge mapping table enabling $O(1)$ dynamic edge state updates when movers (doors, plats, walls) change state.

#### 4. Spatial Indexing ([`nav_kdtree_builder.cpp`](nav_kdtree_builder.cpp))
* **KD-Tree**: Constructs a balanced 3D KD-tree ([`nav_kdtree_node_t`](nav_types.h#L277)) recursively splitting face centroids across X, Y, and Z axes. Provides $O(\log N)$ lookup time for arbitrary 3D queries.
* **BSP Leaf Mapping**: Maps engine BSP leaf numbers to NavMesh face subsets ([`nav_leaf_link_t`](nav_types.h#L302)). When an entity is inside a valid BSP leaf, candidate faces are retrieved in $O(1)$ time.

#### 5. Topological Decomposition & Cover Generation
* **Spatial Regions & Portals**: [`Nav_BuildSpatialRegionsAndPortals`](nav_generate.h#L70) segments the face graph into semantic zones ([`nav_room_t`](nav_types.h#L218) / [`nav_portal_t`](nav_types.h#L192)) — enclosed rooms, flat/stair/ramp corridors, alcoves, elevated platforms — and assigns `nav_face_t::room_id` for $O(1)$ zone queries.
* **Topological Sector Graph**: [`Nav_SectorGraph_Build`](nav_sector_graph.h#L58) derives a gapless connectivity graph across decomposed sectors, computing all-pairs shortest paths and portal sequences.
* **Tactical Cover Points**: [`Nav_GenerateCoverPoints`](nav_cover_generate.h) precalculates cover positions along navigable boundary edges with posture classification and peek validation.
* **Topology Validation**: [`Nav_ValidateTopology`](nav_generate.h#L114) checks half-edge and KD-tree ownership invariants between stages, emitting aggregate diagnostics with a capped failure report.

#### 6. Serialization & Persistence ([`nav_persistence.cpp`](nav_persistence.cpp))
* Serializes the compiled data into `baseq2rtxp/maps/<mapname>.nav7`.
* The header ([`nav_header_t`](nav_types.h#L314)) stores `NAV7_MAGIC` (`'NAV7'`), the format version `NAV7_VERSION` (currently `5`), and element counts for every serialized block: vertices, half-edges, faces, KD-tree nodes, BSP leaf links, leaf face-id spans, and tactical cover points.
* On map load, magic and version are validated before deserialization via [`Nav_Load`](nav_persistence.h#L18). After loading, the cover spatial index is rebuilt ([`Nav_RebuildCoverSpatialIndex`](nav_cover_query.h#L135)) and topological zones/portals are re-derived ([`Nav_BuildSpatialRegionsAndPortals`](nav_generate.h#L70)), so cached meshes behave identically to freshly generated ones.
* Transient cover reservation state (`claimed_by_ent`, `claim_expiration`, `cooldown_until`) is deliberately reset on load and never persisted.
* If the cache is missing or the version mismatches, background generation begins automatically.

---

## 4. Activate: Agent Hull, Stance Configuration & Locomotion Policy

Every navigating entity must establish its physical collision footprint and traversal constraints.

### Locomotion Policy ([`nav_path_policy_t`](nav_path.h#L183))

```cpp
nav_path_policy_t policy{};
policy.agent_radius               = 16.0;   // Bounding cylinder radius (units)
policy.agent_mins                 = { -16.0f, -16.0f, -36.0f }; // Full collision volume mins
policy.agent_maxs                 = {  16.0f,  16.0f,  36.0f }; // Full collision volume maxs
policy.trace_shape                = 1;      // Analytical mover trace shape (0=Auto, 1=Capsule, 2=Cylinder)
policy.min_step_normal            = 0.7f;   // Walkable landing normal Z threshold for step-ups
policy.min_step_height            = 0.0f;   // Minimum step height threshold (units)
policy.max_step_height            = 18.25f; // Max vertical rise agent can climb (units)
policy.max_drop_height            = 128.0f; // Max safe descent drop (units)
policy.enable_max_drop_height_cap = true;   // Strict cutoff on lethal drops (NAV_DROPOFF_MAX_SIZE, 196.0)
policy.max_drop_height_cap        = 196.0f; // Hard lethal drop threshold (units)
policy.min_portal_width           = 0.5f;   // Minimum portal passage width factor
policy.waypoint_radius            = 24.0f;  // Arrival threshold for intermediate points (units)
policy.rebuild_goal_dist_2d       = 32.0f;  // Goal movement 2D threshold triggering repath (units)
policy.rebuild_goal_dist_3d       = 32.0f;  // Goal movement 3D threshold triggering repath (units)
policy.rebuild_interval           = 25;     // Frame debounce interval (ms)
policy.allow_small_obstruction_jump = true; // Allow low vaulting / step hops
policy.max_obstruction_jump_height  = 48.0f;// Maximum obstacle jump height (units)
policy.step_clearance             = 4.0f;   // Overhead clearance padding before attempting step
policy.allow_gap_jumping          = false;  // Whether agent can leap across disjoint meshes
policy.max_jump_distance          = 256.0f; // Maximum gap jump horizontal distance (units)
policy.min_gap_width              = 24.0f;  // Minimum gap width required to trigger leap (units)
policy.ignore_disabled_edges      = false;  // Set true to allow routing through closed doors
policy.allow_entity_deflection    = true;   // Lateral deflection when bumping other living entities

// Optional entity-specific A* cost customization (stair preference, hazard avoidance, etc.):
policy.edge_cost_callback         = &MyEdgeCostFunc;  // nav_path_edge_cost_fptr signature
policy.edge_cost_monster          = myMonster;        // Passed through to the callback
```

### Physical Collision Setup
Navigation agents use `SOLID_CAPSULE` hulls with `MOVETYPE_STEP` or `MOVETYPE_ROOTMOTION`:
* Standard Standing Hull: mins `{-16.0f, -16.0f, -36.0f}`, maxs `{16.0f, 16.0f, 36.0f}`.
* Standard Crouching Hull: mins `{-16.0f, -16.0f, -36.0f}`, maxs `{16.0f, 16.0f, 8.0f}`.
* Trace Shape: `TRACE_SHAPE_CAPSULE` (`MM_SHAPE_CAPSULE`) to match sweep tests with continuous physics.

---

## 5. Assign: Target Assignment, Connected Components & Path Planning

Goal assignment transforms a 3D target coordinate into a smoothed, corner-decoupled polyline.

### Execution Pipeline

#### 1. Localization ([`Nav_FindReachableFaceInLeaf`](nav_path.h#L379))
* Converts feet-origin coordinates ($z_{\text{feet}} = z_{\text{origin}} + \text{mins.z}$) to a NavMesh face index.
* Checks the current BSP leaf first ($O(1)$); falls back to the 3D KD-Tree ($O(\log N)$).
* Validates that the start face and goal face reside on the same connected topological component ([`Nav_GetFaceComponent`](nav_path.h#L367)). If the point rests on an isolated sliver, the nearest face within the goal's connected component that overlaps the agent's capsule footprint is substituted, preventing agents from being stranded on degenerate boundary fragments.
* Primitives: [`Nav_FindPolyInLeaf`](nav_path.h#L277) (leaf query with global fallback), [`Nav_FindFaceInLeafStrict`](nav_path.h#L289) (leaf-local only, no fallback), and [`Nav_FindClosestPolyGlobal`](nav_path.h#L347) (absolute 3D fallback).

#### 2. Topological A\* Search ([`Nav_FindPath`](nav_path.h#L302))
* Traverses the half-edge graph using Euclidean distance heuristic $h(n)$.
* Neighbor cost evaluation accounts for slope, surface material, and step delta. An optional per-entity cost hook (`nav_path_policy_t::edge_cost_callback`) can inflate or penalize individual transitions (e.g. hazardous liquid, stair preference, tactical avoidance).
* **Narrow Portal Rejection**: Portals narrower than `NAV_ABSOLUTE_MIN_PORTAL_PASSAGE_WIDTH` ($20.0\,\text{units}$) are rejected during graph expansion, preventing paths through impassable gaps.
* Outputs an ordered sequence of face indices: `std::vector<int32_t> outFacePath`.
* **Query Diagnostics**: Every A\* query refreshes a one-shot rejection summary ([`nav_path_diagnostics_t`](nav_path.h#L309)) counting expanded faces, accepted transitions, and per-reason rejections (disabled edges, missing portals, narrow portals, missing agent corridor, step/drop height). [`Nav_LogLastPathDiagnostics`](nav_path.h#L340) prints the summary once and is used by `nav_dbg_test` after failed routes — no per-frame or per-edge logging is emitted.

#### 3. Funnel Algorithm & Portal Clipping ([`Nav_StringPull`](nav_path.h#L493))
* For every adjacent face pair $(F_i, F_{i+1})$, extracts the shared portal segment via [`Nav_GetPortalEndpoints`](nav_path.h#L392).
* Clips portal ends inward by `agent_radius + NAV_PORTAL_CLEARANCE_MARGIN` ($16 + 4 = 20\,\text{units}$) using `Nav_ClipPortalForAgentClearance`, which also applies corner clearance (`NAV_CORNER_CLEARANCE_MARGIN`, $12\,\text{units}$) where portals abut obstacle corners.
* **Constricted Portals (Doorways)**: When a passage is narrower than two agent radii ($minT \ge maxT$), it collapses to its exact geometric midpoint and sets `portal.force_waypoint = true`. This forces the funnel algorithm to reset its apex at the doorway center.
* Runs the Simple, Stupid Funnel Algorithm (SSFA) in double precision ([`Vector3DP`](../../../shared/math/qm_vector3_dp.h)) to produce the raw path polyline.

#### 4. Convex Corner Standoff Decoupling (`Nav_EnforceConvexCornerWaypoints`)
* Standard funnel algorithms pull string tight against obstacle corner vertices, which causes a capsule hull of radius $R$ to collide with walls.
* Obstacle corner vertices are precomputed and indexed into a spatial hash grid (`nav_spatial_grid_t`) during map load.
* For each corner, the angle bisector $\vec{b}$ is computed. The standoff distance is analytically evaluated:
  $$d_{\text{standoff}} = \min\left( R_{\text{clearance}} \times 2.0, \; \frac{R_{\text{clearance}}}{\cos(\theta / 2)} \right)$$
* If a path segment passes within $d_{\text{standoff}}$ on the obstacle side of a corner, an outward standoff waypoint $W_{\text{standoff}} = \vec{v}_{\text{corner}} + \vec{b} \cdot d_{\text{standoff}}$ is inserted.
* Verifies bidirectional line-of-sight ([`Nav_HasGeometricLineOfSight2D`](nav_path.h#L477)) from previous waypoint to standoff and from standoff to next waypoint before committing.

#### 5. Collinear Decimation with LOS Guard
* Simplifies redundant intermediate waypoints along straight paths where $\vec{u}_1 \cdot \vec{u}_2 > \text{NAV\_COLLINEAR\_MAX\_DOT}$ ($0.985$, $\approx 10^\circ$).
* **Line-of-Sight Guard**: Waypoints are **NEVER** pruned without first verifying [`Nav_HasGeometricLineOfSight2D(prev, next, clearance)`](nav_path.h#L477). If skipping the point would cut across a wall, corner, or doorjamb, the point is preserved.

---

## 6. Simulate: Locomotion, Kinematic Slide-Move & Active Steering

Navigation simulation executes inside the entity's server think loop ([`GenericThinkBegin`](../entities/monster/svg_monster_base.h#L164) / [`GenericThinkFinish`](../entities/monster/svg_monster_base.h#L171) / physics update).

### 1. Active Waypoint Progression ([`ComputePathSteering`](../entities/monster/svg_monster_base.h#L320))
* Tracks current waypoint index `stringPathPos`.
* **Proximity Deadband**: When within $4.0\,\text{units}$ of an intermediate waypoint, steering blends ahead to $W_{k+1}$ to eliminate $180^\circ$ yaw flutter.
* **Corner Switching Plane Gating**: At sharp turns ($> 30^\circ$), the agent is prevented from switching to the next waypoint while still on the approach side of the corner plane unless it already has clear physical line-of-sight to $W_{k+1}$. This stops agents from cutting corners prematurely.
* Outputs a normalized 2D movement vector and a dynamic `speedScale` for smooth corner deceleration into turns ($> 35^\circ$, speed scale floor $0.40$).

### 2. Kinematic Step Slide-Move ([`SVG_MMove_StepSlideMove`](../monsters/svg_mmove.h#L339))
> [!IMPORTANT]
> **MEMORIZE**: We do **NOT** use legacy `SV_WalkMove`. Locomotion strictly executes through [`SVG_MMove_StepSlideMove`](../monsters/svg_mmove.h#L339) (defined in [`src/baseq2rtxp/svgame/monsters/svg_mmove.cpp`](../monsters/svg_mmove.cpp#L313), signature `SVG_MMove_StepSlideMove( mm_move_t *monsterMove, const nav_path_policy_t &policy )`), which drives the underlying multi-plane [`SVG_MMove_SlideMove`](../monsters/svg_mmove_slidemove.h#L52) sweep in [`svg_mmove_slidemove.cpp`](../monsters/svg_mmove_slidemove.cpp).

* Integrates velocity horizontally across the frame time.
* Sweeps the capsule collision hull against world architecture and entity colliders using native shapes ([`SVG_MMove_GetNativeShape`](../monsters/svg_mmove.h#L259)).
* Handles multi-plane surface sliding, projecting remaining velocity along obstruction crease vectors via [`SVG_MMove_ClipVelocity`](../monsters/svg_mmove_slidemove.h#L29).
* Automatically performs predictive vertical step-ups ($18.25\,\text{units}$) when encountering curbs, stairs, or steep inclines, stepping down to ground cleanly at the destination via `MMove_StepDown`.

### 3. Dynamic Avoidance & Unstick Recovery
* **Crowd Integration**: Entities register with `svg_crowd_manager_t`. Flocking forces and formation slot offsets keep squad members from colliding.
* **Blocked Wall Recovery**: If an entity is blocked by world geometry for $\ge 32$ consecutive frames (`MONSTER_NAV_STUCK_RECOVER_BLOCKED_FRAMES`), [`UpdateBlockedNavigationRecovery`](../entities/monster/svg_monster_base.h#L348) extracts the contact wall normal, nudges the entity $2.0\,\text{units}$ outward into open space, and clears the path to force an immediate A\* recalculation.

---

## 7. How-To Guide: Monster Navigation from A to Z

This step-by-step tutorial explains the complete lifecycle of how a monster entity utilizes the navigation system, from initial spawn configuration to high-level target pursuit, reactive tactical callbacks, cover ambushes, and autonomous recovery.

### 7.1 Stage A: Entity Spawning, Bounding Boxes & Hull Collision Setup
In your monster's `Spawn()` method, initialize physical bounds and collision hulls. The navigation mesh requires capsule hulls with feet-aligned bounding boxes:

```cpp
virtual void Spawn() override {
	svg_monster_base_t::Spawn();

	// 1. Establish capsule collision and root motion kinematics:
	this->solid = SOLID_CAPSULE;
	this->movetype = MOVETYPE_ROOTMOTION;
	gi.setmodel( this, "models/monsters/mutant.iqm" );

	// 2. Set feet-origin standard standing bounding box:
	this->mins = PHYS_DEFAULT_BBOX_STANDUP_MINS; // {-16.0f, -16.0f, -36.0f}
	this->maxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS; // { 16.0f,  16.0f,  36.0f}
	this->viewheight = PHYS_DEFAULT_VIEWHEIGHT_STANDUP; // 30.0f
}
```

### 7.2 Stage B: Traversal Policy Configuration & Custom Edge Costing
Every monster contains an embedded [`PathNavigationState_t`](../entities/monster/svg_monster_base.h#L207) containing its [`nav_path_policy_t`](nav_path.h#L183). Configure these values to reflect your monster's physical capabilities:

```cpp
// 3. Configure movement thresholds:
this->pathNavigationState.policy.agent_radius    = 16.0;
this->pathNavigationState.policy.max_step_height = 18.25f;
this->pathNavigationState.policy.max_drop_height = 128.0f;
this->pathNavigationState.policy.waypoint_radius = 24.0f;

// 4. Configure tactical fallback bitflags:
this->crowdTacticalFlags = CROWD_TACTICAL_FLAG_DEFAULT;
```

To customize routing behavior (e.g. avoiding water, preferring ramps, or maintaining route commitment), override [`OnNavEvaluateEdgeCost`](../entities/monster/svg_monster_base.h#L404):

```cpp
virtual double OnNavEvaluateEdgeCost( const int32_t fromFaceIdx, const int32_t toFaceIdx, const nav_halfedge_t &he, const double baseCost ) override {
	// Example: Heavily penalize vertical stair climbing to prefer ground ramps:
	if ( he.z_diff > 8.0 ) {
		return baseCost * 2.5;
	}
	// Always chain to Super to preserve corridor commitment hysteresis (15% discount on committed path):
	return Super::OnNavEvaluateEdgeCost( fromFaceIdx, toFaceIdx, he, baseCost );
}
```

### 7.3 Stage C: Server Think Loop Lifecycle (`GenericThinkBegin` / `GenericThinkFinish`)
The server think function coordinates AI decisions, navigation updates, and physics execution. Always wrap your think function in `GenericThinkBegin` and `GenericThinkFinish`:

```cpp
void MyMonsterThink() {
	// Step 1: Validate entity state (health > 0, enemy validity, dead check):
	if ( !this->GenericThinkBegin() ) {
		return;
	}

	// Step 2: High-level navigation or combat AI update:
	if ( this->enemy && this->enemy->inUse ) {
		const Vector3 enemyFeet = this->enemy->currentOrigin + Vector3{ 0.0f, 0.0f, this->enemy->mins.z };
		this->MoveAStarToOrigin( enemyFeet, false );
	}

	// Step 3: Execute slide move physics, ground tests, and stuck recovery:
	int32_t blockedMask = 0;
	this->GenericThinkFinish( true, blockedMask );

	this->nextthink = level.time + FRAME_TIME_MS;
}
```

### 7.4 Stage D: Target Assignment, Pursuit & Path Recalculation (`MoveAStarToOrigin`)
[`MoveAStarToOrigin(goalOrigin, force)`](../entities/monster/svg_monster_base.h#L333) is the primary high-level navigation driver. It:
1. Verifies navmesh availability with [`GuardForNullNavMesh()`](../entities/monster/svg_monster_base.h#L293).
2. Debounces recalculations automatically using [`ShouldRecalcPath(goalOrigin)`](../entities/monster/svg_monster_base.h#L302), repathing only when the target moves $\ge 32\,\text{units}$ or after $400\,\text{ms}$.
3. Calls [`ComputePathTo(goalOrigin, force)`](../entities/monster/svg_monster_base.h#L312) to execute A\* and Funnel string-pulling.
4. Executes [`StepMoveToGoal(goalOrigin)`](../entities/monster/svg_monster_base.h#L326) to steer velocity towards the next waypoint.

### 7.5 Stage E: Waypoint Progression, Lookahead & Corner Steering
During [`StepMoveToGoal`](../entities/monster/svg_monster_base.h#L326), the monster calls [`ComputePathSteering`](../entities/monster/svg_monster_base.h#L320), which advances along the string-pulled waypoints:
* Detects when the agent is within `waypoint_radius` of intermediate nodes and triggers [`OnWaypointReached`](../entities/monster/svg_monster_base.h#L357).
* Prevents premature corner cutting via switching plane gating.
* Smooths yaw via [`SVG_MMove_FaceIdealYaw`](../monsters/svg_mmove.h#L344) at $45^\circ/\text{frame}$.
* Automatically selects animation tiers based on computed velocity (`actualFrameVelocity < 40`: Idle, `< 140`: Walk, `$\ge 140$`: Run).

### 7.6 Stage F: Milestone Callbacks & Tactical Event Interception (`OnWaypointReached`, `EvaluateWaypointArrival`)
Override [`OnWaypointReached`](../entities/monster/svg_monster_base.h#L357) to execute tactical checks every time the monster completes a path segment:

```cpp
virtual void OnWaypointReached( const size_t waypointIndex, const Vector3DP &waypointPos, const bool isFinalGoal ) override {
	// Call Super to maintain internal progress tracking:
	Super::OnWaypointReached( waypointIndex, waypointPos, isFinalGoal );

	// Reactive mechanic: Check if player is staring directly at monster ("Weeping Angel"):
	if ( this->HasTacticalBehaviorFlag( CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT ) && this->enemy != nullptr ) {
		if ( this->IsPlayerStaringAtMe( this->enemy ) ) {
			this->isHaltedByPlayerGaze = true;
			this->velocity = Vector3{ 0.0f, 0.0f, 0.0f };
			this->monsterMove.state.velocity = Vector3DP{};
			this->UpdateAnim( 1 ); // IDLE freeze
		}
	}
}
```

### 7.7 Stage G: Tactical Cover Seeking, Dynamic Occlusion & Ambush Postures (`OnCoverPointReached`)
When under fire or executing flanking behavior, query tactical cover points using [`Nav_FindCoverPoints`](nav_cover_query.h#L19):

```cpp
void SeekTacticalCoverFromEnemy() {
	if ( !this->enemy ) return;

	const Vector3DP myFeet = SVG_GetEntityFeetOriginDP( this );
	const Vector3DP threatFeet = SVG_GetEntityFeetOriginDP( this->enemy );

	std::vector<int32_t> coverIndices;
	// Search within 512 units for high or low cover protecting against the threat:
	const bool foundCover = Nav_FindCoverPoints(
		myFeet, threatFeet, 512.0, this->s.number, &coverIndices, NAV_COVER_NONE, Vector3DP{}, 6, false
	);

	if ( foundCover && !coverIndices.empty() ) {
		const int32_t chosenCoverIdx = coverIndices[ 0 ];
		const nav_cover_point_t *cover = Nav_GetCoverPoint( chosenCoverIdx );

		Vector3 worldPos{}, worldNormal{};
		if ( Nav_GetCoverPointWorld( *cover, &worldPos, &worldNormal ) ) {
			// Acquire a 4-second reservation lease:
			Nav_ClaimCoverPoint( chosenCoverIdx, this->s.number, 4000_ms );
			// Command monster to navigate to cover position:
			this->MoveAStarToOrigin( worldPos, true );
		}
	}
}
```

When the monster arrives at the designated cover spot, the engine automatically invokes [`OnCoverPointReached`](../entities/monster/svg_monster_base.h#L393):

```cpp
virtual void OnCoverPointReached( const int32_t coverIndex, const Vector3 &coverPos ) override {
	Super::OnCoverPointReached( coverIndex, coverPos );

	const nav_cover_point_t *cover = Nav_GetCoverPoint( coverIndex );
	if ( cover && cover->cover_type == NAV_COVER_LOW ) {
		// Crouch into ducked ambush posture:
		this->mins = PHYS_DEFAULT_BBOX_DUCKED_MINS;
		this->maxs = PHYS_DEFAULT_BBOX_DUCKED_MAXS;
		this->viewheight = PHYS_DEFAULT_VIEWHEIGHT_DUCKED;
		this->UpdateAnim( 5 ); // DUCK_IDLE
	}
}
```

### 7.8 Stage H: Chokepoint Queuing, Sector Traversal & Right-of-Way
When entering narrow doorways or chokepoints, monsters query sector awareness via [`QuerySectorAwareness`](../entities/monster/svg_monster_base.h#L371) and bottleneck precedence via [`QueryBottleneckRightOfWay`](../entities/monster/svg_monster_base.h#L378):

```cpp
nav_sector_info_t sectorInfo{};
if ( this->QuerySectorAwareness( goalOriginDP, &sectorInfo ) ) {
	if ( sectorInfo.next_portal_id >= 0 ) {
		bool hasRightOfWay = true;
		if ( this->QueryBottleneckRightOfWay( sectorInfo.next_portal_id, &hasRightOfWay ) && !hasRightOfWay ) {
			// Yield and wait outside the doorway for preceding teammates to clear:
			this->velocity.x = this->velocity.y = 0.0f;
			this->UpdateAnim( 1 ); // IDLE
			return;
		}
	}
}
```

### 7.9 Stage I: Contact Stalling, Blocked Frames & Autonomous Unstick Recovery
If an entity becomes wedged against geometry (e.g. encountering an unmodeled physics object or pushing against a closed doorjamb), [`UpdateBlockedNavigationRecovery`](../entities/monster/svg_monster_base.h#L348) automatically tracks contiguous blocked frames:
1. Ignores contacts with dynamic squad members to prevent queue thrashing.
2. When blocked by solid world geometry for $\ge 32$ frames (`MONSTER_NAV_STUCK_RECOVER_BLOCKED_FRAMES`), it reads the captured contact normal `recentWallBlockNormal`.
3. Tests a swept unstick trace outward along the wall normal by $2.0\,\text{units}$ (`MONSTER_NAV_STUCK_WALL_NUDGE_DIST`).
4. If open space exists, it nudges the entity origin outward, clears the active path via [`ResetNavigationPath()`](../entities/monster/svg_monster_base.h#L297), and triggers a fresh A\* path recalculation on the next frame.

---

## 8. Comprehensive Public API Reference

This reference documents the entire public API intended for game coders and modders across all navigation and movement subsystems.

### 8.1 Core Types, Bitmasks & Geometric Limits

#### Header: [`nav_core.h`](nav_core.h)
* [`NAV7_MAGIC`](nav_core.h#L9) (`'NAV7'`): Binary file signature for serialized NavMesh caches.
* [`NAV7_VERSION`](nav_core.h#L10) (`5`): Active NavMesh file format version.
* [`NAV_MAX_STEP_HEIGHT`](nav_core.h#L26) (`18.25f`): Maximum vertical step height Quake 2 AI can climb without jumping (`PHYS_STEP_MAX_SIZE + PHYS_STEP_GROUND_DIST`).
* [`NAV_MIN_WALKABLE_Z`](nav_core.h#L28) (`0.65f`): Minimum surface normal $Z$ required for a brush plane to qualify as walkable ground ($\approx 49.5^\circ$ slope).

#### Header: [`nav_types.h`](nav_types.h)
* [`nav_aabb_t`](nav_types.h#L31): Axis-aligned bounding box helper storing `Vector3DP mins, maxs`. Methods: `Clear()`, `AddPoint(p)`, `Expand(margin)`, `Overlaps(other, margin)`, `ContainsPoint(p, margin)`.
* [`nav_edge_flags_t`](nav_types.h#L145):
  * `NAV_EDGE_NONE` (`0`): Normal walkable edge.
  * `NAV_EDGE_DISABLED` (`1 << 0`): Temporarily impassable edge (e.g. closed or locked door).
  * `NAV_EDGE_WALL` (`1 << 1`): Boundary edge representing a solid vertical obstacle/wall.
  * `NAV_EDGE_DROPOFF` (`1 << 2`): Boundary edge representing an elevated cliff or drop-off ledge.
* [`nav_zone_type_t`](nav_types.h#L158):
  * `ZONE_TYPE_OPEN_SPACE` (`0`): Unbounded open exterior courtyard or terrain.
  * `ZONE_TYPE_ROOM_ENCLOSED` (`1`): Interior room bounded by solid walls and doorway apertures.
  * `ZONE_TYPE_CORRIDOR_FLAT` (`2`): Horizontal linear passage connecting zones.
  * `ZONE_TYPE_CORRIDOR_STAIRS` (`3`): Stepped staircase passage (risers $\ge 16\,\text{units}$).
  * `ZONE_TYPE_CORRIDOR_RAMP` (`4`): Continuous inclined ramp passage (gradient $\ge 5^\circ$).
  * `ZONE_TYPE_ALCOVE` (`5`): Single-portal dead-end niche or defensive nook.
  * `ZONE_TYPE_ELEVATED_PLATFORM` (`6`): Catwalk, roof, or ledge with perimeter drop-off edges.
* [`nav_portal_type_t`](nav_types.h#L178):
  * `PORTAL_TYPE_OPEN_APERTURE` (`0`): Open geometric archway/doorway without entity.
  * `PORTAL_TYPE_DOOR_ENTITY` (`1`): Dynamic sliding or rotating door (`func_door`, `func_door_rotating`).
  * `PORTAL_TYPE_FUNC_WALL` (`2`): Dynamic togglable or destructible wall brush (`func_wall`).
  * `PORTAL_TYPE_ELEVATOR_PLAT` (`3`): Vertical lift platform transition (`func_plat`).
* [`nav_portal_t`](nav_types.h#L192): Transition aperture record between zones (`portal_id`, `portal_type`, `from_room_id`, `to_room_id`, `halfedge_idx`, `entity_number`, `center`, `normal`, `width`, `is_passable`).
* [`nav_room_t`](nav_types.h#L218): Topological zone record (`room_id`, `zone_type`, `centroid`, `bounds`, `face_indices`, `portal_indices`, `avg_elevation`, `max_elevation_delta`, `wall_standoff_back`, `wall_standoff_left`, `wall_standoff_right`).
* [`nav_face_t`](nav_types.h#L244): Compiled NavMesh face record (`face_id`, `first_edge_idx`, `num_edges`, `center`, `normal`, `clearance`, `bsp_leaf_id`, `entity_id`, `transition_entity_id`, `brush_id`, `surface_flags`, `room_id`).
* [`nav_halfedge_t`](nav_types.h#L123): Half-edge descriptor (`vertex_idx`, `twin_idx`, `next_idx`, `face_idx`, `z_diff`, `wall_offset`, `edge_entity_id`, `flags`).

---

### 8.2 Agent Traversal Policy: `nav_path_policy_t`

Defined in [`nav_path.h`](nav_path.h#L183):

| Field | Type | Default | Semantic Description |
| :--- | :--- | :--- | :--- |
| `agent_radius` | `float` | `16.0f` | Horizontal collision cylinder radius in world units. |
| `agent_mins` | `Vector3` | `{-16,-16,-36}` | Physical bounding box mins for swept volumetric traces. |
| `agent_maxs` | `Vector3` | `{16,16,36}` | Physical bounding box maxs for swept volumetric traces. |
| `trace_shape` | `int32_t` | `1` | Collision trace shape (`0` = Auto, `1` = Capsule, `2` = Cylinder). |
| `min_step_normal` | `float` | `0.7f` | Minimum upward normal Z required to treat a landing as a step. |
| `min_step_height` | `float` | `0.0f` | Minimum vertical rise to count as a step. |
| `max_step_height` | `float` | `18.25f` | Maximum step-up height the agent can climb (`NAV_MAX_STEP_SIZE`). |
| `max_drop_height` | `float` | `128.0f` | Maximum safe descent drop height (`NAV_DROPOFF_ALLOWED_SIZE`). |
| `enable_max_drop_height_cap` | `bool` | `true` | Enables hard cutoff on lethal drop-offs. |
| `max_drop_height_cap` | `float` | `196.0f` | Lethal drop-off ceiling (`NAV_DROPOFF_MAX_SIZE`). |
| `min_portal_width` | `float` | `0.5f` | Minimum portal aperture width factor. |
| `waypoint_radius` | `float` | `32.0f` | Arrival acceptance radius for intermediate waypoints. |
| `rebuild_goal_dist_2d` | `float` | `32.0f` | Horizontal goal movement distance triggering path recalculation. |
| `rebuild_goal_dist_3d` | `float` | `32.0f` | 3D goal movement distance triggering path recalculation. |
| `rebuild_interval` | `int32_t` | `25` | Debounce interval between path recalculations in milliseconds. |
| `allow_small_obstruction_jump` | `bool` | `true` | Allows small hops over low obstacles. |
| `max_obstruction_jump_height` | `float` | `48.0f` | Maximum obstacle height jumpable. |
| `step_clearance` | `float` | `4.0f` | Extra overhead clearance required before attempting step. |
| `allow_gap_jumping` | `bool` | `true` | Allows agent to leap across disjoint mesh gaps. |
| `max_jump_distance` | `float` | `256.0f` | Maximum horizontal gap jump span. |
| `min_gap_width` | `float` | `24.0f` | Minimum gap width required to trigger leap behavior. |
| `ignore_disabled_edges` | `bool` | `false` | When true, allows A\* routing through closed/locked doors. |
| `edge_cost_callback` | `nav_path_edge_cost_fptr` | `nullptr` | Custom A\* edge traversal cost callback function pointer. |
| `edge_cost_monster` | `svg_monster_base_t*` | `nullptr` | Monster edict pointer passed to `edge_cost_callback`. |
| `allow_entity_deflection` | `bool` | `true` | Allows lateral sliding deflection when bumping living entities. |

---

### 8.3 Path Planning & Spatial Localization API

Defined in [`nav_path.h`](nav_path.h):

* [`int32_t Nav_FindLeafNode( const Vector3DP &point )`](nav_path.h#L244):
  Traverses the KD-tree to find the leaf node containing `point`. Returns leaf index or `-1` if outside world.
* [`int32_t Nav_FindPolyInLeaf( const Vector3DP &point )`](nav_path.h#L277):
  Finds the nav face containing `point`, checking the local leaf first and falling back to a global 3D search. Returns face index or `-1`.
* [`int32_t Nav_FindFaceInLeafStrict( const Vector3DP &point )`](nav_path.h#L289):
  Strict $O(\log N)$ KD-tree query that never falls back to a global search. Returns face index or `-1`.
* [`int32_t Nav_FindClosestPolyGlobal( const Vector3DP &point )`](nav_path.h#L347):
  Absolute 3D distance fallback finding the nearest face across the entire mesh. Returns face index or `-1`.
* [`int32_t Nav_FindClosestFaceInLeaf( const Vector3DP &point )`](nav_path.h#L357):
  Finds the closest face in the local BSP leaf with KD-tree global fallback. Returns face index or `-1`.
* [`int32_t Nav_GetFaceComponent( const int32_t faceIdx )`](nav_path.h#L367):
  Returns the connected component partition ID for `faceIdx` (used to verify graph reachability).
* [`int32_t Nav_FindReachableFaceInLeaf( const Vector3DP &point, const int32_t targetFace, const double agentRadius )`](nav_path.h#L379):
  Finds the closest face sharing the connected component of `targetFace` that overlaps the agent's capsule footprint, preventing stranding on isolated slivers.
* [`bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy )`](nav_path.h#L302):
  Executes topological A\* search from `startFace` to `goalFace`. Populates `outPath` with face IDs. Returns `true` if route found.
* [`bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3DP &startPos, const Vector3DP &goalPos, double agentRadius, std::vector<Vector3DP> &outWaypoints, std::vector<bool> *outForcedWaypoints, const Vector3 &agentMins, const Vector3 &agentMaxs, int32_t traceShape )`](nav_path.h#L493):
  Executes Simple, Stupid Funnel Algorithm in double precision across the A\* face corridor, applying corner standoff waypoints and doorway pinning. Populates `outWaypoints` and `outForcedWaypoints`.
* [`bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 )`](nav_path.h#L392):
  Retrieves the left (`outV0`) and right (`outV1`) endpoints of the portal segment connecting two adjacent faces.
* [`void Nav_SetEntityEdgesState( int32_t entity_id, uint32_t flags, bool enable )`](nav_path.h#L523):
  Enables or clears edge flags (e.g. `NAV_EDGE_DISABLED`) for all half-edges associated with `entity_id` (doors, plats).
* [`void Nav_ResyncDynamicEntityEdges()`](nav_path.h#L515):
  Re-applies runtime door/mover open/closed states to the mesh after mesh reloading.
* [`void Nav_LogLastPathDiagnostics()`](nav_path.h#L340):
  Prints the one-shot rejection diagnostic summary from the most recent A\* query.

---

### 8.4 Topological Geometric Queries & Half-Edge Raycasting

Defined in [`nav_path.h`](nav_path.h) and [`nav_generate.h`](nav_generate.h):

* [`bool Nav_RaycastHalfEdge2D( const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, nav_raycast_result_t *outResult )`](nav_path.h#L442):
  Traces a 2D ray across the half-edge face graph without BSP traces. Populates [`nav_raycast_result_t`](nav_path.h#L407) (`hitSolidWall`, `hitPortal`, `hitDropoff`, `hitDistance`, `hitPoint`, `hitNormal`, `hitFaceIndex`, `hitHalfedgeIndex`, `zone_type`, `portal_type`, `targetRoomId`, `portalEntityNumber`).
* [`bool Nav_CircleCastHalfEdge2D( const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, const double agentRadius, nav_raycast_result_t *outResult )`](nav_path.h#L453):
  Radius-swept 2D circle cast across half-edges for capsule hull clearance testing.
* [`void Nav_RaycastRoomBoundary2D( const Vector3DP &anchor, const double maxRadius, const int32_t numRays, double *outDoorYaw, bool *outHasDoor, double *outMinWallDist, double *outAvgWallDist, nav_zone_type_t *outZoneType, const double agentRadius )`](nav_path.h#L467):
  Omnidirectional radial boundary scanner resolving primary doorway orientation, perimeter clearance, and zone type in a single sweep.
* [`bool Nav_HasGeometricLineOfSight2D( const Vector3DP &p0, const Vector3DP &p1, const double clearanceMargin )`](nav_path.h#L477):
  Validates clear 2D line-of-sight between two points with obstacle clearance margin.
* [`const nav_room_t *Nav_GetRoomForPoint( const Vector3DP &pos )`](nav_generate.h#L77):
  Returns the enclosing `nav_room_t` record containing bounds, elevation metrics, and interior wall standoffs.
* [`nav_zone_type_t Nav_GetZoneTypeForPoint( const Vector3DP &pos )`](nav_generate.h#L84):
  Returns the semantic zone classification for a world position in $O(1)$ time.
* [`bool Nav_IsPointInEnclosedRoom( const Vector3DP &pos )`](nav_generate.h#L98):
  Returns true if `pos` lies within an enclosed room structure.
* [`double Nav_GetStairCorridorStepHeight( const int32_t room_id )`](nav_generate.h#L105):
  Returns the average stair riser step height for a staircase corridor zone.

---

### 8.5 Topological Sector Graph Subsystem API

Defined in [`nav_sector_graph.h`](nav_sector_graph.h):

* [`int32_t Nav_SectorGraph_GetSectorForPoint( const Vector3DP &pos )`](nav_sector_graph.h#L70):
  Resolves the topological sector index enclosing `pos`.
* [`const nav_portal_t *Nav_SectorGraph_GetNextPortal( const int32_t fromSector, const int32_t toSector )`](nav_sector_graph.h#L78):
  Queries the immediate next transition portal to traverse from `fromSector` towards `toSector`.
* [`double Nav_SectorGraph_GetSectorDistance( const int32_t fromSector, const int32_t toSector )`](nav_sector_graph.h#L86):
  Queries precalculated all-pairs topological distance connecting two sectors in world units.
* [`bool Nav_SectorGraph_BuildRoute( const int32_t fromSector, const int32_t toSector, std::vector<int32_t> &outPortalIds )`](nav_sector_graph.h#L95):
  Constructs the complete sequential list of portal indices between two sectors.
* [`bool Nav_SectorGraph_QueryAwareness( const Vector3DP &currentPos, const Vector3DP &goalPos, nav_sector_info_t *outSectorInfo )`](nav_sector_graph.h#L104):
  Populates [`nav_sector_info_t`](nav_sector_graph.h#L37) with the agent's current sector, zone type, floor elevation, upcoming portal, and topological distance to goal.
* [`void Nav_SectorGraph_SetPortalPassable( const int32_t portalId, const bool isPassable )`](nav_sector_graph.h#L137):
  Updates runtime passability of a portal (e.g. when a door opens or closes).

---

### 8.6 Tactical Cover Point Subsystem API

Defined in [`nav_cover_types.h`](nav_cover_types.h) and [`nav_cover_query.h`](nav_cover_query.h):

* [`const bool Nav_FindCoverPoints( const Vector3DP &search_origin, const Vector3DP &threat_origin, const double radius, const int32_t requester_ent, std::vector<int32_t> *out_cover_indices, const nav_cover_type_t min_cover_type, const Vector3DP &threat_forward, const size_t max_results, const bool require_engagement_los )`](nav_cover_query.h#L19):
  Ranked threat-relative search. Filters by posture requirement (`NAV_COVER_LOW`, `NAV_COVER_HIGH`, `NAV_COVER_NONE`), claim reservations, and optional peek/engagement sightlines.
* [`const bool Nav_QueryCoverPointsRadius( const Vector3DP &search_origin, const double radius, std::vector<int32_t> *out_cover_indices, const size_t max_results )`](nav_cover_query.h#L36):
  Geometric radius query without threat scoring, collecting nearby stand cells for staging.
* [`const float Nav_EvaluateCoverForThreat( const int32_t cover_idx, const Vector3DP &threat_origin, const bool perform_trace_check, const bool require_engagement_los )`](nav_cover_query.h#L65):
  Scores a specific cover point against a threat from `0.0f` (exposed) to `1.0f` (ideal directional occlusion).
* [`const bool Nav_ClaimCoverPoint( const int32_t cover_idx, const int32_t entity_id, const QMTime duration )`](nav_cover_query.h#L86):
  Acquires a timed reservation lease on a cover point (default duration `3000_ms`).
* [`void Nav_ReleaseCoverPoint( const int32_t cover_idx, const int32_t entity_id )`](nav_cover_query.h#L93):
  Releases an active cover reservation.
* [`void Nav_SetCoverPointCooldown( const int32_t cover_idx, const QMTime duration )`](nav_cover_query.h#L100):
  Places a cover point on cooldown after compromise or abandonment.
* [`const bool Nav_IsCoverPointClaimed( const int32_t cover_idx, const int32_t requester_ent )`](nav_cover_query.h#L108):
  Checks if a cover point is currently leased by another entity.
* [`const bool Nav_IsCoverPointSpatiallyClaimed( const int32_t cover_idx, const int32_t requester_ent, const float exclusion_radius )`](nav_cover_query.h#L117):
  Checks if any cover point within `exclusion_radius` is claimed by another entity.
* [`const bool Nav_GetCoverPointWorld( const nav_cover_point_t &cover, Vector3 *out_pos, Vector3 *out_normal, Vector3 *out_tangent )`](nav_cover_types.h#L105):
  Transforms mover-local cover coordinates to world space based on parent entity's live origin and angles.
* [`const bool Nav_IsCoverPointUsable( const nav_cover_point_t &cover, const svg_base_edict_t *requester )`](nav_cover_types.h#L261):
  Verifies that dynamic gating doors are closed and parent movers are stationary (or requester is riding them).

---

### 8.7 Kinematic Locomotion & SlideMove Subsystem

Defined in [`svg_mmove.h`](../monsters/svg_mmove.h) and [`svg_mmove_slidemove.h`](../monsters/svg_mmove_slidemove.h):

* [`const mm_slide_move_flags_t SVG_MMove_StepSlideMove( mm_move_t *monsterMove, const nav_path_policy_t &policy )`](../monsters/svg_mmove.h#L339):
  The authoritative physics engine for all monster locomotion. Executes multi-plane sliding via [`SVG_MMove_SlideMove`](../monsters/svg_mmove_slidemove.h#L52), tests predictive upward step-ups up to `policy.max_step_height`, slides along the elevated plane, steps down to the landing tread, and accepts the superior movement result. Returns bitmask flags:
  * `MM_SLIDEMOVEFLAG_MOVED` (`BIT(0)`): Unobstructed movement achieved.
  * `MM_SLIDEMOVEFLAG_CLIPPED` (`BIT(2)`): Velocity clipped by floor or wall, but movement continued along plane.
  * `MM_SLIDEMOVEFLAG_BLOCKED` (`BIT(3)`): Movement was blocked by an obstacle.
  * `MM_SLIDEMOVEFLAG_TRAPPED` (`BIT(4)`): Entity is completely trapped in geometry.
  * `MM_SLIDEMOVEFLAG_WALL_BLOCKED` (`BIT(5)`): Blocked by a vertical wall surface.
  * `MM_SLIDEMOVEFLAG_PLANE_TOUCHED` (`BIT(6)`): Touched at least one collision plane.
* [`const mm_trace_shape_t SVG_MMove_GetNativeShape( const svg_base_edict_t *passEntity )`](../monsters/svg_mmove.h#L259):
  Resolves the native analytical movement shape (`MM_SHAPE_CAPSULE` or `MM_SHAPE_CYLINDER`) from the entity's solid type.
* [`const svg_trace_t SVG_MMove_Trace( const Vector3DP &start, const Vector3 &mins, const Vector3 &maxs, const Vector3DP &end, svg_base_edict_t *passEntity, cm_contents_t contentMask, mm_trace_shape_t shape )`](../monsters/svg_mmove.h#L264):
  Sweeps an analytical collision shape in double precision.
* [`const bool SVG_MMove_StepProbe( const Vector3DP &start, const Vector3 &mins, const Vector3 &maxs, const Vector3DP &end, svg_base_edict_t *passEntity, Vector3DP *outEndpos, const double maxStepHeight, const double maxDropHeight )`](../monsters/svg_mmove.h#L287):
  Performs a step-aware and slope-aware swept trace probe, stepping up obstacles and down onto stair treads without altering entity state.
* [`void SVG_MMove_FaceIdealYaw( svg_base_edict_t *ent, const float idealYaw, const float yawSpeed )`](../monsters/svg_mmove.h#L344):
  Interpolates entity yaw towards `idealYaw` at `yawSpeed` degrees per frame.
* [`const int32_t SVG_MMove_ClipVelocity( const Vector3DP &in, const Vector3DP &normal, Vector3DP &out, const double overbounce )`](../monsters/svg_mmove_slidemove.h#L29):
  Clips velocity vector against a surface normal in double precision.

---

### 8.8 Monster Base Navigation Interface

Defined in [`svg_monster_base.h`](../entities/monster/svg_monster_base.h):

* [`const bool MoveAStarToOrigin( const Vector3 &goalOrigin, bool force = false )`](../entities/monster/svg_monster_base.h#L333):
  High-level pursuit driver. Recalculates paths when target moves, drives steering towards waypoints, and executes physics.
* [`const bool StepMoveToGoal( const Vector3 &goalOrigin )`](../entities/monster/svg_monster_base.h#L326):
  Steers entity towards `goalOrigin` using active path steering, sets yaw, and selects animations.
* [`const bool ComputePathSteering( const Vector3DP &finalGoal, Vector3DP *outMoveDir, double *outSpeedScale )`](../entities/monster/svg_monster_base.h#L320):
  Progresses along waypoints, enforces corner switching planes, and outputs normalized 2D movement vector and corner deceleration scale.
* [`PathComputeResult ComputePathTo( const Vector3 &target, const bool force = false )`](../entities/monster/svg_monster_base.h#L312):
  Executes A\* and Funnel string pulling, caching results in `navPath` and `stringPulledPath`.
* [`const Vector3DP NextWaypoint( const Vector3DP &finalGoal )`](../entities/monster/svg_monster_base.h#L338):
  Retrieves active waypoint coordinates in double precision.
* [`const int32_t FindCurrentPoly()`](../entities/monster/svg_monster_base.h#L306):
  Locates the nav face currently underneath the monster's feet.
* [`const bool ShouldRecalcPath( const Vector3 &pos )`](../entities/monster/svg_monster_base.h#L302):
  Evaluates whether the destination has moved sufficiently ($\ge 32\,\text{units}$) or timed out ($400\,\text{ms}$) to warrant a repath.
* [`void ResetNavigationPath()`](../entities/monster/svg_monster_base.h#L297):
  Clears active path buffers and resets waypoint indices.
* [`void UpdateBlockedNavigationRecovery( const int32_t blockedMask )`](../entities/monster/svg_monster_base.h#L348):
  Monitors blocked frame stalls against static geometry, applies wall nudges, and triggers repathing after 32 frames.
* [`virtual void OnWaypointReached( const size_t waypointIndex, const Vector3DP &waypointPos, const bool isFinalGoal )`](../entities/monster/svg_monster_base.h#L357):
  Overridable virtual callback fired upon reaching intermediate and final path waypoints.
* [`virtual void OnCoverPointReached( const int32_t coverIndex, const Vector3 &coverPos )`](../entities/monster/svg_monster_base.h#L393):
  Overridable virtual callback fired upon arriving at assigned tactical cover.
* [`virtual double OnNavEvaluateEdgeCost( const int32_t fromFaceIdx, const int32_t toFaceIdx, const nav_halfedge_t &he, const double baseCost )`](../entities/monster/svg_monster_base.h#L404):
  Overridable virtual callback customizing A\* transition costs (chains to Super for corridor commitment hysteresis).
* [`static svg_monster_base_t *FromEdict( svg_base_edict_t *ent )`](../entities/monster/svg_monster_base.h#L415):
  Type-safe cast helper using the engine's TypeInfo system.

---

### 8.9 Server Debug Primitive Drawing API

Defined in [`nav_debug_draw.h`](nav_debug_draw.h):

* [`const bool SVG_Nav_DebugDraw_IsEnabled()`](nav_debug_draw.h#L24):
  Returns true when `nav_debug_draw` is enabled.
* [`void SVG_Nav_DebugDraw_AddLine( const Vector3 &start, const Vector3 &end, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx )`](nav_debug_draw.h#L34):
  Queues a 3D line segment for frame rendering.
* [`void SVG_Nav_DebugDraw_AddAabb( const Vector3 &mins, const Vector3 &maxs, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx )`](nav_debug_draw.h#L42):
  Queues an axis-aligned bounding box.
* [`void SVG_Nav_DebugDraw_AddSphere( const Vector3 &center, const float radius, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx )`](nav_debug_draw.h#L50):
  Queues a 3D sphere primitive.
* [`void SVG_Nav_DebugDraw_AddArrow( const Vector3 &start, const Vector3 &end, const float headLength, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx )`](nav_debug_draw.h#L58):
  Queues a directional 3D arrow with head cone.
* [`void SVG_Nav_DebugDraw_AddCapsule( const Vector3 &start, const Vector3 &end, const float radius, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx )`](nav_debug_draw.h#L66):
  Queues a 3D capsule primitive.
* [`void SVG_Nav_DebugDraw_AddCylinder( const Vector3 &start, const Vector3 &end, const float radius, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx )`](nav_debug_draw.h#L74):
  Queues a 3D cylinder primitive.

---

## 9. Tactical Behavior Bitflags & Crowd Formations

When environments restrict geometric formations (such as small rooms, tight corridors, doorways, or chokepoints), the crowd and monster navigation system dynamically activates fallback tactics governed by [`crowd_tactical_flags_t`](../crowd/svg_crowd_types.h#L326):

```cpp
enum crowd_tactical_flags_t : uint32_t {
	//! No fallback tactics enabled; strictly enforce nominal formation geometry.
	CROWD_TACTICAL_FLAG_NONE = 0,
	//! Collapse formation to single-file / staggered column along the A* guide path when entering narrow passages.
	CROWD_TACTICAL_FLAG_CORRIDOR_COLUMN = BIT( 0 ),
	//! Perform O(1) bounded topological cellular room packing when enclosed in tight/irregular rooms.
	CROWD_TACTICAL_FLAG_CELLULAR_ROOM_PACK = BIT( 1 ),
	//! Stash excess squad members who cannot fit in the primary room into nearby occluded tactical cover nodes.
	CROWD_TACTICAL_FLAG_COVER_OVERFLOW = BIT( 2 ),
	//! Establish a rear-guard staging queue outside chokepoints / doorways when destination room is at full capacity.
	CROWD_TACTICAL_FLAG_REAR_GUARD_BUFFER = BIT( 3 ),
	//! Halt and freeze in place upon reaching a waypoint when actively stared at by the player ("Weeping Angel").
	CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT = BIT( 4 ),
	//! Orient outward in a 360-degree perimeter shield when holding station around a central VIP/leader.
	CROWD_TACTICAL_FLAG_RADIAL_DEFENSE_RING = BIT( 5 ),
	//! Enable progressive rolling headway in corridors and doorways to prevent stop-and-go queue stalling.
	CROWD_TACTICAL_FLAG_ROLLING_QUEUE_HEADWAY = BIT( 6 ),
	//! Disallow stationary slot parking in doorways or chokepoints to ensure portals remain clear for traffic.
	CROWD_TACTICAL_FLAG_PREVENT_CHOKEPOINT_PARKING = BIT( 7 ),

	//! Default tactical preset: enables all adaptive terrain fallbacks, headway regulation, and chokepoint protection.
	CROWD_TACTICAL_FLAG_DEFAULT = CROWD_TACTICAL_FLAG_CORRIDOR_COLUMN |
	                              CROWD_TACTICAL_FLAG_CELLULAR_ROOM_PACK |
	                              CROWD_TACTICAL_FLAG_COVER_OVERFLOW |
	                              CROWD_TACTICAL_FLAG_REAR_GUARD_BUFFER |
	                              CROWD_TACTICAL_FLAG_RADIAL_DEFENSE_RING |
	                              CROWD_TACTICAL_FLAG_ROLLING_QUEUE_HEADWAY |
	                              CROWD_TACTICAL_FLAG_PREVENT_CHOKEPOINT_PARKING,

	//! Comprehensive preset enabling all behaviors including the reactive stare-halt mechanic.
	CROWD_TACTICAL_FLAG_ALL = CROWD_TACTICAL_FLAG_DEFAULT | CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT
};
```

---

## 10. Elaborative Interrogation (Deep-Dive Design Rationale)

To understand why the navigation engine is designed this way, consider these critical architectural questions:

### Q1: Why use double precision (`Vector3DP`) for Funnel String Pulling?
* **Why**: Large Quake 2 levels span coordinates beyond $\pm 4096\,\text{units}$. Single-precision floats have a $24$-bit mantissa (approx. 7 decimal digits of precision).
* **The Failure**: When computing 2D cross products for the funnel algorithm (`Nav_TriArea2D`), subtracting large coordinates causes **catastrophic floating-point cancellation**. Sub-unit differences round to zero, causing the left and right funnel rays to invert, drop spurious waypoints, or fail to detect tight corners.
* **The Solution**: Full double-precision computation ([`Vector3DP`](../../../shared/math/qm_vector3_dp.h), 53-bit mantissa) ensures sub-millimeter geometric accuracy across any map scale.

### Q2: Why use a Half-Edge Mesh instead of simple triangle adjacency?
* **Why**: Triangle lists only describe connectivity, not directionality.
* **The Advantage**: In a Half-Edge mesh, every edge is directional and oriented counter-clockwise around its face. This provides:
  1. $O(1)$ twin lookup (`twin_idx`) to find the adjacent face.
  2. Directed height differentials ($z_{\text{diff}}$): jumping down a $64$-unit drop is legal, but walking up a $64$-unit drop is impossible. Storing $z_{\text{diff}}$ directionally on the half-edge represents one-way cliffs naturally.
  3. Stable portal endpoints: Looking forward along the traversal direction, `seg1` is guaranteed to be Left and `seg0` is guaranteed to be Right, eliminating portal winding flips.

### Q3: Why both a 3D KD-Tree AND a BSP Leaf Mapping?
* **Why**: Speed and robustness serve different query types.
* **The Advantage**: When an entity is grounded on floor geometry inside a known BSP leaf, the BSP leaf link provides an instant $O(1)$ list of $1\text{--}4$ candidate faces.
* **The Fallback**: When an entity is mid-air, jumping, dropping off a cliff, or targeting an arbitrary point in the void, the BSP leaf mapping may return empty. The 3D KD-Tree provides an absolute geometric fallback, resolving the nearest walkable polygon in $O(\log N)$ time.

### Q4: Why must Constricted Doorways force a waypoint (`portal.force_waypoint = true`)?
* **Why**: Funnel string pulling attempts to find the shortest geometric line between start and goal.
* **The Failure**: If a doorway portal collapses to a single point (because wall insets on both sides constrain passage) but is not marked forced, downstream portals inside the room widen back out. Because standard SSFA assumes monotonic tightening, points inside the room never cross over the outside apex ray.
* **The Consequence**: The funnel skips the doorway completely, plotting a straight line from outside the building through the solid exterior wall straight to the interior target!
* **The Solution**: Constricted portals force `portal.force_waypoint = true`, resetting the funnel apex at the exact doorway center.

### Q5: Why is Line-of-Sight verification required during Collinear Decimation?
* **Why**: Collinear simplification removes waypoints if the direction vector barely changes ($\vec{u}_1 \cdot \vec{u}_2 > 0.985$, $\approx 10^\circ$).
* **The Failure**: If an agent approaches a doorway or building corner at a shallow angle, the line to the door and the line into the room can differ by $< 10^\circ$. Unchecked collinear decimation prunes the doorway waypoint, converting `(grass -> doorway -> room)` into `(grass -> room)`, cutting directly through the building's corner wall.
* **The Solution**: [`Nav_HasGeometricLineOfSight2D`](nav_path.h#L477) verifies obstacle clearance before removing any point.

### Q6: Why precompute topological spatial regions and portals at bake/load time rather than dynamically during path queries?
* **Why**: Evaluating room bounds, wall standoffs, and doorway keep-out sectors dynamically during each server frame creates substantial computational overhead and non-deterministic behavior when multiple agents query simultaneously.
* **The Advantage**: Decomposing regions at bake time and assigning `room_id` to each polygon face allows AI agents, crowd managers, and tactical formation algorithms to access complete room geometry, doorway portals, and zone classifications in $O(1)$ time with zero per-frame runtime allocations.

### Q7: Why store cover points in mover-local coordinates instead of world space?
* **Why**: Cover derived from doors, elevators, and rotating brushes moves with its parent entity; a world-space snapshot becomes invalid the instant the mover changes position.
* **The Advantage**: [`nav_cover_point_t`](nav_cover_types.h#L64) stores position, normal, and tangent relative to the parent entity's origin and orientation basis. [`Nav_GetCoverPointWorld`](nav_cover_types.h#L105) re-projects coordinates on demand, so cover attached to a moving platform or a closing door remains geometrically correct without regenerating the mesh.
* **The Safety Net**: Dynamic flags (`NAV_COVER_FLAG_REQUIRES_DOOR_CLOSED`, `NAV_COVER_FLAG_REJECT_WHILE_MOVING`) let tactical queries reject points whose occlusion guarantees are temporarily violated, and transient claim leases ([`Nav_ClaimCoverPoint`](nav_cover_query.h#L86)) prevent two agents from racing to the same cover node.

---

## 11. Integration Guide 1: Custom Entity (`svg_base_edict_t`)

Use this approach when creating custom non-monster entities (e.g. companion droids, automated vehicles, security cameras, floating drones) that need navigation without monster behavior overhead.

```cpp
#pragma once

#include "svgame/svg_local.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/nav/nav_path.h"
#include "svgame/monsters/svg_mmove.h"

/**
*	@brief	A lightweight autonomous companion drone utilizing low-level NavMesh APIs.
**/
class svg_companion_drone_t : public svg_base_edict_t {
public:
	DefineClass( svg_companion_drone_t, svg_base_edict_t );

	svg_companion_drone_t() = default;
	virtual ~svg_companion_drone_t() = default;

	//! Traversal policy defining drone capabilities.
	nav_path_policy_t navPolicy = {};
	//! Sequence of high-precision string-pulled waypoints.
	std::vector<Vector3DP> waypoints = {};
	//! Flags identifying mandatory corner/portal waypoints.
	std::vector<bool> forcedWaypoints = {};
	//! Active index in waypoints vector.
	size_t activeWaypointIndex = 0;
	//! Kinematic movement state for slide physics (mm_move_t wraps mmove_state_t origin/velocity in double precision).
	mm_move_t moveState = {};

	/**
	*	@brief	Initialize drone physics, bounding box, and navigation policy.
	**/
	virtual void Spawn() override {
		this->solid = SOLID_SPHERE;
		this->movetype = MOVETYPE_FLY;
		gi.setmodel( this, "models/examples/drone_sphere.iqm" );
		this->mins = Vector3{ -12.0f, -12.0f, -12.0f };
		this->maxs = Vector3{  12.0f,  12.0f,  12.0f };

		// Configure movement limits.
		navPolicy.agent_radius = 12.0;
		navPolicy.max_step_height = 18.25f;
		navPolicy.max_drop_height = 96.0f;
		navPolicy.waypoint_radius = 20.0f;

		// Initialize slide state (pass-entity, frame time, bounds, and starting origin).
		moveState.monster = this;
		moveState.frameTime = FRAME_TIME_S;
		moveState.mins = this->mins;
		moveState.maxs = this->maxs;
		moveState.state.origin = Vector3DP( this->currentOrigin );
		moveState.navPolicy = &navPolicy;

		this->think = &svg_companion_drone_t::DroneThink;
		this->nextthink = level.time + 100_ms;
	}

	/**
	*	@brief	Command the drone to calculate a path to a world-space destination.
	*	@param	goalOriginFeet Target point in feet-origin space.
	*	@return	True if a valid path was generated.
	**/
	bool SetMoveGoal( const Vector3 &goalOriginFeet ) {
		// 1. Calculate feet position.
		const Vector3 myFeet = this->currentOrigin + Vector3{ 0.0f, 0.0f, this->mins.z };

		// 2. Localize start and goal faces.
		const int32_t startFace = Nav_FindPolyInLeaf( myFeet );
		const int32_t goalFace = Nav_FindPolyInLeaf( goalOriginFeet );
		if ( startFace < 0 || goalFace < 0 ) {
			return false;
		}

		// 3. Perform topological A* search.
		std::vector<int32_t> facePath;
		if ( !Nav_FindPath( startFace, goalFace, facePath, navPolicy ) ) {
			return false;
		}

		// 4. Run Funnel String-Pulling with corner standoffs.
		waypoints.clear();
		forcedWaypoints.clear();
		const bool stringPullOk = Nav_StringPull(
			facePath,
			Vector3DP( myFeet ),
			Vector3DP( goalOriginFeet ),
			navPolicy.agent_radius,
			waypoints,
			&forcedWaypoints,
			this->mins,
			this->maxs,
			TRACE_SHAPE_CAPSULE
		);

		if ( !stringPullOk || waypoints.empty() ) {
			return false;
		}

		activeWaypointIndex = 0;
		return true;
	}

	/**
	*	@brief	Per-frame execution loop: steers towards active waypoint and runs StepSlideMove.
	**/
	void DroneThink() {
		this->nextthink = level.time + FRAME_TIME_MS;

		// If no active path, idle.
		if ( waypoints.empty() || activeWaypointIndex >= waypoints.size() ) {
			return;
		}

		// 1. Current feet origin.
		const Vector3 myFeet = this->currentOrigin + Vector3{ 0.0f, 0.0f, this->mins.z };
		const Vector3DP currentWaypoint = waypoints[ activeWaypointIndex ];

		// 2. Compute 2D horizontal vector to waypoint.
		Vector3DP delta = currentWaypoint - Vector3DP( myFeet );
		delta.z = 0.0;
		const double dist2D = QM_Vector3LengthDP( delta );

		// 3. Check arrival threshold.
		if ( dist2D <= static_cast<double>( navPolicy.waypoint_radius ) ) {
			activeWaypointIndex++;
			if ( activeWaypointIndex >= waypoints.size() ) {
				// Reached final goal: stop.
				waypoints.clear();
				this->velocity = Vector3{ 0.0f, 0.0f, 0.0f };
				return;
			}
		}

		// 4. Compute normalized steering direction.
		const Vector3DP steerDir = ( dist2D > 0.001 ) ? ( delta * ( 1.0 / dist2D ) ) : Vector3DP{};

		// 5. Update yaw to face target.
		this->ideal_yaw = QM_Vector3ToYawDP( steerDir );
		SVG_MMove_FaceIdealYaw( this, this->ideal_yaw, 360.0f );

		// 6. Execute step slide move kinematics.
		constexpr float DRONE_SPEED = 220.0f;
		this->velocity.x = static_cast<float>( steerDir.x ) * DRONE_SPEED;
		this->velocity.y = static_cast<float>( steerDir.y ) * DRONE_SPEED;

		moveState.state.origin = Vector3DP( this->currentOrigin );
		moveState.state.velocity = Vector3DP( this->velocity );

		SVG_MMove_StepSlideMove( &moveState, navPolicy );

		// 7. Commit new physical origin.
		SVG_Util_SetEntityOrigin( this, static_cast<Vector3>( moveState.state.origin ), true );
		this->velocity = static_cast<Vector3>( moveState.state.velocity );
	}
};
```

---

## 12. Integration Guide 2: Production Monster (`svg_monster_tactical_example_t`)

This complete production example demonstrates:
1. **Enumerated Bitflag Configuration**: Querying and modifying behavioral capabilities via [`HasTacticalBehaviorFlag`](../entities/monster/svg_monster_base.h#L253) and [`SetTacticalBehaviorFlag`](../entities/monster/svg_monster_base.h#L261).
2. **Dynamic Waypoint Action Interception ([`OnWaypointReached`](../entities/monster/svg_monster_base.h#L357))**: Intercepts intermediate and final waypoint arrivals to evaluate environmental or tactical triggers.
3. **Player Gaze Detection (`IsPlayerStaringAtMe`)**: Evaluates player field-of-view dot product and raycast line-of-sight to freeze movement when observed ("Weeping Angel").
4. **Cover Milestone Interception ([`OnCoverPointReached`](../entities/monster/svg_monster_base.h#L393))**: Automatically transitions animations and collision boxes upon reaching cover nodes.

```cpp
#pragma once

#include "svgame/svg_local.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include "svgame/crowd/svg_crowd_types.h"

/**
*	@brief	Example production monster demonstrating reactive waypoint callbacks and tactical bitflags.
**/
class svg_monster_tactical_example_t : public svg_monster_base_t {
public:
	DefineClass( svg_monster_tactical_example_t, svg_monster_base_t );

	svg_monster_tactical_example_t() = default;
	virtual ~svg_monster_tactical_example_t() = default;

	/**
	*	Behavioral Configuration & State:
	**/
	//! Whether the monster is currently frozen/halted because the player is actively staring at it.
	bool isHaltedByPlayerGaze = false;
	//! Server timestamp when the stare-halt was initiated.
	QMTime haltStartTime = 0_ms;
	//! Total count of waypoints reached along the active pursuit corridor.
	size_t waypointsTraversedCount = 0;

	/**
	*	@brief	Activate: Spawn monster and configure physical bounds, navigation rules, and tactical bitflags.
	**/
	virtual void Spawn() override {
		svg_monster_base_t::Spawn();

		this->solid = SOLID_CAPSULE;
		this->movetype = MOVETYPE_ROOTMOTION;
		gi.setmodel( this, "models/examples/example_monster.iqm" );
		this->mins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
		this->maxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
		this->viewheight = PHYS_DEFAULT_VIEWHEIGHT_STANDUP;

		// Configure policy parameters preallocated in pathNavigationState.
		this->pathNavigationState.policy.agent_radius    = 16.0;
		this->pathNavigationState.policy.max_step_height = 18.25f;
		this->pathNavigationState.policy.max_drop_height = 128.0f;
		this->pathNavigationState.policy.waypoint_radius = 24.0f;

		// Enable tactical bitflags: single-file corridor collapse, O(1) room packing, rolling headway, and stare-halt:
		this->crowdTacticalFlags = CROWD_TACTICAL_FLAG_DEFAULT | CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT;

		this->think = &svg_monster_tactical_example_t::TacticalThink;
		this->nextthink = level.time + 100_ms;
	}

	/**
	*	@brief	Entity KeyValue parsing for level designers to configure tactical flags via Radiant.
	**/
	virtual const bool KeyValue( const cm_entity_key_value_pair_t *keyValuePair, char *errorStr ) override {
		const std::string keyStr = keyValuePair->key;
		if ( ( keyStr == "tactical_flags" || keyStr == "crowd_tactical_flags" ) && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) ) {
			this->crowdTacticalFlags = static_cast<uint32_t>( keyValuePair->integer );
			return true;
		} else if ( keyStr == "stare_halt" && ( keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) ) {
			this->SetTacticalBehaviorFlag( CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT, ( keyValuePair->integer != 0 ) );
			return true;
		}
		return Super::KeyValue( keyValuePair, errorStr );
	}

	/**
	*	@brief	Optional: Override edge cost callback to bias routing decisions.
	**/
	virtual double OnNavEvaluateEdgeCost( const int32_t fromFaceIdx, const int32_t toFaceIdx, const nav_halfedge_t &he, const double baseCost ) override {
		// Penalize steep upward transitions to prefer ramps over staircases.
		if ( he.z_diff > 8.0 ) {
			return baseCost * 2.0;
		}
		return Super::OnNavEvaluateEdgeCost( fromFaceIdx, toFaceIdx, he, baseCost );
	}

	/**
	*	@brief	Determines whether the player is directly looking/staring at this monster.
	**/
	const bool IsPlayerStaringAtMe( const svg_base_edict_t *player, const float fovDotThreshold = 0.7071f ) const {
		if ( !player || !player->client || !player->inUse || player->health <= 0 ) {
			return false;
		}

		// 1. Calculate player's eye origin in world coordinates.
		const Vector3 playerEye = player->currentOrigin + Vector3{ 0.0f, 0.0f, static_cast<float>( player->viewheight ) };

		// 2. Calculate monster target focal point (chest/head height).
		const Vector3 monsterCenter = this->currentOrigin + Vector3{ 0.0f, 0.0f, static_cast<float>( this->viewheight * 0.8f ) };

		// 3. Compute normalized direction vector from player's eyes to monster center.
		const Vector3 toMonster = monsterCenter - playerEye;
		const float dist = QM_Vector3Length( toMonster );
		if ( dist <= 0.001f ) {
			return true;
		}
		const Vector3 dirToMonster = toMonster * ( 1.0f / dist );

		// 4. Extract player forward view vector from viewMove/client state.
		Vector3 playerForward{};
		QM_AngleVectors( player->client->viewMove.viewAngles, &playerForward, nullptr, nullptr );

		// 5. Evaluate view-cone dot product (cosine of gaze angle).
		const float viewDot = QM_Vector3DotProduct( playerForward, dirToMonster );
		if ( viewDot < fovDotThreshold ) {
			return false;
		}

		// 6. Perform physical raycast trace to verify clear line-of-sight.
		const svg_trace_t tr = SVG_Trace( playerEye, vec3_origin, vec3_origin, monsterCenter, player, CM_CONTENTMASK_SOLID | CM_CONTENTMASK_OPAQUE );
		if ( tr.fraction >= 0.99f || tr.entityNumber == this->s.number ) {
			return true;
		}

		return false;
	}

	/**
	*	@brief	Invoked automatically when an intermediate path waypoint or final destination is reached.
	**/
	virtual void OnWaypointReached( const size_t waypointIndex, const Vector3DP &waypointPos, const bool isFinalGoal ) override {
		this->waypointsTraversedCount++;

		// Evaluate stare-halt trigger when CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT is active:
		if ( this->HasTacticalBehaviorFlag( CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT ) && this->enemy != nullptr ) {
			if ( this->IsPlayerStaringAtMe( this->enemy ) ) {
				this->isHaltedByPlayerGaze = true;
				this->haltStartTime = level.time;
				this->velocity.x = this->velocity.y = 0.0f;
				this->monsterMove.state.velocity.x = this->monsterMove.state.velocity.y = 0.0;
				this->UpdateAnim( 1 ); // IDLE
				gi.dprintf( "%s: Halted at waypoint #%zu (pos=%.1f, %.1f, %.1f) due to player gaze!\n",
					__func__, waypointIndex, waypointPos.x, waypointPos.y, waypointPos.z );
			}
		}

		if ( isFinalGoal ) {
			gi.dprintf( "%s: Final destination reached after %zu waypoints!\n", __func__, this->waypointsTraversedCount );
		}
	}

	/**
	*	@brief	Invoked automatically when a designated tactical cover point is reached.
	**/
	virtual void OnCoverPointReached( const int32_t coverIndex, const Vector3 &coverPos ) override {
		( void )coverIndex;
		( void )coverPos;
		this->UpdateAnim( 5 ); // DUCK_IDLE
	}

	/**
	*	@brief	Simulate: Server think update driving path calculation, steering, and slide move.
	**/
	void TacticalThink() {
		// 1. Run generic base think setup (validates enemy, health, dead states).
		if ( !this->GenericThinkBegin() ) {
			return;
		}

		// 2. Stare-Halt Behavior: If currently halted by player gaze, check if player is still looking:
		if ( this->HasTacticalBehaviorFlag( CROWD_TACTICAL_FLAG_STARE_HALT_WAYPOINT ) && this->enemy != nullptr ) {
			const bool isStaring = this->IsPlayerStaringAtMe( this->enemy );
			if ( isStaring ) {
				this->isHaltedByPlayerGaze = true;
				this->velocity.x = 0.0f;
				this->velocity.y = 0.0f;
				this->monsterMove.state.velocity.x = 0.0;
				this->monsterMove.state.velocity.y = 0.0;
				this->UpdateAnim( 1 ); // IDLE

				int32_t blockedMask = 0;
				this->GenericThinkFinish( true, blockedMask );
				this->nextthink = level.time + FRAME_TIME_MS;
				return;
			} else {
				this->isHaltedByPlayerGaze = false;
			}
		}

		// 3. If we have an active enemy, chase them using high-level MoveAStarToOrigin.
		if ( this->enemy && this->enemy->inUse ) {
			const Vector3 enemyFeet = this->enemy->currentOrigin + Vector3{ 0.0f, 0.0f, this->enemy->mins.z };
			this->MoveAStarToOrigin( enemyFeet, false );
		}

		// 4. Finalize slide move kinematics, obstacle grounding, and stuck recovery.
		int32_t blockedMask = 0;
		this->GenericThinkFinish( true, blockedMask );

		this->nextthink = level.time + FRAME_TIME_MS;
	}
};
```

---

## 13. Level Design, Map Architecture & Brush Guidelines

To guarantee 100% pathfinding reliability, level designers should adhere to the following metrics:

| Feature | Minimum Metric | Recommended Metric | Technical Rationale |
| :--- | :---: | :---: | :--- |
| **Doorway Width** | `24.0 units` | `48.0–64.0 units` | Portals $< 20.0\,\text{units}$ are rejected by A\* as impassable slivers. |
| **Corridor Width** | `36.0 units` | `64.0+ units` | Two $16.0$-radius agents require $64.0\,\text{units}$ to pass without blocking. |
| **Stair Riser Height** | `4.0 units` | `8.0–16.0 units` | Maximum traversable vertical step-up is strictly `18.25 units`. |
| **Stair Tread Depth** | `12.0 units` | `16.0–24.0 units` | Treads $< 2.0\,\text{units}$ get dissolved by the CSG sliver filter. |
| **Ramp Incline Angle**| `0°` | `≤ 35°` | Surfaces $> 49.5^\circ$ ($Z < 0.65$) are rejected as steep walls. |
| **Drop-Off Ledges**  | `0 units` | `≤ 128.0 units` | Drops exceeding `max_drop_height` are marked non-traversable cliffs. |

### Rotating Doors (`func_door_rotating`)
* Ensure the door's origin brush sits flush with the hinge line.
* When open, the door entity disables its blocking boundary edges dynamically, connecting the adjacent navmesh faces across the threshold seam.

### Monsterclip Brushes (`CONTENTS_MONSTERCLIP`)
* Wrap complex high-poly decorative trim, jagged rubble, or thin pipe geometry in simple, convex `monsterclip` brushes.
* The NavMesh generator treats `monsterclip` as solid world volume, creating clean, smooth floor boundaries.

---

## 14. CVars, Console Commands & Visual Diagnostics

### Visual Diagnostic CVars

| Cvar | Default | Permitted Values | Visual Representation & Semantic Description |
| :--- | :---: | :---: | :--- |
| `nav_debug_draw` | `1` | `0`, `1` | **Master Toggle**: Enables or disables all navigation debug primitive rendering. |
| `nav_debug_polys` | `1` | `0`, `1` | **NavMesh Edges**: Renders colored wireframe boundaries of all walkable polygons.<br>• **Red**: Solid boundary edge without twin (wall or lethal drop-off).<br>• **Orange**: Shared coplanar twin edge between walkable faces.<br>• **Green**: Enabled dynamic entity edge (e.g. open door portal).<br>• **Purple**: Disabled dynamic entity edge (e.g. closed door). |
| `nav_debug_tris` | `0` | `0`, `1` | **Internal Triangulation**: Renders tan wireframes (`TRIS_COLOR`) showing the internal triangle fan decomposition of convex n-gons. |
| `nav_debug_nodes` | `0` | `0`, `1` | **KD-Tree Bounding Boxes**: Renders cyan wireframe AABBs (`KDTREE_COLOR`) of all leaf nodes in the 3D KD-Tree. |
| `nav_debug_query_leaves`| `0` | `0`, `1` | **Query Leaves**: Highlights the exact BSP and KD-Tree leaf volumes containing the active test endpoints. |
| `nav_debug_npc_paths` | `1` | `0`, `1` | **Live NPC Paths**: Visualizes active monster pathfinding and steering:<br>• **Slate Grey**: Raw A\* face centroid chain with black outline.<br>• **Bright Yellow Sphere & Line**: Currently active target waypoint and live steering vector.<br>• **Cyan Spheres & Lines**: Traversed funnel path segments.<br>• **Green Spheres**: Mandatory forced stair or doorway waypoints.<br>• **Semi-Transparent Grey**: Future unvisited waypoints.<br>• **Red Arrow**: Wall-contact normal vector when monster experiences collision blocking. |
| `nav_debug_cover` | `0` | `0`, `1` | **Tactical Cover Points**: Visualizes tactical cover nodes:<br>• **Green**: Full standing cover.<br>• **Yellow**: Crouching cover.<br>• **Arrows**: Directional peek vectors. |
| `nav_debug_breadcrumb` | `1` | `0`, `1` | **Player Breadcrumbs**: Renders circular breadcrumb history trail used for player-following behavior. |
| `s_crowd_debug_draw` | `0` | `0`, `1` | **Crowd Formations**: Renders squad formation slots, circle/line layouts, and agent slot assignment lines. |

### Console Server Commands

All commands are executed in the server console (prefix with `sv ` or run locally):

```bash
# 1. NavMesh Generation & Cache Management
sv nav_generate       # Triggers asynchronous NavMesh compilation in background worker thread.
sv nav_status         # Prints compilation progress, memory usage, and face/half-edge counts.
sv nav_save <filepath># Forces immediate serialization of current mesh (e.g. sv nav_save maps/mymap.nav7).
sv nav_load <filepath># Loads a previously serialized .nav7 mesh into memory (validates magic/version).
sv nav_clear          # Clears in-memory NavMesh data structures.

# 2. Interactive Route Testing & Diagnostics
sv nav_dbg_goal_a     # Drops Start Pin (Goal A, Green Sphere) at the player's feet-origin.
sv nav_dbg_goal_b     # Drops End Pin (Goal B, Red Sphere) at the player's feet-origin.
sv nav_dbg_test       # Solves A* and Funnel between Goal A and B, logs bounded diagnostics, and renders the route.
```

### Step-by-Step Interactive Route Debugging Workflow
1. Stand at the desired starting position and type `sv nav_dbg_goal_a`.
2. Move to the destination position and type `sv nav_dbg_goal_b`.
3. Type `sv nav_dbg_test`. The engine will:
   * Print the start/goal leaf indices, face IDs, and policy parameters.
   * Log every face in the A\* corridor and every transition edge ($z_{\text{diff}}$, flags, portal widths).
   * Print funnel waypoint coordinates and forced stair/doorway flags.
   * Render the complete path visually: the purple line shows raw A\* face centers; the cyan/green line shows the string-pulled polyline with corner standoffs.
