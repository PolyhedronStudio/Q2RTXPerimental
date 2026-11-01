# M4 stair and crowd correction outcome — 2026-10-03

M4 remains **IN PROGRESS**. The reproduced ten-member crowd stall is repaired in the fixed-map headless scenario, and native ascent/descent now passes the straight staircase and both bent stair sets. M4's complete geometry, semantic/revision, policy, deterministic repetition and visual gates remain open; M5 worker prerequisites remain closed. Branch `navmesh4`, HEAD `63b55baa`, with existing local changes preserved.

This report supersedes the unresolved current-C12 status in the [historical traversal report](m4-traversal-2026-10-02.md), while retaining its original failed reproducer. The user confirmed the stall in-game and identified the lowest step of a bent stair set. Both compiled-map bent sets were tested because exact in-game coordinates were not supplied. Live rendering and the user's exact run have not been observed.

**Implemented corrections**

- Active crowd orders now own immutable `group.slots` and transient ingress/staging assignments. `SVG_Squad_RebuildSlots` may maintain its tactical template but cannot overwrite those crowd goals while moving. `QuerySquadFormationTarget` also avoids applying a second bottleneck pacing token to an active crowd order. These conflicting authorities previously redirected queued members and stalled admission.
- The strict XY cache separately classifies enabled step transitions rather than treating every legacy height seam as an extruded wall. Disabled portals and over-height walls still block; native hull/support/elevation validation remains mandatory. The strict API is still an owner-thread O(E) reference scan.
- A native horizontal capsule sweep can be clear while its direct landing probe starts embedded in a riser. That specific endpoint condition now permits one raised trial from the original valid start, followed by full XY completion and walkable static-world landing. Normal supported movement uses two traces; a partially blocked horizontal step uses four, and the clear-horizontal/embedded-landing case uses at most five. Missing/steep floors, embedded starts, partial steps, actor support and forbidden elevation remain failures. A raised retry cannot inherit the continuous-ramp exception.
- Initial and runtime waypoint release retain the same geometric/progress guard but preview a bounded horizontal native command (up to 5.5 units at normal 40 Hz). A single step trial no longer has to reach across an entire multi-riser continuation, nor match an exact nav surface Z that differs from native capsule support on a slope. Forced tread progress remains guarded. At an actually reached flat constraint, an impossible outgoing link requests a debounced route replacement instead of perpetual shuffling.
- Corner refinement now checks full-radius stationary clearance before committing the optional bisector candidate. After final route cleaning/decimation, unsafe optional flat portals may be projected away from exact finite-edge witnesses using three margin trials with at most eight corrections each. Acceptance requires mesh containment, the same height and both complete neighboring links at unchanged hull radius. Otherwise an optional excursion is pruned only when its exact closed neighbor replacement is clear. Forced points, elevations and exact start/goal are retained. This fixes the reproduced 40-unit channel where a point was only 12 units from a wall for a radius-16 hull. It is a bounded local repair, not proof of every retained route segment or complete K7–K9 acceptance.
- The third of the existing three final-command candidates may use a tangent with outward bias around a physically witnessed monster outside the current crowd. It passes the same full-radius geometry, native support and reservation checks. Own-crowd queue precedence remains authoritative. This removes the reproduced member-54 stall behind stationary actor 36 without issuing unchecked slide movement.
- Final arrival and crowd staging readiness now require a floor Z difference within 6 units, allowing native slope/support offsets while excluding a neighboring 16-unit tread. Previously a descent could report arrival at feet `(0,576.39,16.03)` with the requested goal floor at Z=0. The corrected run reaches Z=0 before arrival.
- The ingress checker now compares feet against immutable final slots, independently counts actual arrivals and unique slots, rejects missing/inactive roster members, verifies floor height/support and requires pair separation at least the sum of native XY radii. A transient staging goal or controller Boolean alone cannot pass occupancy.

**Passable fixture setup and evidence**

Added read-only `sv nav_corner_probe <ent> <sx sy sz> <ex ey ez> [requireHeight:0|1]`, including trace fractions, brush/actor hits and owned edge coordinates. Added opt-in collision-checked `sv nav_corner_place <ent> <feet xyz>` and `sv nav_corner_goal <ent> <feet xyz>`. The exact-goal command requires a supported, isolated one-member crowd and bypasses formation fitting only. Normal monster AI, `ComputePathTo`, `Nav_StringPull`, waypoint release, steering, final validation and native StepSlideMove remain active.

