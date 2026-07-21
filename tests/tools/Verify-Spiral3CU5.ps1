$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach($name in @('108_Spiral3Execution','115_Spiral3WarpObservationTests','116_Spiral3RecoveryTests','117_Spiral3FreezeTests')){
    if(-not(Test-Path -LiteralPath (Join-Path $root "$name\$name.vcxproj"))){throw "Missing CU5 project: $name"}
}

[xml]$scenarioProject=Get-Content -Raw -LiteralPath (Join-Path $root '107_Spiral3Scenarios\107_Spiral3Scenarios.vcxproj')
$scenarioReferences=@($scenarioProject.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include})
foreach($forbidden in @('14_D3D12Backend','22_CompositionRuntime','23_CompositionRecovery','108_Spiral3Execution')){
    if($scenarioReferences -match [regex]::Escape($forbidden)){throw "CU4 scenario architecture leaked CU5 Runtime dependency: $forbidden"}
}

[xml]$executionProject=Get-Content -Raw -LiteralPath (Join-Path $root '108_Spiral3Execution\108_Spiral3Execution.vcxproj')
$executionReferences=@($executionProject.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include})
foreach($required in @('14_D3D12Backend','22_CompositionRuntime','107_Spiral3Scenarios')){
    if(-not($executionReferences -match [regex]::Escape($required))){throw "Execution support is missing dependency: $required"}
}
$execution=Get-Content -Raw -LiteralPath (Join-Path $root '108_Spiral3Execution\Spiral3Execution.cpp')
foreach($required in @('SubmitStaticComposition','ReadStaticCompositionBuffer','PointsFlowKeyV1','ValidateGpuRecords','expectedDynamicSlotCount!=2','executionOrder.size()!=11','frozenDigestBefore','frozenDigestAfter')){
    if($execution-notmatch[regex]::Escape($required)){throw "Missing CU5 execution authority: $required"}
}

$warp=Get-Content -Raw -LiteralPath (Join-Path $root '115_Spiral3WarpObservationTests\main.cpp')
foreach($required in @('--warp-index','--reorder','ExecuteFrozenReuseCorpusScenarioV1','frames.size()!=12','frozenDigestBefore','frozenDigestAfter')){
    if($warp-notmatch[regex]::Escape($required)){throw "Missing WARP corpus authority: $required"}
}
$recovery=Get-Content -Raw -LiteralPath (Join-Path $root '116_Spiral3RecoveryTests\main.cpp')
foreach($required in @('ControlledRebuild','ForceRemovalForTest','RetryAdapterReacquisition','ValidateStaticCompositionHandleEpoch','missingRebind','leafCountBefore!=11','resourceCountBefore!=12')){
    if($recovery-notmatch[regex]::Escape($required)){throw "Missing recovery authority: $required"}
}
$freeze=Get-Content -Raw -LiteralPath (Join-Path $root '117_Spiral3FreezeTests\main.cpp')
foreach($required in @('ArchitectureFreeze','SerializeRawCandidateGraphV1','SerializeVerifiedReuseRepresentationV1','SerializeFrozenReuseComparisonScenarioEvidenceV1','frames.Value().size()')){
    if($freeze-notmatch[regex]::Escape($required)){throw "Missing Freeze evidence authority: $required"}
}

foreach($runtimeProject in @('13_PackageRuntime','14_D3D12Backend','22_CompositionRuntime')){
    $files=Get-ChildItem -LiteralPath (Join-Path $root $runtimeProject) -File|Where-Object{$_.Extension-in'.h','.cpp','.vcxproj'}
    foreach($file in $files){$text=Get-Content -Raw -LiteralPath $file.FullName;if($text-match 'Spiral3|100_ReuseSweepSemantic|101_Spiral3Contracts|102_Spiral3Corpus|103_ReuseRepresentation|104_ReuseRepresentation|105_ReuseRepresentation|106_Spiral3LeafCompiler|107_Spiral3Scenarios|108_Spiral3Execution'){throw "Spiral 3 concept leaked into runtime project: $($file.FullName)"}}
}
Write-Host 'SGE4-5 Spiral 3 CU5 WARP, freeze, determinism, and recovery boundaries passed.'
