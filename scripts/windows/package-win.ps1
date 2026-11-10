# Assembles a self-contained Windows distribution of ComponentHub (exe + DLLs + Qt plugins)
# in dist/ComponentHub-<version>-win64/ and produces the matching ZIP and, when Inno Setup 6
# is installed, the ComponentHub-<version>-win64-setup.exe installer.
#
# Usage (from anywhere):
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

# Executable + deployed DLLs (Qt via windeployqt + MinGW via deploy-mingw.sh)
Copy-Item (Join-Path $build "ComponentHub.exe") $dist
Copy-Item (Join-Path $build "*.dll") $dist

# Qt plugin folders
foreach ($d in 'platforms','styles','imageformats','tls','networkinformation','generic','iconengines') {
    $src = Join-Path $build $d
    if (Test-Path $src) { Copy-Item $src $dist -Recurse }
}

# Documents
foreach ($f in 'LICENSE','README.md','README.fr.md','CHANGELOG.md','VERSION') {
    $src = Join-Path $root $f
    if (Test-Path $src) { Copy-Item $src $dist }
}

# Installer (Inno Setup 6), built from the same staged folder. Skipped with a warning when
# ISCC.exe is not installed: the ZIP is still produced.
if ($Version -match '^(\d+)\.(\d+)\.(\d+)') { $versionNum = "$($Matches[1]).$($Matches[2]).$($Matches[3]).0" }
else { throw "Version '$Version' is not X.Y.Z." }
$iscc = @(
    (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source,
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if ($iscc) {
    & $iscc "/DAppVersion=$Version" "/DAppVersionNum=$versionNum" "/DStageDir=$dist" `
        "/O$(Join-Path $root 'dist')" (Join-Path $root "installer\componenthub.iss")
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed." }
} else {
    Write-Warning "Inno Setup 6 not found: installer not built (the ZIP still is)."
}

# Archive ZIP
$zip = Join-Path $root "dist\$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zip

$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Output "OK - $zip ($size Mo)"
Write-Output "Dossier : $dist"
