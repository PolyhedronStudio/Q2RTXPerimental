# Navmesh Coordinate Spaces

This document defines the coordinate-space invariants used by the nav system.

## Spaces

- `feet-origin`:
  - Caller-facing world-space origin where `z` is at agent feet.
  - Used by gameplay/AI code and server command inputs.
- `nav-center`:
  - Internal world-space origin centered on the active agent hull.
  - Used for node lookup, A*, and step/drop edge validation.
- `model-local`:
  - Inline-model local space used to store mover tile/cell/layer data.
  - Converted to/from world via rigid transforms at generation/query/debug time.

## Required Conversions

- `feet-origin -> nav-center`:
  - `SVG_Nav_ConvertFeetToCenter(...)`
- `nav-center -> feet-origin`:
  - `SVG_Nav_ConvertCenterToFeet(...)`
- `model-local <-> world` (points):
  - `SVG_Nav_WorldFromLocal(...)`
  - `SVG_Nav_LocalFromWorld(...)`
- `model-local <-> world` (directions):
  - `SVG_Nav_WorldDirectionFromLocal(...)`
  - `SVG_Nav_LocalDirectionFromWorld(...)`

## Invariants

- Public traversal APIs accept `feet-origin` coordinates.
- A* and node resolution operate in `nav-center`.
- Inline model sampling traces in world space, then stores layer hits in model-local.
- Runtime inline-model queries/debug draw convert model-local samples back to world consistently via rigid transforms.

## Validation

- `sv nav_selftest [iterations]` checks:
  - rigid transform point/direction round-trip errors
  - feet-origin/nav-center inverse conversion
  - tile/cell boundary conversion sanity
- `scripts/nav_regression_smoke.ps1` runs `sv nav_selftest 256` per map and fails if result is not `PASS`.
