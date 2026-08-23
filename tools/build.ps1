[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("27", "28", "29")]
    [string]$Version,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("CZE", "INT")]
    [string]$Language = "CZE"
)

$ErrorActionPreference = "Stop"
$preset = "ac$Version-$($Configuration.ToLowerInvariant())"
$buildDirectory = "build/$preset-$($Language.ToLowerInvariant())"
$env:AC_API_DEVKIT_DIR = (uv run tools/devkit.py path $Version).Trim()

if (-not $env:AC_API_DEVKIT_DIR.EndsWith("Support")) {
    throw "The Archicad DevKit path must point to its Support folder."
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
} else {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $visualStudio = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmake = Join-Path $visualStudio "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}

if (-not (Test-Path -LiteralPath $cmake)) {
    throw "CMake was not found on PATH or in Visual Studio Build Tools."
}

& $cmake --preset $preset -B $buildDirectory "-DAC_ADDON_LANGUAGE=$Language"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed for $preset."
}

& $cmake --build $buildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed for $preset."
}

& $cmake --build $buildDirectory --config $Configuration --target RUN_TESTS
if ($LASTEXITCODE -ne 0) {
    throw "CTest failed for $preset."
}
