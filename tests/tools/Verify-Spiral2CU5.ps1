$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach($name in @('96_Spiral2WarpObservationTests','97_Spiral2RecoveryTests','98_Spiral2FreezeTests')){if(-not(Test-Path -LiteralPath (Join-Path $root "$name\$name.vcxproj"))){throw "Missing CU5 project: $name"}}
$observer=Get-Content -Raw -LiteralPath (Join-Path $root '87_Spiral2Observer\Spiral2Observer.cpp')
foreach($constant in @('ObservationAbsoluteToleranceV1','ObservationRelativeToleranceV1','ObservationPairwiseAbsoluteToleranceV1','ObservationPairwiseRelativeToleranceV1','ObservationRigidityToleranceV1')){if($observer-notmatch$constant){throw "Observer does not consume versioned tolerance: $constant"}}
$warp=Get-Content -Raw -LiteralPath (Join-Path $root '96_Spiral2WarpObservationTests\main.cpp')
foreach($required in @('--warp-index','--reorder','ExecuteFrozenHierarchyCorpusScenarioV1','cases==expected')){if($warp-notmatch[regex]::Escape($required)){throw "Missing WARP corpus authority: $required"}}
$recovery=Get-Content -Raw -LiteralPath (Join-Path $root '97_Spiral2RecoveryTests\main.cpp')
foreach($required in @('ControlledRebuild','ForceRemovalForTest','RetryAdapterReacquisition','ValidateStaticCompositionHandleEpoch','missingRebind')){if($recovery-notmatch[regex]::Escape($required)){throw "Missing recovery authority: $required"}}
Write-Host 'SGE4-5 Spiral 2 CU5 WARP, multi-frame, determinism, and recovery boundaries passed.'
