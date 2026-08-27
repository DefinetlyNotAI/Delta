[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path))
    {
        throw "Missing ${Description}: $Path"
    }
}

function Invoke-CMake
{
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    & cmake @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "CMake failed with exit code $LASTEXITCODE."
    }
}

function Get-Sha256
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $sha256 = [Security.Cryptography.SHA256]::Create()
    try
    {
        $bytes = [IO.File]::ReadAllBytes($Path)
        return ([BitConverter]::ToString($sha256.ComputeHash($bytes))).Replace("-", "")
    }
    finally
    {
        $sha256.Dispose()
    }
}

$root = Split-Path -Parent $PSCommandPath
$buildDirectory = Join-Path $root "cmake-build-$($Configuration.ToLowerInvariant() )"
$distRoot = Join-Path $root "dist"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$packageDirectory = Join-Path $distRoot "Delta_$timestamp"
$suffix = 1

while (Test-Path -LiteralPath $packageDirectory)
{
    $packageDirectory = Join-Path $distRoot "Delta_${timestamp}_$suffix"
    $suffix++
}

Require-Path (Join-Path $root "CMakeLists.txt") "CMake project"
Require-Path (Join-Path $root "img\creep") "creep image directory"
Require-Path (Join-Path $root "img\countdown") "countdown image directory"
Require-Path (Join-Path $root "img\static.jpg") "static image"
Require-Path (Join-Path $root "mus") "audio directory"
Require-Path (Join-Path $root "text\poems.txt") "poem text"

Invoke-CMake -Arguments @(
    "-S", $root,
    "-B", $buildDirectory,
    "-DCMAKE_BUILD_TYPE=$Configuration"
)
Invoke-CMake -Arguments @(
    "--build", $buildDirectory,
    "--config", $Configuration,
    "--target", "ARG", "TREE"
)

$argExecutable = Join-Path $buildDirectory "ARG.exe"
$treeExecutable = Join-Path $buildDirectory "TREE.exe"
Require-Path $argExecutable "built ARG.exe"
Require-Path $treeExecutable "built TREE.exe"

$treePayload = [Convert]::ToBase64String(
        [IO.File]::ReadAllBytes($treeExecutable)
)

New-Item -ItemType Directory -Path $packageDirectory | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packageDirectory "img") | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packageDirectory "text") | Out-Null

Copy-Item -LiteralPath $argExecutable -Destination $packageDirectory
Copy-Item -LiteralPath (Join-Path $root "img\creep") `
    -Destination (Join-Path $packageDirectory "img") -Recurse
Copy-Item -LiteralPath (Join-Path $root "img\countdown") `
    -Destination (Join-Path $packageDirectory "img") -Recurse
Copy-Item -LiteralPath (Join-Path $root "img\static.jpg") `
    -Destination (Join-Path $packageDirectory "img\static.jpg")
Copy-Item -LiteralPath (Join-Path $root "mus") `
    -Destination (Join-Path $packageDirectory "mus") -Recurse
Copy-Item -LiteralPath (Join-Path $root "text\poems.txt") `
    -Destination (Join-Path $packageDirectory "text\poems.txt")

[IO.File]::WriteAllText(
        (Join-Path $packageDirectory "tree.dat"),
        $treePayload,
        [Text.Encoding]::ASCII
)

Remove-Item -LiteralPath $treeExecutable -Force

$argHash = Get-Sha256 (Join-Path $packageDirectory "ARG.exe")
$treeHash = Get-Sha256 (Join-Path $packageDirectory "tree.dat")

@"
Delta distribution package
Built: $( Get-Date -Format "yyyy-MM-dd HH:mm:ss K" )
Configuration: $Configuration

Run ARG.exe from this folder. tree.dat is the Base64-encoded TREE payload
consumed by ARG during Step 7; no standalone TREE.exe is included.

ARG.exe SHA-256: $argHash
tree.dat SHA-256: $treeHash
"@ | Set-Content -LiteralPath (Join-Path $packageDirectory "README.txt") `
    -Encoding ASCII

Write-Host "Created distribution: $packageDirectory"
