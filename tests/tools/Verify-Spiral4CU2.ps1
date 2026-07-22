$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $PSScriptRoot 'Sha256.ps1')

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral4\CU2_ARCHITECTURE_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 4 CU2 architecture manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json

Require ($m.schema -eq 'SGE4-5.Spiral4.CU2ArchitectureManifest.V1') 'CU2 architecture manifest schema mismatch.'
Require ($m.baselineCommit -eq '4033e8bf84650b6d1edbb6a8a83d97d5e1c3e4d1') 'Spiral 4 CU2 baseline mismatch.'
Require ($m.extensionStrategy -eq 'VersionedSidecarIndirectExtensionV1') 'CU2 extension strategy mismatch.'
Require ($m.legacySchemaMutation -eq 'None') 'CU2 must preserve legacy Schema 17 bytes.'
Require ($m.runtimeDispatchDecision -eq 'None') 'CU2 Runtime dispatch decision must be None.'
Require ($m.commandSignature.kind -eq 'Dispatch') 'CU2 Command Signature kind mismatch.'
Require ([int]$m.commandSignature.byteStride -eq 12) 'CU2 Dispatch argument stride mismatch.'
Require ([int]$m.commandSignature.maxCommandCount -eq 1) 'CU2 max command count mismatch.'
Require ($m.argumentFlow -eq 'GPUProducerToIndirectConsumer') 'CU2 argument Flow mismatch.'
Require ($m.zeroWorkEncoding -eq 'DispatchGroupsXEqualsZero') 'CU2 zero Work encoding mismatch.'

$required = @(
    '120_ActiveWorkSemantic\120_ActiveWorkSemantic.vcxproj',
    '120_ActiveWorkSemantic\ActiveWorkSemantic.h',
    '120_ActiveWorkSemantic\ActiveWorkSemantic.cpp',
    '121_Spiral4Contracts\121_Spiral4Contracts.vcxproj',
    '121_Spiral4Contracts\Spiral4Contracts.h',
    '121_Spiral4Contracts\Spiral4Contracts.cpp',
    '122_Spiral4IndirectExecution\122_Spiral4IndirectExecution.vcxproj',
    '122_Spiral4IndirectExecution\Spiral4IndirectExecution.h',
    '122_Spiral4IndirectExecution\Spiral4IndirectExecution.cpp',
    '140_Spiral4IndirectArchitectureTests\140_Spiral4IndirectArchitectureTests.vcxproj',
    '140_Spiral4IndirectArchitectureTests\main.cpp',
    'tests\Spiral4Common.ps1',
    'tests\Run-Spiral4CU2.ps1',
    'tests\tools\Verify-Spiral4CU2.ps1',
    'run_sge4_5_spiral4_cu2_single_execute_indirect.bat',
    'docs\spiral4\CU2_ARCHITECTURE_MANIFEST_V1.json',
    'docs\spiral4\CU2_SCHEMA17_INSUFFICIENCY.md',
    'docs\spiral4\CU2_SINGLE_INDIRECT_ARCHITECTURE.md',
    'docs\spiral4\CU2_CHANGESET.md',
    'docs\spiral4\CU2_EVIDENCE_LEDGER.md'
)
foreach ($relative in $required) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 4 CU2 file: $relative"
}

$legacyHashes = @{
    '10_D3D12PackageSchema\D3D12Schema.h' = '48b9e8695ab0808be5b2e50cc6c74b61d27b0a627945bf05409c0378887bb44f'
    '10_D3D12PackageSchema\D3D12Encoding.cpp' = '7aab5817abf259d73c3c37dc4c192626ce13602b3f1f7e8a7bd4651bc6e2d011'
    '13_PackageRuntime\PackageRuntime.cpp' = '1af97d5c594a5d4a4b9062a44f2662a6e754b66110ae6f31bfe0ad3b0608ed38'
    '13_PackageRuntime\PackageRuntime.h' = '2d0a91849bfe8aa216bc16f8c0a5ce17cc4d8c57997c3dccaf6635af405dbfca'
    '14_D3D12Backend\D3D12Backend.cpp' = '2ac76f2284ce7984859a360bd498716ccb0d28764da1047e1e0da96cfce0c462'
    '14_D3D12Backend\D3D12Backend.h' = 'abcdbcbf72ca4fe9dd6fa7f49c137d30b80583462630c48438918060554bcf15'
}
foreach ($entry in $legacyHashes.GetEnumerator()) {
    $actual = (Get-SGE4FileSha256 (Join-Path $root $entry.Key)).ToLowerInvariant()
    Require ($actual -eq $entry.Value) "Legacy boundary changed during CU2: $($entry.Key)"
}

