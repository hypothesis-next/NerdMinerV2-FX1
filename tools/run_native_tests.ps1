$ErrorActionPreference = "Stop"

$projectDirectory = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectDirectory "test\.build"
$jsonInclude = Join-Path $projectDirectory ".pio\libdeps\ESP32_2432S028_2USB\ArduinoJson\src"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $jsonInclude)) {
    throw "ArduinoJson is unavailable. Build ESP32_2432S028_2USB once before running native tests."
}
if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Build Tools were not found."
}

$compiler = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find "VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe" | Select-Object -First 1
if (-not $compiler) {
    throw "The Visual C++ x64 compiler was not found."
}
$installationPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$developerShell = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"

New-Item -ItemType Directory -Force $buildDirectory | Out-Null
$sourceFiles = @(
    "test\native_poolstats.cpp",
    "src\poolstats\PoolRegistry.cpp",
    "src\poolstats\PoolStatsParsers.cpp",
    "src\poolstats\PoolStatsPolicy.cpp"
)
$sourceArguments = $sourceFiles -join " "
$executable = "test\.build\native_poolstats.exe"
$command = "`"$developerShell`" -arch=amd64 -host_arch=amd64 >nul && " +
    "cl.exe /nologo /EHsc /std:c++17 /W4 /D_CRT_SECURE_NO_WARNINGS " +
    "/I src /I `"$jsonInclude`" $sourceArguments /Fo:test\.build\ " +
    "/Fe:$executable && $executable test\fixtures"

Push-Location $projectDirectory
try {
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Native pool statistics tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$miningExecutable = "test\.build\native_mining_validation.exe"
$miningSources = "test\native_mining_validation.cpp " +
    "src\crypto\ReferenceSha256.cpp src\ShaTests\nerdSHA256plus.cpp"
$nativeArduinoStub = Join-Path $projectDirectory "test\native_stubs\Arduino.h"
$miningCommand = "`"$developerShell`" -arch=amd64 -host_arch=amd64 >nul && " +
    "cl.exe /nologo /EHsc /std:c++17 /O2 /W4 /D_CRT_SECURE_NO_WARNINGS " +
    "/FI`"$nativeArduinoStub`" /I test\native_stubs /I src " +
    "$miningSources /Fo:test\.build\ /Fe:$miningExecutable && " +
    "$miningExecutable 5000000"

Push-Location $projectDirectory
try {
    & cmd.exe /d /s /c $miningCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Native mining validation tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
