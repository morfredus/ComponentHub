# Resynchronise les copies vendorées de morfBeacon / morfUpdate / morfdeploy
# dans third_party/morf/ depuis les dépôts sources voisins.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$srcBase = if ($env:MORF_SRC_BASE) { $env:MORF_SRC_BASE } else { Split-Path -Parent $root }
function Sync-One($name, $srcDir, $dstDir) {
    if (-not (Test-Path $srcDir)) {
        Write-Error "Source introuvable pour $name : $srcDir"
    }
    Remove-Item -Recurse -Force "$dstDir\include", "$dstDir\src" -ErrorAction SilentlyContinue
    Copy-Item -Recurse "$srcDir\include" "$dstDir\include"
    Copy-Item -Recurse "$srcDir\src"     "$dstDir\src"
    Copy-Item "$srcDir\VERSION" "$dstDir\VERSION"
    Write-Output "OK  $name"
}
$beacon = if (Test-Path "$srcBase\morfBeacon") { "$srcBase\morfBeacon" } else { "$srcBase\morfBeacon_travail" }
$update = if (Test-Path "$srcBase\morfUpdate") { "$srcBase\morfUpdate" } else { "$srcBase\morfUpdate_travail" }
Sync-One "morfBeacon" $beacon "$root\third_party\morf\beacon"
Sync-One "morfUpdate" $update "$root\third_party\morf\update"

# morfdeploy : paquet Python (ni include\ ni src\), copie telle quelle depuis le
# depot dedie morfDeploy. Sans cette resynchronisation la copie vendoree derive.
$deployRoot = if (Test-Path "$srcBase\morfDeploy") { "$srcBase\morfDeploy" } else { "$srcBase\morfDeploy_travail" }
$deploySrc = "$deployRoot\morfdeploy"
$deployDst = "$root\third_party\morf\morfdeploy"
if (Test-Path $deploySrc) {
    Remove-Item -Recurse -Force $deployDst -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $deployDst | Out-Null
    Copy-Item -Recurse "$deploySrc\*" $deployDst
    Get-ChildItem -Recurse -Directory -Filter "__pycache__" $deployDst | Remove-Item -Recurse -Force
    if (Test-Path "$deployRoot\VERSION") { Copy-Item "$deployRoot\VERSION" "$deployDst\VERSION" }
    Write-Output "OK  morfdeploy"
} else {
    Write-Error "Source introuvable pour morfdeploy : $deploySrc"
}

Write-Output "Synchronisation terminee."
