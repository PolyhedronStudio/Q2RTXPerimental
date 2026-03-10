LOS is intended to work as a direct-shortcut accelerator, and in this codebase it is already used to bypass full pathfinding when safe. It is not meant to be the only stair/ledge solution, but it should handle traversable stairs if the intermediate node validation is correct.
Your log shows two separate problems:
1.	A real async corridor bug
2.	A too-conservative stair/step validator
3.	Heavy debug/log spam causing the FPS drop
---
Evidence from the log
1. The FPS drop is very likely log-driven first
The paste is flooded with repeated:
•	[NavAsync][Stats]
•	[NAV DEBUG][ThinkGate]
•	[NAV STATUS]
•	repeated onTouch
That amount of dprintf traffic alone can tank frame time during movement tests.
2. The failures are mostly StepTest, not occupancy
Examples from your log:
•	handle=19: StepTest=586, TileRouteFilter=339, Occupancy=0
•	handle=22: StepTest=586, TileRouteFilter=339, Occupancy=0
•	handle=24: StepTest=322, TileRouteFilter=292, Occupancy=0
So this is not a dynamic occupancy issue.
3. Some LOS shortcuts do work
You have many successful:
•	direct_shortcut los=1
So LOS is not fundamentally broken. It works on flatter / simpler spans.
---
The biggest concrete bug
In svg_nav_traversal_async.cpp:317-336, Nav_AStar_BuildBufferedTileRouteFilter() returns inside the for loop:
•	file: svg_nav_traversal_async.cpp
•	lines: 317-336
That means the buffered route filter is built for only the first coarse-route tile, not the whole route.
Result:
•	the fine search is artificially constrained
•	tileRouteFilter rejections spike
•	valid stair/connector routes fail early
•	retries keep happening
•	extra logging makes FPS worse
This one is a real bug.
---
Second issue: the current step validator is too conservative
LOS eventually relies on:
•	SVG_Nav_HasLineOfSight() in svg_nav_traversal.cpp:1455
•	Nav_LOS_DDA() in svg_nav_traversal.cpp:1303
•	Nav_CanTraverseStep_ExplicitBBox_NodeRefs() and then
•	Nav_CanTraverseStep_ExplicitBBox_Single()
But Nav_CanTraverseStep_ExplicitBBox_Single() is not doing a true movement-style step trace anymore. It mostly checks:
•	local adjacency
•	walkable flags
•	clearance
•	requiredUp <= stepSize
•	requiredDown <= dropCap
That is much more conservative than real monster movement in svg_mmove.cpp.
So if a stair/ledge is physically traversable by monster movement, but the nav layers resolve into slightly awkward intermediate hops, LOS/A* can still reject it as StepTest.
---
Important design conclusion
LOS here is acting as a fast-path shortcut, not a full replacement for navigation.
That matches the code comment in svg_nav_traversal.cpp:
•	SVG_Nav_HasLineOfSight(...)
•	“not a replacement pathfinder”
So:
•	If your expectation is “monster should use LOS alone to solve stairs” → that expectation is too strong.
•	If your expectation is “LOS shortcut should still accept ordinary stairs/short ledges when they are clearly walkable” → that is reasonable, and the current implementation is failing that.
---
Why the monster gets stuck on stairs / ledges
Most likely chain:
1.	Sound goal updates enqueue many async requests.
2.	The coarse corridor is built.
3.	The buffered tile filter is broken by the early return bug.
4.	Fine A* explores a crippled corridor.
5.	Remaining candidate hops get rejected by conservative StepTest.
6.	Failing requests retry/back off.
7.	Logging spam amplifies the frame drop.
---
What to fix first
Priority 1
Fix Nav_AStar_BuildBufferedTileRouteFilter() so it does not return from inside the loop.
That should reduce:
•	tileRouteFilter rejects
•	false path failures
•	repeated rebuild churn
Priority 2
Disable or heavily throttle the debug spam before judging performance.
Right now the “no FPS drops” goal cannot be evaluated fairly from this log.
Priority 3
Make LOS shortcut use a more authoritative traversal test for vertical connectors.
Best options:
•	reuse a simplified monster-movement step trace derived from SVG_MMove_StepSlideMove(), or
•	keep LOS only for flatter spans and force full corridor/A* on larger vertical deltas
---
Best practical solution
If your end goal is:
“sfxfollow monster should traverse stairs reliably with no FPS drops”
then the best architecture is:
1.	Use A / corridor as the authoritative route*
2.	Use LOS only as path simplification / shortcutting
3.	Do not let LOS be the only solver for multi-level connectors
That is more robust than “LOS-only traversal”.
---
Recommended behavior rule
A good rule would be:
•	allow direct LOS shortcut only when:
•	vertical delta is small, or
•	sampled intermediate hops are clearly valid
•	otherwise:
•	use corridor path
•	then simplify the finalized path with LOS
That gives:
•	reliable stairs
•	fewer StepTest false negatives
•	less thrashing
---
Bottom line
You are not wrong to expect stair traversal to work better than this.
From the code and the log, the current failure is implementation-related, not proof that LOS is the wrong idea.
Most likely root causes
1.	Buffered corridor bug in svg_nav_traversal_async.cpp
2.	Over-conservative step validation
3.	Debug output flood causing perceived FPS loss
If you want, the next best step is for me to propose a minimal patch plan for:
•	the corridor bug,
•	the logging reduction,
•	and a safer LOS fallback policy.
