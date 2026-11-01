# M6 final acceptance - 2026-10-04

Disposition: **M6 DONE**. The dedicated bounded copied-graph service, execution controls, owner frame/lifetime hooks and measured reader budgets are implemented and verified. Gameplay routing remains synchronous. Actor scheduling, native route consumption/commit and delayed-result scenarios remain M7; optimized 64-agent acceptance and default activation remain M8. This report accepts the M6 transport boundary, not completion of the entire masterplan.

## Resulting implementation

One persistent event-driven worker serves a fixed pool with FREE/QUEUED/RUNNING/READY ownership, one exclusive graph context, full copied identities and exact endpoints. It coalesces queued requests, cancels executing obsolete work cooperatively, retains ready storage until owner release, rejects obsolete slot identities and enforces aged priority/group fairness. Caller saturation never creates an overflow queue or an unlimited synchronous escape. Empty tick publications leave the idle worker asleep.

Owner preparation reserves all buffers before thread launch and checks independent retained byte ceilings. Worker queries never allocate/free graph buffers or call engine/entity/collision/logging imports. Every terminal graph reply owns its corridor, endpoints, identity, counters and explicit resource-limit reason. Work/deadline/cancellation checkpoints cover stale pops, directed edges, stamp clear, reconstruction and reversal, discarding partial output. Native M4 semantic route/proof construction remains in the existing M5 owner phase and will be connected during M7.

Frame completion consumption precedes entity thinking; batch publication follows entity/crowd work. Retirement/epoch reset join copied readers before copied publication reuse. The existing engine lifetime export combines query and generation drains before BSP replacement; clear/load/generation/map/module shutdown reclaim reader storage only after the applicable join/pin barrier. Ordinary client callbacks preserve the separate legacy generation exclusion. [The ownership audit](m6-worker-ownership-2026-10-04.md) describes the exact call graph and ordering.

## Verified final bundle

| Gate | Result |
| --- | --- |
| Engine/servergame build and CTest | **4/4 PASS** |
| Pure core | **8,093** checks, including **4,000** concurrent context solves with buffer allocation/free forbidden |
| Worker contracts | **2,322** checks; **100** retained-capacity restart cycles; saturation, coalescing, queued/running cancellation, stale-ready reuse, group/age fairness, work/deadline/storage limits and interrupted stamp rollover |
| Running cancellation | Actual cancellation required; observed solver finish response **29.3us**, excluding publication-lock and join scheduling |
| Isolated idle process | **0.0ms measured CPU** over **1,010.94ms**; no useful wake/busy growth |
| Native geometry and stairs with graph worker activity | **428/428 PASS**, including **108** straight/bent stair cases |
| Corrected synchronous graph parity | **2,073** comparisons, zero mismatches/exclusions/resource limits |
| Complete native route context parity | **1,203** comparisons, zero mismatches |
| Dedicated worker/reference controls | **1,408** compared replies across **22** controls; saturation and coalescing asserted in each |
| Capacity samples | **22,528** deterministic graph samples on both native fixtures |
| Busy reader observations | **459**: 428 geometry cases, 24 crowd phases, four clear/generation/map/shutdown transitions and three real epoch rollovers |
| Default crowd `(0, -800, 0)` | **20/20** phases across ten runs; selected full capture rotates through all ten actors |
| Canonical crowd `(-800, -1504, 0)` | **4/4** phases across two runs |
| All-member crowd safety | **240** member-phase audits; zero wall/solid/nudge/incomplete/reservation/overflow failures; accepted actor clips **0** |
| Exit ownership | **240** independently checked physical hull-clearance witnesses |
| Production identity and owner gates | **3 x 11** checks; three actual MAX/epoch rollovers with running readers; allocator/cache owner gates PASS |
| Snapshot / generation / capture lifetime | **3/3** copied snapshot cases; three generations drained; **5/5** capture lifecycle cases |
| Capture integrity | All **452** original/decompressed movement-capture SHA-256 hashes verified |

