# M5 ownership and generation lifetime audit - 2026-10-04

Disposition: **M5 DONE**. The final unchanged-binary regression bundle passes; [acceptance evidence and limits](m5-final-acceptance-2026-10-04.md) and [hash manifest](m5-final-acceptance-2026-10-04.json) accompany this source boundary audit. No path worker is enabled. Historical pending statements below describe the earlier audit stages and are superseded by final acceptance.

## Query ownership

`nav_query_algorithm.h` implements face/leaf lookup, point containment, portal extraction and weighted A* using only const spans, scalar copied policies and caller-owned buffers. It has no engine imports, entity callbacks, mutable file-scope scratch or allocator calls. The owner sizes face state and reserves heap/corridor storage before entry. Exhausted capacity/work budgets return `resource_limited`; malformed inputs and obsolete publications have distinct outcomes. Query-local stamp rollover clears only that context.

The publication owns graph records, native vertices/halfedges/faces/nodes, reduced scalar room classifications, boundary roles, corner data, components, widths and deep-copied spatial buckets. No span points into live mesh vectors. Pins and reclamation belong to the engine owner; retirement closes acquisition while preserving an existing copied reader. Replacement is blocked until pins release. Scoped leases release on normal and exceptional exits. Components are a policy-independent connectivity superset; directed step/drop and disabled-edge policy remain query decisions.

Monster requests copy exact feet endpoints, native bounds/shape, movement and cost policy, indexed prior corridor membership, sequence, incarnation, goal/state/crowd revisions, generation/topology and service epoch. Unknown virtual cost overrides use an explicit synchronous compatibility branch. Its completed diagnostic is copied immediately. Actor and crowd revisions change before exposing changed assignments. Equal assignments retain their revision. Overflow drains generation and the inline service, retires publication and changes epoch; captured replies are never relabelled across an epoch reset. M6 must extend that drain to its future running reader and queues.

The complete reply owns corridor, exact points, semantic obligations/provenance, graph/refinement diagnostics and native proof. Commit checks actor identity and current submission sequence. Native funnel/LOS/refinement helpers use explicit contexts and const copied route geometry. Each monster retains exclusive graph and native scratch capacity; convenience wrappers own local contexts. Retained corner-candidate scratch stores indices into the currently pinned snapshot or its own context buffers, never pointers into a retired publication or relocated corner vector. The compatibility last-query diagnostic receives a terminal copy rather than supplying ordinary production route input.

## Execution and imports

| Boundary | Permitted access and enforced lifetime |
| --- | --- |
| Pure graph core | Const copied mesh plus exclusive preallocated request/context/result; no entity, collision, logging, allocator or callback imports. |
| Native route preparation | `Nav_PrepareRouteNativeMainThread` and physical proof remain on the owner. Native collision traces, support resolution, tagged preparation/reclamation and compatibility diagnostics are allowed here. This full function is not authorized as an M6 worker entry. |
| Collision-dependent classification | Owner advances at most 128 half-edges and therefore at most 128 boundary traces per normal game frame. Snapshot publication remains pending until preparation is complete. The explicit `nav_wait` diagnostic drains preparation to completion for deterministic fixture setup. |
| Existing generation job | Extraction, CSG, half-edge/KD construction, cover validation/indexing, room/cache ray casts and sector graph construction retain an exclusive live-input window. This is a guarded migration alternative, not private-output generation. |
| Generation progress/logs | All progress fields are mutex-protected and exposed as value copies using monotonic system time. Worker messages use fixed copied storage; owner output occurs after the publication lock releases. Overflow is reported explicitly. |
| Tagged allocation | Engine zone chain/tag accounting and module allocator accounting are synchronized, including static cvar-string accounting. Normal module-to-engine lock order has no inverse module callback. Catastrophic engine allocation failure is not a tested recovery contract. |

The generation call chain reads collision BSP/model data and frozen entity model/origin/flags; cover probes and corner classification reach `SVG_Trace`, then the server trace imports. Box/shape traces use call-local state and thread-local visited-brush tables; generated box/octagon hulls are thread-local. Server area traversal uses a shared mutex. Analytical entity shape sweeps now use local centers rather than temporarily writing entity origins. These facts support concurrent immutable collision reads. They do not authorize concurrent mutation or arbitrary future callers.

