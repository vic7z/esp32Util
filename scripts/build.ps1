param(
  [string]$ArduinoCli = 'arduino-cli',
  [string]$Board = 'esp32:esp32:esp32c3:PartitionScheme=huge_app',
  [string]$BuildPath = (Join-Path $env:TEMP 'esp32Util-c3'),
  [string]$ConfigFile = '',
  [string]$ZigExe = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
  $ArduinoCli = Join-Path $projectRoot '.tools/arduino-cli.exe'
  if (-not (Test-Path -LiteralPath $ArduinoCli)) { throw 'Arduino CLI was not found. Supply -ArduinoCli with its full path.' }
}
if (-not $ConfigFile -and (Test-Path (Join-Path $projectRoot '.tools/arduino.yaml'))) {
  $ConfigFile = Join-Path $projectRoot '.tools/arduino.yaml'
}
if (-not $ZigExe -and (Test-Path (Join-Path $projectRoot '.tools/zig-bin/zig.exe'))) {
  $ZigExe = Join-Path $projectRoot '.tools/zig-bin/zig.exe'
}
# Stage only source files. Arduino otherwise copies development tools and
# fixtures recursively into the sketch build, slowing builds considerably.
$sketchPath = Join-Path $projectRoot 'build/source/esp32Util'
$outputPath = Join-Path $projectRoot 'build/c3'
New-Item -ItemType Directory -Force $sketchPath,$outputPath | Out-Null
Get-ChildItem -LiteralPath $projectRoot -File |
  Where-Object { $_.Extension -in '.ino','.cpp','.h' } |
  Copy-Item -Destination $sketchPath -Force
$cliArguments = @()
if ($ConfigFile) { $cliArguments += @('--config-file', $ConfigFile) }
$cliArguments += @('compile','--jobs','4','--fqbn',$Board,'--build-path',$BuildPath)
if ($ZigExe) {
  $archiverPath = (Resolve-Path -LiteralPath $ZigExe).Path.Replace('\','/')
  $cliArguments += @('--build-property', ('recipe.ar.pattern="{0}" ar cr "{{archive_file_path}}" "{{object_file}}"' -f $archiverPath))
}
$cliArguments += $sketchPath
& $ArduinoCli @cliArguments
if ($LASTEXITCODE -ne 0) { throw 'Firmware compilation failed.' }
Copy-Item -LiteralPath (Join-Path $BuildPath 'esp32Util.ino.bin') -Destination (Join-Path $outputPath 'firmware.bin') -Force
Copy-Item -LiteralPath (Join-Path $BuildPath 'esp32Util.ino.bootloader.bin') -Destination (Join-Path $outputPath 'bootloader.bin') -Force
Copy-Item -LiteralPath (Join-Path $BuildPath 'esp32Util.ino.partitions.bin') -Destination (Join-Path $outputPath 'partitions.bin') -Force
$buildInfo = [ordered]@{
  target = $Board
  builtAtUtc = (Get-Date).ToUniversalTime().ToString('o')
  firmwareSha256 = (Get-FileHash -LiteralPath (Join-Path $outputPath 'firmware.bin') -Algorithm SHA256).Hash
  sourceSha256 = [ordered]@{}
}
Get-ChildItem -LiteralPath $sketchPath -File | ForEach-Object {
  $buildInfo.sourceSha256[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
}
$buildInfo | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $outputPath 'build-info.json') -Encoding utf8
Write-Output "Firmware build saved to $outputPath. No device was flashed."
