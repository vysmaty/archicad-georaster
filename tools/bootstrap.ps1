[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("27", "28", "29")]
    [string]$Version
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
    throw "uv is required. Install it from https://docs.astral.sh/uv/ and run this script again."
}

git submodule update --init --recursive
if ($LASTEXITCODE -ne 0) {
    throw "Unable to initialize the Graphisoft CMake tooling submodule."
}

uv run tools/devkit.py install $Version
if ($LASTEXITCODE -ne 0) {
    throw "Unable to install the Archicad $Version Development Kit."
}

uv run tools/devkit.py path $Version
