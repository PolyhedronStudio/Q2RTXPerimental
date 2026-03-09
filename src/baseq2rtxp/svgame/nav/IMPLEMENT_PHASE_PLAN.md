# Navigation Hierarchy Implementation Plan for Copilot Agent

* Repository: `PolyhedronStudio/Q2RTXPerimental`  
* Branch: `navmesh`  
* Audience: Visual Studio 2026 Copilot Agent / engineers implementing navigation improvements  
* Primary goal: Achieve real-time or near-real-time NPC/Monster navigation performance.  
* Secondary goal: Preserve reasonable path precision.  
* API goal: Keep the public navigation API simple and similar to the current agent/policy-driven model.

---

## 1. Mission Summary

The current navigation/pathfinding can take seconds in some scenarios, especially on open-area maps such as the cpuburst test level. The intended solution is a `static-first` hierarchical navigation system layered above the current tile/leaf representation.

The final architecture should:

- Keep the current gameplay-facing API simple.
- Preserve agent/policy-style path queries.
- Add a `static-first` region/portal hierarchy above existing tiles/leaves.
- Use coarse `A*` for long-range routing.
- Use corridor-constrained local refinement instead of full flat `A*` over the entire map.
- Use LOS tracing as an accelerator:
  - direct start-to-goal shortcut,
  - corridor shortcutting,
  - path simplification.
- Prefer `fast DDA` or `leaf traversal`, whichever is faster; hybrid is allowed.
- Use `dynamic occupancy` as an overlay.
- Support future `local portal invalidation/overlay` for dynamic inline models.
- Never depend on global nav rebuilds for dynamic changes.

---

## 2. Non-Negotiable Constraints

### DO

- Do work incrementally and keep changes reviewable.
- Add instrumentation before major behavioral changes.
- Reuse current data structures where practical.
- Keep the public API stable or minimally changed.
- Prioritize performance first, precision second.
- Maintain safe fallback behavior during migration.
- Optimize specifically for open-area performance.
- Add debug stats and benchmarks.
- Keep code maintainable and documented.

### DO NOT

- Do not replace the pathfinding system in one giant rewrite.
- Do not use LOS tracing as a full replacement for pathfinding.
- Do not implement global dynamic nav rebuilds.
- Do not over-engineer a perfect funnel/navmesh-only solution first.
- Do not broadly refactor unrelated systems.
- Do not significantly change the gameplay-facing API unless absolutely required.
- Do not optimize for perfect path optimality at the expense of query speed.
- Do not expose hierarchy internals as required gameplay-facing concepts.

---

## 3. Intended Final Architecture

### 3.1 Query order

The intended final path query order is:

1. Resolve start/goal leaf/tile and region.
2. Attempt direct LOS shortcut.
3. Run coarse `A*` on region/portal hierarchy.
4. Build a corridor from the coarse result.
5. Run local refinement constrained to that corridor.
6. Simplify resulting path using LOS-based collapse/string-pull-style reduction.
7. Fall back to the old flat pathfinder only if needed.

### 3.2 Static-first hierarchy

The hierarchy should contain:

- Leaves/Tiles: existing fine navigable units.
- Regions: static groups of connected leaves/tiles.
- Portals: static transitions between regions.
- Coarse Graph: graph over regions or portals used for global A*.

### 3.3 Dynamic behavior

Dynamic obstacles and inline models must be handled via:

- dynamic occupancy overlay now,
- future local portal invalidation/overlay later,
- never via global rebuild as the default strategy.

---

## 4. Engineering Task Checklist

> Copilot Agent: update these checkboxes as you complete work.  
> If you cannot update the file automatically, include progress notes in your output and then update this file in a dedicated commit.

---

### Phase 0 — Discovery and Baseline

- [x] Analyze the current navigation/pathfinding architecture.
- [x] Identify current query entry points, core algorithms, and data structures.
- [x] Identify current leaf/tile/cell representation.
- [x] Identify current occupancy or dynamic obstacle logic.
- [x] Identify existing debug hooks or developer test commands.
- [x] Add or update a short design note summarizing the current architecture and likely bottlenecks.

#### Exit criteria
- [x] There is a documented understanding of the current nav stack.
- [x] The minimum files/types/functions to extend are identified.

#### Phase 0 findings