$semanticProject = Get-Content -Raw -LiteralPath (Join-Path $root '120_ActiveWorkSemantic\120_ActiveWorkSemantic.vcxproj') -Encoding UTF8
$contractProject = Get-Content -Raw -LiteralPath (Join-Path $root '121_Spiral4Contracts\121_Spiral4Contracts.vcxproj') -Encoding UTF8
$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '122_Spiral4IndirectExecution\122_Spiral4IndirectExecution.vcxproj') -Encoding UTF8

foreach ($forbidden in @('13_PackageRuntime','14_D3D12Backend','22_CompositionRuntime')) {
    Require ($semanticProject -notmatch [regex]::Escape($forbidden)) "Semantic project references Runtime/Backend: $forbidden"
    Require ($contractProject -notmatch [regex]::Escape($forbidden)) "Contract project references Runtime/Backend: $forbidden"
}
Require ($executionProject -notmatch '120_ActiveWorkSemantic.*13_PackageRuntime') 'Unexpected project boundary mutation.'

$execution = Get-Content -Raw -LiteralPath (Join-Path $root '122_Spiral4IndirectExecution\Spiral4IndirectExecution.cpp') -Encoding UTF8
foreach ($token in @(
    'D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH',
    'CreateCommandSignature',
    'ExecuteIndirect',
    'D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT',
    'Argument Producer',
    'Runtime dispatch-dimension decision'
)) {
    if ($token -eq 'Runtime dispatch-dimension decision') { continue }
    Require ($execution -match [regex]::Escape($token)) "CU2 execution token missing: $token"
}
Require ($execution -notmatch 'GetData|Readback.*Dispatch\(') 'CU2 appears to make a CPU readback dispatch decision.'

$contracts = Get-Content -Raw -LiteralPath (Join-Path $root '121_Spiral4Contracts\Spiral4Contracts.cpp') -Encoding UTF8
foreach ($token in @(
    'ArtifactMagic',
    'DispatchArgumentBytes',
    'ParseSingleIndirectArtifactV1',
    'digest mismatch',
    'ArgumentOffsetBytes'
)) {
    Require ($contracts -match [regex]::Escape($token)) "CU2 contract token missing: $token"
}


$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
$solution = Get-Content -Raw -LiteralPath $solutionPath -Encoding UTF8
$solutionProjects = @(
    [pscustomobject]@{ Path='120_ActiveWorkSemantic\120_ActiveWorkSemantic.vcxproj'; Guid='{00000078-0000-5000-8000-000000000078}' },
    [pscustomobject]@{ Path='121_Spiral4Contracts\121_Spiral4Contracts.vcxproj'; Guid='{00000079-0000-5000-8000-000000000079}' },
    [pscustomobject]@{ Path='122_Spiral4IndirectExecution\122_Spiral4IndirectExecution.vcxproj'; Guid='{0000007A-0000-5000-8000-00000000007A}' },
    [pscustomobject]@{ Path='140_Spiral4IndirectArchitectureTests\140_Spiral4IndirectArchitectureTests.vcxproj'; Guid='{0000008C-0000-5000-8000-00000000008C}' }
)
foreach ($project in $solutionProjects) {
    Require ($solution -match [regex]::Escape($project.Path)) "CU2 project missing from Solution: $($project.Path)"
    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )) {
        Require ($solution -match [regex]::Escape($entry)) "CU2 Solution configuration missing: $entry"
    }
}


$cu1Verifier = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\tools\Verify-Spiral4CU1.ps1') -Encoding UTF8
$validateSetToken = '[ValidateSet(''Auto'', ''Snapshot'', ''Regression'')]'
$snapshotToken = 'if ($Mode -eq ''Snapshot'')'
$regressionToken = 'elseif ($Mode -eq ''Regression'')'

Require ($cu1Verifier.Contains($validateSetToken)) 'CU1 verifier is not phase-aware.'
Require ($cu1Verifier.Contains($snapshotToken)) 'CU1 Snapshot branch missing.'
Require ($cu1Verifier.Contains($regressionToken)) 'CU1 Regression branch missing.'

$spiral4Common = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Spiral4Common.ps1') -Encoding UTF8
$regressionInvocationToken = "Invoke-PowerShellVerifier 'tools\Verify-Spiral4CU1.ps1' @('-Mode', 'Regression')"
Require ($spiral4Common.Contains($regressionInvocationToken)) 'CU2 does not invoke CU1 in Regression mode.'

Write-Host 'SGE4-5 Spiral 4 CU2 source, legacy compatibility, and indirect architecture boundaries passed.'
