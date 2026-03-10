A very thorough explanation about the line-of-sight (LOS) simplification process in the A* pathfinding implementation, including how it evaluates candidate waypoints for LOS collapse, applies distance and clearance thresholds, and ensures safe navigation through narrow passages.

This diagram shows the actual synchronous traversal-query execution path in `src/baseq2rtxp/svgame/nav/svg_nav_traversal.cpp` for a monster or other caller once path generation enters `SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox()`. From there the flow proceeds through endpoint resolution, the direct-LOS shortcut attempt, coarse corridor building, fine `Nav_AStarSearch()`, and finally the post-A* waypoint simplification pass where `SVG_Nav_SimplifyPathPoints()` calls `Nav_LOS_HasClearanceMarginForLongSpan()`.

In other words: this is not a generic nav concept diagram. It documents the real code-path ordering used by the sync traversal query when a direct shortcut is not already enough to finish the request.
```mermaid
flowchart LR
  subgraph "Raw Search"
    A["A* / hierarchy route"] --> B["Raw waypoints (tile/cell layers)"]
  end
  B --> C["LOS candidate evaluation"]
  C --> D{"Long span >= nav_simplify_long_span_min_distance?"}
  D -- no --> E["Normal LOS collapse allowed"]
  D -- yes --> F["Require clearance margin"]
  F --> G{"Clearance >= nav_simplify_clearance_margin?"}
  G -- yes --> H["Collapse span, drop intermediate nodes"]
  G -- no --> I["Keep detailed corridor to preserve safety"]
  H --> J["Simplified path returned to mover"]
  I --> J
  J --> K["Mover follows path with step/drop validation"]
```