Current gameplay-facing query flow

- NPCs and movement callers currently route through `svg_nav_path_process_t`, which owns path buffers, rebuild throttles, async request bookkeeping, and follow-state heuristics.
- The main async entry point is `SVG_Nav_RequestPathAsync()` in `svg_nav_request.cpp`.
- Request execution is split across:
  - main-thread queue bookkeeping and lightweight validation/projection,
  - worker-thread endpoint resolution plus `Nav_AStar_Init()`,
  - main-thread incremental stepping via `Nav_AStar_Step()`,
  - path commit through `svg_nav_path_process_t::CommitAsyncPathFromPoints()`.
- Gameplay path consumption is currently waypoint-driven via `svg_nav_path_process_t::QueryDirection3D()`.

Current fine navigation representation

- `nav_mesh_t` is the authoritative world-navigation container.
- Static world geometry is stored as canonical `world_tiles`, keyed by `(tile_x, tile_y)` and referenced from BSP leaves through `nav_leaf_data_t::tile_ids`.
- Each `nav_tile_t` uses sparse storage:
  - `presence_bits` identify which XY cells exist,
  - `cells` stores `nav_xy_cell_t` entries,
  - each `nav_xy_cell_t` contains multiple `nav_layer_t` walkable layers for stacked floors.
- `nav_layer_t` already carries much of the fine traversal metadata needed for future hierarchy work:
  - traversal feature bits,
  - per-edge feature bits,
  - explicit ladder endpoint flags,
  - quantized ladder start/end heights,
  - clearance.

Current coarse / hierarchical state

- There is already a tile-level cluster graph used as an optional coarse route filter.
- The current coarse layer is still tile-centric, not region/portal-centric.
- Fine async A*  remains the primary global search algorithm, with the coarse graph acting as a search corridor hint rather than a full hierarchical routing system.

Current dynamic handling

- Dynamic occupancy is already overlay-based through `nav_mesh_t::occupancy`.
- Occupancy keys are tile/cell/layer granular and carry both:
  - `soft_cost`,
  - `blocked`.
- Async traversal consults occupancy through `SVG_Nav_Occupancy_Blocked()` and `SVG_Nav_Occupancy_SoftCost()`.
- This means the current codebase already aligns with the intended sparse-overlay direction and does not rely on global nav rebuilds for actor avoidance.

Current debug / diagnostics hooks

- `nav_debug_draw >= 2` enables targeted path diagnostics through `Nav_PathDiagEnabled()` in `svg_nav_traversal.cpp`.
- `nav_nav_async_log_stats` exposes async queue diagnostics in `svg_nav_request.cpp`.
- `nav_expand_diag_enable` and `nav_expand_diag_cooldown_ms` gate neighbor-expansion diagnostics in `svg_nav_traversal_async.cpp`.
- The sound-follow testdummy adds entity-local navigation diagnostics through `DUMMY_NAV_DEBUG`.
- Existing diagnostics are log-oriented; no dedicated benchmark harness or repeatable open-area performance runner was found during this pass.

Likely current bottlenecks

- Long/open-area queries still depend on flat fine-grained async A*  expansion over tile/cell/layer space.
- The optional cluster route filter constrains search, but it does not replace the cost of globally exploring large fine-resolution spaces.
- Request queuing and async stepping are already incremental, so the remaining performance pressure is likely dominated by:
  - neighbor expansion volume,
  - fine step validation,
  - large open-list / node-set growth,
  - absence of a true region/portal hierarchy above tiles.
- Because no benchmark harness was found, the next phase should add measurement before further behavioral or architectural changes.

---

### Phase 1 — Instrumentation and Benchmarking

- [x] Add lightweight navigation query instrumentation.
- [x] Measure total query time.
- [x] Measure node/leaf/tile expansions.
- [x] Measure open-list pushes/pops if practical.
- [x] Measure path length / waypoint count.
- [x] Add structured debug stats output.
- [x] Add a reproducible benchmark/test harness for representative long/open-area queries.
- [x] Ensure benchmark output is deterministic enough for comparison.

#### Suggested debug stats
- [x] `total_ms`
- [x] `coarse_ms`
- [x] `refine_ms`
- [x] `leaf_expansions`
- [x] `coarse_expansions`
- [x] `open_pushes`
- [x] `open_pops`
- [x] `los_tests`
- [x] `los_successes`
- [x] `fallback_used`

