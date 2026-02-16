param(
    [string]$LogsPath = "",
    [string]$OutputCsv = "",
    [string]$OutputMarkdown = "",
    [string]$BudgetFile = "",
    [switch]$FailOnBudget
)

$ErrorActionPreference = "Stop"

function Parse-NullableDouble {
    param(
        [object]$Row,
        [string]$Name
    )

    if (-not ($Row.PSObject.Properties.Name -contains $Name)) {
        return $null
    }

    $raw = [string]$Row.$Name
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return $null
    }

    $value = 0.0
    $ok = [double]::TryParse(
        $raw,
        [System.Globalization.NumberStyles]::Float,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$value
    )
    if (-not $ok) {
        throw "Budget parse failed for '$Name': $raw"
    }

    return [double]$value
}

function Parse-NullableInt {
    param(
        [object]$Row,
        [string]$Name
    )

    if (-not ($Row.PSObject.Properties.Name -contains $Name)) {
        return $null
    }

    $raw = [string]$Row.$Name
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return $null
    }

    $value = 0
    $ok = [int]::TryParse(
        $raw,
        [System.Globalization.NumberStyles]::Integer,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$value
    )
    if (-not $ok) {
        throw "Budget parse failed for '$Name': $raw"
    }

    return [int]$value
}

function Get-BudgetRowForMap {
    param(
        [object[]]$BudgetRows,
        [string]$Map
    )

    foreach ($row in $BudgetRows) {
        if ($row.map -eq $Map) {
            return $row
        }
    }

    foreach ($row in $BudgetRows) {
        if ($row.map -eq "*") {
            return $row
        }
    }

    return $null
}

