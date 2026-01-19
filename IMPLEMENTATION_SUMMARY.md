# Navigation Voxelmesh Implementation Summary

## Overview

This implementation adds the `nav_gen_voxelmesh` server console command to Q2RTXPerimental for generating navigation voxelmesh data at runtime. The voxelmesh is a sparse, multi-layered structure suitable for A* pathfinding.

## Files Added

1. **src/baseq2rtxp/svgame/nav/svg_nav.h** (3,672 bytes)
   - Data structure definitions
   - API declarations
   - CVar externs

2. **src/baseq2rtxp/svgame/nav/svg_nav.cpp** (15,439 bytes)
   - Core implementation
   - World mesh generation framework
   - Inline model mesh generation framework
   - Multi-layer detection algorithm
   - Memory management
   - Statistics tracking

3. **src/baseq2rtxp/svgame/nav/README.md** (5,328 bytes)
   - Comprehensive documentation
   - Usage guide
   - Configuration reference
   - Implementation notes

## Files Modified

1. **src/baseq2rtxp/svgame/svg_main.cpp**
   - Added `#include "svgame/nav/svg_nav.h"`
   - Added `SVG_Nav_Init()` call in `SVG_InitGame()`
   - Added `SVG_Nav_Shutdown()` call in `SVG_ShutdownGame()`

2. **src/baseq2rtxp/svgame/svg_commands_server.cpp**
   - Added `#include "svgame/nav/svg_nav.h"`
   - Added `ServerCommand_NavGenVoxelMesh_f()` function
   - Registered command in `SVG_ServerCommand()`

3. **CMakeSources.cmake**
   - Added `baseq2rtxp/svgame/nav/svg_nav.cpp` to `SRC_BASEQ2RTXP_SVGAME`
   - Added `baseq2rtxp/svgame/nav/svg_nav.h` to `HEADERS_BASEQ2RTXP_SVGAME`

## Command Usage

```
sv nav_gen_voxelmesh
```

Generates the navigation voxelmesh and outputs statistics including:
- Leafs processed
- Inline models detected
- Total tiles, XY cells, and Z layers generated
- Build time in seconds

## Configuration CVars

| CVar | Default | Description |
|------|---------|-------------|
| `nav_cell_size_xy` | 4 | XY grid cell size |
| `nav_z_quant` | 2 | Z-axis quantization |
| `nav_tile_size` | 32 | Cells per tile dimension |
| `nav_max_step` | 18 | Max step height (matches PM_STEP_MAX_SIZE) |
| `nav_max_slope_deg` | 45.57 | Max walkable slope (matches PM_STEP_MIN_NORMAL) |
| `nav_agent_mins_x/y/z` | -16, -16, -36 | Agent bbox mins (player standing) |
| `nav_agent_maxs_x/y/z` | 16, 16, 36 | Agent bbox maxs (player standing) |

## Key Features

### Data Structures
- **nav_mesh_t**: Main mesh with world and inline model data
- **nav_leaf_data_t**: Per-BSP-leaf tiles
- **nav_inline_model_data_t**: Per-brush-model tiles
- **nav_tile_t**: Sparse tile with presence bitset
- **nav_xy_cell_t**: XY cell with multiple Z layers
- **nav_layer_t**: Single Z layer with flags (walkable, water, lava, slime, ladder)

### Generation Approach
1. **World Mesh**: Uses world-only collision traces (`gi.trace` with nullptr passent)
2. **Inline Model Mesh**: Uses per-model traces (`gi.clip`) in local space
3. **Multi-Layer Detection**: Repeated downward traces find all walkable floors per XY column
4. **Slope Checking**: Validates normal.z >= cos(max_slope_deg)
5. **Content Detection**: Identifies water, lava, slime based on trace contents

### Memory Management
- Uses `gi.TagMalloc` / `gi.TagFree` with `TAG_SVGAME_LEVEL`
- Automatically freed on level unload
- Can be regenerated multiple times

### Code Quality
- Named constants (DEG_TO_RAD, MICROSECONDS_PER_SECOND)
- Input validation (bbox, z_quant)
- Thread-safety documentation
- Comprehensive comments

## Implementation Status

### Complete ✅
- Data structure design
- CVar registration with sensible defaults
- Command registration
- Memory allocation/deallocation
- Statistics tracking and output
- Multi-layer detection algorithm
- Slope walkability checking
- Content flag detection
- Input validation
- CMake integration
- Documentation

### Placeholder/TODO ⚠️
- World mesh generation (framework in place)
  - Leaf bounds iteration
  - Tile creation
  - XY grid sampling
  - Downward trace calls
- Inline model mesh generation (framework in place)
  - Model enumeration
  - Per-model collision testing
- Clearance calculation
- Ladder detection

## Next Steps

To complete the implementation:

1. **World Mesh Generation**
   - Iterate through BSP leafs (bsp->leafs[0..numleafs])
   - Extract leaf bounds (mins/maxs)
   - Create tiles covering leaf bounds with tile_size spacing
   - For each tile, sample XY positions at cell_size_xy spacing
   - Call FindWalkableLayers() for each XY position
   - Store results in nav_tile_t structures

2. **Inline Model Mesh Generation**
   - Iterate through inline models (bsp->models[1..nummodels])
   - For each model, extract bounds from mmodel_t
   - Create tiles in model local space
   - Use gi.clip() with model entity for collision tests
   - Store results keyed by model index

3. **Testing**
   - Build with CMake 4.1+
   - Load various maps (small, medium, large)
   - Execute `sv nav_gen_voxelmesh`
   - Verify statistics output
   - Check memory usage
   - Validate multi-layer detection on complex geometry

## Performance Considerations

- Sparse tile structure minimizes memory for empty space
- Presence bitsets reduce per-cell overhead
- Z-quantization compacts vertical layer storage
- Configurable tile_size allows memory/precision tradeoff
- Static temp array in FindWalkableLayers avoids repeated allocations

## Compatibility

- Does NOT modify monster/player movement
- Only generates data for future pathfinding
- No impact on gameplay physics
- Safe to regenerate at any time

## Code Review

All code review feedback has been addressed:
- ✅ Use DEG_TO_RAD constant instead of inline calculation
- ✅ Use MICROSECONDS_PER_SECOND constant
- ✅ Validate agent bounding box (mins < maxs)
- ✅ Validate nav_z_quant > 0 to prevent division by zero
- ✅ Document thread-safety concern for static array
