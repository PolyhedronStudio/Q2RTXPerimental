# M7 and M8 final acceptance - 2026-10-05

**Current outcome: M7 correctness checkpoint retained; M8 REOPENED for reported single-agent rendered FPS regression.** Previous dedicated timing/memory acceptance remains version-specific. Source correction and same-map measurements: [single-agent investigation](single-agent-clearance-regression-2026-10-05.md). User gameplay verification is pending.

**Historical v53 outcome: M7 DONE; M8 dedicated acceptance with a conservative default-enable decision.** The corrected synchronous solver remains the default (`nav_actor_worker=0`). The bounded worker is available through explicit opt-in; current-goal native validation and safe mode-switch retirement remain mandatory.

## Evidence and version boundary

- v52: all 21 frozen native gates pass; 113 inputs remain unchanged at every boundary. Full synchronous/worker matrices each pass 428/428; reverse 20-tick C7/C11 stair/retarget tests pass 288/288. Five crowd modes pass all ten ingress/egress phases. Compiled behavior, actor/crowd lifetime, real pool reuse, saturation, busy transport and identity controls pass.
- v53: the only subsequent source change encloses existing input preparation, reply drain/copy/native delivery and publication in owner timing scopes. Call order and movement/ownership are unchanged. Both configurations link; CTest passes 6/6, including 120 actor contract checks and nested owner timing checks.
- All six v53 targeted native gates pass with 115 inputs frozen: delayed compiled behavior 14/14, actor lifetime, ordinary worker crowd 2/2, both strict optimized benchmarks and 100 exact optimized memory cycles.
- Native reference reuse proves entity 46 changes spawn_count 0 to 1: old leader/target resolution fails until explicit rebinding. Benchmark references additionally check entity number, spawn_count, navigation incarnation and actual live pool pointer.
- Version manifests and progress: `out/build/m7-m8-final-v52-inputs.json`, `out/build/m7-m8-final-v52-progress.json`, `out/build/m8-final-v53-inputs.json`, `out/build/m8-final-v53-progress.json`. Earlier failures remain retained as historical evidence.

## Matched optimized timing

Hardware: Intel i5-10400F, 6 cores/12 logical processors, Windows 10 Pro N build 19045, 42,812,583,936 bytes RAM. MSVC x64 RelWithDebInfo; IPO and edit-and-continue off. Dedicated private process, fixedtime=0/timescale=1, one owned engine at a time.

Each mode uses the same 64 real capsule actors, radius 16, in clear parallel straight lanes on m4_corners. Burst: 160 ticks; warmup: 400 ticks; measured steady: 2,000 ticks (50 simulation seconds). One real sound stimulus per owner tick means 40 stimuli/s; actual query arrivals are reported separately. Native physics and compiled handlers execute normally.

Owner distributions include preparation, transport drain/copy, native completion and publication. Nested commit intervals count once in the paired per-tick aggregate. Steering, physics, crowd and cover retain separate phase rows; total tick includes measured gameplay work. Formatting/reports run after the tick is sealed.

| Metric | Synchronous | Worker | Target |
|---|---:|---:|---:|
| Owner navigation mean | 0.3449 ms | 0.3666 ms | Reported |
| Owner navigation p99 | 0.8296 ms | 0.6710 ms | <= 2 ms |
| Total tick mean | 3.8539 ms | 3.2008 ms | Reported |
| Total tick p99 | 9.2862 ms | 5.0441 ms | < 25 ms |
| Total tick maximum | 12.4208 ms | 10.1541 ms | Reported |
| Worker request-to-native-commit p95 | Not a worker population | 30.9638 ms | <= 50 ms |
| Worker request-to-native-commit p99 | Not a worker population | 31.4987 ms | <= 100 ms |

All absolute timing gates pass in the final strict runs. No sums of per-call percentiles are used. The original M1 baseline did not capture this 64-actor timing workload: historical baseline-relative speedup is intentionally not claimed or reconstructed from a different build.

## Progress, recovery and service capacity

