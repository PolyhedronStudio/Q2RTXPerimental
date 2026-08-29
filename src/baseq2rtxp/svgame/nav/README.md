# Navigation System (NavMesh, Pathfinding & Movement Agents)

A high-performance, deterministic navigation and kinematic locomotion pipeline for AI agents. Built around a double-precision half-edge navigation mesh, KD-Tree accelerated spatial queries, corridor-constrained Funnel string pulling with convex corner standoffs, and custom step-slide capsule physics.

---

## The BAAS Framework

```
[ BAKE ]       BSP Brushes -> CSG Extraction -> Half-Edge Mesh + KD-Tree -> .nav7 Cache
   │
[ ACTIVATE ]   svg_monster_base_t -> nav_path_policy_t -> Capsule Collision Hull
   │
[ ASSIGN ]     Nav_FindReachableFaceInLeaf -> A* Graph Search -> Nav_StringPull
   │
[ SIMULATE ]   Waypoint Steering -> SVG_MMove_StepSlideMove -> Crowd & Stuck Recovery
```

---

## 1. Bake: Navigation Mesh Generation

The NavMesh compiler extracts walkable planar surfaces directly from the BSP collision model asynchronously without halting the game loop.

### Workflow
* **Console Command**: Execute `nav_generate` in the developer console. Generation runs on a background worker thread (`Nav_StartAsyncGeneration`).
* **Status Query**: Check progress via `nav_status`.
* **Persistence**: Once compilation finishes, the mesh serializes to `maps/<mapname>.nav7`. On map load, the engine checks `NAV7_VERSION` and the BSP checksum to load cached data instantly or trigger an automatic rebuild.

### Pipeline Stages
1. **Brush Filtering & Surface Extraction**: Iterates all BSP brushes containing `CONTENTS_SOLID`, `CONTENTS_DETAIL`, or `CONTENTS_MONSTERCLIP`. Surface planes with normal $Z \ge \text{NAV\_MIN\_WALKABLE\_Z}$ ($0.65$, max slope $\approx 49.5^\circ$) are carved into convex polygon windings.
2. **CSG Boolean Subtraction & Sliver Dissolution**: Walkable windings are clipped against intersecting solid geometry (pillars, curbs). Polygons with feature width $2 \times \text{area} / \text{perimeter} < 2.0\,\text{units}$ or bounding extent $< 2.0\,\text{units}$ are dissolved to eliminate degenerate edge artifacts.
3. **Half-Edge Topology & Step Linkage**: Vertices are welded within spatial tolerance. Twin half-edges are linked, recording vertical delta ($z_{\text{diff}}$) to distinguish flat coplanar transitions from traversable stairs ($z_{\text{diff}} \le 18.25\,\text{units}$) and lethal cliffs.
4. **Spatial Partitioning**: Constructs a balanced 3D KD-Tree (`nav_kdtree_node_t`) over face centroids and builds a direct mapping (`nav_leaf_link_t`) between engine BSP leaf indices and NavMesh polygons for $O(1)$ runtime localization.

---

## 2. Activate: Agent Configuration & Spawning

Agents derive from `svg_monster_base_t` and configure kinematic boundaries, physical capsule hulls, and movement capabilities via `nav_path_policy_t`.

### Key Components
* **Collision Model**: Navigation agents use `SOLID_CAPSULE` (typically mins `{-16, -16, -24}`, maxs `{16, 16, 40}`).
* **Policy Parameters**:
  * `agent_radius`: Physical capsule radius (e.g. $16.0\,\text{units}$).
  * `max_step_height`: Maximum vertical rise the agent can step up (standard engine default: $18.25\,\text{units}$).
  * `max_drop_height`: Maximum safe step-down drop (default: $128.0\,\text{units}$).
  * `waypoint_radius`: Radius threshold to consider an intermediate waypoint reached (default: $24.0\,\text{units}$).
* **Locomotion State**: Managed by `svg_monster_move_t`, containing velocity, ground surface normals, ground entities, and step recovery counters.

---

## 3. Assign: Target Assignment & Path Planning

Issue navigation goals programmatically or via AI state transitions using `ComputePathTo`.

### Pathfinding Pipeline
1. **Localization**: Resolves start and goal origins to navmesh face indices via KD-Tree lookups:
   ```cpp
   int32_t startFace = Nav_FindReachableFaceInLeaf( currentOrigin, goalFace, agentRadius );
   ```
2. **A\* Topological Graph Search** (`Nav_FindPath`):
   * Searches the half-edge graph evaluating traversal cost:
     $$\text{Cost} = \text{Distance} \times \text{SlopePenalty} \times \text{ClearancePenalty}$$
   * Portals narrower than `NAV_ABSOLUTE_MIN_PORTAL_PASSAGE_WIDTH` ($20.0\,\text{units}$) are rejected to prevent routing into impassable geometry.
