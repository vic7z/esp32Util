param([string]$ZigExecutable = 'zig')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $ZigExecutable -ErrorAction SilentlyContinue)) {
  $ZigExecutable = Join-Path $projectRoot '.tools/zig/zig-windows-x86_64-0.13.0/zig.exe'
  if (-not (Test-Path -LiteralPath $ZigExecutable)) { throw 'Install Zig or supply -ZigExecutable with its full path.' }
}
$testBuild = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Force $testBuild | Out-Null
& $ZigExecutable c++ -target wasm32-wasi -I (Join-Path $projectRoot 'tests/freestanding') -std=c++17 -O2 -fno-builtin -nostdlib -nostdlib++ '-Wl,--no-entry' (Join-Path $projectRoot 'tests/capture-harness.cpp') -o (Join-Path $testBuild 'capture-harness.wasm')
if ($LASTEXITCODE -ne 0) { throw 'Capture harness compilation failed.' }
& node (Join-Path $projectRoot 'tests/capture-regressions.cjs')
if ($LASTEXITCODE -ne 0) { throw 'Capture regressions failed.' }
& node --check (Join-Path $projectRoot 'data/app.js')
if ($LASTEXITCODE -ne 0) { throw 'Browser JavaScript syntax check failed.' }
& node (Join-Path $projectRoot 'tests/web-regressions.cjs')
if ($LASTEXITCODE -ne 0) { throw 'Browser regressions failed.' }
