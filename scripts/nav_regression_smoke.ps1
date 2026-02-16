param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [string[]]$Maps = @(),
    [string]$MapListFile = "",

    [string]$QueryPairsFile = "",
    [string]$ExpectedStatsFile = "",
    [switch]$RequireQueryPairs,

    [string]$GameDirectory = "baseq2rtxp",
    [int]$PerMapTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"

function Get-MapList {
    param(
        [string[]]$InlineMaps,
        [string]$MapFile
    )

    $result = New-Object System.Collections.Generic.List[string]

    foreach ($map in $InlineMaps) {
        $name = $map.Trim()
        if ($name.Length -gt 0) {
            $result.Add($name)
        }
    }

    if ($MapFile -and (Test-Path -LiteralPath $MapFile)) {
        foreach ($line in Get-Content -LiteralPath $MapFile) {
            $trimmed = $line.Trim()
            if ($trimmed.Length -eq 0) { continue }
            if ($trimmed.StartsWith("#")) { continue }
            $result.Add($trimmed)
        }
    }

    return $result | Select-Object -Unique
}

function Get-QueryPairsByMap {
    param(
        [string]$QueryFile
    )

    $result = @{}
    if (-not $QueryFile) {
        return $result
    }
    if (-not (Test-Path -LiteralPath $QueryFile)) {
        throw "QueryPairsFile not found: $QueryFile"
    }

    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $QueryFile) {
        $lineNumber++
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0) { continue }
        if ($trimmed.StartsWith("#")) { continue }

        $parts = $trimmed.Split(",")
        if ($parts.Count -lt 9) {
            throw ("Invalid query line {0}: expected at least 9 CSV columns" -f $lineNumber)
        }

        $map = $parts[0].Trim()
        $id = $parts[1].Trim()
        if (-not $map) {
            throw ("Invalid query line {0}: missing map name" -f $lineNumber)
        }
        if (-not $id) {
            $id = "q{0}" -f $lineNumber
        }

        try {
            $query = [PSCustomObject]@{
                Map = $map
                Id = $id
                StartX = [double]$parts[2].Trim()
                StartY = [double]$parts[3].Trim()
                StartZ = [double]$parts[4].Trim()
                GoalX = [double]$parts[5].Trim()
                GoalY = [double]$parts[6].Trim()
                GoalZ = [double]$parts[7].Trim()
                ExpectFound = [int]$parts[8].Trim()
                ExpectSegmentsOk = if ($parts.Count -ge 10 -and $parts[9].Trim().Length -gt 0) { [int]$parts[9].Trim() } else { 1 }
            }
        }
        catch {
            throw ("Invalid query line {0}: numeric parse failed ({1})" -f $lineNumber, $_.Exception.Message)
        }

        if (-not $result.ContainsKey($map)) {
            $result[$map] = New-Object System.Collections.Generic.List[object]
        }
        $result[$map].Add($query)
    }

    return $result
}

function Get-ExpectedWorldTiles {
    param(
        [string]$StatsFile
    )

    $result = @{}
    if (-not $StatsFile) {
        return $result
    }
    if (-not (Test-Path -LiteralPath $StatsFile)) {
        throw "ExpectedStatsFile not found: $StatsFile"
    }

    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $StatsFile) {
        $lineNumber++
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0) { continue }
        if ($trimmed.StartsWith("#")) { continue }

        $parts = $trimmed.Split(",")
        if ($parts.Count -lt 2) {
            throw ("Invalid stats line {0}: expected CSV 'map,world_tiles'" -f $lineNumber)
        }

        $map = $parts[0].Trim()
        if (-not $map) {
            throw ("Invalid stats line {0}: missing map name" -f $lineNumber)
        }

        try {
            $expectedWorldTiles = [int]$parts[1].Trim()
        }
        catch {
            throw ("Invalid stats line {0}: world_tiles parse failed ({1})" -f $lineNumber, $_.Exception.Message)
        }

        $result[$map] = $expectedWorldTiles
    }

    return $result
}