During generation the game frame returns before entity thinking, Lua frame callbacks, movement, crowd/cover updates and navigation debug readers. Server-game live console readers are rejected except copied status and controlled generation/wait/clear/load operations. Client think/connect/begin/disconnect/userinfo/command and save/load exports drain before live entity access. Generation completion publishes geometry identities and dynamic edge state on the owner. Clear and module shutdown drain completion before freeing geometry or module code. The engine drains before replacement BSP loading; map spawn clears navigation before releasing entity slots. Level loading recreates entities, so transient navigation identities/contexts are omitted from saves and incarnations are reissued.

The existing engine async queue invokes completion callbacks under its work lock. The navigation completion callback does not enqueue or recursively pump that queue. M6 must use its dedicated bounded service rather than assume this generic queue is a suitable path-query dispatcher. Gameplay telemetry writers exclude the generation worker; cover claims and dynamic edge resync remain owner operations.

## Evidence and limits

Standalone graph checks exercise owned costs, exact endpoints, independent stamps, invalid input, directed step/drop policy, stale publication, work/capacity failure, rollover and 4,000 concurrent solves with allocation/free forbidden. Native opt-in parity compares the pure graph against the retained corrected synchronous graph and compares complete native routes through independent contexts. The latter is a context-isolation/repeatability check, not an independent second implementation of native physics.

No race detector or sanitizer result is claimed. Native lifecycle checks and source ownership evidence establish the specified access/barrier contracts; absence of a crash does not establish general race freedom. Native funnel/refinement still contains owner-side temporary allocation and physical imports. Their movement to a worker requires further separation and capacity preparation under M6. Generation deliberately pauses live simulation access while its legacy inputs are borrowed. No generation latency or path-worker performance improvement is claimed.

The reproduced default crowd order remains an acceptance prerequisite. Failed placement/occupancy/capture reports are preserved. No arrival timeout, physical clearance requirement or safety/capture failure is waived.

## Reproduced crowd approach correction

The default `(0, -800, 0)` order exposed an impossible 13.923-unit waiting cell for a 16-unit native hull and a final-seat sealing pattern. Supported-cell placement now also requires closed-point geometric clearance and protects earlier admission paths. Each active head physically traverses vacated predecessor cells to front cell zero before admission. The last member retains the same obligation. An empty rear-cell route may recover toward the front, never directly to a final seat.

Owned static native cell-to-predecessor routes are prepared once during order construction. Before starting a leg, the head checks unserved actor hulls against that complete route and waits in its supported cell while opposing approaches clear. These frame checks perform no path query, trace or allocation. The head owns movement priority while travelling; an approaching waiter may use the existing bounded tangent around a head holding station. Every actual candidate still passes full static clearance, unchanged native hull/support and finite-aperture reservation. The normal front-cell march leaves queue ranks and final-seat geometry unchanged; existing adjacent-vacancy recovery transfers assignment ownership explicitly. Forced-point steering remains admission-only.

The first complete default-order replay passes both phases, all twenty member-phase audits and complete selected-member captures. Repetition and final unchanged-binary acceptance remain pending. Failed experiments and invalid launches remain separately recorded in the masterplan and their reports.


## Canonical egress aperture ownership

A canonical exit exposed premature token release: actual outward clearance was reached while an unresolved native-refined waypoint still lay at the inner jamb. Releasing the next actor sealed that required return segment. Egress now retains ownership while any point in the bounded next-eight-point prefix intersects the inner aperture; empty travelling routes also retain ownership. Actual depth and supported arrival remain required. This neither skips waypoint obligations nor changes queue order or the physical clearance witness. Watchdog refreshes cannot release this unresolved reservation. The focused canonical replay passes both phases; full final repetitions are pending.

## Allocator control diagnosis

The native timeout was a test setup defect, not failed alignment. Debugger stacks show the injected failure in the allocator rebound to MSVC's debug iterator proxy during a noexcept vector constructor. Constructing the vector before arming fault injection targets the intended throwing reserve operation. Test 8 now verifies the caught bad allocation and subsequent recovery; the no-exception branch verifies actual bridge rejection. The complete native allocator/cache gate passes on the final bundle. The production allocation bridge is unchanged by this correction.


Final acceptance: 428 native cases including 108 stairs, graph/complete-route context parity, 24 repeated crowd phases with 240 member-phase audits and physical exit witnesses, identity/overflow/allocator/cache controls, snapshot retirement, generation drain/read barriers and capture lifecycle all pass. All 452 matrix/crowd capture hashes and frozen inputs match. No synchronous failure was waived.
