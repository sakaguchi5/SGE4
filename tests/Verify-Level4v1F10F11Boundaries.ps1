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
        throw "$ProjectName direct references differ from its F10-F11 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
    }
}

function Assert-Sha256([string]$RelativePath, [string]$Expected) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Frozen file is missing: $RelativePath" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "F10-F11 changed a frozen file: $RelativePath`nExpected: $Expected`nActual:   $actual"
    }
}

Assert-ExactReferences '29B_Schema18CompositionRecovery' @(
    '00_Foundation', '13_PackageRuntime', '14_D3D12Backend',
    '13A_Schema18SharedDeviceDomain', '14A_Schema18SharedResources',
    '29A_Schema18StaticDagRuntime')

$domainText = Get-Content -Raw -LiteralPath (Join-Path $root '13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.cpp') -Encoding UTF8
foreach ($required in @('DiscardLeavesForRecovery', 'RecoverDeviceDomain',
    'MaterializeLeaves', 'stateAfter != runtime::DeviceRuntimeState::Active',
    'domain_->DeviceEpoch() <= oldEpoch')) {
    if (-not $domainText.Contains($required)) { throw "F10 whole-domain boundary is missing: $required" }
}
foreach ($forbidden in @('CompileCanonical', 'ProposeResourceFlowPlan',
    'VerifyAndSeal', 'FreezeVerifiedComposition')) {
    if ($domainText.Contains($forbidden)) { throw "F10 recovery pre-empts compile/link authority: $forbidden" }
}

$recoveryText = Get-Content -Raw -LiteralPath (Join-Path $root '29B_Schema18CompositionRecovery/Schema18CompositionRecovery.cpp') -Encoding UTF8
foreach ($required in @('loaded.resources_.reset()', 'loaded.endpointTokens_.assign(',
    'loaded.domain_->Recover(mode)', 'MaterializeSharedResources(',
    'recoveryInitialResources_', 'resourcesRematerialized')) {
    if (-not $recoveryText.Contains($required)) { throw "F10 Composition recovery boundary is missing: $required" }
}
foreach ($forbidden in @('ReadFrozenComposition(', 'CompileCompositionLeafCanonical',
    'ProposeResourceFlowPlan', 'VerifyAndSeal', 'FreezeVerifiedComposition')) {
    if ($recoveryText.Contains($forbidden)) { throw "F10 recovery reconstructs authority instead of reusing F6/F9: $forbidden" }
}

$recoveryTest = Get-Content -Raw -LiteralPath (Join-Path $root '46J_Schema18CompositionRecoveryTests/main.cpp') -Encoding UTF8
foreach ($required in @('RunBackendStaleEpochRejection', 'backend.ReadSharedBuffer(',
    'staleRead.Error().stage != "domain/readback"', 'ForceRemovalForTest',
    'RetryAdapterReacquisition', 'same-topology retry')) {
    if (-not $recoveryTest.Contains($required)) { throw "F10 recovery qualification is missing: $required" }
}

$backend = Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend/D3D12Backend.cpp') -Encoding UTF8
$backend = $backend.Replace("`r`n", "`n")
$backendMarker = '// SGE4 L4V1-F10-F11 DeviceDomain preserves removed-adapter exclusion across retry.'
$classAnchor = @('class DeviceDomain final : public runtime::IPackageDeviceDomain', '{') -join "`n"
$nextClassAnchor = 'class ExternalBufferResource final'
$classIndex = $backend.IndexOf($classAnchor, [StringComparison]::Ordinal)
if ($classIndex -lt 0) { throw 'F10 DeviceDomain class anchor is missing.' }
$endIndex = $backend.IndexOf($nextClassAnchor, $classIndex + $classAnchor.Length, [StringComparison]::Ordinal)
if ($endIndex -lt 0) { throw 'F10 DeviceDomain end anchor is missing.' }
$deviceDomainText = $backend.Substring($classIndex, $endIndex - $classIndex)
$markerIndex = $backend.LastIndexOf($backendMarker, $classIndex, [StringComparison]::Ordinal)
if ($markerIndex -lt 0 -or ($classIndex - $markerIndex) -gt ($backendMarker.Length + 4)) {
    throw 'F10 DeviceDomain recovery qualification marker is not attached to the DeviceDomain class.'
}
foreach ($requiredPattern in @(
    'if\s*\(hasExcludedAdapterLuid_\s*&&\s*SameLuid\(desc\.AdapterLuid,\s*excludedAdapterLuid_\)\)',
    'excludedAdapterLuid_\s*=\s*adapterLuid_\s*;[\s\S]*?hasExcludedAdapterLuid_\s*=\s*true\s*;',
    'LUID\s+adapterLuid_\{\}\s*;\s*LUID\s+excludedAdapterLuid_\{\}\s*;\s*bool\s+hasExcludedAdapterLuid_\s*=\s*false\s*;',
    'state_\s*=\s*runtime::DeviceRuntimeState::AwaitingAdapter')) {
    if ($deviceDomainText -notmatch $requiredPattern) {
        throw "F10 DeviceDomain-scoped recovery qualification is missing pattern: $requiredPattern"
    }
}
$retryStartText = 'if (mode==runtime::DeviceRecoveryMode::RetryAdapterReacquisition)'
$retryStart = $deviceDomainText.IndexOf($retryStartText, [StringComparison]::Ordinal)
if ($retryStart -lt 0) { throw 'F10 DeviceDomain retry branch is missing.' }
$retryEnd = $deviceDomainText.IndexOf('        if (state_!=runtime::DeviceRuntimeState::Active', $retryStart, [StringComparison]::Ordinal)
if ($retryEnd -lt 0) { throw 'F10 DeviceDomain retry branch end is missing.' }
$retryText = $deviceDomainText.Substring($retryStart, $retryEnd - $retryStart)
if ($retryText -match 'hasExcludedAdapterLuid_\s*=\s*false\s*;') {
    throw 'F10 retry illegally clears the removed-adapter exclusion.'
}
if ($retryText -notmatch 'auto\s+rebuilt\s*=\s*Initialize\(\)\s*;') {
    throw 'F10 retry no longer checks for a newly eligible adapter.'
}

