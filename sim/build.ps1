# Build desktop UI simulator (requires CMake and SDL2)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $here "build"
if (-not (Test-Path $build)) { New-Item -ItemType Directory -Path $build | Out-Null }
Push-Location $build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
Pop-Location
Write-Host "Done: $build\Release\psi_ui_sim.exe or $build\psi_ui_sim"
