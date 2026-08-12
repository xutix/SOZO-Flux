param(
  [string]$C3Project = ""
)

$ErrorActionPreference = "Stop"
$commonRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($C3Project)) {
  $projectsRoot = Split-Path -Parent $commonRoot
  $C3Project = Join-Path $projectsRoot "SOZO-ESP32-C3"
}

$coreModules = @(
  "SozoDomain",
  "SpatialLightCore",
  "SozoLightingCore",
  "SozoSceneCore",
  "SozoNodeProtocol",
  "SozoBusCore"
)
$forbiddenTokens = @(
  "Arduino.h",
  "concrete LED driver",
  "FastLED",
  "NimBLE",
  "WiFi",
  "WebServer",
  "Preferences",
  "driver/gpio"
)
$violations = [System.Collections.Generic.List[string]]::new()

foreach ($module in $coreModules) {
  $moduleRoot = Join-Path $commonRoot "lib\$module"
  if (-not (Test-Path -LiteralPath $moduleRoot)) {
    $violations.Add("Missing core module: $moduleRoot")
    continue
  }
  foreach ($file in Get-ChildItem -LiteralPath $moduleRoot -Recurse -File) {
    foreach ($line in [System.IO.File]::ReadAllLines($file.FullName)) {
      if (-not $line.TrimStart().StartsWith("#include")) { continue }
      foreach ($token in $forbiddenTokens) {
        if ($line.Contains($token)) {
          $violations.Add("$($file.FullName): forbidden dependency '$token'")
        }
      }
    }
  }
}

$c3Config = Join-Path $C3Project "platformio.ini"
if (-not (Test-Path -LiteralPath $c3Config)) {
  $violations.Add("Missing C3 build configuration: $c3Config")
} else {
  $configContent = [System.IO.File]::ReadAllText($c3Config)
  if ($configContent.Contains("../SOZO-ESP32-n8r8/lib")) {
    $violations.Add("${c3Config}: C3 must not depend on the S3 library directory")
  }
}

if ($violations.Count -gt 0) {
  $violations | ForEach-Object { Write-Error $_ }
  exit 1
}

Write-Host "PASS module dependency boundaries"
