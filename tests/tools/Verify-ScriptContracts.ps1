$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $testsRoot
$failures = New-Object System.Collections.Generic.List[string]

function Get-RelativePath([string]$FullName) {
    return $FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
}

function Add-Failure([string]$Message) {
    [void]$failures.Add($Message)
}

$scriptFiles = @(
    Get-ChildItem -Path $root -Recurse -File |
        Where-Object {
            $_.Extension -in '.ps1', '.bat', '.cmd' -and
            $_.FullName -notlike (Join-Path $root 'build\*') -and
            $_.FullName -notlike (Join-Path $root '.git\*') -and
            $_.FullName -notlike (Join-Path $root '.vs\*')
        } |
        Sort-Object FullName
)

foreach ($file in $scriptFiles) {
    $relative = Get-RelativePath $file.FullName
    $bytes = [IO.File]::ReadAllBytes($file.FullName)

    $hasUtf8Bom = $bytes.Length -ge 3 -and
        $bytes[0] -eq 0xEF -and
        $bytes[1] -eq 0xBB -and
        $bytes[2] -eq 0xBF

    if ($file.Extension -eq '.ps1') {
        if (-not $hasUtf8Bom) {
            Add-Failure "$relative must be UTF-8 with BOM for Windows PowerShell 5.1 compatibility."
            continue
        }
        $text = [Text.Encoding]::UTF8.GetString($bytes, 3, $bytes.Length - 3)
    }
    else {
        # cmd.exe may expose a UTF-8 BOM as visible CP932 characters before
        # '@echo off'. Accept historical BOM scripts, but also permit BOM-less
        # UTF-8/ASCII entry-point BAT/CMD files.
        $offset = 0
        if ($hasUtf8Bom) { $offset = 3 }
        $text = [Text.Encoding]::UTF8.GetString($bytes, $offset, $bytes.Length - $offset)
    }
    if ($text.IndexOf([char]0) -ge 0) {
        Add-Failure "$relative contains a NUL character."
    }
    if ($text -match '(?<!\r)\n') {
        Add-Failure "$relative contains LF-only line endings; scripts must use CRLF."
    }
    if (-not $text.EndsWith("`r`n")) {
        Add-Failure "$relative must end with a CRLF newline."
    }

    if ($file.Extension -eq '.ps1') {
        $tokens = $null
        $parseErrors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            $file.FullName,
            [ref]$tokens,
            [ref]$parseErrors)

        foreach ($parseError in @($parseErrors)) {
            $extent = $parseError.Extent
            Add-Failure (
                "${relative}:$($extent.StartLineNumber):$($extent.StartColumnNumber): " +
                $parseError.Message)
        }
    }
    else {
        $firstLine = ($text -split "`r`n", 2)[0]
        if ($firstLine -ne '@echo off') {
            Add-Failure "$relative must begin with '@echo off'."
        }
    }
}

$requiredFiles = @(
    'tests/Invoke-SGE4Tests.ps1',
    'tests/run_suite.bat',
    'tests/tools/Verify-ScriptContracts.ps1',
    'tests/tools/Verify-SourceManifest.ps1',
    'tests/tools/Update-SourceManifest.ps1'
)
foreach ($relative in $requiredFiles) {
    $native = Join-Path $root ($relative.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $native)) {
        Add-Failure "Required script is missing: $relative"
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'SGE4 script contract verification failed:' -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host "  $failure" -ForegroundColor Red
    }
    throw "Script contract verification failed with $($failures.Count) issue(s)."
}

Write-Host "SGE4 script contract verification passed. Scripts: $($scriptFiles.Count)."
