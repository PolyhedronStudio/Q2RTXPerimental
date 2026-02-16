param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [string[]]$Maps = @(),
    [string]$MapListFile = "",
    [string]$QueryPairsFile = "",
    [string]$ExpectedStatsFile = "",
    [switch]$RequireQueryPairs,
    [switch]$RunProfileDashboard,
    [string]$ProfileBudgetFile = "",
    [switch]$FailOnProfileBudget,

    [string]$GameDirectory = "baseq2rtxp",
    [int]$PerMapTimeoutSeconds = 120,

    [switch]$SkipBake,
    [switch]$SkipSmoke,
    [switch]$RebuildExisting
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$bakeScript = Join-Path $scriptRoot "nav_bake_maps.ps1"
$smokeScript = Join-Path $scriptRoot "nav_regression_smoke.ps1"
$profileScript = Join-Path $scriptRoot "nav_profile_dashboard.ps1"
$profileRequested = $RunProfileDashboard -or $ProfileBudgetFile -or $FailOnProfileBudget
$shouldRunProfileDashboard = $profileRequested -and (-not $SkipSmoke)

if (-not (Test-Path -LiteralPath $bakeScript)) {
    throw "Missing script: $bakeScript"
}

if (-not (Test-Path -LiteralPath $smokeScript)) {
    throw "Missing script: $smokeScript"
}

if ($shouldRunProfileDashboard -and -not (Test-Path -LiteralPath $profileScript)) {
    throw "Missing script: $profileScript"
}

if ($SkipBake -and $SkipSmoke) {
    throw "Nothing to run: both -SkipBake and -SkipSmoke were specified."
}

if (-not (Test-Path -LiteralPath $Executable)) {
    throw "Executable not found: $Executable"
}

if (-not $SkipBake) {
    Write-Host "[nav-pipeline] STEP bake: caches"
    $bakeArgs = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $bakeScript,
        "-Executable", $Executable,
        "-GameDirectory", $GameDirectory,
        "-PerMapTimeoutSeconds", $PerMapTimeoutSeconds
    )

    if ($Maps.Count -gt 0) {
        $bakeArgs += @("-Maps") + $Maps
    }
    if ($MapListFile) {
        $bakeArgs += @("-MapListFile", $MapListFile)
    }
    if ($RebuildExisting) {
        $bakeArgs += "-RebuildExisting"
    }

    & powershell @bakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "[nav-pipeline] bake step failed"
    }
}

if (-not $SkipSmoke) {
    Write-Host "[nav-pipeline] STEP smoke: regression"
    $smokeArgs = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $smokeScript,
        "-Executable", $Executable,
        "-GameDirectory", $GameDirectory,
        "-PerMapTimeoutSeconds", $PerMapTimeoutSeconds
    )

    if ($Maps.Count -gt 0) {
        $smokeArgs += @("-Maps") + $Maps
    }
    if ($MapListFile) {
        $smokeArgs += @("-MapListFile", $MapListFile)
    }
    if ($QueryPairsFile) {
        $smokeArgs += @("-QueryPairsFile", $QueryPairsFile)
    }
    if ($ExpectedStatsFile) {
        $smokeArgs += @("-ExpectedStatsFile", $ExpectedStatsFile)
    }
    if ($RequireQueryPairs) {
        $smokeArgs += "-RequireQueryPairs"
    }

    & powershell @smokeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "[nav-pipeline] smoke step failed"
    }
}

if ($profileRequested -and $SkipSmoke) {
    Write-Host "[nav-pipeline] profile dashboard skipped because smoke step was skipped"
}

if ($shouldRunProfileDashboard) {
    Write-Host "[nav-pipeline] STEP profile: dashboard"
    $profileArgs = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $profileScript
    )

    if ($ProfileBudgetFile) {
        $profileArgs += @("-BudgetFile", $ProfileBudgetFile)
    }
    if ($FailOnProfileBudget) {
        $profileArgs += "-FailOnBudget"
    }

    & powershell @profileArgs
    if ($LASTEXITCODE -ne 0) {
        throw "[nav-pipeline] profile dashboard step failed"
    }
}

Write-Host "[nav-pipeline] COMPLETE"
exit 0
