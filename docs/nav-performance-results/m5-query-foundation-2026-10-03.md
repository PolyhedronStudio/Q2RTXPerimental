# M5 query foundation — 2026-10-03

**Historical first-slice record:** superseded by [final M5 acceptance on 2026-10-04](m5-final-acceptance-2026-10-04.md). The implemented/remaining statements below describe this earlier tested slice.

The first M5 slice is implemented and verified. **M5 remains IN PROGRESS; full milestone acceptance is false.** Gameplay path queries still execute synchronously on the server owner thread. M6 has not started.

The active debug dummy and callback-free graph callers now search copied graph inputs with an owned cost policy and exclusive query state. M4 movement, stair handling, route validation and crowd admission remain the owner-side consumers. Unsupported live cost overrides retain an explicitly named synchronous reference path.

## Implemented outcome

- `nav_query_algorithm.h` contains the production engine-independent face/KD lookup, directed portal extraction and A* implementation. Views contain const spans into copied, pointer-free records. Requests own prior-corridor membership, exact supplied endpoints, hull/policy values and an identity record. Results own corridor IDs, score, status and request-local search diagnostics.
- `nav_query.h` defines tagged request/context/result owners and the private copied mesh owner. Owner-side acquisition blocks during generation or retired publication. Existing pins retain their copied data; replacement waits for the last release. A noncopyable scope guard releases on normal returns and exception unwinding.
- The pure search checks prepared capacity and work limits before buffer growth or accepting a partial result. Insufficient resources return `resource_limited`, separately from `no_route`. Independent visitation stamps can wrap locally; a service revision refuses silent `uint64_t` wrap. Controlled service drain/reset is still pending.
- The dummy captures the existing 0.85 prior-corridor preference and scared-state 0.8 stair preference as scalar data. Exact-class opt-in prevents a future derived override from silently inheriting an incompatible snapshot representation. Other overrides remain synchronous until represented explicitly.
- A* preserves the corrected reference's weighted heuristic, tie ordering, stale heap filtering, step/drop rules, narrow-portal exclusions and cost factors. This is compatibility work; neither optimality nor a speedup is claimed.
- Generation progress now returns a mutex-protected owned copy. All accesses to its shared fields are synchronized. Engine clock, logging and queue imports occur outside the progress mutex. Elapsed reporting uses the owner enqueue timestamp, including queue delay. The existing live generation pipeline still needs its separate lifetime and publication work.
- Opt-in `sv nav_query_parity 1` compares graph validity, corridor, graph score, selected strict face and each corridor portal with the corrected synchronous reference. Normal gameplay does not double-search. `sv nav_query_snapshot_check` checks two simultaneous owner pins, retired-data retention and replacement after both releases.

## Current verification

| Check | Result | Evidence |
|---|---|---|
| MSVC Debug standalone and server-game build | PASS | `out/build/m5-query-lifetime-build.log` |
| Standalone CTest | 3/3 PASS | `out/build/nav-corner-geometry/Testing/Temporary/LastTest.log` |
| Production pure-core contracts | 8,088 checks, including 4,000 solves across two threads | `tests/navigation/query_context.cpp` |
| Native navigation matrix | 428/428 PASS, one repetition; includes 108 straight/bent stair cases | `out/build/nav-corner-geometry/matrix/m5-query-parity-final/summary.json` |
| Synchronous graph parity | 2,075 comparisons across 19 private batches; zero mismatches, exclusions or resource limits | Final batch log records listed in the companion JSON |
| All-member crowd regression | 4/4 phases, two ingress/egress repetitions, 40 member-phase audits | `out/build/nav-corner-geometry/crowd_matrix/m5-query-crowd-regression/summary.json` |
| Capture lifecycle | 5/5 PASS: clear, load, regenerate, map/entity-slot reuse and fresh capture | `out/build/nav-corner-geometry/lifecycle/m5-query-capture-lifecycle/summary.json` |
| Copied snapshot lifetime | 3/3 PASS: two pins, two blocked acquisition checks, retained retired search, renewed publication | `out/build/nav-corner-geometry/query_snapshot/m5-query-snapshot-lifecycle/summary.json` |
| Generation progress polling | 18 finite bounded copied status samples during generation | Same snapshot lifecycle report |
| Capture integrity | All 432 native/crowd captures match original or decompressed SHA-256 | Verified before this report was written |

The allocation guard covers query-buffer backing allocation and free during the actual production solver call, including both concurrent contexts. Fixtures also exercise stale generation/topology, independent stamp rollover, revision overflow refusal, insufficient output/heap/work budgets, disconnected components, invalid input, stacked-floor lookup, half-unit edge contact, portal union and malformed KD cycles. This is not a complete engine allocator audit or a race detector result.

Parity counts are summed once per private batch, rather than summing the repeated batch statistics attached to individual cases. These native cases verify actual M4 motion through the new graph bridge, but do not yet compare two independently owned funnel/refinement pipelines. Native scenarios do not separately force the scared mood; that cost representation has production-core unit coverage.