- All 64 exact lifetimes receive real native commits in both burst and measured steady phases in both modes. Worker steady installs: 2014; invalid timestamp samples and missing owner tick pairs: zero.
- Measured worker submissions/starts/completions/drains: 2472 each, 49.44/simulation second. This differs from the explicit 40 stimuli/s because normal behavior also refreshes routes.
- Service-reported retained storage: 3,153,016 bytes against a 4,194,304-byte service ceiling; peak occupied slots 63/64. Saturation, resource limits and solver errors: zero.
- End-point queued population/oldest queued age: 0/0 ns. Maximum serviced queue age: 8.3300 ms; this maximum is cumulative through the run, not a reset steady-only statistic.
- Visible steady recoveries: 458 corridor detachments; 0 goal supersessions. No-route, stale ownership, cancellation and resource-limited owner terminations: zero.
- Detachment is counted only at the native failed-attachment site after exact request identity and physical/tactical policy checks. Actor state remains Stale, the old reply is rejected and existing bounded retry continues. Counts must match the native site; reused owners/policy failures cannot use this accounting category. Goal-only supersession likewise cannot hide another ownership/policy mismatch.
- Successful latency distributions exclude safely rejected/superseded requests; their separate counts remain visible. Per-actor native progress and the independent delayed/ownership/geometry/crowd gates are retained.
- Production solver/transport allocation instrumentation in the standalone suites verifies warm within-capacity operations allocate/free no graph buffers. This is a solver/transport guarantee, not a claim of zero allocation by arbitrary game code.

## Exact map/reset/cancel retirement

100 optimized cycles pass Pending interruption, reader draining, distinct actor incarnations, frozen native inputs and exact stable active byte/block populations. The earlier Debug 100-cycle gate also passes (`out/build/nav-memory-cycles/m8-debug-exact-memory100/summary.json`).

| Engine tag | First sample bytes / blocks | Last sample bytes / blocks |
|---|---:|---:|
| cmodel | 22,384 / 18 | 22,384 / 18 |
| svgame | 132,480 / 2 | 132,480 / 2 |
| svgame_navmesh | 101,249,525 / 28 | 101,249,525 / 28 |
| svgame_level | 720,645 / 16,527 | 720,645 / 16,527 |
| svgame_edicts | 18,698,664 / 8,193 | 18,698,664 / 8,193 |
| svgame_lua | 0 / 0 | 0 / 0 |

Custom allocator active populations are checked independently at every matching quiescent boundary. Collision dictionaries are freed per map, preserving shared BSP ownership; entity/client owners retire before old level storage is freed. Fresh-slot/world-placeholder replacement preserves spawn_count. Client destruction uses the actual constructed allocation count.

## Rollout decision and explicit limits

**Keep nav_actor_worker=0 by default.** All measured absolute gates pass. The final pair has lower worker tails but a slightly higher worker mean orchestration cost; earlier paired diagnostics had a different tail ordering. One clear-lane workload does not establish a repeatable general default-enable benefit. Explicit worker opt-in and synchronous diagnosis remain supported, and safe mode-switch/clear/regenerate/map retirement is verified.

Before changing the default, require repeatable timing on representative complex routes and crowd/cover workloads. This is an intentional rollout deferral rather than an unperformed correctness gate. Extra workers, process-wide heap/concurrent zone redesign, SIMD, flow fields and hierarchical search remain outside this implementation.

The memory result covers the recorded engine/custom tags and bounded service storage; it does not cover all CRT/process/GPU allocations. Current work does not add a visual animation claim or a proof that every possible map, policy or future change is bug-free. M4 visual observations and v45 broad scheduling repetitions remain their own version-specific evidence.

Final artifacts: `out/build/nav-performance-load/m8-final-v53-optimized-sync/summary.json`, `out/build/nav-performance-load/m8-final-v53-optimized-worker/summary.json`, `out/build/nav-memory-cycles/m8-final-v53-optimized-memory100/summary.json`, `out/build/m8-final-v53-runner.log`, `out/build/m8-final-owner-timing-build-v53.log`, `out/build/m8-optimized-build-v53.log`.
