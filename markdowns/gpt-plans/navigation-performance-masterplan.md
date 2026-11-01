# Monster navigation performance masterplan

Analysis date: 2026-10-01. Baseline: branch `draw2d`, commit `63b55baa`, with existing local changes. This is a source-based implementation plan, not a benchmark report. No speedup, race-free execution or gameplay result has been measured in this review. Unrelated local changes are outside scope.

## Decision

Keep authoritative movement, collision, entity state and crowd admission on the 40 Hz game thread. Introduce one event-driven navigation worker for immutable-data path searches and geometric refinement, after fixing cache ownership and query reentrancy. Reuse the same solver synchronously as a reference and fallback mode. Do not continuously spin the worker or accelerate simulation time.

First remove repeated work: lazy cache rebuilds, redundant face lookups, per-tick temporary allocations, duplicate searches and unnecessary crowd-wide scans. A worker can remove search work from the critical tick, but cannot eliminate physics, cover traces, crowd coordination, result validation or queue latency. Measure those separately before attributing a slow frame to A*.

Scope: compiled monster base/debug dummy, crowd/squad callers, cover selection, dynamic doors/walls, generation/load/clear/shutdown, debug queries and regression infrastructure. Separate build-time generation costs from recurring gameplay costs.

## Source findings

Paths in this table beginning with `nav/`, `entities/` or `crowd/` have prefix `src/baseq2rtxp/svgame/`. Line numbers identify the inspected baseline and will drift.

| Area | Source | Finding and consequence |
|---|---|---|
| Tick | `inc/shared/config.h:57`; `src/baseq2rtxp/svgame/svg_main.cpp:735,852` | Base rate is 40 Hz / 25 ms. `SVG_RunFrame` runs entities before crowd coordination. Completed paths need an explicit delivery point before entity thinking. |
| Compiled monsters | `CMakeSources.cmake:252` | Monster base and `svg_monster_testdummy_debug.cpp` compile. SFX-follow and puppet variants are commented out; the old reference dummy is not the active async implementation. |
| Live path pipeline | `entities/monster/svg_monster_base.cpp:297,537,551,586,1221,1416` | `MoveAStarToOrigin` calls synchronous `ComputePathTo`, then `StepMoveToGoal` / `ComputePathSteering`. Search includes face resolution, A*, funnel and corner refinement. Airborne and serialized-egress handling have separate early paths. |
| Existing threading | `nav/nav_thread.cpp:24,77,196`; `src/common/async.c:44,69,85` | The async job builds the mesh, KD tree, cover and regions. `Nav_ProcessPathQuerySliced` loops until its budget without advancing A*: it is a stub, not a functioning asynchronous path service. |
| A* storage | `nav/nav_path.cpp:577-615,652-674` | Vector-backed priority queue and flat per-face score/predecessor/stamp array already retain capacity. Preserve this useful optimization, but remove shared mutable ownership. |
| Duplicate expansion | `nav/nav_path.cpp:678-787` | Heap entries carry face/priority but no queued g-score/version check at pop. Instrument redundant expansions; discard stale entries while retaining valid reopen behavior. |
| Lazy obstacle cache | `nav/nav_path.cpp:1023-1312` | Validity uses XOR of container sizes and nonempty corner output. A valid empty corner set rebuilds on subsequent calls. Equal-size replacement geometry can reuse stale data. Solid-edge pairing is quadratic, followed by linear corner deduplication; its O(E) comment is inaccurate. Boundary classification calls `SVG_Trace` at 1091. |
| Face query mutation | `nav/nav_path.cpp:341-476`; `nav/nav_types.h:270` | Strict/fallback lookup have independent counters but write the same mutable `face.last_query_id`. Equal tokens can skip valid candidates even serially; concurrency adds races. Contiguous leaf ranges do not visibly require deduplication. |
| Global lookup fallback | `nav/nav_path.cpp:291,341,2173`; `nav/nav_generate.cpp:4002` | Failed containment can scan all faces. Room/zone queries resolve faces again. Actual cost is tree depth plus candidate polygon tests, or a global scan; it is not universally O(1). |
| Funnel/refinement storage | `nav/nav_path.cpp:2303-2312,2323,2969-2986,3002` | Many shared retained vectors, repeated spatial queries and corner-by-waypoint/segment loops. Refinement and sanitization copy entire vectors even when capacity prevents allocation. |
| Monster replan policy | `entities/monster/svg_monster_base.cpp:322,506-571,1555` | A 48-unit displacement threshold and cooldowns exist, but moved targets/force bypass cooldown. `navPath` is cleared before the cost callback reads it, defeating old-corridor hysteresis during normal replanning. Funnel return value is ignored before reporting a new path. |
| Dynamic topology | `nav/nav_path.cpp:3482`; `nav/nav_generate.cpp:560`; `nav/nav_persistence.cpp:91` | Edge updates walk the registry and then unconditionally scan all edges. They do not invalidate the size-based obstacle cache. Clear/load also need explicit cache/path-generation invalidation. |
| Crowd tick allocation | `crowd/svg_crowd_manager.cpp:4323-4362` | Each moving group constructs/fills a fresh local member vector per tick. Its capacity is not retained between calls. |
| Crowd neighbors | `crowd/svg_crowd_manager.cpp:2071-2419` | Following-speed logic scans the group per member. Constant work per pair remains O(M²) across M members. Repeated face/waypoint queries add cost. |
| Formation work | `crowd/svg_crowd_manager.cpp:4029,4122`; `crowd/svg_crowd_formations.cpp:3118-3530` | Death compaction has nested scans despite its O(M) comment. Moving-target 2-opt performs bounded iterations of pair scans; static/circle/column/defensive modes already bypass it. Initial ingress assignment includes a dense matrix and Hungarian-style solver: distinguish order-time spikes from per-tick work. |
| Cover | `nav/nav_cover_query.cpp:144-155,487,611-790`; `entities/monster/svg_monster_testdummy_debug.cpp:1500-1520` | Candidate buffers already persist. Searches can retry at larger radii, sort and perform multiple engine traces. Claims/cooldowns are live gameplay state. Moving A* does not remove this cost. |
| Sector graph | `nav/nav_sector_graph.cpp:35-151`; `crowd/svg_squad_coordinator.cpp:467` | Three dense matrices use about 16 N² bytes of payload with O(N³) Floyd-Warshall build work. Indexed queries are cheap; position resolution is not free. The source's sub-millisecond claim is not a measurement. |
| Custom container | `nav/nav_containers.h` | `nav_vector_t` uses tag allocation, memcpy relocation/copy, no per-element destruction on clear, and capacity-preserving clear. It is not a general replacement for `std::vector<T>` containing owning/nontrivial elements. Copy also allocates source capacity. |
| General async queue | `src/common/async.c:35-100` | One shared worker, allocated/copied work items, linear linked-list append and completion callbacks under the queue mutex. A high-rate path stream would compete with unrelated work and can add main-thread contention. |
| Generation lifetime | `nav/nav_thread.cpp`; `nav/nav_generate.cpp:560`; `entities/monster/svg_monster_base.cpp:248` | Generation mutates shared mesh globals. Progress fields have unsynchronized reads/writes. The monster guard checks only empty containers. A path worker requires publication and lifetime rules beyond a lock around A*. |

