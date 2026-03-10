|Function Name|Total CPU \[unit, %\]|Self CPU \[unit, %\]|Module|Category|
|-|-|-|-|-|
|\|\|\|\|\|\|\|\|\|\|\|\| + SVG\_RunFrame|54562 \(56,43%\)|15 \(0,02%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\| + SVG\_Nav\_DebugDraw|39309 \(40,66%\)|1251 \(1,29%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + NavDebug\_FilterTile|32874 \(34,00%\)|876 \(0,91%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - NavDebug\_ParseTileFilter|31559 \(32,64%\)|1052 \(1,09%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \_RTC\_CheckStackVars|413 \(0,43%\)|413 \(0,43%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+53955\(\_RTC\_CheckStackVars\)|21 \(0,02%\)|21 \(0,02%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + 0xfffff806827ffe27|3 \(0,00%\)|0 \(0,00%\)|\[Unknown Code\]||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+53355\(?NavDebug\_ParseTileFilter\@\@YA?B\_NPEAH0\@Z\)|2 \(0,00%\)|2 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + NavDebug\_DrawTileBBox|2326 \(2,41%\)|1402 \(1,45%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::vector\<nav\_tile\_s,std::allocator\<nav\_tile\_s\> \>::size|1472 \(1,52%\)|1469 \(1,52%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::vector\<nav\_tile\_s,std::allocator\<nav\_tile\_s\> \>::operator\[\]|1163 \(1,20%\)|1159 \(1,20%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + SVG\_Nav\_Leaf\_GetTileIds|81 \(0,08%\)|50 \(0,05%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+12455\(??A?$vector\@Unav\_tile\_s\@\@V?$allocator\@Unav\_tile\_s\@\@\@std\@\@\@std\@\@QEBAAEBUnav\_tile\_s\@\@\_K\@Z\)|32 \(0,03%\)|32 \(0,03%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + \@ILT+29015\(?NavDebug\_DrawTileBBox\@\@YAXPEBUnav\_mesh\_s\@\@PEBUnav\_tile\_s\@\@\@Z\)|31 \(0,03%\)|30 \(0,03%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+22875\(?size\@?$vector\@Unav\_tile\_s\@\@V?$allocator\@Unav\_tile\_s\@\@\@std\@\@\@std\@\@QEBA\_KXZ\)|29 \(0,03%\)|29 \(0,03%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - NavDebug\_FilterLeaf|24 \(0,02%\)|24 \(0,02%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + NavDebug\_PurgeCachedPaths|10 \(0,01%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+40135\(?SVG\_Nav\_Leaf\_GetTileIds\@\@YA?AU?$pair\@PEAHH\@std\@\@PEAUnav\_leaf\_data\_s\@\@\@Z\)|3 \(0,00%\)|3 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+57820\(?NavDebug\_FilterTile\@\@YA?B\_NHH\@Z\)|3 \(0,00%\)|3 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+1720\(?NavDebug\_FilterLeaf\@\@YA?B\_NH\@Z\)|2 \(0,00%\)|2 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - NavDebug\_Enabled|2 \(0,00%\)|2 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - 0xfffff806827ffe27|2 \(0,00%\)|0 \(0,00%\)|\[Unknown Code\]||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - 0xfffff8068280505d|2 \(0,00%\)|0 \(0,00%\)|\[Unknown Code\]||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - \_RTC\_CheckStackVars|1 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + 0xfffff806827ff6c1|1 \(0,00%\)|0 \(0,00%\)|\[Unknown Code\]||
|\|\|\|\|\|\|\|\|\|\|\|\|\| + SVG\_Nav\_ProcessRequestQueue|10333 \(10,69%\)|3 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + Nav\_AStar\_Step|10264 \(10,62%\)|4 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::remove\_if\<std::\_Vector\_iterator\<std::\_Vector\_val\<std::\_Simple\_types\<nav\_request\_entry\_t\> \> \>,\`SVG\_Nav\_ProcessRequestQueue'::\`2'::\<lambda\_2\> \>|20 \(0,02%\)|2 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + Nav\_AStar\_Reset|15 \(0,02%\)|0 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::vector\<nav\_request\_entry\_t,std::allocator\<nav\_request\_entry\_t\> \>::erase|8 \(0,01%\)|0 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::vector\<nav\_request\_entry\_t,std::allocator\<nav\_request\_entry\_t\> \>::end|5 \(0,01%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - NavRequest\_HandleFailure|3 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - NavRequest\_GetPrepareRequestBudget|2 \(0,00%\)|2 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - NavRequest\_LogQueueDiagnostics|2 \(0,00%\)|2 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + SVG\_Nav\_GetAsyncRequestExpansions|2 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + SVG\_Nav\_IsAsyncNavEnabled|2 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - PF\_GetRealSystemTime|2 \(0,00%\)|0 \(0,00%\)|q2rtxperimental\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - NavRequest\_GetStepRequestBudget|1 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - std::vector\<nav\_request\_entry\_t,std::allocator\<nav\_request\_entry\_t\> \>::empty|1 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + NavRequest\_FinalizePathForEntry|1 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + NavRequest\_PrepareAStarForEntry|1 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::\_Vector\_iterator\<std::\_Vector\_val\<std::\_Simple\_types\<nav\_request\_entry\_t\> \> \>::~\_Vector\_iterator\<std::\_Vector\_val\<std::\_Simple\_types\<nav\_request\_entry\_t\> \> \>|1 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::\_Vector\_const\_iterator\<std::\_Vector\_val\<std::\_Simple\_types\<nav\_request\_entry\_t\> \> \>::~\_Vector\_const\_iterator\<std::\_Vector\_val\<std::\_Simple\_types\<nav\_request\_entry\_t\> \> \>|1 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::\_Iterator\_base12::~\_Iterator\_base12|1 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + std::\_Iterator\_base12::\_Orphan\_me\_locked\_v3|1 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + msvcp140d.dll!0x00007ff819f9eb37|1 \(0,00%\)|0 \(0,00%\)|msvcp140d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| + msvcp140d.dll!0x00007ff819fa0794|1 \(0,00%\)|0 \(0,00%\)|msvcp140d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\| - ntdll.dll!0x00007ff86e83f25f|1 \(0,00%\)|1 \(0,00%\)|ntdll|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\| - SVG\_RunEntity|4414 \(4,57%\)|35 \(0,04%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\| - SVG\_Client\_BeginServerFrame|192 \(0,20%\)|0 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - EndClientServerFrames|106 \(0,11%\)|0 \(0,00%\)|svgamex86\_64\_d|Kernel|
|\|\|\|\|\|\|\|\|\|\|\|\|\| - SVG\_Entity\_IsActive|61 \(0,06%\)|61 \(0,06%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - CheckEntityEventClearing|45 \(0,05%\)|14 \(0,01%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - CheckEntityGroundChange|31 \(0,03%\)|26 \(0,03%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - CheckSetOldOrigin|19 \(0,02%\)|19 \(0,02%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - std::mersenne\_twister\_engine\<unsigned \_\_int64,64,312,156,31,-5403634167711393303,29,6148914691236517205,17,8202884508482404352,37,-2270628950310912,43,6364136223846793005\>::seed|7 \(0,01%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - SVG\_Lua\_CallBack\_BeginServerFrame|6 \(0,01%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - CheckUpdatePusherLastOrigin|4 \(0,00%\)|4 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - SVG\_Nav\_RefreshInlineModelRuntime|4 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+13090\(?SVG\_Lua\_CallBack\_BeginServerFrame\@\@YAXXZ\)|3 \(0,00%\)|3 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - svg\_gamemode\_singleplayer\_t::PreCheckGameRuleConditions|3 \(0,00%\)|3 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - SVG\_PushMove\_UpdateMoveWithEntities|3 \(0,00%\)|2 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - SVG\_Lua\_CallBack\_RunFrame|2 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+2380\(?SVG\_RunEntity\@\@YAXPEAUsvg\_base\_edict\_t\@\@\@Z\)|1 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - \@ILT+28965\(?PostCheckGameRuleConditions\@svg\_gamemode\_singleplayer\_t\@\@UEAAXXZ\)|1 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - svg\_edict\_pool\_t::EdictForNumber|1 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - svg\_gamemode\_singleplayer\_t::PostCheckGameRuleConditions|1 \(0,00%\)|1 \(0,00%\)|svgamex86\_64\_d||
|\|\|\|\|\|\|\|\|\|\|\|\|\| - QMTime::operator+=|1 \(0,00%\)|0 \(0,00%\)|svgamex86\_64\_d||