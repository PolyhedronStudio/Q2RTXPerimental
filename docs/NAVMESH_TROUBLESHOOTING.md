# Navigation Mesh Troubleshooting Guide

## Overview

This guide helps diagnose and fix pathfinding issues in the Q2RTXP navigation system.

## Common Issue: "No Path Found" with Large Z-Differences

### Symptoms
- Monster at Z=16 cannot path to player at Z=84
- Logs show: `[WARNING][NavPath][A*] Pathfinding failed: No path found`
- Console shows: `Open list exhausted`

### Root Causes

1. **Missing Navmesh Connectivity**
   - The navmesh doesn't have walkable surfaces connecting the two Z-levels
   - Stairs, ramps, or elevators aren't properly represented in the navmesh

2. **Edge Validation Rejection**
   - `Nav_CanTraverseStep` is rejecting connections between nodes
   - Step height or drop limits are too restrictive

3. **Obstacles Blocking All Routes**
   - The navmesh generation didn't sample areas around obstacles
   - Physical obstructions prevent all possible paths

## Diagnostic Steps

### 1. Enable Navigation Debug Visualization

```
nav_debug_draw 1
nav_debug_draw_samples 1
nav_debug_draw_tile_bounds 1
```

This will show:
- Blue boxes: Navigation tile bounds
- White ticks: Walkable surface samples
- Red/green lines: Pathfinding attempts

### 2. Check Current Parameter Values

Key cvars to inspect:
```
nav_max_step       // Default: 18 (Quake 2 player step height)
nav_max_drop       // Default: 128 (Maximum safe drop distance)
nav_max_slope_deg  // Default: 70 (Maximum walkable slope in degrees)
nav_cell_size_xy   // Default: 4 (Horizontal grid resolution)
nav_z_quant        // Default: 2 (Vertical quantization step)
```

### 3. Analyze A* Failure Messages

The enhanced diagnostics will show:
```
[WARNING][NavPath][A*] Pathfinding failed: No path found from (...) to (...)
[WARNING][NavPath][A*] Search stats: explored=X nodes, max_nodes=8192, open_list_empty=true
[WARNING][NavPath][A*] Failure reason: Open list exhausted. This typically means:
[WARNING][NavPath][A*]   1. No navmesh connectivity exists between start and goal Z-levels
[WARNING][NavPath][A*]   2. All potential paths are blocked by obstacles
[WARNING][NavPath][A*]   3. Edge validation (Nav_CanTraverseStep) rejected all connections
```

## Solutions

### Solution 1: Increase Step Height (Temporary Fix)

If your map has taller stairs than standard Quake 2:
```
nav_max_step 24  // Or higher, depending on your stair height
```

**Note:** This is a band-aid. The real solution is to ensure stairs are properly sampled during navmesh generation.

### Solution 2: Regenerate Navmesh with Better Sampling

The navmesh is generated at map load. To regenerate:
1. Restart the map: `map <mapname>`
2. Check console for navmesh generation messages
3. Look for warnings about failed tile generation

If navmesh generation is missing stairs/ramps:
- Ensure stairs have proper walkable surfaces (not too steep)
- Check that `nav_max_slope_deg` allows the slope
- Verify stairs aren't flagged as non-solid in the BSP

### Solution 3: Adjust Agent Bounding Box

The agent bbox affects which areas are considered walkable:
```
nav_agent_mins_x -16  // Agent width (negative)
nav_agent_mins_y -16
nav_agent_mins_z 0    // Usually 0 for ground-based agents
nav_agent_maxs_x 16   // Agent width (positive)
nav_agent_maxs_y 16
nav_agent_maxs_z 72   // Agent height (standard player height)
```

If the agent is too large, it may not fit through narrow passages or on stairs.

### Solution 4: Increase Node Search Limit

If the path is very long:
```cpp
// In src\baseq2rtxp\svgame\nav\svg_nav.cpp, line 2429:
constexpr int32_t MAX_SEARCH_NODES = 16384;  // Increased from 8192
```