## Memory and cache locality

A local vector normally places its small control object on the stack and its elements on the heap. Moving the object to file scope does not intrinsically improve locality. It retains capacity but introduces shared ownership and non-reentrancy. There is no measured large-stack-frame or stack-overflow finding here; obtain compiler stack reports for the actual call chain before changing storage for that reason.

Use three explicit ownership domains:

1. Published mesh snapshot: contiguous vertices, faces, adjacency, portals, static obstacle/corner data, room metadata and spatial index; immutable for readers. Split hot expansion fields from cold generation/debug fields only when profiles justify it.
2. Query context: one main-thread context and one worker context own A* nodes/heap, stamps, portal/corner scratch and refinement buffers. An in-flight resumable search must have exclusive context state. Do not allocate a full face-state array per monster.
3. Committed monster routes and bounded result slots: old routes survive pending replans; results transfer exclusive buffer ownership at a tick boundary. No pointers into scratch or containers another query may resize. Recycle released result buffers.

Retain capacity, reserve from measured high-water marks, and count growth events and retained bytes. Avoid worst-case reserves per monster. Budget snapshot bytes + still-live old snapshot bytes + context count × context capacity + committed route capacities + bounded request/result storage. Use actual `sizeof`, capacities and allocator overhead, not assumed ABI sizes.

Benchmark packed immutable grids: sorted cell keys with `(offset,count)` ranges and a contiguous ID array, optionally indexed by a compact hash. Preserve exact cell identity, negative coordinates, deduplication and Z filtering. Average O(1) hash lookup does not make an entire multi-cell query O(1). Byte flags may simplify `vector<bool>` access, but must earn their larger footprint in measurement.

