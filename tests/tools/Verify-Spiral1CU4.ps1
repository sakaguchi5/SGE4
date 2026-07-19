$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $testsRoot

$required = @(
    '68_Spiral1Scenarios/68_Spiral1Scenarios.vcxproj',
    '68_Spiral1Scenarios/Spiral1Scenarios.h',
    '68_Spiral1Scenarios/Spiral1Scenarios.cpp',
    '74_Spiral1CompositionTests/74_Spiral1CompositionTests.vcxproj',
    '74_Spiral1CompositionTests/main.cpp',
    '75_Spiral1WarpObservationTests/75_Spiral1WarpObservationTests.vcxproj',
    '75_Spiral1WarpObservationTests/main.cpp',
    'docs/spiral1/CU4_EVIDENCE_LEDGER.md',
    'docs/spiral1/OBSERVATION_CONTRACT_V2.md'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root ($relative.Replace('/','\'))))) {
        throw "CU4 required file is missing: $relative"
    }
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln')
foreach ($project in @('68_Spiral1Scenarios','74_Spiral1CompositionTests','75_Spiral1WarpObservationTests')) {
    if ($solution -notmatch [regex]::Escape("$project\$project.vcxproj")) {
        throw "CU4 project is not registered in the solution: $project"
    }
}
$scenarioProject = Get-Content -Raw -LiteralPath (Join-Path $root '68_Spiral1Scenarios\68_Spiral1Scenarios.vcxproj')
foreach ($reference in @('66_Spiral1LeafCompiler','17_CompositionContract','18_CompositionPlan','19_CompositionVerifier','22_CompositionRuntime')) {
    if ($scenarioProject -notmatch [regex]::Escape($reference)) {
        throw "CU4 Scenario project is missing authority/runtime reference: $reference"
    }
}
foreach ($backendProject in @('13_PackageRuntime','14_D3D12Backend','22_CompositionRuntime')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root "$backendProject\$backendProject.vcxproj")
    if ($text -match '68_Spiral1Scenarios|74_Spiral1CompositionTests|75_Spiral1WarpObservationTests') {
        throw "$backendProject illegally references CU4 experiment projects."
    }
}
$source = Get-Content -Raw -LiteralPath (Join-Path $root '68_Spiral1Scenarios\Spiral1Scenarios.cpp')
foreach ($needle in @('FrozenComparisonScenarioV1','BuildFourLeafContractInput','VerifyAndSeal','FreezeVerifiedComposition','lockedConstantPayload','ValidateRoleGraph','InputPointsFlowKeyV1','GPU Comparison Leaf bytes differ')) {
    if ($source -notmatch [regex]::Escape($needle)) { throw "CU4 architecture implementation is missing: $needle" }
}
foreach ($needle in @('precise float matrixDifference','precise float scaledTolerance','referenceScale','pairScale','WithinReferenceV2','WithinPairV2','firstByte=','recordOffset=')) {
    if ($source -notmatch [regex]::Escape($needle)) { throw "CU4 deterministic GPU record algorithm is missing: $needle" }
}
if ($source -match '\(double\).*Reference|\(double\).*matrixValue|\(double\).*m\[component\]') {
    throw 'CU4 GPU record algorithm must not use adapter-dependent double arithmetic.'
}
$contractSource = Get-Content -Raw -LiteralPath (Join-Path $root '61_Spiral1Contracts\Spiral1Contracts.cpp')
foreach ($needle in @('BuildComparisonRecordV2','ReferencePointScaleV2','PairPointScaleV2','WithinReferenceToleranceV2','WithinPairToleranceV2','const float matrixDifference','const float scaledTolerance','const float limit')) {
    if ($contractSource -notmatch [regex]::Escape($needle)) { throw "CU4 deterministic CPU record mirror is missing: $needle" }
}
if ($contractSource -notmatch [regex]::Escape('SGE4-5.Spiral1.ObservationContract.V2')) { throw 'Observation Contract V2 identity domain is missing.' }
if ($contractSource -notmatch [regex]::Escape('SGE4-5.Spiral1.ObservationContract.V1')) { throw 'Frozen Observation Contract V1 negative control was removed.' }
$stage10 = Get-Content -Raw -LiteralPath (Join-Path $root '74_Spiral1CompositionTests\main.cpp')
foreach ($needle in @('swapComparisonInputs','duplicateWriter','role authority gate','tampered Matrix locked constant')) {
    if ($stage10 -notmatch [regex]::Escape($needle)) { throw "Stage 10 authority test is missing: $needle" }
}
$stage11 = Get-Content -Raw -LiteralPath (Join-Path $root '75_Spiral1WarpObservationTests\main.cpp')
foreach ($needle in @('BuildQualificationCorpusV1','ExecuteFrozenComparisonScenarioV1','mismatchCount','readbackDigest','S00-S15')) {
    if ($stage11 -notmatch [regex]::Escape($needle)) { throw "Stage 11 WARP corpus test is missing: $needle" }
}
Write-Host 'SGE4-5 Spiral 1 CU4 structure, role authority and WARP corpus verification passed.'
