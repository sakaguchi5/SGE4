param(
    [ValidateSet('Auto','Snapshot','Regression')]
    [string]$Mode = 'Auto'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

if ($Mode -eq 'Auto') {
    $cu3Manifest = Join-Path $root 'docs\spiral5\CU3_AUTHORITY_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu3Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}

$manifestPath = Join-Path $root 'docs\spiral5\CU2_ARCHITECTURE_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 5 CU2 architecture manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral5.CU2ArchitectureManifest.V1') 'CU2 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU2') 'CU2 manifest completion unit mismatch.'
Require ($m.ownerBaselineCommit -eq 'f29d6597ec370d963c7b7dfbbc9af9590e8bd58f') 'CU2 owner baseline mismatch.'
Require ($m.extensionStrategy -eq 'VersionedSidecarTemporalExtensionV1') 'CU2 temporal extension strategy mismatch.'
Require ($m.legacySchemaMutation -eq 'None') 'CU2 must not mutate Schema 17.'
Require ($m.legacyRuntimeMutation -eq 'None') 'CU2 must not mutate Runtime 17.'
Require ($m.legacyBackendMutation -eq 'None') 'CU2 must not mutate the canonical D3D12 Backend.'
Require ($m.spiral4IndirectExtension -eq 'ReusedAndIdentityBound') 'CU2 must bind the Spiral 4 indirect artifact.'
Require ($m.candidate -eq 'GlobalMotorHistoryReuse') 'CU2 Candidate mismatch.'
Require ($m.historyRole -eq 'GlobalMotorHistory') 'CU2 history role mismatch.'
Require ([int]$m.historyDepth -eq 1) 'CU2 history depth mismatch.'
Require ([int]$m.historyBytes -eq 256) 'CU2 history byte count mismatch.'
Require ($m.initialHistory -eq 'InvalidUntilInvocationZeroUpdate') 'CU2 initial history rule mismatch.'
Require ($m.holdCompletion -eq 'ConsumerCompletionRetainsHistory') 'CU2 Hold completion mismatch.'
Require ($m.recoveryPolicy -eq 'InvalidateThenExplicitUpdateReseed') 'CU2 Recovery policy mismatch.'
Require ($m.runtimeTemporalPolicyDecision -eq 'None') 'CU2 Runtime temporal decision must remain None.'
Require ($m.updateDispatchRule -eq 'GPUGeneratedUpdate1Hold0') 'CU2 GPU Dispatch rule mismatch.'
Require ([int]$m.consumerDispatchX -eq 64) 'CU2 Consumer dispatch mismatch.'
Require (($m.architectureSchedules -join ',') -eq 'P1,P4,P64,Irregular') 'CU2 architecture schedule corpus mismatch.'
Require ([int]$m.architectureInvocationCount -eq 512) 'CU2 architecture invocation count mismatch.'

