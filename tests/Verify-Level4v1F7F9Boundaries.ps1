$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

function Get-DirectReferenceNames([string]$ProjectName) {
    $projectPath = Join-Path $root "$ProjectName/$ProjectName.vcxproj"
    if (-not (Test-Path -LiteralPath $projectPath)) { throw "Project is missing: $ProjectName" }
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $names = foreach ($node in $xml.SelectNodes("//*[local-name()='ProjectReference']")) {
        Split-Path (Split-Path $node.Include -Parent) -Leaf
    }
    return [string[]]@($names | Sort-Object -Unique)
}

function Assert-ExactReferences([string]$ProjectName, [string[]]$Expected) {
    $actual = [string[]]@(Get-DirectReferenceNames $ProjectName)
    $expectedSorted = [string[]]@($Expected | Sort-Object -Unique)
    $difference = @(Compare-Object -ReferenceObject $expectedSorted -DifferenceObject $actual)
    if ($difference.Count -ne 0) {
        throw "$ProjectName direct references differ from its F7-F9 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
    }
}

function Assert-Sha256([string]$RelativePath, [string]$Expected) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Frozen file is missing: $RelativePath" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "F7-F9 changed a frozen F1-F6 file: $RelativePath`nExpected: $Expected`nActual:   $actual"
    }
}

Assert-ExactReferences '13A_Schema18SharedDeviceDomain' @(
    '00_Foundation', '09_FrozenPackageCore', '10_D3D12PackageSchema',
    '10A_D3D12CompositionSchema18', '13_PackageRuntime', '14_D3D12Backend',
    '16A_Schema18CompositionContract', '17A_Schema18ResourceFlowPlan',
    '18A_Schema18ResourceFlowVerifier', '19A_Schema18CompositionLinker')
Assert-ExactReferences '14A_Schema18SharedResources' @(
    '00_Foundation', '10_D3D12PackageSchema', '10A_D3D12CompositionSchema18',
    '13_PackageRuntime', '14_D3D12Backend', '13A_Schema18SharedDeviceDomain',
    '16A_Schema18CompositionContract', '17A_Schema18ResourceFlowPlan',
    '19A_Schema18CompositionLinker')
Assert-ExactReferences '29A_Schema18StaticDagRuntime' @(
    '00_Foundation', '13_PackageRuntime', '14_D3D12Backend',
    '13A_Schema18SharedDeviceDomain', '14A_Schema18SharedResources',
    '16A_Schema18CompositionContract', '17A_Schema18ResourceFlowPlan',
    '18A_Schema18ResourceFlowVerifier', '19A_Schema18CompositionLinker')

$f7Text = Get-Content -Raw -LiteralPath (Join-Path $root '13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.cpp') -Encoding UTF8
foreach ($required in @('ReadFrozenComposition', 'CompositionLeafView::Decode',
    'Schema17BasePackageBytes', 'CreateDeviceDomain', 'LoadIntoDomain')) {
    if (-not $f7Text.Contains($required)) { throw "F7 DeviceDomain boundary is missing: $required" }
}
if ($f7Text.Contains('CompileCanonical') -or $f7Text.Contains('ProposeResourceFlowPlan')) {
    throw 'F7 rematerialization calls a Compiler or Planner.'
}

$f8Text = Get-Content -Raw -LiteralPath (Join-Path $root '14A_Schema18SharedResources/Schema18SharedResources.cpp') -Encoding UTF8
foreach ($required in @('CreateSharedBuffer', 'TransitionSharedBuffer',
    'PrepareForEndpoint', 'UpdateAfterRelease', 'ReadSharedBuffer')) {
    if (-not $f8Text.Contains($required)) { throw "F8 shared-resource boundary is missing: $required" }
}

$f9Text = Get-Content -Raw -LiteralPath (Join-Path $root '29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.cpp') -Encoding UTF8
foreach ($required in @('ReadFrozenComposition', 'const auto& plan = artifact.plan;',
    'for (const auto& entry : plan.schedule)', 'PrepareForEndpoint',
    'SubmitStaticComposition', 'ReadStaticCompositionBuffer')) {
    if (-not $f9Text.Contains($required)) { throw "F9 static Runtime boundary is missing: $required" }
}
foreach ($forbidden in @('ProposeResourceFlowPlan', 'VerifyAndSeal',
    'FreezeVerifiedComposition', 'CompileCompositionLeafCanonical', 'CompileCanonical')) {
    if ($f9Text.Contains($forbidden)) { throw "F9 Runtime pre-empts compile/link authority: $forbidden" }
}

