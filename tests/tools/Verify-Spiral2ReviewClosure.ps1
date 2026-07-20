$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Read-Text([string]$relative) {
    $path = Join-Path $root $relative
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing review-closure file: $relative" }
    return Get-Content -Raw -LiteralPath $path -Encoding UTF8
}
function Require-Token([string]$text,[string]$token,[string]$reason) {
    if ($text -notmatch [regex]::Escape($token)) { throw "$reason Missing token: $token" }
}
function Forbid-Token([string]$text,[string]$token,[string]$reason) {
    if ($text -match [regex]::Escape($token)) { throw "$reason Forbidden token: $token" }
}

$candidateHeader = Read-Text '83_HierarchyRepresentationCandidate/HierarchyRepresentationCandidate.h'
$candidateSource = Read-Text '83_HierarchyRepresentationCandidate/HierarchyRepresentationCandidate.cpp'
$verifierHeader = Read-Text '85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h'
$verifierSource = Read-Text '85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.cpp'
$leafHeader = Read-Text '86_Spiral2LeafCompiler/Spiral2LeafCompiler.h'
$leafSource = Read-Text '86_Spiral2LeafCompiler/Spiral2LeafCompiler.cpp'
$leafProject = Read-Text '86_Spiral2LeafCompiler/86_Spiral2LeafCompiler.vcxproj'
$packageLoweringSource = Read-Text '11_D3D12PackageLowering/D3D12PackageLowering.cpp'
$compilerHeader = Read-Text '12_SGE4_5Compiler/SGE4_5Compiler.h'
$compilerSource = Read-Text '12_SGE4_5Compiler/SGE4_5Compiler.cpp'
$observerHeader = Read-Text '87_Spiral2Observer/Spiral2Observer.h'
$observerSource = Read-Text '87_Spiral2Observer/Spiral2Observer.cpp'
$scenarioSource = Read-Text '88_Spiral2Scenarios/Spiral2Scenarios.cpp'
$performanceHeader = Read-Text '89_Spiral2PerformanceExperiment/Spiral2PerformanceExperiment.h'
$performanceSource = Read-Text '89_Spiral2PerformanceExperiment/Spiral2PerformanceExperiment.cpp'
$graphTest = Read-Text '92_Spiral2CandidateGraphTests/main.cpp'
$mutationTest = Read-Text '93_Spiral2AuthorityMutationTests/main.cpp'
$leafTest = Read-Text '94_Spiral2LeafPackageTests/main.cpp'
$warpTest = Read-Text '96_Spiral2WarpObservationTests/main.cpp'

# 1. Exact verified operation plan is the only lowering input.
foreach ($token in @('VerifiedHierarchyOperationV1','VerifiedHierarchyExecutionPlanV1','const VerifiedHierarchyExecutionPlanV1& Plan()')) {
    Require-Token ($verifierHeader + $verifierSource) $token 'Verified execution IR is incomplete.'
}
Require-Token $leafSource 'for(std::size_t i=1;i<plan.operations.size();++i)' 'Leaf Compiler does not lower every verified operation.'
Require-Token $leafSource 'CompileVerifiedOperation(h,plan.kind,plan.operations[i]' 'Leaf Compiler does not consume the exact verified operation.'
Forbid-Token $leafSource 'switch(group.kind)' 'Leaf Compiler still rebuilds execution from CandidateKind.'
Forbid-Token $verifierSource 'candidate::ProgramIdentityV1' 'Verifier reuses Candidate program-identity implementation.'
Forbid-Token $verifierSource 'PlanCandidateGraphV1' 'Verifier calls Planner implementation.'

# 2. Package operation, program, binding and dispatch are rebound and revalidated.
# Schema decoding stays in Package Lowering; Spiral2 Leaf Compiler reaches it only through the authorized Compiler facade.
foreach ($token in @('D3D12PackageView::Decode','ExecuteCompute','InspectFrozenComputePackageEvidenceV1')) {
    Require-Token $packageLoweringSource $token 'Schema-owned Package evidence inspection is incomplete.'
}
foreach ($token in @('InspectCanonicalComputePackageEvidenceV1','FrozenComputePackageEvidenceV1')) {
    Require-Token ($compilerHeader + $compilerSource) $token 'Compiler Package-evidence facade is incomplete.'
}
foreach ($token in @('InspectCanonicalComputePackageEvidenceV1','packageProgramInterfaceDigest','packageBindingLayoutDigest','packageShaderBytecodeDigest','Package dispatch does not match the verified operation','Package is not bound to the verified operation evidence')) {
    Require-Token ($leafHeader + $leafSource) $token 'Verified Plan to Package evidence is incomplete.'
}
Forbid-Token ($leafSource + $leafProject) '10_D3D12PackageSchema' 'Spiral2 Leaf Compiler bypasses the authorized Compiler facade.'
Forbid-Token $leafSource 'D3D12PackageView::Decode' 'Spiral2 Leaf Compiler decodes Target Schema directly.'
foreach ($token in @('verifiedProgramTemplateIdentity','packageProgramInterfaceDigest','packageBindingLayoutDigest','packageShaderBytecodeDigest','mutations==63')) {
    Require-Token $leafTest $token 'Leaf Package mutation proof is incomplete.'
}

