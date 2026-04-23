$ErrorActionPreference = 'Stop'
$script:ThisScriptPath = $PSCommandPath
if ([string]::IsNullOrWhiteSpace($script:ThisScriptPath)) {
    $script:ThisScriptPath = $MyInvocation.PSCommandPath
}

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$input = Join-Path $repoRoot 'include/gaia.h'
$output = Join-Path $repoRoot 'single_include/gaia.h'
$includeDir = Join-Path $repoRoot 'include'

function Show-Usage {
    Write-Host 'Usage:'
    Write-Host '  ./make_single_header.ps1 [--format|--no-format] [clang-format-executable]'
    Write-Host ''
    Write-Host 'Options:'
    Write-Host '  --format      Enable formatting (default when clang-format is available).'
    Write-Host '  --no-format   Skip formatting.'
    Write-Host '  -h, --help    Print this help.'
    Write-Host ''
    Write-Host 'Examples:'
    Write-Host '  ./make_single_header.ps1                          # default: format when clang-format is available'
    Write-Host '  ./make_single_header.ps1 clang-format-17          # same, but with an explicit formatter'
    Write-Host '  ./make_single_header.ps1 --format clang-format-17 # explicit format request'
    Write-Host '  ./make_single_header.ps1 --no-format              # skip formatting'
}

function Get-ScriptArguments {
    $commandLineArgs = [Environment]::GetCommandLineArgs()
    $scriptPath = [System.IO.Path]::GetFullPath($script:ThisScriptPath)
    $scriptArgs = [System.Collections.Generic.List[string]]::new()
    $seenScript = $false

    foreach ($arg in $commandLineArgs) {
        if (-not $seenScript) {
            if ([string]::Equals([System.IO.Path]::GetFullPath($arg), $scriptPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                $seenScript = $true
            }
            continue
        }

        $scriptArgs.Add($arg)
    }

    return $scriptArgs.ToArray()
}

function Parse-Arguments {
    param(
        [string[]]$CliArgs
    )

    $formatMode = 'auto'
    $clangFormatArg = $null

    foreach ($arg in $CliArgs) {
        if ([string]::IsNullOrWhiteSpace($arg)) {
            continue
        }

        switch ($arg) {
            '--format' {
                $formatMode = 'on'
                continue
            }
            '--no-format' {
                $formatMode = 'off'
                continue
            }
            '-h' {
                Show-Usage
                exit 0
            }
            '--help' {
                Show-Usage
                exit 0
            }
            default {
                if ($clangFormatArg) {
                    throw "ERROR: unexpected argument '$arg'."
                }
                $clangFormatArg = $arg
            }
        }
    }

    return @{
        FormatMode = $formatMode
        ClangFormatArg = $clangFormatArg
    }
}

function Resolve-ClangFormat {
    param(
        [string]$Arg,
        [string]$FormatMode
    )

    if ($FormatMode -eq 'off') {
        return $null
    }

    if ($Arg) {
        if (Test-Path -LiteralPath $Arg -PathType Leaf) {
            return (Resolve-Path -LiteralPath $Arg).Path
        }

        $cmd = Get-Command $Arg -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) {
            if ($cmd.Path) {
                return $cmd.Path
            }
            return $cmd.Source
        }

        throw "ERROR: '$Arg' was not found as a file and is not on PATH."
    }

    foreach ($candidate in @('clang-format', 'clang-format-19', 'clang-format-18', 'clang-format-17')) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) {
            if ($cmd.Path) {
                return $cmd.Path
            }
            return $cmd.Source
        }
    }

    return $null
}

function Resolve-IncludePath {
    param(
        [string]$CurrentFileDir,
        [string]$IncludePath
    )

    $candidate = Join-Path $CurrentFileDir $IncludePath
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $resolved = (Resolve-Path -LiteralPath $candidate).Path
        if ($resolved.StartsWith($includeDir, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $resolved
        }
    }

    $candidate = Join-Path $includeDir $IncludePath
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return (Resolve-Path -LiteralPath $candidate).Path
    }

    $baseName = Split-Path -Leaf $IncludePath
    $match = Get-ChildItem -Path $includeDir -Recurse -File -Filter $baseName | Select-Object -First 1
    if ($match) {
        return $match.FullName
    }

    return $null
}

$scriptArgs = Get-ScriptArguments
$argsParsed = Parse-Arguments $scriptArgs
$formatMode = $argsParsed.FormatMode
$clangFormat = Resolve-ClangFormat -Arg $argsParsed.ClangFormatArg -FormatMode $formatMode

Write-Host "Input        : $input"
Write-Host "Output       : $output"
if ($clangFormat) {
    Write-Host "clang-format : $clangFormat"
}
elseif ($formatMode -eq 'off') {
    Write-Host 'clang-format : disabled'
}
else {
    Write-Host 'clang-format : not found - formatting will be skipped'
}

[System.IO.Directory]::CreateDirectory((Split-Path -Parent $output)) | Out-Null

$visited = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$builder = [System.Text.StringBuilder]::new()

[void]$builder.AppendLine('// Amalgamated single-header build of Gaia-ECS.')
[void]$builder.AppendLine('// The file is generated. Do not edit it.')
[void]$builder.AppendLine('#pragma once')
[void]$builder.AppendLine('')

$includeRegex = [regex]'^\s*#\s*include\s*(["<])([^">]+)[">]'
$pragmaOnceRegex = [regex]'^\s*#\s*pragma\s+once\s*$'

function Append-File {
    param(
        [string]$FilePath
    )

    $resolvedFile = (Resolve-Path -LiteralPath $FilePath).Path
    if (-not $visited.Add($resolvedFile)) {
        return
    }

    $fileDir = Split-Path -Parent $resolvedFile

    foreach ($line in [System.IO.File]::ReadLines($resolvedFile)) {
        if ($pragmaOnceRegex.IsMatch($line)) {
            continue
        }

        $match = $includeRegex.Match($line)
        if (-not $match.Success) {
            [void]$builder.AppendLine($line)
            continue
        }

        $delimiter = $match.Groups[1].Value
        if ($delimiter -eq '<') {
            [void]$builder.AppendLine($line)
            continue
        }

        $includePath = $match.Groups[2].Value.Trim()
        if (-not $includePath) {
            [void]$builder.AppendLine($line)
            continue
        }

        $resolvedInclude = Resolve-IncludePath -CurrentFileDir $fileDir -IncludePath $includePath
        if ($resolvedInclude) {
            Append-File $resolvedInclude
        }
        else {
            [void]$builder.AppendLine($line)
        }
    }
}

Append-File $input

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($output, $builder.ToString(), $utf8NoBom)

if ($clangFormat) {
    Write-Host "Formatting   : $output"
    & $clangFormat -i --style=file $output
    if ($LASTEXITCODE -ne 0) {
        throw 'ERROR: clang-format failed.'
    }
}
else {
    Write-Host 'Formatting   : skipped'
}

$lineCount = ([System.IO.File]::ReadLines($output) | Measure-Object -Line).Lines
Write-Host "Done: $output ($lineCount lines)"