Ordinary formation fitting initially redirected a narrow stair destination onto another floor; those runs are not stair acceptance. A bent target `(-96,-260,64.03125)` failed stationary hull clearance because it was too close to the next riser. That invalid setup is also excluded. Valid goals below were derived from the compiled BSP and passed full native placement/target support checks. No real gameplay failure or required test was intentionally skipped.

Map: `q2rtxp_dev_nav01`, SHA256 **137B9B110CC460DE1F87144E48F1A79F1488306DF55A2C230F600A31FB9124FA**. Bounds `(-16,-16,-36)..(16,16,36)`, native capsule, step 18.25, maximum drop 128, minimum landing normal Z 0.7. Engine simulation retained normal 40 Hz timing.

| Verified fixture | Authored start feet → target feet | Ascent / descent frames | Result |
|---|---|---:|---|
| `C4-stair-native` | `(0,560,0.03125)` → `(0,680,48.03125)` | 19 / 18 | Both directions observe all Z=0/16/32/48 treads and reach the correct goal floor. |
| `C4-bent-stair-native` | `(-96,-404,0.03125)` → `(32,-220,128.03125)` | 49 / 50 | Lowest riser at Y=-380, turn across the 80-unit landing, every 16-unit tread through Z=128, both directions. |
| `C4-east-bent-stair-native` | `(596,224,0.03125)` → `(412,352,128.03125)` | 50 / 49 | Lowest riser at X=572, turn across the 80-unit landing, every 16-unit tread through Z=128, both directions. |
| `C12` crowd reproducer | `sv crowd_move 2 -331.6 -1504.4 24 circle` | 120 retained leader frames | All ten released, arrived and grounded in unique final slots; queue complete. |

Each stair run had zero rejected commands, accepted wall/actor clips, startsolid/allsolid or recovery nudges. Maximum XY command/prediction discrepancy was under 0.00003 units, below the 0.125 reporting tolerance. Final floor error was zero at the captured precision; arrival XY remains the established 16-unit gameplay tolerance, not exact endpoint equality. Captured final positions and first-riser transitions are in the [machine-readable evidence](m4-stairs-crowd-2026-10-03.json). For example the south bent first step moves bbox `(-96,-398.879303,36.03125)` → `(-96,-393.379303,52.03125)` with clear physical/geometric results and four native validation traces.

Three fresh launches of the corrected crowd behavior bundle passed (two before, one after comment-only source edits). The final DLL was verified once for the crowd, straight stairs and each bent set. Final crowd state: `moving=0 ingress=0 egress=0 inHead=10/10 egHead=10/10`; **members=10, arrived=10, unique_slots=10, max_seat_err=7.71, min_sep2d=34.70**. Hull diameter is 32. The retained leader window is 14.650–17.625 s, 120 samples, zero accepted wall/actor clips or embedded/recovery events, one safely held command at 16.650 s, and at most nine total internal validation traces in a captured update. The window is not a complete ten-member motion trace. No claim of zero holds across the whole crowd run is made.

**Build and reproducible commands**

