param(
    [string]$Environment = "ESP32_2432S028_2USB",
    [int]$StaticRamBytes = 0,
    [int]$ProgramFlashBytes = 0
)

$ErrorActionPreference = "Stop"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Write-Utf8NoBom {
    param([string]$Path, [string]$Content)
    [System.IO.File]::WriteAllText($Path, $Content + [Environment]::NewLine, $utf8NoBom)
}

$projectDirectory = Split-Path -Parent $PSScriptRoot
$versionHeader = Join-Path $projectDirectory "src\version.h"
$versionMatch = Select-String -Path $versionHeader -Pattern '#define\s+CURRENT_VERSION\s+"([^"]+)"'
if (-not $versionMatch) {
    throw "CURRENT_VERSION was not found in src/version.h."
}
$version = $versionMatch.Matches[0].Groups[1].Value
$releaseDirectory = Join-Path $projectDirectory "firmware\$version"
$prefix = "NerdMinerV2-$version-$Environment"
$applicationName = "$prefix-firmware.bin"
$factoryName = "$prefix-factory.bin"
$applicationPath = Join-Path $releaseDirectory $applicationName
$factoryPath = Join-Path $releaseDirectory $factoryName

foreach ($required in @($applicationPath, $factoryPath)) {
    if (-not (Test-Path $required)) {
        throw "Required build artifact is missing: $required"
    }
}

Push-Location $projectDirectory
try {
    $status = git status --porcelain
    if ($status) {
        throw "Commit the reviewed source before creating a release package."
    }
    $commit = git rev-parse HEAD
    $branch = git branch --show-current
    $sourceName = "$prefix-source.zip"
    $sourcePath = Join-Path $releaseDirectory $sourceName
    git archive --format=zip --output="$sourcePath" `
        --prefix="NerdMinerV2-$version/" HEAD -- . ":(exclude)bin"
    if ($LASTEXITCODE -ne 0) {
        throw "git archive failed."
    }
}
finally {
    Pop-Location
}

$manifest = [ordered]@{
    name = "NerdMiner v2 Multi-Pool (unofficial)"
    version = $version
    new_install_prompt_erase = $true
    builds = @(
        [ordered]@{
            chipFamily = "ESP32"
            parts = @(
                [ordered]@{ path = $factoryName; offset = 0 }
            )
        }
    )
}
Write-Utf8NoBom (Join-Path $releaseDirectory "manifest.json") ($manifest | ConvertTo-Json -Depth 6)

$buildUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$applicationSize = (Get-Item $applicationPath).Length
$factorySize = (Get-Item $factoryPath).Length
$buildInfo = @"
Version: $version
Status: local release candidate; physical ESP32 validation deferred
Target environment: $Environment
Target MCU: classic ESP32 / Xtensa LX6 / 240 MHz / 4 MB flash
Git branch: $branch
Git commit: $commit
Build UTC: $buildUtc
PlatformIO platform: espressif32 6.6.0
Framework: Arduino-ESP32 2.0.14 (ESP-IDF 4.4.6)
Toolchain: xtensa-esp32 8.4.0+2021r2-patch5
Static RAM used: $StaticRamBytes bytes
Application flash used: $ProgramFlashBytes bytes
Application binary size: $applicationSize bytes
Factory binary size: $factorySize bytes
"@
Write-Utf8NoBom (Join-Path $releaseDirectory "BUILD_INFO.txt") $buildInfo

$flashing = @"
# Flashing ESP32_2432S028_2USB

This is an unofficial local release candidate. It has not yet been validated on
physical ESP32 hardware.

For a complete installation, flash $factoryName at offset 0x0000. The merged
image contains the classic ESP32 bootloader at 0x1000, the partition table at
0x8000, boot_app0 at 0xE000, and the application at 0x10000.

The application-only file $applicationName may be flashed at 0x10000 only
when the board already has the compatible classic-ESP32 bootloader and
huge_app.csv partition layout. Use the factory image for the eventual first
release-candidate hardware test.

Do not flash this artifact to a different board target. Preserve a known-good
factory image for rollback before physical testing.
"@
Write-Utf8NoBom (Join-Path $releaseDirectory "FLASHING.md") $flashing

Copy-Item (Join-Path $projectDirectory "CHANGELOG.md") `
    (Join-Path $releaseDirectory "CHANGELOG.md") -Force
Copy-Item (Join-Path $projectDirectory "PERFORMANCE_RELEASE_NOTES.md") `
    (Join-Path $releaseDirectory "RELEASE_NOTES.md") -Force
Copy-Item (Join-Path $projectDirectory "LICENSE") `
    (Join-Path $releaseDirectory "LICENSE") -Force

$checksumTargets = @(
    $applicationName,
    $factoryName,
    $sourceName,
    "manifest.json",
    "BUILD_INFO.txt",
    "FLASHING.md",
    "CHANGELOG.md",
    "RELEASE_NOTES.md",
    "LICENSE"
)
$checksumLines = foreach ($name in $checksumTargets) {
    $hash = (Get-FileHash -Algorithm SHA256 (Join-Path $releaseDirectory $name)).Hash.ToLowerInvariant()
    "$hash  $name"
}
$checksumLines | Set-Content -Encoding ascii (Join-Path $releaseDirectory "SHA256SUMS.txt")

Write-Host "Release package created in $releaseDirectory"