function Parse-ProfileLog {
    param(
        [string]$LogPath
    )

    $logText = Get-Content -LiteralPath $LogPath -Raw

    $head = [regex]::Match($logText, "backend=(\S+)\s+mesh_loaded=(\d+)\s+map=(\S+)")
    if (-not $head.Success) {
        return $null
    }

    $mesh = [regex]::Match($logText, "mesh world_tiles=(\d+)\s+inline_models=(\d+)\s+total_tiles=(\d+)\s+total_cells=(\d+)\s+total_layers=(\d+)\s+cell_size=([0-9.+-]+)\s+z_quant=([0-9.+-]+)\s+tile_size=(\d+)")
    if (-not $mesh.Success) {
        return $null
    }

    $generation = [regex]::Match($logText, "generation world_ms=([0-9.+-]+)\s+inline_ms=([0-9.+-]+)\s+total_ms=([0-9.+-]+)\s+generated_at_ms=(\d+)")
    $worldGenLegacy = [regex]::Match($logText, "World mesh generation complete \(tile-first\) -\s+([0-9.+-]+)\s+ms")
    $inlineGenLegacy = [regex]::Match($logText, "Inline models' navmesh generation complete -\s+([0-9.+-]+)\s+ms")
    $totalGenLegacy = [regex]::Match($logText, "Overall generation time:\s+([0-9.+-]+)\s+ms")

    $asyncQueue = [regex]::Match($logText, "async queue_depth=(\d+)\s+queue_peak=(\d+)\s+enqueued=(\d+)\s+refreshed=(\d+)\s+processed=(\d+)\s+completed=(\d+)\s+failed=(\d+)\s+cancelled=(\d+)\s+prepare_failures=(\d+)\s+expansions=(\d+)")
    if (-not $asyncQueue.Success) {
        return $null
    }

    $asyncWait = [regex]::Match($logText, "async wait_avg_ms=([0-9.+-]+)\s+wait_max_ms=(\d+)\s+wait_samples=(\d+)\s+frame_elapsed_ms=(\d+)\s+frame_processed=(\d+)\s+frame_expansions=(\d+)\s+budget_req=(\d+)\s+budget_expand=(\d+)\s+budget_queue_ms=([0-9.+-]+)\s+configured_queue_budget_ms=([0-9.+-]+)\s+over_budget_frames=(\d+)")
    if (-not $asyncWait.Success) {
        return $null
    }
    $backendDetour = [regex]::Match($logText, "backend detour_queries=(\d+)\s+detour_success=(\d+)\s+detour_failed=(\d+)\s+detour_avg_ms=([0-9.+-]+)\s+detour_max_ms=(\d+)\s+detour_points_before=(\d+)\s+detour_points_after=(\d+)")

    $worldGenMs = 0.0
    $inlineGenMs = 0.0
    $totalGenMs = 0.0
    $generatedAtMs = 0
    $detourQueries = 0
    $detourSuccess = 0
    $detourFailed = 0
    $detourAvgMs = 0.0
    $detourMaxMs = 0
    $detourPointsBefore = 0
    $detourPointsAfter = 0

    if ($generation.Success) {
        $worldGenMs = [double]$generation.Groups[1].Value
        $inlineGenMs = [double]$generation.Groups[2].Value
        $totalGenMs = [double]$generation.Groups[3].Value
        $generatedAtMs = [int64]$generation.Groups[4].Value
    } else {
        if ($worldGenLegacy.Success) { $worldGenMs = [double]$worldGenLegacy.Groups[1].Value }
        if ($inlineGenLegacy.Success) { $inlineGenMs = [double]$inlineGenLegacy.Groups[1].Value }
        if ($totalGenLegacy.Success) { $totalGenMs = [double]$totalGenLegacy.Groups[1].Value }
    }
    if ($backendDetour.Success) {
        $detourQueries = [int]$backendDetour.Groups[1].Value
        $detourSuccess = [int]$backendDetour.Groups[2].Value
        $detourFailed = [int]$backendDetour.Groups[3].Value
        $detourAvgMs = [double]$backendDetour.Groups[4].Value
        $detourMaxMs = [int64]$backendDetour.Groups[5].Value
        $detourPointsBefore = [int64]$backendDetour.Groups[6].Value
        $detourPointsAfter = [int64]$backendDetour.Groups[7].Value
    }

    return [PSCustomObject]@{
        log_path = $LogPath
        log_timestamp = (Get-Item -LiteralPath $LogPath).LastWriteTimeUtc.ToString("o")
        map = $head.Groups[3].Value
        backend = $head.Groups[1].Value
        mesh_loaded = [int]$head.Groups[2].Value

        world_tiles = [int]$mesh.Groups[1].Value
        inline_models = [int]$mesh.Groups[2].Value
        total_tiles = [int]$mesh.Groups[3].Value
        total_cells = [int]$mesh.Groups[4].Value
        total_layers = [int]$mesh.Groups[5].Value
        cell_size = [double]$mesh.Groups[6].Value
        z_quant = [double]$mesh.Groups[7].Value
        tile_size = [int]$mesh.Groups[8].Value

        generation_world_ms = [double]$worldGenMs
        generation_inline_ms = [double]$inlineGenMs
        generation_total_ms = [double]$totalGenMs
        generation_generated_at_ms = [int64]$generatedAtMs

        async_queue_depth = [int]$asyncQueue.Groups[1].Value
        async_queue_peak = [int]$asyncQueue.Groups[2].Value
        async_enqueued = [int]$asyncQueue.Groups[3].Value
        async_refreshed = [int]$asyncQueue.Groups[4].Value
        async_processed = [int]$asyncQueue.Groups[5].Value
        async_completed = [int]$asyncQueue.Groups[6].Value
        async_failed = [int]$asyncQueue.Groups[7].Value
        async_cancelled = [int]$asyncQueue.Groups[8].Value
        async_prepare_failures = [int]$asyncQueue.Groups[9].Value
        async_expansions = [int64]$asyncQueue.Groups[10].Value

        async_wait_avg_ms = [double]$asyncWait.Groups[1].Value
        async_wait_max_ms = [int64]$asyncWait.Groups[2].Value
        async_wait_samples = [int]$asyncWait.Groups[3].Value
        async_frame_elapsed_ms = [int64]$asyncWait.Groups[4].Value
        async_frame_processed = [int]$asyncWait.Groups[5].Value
        async_frame_expansions = [int]$asyncWait.Groups[6].Value
        async_budget_requests = [int]$asyncWait.Groups[7].Value
        async_budget_expansions = [int]$asyncWait.Groups[8].Value
        async_budget_queue_ms = [double]$asyncWait.Groups[9].Value
        async_budget_queue_configured_ms = [double]$asyncWait.Groups[10].Value
        async_over_budget_frames = [int]$asyncWait.Groups[11].Value

        detour_queries = [int]$detourQueries
        detour_success = [int]$detourSuccess
        detour_failed = [int]$detourFailed
        detour_avg_ms = [double]$detourAvgMs
        detour_max_ms = [int64]$detourMaxMs
        detour_points_before = [int64]$detourPointsBefore
        detour_points_after = [int64]$detourPointsAfter
    }
}

