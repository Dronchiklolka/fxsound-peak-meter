# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Dronchiklolka
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BinaryDirectory,
    [Parameter(Mandatory=$true)][string]$JuceModulesPath,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Use a numeric major.minor.patch version.' }
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskExe = Join-Path $BinaryDirectory 'FxSound.exe'
$taskBytes = [IO.File]::ReadAllBytes($taskExe)
foreach ($taskEncoding in [Text.Encoding]::ASCII, [Text.Encoding]::Unicode) {
    if ($taskEncoding.GetString($taskBytes) -match '(?i)[a-z]:\\Users\\') {
        throw 'The executable contains a builder home-directory path. Build public releases in a neutral CI checkout.'
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$taskStage = Join-Path $OutputDirectory "fxsound-peak-meter-$Version-windows-x64"
if (Test-Path -LiteralPath $taskStage) { throw 'Package staging directory already exists; choose a fresh output directory.' }
New-Item -ItemType Directory -Path $taskStage -Force | Out-Null

# Deliberate allowlist: never recursively package an installed application,
# APPDATA, a build directory, user presets, diagnostic logs or PDB files.
Copy-Item -LiteralPath $taskExe -Destination $taskStage
$taskFactoryOutput = New-Item -ItemType Directory -Path (Join-Path $taskStage 'Factsoft')
Copy-Item -Path (Join-Path $taskRoot 'bin\x64\Factsoft\*.fac') -Destination $taskFactoryOutput.FullName
foreach ($taskFile in 'README.md','README.ru.md','LICENSE','NOTICE.md','MODIFICATIONS.md') {
    Copy-Item -LiteralPath (Join-Path $taskRoot $taskFile) -Destination $taskStage
}
$taskDocsOutput = New-Item -ItemType Directory -Path (Join-Path $taskStage 'docs')
Copy-Item -LiteralPath (Join-Path $taskRoot 'docs\CLIPPING_METER.md') -Destination $taskDocsOutput.FullName
Copy-Item -LiteralPath (Join-Path $taskRoot 'licenses') -Destination $taskStage -Recurse
$taskCommit = (& git -C $taskRoot rev-parse HEAD)
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the packaged source commit.' }
Set-Content -LiteralPath (Join-Path $taskStage 'SOURCE.txt') -Encoding utf8 -Value @"
Unofficial FxSound Peak Meter $Version
Source: https://github.com/Dronchiklolka/fxsound-peak-meter
Commit: $taskCommit
Matching source archive: fxsound-peak-meter-$Version-source-with-juce.zip
JUCE 6.1.6: 2f980209cc4091a4490bb1bafc5d530f16834e58
"@
$taskBinaryZip = Join-Path $OutputDirectory "fxsound-peak-meter-$Version-windows-x64.zip"
Compress-Archive -Path (Join-Path $taskStage '*') -DestinationPath $taskBinaryZip

$taskSourceZip = Join-Path $OutputDirectory "fxsound-peak-meter-$Version-source-with-juce.zip"
$taskSourcePaths = @('audiopassthru','dsp','fxsound','tests','tools','docs','licenses',
    'README.md','README.ru.md','LICENSE','NOTICE.md','MODIFICATIONS.md','.gitignore','bin/x64/Factsoft')
& git -C $taskRoot archive --format=zip "--output=$taskSourceZip" HEAD -- @taskSourcePaths
if ($LASTEXITCODE -ne 0) { throw 'Cannot archive the corresponding application source.' }

Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskModules = (Resolve-Path -LiteralPath $JuceModulesPath).Path
$taskJuceLicense = Join-Path (Split-Path $taskModules -Parent) 'LICENSE.md'
$taskArchive = [IO.Compression.ZipFile]::Open($taskSourceZip, [IO.Compression.ZipArchiveMode]::Update)
try {
    foreach ($taskFile in Get-ChildItem -LiteralPath $taskModules -Recurse -File) {
        $taskRelative = $taskFile.FullName.Substring($taskModules.Length).TrimStart('\','/').Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($taskArchive, $taskFile.FullName,
            ('third_party/JUCE/modules/' + $taskRelative), [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($taskArchive, $taskJuceLicense,
        'third_party/JUCE/LICENSE.md', [IO.Compression.CompressionLevel]::Optimal) | Out-Null
} finally { $taskArchive.Dispose() }
Write-Output "Public packages ready: $taskBinaryZip ; $taskSourceZip"
