$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $testsRoot
$required = @(
    '78_Spiral1PerformanceExperiment/78_Spiral1PerformanceExperiment.vcxproj',
    '78_Spiral1PerformanceExperiment/Spiral1PerformanceExperiment.h',
    '78_Spiral1PerformanceExperiment/Spiral1PerformanceExperiment.cpp',
    '79_Spiral1Launcher/79_Spiral1Launcher.vcxproj',
    '79_Spiral1Launcher/main.cpp',
    'docs/spiral1/STAGE13_REAL_GPU_MEASUREMENT_V1.md',
    'docs/spiral1/STAGE14_DECISION_EVIDENCE_V1.md',
    'docs/spiral1/CU6_EVIDENCE_LEDGER.md',
    'docs/spiral1/SPIRAL1_STATUS_AFTER_CU6.md',
    'tests/Run-Spiral1CU6.ps1',
    'run_sge4_5_stage13_real_gpu_measurement.bat',
    'run_sge4_5_stage14_decision_evidence.bat',
    'run_sge4_5_cu6_measurement_decision_evidence.bat'
)
foreach ($relative in $required) { if (-not (Test-Path -LiteralPath (Join-Path $root ($relative.Replace('/','\'))))) { throw "CU6 required file is missing: $relative" } }
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln')
foreach ($project in @('78_Spiral1PerformanceExperiment','79_Spiral1Launcher')) { if ($solution -notmatch [regex]::Escape("$project\$project.vcxproj")) { throw "CU6 project is not registered: $project" } }
$experiment = Get-Content -Raw -LiteralPath (Join-Path $root '78_Spiral1PerformanceExperiment\Spiral1PerformanceExperiment.cpp')
foreach ($needle in @('DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE','DXGI_ADAPTER_FLAG_SOFTWARE','GetTimestampFrequency','D3D12_QUERY_HEAP_TYPE_TIMESTAMP','ABBA','S03','S15','S07','S12','ObservationContractIdentityV2','measurementSamplesPerAlgorithm/2','DeferredByOwner')) { if ($experiment -notmatch [regex]::Escape($needle)) { throw "CU6 experiment is missing: $needle" } }
$launcher = Get-Content -Raw -LiteralPath (Join-Path $root '79_Spiral1Launcher\main.cpp')
foreach ($needle in @('--measure','--report','--self-test','--warmup','--measurement','--runs','--adapter-index','--clock-policy','DeferredByOwner')) { if ($launcher -notmatch [regex]::Escape($needle)) { throw "CU6 launcher is missing: $needle" } }
$decision = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral1\STAGE14_DECISION_EVIDENCE_V1.md')
if ($decision -notmatch 'NextCapabilitySelection = DeferredByOwner') { throw 'CU6 deferred decision marker is missing.' }
foreach ($forbidden in @('Spiral 2A selected','Spiral 2B selected','Spiral 2C selected','Spiral 2D selected','Spiral 2E selected','Spiral 2F selected')) { if ($decision -match [regex]::Escape($forbidden)) { throw "CU6 illegally selects a next capability: $forbidden" } }
foreach ($backendProject in @('13_PackageRuntime','14_D3D12Backend','22_CompositionRuntime','23_CompositionRecovery')) { $text=Get-Content -Raw -LiteralPath (Join-Path $root "$backendProject\$backendProject.vcxproj"); if($text -match '78_Spiral1PerformanceExperiment|79_Spiral1Launcher'){throw "$backendProject illegally references CU6 experiment projects."} }
Write-Host 'SGE4-5 Spiral 1 CU6 measurement and deferred-decision structure passed.'
