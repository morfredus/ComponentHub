param(
    [ValidateSet("configure", "build", "run", "clean")]
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"

function Find-MsysBash {
    $candidates = @()
    if ($env:MSYS2_ROOT) { $candidates += (Join-Path $env:MSYS2_ROOT "usr\bin\bash.exe") }
    $candidates += @("C:\msys64\usr\bin\bash.exe", "C:\msys2\usr\bin\bash.exe")
    foreach ($candidate in $candidates) { if (Test-Path $candidate) { return $candidate } }
    throw "MSYS2 introuvable. Installe MSYS2 ou definis MSYS2_ROOT (ex. C:\msys64)."
}

function Convert-ToMsysPath {
    param([string]$Path)
    $fullPath = (Resolve-Path $Path).Path
    if ($fullPath -match "^([A-Za-z]):\\(.*)$") {
        $drive = $matches[1].ToLowerInvariant()
        $rest = $matches[2] -replace "\\", "/"
        return "/$drive/$rest"
    }
    return ($fullPath -replace "\\", "/")
}

function Quote-Bash { param([string]$Value) return "'" + ($Value -replace "'", "'\''") + "'" }

function Normalize-PathText { param([string]$Path) return (($Path -replace "\\", "/").TrimEnd("/")).ToLowerInvariant() }

function Reset-StaleBuildDir {
    param([string]$BuildDir, [string]$SourceDir)
    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path $cachePath)) { return }
    $homeLine = Select-String -Path $cachePath -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$" | Select-Object -First 1
    if (-not $homeLine) { return }
    $cachedSource = $homeLine.Matches[0].Groups[1].Value
    if ((Normalize-PathText $cachedSource) -eq (Normalize-PathText $SourceDir)) { return }
    $resolvedBuild = (Resolve-Path $BuildDir).Path
    $resolvedSource = (Resolve-Path $SourceDir).Path
    if (-not ((Normalize-PathText $resolvedBuild).StartsWith((Normalize-PathText $resolvedSource) + "/"))) {
        throw "Refus de supprimer un dossier de build hors du projet: $resolvedBuild"
    }
    Write-Host "Le cache CMake pointe vers '$cachedSource'. Reinitialisation de '$resolvedBuild'."
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

$bash = Find-MsysBash
# Ce script vit dans scripts/windows/ : la racine du projet est deux niveaux plus haut.
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$buildDir = Join-Path $root "build-mingw"
Reset-StaleBuildDir -BuildDir $buildDir -SourceDir $root

$rootMsys = Convert-ToMsysPath $root
$quotedRoot = Quote-Bash $rootMsys

$env:MSYSTEM = if ($env:MSYSTEM) { $env:MSYSTEM } else { "MINGW64" }
$env:CHERE_INVOKING = "1"
$env:MSYS2_PATH_TYPE = "inherit"

$command = switch ($Action) {
    "configure" { "cd $quotedRoot && cmake --preset mingw" }
    "build" { "cd $quotedRoot && { test -f build-mingw/build.ninja || cmake --preset mingw; } && cmake --build --preset mingw" }
    "run" { "cd $quotedRoot && ./build-mingw/ComponentHub.exe" }
    "clean" { "cd $quotedRoot && cmake --build --preset mingw --target clean" }
}

& $bash -lc $command
exit $LASTEXITCODE
