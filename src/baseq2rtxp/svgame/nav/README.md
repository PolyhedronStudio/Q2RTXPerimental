# Navigation System (NavMesh, Pathfinding & Locomotion)

A high-performance, deterministic 3D navigation and kinematic locomotion pipeline for ServerGame entities. Built around a double-precision half-edge navigation mesh, KD-Tree accelerated spatial queries, corridor-constrained Funnel string pulling with convex corner standoffs, and custom step-slide capsule physics.

---

## Table of Contents
1. [Architectural Overview](#1-architectural-overview)
2. [The BAAS Framework](#2-the-baas-framework)
3. [Bake: NavMesh Generation & Compilation](#3-bake-navmesh-generation--compilation)
4. [Activate: Agent Configuration & Spawning](#4-activate-agent-configuration--spawning)
5. [Assign: Target Assignment & Path Planning](#5-assign-target-assignment--path-planning)
6. [Simulate: Locomotion, Kinematics & Avoidance](#6-simulate-locomotion-kinematics--avoidance)
7. [Elaborative Interrogation (Deep-Dive Design Rationale)](#7-elaborative-interrogation-deep-dive-design-rationale)
8. [Integration Guide 1: Custom Entity (`svg_base_edict_t`)](#8-integration-guide-1-custom-entity-svg_base_edict_t)
9. [Integration Guide 2: Standard Monster (`svg_monster_base_t`)](#9-integration-guide-2-standard-monster-svg_monster_base_t)
10. [Level Design & Brush Authoring Guidelines](#10-level-design--brush-authoring-guidelines)
11. [CVars, Console Commands & Visual Diagnostics](#11-cvars-console-commands--visual-diagnostics)

---

## 1. Architectural Overview

The navigation system provides full-stack pathfinding and steering for AI agents:
* **Geometry Engine**: Extracts walkable brush windings directly from the BSP collision model (`cm_t`), performs CSG boolean clipping, dissolves sliver artifacts, and constructs a topological Half-Edge mesh.
* **Spatial Acceleration**: Combines a balanced 3D KD-Tree with a direct BSP Leaf Mapping table for instant $O(1)$ point-to-polygon localization.
* **Path Search**: Evaluates multi-criteria A\* over half-edge twins, handling stairs, step-ups, drops, dynamic door states, and custom cost callbacks.
* **Corridor Funneling**: Converts A\* face corridors into smooth, minimum-distance polylines via the Simple, Stupid Funnel Algorithm (SSFA) in double precision (`Vector3DP`).
* **Convex Corner Decoupling**: Analytically projects obstacle corner bisectors to insert standoff waypoints, guaranteeing physical capsule clearance around sharp geometry.
* **Kinematic Simulation**: Steers capsules using `SVG_MMove_StepSlideMove` (multi-plane sliding with predictive step-ups), completely bypassing legacy `SV_WalkMove`.

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

## 3. Bake: NavMesh Generation & Compilation

The NavMesh compiler extracts walkable surfaces directly from the BSP collision model asynchronously on a worker thread (`Nav_StartAsyncGeneration`), ensuring the server never stalls.

### Step-by-Step Compilation Pipeline

#### 1. Brush Filtering & Winding Construction (`Nav_DoExtractionWork`)
* Scans all BSP brushes in the map for `CONTENTS_SOLID`, `CONTENTS_DETAIL`, and `CONTENTS_MONSTERCLIP`.
* Examines every brush plane. Only surfaces with normal $Z \ge \text{NAV\_MIN\_WALKABLE\_Z}$ ($0.65$, max slope $\approx 49.5^\circ$) are considered walkable ground.
* Generates a base winding on the plane and chops it against all other bounding planes of the brush, creating a convex walkable polygon.

#### 2. CSG Boolean Subtraction & Sliver Dissolution (`nav_csg.cpp`)
* Clips walkable polygons against intersecting solid brushes (e.g. pillars, walls, curbs) to carve out obstacles.
* **Sliver Dissolution**: High-poly BSP cuts often create degenerate, razor-thin sliver triangles. Polygons are evaluated for feature width:
  $$w = \frac{2 \times \text{Area}}{\text{Perimeter}}$$
  Any fragment with $w < 2.0\,\text{units}$, bounding extent $< 2.0\,\text{units}$, or $\text{Area} < 4.0\,\text{units}^2$ is dissolved and merged into neighboring polygons to prevent routing through sub-hull gaps.

#### 3. Half-Edge Topology Construction (`nav_generate.cpp`)
* Welds co-located vertices within spatial tolerance into a unified vertex array (`g_nav_vertices`).
* Builds half-edge pairs (`nav_halfedge_t` and `nav_face_t`). Each half-edge stores:
  * `vertex_idx`: Originating vertex.
  * `next_idx`: Next half-edge in counter-clockwise order around the face.
  * `twin_idx`: Index of the opposing half-edge belonging to the neighboring polygon ($-1$ if solid boundary).
  * `z_diff`: Vertical difference across the seam. If $|z_{\text{diff}}| \le \text{NAV\_MAX\_STEP\_HEIGHT}$ ($18.25\,\text{units}$), it is tagged as a traversable step; if higher, it represents a one-way drop-off or impassable cliff.
  * `flags`: Dynamic attributes (e.g. `NAV_EDGE_DISABLED` for closed doors).

#### 4. Spatial Indexing (`nav_kdtree_builder.cpp`)
* **KD-Tree**: Constructs a balanced 3D KD-tree (`nav_kdtree_node_t`) recursively splitting face centroids across X, Y, and Z axes. Provides $O(\log N)$ lookup time for arbitrary 3D queries.
* **BSP Leaf Mapping**: Maps engine BSP leaf numbers to NavMesh face subsets (`nav_leaf_link_t`). When an entity is inside a valid BSP leaf, candidate faces are retrieved in $O(1)$ time.

#### 5. Serialization & Persistence (`nav_persistence.cpp`)
* Serializes the compiled data into `baseq2rtxp/maps/<mapname>.nav7`.
* Includes `NAV7_MAGIC`, `NAV7_VERSION`, and the map's BSP 32-bit checksum.
* On map load, the engine validates the header. If the BSP checksum matches, the `.nav7` is loaded in milliseconds; if outdated or missing, background generation begins automatically.

---

## 4. Activate: Agent Configuration & Spawning

Every navigating entity must establish its physical collision footprint and traversal constraints.

### Locomotion Policy (`nav_path_policy_t`)

```cpp
nav_path_policy_t policy{};
policy.agent_radius               = 16.0;   // Bounding cylinder radius (units)
policy.max_step_height            = 18.25f; // Max vertical rise agent can climb (units)
policy.max_drop_height            = 128.0f; // Max safe descent drop (units)
policy.waypoint_radius            = 24.0f;  // Arrival threshold for intermediate points (units)
policy.allow_gap_jumping          = false;  // Whether agent can leap across disjoint meshes
policy.enable_max_drop_height_cap = true;   // Strict cutoff on lethal drops
```

### Physical Collision Setup
Navigation agents use `SOLID_CAPSULE` hulls with `MOVETYPE_STEP`:
* Standard Bounding Box: mins `{-16.0f, -16.0f, -24.0f}`, maxs `{16.0f, 16.0f, 40.0f}`.
* Trace Shape: `TRACE_SHAPE_CAPSULE` to match sweep tests with continuous physics.

---

## 5. Assign: Target Assignment & Path Planning

Goal assignment transforms a 3D target coordinate into a smoothed, corner-decoupled polyline.

### Execution Pipeline

#### 1. Localization (`Nav_FindReachableFaceInLeaf`)
* Converts feet-origin coordinates ($z_{\text{feet}} = z_{\text{origin}} + \text{mins.z}$) to a NavMesh face index.
* Checks the current BSP leaf first ($O(1)$); falls back to the 3D KD-Tree ($O(\log N)$).
* Validates that the start face and goal face reside on the same connected topological component.

#### 2. Topological A\* Search (`Nav_FindPath`)
* Traverses the half-edge graph using Euclidean distance heuristic $h(n)$.
* Neighbor cost evaluation accounts for slope, surface material, and step delta.
* **Narrow Portal Rejection**: Portals narrower than `NAV_ABSOLUTE_MIN_PORTAL_PASSAGE_WIDTH` ($20.0\,\text{units}$) are rejected during graph expansion, preventing paths through impassable gaps.
* Outputs an ordered sequence of face indices: `std::vector<int32_t> outFacePath`.

#### 3. Funnel Algorithm & Portal Clipping (`Nav_StringPull`)
* For every adjacent face pair $(F_i, F_{i+1})$, extracts the shared portal segment via `Nav_GetPortalEndpoints`.
* Clips portal ends inward by `agent_radius + NAV_PORTAL_CLEARANCE_MARGIN` ($16 + 8 = 24\,\text{units}$) using `Nav_ClipPortalForAgentClearance`.
* **Constricted Portals (Doorways)**: When a passage is narrower than two agent radii ($minT \ge maxT$), it collapses to its exact geometric midpoint and sets `portal.force_waypoint = true`. This forces the funnel algorithm to reset its apex at the doorway center.
* Runs the Simple, Stupid Funnel Algorithm (SSFA) in double precision (`Vector3DP`) to produce the raw path polyline.

#### 4. Convex Corner Standoff Decoupling (`Nav_EnforceConvexCornerWaypoints`)
* Standard funnel algorithms pull string tight against obstacle corner vertices, which causes a capsule hull of radius $R$ to collide with walls.
* Obstacle corner vertices are precomputed and indexed into a spatial hash grid (`nav_spatial_grid_t`) during map load.
* For each corner, the angle bisector $\vec{b}$ is computed. The standoff distance is analytically evaluated:
  $$d_{\text{standoff}} = \min\left( R_{\text{clearance}} \times 2.0, \; \frac{R_{\text{clearance}}}{\cos(\theta / 2)} \right)$$
* If a path segment passes within $d_{\text{standoff}}$ on the obstacle side of a corner, an outward standoff waypoint $W_{\text{standoff}} = \vec{v}_{\text{corner}} + \vec{b} \cdot d_{\text{standoff}}$ is inserted.
* Verifies bidirectional line-of-sight (`Nav_HasGeometricLineOfSight2D`) from previous waypoint to standoff and from standoff to next waypoint before committing.

#### 5. Collinear Decimation with LOS Guard
* Simplifies redundant intermediate waypoints along straight paths where $\vec{u}_1 \cdot \vec{u}_2 > \text{NAV\_COLLINEAR\_MAX\_DOT}$ ($0.985$, $\approx 10^\circ$).
* **Line-of-Sight Guard**: Waypoints are **NEVER** pruned without first verifying `Nav_HasGeometricLineOfSight2D(prev, next, clearance)`. If skipping the point would cut across a wall, corner, or doorjamb, the point is preserved.

---

## 6. Simulate: Locomotion, Kinematics & Avoidance

Navigation simulation executes inside the entity's server think loop (`ThinkFinish` / physics update).

### 1. Active Waypoint Progression (`ComputePathSteering`)
* Tracks current waypoint index `stringPathPos`.
* **Proximity Deadband**: When within $4.0\,\text{units}$ of an intermediate waypoint, steering blends ahead to $W_{k+1}$ to eliminate $180^\circ$ yaw flutter.
* **Corner Switching Plane Gating**: At sharp turns ($> 30^\circ$), the agent is prevented from switching to the next waypoint while still on the approach side of the corner plane unless it already has clear physical line-of-sight to $W_{k+1}$. This stops agents from cutting corners prematurely.
* Outputs a normalized 2D movement vector and a dynamic `speedScale` for smooth corner deceleration.

### 2. Kinematic Step Slide-Move (`SVG_MMove_StepSlideMove`)
> [!IMPORTANT]
> **MEMORIZE**: We do **NOT** use legacy `SV_WalkMove`. Locomotion strictly executes through `SVG_MMove_StepSlideMove` in `src/baseq2rtxp/svgame/monsters/svg_mmove_slidemove.cpp`.

* Integrates velocity horizontally across the frame time.
* Sweeps the capsule collision hull against world architecture and entity colliders.
* Handles multi-plane surface sliding, projecting remaining velocity along obstruction crease vectors.
* Automatically performs predictive vertical step-ups ($18.25\,\text{units}$) when encountering curbs, stairs, or steep inclines, stepping down to ground cleanly at the destination.

### 3. Dynamic Avoidance & Unstick Recovery
* **Crowd Integration**: Entities register with `svg_crowd_manager_t`. Flocking forces and formation slot offsets keep squad members from colliding.
* **Blocked Wall Recovery**: If an entity is blocked by world geometry for $\ge 32$ consecutive frames (`MONSTER_NAV_STUCK_RECOVER_BLOCKED_FRAMES`), `UpdateBlockedNavigationRecovery` extracts the contact wall normal, nudges the entity $2.0\,\text{units}$ outward into open space, and clears the path to force an immediate A\* recalculation.

---

## 7. Elaborative Interrogation (Deep-Dive Design Rationale)

To understand why the navigation engine is designed this way, consider these critical architectural questions:

### Q1: Why use double precision (`Vector3DP`) for Funnel String Pulling?
* **Why**: Large Quake 2 levels span coordinates beyond $\pm 4096\,\text{units}$. Single-precision floats have a $24$-bit mantissa (approx. 7 decimal digits of precision).
* **The Failure**: When computing 2D cross products for the funnel algorithm (`Nav_TriArea2D`), subtracting large coordinates causes **catastrophic floating-point cancellation**. Sub-unit differences round to zero, causing the left and right funnel rays to invert, drop spurious waypoints, or fail to detect tight corners.
* **The Solution**: Full double-precision computation (`Vector3DP`, 53-bit mantissa) ensures sub-millimeter geometric accuracy across any map scale.

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
* **The Solution**: `Nav_HasGeometricLineOfSight2D` verifies obstacle clearance before removing any point.

---

## 8. Integration Guide 1: Custom Entity (`svg_base_edict_t`)

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
	//! Kinematic movement state for slide physics.
	svg_monster_move_state_t moveState = {};

	/**
	*	@brief	Initialize drone physics, bounding box, and navigation policy.
	**/
	virtual void Spawn() override {
		this->solid = SOLID_CAPSULE;
		this->movetype = MOVETYPE_STEP;
		this->mins = Vector3{ -12.0f, -12.0f, -16.0f };
		this->maxs = Vector3{  12.0f,  12.0f,  16.0f };

		// Configure movement limits.
		navPolicy.agent_radius = 12.0;
		navPolicy.max_step_height = 18.25f;
		navPolicy.max_drop_height = 96.0f;
		navPolicy.waypoint_radius = 20.0f;

		// Initialize slide state.
		moveState.origin = this->currentOrigin;
		moveState.mins = this->mins;
		moveState.maxs = this->maxs;

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

		moveState.origin = this->currentOrigin;
		moveState.velocity = this->velocity;

		SVG_MMove_StepSlideMove( &moveState, this->velocity, this );

		// 7. Commit new physical origin.
		SVG_Util_SetEntityOrigin( this, moveState.origin, true );
		this->velocity = moveState.velocity;
	}
};
```

---

## 9. Integration Guide 2: Standard Monster (`svg_monster_base_t`)

Monsters inherit built-in path caching, lookahead smoothing, crowd avoidance, and unstick recovery.

```cpp
#pragma once

#include "svgame/svg_local.h"
#include "svgame/entities/monster/svg_monster_base.h"

/**
*	@brief	Production AI soldier monster leveraging built-in svg_monster_base_t navigation.
**/
class svg_monster_soldier_t : public svg_monster_base_t {
public:
	DefineClass( svg_monster_soldier_t, svg_monster_base_t );

	svg_monster_soldier_t() = default;
	virtual ~svg_monster_soldier_t() = default;

	/**
	*	@brief	Activate: Spawn monster and configure physical bounds and navigation rules.
	**/
	virtual void Spawn() override {
		svg_monster_base_t::Spawn();

		this->solid = SOLID_CAPSULE;
		this->movetype = MOVETYPE_STEP;
		this->mins = Vector3{ -16.0f, -16.0f, -24.0f };
		this->maxs = Vector3{  16.0f,  16.0f,  40.0f };

		// Configure policy parameters preallocated in pathNavigationState.
		this->pathNavigationState.policy.agent_radius    = 16.0;
		this->pathNavigationState.policy.max_step_height = 18.25f;
		this->pathNavigationState.policy.max_drop_height = 128.0f;
		this->pathNavigationState.policy.waypoint_radius = 24.0f;

		this->think = &svg_monster_soldier_t::SoldierThink;
		this->nextthink = level.time + 100_ms;
	}

	/**
	*	@brief	Optional: Override edge cost callback to avoid hazardous sectors (slime, lava).
	**/
	virtual double OnNavEvaluateEdgeCost( int32_t fromFace, int32_t toFace, const nav_halfedge_t &he, double baseCost ) override {
		// Quadruple traversal cost across liquid faces.
		if ( ( g_nav_faces[ toFace ].flags & NAV_FACE_LIQUID ) != 0 ) {
			return baseCost * 4.0;
		}
		return baseCost;
	}

	/**
	*	@brief	Simulate: Server think update driving path calculation, steering, and slide move.
	**/
	void SoldierThink() {
		// 1. Run generic base think setup (validates enemy, health, dead states).
		if ( !this->GenericThinkBegin() ) {
			return;
		}

		// 2. If we have an active enemy, chase them using high-level MoveAStarToOrigin.
		if ( this->enemy && this->enemy->inUse ) {
			const Vector3 enemyFeet = this->enemy->currentOrigin + Vector3{ 0.0f, 0.0f, this->enemy->mins.z };

			// MoveAStarToOrigin debounces recalculations automatically (ShouldRecalcPath)
			// and drives physical movement towards the next waypoint.
			const bool isMoving = this->MoveAStarToOrigin( enemyFeet, false );
			if ( !isMoving ) {
				// Reached attack range or within goal radius.
			}
		}

		// 3. Finalize slide move kinematics, obstacle grounding, and stuck recovery.
		int32_t blockedMask = 0;
		this->GenericThinkFinish( true, blockedMask );

		this->nextthink = level.time + FRAME_TIME_MS;
	}
};
```

---

## 10. Level Design & Brush Authoring Guidelines

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

## 11. CVars, Console Commands & Visual Diagnostics

### Visual Diagnostic CVars

| Cvar | Default | Permitted Values | Visual Representation & Semantic Description |
| :--- | :---: | :---: | :--- |
| `nav_debug_draw` | `1` | `0`, `1` | **Master Toggle**: Enables or disables all navigation debug primitive rendering. |
| `nav_debug_polys` | `1` | `0`, `1` | **NavMesh Edges**: Renders colored wireframe boundaries of all walkable polygons.<br>• **Red**: Solid boundary edge without twin (wall or lethal drop-off).<br>• **Orange**: Shared coplanar twin edge between walkable faces.<br>• **Green**: Enabled dynamic entity edge (e.g. open door portal).<br>• **Purple**: Disabled dynamic entity edge (e.g. closed door). |
| `nav_debug_tris` | `0` | `0`, `1` | **Internal Triangulation**: Renders tan wireframes (`TRIS_COLOR`) showing the internal triangle fan decomposition of convex n-gons. |
| `nav_debug_nodes` | `0` | `0`, `1` | **KD-Tree Bounding Boxes**: Renders cyan wireframe AABBs (`KDTREE_COLOR`) of all leaf nodes in the 3D KD-Tree. |
| `nav_debug_query_leaves`| `0` | `0`, `1` | **Query Leaves**: Highlights the exact BSP and KD-Tree leaf volumes containing the active test endpoints. |
| `nav_debug_draw_radius`| `8192` | `0` to `32768` | **Culling Radius**: Maximum distance from player camera (in units) to render navigation debug primitives. |
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
sv nav_save <name>    # Forces immediate serialization of current mesh to baseq2rtxp/maps/<name>.nav7.
sv nav_load <name>    # Loads baseq2rtxp/maps/<name>.nav7 into memory.
sv nav_clear          # Clears in-memory NavMesh data structures.

# 2. Interactive Route Testing & Diagnostics
sv nav_dbg_goal_a     # Drops Start Pin (Goal A, Green Sphere) at crosshair hit-point or player feet.
sv nav_dbg_goal_b     # Drops End Pin (Goal B, Red Sphere) at crosshair hit-point or player feet.
sv nav_dbg_test       # Solves A* and Funnel between Goal A and B, logs diagnostics, and renders the route.
```

### Step-by-Step Interactive Route Debugging Workflow
1. Stand at or aim at the desired starting position and type `sv nav_dbg_goal_a`.
2. Move to or aim at the destination position and type `sv nav_dbg_goal_b`.
3. Type `sv nav_dbg_test`. The engine will:
   * Print the start/goal leaf indices, face IDs, and policy parameters.
   * Log every face in the A\* corridor and every transition edge ($z_{\text{diff}}$, flags, portal widths).
   * Print funnel waypoint coordinates and forced stair/doorway flags.
   * Render the complete path visually: the purple line shows raw A\* face centers; the cyan/green line shows the string-pulled polyline with corner standoffs.