# F8 Resource authority and F6 Frozen artifact remain byte-frozen.
Assert-Sha256 '14A_Schema18SharedResources/Schema18SharedResources.cpp' 'bf0c88d0370919abce33f564934dc0468d22754121c1bd396da05eee67721ed4'
Assert-Sha256 '14A_Schema18SharedResources/Schema18SharedResources.h' '4bbbae516e492fc99b9977fa5ea59f3f026a4b95957184a6edc89e8edf072d50'
Assert-Sha256 '19A_Schema18CompositionLinker/Schema18CompositionLinker.cpp' '09952c82b9f455b327a662a91598eb4d809faa3d4827bd56d9bea276e22e4f35'
Assert-Sha256 '19A_Schema18CompositionLinker/Schema18CompositionLinker.h' 'c2a93724fd51b627b408a820880613af4ac47ef0222415f5b97d936295ef7302'
Assert-Sha256 'SemanticGpuEngine4_Level4v1_F7_F9.sln' '7093b9a655ef8386e3793c832301c88a8561f5bce18c606648992252f9060c6c'

foreach ($solutionName in @('SemanticGpuEngine4.sln',
    'SemanticGpuEngine4_Level4v1_F1.sln', 'SemanticGpuEngine4_Level4v1_F2.sln',
    'SemanticGpuEngine4_Level4v1_F3.sln', 'SemanticGpuEngine4_Level4v1_F4_F6.sln',
    'SemanticGpuEngine4_Level4v1_F7_F9.sln')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $solutionName) -Encoding UTF8
    if ($text -match '29B_Schema18CompositionRecovery|46J_Schema18CompositionRecoveryTests|46K_Schema18FinalQualificationTests') {
        throw "F10-F11 projects leaked into frozen solution: $solutionName"
    }
}

$solutionPath = Join-Path $root 'SemanticGpuEngine4_Level4v1_F10_F11.sln'
$solution = Get-Content -Raw -LiteralPath $solutionPath -Encoding UTF8
if ($solution -match '\|Win32') { throw 'F10-F11 solution contains a Win32 configuration.' }
foreach ($project in @('13_PackageRuntime', '14_D3D12Backend', '15_PlatformWin32',
    '13A_Schema18SharedDeviceDomain', '14A_Schema18SharedResources',
    '29A_Schema18StaticDagRuntime', '29B_Schema18CompositionRecovery',
    '46J_Schema18CompositionRecoveryTests', '46K_Schema18FinalQualificationTests')) {
    if ($solution -notmatch ('"' + [regex]::Escape($project) + '"')) {
        throw "F10-F11 solution is missing project: $project"
    }
}
$projectNames = @([regex]::Matches($solution, '(?m)^Project\([^\x0A]+\) = "(?<name>[^"]+)"') |
    ForEach-Object { $_.Groups['name'].Value })
if ($projectNames.Count -ne 47) { throw "F10-F11 solution project count changed: $($projectNames.Count)" }
$projectSet = @{}
foreach ($name in $projectNames) { $projectSet[$name] = $true }
foreach ($name in $projectNames) {
    foreach ($reference in Get-DirectReferenceNames $name) {
        if (-not $projectSet.ContainsKey($reference)) {
            throw "F10-F11 solution omits transitive dependency $reference referenced by $name"
        }
    }
}

Write-Host 'L4V1-F10-F11 whole-domain recovery, removed-adapter safety, final qualification, authority, and x64 boundaries passed.'
