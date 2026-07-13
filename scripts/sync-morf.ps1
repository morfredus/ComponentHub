# Resynchronise les copies vendorées de morfBeacon / morfUpdate dans
# third_party/morf/ depuis les dépôts sources voisins.
#
# Source par défaut : le dossier parent du projet (ex. 01-Travail/).
# Surcharge possible : $env:MORF_SRC_BASE = "C:\chemin\vers\depots"
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot        # racine du projet
$srcBase = if ($env:MORF_SRC_BASE) { $env:MORF_SRC_BASE } else { Split-Path -Parent $root }

function Sync-One($name, $srcDir, $dstDir) {
    if (-not (Test-Path $srcDir)) {
        Write-Error "Source introuvable pour $name : $srcDir (définir MORF_SRC_BASE si ailleurs)"
    }
    Remove-Item -Recurse -Force "$dstDir\include", "$dstDir\src" -ErrorAction SilentlyContinue
    Copy-Item -Recurse "$srcDir\include" "$dstDir\include"
    Copy-Item -Recurse "$srcDir\src"     "$dstDir\src"
    Copy-Item "$srcDir\VERSION" "$dstDir\VERSION"
    $v = (Get-Content "$dstDir\VERSION" -First 1).Trim()
    Write-Output "OK  $name  (version $v)"
}

Sync-One "morfBeacon" "$srcBase\morfBeacon_travail" "$root\third_party\morf\beacon"
Sync-One "morfUpdate" "$srcBase\morfUpdate_travail" "$root\third_party\morf\update"
Write-Output "Synchronisation terminee. Le CMakeLists vendore n'est pas modifie."