Use two retained buffers with alternating roles for refinement passes to reduce copies. Do not blindly swap a monster's route with shared scratch. Restrict `nav_vector_t` to suitably trivial types with enforced contracts, or implement correct lifetime semantics before expanding its use. Do not assume engine tag allocation/free is thread-safe.

## Milestones and status

DONE means completed in this analysis. TODO means implementation not started. BLOCKED identifies a missing prerequisite. DEFERRED means deliberately excluded until evidence warrants it. Update these statuses in dependency order during implementation.

- [x] M0 — DONE: source/caller/storage/threading audit and this plan.
- [ ] M1 — TODO: measurement and deterministic fixtures. Automated ingress regression is BLOCKED by the absent current `scripts/test_crowd_ingress.ps1`; recreate it in M1.
- [ ] M2 — TODO, after M1: synchronous correctness and repeated-work fixes.
- [ ] M3 — TODO, after M2: immutable snapshots and exclusive query contexts; synchronous parity.
- [ ] M4 — TODO, after M3: bounded dedicated worker, cancellation and lifecycle.
- [ ] M5 — TODO, after M4: compiled monster/crowd integration and behavior proof.
- [ ] M6 — TODO, after M5: benchmarks, tuning and default-enable decision.
- [ ] DEFERRED: extra workers, SIMD/precision reduction, general allocator replacement, all-agent flow fields and hierarchical A*. Preserve double precision and existing geometric invariants first.

### M1: instrumentation and reproducible baseline

Measure high-resolution elapsed time for total server tick, navigation orchestration, start/goal lookup, global fallbacks, cache build, A*, portal/funnel construction, corner refinement, geometric LOS, movement traces, cover scoring/traces, crowd/squad work and debug drawing. Report p50/p95/p99/max, sample count and ticks exceeding 25 ms. Use thread CPU time where available to separate compute from waits. Separate generation/load/first query from warm gameplay.

Count request reasons (new/moved goal, force, blocked, topology change), reuse, failures, expansions, stale heap pops, peak heap, spatial cells/candidates, global face scans, copied bytes, allocations/reallocations and retained capacity. Include STL and tag allocator activity. Aggregate per thread/request; no per-query console spam or mutex on every expansion.

Add proposed commands `sv nav_perf_reset`, `sv nav_perf_report` and deterministic coordinate/fixture-driven `sv nav_bench_path`. They do not exist yet: the commented stats/bench dispatcher entries are not working commands. Emit machine-readable request/entity/state IDs, build ID, map, mesh revision and timing.

Exercise 1, 10, 32, 64 and 128 compiled debug dummies where map capacity permits: distributed goals, simultaneous orders, moving targets, unreachable targets and stable cached routes. Fix seed/spawns/goals. Warm for 10 seconds, sample at least 60 seconds, repeat five times; report distributions. Record CPU, build configuration, profiler overhead and debug-draw settings. Use Release/RelWithDebInfo for timing, Debug for assertions.

Gate: reproducible logs and allocation counts; test harness rejects an intentionally invalid endpoint, duplicate slot and stalled member. No speedup claim before a baseline exists.

### M2: correctness and repeated-work fixes