3. **Double-Precision Funnel String Pulling** (`Nav_StringPull`):
   * Clips raw mesh portals against adjacent obstacle walls and corners.
   * Runs the Simple, Stupid Funnel Algorithm (SSFA) to produce a minimum-distance polyline.
   * **Convex Corner Decoupling**: Analytically computes obstacle corner angle bisectors and inserts outward standoff waypoints (`Nav_EnforceConvexCornerWaypoints`), ensuring agents round sharp corners with guaranteed capsule clearance.
   * **Collinear Decimation**: Simplifies redundant straight-line points while enforcing strict line-of-sight verification before pruning.

---

## 4. Simulate: Game Loop Execution & Kinematics

Navigation simulation runs within the entity's server tick (`ThinkFinish` / physics integration).

### Execution Steps
1. **Waypoint Steering** (`ComputePathSteering`):
   * Tracks active path index `stringPathPos`.
   * Computes normalized 2D direction toward target waypoint $W_k$.
   * **Deadband Gating**: Within $4.0\,\text{units}$ of intermediate waypoints, looks ahead to $W_{k+1}$ to eliminate $180^\circ$ yaw flutter.
   * **Corner Gating**: Restricts switching planes on sharp corners ($> 30^\circ$) until the agent has cleared the approach side of the corner apex.
2. **Kinematic Locomotion** (`SVG_MMove_StepSlideMove`):
   * **NOTE**: The engine does **NOT** use legacy `SV_WalkMove`. Locomotion strictly executes through `SVG_MMove_StepSlideMove` in `svg_mmove_slidemove.cpp`.
   * Integrates horizontal velocity, performs multi-plane surface sliding, and automatically attempts step-up sweeps ($18.25\,\text{units}$) when encountering curbs, stairs, or inclines.
3. **Dynamic Avoidance & Unstick Recovery**:
   * Squad members integrate with the Crowd Manager for formation slotting and steering separation.
   * If an agent is blocked by world geometry for $\ge 32$ consecutive frames, `UpdateBlockedNavigationRecovery` nudges the entity outward along the contact wall normal and forces an immediate A\* path recalculation.

---

## Production C++ Implementation Example

```cpp
#include "svgame/svg_local.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include "svgame/nav/nav_path.h"
#include "svgame/monsters/svg_mmove.h"

/**
*	@brief	Minimal production navigation agent demonstrating the BAAS workflow.
**/
class svg_nav_agent_example_t : public svg_monster_base_t {
public:
	svg_nav_agent_example_t() = default;
	virtual ~svg_nav_agent_example_t() = default;

	/**
	*	@brief	Activate: Initialize agent bounds, collision hull, and path policy.
	**/
	void Spawn() {
		// 1. Physical collision setup.
		this->solid = SOLID_CAPSULE;
		this->movetype = MOVETYPE_STEP;
		this->mins = Vector3{ -16.0f, -16.0f, -24.0f };
		this->maxs = Vector3{  16.0f,  16.0f,  40.0f };

		// 2. Navigation policy configuration.
		this->pathNavigationState.policy.agent_radius = 16.0;
		this->pathNavigationState.policy.max_step_height = 18.25f;
		this->pathNavigationState.policy.max_drop_height = 128.0f;
		this->pathNavigationState.policy.waypoint_radius = 24.0f;
	}

	/**
	*	@brief	Assign: Command the agent to navigate to a target destination.
	*	@param	destinationWorld	3D target coordinate in feet-origin space.
	*	@return	True if a valid NavMesh path was calculated.
	**/
	bool MoveTo( const Vector3 &destinationWorld ) {
		return this->ComputePathTo( destinationWorld, this->pathNavigationState.policy );
	}

	/**
	*	@brief	Simulate: Per-frame steering, physics step, and waypoint progression.
	**/
	void Think() {
		// 1. Verify active path.
		if ( this->stringPulledPath.empty() || this->stringPathPos >= this->stringPulledPath.size() ) {
			return;
		}

		// 2. Compute active steering direction and speed scaling.
		Vector3DP moveDir2D{};
		float speedScale = 1.0f;
		if ( !this->ComputePathSteering( &moveDir2D, &speedScale ) ) {
			// Arrived at destination.
			this->ResetNavigationPath();
			return;
		}

		// 3. Update yaw orientation.
		this->ideal_yaw = QM_Vector3ToYawDP( moveDir2D );
		SVG_MMove_FaceIdealYaw( this, this->ideal_yaw, 360.0f );

		// 4. Kinematic step slide-move simulation (no SV_WalkMove).
		const float moveSpeed = 200.0f * speedScale;
		this->velocity.x = static_cast<float>( moveDir2D.x ) * moveSpeed;
		this->velocity.y = static_cast<float>( moveDir2D.y ) * moveSpeed;

		SVG_MMove_StepSlideMove( &this->monsterMove.state, this->velocity, this );
	}
};
```

---

## Debugging & Visual Diagnostics

| Cvar | Value | Description |
| :--- | :---: | :--- |
| `nav_debug_draw` | `1` | Renders global NavMesh wireframes and polygon boundaries. |
| `nav_debug_path` | `1` | Visualizes active A\* corridors, traversed/future waypoints, and corner standoffs. |
| `nav_debug_corners` | `1` | Displays cached convex obstacle corner vertices and outward angle bisectors. |
| `nav_status` | *N/A* | Prints memory usage, face/edge counts, and active worker thread status to console. |