#### Exit criteria
- [x] Baseline performance can be measured before hierarchy rollout.
- [x] There is a clear benchmark path for cpuburst/open-area scenarios.

---

### Phase 2 — Hierarchy Scaffolding

- [x] Introduce internal types for regions.
- [x] Introduce internal types for portals.
- [x] Introduce hierarchy storage/container.
- [x] Add region membership storage for leaves/tiles.
- [x] Add policy/clearance compatibility hooks for regions/portals.
- [x] Add future-facing portal invalidation/dirty flags.
- [x] Keep hierarchy internal and non-breaking.

#### Exit criteria
- The codebase has internal abstractions ready for static hierarchy building.
- No gameplay-facing API break is introduced.

---

### Phase 3 — Static Region Construction

- [x] Build or formalize fine leaf/tile adjacency.
- [x] Partition navigable leaves/tiles into static regions.
- [x] Make region partitioning deterministic.
- [x] Validate connectedness of regions.
- [x] Add debug counters for number of regions and average region size.
- [x] Log suspicious oversized or isolated regions.

#### Notes
- First pass should be practical and robust.
- Do not over-engineer partitioning heuristics yet.

#### Exit criteria
- Static region decomposition exists and is inspectable/debuggable.

---

### Phase 4 — Portal Extraction

- [x] Detect traversable boundaries between neighboring regions.
- [x] Create portal records between connected regions.
- [x] Store representative portal geometry.
- [x] Store traversal cost estimates.
- [x] Store policy/clearance applicability.
- [x] Build region/portal coarse connectivity graph.
- [x] Add graph validation and connectivity debug output.

#### Notes
- Merged/simple representative portal geometry is acceptable initially.
- Avoid unnecessary geometric complexity.

#### Exit criteria
- A valid static region/portal graph exists.

---

### Phase 5 — Coarse A*  Implementation

- [x] Implement start/goal lookup to leaf/tile and region.
- [x] Implement coarse A*  over region/portal graph.
- [x] Add heuristics suitable for fast global routing.
- [x] Keep the new path behind a feature flag or controlled code path initially.
- [x] Preserve fallback to the current flat pathfinder.
- [x] Add coarse-search-specific instrumentation.

#### Exit criteria
- Coarse routes can be computed and debugged.
- Existing gameplay pathfinding still works via fallback.

---

### Phase 6 — LOS Trace System

- [x] Add a reusable LOS trace interface.
- [x] Support `Auto` mode if practical.
- [x] Support `DDA` mode if practical.
- [x] Support `LeafTraversal` mode if practical.
- [x] Prefer DDA where data layout favors it.
- [x] Allow hybrid mode selection where beneficial.
- [x] Ensure policy-aware and occupancy-aware checks.
- [x] Add LOS instrumentation.

#### LOS intended uses
- [x] Direct start-to-goal shortcut.
- [ ] Portal-to-portal or path-node shortcutting.
- [ ] Path simplification.

#### Exit criteria
- LOS trace exists as a reusable accelerator, not a replacement pathfinder.

---

### Phase 7 — Direct Goal Shortcut

- [x] Attempt direct LOS at the beginning of path queries.
- [x] Return direct path when valid.
- [x] Fall back safely when direct LOS fails.
- [x] Preserve policy and occupancy semantics.
- [x] Measure success rate via stats.

#### Exit criteria
- Cheap direct paths can bypass expensive search where possible.

---

### Phase 8 — Corridor-Constrained Refinement

- [x] Build a corridor from the coarse region/portal route.
- [x] Restrict local refinement to corridor-relevant leaves/tiles.
- [x] Run local A*  inside the corridor instead of full-map fine A* .
- [x] Preserve fallback behavior on failure.
- [x] Instrument refinement-specific work and timing.

#### Notes
- This is preferred now over implementing a full perfect funnel system.
- Focus on robust, bounded local refinement.

#### Exit criteria
- Long-range queries use much smaller local search spaces.

---

### Phase 9 — Path Simplification

- [x] Add LOS-based path simplification after refinement.
- [x] Greedily collapse intermediate nodes where valid.
- [x] Keep simplification policy-aware and occupancy-aware.
- [x] Measure simplification attempts/successes if practical.

