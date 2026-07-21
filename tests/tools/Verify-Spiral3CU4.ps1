$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach($name in @('107_Spiral3Scenarios','114_Spiral3CompositionTests')){if(-not(Test-Path (Join-Path $root "$name\$name.vcxproj"))){throw "Missing CU4 project: $name"}}
[xml]$scenarioProject=Get-Content -Raw (Join-Path $root '107_Spiral3Scenarios\107_Spiral3Scenarios.vcxproj')
$references=@($scenarioProject.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include})
foreach($forbidden in @('14_D3D12Backend','22_CompositionRuntime','23_CompositionRecovery')){if($references -match $forbidden){throw "Spiral 3 CU4 scenario leaked into Runtime/Backend: $forbidden"}}
foreach($required in @('106_Spiral3LeafCompiler','17_CompositionContract','18_CompositionPlan','19_CompositionVerifier')){if(-not($references -match $required)){throw "Spiral 3 CU4 scenario is missing authority dependency: $required"}}
$header=Get-Content -Raw (Join-Path $root '107_Spiral3Scenarios\Spiral3Scenarios.h')
foreach($required in @('FrozenReuseComparisonScenarioV1','BuildFrozenReuseComparisonScenarioV1','ValidateFrozenReuseComparisonScenarioV1','ComputeFrozenReuseComparisonScenarioIdentityV1')){if($header -notmatch [regex]::Escape($required)){throw "Missing CU4 Frozen Composition boundary: $required"}}
$tests=Get-Content -Raw (Join-Path $root '114_Spiral3CompositionTests\main.cpp')
foreach($required in @('contract.leaves.size()!=11','contract.resources.size()!=12','plan.handoffs.size()!=15','plan.signals.size()!=11','plan.waits.size()!=15','SwapObserverAB','SwapReferencePoints','DuplicateWriter','MissingCPointConsumer')){if($tests -notmatch [regex]::Escape($required)){throw "Missing CU4 topology or mutation gate: $required"}}
# Accept both a success expression (mutations == 54) and a diagnostic failure guard (mutations != 54).
# The authority requirement is the cardinality 54, not one particular C++ spelling.
if($tests -notmatch 'mutations\s*(?:==|!=)\s*54'){throw 'Missing CU4 mutation cardinality gate: mutations must be checked against 54'}
Write-Host 'Spiral 3 CU4 eleven-Leaf/twelve-Flow Frozen Composition and role-authority boundaries passed.'
