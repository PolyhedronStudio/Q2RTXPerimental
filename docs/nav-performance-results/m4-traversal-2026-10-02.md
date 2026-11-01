# M4 traversal and accepted-motion continuation — 2026-10-02

Historical outcome: the failed runs below are preserved. The reproduced C12 stall and default-capsule straight/bent staircase subset were corrected and verified on 2026-10-03; see [the current correction report](m4-stairs-crowd-2026-10-03.md). M4 overall remains in progress.

Status: **IN PROGRESS**. M4 is not accepted. Branch `navmesh4`, HEAD `63b55baa`, with existing dirty M1–M3 and unrelated changes preserved. This continues the [strict-clearance foundation](m4-2026-10-02.md); its earlier caller-migration and runtime limitations describe that earlier patch, not the current integration.

## Implemented outcome

- **M4.2 partial:** added standalone `sv nav_corner_capture <entityNum> <frameCount:1..4096>` and `sv nav_corner_report <entityNum> [output.jsonl]`. A disabled-by-default fixed 120-frame ring copies one selected monster's command, predicted/realized bbox endpoints, adjacent feet-space waypoints, hull bounds, mesh/topology, validation reasons/witness, total validation traces, and ground/elevated/accepted-trial clip counts. Contact or command rejection freezes after 40 contact/post-contact samples; up to 80 preceding samples remain. Capture uses entity slot, spawn count and map identity. No per-frame logging or allocations are added by capture. Long console JSON was fragmented in one engine log, so machine evidence must use the explicit output file.
- **M4.3 partial:** added `SVG_Nav_TestMovementCandidate` and an injected-trace decision core shared by production and tests. Feet convert to bbox origin once; native tracing then applies its existing analytical-center conversion. Symmetric centered square XY profiles are required; asymmetric Z is supported, asymmetric/non-square XY is explicitly rejected. The actual native shape and actor-inclusive clip mask are used. Full horizontal completion, finite trace output, non-embedded state, walkable landing and rise/drop policy are required. Partial progress and missing floor no longer authorize continuation. At most four internal sweeps per physical validation; endpoint tolerance is 0.125 units and never shrinks hull clearance.
- **M4.4 partial:** initial W1 release and runtime radius/plane proposals share `SVG_Nav_CanAdvanceWaypoint`. Progress eligibility must also pass current mesh/topology, unchanged-radius static clearance on flat surfaces, and supported native continuation to the next waypoint. Forced elevation holds remain. Callbacks occur only after release, and the final callback is guarded once per committed route. Semantic waypoint records and complete route/goal revisions remain pending.
- **M4.5 partial:** removed three short-link radius reductions, used strict geometry in shortcut/departure checks, removed the separation feet/bbox probe mismatch, and combined squad/follow/catch-up speed factors after steering. Grounded final commands are tested after separation and reservation projection using the actual `P + v*dt`. Travel is capped at the active waypoint. The bounded candidates are blended movement, direct active-waypoint approach, and half-speed direct approach, each reservation-projected and revalidated. Failure holds with a debounced replan; there is no unchecked candidate fallback. Airborne movement remains a separate existing physics path.
- **M4.6 partial:** added versioned `tests/navigation/corner_cases.json`, the movement contract test target, and `scripts/test_nav_corner_ingress.ps1`. The runner checks map SHA256, launches a hidden private dedicated instance of the working client binary, records complete JSON to disk, and requires explicit physical occupancy PASS plus stopped ingress and 10/10 admission. It removes only its exact prior artifacts before launching, so stale evidence cannot pass a failed report. The fixture's RNG seed is not pinned; it is a fixed map/order diagnostic fixture, not deterministic acceptance.

The geometric query remains an owner-thread O(E) reference scan. Bounded candidate/release counts do not make the whole update O(1). The new native checks add trace work; no performance gain has been measured. Ramp/stair faces use native validation rather than flat XY wall clearance. Full-route geometric/physical validity, explicit recovery from invalid endpoints and safe standoff/chord construction are not implemented by this continuation.

## Verification and identities

MSVC 19.51.36260 / Visual Studio 18 Insiders, Ninja, C++20, Debug. The inspected build cache belongs to this checkout.

```text
# First enter VsDevCmd.bat -arch=x64 -host_arch=x64
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_nav_corners.ps1
cmake --build out/build/x64-Debug --target baseq2rtxp_svgame
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_nav_corner_ingress.ps1 -LogName nav-m4-ingress-runner
```