1. Replace size fingerprints/nonempty-output validity with mesh generation, obstacle/topology revisions and a valid flag. Valid empty caches remain valid. Load/clear/regeneration/dynamic changes invalidate affected data. Build before publication; do not trigger full rebuild inside search or LOS. Stage expensive rebuilds while retaining ownership of the old snapshot.
2. Remove leaf mailboxing if contiguous traversal already visits each face once; otherwise use context-local stamps and tested wrap handling. Test strict/fallback lookup interleaving on the same leaf.
3. Replace all-pairs edge matching with endpoint incidence plus spatial neighboring-endpoint buckets. Preserve exact XY/Z tolerances and wall/drop-off types. Spatially deduplicate corners. Expected cost depends on local density/output; coincident dense geometry can remain quadratic. Compare corner sets and routes against fixtures.
4. Resolve the current face once per movement update for zone/corridor/teammate checks. Store waypoint face IDs with revision. Use squared distances for ordering/thresholds. Preserve containment, stacked-floor Z checks and seam recovery; try bounded neighboring leaves before whole-mesh fallback.
5. Build replacement routes without clearing the committed route. Snapshot corridor membership into query-local indexed marks: O(path length) setup and O(1) membership per edge. Restoring the currently ineffective hysteresis is a behavioral change and needs its own test. Check funnel success before committing.
6. Add stale heap-entry rejection and deterministic tie-breaking. Validate finite nonnegative costs. Compare against Dijkstra on small graphs: the slope-scaled heuristic and possible 0.85 discount do not establish admissibility. Use a proven lower bound or explicitly document weighted-search tradeoffs; do not promise shortest paths without validation.
7. Make entity-to-edge registration complete and generation-tagged. Use whole-edge fallback only when registry data is absent/invalid. Coalesce state changes; update twins, obstacle data and sector passability consistently.
8. Reuse group-member scratch or iterate stable entity handles. Cache per-tick neighbor data and use a spatial neighbor index for following/separation while preserving exact queue predecessor/reservation rules. Typical complexity becomes O(M + candidate pairs), still O(M²) for dense cases. Use slot-to-member indexing and death-triggered compaction; retain existing 2-opt bypasses and budget moving-target optimization.
9. Separate cover geometry from main-thread claims/traces. Avoid identical repeated radius searches for unchanged inputs; keep trace candidates capped and budgeted. Preserve threat/engagement LOS semantics.

Gate: no warm cache rebuild for unchanged or zero-corner meshes; query interleaving, endpoints and forced points pass; zero warm solver scratch allocations within reserved capacity; no per-tick member-vector allocation. Cold/oversized growth is bounded and reported.

### M3: solver inputs and ownership

Refactor face resolution, `Nav_FindPath`, portal extraction, `Nav_StringPull`, LOS and refinement to take an immutable mesh view and explicit context. Keep main-thread wrappers temporarily. Diagnostics belong to each request, not the global last-query record.

Request payload: start/goal feet positions, hull/radius/shape, step/drop policy, immutable cost-policy inputs, prior-corridor preference, request sequence, entity slot AND incarnation, goal/state revision, crowd assignment revision, mesh generation and topology revision. Never send `this`, `goalentity`, raw monster callbacks or pointers to live vectors. Convert `OnNavEvaluateEdgeCost` to a pure snapshot policy; future overrides need a snapshot representation or explicit synchronous handling.

Precompute trace-dependent boundary classification on the main thread in bounded work, or use separately audited immutable collision inputs. Initial worker code must not call `SVG_Trace`, `SVG_MMove_Trace`, entity APIs, logging imports or unverified engine allocators. Pure funnel/refinement geometry can move after lazy cache dependencies are removed.

Generation currently writes globals: build private output and publish only when complete. Generation itself needs immutable collision/geometry input or a proven engine lifetime barrier. Publish progress through a synchronized copied snapshot. Until conversion is complete, prevent regeneration/load/clear overlapping query readers and drain/cancel at controlled boundaries. Locking only A* does not protect steering, cache helpers or generation.

Gate: corrected synchronous and context-based solvers agree on validity, endpoints, mandatory waypoints and costs. Audit the complete worker call graph. Two independent contexts can query one snapshot without shared writes.

### M4: bounded worker and lifecycle

Use one persistent dedicated worker, a condition variable/event, normal scheduling priority and bounded request/result storage. Wake for useful work, sleep when idle. Do not busy-spin, request real-time priority or pin a CPU without measurement. Do not stream paths through the existing general async queue unchanged; its sharing, allocations, list append and callbacks under lock are unsuitable for this workload. Avoid an unrelated engine-wide queue rewrite.

The main thread schedules and applies results. Coalesce to one latest queued request per entity, with at most one executing request; newer goals cancel obsolete work cooperatively. Priority tiers: no safe route/invalid route, urgent stalled replan, normal pursuit, optional refresh. Add age promotion and group fairness. Force means scheduling urgency, not unlimited synchronous work.

Bound slot count, bytes and per-request work. Check cancellation at bounded intervals in lookup, A*, funnel and refinement. A search exhausting its quantum resumes with exclusive state, defers, or returns a distinct resource-limit status; it must not claim unreachable. Cap resumable contexts to avoid multiplying full face arrays. Saturation preserves a safe old route or holds/retries with backoff; never fall back to unlimited synchronous search for every dropped request.

Results own paths, waypoints/flags and diagnostics, tagged with identity/revisions/status. Publish using acquire/release synchronization or a short mutex; no lock during solve or commit. Drain before entity thinking under a bounded application budget. Batch new requests collected during the tick and wake at its end, with immediate wake available for urgent submissions. Record snapshot/enqueue/start/finish/commit timestamps.

