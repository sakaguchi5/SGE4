$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projects=@('83_HierarchyRepresentationCandidate','84_HierarchyRepresentationPlanner','85_HierarchyRepresentationVerifier','92_Spiral2CandidateGraphTests','93_Spiral2AuthorityMutationTests')
foreach($name in $projects){$path=Join-Path $root "$name\$name.vcxproj";if(-not(Test-Path -LiteralPath $path)){throw "Missing CU2 project: $name"}}
[xml]$verifier=Get-Content -Raw -LiteralPath (Join-Path $root '85_HierarchyRepresentationVerifier\85_HierarchyRepresentationVerifier.vcxproj')
$references=@($verifier.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include})
if($references -match '84_HierarchyRepresentationPlanner'){throw 'Independent verifier references Planner.'}
$verifierText=Get-Content -Raw -LiteralPath (Join-Path $root '85_HierarchyRepresentationVerifier\HierarchyRepresentationVerifier.cpp')
if($verifierText -match 'PlanCandidateGraphV1|HierarchyRepresentationPlanner'){throw 'Verifier calls or includes Planner implementation.'}
$rawText=Get-Content -Raw -LiteralPath (Join-Path $root '83_HierarchyRepresentationCandidate\HierarchyRepresentationCandidate.h')
if($rawText -match '\b(Freeze|Execute|Submit)\b'){throw 'Raw Candidate Graph exposes execution/freeze authority.'}
Write-Host 'SGE4-5 Spiral 2 CU2 independent authority boundaries passed.'
