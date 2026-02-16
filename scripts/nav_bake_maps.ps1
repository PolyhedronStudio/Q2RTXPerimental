param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [string[]]$Maps = @(),
    [string]$MapListFile = "",

    [string]$GameDirectory = "baseq2rtxp",
    [string]$CacheExtension = "vans",
    [int]$PerMapTimeoutSeconds = 120,

    [switch]$RebuildExisting
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

if (-not (Test-Path -LiteralPath $Executable)) {
    throw "Executable not found: $Executable"
}

$mapList = Get-MapList -InlineMaps $Maps -MapFile $MapListFile
if ($mapList.Count -eq 0) {
    throw "No maps provided. Use -Maps or -MapListFile."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$cacheRoot = Join-Path $repoRoot "$GameDirectory/maps/nav"
$logRoot = Join-Path $repoRoot "build-nav/nav-bake-logs"
New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null

$failures = New-Object System.Collections.Generic.List[string]

foreach ($map in $mapList) {
    $cacheFilename = "$map.$CacheExtension"
    $cachePath = Join-Path $cacheRoot $cacheFilename
    $logPath = Join-Path $logRoot ("bake_{0}_{1:yyyyMMdd_HHmmss}.log" -f $map, (Get-Date))

    if ($RebuildExisting -and (Test-Path -LiteralPath $cachePath)) {
        Remove-Item -LiteralPath $cachePath -Force
    }

    Write-Host ("[nav-bake] map={0} cache={1}" -f $map, $cacheFilename)

    $args = @(
        "+set", "dedicated", "1",
        "+set", "developer", "1",
        "+set", "cheats", "1",
        "+set", "game", $GameDirectory,
        "+set", "nav_cache_auto_load", "0",
        "+set", "nav_cache_auto_bake_missing", "0",
        "+set", "nav_cache_auto_save_after_bake", "0",
        "+map", $map,
        "+sv", "nav_bake", $cacheFilename,
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
        Write-Host ("[nav-bake][FAIL] map={0} timed out ({1}s)" -f $map, $PerMapTimeoutSeconds)
        continue
    }

    if ($proc.ExitCode -ne 0) {
        $failures.Add("$map (exit $($proc.ExitCode))")
        Write-Host ("[nav-bake][FAIL] map={0} exited with code {1}" -f $map, $proc.ExitCode)
        continue
    }

    if (-not (Test-Path -LiteralPath $cachePath)) {
        $failures.Add("$map (missing cache)")
        Write-Host ("[nav-bake][FAIL] map={0} cache not found at {1}" -f $map, $cachePath)
        continue
    }

    $sizeBytes = (Get-Item -LiteralPath $cachePath).Length
    Write-Host ("[nav-bake][OK] map={0} wrote {1} ({2} bytes)" -f $map, $cacheFilename, $sizeBytes)
}

if ($failures.Count -gt 0) {
    Write-Host ""
    Write-Host ("[nav-bake] FAILED: {0} map(s)" -f $failures.Count)
    foreach ($f in $failures) {
        Write-Host ("  - {0}" -f $f)
    }
    exit 1
}

Write-Host ""
Write-Host ("[nav-bake] SUCCESS: baked {0} map(s)" -f $mapList.Count)
exit 0
