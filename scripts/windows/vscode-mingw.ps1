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
    # Un dossier de build configure avec un autre environnement MSYS2 (mingw64, ucrt64...)
    # ne doit pas etre reutilise : il melangerait deux runtimes C.
    $toolLine = Select-String -Path $cachePath -Pattern "^(CMAKE_CXX_COMPILER|Qt6_DIR):.*/(mingw64|ucrt64|clang64)/" | Select-Object -First 1
    $sameSource = (Normalize-PathText $cachedSource) -eq (Normalize-PathText $SourceDir)
    $sameTools = (-not $toolLine) -or ($toolLine.Matches[0].Groups[2].Value -eq $env:MSYSTEM.ToLowerInvariant())
    if ($sameSource -and $sameTools) { return }
    $resolvedBuild = (Resolve-Path $BuildDir).Path
    $resolvedSource = (Resolve-Path $SourceDir).Path
    if (-not ((Normalize-PathText $resolvedBuild).StartsWith((Normalize-PathText $resolvedSource) + "/"))) {
        throw "Refus de supprimer un dossier de build hors du projet: $resolvedBuild"
    }
    Write-Host "Le cache CMake ne correspond pas a ce dossier source ou a cet environnement MSYS2 ($cachedSource). Reinitialisation de '$resolvedBuild'."
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

$bash = Find-MsysBash
# This script lives in scripts/windows/: the project root is two levels up.
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$buildDir = Join-Path $root "build-mingw"
# UCRT64 (MINGW64 est deprecie) ; defini avant la verification du cache de build.
$env:MSYSTEM = if ($env:MSYSTEM) { $env:MSYSTEM } else { "UCRT64" }
Reset-StaleBuildDir -BuildDir $buildDir -SourceDir $root

$rootMsys = Convert-ToMsysPath $root
$quotedRoot = Quote-Bash $rootMsys

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