**Warning:** This increases memory usage and search time.

## Understanding the Z-Gap Warning

When you see:
```
[INFO][NavPath] Z gap (68.0) exceeds step height (18.0). 
Start(...) Goal(...). A* will search for alternative routes.
```

This is **informational**, not an error. It means:
- Direct stepping from start to goal isn't possible (Z-difference too large)
- A* will search for indirect routes (stairs, ramps, detours)
- If A* finds no path, the issue is **connectivity**, not the Z-gap itself

## Policy-Based Step/Drop Limits

Monsters can have custom traversal limits via policy:
```cpp
policy.max_step_height = 24.0;  // Monster can climb 24-unit steps
policy.max_drop_height = 64.0;  // Monster won't drop more than 64 units
policy.cap_drop_height = true;  // Enforce drop limit
```

This allows different monster types to have different capabilities:
- Flying monsters: high drop tolerance, low step requirement
- Ground monsters: standard step/drop limits
- Climbing monsters: high step tolerance

## Advanced: Edge Validation Tuning

The `Nav_CanTraverseStep_ExplicitBBox` function validates each edge during A*. It performs:
1. Horizontal trace from start to end
2. Step-up trace if blocked
3. Step-down trace to find ground

If edges are being rejected incorrectly, check:
- Collision mask (`CM_CONTENTMASK_MONSTERSOLID`)
- Agent bounding box vs. actual geometry clearances
- Step-up/step-down trace distances

## Navmesh Generation Parameters

### Cell Size (`nav_cell_size_xy`)
- **Smaller values (2-4)**: Higher resolution, more memory, slower generation
- **Larger values (8-16)**: Lower resolution, less memory, faster generation
- **Recommendation**: 4 for most maps, 2 for detailed areas

### Z Quantization (`nav_z_quant`)
- **Smaller values (1-2)**: More precise vertical placement
- **Larger values (4-8)**: Coarser vertical grid, may miss thin floors
- **Recommendation**: 2 for most maps

### Tile Size (`nav_tile_size`)
- **Smaller values (16-32)**: More tiles, better memory locality
- **Larger values (64-128)**: Fewer tiles, less overhead
- **Recommendation**: 32 for most maps

## Case Study: Your Specific Issue

### Problem
Monster at (-104.8, -144.1, 16.0) cannot path to player at (-70.5, 33.9, 52.0)
- Z-difference: 36 units (2x the step height)
- Distance: ~240 units
- Logs show: `SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox returned 0`
- **CRITICAL**: No A* diagnostics appearing in logs

### Critical Observation
The enhanced A* diagnostics I added are **NOT appearing** in your logs. This means pathfinding is failing **before** it even reaches the A* search function. The failure is happening in `SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox` at an earlier stage.

### What This Means
The pathfinding system is failing during one of these pre-A* stages:
1. **Start/Goal Node Lookup**: Cannot find a navmesh node at the start or goal position
2. **Navmesh Missing**: No navmesh tiles exist in that area
3. **Invalid Parameters**: The parameters passed to the pathfinding function are invalid

### Diagnosis Steps

#### Step 1: Check Navmesh Coverage
```
nav_debug_draw 1
nav_debug_draw_samples 1
nav_debug_draw_tile_bounds 1
```

Then:
1. Teleport to monster position: `setpos -104.8 -144.1 16.0`
2. Look around - do you see white sample ticks on the ground?
3. Teleport to player position: `setpos -70.5 33.9 52.0`  
4. Look around - do you see white sample ticks here too?

**If you DON'T see white ticks at either position**, the navmesh wasn't generated there. This is the problem.

####Step 2: Check Navmesh Generation Logs
When you loaded the map, the console should have shown:
```
[Nav] Generating navmesh for map 'mapname'...
[Nav] Generated X tiles in Y seconds
```

