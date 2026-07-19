$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot

$requiredProjects = @(
    '60_PgaRigidTransformSemantic','61_Spiral1Contracts','62_Spiral1Corpus','67_Spiral1Observer','70_Spiral1SemanticTests'
)
$projects = @(Get-ChildItem -Path $root -Recurse -File -Filter *.vcxproj | Where-Object { $_.FullName -notlike (Join-Path $root 'build\*') })
if ($projects.Count -ne 62) { throw "CU1 requires exactly 62 projects; found $($projects.Count)." }
foreach ($name in $requiredProjects) {
    $path = Join-Path $root "$name\$name.vcxproj"
    if (-not (Test-Path -LiteralPath $path)) { throw "CU1 project is missing: $name" }
}
foreach ($name in @('63_D3D12RepresentationCandidate','64_D3D12RepresentationPlanner','65_D3D12RepresentationVerifier','66_Spiral1LeafCompiler','68_Spiral1Scenarios')) {
    if (Test-Path -LiteralPath (Join-Path $root $name)) { throw "A future project was created early: $name" }
}

$semanticRoot = Join-Path $root '60_PgaRigidTransformSemantic'
$semanticText = (Get-ChildItem -Path $semanticRoot -File | Where-Object { $_.Extension -in '.h','.cpp' } | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName -Encoding UTF8 }) -join "`n"
$forbidden = @('Matrix','D3D12','HLSL','Shader','Descriptor','Queue','ResourceState','FrozenPackage','PackageRuntime','Backend')
foreach ($token in $forbidden) {
    if ($semanticText -match [regex]::Escape($token)) { throw "Semantic project contains forbidden execution vocabulary: $token" }
}
if ($semanticText -notmatch 'CanonicalPgaMotorV1' -or $semanticText -notmatch 'ApplyPgaMotorSemanticV1') { throw 'Canonical Semantic public types are absent.' }
if ($semanticText -match '01_MathVocabulary') { throw 'PGA Semantic depends on the Matrix-bearing MathVocabulary project.' }

$contractHeader = Get-Content -Raw -LiteralPath (Join-Path $root '61_Spiral1Contracts\Spiral1Contracts.h') -Encoding UTF8
if ($contractHeader -notmatch 'ComparisonRecordBytesV1 = 96') { throw 'Comparison record byte contract is absent.' }
if ($contractHeader -notmatch 'AbsoluteToleranceV1 = 1.0e-4f' -or $contractHeader -notmatch 'RelativeToleranceV1 = 1.0e-5f') { throw 'Observation tolerance constants changed.' }
$corpusSource = Get-Content -Raw -LiteralPath (Join-Path $root '62_Spiral1Corpus\Spiral1Corpus.cpp') -Encoding UTF8
for ($index = 0; $index -le 15; ++$index) {
    $id = 'S{0:D2}' -f $index
    if ($corpusSource -notmatch ('"' + $id + '"')) { throw "Corpus implementation does not contain $id." }
}
if ($corpusSource -notmatch 'SplitMix64') { throw 'Corpus V1 SplitMix64 generator is absent.' }
if ($corpusSource -match 'Matrix|CandidatePlanner|D3D12') { throw 'CPU reference/corpus project depends on a forbidden lowering concept.' }

$proof = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral1\PROOF_LEDGER_V1.md') -Encoding UTF8
if ($proof -notmatch 'S1-I01.*Pending') { throw 'Frozen Proof Ledger was unexpectedly rewritten before qualification evidence.' }
$cu1Doc = Join-Path $root 'docs\spiral1\CU1_STAGE04_05_SEMANTIC_OBSERVATION_FOUNDATION.md'
if (-not (Test-Path -LiteralPath $cu1Doc)) { throw 'CU1 completion document is missing.' }
$evidencePath = Join-Path $root 'docs\spiral1\CU1_EVIDENCE_LEDGER.md'
if (-not (Test-Path -LiteralPath $evidencePath)) { throw 'CU1 evidence ledger is missing.' }
$evidence = Get-Content -Raw -LiteralPath $evidencePath -Encoding UTF8
if ($evidence -notmatch 'S1-I01' -or $evidence -notmatch 'S1-I13' -or $evidence -notmatch 'Debug process A') { throw 'CU1 evidence mapping is incomplete.' }

Write-Host 'SGE4-5 Spiral 1 CU1 static verification passed.'
Write-Host 'Projects: 62; new CU1 projects: 5; scenarios: S00-S15; point record: 96 bytes.'
