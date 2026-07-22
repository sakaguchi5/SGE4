param(
    [switch]$RequireClosure,
    [string]$OutputDirectory='build/tests/spiral3/measurement'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach($name in @('109_Spiral3PerformanceExperiment','118_Spiral3PerformanceTests','119_Spiral3Launcher')){
    if(-not(Test-Path -LiteralPath (Join-Path $root "$name\$name.vcxproj"))){throw "Missing CU6 project: $name"}
}
foreach($relative in @(
    '109_Spiral3PerformanceExperiment/Spiral3PerformanceExperiment.h',
    '109_Spiral3PerformanceExperiment/Spiral3PerformanceExperiment.cpp',
    '118_Spiral3PerformanceTests/main.cpp',
    '119_Spiral3Launcher/main.cpp',
    'docs/spiral3/CU6_CHANGESET.md',
    'docs/spiral3/CU6_EVIDENCE_LEDGER.md')){
    if(-not(Test-Path -LiteralPath (Join-Path $root $relative))){throw "Missing CU6 file: $relative"}
}

[xml]$project=Get-Content -Raw -LiteralPath (Join-Path $root '109_Spiral3PerformanceExperiment\109_Spiral3PerformanceExperiment.vcxproj')
$refs=@($project.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include})
foreach($required in @('14_D3D12Backend','22_CompositionRuntime','107_Spiral3Scenarios','108_Spiral3Execution')){
    if(-not($refs -match [regex]::Escape($required))){throw "CU6 measurement project is missing dependency: $required"}
}
$experiment=(Get-Content -Raw -LiteralPath (Join-Path $root '109_Spiral3PerformanceExperiment\Spiral3PerformanceExperiment.h') -Encoding UTF8)+(Get-Content -Raw -LiteralPath (Join-Path $root '109_Spiral3PerformanceExperiment\Spiral3PerformanceExperiment.cpp') -Encoding UTF8)
$timestampAuthority=(Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend\D3D12Backend.h') -Encoding UTF8)+(Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend\D3D12Backend.cpp') -Encoding UTF8)+$experiment
foreach($token in @('enableTimestampProfiling','ConsumeTimestampProfileSamples','EndQuery','ResolveQueryData','timestampFrequency')){
    if($timestampAuthority -notmatch [regex]::Escape($token)){throw "Missing CU6 passive timestamp authority: $token"}
}
foreach($token in @(
    'ABC','ACB','BAC','BCA','CAB','CBA',
    'DXGI_ADAPTER_FLAG_SOFTWARE',
    'CanonicalWarmupCyclesPerOrderV1','CanonicalMeasurementCyclesPerOrderV1','CanonicalRunCountV1',
    'candidateCompositionDigest','targetProfileIdentity','measurementProfileIdentity',
    'NoObservedWinnerTransition','SingleStableWinnerTransition','MultipleWinnerTransitions',
    'RecommendationAuthority = NonAuthoritative','RuntimePolicyAuthorization = None',
    'NextCapabilitySelection = DeferredByOwner','SelectionStatus = OWNER_DECISION_REQUIRED')){
    if($experiment -notmatch [regex]::Escape($token)){throw "Missing CU6 measurement/decision authority: $token"}
}
$launcher=Get-Content -Raw -LiteralPath (Join-Path $root '119_Spiral3Launcher\main.cpp') -Encoding UTF8
foreach($token in @('--measure','--evidence','--report','--frame-index','DeferredByOwner','OWNER_DECISION_REQUIRED')){
    if($launcher -notmatch [regex]::Escape($token)){throw "Missing CU6 launcher boundary: $token"}
}
$selfTest=Get-Content -Raw -LiteralPath (Join-Path $root '118_Spiral3PerformanceTests\main.cpp') -Encoding UTF8
foreach($token in @('RunFormatSelfTestV1','SingleStableWinnerTransition','lowerReuseCount != 4','upperReuseCount != 16')){
    if($selfTest -notmatch [regex]::Escape($token)){throw "Missing CU6 format/crossover self-test: $token"}
}

foreach($runtimeProject in @('13_PackageRuntime','14_D3D12Backend','22_CompositionRuntime')){
    $files=Get-ChildItem -LiteralPath (Join-Path $root $runtimeProject) -File|Where-Object{$_.Extension-in'.h','.cpp','.vcxproj'}
    foreach($file in $files){$text=Get-Content -Raw -LiteralPath $file.FullName;if($text-match 'Spiral3Performance|109_Spiral3PerformanceExperiment|118_Spiral3PerformanceTests|119_Spiral3Launcher'){throw "CU6 concept leaked into Runtime/Backend: $($file.FullName)"}}
}

if($RequireClosure){
    $output=Join-Path $root $OutputDirectory
    foreach($name in @('spiral3_measurement_evidence_v1.bin','measurement_samples.csv','measurement_profile.json','SPIRAL3_DECISION_EVIDENCE_REPORT.md')){
        if(-not(Test-Path -LiteralPath (Join-Path $output $name))){throw "Missing CU6 closure artifact: $name"}
    }
    $report=Get-Content -Raw -LiteralPath (Join-Path $output 'SPIRAL3_DECISION_EVIDENCE_REPORT.md') -Encoding UTF8
    foreach($token in @('HardwareEvidence = Qualified','MeasurementSamples = 576','BalancedOrders = ABC/ACB/BAC/BCA/CAB/CBA','CrossoverClassification =','RecommendationAuthority = NonAuthoritative','RuntimePolicyAuthorization = None','NextCapabilitySelection = DeferredByOwner','SelectionStatus = OWNER_DECISION_REQUIRED')){
        if($report -notmatch [regex]::Escape($token)){throw "Missing CU6 closure boundary: $token"}
    }
}
Write-Host 'SGE4-5 Spiral 3 CU6 measurement, crossover, and Owner-decision boundaries passed.'