Geometry: 13 analytical assertions. Native movement decision: 23 primary contract assertions, plus callback assertions. CTest **2/2 passed** (latest 0.02 s per target, 0.17 s total); server-game target compiled and linked. Tests cover supported/asymmetric hulls, partial and complete step progress, solids, nonfinite traces, missing/steep floors, exact elevation, zero-height policy, forbidden drops and continuous ramps. Injected trace tests verify the production decision core, not engine collision itself. `git diff --check` passed for the modified tracked M4 files. A whole-checkout check still reports pre-existing trailing whitespace in unrelated `baseq2rtxp/materials/lights.mat`; it was preserved. Test timings are observations, not navigation benchmarks.

Final DLL: `baseq2rtxp/svgamex86_64_d.dll`, SHA256 **1DF8F8D3470BE8D00CA661CD073BB2C25570FB78CDCC46428BC405FF605BE0BC**. The final explicit-file corner replay logged loading this DLL. The earlier 60-second current run used SHA256 **106EE2691F8EDC4E77CB6F41CB3BEF480C2DF55D5DB62ECFE430CCCDF13EFFAA**; the subsequent change was diagnostic file output, with movement source unchanged.

For comparison, the pre-continuation monster movement source was temporarily built with the new diagnostic hook and trial accounting, then restored in a guarded `finally` before running the isolated baseline DLL. Baseline artifact: `out/build/nav-corner-geometry/svgame_m4baseline_d.dll`, SHA256 **71750951927362D5274960DE5C948CCB52832477D78B654F60CFB0463F8EFF7C**. Its engine log confirms that exact isolated library was selected via `sys_forcesvgamelib`. This is a pre-continuation movement comparison, not a pristine commit baseline.

Map `q2rtxp_dev_nav01`, SHA256 **137b9b110cc460de1f87144e48f1a79f1488306df55a2c230f600a31fb9124fa**. Generated 663 faces, 3499 edges, 2063 vertices, 399 KD nodes and 1600 cover points. Selected authored group 2 has ten members; captured leader entity 46 starts at authored bbox `(1360,-1568,72)`, observed grounded bbox `(1360,-1568,36.03125)`, capsule bounds `(-16,-16,-36)..(16,16,36)`. Order: `sv crowd_move 2 -331.6 -1504.4 24 circle`. Normal 40 Hz simulation was retained. Mesh artifact: `out/build/nav-corner-geometry/q2rtxp_dev_nav01.nav7`.

## Recorded engine outcomes

| Evidence | Simulation interval / samples | Accepted static-wall clips | Solids / nudges | Outcome |
|---|---|---:|---:|---|
| [Pre-continuation baseline](nav-m4-ingress-baseline.jsonl) | 8.225–10.300 s, 84 frames | 5 | 0 / 0 | Reproduced corner clipping; contact trigger at 9.325 s. Only 44 pre-contact frames were available, so the full 80-frame pre-window was not captured. |
| [Final explicit-file current replay](nav-m4-corner-current.jsonl) | 8.225–11.200 s, 120 frames | 0 | 0 / 0 | Complete JSON; zero held commands; maximum 6 internal validation traces per captured update. Its first 84 frames match the baseline time interval. |
| [Earlier current 60-second queue replay](nav-m4-ingress-final.jsonl) | 65.225–68.200 s, last 120 frames | 0 | 0 / 0 | Leader's sampled late window is clean, but final ten-member occupancy fails. Maximum 8 internal validation traces per sampled update. |

At baseline t=9.325 s, command start bbox `(1217.025513,-1742.464478,36.03125)`, active index 2, command `(-219.642151,-12.542832,0)`, predicted XY `(1211.534459,-1742.778048)`, realized XY `(1216.076172,-1742.778076)`: actual static-wall velocity clipping despite final blocked flags being insufficient to describe it. Current t=9.325 s has progressed to bbox `(1160.872925,-1748.338501,36.03125)`, no accepted wall clip. The same map/order and time window compare the behavior bundle; routes/speed and realized trajectory differ. Frozen-route replay modes and one-branch-at-a-time causal attribution are pending. This map corner was not identified as the user's original problematic corner.

Baseline and current 60-second C12 replays both ended `moving=1 ingress=1 egress=0 inHead=0/10 egHead=10/10` and explicit `VERDICT: FAIL`, maximum seat error **1550.65** units. Minimum separation was **32.04** baseline / **32.97** current. Preserved summaries: [baseline](nav-m4-ingress-baseline-summary.log), [current](nav-m4-ingress-final-summary.log). Per-member records show incomplete arrival; the checker's aggregate `arrived=10` text counts inconsistently and must not be used as a success oracle. The runner requires the explicit verdict and admission/status invariants. The existing occupancy failure is reproduced in the baseline and remains unresolved; early or late clip-free leader samples are insufficient for C12 acceptance.

The final short explicit-file engine command was:

