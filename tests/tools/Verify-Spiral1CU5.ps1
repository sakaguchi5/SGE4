$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $testsRoot

$required = @(
    '76_Spiral1RecoveryTests/76_Spiral1RecoveryTests.vcxproj',
    '76_Spiral1RecoveryTests/main.cpp',
    '77_Spiral1FreezeTests/77_Spiral1FreezeTests.vcxproj',
    '77_Spiral1FreezeTests/main.cpp',
    'docs/spiral1/CU5_EVIDENCE_LEDGER.md',
    'docs/spiral1/ARCHITECTURE_FINAL_FREEZE_V1.md',
    'tests/Run-Spiral1CU5.ps1',
    'run_sge4_5_stage12_architecture_freeze.bat',
    'run_sge4_5_cu5_architecture_final_freeze.bat'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root ($relative.Replace('/','\'))))) {
        throw "CU5 required file is missing: $relative"
    }
}
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln')
foreach ($project in @('76_Spiral1RecoveryTests','77_Spiral1FreezeTests')) {
    if ($solution -notmatch [regex]::Escape("$project\$project.vcxproj")) {
        throw "CU5 project is not registered in the solution: $project"
    }
}
$recoveryProject = Get-Content -Raw -LiteralPath (Join-Path $root '76_Spiral1RecoveryTests\76_Spiral1RecoveryTests.vcxproj')
foreach ($reference in @('23_CompositionRecovery','68_Spiral1Scenarios','67_Spiral1Observer','22_CompositionRuntime')) {
    if ($recoveryProject -notmatch [regex]::Escape($reference)) {
        throw "CU5 Recovery project is missing: $reference"
    }
}
$recovery = Get-Content -Raw -LiteralPath (Join-Path $root '76_Spiral1RecoveryTests\main.cpp')
foreach ($needle in @('ControlledRebuild','ForceRemovalForTest','RetryAdapterReacquisition','static-runtime/stale-epoch','static-runtime/device-state','AwaitingAdapter','FreshProcessRematerializationEvidence.V1','--fresh-rematerialization','RequireSameObservation','S15",15')) {
    if ($recovery -notmatch [regex]::Escape($needle)) { throw "CU5 recovery qualification is missing: $needle" }
}
$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral1CU5.ps1')
foreach ($needle in @('fresh-rematerialization-before-removal.bin','--actual-removal','fresh-rematerialization-after-removal.bin','Assert-Triplet')) {
    if ($runner -notmatch [regex]::Escape($needle)) { throw "CU5 process-boundary recovery runner is missing: $needle" }
}
$freeze = Get-Content -Raw -LiteralPath (Join-Path $root '77_Spiral1FreezeTests\main.cpp')
foreach ($needle in @('ArchitectureFinalFreeze.V1','ObservationContractIdentityV2','matrixLeaf.representationSeal','directPgaLeaf.candidateIdentity','frozenCompositionBytes.back','Mutation rejections: 8','S1-I01 through S1-I16')) {
    if ($freeze -notmatch [regex]::Escape($needle)) { throw "CU5 final freeze is missing: $needle" }
}
foreach ($backendProject in @('13_PackageRuntime','14_D3D12Backend','22_CompositionRuntime','23_CompositionRecovery')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root "$backendProject\$backendProject.vcxproj")
    if ($text -match '76_Spiral1RecoveryTests|77_Spiral1FreezeTests') {
        throw "$backendProject illegally references CU5 qualification projects."
    }
}
Write-Host 'SGE4-5 Spiral 1 CU5 recovery, authority and determinism freeze structure passed.'