#### Exit criteria
- Paths are shorter/smoother and waypoint counts are reduced, especially in open areas.

---

### Phase 10 — Dynamic Occupancy Overlay

- [x] Introduce or formalize dynamic occupancy overlay.
- [x] Ensure LOS checks consult dynamic occupancy.
- [x] Ensure corridor refinement consults dynamic occupancy.
- [x] Ensure hierarchical queries remain based on static hierarchy + dynamic overlay.
- [x] Avoid any global rebuild trigger for dynamic occupancy changes.

#### Exit criteria
- Dynamic blockers can affect path results without global hierarchy rebuilds.

---

### Phase 11 — Local Portal Invalidation Hooks

- [x] Add lightweight local portal invalidation/overlay flags.
- [x] Make portals skippable or penalizable when locally invalidated.
- [x] Keep this local and overlay-based.
- [x] Avoid implementing full dynamic portal regeneration.
- [x] Leave room for future inline-model invalidation logic.

#### Exit criteria
- The system is prepared for future dynamic inline model handling without global rebuild architecture.

---

### Phase 12 — Hierarchical Path Becomes Preferred

- [x] Enable hierarchical routing as the preferred route for suitable queries.
- [x] Preserve fallback to old flat pathfinding when needed.
- [x] Ensure the final query flow is:
  - [x] start/goal lookup
  - [x] direct LOS
  - [x] coarse A* 
  - [x] corridor refinement
  - [x] simplification
  - [x] fallback if required
- [x] Add clear stats for when fallback is used.

#### Exit criteria
- The hierarchy is the main pathfinding path for long-range/open-area queries.

---

### Phase 13 — Open-Area Tuning

- [x] Tune thresholds for when to attempt direct LOS.
- [x] Tune thresholds for when to use hierarchy.
- [x] Tune corridor width / refinement scope.
- [x] Tune LOS simplification aggressiveness.
- [x] Tune to avoid pathological refinement expansion.
- [x] Use cpuburst/open-area benchmarks to validate gains.

#### Exit criteria
- Measured performance improves substantially on representative open-area cases.

---

### Phase 14 — Public API Stability

- [x] Review public navigation API.
- [x] Keep agent/policy-driven query semantics.
- [x] Avoid exposing region/portal internals.
- [x] Keep callsite changes minimal.
- [x] Document any necessary API additions as optional/minimal.

#### Exit criteria
- Gameplay systems still interact with navigation through a simple, similar API.

---

### Phase 15 — Final Documentation and Cleanup

- [x] Document final architecture in repo.
- [x] Explain why flat pathfinding was too slow.
- [x] Document hierarchy, LOS, refinement, and dynamic occupancy overlay.
- [x] Explicitly document what is intentionally not implemented.
- [x] Perform limited cleanup only in touched nav/path code.
- [x] Remove temporary migration dead code where safe.
- [x] Keep instrumentation and benchmark hooks intact.

#### Exit criteria
- Repo contains a clear design explanation.
- Final code matches intended architecture and excludes forbidden approaches.

---

## 5. Copilot Agent Working Rules

When using this file as an implementation guide, follow these rules during every step:

### Required behavior

- Work on one checklist phase or sub-phase at a time.
- Before changing behavior, inspect current code paths.
- If a phase requires measurement, implement measurement before optimization.
- Prefer additive/internal changes first.
- Keep diffs narrow and easy to review.
- Maintain comments/docstrings for new systems.

### Forbidden behavior

- Do not silently introduce broad refactors.
- Do not remove fallback behavior too early.
- Do not replace search with LOS-only routing.
- Do not implement dynamic global nav rebuilds.
- Do not build a giant speculative system unrelated to measured bottlenecks.

---

## 6. Recommended Commit Strategy

Use small, phase-based commits where possible. Example:

1. `nav: document current pathfinding and add debug stats`
2. `nav: add benchmark harness for cpuburst path queries`
3. `nav: add static region/portal hierarchy scaffolding`
4. `nav: build static region partition and portal graph`
5. `nav: add coarse A*  on region hierarchy`
6. `nav: add LOS trace interface and direct path shortcut`
7. `nav: add corridor-constrained refinement`
8. `nav: add path simplification and dynamic occupancy overlay`
9. `nav: add local portal invalidation hooks`
10. `nav: enable hierarchical pathfinding by default for long-range queries`
11. `nav: tune open-area navigation thresholds and document architecture`

