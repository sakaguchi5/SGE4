$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$required=@('63_D3D12RepresentationCandidate','64_D3D12RepresentationPlanner','65_D3D12RepresentationVerifier','71_Spiral1RepresentationPlanningTests','72_Spiral1RepresentationAuthorityTests')
$projects=@(Get-ChildItem -Path $root -Recurse -File -Filter *.vcxproj | Where-Object {$_.FullName -notlike (Join-Path $root 'build\*')})
if($projects.Count -lt 67){throw "The repository lost a CU2 project; found $($projects.Count)."}
foreach($n in $required){if(-not(Test-Path -LiteralPath (Join-Path $root "$n\$n.vcxproj"))){throw "Missing CU2 project: $n"}}
$verifierProject=Get-Content -Raw -LiteralPath (Join-Path $root '65_D3D12RepresentationVerifier\65_D3D12RepresentationVerifier.vcxproj') -Encoding UTF8
if($verifierProject -match '64_D3D12RepresentationPlanner'){throw 'Independent Verifier references the Planner.'}
foreach($name in @('09_FrozenPackageCore','10_D3D12PackageSchema','11_D3D12PackageLowering','13_PackageRuntime','14_D3D12Backend','16_FrozenCompositionArtifact','22_CompositionRuntime')){
  foreach($project in @('63_D3D12RepresentationCandidate','64_D3D12RepresentationPlanner','65_D3D12RepresentationVerifier')){
    $text=Get-Content -Raw -LiteralPath (Join-Path $root "$project\$project.vcxproj") -Encoding UTF8
    if($text -match [regex]::Escape($name)){throw "$project has forbidden dependency on $name"}
  }
}
$backend=Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend\14_D3D12Backend.vcxproj') -Encoding UTF8
foreach($n in @('60_PgaRigidTransformSemantic','61_Spiral1Contracts','63_D3D12RepresentationCandidate','64_D3D12RepresentationPlanner','65_D3D12RepresentationVerifier')){if($backend -match [regex]::Escape($n)){throw "Backend references Spiral 1 project: $n"}}
$candidateHeader=Get-Content -Raw -LiteralPath (Join-Path $root '63_D3D12RepresentationCandidate\D3D12RepresentationCandidate.h') -Encoding UTF8
if($candidateHeader -match 'Freeze|Execute\('){throw 'Raw Candidate public contract exposes Freeze or Execute.'}
$verifierHeader=Get-Content -Raw -LiteralPath (Join-Path $root '65_D3D12RepresentationVerifier\D3D12RepresentationVerifier.h') -Encoding UTF8
if($verifierHeader -notmatch 'friend class IndependentRepresentationVerifierV1' -or $verifierHeader -notmatch 'VerifiedRepresentationPlanV1\('){throw 'Opaque verifier-only plan constructor is absent.'}
if(-not(Test-Path -LiteralPath (Join-Path $root 'docs\spiral1\CU2_EVIDENCE_LEDGER.md'))){throw 'CU2 evidence ledger is missing.'}
Write-Host 'SGE4-5 Spiral 1 CU2 dependency and authority boundary verification passed.'
Write-Host "CU2 project floor: 67; current projects: $($projects.Count); Verifier -> Planner reference: absent; Package/Runtime dependencies: absent."