Look for:
- Warnings about failed tile generation
- Errors during navmesh creation  
- Messages about areas being skipped

#### Step 3: Force Navmesh Regeneration
Try:
```
map <your_mapname>
```

Watch the console during map load. If you see errors or very few tiles generated, the navmesh generation failed.

### Likely Root Cause

Based on your logs showing Z=16 to Z=52 with no path, the most likely cause is:

**The navmesh does NOT have walkable surfaces connecting those two Z-levels.**

This could be because:
1. **No stairs/ramps exist** between those heights
2. **Stairs exist but weren't sampled** during navmesh generation (too steep, wrong geometry)
3. **Map geometry blocking** navmesh generation in that area

### Quick Tests

#### Test 1: Can the monster path on the SAME Z-level?
1. Stand at Z=16 (same as monster)
2. Spawn monster
3. Does it path to you?

**If YES**: The problem is vertical connectivity. The navmesh has no stairs/ramps.  
**If NO**: The problem is navmesh coverage at Z=16 is missing entirely.

#### Test 2: Increase Step Height Temporarily
```
nav_max_step 40
```

Then try again. 

**If pathfinding suddenly works**: Your stairs are between 18-40 units high (non-standard).  
**If it still fails**: The problem is navmesh connectivity, not step height.

### Solutions

#### If Navmesh is Missing
- Check that your map has proper collision geometry
- Verify surfaces are walkable (not too steep)
- Check `nav_max_slope_deg` (default 70°)
- Regenerate navmesh after fixing geometry

#### If Stairs Aren't Sampled
- Increase `nav_cell_size_xy` (try 8 instead of 4) for coarser but more robust sampling
- Check stair geometry - each step should be ≤ 18 units high
- Ensure stairs have solid collision surfaces

#### If No Vertical Connection Exists
You need to either:
1. **Add stairs/ramps** to your map geometry
2. **Use alternative navigation** (direct pursuit, manual waypoints)
3. **Accept that monsters can't reach that area**

### Expected Behavior After Fix

Once fixed, you should see:
```
[DEBUG][NavPath] RebuildPathToWithAgentBBox: start(...) goal(...)
[DEBUG][NavPath] nav_max_step=18.0
[DEBUG][NavPath] nav_max_drop=128.0
[WARNING][NavPath] Z gap (36.0) exceeds nav_max_step (18.0). A* will search for alternative routes.
[WARNING][NavPath][A*] Pathfinding failed: No path found from (...) to (...)
[WARNING][NavPath][A*] Search stats: explored=X nodes
[WARNING][NavPath][A*] Failure reason: ...
```

The fact that you're NOT seeing the A* messages means it's failing before even trying A*.

### Monster at (-104.8, -144.1, 16.0) cannot path to player at (-70.5, 33.9, 52.0)
- Z-difference: 36 units (2x the step height)
- Distance: ~240 units
- A* consistently fails with "open list exhausted"

### Diagnosis
1. Enable debug visualization: `nav_debug_draw 1`
2. Stand at both positions and observe:
   - Are there white sample ticks at both heights?
   - Do the ticks form a continuous path?
   - Is there a visible gap in coverage?

### Likely Causes
1. **No stairs/ramps in navmesh**: The vertical connection wasn't sampled
2. **Obstacle blocking path**: The navmesh has a hole where the obstacle is
3. **Step validation too strict**: `nav_max_step 18` is rejecting valid stair connections

### Recommended Actions
1. Try `nav_max_step 24` temporarily
2. If that doesn't work, check for navmesh coverage gaps with debug visualization
3. If gaps exist, the navmesh needs regeneration with adjusted parameters
4. Consider adding intermediate waypoints or func_areaportal hints

## Contact & Support

For further assistance:
- Check console for detailed A* failure diagnostics
- Save the full console log including navmesh generation messages
- Take screenshots with `nav_debug_draw 1` enabled
- Report issue with map name, entity positions, and parameters used
