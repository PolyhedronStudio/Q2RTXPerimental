# Single-agent clearance regression investigation - 2026-10-05

Status: source correction rebuilt; rendered FPS recovery awaits user gameplay verification. M8 remains reopened.

## Findings

The owner strict-clearance wrapper allocated and zeroed a boundary-sized visitation array for every call, although the strict linear scan never accessed it. The strict scan evaluated expensive exact segment distances against every relevant-height boundary, including remote walls. Remove only the unused allocation and conservatively reject exact distance calculations that cannot improve the nearest witness. Preserve finite-input checks, elevation classification, exact nearest-witness ordering, ties, endpoints, spawn-count identity, native hull/support and final-command validation.

The lower bound is XY axis-aligned box separation. It is used only within +/-32768 coordinate bounds with a 0.0001 squared-distance rounding cushion; larger coordinates fall through to the original evaluator. It does not change precision or authorizations. All input coordinates are checked finite before this helper is called.

## Measured evidence and limits

The first initializer-list pruning attempt was rejected: Debug synthetic exhaustive 8757 ms versus pruned 15786 ms despite exact parity. The retained scalar version gives exhaustive 8853 ms versus pruned 1153 ms for 2048 queries over 8195 edges. Exact distance and first-witness index agree for every query; 19241 exact evaluations replace 16783360 evaluations. This is a synthetic geometry measurement, not game FPS.

The real-time dedicated workload uses q2rtxp_dev_nav01, entity 46, an idle control of 400 ticks and 1200 ticks of twelve alternating east-bent-stair legs. Each navigating run has 36 actual graph searches and 590 steering samples. The Debug reference restores only the original exhaustive clearance scan and unused array allocation, retaining all other current source changes. It is isolated under out/build/single-agent-debug-reference. The optimized reference is the frozen pre-patch v53 binary. Engine/module/map/config hashes and actor lifetime snapshots are retained in each summary.

| Build | Measurement | Before mean / p99 (us) | After mean / p99 (us) |
| --- | --- | --- | --- |
| Debug | Actor Steering & Animation | 754.98 / 1701.00 | 648.12 / 1159.60 |
| Debug | Portal & Funnel | 8720.94 / 13854.00 | 7648.48 / 9597.20 |
| Debug | Server Tick (Total) | 2842.72 / 5887.20 | 2702.48 / 6027.60 |
| Optimized | Actor Steering & Animation | 82.43 / 184.80 | 43.10 / 79.30 |
| Optimized | Portal & Funnel | 1855.24 / 2772.70 | 1672.01 / 2064.10 |
| Optimized | Server Tick (Total) | 1042.76 / 1946.80 | 923.75 / 2580.30 |

Total tick p99 does not consistently improve; idle-control timing also varies. These measurements establish reduced local steering/refinement cost, not recovery from 76 to 110 rendered FPS. A* is small on this workload; moving-target/cover and the user renderer workload remain unmeasured. No worker-default change is made.

## Validation and handoff

- Actual project-root Debug DLL links succeed; final source restored and linked after the isolated reference build. Optimized module is linked under out/build/m8-module; the root normal release binary/module are not replaced.
- Standalone CTest 7/7 passes on the scalar version, including exact nearest-witness parity and existing movement/identity/worker contracts.
- The earlier initializer-list v54 implementation passes 428/428 native synchronous corner/stair/retarget cases. That report is version-specific; the final scalar v55 change is covered by exact helper parity, not a falsely claimed fresh native matrix.
- Each of the four same-map timing runs completes with frozen native inputs and twelve actual actor snapshots. The first baseline parser expected the wrong snapshot label; retained raw logs prove all twelve actual snapshots and the corrected artifact retains that original parser error.
- Further automated matrices, repetition and rendered verification are intentionally skipped at the user request to test gameplay personally. Native checks are not disabled. M8 remains pending that observation.

Artifacts:
- debug_before: out/build/nav-single-agent-performance/single-agent-v55-debug-sync-reference/summary.json
- debug_after: out/build/nav-single-agent-performance/single-agent-v55-debug-sync-after/summary.json
- optimized_before: out/build/nav-single-agent-performance/single-agent-v53-optimized-sync-before/summary.json
- optimized_after: out/build/nav-single-agent-performance/single-agent-v55-optimized-sync-after/summary.json
