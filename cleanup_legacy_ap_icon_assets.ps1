$root = Join-Path $PSScriptRoot "soh\assets\custom\textures\parameter_static"
$legacy = @(
    "gArchipelagoItemImportant.rgba32.png",
    "gArchipelagoItemImportant.rgba16.png",
    "gArchipelagoItemNormal.rgba32.png",
    "gArchipelagoItemNormal.rgba16.png"
)
foreach ($name in $legacy) {
    $p = Join-Path $root $name
    if (Test-Path $p) {
        Remove-Item -Force $p
        Write-Host "Removed stale legacy AP icon asset: $name"
    }
}
Write-Host "0.7.33 uses collision-proof gArchipelagoRemote*32 resources."
