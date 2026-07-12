# Assemble une distribution Windows autonome de ComponentHub (exe + DLL + plugins Qt)
# dans dist/ComponentHub-<version>-win64/ et produit le ZIP correspondant.
#
# Usage (depuis n'importe où) :
#   powershell -ExecutionPolicy Bypass -File scripts\windows\package-win.ps1
param(
    [string]$BuildDir = "build-mingw",
    [string]$Version
)
$ErrorActionPreference = "Stop"
$root  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # racine (scripts/windows/ -> ../..)
$build = Join-Path $root $BuildDir
if (-not (Test-Path (Join-Path $build "ComponentHub.exe"))) {
    throw "ComponentHub.exe introuvable dans $build. Compile d'abord (cmake --build --preset mingw)."
}

if (-not $Version) {
    $versionFile = Join-Path $root "VERSION"
    if (-not (Test-Path $versionFile)) { throw "Fichier VERSION introuvable a la racine du projet." }
    $Version = (Get-Content -Path $versionFile -TotalCount 1).Trim()
    if (-not $Version) { throw "Le fichier VERSION est vide." }
}

$name = "ComponentHub-$Version-win64"
$dist = Join-Path $root "dist\$name"
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force -Path $dist | Out-Null

# Exécutable + DLL déployées (Qt via windeployqt + MinGW via deploy-mingw.sh)
Copy-Item (Join-Path $build "ComponentHub.exe") $dist
Copy-Item (Join-Path $build "*.dll") $dist

# Dossiers de plugins Qt
foreach ($d in 'platforms','styles','imageformats','tls','networkinformation','generic','iconengines') {
    $src = Join-Path $build $d
    if (Test-Path $src) { Copy-Item $src $dist -Recurse }
}

# Documents
foreach ($f in 'LICENSE','README.md','CHANGELOG.md','VERSION') {
    $src = Join-Path $root $f
    if (Test-Path $src) { Copy-Item $src $dist }
}

# Archive ZIP
$zip = Join-Path $root "dist\$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zip

$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Output "OK - $zip ($size Mo)"
Write-Output "Dossier : $dist"