The final native engine hash is `27F0C4A3689ABD707DA8E4F2DD85192EBCA5F27546A6654F208E62521505D3CA`; servergame hash is `6F0CA62F56877504CFD8C5B13A829B57A10164CAA99D19D12A34F600AC5D8496`. The relevant **86** source/fixture/harness files, sealed maps, binary inputs and retained test-executable hashes remained unchanged through acceptance. Four current CTest targets were executed; the freeze also records an older unused test executable without claiming it ran. M5 and preliminary M6 execution segments retain separate hashes and are not merged into this bundle.

Final reports under `out/build/nav-corner-geometry`: `worker_service/m6-final-native-busy-v2`, `matrix/m6-final-shadow-matrix-v2`, `crowd_matrix/m6-final-shadow-crowd`, `crowd_matrix/m6-final-shadow-crowd-canonical-v2`, `query_identity/m6-final-busy-identity`, `query_snapshot/m6-final-snapshot`, `generation_drain/m6-final-generation-drain` and `lifecycle/m6-final-capture-lifecycle`. Build/CTest evidence: `out/build/m6-final-build.log` and `out/build/m6-final-standalone.log`; isolated CPU sample: `out/build/m6-worker-idle-probe.json`. `out/build/verify-m6-final.py` verifies report gates, physical witnesses, inputs and artifact hashes before writing [the machine-readable acceptance record](m6-final-acceptance-2026-10-04.json).

## Measured and enforced budgets

Measured maxima: **663** faces, **3,573** directed edges, **3,933** counted work units, heap **124**, corridor **44**; retained transport **2,856,568 bytes**, copied snapshot **2,361,145 bytes**. Maximum recorded queue lock wait/hold was **8.0us / 31.0us**, owner submission **12.5us**, graph worker solve **2.2899ms**. These are Debug observations, not production timing guarantees.

Enforced reader envelope: **64** slots, one context, **4,096** faces, **32,768** edges, **65,537** heap entries, **131,072** work units, cooperative **8ms** quantum and cancellation within **32** counted operations. Retained transport/snapshot ceilings are independently **4MiB / 32MiB**, combined **36MiB**. Owner graph drain permits at most two replies and checks a **500us** deadline before each operation. [The checked-in budget table](m6-worker-budgets-2026-10-04.md) records request/result bytes, capacity derivation, old-snapshot retention, priority admission and construction/runtime exclusions.

## Corrected failures and boundaries

The first standalone allocation assertion included caller-created MSVC debug request proxies. Its corrected scope starts after caller preparation and asserts no warm transport growth, while the allocator independently asserts owner-only allocation/free. An initial Ninja recompaction permission failure was preserved; a successful build retry followed. The first native harness expected eight joins for an idle sequence that actually has seven; its failure is preserved, and corrected idle plus genuinely busy controls reran successfully. A test-harness indentation error was repaired before launch.

The initially queued canonical command used `(-251.54, -788.58, 31.25)` by mistake. Formation fitting found no valid slots and rejected its order; its expected-arrival egress test failed. That report/log and the exact invalid-input disposition are retained separately. The unnecessary second replay was stopped only after verifying the owned command lines and parent/child process IDs. Canonical acceptance reran twice at the original `(-800, -1504, 0)` and passes all four phases. No required movement, stair or reproduced default/canonical crowd failure was waived.

Source audit and allocator/concurrency instrumentation establish the specific copied-graph ownership boundary. No sanitizer/race-detector, general race-freedom, fresh visual/animation review, asynchronous actor commit latency or speedup is claimed. The service has one executing context and returns a distinct limit rather than suspending/resuming searches. Reader ceilings exclude total engine live generation data, temporary owner snapshot construction, native route scratch and standard runtime/OS thread storage. Deadline/completion budgets are cooperative and can be exceeded by preemption or one owner operation. M7 must add actor pending/backoff and safe attachment/native commits; M8 must measure optimized load before changing the default.