function Format-InvariantDouble {
    param(
        [double]$Value
    )

    return $Value.ToString([System.Globalization.CultureInfo]::InvariantCulture)
}

if (-not (Test-Path -LiteralPath $Executable)) {
    throw "Executable not found: $Executable"
}

$mapList = Get-MapList -InlineMaps $Maps -MapFile $MapListFile
if ($mapList.Count -eq 0) {
    throw "No maps provided. Use -Maps or -MapListFile."
}

$queriesByMap = Get-QueryPairsByMap -QueryFile $QueryPairsFile
$expectedWorldTilesByMap = Get-ExpectedWorldTiles -StatsFile $ExpectedStatsFile

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$logRoot = Join-Path $repoRoot "build-nav/nav-regression-logs"
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null

$failures = New-Object System.Collections.Generic.List[string]

foreach ($map in $mapList) {
    $logPath = Join-Path $logRoot ("smoke_{0}_{1:yyyyMMdd_HHmmss}.log" -f $map, (Get-Date))
    Write-Host ("[nav-smoke] map={0}" -f $map)

    $mapQueries = New-Object System.Collections.Generic.List[object]
    if ($queriesByMap.ContainsKey($map)) {
        foreach ($q in $queriesByMap[$map]) {
            $mapQueries.Add($q)
        }
    }

    if ($RequireQueryPairs -and $mapQueries.Count -eq 0) {
        $failures.Add("$map (missing query pairs)")
        Write-Host ("[nav-smoke][FAIL] map={0} no query pairs provided while -RequireQueryPairs is set" -f $map)
        continue
    }

    $args = @(
        "+set", "dedicated", "1",
        "+set", "developer", "1",
        "+set", "cheats", "1",
        "+set", "game", $GameDirectory,
        "+set", "nav_cache_auto_load", "0",
        "+set", "nav_cache_auto_bake_missing", "0",
        "+set", "nav_cache_auto_save_after_bake", "0",
        "+map", $map,
        "+sv", "nav_gen_voxelmesh",
        "+sv", "nav_selftest", "256"
    )

    foreach ($query in $mapQueries) {
        $args += @(
            "+sv", "nav_query_path",
            $query.Id,
            (Format-InvariantDouble -Value $query.StartX),
            (Format-InvariantDouble -Value $query.StartY),
            (Format-InvariantDouble -Value $query.StartZ),
            (Format-InvariantDouble -Value $query.GoalX),
            (Format-InvariantDouble -Value $query.GoalY),
            (Format-InvariantDouble -Value $query.GoalZ)
        )
    }

    $args += @(
        "+sv", "nav_profile_dump",
        "+quit"
    )

    $proc = Start-Process -FilePath $Executable `
        -ArgumentList $args `
        -WorkingDirectory $repoRoot `
        -RedirectStandardOutput $logPath `
        -RedirectStandardError $logPath `
        -PassThru

    if (-not $proc.WaitForExit($PerMapTimeoutSeconds * 1000)) {
        try { $proc.Kill() } catch {}
        $failures.Add("$map (timeout)")
        Write-Host ("[nav-smoke][FAIL] map={0} timed out ({1}s)" -f $map, $PerMapTimeoutSeconds)
        continue
    }

    if ($proc.ExitCode -ne 0) {
        $failures.Add("$map (exit $($proc.ExitCode))")
        Write-Host ("[nav-smoke][FAIL] map={0} exited with code {1}" -f $map, $proc.ExitCode)
        continue
    }

    if (-not (Test-Path -LiteralPath $logPath)) {
        $failures.Add("$map (missing log)")
        Write-Host ("[nav-smoke][FAIL] map={0} log file missing" -f $map)
        continue
    }

    $logText = Get-Content -LiteralPath $logPath -Raw
    if ($logText -notmatch "=== nav_profile_dump ===") {
        $failures.Add("$map (no profile dump)")
        Write-Host ("[nav-smoke][FAIL] map={0} missing nav_profile_dump output" -f $map)
        continue
    }

    $selfTestMatch = [regex]::Match($logText, "\[nav-selftest\]\s+result=(PASS|FAIL)")
    if (-not $selfTestMatch.Success -or $selfTestMatch.Groups[1].Value -ne "PASS") {
        $failures.Add("$map (nav_selftest failed)")
        Write-Host ("[nav-smoke][FAIL] map={0} nav_selftest missing or failed" -f $map)
        continue
    }

    $meshLoadedMatch = [regex]::Match($logText, "mesh_loaded=(\d+)")
    if (-not $meshLoadedMatch.Success -or $meshLoadedMatch.Groups[1].Value -ne "1") {
        $failures.Add("$map (mesh not loaded)")
        Write-Host ("[nav-smoke][FAIL] map={0} mesh_loaded check failed" -f $map)
        continue
    }

    $worldTilesMatch = [regex]::Match($logText, "world_tiles=(\d+)")
    if (-not $worldTilesMatch.Success) {
        $failures.Add("$map (missing world_tiles)")
        Write-Host ("[nav-smoke][FAIL] map={0} world_tiles metric missing" -f $map)
        continue
    }

    $worldTiles = [int]$worldTilesMatch.Groups[1].Value
    if ($worldTiles -le 0) {
        $failures.Add("$map (world_tiles <= 0)")
        Write-Host ("[nav-smoke][FAIL] map={0} world_tiles={1}" -f $map, $worldTiles)
        continue
    }

    if ($expectedWorldTilesByMap.ContainsKey($map)) {
        $expectedWorldTiles = [int]$expectedWorldTilesByMap[$map]
        if ($worldTiles -ne $expectedWorldTiles) {
            $failures.Add("$map (world_tiles mismatch expected=$expectedWorldTiles actual=$worldTiles)")
            Write-Host ("[nav-smoke][FAIL] map={0} world_tiles expected {1} got {2}" -f $map, $expectedWorldTiles, $worldTiles)
            continue
        }
    }

    $queryFailure = $false
    foreach ($query in $mapQueries) {
        $pattern = "\[nav-query\]\s+id=" + [regex]::Escape($query.Id) + "\s+found=(\d+)\s+points=(\d+)\s+segments_ok=(\d+)\s+invalid_segment=(-?\d+)"
        $m = [regex]::Match($logText, $pattern)
        if (-not $m.Success) {
            $failures.Add("$map (missing query result id=$($query.Id))")
            Write-Host ("[nav-smoke][FAIL] map={0} missing nav-query result id={1}" -f $map, $query.Id)
            $queryFailure = $true
            break
        }

        $found = [int]$m.Groups[1].Value
        $segmentsOk = [int]$m.Groups[3].Value
        if ($found -ne [int]$query.ExpectFound) {
            $failures.Add("$map (query id=$($query.Id) expect_found=$($query.ExpectFound) actual=$found)")
            Write-Host ("[nav-smoke][FAIL] map={0} query={1} expected found={2} got {3}" -f $map, $query.Id, $query.ExpectFound, $found)
            $queryFailure = $true
            break
        }

        if ($found -eq 1 -and $segmentsOk -ne [int]$query.ExpectSegmentsOk) {
            $failures.Add("$map (query id=$($query.Id) expect_segments_ok=$($query.ExpectSegmentsOk) actual=$segmentsOk)")
            Write-Host ("[nav-smoke][FAIL] map={0} query={1} expected segments_ok={2} got {3}" -f $map, $query.Id, $query.ExpectSegmentsOk, $segmentsOk)
            $queryFailure = $true
            break
        }
    }
    if ($queryFailure) {
        continue
    }

    Write-Host ("[nav-smoke][OK] map={0} world_tiles={1} queries={2}" -f $map, $worldTiles, $mapQueries.Count)
}

if ($failures.Count -gt 0) {
    Write-Host ""
    Write-Host ("[nav-smoke] FAILED: {0} map(s)" -f $failures.Count)
    foreach ($f in $failures) {
        Write-Host ("  - {0}" -f $f)
    }
    exit 1
}

Write-Host ""
Write-Host ("[nav-smoke] SUCCESS: validated {0} map(s)" -f $mapList.Count)
exit 0
