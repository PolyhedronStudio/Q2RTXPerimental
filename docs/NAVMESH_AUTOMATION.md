# Navmesh Automation

This document provides practical automation for two production tasks from the navmesh roadmap:

- Offline nav-cache baking during content/asset builds.
- Headless nav regression smoke tests.

Coordinate-space invariants used by these tools are documented in:

- `docs/NAVMESH_COORDINATE_SPACES.md`

## Scripts

- `scripts/nav_bake_maps.ps1`
- `scripts/nav_regression_smoke.ps1`
- `scripts/nav_profile_dashboard.ps1`
- `scripts/nav_pipeline.ps1`
- `scripts/nav_maps_example.txt`
- `scripts/nav_query_pairs_example.csv`
- `scripts/nav_expected_stats_example.csv`
- `scripts/nav_profile_budget_example.csv`

Both scripts run the dedicated server executable in headless mode and issue server commands via command-line `+` arguments.

## Offline Bake

Example:

```powershell
.\scripts\nav_bake_maps.ps1 `
  -Executable .\build-nav\Bin\Q2RTXPerimental_ded.exe `
  -Maps q2rtxp_dev_sigio_targetrange q2dm1
```

Map list file example (one map per line, `#` comments allowed):

```text
# maps for nav bake
q2rtxp_dev_sigio_targetrange
q2dm1
```

```powershell
.\scripts\nav_bake_maps.ps1 `
  -Executable .\build-nav\Bin\Q2RTXPerimental_ded.exe `
  -MapListFile .\build-nav\nav_maps.txt
```

The script bakes `<map>.vans` files using `sv nav_bake` and validates that each cache was written to:

- `baseq2rtxp/maps/nav/`

Cache loads now enforce map/parameter hash matching by default (`nav_cache_require_hash_match 1`), so stale files cleanly miss and can be rebaked.

## Regression Smoke Harness

Example:

```powershell
.\scripts\nav_regression_smoke.ps1 `
  -Executable .\build-nav\Bin\Q2RTXPerimental_ded.exe `
  -Maps q2rtxp_dev_sigio_targetrange q2dm1
```

For each map, the harness runs:

- `sv nav_gen_voxelmesh`
- `sv nav_selftest 256`
- `sv nav_query_path ...` (for each configured query pair)
- `sv nav_profile_dump`

And validates:

- `nav_selftest` reports `result=PASS`
- profile dump exists in log output
- `mesh_loaded=1`
- `world_tiles > 0`
- optional exact `world_tiles` match from `-ExpectedStatsFile`
- optional deterministic query outcomes from `-QueryPairsFile`

`nav_profile_dump` includes generation timings (`world_ms`, `inline_ms`, `total_ms`), async queue metrics, and backend detour prototype metrics (`detour_*`) for dashboarding and budget checks.

Logs are written under:

- `build-nav/nav-regression-logs/`

Query pairs CSV format:

```text
# map,id,start_x,start_y,start_z,goal_x,goal_y,goal_z,expect_found,expect_segments_ok
q2dm1,q0,-128,64,24,512,256,24,1,1
```

Expected world-tile baseline CSV format:

```text
# map,world_tiles
q2dm1,1234
```

Example with deterministic query/stat checks enabled:

```powershell
.\scripts\nav_regression_smoke.ps1 `
  -Executable .\build-nav\Bin\Q2RTXPerimental_ded.exe `
  -MapListFile .\scripts\nav_maps_example.txt `
  -QueryPairsFile .\scripts\nav_query_pairs_example.csv `
  -ExpectedStatsFile .\scripts\nav_expected_stats_example.csv `
  -RequireQueryPairs
```

## Profiling Dashboard and Budgets

Generate per-map dashboard outputs from smoke logs:

```powershell
.\scripts\nav_profile_dashboard.ps1
```

Outputs:

- `build-nav/nav-regression-logs/nav_profile_dashboard.csv`
- `build-nav/nav-regression-logs/nav_profile_dashboard.md`

Optional budget validation:

```powershell
.\scripts\nav_profile_dashboard.ps1 `
  -BudgetFile .\scripts\nav_profile_budget_example.csv `
  -FailOnBudget
```

Budget CSVs can gate both queue/generation and backend detour behavior:

- `max_world_gen_ms`, `max_inline_gen_ms`, `max_total_gen_ms`
- `max_queue_frame_ms`, `max_queue_wait_avg_ms`, `max_over_budget_frames`
- `max_detour_avg_ms`, `max_detour_points_after`
- `min_world_tiles`

## Combined Pipeline

Run bake + smoke in one command:

```powershell
.\scripts\nav_pipeline.ps1 `
  -Executable .\build-nav\Bin\Q2RTXPerimental_ded.exe `
  -MapListFile .\scripts\nav_maps_example.txt `
  -QueryPairsFile .\scripts\nav_query_pairs_example.csv `
  -ExpectedStatsFile .\scripts\nav_expected_stats_example.csv `
  -RunProfileDashboard `
  -ProfileBudgetFile .\scripts\nav_profile_budget_example.csv
```

## CI/Pipeline Notes

- Use `nav_bake_maps.ps1` in post-map-build steps to generate and ship cache assets.
- Use `nav_regression_smoke.ps1` as a quick guardrail in CI for deterministic nav bootstrapping on curated maps.
- Use `nav_profile_dashboard.ps1` to publish perf/capacity snapshots and enforce queue/generation budgets.
- Use `.github/workflows/navmesh-ci.yml` for automatic nav-target validation on push/PR (Windows + Linux builds, Linux sanitizer build, Linux clang-tidy scan).
- Keep map lists small and representative to avoid long CI times.
- A manual GitHub Actions entry point is provided at `.github/workflows/navmesh-automation.yml` for runner setups that already have a dedicated executable and map assets available.