MSVC 19.51.36260 / Visual Studio 18 Insiders, Ninja, C++20, Debug. Enter `VsDevCmd.bat -arch=x64 -host_arch=x64` first.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_nav_corners.ps1
cmake --build out/build/x64-Debug --target baseq2rtxp_svgame
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_nav_corner_ingress.ps1 -LogName nav-m4-final-crowd -Port 27951
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_nav_stairs.ps1 -CaseId C4-stair-native -LogName nav-m4-final-straight -Port 27952
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_nav_stairs.ps1 -CaseId C4-bent-stair-native -LogName nav-m4-final-bent -Port 27953
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_nav_stairs.ps1 -CaseId C4-east-bent-stair-native -LogName nav-m4-final-east-bent -Port 27954
```

CTest **2/2 passed**: 13 geometry and 28 primary native-contract assertions, plus callback assertions. Final observed timings: 0.02 s per target, 0.05 s total; these are test observations, not navigation benchmarks. The Debug server-game target compiled/linked successfully. All four final engine runners exited 0. They launch the working client binary in private hidden dedicated mode, use exact map/hash coordinates, remove only their exact stale artifacts, require native-valid setup and real completion, and fail on timeout or missing/unsafe evidence.

Final DLL: `baseq2rtxp/svgamex86_64_d.dll`, SHA256 **59CA72CDB3B1B401918CD3B6A16465F8181821A2D7671B958EDABCD096B4B23E**. Each final log confirms that library path, with no intervening build. Capture's compiled date/time identifies the capture translation unit; DLL SHA256 identifies the complete linked artifact. Pre-comment corrected bundle hash: `C1A03292E4F37C8B76EBBCF4261EC10F3B74B4938093F851698680B2E76C86AD`.

Preserved current artifacts:

- [Crowd summary](nav-m4-final-crowd-summary.log) and [leader capture](nav-m4-final-crowd.jsonl).
- [Straight summary](nav-m4-final-straight-summary.log), [ascent](nav-m4-final-straight-ascent.jsonl), [descent](nav-m4-final-straight-descent.jsonl).
- [South bent summary](nav-m4-final-bent-summary.log), [ascent](nav-m4-final-bent-ascent.jsonl), [descent](nav-m4-final-bent-descent.jsonl).
- [East bent summary](nav-m4-final-east-bent-summary.log), [ascent](nav-m4-final-east-bent-ascent.jsonl), [descent](nav-m4-final-east-bent-descent.jsonl).

**Remaining gates in dependency order**

- [x] Repair the reproduced C12 stall and verify physical final occupancy; retain the historical failed reproducer.
- [x] Add explicit first-riser/turn/ascent/descent native engine coverage for default capsule stairs; reject invalid fixture setup and false neighboring-tread arrival.
- [ ] M4.2: complete schema/revision/semantic/candidate/speed/fraction witnesses, frozen-route causal isolation and capture lifecycle coverage.
- [ ] M4.3: complete K7–K9 standoff/chord/all-retained-segment proof, cold/stale/cache and height engine cases, native cylinder/asymmetric-Z and complete mask/support parity.
- [ ] M4.4: semantic waypoint records and complete mesh/route/goal revision/callback scenarios.
- [ ] M4.5: steps/drops below, at and above policy limits, all shapes/speeds, reservations/separation, top exits, airborne boundaries and infeasible-route behavior. Default-capsule tests above are a subset of C4/C7/C9/C10.
- [ ] M4.6: full synchronous C2–C12 matrix, pinned RNG seed and 20 repetitions per fixture, normal door/stair/crowd acceptance and representative mirrored-turn rendering/animation review. The exact user-observed corner and stair run remain unrecorded.
- [ ] M4 acceptance; M5 worker activation remains blocked by these prerequisites. No performance improvement has been measured.
- [x] Intentional deferral: only C11's delayed-worker extension to M7; synchronous C11 remains required in M4.

**C++ commenting checklist for changed files**

- [x] `nav_path.h/.cpp`: owned witness/strict role metadata documented; cache revisions, enabled-riser distinction, candidate clearance and optional portal projection/pruning sections explain geometry and bounded work. Added helpers have tabbed Doxygen; new globals have preceding `//!`.
- [x] `nav_movement_geometry.h`: header and sanity/direct/landing/raised-retry sections document full progress, static-world support, the five-trace ceiling and discrete-step/ramp distinction.
- [x] `svg_mmove.h/.cpp`: actor witness/reporting fields and trace adapter capture intent documented; native shape, coordinate conversion and mask remain explicit.
- [x] `svg_monster_base.h/.cpp`: shared waypoint release, bounded horizontal preview, debounced invalid-link replacement, crowd authority, actor tangent and final-candidate checks documented; six-unit final floor invariant documented.
- [x] `svg_squad_coordinator.cpp`: rebuilt tactical template, active-crowd ownership guard and bounded member mutation loop documented throughout `SVG_Squad_RebuildSlots`.
- [x] `svg_crowd_types.h`, `svg_crowd_manager.cpp` and `svg_monster_testdummy_debug.cpp`: arrival floor tolerance and modified arrival/staging guards explain why another tread cannot count as arrival.
- [x] `svg_crowd_commands.cpp`: status/route diagnostics and controller/final-slot/arrival/support/pair-separation checker sections document their independent invariants and roster failures.
- [x] `nav_corner_capture.h/.cpp`: probe/placement/singleton-goal commands have tabbed Doxygen and commented input, support, ownership and mutation guards; capture globals are documented.
- [x] `svg_commands_server.cpp`: standalone diagnostic command dispatch branches explain opt-in purpose.
- [x] `movement_geometry.cpp`: added embedded-end retry, forbidden partial/zero-step/over-rise and actor-floor cases use narrated sections and explicit Q_assert contracts.

No tests were waived. The broad pending matrix is unimplemented acceptance work, distinct from a reproduced failing gameplay scenario or an invalid fixture setup.
