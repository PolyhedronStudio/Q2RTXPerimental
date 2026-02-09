# Navigation Voxelmesh Generator

## Overview

The navigation voxelmesh generator creates a sparse, multi-layered navigation mesh suitable for A* pathfinding in Quake 2 BSP maps. The implementation is designed to be bbox-independent with configurable parameters.

## Files

- `src/baseq2rtxp/svgame/nav/svg_nav.h` - Header file with data structures and API
- `src/baseq2rtxp/svgame/nav/svg_nav.cpp` - Implementation file

## Console Command

**Usage**: `sv nav_gen_voxelmesh`

Generates the navigation voxelmesh for the currently loaded level and prints statistics.

## Configuration CVars

| CVar | Default | Description |
|------|---------|-------------|
| `nav_cell_size_xy` | 4 | XY grid cell size in world units |
| `nav_z_quant` | 2 | Z-axis quantization step |
| `nav_tile_size` | 32 | Number of cells per tile dimension |
| `nav_max_step` | 18 | Maximum step height (matches PM_STEP_MAX_SIZE) |
| `nav_max_drop` | 128 | Maximum safe downward drop height before traversal is rejected |
| `nav_drop_cap` | 18 | Maximum downard(Z) height that agents can drop safely |
| `nav_max_slope_deg` | 45.57 | Maximum walkable slope in degrees (matches PM_STEP_MIN_NORMAL = 0.7) |
| `nav_agent_mins_x` | -16 | Agent bounding box minimum X |
| `nav_agent_mins_y` | -16 | Agent bounding box minimum Y |
| `nav_agent_mins_z` | -36 | Agent bounding box minimum Z |
| `nav_agent_maxs_x` | 16 | Agent bounding box maximum X |
| `nav_agent_maxs_y` | 16 | Agent bounding box maximum Y |
| `nav_agent_maxs_z` | 36 | Agent bounding box maximum Z |

## Data Structures

### nav_mesh_t
Main navigation mesh structure containing:
- World mesh data (per-leaf tiles)
- Inline model mesh data (per-model tiles)
- Generation parameters
- Statistics (total tiles, cells, layers)

### nav_leaf_data_t
Per-BSP-leaf navigation data with an array of tiles.

### nav_inline_model_data_t
Per-inline-model (brush model) navigation data with tiles in local space.

### nav_tile_t
A tile covering a region of XY cells with:
- Tile coordinates (tile_x, tile_y)
- Presence bitset for sparse storage
- Array of XY cells

### nav_xy_cell_t
XY cell entry containing multiple Z layers.

### nav_layer_t
A single Z layer with:
- Quantized Z position
- Flags (walkable, water, lava, slime, ladder)
- Clearance information

## Generation Approach

### World Mesh
1. Uses **world-only collision** traces (`gi.trace` with `nullptr` passent)
2. Iterates through BSP leafs
3. For each leaf, creates tiles based on leaf bounds
4. For each tile, samples XY grid positions
5. For each XY position, performs downward traces to find multiple walkable layers

### Inline Model Mesh
1. Identifies inline BSP models (*1, *2, etc.)
2. Bakes each model in its **local space**
3. Uses `gi.clip` traces against the specific model entity
4. Stores meshes keyed by model index for later entity transform application

### Multi-Layer Detection
- Performs repeated downward traces at each XY column
- Checks for walkable slopes (normal.z >= cos(max_slope_deg)) and step heights
- Detects content flags (water, lava, slime)
- Stores all walkable layers, not just the topmost hit

## Memory Management

All allocations use `gi.TagMalloc` / `gi.TagFree` with `TAG_SVGAME_LEVEL` tag:
- Navigation mesh is freed on level unload
- Can be regenerated multiple times with `sv nav_gen_voxelmesh`

## Integration Points

### Initialization
- `SVG_Nav_Init()` called from `SVG_InitGame()` in `svg_main.cpp`
- Registers all cvars with default values

### Shutdown
- `SVG_Nav_Shutdown()` called from `SVG_ShutdownGame()` in `svg_main.cpp`
- Frees all allocated navigation mesh memory

### Command Registration
- `ServerCommand_NavGenVoxelMesh_f()` registered in `SVG_ServerCommand()` in `svg_commands_server.cpp`
- Callable via console: `sv nav_gen_voxelmesh`

## Current Implementation Status

### Completed
- ✅ Data structure definitions
- ✅ CVar registration with sensible defaults matching player physics
- ✅ Memory allocation/deallocation system
- ✅ Command registration
- ✅ Statistics tracking and output
- ✅ Multi-layer detection algorithm
- ✅ Slope walkability checking
- ✅ Content flag detection
- ✅ Ladder content detection
- ✅ World mesh generation with BSP leaf traversal
- ✅ A* traversal pathfinding and movement direction queries
- ✅ CMake integration

### Placeholder/TODO
- ⚠️ Inline model mesh generation (basic structure in place, needs full implementation)
- ⚠️ Clearance calculation
- ⚠️ Inline model tile creation and XY cell sampling

## Notes

- The implementation does NOT change monster/player movement - it only generates data
- The voxelmesh now supports traversal pathfinding via A* helpers
- Generation parameters are designed to match Quake 2 player physics constraints
- The sparse tile structure minimizes memory usage in large maps

## Testing

To test the implementation:
1. Build the project with CMake
2. Start a dedicated server or listen server
3. Load a map
4. Execute `sv nav_gen_voxelmesh` in the console
5. Review the statistics output showing leafs, tiles, cells, and layers generated

## Traversal Pathfinding API

The navigation system now exposes traversal helpers for A* pathfinding:

- `SVG_Nav_GenerateTraversalPathForOrigin` builds a waypoint list between a start and goal.
- `SVG_Nav_QueryMovementDirection` returns a normalized direction toward the next waypoint.
- `SVG_Nav_FreeTraversalPath` releases path memory.

Example output:
```
=== Navigation Voxelmesh Generation ===
Parameters:
  cell_size_xy: 4.0
  z_quant: 2.0
  tile_size: 32
  max_step: 18.0
  max_slope_deg: 45.6
  agent_mins: (-16.0, -16.0, -36.0)
  agent_maxs: (16.0, 16.0, 36.0)
Generating world mesh for X leafs...
Generating inline model mesh for Y models...

=== Generation Statistics ===
  Leafs processed: X
  Inline models: Y
  Total tiles: Z
  Total XY cells: A
  Total layers: B
  Build time: C.CCC seconds
===================================
```