---

## 7. Validation Checklist for the Final Result

The final implementation is acceptable only if all of the following are true:

- [ ] Navigation performance is substantially improved for long/open-area queries.
- [x] Public API remains simple and similar.
- [x] Path queries still use policy/agent semantics.
- [x] Static-first hierarchy exists and is used.
- [x] Coarse A*  is used for long-range routing.
- [x] Local refinement is corridor-constrained.
- [x] LOS is used as an accelerator and simplifier.
- [x] Dynamic occupancy is overlay-based.
- [x] Future local portal invalidation hooks exist.
- [x] No global dynamic rebuild architecture was introduced as the main strategy.
- [x] No LOS-only routing architecture replaced pathfinding.
- [x] No broad unrelated refactor was bundled into this work.

---

## 8. Progress Log

> Update this section during implementation.

### Current status
- Phase in progress: `Completed through Phase 15 — Final Documentation and Cleanup`
- Last updated by: `GitHub Copilot`
- Benchmark status: `Implemented sync-query instrumentation plus the deterministic server command sv nav_bench_path <sx> <sy> <sz> <gx> <gy> <gz> [iterations] and structured snapshot output via sv nav_query_stats.`

### Notes
- Current gameplay pathfinding is already split into queue bookkeeping, worker-thread A*  initialization, main-thread incremental stepping, and path-follow consumption.
- The world mesh already has the fine-grained metadata needed for future hierarchy work: sparse tiles, multi-layer cells, per-edge feature bits, ladder endpoint semantics, and sparse occupancy overlays.
- A tile-level cluster graph already exists, but it currently behaves as a coarse route filter rather than a full static-first region/portal hierarchy.
- Phase 1 now captures lightweight sync-query timing/counter snapshots from the shared traversal path and exposes them through `SVG_Nav_GetLastQueryStats()`, `sv nav_query_stats`, and `sv nav_bench_path`.
- The current benchmark harness measures the existing flat/coarse-filtered traversal stack with explicit coordinate inputs, which provides a deterministic baseline before hierarchy rollout.
- `los_tests` and `los_successes` now report shared LOS usage from direct-shortcut and simplification passes in sync queries, while the newer simplification counters separate post-refinement collapse work from the aggregate LOS totals.
- Phase 2 now adds runtime-only hierarchy scaffolding to `nav_mesh_t`: internal region/portal records, compatibility hooks, future-facing portal invalidation flags, and tile/leaf membership storage kept reset on load/regenerate until Phase 3 populates them.
- Phase 3/4 now formalize canonical world-tile adjacency from persisted per-layer edge metadata, split connected tile space into deterministic coarse regions using a fixed tile budget, rebuild leaf-to-region membership, merge traversable cross-region seams into initial portal records, validate graph consistency, and emit concise region/portal summary diagnostics.
- Phase 5 now resolves start/goal region membership from canonical nodes, runs initial coarse A*  over the region/portal graph, materializes the region path back into the existing tile-key filter for fine A* , keeps the hierarchy route behind `nav_hierarchy_route_enable`, and falls back to the legacy cluster route when hierarchy routing is unavailable or unsuitable.
- Phase 6 now adds a reusable canonical-node LOS API with `Auto`, `DDA`, and conservative `LeafTraversal` modes, keeps `Auto` biased toward nav-grid DDA, applies hazard and sparse-occupancy policy checks, and wires LOS attempts/successes into the existing sync query stats collector without changing path-query control flow yet.
- Phase 7 now attempts the shared direct LOS shortcut immediately after canonical endpoint resolution in both sync and async query flows, returns a minimal two-point nav-center path on success, and falls through to the existing coarse-route plus fine-A*  pipeline when LOS fails.
- Phase 8 now formalizes bounded local refinement as an explicit corridor object, carries hierarchy or cluster-derived corridor metadata into sync and async A*  initialization, constrains fine expansion to corridor tiles, and exposes corridor size/usage metrics through the existing structured stats output.
- Phase 9 now applies a shared greedy LOS simplifier to finalized multi-point paths after refinement, reuses the policy-aware and occupancy-aware LOS API for collapse tests, and reports simplification attempt/success counters through structured sync query stats and benchmark output.
- Phase 10 now formalizes sparse dynamic occupancy around a shared overlay lookup helper, keeps corridor refinement on soft-cost-first and hard-block-second semantics, makes LOS-based shortcuts/simplification conservatively fail when the overlay must influence the result, and keeps dynamic updates strictly local without any hierarchy rebuild coupling.
- Phase 11 now adds a lightweight local portal overlay alongside the static portal graph, lets coarse hierarchy routing skip or penalize locally invalidated portals without regenerating the graph, and exposes coarse portal-overlay usage through structured sync query stats and benchmark output.
- Phase 12 now treats hierarchy routing as the preferred coarse path whenever the static hierarchy is enabled and ready, keeps direct LOS ahead of it, preserves cluster or unrestricted fallback when hierarchy routing cannot provide a usable corridor, and adds explicit stats for hierarchy preference, hierarchy fallback, and unrestricted refinement fallback. The preferred-flow implementation now also constrains long LOS collapses in narrow passages by requiring additional clearance margin and keeps async simplification on a revalidated candidate-swap path immediately before commit.
- Phase 13 now converts the main open-area heuristics into runtime tuning controls: direct LOS attempts are gated by a LOS sample budget, hierarchy preference is gated by a minimum query distance, corridor widening uses tunable near/mid/far thresholds and radii, and long-span LOS simplification uses tunable narrow-passage distance and clearance-margin knobs. The benchmark output now also reports buffered corridor radius and buffered corridor tile counts so refinement-scope tuning is measurable.
- Phase 14 now confirms the gameplay-facing navigation API remains centered on simple origin/agent/policy entry points and `svg_nav_path_process_t`, keeps hierarchy, corridor, portal-overlay, and tuning behavior internal to nav implementation files, and narrows back one unnecessary public helper exposure so recent tuning work does not widen the caller-facing API.
- Phase 15 now adds final repo documentation in `NAV_HIERARCHY_ARCHITECTURE.md`, includes contextual `.svg` visuals for the hierarchy, corridor refinement, and dynamic overlays, explicitly documents the implemented non-goals, and preserves the benchmark plus instrumentation hooks as part of the final measured architecture story.
- Practical simplification follow-up now keeps funnel deferred until portal extraction stores true portal segment geometry, strengthens the shared simplifier with duplicate removal plus collinearity pruning, uses a tiny sync LOS budget after refinement, uses a more aggressive async farthest-jump LOS search before revalidated swap, and extends benchmark-visible simplification metrics with before/after waypoint counts, pruning counts, and simplification overhead.

