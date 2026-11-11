# Windows version of run.sh: build (only what changed), run the sim, plot.
#
#   .\run.ps1
#   .\run.ps1 kleiber_k03
#   .\run.ps1 test1 --light uniform --ticks 10000 --seed 3
#
# If scripts are blocked, run once:  Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$name = "latest"
$rest = @($args)
if ($rest.Count -gt 0 -and -not ([string]$rest[0]).StartsWith("--")) {
    $name = $rest[0]
    $rest = if ($rest.Count -gt 1) { $rest[1..($rest.Count - 1)] } else { @() }
}
$out = "out/$name"

if (-not (Test-Path "build/CMakeCache.txt")) {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { exit 1 }
}
cmake --build build -j --config Release
if ($LASTEXITCODE -ne 0) { exit 1 }

# Visual Studio puts the exe in build/Release/, other generators in build/.
$exe = if (Test-Path "build/Release/luca_headless.exe") { "build/Release/luca_headless.exe" } else { "build/luca_headless.exe" }

& $exe --ticks 20000 --out $out @rest
if ($LASTEXITCODE -ne 0) { exit 1 }
python tools/plot.py $out
Write-Host "plots: $out/run.png  $out/map.png"