$backendHeader = Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend/D3D12Backend.h') -Encoding UTF8
$backendSource = Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend/D3D12Backend.cpp') -Encoding UTF8
if (-not $backendHeader.Contains('TransitionSharedBuffer(') -or
    -not $backendSource.Contains('D3D12Backend::TransitionSharedBuffer(')) {
    throw 'F8 Backend shared-Buffer state-transition boundary is not applied.'
}
foreach ($required in @('native->Owner() != nativeDomain',
    'token->Owner() != nativeDomain', 'native->CurrentState() == beforeState',
    'queue->Wait(token->NativeFence()', 'native->SetCurrentState(afterState)')) {
    if (-not $backendSource.Contains($required)) {
        throw "F8 Backend transition authority check is missing: $required"
    }
}

# F1-F6 products remain byte-frozen. Runtime/Backend are the only pre-existing
# implementation files intentionally extended by this phase.
Assert-Sha256 '19A_Schema18CompositionLinker/Schema18CompositionLinker.cpp' '09952c82b9f455b327a662a91598eb4d809faa3d4827bd56d9bea276e22e4f35'
Assert-Sha256 '19A_Schema18CompositionLinker/Schema18CompositionLinker.h' 'c2a93724fd51b627b408a820880613af4ac47ef0222415f5b97d936295ef7302'
Assert-Sha256 'SemanticGpuEngine4_Level4v1_F4_F6.sln' '5ae9907faa7807802c9c29996186afe439e7928137b43a6a15bdf6252f82eb88'

foreach ($solutionName in @('SemanticGpuEngine4.sln',
    'SemanticGpuEngine4_Level4v1_F1.sln', 'SemanticGpuEngine4_Level4v1_F2.sln',
    'SemanticGpuEngine4_Level4v1_F3.sln', 'SemanticGpuEngine4_Level4v1_F4_F6.sln')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $solutionName) -Encoding UTF8
    if ($text -match '13A_Schema18SharedDeviceDomain|14A_Schema18SharedResources|29A_Schema18StaticDagRuntime|46G_|46H_|46I_') {
        throw "F7-F9 projects leaked into frozen solution: $solutionName"
    }
}

$solutionPath = Join-Path $root 'SemanticGpuEngine4_Level4v1_F7_F9.sln'
$solution = Get-Content -Raw -LiteralPath $solutionPath -Encoding UTF8
if ($solution -match '\|Win32') { throw 'F7-F9 solution contains a Win32 configuration.' }
foreach ($project in @('13_PackageRuntime', '14_D3D12Backend', '15_PlatformWin32',
    '13A_Schema18SharedDeviceDomain', '14A_Schema18SharedResources',
    '29A_Schema18StaticDagRuntime', '46G_Schema18SharedDeviceDomainTests',
    '46H_Schema18SharedResourceTests', '46I_Schema18StaticDagRuntimeTests')) {
    if ($solution -notmatch ('"' + [regex]::Escape($project) + '"')) {
        throw "F7-F9 solution is missing project: $project"
    }
}
$projectNames = @([regex]::Matches($solution, '(?m)^Project\([^\x0A]+\) = "(?<name>[^"]+)"') |
    ForEach-Object { $_.Groups['name'].Value })
if ($projectNames.Count -ne 44) { throw "F7-F9 solution project count changed: $($projectNames.Count)" }
$projectSet = @{}
foreach ($name in $projectNames) { $projectSet[$name] = $true }
foreach ($name in $projectNames) {
    foreach ($reference in Get-DirectReferenceNames $name) {
        if (-not $projectSet.ContainsKey($reference)) {
            throw "F7-F9 solution omits transitive dependency $reference referenced by $name"
        }
    }
}

Write-Host 'L4V1-F7-F9 Shared DeviceDomain, shared Resource, static DAG Runtime, and x64 boundaries passed.'