### Recent completed tasks
- [x] Documented the current gameplay-facing async path request flow.
- [x] Documented the current tile/cell/layer mesh representation and sparse occupancy overlay.
- [x] Documented existing debug hooks and confirmed that a dedicated benchmark harness is still missing.
- [x] Added lightweight per-query timings and fine-search work counters to the shared sync traversal path.
- [x] Added structured `sv nav_query_stats` output for the most recent sync query snapshot.
- [x] Added deterministic `sv nav_bench_path` benchmarking for explicit long/open-area path queries.
- [x] Added internal hierarchy region/portal/container types without changing the gameplay-facing navigation API.
- [x] Added tile/leaf region membership storage plus hierarchy reset wiring for free/load/regenerate paths.
- [x] Added conservative canonical world-tile adjacency derived from persisted fine edge metadata.
- [x] Added deterministic Phase 3 static region partitioning plus rebuilt leaf-region memberships.
- [x] Added region validation and concise debug summaries for region count, average size, isolated regions, and oversized regions.
- [x] Added deterministic coarse region splitting so traversable inter-region seams exist for portal extraction.
- [x] Added merged initial portal extraction, representative portal geometry/cost estimates, and region-to-portal connectivity ranges.
- [x] Added portal graph validation plus concise region/portal graph summary diagnostics.
- [x] Added hierarchy-first coarse route building from canonical node region lookup plus region/portal A* .
- [x] Kept the initial hierarchy route behind `nav_hierarchy_route_enable` with fallback to the legacy tile-cluster route filter.
- [x] Extended structured sync-query and benchmark stats to report hierarchy-route usage separately from cluster fallback.
- [x] Added reusable LOS request/result types and a shared canonical-node LOS entry point.
- [x] Added nav-grid DDA LOS plus conservative BSP leaf-sampled LOS backends with `Auto` selection.
- [x] Added policy-aware, occupancy-aware LOS validation and wired LOS counters into sync query stats.
- [x] Added direct start-to-goal LOS shortcutting ahead of coarse routing and fine A*  in sync path queries.
- [x] Added async worker direct-path preparation plus main-thread commit without entering the normal A*  running state.
- [x] Reused existing LOS counters so direct-shortcut attempts and successes are visible in structured sync query stats.
- [x] Added an explicit refinement corridor representation carrying hierarchy region/portal paths or legacy cluster routes.
- [x] Updated sync and async fine-search initialization to consume the bounded corridor instead of only a raw tile-route vector.
- [x] Extended structured query stats and benchmark output with corridor usage plus region/portal/tile counts.
- [x] Added a shared LOS-based greedy path simplifier for finalized nav-center waypoint lists.
- [x] Applied post-refinement simplification to sync A*  output and replaced the older async string-pull block with the shared LOS simplifier.
- [x] Extended structured query stats and benchmark output with simplification attempt/success counters.
- [x] Formalized the sparse occupancy overlay behind a shared query helper used by LOS and corridor refinement.
- [x] Kept fine refinement on soft-cost-first and hard-block-second occupancy semantics while making LOS shortcuts fall back when occupancy is present.
- [x] Extended structured query stats and benchmark output with occupancy overlay participation counters.
- [x] Added local portal overlay storage plus clear/set/query helpers without mutating static portal records.
- [x] Updated hierarchy coarse routing to skip invalidated portals and apply local portal penalties when present.
- [x] Extended structured query stats and benchmark output with local portal overlay block and penalty counters.
- [x] Recorded hierarchy routing as the preferred coarse path in sync query stats and distinguished hierarchy fallback from older endpoint fallback behavior.
- [x] Extended structured query stats and benchmark output with hierarchy-preferred, hierarchy-fallback, and unrestricted-refinement fallback indicators.
- [x] Added a clearance-margin guard for long LOS collapses in narrow passages and made async simplification use an explicit revalidated candidate swap immediately before commit.
- [x] Added runtime tuning controls for direct LOS attempt budget, hierarchy-preference minimum distance, corridor widening thresholds/radii, and long-span simplification distance plus clearance margin.
- [x] Applied the new tuning gates across sync and async direct-shortcut flows and distance-aware hierarchy preference checks.
- [x] Extended structured query stats and benchmark output with buffered corridor radius and buffered corridor tile counts for refinement-scope tuning.
- [x] Reviewed the current gameplay-facing nav API surface and confirmed callers still interact through simple origin/agent/policy or path-process entry points.
- [x] Removed an unnecessary public direct-LOS tuning helper exposure and kept that logic internal to nav implementation files.
- [x] Confirmed that hierarchy, corridor, and overlay internals remain implementation details rather than required gameplay-facing concepts.
- [x] Added `NAV_HIERARCHY_ARCHITECTURE.md` describing the final static-first hierarchy, LOS, corridor refinement, overlays, and intentional non-goals.
- [x] Added contextual `.svg` explanatory visuals for hierarchy overview, corridor refinement, and dynamic overlays.
- [x] Preserved instrumentation and benchmark hooks while limiting final cleanup to narrow, already-touched nav/path scope.
- [x] Applied the practical sync simplification pass: duplicate removal, collinearity pruning, and a tightly budgeted LOS collapse pass after refinement.
- [x] Applied a more aggressive bounded async simplification search while preserving occupancy/clearance revalidation before swapping simplified output.
- [x] Extended structured stats and benchmark output with simplification input/output waypoint counts, duplicate/collinearity pruning counts, simplification ms, and fallback local-replan counts.

---

## 9. Quick Start Instruction for Copilot Agent

If you are a Copilot agent starting from this file:

1. Read this entire file first.
2. Start at Phase 0 unless prior completed work already exists.
3. Update the checklist as you complete tasks.
4. Keep all changes aligned with:
   - performance first,
   - precision second,
   - API stability,
   - static-first hierarchy,
   - overlay-based dynamic handling.
5. Do not introduce any forbidden architecture listed above.

---