```text
Q2RTXPerimental_d.exe +set dedicated 1 +set sys_exitonerror 1 +set logfile 1 +set logfile_flush 1 +set logfile_name nav-m4-corner-file +set net_port 27951 +set sv_public 0 +set game baseq2rtxp +set deathmatch 0 +map q2rtxp_dev_nav01 +sv nav_generate +wait 320 +sv nav_status +sv nav_corner_capture 46 120 +sv crowd_move 2 -331.6 -1504.4 24 circle +wait 160 +sv nav_corner_report 46 docs/nav-performance-results/nav-m4-corner-current.jsonl +quit
```

The client binary in dedicated mode exited 0. A separate older dedicated-only executable did not complete startup and is not evidence of movement correctness. Capture argument guards for invalid entity/overflow/trailing numeric input were exercised in the working host. One malformed console-only comparison was rejected rather than repaired into passing evidence.

The final full runner was also executed end to end against the final DLL: engine exit 0, 120 complete late-window records with zero sampled unsafe frames and maximum 8 validation sweeps, followed by explicit occupancy FAIL. The PowerShell acceptance runner correctly exited 1; this is an expected failed gate, not a passing regression run. Final artifacts: [runner capture](nav-m4-ingress-runner.jsonl) and [runner summary](nav-m4-ingress-runner-summary.log). Result remains `moving=1 ingress=1 inHead=0/10`, max seat error 1550.65 and minimum separation 32.97.

## Remaining gates in dependency order

- [ ] **ERROR — C12:** diagnose the reproduced group arrival/queue failure, including all member paths and occupancy; establish baseline passing door/stair/crowd cases before accepting changes. No regression-free claim is made.
- [ ] M4.2: complete schema (AI, route/goal/crowd revisions, semantic constraints, intermediate candidates, speed factors, brush witnesses and collision fractions); frozen-route replay stages; capture lifecycle/recycled-slot tests and complete pre-contact window.
- [ ] M4.3: strict API cold/stale-cache, invalid-input and height engine cases; native capsule/cylinder/asymmetric-Z engine cases; complete 3D/mask/landing parity, invalid-start recovery, all retained route segments, standoff normal constraints and arc chords (K7–K9).
- [ ] M4.4: semantic waypoint records replacing authoritative forced flags; all generation/debug/consumer migration; synchronous route/goal revision and callback scenarios.
- [ ] M4.5: full named candidate/speed histories and exact engine validation matrix, including reservations, airborne boundaries, allowed/forbidden ramp/step policies and infeasible-route behavior.
- [ ] M4.6: grounded and clearance-validated C2–C11 engine fixtures, pinned seed/setup, 20 repetitions per engine case, both directions where applicable, exact endpoints/arrival and representative mirrored-turn rendering/animation review. The C7 standalone contract subset is not engine acceptance.
- [ ] M4 acceptance: all synchronous C1–C12 gates, demonstrated clean traversal/arrival, no introduced door/stair/crowd deadlock. No original-user-corner or visual verification yet.
- [x] Intentional deferral: delayed-worker C11 extension only to M7. Synchronous C11 remains required in M4; immutable snapshot/context ownership and acceleration belong to M5.

## C++ commenting checklist for this continuation

- [x] `nav_movement_geometry.h`: tabbed Doxygen for hull, progress and traversal helpers; validation/direct/step/landing subsections and completion/elevation guards explain intent; result members/constants documented.
- [x] `svg_mmove.h/.cpp`: native candidate API/result and diagnostics fields documented; feet-to-bbox conversion, shape/mask, policy and bounded trace adapter sections explain invariants; step-slide trial reset/selection comments distinguish accepted and discarded motion.
- [x] `svg_mmove_slidemove.h/.cpp`: DP SlideMove declaration/implementation documents optional diagnostics; clip classification, solid flags, plane ownership, repeated-plane recovery and existing solver subsections explain intent. Native clip behavior is retained.
- [x] `svg_monster_base.h/.cpp`: touched reset/route/advance/steering/movement/finish sections document committed topology, callback guards, unchanged radius, coordinate ownership, candidate projection/validation and authoritative capture. Fields have `//!` descriptions. Existing local formatting conventions are preserved.
- [x] `nav_corner_capture.h/.cpp`: public commands/capture and helpers have tabbed Doxygen; every global has preceding `//!`; argument guards, identity/reset, copied ring samples, trigger/freeze, report and file errors have intent comments.
- [x] `svg_commands_server.cpp`: diagnostic includes and direct command-dispatch branches annotated; active standalone commands use the new functions.
- [x] `movement_geometry.cpp`: tabbed Doxygen and narrated fixture groups/trace callbacks; tests use explicit contract assertions without replacing production `Q_assert`.

This report records partial implementation and failed gates; it does not waive any milestone acceptance requirement.