Commit validation checks entity incarnation/aliveness, goal/state/crowd revisions, mesh/topology versions, hull policy and current-start compatibility. Reject obsolete results without disturbing newer routes. Initially reject any topology-version mismatch; optimize to edge-specific checks only if discard rates justify it. Attach to current position through bounded containment/segment and main-thread collision checks; resubmit if too far from the old start. Never teleport to the request origin. Preserve exact endpoints and forced points.

Death/removal/teleport/reset/state transition/crowd reassignment increment revisions and cancel requests. Load/clear/rebuild close submission, cancel old work and retain snapshots until readers release them. Shutdown/module unload must join or prove worker callbacks gone BEFORE freeing code/entities/tag memory/collision data. Level setup starts clean. Test repeated start/stop and error paths.

Gate: saturation does not cause unbounded memory or tick blocking; idle CPU is negligible; slot reuse cannot receive stale results; generation/shutdown stress passes; worker engine-call audit passes. Record lock time, cancellation latency and old-snapshot peak memory.

### M5: monster and crowd integration

Expose idle, pending, ready, no-route, stale/cancelled and resource-limited states. Pending is neither failure nor arrival. Continue a valid committed route while waiting. Without one, safely hold or use bounded collision-validated direct movement. Keep steering, waypoint callbacks, floor support, velocity/animation and `GenericThinkBegin/Finish` on the game thread.

| Compiled behavior | Debug dummy entry point / move site | Required test |
|---|---|---|
| Visible pursuit | `onThink_AStarToPlayer`, line 1158 | Target circles a pillar and repeatedly crosses replan threshold; newest goal wins, requests coalesce and routes are not repeatedly destroyed. |
| Trail pursuit | `onThink_AStarPursuitTrail`, 1226 | Lost sight during pending request; prior pursuit cannot overwrite trail/idle goal. |
| Sound | `onThink_InvestigateSound`, 1315 | New sound or visible target supersedes investigation without stale commit. |
| Cover/flee | `onThink_HideInCover`, forced moves 1665/1752, regular 1887 | Claim/cooldown/threat changes during request; no stale assignment, double claim or force burst. Include no-cover fleeing. |
| Formation | `onThink_CrowdFormation`, 2271 | Staging-to-final revisions, serialized ingress/egress, member death, leader protection and reassignment while pending. |
| Idle/death | reset sites and `onThink_Dead`, 2339 | Late result cannot restore cleared route or move dead/reused entity. |
| Shared movement | base `MoveAStarToOrigin`, blocked recovery | Airborne, steps, ramps, trapped recovery and cached following remain responsive with delayed results. |

Crowd guide paths also call A* during move/follow construction, including `svg_crowd_manager.cpp:3526`. Route them through the same context contract. If asynchronous, tag complete group orders with revisions and keep the previous order active until replacement is ready. Keep formation assignment synchronous initially under separate measurement; offload pure assignment geometry later only if order-time spikes justify it. Debug queries need their own main-thread context.

Gate: each row has a runnable scenario and asserted final behavior. Callers cannot silently require same-call completion. SFX-follow, puppet and reference variants are intentionally excluded unless re-enabled in the build.

### M6: acceptance and rollout

Compare captured original baseline, optimized synchronous solver and optimized worker solver on identical fixtures. Separate correctness changes from scheduling gains. Proposed initial targets, to calibrate against M1 hardware/workload:

- At agreed 64-agent load, warm main-thread orchestration plus commit p99 <= 2 ms. Report steering/physics/crowd/cover separately AND in total.
- Total game tick p99 < 25 ms, publishing maximum and deadline misses. Attribute any remaining non-navigation bottleneck rather than declaring success from A* timing alone.
- Under supported steady load, request-to-commit p95 <= 50 ms and p99 <= 100 ms. Bursts recover without starvation; report oldest age, throughput and drops. These are targets, not arbitrary-load guarantees.
- Zero warm solver scratch allocations within capacity; bounded queue count/bytes; no retained-memory growth across 100 cancel/reset/map cycles.
- Geometry, policy and monster/crowd tests pass in both modes; no increased stalls, illegal portal crossings, final-slot overlap or route oscillation.

Latency = enqueue delay + queue wait + solve time + tick-boundary wait + commit-budget delay. Faster computation does not make movement run faster than 40 Hz. Keep request arrival below sustainable service rate after coalescing; otherwise shed optional refresh work and retain fairness.

