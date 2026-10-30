param([string]$ZigExecutable = 'zig')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $ZigExecutable -ErrorAction SilentlyContinue)) {
  $ZigExecutable = Join-Path $projectRoot '.tools/zig/zig-windows-x86_64-0.13.0/zig.exe'
  if (-not (Test-Path -LiteralPath $ZigExecutable)) { throw 'Install Zig or supply -ZigExecutable.' }
}
New-Item -ItemType Directory -Force (Join-Path $projectRoot 'build') | Out-Null
foreach ($harness in @('lan-harness','lan-runtime-harness')) {
  & $ZigExecutable c++ -target wasm32-wasi -I (Join-Path $projectRoot 'tests/lan-fakes') -I (Join-Path $projectRoot 'tests/freestanding') -std=c++17 -O2 -fno-builtin -nostdlib -nostdlib++ '-Wl,--no-entry' (Join-Path $projectRoot "tests/$harness.cpp") -o (Join-Path $projectRoot "build/$harness.wasm")
  if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $harness" }
}
foreach ($test in @('lan-regressions.cjs','lan-runtime-regressions.cjs','lan-identity-regressions.cjs','web-regressions.cjs')) {
  & node (Join-Path $projectRoot "tests/$test")
  if ($LASTEXITCODE -ne 0) { throw "Regression failure: $test" }
}