function New-StatRow {
    param(
        [string]$Name,
        [double[]]$Values
    )

    if (-not $Values -or $Values.Count -eq 0) {
        return [PSCustomObject]@{
            Metric = $Name
            Min = ""
            Avg = ""
            Max = ""
        }
    }

    $sum = 0.0
    foreach ($v in $Values) {
        $sum += $v
    }

    return [PSCustomObject]@{
        Metric = $Name
        Min = ("{0:N2}" -f (($Values | Measure-Object -Minimum).Minimum))
        Avg = ("{0:N2}" -f ($sum / $Values.Count))
        Max = ("{0:N2}" -f (($Values | Measure-Object -Maximum).Maximum))
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $LogsPath) {
    $LogsPath = Join-Path $repoRoot "build-nav/nav-regression-logs"
}

if (-not (Test-Path -LiteralPath $LogsPath)) {
    throw "Logs path not found: $LogsPath"
}

if (-not $OutputCsv) {
    $OutputCsv = Join-Path $LogsPath "nav_profile_dashboard.csv"
}
if (-not $OutputMarkdown) {
    $OutputMarkdown = Join-Path $LogsPath "nav_profile_dashboard.md"
}

$logFiles = Get-ChildItem -LiteralPath $LogsPath -File -Filter "smoke_*.log" | Sort-Object LastWriteTimeUtc
if ($logFiles.Count -eq 0) {
    throw "No smoke logs found in: $LogsPath"
}

$allRows = New-Object System.Collections.Generic.List[object]
foreach ($log in $logFiles) {
    $row = Parse-ProfileLog -LogPath $log.FullName
    if ($null -ne $row) {
        $allRows.Add($row)
    }
}

if ($allRows.Count -eq 0) {
    throw "No parseable nav_profile_dump entries found in smoke logs."
}

$latestByMap = @($allRows `
    | Group-Object map `
    | ForEach-Object { $_.Group | Sort-Object log_timestamp -Descending | Select-Object -First 1 } `
    | Sort-Object map)

$latestByMap | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding UTF8

$stats = New-Object System.Collections.Generic.List[object]
$stats.Add((New-StatRow -Name "generation_total_ms" -Values @($latestByMap | ForEach-Object { [double]$_.generation_total_ms })))
$stats.Add((New-StatRow -Name "generation_world_ms" -Values @($latestByMap | ForEach-Object { [double]$_.generation_world_ms })))
$stats.Add((New-StatRow -Name "generation_inline_ms" -Values @($latestByMap | ForEach-Object { [double]$_.generation_inline_ms })))
$stats.Add((New-StatRow -Name "async_wait_avg_ms" -Values @($latestByMap | ForEach-Object { [double]$_.async_wait_avg_ms })))
$stats.Add((New-StatRow -Name "async_frame_elapsed_ms" -Values @($latestByMap | ForEach-Object { [double]$_.async_frame_elapsed_ms })))
$stats.Add((New-StatRow -Name "world_tiles" -Values @($latestByMap | ForEach-Object { [double]$_.world_tiles })))
$stats.Add((New-StatRow -Name "detour_avg_ms" -Values @($latestByMap | ForEach-Object { [double]$_.detour_avg_ms })))
$stats.Add((New-StatRow -Name "detour_points_after" -Values @($latestByMap | ForEach-Object { [double]$_.detour_points_after })))

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Nav Profile Dashboard")
$md.Add("")
$md.Add(("Generated (UTC): {0}" -f (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss")))
$md.Add("")
$md.Add("## Per-map Latest")
$md.Add("")
$md.Add("| Map | Backend | World Tiles | Total Tiles | Gen Total ms | Queue Wait Avg ms | Queue Frame ms | Detour Avg ms | Detour Points After | Over Budget Frames |")
$md.Add("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
foreach ($row in $latestByMap) {
    $md.Add(("| {0} | {1} | {2} | {3} | {4:N2} | {5:N2} | {6} | {7:N2} | {8} | {9} |" -f `
        $row.map,
        $row.backend,
        $row.world_tiles,
        $row.total_tiles,
        [double]$row.generation_total_ms,
        [double]$row.async_wait_avg_ms,
        [int64]$row.async_frame_elapsed_ms,
        [double]$row.detour_avg_ms,
        [int64]$row.detour_points_after,
        [int]$row.async_over_budget_frames))
}

$md.Add("")
$md.Add("## Aggregate")
$md.Add("")
$md.Add("| Metric | Min | Avg | Max |")
$md.Add("| --- | ---: | ---: | ---: |")
foreach ($s in $stats) {
    $md.Add(("| {0} | {1} | {2} | {3} |" -f $s.Metric, $s.Min, $s.Avg, $s.Max))
}

$md | Set-Content -LiteralPath $OutputMarkdown -Encoding UTF8

Write-Host ("[nav-profile] rows={0} maps={1}" -f $allRows.Count, $latestByMap.Count)
Write-Host ("[nav-profile] csv={0}" -f $OutputCsv)
Write-Host ("[nav-profile] md={0}" -f $OutputMarkdown)