$required = @(
'134_TemporalStateSemantic\134_TemporalStateSemantic.vcxproj','134_TemporalStateSemantic\TemporalStateSemantic.h','134_TemporalStateSemantic\TemporalStateSemantic.cpp',
'135_Spiral5TemporalContracts\135_Spiral5TemporalContracts.vcxproj','135_Spiral5TemporalContracts\Spiral5TemporalContracts.h','135_Spiral5TemporalContracts\Spiral5TemporalContracts.cpp',
'136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj','136_Spiral5TemporalExecution\Spiral5TemporalExecution.h','136_Spiral5TemporalExecution\Spiral5TemporalExecution.cpp',
'145_Spiral5TemporalArchitectureTests\145_Spiral5TemporalArchitectureTests.vcxproj','145_Spiral5TemporalArchitectureTests\main.cpp',
'tests\Spiral5Common.ps1','tests\Run-Spiral5CU2.ps1','tests\tools\Register-Spiral5CU2Projects.ps1','tests\tools\Verify-Spiral5CU2.ps1',
'run_sge4_5_spiral5_cu2_prepare.bat','run_sge4_5_spiral5_cu2_global_motor_history.bat',
'docs\spiral5\CU2_ARCHITECTURE_MANIFEST_V1.json','docs\spiral5\CU2_BASELINE_SUFFICIENCY_AND_GAP.md','docs\spiral5\CU2_GLOBAL_MOTOR_HISTORY_ARCHITECTURE.md','docs\spiral5\CU2_CHANGESET.md','docs\spiral5\CU2_EVIDENCE_LEDGER.md')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 5 CU2 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='134_TemporalStateSemantic\134_TemporalStateSemantic.vcxproj';Guid='{00000086-0000-5000-8000-000000000086}'},
    [pscustomobject]@{Path='135_Spiral5TemporalContracts\135_Spiral5TemporalContracts.vcxproj';Guid='{00000087-0000-5000-8000-000000000087}'},
    [pscustomobject]@{Path='136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj';Guid='{00000088-0000-5000-8000-000000000088}'},
    [pscustomobject]@{Path='145_Spiral5TemporalArchitectureTests\145_Spiral5TemporalArchitectureTests.vcxproj';Guid='{00000091-0000-5000-8000-000000000091}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

$schema = Get-Content -Raw -LiteralPath (Join-Path $root '10_D3D12PackageSchema\D3D12Schema.h') -Encoding UTF8
Require ($schema -match 'Temporal\s*=\s*1u\s*<<\s*1') 'Existing Package Temporal Resource flag missing.'
Require ($schema -match 'TemporalCurrent') 'Existing TemporalCurrent view flag missing.'
Require ($schema -match 'TemporalPrevious') 'Existing TemporalPrevious view flag missing.'
Require ($schema -match 'WaitTemporal') 'Existing WaitTemporal operation missing.'
$composition = Get-Content -Raw -LiteralPath (Join-Path $root '17_CompositionContract\CompositionContract.h') -Encoding UTF8
Require ($composition -notmatch '\bHistoryGeneration\b') 'Canonical Composition Contract unexpectedly contains HistoryGeneration.'
Require ($composition -notmatch '\bUpdateSchedule\b') 'Canonical Composition Contract unexpectedly contains UpdateSchedule.'
Require ($composition -notmatch '\bRetainedHistoryCompletion\b') 'Canonical Composition Contract unexpectedly contains retained-history completion.'

$contractProject = Get-Content -Raw -LiteralPath (Join-Path $root '135_Spiral5TemporalContracts\135_Spiral5TemporalContracts.vcxproj') -Encoding UTF8
Require ($contractProject -match '121_Spiral4Contracts') 'Temporal Contract must bind the Spiral 4 indirect sidecar.'
$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj') -Encoding UTF8
if ($Mode -eq 'Snapshot') {
    foreach($forbidden in @('137_TemporalLoweringCandidate','138_TemporalLoweringPlanner','139_TemporalLoweringVerifier','22_CompositionRuntime','14_D3D12Backend')){Require ($executionProject -notmatch [regex]::Escape($forbidden)) "CU2 Snapshot execution has forbidden dependency: $forbidden"}
} elseif ($Mode -eq 'Regression') {
    Require (Test-Path -LiteralPath (Join-Path $root 'docs\spiral5\CU3_AUTHORITY_MANIFEST_V1.json') -PathType Leaf) 'CU2 Regression requires the CU3 authority marker.'
    Require ($executionProject -match '139_TemporalLoweringVerifier') 'CU3 must route verified execution through the Temporal Verifier type.'
    foreach($forbidden in @('137_TemporalLoweringCandidate','138_TemporalLoweringPlanner','22_CompositionRuntime','14_D3D12Backend')){Require ($executionProject -notmatch [regex]::Escape($forbidden)) "CU2 Regression execution has forbidden direct dependency: $forbidden"}
} else { throw "Unexpected Spiral 5 CU2 verification mode: $Mode" }

$contractCpp = Get-Content -Raw -LiteralPath (Join-Path $root '135_Spiral5TemporalContracts\Spiral5TemporalContracts.cpp') -Encoding UTF8
foreach($token in @('VersionedSidecarTemporalExtension','GlobalMotorHistoryReuse','ConsumerCompletionRetainsHistory','Spiral4IndirectIdentity','HierarchyHoldDispatchX')){Require ($contractCpp -match [regex]::Escape($token)) "CU2 contract token missing: $token"}
$executionCpp = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\Spiral5TemporalExecution.cpp') -Encoding UTF8
foreach($token in @('updateFlag','sourceGeneration','GlobalHistory','ExecuteIndirect','D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH','holdHistoryByteStable','outputMatchesLatestUpdate')){Require ($executionCpp -match [regex]::Escape($token)) "CU2 execution token missing: $token"}
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '145_Spiral5TemporalArchitectureTests\main.cpp') -Encoding UTF8
foreach($token in @('invalidInitial','invalidGeneration','corruptedBytes','P64','Irregular','Runtime temporal-policy decision: None')){Require ($testCpp -match [regex]::Escape($token)) "CU2 test token missing: $token"}

Write-Host "SGE4-5 Spiral 5 CU2 static architecture boundaries passed. Mode: $Mode"
Write-Host 'Package Temporal support reused; Composition generation/schedule/completion gap proven.'
Write-Host 'Legacy Schema 17 / Runtime 17 / Backend mutation: None.'
