$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solution = Join-Path $root 'SemanticGpuEngine4-5.sln'

# ZIP portability rule: repository paths must be ASCII-only. Japanese text is
# allowed inside UTF-8 files, but non-ASCII archive entry names can be decoded
# differently by Windows extraction tools and would invalidate SOURCE_MANIFEST.
$nonPortablePaths = @(
    Get-ChildItem -Path $root -Recurse -Force | ForEach-Object {
        $_.FullName.Substring($root.Length).TrimStart('\','/')
    } | Where-Object { $_ -match '[^\x00-\x7F]' }
)
if ($nonPortablePaths.Count -gt 0) {
    Write-Host 'Non-ASCII repository paths are not allowed:' -ForegroundColor Red
    $nonPortablePaths | Sort-Object | ForEach-Object { Write-Host "  $_" }
    throw 'SGE4-5 archive path portability verification failed.'
}
if (-not (Test-Path -LiteralPath $solution)) { throw 'SemanticGpuEngine4-5.sln is missing.' }
if (Test-Path -LiteralPath (Join-Path $root 'SemanticGpuEngine4.sln')) { throw 'Old SemanticGpuEngine4.sln remains.' }
if (Test-Path -LiteralPath (Join-Path $root '12_SGE4Compiler')) { throw 'Old 12_SGE4Compiler project remains.' }
if (-not (Test-Path -LiteralPath (Join-Path $root '12_SGE4_5Compiler\12_SGE4_5Compiler.vcxproj'))) { throw '12_SGE4_5Compiler is missing.' }

$sourceFiles = Get-ChildItem -Path $root -Recurse -File | Where-Object {
    $_.Extension -in '.h','.cpp' -and
    $_.FullName -notlike (Join-Path $root 'build\*')
}
foreach ($file in $sourceFiles) {
    $text = Get-Content -Raw -LiteralPath $file.FullName -Encoding UTF8
    if ($text -match '(?m)\bnamespace\s+sge4(?=\s*\{|::)') {
        throw "Old sge4 namespace declaration remains: $($file.FullName)"
    }
    if ($text -match '(?m)\busing\s+namespace\s+sge4\s*;') {
        throw "Old sge4 using-directive remains: $($file.FullName)"
    }
    if ($text -match '(?<![A-Za-z0-9_])(?:::)?sge4::') {
        throw "Old sge4 namespace reference remains: $($file.FullName)"
    }
}

$projectFiles = @(Get-ChildItem -Path $root -Recurse -File -Filter *.vcxproj | Where-Object {
    $_.FullName -notlike (Join-Path $root 'build\*')
})
$guidOwners = @{}
foreach ($project in $projectFiles) {
    [xml]$xml = Get-Content -Raw -LiteralPath $project.FullName -Encoding UTF8
    $guid = [string]($xml.Project.PropertyGroup | Where-Object { $_.Label -eq 'Globals' } | Select-Object -First 1).ProjectGuid
    if ([string]::IsNullOrWhiteSpace($guid)) { throw "ProjectGuid missing: $($project.FullName)" }
    $key = $guid.ToUpperInvariant()
    if ($guidOwners.ContainsKey($key)) { throw "Duplicate ProjectGuid: $key" }
    $guidOwners[$key] = $project.FullName
}

$solutionText = Get-Content -Raw -LiteralPath $solution -Encoding UTF8
foreach ($project in $projectFiles) {
    $relative = $project.FullName.Substring($root.Length).TrimStart('\','/').Replace('/','\')
    if ($solutionText -notmatch [regex]::Escape($relative)) {
        throw "Project missing from solution: $relative"
    }
}

Write-Host "SGE4-5 independent identity check passed. Projects: $($projectFiles.Count)."