Default-enable only after gates pass. Retain runtime synchronous diagnosis using the same corrected solver. Mode switches drain/cancel safely. Record actual timings, memory, regressions and remaining limits here.

## Regression protocol

### Existing commands and known scenario

Current commands: `sv nav_generate`, `sv nav_status`, `sv nav_load <filename>`, `sv nav_save <filename>`, `sv nav_clear`, `sv nav_dbg_goal_a`, `sv nav_dbg_goal_b`, `sv nav_dbg_test`, `sv crowd_status <id>`. Debug endpoints sample the player, so they are interactive checks rather than a sufficient headless fixture.

For the known ingress scenario: load `q2rtxp_dev_nav01`, generate/load the matching navmesh and wait for completion. Confirm crowd 2 contains the intended 10 live members with status; otherwise the fixture must explicitly spawn/register them. Then:

```text
sv crowd_move 2 -331.6 -1504.4 24 circle
sv crowd_status 2
```

Capture status until completion or timeout. Coordinates/setup came from a prior successful regression, not a rerun on this branch. Current output uses `inHead`, `egHead`, `inRel`, `egRel`; restore the parser against those fields, not historical `head`/`released`. `scripts/test_crowd_ingress.ps1` is currently absent.

Recreate the checker in M1. Validate 10 arrivals, `moving=0`, `ingress=0`, `inHead=10/10`, unique final slots, actual feet-to-final-slot error, floor support and pairwise hull separation. Log immutable final slots explicitly because assigned goals can be temporary staging targets. Use hull-derived tolerances. Historical 7.70-unit maximum seat error and 34.34-unit minimum separation were observations, not universal thresholds. Independently inspect animation, corner cutting and jitter visually.

### Fixture matrix to implement

| Fixture | Assertions |
|---|---|
| No mesh, empty path, same face | No busy loop; defined pending/failure; successful exact endpoints. |
| Zero corners; equal-size geometry reload | One build per revision; no stale/repeated warm rebuild. |
| Strict/fallback interleaving and token wrap | Face result independent of call order; no shared writes. |
| Open plane, narrow door, U-turn, acute corner, sliver | Clearance/mandatory points preserved; no introduced hairpins or displaced endpoints. |
| Stairs, ramp, allowed/forbidden drop, stacked floors | Correct elevation and policy. Component rejection must be conservative for all supported policies: a default-drop cache cannot reject a more permissive valid route. |
| Door, rotating door, wall, areaportal changes | Closing invalidates unsafe results; opening restores reachability; boundary/sector state matches edges. |
| Circle ingress and egress | All final slots occupied uniquely; adjacent vacancies only; no overtaking/deadlock. Strict ingress steering applies only to admitted members. |
| Moving formation, death, protected leader | Reassignment invalidates pending result; leader preserved; compaction terminates. |
| Delayed/reordered completion and entity reuse | Inject 1/4/20-tick delays and reverse result order in test transport; only current revisions commit. |
| Saturation/unreachable goals | Bounded memory, fairness and backoff; no unlimited synchronous escape; distinguish no-route/resource limit. |
| Save/load/clear/regenerate/shutdown while busy | Coherent saved/published snapshot; no partial reads, use-after-free or late module callback. |
| Debug/cover queries during worker activity | Independent scratch/diagnostics; claims/traces stay on main thread. |

Use small graph/unit fixtures for cache/search/lifecycle contracts and engine scenarios for monsters/physics/formations. Use assertion-enabled stress and race detection where supported, otherwise concurrency instrumentation and available sanitizers. No crash is not proof of race freedom.

## Review boundaries

Expected files: `nav_path.*`, `nav_types.h`, `nav_thread.*`, `nav_generate.*`, persistence/cover/sector files, monster base/debug files, crowd manager/formations/squad files, `svg_main.cpp`, `svg_commands_server.cpp`, CMake lists and new tests. Avoid broad allocator/generation rewrites in the first performance patch.

Each milestone is independently reviewable and updates this plan. Follow the supplied AGENTS.md: K&R braces, tabs, fixed-width types where applicable, `nullptr`, const/constexpr, `Q_assert`, tabbed Doxygen and intent comments throughout touched functions. C++ PRs include the mandatory per-file annotated commenting checklist. Preserve exact endpoints, native hull behavior, admission-only forced steering and final-slot/non-overlap tests throughout.
