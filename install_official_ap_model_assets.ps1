$ErrorActionPreference = 'Stop'

# SOH-EXTREME 0.7.37
# The current SOH-EXTREME CMakeLists.txt packs BB\assets\custom (NOT BB\soh\assets\custom).
# This installer makes the official Archipelago-SoH model self-healing before GenerateSohOtr.

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dest = Join-Path $root 'assets\custom\objects\object_archipelago_item'
$legacy = Join-Path $root 'soh\assets\custom\objects\object_archipelago_item'
New-Item -ItemType Directory -Force -Path $dest | Out-Null

$files = @(
    'gArchipelagoItemDL',
    'mat_gArchipelagoItemDL_red',
    'mat_gArchipelagoItemDL_yellow',
    'mat_gArchipelagoItemDL_blue',
    'mat_gArchipelagoItemDL_orange',
    'mat_gArchipelagoItemDL_purple',
    'mat_gArchipelagoItemDL_green',
    'gArchipelagoItemDL_tri_0',
    'gArchipelagoItemDL_tri_1',
    'gArchipelagoItemDL_tri_2',
    'gArchipelagoItemDL_tri_3',
    'gArchipelagoItemDL_tri_4',
    'gArchipelagoItemDL_tri_5',
    'gArchipelagoItemDL_vtx_0',
    'gArchipelagoItemDL_vtx_1',
    'gArchipelagoItemDL_vtx_2',
    'gArchipelagoItemDL_vtx_3',
    'gArchipelagoItemDL_vtx_4',
    'gArchipelagoItemDL_vtx_5'
)

$base = 'https://raw.githubusercontent.com/aMannus/Shipwright/archipelago/soh/assets/custom/objects/object_archipelago_item/'

function Test-ApAsset([string]$path) {
    if (!(Test-Path $path)) { return $false }
    if ((Get-Item $path).Length -lt 20) { return $false }
    try {
        $first = Get-Content -LiteralPath $path -TotalCount 1 -ErrorAction Stop
        return ($first -match '^<(DisplayList|Vertex)')
    } catch {
        return $false
    }
}

Write-Host 'SOH-EXTREME 0.7.37 - installing official Archipelago model'
Write-Host "Correct packer root: $dest"

foreach ($name in $files) {
    $target = Join-Path $dest $name
    $old = Join-Path $legacy $name

    if (!(Test-ApAsset $target) -and (Test-ApAsset $old)) {
        Copy-Item -Force $old $target
        Write-Host "  COPIED     $name"
    }

    if (!(Test-ApAsset $target)) {
        Write-Host "  DOWNLOAD   $name"
        $tmp = "$target.download"
        if (Test-Path $tmp) { Remove-Item -Force $tmp }
        Invoke-WebRequest -UseBasicParsing -Headers @{ 'User-Agent'='SOH-EXTREME-asset-installer' } -Uri ($base + $name) -OutFile $tmp
        Move-Item -Force $tmp $target
    }

    if (!(Test-ApAsset $target)) {
        throw "Official AP model asset is missing or invalid: $target"
    }
}

# An empty model.xml was used in 0.7.35. The official object does not need it and
# leaving a zero-byte pseudo-resource can confuse asset scans, so remove it.
$modelXml = Join-Path $dest 'model.xml'
if ((Test-Path $modelXml) -and ((Get-Item $modelXml).Length -eq 0)) {
    Remove-Item -Force $modelXml
}

Write-Host ''
Write-Host 'All 19 official model resources are valid.' -ForegroundColor Green
Write-Host 'GenerateSohOtr must now show objects/object_archipelago_item/gArchipelagoItemDL being added.'