The M5 regression repetitions are explicitly smaller than M4's twenty-repetition acceptance. The accepted M4 evidence remains historical and unchanged. Earlier `m5-query-parity-reference` results used DLL `16599E4E1CE06965A2CBA008E454C0E86C7D81EDEE45DB020AAB6BB1A2FA8CE3`; they are historical and excluded from current totals.

## Binary and source provenance

All current native runs use these unchanged inputs:

- `Q2RTXPerimental_d.exe`: `B39B2A1D7B7EBB61B5F5B6731FA3323F3EBC9E7E594308DC029948065FFF7907`
- `baseq2rtxp\svgamex86_64_d.dll`: `422F1A1CC4F8A82C9201968390F7A3E2441B7B716BF5B2EC770B16EA6EF7419B`
- `baseq2rtxp\maps\q2rtxp_dev_nav01.bsp`: `137B9B110CC460DE1F87144E48F1A79F1488306DF55A2C230F600A31FB9124FA`
- `baseq2rtxp\maps\m4_corners.bsp`: `873529E960484C16083599939E4365E1A689FD47742895C40DFD66ADF0F3D6F9`

The [machine-readable record](m5-query-foundation-2026-10-03.json) includes source fingerprints for this slice, report/build/CTest hashes, all nineteen parity log hashes and explicit remaining work. The checkout also contains prior user/M4 edits; these fingerprints identify the tested source state without claiming every checkout change belongs to M5.

## Ownership audit and remaining gate

| Path | Current boundary | Remaining work |
|---|---|---|
| `Nav_QueryFindLeaf`, `Nav_QueryInsideFace`, `Nav_QueryFindFace`, `Nav_QueryPortal`, `Nav_QueryFindPath` | Const copied geometry; exclusive prepared context/result; no engine includes/imports, live globals, callbacks, logging, resize or reserve | Broader full-route pipeline parity and measured M6 capacities |
| Snapshot builder, acquire/release/retire and synchronous bridge | Owner-only tagged allocations, cache/trace preparation, publication and compatibility diagnostics | Bound preparation, finish teardown/reclamation audit and production identity wiring |
| `Nav_FindPathLegacyMainThread` | Explicit owner-only reference and unsupported-policy fallback | Remove shared solver state from all remaining worker candidates; retain diagnostic compatibility only as justified |
| `Nav_StringPull`, LOS/refinement and other live lookup callers | Existing owner-side geometry/trace/cache dependencies remain | Copy geometric inputs, own portal/corner/LOS/refinement scratch and request-local diagnostics; name the native collision proof/commit phase |
| Generation progress | Synchronized copied publication; engine imports outside its mutex | This lock does not protect the live mesh, collision data or allocator |
| Existing generation/load/clear | Snapshot acquisition closes before generation; pinned copies remain readable | Prevent unsupported live readers, use private generation output or a proven complete lifetime barrier, and audit drain/module unload |
| Entity/crowd request identity | Scalar schema exists; synchronous bridge copies mesh/topology/sequence and exact endpoints | Populate entity incarnation, goal/state/crowd revisions; invalidate consistently; implement overflow drain/reset |
| Search result | Owns graph corridor, status and diagnostics | Add full owned M4 waypoint records and route-stage diagnostics with commit identities |

Continue in dependency order: finish full request/result identities and ownership; migrate funnel/LOS/refinement and remaining lookup scratch; establish generation/load/clear/native lifetime boundaries; then rerun full route/waypoint/cost parity and lifecycle stress. Only the complete M5 gate can authorize moving to M6.

No failing test was waived. Delayed-worker C11 remains intentionally deferred to M7 because no path worker exists yet. Fresh renderer/animation review, scheduling gains, total race freedom and engine-wide allocator safety are not claimed by this slice.

## Scoped commenting review

The M5 additions use K&R braces, fixed-width integer identities, `nullptr`, tabbed Doxygen and intent comments. This review concerns the touched M5 sections; it does not certify unrelated checkout changes.

| File | Documented M5 sections |
|---|---|
| `nav_query_algorithm.h` | Copied records, request/context/result contracts; lookup and portal guards; search setup, capacity/work limits, heap ordering, scoring, reconstruction and failure invariants |
| `nav_query.h` | Private snapshot storage, const view lifetime, owner APIs, tagged owners and synchronous/parity diagnostics |
| `nav_path.cpp` / `.h` | Owner affinity, copied preparation, pin/release/retirement, scope guard, request capture, pure solve, compatibility diagnostics, reference parity and retirement diagnostic |
| Monster base and debug dummy `.cpp` / `.h` | Snapshot-cost opt-in, captured policy, synchronous corridor choice and conservative future override handling |
| `nav_thread.cpp` / `.h` | Progress locks/copies, owner enqueue clock, engine imports outside locks and separate remaining generation lifetime boundary |
| `nav_generate.cpp` | Status consumes an owned progress value |
| `svg_commands_server.cpp` | Standalone diagnostic command dispatch |
| `tests/navigation/query_context.cpp` | Allocator instrumentation, independent-context concurrency, search/resource/revision and copied geometry fixtures |
