# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Dronchiklolka
[CmdletBinding()]
param(
    [string]$JuceModulesPath,
    [string]$OutputDirectory,
    [switch]$TestOnly,
    [switch]$DspTestsOnly
)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVswhere)) { throw 'Install Visual Studio 2022 C++ v143 Build Tools and Windows SDK.' }
$taskVsPath = (& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $taskVsPath) { throw 'The C++ build component is not installed.' }
$taskMsbuild = Join-Path $taskVsPath 'MSBuild\Current\Bin\MSBuild.exe'
$taskBuild = Join-Path $taskRoot 'build\clip-meter'
New-Item -ItemType Directory -Path $taskBuild -Force | Out-Null

# Use Microsoft's Developer PowerShell rather than a generated CMD file, so paths
# containing spaces or non-ASCII characters reach cl.exe without codepage conversion.
& (Join-Path $taskVsPath 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$taskCompiler = (Get-Command cl.exe -ErrorAction Stop).Source

function Invoke-MeterTest([string]$Name, [string[]]$LinkOptions = @()) {
    $taskSource = Join-Path $taskRoot "tests\$Name.cpp"
    $taskTestExe = Join-Path $taskBuild "$Name.exe"
    $taskLog = Join-Path $taskBuild "$Name.log"
    $taskOptions = @('/nologo', '/std:c++17', '/EHsc', '/MT', '/W4', '/WX', '/O2', '/fp:precise', '/utf-8',
        $taskSource, "/Fe:$taskTestExe", "/Fo:$taskBuild\$Name.obj")
    & $taskCompiler @taskOptions @LinkOptions *> $taskLog
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $taskLog -Tail 45; throw "$Name failed. See $taskLog" }
    # Legacy DSP errors can open a modal dialog; never let CI wait indefinitely.
    $taskStdout = Join-Path $taskBuild "$Name.stdout.log"
    $taskStderr = Join-Path $taskBuild "$Name.stderr.log"
    $taskProcess = Start-Process -FilePath $taskTestExe -WorkingDirectory $taskBuild -PassThru -WindowStyle Hidden -RedirectStandardOutput $taskStdout -RedirectStandardError $taskStderr
    if (-not $taskProcess.WaitForExit(30000)) {
        $taskProcess.Kill()
        throw "$Name timed out; check for a legacy DSP error dialog."
    }
    Get-Content -LiteralPath $taskStdout,$taskStderr | Add-Content -LiteralPath $taskLog
    if ($taskProcess.ExitCode -ne 0) { Get-Content -LiteralPath $taskLog -Tail 45; throw "$Name failed. See $taskLog" }
    Get-Content -LiteralPath $taskLog | Select-Object -Last 1
}
function Invoke-MeterBuild([string]$Project, [string]$LogName) {
    $taskLog = Join-Path $taskBuild $LogName
    $taskOptions = @('/m:2', '/nologo', '/v:minimal', '/p:Configuration=Release', '/p:Platform=x64')
    if ($JuceModulesPath) { $taskOptions += "/p:JuceModulesPath=$JuceModulesPath" }
    & $taskMsbuild (Join-Path $taskRoot $Project) @taskOptions *> $taskLog
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $taskLog -Tail 55; throw "Build failed. See $taskLog" }
}

Invoke-MeterTest 'AudioPeakMeterTests'
if ($TestOnly) { return }

if ($DspTestsOnly) {
    Invoke-MeterBuild 'dsp\DfxDsp.vcxproj' 'dsp-build.log'
    Invoke-MeterBuild 'audiopassthru\audiopassthru.vcxproj' 'passthru-build.log'
} else {
    if (-not $JuceModulesPath -or -not (Test-Path -LiteralPath (Join-Path $JuceModulesPath 'juce_core\juce_core.h'))) {
        throw 'Pass -JuceModulesPath pointing to JUCE 6.1.6 modules, or use -TestOnly / -DspTestsOnly.'
    }
    New-Item -ItemType Directory -Path (Join-Path $taskRoot 'bin\x64') -Force | Out-Null
    Invoke-MeterBuild 'fxsound\Project\FxSound.sln' 'build.log'
}

$taskDspLib = Join-Path $taskRoot 'lib\Release-x64\DfxDsp.lib'
$taskSharedLib = Join-Path $taskRoot 'lib\Release-x64\audiopassthru.lib'
Invoke-MeterTest 'DspPeakMeterTests' -LinkOptions @('/link', $taskDspLib, $taskSharedLib, 'advapi32.lib', 'user32.lib', 'winmm.lib', 'shell32.lib', 'shlwapi.lib', 'ole32.lib', 'uuid.lib')
if ($DspTestsOnly) { return }

if (-not $OutputDirectory) { $OutputDirectory = Join-Path $taskBuild 'app' }
$taskBuiltExe = Join-Path $taskRoot 'fxsound\Project\x64\Release\App\FxSound.exe'
if (-not (Test-Path -LiteralPath $taskBuiltExe)) { throw 'Expected FxSound.exe output is missing.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Copy-Item -LiteralPath $taskBuiltExe -Destination (Join-Path $OutputDirectory 'FxSound.exe') -Force
$taskDefaultPresets = Join-Path $taskRoot 'bin\x64\Factsoft'
if (-not (Test-Path -LiteralPath $taskDefaultPresets)) {
    throw 'Upstream default presets are missing. Use a full checkout or include bin/x64/Factsoft in the sparse checkout.'
}
$taskPresetOutput = New-Item -ItemType Directory -Path (Join-Path $OutputDirectory 'Factsoft') -Force
Copy-Item -Path (Join-Path $taskDefaultPresets '*.fac') -Destination $taskPresetOutput.FullName -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'LICENSE') -Destination $OutputDirectory -Force
Write-Output "Build ready: $(Join-Path $OutputDirectory 'FxSound.exe')"
