# M5 final acceptance - 2026-10-04

**M5 is DONE.** Immutable query inputs, exclusive contexts, owned routes, production identities and generation lifetime barriers are implemented and verified. Gameplay path queries still execute synchronously. M6 remains TODO; no path worker is enabled.

The [machine-readable acceptance record](m5-final-acceptance-2026-10-04.json) records the tested binaries/maps, 79 frozen source/fixture/harness hashes and the evidence artifact hashes. The [ownership audit](m5-ownership-audit-2026-10-04.md) documents permitted execution phases and lifetime barriers. Earlier first-slice and failed trial records remain historical evidence.

## Implemented result

- Pure face/KD lookup, portal extraction and A* use const copied graph records plus owned scalar cost policy and exclusive preallocated search buffers. Capacity/work exhaustion is distinct from no route. Supported monster preferences are captured; unknown live overrides retain an explicit owner-only compatibility branch.
- Deep-copied native route geometry, retained context scratch and generation-scoped indices replace shared funnel/LOS/refinement state. Snapshot retirement preserves existing copied readers and blocks replacement until pins release. Scoped leases release on exceptional and normal exits.
- Complete replies own corridor, exact feet endpoints/points, mandatory waypoint semantics and provenance, costs/diagnostics and native proof. Installation checks entity slot/incarnation, goal/state/crowd revisions, mesh/topology, service epoch and latest submission sequence. State reset/load reissues transient identities; controlled overflow drains readers before changing epoch. No request or context is serialized.
- Owner-only physical preparation retains native hull, support and complete-route collision validation. Normal classification preparation advances at most 128 edges/traces per game frame; incomplete inputs cannot publish a partial snapshot. Explicit diagnostic wait completes preparation for deterministic fixture setup.
- Generation progress and logs publish copied synchronized data. Live generation uses an exclusive input window: frame readers pause, live console readers are rejected, and entity/client/save/load/map/clear/shutdown/module boundaries drain before lifetime replacement. Engine and module allocator bookkeeping is synchronized. Analytical shape sweeps use local centers rather than mutating entity origins.

## Reproduced failures corrected

Default crowd placement rejects waiting cells too narrow for the native hull and protects earlier admission routes. Heads physically traverse vacated predecessor cells to front cell zero before final-seat admission. Owned order-time transit routes let a head wait safely while an opposing approach clears; bounded native-validated tangents and head priority resolve contention. Final slots remain immutable and forced steering remains admission-only.

Canonical exit retains the aperture token while unresolved inner-jamb points remain in the bounded next-eight-point route prefix. Actual outward clearance alone cannot release a follower that would seal the head's mandatory continuation. Watchdog refreshes retain ownership; empty travelling routes also retain the token. Neither queue order nor mandatory points are bypassed.

Opt-in full capture reserves at most 128 route versions at command time (14,704,640 bytes at maximum), preserving complete evidence for the reproduced 40-revision member. Movement-time capture cannot grow. Overflow and incomplete capture still fail acceptance.

The allocator gate timeout was a test setup defect: injection during MSVC's noexcept debug-vector constructor failed its iterator proxy and terminated the process. Constructing debug bookkeeping before injection exercises the intended throwing reserve operation and recovery. The no-exception branch verifies real bridge rejection. All native allocator/cache gates pass; no test was waived. The diagnostic stack is retained in `out/build/m5-owner-debugger-v3.log`.

## Final unchanged-binary verification

| Check | Result |
| --- | --- |
| MSVC Debug engine/servergame build and standalone CTest | 3/3 PASS |
| Production pure query contracts | 8,093 checks; 4,000 concurrent solves with allocation/free forbidden |
| Native matrix | 428/428 PASS, one repetition; 108 straight/bent stair cases |
| Graph parity against corrected synchronous graph | 2,073 comparisons; zero mismatches, exclusions or resource limits |
| Complete native route comparison across independent contexts | 1,203 comparisons; zero mismatches |
| Default crowd `(0, -800, 0)` | 20/20 phases across ten runs; full capture rotates all ten actors |
| Canonical crowd `(-800, -1504, 0)` | 4/4 phases across two runs |
| All-member crowd command/admission audits | 240 member-phases; zero wall/solid/nudge/incomplete/reservation/overflow failures; actor clips recorded: 0 |
| Physical exit reservation release | 240 owned hull-clearance witnesses, independently checked |
| Production identity/overflow and owner gates | 3 x 11 checks; three actual epoch rollovers; allocator/cache gates PASS |
| Copied snapshot pin/retire/read/release/renew | 3/3 PASS |
| Generation reader barriers and clear/map/shutdown drain | All three generations completed before reclamation |
| Capture clear/load/generate/map/new-lifetime controls | 5/5 PASS |
| Matrix and crowd capture integrity | All 452 original/decompressed SHA-256 hashes verified |

Reports are under `out/build/nav-corner-geometry`: `matrix/m5-final-full-matrix-v3`, `crowd_matrix/m5-final-default-crowd-v3`, `crowd_matrix/m5-final-canonical-crowd-v3`, `query_identity/m5-final-identity-owner-gates-v2`, `query_snapshot/m5-final-snapshot-lifetime`, `generation_drain/m5-final-generation-drain` and `lifecycle/m5-final-capture-lifecycle`. Each successful phase requires physical slot occupancy, final separation/support, settled controllers, complete captures and native safety. Captures may end at supported arrival; they are not all claimed to contain 2,401 frames.

Build/CTest evidence: `out/build/m5-fault-injection-control-build.log` and `out/build/nav-corner-geometry/Testing/Temporary/LastTest.log`. Verification: `out/build/verify-m5-final.py`. Engine SHA-256: `5B4C04BB9597E2B5422FDB55A06D46EEE04081BDE33D7963320BE41933D73F45`. Servergame SHA-256: `9277497E7B71C116378F9CBEB78396BC9C33B826F8A6C9182991C9A3432DAF6A`. Binary/map inputs and the frozen relevant source/fixture/harness bundle match at final verification.

## Limits and intentional deferrals

Native preparation remains owner-only and may allocate temporary storage or call physical imports; the full function is not an M6 worker entry. Legacy generation pauses live simulation while borrowing inputs rather than generating private output. Source collision ownership evidence supports the audited immutable reads and lifetime barriers, not arbitrary concurrent mutation or general race freedom. No sanitizer/race-detector, new rendering/animation review, latency benchmark or speedup is claimed. M4's historical visual and twenty-repetition acceptance remains separate from this final one-matrix M5 bundle.

Complete-route parity establishes independent-context repeatability, including exact points, semantic provenance, native validation and graph costs; it is not an independent second native physics algorithm. M6 must complete worker capacities, cancellation/queue/hand-off contracts and further separation before off-thread activation. Delayed-result C11 remains intentionally deferred to M7 because no path worker exists. No synchronous correctness or reproduced crowd failure was skipped.
