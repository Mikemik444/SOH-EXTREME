$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dest = Join-Path $root 'soh\assets\custom\objects\object_archipelago_item'
New-Item -ItemType Directory -Force -Path $dest | Out-Null

$base = 'https://raw.githubusercontent.com/aMannus/Shipwright/archipelago/soh/assets/custom/objects/object_archipelago_item/'
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

Write-Host 'Fetching the exact original Archipelago-SoH 3D item model assets...'
foreach ($name in $files) {
    $out = Join-Path $dest $name
    Invoke-WebRequest -UseBasicParsing -Uri ($base + $name) -OutFile $out
    if ((Get-Item $out).Length -eq 0) {
        throw "Downloaded asset is empty: $name"
    }
    Write-Host "  OK $name"
}

# The old model.xml in the reference repository is intentionally an empty marker.
New-Item -ItemType File -Force -Path (Join-Path $dest 'model.xml') | Out-Null

Write-Host ''
Write-Host 'Official AP model assets installed.'
Write-Host 'Now rebuild GenerateSohOtr, then rebuild Release.'
