# Navigation Performance & Corner-Traversal Baseline Report (Milestone M1)

**Document Version**: 1.0 (Immutable M1 Baseline)  
**Creation Date**: 2026-10-02  
**Baseline Branch**: `navmesh4`  
**Git Baseline Commit**: `63b55baa`  
**Execution Environment**: MSVC 2026 (v18.11.0-insiders) x64 Debug (`out/build/x64-Debug`)  
**Target Module**: `baseq2rtxp_svgame` (`baseq2rtxp/svgamex86_64_d.dll`)  
**Engine Architecture**: Cross-platform POSIX threading interface (`inc/system/pthread.h`, `src/windows/pthread.c`, `src/unix/pthread.c`), decoupled game module boundary communicating via `svgame_import_t gi.GetThreadCpuTimeNs()`.

---

## 1. Executive Summary & Purpose

This document establishes the official, immutable Milestone `M1` baseline for the Quake II RTX navigation performance, memory modernization, and corner-traversal refactoring project as governed by [`docs/navigation-performance-masterplan.md`](file:///d:/Dev/Projects/Q2RTXPerimental/code/docs/navigation-performance-masterplan.md).

Prior to Milestone `M2` (Server-Game Allocator Rollout) and Milestone `M4` (Corner Generation, Progression, and Movement Correction), this baseline captures:
1. **Verified Passing Invariants**: All system invariants, architectural boundaries, telemetry instrumentation, and gate assertion checks that must remain strictly operational and non-regressed across all future milestones.
2. **Classified Source Findings & Known Expected Failures (`K1`–`K12`)**: Known defects, mathematical shortcuts, coordinate mismatches, and premature corner-cutting vulnerabilities audited in the baseline source code. These failures are formally accepted as pre-existing baseline behaviors and will be methodically eliminated in Milestones `M3` and `M4`.
3. **Telemetry & Verification Commands**: In-engine console tools registered to measure CPU phase breakdowns, run deterministic path benchmarks, and execute automated validation gates.

---

## 2. Verified Passing Invariants (M1 Correctness Gate)

The following architectural, algorithmic, and telemetry invariants are implemented, compiled, and verified operational:

### 2.1 Frame-Rate Discipline & Engine Isolation
- **40 Hz / 25 ms Game Thread Discipline**: Authoritative entity thinking, monster movement physics (`svg_mmove.cpp`, `svg_mmove_slidemove.cpp`), collision tracing, and crowd coordination remain strictly bounded to the game thread (`SVG_RunFrame`). Frame overruns exceeding the 25.0 ms deadline are monitored and tallied by `NAV_PERF_PHASE_SERVER_TICK`.
- **Game Module Isolation & Portability**: The game module (`baseq2rtxp_svgame`) does not include platform-specific OS headers (`<windows.h>`, `<synchapi.h>`), does not link platform threading libraries directly, and does not compile host OS translation units. Cross-platform capabilities (such as high-resolution thread CPU time) are exposed exclusively through the host engine imports table (`svgame_import_t gi;`).
- **Cross-Platform Threading Abstraction**: Engine pthread primitives are declared via clean POSIX headers (`inc/system/pthread.h`) with concrete definitions segregated into OS translation units (`src/windows/pthread.c` utilizing Slim Reader/Writer Locks and Condition Variables; `src/unix/pthread.c` utilizing native `pthread` and `clock_gettime(CLOCK_THREAD_CPUTIME_ID)`).

### 2.2 Gate Verification Harness (`sv nav_perf_check_gates`)
The test harness validates non-negotiable failure-handling invariants via `Nav_Perf_CheckGates()`:
- **Gate 1: Intentionally Invalid Endpoint Rejection**:
  - Out-of-bounds coordinates (e.g. `Vector3DP(9999999, 9999999, 9999999)`) return `Nav_FindPolyInLeaf() == -1` cleanly without memory corruption or bounds overflow.
  - Deep in-solid coordinates (e.g. `Vector3DP(-9999999, -9999999, -9999999)`) return `Nav_FindPolyInLeaf() == -1` cleanly.
  - Invalid start face (`startFace = -1`) and invalid goal face (`goalFace = -1`) are cleanly rejected by `Nav_FindPath()` returning `false` with an empty corridor vector.
  - Empty path corridors passed to `Nav_StringPull()` return `false` with empty waypoint and forced vectors.
  - Invariant telemetries record `NAV_PERF_OUTCOME_FAILED_NO_ROUTE` without runaway allocations or crashes.
- **Gate 2: Duplicate Formation Slot Detection**:
  - The formation assignment validation harness detects and flags any collision where two distinct crowd members claim or occupy the same formation slot index (`simulatedSlots[slot] == memberA && slot == memberB`).
- **Gate 3: Stalled Member Arrival Detection**:
  - The crowd motion validator flags any condition where a crowd group transitions to idle (`isMoving == false`) while any active member has not satisfied arrival criteria (`reachedGoal == false`), preventing silent abandonment of stalled agents.

### 2.3 13-Phase RAII Performance Instrumentation
High-resolution elapsed wall-clock and thread CPU time tracking is active across 13 distinct phases:
1. `SERVER_TICK`: Complete server frame duration and 25 ms deadline violation tracking.
2. `NAV_ORCHESTRATION`: Top-level monster path planning orchestration (`ComputePathTo`).
3. `START_GOAL_LOOKUP`: KD-tree leaf face resolution (`Nav_FindPolyInLeaf`, `Nav_FindFaceInLeafStrict`).
4. `GLOBAL_FALLBACKS`: Global mesh polygon scans when leaf lookup fails (`Nav_FindClosestPolyGlobal`).
5. `CACHE_BUILD`: Obstacle corner caching and boundary classification (`Nav_EnsureObstacleCornersTable`).
6. `ASTAR_SEARCH`: Graph search expansion and priority queue execution (`Nav_FindPath`).
7. `PORTAL_FUNNEL`: String pulling portal-crossing optimization (`Nav_StringPull`).
8. `CORNER_REFINEMENT`: Convex corner standoff and waypoint insertion (`Nav_EnforceConvexCornerWaypoints`).
9. `GEOMETRIC_LOS`: Geometric 2D line-of-sight checks (`Nav_HasGeometricLineOfSight2D`).
10. `MOVEMENT_TRACES`: Physical movement swept traces (`SVG_MMove_Trace`, `SVG_Trace`).
11. `COVER_SCORING_TRACES`: Tactical cover point evaluation and visibility traces (`Nav_FindCoverPoints`).
12. `CROWD_COORDINATION`: Crowd group lifecycle, formation evaluation, and ingress (`SVG_Crowd_Frame`).
13. `DEBUG_DRAWING`: Server-side navigation mesh and waypoint debug visualization flushes.

---

## 3. Classified Source Findings & Expected Baseline Failures (`K1`–`K12`)

The following findings represent known algorithmic shortcuts and failure modes documented in the baseline source audit. Under Milestone `M1` and `M2`, these behaviors are recognized as known defects and must **not** be inadvertently masked, altered, or partially tuned until their designated milestones (`M3` for query/cache fixes, `M4` for corner/steering fixes).

| Finding ID | Source Location | Audit Classification & Reproduction Risk | Target Milestone |
|---|---|---|---|
| **K1** | `entities/monster/svg_monster_base.cpp:551-564` | **Initial Waypoint Skipping**: `ComputePathTo` immediately skips waypoint 1 (`W1`) in favor of waypoint 2 (`W2`) solely because `W1` falls within arrival radius, without verifying whether `W1` was a forced standoff point or whether the outgoing hull sweep to `W2` is physically clear. | **M4** |
| **K2** | `entities/monster/svg_monster_base.cpp:746-841` | **Premature Sharp Corner Release**: For standard agent radius 16, sharp reach threshold is computed as `max(12, 16 + 4) = 20` (contradicting documentation asserting 12). `withinRadius` triggers waypoint advance prior to physical clearance probe, and its approach-plane gate excludes forced corner standoffs, causing premature turn initiation into corner geometry. | **M4** |
| **K3** | `entities/monster/svg_monster_base.cpp:762-770, 825-832, 854-860` | **Clearance Envelope Shrinking**: Short-link clearance calculation drops to `min(R, 0.4 * distance)` when waypoint distance is `< 2R`. At radius 16 and distance 20, the safety envelope shrinks to radius 8. Physical hull requirements must remain invariant regardless of waypoint density. | **M4** |
| **K4** | `entities/monster/svg_monster_base.cpp:929-956` | **Excessive Lookahead Tolerance**: Lookahead steering activates within 24 units and accepts waypoint angle deviations where `dot > 0.7071` (~45°), despite documentation stating 30°. Establishing 2D LOS to this distant point does not validate subsequent intermediate steering modifications. | **M4** |
| **K5** | `entities/monster/svg_monster_base.cpp:1079-1159` | **Unchecked Steering Overrides**: Transit blending toward outgoing tangent vectors, forward-monotonicity overrides, and deadband heading corrections lack a final common hull sweep. Blended steering can turn into geometry even after an earlier departure test rejected clearance. | **M4** |
| **K6** | `nav/nav_path.cpp:1826-1903` | **Wall Endpoint Projection Loophole**: `Nav_HasGeometricLineOfSight2D` only rejects clearance if a wall's endpoint projects strictly inside travel segment `t ∈ (0.05, 0.95)`. Long continuous walls spanning the travel segment bypass rejection. Counterexample: segment `(0, 10, 0)` to `(10, 10, 0)` with wall `(-100, 0, 0)` to `(100, 0, 0)` and radius 16 permits clearance despite distance 10 < 16. | **M4** |
| **K7** | `nav/nav_path.cpp:2392-2441, 2556-2624, 2725-2782` | **Standoff Bisector Contraction**: Radial corner standoff constrains distance along a bisector rather than perpendicular clearance to both intersecting wall planes (`d * dot(b, n) >= R + m`). Standoff points can contract below safe physical hull thresholds at sharp acute angles. | **M4** |
| **K8** | `nav/nav_path.cpp:2800-2960, 3336-3455` | **Arc Chord Corner Cutting**: Polygonal arc discretization around convex corners can produce chords that slice through inflated corner boundaries even when the discrete vertices are clear. | **M4** |
| **K9** | `nav/nav_path.cpp:3002` | **Omission of Swept Hull in String Pulling**: Double-precision `Nav_StringPull` accepts bounding box dimensions (`mins`, `maxs`) and trace flags in its parameter list, but does not perform physical hull sweeps in its loop body, operating as a purely 2D point string pull. | **M4** |
| **K10** | `entities/monster/svg_monster_base.cpp:1251-1275`, `monsters/svg_mmove.cpp:145-256` | **Coordinate Space Mismatch in Probes**: Monster separation logic passes feet-origin coordinates with bounding-box-relative dimensions directly to probes expecting bounding-box-center coordinates. Furthermore, probe Boolean returns are treated as total clearance even when progress was partial. | **M4** |
| **K11** | `entities/monster/svg_monster_base.cpp:1166-1176, 1219-1221, 1326-1397`, `monsters/svg_mmove.cpp:455` | **Decoupled Yaw vs. Translation Velocity**: Desired steering direction directly commands velocity vectors while cosmetic visual yaw is clamped to 45° per tick. Slowing visual yaw does not decelerate translational motion, leading to sideways sliding into walls. | **M4** |
| **K12** | `entities/monster/svg_monster_base.cpp:106, 166`, `monsters/svg_mmove.cpp:313`, `monsters/svg_mmove_slidemove.cpp:140` | **Velocity Clipping Masking**: Ground and elevated step trials deduplicate contacts by entity; subsequent clear trials can overwrite blocked flags, masking wall contacts in telemetry unless raw velocity clipping events are explicitly tracked. | **M4** |

---

## 4. Operational In-Engine Commands

The baseline delivers the following console commands registered in `src/baseq2rtxp/svgame/svg_commands_server.cpp`:

1. `sv nav_perf_reset`:
   Resets all circular phase reservoirs, percentiles, and telemetry counters across all 13 phases.
2. `sv nav_perf_report [json]`:
   Prints high-resolution profiling tables displaying `p50`, `p95`, `p99`, `min`, `max`, `mean`, execution counts, and deadline violations for all 13 phases. Passing `json` outputs structured machine-readable JSON.
3. `sv nav_bench_path [runs] [sx sy sz gx gy gz] [json]`:
   Executes deterministic $N$-run pathfinding benchmark against explicit start/goal coordinates or active debug goals A & B (`sv nav_dbg_goal_a`, `sv nav_dbg_goal_b`), reporting timing distributions and throughput.
4. `sv nav_perf_check_gates`:
   Executes the automated M1 gate harness verifying invalid endpoint rejection, formation slot collision detection, and stalled member arrival detection.
5. `sv crowd_check_ingress <crowdID> [max_seat_error] [min_separation]`:
   Recreated automated ingress checker. Evaluates crowd arrival conditions (`reachedGoal == true`, `moving == 0`, `ingress == 0`, `inHead == N/N`), seat placement accuracy, and pairwise hull separation distances.

---

## 5. Transition Gates to Milestone M2

With Milestone `M1` formally closed and verified, development proceeds to **Milestone M2 (Server-Game Allocator Rollout)** under the following binding constraints:
- **Zero Behavioral Regressions**: The replacement of `nav_vector_t` with `svg_vector_t<T, TAG_SVGAME_NAVMESH>` must not alter pathfinding outcomes, waypoint sequences, steering thresholds, or physics simulation outputs.
- **Engine-Tag Accounting Integrity**: All heap backing allocations in server-game navigation structures must route through `svg_zone_allocator_t` under `TAG_SVGAME_NAVMESH` via `gi.TagMalloc` / `gi.TagFree`.
- **Preservation of Baseline Known Defects**: Milestones `M2` must strictly preserve existing baseline geometry and corner handling behaviors (`K1`–`K12`) so that memory refactoring is independently verified before behavioral changes occur in `M3` and `M4`.
