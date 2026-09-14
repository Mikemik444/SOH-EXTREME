$root = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $root 'install_official_ap_model_assets.ps1')
