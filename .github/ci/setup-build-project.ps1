<#
.SYNOPSIS
  Makes the dedicated build project the release workflow builds in.

.DESCRIPTION
  Copies a Satisfactory modding project (the one in which this mod is developed) to a separate folder,
  leaving out what a build doesn't need: the git history, Visual Studio files, local build output,
  and every mod except the ones the build itself needs (SML and the other infrastructure folders).
  The release workflow then builds in the copy, so it never
  touches the development project.

  The first build in the copy compiles the whole game module and takes a long time. Later builds are
  incremental, so keep the copy.

.EXAMPLE
  .\setup-build-project.ps1 -Source D:\Modding\Satisfactory -Destination E:\CI\project
#>
param(
    [Parameter(Mandatory)] [string] $Source,
    [Parameter(Mandatory)] [string] $Destination
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path "$Source\FactoryGame.uproject")) { throw "'$Source' is not a modding project (no FactoryGame.uproject)." }
if ([IO.Path]::GetFullPath($Source).TrimEnd('\') -eq [IO.Path]::GetFullPath($Destination).TrimEnd('\')) { throw 'Source and destination are the same folder.' }

# Directory names are excluded at any depth; full paths only where given.
$excludeDirs = @('.git', '.vs', '.vscode', 'Intermediate', 'Saved', 'DerivedDataCache')

# Of the folders in Mods, keep only what the build itself needs. Every other mod (including the one being
# released, which the workflow copies in on every build) is left out, so new mods need no change here.
$keepMods = @('SML', 'Alpakit', 'SMLEditor', 'AccessTransformers', 'GameFeatures', 'WwisePatches')
$excludeDirs += Get-ChildItem "$Source\Mods" -Directory |
    Where-Object { $keepMods -notcontains $_.Name } |
    ForEach-Object { $_.FullName }

robocopy $Source $Destination /E /XD @excludeDirs /NFL /NDL /NP
# robocopy reports success with codes 0-7.
if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }
# Don't let robocopy's "files were copied" code (1) become the script's own exit code.
$global:LASTEXITCODE = 0

Write-Host "Build project ready at $Destination"
