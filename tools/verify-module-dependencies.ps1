$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$violations = [System.Collections.Generic.List[string]]::new()

function Assert-NoMatch {
  param([string]$Path, [string]$Pattern, [string]$Message)
  $matches = Get-ChildItem -LiteralPath $Path -Recurse -File -Include *.h,*.cpp |
    Select-String -Pattern $Pattern
  foreach ($match in $matches) {
    $relative = $match.Path.Substring($repoRoot.Length).TrimStart('\')
    $violations.Add("$relative`:$($match.LineNumber): $Message")
  }
}

Assert-NoMatch "$repoRoot\SOZO-Common\lib" '#include\s*[<"](Arduino|Preferences|NimBLE|Adafruit_NeoPixel)' 'Common must remain platform independent.'
Assert-NoMatch "$repoRoot\SOZO-Common\lib\SozoSceneCore" '#include\s*[<"]SozoNode(Protocol|Messages)' 'SceneCore is a domain module and must not depend on the wire protocol.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-S3\lib\SozoControl" '#include\s*[<"](Arduino|Preferences|SettingsStore|LightingSceneStore)' 'Application control may depend only on persistence ports.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-S3\lib\SozoNetwork" '#include\s*[<"]SettingsStore' 'Network must depend on its credential repository port, not the NVS adapter.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-S3\lib\SozoWeb" 'SpaceSceneCoordinator|CommandRouter' 'Web must use the single lighting application entry.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-S3\lib\SozoSerial" 'SpaceSceneCoordinator|CommandRouter' 'Serial must use the single lighting application entry.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-S3\src" 'SOZO-ESP32-C3' 'S3 may not depend on the C3 firmware project.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-S3\lib" 'SOZO-ESP32-C3' 'S3 may not depend on the C3 firmware project.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-C3\src" 'SOZO-ESP32-S3' 'C3 may not depend on the S3 firmware project.'
Assert-NoMatch "$repoRoot\SOZO-ESP32-C3\lib" 'SOZO-ESP32-S3' 'C3 may not depend on the S3 firmware project.'

if ($violations.Count -gt 0) {
  $violations | ForEach-Object { Write-Error $_ }
  exit 1
}

Write-Host 'SOZO module dependency guard passed.'