# 3. One authoritative hierarchy execution model: one serial canonical-order dispatch.
foreach ($token in @('SingleDispatchCanonicalOrderSerial','proposedCandidateLeafDispatchCount')) {
    Require-Token ($candidateHeader + $candidateSource + $verifierHeader + $verifierSource) $token 'Hierarchy dispatch model is not explicit.'
}
foreach ($token in @('MatrixHierarchy,48,bones,48,bones,1','MotorHierarchy,32,bones,32,bones,1','[numthreads(64,1,1)]','if(id.x!=0)return')) {
    Require-Token ($candidateSource + $verifierSource + $leafSource) $token 'Verified and executable hierarchy dispatch models differ.'
}
foreach ($token in @('candidateLeafDispatchCount','dispatchX!=1')) {
    Require-Token ($graphTest + $mutationTest) $token 'Dispatch-model tests are incomplete.'
}

# 4. Verified identity and actual Package artifact digests are one binding identity.
foreach ($token in @('verifiedRepresentationCertificate','verifiedOperationIdentity','verifiedProgramTemplateIdentity','executionDigest','packageProgramInterfaceDigest','packageBindingLayoutDigest','packageShaderBytecodeDigest','packageBindingIdentity')) {
    Require-Token $leafSource $token 'Package binding identity omits required authority evidence.'
}

# 5. Complete observation ledger and componentwise GPU records.
foreach ($token in @('matrixReference','directReference','hybridReference','matrixDirect','matrixHybrid','directHybrid','maxRelativeError','maxUlpDistance','rmsEuclideanError','firstMismatchIndex','maxTranslationError','minimumDeterminant')) {
    Require-Token ($observerHeader + $observerSource) $token 'CPU Observation Contract is incomplete.'
}
foreach ($token in @('RecordStride=304','absARef','absBRef','absCRef','absAB','absAC','absBC','relARef','relAB','ulpARef','ulpAB','gpuRecordStride=304')) {
    Require-Token $scenarioSource $token 'GPU componentwise Observation Record is incomplete.'
}
foreach ($token in @('firstMismatchIndex','maxPairwiseRelativeError','maxPairwiseUlpDistance','maxTranslationError','minimumDeterminant')) {
    Require-Token $warpTest $token 'WARP test does not inspect the complete Observation Contract.'
}

# 6. Candidate-specific materialization and end-to-end evidence only.
foreach ($token in @('MeasurementEvidenceSchemaVersionV1 = 2','materializationNanoseconds','dynamicInvocationBytes','plannedDispatchCount','packageDispatchCount','endToEndNanoseconds','candidate_e2e_ns','candidate_materialization_ns','spiral2_measurement_evidence_v2.bin')) {
    Require-Token ($performanceHeader + $performanceSource) $token 'Candidate-specific measurement evidence is incomplete.'
}
foreach ($token in @('compositionMaterializationNanoseconds','compositionEndToEndNanoseconds','proposedDispatchCount','dynamicUploadBytes')) {
    Forbid-Token ($performanceHeader + $performanceSource) $token 'Ambiguous aggregate or proposal-derived measurement remains.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 2 REVIEW CLOSURE STATIC AUDIT PASSED'
Write-Host '1: Verified operation IR -> exact Leaf lowering'
Write-Host '2: Package operation/program/binding/dispatch rebound'
Write-Host '3: Single-dispatch canonical-order serial hierarchy model'
Write-Host '4: Verified identities bound to actual Package artifacts'
Write-Host '5: Full CPU and componentwise GPU Observation Contract'
Write-Host '6: Candidate-specific materialization and end-to-end evidence V2'
Write-Host '============================================================'