$violations = New-Object System.Collections.Generic.List[string]

if ($BudgetFile) {
    if (-not (Test-Path -LiteralPath $BudgetFile)) {
        throw "Budget file not found: $BudgetFile"
    }

    $budgetRows = Import-Csv -LiteralPath $BudgetFile
    foreach ($row in $latestByMap) {
        $budget = Get-BudgetRowForMap -BudgetRows $budgetRows -Map $row.map
        if ($null -eq $budget) {
            continue
        }

        $maxWorldGenMs = Parse-NullableDouble -Row $budget -Name "max_world_gen_ms"
        $maxInlineGenMs = Parse-NullableDouble -Row $budget -Name "max_inline_gen_ms"
        $maxTotalGenMs = Parse-NullableDouble -Row $budget -Name "max_total_gen_ms"
        $maxQueueFrameMs = Parse-NullableDouble -Row $budget -Name "max_queue_frame_ms"
        $maxQueueWaitAvgMs = Parse-NullableDouble -Row $budget -Name "max_queue_wait_avg_ms"
        $maxDetourAvgMs = Parse-NullableDouble -Row $budget -Name "max_detour_avg_ms"
        $maxDetourPointsAfter = Parse-NullableInt -Row $budget -Name "max_detour_points_after"
        $maxOverBudgetFrames = Parse-NullableInt -Row $budget -Name "max_over_budget_frames"
        $minWorldTiles = Parse-NullableInt -Row $budget -Name "min_world_tiles"

        if ($null -ne $maxWorldGenMs -and [double]$row.generation_world_ms -gt $maxWorldGenMs) {
            $violations.Add(("{0}: generation_world_ms {1:N2} > budget {2:N2}" -f $row.map, [double]$row.generation_world_ms, $maxWorldGenMs))
        }
        if ($null -ne $maxInlineGenMs -and [double]$row.generation_inline_ms -gt $maxInlineGenMs) {
            $violations.Add(("{0}: generation_inline_ms {1:N2} > budget {2:N2}" -f $row.map, [double]$row.generation_inline_ms, $maxInlineGenMs))
        }
        if ($null -ne $maxTotalGenMs -and [double]$row.generation_total_ms -gt $maxTotalGenMs) {
            $violations.Add(("{0}: generation_total_ms {1:N2} > budget {2:N2}" -f $row.map, [double]$row.generation_total_ms, $maxTotalGenMs))
        }
        if ($null -ne $maxQueueFrameMs -and [double]$row.async_frame_elapsed_ms -gt $maxQueueFrameMs) {
            $violations.Add(("{0}: async_frame_elapsed_ms {1} > budget {2:N2}" -f $row.map, [int64]$row.async_frame_elapsed_ms, $maxQueueFrameMs))
        }
        if ($null -ne $maxQueueWaitAvgMs -and [double]$row.async_wait_avg_ms -gt $maxQueueWaitAvgMs) {
            $violations.Add(("{0}: async_wait_avg_ms {1:N2} > budget {2:N2}" -f $row.map, [double]$row.async_wait_avg_ms, $maxQueueWaitAvgMs))
        }
        if ($null -ne $maxDetourAvgMs -and [double]$row.detour_avg_ms -gt $maxDetourAvgMs) {
            $violations.Add(("{0}: detour_avg_ms {1:N2} > budget {2:N2}" -f $row.map, [double]$row.detour_avg_ms, $maxDetourAvgMs))
        }
        if ($null -ne $maxDetourPointsAfter -and [int64]$row.detour_points_after -gt $maxDetourPointsAfter) {
            $violations.Add(("{0}: detour_points_after {1} > budget {2}" -f $row.map, [int64]$row.detour_points_after, $maxDetourPointsAfter))
        }
        if ($null -ne $maxOverBudgetFrames -and [int]$row.async_over_budget_frames -gt $maxOverBudgetFrames) {
            $violations.Add(("{0}: async_over_budget_frames {1} > budget {2}" -f $row.map, [int]$row.async_over_budget_frames, $maxOverBudgetFrames))
        }
        if ($null -ne $minWorldTiles -and [int]$row.world_tiles -lt $minWorldTiles) {
            $violations.Add(("{0}: world_tiles {1} < budget minimum {2}" -f $row.map, [int]$row.world_tiles, $minWorldTiles))
        }
    }

    if ($violations.Count -gt 0) {
        Write-Host "[nav-profile][BUDGET] violations:"
        foreach ($v in $violations) {
            Write-Host ("  - {0}" -f $v)
        }

        if ($FailOnBudget) {
            exit 1
        }
    } else {
        Write-Host "[nav-profile][BUDGET] PASS"
    }
}

exit 0
