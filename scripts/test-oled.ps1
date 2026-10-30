param([string]$ZigExecutable = '.tools/zig/zig-windows-x86_64-0.13.0/zig.exe', [string]$U8g2Path = '.tools/arduino/libraries/U8g2/src/clib')
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
  New-Item -ItemType Directory -Force build | Out-Null
  & node tests/render-oled-source.cjs
  if ($LASTEXITCODE -ne 0) { throw 'OLED harness generation failed' }
  $sources = @('u8g2_setup.c','u8g2_font.c','u8g2_fonts.c','u8g2_hvline.c','u8g2_ll_hvline.c','u8g2_box.c','u8g2_circle.c','u8g2_line.c','u8g2_intersection.c','u8x8_8x8.c','u8x8_string.c') | ForEach-Object { Join-Path $U8g2Path $_ }
  & $ZigExecutable c++ -target wasm32-wasi -mexec-model=reactor -nostdlib++ -I $U8g2Path -O2 build/oled-render.cpp @sources -o build/oled-render.wasm
  if ($LASTEXITCODE -ne 0) { throw 'OLED harness compilation failed' }
  & node tests/oled-preview.cjs
  if ($LASTEXITCODE -ne 0) { throw 'OLED layout checks failed' }
} finally { Pop-Location }
