# 🎯 Validate Nav A* Stall Regression via HandleFailure Logs
**Overview**: Validate Nav A* Stall Regression via HandleFailure Logs

**Progress**: 68% [███████████░]

**Last Updated**: 2026-03-02 01:48:00

## 📝 Plan Steps
- ✅  **Locate nav debug scenario entry points**
- ✅  **Identify HandleFailure counter log format**
- 🔄  **Execute focused nav debug scenarios** (IN-PROGRESS)
   - Reverted old-plan failure-summary instrumentation (`nav_failure_summary_t`, request map tracking, and `nav_fail_summary` command).
   - Kept debug testdummy state machine behavior and maintained stateless dispatcher design.
   - Migrated transition guard responsibility to individual state handlers (dispatcher only routes state).
   - Restored direct fallback movement in `MoveAStarToOrigin` so the debug monster can move/animate when no path/navmesh is available.
   - Added deterministic state-transition logging (`oldState -> newState`) and aligned pursuit reacquire gates to reduce `PursuePlayer`/`IdleLookout` oscillation.
-  **Collect before counters from HandleFailure logs**
-  **Collect after counters from HandleFailure logs**
-  **Compare one-node stall signatures**
-  **Report findings and confidence**

## Files changed (revert + behavior stabilization)
- `src/baseq2rtxp/svgame/nav/svg_nav_request.{h,cpp}` — removed old-plan failure-summary API and storage.
- `src/baseq2rtxp/svgame/nav/svg_nav_path_process.{h,cpp}` — removed summary-related owner field and reset coupling.
- `src/baseq2rtxp/svgame/svg_commands_game.cpp` — removed `nav_fail_summary` command wiring.
- `src/baseq2rtxp/svgame/entities/monster/svg_monster_testdummy_debug.cpp` — removed summary-print dependency and restored direct fallback movement (yaw + run anim + velocity) when no path/navmesh exists.
- `src/baseq2rtxp/svgame/entities/monster/svg_monster_testdummy_puppet.cpp` — removed summary-owner assignment tied to reverted instrumentation.

## New root-cause remediation plan (post-revert)
1. **Reproduce in two controlled modes**
   - `No navmesh loaded` mode (current map case from logs).
   - `Navmesh loaded` mode (known working nav map).
2. **Capture state-machine transitions deterministically**
   - Log transition reason + source/target state per think tick.
   - Confirm no oscillation loops (`PursuePlayer` ↔ `IdleLookout`) when LOS toggles.
3. **Validate movement command issuance per state**
   - Assert each active pursue/investigate state sets either path-follow velocity or fallback velocity each frame.
   - Verify run animation frame updates whenever non-zero movement is requested.
4. **Audit async request lifecycle for stale markers**
   - Check `pending_request_handle`, `rebuild_in_progress`, and queue pending status coherence.
   - Ensure state transitions cancel/reset requests exactly once.
5. **Stabilize LOS/trail handoff rules**
   - Confirm breadcrumb fallback window behavior after LOS loss.
   - Prevent immediate idle fallback if valid breadcrumb target is available.
6. **Add explicit no-navmesh behavior contract**
   - In debug entity, always allow direct fallback steering when navmesh is absent.
   - Keep this behavior isolated to debug testdummy variant.
7. **Run scenario matrix and collect evidence**
   - Activation near player, LOS loss/regain, trail follow, sound investigate, blocked recovery.
   - Record movement/animation outcomes for each case.
8. **Finalize with targeted cleanup**
   - Remove temporary diagnostic noise that is no longer needed.
   - Keep only durable debug prints that help future nav regressions.

