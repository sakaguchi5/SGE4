$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$spiralRoot = Join-Path $root 'docs\spiral1'
$manifestPath = Join-Path $spiralRoot 'CONTRACT_MANIFEST_V1.json'
$codeBaselinePath = Join-Path $spiralRoot 'F0_CODE_BASELINE.sha256'

function Get-Sha256([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Spiral 1 contract manifest is missing.' }
$manifest = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
if ($manifest.schema -ne 'SGE4-5.Spiral1.ContractManifest.V1') { throw 'Unexpected contract manifest schema.' }
if ($manifest.status -ne 'frozen') { throw 'Contract manifest is not frozen.' }
if ($manifest.stage -ne 'Stage03') { throw 'Contract manifest is not Stage03.' }
if ($manifest.foundationCommit -ne '2e79dbf96255f1cac4dbb9fc133662123e78cf4a') { throw 'Foundation commit mismatch.' }
if ([int]$manifest.foundationProjectCount -ne 57) { throw 'Foundation project count mismatch.' }
if ([int]$manifest.targetAbi.d3d12Schema -ne 17 -or [int]$manifest.targetAbi.runtime -ne 17) { throw 'Target ABI mismatch.' }

$solution = Join-Path $root 'SemanticGpuEngine4-5.sln'
if ((Get-Sha256 $solution) -ne [string]$manifest.foundationSolutionSha256) { throw 'Foundation solution digest changed during Stage03.' }
$projects = @(Get-ChildItem -Path $root -Recurse -File -Filter *.vcxproj | Where-Object { $_.FullName -notlike (Join-Path $root 'build\*') })
if ($projects.Count -ne 57) { throw "Stage03 must preserve 57 projects; found $($projects.Count)." }

$expectedContractPaths = @(
    'docs/spiral1/SGE4-5_Spiral1_Completion_Spec_v0.3.md',
    'docs/spiral1/STAGE03_RESEARCH_CONTRACT_FREEZE.md',
    'docs/spiral1/SEMANTIC_CONVENTION_V1.md',
    'docs/spiral1/OBSERVATION_CONTRACT_V1.md',
    'docs/spiral1/CORPUS_V1.md',
    'docs/spiral1/CORPUS_DEFINITION_V1.json',
    'docs/spiral1/PROGRAM_TEMPLATE_CONTRACT_V1.md',
    'docs/spiral1/AUTHORITY_AND_PROJECT_BOUNDARIES_V1.md',
    'docs/spiral1/NON_GOALS_V1.md',
    'docs/spiral1/PROOF_LEDGER_V1.md',
    'docs/spiral1/STAGE_MAP_V1.md',
    'docs/spiral1/F0_CODE_BASELINE.sha256'
)
$manifestPaths = @($manifest.contracts | ForEach-Object { [string]$_.path })
foreach ($relative in $expectedContractPaths) {
    if ($manifestPaths -notcontains $relative) { throw "Contract is absent from manifest: $relative" }
}
if ($manifestPaths.Count -ne $expectedContractPaths.Count) { throw 'Contract manifest contains an unexpected number of contract files.' }

foreach ($entry in @($manifest.contracts)) {
    $relative = [string]$entry.path
    if ($relative -match '[^\x00-\x7F]') { throw "Non-ASCII contract path: $relative" }
    $path = Join-Path $root ($relative.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing contract file: $relative" }
    $actual = Get-Sha256 $path
    if ($actual -ne [string]$entry.sha256) { throw "Contract digest mismatch: $relative" }
    if ((Get-Item -LiteralPath $path).Length -ne [long]$entry.bytes) { throw "Contract byte count mismatch: $relative" }
    if ([IO.Path]::GetExtension($path) -eq '.md') {
        $text = Get-Content -Raw -LiteralPath $path -Encoding UTF8
        if ($text -match '(?im)\b(TODO|TBD|PLACEHOLDER)\b') { throw "Unfrozen placeholder remains: $relative" }
    }
}

if (-not (Test-Path -LiteralPath $codeBaselinePath)) { throw 'F0 code baseline is missing.' }
$baselineEntries = @{}
foreach ($line in Get-Content -LiteralPath $codeBaselinePath -Encoding UTF8) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') { throw "Malformed F0 code baseline line: $line" }
    $baselineEntries[$Matches[2]] = $Matches[1]
}
foreach ($relative in $baselineEntries.Keys) {
    $path = Join-Path $root ($relative.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $path)) { throw "F0 code file is missing: $relative" }
    if ((Get-Sha256 $path) -ne $baselineEntries[$relative]) { throw "F0 code changed during contract freeze: $relative" }
}
$codeExtensions = @('.cpp','.h','.hpp','.c','.cc','.hlsl','.vcxproj','.sln','.props','.targets')
$currentCodeFiles = @(Get-ChildItem -Path $root -Recurse -File | Where-Object {
    $codeExtensions -contains $_.Extension.ToLowerInvariant() -and
    $_.FullName -notlike (Join-Path $root 'build\*') -and
    $_.FullName -notlike (Join-Path $root '.git\*') -and
    $_.FullName -notlike (Join-Path $root '.vs\*')
})
if ($currentCodeFiles.Count -ne $baselineEntries.Count) { throw 'Stage03 added or removed a code/project file.' }

$proofPath = Join-Path $spiralRoot 'PROOF_LEDGER_V1.md'
$proofText = Get-Content -Raw -LiteralPath $proofPath -Encoding UTF8
for ($index = 1; $index -le 18; ++$index) {
    $id = 'S1-I{0:D2}' -f $index
    $count = ([regex]::Matches($proofText, [regex]::Escape($id))).Count
    if ($count -ne 1) { throw "Proof Ledger must contain $id exactly once; found $count." }
}

$corpusPath = Join-Path $spiralRoot 'CORPUS_DEFINITION_V1.json'
$corpus = Get-Content -Raw -LiteralPath $corpusPath -Encoding UTF8 | ConvertFrom-Json
$scenarioIds = @($corpus.scenarios | ForEach-Object { [string]$_.id })
$expectedIds = 0..15 | ForEach-Object { 'S{0:D2}' -f $_ }
if ($scenarioIds.Count -ne 16) { throw 'Corpus V1 must contain 16 scenarios.' }
for ($index = 0; $index -lt 16; ++$index) {
    if ($scenarioIds[$index] -ne $expectedIds[$index]) { throw 'Corpus scenario order is not S00-S15.' }
}

$forbiddenStage03Projects = @(
    '60_PgaRigidTransformSemantic','61_Spiral1Contracts','62_Spiral1Corpus',
    '63_D3D12RepresentationCandidate','64_D3D12RepresentationPlanner',
    '65_D3D12RepresentationVerifier','66_Spiral1LeafCompiler','67_Spiral1Observer','68_Spiral1Scenarios'
)
foreach ($name in $forbiddenStage03Projects) {
    if (Test-Path -LiteralPath (Join-Path $root $name)) { throw "Stage03 must not create empty future project: $name" }
}

Write-Host 'SGE4-5 Spiral 1 Research Contract verification passed.'
Write-Host "Contracts: $($manifest.contracts.Count), invariants: 18, scenarios: 16, preserved projects: 57."
